#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <qqmlintegration.h>

class QUndoStack;

namespace hmi
{

class AppSettings;
class Project;

// The document as a file: new, open, save, save as, recent projects, the
// unsaved-changes prompt, autosave and recovery after a crash.
//
// Actions that would lose unsaved changes (new, open, close) first ask:
// the controller emits confirmRequested and waits for saveChanges(),
// discardChanges() or cancelPending(); QML only shows the question. When a
// file name is needed it emits openDialogRequested or saveAsDialogRequested
// and continues when QML calls openFolder() or saveAs().
//
// Opening replaces the contents of the one Project (Project::replaceContents),
// so the editor, tools and models keep their references. "Modified" is the
// undo stack not being at its clean state.
//
// Autosave writes a copy of a modified project to `autosaveFolder` at the
// interval from the settings (Memento). A normal exit removes it; if it is
// still there at start, recoveryAvailable is true and recover() opens it.
class DocumentController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("DocumentController is provided by the application")
    Q_PROPERTY(QString filePath READ filePath NOTIFY fileChanged)
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    Q_PROPERTY(bool modified READ modified NOTIFY modifiedChanged)
    Q_PROPERTY(QStringList recentProjects READ recentProjects NOTIFY recentProjectsChanged)
    Q_PROPERTY(bool recoveryAvailable READ recoveryAvailable NOTIFY recoveryAvailableChanged)
    // The latest status note ("Saved ..."), cleared after a few seconds.
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    // The latest error, for the error dialog.
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorOccurred)

public:
    DocumentController(Project& project, QUndoStack& undoStack, AppSettings& settings,
                       QString autosaveFolder, QObject* parent = nullptr);
    ~DocumentController() override;
    DocumentController(const DocumentController&) = delete;
    DocumentController& operator=(const DocumentController&) = delete;
    DocumentController(DocumentController&&) = delete;
    DocumentController& operator=(DocumentController&&) = delete;

    [[nodiscard]] QString filePath() const { return m_filePath; }
    [[nodiscard]] QString title() const;
    [[nodiscard]] bool modified() const;
    [[nodiscard]] QStringList recentProjects() const;
    [[nodiscard]] bool recoveryAvailable() const { return m_recoveryAvailable; }
    [[nodiscard]] QString statusText() const { return m_statusText; }
    [[nodiscard]] QString errorText() const { return m_errorText; }

    static constexpr int kStatusMs = 4000;

    Q_INVOKABLE void newProject();
    Q_INVOKABLE void open();
    Q_INVOKABLE void openRecent(const QString& folder);
    // Saves to the current folder; without one, asks for a name first.
    // Returns true when the project was written.
    Q_INVOKABLE bool save();
    // "Save as": asks QML for a location (saveAsDialogRequested).
    Q_INVOKABLE void chooseSaveLocation();
    // Called from the dialogs. `url` may be the project folder or a file in
    // it (project.json); saveAs adds the .hmiproj suffix when it is missing.
    Q_INVOKABLE bool openFolder(const QUrl& url);
    Q_INVOKABLE bool saveAs(const QUrl& url);

    // Answers to confirmRequested.
    Q_INVOKABLE void saveChanges();
    Q_INVOKABLE void discardChanges();
    Q_INVOKABLE void cancelPending();

    // The window is about to close. Returns true when it may; otherwise the
    // question is asked and closeApproved follows if the user agrees.
    Q_INVOKABLE bool requestClose();

    Q_INVOKABLE void recover();
    Q_INVOKABLE void discardRecovery();

    // Writes the autosave copy now if the project changed since the last
    // one (the timer calls this). Returns true if a copy was written.
    bool autosave();
    [[nodiscard]] QString autosaveFolder() const { return m_autosaveFolder; }

Q_SIGNALS:
    void fileChanged();
    void titleChanged();
    void modifiedChanged();
    void recentProjectsChanged();
    void recoveryAvailableChanged();
    void confirmRequested();
    void openDialogRequested();
    void saveAsDialogRequested();
    void closeApproved();
    // A file could not be read or written; the message says what to do.
    void errorOccurred(const QString& message);
    // A short note for the status bar ("Saved", "Opened ...").
    void statusMessage(const QString& message);
    void statusTextChanged();

private:
    enum class Pending
    {
        None,
        NewProject,
        OpenDialog,
        OpenFolder,
        Close,
    };

    void guard(Pending action, const QString& folder = {});
    void perform(Pending action);
    bool load(const QString& folder);
    bool writeTo(const QString& folder);
    void setFilePath(const QString& folder);
    void removeAutosave();
    void reportError(const QString& message);

    Project& m_project;
    QUndoStack& m_undoStack;
    AppSettings& m_settings;
    QString m_autosaveFolder;
    QString m_filePath;
    QTimer m_autosaveTimer;
    QTimer m_statusTimer;
    QString m_statusText;
    QString m_errorText;
    int m_autosavedIndex = -1;
    bool m_recoveryAvailable = false;
    bool m_closeApproved = false;
    bool m_continueAfterSaveAs = false;
    Pending m_pending = Pending::None;
    QString m_pendingFolder;
};

} // namespace hmi
