#include "core/objects/IoField.h"

#include "core/model/ObjectVisitor.h"
#include "core/objects/PaintHelpers.h"
#include "core/property/CommonDescriptors.h"
#include "core/property/DescriptorBuilders.h"
#include "core/property/Prop.h"
#include "core/registry/ObjectRegistry.h"

#include <QPainter>

namespace hmi
{

namespace
{

constexpr EnumOption kModeOptions[] = {
    {.value = IoField::Output,
     .key = "output",
     .displayName = QT_TRANSLATE_NOOP("hmi::Property", "Output")},
    {.value = IoField::Input,
     .key = "input",
     .displayName = QT_TRANSLATE_NOOP("hmi::Property", "Input")},
    {.value = IoField::InputOutput,
     .key = "inputOutput",
     .displayName = QT_TRANSLATE_NOOP("hmi::Property", "Input and output")},
};

} // namespace

IoField::IoField(ObjectId id)
    : ElementObject(staticDescriptors(), id)
{
}

std::span<const PropertyDescriptor> IoField::staticDescriptors()
{
    using namespace PropertyCategory;
    static const std::vector<PropertyDescriptor> table = withCommonDescriptors(
        {120.0, 34.0},
        {
            Describe::value(0.0),
            Describe::integer(Prop::Decimals, "decimals",
                              QT_TRANSLATE_NOOP("hmi::Property", "Decimals"), Behaviour, 1, 0, 6),
            Describe::string(Prop::Unit, "unit", QT_TRANSLATE_NOOP("hmi::Property", "Unit"), General,
                             QString()),
            Describe::choice(Prop::IoMode, "ioMode", QT_TRANSLATE_NOOP("hmi::Property", "Mode"),
                             Behaviour, InputOutput, kModeOptions),
            Describe::styled(Describe::fillColor(QColor(Qt::white),
                                                 QT_TRANSLATE_NOOP("hmi::Property", "Background")),
                             StyleRole::Face),
            Describe::styled(Describe::lineColor(QColor(0x9A, 0xA1, 0xAB)), StyleRole::Line),
            Describe::textColor(QColor(0x1F, 0x23, 0x28)),
            Describe::font(14),
        });
    return table;
}

std::unique_ptr<HmiObject> IoField::clone() const
{
    return std::unique_ptr<HmiObject>(new IoField(*this));
}

void IoField::paint(QPainter& painter, const PaintContext& context) const
{
    [[maybe_unused]] const QSizeF size = geometry().size();
    [[maybe_unused]] const QRectF bounds(QPointF(0, 0), size);
    Q_UNUSED(context)
    PaintHelpers::drawFace(painter, bounds, value<QColor>(Prop::FillColor),
                           value<QColor>(Prop::LineColor), 3.0);
    // The value grows with the field.
    const double k = PaintHelpers::contentScale(*this);
    PaintHelpers::drawText(painter, *this, bounds.adjusted(6 * k, 0, -6 * k, 0),
                           Qt::AlignRight | Qt::AlignVCenter, displayText(), k);
}

QString IoField::displayText() const
{
    const QString number =
        QString::number(value<double>(Prop::Value), 'f', value<int>(Prop::Decimals));
    const QString unit = value<QString>(Prop::Unit);
    return unit.isEmpty() ? number : number + u' ' + unit;
}

RuntimeBehavior IoField::runtimeBehavior() const
{
    return value<EnumValue>(Prop::IoMode).value == Output
               ? RuntimeBehavior{}
               : RuntimeBehavior{RuntimeBehavior::Kind::ValueInput, Prop::Value};
}

void IoField::accept(ObjectVisitor& visitor)
{
    visitor.visit(*this);
}

} // namespace hmi

HMI_REGISTER_OBJECT(hmi::IoField, "ioField", QT_TRANSLATE_NOOP("hmi::ObjectRegistry", "I/O field"),
                    hmi::ObjectCategory::Elements, ":/icons/io-field.svg")
