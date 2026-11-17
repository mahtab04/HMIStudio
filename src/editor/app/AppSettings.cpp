#include "editor/app/AppSettings.h"

#include <QDir>
#include <QSettings>

namespace hmi
{

namespace
{

const auto kDensity = QStringLiteral("ui/density");
const auto kToolbox = QStringLiteral("ui/toolboxPanelVisible");
const auto kProperties = QStringLiteral("ui/propertiesPanelVisible");
const auto kBottom = QStringLiteral("ui/bottomPanelVisible");
// Version 2: sizes saved before side panels had a maximum width could make
// a panel take half the window; they are not restored.
const auto kMainSplit = QStringLiteral("ui/mainSplitState2");
const auto kCentreSplit = QStringLiteral("ui/centreSplitState2");
const auto kRecent = QStringLiteral("projects/recent");
const auto kAutosave = QStringLiteral("projects/autosaveSeconds");

} // namespace

AppSettings::AppSettings(const QString& file, QObject* parent)
    : QObject(parent)
    , m_settings(file.isEmpty() ? std::make_unique<QSettings>()
                                : std::make_unique<QSettings>(file, QSettings::IniFormat))
{
}

AppSettings::~AppSettings() = default;

int AppSettings::density() const
{
    return m_settings->value(kDensity, -1).toInt();
}

void AppSettings::setDensity(int density)
{
    m_settings->setValue(kDensity, density);
}

void AppSettings::setBool(const QString& key, bool value)
{
    if (m_settings->value(key, true).toBool() == value)
        return;
    m_settings->setValue(key, value);
    Q_EMIT panelsChanged();
}

bool AppSettings::toolboxPanelVisible() const
{
    return m_settings->value(kToolbox, true).toBool();
}

void AppSettings::setToolboxPanelVisible(bool visible)
{
    setBool(kToolbox, visible);
}

bool AppSettings::propertiesPanelVisible() const
{
    return m_settings->value(kProperties, true).toBool();
}

void AppSettings::setPropertiesPanelVisible(bool visible)
{
    setBool(kProperties, visible);
}

bool AppSettings::bottomPanelVisible() const
{
    return m_settings->value(kBottom, true).toBool();
}

void AppSettings::setBottomPanelVisible(bool visible)
{
    setBool(kBottom, visible);
}

QByteArray AppSettings::mainSplitState() const
{
    return m_settings->value(kMainSplit).toByteArray();
}

void AppSettings::setMainSplitState(const QByteArray& state)
{
    if (state == mainSplitState())
        return;
    m_settings->setValue(kMainSplit, state);
    Q_EMIT panelsChanged();
}

QByteArray AppSettings::centreSplitState() const
{
    return m_settings->value(kCentreSplit).toByteArray();
}

void AppSettings::setCentreSplitState(const QByteArray& state)
{
    if (state == centreSplitState())
        return;
    m_settings->setValue(kCentreSplit, state);
    Q_EMIT panelsChanged();
}

QStringList AppSettings::recentProjects() const
{
    return m_settings->value(kRecent).toStringList();
}

void AppSettings::addRecentProject(const QString& folder)
{
    const QString path = QDir::cleanPath(folder);
    QStringList recent = recentProjects();
    recent.removeAll(path);
    recent.prepend(path);
    while (recent.size() > kMaxRecentProjects)
        recent.removeLast();
    m_settings->setValue(kRecent, recent);
    Q_EMIT recentProjectsChanged();
}

void AppSettings::removeRecentProject(const QString& folder)
{
    QStringList recent = recentProjects();
    if (recent.removeAll(QDir::cleanPath(folder)) == 0)
        return;
    m_settings->setValue(kRecent, recent);
    Q_EMIT recentProjectsChanged();
}

QStringList AppSettings::trustedLibraries() const
{
    return m_settings->value(QStringLiteral("plc/trustedLibraries")).toStringList();
}

void AppSettings::trustLibrary(const QString& path)
{
    const QString clean = QDir::cleanPath(path);
    QStringList trusted = trustedLibraries();
    if (clean.isEmpty() || trusted.contains(clean))
        return;
    trusted.append(clean);
    m_settings->setValue(QStringLiteral("plc/trustedLibraries"), trusted);
}

int AppSettings::autosaveSeconds() const
{
    return m_settings->value(kAutosave, kDefaultAutosaveSeconds).toInt();
}

} // namespace hmi
