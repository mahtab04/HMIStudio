#pragma once

#include "comm/IDriver.h"

#include <QHash>
#include <QRandomGenerator>

#include <optional>

namespace hmi
{

// Simulates every tag from its simulation settings: ramp, sine, random or
// toggle between minimum and maximum with the tag's period; tags without a
// simulation keep their start value until written. Values follow the time
// passed to poll(), so tests can drive it step by step.
class SimulatorDriver final : public IDriver
{
public:
    // `seed` makes random values repeatable (tests).
    explicit SimulatorDriver(std::optional<quint32> seed = std::nullopt);

    Result<void> start(std::span<const Tag> tags) override;
    void stop() override;
    Result<void> write(const QString& tag, const TagValue& value) override;
    void poll(double seconds, const Publish& publish) override;

    // The simulated value of `tag` at `seconds`, before conversion.
    [[nodiscard]] static TagValue simulate(const Tag& tag, double seconds, double random);

private:
    struct State
    {
        Tag tag;
        TagValue value;
        double random = 0.0;     // current random step
        double nextRandom = 0.0; // when the random step changes
        TagValue written;        // set by write(); stays until the next write
    };

    QHash<QString, State> m_states; // by lower-case name
    QRandomGenerator m_random;
};

} // namespace hmi
