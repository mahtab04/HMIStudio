#pragma once

#include "core/Result.h"

#include <QObject>
#include <QThread>

namespace hmi
{

// A test PLC on this computer, for trying PLC tags without hardware: an S7
// server (as an S7-1200/1500 with PUT/GET) and a Modbus TCP server on
// 127.0.0.1, in a thread of their own. Its memory starts at zero and keeps
// what the HMI writes:
//   S7: any data block (DB1, DB2, ... 1024 bytes each), markers M (1024
//   bytes), inputs I and outputs Q (128 bytes each).
//   Modbus: 10,000 coils, discrete inputs, holding and input registers.
// A heartbeat changes a few values every second, so reading shows life:
//   S7: MW100 counts seconds, M102.0 toggles.
//   Modbus: ir0 counts seconds, di0 toggles.
class TestPlc : public QObject
{
    Q_OBJECT

public:
    static constexpr quint16 kS7Port = 1102;
    static constexpr quint16 kModbusPort = 1502;

    explicit TestPlc(QObject* parent = nullptr);
    ~TestPlc() override;
    TestPlc(const TestPlc&) = delete;
    TestPlc& operator=(const TestPlc&) = delete;
    TestPlc(TestPlc&&) = delete;
    TestPlc& operator=(TestPlc&&) = delete;

    // Listens on 127.0.0.1; 0 picks a free port (tests).
    Result<void> start(quint16 s7Port = kS7Port, quint16 modbusPort = kModbusPort);
    void stop();
    [[nodiscard]] bool isRunning() const noexcept { return m_server != nullptr; }
    [[nodiscard]] quint16 s7Port() const noexcept { return m_s7Port; }
    [[nodiscard]] quint16 modbusPort() const noexcept { return m_modbusPort; }

Q_SIGNALS:
    void runningChanged();

private:
    class Server;
    QThread m_thread;
    Server* m_server = nullptr; // lives in m_thread
    quint16 m_s7Port = 0;
    quint16 m_modbusPort = 0;
};

} // namespace hmi
