#include "comm/RuntimeDriver.h"

#include <vector>

namespace hmi
{

RuntimeDriver::RuntimeDriver(std::unique_ptr<IDriver> local, std::unique_ptr<IDriver> plc)
    : m_local(std::move(local))
    , m_plc(std::move(plc))
{
    Q_ASSERT(m_local && m_plc);
}

bool RuntimeDriver::simulates(const Tag& tag) const
{
    return tag.connection.isEmpty() && m_local->simulates(tag);
}

Result<void> RuntimeDriver::start(std::span<const Tag> tags)
{
    std::vector<Tag> local;
    std::vector<Tag> plc;
    m_plcTags.clear();
    for (const Tag& tag : tags)
    {
        if (tag.connection.isEmpty())
            local.push_back(tag);
        else
        {
            plc.push_back(tag);
            m_plcTags.insert(tag.name.toLower());
        }
    }
    if (auto started = m_local->start(local); !started)
        return started;
    return m_plc->start(plc);
}

void RuntimeDriver::stop()
{
    m_local->stop();
    m_plc->stop();
    m_plcTags.clear();
}

Result<void> RuntimeDriver::write(const QString& tag, const TagValue& value)
{
    return m_plcTags.contains(tag.toLower()) ? m_plc->write(tag, value)
                                             : m_local->write(tag, value);
}

void RuntimeDriver::poll(double seconds, const Publish& publish)
{
    m_local->poll(seconds, publish);
    m_plc->poll(seconds, publish);
}

QString RuntimeDriver::problem(const QString& tag) const
{
    return m_plcTags.contains(tag.toLower()) ? m_plc->problem(tag) : m_local->problem(tag);
}

} // namespace hmi
