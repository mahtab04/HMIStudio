#include "core/model/GroupObject.h"

#include "core/model/ObjectVisitor.h"
#include "core/objects/Ellipse.h"
#include "core/objects/ObjectContract.h"
#include "core/objects/Rectangle.h"
#include "core/property/Prop.h"
#include "core/registry/ObjectRegistry.h"
#include "core/spatial/HitTester.h"

#include <QTest>

using namespace hmi;

namespace
{

std::unique_ptr<HmiObject> rect(const QRectF& g, double rotation = 0.0)
{
    auto object = std::make_unique<Rectangle>();
    [[maybe_unused]] const bool ok = object->setProperty(Prop::X, g.x()) &&
                                     object->setProperty(Prop::Y, g.y()) &&
                                     object->setProperty(Prop::Width, g.width()) &&
                                     object->setProperty(Prop::Height, g.height()) &&
                                     object->setProperty(Prop::Rotation, rotation);
    Q_ASSERT(ok);
    return object;
}

std::unique_ptr<GroupObject> twoRects()
{
    std::vector<std::unique_ptr<HmiObject>> objects;
    objects.push_back(rect({10, 20, 100, 50}));
    objects.push_back(rect({150, 40, 50, 80}));
    return GroupObject::create(std::move(objects));
}

bool fuzzyRect(const QRectF& a, const QRectF& b)
{
    const auto near = [](double x, double y)
    {
        return std::abs(x - y) < 1e-6;
    };
    return near(a.x(), b.x()) && near(a.y(), b.y()) && near(a.width(), b.width()) &&
           near(a.height(), b.height());
}

} // namespace

class GroupObjectTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void contract()
    {
        GroupObject group;
        test::checkDefaults(group);
        const ObjectTypeInfo* info = ObjectRegistry::instance().find(GroupObject::kTypeId);
        QVERIFY(info);
        QVERIFY(!info->inToolbox);
    }

    void createWrapsChildren()
    {
        const auto group = twoRects();
        QCOMPARE(group->geometry(), QRectF(10, 20, 190, 100));
        QCOMPARE(group->children().size(), std::size_t(2));
        // Children are stored relative to the group.
        QCOMPARE(group->children()[0]->geometry(), QRectF(0, 0, 100, 50));
        QCOMPARE(group->children()[1]->geometry(), QRectF(140, 20, 50, 80));
    }

    void ungroupRestoresScreenGeometry()
    {
        const auto group = twoRects();
        const auto children = group->ungroupedChildren();
        QCOMPARE(children.size(), std::size_t(2));
        QCOMPARE(children[0]->geometry(), QRectF(10, 20, 100, 50));
        QCOMPARE(children[1]->geometry(), QRectF(150, 40, 50, 80));
        QCOMPARE(children[0]->id(), group->children()[0]->id());
    }

    void ungroupAppliesMoveScaleAndRotation()
    {
        auto group = twoRects();
        // Move by (5, 5) and double the width.
        QVERIFY(group->setProperty(Prop::X, 15.0));
        QVERIFY(group->setProperty(Prop::Y, 25.0));
        QVERIFY(group->setProperty(Prop::Width, 380.0));
        auto children = group->ungroupedChildren();
        QVERIFY(fuzzyRect(children[0]->geometry(), {15, 25, 200, 50}));
        QVERIFY(fuzzyRect(children[1]->geometry(), {295, 45, 100, 80}));

        // A 90° turn of the group turns each child and moves it about the
        // group's centre.
        QVERIFY(group->setProperty(Prop::Width, 190.0));
        QVERIFY(group->setProperty(Prop::Rotation, 90.0));
        children = group->ungroupedChildren();
        QCOMPARE(children[0]->rotation(), 90.0);
        const QPointF centre = group->geometry().center();
        const QPointF before(15 + 50, 25 + 25); // first child's centre before turning
        const QPointF expected =
            centre + QPointF(-(before.y() - centre.y()), before.x() - centre.x());
        QVERIFY(fuzzyRect(children[0]->geometry(),
                          QRectF(expected - QPointF(50, 25), QSizeF(100, 50))));
    }

    void hitShapeFollowsChildren()
    {
        const auto group = twoRects();
        // Inside the first child, inside the second, and in the gap between.
        QVERIFY(HitTester::hits(*group, {20, 30}, 0.0));
        QVERIFY(HitTester::hits(*group, {170, 100}, 0.0));
        QVERIFY(!HitTester::hits(*group, {130, 100}, 0.0));
    }

    void cloneIsDeep()
    {
        const auto group = twoRects();
        const auto copy = group->clone();
        QCOMPARE(copy->id(), group->id());
        QVERIFY(copy->hasSameProperties(*group));
        QCOMPARE(copy->children()[0]->id(), group->children()[0]->id());
        QVERIFY(copy->children()[0].get() != group->children()[0].get());

        const auto pasted = group->cloneWithNewId();
        QVERIFY(pasted->id() != group->id());
        QVERIFY(pasted->children()[0]->id() != group->children()[0]->id());
        QVERIFY(pasted->hasSameProperties(*group));
    }

    void comparisonIncludesChildren()
    {
        const auto a = twoRects();
        const auto b = twoRects();
        QVERIFY(a->hasSameProperties(*b));
        std::vector<std::unique_ptr<HmiObject>> other;
        other.push_back(rect({10, 20, 100, 50}));
        other.push_back(rect({150, 40, 50, 81}));
        QVERIFY(!a->hasSameProperties(*GroupObject::create(std::move(other))));
    }

    void leafTypesRejectChildren()
    {
        Ellipse ellipse;
        QVERIFY(ellipse.setChildren({}));
        std::vector<std::unique_ptr<HmiObject>> children;
        children.push_back(rect({0, 0, 1, 1}));
        QVERIFY(!ellipse.setChildren(std::move(children)));
        QVERIFY(ellipse.children().empty());
    }

    void visitorSeesGroup()
    {
        struct Counter : ObjectVisitor
        {
            using ObjectVisitor::visit;
            int groups = 0;
            void visit(GroupObject& /*object*/) override { ++groups; }
        } counter;
        twoRects()->accept(counter);
        QCOMPARE(counter.groups, 1);
    }
};

QTEST_MAIN(GroupObjectTest)
#include "GroupObjectTest.moc"
