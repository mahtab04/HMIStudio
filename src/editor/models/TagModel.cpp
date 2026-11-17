#include "editor/models/TagModel.h"

#include "comm/PlcDriver.h"
#include "comm/TagEngine.h"
#include "core/model/HmiObject.h"
#include "core/model/Project.h"
#include "core/tags/TagCsv.h"
#include "core/tags/TagUsage.h"
#include "editor/commands/TagCommands.h"
#include "editor/tools/ToolContext.h"

#include <QCoreApplication>
#include <QFile>
#include <QSaveFile>
#include <QUndoStack>

#include <cmath>

namespace hmi
{

namespace
{

constexpr Tag::Type kTypes[] = {Tag::Type::Bool, Tag::Type::Int, Tag::Type::Real,
                                Tag::Type::String};
constexpr Tag::Simulation kSimulations[] = {Tag::Simulation::None, Tag::Simulation::Ramp,
                                            Tag::Simulation::Sine, Tag::Simulation::Random,
                                            Tag::Simulation::Toggle};

QString localFile(const QUrl& url)
{
    return url.isLocalFile() ? url.toLocalFile() : url.toString();
}

} // namespace

TagModel::TagModel(ToolContext& context, TagEngine& engine, QObject* parent)
    : QAbstractTableModel(parent)
    , m_context(context)
    , m_engine(engine)
    , m_connections(new TagConnections(context, this))
{
    connect(this, &TagModel::problem, this,
            [this](const QString& message)
            {
                m_message = message;
                Q_EMIT messageChanged();
            });
    connect(&m_engine, &TagEngine::valuesChanged, this, &TagModel::monitorProblemChanged);
    connect(&m_engine, &TagEngine::runningChanged, this, &TagModel::monitorProblemChanged);
    connect(m_connections, &TagConnections::changed, this, &TagModel::rebuild);
    connect(m_connections, &TagConnections::problem, this, &TagModel::problem);
    connect(m_connections, &TagConnections::saved, this, &TagModel::clearMessage);
    DocumentNotifier& notifier = m_context.project().notifier();
    // The notifier reports after the fact; a reset is correct and tag lists
    // are short enough.
    connect(&notifier, &DocumentNotifier::tagAdded, this, &TagModel::rebuild);
    connect(&notifier, &DocumentNotifier::tagRemoved, this, &TagModel::rebuild);
    connect(&notifier, &DocumentNotifier::tagChanged, this, &TagModel::rebuild);
    connect(&notifier, &DocumentNotifier::projectReset, this,
            [this]
            {
                m_tableFilter.clear();
                Q_EMIT tableFilterChanged();
                rebuild();
            });
    connect(&m_engine, &TagEngine::valuesChanged, this,
            [this]
            {
                if (!m_rows.empty())
                    Q_EMIT dataChanged(index(0, 0), index(rowCount() - 1, ColumnCount - 1));
            });
    connect(&m_engine, &TagEngine::runningChanged, this,
            [this]
            {
                if (!m_rows.empty())
                    Q_EMIT dataChanged(index(0, 0), index(rowCount() - 1, ColumnCount - 1));
            });
    rebuild();
}

void TagModel::rebuild()
{
    beginResetModel();
    m_rows.clear();
    const QString filter = m_filter.trimmed();
    for (const Tag& tag : m_context.project().tags().tags())
    {
        if ((m_tableFilter.isEmpty() || tag.table == m_tableFilter) &&
            (filter.isEmpty() || tag.name.contains(filter, Qt::CaseInsensitive) ||
             tag.comment.contains(filter, Qt::CaseInsensitive) ||
             tag.address.contains(filter, Qt::CaseInsensitive) ||
             m_connections->name(tag.connection).contains(filter, Qt::CaseInsensitive) ||
             tag.unit.contains(filter, Qt::CaseInsensitive)))
            m_rows.push_back(tag.name);
    }
    endResetModel();
    Q_EMIT tagCountChanged();
}

void TagModel::setFilterText(const QString& text)
{
    if (text == m_filter)
        return;
    m_filter = text;
    rebuild();
    Q_EMIT filterTextChanged();
}

QString TagModel::monitorProblem() const
{
    for (const Tag& tag : m_context.project().tags().tags())
        if (const QString error = m_engine.problem(tag.name); !error.isEmpty())
            return tr("%1: %2").arg(tag.name, error);
    return {};
}

QStringList TagModel::tableNames() const
{
    QStringList names{tr("All tables")};
    for (const Tag& tag : m_context.project().tags().tags())
        if (!names.contains(tag.table))
            names.append(tag.table);
    if (!m_tableFilter.isEmpty() && !names.contains(m_tableFilter))
        names.append(m_tableFilter);
    return names;
}

void TagModel::setTableFilter(const QString& table)
{
    if (m_tableFilter == table)
        return;
    m_tableFilter = table;
    rebuild();
    Q_EMIT tableFilterChanged();
}

void TagModel::setSimulationSettings(bool simulation)
{
    if (simulation == m_simulationSettings)
        return;
    m_simulationSettings = simulation;
    Q_EMIT viewChanged();
}

double TagModel::columnWidth(int column, double width, double minimum) const
{
    const std::vector<int> columns =
        m_simulationSettings
            ? std::vector<int>{NameColumn,       TypeColumn,   MinimumColumn, MaximumColumn,
                               SimulationColumn, PeriodColumn, ValueColumn,   InitialColumn}
            : std::vector<int>{NameColumn,        TypeColumn,  ConnectionColumn, AddressColumn,
                               AcquisitionColumn, ValueColumn, TableColumn,      QualityColumn,
                               CommentColumn,     UnitColumn};
    if (std::ranges::find(columns, column) == columns.end())
        return 0;
    const auto weight = [](int c)
    {
        return c == NameColumn || c == AddressColumn || c == CommentColumn ? 1.8 : 1.0;
    };
    double total = 0;
    for (const int c : columns)
        total += weight(c);
    return std::max(minimum, (width - double(columns.size() - 1)) * weight(column) / total);
}

int TagModel::createTag(const QString& name, int type, const QString& table, int source,
                        const QString& address)
{
    if (type < 0 || type >= int(std::size(kTypes)) || source < 0 ||
        source >= m_connections->names().size())
        return -1;
    clearMessage();
    Tag tag;
    tag.name = name.trimmed();
    tag.type = kTypes[type];
    tag.table = table.trimmed().isEmpty() ? QStringLiteral("Default") : table.trimmed();
    tag.connection = m_connections->idAt(source);
    tag.address = address.trimmed();
    if (!Tag::isValidName(tag.name) || m_context.project().tags().find(tag.name))
    {
        Q_EMIT problem(tr("Enter a unique tag name."));
        return -1;
    }
    if (const Connection* connection = m_context.project().connection(tag.connection))
    {
        if (auto valid = PlcDriver::validateAddress(*connection, tag); !valid)
        {
            Q_EMIT problem(valid.error().message);
            return -1;
        }
    }
    tag.initial = tag.startValue();
    m_context.undoStack().push(new AddTagCommand(m_context.project(), tag));
    setTableFilter({});
    setFilterText({});
    const auto found = std::ranges::find(m_rows, tag.name);
    const int row = found == m_rows.end() ? -1 : int(found - m_rows.begin());
    Q_EMIT tagCreated(row);
    return row;
}

int TagModel::tagCount() const
{
    return int(m_context.project().tags().size());
}

const Tag* TagModel::tagAt(int row) const
{
    if (row < 0 || std::size_t(row) >= m_rows.size())
        return nullptr;
    return m_context.project().tags().find(m_rows[std::size_t(row)]);
}

int TagModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

int TagModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QStringList TagModel::columnTitles() const
{
    return {tr("Name"),       tr("Type"),       tr("Connection"), tr("PLC address"),
            tr("Tag table"),  tr("Cycle (ms)"), tr("Value"),      tr("Quality"),
            tr("Comment"),    tr("Unit"),       tr("Minimum"),    tr("Maximum"),
            tr("Simulation"), tr("Period (s)"), tr("Start value")};
}

QStringList TagModel::typeNames() const
{
    return {tr("Bool"), tr("Int"), tr("Real"), tr("String")};
}

QStringList TagModel::simulationNames() const
{
    return {tr("None"), tr("Ramp"), tr("Sine"), tr("Random"), tr("Toggle")};
}

QVariant TagModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole && section >= 0 &&
        section < ColumnCount)
        return columnTitles().at(section);
    return QAbstractTableModel::headerData(section, orientation, role);
}

QVariant TagModel::data(const QModelIndex& index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};
    const Tag* tag = tagAt(index.row());
    if (!tag)
        return {};
    const int column = index.column();
    if (role == EditorRole)
    {
        switch (column)
        {
        case QualityColumn:
            return QStringLiteral("readonly");
        case ConnectionColumn:
            return QStringLiteral("connection");
        case TypeColumn:
            return QStringLiteral("type");
        case SimulationColumn:
            return QStringLiteral("simulation");
        case MinimumColumn:
        case MaximumColumn:
        case PeriodColumn:
            return QStringLiteral("number");
        case ValueColumn:
            return m_engine.isRunning() && m_engine.isReadOnly() ? QStringLiteral("readonly")
                                                                 : QStringLiteral("value");
        default:
            return QStringLiteral("text");
        }
    }
    if (role == ChoiceRole)
    {
        if (column == TypeColumn)
            return int(std::ranges::find(kTypes, tag->type) - std::begin(kTypes));
        if (column == ConnectionColumn)
            return m_connections->choice(tag->connection);
        if (column == SimulationColumn)
            return int(std::ranges::find(kSimulations, tag->simulation) - std::begin(kSimulations));
        return -1;
    }
    if (role == Qt::ToolTipRole)
    {
        if (!tag->connection.isEmpty())
        {
            const Connection* connection = m_context.project().connection(tag->connection);
            if (!connection)
                return tr("Choose a connection for this imported tag.");
            if (auto valid = PlcDriver::validateAddress(*connection, *tag); !valid)
                return valid.error().message;
        }
        return m_engine.problem(tag->name);
    }
    if (role != Qt::DisplayRole && role != Qt::EditRole)
        return {};
    switch (column)
    {
    case ConnectionColumn:
        return m_connections->name(tag->connection);
    case AddressColumn:
        return tag->address;
    case AcquisitionColumn:
        return tag->acquisitionMs;
    case InitialColumn:
        return TagValues::toText(tag->startValue());
    case TableColumn:
        return tag->table;
    case QualityColumn:
        if (!tag->connection.isEmpty())
        {
            const Connection* connection = m_context.project().connection(tag->connection);
            if (!connection)
                return tr("Missing connection");
            if (!PlcDriver::validateAddress(*connection, *tag))
                return tr("Check address");
        }
        return m_engine.quality(tag->name);
    case NameColumn:
        return tag->name;
    case TypeColumn:
        return typeNames().at(data(index, ChoiceRole).toInt());
    case UnitColumn:
        return tag->unit;
    // A range means nothing for flags and texts.
    case MinimumColumn:
        return tag->type == Tag::Type::Bool || tag->type == Tag::Type::String
                   ? QVariant(QString())
                   : QVariant(tag->minimum);
    case MaximumColumn:
        return tag->type == Tag::Type::Bool || tag->type == Tag::Type::String
                   ? QVariant(QString())
                   : QVariant(tag->maximum);
    case SimulationColumn:
        return simulationNames().at(data(index, ChoiceRole).toInt());
    case PeriodColumn:
        return tag->period;
    case CommentColumn:
        return tag->comment;
    case ValueColumn:
        return m_engine.isRunning() ? TagValues::toText(m_engine.value(tag->name))
                                    : TagValues::toText(tag->startValue());
    default:
        return {};
    }
}

Qt::ItemFlags TagModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    if (index.column() != QualityColumn &&
        !(index.column() == ValueColumn && m_engine.isRunning() && m_engine.isReadOnly()))
        flags |= Qt::ItemIsEditable;
    return flags;
}

QHash<int, QByteArray> TagModel::roleNames() const
{
    QHash<int, QByteArray> roles = QAbstractTableModel::roleNames();
    roles.insert(EditorRole, "editor");
    roles.insert(ChoiceRole, "choice");
    return roles;
}

bool TagModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    // QML table edit delegates write the "display" role.
    if ((role != Qt::EditRole && role != Qt::DisplayRole) || !index.isValid())
        return false;
    return setCell(index.row(), index.column(), value);
}

bool TagModel::setCell(int row, int column, const QVariant& value)
{
    const Tag* current = tagAt(row);
    if (!current)
        return false;
    if (column == QualityColumn)
        return false;
    if (column == ValueColumn)
    {
        if (m_engine.isRunning() && m_engine.isReadOnly())
        {
            Q_EMIT problem(tr("PLC monitoring is read only. Use Simulation to test values."));
            return false;
        }
        // While simulating, the value goes to the engine; otherwise it is the
        // tag's start value.
        if (m_engine.isRunning())
        {
            m_engine.write(current->name, TagValue(value.toString()));
            return true;
        }
        Tag changed = *current;
        changed.initial = changed.converted(TagValue(value.toString()));
        if (changed == *current)
            return true;
        m_context.undoStack().push(
            new ChangeTagCommand(m_context.project(), current->name, std::move(changed)));
        return true;
    }
    if (column == NameColumn)
        return renameTag(current->name, value.toString().trimmed());

    Tag changed = *current;
    bool ok = true;
    const auto number = [&value, &ok]
    {
        bool good = false;
        const double n = value.toString().trimmed().toDouble(&good);
        ok = good && std::isfinite(n);
        return n;
    };
    switch (column)
    {
    case TypeColumn:
    {
        const int i = value.toInt();
        ok = i >= 0 && i < int(std::size(kTypes));
        if (ok)
        {
            changed.type = kTypes[i];
            changed.initial = changed.converted(changed.initial);
        }
        break;
    }
    case ConnectionColumn:
    {
        const int choice = value.toInt();
        if (choice < 0 || choice >= m_connections->names().size())
            return false;
        changed.connection = m_connections->idAt(choice);
        break;
    }
    case AddressColumn:
        changed.address = value.toString().trimmed();
        break;
    case TableColumn:
        changed.table = value.toString().trimmed();
        ok = !changed.table.isEmpty();
        break;
    case AcquisitionColumn:
        changed.acquisitionMs = value.toInt(&ok);
        ok = ok && changed.acquisitionMs >= 100 && changed.acquisitionMs <= 60000;
        break;
    case InitialColumn:
        changed.initial = changed.converted(TagValue(value.toString()));
        break;
    case UnitColumn:
        changed.unit = value.toString();
        break;
    case MinimumColumn:
        changed.minimum = number();
        break;
    case MaximumColumn:
        changed.maximum = number();
        break;
    case SimulationColumn:
    {
        const int i = value.toInt();
        ok = i >= 0 && i < int(std::size(kSimulations));
        if (ok)
            changed.simulation = kSimulations[i];
        break;
    }
    case PeriodColumn:
        changed.period = number();
        ok = ok && changed.period > 0.0;
        break;
    case CommentColumn:
        changed.comment = value.toString();
        break;
    default:
        return false;
    }
    if (!ok)
    {
        Q_EMIT problem(tr("“%1” is not a valid value here.").arg(value.toString()));
        Q_EMIT dataChanged(index(row, column), index(row, column));
        return false;
    }
    if (changed == *current)
        return true;
    m_context.undoStack().push(
        new ChangeTagCommand(m_context.project(), current->name, std::move(changed)));
    return true;
}

bool TagModel::renameTag(const QString& name, const QString& newName)
{
    if (newName == name)
        return true;
    TagDatabase& db = m_context.project().tags();
    const auto clash = db.indexOf(newName);
    if (!Tag::isValidName(newName) || (clash && clash != db.indexOf(name)))
    {
        Q_EMIT problem(newName.isEmpty() ? tr("A tag needs a name.")
                                         : tr("A tag called %1 already exists.").arg(newName));
        rebuild();
        return false;
    }
    Tag renamed = *db.find(name);
    renamed.name = newName;
    m_context.undoStack().push(new ChangeTagCommand(m_context.project(), name, std::move(renamed)));
    return true;
}

int TagModel::addTag()
{
    // Like TIA Portal's "<Add new>": the next free Tag_N, with the type of
    // the last tag in the table.
    const TagDatabase& db = m_context.project().tags();
    Tag tag;
    int n = 1;
    while (db.indexOf(QStringLiteral("%1_%2").arg(tr("Tag")).arg(n)))
        ++n;
    tag.name = QStringLiteral("%1_%2").arg(tr("Tag")).arg(n);
    tag.table = m_tableFilter.isEmpty() ? QStringLiteral("Default") : m_tableFilter;
    if (!db.tags().empty())
    {
        const Tag& last = db.tags().back();
        tag.type = last.type;
        tag.minimum = last.minimum;
        tag.maximum = last.maximum;
    }
    tag.initial = tag.startValue();
    const QString name = tag.name;
    m_context.undoStack().push(new AddTagCommand(m_context.project(), std::move(tag)));
    if (!m_filter.isEmpty())
        setFilterText({});
    const auto it = std::ranges::find(m_rows, name);
    return it == m_rows.end() ? -1 : int(it - m_rows.begin());
}

int TagModel::addTags(const QString& name, int typeIndex, int count)
{
    const QString stem = Tag::isValidName(name.trimmed()) ? name.trimmed() : tr("Tag");
    if (count < 1 || typeIndex < 0 || typeIndex >= int(std::size(kTypes)))
        return -1;
    QUndoStack& stack = m_context.undoStack();
    TagDatabase& db = m_context.project().tags();
    stack.beginMacro(tr("Add %n tag(s)", nullptr, count));
    QString first;
    int n = 1;
    for (int added = 0; added < count; ++n)
    {
        const QString candidate = QStringLiteral("%1_%2").arg(stem).arg(n);
        if (db.indexOf(candidate))
            continue;
        Tag tag;
        tag.name = candidate;
        tag.table = m_tableFilter.isEmpty() ? QStringLiteral("Default") : m_tableFilter;
        tag.type = kTypes[typeIndex];
        tag.initial = tag.startValue();
        if (first.isEmpty())
            first = candidate;
        stack.push(new AddTagCommand(m_context.project(), std::move(tag)));
        ++added;
    }
    stack.endMacro();
    if (!m_filter.isEmpty())
        setFilterText({});
    const auto it = std::ranges::find(m_rows, first);
    return it == m_rows.end() ? -1 : int(it - m_rows.begin());
}

void TagModel::removeTag(int row)
{
    if (const Tag* tag = tagAt(row))
        m_context.undoStack().push(new RemoveTagCommand(m_context.project(), tag->name));
}

QStringList TagModel::whereUsed(int row) const
{
    QStringList places;
    const Tag* tag = tagAt(row);
    if (!tag)
        return places;
    for (const TagUse& use : TagUsage::find(m_context.project(), tag->name))
    {
        const Screen* screen = m_context.project().screen(use.screen);
        const HmiObject* object = screen ? screen->object(use.object) : nullptr;
        if (!object)
            continue;
        const PropertyDescriptor* d = findDescriptor(object->descriptors(), use.property);
        const QString objectName = object->name().isEmpty() ? tr("(unnamed)") : object->name();
        places << tr("%1 · %2 · %3")
                      .arg(screen->name(), objectName,
                           d ? QCoreApplication::translate("hmi::Property", d->displayName)
                             : QString());
    }
    return places;
}

int TagModel::usageCount(int row) const
{
    const Tag* tag = tagAt(row);
    return tag ? int(TagUsage::find(m_context.project(), tag->name).size()) : 0;
}

QStringList TagModel::namesMatching(const QString& text) const
{
    QStringList names;
    for (const Tag& tag : m_context.project().tags().tags())
    {
        if (text.trimmed().isEmpty() || tag.name.contains(text.trimmed(), Qt::CaseInsensitive))
            names << tag.name;
    }
    return names;
}

bool TagModel::exportCsv(const QUrl& file)
{
    QSaveFile out(localFile(file));
    const auto source = m_context.project().tags().tags();
    std::vector<Tag> tags(source.begin(), source.end());
    for (Tag& tag : tags)
        if (const Connection* connection = m_context.project().connection(tag.connection))
            tag.connection = connection->name;
    if (!out.open(QIODevice::WriteOnly) || out.write(TagCsv::write(tags)) < 0 || !out.commit())
    {
        Q_EMIT problem(tr("Could not write %1: %2").arg(localFile(file), out.errorString()));
        return false;
    }
    return true;
}

int TagModel::importCsv(const QUrl& file)
{
    QFile in(localFile(file));
    if (!in.open(QIODevice::ReadOnly))
    {
        Q_EMIT problem(tr("Could not read %1: %2").arg(localFile(file), in.errorString()));
        return -1;
    }
    auto tags = TagCsv::read(in.readAll());
    if (!tags)
    {
        Q_EMIT problem(tr("Could not import the tags: %1").arg(tags.error().message));
        return -1;
    }
    QUndoStack& stack = m_context.undoStack();
    stack.beginMacro(tr("Import %n tag(s)", nullptr, int(tags->size())));
    for (Tag& tag : *tags)
    {
        if (!m_context.project().connection(tag.connection))
        {
            for (const Connection& connection : m_context.project().connections())
            {
                if (connection.name.compare(tag.connection, Qt::CaseInsensitive) == 0)
                {
                    tag.connection = connection.id;
                    break;
                }
            }
        }
        if (const Tag* existing = m_context.project().tags().find(tag.name))
        {
            tag.name = existing->name; // keep the spelling the bindings use
            stack.push(new ChangeTagCommand(m_context.project(), existing->name, std::move(tag)));
        }
        else
        {
            stack.push(new AddTagCommand(m_context.project(), std::move(tag)));
        }
    }
    stack.endMacro();
    return int(tags->size());
}

} // namespace hmi
