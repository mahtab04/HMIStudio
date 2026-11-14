#include "core/property/PropertyDescriptor.h"

#include <QTest>

#include <array>
#include <limits>

using namespace hmi;

namespace
{

constexpr std::array kOptions{
    EnumOption{.value = 0, .key = "off", .displayName = "Off"},
    EnumOption{.value = 2, .key = "on", .displayName = "On"},
};

const PropertyDescriptor kNumber{.id = PropertyId{"level"},
                                 .key = "level",
                                 .displayName = "Level",
                                 .category = "Behaviour",
                                 .defaultValue = 0.0,
                                 .constraint = {.minimum = 0.0, .maximum = 100.0},
                                 .bindable = true};

const PropertyDescriptor kCount{.id = PropertyId{"count"},
                                .key = "count",
                                .displayName = "Count",
                                .category = "Behaviour",
                                .defaultValue = 1,
                                .constraint = {.minimum = 1.0, .maximum = 10.0}};

const PropertyDescriptor kMode{.id = PropertyId{"mode"},
                               .key = "mode",
                               .displayName = "Mode",
                               .category = "Behaviour",
                               .defaultValue = EnumValue{0},
                               .constraint = {.enumOptions = kOptions}};

const PropertyDescriptor kColor{.id = PropertyId{"color"},
                                .key = "color",
                                .displayName = "Colour",
                                .category = "Appearance",
                                .defaultValue = QColor(Qt::red)};

} // namespace

class PropertyDescriptorTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void propertyTypeConcept()
    {
        static_assert(PropertyType<double>);
        static_assert(PropertyType<QColor>);
        static_assert(PropertyType<EnumValue>);
        static_assert(!PropertyType<float>);
        static_assert(!PropertyType<QRectF>);
    }

    void valueKindFollowsVariantOrder()
    {
        QCOMPARE(valueKind(PropertyValue(true)), ValueKind::Bool);
        QCOMPARE(valueKind(PropertyValue(1)), ValueKind::Int);
        QCOMPARE(valueKind(PropertyValue(1.0)), ValueKind::Double);
        QCOMPARE(valueKind(PropertyValue(QString())), ValueKind::String);
        QCOMPARE(valueKind(PropertyValue(QColor())), ValueKind::Color);
        QCOMPARE(valueKind(PropertyValue(QPointF())), ValueKind::Point);
        QCOMPARE(valueKind(PropertyValue(QSizeF())), ValueKind::Size);
        QCOMPARE(valueKind(PropertyValue(QFont())), ValueKind::Font);
        QCOMPARE(valueKind(PropertyValue(TagRef{})), ValueKind::Tag);
        QCOMPARE(valueKind(PropertyValue(EnumValue{})), ValueKind::Enum);
    }

    void acceptsValueInRange()
    {
        const auto result = validate(kNumber, 42.5);
        QVERIFY(result.has_value());
        QCOMPARE(std::get<double>(*result), 42.5);
    }

    void acceptsBoundaries()
    {
        QVERIFY(validate(kNumber, 0.0).has_value());
        QVERIFY(validate(kNumber, 100.0).has_value());
    }

    void widensIntToDouble()
    {
        const auto result = validate(kNumber, 7);
        QVERIFY(result.has_value());
        QVERIFY(std::holds_alternative<double>(*result));
        QCOMPARE(std::get<double>(*result), 7.0);
    }

    void doesNotNarrowDoubleToInt()
    {
        const auto result = validate(kCount, 2.0);
        QVERIFY(!result.has_value());
        QCOMPARE(result.error().code, ErrorCode::TypeMismatch);
    }

    void rejectsWrongType()
    {
        const auto result = validate(kNumber, QStringLiteral("high"));
        QVERIFY(!result.has_value());
        QCOMPARE(result.error().code, ErrorCode::TypeMismatch);
        QVERIFY(result.error().message.contains(u"level"));
    }

    void rejectsOutOfRange_data()
    {
        QTest::addColumn<double>("value");
        QTest::newRow("below") << -0.1;
        QTest::newRow("above") << 100.1;
        QTest::newRow("nan") << std::numeric_limits<double>::quiet_NaN();
        QTest::newRow("infinity") << std::numeric_limits<double>::infinity();
    }

    void rejectsOutOfRange()
    {
        QFETCH(double, value);
        const auto result = validate(kNumber, value);
        QVERIFY(!result.has_value());
        QCOMPARE(result.error().code, ErrorCode::OutOfRange);
    }

    void checksIntRange()
    {
        QVERIFY(validate(kCount, 10).has_value());
        QCOMPARE(validate(kCount, 0).error().code, ErrorCode::OutOfRange);
        QCOMPARE(validate(kCount, 11).error().code, ErrorCode::OutOfRange);
    }

    void checksEnumOptions()
    {
        QVERIFY(validate(kMode, EnumValue{0}).has_value());
        QVERIFY(validate(kMode, EnumValue{2}).has_value());
        QCOMPARE(validate(kMode, EnumValue{1}).error().code, ErrorCode::OutOfRange);
    }

    void rejectsInvalidColour()
    {
        QVERIFY(validate(kColor, QColor(Qt::blue)).has_value());
        QCOMPARE(validate(kColor, QColor()).error().code, ErrorCode::InvalidArgument);
    }

    void findsDescriptorById()
    {
        const std::array table{kNumber, kCount, kMode};
        QCOMPARE(findDescriptor(table, PropertyId{"count"}), &table[1]);
        QVERIFY(findDescriptor(table, PropertyId{"missing"}) == nullptr);
    }
};

QTEST_MAIN(PropertyDescriptorTest)
#include "PropertyDescriptorTest.moc"
