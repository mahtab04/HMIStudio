#pragma once

#include "comm/IDriver.h"
#include "core/tags/Connection.h"

#include <QStringList>

#include <memory>
#include <optional>
#include <vector>

namespace hmi
{

// Reads and writes PLC tags through the optional Snap7 (Siemens S7) and
// libplctag (Modbus TCP, Allen-Bradley EtherNet/IP) shared libraries. All
// native handles, calls, retries and destruction stay on the worker thread.
// A write is sent at once and the tag is read back on the next poll.
//
// A connection may name its native library by path. That path comes from the
// project file, which may come from anyone, and loading a library runs its
// code; so a named library is loaded only when it is in `trustedLibraries`
// (paths the user chose on this computer). The library beside the
// application is always allowed.
class PlcDriver : public IDriver
{
public:
    explicit PlcDriver(std::vector<Connection> connections, QStringList trustedLibraries = {});
    // Whether `path` is one of `trusted`, compared as absolute, clean paths
    // (case-insensitively on Windows).
    [[nodiscard]] static bool isTrustedLibrary(const QString& path, const QStringList& trusted);
    ~PlcDriver() override;
    PlcDriver(const PlcDriver&) = delete;
    PlcDriver& operator=(const PlcDriver&) = delete;
    PlcDriver(PlcDriver&&) = delete;
    PlcDriver& operator=(PlcDriver&&) = delete;

    Result<void> start(std::span<const Tag> tags) override;
    void stop() override;
    Result<void> write(const QString& tag, const TagValue& value) override;
    void poll(double seconds, const Publish& publish) override;
    [[nodiscard]] QString problem(const QString& tag) const override;

    // Shared by configuration validation and the native adapter.
    [[nodiscard]] static Result<void> validateAddress(const Connection& connection, const Tag& tag);
    [[nodiscard]] static Result<QString> attributes(const Connection& connection, const Tag& tag);
    // The HMI data type an address implies: DB1.DBX0.0, M0.0, co0 a Bool;
    // DB1.DBW2, MW10, hr0 an Int; DB1.DBD4, MD20 a Real (an Int may be
    // chosen instead). Nothing for symbolic (Allen-Bradley) addresses.
    [[nodiscard]] static std::optional<Tag::Type> typeForAddress(const QString& driver,
                                                                 const QString& address);
    // Address examples for a driver, for hints.
    [[nodiscard]] static QString addressExamples(const QString& driver);

private:
    class State;
    std::unique_ptr<State> m_state;
    std::vector<Connection> m_connections;
    QStringList m_trustedLibraries;
};

} // namespace hmi
