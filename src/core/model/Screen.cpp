#include "core/model/Screen.h"

#include "core/model/DocumentNotifier.h"

#include <algorithm>

namespace hmi
{

Screen::Screen(QString name, QSizeF size, ScreenId id)
    : m_id(id)
    , m_name(std::move(name))
    , m_size(size)
{
    Q_ASSERT(!m_id.isNull());
    Q_ASSERT(size.isValid());
}

Screen::~Screen() = default;

std::unique_ptr<Screen> Screen::clone() const
{
    auto copy = std::make_unique<Screen>(m_name, m_size, m_id);
    copy->m_background = m_background;
    copy->m_objects.reserve(m_objects.size());
    for (const auto& object : m_objects)
        copy->m_objects.push_back(object->clone());
    return copy;
}

void Screen::setName(const QString& name)
{
    if (name == m_name)
        return;
    m_name = name;
    notifyChanged();
}

Result<void> Screen::setSize(QSizeF size)
{
    if (!size.isValid() || size.isEmpty())
    {
        return makeError(ErrorCode::OutOfRange,
                         QStringLiteral("Screen size must be positive, got %1 x %2")
                             .arg(size.width())
                             .arg(size.height()));
    }
    if (size == m_size)
        return {};
    m_size = size;
    notifyChanged();
    return {};
}

Result<void> Screen::setBackgroundColor(const QColor& color)
{
    if (!color.isValid())
        return makeError(ErrorCode::InvalidArgument, QStringLiteral("Invalid background colour"));
    if (color == m_background)
        return {};
    m_background = color;
    notifyChanged();
    return {};
}

HmiObject* Screen::object(ObjectId id) const noexcept
{
    const auto index = indexOf(id);
    return index ? m_objects[*index].get() : nullptr;
}

std::optional<std::size_t> Screen::indexOf(ObjectId id) const noexcept
{
    const auto it =
        std::ranges::find_if(m_objects, [id](const auto& object) { return object->id() == id; });
    if (it == m_objects.end())
        return std::nullopt;
    return std::size_t(it - m_objects.begin());
}

Result<HmiObject*> Screen::insertObject(std::unique_ptr<HmiObject> object,
                                        std::optional<std::size_t> index)
{
    if (!object)
        return makeError(ErrorCode::InvalidArgument, QStringLiteral("Cannot insert a null object"));
    if (indexOf(object->id()))
    {
        return makeError(ErrorCode::Duplicate, QStringLiteral("Object %1 is already on screen '%2'")
                                                   .arg(object->id().toString(), m_name));
    }
    const std::size_t at = index.value_or(m_objects.size());
    if (at > m_objects.size())
    {
        return makeError(ErrorCode::OutOfRange,
                         QStringLiteral("Insert index %1 is past the end (%2 objects)")
                             .arg(at)
                             .arg(m_objects.size()));
    }

    HmiObject* raw = object.get();
    raw->setNotifier(m_notifier);
    m_objects.insert(m_objects.begin() + std::ptrdiff_t(at), std::move(object));
    if (m_notifier)
        Q_EMIT m_notifier->objectAdded(m_id, raw->id(), int(at));
    return raw;
}

Result<std::unique_ptr<HmiObject>> Screen::takeObject(ObjectId id)
{
    const auto index = indexOf(id);
    if (!index)
    {
        return makeError(
            ErrorCode::NotFound,
            QStringLiteral("Object %1 is not on screen '%2'").arg(id.toString(), m_name));
    }
    std::unique_ptr<HmiObject> object = std::move(m_objects[*index]);
    m_objects.erase(m_objects.begin() + std::ptrdiff_t(*index));
    object->setNotifier(nullptr);
    if (m_notifier)
        Q_EMIT m_notifier->objectRemoved(m_id, id);
    return object;
}

Result<void> Screen::moveObject(ObjectId id, std::size_t newIndex)
{
    const auto index = indexOf(id);
    if (!index)
    {
        return makeError(
            ErrorCode::NotFound,
            QStringLiteral("Object %1 is not on screen '%2'").arg(id.toString(), m_name));
    }
    if (newIndex >= m_objects.size())
    {
        return makeError(ErrorCode::OutOfRange,
                         QStringLiteral("Z-index %1 is past the end (%2 objects)")
                             .arg(newIndex)
                             .arg(m_objects.size()));
    }
    if (newIndex == *index)
        return {};

    const auto from = m_objects.begin() + std::ptrdiff_t(*index);
    const auto to = m_objects.begin() + std::ptrdiff_t(newIndex);
    if (newIndex < *index)
        std::rotate(to, from, from + 1);
    else
        std::rotate(from, from + 1, to + 1);

    if (m_notifier)
        Q_EMIT m_notifier->objectsReordered(m_id);
    return {};
}

void Screen::setNotifier(DocumentNotifier* notifier) noexcept
{
    m_notifier = notifier;
    for (const auto& object : m_objects)
        object->setNotifier(notifier);
}

void Screen::notifyChanged()
{
    if (m_notifier)
        Q_EMIT m_notifier->screenChanged(m_id);
}

} // namespace hmi
