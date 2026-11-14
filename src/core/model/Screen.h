#pragma once

#include "core/Result.h"
#include "core/model/HmiObject.h"
#include "core/model/Ids.h"

#include <QColor>
#include <QSizeF>
#include <QString>

#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace hmi
{

class DocumentNotifier;

// One HMI screen: a size, a background and the objects on it in z-order
// (index 0 is drawn first, at the bottom). The screen owns its objects.
// Inside a Project, every change is announced through the project's
// DocumentNotifier; a screen on its own (for example in a test fixture)
// changes silently.
class Screen
{
public:
    static constexpr QSizeF kDefaultSize{1280.0, 800.0};

    explicit Screen(QString name, QSizeF size = kDefaultSize, ScreenId id = ScreenId::create());
    ~Screen();
    Screen(const Screen&) = delete;
    Screen& operator=(const Screen&) = delete;
    Screen(Screen&&) = delete;
    Screen& operator=(Screen&&) = delete;

    // Deep copy with the same screen and object ids.
    [[nodiscard]] std::unique_ptr<Screen> clone() const;

    [[nodiscard]] ScreenId id() const noexcept { return m_id; }

    [[nodiscard]] const QString& name() const noexcept { return m_name; }
    void setName(const QString& name);

    [[nodiscard]] QSizeF size() const noexcept { return m_size; }
    Result<void> setSize(QSizeF size);

    [[nodiscard]] const QColor& backgroundColor() const noexcept { return m_background; }
    Result<void> setBackgroundColor(const QColor& color);

    [[nodiscard]] std::size_t objectCount() const noexcept { return m_objects.size(); }
    [[nodiscard]] std::span<const std::unique_ptr<HmiObject>> objects() const noexcept
    {
        return m_objects;
    }
    [[nodiscard]] HmiObject* object(ObjectId id) const noexcept;
    [[nodiscard]] std::optional<std::size_t> indexOf(ObjectId id) const noexcept;

    // Inserts at `index` (default: on top). Fails for a null object, a
    // duplicate id or an index past the end. Returns the inserted object.
    Result<HmiObject*> insertObject(std::unique_ptr<HmiObject> object,
                                    std::optional<std::size_t> index = std::nullopt);

    // Removes an object and hands ownership back, for undo.
    Result<std::unique_ptr<HmiObject>> takeObject(ObjectId id);

    // Moves an object to a new z-index.
    Result<void> moveObject(ObjectId id, std::size_t newIndex);

private:
    friend class Project;

    void setNotifier(DocumentNotifier* notifier) noexcept;
    void notifyChanged();

    ScreenId m_id;
    QString m_name;
    QSizeF m_size;
    QColor m_background{0xE6, 0xE8, 0xEB};
    std::vector<std::unique_ptr<HmiObject>> m_objects;
    DocumentNotifier* m_notifier = nullptr;
};

} // namespace hmi
