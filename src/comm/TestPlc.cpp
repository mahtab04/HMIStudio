#include "comm/TestPlc.h"

#include "comm/PlcProtocols.h"

#include <QHash>
#include <QList>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

namespace hmi
{

namespace
{

void put16(QByteArray& bytes, int value)
{
    bytes.append(char((value >> 8) & 0xFF));
    bytes.append(char(value & 0xFF));
}

int get16(const QByteArray& bytes, qsizetype at)
{
    return (quint8(bytes.at(at)) << 8) | quint8(bytes.at(at + 1));
}

constexpr int kDbSize = 1024;
constexpr int kMarkerSize = 1024;
constexpr int kIoSize = 128;
constexpr int kModbusSize = 10000;

} // namespace

class TestPlc::Server : public QObject
{
public:
    Server()
        : m_markers(kMarkerSize, '\0')
        , m_inputs(kIoSize, '\0')
        , m_outputs(kIoSize, '\0')
        , m_coils(kModbusSize, '\0')
        , m_discrete(kModbusSize, '\0')
        , m_holding(kModbusSize, 0)
        , m_input(kModbusSize, 0)
    {
    }

    // Runs in the server's thread.
    QString listen(quint16 s7Port, quint16 modbusPort, quint16* s7Used, quint16* modbusUsed)
    {
        m_s7 = new QTcpServer(this);
        m_modbus = new QTcpServer(this);
        if (!m_s7->listen(QHostAddress::LocalHost, s7Port))
            return QStringLiteral("S7 port %1: %2").arg(s7Port).arg(m_s7->errorString());
        if (!m_modbus->listen(QHostAddress::LocalHost, modbusPort))
            return QStringLiteral("Modbus port %1: %2")
                .arg(modbusPort)
                .arg(m_modbus->errorString());
        *s7Used = m_s7->serverPort();
        *modbusUsed = m_modbus->serverPort();
        connect(m_s7, &QTcpServer::newConnection, this, [this] { accept(*m_s7, true); });
        connect(m_modbus, &QTcpServer::newConnection, this, [this] { accept(*m_modbus, false); });
        auto* heartbeat = new QTimer(this);
        heartbeat->setInterval(1000);
        connect(heartbeat, &QTimer::timeout, this, &Server::beat);
        heartbeat->start();
        return {};
    }

private:
    void accept(QTcpServer& server, bool s7)
    {
        while (QTcpSocket* socket = server.nextPendingConnection())
        {
            socket->setParent(this);
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QTcpSocket::readyRead, this,
                    [this, socket, s7] { s7 ? serveS7(*socket) : serveModbus(*socket); });
        }
    }

    void beat()
    {
        ++m_seconds;
        m_markers[100] = char((m_seconds >> 8) & 0xFF); // MW100
        m_markers[101] = char(m_seconds & 0xFF);
        m_markers[102] = char(quint8(m_markers[102]) ^ 0x01); // M102.0
        m_input[0] = quint16(m_seconds);                      // ir0
        m_discrete[0] = char(m_discrete[0] ? 0 : 1);          // di0
    }

    // --- S7 ---------------------------------------------------------------

    QByteArray* area(int code, int db)
    {
        switch (code)
        {
        case S7::Inputs:
            return &m_inputs;
        case S7::Outputs:
            return &m_outputs;
        case S7::Markers:
            return &m_markers;
        case S7::DataBlock:
        {
            if (db < 1 || db > 65535)
                return nullptr;
            auto it = m_dbs.find(db);
            if (it == m_dbs.end())
                it = m_dbs.insert(db, QByteArray(kDbSize, '\0'));
            return &*it;
        }
        default:
            return nullptr;
        }
    }

    void serveS7(QTcpSocket& socket)
    {
        QByteArray& buffer = m_buffers[&socket];
        buffer += socket.readAll();
        while (buffer.size() >= 4)
        {
            const int length = get16(buffer, 2);
            if (quint8(buffer.at(0)) != 0x03 || length < 7)
            {
                socket.abort();
                return;
            }
            if (buffer.size() < length)
                return;
            const QByteArray frame = buffer.left(length);
            buffer.remove(0, length);
            const QByteArray reply = answerS7(frame);
            if (reply.isEmpty())
            {
                socket.abort();
                return;
            }
            socket.write(reply);
        }
    }

    QByteArray answerS7(const QByteArray& frame)
    {
        const quint8 code = quint8(frame.at(5));
        if (code == 0xE0) // connection request: confirm, echoing its parameters
        {
            // COTP: length, code, destination and source references, ...
            QByteArray cotp = frame.mid(4);
            if (cotp.size() < 7)
                return {};
            cotp[1] = char(0xD0);
            cotp[2] = frame.at(8); // to their source reference
            cotp[3] = frame.at(9);
            cotp[4] = 0x00; // from ours
            cotp[5] = 0x01;
            return S7::tpkt(cotp);
        }
        if (code != 0xF0 || frame.size() < 17 || quint8(frame.at(7)) != 0x32)
            return {};
        const QByteArray s7 = frame.mid(7);
        const int reference = get16(s7, 4);
        const int parameterLength = get16(s7, 6);
        const int dataLength = get16(s7, 8);
        if (s7.size() < 10 + parameterLength + dataLength || parameterLength < 1)
            return {};
        const QByteArray parameters = s7.mid(10, parameterLength);
        const QByteArray data = s7.mid(10 + parameterLength, dataLength);
        const quint8 function = quint8(parameters.at(0));
        if (function == 0xF0 && parameters.size() >= 8)
        {
            QByteArray reply = parameters;
            const int pdu = std::min(get16(parameters, 6), 480);
            reply[6] = char(pdu >> 8);
            reply[7] = char(pdu & 0xFF);
            return acknowledge(reference, reply, {});
        }
        if ((function == 0x04 || function == 0x05) && parameters.size() >= 2)
            return function == 0x04 ? readVariables(reference, parameters)
                                    : writeVariables(reference, parameters, data);
        return acknowledge(reference, {}, {}, 0x81, 0x04); // not supported
    }

    static QByteArray acknowledge(int reference, const QByteArray& parameters,
                                  const QByteArray& data, quint8 errorClass = 0,
                                  quint8 errorCode = 0)
    {
        QByteArray s7;
        s7.append(char(0x32));
        s7.append(char(0x03)); // Ack_Data
        put16(s7, 0);
        put16(s7, reference);
        put16(s7, int(parameters.size()));
        put16(s7, int(data.size()));
        s7.append(char(errorClass));
        s7.append(char(errorCode));
        return S7::dataFrame(s7 + parameters + data);
    }

    struct Spec
    {
        bool bit = false;
        int length = 0; // bytes, or 1 for a bit
        int db = 0;
        int area = 0;
        int address = 0; // in bits
    };

    static bool specAt(const QByteArray& parameters, int index, Spec* spec)
    {
        const qsizetype at = 2 + qsizetype(index) * 12;
        if (parameters.size() < at + 12 || quint8(parameters.at(at)) != 0x12)
            return false;
        spec->bit = quint8(parameters.at(at + 3)) == 0x01;
        spec->length = get16(parameters, at + 4);
        spec->db = get16(parameters, at + 6);
        spec->area = quint8(parameters.at(at + 8));
        spec->address = (quint8(parameters.at(at + 9)) << 16) |
                        (quint8(parameters.at(at + 10)) << 8) | quint8(parameters.at(at + 11));
        return true;
    }

    QByteArray readVariables(int reference, const QByteArray& parameters)
    {
        const int count = quint8(parameters.at(1));
        QByteArray data;
        for (int i = 0; i < count; ++i)
        {
            Spec spec;
            if (!specAt(parameters, i, &spec))
                return {};
            QByteArray* memory = area(spec.area, spec.db);
            const int byte = spec.address / 8;
            const int bytes = spec.bit ? 1 : spec.length;
            if (!memory)
                data += QByteArray("\x0A\x00\x00\x00", 4);
            else if (byte + bytes > memory->size())
                data += QByteArray("\x05\x00\x00\x00", 4);
            else
            {
                data.append(char(S7::kReturnOk));
                data.append(char(spec.bit ? 0x03 : 0x04));
                put16(data, spec.bit ? 1 : bytes * 8);
                if (spec.bit)
                    data.append(char((quint8(memory->at(byte)) >> (spec.address % 8)) & 1));
                else
                    data += memory->mid(byte, bytes);
            }
            if (i + 1 < count && data.size() % 2)
                data.append('\0');
        }
        QByteArray reply;
        reply.append(char(0x04));
        reply.append(char(count));
        return acknowledge(reference, reply, data);
    }

    QByteArray writeVariables(int reference, const QByteArray& parameters, const QByteArray& data)
    {
        const int count = quint8(parameters.at(1));
        QByteArray results;
        qsizetype at = 0;
        for (int i = 0; i < count; ++i)
        {
            Spec spec;
            if (!specAt(parameters, i, &spec) || data.size() < at + 4)
                return {};
            const quint8 transport = quint8(data.at(at + 1));
            int length = get16(data, at + 2);
            if (transport == 0x03 || transport == 0x04 || transport == 0x05)
                length = (length + 7) / 8;
            const QByteArray value = data.mid(at + 4, length);
            at += 4 + length + ((i + 1 < count && length % 2) ? 1 : 0);
            QByteArray* memory = area(spec.area, spec.db);
            const int byte = spec.address / 8;
            if (!memory)
                results.append(char(S7::kReturnObjectDoesNotExist));
            else if (value.size() < length ||
                     byte + (spec.bit ? 1 : int(value.size())) > memory->size())
                results.append(char(S7::kReturnAddressOutOfRange));
            else
            {
                if (spec.bit)
                {
                    const char mask = char(1 << (spec.address % 8));
                    (*memory)[byte] =
                        value.at(0) ? char((*memory)[byte] | mask) : char((*memory)[byte] & ~mask);
                }
                else
                    memory->replace(byte, value.size(), value);
                results.append(char(S7::kReturnOk));
            }
        }
        QByteArray reply;
        reply.append(char(0x05));
        reply.append(char(count));
        return acknowledge(reference, reply, results);
    }

    // --- Modbus -----------------------------------------------------------

    void serveModbus(QTcpSocket& socket)
    {
        QByteArray& buffer = m_buffers[&socket];
        buffer += socket.readAll();
        while (buffer.size() >= 7)
        {
            const int length = get16(buffer, 4);
            if (get16(buffer, 2) != 0 || length < 2 || length > 260)
            {
                socket.abort();
                return;
            }
            if (buffer.size() < 6 + length)
                return;
            const QByteArray request = buffer.left(6 + length);
            buffer.remove(0, 6 + length);
            const QByteArray pdu = answerModbus(request.mid(7));
            socket.write(Modbus::frame(quint16(get16(request, 0)), quint8(request.at(6)), pdu));
        }
    }

    static QByteArray exception(quint8 function, quint8 code)
    {
        QByteArray pdu;
        pdu.append(char(function | 0x80));
        pdu.append(char(code));
        return pdu;
    }

    QByteArray answerModbus(const QByteArray& pdu)
    {
        const quint8 function = quint8(pdu.at(0));
        if (pdu.size() < 5)
            return exception(function, 0x03);
        const int offset = get16(pdu, 1);
        const int count = get16(pdu, 3);
        switch (function)
        {
        case 1:
        case 2:
        {
            const QByteArray& bits = function == 1 ? m_coils : m_discrete;
            if (count < 1 || count > 2000 || offset + count > bits.size())
                return exception(function, 0x02);
            QByteArray reply;
            reply.append(char(function));
            QByteArray packed((count + 7) / 8, '\0');
            for (int i = 0; i < count; ++i)
            {
                if (bits.at(offset + i))
                    packed[i / 8] = char(packed[i / 8] | (1 << (i % 8)));
            }
            reply.append(char(packed.size()));
            return reply + packed;
        }
        case 3:
        case 4:
        {
            const QList<quint16>& registers = function == 3 ? m_holding : m_input;
            if (count < 1 || count > 125 || offset + count > registers.size())
                return exception(function, 0x02);
            QByteArray reply;
            reply.append(char(function));
            reply.append(char(count * 2));
            for (int i = 0; i < count; ++i)
                put16(reply, registers.at(offset + i));
            return reply;
        }
        case 5:
            if (offset >= m_coils.size() || (count != 0xFF00 && count != 0))
                return exception(function, 0x02);
            m_coils[offset] = char(count == 0xFF00 ? 1 : 0);
            return pdu.left(5);
        case 6:
            if (offset >= m_holding.size())
                return exception(function, 0x02);
            m_holding[offset] = quint16(count);
            return pdu.left(5);
        case 16:
        {
            if (count < 1 || count > 123 || offset + count > m_holding.size() ||
                pdu.size() < 6 + count * 2)
                return exception(function, 0x02);
            for (int i = 0; i < count; ++i)
                m_holding[offset + i] = quint16(get16(pdu, 6 + i * 2));
            return pdu.left(5);
        }
        default:
            return exception(function, 0x01);
        }
    }

    QTcpServer* m_s7 = nullptr;
    QTcpServer* m_modbus = nullptr;
    QHash<QTcpSocket*, QByteArray> m_buffers;
    QHash<int, QByteArray> m_dbs;
    QByteArray m_markers;
    QByteArray m_inputs;
    QByteArray m_outputs;
    QByteArray m_coils;
    QByteArray m_discrete;
    QList<quint16> m_holding;
    QList<quint16> m_input;
    quint32 m_seconds = 0;
};

TestPlc::TestPlc(QObject* parent)
    : QObject(parent)
{
}

TestPlc::~TestPlc()
{
    stop();
}

Result<void> TestPlc::start(quint16 s7Port, quint16 modbusPort)
{
    stop();
    m_thread.start();
    auto* server = new Server;
    server->moveToThread(&m_thread);
    QString error;
    quint16 s7Used = 0;
    quint16 modbusUsed = 0;
    QMetaObject::invokeMethod(
        server, [&] { error = server->listen(s7Port, modbusPort, &s7Used, &modbusUsed); },
        Qt::BlockingQueuedConnection);
    if (!error.isEmpty())
    {
        QMetaObject::invokeMethod(
            server, [server] { delete server; }, Qt::BlockingQueuedConnection);
        m_thread.quit();
        m_thread.wait();
        return makeError(ErrorCode::Io, error);
    }
    m_server = server;
    m_s7Port = s7Used;
    m_modbusPort = modbusUsed;
    Q_EMIT runningChanged();
    return {};
}

void TestPlc::stop()
{
    if (!m_server)
        return;
    QMetaObject::invokeMethod(
        m_server, [server = m_server] { delete server; }, Qt::BlockingQueuedConnection);
    m_server = nullptr;
    m_thread.quit();
    m_thread.wait();
    m_s7Port = 0;
    m_modbusPort = 0;
    Q_EMIT runningChanged();
}

} // namespace hmi
