#include "editor/app/Theme.h"

#include <QInputDevice>

namespace hmi
{

Theme::Theme(QObject* parent)
    : QObject(parent)
    , m_density(densityForAttachedDevices())
{
}

Theme::Density Theme::densityForAttachedDevices()
{
    for (const QInputDevice* device : QInputDevice::devices())
    {
        if (device->type() == QInputDevice::DeviceType::TouchScreen)
            return Density::Comfortable;
    }
    return Density::Compact;
}

void Theme::setDensity(Density density)
{
    if (density == m_density)
        return;
    m_density = density;
    Q_EMIT densityChanged();
}

QString Theme::densityName() const
{
    return comfortable() ? tr("Touch") : tr("Mouse");
}

void Theme::toggleDensity()
{
    setDensity(comfortable() ? Density::Compact : Density::Comfortable);
}

int Theme::controlHeight() const noexcept
{
    return comfortable() ? kTouchTarget : 32;
}

int Theme::toolboxTile() const noexcept
{
    return comfortable() ? 84 : 72;
}

int Theme::iconSize() const noexcept
{
    return comfortable() ? 24 : 20;
}

int Theme::spacing() const noexcept
{
    return comfortable() ? 10 : 8;
}

int Theme::padding() const noexcept
{
    return comfortable() ? 14 : 10;
}

int Theme::sidePanelWidth() const noexcept
{
    return comfortable() ? 340 : 290;
}

QFont Theme::fontOfSize(int pixelSize, bool bold) const
{
    QFont f;
    f.setPixelSize(pixelSize + (comfortable() ? 2 : 0));
    f.setBold(bold);
    return f;
}

QFont Theme::font() const
{
    return fontOfSize(13);
}

QFont Theme::fontSmall() const
{
    return fontOfSize(11);
}

QFont Theme::fontHeading() const
{
    return fontOfSize(11, true);
}

QPalette Theme::palette() const
{
    QPalette p;
    p.setColor(QPalette::Window, m_panel);
    p.setColor(QPalette::WindowText, m_text);
    p.setColor(QPalette::Base, m_window);
    p.setColor(QPalette::AlternateBase, m_panelRaised);
    p.setColor(QPalette::Text, m_text);
    p.setColor(QPalette::PlaceholderText, m_textMuted);
    p.setColor(QPalette::Button, m_panelRaised);
    p.setColor(QPalette::ButtonText, m_text);
    p.setColor(QPalette::Highlight, m_accent);
    p.setColor(QPalette::HighlightedText, m_accentText);
    p.setColor(QPalette::ToolTipBase, m_panelRaised);
    p.setColor(QPalette::ToolTipText, m_text);
    p.setColor(QPalette::Mid, m_border);
    p.setColor(QPalette::Dark, m_window);
    p.setColor(QPalette::Light, m_panelRaised);
    p.setColor(QPalette::Disabled, QPalette::Text, m_textMuted);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, m_textMuted);
    p.setColor(QPalette::Disabled, QPalette::WindowText, m_textMuted);
    return p;
}

} // namespace hmi
