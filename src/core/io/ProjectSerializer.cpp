#include "core/io/ProjectSerializer.h"

#include "core/io/ObjectJson.h"
#include "core/model/HmiObject.h"
#include "core/model/Project.h"
#include "core/model/Screen.h"
#include "core/property/Prop.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>

#include <algorithm>
#include <set>

namespace hmi::ProjectSerializer
{

namespace
{

const auto kFormatVersion = QStringLiteral("formatVersion");
const auto kName = QStringLiteral("name");
const auto kScreens = QStringLiteral("screens");
const auto kId = QStringLiteral("id");
const auto kSize = QStringLiteral("size");
const auto kBackground = QStringLiteral("background");
const auto kObjects = QStringLiteral("objects");
const auto kAssets = QStringLiteral("assets");
const auto kFile = QStringLiteral("file");
const auto kStyle = QStringLiteral("style");
const auto kTags = QStringLiteral("tags");
const auto kTagsFile = QStringLiteral("tags.json");
const auto kAlarmsFile = QStringLiteral("alarms.json");

QString assetPath(const Asset& asset)
{
    const QString suffix = asset.suffix.isEmpty() ? QStringLiteral("bin") : asset.suffix;
    return QStringLiteral("%1/%2.%3").arg(QLatin1StringView(kAssetsFolder), asset.id, suffix);
}

void collectAssets(const HmiObject& object, std::set<QString>& ids)
{
    if (const auto value = object.property(Prop::Asset))
    {
        if (const auto* id = std::get_if<QString>(&*value); id && !id->isEmpty())
            ids.insert(*id);
    }
    for (const auto& child : object.children())
        collectAssets(*child, ids);
}

bool isInside(const QString& path, const char* folder)
{
    return path.startsWith(QLatin1StringView(folder) + u'/') && !path.contains(u"..");
}

// Migration from version `from` to `from + 1`, for every kind of file. Each
// format change appends one step; a step changes only what that version
// changed.
struct Migration
{
    int from;
    void (*apply)(QJsonObject& file);
};

// Version 0: files from before the format was versioned. Their content is
// the same as version 1; only formatVersion was missing.
void migrateV0ToV1(QJsonObject& /*file*/) {}

// Version 2 adds optional connection definitions and tag organization.
// Legacy tags keep their existing simulator settings and default table.
void migrateV1ToV2(QJsonObject& /*file*/) {}

// Version 3 adds optional button operations and a saved demo simulation
// profile. Missing operations default to None; old projects keep tag simulation.
void migrateV2ToV3(QJsonObject& /*file*/) {}

// Version 4 replaces the single tag operation of buttons and touch areas
// (props action, actionTag, actionValue) with event action lists: Set,
// Toggle and Increment become Click actions; Momentary becomes "set true"
// on Press and "set false" on Release.
void migrateObjectV3ToV4(QJsonObject& object)
{
    QJsonObject props = object.value(QStringLiteral("props")).toObject();
    if (props.contains(QStringLiteral("action")))
    {
        const QString operation = props.value(QStringLiteral("action")).toString();
        const QString tag = props.value(QStringLiteral("actionTag")).toString();
        const QString value =
            props.value(QStringLiteral("actionValue")).toString(QStringLiteral("1"));
        for (const char* key : {"action", "actionTag", "actionValue"})
            props.remove(QLatin1StringView(key));
        object.insert(QStringLiteral("props"), props);

        QJsonObject events = object.value(QStringLiteral("events")).toObject();
        const auto append = [&events](const char* event, QJsonObject action)
        {
            QJsonArray list = events.value(QLatin1StringView(event)).toArray();
            list.append(action);
            events.insert(QLatin1StringView(event), list);
        };
        const auto tagAction = [&tag](const char* kind, const QString& actionValue)
        {
            QJsonObject action{{QStringLiteral("action"), QLatin1StringView(kind)},
                               {QStringLiteral("tag"), tag}};
            if (!actionValue.isNull())
                action.insert(QStringLiteral("value"), actionValue);
            return action;
        };
        if (operation == u"set")
            append("click", tagAction("setTag", value));
        else if (operation == u"toggle")
            append("click", tagAction("toggleTag", QString()));
        else if (operation == u"increment")
            append("click", tagAction("incrementTag", value));
        else if (operation == u"momentary")
        {
            append("press", tagAction("setTag", QStringLiteral("true")));
            append("release", tagAction("setTag", QStringLiteral("false")));
        }
        if (!events.isEmpty())
            object.insert(QStringLiteral("events"), events);
    }
    if (object.contains(QStringLiteral("children")))
    {
        QJsonArray children = object.value(QStringLiteral("children")).toArray();
        for (QJsonValueRef child : children)
        {
            QJsonObject entry = child.toObject();
            migrateObjectV3ToV4(entry);
            child = entry;
        }
        object.insert(QStringLiteral("children"), children);
    }
}

void migrateV3ToV4(QJsonObject& file)
{
    if (!file.contains(kObjects))
        return; // project.json and tags.json have nothing to change
    QJsonArray objects = file.value(kObjects).toArray();
    for (QJsonValueRef entry : objects)
    {
        QJsonObject object = entry.toObject();
        migrateObjectV3ToV4(object);
        entry = object;
    }
    file.insert(kObjects, objects);
}

constexpr Migration kMigrations[] = {
    {0, &migrateV0ToV1},
    {1, &migrateV1ToV2},
    {2, &migrateV2ToV3},
    {3, &migrateV3ToV4},
};
static_assert(std::size(kMigrations) == kCurrentFormatVersion,
              "every version needs a migration from the one before");

// File name for the screen at `index`: "01_line_overview.json". The number
// keeps the files in screen order in a file browser; the name helps a
// reader of a diff.
QString screenFileName(std::size_t index, const QString& name)
{
    static const QRegularExpression notWord(QStringLiteral("[^a-z0-9]+"));
    QString slug = name.toLower().replace(notWord, QStringLiteral("_"));
    while (slug.startsWith(u'_'))
        slug.remove(0, 1);
    while (slug.endsWith(u'_'))
        slug.chop(1);
    slug.truncate(40);
    if (slug.isEmpty())
        slug = QStringLiteral("screen");
    return QStringLiteral("%1/%2_%3.json")
        .arg(QLatin1StringView(kScreensFolder))
        .arg(index + 1, 2, 10, QChar(u'0'))
        .arg(slug);
}

QJsonObject screenJson(const Screen& screen)
{
    QJsonObject json = screen.preservedJson();
    json.insert(kFormatVersion, kCurrentFormatVersion);
    json.insert(kId, screen.id().toString());
    json.insert(kName, screen.name());
    json.insert(kSize, QJsonArray{screen.size().width(), screen.size().height()});
    json.insert(kBackground, screen.backgroundColor().name(QColor::HexArgb));
    QJsonArray objects;
    for (const auto& object : screen.objects())
        objects.append(ObjectJson::write(*object));
    json.insert(kObjects, objects);
    return json;
}

Result<std::unique_ptr<Screen>> readScreen(const QJsonObject& json, const QString& path)
{
    const auto fail = [&path](const QString& what)
    {
        return makeError(ErrorCode::BadFormat, QStringLiteral("%1: %2").arg(path, what));
    };

    const auto id = ScreenId::fromString(json.value(kId).toString());
    if (!id)
        return fail(QStringLiteral("missing or invalid screen id"));
    const QJsonArray size = json.value(kSize).toArray();
    if (size.size() != 2 || size[0].toDouble() <= 0.0 || size[1].toDouble() <= 0.0)
        return fail(QStringLiteral("missing or invalid screen size"));

    auto screen = std::make_unique<Screen>(json.value(kName).toString(),
                                           QSizeF(size[0].toDouble(), size[1].toDouble()), *id);
    if (json.contains(kBackground))
    {
        if (auto set =
                screen->setBackgroundColor(QColor::fromString(json.value(kBackground).toString()));
            !set)
            return fail(set.error().message);
    }

    auto objects = ObjectJson::readAll(json.value(kObjects).toArray());
    if (!objects)
        return fail(objects.error().message);
    for (auto& object : *objects)
    {
        if (auto inserted = screen->insertObject(std::move(object)); !inserted)
            return fail(inserted.error().message);
    }

    QJsonObject preserved = json;
    for (const QString& known : {kFormatVersion, kId, kName, kSize, kBackground, kObjects})
        preserved.remove(known);
    screen->setPreservedJson(preserved);
    return screen;
}

Result<QJsonObject> readFile(const QString& folder, const QString& path)
{
    QFile file(QDir(folder).filePath(path));
    if (!file.open(QIODevice::ReadOnly))
    {
        return makeError(ErrorCode::Io,
                         QStringLiteral("Cannot read %1: %2").arg(path, file.errorString()));
    }
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
    {
        return makeError(ErrorCode::BadFormat,
                         QStringLiteral("%1 is not a valid project file (%2 at offset %3)")
                             .arg(path, error.errorString())
                             .arg(error.offset));
    }
    return document.object();
}

Result<void> writeFile(const QString& folder, const QString& path, const QJsonObject& json)
{
    // QSaveFile writes to a temporary file and renames it, so a crash never
    // leaves a half-written file behind.
    QSaveFile file(QDir(folder).filePath(path));
    if (!file.open(QIODevice::WriteOnly) || file.write(toBytes(json)) < 0 || !file.commit())
    {
        return makeError(ErrorCode::Io,
                         QStringLiteral("Cannot write %1: %2").arg(path, file.errorString()));
    }
    return {};
}

} // namespace

QByteArray toBytes(const QJsonObject& json)
{
    return QJsonDocument(json).toJson(QJsonDocument::Indented);
}

Result<QJsonObject> migrate(QJsonObject file, const QString& name)
{
    int version = file.value(kFormatVersion).toInt(0);
    if (version < 0)
        return makeError(ErrorCode::BadFormat, QStringLiteral("Negative file format version"));
    if (version > kCurrentFormatVersion)
    {
        return makeError(ErrorCode::TooNew,
                         QStringLiteral("%1 was saved by a newer version of HMI Studio (format %2; "
                                        "this version reads up to %3)")
                             .arg(name)
                             .arg(version)
                             .arg(kCurrentFormatVersion));
    }
    for (; version < kCurrentFormatVersion; ++version)
        kMigrations[version].apply(file);
    file.insert(kFormatVersion, kCurrentFormatVersion);
    return file;
}

std::vector<QString> usedAssets(const Project& project)
{
    std::set<QString> ids;
    for (const auto& screen : project.screens())
    {
        for (const auto& object : screen->objects())
            collectAssets(*object, ids);
    }
    return {ids.begin(), ids.end()};
}

std::vector<File> toFiles(const Project& project)
{
    std::vector<File> files;
    QJsonArray screenList;
    std::vector<File> screens;
    for (std::size_t i = 0; i < project.screenCount(); ++i)
    {
        const Screen& screen = *project.screens()[i];
        const QString path = screenFileName(i, screen.name());
        screenList.append(path);
        screens.emplace_back(path, screenJson(screen));
    }
    QJsonObject projectJson = project.preservedJson();
    projectJson.insert(kFormatVersion, kCurrentFormatVersion);
    projectJson.insert(kName, project.name());
    projectJson.insert(kScreens, screenList);
    projectJson.insert(kStyle, project.style().toJson());
    projectJson.insert(QStringLiteral("simulationProfile"), project.simulationProfile());
    QJsonArray connections;
    for (const Connection& connection : project.connections())
        connections.append(connection.toJson());
    projectJson.insert(QStringLiteral("connections"), connections);
    QJsonArray assetList;
    for (const QString& id : usedAssets(project))
    {
        if (const auto asset = project.assets().find(id))
        {
            assetList.append(QJsonObject{
                {kId, asset->id},
                {kFile, assetPath(*asset)},
                {kName, asset->fileName},
            });
        }
    }
    if (assetList.isEmpty())
        projectJson.remove(kAssets);
    else
        projectJson.insert(kAssets, assetList);
    files.emplace_back(QString::fromLatin1(kProjectFile), projectJson);

    QJsonArray tags;
    for (const Tag& tag : project.tags().tags())
        tags.append(tag.toJson());
    files.emplace_back(kTagsFile,
                       QJsonObject{{kFormatVersion, kCurrentFormatVersion}, {kTags, tags}});
    QJsonArray alarmClasses;
    for (const AlarmClass& alarmClass : project.alarms().classes)
        alarmClasses.append(alarmClass.toJson());
    QJsonArray alarms;
    for (const Alarm& alarm : project.alarms().alarms)
        alarms.append(alarm.toJson());
    files.emplace_back(kAlarmsFile, QJsonObject{{kFormatVersion, kCurrentFormatVersion},
                                                {QStringLiteral("classes"), alarmClasses},
                                                {QStringLiteral("alarms"), alarms}});
    files.insert(files.end(), std::make_move_iterator(screens.begin()),
                 std::make_move_iterator(screens.end()));
    return files;
}

Result<std::unique_ptr<Project>> fromFiles(const Reader& read, const BinaryReader& readAsset)
{
    const QString projectPath = QString::fromLatin1(kProjectFile);
    auto raw = read(projectPath);
    if (!raw)
        return tl::unexpected(raw.error());
    auto projectJson = migrate(std::move(*raw), projectPath);
    if (!projectJson)
        return tl::unexpected(projectJson.error());

    auto project = std::make_unique<Project>(projectJson->value(kName).toString());
    project->setStyle(HmiStyle::fromJson(projectJson->value(kStyle).toObject()));
    const auto profile = projectJson->value(QStringLiteral("simulationProfile"));
    if (!profile.isUndefined() && !profile.isString())
        return makeError(ErrorCode::BadFormat,
                         QStringLiteral("simulationProfile must be a string"));
    if (auto set = project->setSimulationProfile(profile.toString()); !set)
        return tl::unexpected(set.error());
    if (projectJson->contains(QStringLiteral("connections")) &&
        !projectJson->value(QStringLiteral("connections")).isArray())
        return makeError(ErrorCode::BadFormat, QStringLiteral("Connections must be an array"));
    std::vector<Connection> connections;
    for (const QJsonValue& value : projectJson->value(QStringLiteral("connections")).toArray())
    {
        auto connection = Connection::fromJson(value.toObject());
        if (!connection)
            return tl::unexpected(connection.error());
        connections.push_back(std::move(*connection));
    }
    if (auto set = project->setConnections(std::move(connections)); !set)
        return tl::unexpected(set.error());
    const QJsonArray screenList = projectJson->value(kScreens).toArray();
    for (const QJsonValue& entry : screenList)
    {
        const QString path = entry.toString();
        // Screen files live in the screens folder; never follow a path out
        // of the project.
        if (!path.startsWith(QLatin1StringView(kScreensFolder) + u'/') || path.contains(u".."))
            return makeError(ErrorCode::BadFormat,
                             QStringLiteral("Invalid screen file path '%1'").arg(path));
        auto screenRaw = read(path);
        if (!screenRaw)
            return tl::unexpected(screenRaw.error());
        auto screenJson = migrate(std::move(*screenRaw), path);
        if (!screenJson)
            return tl::unexpected(screenJson.error());
        auto screen = readScreen(*screenJson, path);
        if (!screen)
            return tl::unexpected(screen.error());
        if (auto inserted = project->insertScreen(std::move(*screen)); !inserted)
            return tl::unexpected(inserted.error());
    }
    if (project->screenCount() == 0)
        return makeError(ErrorCode::BadFormat, QStringLiteral("The project has no screens"));

    // Tags: a project without tags.json (older or hand-made) has none.
    if (auto tagsRaw = read(kTagsFile))
    {
        auto tagsJson = migrate(std::move(*tagsRaw), kTagsFile);
        if (!tagsJson)
            return tl::unexpected(tagsJson.error());
        for (const QJsonValue& entry : tagsJson->value(kTags).toArray())
        {
            auto tag = Tag::fromJson(entry.toObject());
            if (!tag)
                return makeError(ErrorCode::BadFormat,
                                 QStringLiteral("%1: %2").arg(kTagsFile, tag.error().message));
            if (auto inserted = project->tags().insert(std::move(*tag)); !inserted)
                return makeError(ErrorCode::BadFormat,
                                 QStringLiteral("%1: %2").arg(kTagsFile, inserted.error().message));
        }
    }
    else if (tagsRaw.error().code != ErrorCode::Io)
    {
        return tl::unexpected(tagsRaw.error());
    }

    // Alarms: a project without alarms.json has none, and the predefined
    // alarm classes.
    if (auto alarmsRaw = read(kAlarmsFile))
    {
        auto alarmsJson = migrate(std::move(*alarmsRaw), kAlarmsFile);
        if (!alarmsJson)
            return tl::unexpected(alarmsJson.error());
        AlarmConfiguration alarms;
        alarms.classes.clear();
        for (const QJsonValue& entry : alarmsJson->value(QStringLiteral("classes")).toArray())
        {
            auto alarmClass = AlarmClass::fromJson(entry.toObject());
            if (!alarmClass)
                return makeError(
                    ErrorCode::BadFormat,
                    QStringLiteral("%1: %2").arg(kAlarmsFile, alarmClass.error().message));
            alarms.classes.push_back(std::move(*alarmClass));
        }
        // Predefined classes a file leaves out (an older or hand-made one).
        for (AlarmClass& fixed : AlarmClass::predefinedClasses())
        {
            if (!alarms.findClass(fixed.name))
                alarms.classes.push_back(std::move(fixed));
        }
        for (const QJsonValue& entry : alarmsJson->value(QStringLiteral("alarms")).toArray())
        {
            auto alarm = Alarm::fromJson(entry.toObject());
            if (!alarm)
                return makeError(ErrorCode::BadFormat,
                                 QStringLiteral("%1: %2").arg(kAlarmsFile, alarm.error().message));
            alarms.alarms.push_back(std::move(*alarm));
        }
        if (auto set = project->setAlarms(std::move(alarms)); !set)
            return makeError(ErrorCode::BadFormat,
                             QStringLiteral("%1: %2").arg(kAlarmsFile, set.error().message));
    }
    else if (alarmsRaw.error().code != ErrorCode::Io)
    {
        return tl::unexpected(alarmsRaw.error());
    }

    if (readAsset)
    {
        for (const QJsonValue& entry : projectJson->value(kAssets).toArray())
        {
            const QJsonObject info = entry.toObject();
            const QString path = info.value(kFile).toString();
            const QString id = info.value(kId).toString();
            if (!isInside(path, kAssetsFolder))
                return makeError(ErrorCode::BadFormat,
                                 QStringLiteral("Invalid asset file path '%1'").arg(path));
            auto bytes = readAsset(path);
            if (!bytes)
                return tl::unexpected(bytes.error());
            // The id is the content's hash; a file that no longer matches
            // was changed outside the editor.
            if (AssetStore::idFor(*bytes) != id)
                return makeError(
                    ErrorCode::BadFormat,
                    QStringLiteral("%1 does not match its id (the file was changed)").arg(path));
            project->assets().add(std::move(*bytes), info.value(kName).toString());
        }
    }

    QJsonObject preserved = *projectJson;
    for (const QString& known :
         {kFormatVersion, kName, kScreens, kAssets, kStyle, QStringLiteral("connections"),
          QStringLiteral("simulationProfile")})
        preserved.remove(known);
    project->setPreservedJson(preserved);
    return project;
}

Result<void> save(const Project& project, const QString& folder)
{
    const QDir dir(folder);
    if (!dir.mkpath(QLatin1StringView(kScreensFolder)))
        return makeError(ErrorCode::Io, QStringLiteral("Cannot create the folder %1").arg(folder));

    // Screen files this project wrote last time; the ones no longer listed
    // (deleted or renamed screens) are removed after the new files are in
    // place. Other files in the folder are left alone.
    QSet<QString> previous;
    if (auto old = readFile(folder, QString::fromLatin1(kProjectFile)))
    {
        for (const QJsonValue& entry : old->value(kScreens).toArray())
            previous.insert(entry.toString());
        for (const QJsonValue& entry : old->value(kAssets).toArray())
            previous.insert(entry.toObject().value(kFile).toString());
    }

    // Assets first: a file named by its content hash never changes, so an
    // existing one is kept as it is.
    for (const QString& id : usedAssets(project))
    {
        const auto asset = project.assets().find(id);
        if (!asset)
            continue;
        const QString path = assetPath(*asset);
        previous.remove(path);
        if (QFileInfo(dir.filePath(path)).size() == asset->data.size())
            continue;
        if (!dir.mkpath(QLatin1StringView(kAssetsFolder)))
            return makeError(ErrorCode::Io,
                             QStringLiteral("Cannot create the folder %1").arg(path));
        QSaveFile file(dir.filePath(path));
        if (!file.open(QIODevice::WriteOnly) || file.write(asset->data) < 0 || !file.commit())
            return makeError(ErrorCode::Io,
                             QStringLiteral("Cannot write %1: %2").arg(path, file.errorString()));
    }

    const std::vector<File> files = toFiles(project);
    // Screens first and project.json last: until it is replaced, the old
    // project.json still lists a complete set of files.
    for (std::size_t i = files.size(); i-- > 0;)
    {
        if (auto written = writeFile(folder, files[i].first, files[i].second); !written)
            return written;
        previous.remove(files[i].first);
    }
    for (const QString& stale : std::as_const(previous))
    {
        if (isInside(stale, kScreensFolder) || isInside(stale, kAssetsFolder))
            QFile::remove(dir.filePath(stale));
    }
    return {};
}

Result<std::unique_ptr<Project>> load(const QString& folder)
{
    return fromFiles(
        [&folder](const QString& path) { return readFile(folder, path); },
        [&folder](const QString& path) -> Result<QByteArray>
        {
            QFile file(QDir(folder).filePath(path));
            if (!file.open(QIODevice::ReadOnly))
                return makeError(
                    ErrorCode::Io,
                    QStringLiteral("Cannot read %1: %2").arg(path, file.errorString()));
            return file.readAll();
        });
}

} // namespace hmi::ProjectSerializer
