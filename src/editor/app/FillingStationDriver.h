#pragma once

#include "comm/SimulatorDriver.h"

namespace hmi
{
// Deterministic training process for the sample project, chosen by the
// project's simulationProfile "filling-station". It belongs to the sample,
// so it lives with it in the editor app rather than in hmi_comm. Commands, feedback
// and measurements are separate tags. This driver never connects to a PLC.
class FillingStationDriver final : public IDriver
{
public:
    [[nodiscard]] bool isSimulation() const noexcept override { return true; }
    Result<void> start(std::span<const Tag> tags) override;
    void stop() override;
    Result<void> write(const QString& tag, const TagValue& value) override;
    void poll(double seconds, const Publish& publish) override;

private:
    SimulatorDriver m_tags;
    QHash<QString, TagValue> m_values;
    QHash<QString, TagValue> m_published;
    double m_level = 35.0;
    double m_flow = 0.0;
    double m_seconds = 0.0;
    bool m_autoFilling = true;
};
} // namespace hmi
