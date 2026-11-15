#include "core/io/ProjectSerializer.h"

#include "core/io/ObjectJson.h"
#include "core/model/GroupObject.h"
#include "core/model/Project.h"
#include "core/model/ScreenBuilder.h"
#include "core/model/UnknownObject.h"
#include "core/objects/Ellipse.h"
#include "core/objects/Line.h"
#include "core/objects/Rectangle.h"
#include "core/objects/TextBox.h"
#include "core/property/Prop.h"
#include "core/registry/ObjectRegistry.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace hmi;
using namespace Qt::StringLiterals;

namespace
{

bool sameObject(const HmiObject& x, const HmiObject& y)
{
    if (x.id() != y.id() || !x.hasSameProperties(y) || x.preservedJson() != y.preservedJson())
        return false;
    for (std::size_t i = 0; i < x.children().size(); ++i)
    {
        if (!sameObject(*x.children()[i], *y.children()[i]))
            return false;
    }
    return true;
}

bool sameProject(const Project& a, const Project& b)
{
    if (a.name() != b.name() || a.screenCount() != b.screenCount())
        return false;
    for (std::size_t s = 0; s < a.screenCount(); ++s)
    {
        const Screen& x = *a.screens()[s];
        const Screen& y = *b.screens()[s];
        if (x.id() != y.id() || x.name() != y.name() || x.size() != y.size() ||
            x.backgroundColor() != y.backgroundColor() || x.objectCount() != y.objectCount())
            return false;
        for (std::size_t i = 0; i < x.objectCount(); ++i)
        {
            if (!sameObject(*x.objects()[i], *y.objects()[i]))
                return false;
        }
    }
    return true;
}

// Every file under `folder`, by relative path, with its bytes.
QMap<QString, QByteArray> filesIn(const QString& folder)
{
    QMap<QString, QByteArray> files;
    QDirIterator it(folder, QDir::Files, QDirIterator::Subdirectories);
    const QDir root(folder);
    while (it.hasNext())
    {
        const QString path = it.next();
        QFile file(path);
        if (file.open(QIODevice::ReadOnly))
            files.insert(root.relativeFilePath(path), file.readAll());
    }
    return files;
}

QJsonObject readJson(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}

void writeJson(const QString& path, const QJsonObject& json)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QJsonDocument(json).toJson());
}

std::unique_ptr<Project> sampleProject()
{
    QFont font;
    font.setPixelSize(18);
    font.setBold(true);
    auto overview = ScreenBuilder(u"Line overview"_s)
                        .size({1280, 800})
                        .background(QColor(0x20, 0x30, 0x40))
                        .add(Rectangle::kTypeId, {10, 20, 100, 50})
                        .with(Prop::FillColor, QColor(255, 0, 0, 128))
                        .with(Prop::Rotation, 30.0)
                        .add(TextBox::kTypeId, {200, 20, 150, 40})
                        .with(Prop::Text, u"Pump 1 — läuft"_s)
                        .with(Prop::Font, font)
                        .add(Line::kTypeId, {10, 300, 200, 50})
                        .with(Prop::LineDirection, EnumValue{Line::Rising})
                        .add(Ellipse::kTypeId, {400, 300, 80, 80})
                        .with(Prop::Locked, true)
                        .with(Prop::Visible, false)
                        .build();
    // A group, so nested children are saved too.
    std::vector<std::unique_ptr<HmiObject>> parts;
    parts.push_back(overview->objects()[0]->cloneWithNewId());
    parts.push_back(overview->objects()[3]->cloneWithNewId());
    (void)overview->insertObject(GroupObject::create(std::move(parts)));

    auto project = std::make_unique<Project>(u"Line 3"_s);
    (void)project->insertScreen(std::move(overview));
    (void)project->insertScreen(
        ScreenBuilder(u"Filling"_s).add(Rectangle::kTypeId, {0, 0, 10, 10}).build());
    (void)project->insertScreen(std::make_unique<Screen>(u"Empty / Alarms"_s));
    return project;
}

} // namespace

class ProjectSerializerTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void saveLoadSaveIsByteIdentical()
    {
        const auto project = sampleProject();
        QTemporaryDir first;
        QTemporaryDir second;
        QVERIFY(ProjectSerializer::save(*project, first.path()));

        auto loaded = ProjectSerializer::load(first.path());
        QVERIFY2(loaded, qPrintable(loaded ? QString() : loaded.error().message));
        QVERIFY(sameProject(**loaded, *project));

        QVERIFY(ProjectSerializer::save(**loaded, second.path()));
        const auto a = filesIn(first.path());
        QCOMPARE(a.keys(),
                 (QStringList{u"project.json"_s, u"screens/01_line_overview.json"_s,
                              u"screens/02_filling.json"_s, u"screens/03_empty_alarms.json"_s}));
        QCOMPARE(filesIn(second.path()), a);

        // Saving again over the same folder changes nothing.
        QVERIFY(ProjectSerializer::save(**loaded, first.path()));
        QCOMPARE(filesIn(first.path()), a);
    }

    void filesAreReadable()
    {
        const auto project = sampleProject();
        QTemporaryDir dir;
        QVERIFY(ProjectSerializer::save(*project, dir.path()));
        const QJsonObject json = readJson(dir.filePath(u"project.json"_s));
        QCOMPARE(json.value(u"formatVersion").toInt(), ProjectSerializer::kCurrentFormatVersion);
        QCOMPARE(json.value(u"name").toString(), u"Line 3"_s);
        QFile file(dir.filePath(u"screens/02_filling.json"_s));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray text = file.readAll();
        QVERIFY(text.contains("\n    \"background\": \"#ffe6e8eb\",\n")); // pretty, sorted
    }

    void unknownDataIsKept()
    {
        QTemporaryDir dir;
        QVERIFY(ProjectSerializer::save(*sampleProject(), dir.path()));

        // Add what a newer version might write: project and screen keys, an
        // object key, an unknown property and an object of an unknown type.
        QJsonObject projectJson = readJson(dir.filePath(u"project.json"_s));
        projectJson.insert(u"targetDevice"_s, u"panel-7"_s);
        writeJson(dir.filePath(u"project.json"_s), projectJson);

        const QString screenPath = dir.filePath(u"screens/02_filling.json"_s);
        QJsonObject screenJson = readJson(screenPath);
        screenJson.insert(u"layers"_s, QJsonArray{u"base"_s});
        QJsonArray objects = screenJson.value(u"objects").toArray();
        QJsonObject rect = objects[0].toObject();
        rect.insert(u"bindings"_s, QJsonArray{QJsonObject{{u"tag"_s, u"Level"_s}}});
        QJsonObject props = rect.value(u"props").toObject();
        props.insert(u"blink"_s, true);
        rect.insert(u"props"_s, props);
        objects[0] = rect;
        const QJsonObject tank{
            {u"type"_s, u"fluxCapacitor"_s},
            {u"id"_s, u"6a1e9b0c-1d2e-4f3a-8b4c-5d6e7f8a9b0c"_s},
            {u"props"_s, QJsonObject{{u"x"_s, 50},
                                     {u"y"_s, 60},
                                     {u"width"_s, 70},
                                     {u"height"_s, 80},
                                     {u"level"_s, 0.5}}},
            {u"events"_s, QJsonArray{u"press"_s}},
        };
        objects.append(tank);
        screenJson.insert(u"objects"_s, objects);
        writeJson(screenPath, screenJson);
        QFile other(dir.filePath(u"tags.json"_s));
        QVERIFY(other.open(QIODevice::WriteOnly));
        other.write("{}");
        other.close();

        auto loaded = ProjectSerializer::load(dir.path());
        QVERIFY(loaded);
        const Screen& filling = *(*loaded)->screens()[1];
        QCOMPARE(filling.objectCount(), std::size_t(2));
        const HmiObject& unknown = *filling.objects()[1];
        QCOMPARE(unknown.typeId(), UnknownObject::kTypeId);
        QCOMPARE(unknown.geometry(), QRectF(50, 60, 70, 80));

        // Moving the unknown object still keeps everything else about it.
        QVERIFY(filling.objects()[1]->setProperty(Prop::X, 55.0));
        QVERIFY(ProjectSerializer::save(**loaded, dir.path()));

        QCOMPARE(readJson(dir.filePath(u"project.json"_s)).value(u"targetDevice").toString(),
                 u"panel-7"_s);
        const QJsonObject saved = readJson(screenPath);
        QCOMPARE(saved.value(u"layers").toArray(), QJsonArray{u"base"_s});
        const QJsonArray savedObjects = saved.value(u"objects").toArray();
        const QJsonObject savedRect = savedObjects[0].toObject();
        QVERIFY(savedRect.contains(u"bindings"));
        QCOMPARE(savedRect.value(u"props").toObject().value(u"blink"), QJsonValue(true));
        const QJsonObject savedTank = savedObjects[1].toObject();
        QCOMPARE(savedTank.value(u"type").toString(), u"fluxCapacitor"_s);
        QCOMPARE(savedTank.value(u"events").toArray(), QJsonArray{u"press"_s});
        QCOMPARE(savedTank.value(u"props").toObject().value(u"level").toDouble(), 0.5);
        QCOMPARE(savedTank.value(u"props").toObject().value(u"x").toDouble(), 55.0);
        QVERIFY(QFile::exists(dir.filePath(u"tags.json"_s)));
    }

    void removedScreensLoseTheirFilesOnly()
    {
        auto project = sampleProject();
        QTemporaryDir dir;
        QVERIFY(ProjectSerializer::save(*project, dir.path()));
        QFile mine(dir.filePath(u"screens/notes.json"_s));
        QVERIFY(mine.open(QIODevice::WriteOnly));
        mine.close();

        (void)project->takeScreen(project->screens()[1]->id());
        project->screens()[1]->setName(u"Alarms"_s);
        QVERIFY(ProjectSerializer::save(*project, dir.path()));
        QCOMPARE(filesIn(dir.path()).keys(),
                 (QStringList{u"project.json"_s, u"screens/01_line_overview.json"_s,
                              u"screens/02_alarms.json"_s, u"screens/notes.json"_s}));
    }

    void migratesVersionZero()
    {
        const QString folder = QFINDTESTDATA("../../data/format-v0");
        QVERIFY(!folder.isEmpty());
        auto loaded = ProjectSerializer::load(folder);
        QVERIFY2(loaded, qPrintable(loaded ? QString() : loaded.error().message));
        QCOMPARE((*loaded)->name(), u"Old line"_s);
        const Screen& screen = *(*loaded)->screens()[0];
        QCOMPARE(screen.size(), QSizeF(800, 600));
        QCOMPARE(screen.objects()[0]->geometry(), QRectF(10, 20, 100, 50));

        QTemporaryDir dir;
        QVERIFY(ProjectSerializer::save(**loaded, dir.path()));
        QCOMPARE(readJson(dir.filePath(u"screens/01_main.json"_s)).value(u"formatVersion").toInt(),
                 ProjectSerializer::kCurrentFormatVersion);
    }

    void refusesNewerFormats()
    {
        QTemporaryDir dir;
        QVERIFY(ProjectSerializer::save(*sampleProject(), dir.path()));
        QJsonObject json = readJson(dir.filePath(u"project.json"_s));
        json.insert(u"formatVersion"_s, ProjectSerializer::kCurrentFormatVersion + 1);
        writeJson(dir.filePath(u"project.json"_s), json);
        const auto loaded = ProjectSerializer::load(dir.path());
        QVERIFY(!loaded);
        QCOMPARE(loaded.error().code, ErrorCode::TooNew);
        QVERIFY(loaded.error().message.contains(u"newer version"));
    }

    void reportsBrokenFiles()
    {
        QTemporaryDir dir;
        QCOMPARE(ProjectSerializer::load(dir.path()).error().code, ErrorCode::Io);

        QVERIFY(ProjectSerializer::save(*sampleProject(), dir.path()));
        QFile broken(dir.filePath(u"screens/02_filling.json"_s));
        QVERIFY(broken.open(QIODevice::WriteOnly));
        broken.write("{ \"objects\": [");
        broken.close();
        const auto loaded = ProjectSerializer::load(dir.path());
        QCOMPARE(loaded.error().code, ErrorCode::BadFormat);
        QVERIFY(loaded.error().message.contains(u"02_filling.json"));
    }

    void refusesPathsOutsideTheProject()
    {
        QTemporaryDir dir;
        QVERIFY(ProjectSerializer::save(*sampleProject(), dir.path()));
        QJsonObject json = readJson(dir.filePath(u"project.json"_s));
        json.insert(u"screens"_s, QJsonArray{u"screens/../../secret.json"_s});
        writeJson(dir.filePath(u"project.json"_s), json);
        QCOMPARE(ProjectSerializer::load(dir.path()).error().code, ErrorCode::BadFormat);
    }

    void replaceContentsKeepsTheNotifier()
    {
        Project project(u"A"_s);
        (void)project.insertScreen(std::make_unique<Screen>(u"S"_s));
        DocumentNotifier* notifier = &project.notifier();
        QSignalSpy reset(notifier, &DocumentNotifier::projectReset);
        QSignalSpy changed(notifier, &DocumentNotifier::propertyChanged);

        auto other = sampleProject();
        project.replaceContents(std::move(*other));
        QCOMPARE(reset.count(), 1);
        QCOMPARE(&project.notifier(), notifier);
        QCOMPARE(project.name(), u"Line 3"_s);
        QCOMPARE(project.screenCount(), std::size_t(3));
        // The new screens report through this project's notifier.
        QVERIFY(project.screens()[1]->objects()[0]->setProperty(Prop::X, 5.0));
        QCOMPARE(changed.count(), 1);
    }

    void assetsAreSavedOnlyWhileUsed()
    {
        auto project = sampleProject();
        const QString used = project->assets().add("picture bytes", u"logo.png"_s);
        const QString unused = project->assets().add("other bytes", u"old.svg"_s);
        Screen& screen = *project->screens()[1];
        auto view = ObjectRegistry::instance().create(ObjectTypeId{"graphicView"});
        QVERIFY(view->setProperty(Prop::Asset, used));
        const ObjectId viewId = view->id();
        QVERIFY(screen.insertObject(std::move(view)));
        QCOMPARE(ProjectSerializer::usedAssets(*project), std::vector{used});

        QTemporaryDir dir;
        QVERIFY(ProjectSerializer::save(*project, dir.path()));
        const QString usedFile = u"assets/"_s + used + u".png"_s;
        QVERIFY(QFile::exists(dir.filePath(usedFile)));
        QVERIFY(!QFile::exists(dir.filePath(u"assets/"_s + unused + u".svg"_s)));

        auto loaded = ProjectSerializer::load(dir.path());
        QVERIFY(loaded);
        QCOMPARE((*loaded)->assets().size(), std::size_t(1));
        QCOMPARE((*loaded)->assets().find(used)->fileName, u"logo.png"_s);
        QCOMPARE((*loaded)->assets().find(used)->data, QByteArray("picture bytes"));

        // Once nothing uses it, the next save removes the file.
        (void)(*loaded)->screens()[1]->takeObject(viewId);
        QVERIFY(ProjectSerializer::save(**loaded, dir.path()));
        QVERIFY(!QFile::exists(dir.filePath(usedFile)));
    }

    void changedAssetFilesAreReported()
    {
        auto project = sampleProject();
        const QString id = project->assets().add("picture bytes", u"logo.png"_s);
        auto view = ObjectRegistry::instance().create(ObjectTypeId{"graphicView"});
        QVERIFY(view->setProperty(Prop::Asset, id));
        QVERIFY(project->screens()[0]->insertObject(std::move(view)));
        QTemporaryDir dir;
        QVERIFY(ProjectSerializer::save(*project, dir.path()));
        QFile file(dir.filePath(u"assets/"_s + id + u".png"_s));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("edited outside");
        file.close();
        const auto loaded = ProjectSerializer::load(dir.path());
        QCOMPARE(loaded.error().code, ErrorCode::BadFormat);
    }

    void everyToolboxTypeSurvivesSaveAndReload()
    {
        // PLAN.md phase 5: every toolbox entry can be saved and reloaded.
        Project project(u"All objects"_s);
        auto screen = std::make_unique<Screen>(u"All"_s);
        int count = 0;
        for (const ObjectTypeInfo& info : ObjectRegistry::instance().types())
        {
            if (!info.inToolbox)
                continue;
            auto object = info.create(ObjectId::create());
            QVERIFY(object->setProperty(Prop::X, 10.0 * count));
            QVERIFY(object->setProperty(Prop::Name, info.key));
            QVERIFY(screen->insertObject(std::move(object)));
            ++count;
        }
        QCOMPARE(count, 36);
        (void)project.insertScreen(std::move(screen));

        QTemporaryDir first;
        QTemporaryDir second;
        QVERIFY(ProjectSerializer::save(project, first.path()));
        auto loaded = ProjectSerializer::load(first.path());
        QVERIFY(loaded);
        QVERIFY(sameProject(**loaded, project));
        QVERIFY(ProjectSerializer::save(**loaded, second.path()));
        QCOMPARE(filesIn(second.path()), filesIn(first.path()));
    }
};

QTEST_MAIN(ProjectSerializerTest)
#include "ProjectSerializerTest.moc"
