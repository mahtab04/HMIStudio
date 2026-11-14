#include "core/model/ScreenBuilder.h"

#include "core/model/HmiObject.h"
#include "core/property/Prop.h"
#include "core/registry/ObjectRegistry.h"

namespace hmi
{

ScreenBuilder::ScreenBuilder(QString name)
    : m_screen(std::make_unique<Screen>(std::move(name)))
{
}

ScreenBuilder::~ScreenBuilder() = default;
ScreenBuilder::ScreenBuilder(ScreenBuilder&&) noexcept = default;
ScreenBuilder& ScreenBuilder::operator=(ScreenBuilder&&) noexcept = default;

ScreenBuilder& ScreenBuilder::size(QSizeF size)
{
    Q_ASSERT(m_screen);
    [[maybe_unused]] const auto ok = m_screen->setSize(size);
    Q_ASSERT_X(ok, "ScreenBuilder::size", "invalid screen size");
    return *this;
}

ScreenBuilder& ScreenBuilder::background(const QColor& color)
{
    Q_ASSERT(m_screen);
    [[maybe_unused]] const auto ok = m_screen->setBackgroundColor(color);
    Q_ASSERT_X(ok, "ScreenBuilder::background", "invalid colour");
    return *this;
}

ScreenBuilder& ScreenBuilder::add(ObjectTypeId type, const QRectF& geometry)
{
    Q_ASSERT(m_screen);
    std::unique_ptr<HmiObject> object = ObjectRegistry::instance().create(type);
    Q_ASSERT_X(object, "ScreenBuilder::add", "type is not registered");

    for (const auto& [id, number] :
         {std::pair{Prop::X, geometry.x()}, std::pair{Prop::Y, geometry.y()},
          std::pair{Prop::Width, geometry.width()}, std::pair{Prop::Height, geometry.height()}})
    {
        [[maybe_unused]] const auto ok = object->setProperty(id, number);
        Q_ASSERT_X(ok, "ScreenBuilder::add", "geometry rejected");
    }

    auto inserted = m_screen->insertObject(std::move(object));
    Q_ASSERT(inserted);
    m_last = *inserted;
    return *this;
}

ScreenBuilder& ScreenBuilder::with(PropertyId id, const PropertyValue& value)
{
    Q_ASSERT_X(m_last, "ScreenBuilder::with", "no object added yet");
    [[maybe_unused]] const auto ok = m_last->setProperty(id, value);
    Q_ASSERT_X(ok, "ScreenBuilder::with", "property rejected");
    return *this;
}

ObjectId ScreenBuilder::lastId() const
{
    Q_ASSERT(m_last);
    return m_last->id();
}

std::unique_ptr<Screen> ScreenBuilder::build()
{
    m_last = nullptr;
    return std::move(m_screen);
}

} // namespace hmi
