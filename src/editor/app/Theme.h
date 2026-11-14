#pragma once

#include <QColor>
#include <QFont>
#include <QObject>
#include <QPalette>
#include <qqmlintegration.h>

namespace hmi
{

// Colours, spacing and fonts for the shell, and the compact (mouse) /
// comfortable (touch) density switch. QML takes every size and colour from
// here; nothing is hard-coded in QML. Passed to QML as a required property,
// not a singleton.
class Theme : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Theme is provided by the application")

    Q_PROPERTY(Density density READ density WRITE setDensity NOTIFY densityChanged)
    Q_PROPERTY(bool comfortable READ comfortable NOTIFY densityChanged)
    Q_PROPERTY(QString densityName READ densityName NOTIFY densityChanged)

    Q_PROPERTY(QColor window MEMBER m_window CONSTANT)
    Q_PROPERTY(QColor panel MEMBER m_panel CONSTANT)
    Q_PROPERTY(QColor panelRaised MEMBER m_panelRaised CONSTANT)
    Q_PROPERTY(QColor border MEMBER m_border CONSTANT)
    Q_PROPERTY(QColor text MEMBER m_text CONSTANT)
    Q_PROPERTY(QColor textMuted MEMBER m_textMuted CONSTANT)
    Q_PROPERTY(QColor accent MEMBER m_accent CONSTANT)
    Q_PROPERTY(QColor accentText MEMBER m_accentText CONSTANT)
    Q_PROPERTY(QColor canvasBackground MEMBER m_canvasBackground CONSTANT)
    Q_PROPERTY(QColor selection MEMBER m_selection CONSTANT)
    Q_PROPERTY(QColor handleFill MEMBER m_handleFill CONSTANT)
    Q_PROPERTY(QColor rubberBand MEMBER m_rubberBand CONSTANT)
    Q_PROPERTY(QColor grid MEMBER m_grid CONSTANT)
    Q_PROPERTY(QColor rulerBackground MEMBER m_rulerBackground CONSTANT)
    Q_PROPERTY(QColor rulerTick MEMBER m_rulerTick CONSTANT)
    Q_PROPERTY(QColor good MEMBER m_good CONSTANT)

    Q_PROPERTY(int touchTarget READ touchTarget CONSTANT)
    Q_PROPERTY(int controlHeight READ controlHeight NOTIFY densityChanged)
    Q_PROPERTY(int toolboxTile READ toolboxTile NOTIFY densityChanged)
    Q_PROPERTY(int iconSize READ iconSize NOTIFY densityChanged)
    Q_PROPERTY(int spacing READ spacing NOTIFY densityChanged)
    Q_PROPERTY(int padding READ padding NOTIFY densityChanged)
    Q_PROPERTY(int radius READ radius CONSTANT)
    Q_PROPERTY(int rulerSize READ rulerSize CONSTANT)
    Q_PROPERTY(int sidePanelWidth READ sidePanelWidth NOTIFY densityChanged)
    Q_PROPERTY(int bottomPanelHeight READ bottomPanelHeight CONSTANT)

    Q_PROPERTY(QFont font READ font NOTIFY densityChanged)
    Q_PROPERTY(QFont fontSmall READ fontSmall NOTIFY densityChanged)
    Q_PROPERTY(QFont fontHeading READ fontHeading NOTIFY densityChanged)

public:
    enum class Density
    {
        Compact,     // mouse and keyboard
        Comfortable, // touch: every control at least touchTarget high
    };
    Q_ENUM(Density)

    static constexpr int kTouchTarget = 44;

    explicit Theme(QObject* parent = nullptr);

    // Comfortable when a touch screen is attached, otherwise compact.
    [[nodiscard]] static Density densityForAttachedDevices();

    [[nodiscard]] Density density() const noexcept { return m_density; }
    void setDensity(Density density);
    [[nodiscard]] bool comfortable() const noexcept { return m_density == Density::Comfortable; }
    [[nodiscard]] QString densityName() const;
    Q_INVOKABLE void toggleDensity();

    [[nodiscard]] static int touchTarget() noexcept { return kTouchTarget; }
    [[nodiscard]] int controlHeight() const noexcept;
    [[nodiscard]] int toolboxTile() const noexcept;
    [[nodiscard]] int iconSize() const noexcept;
    [[nodiscard]] int spacing() const noexcept;
    [[nodiscard]] int padding() const noexcept;
    [[nodiscard]] static int radius() noexcept { return 6; }
    [[nodiscard]] static int rulerSize() noexcept { return 20; }
    [[nodiscard]] int sidePanelWidth() const noexcept;
    [[nodiscard]] static int bottomPanelHeight() noexcept { return 220; }

    [[nodiscard]] QFont font() const;
    [[nodiscard]] QFont fontSmall() const;
    [[nodiscard]] QFont fontHeading() const;

    // Palette for the Qt Quick Controls style, matching these colours.
    [[nodiscard]] QPalette palette() const;

Q_SIGNALS:
    void densityChanged();

private:
    [[nodiscard]] QFont fontOfSize(int pixelSize, bool bold = false) const;

    Density m_density = Density::Compact;

    QColor m_window{0x0F, 0x11, 0x14};
    QColor m_panel{0x16, 0x19, 0x1D};
    QColor m_panelRaised{0x1E, 0x22, 0x27};
    QColor m_border{0x2A, 0x2F, 0x36};
    QColor m_text{0xE6, 0xE8, 0xEB};
    QColor m_textMuted{0x8B, 0x91, 0x99};
    QColor m_accent{0x3B, 0x82, 0xF6};
    QColor m_accentText{Qt::white};
    QColor m_canvasBackground{0x11, 0x13, 0x16};
    QColor m_selection{0x4C, 0x8D, 0xFF};
    QColor m_handleFill{Qt::white};
    QColor m_rubberBand{0x4C, 0x8D, 0xFF, 0x30};
    QColor m_grid{0x00, 0x00, 0x00, 0x14};
    QColor m_rulerBackground{0x16, 0x19, 0x1D};
    QColor m_rulerTick{0x4A, 0x50, 0x58};
    QColor m_good{0x22, 0xC5, 0x5E};
};

} // namespace hmi
