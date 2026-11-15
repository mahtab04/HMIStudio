#include "editor/models/PropertyModel.h"

#include "core/graphics/AssetStore.h"
#include "core/model/HmiObject.h"
#include "core/model/Project.h"
#include "core/property/Prop.h"
#include "core/property/PropertyDescriptor.h"
#include "core/registry/ObjectRegistry.h"
#include "editor/commands/ChangeStyleCommand.h"
#include "editor/commands/ScreenCommands.h"
#include "editor/commands/SetPropertyCommand.h"
#include "editor/models/SelectionModel.h"
#include "editor/tools/ToolContext.h"
#include "render/GraphicFactory.h"
#include "render/SvgInspector.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QLocale>
#include <QUndoStack>
#include <QUrl>

#include <algorithm>
#include <cmath>

namespace hmi
{

namespace
{

constexpr double kMaxScreenSide = 10000.0;

QString editorFor(ValueKind kind)
{
    switch (kind)
    {
    case ValueKind::Bool:
        return QStringLiteral("bool");
    case ValueKind::Int:
        return QStringLiteral("integer");
    case ValueKind::Double:
        return QStringLiteral("number");
    case ValueKind::Color:
        return QStringLiteral("color");
    case ValueKind::Point:
        return QStringLiteral("point");
    case ValueKind::Size:
        return QStringLiteral("size");
    case ValueKind::Font:
        return QStringLiteral("font");
    case ValueKind::Enum:
        return QStringLiteral("enum");
    case ValueKind::Points:
        return QStringLiteral("points");
    case ValueKind::String:
    case ValueKind::Tag:
        break;
    }
    return QStringLiteral("text");
}

// A number typed in the panel arrives as a string or a number; accept both,
// in the user's locale or with a decimal point.
std::optional<double> toNumber(const QVariant& value)
{
    if (value.typeId() == QMetaType::QString)
    {
        const QString text = value.toString().trimmed();
        bool ok = false;
        double number = QLocale().toDouble(text, &ok);
        if (!ok)
            number = text.toDouble(&ok);
        return ok && std::isfinite(number) ? std::optional(number) : std::nullopt;
    }
    bool ok = false;
    const double number = value.toDouble(&ok);
    return ok && std::isfinite(number) ? std::optional(number) : std::nullopt;
}

// Points as "x,y x,y …" with a decimal point whatever the locale: commas
// already separate the coordinates.
QString pointsText(const QPolygonF& points)
{
    QStringList parts;
    for (const QPointF& p : points)
        parts << QStringLiteral("%1,%2").arg(p.x(), 0, 'g', 4).arg(p.y(), 0, 'g', 4);
    return parts.join(u' ');
}

std::optional<QPolygonF> parsePoints(const QString& text)
{
    QPolygonF points;
    const QStringList parts = text.split(u' ', Qt::SkipEmptyParts);
    for (const QString& part : parts)
    {
        const QStringList xy = part.split(u',');
        bool okX = false;
        bool okY = false;
        const double x = xy.value(0).toDouble(&okX);
        const double y = xy.value(1).toDouble(&okY);
        if (xy.size() != 2 || !okX || !okY)
            return std::nullopt;
        points.append(QPointF(x, y));
    }
    return points;
}

// Short, locale-aware text for a value, as the panel shows it.
QString displayText(const QVariant& value)
{
    const QLocale locale;
    switch (value.typeId())
    {
    case QMetaType::Double:
    {
        // Up to two decimals, without trailing zeros.
        QString text = locale.toString(value.toDouble(), 'f', 2);
        const QString point = locale.decimalPoint();
        if (text.contains(point))
        {
            while (text.endsWith(u'0'))
                text.chop(1);
            if (text.endsWith(point))
                text.chop(point.size());
        }
        return text;
    }
    case QMetaType::Int:
        return locale.toString(value.toInt());
    case QMetaType::QColor:
    {
        const QColor color = value.value<QColor>();
        return color.name(color.alpha() == 255 ? QColor::HexRgb : QColor::HexArgb).toUpper();
    }
    case QMetaType::QFont:
    {
        const QFont font = value.value<QFont>();
        const int size = font.pixelSize() > 0 ? font.pixelSize() : int(font.pointSizeF());
        QString text = QCoreApplication::translate("hmi::PropertyModel", "%1, %2 px")
                           .arg(font.family())
                           .arg(size);
        if (font.bold())
            text += QCoreApplication::translate("hmi::PropertyModel", ", bold");
        if (font.italic())
            text += QCoreApplication::translate("hmi::PropertyModel", ", italic");
        return text;
    }
    case QMetaType::QPolygonF:
        return pointsText(value.value<QPolygonF>());
    case QMetaType::Bool:
        return value.toBool() ? QCoreApplication::translate("hmi::PropertyModel", "Yes")
                              : QCoreApplication::translate("hmi::PropertyModel", "No");
    default:
        return value.toString();
    }
}

QVariantList components(const QVariant& value)
{
    if (value.typeId() == QMetaType::QPointF)
        return {value.toPointF().x(), value.toPointF().y()};
    if (value.typeId() == QMetaType::QSizeF)
        return {value.toSizeF().width(), value.toSizeF().height()};
    return {};
}

QVariant toVariant(const PropertyDescriptor& descriptor, const PropertyValue& value)
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
        return std::get<QColor>(value);
    case ValueKind::Point:
        return std::get<QPointF>(value);
    case ValueKind::Size:
        return std::get<QSizeF>(value);
    case ValueKind::Font:
        return std::get<QFont>(value);
    case ValueKind::Tag:
        return std::get<TagRef>(value).tagName;
    case ValueKind::Points:
        return QVariant::fromValue(std::get<PointList>(value));
    case ValueKind::Enum:
    {
        const auto options = descriptor.constraint.enumOptions;
        const int number = std::get<EnumValue>(value).value;
        const auto it = std::ranges::find(options, number, &EnumOption::value);
        return it == options.end() ? -1 : int(it - options.begin());
    }
    }
    return {};
}

// Converts what QML sent to the descriptor's value kind, or nullopt.
std::optional<PropertyValue> fromVariant(const PropertyDescriptor& descriptor,
                                         const QVariant& value)
{
    switch (valueKind(descriptor.defaultValue))
    {
    case ValueKind::Bool:
        return PropertyValue(value.toBool());
    case ValueKind::Int:
        if (const auto number = toNumber(value))
            return PropertyValue(int(std::lround(*number)));
        return std::nullopt;
    case ValueKind::Double:
        if (const auto number = toNumber(value))
            return PropertyValue(*number);
        return std::nullopt;
    case ValueKind::String:
        return PropertyValue(value.toString());
    case ValueKind::Tag:
        return PropertyValue(TagRef{value.toString().trimmed()});
    case ValueKind::Color:
    {
        const QColor color = value.typeId() == QMetaType::QColor
                                 ? value.value<QColor>()
                                 : QColor::fromString(value.toString());
        return color.isValid() ? std::optional(PropertyValue(color)) : std::nullopt;
    }
    case ValueKind::Point:
        if (value.canConvert<QPointF>())
            return PropertyValue(value.toPointF());
        return std::nullopt;
    case ValueKind::Size:
        if (value.canConvert<QSizeF>())
            return PropertyValue(value.toSizeF());
        return std::nullopt;
    case ValueKind::Font:
        if (value.typeId() == QMetaType::QFont)
            return PropertyValue(value.value<QFont>());
        return std::nullopt;
    case ValueKind::Points:
    {
        if (value.typeId() == QMetaType::QPolygonF)
            return PropertyValue(value.value<QPolygonF>());
        if (const auto points = parsePoints(value.toString()))
            return PropertyValue(*points);
        return std::nullopt;
    }
    case ValueKind::Enum:
    {
        const auto options = descriptor.constraint.enumOptions;
        const int index = value.toInt();
        if (index < 0 || std::size_t(index) >= options.size())
            return std::nullopt;
        return PropertyValue(EnumValue{options[std::size_t(index)].value});
    }
    }
    return std::nullopt;
}

} // namespace

PropertyModel::PropertyModel(ToolContext& context, QObject* parent)
    : QAbstractListModel(parent)
    , m_context(context)
{
    connect(&m_context.selection(), &SelectionModel::changed, this, &PropertyModel::refresh);
    DocumentNotifier& notifier = m_context.project().notifier();
    connect(&notifier, &DocumentNotifier::propertyChanged, this, &PropertyModel::onPropertyChanged);
    connect(&notifier, &DocumentNotifier::screenChanged, this,
            [this](ScreenId screen)
            {
                if (screen == m_context.screenId() && !m_rows.empty() && !m_rows.front().descriptor)
                    Q_EMIT dataChanged(index(0), index(rowCount() - 1), {ValueRole, TextRole});
                Q_EMIT titleChanged();
            });
    connect(&notifier, &DocumentNotifier::projectReset, this, &PropertyModel::refresh);
    connect(&notifier, &DocumentNotifier::styleChanged, this,
            [this]
            {
                if (!m_rows.empty() && !m_rows.front().descriptor)
                    Q_EMIT dataChanged(index(0), index(rowCount() - 1), {ValueRole, TextRole});
            });
    refresh();
}

std::vector<HmiObject*> PropertyModel::selectedObjects() const
{
    std::vector<HmiObject*> objects;
    const Screen* screen = m_context.screen();
    if (!screen)
        return objects;
    // Selection order, so the first selected object gives the shown value.
    for (const ObjectId& id : m_context.selection().ids())
    {
        if (HmiObject* object = screen->object(id))
            objects.push_back(object);
    }
    return objects;
}

void PropertyModel::refresh()
{
    endEdit();
    beginResetModel();
    m_rows.clear();
    const auto objects = selectedObjects();
    if (objects.empty())
    {
        if (m_context.screen())
        {
            for (const ScreenField field :
                 {ScreenField::Name, ScreenField::Width, ScreenField::Height,
                  ScreenField::Background, ScreenField::StyleFace, ScreenField::StyleLine,
                  ScreenField::StyleText, ScreenField::StyleAccent, ScreenField::StyleFont})
                m_rows.push_back({.descriptor = nullptr, .field = field});
        }
    }
    else
    {
        // Properties every selected object has, with the same kind of value,
        // in the first object's order, grouped by category.
        std::vector<const char*> categories;
        std::vector<Row> rows;
        for (const PropertyDescriptor& d : objects.front()->descriptors())
        {
            const bool shared = std::ranges::all_of(
                objects,
                [&d](const HmiObject* object)
                {
                    const PropertyDescriptor* other = findDescriptor(object->descriptors(), d.id);
                    return other && valueKind(other->defaultValue) == valueKind(d.defaultValue);
                });
            if (!shared)
                continue;
            rows.push_back({.descriptor = &d});
            if (std::ranges::find(categories, d.category) == categories.end())
                categories.push_back(d.category);
        }
        std::ranges::stable_sort(
            rows, {}, [&categories](const Row& r)
            { return std::ranges::find(categories, r.descriptor->category) - categories.begin(); });
        m_rows = std::move(rows);
    }
    endResetModel();
    Q_EMIT titleChanged();
}

QString PropertyModel::title() const
{
    const auto objects = selectedObjects();
    if (objects.empty())
    {
        const Screen* screen = m_context.screen();
        return screen ? tr("Screen %1").arg(screen->name()) : QString();
    }
    if (objects.size() > 1)
        return tr("%n objects", nullptr, int(objects.size()));
    const ObjectTypeInfo* info = ObjectRegistry::instance().find(objects.front()->typeId());
    const QString type = info
                             ? QCoreApplication::translate("hmi::ObjectRegistry", info->displayName)
                             : tr("Unknown object");
    const QString name = objects.front()->name();
    return name.isEmpty() ? type : tr("%1 · %2").arg(type, name);
}

QStringList PropertyModel::fontFamilies() const
{
    return QFontDatabase::families();
}

int PropertyModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant PropertyModel::objectValue(const Row& row, bool* mixed) const
{
    const auto objects = selectedObjects();
    if (objects.empty())
        return {};
    const auto first = objects.front()->property(row.descriptor->id);
    if (mixed)
    {
        *mixed = std::ranges::any_of(objects, [&](const HmiObject* o)
                                     { return o->property(row.descriptor->id) != first; });
    }
    return first ? toVariant(*row.descriptor, *first) : QVariant();
}

QVariant PropertyModel::screenValue(ScreenField field) const
{
    const Screen* screen = m_context.screen();
    if (!screen)
        return {};
    switch (field)
    {
    case ScreenField::Name:
        return screen->name();
    case ScreenField::Width:
        return screen->size().width();
    case ScreenField::Height:
        return screen->size().height();
    case ScreenField::Background:
        return screen->backgroundColor();
    case ScreenField::StyleFace:
        return m_context.project().style().face;
    case ScreenField::StyleLine:
        return m_context.project().style().line;
    case ScreenField::StyleText:
        return m_context.project().style().text;
    case ScreenField::StyleAccent:
        return m_context.project().style().accent;
    case ScreenField::StyleFont:
        return m_context.project().style().fontFamily;
    }
    return {};
}

QVariant PropertyModel::currentValue(int row) const
{
    const Row& r = m_rows[std::size_t(row)];
    return r.descriptor ? objectValue(r, nullptr) : screenValue(r.field);
}

QVariant PropertyModel::data(const QModelIndex& index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};
    const Row& row = m_rows[std::size_t(index.row())];

    if (!row.descriptor)
    {
        static constexpr const char* kNames[] = {
            QT_TRANSLATE_NOOP("hmi::PropertyModel", "Name"),
            QT_TRANSLATE_NOOP("hmi::PropertyModel", "Width"),
            QT_TRANSLATE_NOOP("hmi::PropertyModel", "Height"),
            QT_TRANSLATE_NOOP("hmi::PropertyModel", "Background"),
            QT_TRANSLATE_NOOP("hmi::PropertyModel", "Field colour"),
            QT_TRANSLATE_NOOP("hmi::PropertyModel", "Outline colour"),
            QT_TRANSLATE_NOOP("hmi::PropertyModel", "Text colour"),
            QT_TRANSLATE_NOOP("hmi::PropertyModel", "Accent colour"),
            QT_TRANSLATE_NOOP("hmi::PropertyModel", "Font family"),
        };
        static constexpr const char* kKeys[] = {"name",       "width",       "height",
                                                "background", "styleFace",   "styleLine",
                                                "styleText",  "styleAccent", "styleFont"};
        const bool isSize = row.field == ScreenField::Width || row.field == ScreenField::Height;
        const bool isStyle = int(row.field) >= int(ScreenField::StyleFace);
        const bool isText = row.field == ScreenField::Name || row.field == ScreenField::StyleFont;
        switch (role)
        {
        case KeyRole:
            return QString::fromLatin1(kKeys[int(row.field)]);
        case NameRole:
            return QCoreApplication::translate("hmi::PropertyModel", kNames[int(row.field)]);
        case CategoryRole:
            return isStyle ? tr("Project style") : tr("Screen");
        case EditorRole:
            return isText   ? QStringLiteral("text")
                   : isSize ? QStringLiteral("number")
                            : QStringLiteral("color");
        case ValueRole:
            return screenValue(row.field);
        case MixedRole:
        case ReadOnlyRole:
            return false;
        case MinimumRole:
            return isSize ? QVariant(1.0) : QVariant();
        case MaximumRole:
            return isSize ? QVariant(kMaxScreenSide) : QVariant();
        case OptionsRole:
            return QStringList();
        case TextRole:
            return displayText(screenValue(row.field));
        case ComponentsRole:
            return QVariantList();
        default:
            return {};
        }
    }

    const PropertyDescriptor& d = *row.descriptor;
    switch (role)
    {
    case KeyRole:
        return QString::fromLatin1(d.key);
    case NameRole:
        return QCoreApplication::translate("hmi::Property", d.displayName);
    case CategoryRole:
        return QCoreApplication::translate("hmi::Property", d.category);
    case EditorRole:
        // A graphic is a string id, but picked from files, not typed.
        return d.id == Prop::Asset ? QStringLiteral("asset") : editorFor(valueKind(d.defaultValue));
    case ValueRole:
        return objectValue(row, nullptr);
    case MixedRole:
    {
        bool mixed = false;
        (void)objectValue(row, &mixed);
        return mixed;
    }
    case ReadOnlyRole:
        return d.constraint.readOnly;
    case MinimumRole:
        return d.constraint.minimum ? QVariant(*d.constraint.minimum) : QVariant();
    case MaximumRole:
        return d.constraint.maximum ? QVariant(*d.constraint.maximum) : QVariant();
    case OptionsRole:
    {
        QStringList options;
        for (const EnumOption& option : d.constraint.enumOptions)
            options << QCoreApplication::translate("hmi::Property", option.displayName);
        return options;
    }
    case TextRole:
    {
        bool mixed = false;
        const QVariant value = objectValue(row, &mixed);
        if (mixed)
            return QString();
        if (d.id == Prop::Asset)
        {
            const QString id = value.toString();
            const auto asset = m_context.project().assets().find(id);
            return id.isEmpty() ? tr("None") : asset ? asset->fileName : tr("Missing (%1)").arg(id);
        }
        if (valueKind(d.defaultValue) == ValueKind::Enum)
        {
            const int option = value.toInt();
            return option >= 0 ? QCoreApplication::translate(
                                     "hmi::Property",
                                     d.constraint.enumOptions[std::size_t(option)].displayName)
                               : QString();
        }
        return displayText(value);
    }
    case ComponentsRole:
        return components(objectValue(row, nullptr));
    default:
        return {};
    }
}

QHash<int, QByteArray> PropertyModel::roleNames() const
{
    return {
        {KeyRole, "key"},           {NameRole, "name"},       {CategoryRole, "category"},
        {EditorRole, "editor"},     {ValueRole, "value"},     {MixedRole, "mixed"},
        {ReadOnlyRole, "readOnly"}, {MinimumRole, "minimum"}, {MaximumRole, "maximum"},
        {OptionsRole, "options"},   {TextRole, "text"},       {ComponentsRole, "components"},
    };
}

bool PropertyModel::reject(const QString& message)
{
    Q_EMIT editRejected(message);
    // The editor still shows the rejected text, and the model's value did not
    // change, so nothing would replace it. Rebuild the rows once the editor's
    // handler has returned (it is running inside a delegate).
    QMetaObject::invokeMethod(this, &PropertyModel::refresh, Qt::QueuedConnection);
    return false;
}

bool PropertyModel::isBlank(const QVariant& value)
{
    return value.typeId() == QMetaType::QString && value.toString().trimmed().isEmpty();
}

bool PropertyModel::applyObjectValue(const Row& row, const QVariant& value)
{
    const PropertyDescriptor& d = *row.descriptor;
    // An emptied number field (or one left empty for a mixed value) changes
    // nothing.
    const ValueKind kind = valueKind(d.defaultValue);
    if (isBlank(value) && (kind == ValueKind::Double || kind == ValueKind::Int))
        return true;
    if (d.constraint.readOnly)
        return reject(tr("%1 cannot be changed.")
                          .arg(QCoreApplication::translate("hmi::Property", d.displayName)));
    const auto converted = fromVariant(d, value);
    if (!converted)
        return reject(tr("“%1” is not a valid %2.")
                          .arg(value.toString(),
                               QCoreApplication::translate("hmi::Property", d.displayName)));
    if (Result<PropertyValue> checked = validate(d, *converted); !checked)
        return reject(checked.error().message);

    const auto objects = selectedObjects();
    std::vector<ObjectId> ids;
    // A typed value applies to locked objects too: the lock guards against
    // accidental drags on the canvas, not deliberate edits.
    for (const HmiObject* object : objects)
    {
        if (object->property(d.id) != converted)
            ids.push_back(object->id());
    }
    if (ids.empty())
        return true;
    m_context.undoStack().push(new SetPropertyCommand(m_context.project(), m_context.screenId(),
                                                      std::move(ids), d.id, *converted,
                                                      m_editSession));
    return true;
}

bool PropertyModel::applyScreenValue(ScreenField field, const QVariant& value)
{
    const Screen* screen = m_context.screen();
    if (!screen)
        return false;
    ScreenSettings settings = ChangeScreenCommand::settingsOf(*screen);
    switch (field)
    {
    case ScreenField::Name:
    {
        const QString name = value.toString().trimmed();
        if (name.isEmpty())
            return reject(tr("A screen needs a name."));
        settings.name = name;
        break;
    }
    case ScreenField::Width:
    case ScreenField::Height:
    {
        if (isBlank(value))
            return true;
        const auto number = toNumber(value);
        if (!number || *number < 1.0 || *number > kMaxScreenSide)
            return reject(tr("Screen sizes go from 1 to %1.").arg(kMaxScreenSide));
        if (field == ScreenField::Width)
            settings.size.setWidth(std::round(*number));
        else
            settings.size.setHeight(std::round(*number));
        break;
    }
    case ScreenField::Background:
    {
        const QColor color = value.typeId() == QMetaType::QColor
                                 ? value.value<QColor>()
                                 : QColor::fromString(value.toString());
        if (!color.isValid())
            return reject(tr("“%1” is not a colour.").arg(value.toString()));
        settings.background = color;
        break;
    }
    case ScreenField::StyleFace:
    case ScreenField::StyleLine:
    case ScreenField::StyleText:
    case ScreenField::StyleAccent:
    case ScreenField::StyleFont:
        return applyStyleValue(field, value);
    }
    if (settings == ChangeScreenCommand::settingsOf(*screen))
        return true;
    m_context.undoStack().push(
        new ChangeScreenCommand(m_context.project(), m_context.screenId(), settings));
    return true;
}

bool PropertyModel::applyStyleValue(ScreenField field, const QVariant& value)
{
    HmiStyle style = m_context.project().style();
    if (field == ScreenField::StyleFont)
    {
        style.fontFamily = value.toString().trimmed();
    }
    else
    {
        const QColor color = value.typeId() == QMetaType::QColor
                                 ? value.value<QColor>()
                                 : QColor::fromString(value.toString());
        if (!color.isValid())
            return reject(tr("“%1” is not a colour.").arg(value.toString()));
        (field == ScreenField::StyleFace   ? style.face
         : field == ScreenField::StyleLine ? style.line
         : field == ScreenField::StyleText ? style.text
                                           : style.accent) = color;
    }
    if (style == m_context.project().style())
        return true;
    m_context.undoStack().push(new ChangeStyleCommand(m_context.project(), style));
    return true;
}

bool PropertyModel::setValue(int row, const QVariant& value)
{
    if (row < 0 || std::size_t(row) >= m_rows.size())
        return false;
    const Row& r = m_rows[std::size_t(row)];
    return r.descriptor ? applyObjectValue(r, value) : applyScreenValue(r.field, value);
}

bool PropertyModel::setComponent(int row, int component, const QVariant& text)
{
    if (row < 0 || std::size_t(row) >= m_rows.size() || isBlank(text))
        return false;
    const auto number = toNumber(text);
    if (!number)
        return reject(tr("“%1” is not a number.").arg(text.toString()));
    const double value = *number;
    const QVariant current = currentValue(row);
    if (current.typeId() == QMetaType::QPointF)
    {
        QPointF p = current.toPointF();
        (component == 0 ? p.rx() : p.ry()) = value;
        return setValue(row, p);
    }
    if (current.typeId() == QMetaType::QSizeF)
    {
        QSizeF s = current.toSizeF();
        (component == 0 ? s.rwidth() : s.rheight()) = value;
        return setValue(row, s);
    }
    return false;
}

bool PropertyModel::stepValue(int row, int steps)
{
    if (row < 0 || std::size_t(row) >= m_rows.size())
        return false;
    const auto number = toNumber(currentValue(row));
    if (!number)
        return false;
    QVariant next = *number + steps;
    // Stop at the limits instead of rejecting the step.
    const QVariant minimum = data(index(row), MinimumRole);
    const QVariant maximum = data(index(row), MaximumRole);
    if (minimum.isValid())
        next = std::max(next.toDouble(), minimum.toDouble());
    if (maximum.isValid())
        next = std::min(next.toDouble(), maximum.toDouble());
    return setValue(row, next);
}

bool PropertyModel::setFontFamily(int row, const QString& family)
{
    QFont font = currentValue(row).value<QFont>();
    font.setFamily(family);
    return setValue(row, font);
}

bool PropertyModel::setFontPixelSize(int row, int pixelSize)
{
    if (pixelSize < 1)
        return reject(tr("A font size must be at least 1."));
    QFont font = currentValue(row).value<QFont>();
    font.setPixelSize(pixelSize);
    return setValue(row, font);
}

bool PropertyModel::setFontBold(int row, bool bold)
{
    QFont font = currentValue(row).value<QFont>();
    font.setBold(bold);
    return setValue(row, font);
}

bool PropertyModel::setFontItalic(int row, bool italic)
{
    QFont font = currentValue(row).value<QFont>();
    font.setItalic(italic);
    return setValue(row, font);
}

bool PropertyModel::importAsset(int row, const QUrl& file, ImportMode mode)
{
    if (row < 0 || std::size_t(row) >= m_rows.size() || !m_rows[std::size_t(row)].descriptor)
        return false;
    const QString path = file.isLocalFile() ? file.toLocalFile() : file.toString();
    QFile input(path);
    if (!input.open(QIODevice::ReadOnly))
        return reject(tr("Cannot read %1: %2").arg(path, input.errorString()));
    QByteArray data = input.readAll();
    QString name = QFileInfo(path).fileName();

    if (GraphicFactory::looksLikeSvg(data))
    {
        const QStringList unsupported = SvgInspector::unsupportedElements(data);
        if (!unsupported.isEmpty() && mode == AskAboutSvg)
        {
            Q_EMIT svgImportQuestion(row, file, unsupported.join(QStringLiteral(", ")));
            return false;
        }
        if (mode == Rasterize)
        {
            data = SvgInspector::rasterize(data);
            if (data.isEmpty())
                return reject(tr("%1 could not be rasterized.").arg(name));
            name = QFileInfo(name).completeBaseName() + QStringLiteral(".png");
        }
    }

    // Check that it can be shown before it becomes part of the project.
    const Asset probe{
        .id = {}, .fileName = name, .suffix = QFileInfo(name).suffix().toLower(), .data = data};
    if (!GraphicFactory::create(probe))
        return reject(tr("%1 is not an image or SVG file that can be shown.").arg(name));
    const QString id = m_context.project().assets().add(std::move(data), name);
    return setValue(row, id);
}

void PropertyModel::beginEdit()
{
    m_editSession = m_nextSession++;
}

void PropertyModel::endEdit()
{
    m_editSession = 0;
}

void PropertyModel::onPropertyChanged(ObjectId object, PropertyId property)
{
    if (!m_context.selection().contains(object))
        return;
    for (std::size_t i = 0; i < m_rows.size(); ++i)
    {
        if (m_rows[i].descriptor && m_rows[i].descriptor->id == property)
            Q_EMIT dataChanged(index(int(i)), index(int(i)),
                               {ValueRole, MixedRole, TextRole, ComponentsRole});
    }
    if (property == Prop::Name)
        Q_EMIT titleChanged();
}

} // namespace hmi
