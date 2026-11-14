#include "core/model/Ids.h"

#include <QSet>
#include <QTest>

#include <unordered_set>

using namespace hmi;

class IdsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void hashIdIsComputedAtCompileTime()
    {
        constexpr PropertyId a{"fillColor"};
        constexpr PropertyId b{"fillColor"};
        constexpr PropertyId c{"lineColor"};
        static_assert(a == b);
        static_assert(a != c);
        static_assert(!a.isNull());
        static_assert(PropertyId{}.isNull());
    }

    void hashIdMatchesRuntimeKey()
    {
        constexpr PropertyId fromLiteral{"cornerRadius"};
        QCOMPARE(PropertyId::fromKey(u"cornerRadius"), fromLiteral);
        QVERIFY(PropertyId::fromKey(u"cornerradius") != fromLiteral);
    }

    void hashIdOrderingIsConsistent()
    {
        const PropertyId a{"a"};
        const PropertyId b{"b"};
        QVERIFY((a < b) != (b < a));
        QVERIFY((a <=> a) == 0);
    }

    void distinctTagsAreDistinctTypes()
    {
        static_assert(!std::is_same_v<PropertyId, ObjectTypeId>);
        static_assert(!std::is_same_v<ObjectId, ScreenId>);
        static_assert(!std::is_convertible_v<PropertyId, ObjectTypeId>);
    }

    void uuidIdsAreUnique()
    {
        QSet<ObjectId> ids;
        std::unordered_set<ObjectId> stdIds;
        for (int i = 0; i < 1000; ++i)
        {
            const ObjectId id = ObjectId::create();
            QVERIFY(!id.isNull());
            ids.insert(id);
            stdIds.insert(id);
        }
        QCOMPARE(ids.size(), 1000);
        QCOMPARE(stdIds.size(), std::size_t(1000));
        QVERIFY(ObjectId().isNull());
    }

    void uuidIdRoundTripsThroughString()
    {
        const ObjectId id = ObjectId::create();
        const auto parsed = ObjectId::fromString(id.toString());
        QVERIFY(parsed.has_value());
        QCOMPARE(*parsed, id);
        QVERIFY(!ObjectId::fromString(u"not a uuid").has_value());
    }

    void uuidIdOrderingIsTotal()
    {
        const ObjectId a = ObjectId::create();
        const ObjectId b = ObjectId::create();
        QVERIFY((a <=> a) == 0);
        QVERIFY((a < b) != (b < a));
        QVERIFY((a <=> b) == 0 ? a == b : a != b);
    }
};

QTEST_GUILESS_MAIN(IdsTest)
#include "IdsTest.moc"
