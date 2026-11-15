#include "core/io/ObjectJson.h"

#include "core/model/HmiObject.h"
#include "core/registry/ObjectRegistry.h"

#include <cmath>
#include <limits>

namespace hmi::ObjectJson
{

namespace
{

const auto kType = QStringLiteral("type");
const auto kId = QStringLiteral("id");
const auto kProps = QStringLiteral("props");
const auto kChildren = QStringLiteral("children");

QJsonArray pair(double a, double b)
{
    return QJsonArray{a, b};
}

std::optional<std::pair<double, double>> readPair(const QJsonValue& json)
{
    const QJsonArray array = json.toArray();
    if (!json.isArray() || array.size() != 2 || !array[0].isDouble() || !array[1].isDouble())
        return std::nullopt;
    return std::pair(array[0].toDouble(), array[1].toDouble());
}

tl::unexpected<Error> wrongKind(const PropertyDescriptor& descriptor)
{
    return makeError(
        ErrorCode::TypeMismatch,
        QStringLiteral("Property '%1' expects a %2 value")
            .arg(QLatin1StringView(descriptor.key),
                 QLatin1StringView(valueKindName(valueKind(descriptor.defaultValue)))));
}

} // namespace

QJsonValue writeValue(const PropertyDescriptor& descriptor, const PropertyValue& value)
{
    switch (valueKind(value))
    {
    case ValueKind::Bool:
        return std::get<bool>(value);
    case ValueKind::Int:
        return std::get<int>(value);
    case ValueKind::Double:
        return std::get<double>(value);
    case ValueKind::String:
        return std::get<QString>(value);
    case ValueKind::Color:
        return std::get<QColor>(value).name(QColor::HexArgb);
    case ValueKind::Point:
    {
        const QPointF p = std::get<QPointF>(value);
        return pair(p.x(), p.y());
    }
    case ValueKind::Size:
    {
        const QSizeF s = std::get<QSizeF>(value);
        return pair(s.width(), s.height());
    }
    case ValueKind::Font:
        return std::get<QFont>(value).toString();
    case ValueKind::Tag:
        return std::get<TagRef>(value).tagName;
    case ValueKind::Enum:
    {
        const int number = std::get<EnumValue>(value).value;
        for (const EnumOption& option : descriptor.constraint.enumOptions)
        {
            if (option.value == number)
                return QString::fromLatin1(option.key);
        }
        return number;
    }
    }
    return {};
}

Result<PropertyValue> readValue(const PropertyDescriptor& descriptor, const QJsonValue& json)
{
    std::optional<PropertyValue> value;
    switch (valueKind(descriptor.defaultValue))
    {
    case ValueKind::Bool:
        if (json.isBool())
            value = json.toBool();
        break;
    case ValueKind::Int:
    {
        const double number = json.toDouble(std::numeric_limits<double>::quiet_NaN());
        if (json.isDouble() && number == std::trunc(number) &&
            number >= std::numeric_limits<int>::min() && number <= std::numeric_limits<int>::max())
            value = int(number);
        break;
    }
    case ValueKind::Double:
        if (json.isDouble())
            value = json.toDouble();
        break;
    case ValueKind::String:
        if (json.isString())
            value = json.toString();
        break;
    case ValueKind::Color:
        if (const QColor color = QColor::fromString(json.toString());
            json.isString() && color.isValid())
            value = color;
        break;
    case ValueKind::Point:
        if (const auto p = readPair(json))
            value = QPointF(p->first, p->second);
        break;
    case ValueKind::Size:
        if (const auto p = readPair(json))
            value = QSizeF(p->first, p->second);
        break;
    case ValueKind::Font:
        if (QFont font; json.isString() && font.fromString(json.toString()))
            value = font;
        break;
    case ValueKind::Tag:
        if (json.isString())
            value = TagRef{json.toString()};
        break;
    case ValueKind::Enum:
        if (json.isString())
        {
            const QString key = json.toString();
            for (const EnumOption& option : descriptor.constraint.enumOptions)
            {
                if (key == QLatin1StringView(option.key))
                    value = EnumValue{option.value};
            }
        }
        else if (json.isDouble())
        {
            value = EnumValue{json.toInt()};
        }
        break;
    }
    if (!value)
        return wrongKind(descriptor);
    return validate(descriptor, *value);
}

QJsonObject write(const HmiObject& object)
{
    const ObjectTypeInfo* info = ObjectRegistry::instance().find(object.typeId());
    Q_ASSERT_X(info, "ObjectJson::write", "object type is not registered");

    QJsonObject props;
    for (const PropertyDescriptor& descriptor : object.descriptors())
    {
        if (const auto value = object.property(descriptor.id))
            props.insert(QLatin1StringView(descriptor.key), writeValue(descriptor, *value));
    }

    QJsonObject json{
        {kType, info ? info->key : QString()},
        {kId, object.id().toString()},
        {kProps, props},
    };
    if (!object.children().empty())
    {
        QJsonArray children;
        for (const auto& child : object.children())
            children.append(write(*child));
        json.insert(kChildren, children);
    }
    return json;
}

Result<std::unique_ptr<HmiObject>> read(const QJsonObject& json)
{
    const QString key = json.value(kType).toString();
    const ObjectTypeInfo* info = ObjectRegistry::instance().findByKey(key);
    if (!info)
        return makeError(ErrorCode::UnknownType,
                         QStringLiteral("Unknown object type '%1'").arg(key));

    const auto id = ObjectId::fromString(json.value(kId).toString());
    std::unique_ptr<HmiObject> object = info->create(id.value_or(ObjectId::create()));

    const QJsonObject props = json.value(kProps).toObject();
    for (const PropertyDescriptor& descriptor : object->descriptors())
    {
        const auto it = props.constFind(QLatin1StringView(descriptor.key));
        if (it == props.constEnd())
            continue;
        Result<PropertyValue> value = readValue(descriptor, *it);
        if (!value)
            return tl::unexpected(value.error());
        if (auto set = object->setProperty(descriptor.id, *value); !set)
            return tl::unexpected(set.error());
    }

    if (json.contains(kChildren))
    {
        auto children = readAll(json.value(kChildren).toArray());
        if (!children)
            return tl::unexpected(children.error());
        if (auto adopted = object->setChildren(std::move(*children)); !adopted)
            return tl::unexpected(adopted.error());
    }
    return object;
}

QJsonArray writeAll(std::span<const HmiObject* const> objects)
{
    QJsonArray array;
    for (const HmiObject* object : objects)
        array.append(write(*object));
    return array;
}

Result<std::vector<std::unique_ptr<HmiObject>>> readAll(const QJsonArray& json)
{
    std::vector<std::unique_ptr<HmiObject>> objects;
    objects.reserve(std::size_t(json.size()));
    for (const QJsonValue& entry : json)
    {
        if (!entry.isObject())
            return makeError(ErrorCode::InvalidArgument,
                             QStringLiteral("Object entry is not a JSON object"));
        auto object = read(entry.toObject());
        if (!object)
            return tl::unexpected(object.error());
        objects.push_back(std::move(*object));
    }
    return objects;
}

} // namespace hmi::ObjectJson
