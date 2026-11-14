#pragma once

#include "core/Result.h"
#include "core/model/DocumentNotifier.h"
#include "core/model/Ids.h"
#include "core/model/Screen.h"

#include <QString>

#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace hmi
{

// Root of the document tree: owns the screens (tags and connections arrive
// in phase 6) and the document's DocumentNotifier. Screens added here, and
// their objects, report every change through notifier().
class Project
{
public:
    explicit Project(QString name = {});
    ~Project();
    Project(const Project&) = delete;
    Project& operator=(const Project&) = delete;
    Project(Project&&) = delete;
    Project& operator=(Project&&) = delete;

    [[nodiscard]] const QString& name() const noexcept { return m_name; }
    void setName(QString name) { m_name = std::move(name); }

    [[nodiscard]] DocumentNotifier& notifier() const noexcept { return *m_notifier; }

    [[nodiscard]] std::size_t screenCount() const noexcept { return m_screens.size(); }
    [[nodiscard]] std::span<const std::unique_ptr<Screen>> screens() const noexcept
    {
        return m_screens;
    }
    [[nodiscard]] Screen* screen(ScreenId id) const noexcept;
    [[nodiscard]] std::optional<std::size_t> indexOf(ScreenId id) const noexcept;

    // Adds a screen at `index` (default: last). Fails for null, duplicate id
    // or an index past the end.
    Result<Screen*> insertScreen(std::unique_ptr<Screen> screen,
                                 std::optional<std::size_t> index = std::nullopt);

    // Removes a screen and hands ownership back, for undo.
    Result<std::unique_ptr<Screen>> takeScreen(ScreenId id);

private:
    QString m_name;
    std::unique_ptr<DocumentNotifier> m_notifier;
    std::vector<std::unique_ptr<Screen>> m_screens;
};

} // namespace hmi
