// Documents (new, open, save, the unsaved-changes prompt, recent projects,
// autosave and recovery) and screen management, through a real AppContext
// with temporary settings and autosave folders.

#include "editor/app/DocumentController.h"

#include "core/io/ProjectSerializer.h"
#include "core/model/Project.h"
#include "core/model/ScreenBuilder.h"
#include "core/objects/Rectangle.h"
#include "core/objects/ScreenWindow.h"
#include "core/property/Prop.h"
#include "core/registry/ObjectRegistry.h"
#include "editor/app/AppContext.h"
#include "editor/app/AppSettings.h"
#include "editor/app/EditActions.h"
#include "editor/app/EditorController.h"
#include "editor/app/Theme.h"
#include "editor/models/PropertyModel.h"
#include "editor/models/ScreenListModel.h"
#include "editor/models/SelectionModel.h"
#include "editor/tools/ToolContext.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUndoStack>

#include <memory>

using namespace hmi;
using namespace Qt::StringLiterals;

namespace
{

std::unique_ptr<Project> twoScreens()
{
    auto project = std::make_unique<Project>(u"Plant"_s);
    (void)project->insertScreen(
        ScreenBuilder(u"Overview"_s).add(Rectangle::kTypeId, {10, 10, 100, 50}).build());
    (void)project->insertScreen(ScreenBuilder(u"Details"_s).build());
    return project;
}

QByteArray readAll(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

} // namespace

class DocumentControllerTest : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<AppContext> m_app;

    [[nodiscard]] AppContext::Paths paths() const
    {
        return {.settingsFile = m_dir->filePath(u"settings.ini"_s),
                .autosaveFolder = m_dir->filePath(u"autosave"_s)};
    }

    [[nodiscard]] DocumentController& document() const { return m_app->document(); }

    // An edit through the undo stack, so the document is modified.
    void edit()
    {
        ToolContext& context = m_app->toolContext();
        context.selection().selectOnly(context.screen()->objects().front()->id());
        QVERIFY(m_app->editActions().nudge({1, 0}));
        // Nudges merge; a new step each time keeps the stack moving.
        m_app->undoStack().push(new QUndoCommand(u"marker"_s));
    }

private Q_SLOTS:
    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        m_app = std::make_unique<AppContext>(twoScreens(), paths());
    }

    void cleanup()
    {
        m_app.reset();
        m_dir.reset();
    }

    void saveAsThenOpenRoundTrips()
    {
        QVERIFY(!document().modified());
        edit();
        QVERIFY(document().modified());
        QVERIFY(document().title().endsWith(u'*'));

        // Without a folder, save asks for one.
        QSignalSpy askName(&document(), &DocumentController::saveAsDialogRequested);
        QVERIFY(!document().save());
        QCOMPARE(askName.count(), 1);

        const QString folder = m_dir->filePath(u"Plant"_s);
        QVERIFY(document().saveAs(QUrl::fromLocalFile(folder)));
        const QString saved = folder + u".hmiproj"_s;
        QCOMPARE(document().filePath(), saved);
        QVERIFY(!document().modified());
        QCOMPARE(m_app->settings().recentProjects(), QStringList{saved});
        const QByteArray firstSave = readAll(saved + u"/screens/01_overview.json"_s);

        // Close and reopen in a fresh application: same model, and saving
        // again writes the same bytes.
        m_app.reset();
        m_app = std::make_unique<AppContext>(twoScreens(), paths());
        QVERIFY(document().openFolder(QUrl::fromLocalFile(saved + u"/project.json"_s)));
        QCOMPARE(m_app->project().screenCount(), std::size_t(2));
        QCOMPARE(m_app->project().screens()[0]->objects()[0]->geometry().x(), 11.0);
        QVERIFY(document().save());
        QCOMPARE(readAll(saved + u"/screens/01_overview.json"_s), firstSave);
    }

    void openingShowsTheFirstScreen()
    {
        const QString folder = m_dir->filePath(u"Other.hmiproj"_s);
        auto other = std::make_unique<Project>(u"Other"_s);
        (void)other->insertScreen(ScreenBuilder(u"Only"_s).build());
        QVERIFY(ProjectSerializer::save(*other, folder));

        m_app->editor().setCurrentScreen(1);
        QVERIFY(document().openFolder(QUrl::fromLocalFile(folder)));
        QCOMPARE(m_app->editor().screenTitle(), u"Only"_s);
        QCOMPARE(m_app->screens().rowCount(), 1);
        QCOMPARE(m_app->undoStack().count(), 0);
    }

    void unsavedChangesAreAskedFor()
    {
        edit();
        QSignalSpy asked(&document(), &DocumentController::confirmRequested);
        document().newProject();
        QCOMPARE(asked.count(), 1);
        QCOMPARE(m_app->project().name(), u"Plant"_s); // nothing happened yet

        document().cancelPending();
        document().discardChanges(); // nothing pending any more
        QCOMPARE(m_app->project().name(), u"Plant"_s);

        document().newProject();
        document().discardChanges();
        QCOMPARE(m_app->project().screenCount(), std::size_t(1));
        QVERIFY(!document().modified());
        QVERIFY(document().filePath().isEmpty());
    }

    void saveFromThePromptAsksForANameThenContinues()
    {
        edit();
        document().newProject();
        QSignalSpy askName(&document(), &DocumentController::saveAsDialogRequested);
        document().saveChanges();
        QCOMPARE(askName.count(), 1);
        QVERIFY(document().saveAs(QUrl::fromLocalFile(m_dir->filePath(u"Kept"_s))));
        // The new project followed the save.
        QCOMPARE(m_app->project().screenCount(), std::size_t(1));
        QVERIFY(QFile::exists(m_dir->filePath(u"Kept.hmiproj/project.json"_s)));
    }

    void closingAsksOnlyWhenModified()
    {
        QVERIFY(document().requestClose());
        edit();
        QSignalSpy approved(&document(), &DocumentController::closeApproved);
        QVERIFY(!document().requestClose());
        document().discardChanges();
        QCOMPARE(approved.count(), 1);
        QVERIFY(document().requestClose());
    }

    void openErrorsAreReported()
    {
        QSignalSpy error(&document(), &DocumentController::errorOccurred);
        QVERIFY(!document().openFolder(QUrl::fromLocalFile(m_dir->filePath(u"missing"_s))));
        QCOMPARE(error.count(), 1);
        QCOMPARE(m_app->project().name(), u"Plant"_s);
    }

    void autosaveAndRecovery()
    {
        QVERIFY(!document().autosave()); // nothing changed yet
        edit();
        QVERIFY(document().autosave());
        QVERIFY(!document().autosave()); // nothing new since
        QVERIFY(QFile::exists(document().autosaveFolder() + u"/project.json"_s));

        // A crash leaves the copy behind; simulate it by copying it away and
        // back around a normal exit (which removes it).
        const QString kept = m_dir->filePath(u"kept"_s);
        QDir().rename(document().autosaveFolder(), kept);
        m_app.reset();
        QDir().rename(kept, paths().autosaveFolder);

        m_app = std::make_unique<AppContext>(twoScreens(), paths());
        QVERIFY(document().recoveryAvailable());
        document().recover();
        QVERIFY(!document().recoveryAvailable());
        QVERIFY(document().modified());
        QCOMPARE(m_app->project().screens()[0]->objects()[0]->geometry().x(), 11.0);

        // A normal exit after recovering cleans up; no recovery next time.
        m_app.reset();
        m_app = std::make_unique<AppContext>(twoScreens(), paths());
        QVERIFY(!document().recoveryAvailable());
    }

    void discardingRecoveryRemovesTheCopy()
    {
        edit();
        QVERIFY(document().autosave());
        const QString kept = m_dir->filePath(u"kept"_s);
        QDir().rename(document().autosaveFolder(), kept);
        m_app.reset();
        QDir().rename(kept, paths().autosaveFolder);
        m_app = std::make_unique<AppContext>(twoScreens(), paths());
        QVERIFY(document().recoveryAvailable());
        document().discardRecovery();
        QVERIFY(!QFile::exists(paths().autosaveFolder));
    }

    void settingsPersist()
    {
        m_app->theme().setDensity(Theme::Density::Comfortable);
        m_app->settings().setPropertiesPanelVisible(false);
        m_app->settings().setMainSplitState("state");
        m_app.reset();
        m_app = std::make_unique<AppContext>(twoScreens(), paths());
        QCOMPARE(m_app->theme().density(), Theme::Density::Comfortable);
        QVERIFY(!m_app->settings().propertiesPanelVisible());
        QCOMPARE(m_app->settings().mainSplitState(), QByteArray("state"));
    }

    void recentProjectsAreBounded()
    {
        AppSettings& settings = m_app->settings();
        for (int i = 0; i < AppSettings::kMaxRecentProjects + 3; ++i)
            settings.addRecentProject(u"C:/p%1"_s.arg(i));
        settings.addRecentProject(u"C:/p2"_s);
        const QStringList recent = settings.recentProjects();
        QCOMPARE(recent.size(), AppSettings::kMaxRecentProjects);
        QCOMPARE(recent.front(), u"C:/p2"_s);
        QCOMPARE(recent.count(u"C:/p2"_s), 1);
    }

    void screenWindowsFollowRenamedScreens()
    {
        EditorController& editor = m_app->editor();
        Project& project = m_app->project();
        Screen& overview = *project.screens()[0];
        auto window =
            ObjectRegistry::instance().find(ScreenWindow::kTypeId)->create(ObjectId::create());
        (void)window->setProperty(Prop::ScreenName, u"Details"_s);
        const ObjectId id = window->id();
        QVERIFY(overview.insertObject(std::move(window)));

        // The Screen property is picked from the other screens, not bound.
        m_app->selection().selectOnly(id);
        PropertyModel& model = m_app->properties();
        int row = -1;
        for (int i = 0; i < model.rowCount(); ++i)
        {
            if (model.data(model.index(i), PropertyModel::KeyRole).toString() == u"screenName")
                row = i;
        }
        QVERIFY(row >= 0);
        QCOMPARE(model.data(model.index(row), PropertyModel::EditorRole).toString(), u"screen"_s);
        QCOMPARE(model.data(model.index(row), PropertyModel::OptionsRole).toStringList(),
                 QStringList{u"Details"_s});
        QVERIFY(!model.data(model.index(row), PropertyModel::BindableRole).toBool());

        // Renaming the screen updates the window, in one step.
        editor.renameScreen(1, u"Motors"_s);
        QCOMPARE(overview.object(id)->value<QString>(Prop::ScreenName), u"Motors"_s);
        m_app->undoStack().undo();
        QCOMPARE(project.screens()[1]->name(), u"Details"_s);
        QCOMPARE(overview.object(id)->value<QString>(Prop::ScreenName), u"Details"_s);
    }

    void screensAddDuplicateRenameDelete()
    {
        EditorController& editor = m_app->editor();
        Project& project = m_app->project();
        QUndoStack& stack = m_app->undoStack();

        editor.addScreen();
        QCOMPARE(project.screenCount(), std::size_t(3));
        QCOMPARE(editor.currentScreenIndex(), 2);
        QCOMPARE(editor.screenTitle(), u"Screen_1"_s); // TIA style, next free number

        editor.duplicateScreen(0);
        QCOMPARE(project.screenCount(), std::size_t(4));
        QCOMPARE(editor.currentScreenIndex(), 1);
        const Screen& copy = *project.screens()[1];
        QCOMPARE(copy.name(), u"Overview copy"_s);
        QCOMPARE(copy.objectCount(), std::size_t(1));
        QVERIFY(copy.objects()[0]->id() != project.screens()[0]->objects()[0]->id());

        editor.renameScreen(1, u"Second"_s);
        QCOMPARE(copy.name(), u"Second"_s);
        QCOMPARE(
            m_app->screens().data(m_app->screens().index(1), ScreenListModel::NameRole).toString(),
            u"Second"_s);

        // Deleting the shown screen shows its neighbour; undo brings it back.
        editor.removeScreen(1);
        QCOMPARE(project.screenCount(), std::size_t(3));
        QCOMPARE(editor.screenTitle(), u"Details"_s);
        stack.undo();
        QCOMPARE(project.screens()[1]->name(), u"Second"_s);

        // Undo all the way: the original two screens, each step reversible.
        while (stack.canUndo())
            stack.undo();
        QCOMPARE(project.screenCount(), std::size_t(2));
        QCOMPARE(project.screens()[0]->name(), u"Overview"_s);

        // The last screen cannot be deleted.
        editor.removeScreen(1);
        QVERIFY(!editor.canRemoveScreen());
        editor.removeScreen(0);
        QCOMPARE(project.screenCount(), std::size_t(1));
    }
};

QTEST_MAIN(DocumentControllerTest)
#include "DocumentControllerTest.moc"
