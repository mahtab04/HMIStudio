#include "core/tags/Binding.h"

#include "core/io/ObjectJson.h"
#include "core/tags/Expression.h"

#include <QCoreApplication>
#include <QJsonArray>

#include <cmath>

namespace hmi
{

namespace
{

const auto kProperty = QStringLiteral("property");
const auto kTag = QStringLiteral("tag");
const auto kTransform = QStringLiteral("transform");
const auto kKind = QStringLiteral("kind");

QString number(double value)
{
    return QString::number(value, 'g', 6);
}

// The tag value itself.
class DirectTransform final : public Transform
{
public:
    [[nodiscard]] QString kind() const override { return QStringLiteral("direct"); }
    [[nodiscard]] Result<PropertyValue> apply(const TagValue& value,
                                              const PropertyDescriptor& target) const override
    {
        return toProperty(value, target);
    }
    [[nodiscard]] QJsonObject parameters() const override { return {}; }
    [[nodiscard]] QString summary() const override { return {}; }
    [[nodiscard]] Result<TagValue> inverse(const PropertyValue& value) const override
    {
        if (const auto* flag = std::get_if<bool>(&value))
            return TagValue(*flag);
        if (const auto* integer = std::get_if<int>(&value))
            return TagValue(qint64(*integer));
        if (const auto* number = std::get_if<double>(&value))
            return TagValue(*number);
        if (const auto* text = std::get_if<QString>(&value))
            return TagValue(*text);
        return Transform::inverse(value);
    }
};

// Maps [inMin, inMax] linearly onto [outMin, outMax]; values outside are
// extended, not clamped (a scale, not a limit).
class LinearTransform final : public Transform
{
public:
    LinearTransform(double inMin, double inMax, double outMin, double outMax)
        : m_inMin(inMin)
        , m_inMax(inMax)
        , m_outMin(outMin)
        , m_outMax(outMax)
    {
    }

    [[nodiscard]] QString kind() const override { return QStringLiteral("linear"); }
    [[nodiscard]] Result<PropertyValue> apply(const TagValue& value,
                                              const PropertyDescriptor& target) const override
    {
        const auto x = TagValues::toNumber(value);
        if (!x)
            return makeError(ErrorCode::TypeMismatch, QStringLiteral("Not a number"));
        const double span = m_inMax - m_inMin;
        const double t = span == 0.0 ? 0.0 : (*x - m_inMin) / span;
        return toProperty(m_outMin + t * (m_outMax - m_outMin), target);
    }
    [[nodiscard]] QJsonObject parameters() const override
    {
        return {{QStringLiteral("inMin"), m_inMin},
                {QStringLiteral("inMax"), m_inMax},
                {QStringLiteral("outMin"), m_outMin},
                {QStringLiteral("outMax"), m_outMax}};
    }
    [[nodiscard]] QString summary() const override
    {
        return QStringLiteral("%1–%2 → %3–%4")
            .arg(number(m_inMin), number(m_inMax), number(m_outMin), number(m_outMax));
    }

private:
    double m_inMin;
    double m_inMax;
    double m_outMin;
    double m_outMax;

public:
    [[nodiscard]] Result<TagValue> inverse(const PropertyValue& value) const override
    {
        const auto direct = Binding::direct()->inverse(value);
        const auto number = direct ? TagValues::toNumber(*direct) : std::nullopt;
        if (!number || m_outMax == m_outMin || m_inMax == m_inMin)
            return Transform::inverse(value);
        const double result =
            m_inMin + (*number - m_outMin) * (m_inMax - m_inMin) / (m_outMax - m_outMin);
        if (!std::isfinite(result))
            return Transform::inverse(value);
        return TagValue(result);
    }
};

// Value ranges to fixed property values (colours, visibility, texts):
// the first range with from <= x < to wins, else the default.
class RangesTransform final : public Transform
{
public:
    struct Range
    {
        double from;
        double to;
        QJsonValue value;
    };

    RangesTransform(std::vector<Range> ranges, QJsonValue fallback)
        : m_ranges(std::move(ranges))
        , m_fallback(std::move(fallback))
    {
    }

    [[nodiscard]] QString kind() const override { return QStringLiteral("ranges"); }
    [[nodiscard]] Result<PropertyValue> apply(const TagValue& value,
                                              const PropertyDescriptor& target) const override
    {
        const auto x = TagValues::toNumber(value);
        const QJsonValue* chosen = &m_fallback;
        if (x)
        {
            for (const Range& range : m_ranges)
            {
                if (*x >= range.from && *x < range.to)
                {
                    chosen = &range.value;
                    break;
                }
            }
        }
        if (chosen->isUndefined() || chosen->isNull())
            return makeError(ErrorCode::OutOfRange, QStringLiteral("No range matches"));
        return ObjectJson::readValue(target, *chosen);
    }
    [[nodiscard]] QJsonObject parameters() const override
    {
        QJsonArray ranges;
        for (const Range& r : m_ranges)
            ranges.append(QJsonObject{{QStringLiteral("from"), r.from},
                                      {QStringLiteral("to"), r.to},
                                      {QStringLiteral("value"), r.value}});
        QJsonObject json{{QStringLiteral("ranges"), ranges}};
        if (!m_fallback.isUndefined() && !m_fallback.isNull())
            json.insert(QStringLiteral("default"), m_fallback);
        return json;
    }
    [[nodiscard]] QString summary() const override
    {
        return QCoreApplication::translate("hmi::Binding", "%n range(s)", nullptr,
                                           int(m_ranges.size()));
    }

private:
    std::vector<Range> m_ranges;
    QJsonValue m_fallback;
};

class ExpressionTransform final : public Transform
{
public:
    explicit ExpressionTransform(Expression expression)
        : m_expression(std::move(expression))
    {
    }

    [[nodiscard]] QString kind() const override { return QStringLiteral("expression"); }
    [[nodiscard]] Result<PropertyValue> apply(const TagValue& value,
                                              const PropertyDescriptor& target) const override
    {
        const auto x = TagValues::toNumber(value);
        if (!x)
            return makeError(ErrorCode::TypeMismatch, QStringLiteral("Not a number"));
        const double result = m_expression.evaluate(*x);
        if (!std::isfinite(result))
            return makeError(ErrorCode::OutOfRange,
                             QStringLiteral("The expression gave no number"));
        return toProperty(result, target);
    }
    [[nodiscard]] QJsonObject parameters() const override
    {
        return {{QStringLiteral("text"), m_expression.text()}};
    }
    [[nodiscard]] QString summary() const override { return m_expression.text(); }

private:
    Expression m_expression;
};

[[maybe_unused]] const bool kRegistered = []
{
    TransformRegistry& r = TransformRegistry::instance();
    r.add(QStringLiteral("direct"),
          [](const QJsonObject&) -> Result<TransformPtr> { return Binding::direct(); });
    r.add(QStringLiteral("linear"),
          [](const QJsonObject& p) -> Result<TransformPtr>
          {
              const double inMin = p.value(u"inMin").toDouble(0);
              const double inMax = p.value(u"inMax").toDouble(100);
              const double outMin = p.value(u"outMin").toDouble(0);
              const double outMax = p.value(u"outMax").toDouble(100);
              if (!std::isfinite(inMin) || !std::isfinite(inMax) || !std::isfinite(outMin) ||
                  !std::isfinite(outMax))
                  return makeError(ErrorCode::InvalidArgument, QStringLiteral("Invalid scale"));
              return TransformPtr(std::make_shared<LinearTransform>(inMin, inMax, outMin, outMax));
          });
    r.add(QStringLiteral("ranges"),
          [](const QJsonObject& p) -> Result<TransformPtr>
          {
              std::vector<RangesTransform::Range> ranges;
              for (const QJsonValue& entry : p.value(u"ranges").toArray())
              {
                  const QJsonObject o = entry.toObject();
                  const double from = o.value(u"from").toDouble(-HUGE_VAL);
                  const double to = o.value(u"to").toDouble(HUGE_VAL);
                  if (!(from < to))
                      return makeError(ErrorCode::InvalidArgument,
                                       QStringLiteral("A range must have 'from' below 'to'"));
                  ranges.push_back({from, to, o.value(u"value")});
              }
              return TransformPtr(
                  std::make_shared<RangesTransform>(std::move(ranges), p.value(u"default")));
          });
    r.add(QStringLiteral("expression"),
          [](const QJsonObject& p) -> Result<TransformPtr>
          {
              auto expression = Expression::parse(p.value(u"text").toString());
              if (!expression)
                  return tl::unexpected(expression.error());
              return TransformPtr(std::make_shared<ExpressionTransform>(std::move(*expression)));
          });
    return true;
}();

} // namespace

Result<TagValue> Transform::inverse(const PropertyValue&) const
{
    return makeError(ErrorCode::InvalidArgument,
                     QCoreApplication::translate("hmi::Binding",
                                                 "This transform is display only. Use a direct "
                                                 "binding or a non-zero linear scale for input."));
}

Result<PropertyValue> Transform::toProperty(const TagValue& value, const PropertyDescriptor& target)
{
    std::optional<PropertyValue> converted;
    switch (valueKind(target.defaultValue))
    {
    case ValueKind::Bool:
        converted = TagValues::toBool(value);
        break;
    case ValueKind::Int:
        if (const auto x = TagValues::toNumber(value))
            converted = int(std::lround(*x));
        break;
    case ValueKind::Double:
        if (const auto x = TagValues::toNumber(value))
            converted = *x;
        break;
    case ValueKind::String:
        converted = TagValues::toText(value);
        break;
    case ValueKind::Color:
        if (const QColor color = QColor::fromString(TagValues::toText(value)); color.isValid())
            converted = color;
        break;
    case ValueKind::Enum:
        if (const auto x = TagValues::toNumber(value))
            converted = EnumValue{int(std::lround(*x))};
        break;
    case ValueKind::Tag:
        converted = TagRef{TagValues::toText(value)};
        break;
    case ValueKind::Point:
    case ValueKind::Size:
    case ValueKind::Font:
    case ValueKind::Points:
        break;
    }
    if (!converted)
        return makeError(ErrorCode::TypeMismatch,
                         QStringLiteral("'%1' cannot drive %2")
                             .arg(TagValues::toText(value), QLatin1StringView(target.key)));
    return validate(target, *converted);
}

TransformRegistry& TransformRegistry::instance()
{
    static TransformRegistry registry;
    return registry;
}

bool TransformRegistry::add(const QString& kind, Factory factory)
{
    return m_factories.emplace(kind, std::move(factory)).second;
}

Result<TransformPtr> TransformRegistry::create(const QString& kind,
                                               const QJsonObject& parameters) const
{
    const auto it = m_factories.find(kind);
    if (it == m_factories.end())
        return makeError(ErrorCode::UnknownType,
                         QStringLiteral("Unknown transform '%1'").arg(kind));
    return it->second(parameters);
}

QStringList TransformRegistry::kinds() const
{
    QStringList list;
    for (const auto& entry : m_factories)
        list << entry.first;
    return list;
}

TransformPtr Binding::direct()
{
    static const TransformPtr transform = std::make_shared<DirectTransform>();
    return transform;
}

QJsonObject Binding::toJson(const PropertyDescriptor& descriptor) const
{
    QJsonObject json{
        {kProperty, QLatin1StringView(descriptor.key)},
        {kTag, tag},
    };
    if (transform && transform->kind() != u"direct")
    {
        QJsonObject t = transform->parameters();
        t.insert(kKind, transform->kind());
        json.insert(kTransform, t);
    }
    return json;
}

Result<Binding> Binding::fromJson(const QJsonObject& json,
                                  std::span<const PropertyDescriptor> descriptors)
{
    const QString key = json.value(kProperty).toString();
    const auto it = std::ranges::find_if(descriptors, [&key](const PropertyDescriptor& d)
                                         { return key == QLatin1StringView(d.key); });
    if (it == descriptors.end() || !it->bindable)
        return makeError(ErrorCode::UnknownProperty,
                         QStringLiteral("'%1' is not a bindable property").arg(key));
    const QString tag = json.value(kTag).toString();
    if (!Tag::isValidName(tag))
        return makeError(ErrorCode::InvalidArgument, QStringLiteral("Binding without a tag"));
    TransformPtr transform = direct();
    if (json.contains(kTransform))
    {
        const QJsonObject t = json.value(kTransform).toObject();
        auto created = TransformRegistry::instance().create(t.value(kKind).toString(), t);
        if (!created)
            return tl::unexpected(created.error());
        transform = *created;
    }
    return Binding{it->id, tag, transform};
}

bool operator==(const Binding& a, const Binding& b)
{
    const auto signature = [](const Binding& x)
    {
        return x.transform ? std::pair(x.transform->kind(), x.transform->parameters())
                           : std::pair(QString(), QJsonObject());
    };
    return a.property == b.property && a.tag == b.tag && signature(a) == signature(b);
}

} // namespace hmi
