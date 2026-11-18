#pragma once

#include "core/Result.h"

#include <QByteArray>
#include <QString>

#include <memory>

class QTcpSocket;

namespace hmi
{

// The Siemens S7 protocol as PLCs, HMIs and S7-PLCSIM Advanced speak it over
// TCP port 102: ISO-on-TCP (TPKT, RFC 1006) carrying COTP and S7
// communication jobs. Only what an HMI needs: connect, negotiate the PDU,
// read and write one variable of a data block, the markers, inputs or
// outputs. Shared by the client and the built-in test PLC.
namespace S7
{

enum Area : quint8
{
    Inputs = 0x81,
    Outputs = 0x82,
    Markers = 0x83,
    DataBlock = 0x84,
};

// One variable: `bytes` bytes from `byte`, or one bit when bit >= 0.
struct Item
{
    Area area = DataBlock;
    int db = 0;
    int byte = 0;
    int bit = -1;
    int bytes = 1;
};

inline constexpr quint8 kReturnOk = 0xFF;
inline constexpr quint8 kReturnAddressOutOfRange = 0x05;
inline constexpr quint8 kReturnObjectDoesNotExist = 0x0A;

// A TPKT frame around `body`.
[[nodiscard]] QByteArray tpkt(const QByteArray& body);
// A COTP data frame carrying an S7 message.
[[nodiscard]] QByteArray dataFrame(const QByteArray& s7);
// The PG connection's TSAPs: local 0x0100, remote 0x01 rack*32+slot.
[[nodiscard]] QByteArray connectRequest(int rack, int slot);
[[nodiscard]] QByteArray setupRequest(quint16 reference, quint16 pdu);
[[nodiscard]] QByteArray readRequest(quint16 reference, const Item& item);
[[nodiscard]] QByteArray writeRequest(quint16 reference, const Item& item, const QByteArray& data);

// Checks a reply frame (the whole TPKT frame).
[[nodiscard]] Result<void> connectConfirm(const QByteArray& frame);
// The negotiated PDU size.
[[nodiscard]] Result<int> setupResponse(const QByteArray& frame);
// The bytes read (one byte holding the bit, for a bit).
[[nodiscard]] Result<QByteArray> readResponse(const QByteArray& frame);
[[nodiscard]] Result<void> writeResponse(const QByteArray& frame);

} // namespace S7

// Blocking S7 client for the TagEngine's worker thread: one TCP connection,
// connected on first use; a failed connection waits kRetryMs before the
// next attempt, so an unreachable PLC does not hold up the other tags.
class S7Client
{
public:
    static constexpr int kTimeoutMs = 1000;
    static constexpr int kRetryMs = 2000;

    S7Client(QString host, int port, int rack, int slot);
    ~S7Client();
    S7Client(const S7Client&) = delete;
    S7Client& operator=(const S7Client&) = delete;
    S7Client(S7Client&&) = delete;
    S7Client& operator=(S7Client&&) = delete;

    Result<QByteArray> read(const S7::Item& item);
    Result<void> write(const S7::Item& item, const QByteArray& data);

private:
    Result<void> connectIfNeeded();
    Result<QByteArray> exchange(const QByteArray& request);
    void disconnect();

    QString m_host;
    int m_port;
    int m_rack;
    int m_slot;
    std::unique_ptr<QTcpSocket> m_socket;
    bool m_ready = false;
    quint16 m_reference = 0;
    qint64 m_retryAt = 0; // monotonic ms
    QString m_lastError;
};

// Modbus TCP, as an HMI uses it: read coils (FC1), discrete inputs (FC2),
// holding registers (FC3) and input registers (FC4); write a coil (FC5), a
// register (FC6) or registers (FC16). Shared by the client and the test PLC.
namespace Modbus
{

enum Table
{
    Coils,
    DiscreteInputs,
    HoldingRegisters,
    InputRegisters,
};

// "co0", "di3", "hr10", "ir2": the table and the zero-based offset.
struct Address
{
    Table table = HoldingRegisters;
    int offset = 0;
};
[[nodiscard]] Result<Address> parseAddress(const QString& text);

[[nodiscard]] QByteArray frame(quint16 transaction, quint8 unit, const QByteArray& pdu);

} // namespace Modbus

// Blocking Modbus TCP client for the worker thread, like S7Client.
class ModbusClient
{
public:
    static constexpr int kTimeoutMs = 1000;
    static constexpr int kRetryMs = 2000;

    ModbusClient(QString host, int port, int unit);
    ~ModbusClient();
    ModbusClient(const ModbusClient&) = delete;
    ModbusClient& operator=(const ModbusClient&) = delete;
    ModbusClient(ModbusClient&&) = delete;
    ModbusClient& operator=(ModbusClient&&) = delete;

    // Bits (coils, discrete inputs) as 0/1 bytes; registers big-endian.
    Result<QByteArray> read(Modbus::Table table, int offset, int count);
    Result<void> writeCoil(int offset, bool value);
    Result<void> writeRegisters(int offset, const QByteArray& bigEndianRegisters);

private:
    Result<void> connectIfNeeded();
    Result<QByteArray> exchange(const QByteArray& pdu);
    void disconnect();

    QString m_host;
    int m_port;
    int m_unit;
    std::unique_ptr<QTcpSocket> m_socket;
    quint16 m_transaction = 0;
    qint64 m_retryAt = 0;
    QString m_lastError;
};

} // namespace hmi
