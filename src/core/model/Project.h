#pragma once

#include "core/Result.h"
#include "core/alarms/Alarm.h"
#include "core/graphics/AssetStore.h"
#include "core/model/DocumentNotifier.h"
#include "core/model/HmiStyle.h"
#include "core/model/IScreenSource.h"
#include "core/model/Ids.h"
#include "core/model/Screen.h"
#include "core/tags/Connection.h"
#include "core/tags/TagDatabase.h"

#include <QJsonObject>
#include <QString>

#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace hmi
{

// Root of the document tree: owns screens, tags, connection definitions,
// assets and the document's DocumentNotifier. Screens added here, and
// their objects, report every change through notifier().
class Project : public IScreenSource
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
    // Optional, explicitly saved demo process model. Empty uses independent
    // per-tag generators. Never used by PLC monitoring.
    [[nodiscard]] const QString& simulationProfile() const noexcept { return m_simulationProfile; }
    Result<void> setSimulationProfile(QString profile);

    // project.json data this version does not understand; written back as is.
    [[nodiscard]] const QJsonObject& preservedJson() const noexcept { return m_preserved; }
    void setPreservedJson(QJsonObject json) { m_preserved = std::move(json); }

    // Replaces name, screens, assets and preserved data with those of `other` (new
    // project, open). This object, and so its notifier, stays the same, so
    // everything holding a Project& keeps working. Emits
    // DocumentNotifier::projectReset; the old screens are destroyed only
    // after the signal, when views have moved to the new ones.
    void replaceContents(Project&& other);

    [[nodiscard]] DocumentNotifier& notifier() const noexcept { return *m_notifier; }

    // Shared look for new objects. Setting it announces styleChanged.
    [[nodiscard]] const HmiStyle& style() const noexcept { return m_style; }
    void setStyle(const HmiStyle& style);

    // The project's tags; inside the project every change is announced.
    [[nodiscard]] TagDatabase& tags() const noexcept { return *m_tags; }

    [[nodiscard]] std::span<const Connection> connections() const noexcept { return m_connections; }
    [[nodiscard]] const Connection* connection(const QString& id) const;
    Result<void> setConnections(std::vector<Connection> connections);

    // Alarms and alarm classes (HMI alarms). Rejected when not valid
    // (AlarmConfiguration::validate); announced by alarmsChanged.
    [[nodiscard]] const AlarmConfiguration& alarms() const noexcept { return m_alarms; }
    Result<void> setAlarms(AlarmConfiguration alarms);

    // Imported images and SVG files used by graphic objects.
    [[nodiscard]] AssetStore& assets() const noexcept { return *m_assets; }

    [[nodiscard]] std::size_t screenCount() const noexcept { return m_screens.size(); }
    [[nodiscard]] std::span<const std::unique_ptr<Screen>> screens() const noexcept
    {
        return m_screens;
    }
    [[nodiscard]] Screen* screen(ScreenId id) const noexcept;
    [[nodiscard]] const Screen* screenNamed(const QString& name) const override;
    [[nodiscard]] std::optional<std::size_t> indexOf(ScreenId id) const noexcept;

    // Adds a screen at `index` (default: last). Fails for null, duplicate id
    // or an index past the end.
    Result<Screen*> insertScreen(std::unique_ptr<Screen> screen,
                                 std::optional<std::size_t> index = std::nullopt);

    // Removes a screen and hands ownership back, for undo.
    Result<std::unique_ptr<Screen>> takeScreen(ScreenId id);

private:
    std::vector<Connection> m_connections;
    AlarmConfiguration m_alarms;
    QString m_name;
    QString m_simulationProfile;
    QJsonObject m_preserved;
    HmiStyle m_style;
    std::unique_ptr<DocumentNotifier> m_notifier;
    std::unique_ptr<AssetStore> m_assets = std::make_unique<AssetStore>();
    std::unique_ptr<TagDatabase> m_tags = std::make_unique<TagDatabase>();
    std::vector<std::unique_ptr<Screen>> m_screens;
};

} // namespace hmi
