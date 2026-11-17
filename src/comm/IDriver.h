#pragma once

#include "core/Result.h"
#include "core/tags/Tag.h"

#include <QString>

#include <functional>
#include <span>

namespace hmi
{

// A source of tag values, with the simulator and optional native PLC
// adapters exposing their capabilities through one interface. Every
// member is called on the TagEngine's worker thread, never on the GUI
// thread, so a driver may block on I/O.
class IDriver
{
public:
    using Publish = std::function<void(const QString& tag, const TagValue& value)>;

    IDriver() = default;
    virtual ~IDriver() = default;
    IDriver(const IDriver&) = delete;
    IDriver& operator=(const IDriver&) = delete;
    IDriver(IDriver&&) = delete;
    IDriver& operator=(IDriver&&) = delete;

    [[nodiscard]] virtual bool isReadOnly() const noexcept { return false; }
    [[nodiscard]] virtual bool isSimulation() const noexcept { return false; }

    // Starts serving these tags.
    virtual Result<void> start(std::span<const Tag> tags) = 0;
    virtual void stop() = 0;
    // Sets a tag (an operator input, or a value typed in the tags panel).
    virtual Result<void> write(const QString& tag, const TagValue& value) = 0;
    // Called every TagEngine tick with the seconds since start; reports
    // values that changed since the last call.
    virtual void poll(double seconds, const Publish& publish) = 0;
    [[nodiscard]] virtual QString problem(const QString& /*tag*/) const { return {}; }
};

} // namespace hmi
