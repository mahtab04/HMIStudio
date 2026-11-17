#pragma once

#include <QByteArray>
#include <QObject>
#include <QStringList>
#include <qqmlintegration.h>

#include <memory>

class QSettings;

namespace hmi
{

// User settings that outlive a session: input density, panel visibility
// and sizes, recent projects, autosave interval. Backed by QSettings; every
// change is written immediately. Created by AppContext (tests pass their own
// file) and handed to QML as a required property.
class AppSettings : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("AppSettings is provided by the application")
    Q_PROPERTY(bool toolboxPanelVisible READ toolboxPanelVisible WRITE setToolboxPanelVisible NOTIFY
                   panelsChanged)
    Q_PROPERTY(bool propertiesPanelVisible READ propertiesPanelVisible WRITE
                   setPropertiesPanelVisible NOTIFY panelsChanged)
    Q_PROPERTY(bool bottomPanelVisible READ bottomPanelVisible WRITE setBottomPanelVisible NOTIFY
                   panelsChanged)
    // SplitView.saveState() of the main and the centre split.
    Q_PROPERTY(
        QByteArray mainSplitState READ mainSplitState WRITE setMainSplitState NOTIFY panelsChanged)
    Q_PROPERTY(QByteArray centreSplitState READ centreSplitState WRITE setCentreSplitState NOTIFY
                   panelsChanged)
    Q_PROPERTY(QStringList recentProjects READ recentProjects NOTIFY recentProjectsChanged)

public:
    static constexpr int kMaxRecentProjects = 8;
    static constexpr int kDefaultAutosaveSeconds = 60;

    // `file` empty: the platform's standard location for the application.
    explicit AppSettings(const QString& file = {}, QObject* parent = nullptr);
    ~AppSettings() override;
    AppSettings(const AppSettings&) = delete;
    AppSettings& operator=(const AppSettings&) = delete;
    AppSettings(AppSettings&&) = delete;
    AppSettings& operator=(AppSettings&&) = delete;

    // Density as Theme::Density's value, or -1 when never chosen.
    [[nodiscard]] int density() const;
    void setDensity(int density);

    [[nodiscard]] bool toolboxPanelVisible() const;
    void setToolboxPanelVisible(bool visible);
    [[nodiscard]] bool propertiesPanelVisible() const;
    void setPropertiesPanelVisible(bool visible);
    [[nodiscard]] bool bottomPanelVisible() const;
    void setBottomPanelVisible(bool visible);
    [[nodiscard]] QByteArray mainSplitState() const;
    void setMainSplitState(const QByteArray& state);
    [[nodiscard]] QByteArray centreSplitState() const;
    void setCentreSplitState(const QByteArray& state);

    [[nodiscard]] QStringList recentProjects() const;
    // Moves `folder` to the front of the list.
    void addRecentProject(const QString& folder);
    void removeRecentProject(const QString& folder);

    [[nodiscard]] int autosaveSeconds() const;

    // Native PLC libraries the user chose on this computer; only these are
    // loaded when a project names a library (see PlcDriver).
    [[nodiscard]] QStringList trustedLibraries() const;
    void trustLibrary(const QString& path);

Q_SIGNALS:
    void panelsChanged();
    void recentProjectsChanged();

private:
    void setBool(const QString& key, bool value);

    std::unique_ptr<QSettings> m_settings;
};

} // namespace hmi
