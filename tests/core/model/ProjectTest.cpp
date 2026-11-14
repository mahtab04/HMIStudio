#include "core/model/Project.h"

#include "core/model/ScreenBuilder.h"
#include "core/objects/Ellipse.h"
#include "core/objects/Rectangle.h"
#include "core/property/Prop.h"

#include <QSignalSpy>
#include <QTest>

using namespace hmi;

class ProjectTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void insertAndTakeScreens()
    {
        Project project(QStringLiteral("Line 3"));
        QSignalSpy added(&project.notifier(), &DocumentNotifier::screenAdded);
        QSignalSpy removed(&project.notifier(), &DocumentNotifier::screenRemoved);

        auto overview = project.insertScreen(std::make_unique<Screen>(QStringLiteral("Overview")));
        auto filling = project.insertScreen(std::make_unique<Screen>(QStringLiteral("Filling")));
        auto first = project.insertScreen(std::make_unique<Screen>(QStringLiteral("Start")), 0);
        QVERIFY(overview && filling && first);
        QCOMPARE(project.screenCount(), std::size_t(3));
        QCOMPARE(project.screens().front().get(), *first);

        QCOMPARE(added.count(), 3);
        QCOMPARE(added.at(2).at(0).value<ScreenId>(), (*first)->id());
        QCOMPARE(added.at(2).at(1).toInt(), 0);

        const ScreenId id = (*filling)->id();
        auto taken = project.takeScreen(id);
        QVERIFY(taken);
        QCOMPARE(project.screenCount(), std::size_t(2));
        QCOMPARE(removed.count(), 1);
        QCOMPARE(removed.at(0).at(0).value<ScreenId>(), id);

        QCOMPARE(project.takeScreen(id).error().code, ErrorCode::NotFound);
        QCOMPARE(project.insertScreen(nullptr).error().code, ErrorCode::InvalidArgument);
    }

    void rejectsDuplicateScreen()
    {
        Project project;
        auto screen = std::make_unique<Screen>(QStringLiteral("A"));
        const ScreenId id = screen->id();
        QVERIFY(project.insertScreen(std::move(screen)));
        auto duplicate = std::make_unique<Screen>(QStringLiteral("B"), Screen::kDefaultSize, id);
        QCOMPARE(project.insertScreen(std::move(duplicate)).error().code, ErrorCode::Duplicate);
    }

    void objectChangesAreNotified()
    {
        Project project;
        Screen* screen = *project.insertScreen(std::make_unique<Screen>(QStringLiteral("S")));
        DocumentNotifier& notifier = project.notifier();
        QSignalSpy added(&notifier, &DocumentNotifier::objectAdded);
        QSignalSpy removed(&notifier, &DocumentNotifier::objectRemoved);
        QSignalSpy reordered(&notifier, &DocumentNotifier::objectsReordered);
        QSignalSpy changed(&notifier, &DocumentNotifier::propertyChanged);
        QSignalSpy screenChanged(&notifier, &DocumentNotifier::screenChanged);

        HmiObject* rect = *screen->insertObject(std::make_unique<Rectangle>());
        HmiObject* ellipse = *screen->insertObject(std::make_unique<Ellipse>());
        QCOMPARE(added.count(), 2);
        QCOMPARE(added.at(1).at(0).value<ScreenId>(), screen->id());
        QCOMPARE(added.at(1).at(1).value<ObjectId>(), ellipse->id());
        QCOMPARE(added.at(1).at(2).toInt(), 1);

        QVERIFY(rect->setProperty(Prop::X, 25.0));
        QCOMPARE(changed.count(), 1);
        QCOMPARE(changed.at(0).at(0).value<ObjectId>(), rect->id());
        QCOMPARE(changed.at(0).at(1).value<PropertyId>(), Prop::X);

        // Same value again, or a rejected value: no notification.
        QVERIFY(rect->setProperty(Prop::X, 25.0));
        QVERIFY(!rect->setProperty(Prop::X, QStringLiteral("left")));
        QCOMPARE(changed.count(), 1);

        QVERIFY(screen->moveObject(rect->id(), 1));
        QCOMPARE(reordered.count(), 1);

        screen->setName(QStringLiteral("Renamed"));
        screen->setName(QStringLiteral("Renamed"));
        QCOMPARE(screenChanged.count(), 1);

        const ObjectId rectId = rect->id();
        auto taken = screen->takeObject(rectId);
        QVERIFY(taken);
        QCOMPARE(removed.count(), 1);
        QCOMPARE(removed.at(0).at(1).value<ObjectId>(), rectId);

        // A removed object no longer reports to the document.
        QVERIFY((*taken)->setProperty(Prop::X, 50.0));
        QCOMPARE(changed.count(), 1);
    }

    void screenBuiltOutsideIsAttachedOnInsert()
    {
        auto built =
            ScreenBuilder(QStringLiteral("S")).add(Rectangle::kTypeId, {0, 0, 10, 10}).build();
        HmiObject* rect = built->objects().front().get();

        Project project;
        QSignalSpy changed(&project.notifier(), &DocumentNotifier::propertyChanged);
        QVERIFY(rect->setProperty(Prop::Y, 5.0)); // not yet in the project
        QCOMPARE(changed.count(), 0);

        auto inserted = project.insertScreen(std::move(built));
        QVERIFY(inserted);
        QVERIFY(rect->setProperty(Prop::Y, 6.0));
        QCOMPARE(changed.count(), 1);

        auto taken = project.takeScreen((*inserted)->id());
        QVERIFY(rect->setProperty(Prop::Y, 7.0));
        QCOMPARE(changed.count(), 1);
    }
};

QTEST_MAIN(ProjectTest)
#include "ProjectTest.moc"
