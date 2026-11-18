#include "comm/PlcProtocols.h"

#include <QDeadlineTimer>
#include <QElapsedTimer>
#include <QRegularExpression>
#include <QTcpSocket>

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

qint64 now()
{
    static QElapsedTimer clock = []
    {
        QElapsedTimer t;
        t.start();
        return t;
    }();
    return clock.elapsed();
}

// Reads exactly `count` bytes, or fails at the deadline.
Result<QByteArray> readExactly(QTcpSocket& socket, qint64 count, QDeadlineTimer deadline)
{
    QByteArray result;
    while (result.size() < count)
    {
        if (socket.bytesAvailable() == 0 &&
            !socket.waitForReadyRead(int(std::max<qint64>(1, deadline.remainingTime()))))
            return makeError(ErrorCode::Io, deadline.hasExpired()
                                                ? QStringLiteral("no reply in time")
                                                : socket.errorString());
        result += socket.read(count - result.size());
    }
    return result;
}

Result<void> connectSocket(std::unique_ptr<QTcpSocket>& socket, const QString& host, int port,
                           int timeoutMs)
{
    socket = std::make_unique<QTcpSocket>();
    socket->connectToHost(host, quint16(port));
    if (!socket->waitForConnected(timeoutMs))
    {
        const QString error = socket->errorString();
        socket.reset();
        return makeError(ErrorCode::Io, error);
    }
    socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    return {};
}

Result<void> send(QTcpSocket& socket, const QByteArray& bytes, int timeoutMs)
{
    if (socket.write(bytes) != bytes.size() || !socket.waitForBytesWritten(timeoutMs))
        return makeError(ErrorCode::Io, socket.errorString());
    return {};
}

// The S7 message of a TPKT/COTP data frame, checked to be an Ack_Data reply.
Result<QByteArray> s7Reply(const QByteArray& frame)
{
    // TPKT (4) + COTP DT (3) + S7 Ack_Data header (12)
    if (frame.size() < 19 || quint8(frame.at(5)) != 0xF0 || quint8(frame.at(7)) != 0x32)
        return makeError(ErrorCode::BadFormat, QStringLiteral("not an S7 reply"));
    const QByteArray s7 = frame.mid(7);
    if (quint8(s7.at(1)) != 0x03)
        return makeError(ErrorCode::BadFormat, QStringLiteral("not an S7 acknowledgment"));
    if (s7.at(10) != 0 || s7.at(11) != 0)
        return makeError(ErrorCode::Io, QStringLiteral("the PLC refused the request (error %1/%2)")
                                            .arg(quint8(s7.at(10)), 2, 16, QLatin1Char('0'))
                                            .arg(quint8(s7.at(11)), 2, 16, QLatin1Char('0')));
    const int parameters = get16(s7, 6);
    const int data = get16(s7, 8);
    if (s7.size() < 12 + parameters + data)
        return makeError(ErrorCode::BadFormat, QStringLiteral("a short S7 reply"));
    return s7;
}

QByteArray job(quint16 reference, const QByteArray& parameters, const QByteArray& data = {})
{
    QByteArray s7;
    s7.append(char(0x32));
    s7.append(char(0x01)); // job
    put16(s7, 0);
    put16(s7, reference);
    put16(s7, int(parameters.size()));
    put16(s7, int(data.size()));
    return s7 + parameters + data;
}

QByteArray itemSpec(const S7::Item& item)
{
    QByteArray spec;
    spec.append(char(0x12));
    spec.append(char(0x0A));
    spec.append(char(0x10));
    const bool bit = item.bit >= 0;
    spec.append(char(bit ? 0x01 : 0x02)); // BIT or BYTE
    put16(spec, bit ? 1 : item.bytes);
    put16(spec, item.area == S7::DataBlock ? item.db : 0);
    spec.append(char(item.area));
    const int address = item.byte * 8 + (bit ? item.bit : 0);
    spec.append(char((address >> 16) & 0xFF));
    spec.append(char((address >> 8) & 0xFF));
    spec.append(char(address & 0xFF));
    return spec;
}

QString returnText(quint8 code)
{
    switch (code)
    {
    case S7::kReturnAddressOutOfRange:
        return QStringLiteral("the address is out of range");
    case S7::kReturnObjectDoesNotExist:
        return QStringLiteral("the data block does not exist");
    case 0x03:
        return QStringLiteral("access denied: allow PUT/GET, and use a non-optimized data block");
    default:
        return QStringLiteral("item error %1").arg(code, 2, 16, QLatin1Char('0'));
    }
}

} // namespace

namespace S7
{

QByteArray tpkt(const QByteArray& body)
{
    QByteArray frame;
    frame.append(char(0x03));
    frame.append(char(0x00));
    put16(frame, int(body.size()) + 4);
    return frame + body;
}

QByteArray dataFrame(const QByteArray& s7)
{
    return tpkt(QByteArray("\x02\xF0\x80", 3) + s7);
}

QByteArray connectRequest(int rack, int slot)
{
    QByteArray cotp;
    cotp.append(char(0x11));                        // length after this byte
    cotp.append(char(0xE0));                        // connection request
    put16(cotp, 0);                                 // destination reference
    put16(cotp, 1);                                 // source reference
    cotp.append(char(0x00));                        // class 0
    cotp.append(QByteArray("\xC0\x01\x0A", 3));     // TPDU size 1024
    cotp.append(QByteArray("\xC1\x02\x01\x00", 4)); // local TSAP: PG
    cotp.append(char(0xC2));
    cotp.append(char(0x02));
    cotp.append(char(0x01));                                 // PG connection
    cotp.append(char(((rack & 0x07) << 5) | (slot & 0x1F))); // rack, slot
    return tpkt(cotp);
}

QByteArray setupRequest(quint16 reference, quint16 pdu)
{
    QByteArray parameters("\xF0\x00\x00\x01\x00\x01", 6);
    put16(parameters, pdu);
    return dataFrame(job(reference, parameters));
}

QByteArray readRequest(quint16 reference, const Item& item)
{
    return dataFrame(job(reference, QByteArray("\x04\x01", 2) + itemSpec(item)));
}

QByteArray writeRequest(quint16 reference, const Item& item, const QByteArray& data)
{
    QByteArray payload;
    payload.append(char(0x00));
    const bool bit = item.bit >= 0;
    payload.append(char(bit ? 0x03 : 0x04)); // BIT, or BYTE/WORD/DWORD in bits
    put16(payload, bit ? 1 : int(data.size()) * 8);
    payload += bit ? data.left(1) : data;
    return dataFrame(job(reference, QByteArray("\x05\x01", 2) + itemSpec(item), payload));
}

Result<void> connectConfirm(const QByteArray& frame)
{
    if (frame.size() < 7 || quint8(frame.at(5)) != 0xD0)
        return makeError(ErrorCode::Io,
                         QStringLiteral("the PLC refused the connection (check rack and slot)"));
    return {};
}

Result<int> setupResponse(const QByteArray& frame)
{
    auto s7 = s7Reply(frame);
    if (!s7)
        return tl::unexpected(s7.error());
    if (get16(*s7, 6) < 8 || quint8(s7->at(12)) != 0xF0)
        return makeError(ErrorCode::BadFormat, QStringLiteral("not an S7 setup reply"));
    return get16(*s7, 18);
}

Result<QByteArray> readResponse(const QByteArray& frame)
{
    auto s7 = s7Reply(frame);
    if (!s7)
        return tl::unexpected(s7.error());
    const int parameters = get16(*s7, 6);
    const QByteArray data = s7->mid(12 + parameters);
    if (parameters < 2 || quint8(s7->at(12)) != 0x04 || data.size() < 4)
        return makeError(ErrorCode::BadFormat, QStringLiteral("not an S7 read reply"));
    if (quint8(data.at(0)) != kReturnOk)
        return makeError(ErrorCode::Io, returnText(quint8(data.at(0))));
    const quint8 transport = quint8(data.at(1));
    int length = get16(data, 2);
    if (transport == 0x03 || transport == 0x04 || transport == 0x05)
        length = (length + 7) / 8; // given in bits
    if (data.size() < 4 + length)
        return makeError(ErrorCode::BadFormat, QStringLiteral("a short S7 read reply"));
    return data.mid(4, length);
}

Result<void> writeResponse(const QByteArray& frame)
{
    auto s7 = s7Reply(frame);
    if (!s7)
        return tl::unexpected(s7.error());
    const int parameters = get16(*s7, 6);
    const QByteArray data = s7->mid(12 + parameters);
    if (parameters < 2 || quint8(s7->at(12)) != 0x05 || data.isEmpty())
        return makeError(ErrorCode::BadFormat, QStringLiteral("not an S7 write reply"));
    if (quint8(data.at(0)) != kReturnOk)
        return makeError(ErrorCode::Io, returnText(quint8(data.at(0))));
    return {};
}

} // namespace S7

S7Client::S7Client(QString host, int port, int rack, int slot)
    : m_host(std::move(host))
    , m_port(port)
    , m_rack(rack)
    , m_slot(slot)
{
}

S7Client::~S7Client() = default;

void S7Client::disconnect()
{
    m_socket.reset();
    m_ready = false;
}

Result<QByteArray> S7Client::exchange(const QByteArray& request)
{
    if (auto sent = send(*m_socket, request, kTimeoutMs); !sent)
        return tl::unexpected(sent.error());
    const QDeadlineTimer deadline(kTimeoutMs);
    auto header = readExactly(*m_socket, 4, deadline);
    if (!header)
        return header;
    const int length = get16(*header, 2);
    if (quint8(header->at(0)) != 0x03 || length < 7)
        return makeError(ErrorCode::BadFormat, QStringLiteral("not an ISO-on-TCP frame"));
    auto rest = readExactly(*m_socket, length - 4, deadline);
    if (!rest)
        return rest;
    return *header + *rest;
}

Result<void> S7Client::connectIfNeeded()
{
    if (m_ready)
        return {};
    if (now() < m_retryAt)
        return makeError(ErrorCode::Io, m_lastError);
    const auto fail = [this](const QString& why) -> Result<void>
    {
        disconnect();
        m_lastError = QStringLiteral("No connection to the PLC at %1:%2: %3")
                          .arg(m_host)
                          .arg(m_port)
                          .arg(why);
        m_retryAt = now() + kRetryMs;
        return makeError(ErrorCode::Io, m_lastError);
    };
    if (auto connected = connectSocket(m_socket, m_host, m_port, kTimeoutMs); !connected)
        return fail(connected.error().message);
    auto confirm = exchange(S7::connectRequest(m_rack, m_slot));
    if (!confirm)
        return fail(confirm.error().message);
    if (auto accepted = S7::connectConfirm(*confirm); !accepted)
        return fail(accepted.error().message);
    auto setup = exchange(S7::setupRequest(++m_reference, 480));
    if (!setup)
        return fail(setup.error().message);
    if (auto pdu = S7::setupResponse(*setup); !pdu)
        return fail(pdu.error().message);
    m_ready = true;
    return {};
}

Result<QByteArray> S7Client::read(const S7::Item& item)
{
    if (auto ready = connectIfNeeded(); !ready)
        return tl::unexpected(ready.error());
    auto reply = exchange(S7::readRequest(++m_reference, item));
    if (!reply)
    {
        disconnect();
        return reply;
    }
    return S7::readResponse(*reply);
}

Result<void> S7Client::write(const S7::Item& item, const QByteArray& data)
{
    if (auto ready = connectIfNeeded(); !ready)
        return ready;
    auto reply = exchange(S7::writeRequest(++m_reference, item, data));
    if (!reply)
    {
        disconnect();
        return tl::unexpected(reply.error());
    }
    return S7::writeResponse(*reply);
}

namespace Modbus
{

Result<Address> parseAddress(const QString& text)
{
    static const QRegularExpression pattern(QStringLiteral("^(co|di|hr|ir)(\\d+)$"));
    const auto match = pattern.match(text.trimmed().toLower());
    bool ok = false;
    const int offset = match.captured(2).toInt(&ok);
    if (!match.hasMatch() || !ok || offset > 65535)
        return makeError(ErrorCode::InvalidArgument,
                         QStringLiteral("Use co0, di0, hr0 or ir0 (zero based)"));
    const QString kind = match.captured(1);
    const Table table = kind == u"co"   ? Coils
                        : kind == u"di" ? DiscreteInputs
                        : kind == u"hr" ? HoldingRegisters
                                        : InputRegisters;
    return Address{table, offset};
}

QByteArray frame(quint16 transaction, quint8 unit, const QByteArray& pdu)
{
    QByteArray bytes;
    put16(bytes, transaction);
    put16(bytes, 0); // protocol
    put16(bytes, int(pdu.size()) + 1);
    bytes.append(char(unit));
    return bytes + pdu;
}

} // namespace Modbus

ModbusClient::ModbusClient(QString host, int port, int unit)
    : m_host(std::move(host))
    , m_port(port)
    , m_unit(unit)
{
}

ModbusClient::~ModbusClient() = default;

void ModbusClient::disconnect()
{
    m_socket.reset();
}

Result<void> ModbusClient::connectIfNeeded()
{
    if (m_socket)
        return {};
    if (now() < m_retryAt)
        return makeError(ErrorCode::Io, m_lastError);
    if (auto connected = connectSocket(m_socket, m_host, m_port, kTimeoutMs); !connected)
    {
        m_lastError = QStringLiteral("No connection to the Modbus device at %1:%2: %3")
                          .arg(m_host)
                          .arg(m_port)
                          .arg(connected.error().message);
        m_retryAt = now() + kRetryMs;
        return makeError(ErrorCode::Io, m_lastError);
    }
    return {};
}

Result<QByteArray> ModbusClient::exchange(const QByteArray& pdu)
{
    if (auto ready = connectIfNeeded(); !ready)
        return tl::unexpected(ready.error());
    const quint16 transaction = ++m_transaction;
    if (auto sent = send(*m_socket, Modbus::frame(transaction, quint8(m_unit), pdu), kTimeoutMs);
        !sent)
    {
        disconnect();
        return tl::unexpected(sent.error());
    }
    const QDeadlineTimer deadline(kTimeoutMs);
    auto header = readExactly(*m_socket, 7, deadline);
    if (!header)
    {
        disconnect();
        return header;
    }
    const int length = get16(*header, 4);
    if (get16(*header, 0) != transaction || get16(*header, 2) != 0 || length < 2)
    {
        disconnect();
        return makeError(ErrorCode::BadFormat, QStringLiteral("not a Modbus TCP reply"));
    }
    auto reply = readExactly(*m_socket, length - 1, deadline);
    if (!reply)
    {
        disconnect();
        return reply;
    }
    if (quint8(reply->at(0)) & 0x80)
        return makeError(ErrorCode::Io,
                         QStringLiteral("the device refused the request (exception %1)")
                             .arg(reply->size() > 1 ? quint8(reply->at(1)) : 0));
    if (reply->at(0) != pdu.at(0))
        return makeError(ErrorCode::BadFormat, QStringLiteral("an unexpected Modbus reply"));
    return reply;
}

Result<QByteArray> ModbusClient::read(Modbus::Table table, int offset, int count)
{
    QByteArray pdu;
    pdu.append(char(int(table) + 1)); // FC1..FC4
    put16(pdu, offset);
    put16(pdu, count);
    auto reply = exchange(pdu);
    if (!reply)
        return reply;
    const int bytes = reply->size() > 1 ? quint8(reply->at(1)) : 0;
    if (reply->size() < 2 + bytes)
        return makeError(ErrorCode::BadFormat, QStringLiteral("a short Modbus reply"));
    const QByteArray data = reply->mid(2, bytes);
    if (table == Modbus::Coils || table == Modbus::DiscreteInputs)
    {
        QByteArray bits;
        for (int i = 0; i < count; ++i)
            bits.append(char((quint8(data.at(i / 8)) >> (i % 8)) & 1));
        return bits;
    }
    if (data.size() < count * 2)
        return makeError(ErrorCode::BadFormat, QStringLiteral("a short Modbus reply"));
    return data;
}

Result<void> ModbusClient::writeCoil(int offset, bool value)
{
    QByteArray pdu;
    pdu.append(char(0x05));
    put16(pdu, offset);
    put16(pdu, value ? 0xFF00 : 0x0000);
    auto reply = exchange(pdu);
    return reply ? Result<void>() : tl::unexpected(reply.error());
}

Result<void> ModbusClient::writeRegisters(int offset, const QByteArray& registers)
{
    QByteArray pdu;
    if (registers.size() == 2)
    {
        pdu.append(char(0x06));
        put16(pdu, offset);
        pdu += registers;
    }
    else
    {
        pdu.append(char(0x10));
        put16(pdu, offset);
        put16(pdu, int(registers.size() / 2));
        pdu.append(char(registers.size()));
        pdu += registers;
    }
    auto reply = exchange(pdu);
    return reply ? Result<void>() : tl::unexpected(reply.error());
}

} // namespace hmi
