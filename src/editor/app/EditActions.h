#pragma once

#include "core/model/Ids.h"

#include <QPointF>
#include <QString>

#include <memory>
#include <vector>

namespace hmi
{

class HmiObject;
class ToolContext;

// The editing operations on the selection of the current screen: delete,
// clipboard, duplicate, nudge, group, align, distribute, same size and
// z-order. Each operation pushes one undo step (a command or a macro) and
// returns false when there was nothing to do. Reached from QML through
// EditorController, so keyboard, context menu and the touch toolbar share
// the same code.
//
// Locked objects can be selected, copied, deleted, grouped and reordered,
// but operations that move or resize leave them in place.
class EditActions
{
public:
    enum class Alignment
    {
        Left,
        HorizontalCentre,
        Right,
        Top,
        VerticalCentre,
        Bottom,
    };

    enum class Order
    {
        Forward,
        Backward,
        ToFront,
        ToBack,
    };

    enum class SizeMatch
    {
        Width,
        Height,
        Both,
    };

    // Offset of a paste or duplicate from the original, in scene units.
    static constexpr double kPasteOffset = 16.0;
    // Clipboard MIME type: UTF-8 JSON {"formatVersion": 1, "objects": [...]}.
    static constexpr const char* kMimeType = "application/x-hmistudio-objects";
    static constexpr int kClipboardFormatVersion = 1;

    explicit EditActions(ToolContext& context);

    void selectAll();
    bool deleteSelection();
    bool duplicate();
    bool copy();
    bool cut();
    bool paste();
    bool nudge(QPointF delta);
    bool group();
    bool ungroup();
    // With one object selected, aligns it on the screen; with more, aligns
    // them on the rectangle around the selection.
    bool align(Alignment alignment);
    // Equal gaps between three or more objects, along x or y.
    bool distribute(Qt::Orientation orientation);
    // Gives every selected object the size of the first selected one.
    bool matchSize(SizeMatch match);
    bool reorder(Order order);

    [[nodiscard]] bool hasSelection() const;
    [[nodiscard]] bool canGroup() const;
    [[nodiscard]] bool canUngroup() const;
    [[nodiscard]] bool canPaste() const;
    [[nodiscard]] bool canAlign() const;
    [[nodiscard]] bool canDistribute() const;
    [[nodiscard]] bool canMatchSize() const;

private:
    // Selected objects in z-order (bottom first); optionally without locked
    // ones.
    [[nodiscard]] std::vector<HmiObject*> selectedObjects(bool skipLocked) const;
    [[nodiscard]] std::vector<ObjectId> selectedIds() const;
    // Adds objects (already with fresh ids) moved by `offset` on top, in one
    // undo step, and selects them.
    bool insertCopies(std::vector<std::unique_ptr<HmiObject>> objects, QPointF offset,
                      const QString& text);

    ToolContext& m_context;
    int m_pasteCount = 0;
};

} // namespace hmi
