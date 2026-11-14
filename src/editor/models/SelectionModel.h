#pragma once

#include "core/model/Ids.h"

#include <QObject>

#include <vector>

namespace hmi
{

// The selected objects of the current screen, in selection order. Selection
// is editor state, not document state, so it changes outside the undo stack.
// `multiSelectMode` makes taps add to the selection, the touch alternative
// to Shift+click.
class SelectionModel : public QObject
{
    Q_OBJECT

public:
    explicit SelectionModel(QObject* parent = nullptr);

    [[nodiscard]] const std::vector<ObjectId>& ids() const noexcept { return m_ids; }
    [[nodiscard]] std::size_t size() const noexcept { return m_ids.size(); }
    [[nodiscard]] bool isEmpty() const noexcept { return m_ids.empty(); }
    [[nodiscard]] bool contains(ObjectId id) const;

    void set(std::vector<ObjectId> ids);
    void selectOnly(ObjectId id);
    void add(ObjectId id);
    void toggle(ObjectId id);
    void remove(ObjectId id);
    void clear();

    [[nodiscard]] bool multiSelectMode() const noexcept { return m_multiSelectMode; }
    void setMultiSelectMode(bool enabled);

Q_SIGNALS:
    void changed();
    void multiSelectModeChanged();

private:
    std::vector<ObjectId> m_ids;
    bool m_multiSelectMode = false;
};

} // namespace hmi
