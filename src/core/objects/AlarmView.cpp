#include "core/objects/AlarmView.h"

#include "core/alarms/IAlarmSource.h"
#include "core/model/ObjectVisitor.h"
#include "core/objects/PaintHelpers.h"
#include "core/property/CommonDescriptors.h"
#include "core/property/DescriptorBuilders.h"
#include "core/property/Prop.h"
#include "core/registry/ObjectRegistry.h"

#include <QCoreApplication>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>

namespace hmi
{

namespace
{

QString tr(const char* text)
{
    return QCoreApplication::translate("hmi::Objects", text);
}

// What a column shows, from its name in the Columns property.
enum class Field
{
    Number,
    Date,
    Time,
    Class,
    Text,
    Status,
    InfoText,
    Priority,
    Origin,
};

Field fieldOf(const QString& column)
{
    const QString c = column.toLower();
    if (c.contains(u"number") || c == u"id" || c == u"no")
        return Field::Number;
    if (c.contains(u"date"))
        return Field::Date;
    if (c.contains(u"time"))
        return Field::Time;
    if (c.contains(u"class"))
        return Field::Class;
    if (c.contains(u"info"))
        return Field::InfoText;
    if (c.contains(u"status") || c.contains(u"state"))
        return Field::Status;
    if (c.contains(u"priority"))
        return Field::Priority;
    if (c.contains(u"origin") || c.contains(u"area"))
        return Field::Origin;
    return Field::Text; // "Alarm text", "Message", ...
}

double weightOf(Field field)
{
    switch (field)
    {
    case Field::Text:
        return 3.0;
    case Field::InfoText:
        return 2.0;
    case Field::Number:
    case Field::Priority:
        return 0.6;
    default:
        return 1.0;
    }
}

QString cellOf(const AlarmRow& row, Field field)
{
    switch (field)
    {
    case Field::Number:
        return QString::number(row.id);
    case Field::Date:
        return row.date;
    case Field::Time:
        return row.time;
    case Field::Class:
        return row.alarmClass;
    case Field::Text:
        return row.text;
    case Field::Status:
        return row.status;
    case Field::InfoText:
        return row.infoText;
    case Field::Priority:
        return QString::number(row.priority);
    case Field::Origin:
        return row.origin;
    }
    return {};
}

using enum AlarmView::ToolbarButton;

struct ControlLayout
{
    double rowHeight = 0;
    QRectF toolbar;
    std::array<QRectF, ButtonCount> buttons{};
    QRectF header;
    QRectF rows; // the area below the header
    int visibleRows = 0;
};

void drawButton(QPainter& painter, const QRectF& rect, int button, bool checked, bool enabled,
                const QColor& line)
{
    painter.save();
    painter.setPen(QPen(line, 1.0));
    painter.setBrush(checked ? QColor(0xBF, 0xD4, 0xF2) : QColor(0xF4, 0xF5, 0xF7));
    painter.drawRoundedRect(rect.adjusted(0.5, 0.5, -0.5, -0.5), 3, 3);
    QColor ink = enabled ? QColor(0x2B, 0x2F, 0x36) : QColor(0xA8, 0xAE, 0xB6);
    if (button == ShowActive)
        ink = enabled ? QColor(0xC2, 0x3B, 0x37) : ink;
    if (button == Acknowledge || button == AcknowledgeVisible || button == Confirm)
        ink = enabled ? QColor(0x1E, 0x7D, 0x4A) : ink;
    QPen pen(ink, std::max(1.2, rect.height() * 0.09), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    const QRectF r = rect.adjusted(rect.width() * 0.25, rect.height() * 0.25, -rect.width() * 0.25,
                                   -rect.height() * 0.25);
    const QPointF c = r.center();
    const auto triangle = [&](bool up)
    {
        const double y = up ? r.top() + r.height() * 0.3 : r.bottom() - r.height() * 0.3;
        const double tip = up ? r.top() - r.height() * 0.1 : r.bottom() + r.height() * 0.1;
        QPainterPath p;
        p.moveTo(r.left() + r.width() * 0.15, y + (up ? r.height() * 0.4 : -r.height() * 0.4));
        p.lineTo(c.x(), tip + (up ? r.height() * 0.4 : -r.height() * 0.4));
        p.lineTo(r.right() - r.width() * 0.15, y + (up ? r.height() * 0.4 : -r.height() * 0.4));
        painter.drawPath(p);
    };
    const auto check = [&](QPointF origin, double size)
    {
        QPainterPath p;
        p.moveTo(origin + QPointF(0, size * 0.5));
        p.lineTo(origin + QPointF(size * 0.35, size * 0.85));
        p.lineTo(origin + QPointF(size, size * 0.1));
        painter.drawPath(p);
    };
    switch (button)
    {
    case ShowActive: // a warning sign
    {
        QPainterPath p;
        p.moveTo(c.x(), r.top());
        p.lineTo(r.right(), r.bottom());
        p.lineTo(r.left(), r.bottom());
        p.closeSubpath();
        painter.drawPath(p);
        painter.drawLine(QPointF(c.x(), r.top() + r.height() * 0.4),
                         QPointF(c.x(), r.top() + r.height() * 0.7));
        break;
    }
    case ShowLogged: // a list
        for (int i = 0; i < 3; ++i)
        {
            const double y = r.top() + r.height() * (0.15 + 0.35 * i);
            painter.drawLine(QPointF(r.left(), y), QPointF(r.right(), y));
        }
        break;
    case FirstLine:
        painter.drawLine(r.topLeft(), r.topRight());
        triangle(true);
        break;
    case PreviousLine:
        triangle(true);
        break;
    case NextLine:
        triangle(false);
        break;
    case LastLine:
        painter.drawLine(r.bottomLeft(), r.bottomRight());
        triangle(false);
        break;
    case Acknowledge:
        check(r.topLeft(), r.width());
        break;
    case AcknowledgeVisible:
        check(r.topLeft() + QPointF(-r.width() * 0.15, 0), r.width() * 0.75);
        check(r.topLeft() + QPointF(r.width() * 0.35, 0), r.width() * 0.75);
        break;
    case Confirm:
        painter.drawEllipse(
            r.adjusted(-r.width() * 0.1, -r.height() * 0.1, r.width() * 0.1, r.height() * 0.1));
        check(r.topLeft() + QPointF(r.width() * 0.15, r.height() * 0.1), r.width() * 0.7);
        break;
    case ShowCurrent:
    {
        QPainterPath p;
        p.moveTo(r.left() + r.width() * 0.15, r.top());
        p.lineTo(r.right(), c.y());
        p.lineTo(r.left() + r.width() * 0.15, r.bottom());
        p.closeSubpath();
        painter.setBrush(ink);
        painter.drawPath(p);
        break;
    }
    default:
        break;
    }
    painter.restore();
}

} // namespace

AlarmView::AlarmView(ObjectId id)
    : ControlObject(staticDescriptors(), id)
{
}

std::span<const PropertyDescriptor> AlarmView::staticDescriptors()
{
    using namespace PropertyCategory;
    static const std::vector<PropertyDescriptor> table = withCommonDescriptors(
        {560.0, 260.0},
        {
            // The columns, in order; names as in WinCC Unified: Alarm number,
            // Date, Time, Alarm class, Alarm text, Status, Info text,
            // Priority, Origin.
            Describe::string(
                Prop::Columns, "columns", QT_TRANSLATE_NOOP("hmi::Property", "Columns (a;b;c)"),
                Layout, QStringLiteral("Alarm number;Date;Time;Alarm class;Alarm text;Status")),
            Describe::integer(Prop::RowCount, "rowCount",
                              QT_TRANSLATE_NOOP("hmi::Property", "Rows"), Layout, 8, 1, 100),
            Describe::flag(Prop::ShowToolbar, "showToolbar",
                           QT_TRANSLATE_NOOP("hmi::Property", "Toolbar"), Appearance, true),
            Describe::fillColor(QColor(Qt::white),
                                QT_TRANSLATE_NOOP("hmi::Property", "Background")),
            Describe::color(Prop::BackgroundColor, "backgroundColor",
                            QT_TRANSLATE_NOOP("hmi::Property", "Header colour"), Appearance,
                            QColor(0xE5, 0xE7, 0xEB)),
            Describe::lineColor(QColor(0xC9, 0xCE, 0xD6)),
            Describe::textColor(QColor(0x1F, 0x23, 0x28)),
            Describe::font(13),
        });
    return table;
}

std::unique_ptr<HmiObject> AlarmView::clone() const
{
    return std::unique_ptr<HmiObject>(new AlarmView(*this));
}

RuntimeBehavior AlarmView::runtimeBehavior() const
{
    return {RuntimeBehavior::Kind::Control, {}};
}

namespace
{

ControlLayout layoutOf(const HmiObject& view)
{
    ControlLayout l;
    const QSizeF size = view.geometry().size();
    const double k = PaintHelpers::contentScale(view);
    l.rowHeight = std::max(16.0, view.value<QFont>(Prop::Font).pixelSize() * 1.7) * k;
    double top = 1.0;
    if (view.value<bool>(Prop::ShowToolbar))
    {
        l.toolbar = QRectF(1, top, size.width() - 2, l.rowHeight * 1.35);
        const double side = l.toolbar.height() - 4 * k;
        double x = l.toolbar.left() + 3 * k;
        for (int b = 0; b < ButtonCount; ++b)
        {
            // Gaps group the buttons: views, navigation, acknowledgment.
            if (b == FirstLine || b == Acknowledge || b == ShowCurrent)
                x += side * 0.4;
            l.buttons[std::size_t(b)] = QRectF(x, l.toolbar.top() + 2 * k, side, side);
            x += side + 2 * k;
        }
        top = l.toolbar.bottom();
    }
    l.header = QRectF(1, top, size.width() - 2, l.rowHeight);
    l.rows = QRectF(1, l.header.bottom(), size.width() - 2,
                    std::max(0.0, size.height() - 1 - l.header.bottom()));
    l.visibleRows = l.rowHeight > 0 ? int(std::floor(l.rows.height() / l.rowHeight)) : 0;
    return l;
}

} // namespace

void AlarmView::paint(QPainter& painter, const PaintContext& context) const
{
    [[maybe_unused]] const QSizeF size = geometry().size();
    [[maybe_unused]] const QRectF bounds(QPointF(0, 0), size);
    const double k = PaintHelpers::contentScale(*this);
    const ControlLayout l = layoutOf(*this);
    // Live in the runtime; the layout with empty rows in the editor.
    const IAlarmSource* source = context.editing ? nullptr : context.alarms;
    const AlarmViewState state = source ? source->viewState(id()) : AlarmViewState{};
    const std::vector<AlarmRow> rows =
        source ? source->rows(state.logged) : std::vector<AlarmRow>{};
    const int selected = state.showCurrent ? 0 : state.selected;
    const int first = state.showCurrent ? 0 : state.firstRow;

    PaintHelpers::drawFace(painter, bounds, value<QColor>(Prop::FillColor),
                           value<QColor>(Prop::LineColor), 2.0);
    if (!l.toolbar.isEmpty())
    {
        painter.fillRect(l.toolbar, value<QColor>(Prop::BackgroundColor));
        for (int b = 0; b < ButtonCount; ++b)
        {
            const bool checked = (b == ShowActive && source && !state.logged) ||
                                 (b == ShowLogged && state.logged) ||
                                 (b == ShowCurrent && (state.showCurrent || !source));
            const bool acknowledging = b == Acknowledge || b == AcknowledgeVisible || b == Confirm;
            drawButton(painter, l.buttons[std::size_t(b)], b, checked,
                       !acknowledging || (source && !state.logged && !rows.empty()),
                       value<QColor>(Prop::LineColor));
        }
        // Pending alarms, and those to acknowledge, at the right.
        if (source)
        {
            const QRectF counts(l.buttons[ShowCurrent].right() + 8 * k, l.toolbar.top(),
                                l.toolbar.right() - l.buttons[ShowCurrent].right() - 14 * k,
                                l.toolbar.height());
            PaintHelpers::drawText(painter, *this, counts, Qt::AlignRight | Qt::AlignVCenter,
                                   tr("%1 pending · %2 to acknowledge")
                                       .arg(source->pendingCount())
                                       .arg(source->unacknowledgedCount()),
                                   k);
        }
    }

    const QStringList columns = Describe::splitItems(value<QString>(Prop::Columns));
    std::vector<Field> fields;
    double total = 0;
    for (const QString& column : columns)
    {
        fields.push_back(fieldOf(column));
        total += weightOf(fields.back());
    }
    std::vector<QRectF> cells; // column extents, at the header
    double x = 1.0;
    for (const Field field : fields)
    {
        const double w = total > 0 ? (size.width() - 2) * weightOf(field) / total : 0;
        cells.emplace_back(x, l.header.top(), w, l.rowHeight);
        x += w;
    }
    painter.fillRect(l.header, value<QColor>(Prop::BackgroundColor).darker(104));
    for (std::size_t i = 0; i < cells.size(); ++i)
        PaintHelpers::drawText(painter, *this, cells[i].adjusted(6 * k, 0, -4 * k, 0),
                               Qt::AlignLeft | Qt::AlignVCenter, columns[qsizetype(i)], k);

    // The lines: coloured by alarm class and status, the selected one framed.
    const int shown = source ? std::min<int>(l.visibleRows, int(rows.size()) - first)
                             : std::min(l.visibleRows, value<int>(Prop::RowCount));
    for (int r = 0; r < shown; ++r)
    {
        const QRectF line(l.rows.left(), l.rows.top() + r * l.rowHeight, l.rows.width(),
                          l.rowHeight);
        if (!source)
            continue;
        const AlarmRow& row = rows[std::size_t(first + r)];
        const bool steady = !row.colors.flashing || source->flashOn();
        const QColor background = steady && row.colors.background.isValid()
                                      ? row.colors.background
                                      : value<QColor>(Prop::FillColor);
        const QColor ink =
            steady && row.colors.text.isValid() ? row.colors.text : value<QColor>(Prop::TextColor);
        painter.fillRect(line, background);
        painter.save();
        painter.setFont(PaintHelpers::scaledFont(*this, k));
        painter.setPen(ink);
        for (std::size_t c = 0; c < cells.size(); ++c)
        {
            const QRectF cell(cells[c].left(), line.top(), cells[c].width(), line.height());
            painter.save();
            painter.setClipRect(cell, Qt::IntersectClip);
            painter.drawText(cell.adjusted(6 * k, 0, -4 * k, 0), Qt::AlignLeft | Qt::AlignVCenter,
                             cellOf(row, fields[c]));
            painter.restore();
        }
        painter.restore();
        if (first + r == selected)
        {
            painter.save();
            painter.setPen(QPen(QColor(0x2D, 0x6C, 0xDF), 2.0 * std::min(k, 2.0)));
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(line.adjusted(1, 1, -1, -1));
            painter.restore();
        }
    }
    painter.setPen(QPen(value<QColor>(Prop::LineColor), 1.0));
    for (std::size_t c = 1; c < cells.size(); ++c)
        painter.drawLine(QPointF(cells[c].left(), l.header.top()),
                         QPointF(cells[c].left(), size.height() - 1));
    const int lines = source ? l.visibleRows : std::min(l.visibleRows, value<int>(Prop::RowCount));
    for (int r = 1; r <= lines; ++r)
    {
        const double y = l.rows.top() + l.rowHeight * r;
        painter.drawLine(QPointF(1, y), QPointF(size.width() - 1, y));
    }
    if (source && rows.empty())
        PaintHelpers::drawText(painter, *this, l.rows, Qt::AlignCenter,
                               state.logged ? tr("No logged alarms") : tr("No active alarms"), k);
}

bool AlarmView::operateAt(QPointF point, IAlarmSource* alarms) const
{
    if (!alarms)
        return false;
    const ControlLayout l = layoutOf(*this);
    AlarmViewState state = alarms->viewState(id());
    const std::vector<AlarmRow> rows = alarms->rows(state.logged);
    const int count = int(rows.size());
    const int current =
        state.showCurrent ? 0 : std::clamp(state.selected, 0, std::max(0, count - 1));
    const auto select = [&](int row)
    {
        state.showCurrent = false;
        state.selected = std::clamp(row, 0, std::max(0, count - 1));
        // Keep the selected line in view.
        if (state.selected < state.firstRow)
            state.firstRow = state.selected;
        else if (l.visibleRows > 0 && state.selected >= state.firstRow + l.visibleRows)
            state.firstRow = state.selected - l.visibleRows + 1;
    };
    for (int b = 0; b < ButtonCount; ++b)
    {
        if (!l.buttons[std::size_t(b)].contains(point))
            continue;
        switch (b)
        {
        case ShowActive:
            state.logged = false;
            state.firstRow = 0;
            state.selected = 0;
            break;
        case ShowLogged:
            state.logged = true;
            state.firstRow = 0;
            state.selected = 0;
            break;
        case FirstLine:
            select(0);
            break;
        case PreviousLine:
            select(current - 1);
            break;
        case NextLine:
            select(current + 1);
            break;
        case LastLine:
            select(count - 1);
            break;
        case Acknowledge:
            if (!state.logged && current < count)
                alarms->acknowledge(rows[std::size_t(current)].id);
            return true;
        case AcknowledgeVisible:
            if (!state.logged)
            {
                // Group acknowledgment of the visible alarms.
                std::vector<int> ids;
                const int first = state.showCurrent ? 0 : state.firstRow;
                for (int r = first; r < std::min(count, first + l.visibleRows); ++r)
                    ids.push_back(rows[std::size_t(r)].id);
                alarms->acknowledge(ids);
            }
            return true;
        case Confirm:
            if (!state.logged && current < count)
                alarms->confirm(rows[std::size_t(current)].id);
            return true;
        case ShowCurrent:
            state.showCurrent = !state.showCurrent;
            state.selected = 0;
            state.firstRow = 0;
            break;
        default:
            break;
        }
        alarms->setViewState(id(), state);
        return true;
    }
    // A line selects that alarm (and stops showing the newest).
    if (l.rows.contains(point) && l.rowHeight > 0)
    {
        const int first = state.showCurrent ? 0 : state.firstRow;
        const int row = first + int((point.y() - l.rows.top()) / l.rowHeight);
        if (row < count)
        {
            state.firstRow = first;
            select(row);
            alarms->setViewState(id(), state);
        }
        return true;
    }
    return false;
}

QRectF AlarmView::buttonRect(ToolbarButton button) const
{
    return button >= 0 && button < ButtonCount ? layoutOf(*this).buttons[std::size_t(button)]
                                               : QRectF();
}

QRectF AlarmView::lineRect(int visibleLine) const
{
    const ControlLayout l = layoutOf(*this);
    return {l.rows.left(), l.rows.top() + visibleLine * l.rowHeight, l.rows.width(), l.rowHeight};
}

void AlarmView::accept(ObjectVisitor& visitor)
{
    visitor.visit(*this);
}

} // namespace hmi

HMI_REGISTER_OBJECT(hmi::AlarmView, "alarmView",
                    QT_TRANSLATE_NOOP("hmi::ObjectRegistry", "Alarm view"),
                    hmi::ObjectCategory::Controls, ":/icons/alarm-view.svg")
