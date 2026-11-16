#include "editor/models/TagModel.h"

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
{
    DocumentNotifier& notifier = m_context.project().notifier();
    // The notifier reports after the fact; a reset is correct and tag lists
    // are short enough.
    connect(&notifier, &DocumentNotifier::tagAdded, this, &TagModel::rebuild);
    connect(&notifier, &DocumentNotifier::tagRemoved, this, &TagModel::rebuild);
    connect(&notifier, &DocumentNotifier::tagChanged, this, &TagModel::rebuild);
    connect(&notifier, &DocumentNotifier::projectReset, this, &TagModel::rebuild);
    connect(&m_engine, &TagEngine::valuesChanged, this,
            [this]
            {
                if (!m_rows.empty())
                    Q_EMIT dataChanged(index(0, ValueColumn), index(rowCount() - 1, ValueColumn));
            });
    connect(&m_engine, &TagEngine::runningChanged, this,
            [this]
            {
                if (!m_rows.empty())
                    Q_EMIT dataChanged(index(0, ValueColumn), index(rowCount() - 1, ValueColumn));
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
        if (filter.isEmpty() || tag.name.contains(filter, Qt::CaseInsensitive) ||
            tag.comment.contains(filter, Qt::CaseInsensitive) ||
            tag.unit.contains(filter, Qt::CaseInsensitive))
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
    return {tr("Name"),       tr("Type"),       tr("Unit"),    tr("Minimum"), tr("Maximum"),
            tr("Simulation"), tr("Period (s)"), tr("Comment"), tr("Value")};
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
        case TypeColumn:
            return QStringLiteral("type");
        case SimulationColumn:
            return QStringLiteral("simulation");
        case MinimumColumn:
        case MaximumColumn:
        case PeriodColumn:
            return QStringLiteral("number");
        case ValueColumn:
            return QStringLiteral("value");
        default:
            return QStringLiteral("text");
        }
    }
    if (role == ChoiceRole)
    {
        if (column == TypeColumn)
            return int(std::ranges::find(kTypes, tag->type) - std::begin(kTypes));
        if (column == SimulationColumn)
            return int(std::ranges::find(kSimulations, tag->simulation) - std::begin(kSimulations));
        return -1;
    }
    if (role != Qt::DisplayRole && role != Qt::EditRole)
        return {};
    switch (column)
    {
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
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
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
    if (column == ValueColumn)
    {
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
    // One step: the tag and every binding that names it.
    QUndoStack& stack = m_context.undoStack();
    stack.beginMacro(tr("Rename tag %1 to %2").arg(name, newName));
    for (const TagUse& use : TagUsage::find(m_context.project(), name))
    {
        const Screen* screen = m_context.project().screen(use.screen);
        const HmiObject* object = screen ? screen->object(use.object) : nullptr;
        const Binding* binding = object ? object->binding(use.property) : nullptr;
        if (!binding)
            continue; // a use inside a group: the group's children are not renamed yet
        Binding updated = *binding;
        updated.tag = newName;
        stack.push(new SetBindingCommand(m_context.project(), use.screen, {use.object},
                                         use.property, updated));
    }
    stack.push(new ChangeTagCommand(m_context.project(), name, std::move(renamed)));
    stack.endMacro();
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
    if (!out.open(QIODevice::WriteOnly) ||
        out.write(TagCsv::write(m_context.project().tags().tags())) < 0 || !out.commit())
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
