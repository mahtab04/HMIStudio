#include "core/model/Project.h"

#include <algorithm>

namespace hmi
{

Project::Project(QString name)
    : m_name(std::move(name))
    , m_notifier(std::make_unique<DocumentNotifier>())
{
}

// Screens detach from the notifier first so nothing emits while the project
// is being destroyed.
Project::~Project()
{
    for (const auto& screen : m_screens)
        screen->setNotifier(nullptr);
}

Screen* Project::screen(ScreenId id) const noexcept
{
    const auto index = indexOf(id);
    return index ? m_screens[*index].get() : nullptr;
}

std::optional<std::size_t> Project::indexOf(ScreenId id) const noexcept
{
    const auto it =
        std::ranges::find_if(m_screens, [id](const auto& screen) { return screen->id() == id; });
    if (it == m_screens.end())
        return std::nullopt;
    return std::size_t(it - m_screens.begin());
}

Result<Screen*> Project::insertScreen(std::unique_ptr<Screen> screen,
                                      std::optional<std::size_t> index)
{
    if (!screen)
        return makeError(ErrorCode::InvalidArgument, QStringLiteral("Cannot insert a null screen"));
    if (indexOf(screen->id()))
    {
        return makeError(
            ErrorCode::Duplicate,
            QStringLiteral("Screen %1 is already in the project").arg(screen->id().toString()));
    }
    const std::size_t at = index.value_or(m_screens.size());
    if (at > m_screens.size())
    {
        return makeError(ErrorCode::OutOfRange,
                         QStringLiteral("Insert index %1 is past the end (%2 screens)")
                             .arg(at)
                             .arg(m_screens.size()));
    }

    Screen* raw = screen.get();
    raw->setNotifier(m_notifier.get());
    m_screens.insert(m_screens.begin() + std::ptrdiff_t(at), std::move(screen));
    Q_EMIT m_notifier->screenAdded(raw->id(), int(at));
    return raw;
}

Result<std::unique_ptr<Screen>> Project::takeScreen(ScreenId id)
{
    const auto index = indexOf(id);
    if (!index)
    {
        return makeError(ErrorCode::NotFound,
                         QStringLiteral("Screen %1 is not in the project").arg(id.toString()));
    }
    std::unique_ptr<Screen> screen = std::move(m_screens[*index]);
    m_screens.erase(m_screens.begin() + std::ptrdiff_t(*index));
    screen->setNotifier(nullptr);
    Q_EMIT m_notifier->screenRemoved(id);
    return screen;
}

} // namespace hmi
