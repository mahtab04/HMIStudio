#pragma once

#include "core/Result.h"

#include <QJsonObject>
#include <QString>

#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace hmi
{

class Project;

// Saves and loads a project folder (ARCHITECTURE.md section 10):
//
//     MyLine.hmiproj/
//     ├─ project.json          formatVersion, name, screen and asset lists
//     ├─ tags.json             formatVersion, tags
//     ├─ screens/01_overview.json …
//     └─ assets/<content hash>.png …   images and SVG used by objects
//
// Files are pretty-printed JSON with sorted keys, so saving an unchanged
// project gives byte-identical files and diffs stay readable. Every file
// carries `formatVersion`; older files are migrated on load, newer ones are
// refused. Keys and objects this version does not know are kept and written
// back. Files in the folder that the project does not list (connections
// from later phases, the user's own files) are never touched.
namespace ProjectSerializer
{

inline constexpr int kCurrentFormatVersion = 4;
inline constexpr const char* kProjectFile = "project.json";
inline constexpr const char* kScreensFolder = "screens";
inline constexpr const char* kAssetsFolder = "assets";
// Suggested suffix for project folders.
inline constexpr const char* kFolderSuffix = ".hmiproj";

// One file of a project: path relative to the project folder, and content.
using File = std::pair<QString, QJsonObject>;

// The project as files, project.json first. Used by save() and autosave.
[[nodiscard]] std::vector<File> toFiles(const Project& project);

// Returns a file's content by path relative to the project folder.
using Reader = std::function<Result<QJsonObject>(const QString& path)>;
using BinaryReader = std::function<Result<QByteArray>(const QString& path)>;

// Builds a project from files, migrating each to the current format first.
// Without `readAsset`, assets are not loaded.
[[nodiscard]] Result<std::unique_ptr<Project>> fromFiles(const Reader& read,
                                                         const BinaryReader& readAsset = {});

// Ids of the assets the project's objects use (Prop::Asset), sorted. Only
// these are saved, so assets no longer used drop out of the folder.
[[nodiscard]] std::vector<QString> usedAssets(const Project& project);

[[nodiscard]] QByteArray toBytes(const QJsonObject& json);

Result<void> save(const Project& project, const QString& folder);
[[nodiscard]] Result<std::unique_ptr<Project>> load(const QString& folder);

// Brings a file to kCurrentFormatVersion, one migration step per version.
// A file without formatVersion predates versioning and counts as version 0.
[[nodiscard]] Result<QJsonObject> migrate(QJsonObject file, const QString& name);

} // namespace ProjectSerializer

} // namespace hmi
