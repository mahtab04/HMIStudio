#include "comm/SimulatorDriver.h"

#include <cmath>
#include <numbers>

namespace hmi
{

SimulatorDriver::SimulatorDriver(std::optional<quint32> seed)
    : m_random(seed ? QRandomGenerator(*seed) : QRandomGenerator::securelySeeded())
{
}

Result<void> SimulatorDriver::start(std::span<const Tag> tags)
{
    m_states.clear();
    for (const Tag& tag : tags)
    {
        State state{.tag = tag, .value = {}, .random = m_random.generateDouble()};
        m_states.insert(tag.name.toLower(), std::move(state));
    }
    return {};
}

void SimulatorDriver::stop()
{
    m_states.clear();
}

Result<void> SimulatorDriver::write(const QString& tag, const TagValue& value)
{
    const auto it = m_states.find(tag.toLower());
    if (it == m_states.end())
        return makeError(ErrorCode::NotFound, QStringLiteral("No tag called '%1'").arg(tag));
    // Published by the next poll, like any other change.
    it->written = it->tag.converted(value);
    return {};
}

TagValue SimulatorDriver::simulate(const Tag& tag, double seconds, double random)
{
    const double low = tag.minimum;
    const double high = tag.maximum;
    const double period = tag.period > 0.0 ? tag.period : 1.0;
    const double phase = std::fmod(seconds, period) / period; // 0..1
    switch (tag.simulation)
    {
    case Tag::Simulation::None:
        return tag.startValue();
    case Tag::Simulation::Ramp:
        return low + (high - low) * phase;
    case Tag::Simulation::Sine:
        return (low + high) / 2.0 +
               (high - low) / 2.0 * std::sin(2.0 * std::numbers::pi * seconds / period);
    case Tag::Simulation::Random:
        return low + (high - low) * random;
    case Tag::Simulation::Toggle:
    {
        const bool on = phase >= 0.5;
        if (tag.type == Tag::Type::Bool)
            return on;
        return on ? high : low;
    }
    }
    return {};
}

void SimulatorDriver::poll(double seconds, const Publish& publish)
{
    for (State& state : m_states)
    {
        TagValue next;
        const bool written = !std::holds_alternative<std::monostate>(state.written);
        if (state.tag.simulation == Tag::Simulation::None || written)
        {
            next = written ? state.written : state.tag.startValue();
        }
        else
        {
            if (state.tag.simulation == Tag::Simulation::Random && seconds >= state.nextRandom)
            {
                state.random = m_random.generateDouble();
                state.nextRandom = seconds + std::max(0.05, state.tag.period / 10.0);
            }
            next = state.tag.converted(simulate(state.tag, seconds, state.random));
        }
        if (next != state.value)
        {
            state.value = next;
            publish(state.tag.name, next);
        }
    }
}

} // namespace hmi
