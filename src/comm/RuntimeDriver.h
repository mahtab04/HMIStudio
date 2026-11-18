#pragma once

#include "comm/IDriver.h"

#include <QSet>

#include <memory>

namespace hmi
{

// The runtime's one source of values: internal tags from `local` (the
// simulator, or a training profile) and PLC tags (those with a connection)
// from `plc`, which reads and writes the PLCs. Starting the runtime uses it
// whenever the project has PLC tags, so there is no separate monitor mode.
class RuntimeDriver final : public IDriver
{
public:
    RuntimeDriver(std::unique_ptr<IDriver> local, std::unique_ptr<IDriver> plc);

    [[nodiscard]] bool simulates(const Tag& tag) const override;
    Result<void> start(std::span<const Tag> tags) override;
    void stop() override;
    Result<void> write(const QString& tag, const TagValue& value) override;
    void poll(double seconds, const Publish& publish) override;
    [[nodiscard]] QString problem(const QString& tag) const override;

private:
    std::unique_ptr<IDriver> m_local;
    std::unique_ptr<IDriver> m_plc;
    QSet<QString> m_plcTags; // lower-case names served by m_plc
};

} // namespace hmi
