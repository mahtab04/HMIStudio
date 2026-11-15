#include "editor/models/LayerModel.h"

#include "core/model/HmiObject.h"
#include "core/model/Project.h"
#include "core/property/Prop.h"
#include "core/registry/ObjectRegistry.h"
#include "editor/commands/ReorderCommand.h"
#include "editor/commands/SetPropertyCommand.h"
#include "editor/models/SelectionModel.h"
#include "editor/tools/ToolContext.h"

#include <QCoreApplication>
#include <QUndoStack>
#include <QUrl>

#include <algorithm>

namespace hmi
{

namespace
{

QString typeName(const HmiObject& object)
{
    const ObjectTypeInfo* info = ObjectRegistry::instance().find(object.typeId());
    return info ? QCoreApplication::translate("hmi::ObjectRegistry", info->displayName)
                : QCoreApplication::translate("hmi::LayerModel", "Unknown object");
}

} // namespace

LayerModel::LayerModel(ToolContext& context, QObject* parent)
    : QAbstractListModel(parent)
    , m_context(context)
{
    DocumentNotifier& notifier = m_context.project().notifier();
    const auto refreshIfCurrent = [this](ScreenId screen)
    {
        if (screen == m_context.screenId())
            refresh();
    };
    connect(&notifier, &DocumentNotifier::objectAdded, this,
            [refreshIfCurrent](ScreenId screen, ObjectId, int) { refreshIfCurrent(screen); });
    connect(&notifier, &DocumentNotifier::objectRemoved, this,
            [refreshIfCurrent](ScreenId screen, ObjectId) { refreshIfCurrent(screen); });
    connect(&notifier, &DocumentNotifier::objectsReordered, this, refreshIfCurrent);
    connect(&notifier, &DocumentNotifier::projectReset, this, &LayerModel::refresh);
    connect(&notifier, &DocumentNotifier::propertyChanged, this, &LayerModel::onPropertyChanged);
    connect(&m_context.selection(), &SelectionModel::changed, this,
            [this]
            {
                if (!m_rows.empty())
                    Q_EMIT dataChanged(index(0), index(rowCount() - 1), {SelectedRole});
            });
    refresh();
}

void LayerModel::refresh()
{
    beginResetModel();
    m_rows.clear();
    if (const Screen* screen = m_context.screen())
    {
        const auto objects = screen->objects();
        int topLevel = 0;
        for (auto it = objects.rbegin(); it != objects.rend(); ++it)
            addRows(**it, 0, topLevel++);
    }
    endResetModel();
}

void LayerModel::addRows(const HmiObject& object, int depth, int topLevel)
{
    m_rows.push_back({.object = &object, .depth = depth, .topLevel = topLevel});
    if (!m_expanded.contains(object.id()))
        return;
    const auto children = object.children();
    for (auto it = children.rbegin(); it != children.rend(); ++it)
        addRows(**it, depth + 1, topLevel);
}

int LayerModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

const LayerModel::Row* LayerModel::rowAt(int row) const
{
    return row >= 0 && std::size_t(row) < m_rows.size() ? &m_rows[std::size_t(row)] : nullptr;
}

QVariant LayerModel::data(const QModelIndex& index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};
    const Row& row = m_rows[std::size_t(index.row())];
    const HmiObject& object = *row.object;
    switch (role)
    {
    case Qt::DisplayRole:
    case NameRole:
        return object.name().isEmpty() ? typeName(object) : object.name();
    case TypeRole:
        return typeName(object);
    case IconRole:
    {
        const ObjectTypeInfo* info = ObjectRegistry::instance().find(object.typeId());
        const QString path = info && !info->iconPath.isEmpty()
                                 ? info->iconPath
                                 : QStringLiteral(":/icons/group.svg");
        return QUrl(QStringLiteral("qrc") + path);
    }
    case DepthRole:
        return row.depth;
    case HasChildrenRole:
        return !object.children().empty();
    case ExpandedRole:
        return m_expanded.contains(object.id());
    case ObjectVisibleRole:
        return object.isVisible();
    case LockedRole:
        return object.isLocked();
    case SelectedRole:
        return row.depth == 0 && m_context.selection().contains(object.id());
    case EditableRole:
        return row.depth == 0;
    default:
        return {};
    }
}

QHash<int, QByteArray> LayerModel::roleNames() const
{
    return {
        {NameRole, "name"},
        {TypeRole, "typeName"},
        {IconRole, "iconSource"},
        {DepthRole, "depth"},
        {HasChildrenRole, "hasChildren"},
        {ExpandedRole, "expanded"},
        {ObjectVisibleRole, "objectVisible"},
        {LockedRole, "locked"},
        {SelectedRole, "selected"},
        {EditableRole, "editable"},
    };
}

void LayerModel::select(int row, bool additive)
{
    const Row* r = rowAt(row);
    const Screen* screen = m_context.screen();
    if (!r || !screen)
        return;
    // A child row stands for its group, the object the canvas can select.
    const auto objects = screen->objects();
    const ObjectId id = objects[objects.size() - 1 - std::size_t(r->topLevel)]->id();
    if (additive)
        m_context.selection().toggle(id);
    else
        m_context.selection().selectOnly(id);
}

void LayerModel::toggleExpanded(int row)
{
    const Row* r = rowAt(row);
    if (!r || r->object->children().empty())
        return;
    const ObjectId id = r->object->id();
    if (!m_expanded.remove(id))
        m_expanded.insert(id);
    refresh();
}

void LayerModel::setProperty(int row, PropertyId property, const QVariant& value)
{
    const Row* r = rowAt(row);
    if (!r || r->depth != 0)
        return;
    PropertyValue converted =
        property == Prop::Name ? PropertyValue(value.toString()) : PropertyValue(value.toBool());
    if (r->object->property(property) == converted)
        return;
    m_context.undoStack().push(new SetPropertyCommand(m_context.project(), m_context.screenId(),
                                                      {r->object->id()}, property,
                                                      std::move(converted)));
}

void LayerModel::setObjectVisible(int row, bool visible)
{
    setProperty(row, Prop::Visible, visible);
}

void LayerModel::setLocked(int row, bool locked)
{
    setProperty(row, Prop::Locked, locked);
}

void LayerModel::rename(int row, const QString& name)
{
    setProperty(row, Prop::Name, name.trimmed());
}

bool LayerModel::moveBy(int row, int delta)
{
    const Row* r = rowAt(row);
    const Screen* screen = m_context.screen();
    if (!r || !screen || r->depth != 0 || delta == 0 || m_rows.empty())
        return false;
    const Row& target = m_rows[std::size_t(std::clamp(row + delta, 0, rowCount() - 1))];
    const int from = r->topLevel;
    const int to = target.topLevel;
    if (from == to)
        return false;

    // Display order is topmost first; z-order is bottom first.
    std::vector<ObjectId> before;
    for (const auto& object : screen->objects())
        before.push_back(object->id());
    std::vector<ObjectId> display(before.rbegin(), before.rend());
    const ObjectId moved = display[std::size_t(from)];
    display.erase(display.begin() + from);
    display.insert(display.begin() + to, moved);
    std::vector<ObjectId> after(display.rbegin(), display.rend());

    m_context.undoStack().push(new ReorderCommand(
        m_context.project(), m_context.screenId(), std::move(before), std::move(after),
        QCoreApplication::translate("hmi::Commands", "Reorder layers")));
    return true;
}

void LayerModel::onPropertyChanged(ObjectId object, PropertyId property)
{
    if (property != Prop::Name && property != Prop::Visible && property != Prop::Locked)
        return;
    for (std::size_t i = 0; i < m_rows.size(); ++i)
    {
        if (m_rows[i].depth == 0 && m_rows[i].object->id() == object)
        {
            Q_EMIT dataChanged(index(int(i)), index(int(i)),
                               {NameRole, ObjectVisibleRole, LockedRole});
            return;
        }
    }
}

} // namespace hmi
