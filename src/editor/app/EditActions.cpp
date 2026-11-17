#include "editor/app/EditActions.h"

#include "core/io/ObjectJson.h"
#include "core/model/HmiObject.h"
#include "core/model/Project.h"
#include "core/model/Screen.h"
#include "core/property/Prop.h"
#include "editor/commands/AddObjectCommand.h"
#include "editor/commands/GroupObjectsCommand.h"
#include "editor/commands/MoveObjectsCommand.h"
#include "editor/commands/RemoveObjectsCommand.h"
#include "editor/commands/ReorderCommand.h"
#include "editor/commands/ResizeObjectCommand.h"
#include "editor/commands/SetPropertyCommand.h"
#include "editor/commands/UngroupCommand.h"
#include "editor/models/SelectionModel.h"
#include "editor/tools/ToolContext.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QMimeData>
#include <QUndoStack>

#include <algorithm>
#include <cmath>

namespace hmi
{

namespace
{

QClipboard* clipboard()
{
    return QGuiApplication::clipboard();
}

QString mimeType()
{
    return QString::fromLatin1(EditActions::kMimeType);
}

// Pushes `macro` if it has children, else deletes it. Returns whether
// anything was pushed.
bool pushIfNotEmpty(QUndoStack& stack, QUndoCommand* macro)
{
    if (macro->childCount() == 0)
    {
        delete macro;
        return false;
    }
    stack.push(macro);
    return true;
}

} // namespace

EditActions::EditActions(ToolContext& context)
    : m_context(context)
{
}

std::vector<HmiObject*> EditActions::selectedObjects(bool skipLocked) const
{
    std::vector<HmiObject*> result;
    const Screen* screen = m_context.screen();
    if (!screen)
        return result;
    const SelectionModel& selection = m_context.selection();
    for (const auto& object : screen->objects())
    {
        if (selection.contains(object->id()) && !(skipLocked && object->isLocked()))
            result.push_back(object.get());
    }
    return result;
}

std::vector<ObjectId> EditActions::selectedIds() const
{
    std::vector<ObjectId> ids;
    for (const HmiObject* object : selectedObjects(false))
        ids.push_back(object->id());
    return ids;
}

bool EditActions::hasSelection() const
{
    return !selectedObjects(false).empty();
}

bool EditActions::canGroup() const
{
    return selectedObjects(false).size() >= 2;
}

bool EditActions::canUngroup() const
{
    const auto objects = selectedObjects(false);
    return std::ranges::any_of(objects, [](const HmiObject* o) { return !o->children().empty(); });
}

bool EditActions::canPaste() const
{
    const QMimeData* data = clipboard() ? clipboard()->mimeData() : nullptr;
    return data && data->hasFormat(mimeType());
}

bool EditActions::canAlign() const
{
    return !selectedObjects(true).empty();
}

bool EditActions::canDistribute() const
{
    return selectedObjects(true).size() >= 3;
}

bool EditActions::canMatchSize() const
{
    return selectedObjects(false).size() >= 2;
}

void EditActions::selectAll()
{
    const Screen* screen = m_context.screen();
    if (!screen)
        return;
    std::vector<ObjectId> ids;
    ids.reserve(screen->objectCount());
    // Hidden objects are left out, so they cannot be moved by accident.
    for (const auto& object : screen->objects())
    {
        if (object->isVisible())
            ids.push_back(object->id());
    }
    m_context.selection().set(std::move(ids));
}

bool EditActions::deleteSelection()
{
    auto ids = selectedIds();
    if (ids.empty())
        return false;
    m_context.undoStack().push(
        new RemoveObjectsCommand(m_context.project(), m_context.screenId(), std::move(ids)));
    m_context.selection().clear();
    return true;
}

bool EditActions::insertCopies(std::vector<std::unique_ptr<HmiObject>> objects, QPointF offset,
                               const QString& text)
{
    if (objects.empty() || !m_context.screen())
        return false;
    QUndoStack& stack = m_context.undoStack();
    std::vector<ObjectId> ids;
    stack.beginMacro(text);
    for (auto& object : objects)
    {
        const QPointF pos = object->geometry().topLeft() + offset;
        [[maybe_unused]] const bool moved =
            object->setProperty(Prop::X, pos.x()) && object->setProperty(Prop::Y, pos.y());
        Q_ASSERT(moved);
        ids.push_back(object->id());
        stack.push(
            new AddObjectCommand(m_context.project(), m_context.screenId(), std::move(object)));
    }
    stack.endMacro();
    m_context.selection().set(std::move(ids));
    return true;
}

bool EditActions::duplicate()
{
    std::vector<std::unique_ptr<HmiObject>> copies;
    for (const HmiObject* object : selectedObjects(false))
        copies.push_back(object->cloneWithNewId());
    return insertCopies(std::move(copies), {kPasteOffset, kPasteOffset},
                        QCoreApplication::translate("hmi::EditActions", "Duplicate"));
}

bool EditActions::copy()
{
    const auto objects = selectedObjects(false);
    if (objects.empty() || !clipboard())
        return false;
    std::vector<const HmiObject*> constObjects(objects.begin(), objects.end());
    const QJsonObject json{
        {QStringLiteral("formatVersion"), kClipboardFormatVersion},
        {QStringLiteral("objects"), ObjectJson::writeAll(constObjects)},
    };
    auto* data = new QMimeData();
    data->setData(mimeType(), QJsonDocument(json).toJson(QJsonDocument::Compact));
    clipboard()->setMimeData(data);
    m_pasteCount = 0;
    return true;
}

bool EditActions::cut()
{
    if (!copy())
        return false;
    QUndoStack& stack = m_context.undoStack();
    stack.beginMacro(QCoreApplication::translate("hmi::EditActions", "Cut"));
    deleteSelection();
    stack.endMacro();
    // A cut is pasted where it was, the first time.
    m_pasteCount = -1;
    return true;
}

bool EditActions::paste()
{
    const QMimeData* data = clipboard() ? clipboard()->mimeData() : nullptr;
    if (!data || !data->hasFormat(mimeType()))
        return false;
    const QJsonObject json = QJsonDocument::fromJson(data->data(mimeType())).object();
    if (json.value(u"formatVersion").toInt() != kClipboardFormatVersion)
    {
        qWarning("EditActions: unsupported clipboard format");
        return false;
    }
    auto objects = ObjectJson::readAll(json.value(u"objects").toArray());
    if (!objects)
    {
        qWarning("EditActions: cannot paste: %s", qPrintable(objects.error().message));
        return false;
    }
    // Fresh ids: the originals may still be on the screen, or the same
    // clipboard may be pasted many times.
    std::vector<std::unique_ptr<HmiObject>> copies;
    for (const auto& object : *objects)
        copies.push_back(object->cloneWithNewId());
    ++m_pasteCount;
    const double offset = kPasteOffset * m_pasteCount;
    return insertCopies(std::move(copies), {offset, offset},
                        QCoreApplication::translate("hmi::EditActions", "Paste"));
}

bool EditActions::nudge(QPointF delta)
{
    std::vector<ObjectId> ids;
    for (const HmiObject* object : selectedObjects(true))
        ids.push_back(object->id());
    if (ids.empty() || delta.isNull())
        return false;
    m_context.undoStack().push(new MoveObjectsCommand(m_context.project(), m_context.screenId(),
                                                      std::move(ids), delta,
                                                      MoveObjectsCommand::Kind::Nudge));
    return true;
}

bool EditActions::group()
{
    if (!canGroup())
        return false;
    auto* command =
        new GroupObjectsCommand(m_context.project(), m_context.screenId(), selectedIds());
    const ObjectId group = command->groupId();
    m_context.undoStack().push(command);
    m_context.selection().selectOnly(group);
    return true;
}

bool EditActions::ungroup()
{
    if (!canUngroup())
        return false;
    std::vector<ObjectId> groups;
    std::vector<ObjectId> keep;
    for (const HmiObject* object : selectedObjects(false))
        (object->children().empty() ? keep : groups).push_back(object->id());
    auto* command =
        new UngroupCommand(m_context.project(), m_context.screenId(), std::move(groups));
    m_context.undoStack().push(command);
    keep.insert(keep.end(), command->childIds().begin(), command->childIds().end());
    m_context.selection().set(std::move(keep));
    return true;
}

bool EditActions::align(Alignment alignment)
{
    const Screen* screen = m_context.screen();
    const auto all = selectedObjects(false);
    const auto movable = selectedObjects(true);
    if (!screen || movable.empty())
        return false;

    QRectF target;
    if (all.size() == 1)
    {
        target = QRectF(QPointF(0, 0), screen->size());
    }
    else
    {
        for (const HmiObject* object : all)
            target = target.isNull() ? object->sceneBoundingRect()
                                     : target.united(object->sceneBoundingRect());
    }

    static constexpr const char* kTexts[] = {
        QT_TRANSLATE_NOOP("hmi::EditActions", "Align left"),
        QT_TRANSLATE_NOOP("hmi::EditActions", "Align centre"),
        QT_TRANSLATE_NOOP("hmi::EditActions", "Align right"),
        QT_TRANSLATE_NOOP("hmi::EditActions", "Align top"),
        QT_TRANSLATE_NOOP("hmi::EditActions", "Align middle"),
        QT_TRANSLATE_NOOP("hmi::EditActions", "Align bottom"),
    };
    auto* macro =
        new QUndoCommand(QCoreApplication::translate("hmi::EditActions", kTexts[int(alignment)]));
    for (const HmiObject* object : movable)
    {
        const QRectF bounds = object->sceneBoundingRect();
        QPointF delta;
        switch (alignment)
        {
        case Alignment::Left:
            delta.setX(target.left() - bounds.left());
            break;
        case Alignment::HorizontalCentre:
            delta.setX(target.center().x() - bounds.center().x());
            break;
        case Alignment::Right:
            delta.setX(target.right() - bounds.right());
            break;
        case Alignment::Top:
            delta.setY(target.top() - bounds.top());
            break;
        case Alignment::VerticalCentre:
            delta.setY(target.center().y() - bounds.center().y());
            break;
        case Alignment::Bottom:
            delta.setY(target.bottom() - bounds.bottom());
            break;
        }
        delta = QPointF(std::round(delta.x()), std::round(delta.y()));
        if (delta.isNull())
            continue;
        const QRectF before = object->geometry();
        new ResizeObjectCommand(m_context.project(), m_context.screenId(), object->id(), before,
                                before.translated(delta), macro);
    }
    return pushIfNotEmpty(m_context.undoStack(), macro);
}

bool EditActions::distribute(Qt::Orientation orientation)
{
    auto objects = selectedObjects(true);
    if (objects.size() < 3)
        return false;
    const bool horizontal = orientation == Qt::Horizontal;
    const auto start = [horizontal](const QRectF& r)
    {
        return horizontal ? r.left() : r.top();
    };
    const auto length = [horizontal](const QRectF& r)
    {
        return horizontal ? r.width() : r.height();
    };
    const auto centre = [horizontal](const QRectF& r)
    {
        return horizontal ? r.center().x() : r.center().y();
    };

    std::ranges::stable_sort(objects, {},
                             [&](const HmiObject* o) { return centre(o->sceneBoundingRect()); });
    const QRectF first = objects.front()->sceneBoundingRect();
    const QRectF last = objects.back()->sceneBoundingRect();
    double total = 0.0;
    for (const HmiObject* object : objects)
        total += length(object->sceneBoundingRect());
    const double span = start(last) + length(last) - start(first);
    const double gap = (span - total) / double(objects.size() - 1);

    auto* macro = new QUndoCommand(
        horizontal ? QCoreApplication::translate("hmi::EditActions", "Distribute horizontally")
                   : QCoreApplication::translate("hmi::EditActions", "Distribute vertically"));
    double position = start(first);
    for (const HmiObject* object : objects)
    {
        const QRectF bounds = object->sceneBoundingRect();
        const double shift = std::round(position - start(bounds));
        position += length(bounds) + gap;
        if (shift == 0.0)
            continue;
        const QRectF before = object->geometry();
        const QRectF after = before.translated(horizontal ? QPointF(shift, 0) : QPointF(0, shift));
        new ResizeObjectCommand(m_context.project(), m_context.screenId(), object->id(), before,
                                after, macro);
    }
    return pushIfNotEmpty(m_context.undoStack(), macro);
}

bool EditActions::matchSize(SizeMatch match)
{
    const Screen* screen = m_context.screen();
    const auto& ids = m_context.selection().ids();
    if (!screen || ids.size() < 2)
        return false;
    const HmiObject* reference = screen->object(ids.front());
    if (!reference)
        return false;
    const QSizeF size = reference->geometry().size();

    auto* macro =
        new QUndoCommand(QCoreApplication::translate("hmi::EditActions", "Make same size"));
    for (const HmiObject* object : selectedObjects(true))
    {
        if (object == reference)
            continue;
        const QRectF before = object->geometry();
        QRectF after = before;
        if (match != SizeMatch::Height)
            after.setWidth(size.width());
        if (match != SizeMatch::Width)
            after.setHeight(size.height());
        if (after != before)
        {
            new ResizeObjectCommand(m_context.project(), m_context.screenId(), object->id(), before,
                                    after, macro);
        }
    }
    return pushIfNotEmpty(m_context.undoStack(), macro);
}

bool EditActions::applyProjectStyle()
{
    const HmiStyle& style = m_context.project().style();
    auto* macro =
        new QUndoCommand(QCoreApplication::translate("hmi::EditActions", "Apply project style"));
    for (const HmiObject* object : selectedObjects(false))
    {
        // Work out the styled values on a copy, then set each that differs.
        const auto styled = object->clone();
        if (!style.applyTo(*styled))
            continue;
        for (const PropertyDescriptor& d : object->descriptors())
        {
            const auto value = styled->property(d.id);
            if (d.styleRole != StyleRole::None && value && *value != object->property(d.id))
            {
                new SetPropertyCommand(m_context.project(), m_context.screenId(), {object->id()},
                                       d.id, *value, 0, macro);
            }
        }
    }
    return pushIfNotEmpty(m_context.undoStack(), macro);
}

bool EditActions::reorder(Order order)
{
    const Screen* screen = m_context.screen();
    if (!screen)
        return false;
    const SelectionModel& selection = m_context.selection();
    std::vector<ObjectId> before;
    std::vector<char> selected; // not vector<bool>: elements are swapped
    for (const auto& object : screen->objects())
    {
        before.push_back(object->id());
        selected.push_back(selection.contains(object->id()));
    }
    std::vector<ObjectId> after = before;

    const auto swap = [&](std::size_t a, std::size_t b)
    {
        std::swap(after[a], after[b]);
        std::swap(selected[a], selected[b]);
    };
    const std::size_t n = after.size();
    QString text;
    switch (order)
    {
    case Order::Forward:
        // From the top down, each selected object steps over the unselected
        // one above it; a selected block moves up as one.
        for (std::size_t i = n; i-- > 1;)
        {
            if (selected[i - 1] && !selected[i])
                swap(i - 1, i);
        }
        text = QCoreApplication::translate("hmi::EditActions", "Bring forward");
        break;
    case Order::Backward:
        for (std::size_t i = 1; i < n; ++i)
        {
            if (selected[i] && !selected[i - 1])
                swap(i - 1, i);
        }
        text = QCoreApplication::translate("hmi::EditActions", "Send backward");
        break;
    case Order::ToFront:
    case Order::ToBack:
    {
        const bool front = order == Order::ToFront;
        std::vector<ObjectId> picked;
        std::vector<ObjectId> rest;
        for (std::size_t i = 0; i < n; ++i)
            (selected[i] ? picked : rest).push_back(after[i]);
        after = front ? rest : picked;
        after.insert(after.end(), front ? picked.begin() : rest.begin(),
                     front ? picked.end() : rest.end());
        text = front ? QCoreApplication::translate("hmi::EditActions", "Bring to front")
                     : QCoreApplication::translate("hmi::EditActions", "Send to back");
        break;
    }
    }
    if (after == before)
        return false;
    m_context.undoStack().push(new ReorderCommand(m_context.project(), m_context.screenId(),
                                                  std::move(before), std::move(after), text));
    return true;
}

} // namespace hmi
