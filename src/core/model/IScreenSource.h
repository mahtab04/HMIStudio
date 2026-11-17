#pragma once

#include <QString>

namespace hmi
{

class Screen;

// Finds a screen of the project by name, for objects that show another
// screen (the screen window). Implemented by Project.
class IScreenSource
{
public:
    IScreenSource() = default;
    virtual ~IScreenSource() = default;
    IScreenSource(const IScreenSource&) = delete;
    IScreenSource& operator=(const IScreenSource&) = delete;
    IScreenSource(IScreenSource&&) = delete;
    IScreenSource& operator=(IScreenSource&&) = delete;

    // The screen called `name` (case-insensitive), or nullptr.
    [[nodiscard]] virtual const Screen* screenNamed(const QString& name) const = 0;
};

} // namespace hmi
