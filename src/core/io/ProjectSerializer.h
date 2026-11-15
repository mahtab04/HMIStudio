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
//     ├─ project.json          formatVersion, name, screen file list in order
//     └─ screens/01_overview.json …
//
// Files are pretty-printed JSON with sorted keys, so saving an unchanged
// project gives byte-identical files and diffs stay readable. Every file
// carries `formatVersion`; older files are migrated on load, newer ones are
// refused. Keys and objects this version does not know are kept and written
// back. Files in the folder that the project does not list (tags and
// connections from later phases, the user's own files) are never touched.
namespace ProjectSerializer
{

inline constexpr int kCurrentFormatVersion = 1;
inline constexpr const char* kProjectFile = "project.json";
inline constexpr const char* kScreensFolder = "screens";
// Suggested suffix for project folders.
inline constexpr const char* kFolderSuffix = ".hmiproj";

// One file of a project: path relative to the project folder, and content.
using File = std::pair<QString, QJsonObject>;

// The project as files, project.json first. Used by save() and autosave.
[[nodiscard]] std::vector<File> toFiles(const Project& project);

// Returns a file's content by path relative to the project folder.
using Reader = std::function<Result<QJsonObject>(const QString& path)>;

// Builds a project from files, migrating each to the current format first.
[[nodiscard]] Result<std::unique_ptr<Project>> fromFiles(const Reader& read);

[[nodiscard]] QByteArray toBytes(const QJsonObject& json);

Result<void> save(const Project& project, const QString& folder);
[[nodiscard]] Result<std::unique_ptr<Project>> load(const QString& folder);

// Brings a file to kCurrentFormatVersion, one migration step per version.
// A file without formatVersion predates versioning and counts as version 0.
[[nodiscard]] Result<QJsonObject> migrate(QJsonObject file, const QString& name);

} // namespace ProjectSerializer

} // namespace hmi
