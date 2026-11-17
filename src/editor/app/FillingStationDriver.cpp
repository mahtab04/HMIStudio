#include "editor/app/FillingStationDriver.h"

#include <algorithm>
#include <cmath>

namespace hmi
{
Result<void> FillingStationDriver::start(std::span<const Tag> tags)
{
    stop();
    for (const Tag& tag : tags)
        m_values.insert(tag.name.toLower(), tag.startValue());
    m_level = TagValues::toNumber(m_values.value(QStringLiteral("level"))).value_or(35.0);
    return m_tags.start(tags);
}

void FillingStationDriver::stop()
{
    m_tags.stop();
    m_values.clear();
    m_published.clear();
    m_level = 35.0;
    m_flow = 0.0;
    m_seconds = 0.0;
    m_autoFilling = true;
}

Result<void> FillingStationDriver::write(const QString& tag, const TagValue& value)
{
    auto result = m_tags.write(tag, value);
    if (result)
        m_values.insert(tag.toLower(), value);
    return result;
}

void FillingStationDriver::poll(double seconds, const Publish& publish)
{
    m_tags.poll(seconds, [this](const QString& tag, const TagValue& value)
                { m_values.insert(tag.toLower(), value); });
    // Bound the integration step after a suspended application.
    const double dt = std::clamp(seconds - m_seconds, 0.0, 0.5);
    m_seconds = seconds;
    const bool automatic = TagValues::toBool(m_values.value(QStringLiteral("automode")));
    if (m_level >= 80.0)
        m_autoFilling = false;
    else if (m_level <= 30.0)
        m_autoFilling = true;
    const bool command = TagValues::toBool(m_values.value(QStringLiteral("runcommand")));
    const bool jog = TagValues::toBool(m_values.value(QStringLiteral("jog")));
    const bool high = m_level >= 90.0;
    const bool running = !high && ((command && (!automatic || m_autoFilling)) || jog);
    const double setpoint =
        TagValues::toNumber(m_values.value(QStringLiteral("flowsetpoint"))).value_or(50.0);
    const double target = running ? std::clamp(setpoint, 0.0, 100.0) : 0.0;
    m_flow += (target - m_flow) * std::min(1.0, dt * 3.0);
    // A transfer pump fills the buffer; a constant downstream demand drains it.
    m_level = std::clamp(m_level + (m_flow * 0.07 - 1.4) * dt, 0.0, 100.0);
    m_values.insert(QStringLiteral("pumprunning"), running);
    m_values.insert(QStringLiteral("level"), m_level);
    m_values.insert(QStringLiteral("flow"), m_flow);
    m_values.insert(QStringLiteral("pressure"), 0.4 + m_flow * 0.035);
    m_values.insert(QStringLiteral("temperature"), 22.0 + 0.5 * std::sin(seconds / 20.0));
    m_values.insert(QStringLiteral("highlevel"), high);
    for (auto it = m_values.cbegin(); it != m_values.cend(); ++it)
    {
        if (!m_published.contains(it.key()) || m_published.value(it.key()) != it.value())
        {
            m_published.insert(it.key(), it.value());
            publish(it.key(), it.value());
        }
    }
}
} // namespace hmi
