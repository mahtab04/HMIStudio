#pragma once

#include "comm/IDriver.h"
#include "core/tags/Connection.h"

#include <QStringList>

#include <memory>
#include <vector>

namespace hmi
{

// Read-only monitoring through optional Snap7 and libplctag shared libraries.
// All native handles, calls, retries and destruction stay on the worker thread.
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
    [[nodiscard]] bool isReadOnly() const noexcept override { return true; }
    Result<void> start(std::span<const Tag> tags) override;
    void stop() override;
    Result<void> write(const QString& tag, const TagValue& value) override;
    void poll(double seconds, const Publish& publish) override;
    [[nodiscard]] QString problem(const QString& tag) const override;

    // Shared by configuration validation and the native adapter.
    [[nodiscard]] static Result<void> validateAddress(const Connection& connection, const Tag& tag);
    [[nodiscard]] static Result<QString> attributes(const Connection& connection, const Tag& tag);

private:
    class State;
    std::unique_ptr<State> m_state;
    std::vector<Connection> m_connections;
    QStringList m_trustedLibraries;
};

} // namespace hmi
