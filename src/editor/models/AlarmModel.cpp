#include "editor/models/AlarmModel.h"

#include "core/model/Project.h"
#include "editor/commands/AlarmCommands.h"
#include "editor/tools/ToolContext.h"

#include <QCoreApplication>
#include <QUndoStack>

#include <algorithm>
#include <cmath>

namespace hmi
{

namespace
{
constexpr const char* kStatusTitles[] = {
    QT_TRANSLATE_NOOP("hmi::AlarmModel", "Incoming"),
    QT_TRANSLATE_NOOP("hmi::AlarmModel", "Incoming/Outgoing"),
    QT_TRANSLATE_NOOP("hmi::AlarmModel", "Incoming/Acknowledged"),
    QT_TRANSLATE_NOOP("hmi::AlarmModel", "Incoming/Outgoing/Acknowledged"),
};
} // namespace

AlarmModel::AlarmModel(ToolContext& context, QObject* parent)
    : QAbstractTableModel(parent)
    , m_context(context)
{
    DocumentNotifier& notifier = m_context.project().notifier();
    connect(&notifier, &DocumentNotifier::alarmsChanged, this, &AlarmModel::rebuild);
    connect(&notifier, &DocumentNotifier::projectReset, this, &AlarmModel::rebuild);
    connect(&notifier, &DocumentNotifier::tagAdded, this, &AlarmModel::namesChanged);
    connect(&notifier, &DocumentNotifier::tagRemoved, this, &AlarmModel::namesChanged);
    connect(&notifier, &DocumentNotifier::tagChanged, this, &AlarmModel::namesChanged);
    rebuild();
}

const AlarmConfiguration& AlarmModel::configuration() const
{
    return m_context.project().alarms();
}

std::vector<std::size_t> AlarmModel::alarmRows() const
{
    std::vector<std::size_t> rows;
    const Alarm::Kind kind = m_page == AnalogPage ? Alarm::Kind::Analog : Alarm::Kind::Discrete;
    const auto& alarms = configuration().alarms;
    for (std::size_t i = 0; i < alarms.size(); ++i)
    {
        if (alarms[i].kind == kind)
            rows.push_back(i);
    }
    return rows;
}

void AlarmModel::rebuild()
{
    beginResetModel();
    m_rows = m_page == ClassesPage ? std::vector<std::size_t>{} : alarmRows();
    endResetModel();
    Q_EMIT namesChanged();
}

void AlarmModel::setPage(int page)
{
    if (page < DiscretePage || page > ClassesPage || page == int(m_page))
        return;
    m_page = Page(page);
    rebuild();
    Q_EMIT pageChanged();
}

QStringList AlarmModel::pageNames() const
{
    return {tr("Discrete alarms"), tr("Analog alarms"), tr("Alarm classes")};
}

QStringList AlarmModel::columnTitles() const
{
    if (m_page == ClassesPage)
    {
        QStringList titles{tr("Name"), tr("State machine"), tr("Priority")};
        for (const char* status : kStatusTitles)
        {
            const QString name = QCoreApplication::translate("hmi::AlarmModel", status);
            titles << tr("%1 background").arg(name) << tr("%1 text").arg(name)
                   << tr("%1 flashing").arg(name);
        }
        return titles;
    }
    return {tr("ID"),          tr("Name"),        tr("Alarm text"),   tr("Alarm class"),
            tr("Trigger tag"), tr("Trigger bit"), tr("Trigger mode"), tr("Mode"),
            tr("Limit"),       tr("Origin"),      tr("Info text")};
}

QStringList AlarmModel::classNames() const
{
    QStringList names;
    for (const AlarmClass& c : configuration().classes)
        names << c.name;
    return names;
}

QStringList AlarmModel::tagNames() const
{
    QStringList names;
    for (const Tag& tag : m_context.project().tags().tags())
        names << tag.name;
    return names;
}

QStringList AlarmModel::edgeNames() const
{
    return {tr("On rising edge"), tr("On falling edge")};
}

QStringList AlarmModel::yesNo() const
{
    return {tr("No"), tr("Yes")};
}

int AlarmModel::alarmCount() const
{
    return int(configuration().alarms.size());
}

int AlarmModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return m_page == ClassesPage ? int(configuration().classes.size()) : int(m_rows.size());
}

int AlarmModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return m_page == ClassesPage ? int(ClassColumnCount) : int(AlarmColumnCount);
}

double AlarmModel::columnWidth(int column, double width, double minimum) const
{
    std::vector<std::pair<int, double>> columns; // column, weight
    if (m_page == ClassesPage)
    {
        columns = {{ClassNameColumn, 1.6}, {StateMachineColumn, 2.2}, {PriorityColumn, 0.7}};
        for (int c = ClassColorColumn; c < ClassColumnCount; ++c)
            columns.emplace_back(c, 1.0);
    }
    else
    {
        columns = {{IdColumn, 0.5},  {NameColumn, 1.2},   {TextColumn, 2.4}, {ClassColumn, 1.1},
                   {TagColumn, 1.2}, {OriginColumn, 1.0}, {InfoColumn, 1.6}};
        if (m_page == DiscretePage)
            columns.insert(columns.begin() + 5, {{BitColumn, 0.7}, {EdgeColumn, 1.2}});
        else
            columns.insert(columns.begin() + 5, {{ModeColumn, 1.2}, {LimitColumn, 0.8}});
    }
    const auto it = std::ranges::find(columns, column, &std::pair<int, double>::first);
    if (it == columns.end())
        return 0;
    double total = 0;
    for (const auto& [c, w] : columns)
        total += w;
    return std::max(minimum, (width - double(columns.size() - 1)) * it->second / total);
}

QVariant AlarmModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    const QStringList titles = columnTitles();
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole && section >= 0 &&
        section < titles.size())
        return titles.at(section);
    return QAbstractTableModel::headerData(section, orientation, role);
}

Qt::ItemFlags AlarmModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
}

QHash<int, QByteArray> AlarmModel::roleNames() const
{
    QHash<int, QByteArray> roles = QAbstractTableModel::roleNames();
    roles.insert(EditorRole, "editor");
    roles.insert(ChoiceRole, "choice");
    roles.insert(SwatchRole, "swatch");
    return roles;
}

QVariant AlarmModel::alarmData(const Alarm& alarm, int column, int role) const
{
    if (role == EditorRole)
    {
        switch (column)
        {
        case IdColumn:
        case BitColumn:
        case LimitColumn:
            return QStringLiteral("number");
        case ClassColumn:
            return QStringLiteral("class");
        case TagColumn:
            return QStringLiteral("tag");
        case EdgeColumn:
            return QStringLiteral("edge");
        case ModeColumn:
            return QStringLiteral("mode");
        default:
            return QStringLiteral("text");
        }
    }
    if (role == ChoiceRole)
    {
        switch (column)
        {
        case ClassColumn:
            return int(classNames().indexOf(alarm.alarmClass));
        case TagColumn:
            return int(tagNames().indexOf(alarm.tag));
        case EdgeColumn:
            return alarm.fallingEdge ? 1 : 0;
        case ModeColumn:
            return int(alarm.mode);
        default:
            return -1;
        }
    }
    if (role != Qt::DisplayRole && role != Qt::EditRole)
        return {};
    switch (column)
    {
    case IdColumn:
        return alarm.id;
    case NameColumn:
        return alarm.name;
    case TextColumn:
        return alarm.text;
    case ClassColumn:
        return alarm.alarmClass;
    case TagColumn:
        return alarm.tag;
    case BitColumn:
        return alarm.bit;
    case EdgeColumn:
        return edgeNames().at(alarm.fallingEdge ? 1 : 0);
    case ModeColumn:
        return modeNames().at(int(alarm.mode));
    case LimitColumn:
        return alarm.limit;
    case OriginColumn:
        return alarm.origin;
    case InfoColumn:
        return alarm.infoText;
    default:
        return {};
    }
}

QVariant AlarmModel::classData(const AlarmClass& alarmClass, int column, int role) const
{
    const int colorColumn = column - ClassColorColumn;
    const int state = colorColumn / 3;
    const int part = colorColumn % 3; // background, text, flashing
    const AlarmColors* colors = colorColumn >= 0 ? &alarmClass.colors[std::size_t(state)] : nullptr;
    if (role == EditorRole)
    {
        if (column == ClassNameColumn || column == StateMachineColumn)
            return alarmClass.predefined
                       ? QStringLiteral("readonly")
                       : (column == ClassNameColumn ? QStringLiteral("text")
                                                    : QStringLiteral("stateMachine"));
        if (column == PriorityColumn)
            return QStringLiteral("number");
        return part == 2 ? QStringLiteral("flag") : QStringLiteral("color");
    }
    if (role == ChoiceRole)
    {
        if (column == StateMachineColumn)
            return int(alarmClass.stateMachine);
        if (colors && part == 2)
            return colors->flashing ? 1 : 0;
        return -1;
    }
    if (role == SwatchRole)
        return colors && part < 2 ? (part == 0 ? colors->background : colors->text) : QColor();
    if (role != Qt::DisplayRole && role != Qt::EditRole)
        return {};
    switch (column)
    {
    case ClassNameColumn:
        return alarmClass.name;
    case StateMachineColumn:
        return stateMachineNames().at(int(alarmClass.stateMachine));
    case PriorityColumn:
        return alarmClass.priority;
    default:
        break;
    }
    if (!colors)
        return {};
    if (part == 2)
        return yesNo().at(colors->flashing ? 1 : 0);
    const QColor& color = part == 0 ? colors->background : colors->text;
    return color.isValid() ? color.name().toUpper() : QString();
}

QVariant AlarmModel::data(const QModelIndex& index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};
    if (m_page == ClassesPage)
        return classData(configuration().classes[std::size_t(index.row())], index.column(), role);
    return alarmData(configuration().alarms[m_rows[std::size_t(index.row())]], index.column(),
                     role);
}

bool AlarmModel::change(AlarmConfiguration after, const QString& text)
{
    if (after == configuration())
        return true;
    if (auto valid = after.validate(); !valid)
    {
        Q_EMIT problem(valid.error().message);
        rebuild(); // back to what the cell held
        return false;
    }
    m_context.undoStack().push(
        new ChangeAlarmsCommand(m_context.project(), std::move(after), text));
    return true;
}

bool AlarmModel::setCell(int row, int column, const QVariant& value)
{
    AlarmConfiguration after = configuration();
    const auto number = [&value](bool* ok)
    {
        const double n = value.toString().trimmed().toDouble(ok);
        *ok = *ok && std::isfinite(n);
        return n;
    };
    bool ok = true;
    if (m_page == ClassesPage)
    {
        if (row < 0 || std::size_t(row) >= after.classes.size())
            return false;
        AlarmClass& c = after.classes[std::size_t(row)];
        const int colorColumn = column - ClassColorColumn;
        switch (column)
        {
        case ClassNameColumn:
        {
            const QString name = value.toString().trimmed();
            // Alarms of the class follow its new name.
            for (Alarm& alarm : after.alarms)
            {
                if (alarm.alarmClass == c.name)
                    alarm.alarmClass = name;
            }
            c.name = name;
            break;
        }
        case StateMachineColumn:
            ok = !c.predefined && value.toInt() >= 0 && value.toInt() <= 2;
            c.stateMachine = AlarmStateMachine(value.toInt());
            break;
        case PriorityColumn:
        {
            const double p = number(&ok);
            c.priority = int(std::lround(p));
            break;
        }
        default:
        {
            if (colorColumn < 0 || colorColumn >= 3 * AlarmClass::ColorStateCount)
                return false;
            AlarmColors& colors = c.colors[std::size_t(colorColumn / 3)];
            if (colorColumn % 3 == 2)
                colors.flashing = value.toInt() == 1;
            else
            {
                const QColor color(value.toString().trimmed());
                ok = color.isValid();
                (colorColumn % 3 == 0 ? colors.background : colors.text) = color;
            }
            break;
        }
        }
        if (!ok)
        {
            Q_EMIT problem(tr("“%1” is not a valid value here.").arg(value.toString()));
            rebuild();
            return false;
        }
        return change(std::move(after), tr("Change alarm class %1").arg(c.name));
    }

    if (row < 0 || std::size_t(row) >= m_rows.size())
        return false;
    Alarm& alarm = after.alarms[m_rows[std::size_t(row)]];
    switch (column)
    {
    case IdColumn:
        alarm.id = int(std::lround(number(&ok)));
        break;
    case NameColumn:
        alarm.name = value.toString().trimmed();
        break;
    case TextColumn:
        alarm.text = value.toString();
        break;
    case ClassColumn:
    {
        const QStringList names = classNames();
        const int i = value.toInt();
        ok = i >= 0 && i < names.size();
        if (ok)
            alarm.alarmClass = names.at(i);
        break;
    }
    case TagColumn:
    {
        const QStringList names = tagNames();
        const int i = value.toInt();
        ok = i >= 0 && i < names.size();
        if (ok)
            alarm.tag = names.at(i);
        break;
    }
    case BitColumn:
        alarm.bit = int(std::lround(number(&ok)));
        break;
    case EdgeColumn:
        alarm.fallingEdge = value.toInt() == 1;
        break;
    case ModeColumn:
        ok = value.toInt() >= 0 && value.toInt() < modeNames().size();
        alarm.mode = Alarm::Mode(value.toInt());
        break;
    case LimitColumn:
        alarm.limit = number(&ok);
        break;
    case OriginColumn:
        alarm.origin = value.toString();
        break;
    case InfoColumn:
        alarm.infoText = value.toString();
        break;
    default:
        return false;
    }
    if (!ok)
    {
        Q_EMIT problem(tr("“%1” is not a valid value here.").arg(value.toString()));
        rebuild();
        return false;
    }
    return change(std::move(after), tr("Change alarm %1").arg(alarm.id));
}

int AlarmModel::addRow()
{
    AlarmConfiguration after = configuration();
    if (m_page == ClassesPage)
    {
        // A new class starts with the Warning class's colours.
        AlarmClass c = *after.findClass(QStringLiteral("Warning"));
        c.predefined = false;
        c.preserved = {};
        int n = 1;
        while (after.findClass(tr("Class_%1").arg(n)))
            ++n;
        c.name = tr("Class_%1").arg(n);
        after.classes.push_back(c);
        if (!change(std::move(after), tr("Add alarm class %1").arg(c.name)))
            return -1;
        return rowCount() - 1;
    }
    Alarm alarm;
    alarm.id = after.nextId();
    alarm.kind = m_page == AnalogPage ? Alarm::Kind::Analog : Alarm::Kind::Discrete;
    alarm.name = tr("Alarm_%1").arg(alarm.id);
    alarm.text = alarm.name;
    alarm.alarmClass = m_page == AnalogPage ? QStringLiteral("Warning") : QStringLiteral("Alarm");
    if (const QStringList tags = tagNames(); !tags.isEmpty())
        alarm.tag = tags.front();
    const int id = alarm.id;
    after.alarms.push_back(std::move(alarm));
    if (!change(std::move(after), tr("Add alarm %1").arg(id)))
        return -1;
    const auto it = std::ranges::find_if(m_rows, [this, id](std::size_t i)
                                         { return configuration().alarms[i].id == id; });
    return it == m_rows.end() ? -1 : int(it - m_rows.begin());
}

void AlarmModel::removeRow(int row)
{
    AlarmConfiguration after = configuration();
    if (m_page == ClassesPage)
    {
        if (row < 0 || std::size_t(row) >= after.classes.size())
            return;
        const AlarmClass& c = after.classes[std::size_t(row)];
        if (c.predefined)
        {
            Q_EMIT problem(tr("%1 is a predefined alarm class; it cannot be deleted.").arg(c.name));
            return;
        }
        if (std::ranges::any_of(after.alarms,
                                [&c](const Alarm& a) { return a.alarmClass == c.name; }))
        {
            Q_EMIT problem(
                tr("Alarms use the class %1; give them another class first.").arg(c.name));
            return;
        }
        const QString name = c.name;
        after.classes.erase(after.classes.begin() + row);
        change(std::move(after), tr("Delete alarm class %1").arg(name));
        return;
    }
    if (row < 0 || std::size_t(row) >= m_rows.size())
        return;
    const int id = after.alarms[m_rows[std::size_t(row)]].id;
    after.alarms.erase(after.alarms.begin() + std::ptrdiff_t(m_rows[std::size_t(row)]));
    change(std::move(after), tr("Delete alarm %1").arg(id));
}

} // namespace hmi
