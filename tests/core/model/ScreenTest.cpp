#include "core/model/Screen.h"

#include "core/model/ScreenBuilder.h"
#include "core/objects/Ellipse.h"
#include "core/objects/Line.h"
#include "core/objects/Rectangle.h"
#include "core/property/Prop.h"

#include <QTest>

using namespace hmi;

namespace
{

std::vector<ObjectId> zOrder(const Screen& screen)
{
    std::vector<ObjectId> ids;
    for (const auto& object : screen.objects())
        ids.push_back(object->id());
    return ids;
}

} // namespace

class ScreenTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void defaults()
    {
        Screen screen(QStringLiteral("Overview"));
        QCOMPARE(screen.name(), QStringLiteral("Overview"));
        QCOMPARE(screen.size(), Screen::kDefaultSize);
        QVERIFY(!screen.id().isNull());
        QCOMPARE(screen.objectCount(), std::size_t(0));
    }

    void insertAppendsOnTop()
    {
        Screen screen(QStringLiteral("S"));
        auto first = screen.insertObject(std::make_unique<Rectangle>());
        auto second = screen.insertObject(std::make_unique<Ellipse>());
        QVERIFY(first && second);
        QCOMPARE(screen.objectCount(), std::size_t(2));
        QCOMPARE(screen.indexOf((*first)->id()), std::optional<std::size_t>(0));
        QCOMPARE(screen.indexOf((*second)->id()), std::optional<std::size_t>(1));
        QCOMPARE(screen.object((*second)->id()), *second);
    }

    void insertAtIndex()
    {
        Screen screen(QStringLiteral("S"));
        QVERIFY(screen.insertObject(std::make_unique<Rectangle>()));
        auto bottom = screen.insertObject(std::make_unique<Line>(), 0);
        QVERIFY(bottom);
        QCOMPARE(screen.objects().front().get(), *bottom);
    }

    void insertRejectsBadInput()
    {
        Screen screen(QStringLiteral("S"));
        QCOMPARE(screen.insertObject(nullptr).error().code, ErrorCode::InvalidArgument);
        QCOMPARE(screen.insertObject(std::make_unique<Rectangle>(), 1).error().code,
                 ErrorCode::OutOfRange);

        auto rect = std::make_unique<Rectangle>();
        const ObjectId id = rect->id();
        QVERIFY(screen.insertObject(std::move(rect)));
        QCOMPARE(screen.insertObject(std::make_unique<Rectangle>(id)).error().code,
                 ErrorCode::Duplicate);
        QCOMPARE(screen.objectCount(), std::size_t(1));
    }

    void takeReturnsOwnership()
    {
        Screen screen(QStringLiteral("S"));
        auto inserted = screen.insertObject(std::make_unique<Rectangle>());
        const ObjectId id = (*inserted)->id();

        auto taken = screen.takeObject(id);
        QVERIFY(taken);
        QCOMPARE((*taken)->id(), id);
        QCOMPARE(screen.objectCount(), std::size_t(0));
        QVERIFY(screen.object(id) == nullptr);
        QCOMPARE(screen.takeObject(id).error().code, ErrorCode::NotFound);
    }

    void moveChangesZOrder()
    {
        auto screen = ScreenBuilder(QStringLiteral("S"))
                          .add(Rectangle::kTypeId, {0, 0, 10, 10})
                          .add(Ellipse::kTypeId, {0, 0, 10, 10})
                          .add(Line::kTypeId, {0, 0, 10, 10})
                          .build();
        const auto original = zOrder(*screen);

        QVERIFY(screen->moveObject(original[0], 2));
        QCOMPARE(zOrder(*screen), (std::vector{original[1], original[2], original[0]}));

        QVERIFY(screen->moveObject(original[0], 0));
        QCOMPARE(zOrder(*screen), original);

        QVERIFY(screen->moveObject(original[2], 1));
        QCOMPARE(zOrder(*screen), (std::vector{original[0], original[2], original[1]}));

        QCOMPARE(screen->moveObject(original[0], 3).error().code, ErrorCode::OutOfRange);
        QCOMPARE(screen->moveObject(ObjectId::create(), 0).error().code, ErrorCode::NotFound);
    }

    void sizeAndBackgroundValidation()
    {
        Screen screen(QStringLiteral("S"));
        QVERIFY(screen.setSize({800, 480}));
        QCOMPARE(screen.size(), QSizeF(800, 480));
        QCOMPARE(screen.setSize({0, 480}).error().code, ErrorCode::OutOfRange);
        QCOMPARE(screen.setSize({-1, -1}).error().code, ErrorCode::OutOfRange);
        QVERIFY(screen.setBackgroundColor(Qt::black));
        QVERIFY(!screen.setBackgroundColor(QColor()));
        QCOMPARE(screen.backgroundColor(), QColor(Qt::black));
    }

    void cloneIsDeepAndEqual()
    {
        auto screen = ScreenBuilder(QStringLiteral("Filling"))
                          .size({1024, 600})
                          .background(Qt::white)
                          .add(Rectangle::kTypeId, {70, 110, 110, 192})
                          .with(Prop::FillColor, QColor(0x3B, 0x6F, 0xD4))
                          .add(Line::kTypeId, {0, 0, 50, 0})
                          .build();

        const auto copy = screen->clone();
        QCOMPARE(copy->id(), screen->id());
        QCOMPARE(copy->name(), screen->name());
        QCOMPARE(copy->size(), screen->size());
        QCOMPARE(copy->backgroundColor(), screen->backgroundColor());
        QCOMPARE(zOrder(*copy), zOrder(*screen));
        for (std::size_t i = 0; i < screen->objectCount(); ++i)
        {
            QVERIFY(copy->objects()[i].get() != screen->objects()[i].get());
            QVERIFY(copy->objects()[i]->hasSameProperties(*screen->objects()[i]));
        }
    }

    void builderSetsGeometryAndProperties()
    {
        ScreenBuilder builder(QStringLiteral("S"));
        builder.add(Rectangle::kTypeId, {70, 110, 110, 192}).with(Prop::CornerRadius, 14.0);
        const ObjectId id = builder.lastId();
        auto screen = builder.build();

        const HmiObject* tank = screen->object(id);
        QVERIFY(tank);
        QCOMPARE(tank->geometry(), QRectF(70, 110, 110, 192));
        QCOMPARE(tank->value<double>(Prop::CornerRadius), 14.0);
    }
};

QTEST_MAIN(ScreenTest)
#include "ScreenTest.moc"
