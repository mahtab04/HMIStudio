#include "editor/app/DocumentController.h"

#include "core/io/ProjectSerializer.h"
#include "core/model/Project.h"
#include "editor/app/AppSettings.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUndoStack>

namespace hmi
{

namespace
{

const auto kRecoveryFile = QStringLiteral("recovery.json");
const auto kOriginalPath = QStringLiteral("originalPath");

QString untitledName()
{
    return QCoreApplication::translate("hmi::DocumentController", "Untitled project");
}

QString folderFromUrl(const QUrl& url)
{
    const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
    if (path.isEmpty())
        return {};
    // A project file inside the folder stands for its folder.
    const QFileInfo info(path);
    if (info.isFile() || info.fileName() == QLatin1StringView(ProjectSerializer::kProjectFile))
        return QDir::cleanPath(info.absolutePath());
    return QDir::cleanPath(info.absoluteFilePath());
}

} // namespace

DocumentController::DocumentController(Project& project, QUndoStack& undoStack,
                                       AppSettings& settings, QString autosaveFolder,
                                       QObject* parent)
    : QObject(parent)
    , m_project(project)
    , m_undoStack(undoStack)
    , m_settings(settings)
    , m_autosaveFolder(std::move(autosaveFolder))
{
    connect(&m_undoStack, &QUndoStack::cleanChanged, this,
            [this]
            {
                Q_EMIT modifiedChanged();
                Q_EMIT titleChanged();
            });
    connect(&m_settings, &AppSettings::recentProjectsChanged, this,
            &DocumentController::recentProjectsChanged);

    m_statusTimer.setSingleShot(true);
    m_statusTimer.setInterval(kStatusMs);
    connect(&m_statusTimer, &QTimer::timeout, this,
            [this]
            {
                m_statusText.clear();
                Q_EMIT statusTextChanged();
            });
    connect(this, &DocumentController::statusMessage, this,
            [this](const QString& message)
            {
                m_statusText = message;
                m_statusTimer.start();
                Q_EMIT statusTextChanged();
            });

    m_recoveryAvailable =
        !m_autosaveFolder.isEmpty() &&
        QFileInfo::exists(
            QDir(m_autosaveFolder).filePath(QLatin1StringView(ProjectSerializer::kProjectFile)));

    const int seconds = m_settings.autosaveSeconds();
    if (seconds > 0 && !m_autosaveFolder.isEmpty())
    {
        m_autosaveTimer.setInterval(seconds * 1000);
        connect(&m_autosaveTimer, &QTimer::timeout, this, &DocumentController::autosave);
        m_autosaveTimer.start();
    }
}

// A normal exit means the user saved or chose to discard; a copy left
// behind would offer a recovery that is not wanted. A pending recovery is
// kept until it is answered.
DocumentController::~DocumentController()
{
    if (!m_recoveryAvailable)
        removeAutosave();
}

QString DocumentController::title() const
{
    const QString name = m_project.name().isEmpty() ? untitledName() : m_project.name();
    return modified() ? tr("%1 *").arg(name) : name;
}

bool DocumentController::modified() const
{
    return !m_undoStack.isClean();
}

QStringList DocumentController::recentProjects() const
{
    return m_settings.recentProjects();
}

void DocumentController::guard(Pending action, const QString& folder)
{
    m_pendingFolder = folder;
    if (!modified())
    {
        perform(action);
        return;
    }
    m_pending = action;
    Q_EMIT confirmRequested();
}

void DocumentController::perform(Pending action)
{
    m_pending = Pending::None;
    switch (action)
    {
    case Pending::NewProject:
    {
        Project fresh(untitledName());
        (void)fresh.insertScreen(std::make_unique<Screen>(tr("Screen 1")));
        m_project.replaceContents(std::move(fresh));
        m_undoStack.clear();
        setFilePath({});
        removeAutosave();
        Q_EMIT titleChanged();
        break;
    }
    case Pending::OpenDialog:
        Q_EMIT openDialogRequested();
        break;
    case Pending::OpenFolder:
        load(m_pendingFolder);
        break;
    case Pending::Close:
        m_closeApproved = true;
        Q_EMIT closeApproved();
        break;
    case Pending::None:
        break;
    }
}

void DocumentController::newProject()
{
    guard(Pending::NewProject);
}

void DocumentController::open()
{
    guard(Pending::OpenDialog);
}

void DocumentController::openRecent(const QString& folder)
{
    guard(Pending::OpenFolder, folder);
}

bool DocumentController::openFolder(const QUrl& url)
{
    return load(folderFromUrl(url));
}

bool DocumentController::load(const QString& folder)
{
    auto loaded = ProjectSerializer::load(folder);
    if (!loaded)
    {
        reportError(tr("Could not open the project: %1").arg(loaded.error().message));
        if (loaded.error().code == ErrorCode::Io)
            m_settings.removeRecentProject(folder);
        return false;
    }
    m_project.replaceContents(std::move(**loaded));
    m_undoStack.clear();
    setFilePath(folder);
    m_settings.addRecentProject(folder);
    removeAutosave();
    Q_EMIT titleChanged();
    Q_EMIT statusMessage(tr("Opened %1").arg(QDir::toNativeSeparators(folder)));
    return true;
}

bool DocumentController::save()
{
    if (m_filePath.isEmpty())
    {
        Q_EMIT saveAsDialogRequested();
        return false;
    }
    return writeTo(m_filePath);
}

void DocumentController::chooseSaveLocation()
{
    Q_EMIT saveAsDialogRequested();
}

bool DocumentController::saveAs(const QUrl& url)
{
    QString folder = folderFromUrl(url);
    if (folder.isEmpty())
        return false;
    const QString suffix = QString::fromLatin1(ProjectSerializer::kFolderSuffix);
    // A new name gets the project suffix; an existing project folder is
    // used as it is.
    if (!folder.endsWith(suffix) && !QFileInfo::exists(QDir(folder).filePath(
                                        QLatin1StringView(ProjectSerializer::kProjectFile))))
        folder += suffix;

    // A project that never had a name takes the folder's.
    if (m_project.name().isEmpty() || m_project.name() == untitledName())
        m_project.setName(QFileInfo(folder).completeBaseName());
    if (!writeTo(folder))
        return false;
    if (m_continueAfterSaveAs)
    {
        m_continueAfterSaveAs = false;
        perform(m_pending);
    }
    return true;
}

bool DocumentController::writeTo(const QString& folder)
{
    if (auto saved = ProjectSerializer::save(m_project, folder); !saved)
    {
        reportError(tr("Could not save the project: %1").arg(saved.error().message));
        return false;
    }
    m_undoStack.setClean();
    setFilePath(folder);
    m_settings.addRecentProject(folder);
    removeAutosave();
    Q_EMIT titleChanged();
    Q_EMIT statusMessage(tr("Saved %1").arg(QDir::toNativeSeparators(folder)));
    return true;
}

void DocumentController::setFilePath(const QString& folder)
{
    if (folder == m_filePath)
        return;
    m_filePath = folder;
    Q_EMIT fileChanged();
}

void DocumentController::saveChanges()
{
    if (m_filePath.isEmpty())
    {
        m_continueAfterSaveAs = true;
        Q_EMIT saveAsDialogRequested();
        return;
    }
    if (writeTo(m_filePath))
        perform(m_pending);
}

void DocumentController::discardChanges()
{
    perform(m_pending);
}

void DocumentController::cancelPending()
{
    m_pending = Pending::None;
    m_continueAfterSaveAs = false;
}

bool DocumentController::requestClose()
{
    if (m_closeApproved || !modified())
        return true;
    guard(Pending::Close);
    return false;
}

bool DocumentController::autosave()
{
    // A copy still waiting to be recovered must not be overwritten.
    if (m_autosaveFolder.isEmpty() || m_recoveryAvailable || !modified() ||
        m_undoStack.index() == m_autosavedIndex)
        return false;
    if (auto saved = ProjectSerializer::save(m_project, m_autosaveFolder); !saved)
    {
        qWarning("Autosave failed: %s", qPrintable(saved.error().message));
        return false;
    }
    QSaveFile file(QDir(m_autosaveFolder).filePath(kRecoveryFile));
    const QJsonObject recovery{
        {kOriginalPath, m_filePath},
        {QStringLiteral("savedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
    };
    if (file.open(QIODevice::WriteOnly))
    {
        file.write(QJsonDocument(recovery).toJson());
        file.commit();
    }
    m_autosavedIndex = m_undoStack.index();
    return true;
}

void DocumentController::recover()
{
    if (!m_recoveryAvailable)
        return;
    auto loaded = ProjectSerializer::load(m_autosaveFolder);
    QFile recoveryFile(QDir(m_autosaveFolder).filePath(kRecoveryFile));
    QString original;
    if (recoveryFile.open(QIODevice::ReadOnly))
        original = QJsonDocument::fromJson(recoveryFile.readAll())
                       .object()
                       .value(kOriginalPath)
                       .toString();
    m_recoveryAvailable = false;
    Q_EMIT recoveryAvailableChanged();
    if (!loaded)
    {
        reportError(tr("Could not recover the project: %1").arg(loaded.error().message));
        return;
    }
    m_project.replaceContents(std::move(**loaded));
    m_undoStack.clear();
    // The recovered work is not saved anywhere yet.
    m_undoStack.resetClean();
    m_autosavedIndex = m_undoStack.index();
    setFilePath(original);
    Q_EMIT modifiedChanged();
    Q_EMIT titleChanged();
    Q_EMIT statusMessage(tr("Recovered unsaved changes"));
}

void DocumentController::discardRecovery()
{
    m_recoveryAvailable = false;
    Q_EMIT recoveryAvailableChanged();
    removeAutosave();
}

void DocumentController::reportError(const QString& message)
{
    m_errorText = message;
    Q_EMIT errorOccurred(message);
}

void DocumentController::removeAutosave()
{
    m_autosavedIndex = -1;
    if (!m_autosaveFolder.isEmpty())
        QDir(m_autosaveFolder).removeRecursively();
}

} // namespace hmi
