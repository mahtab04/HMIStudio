#include "core/registry/ObjectRegistry.h"

#include "core/model/HmiObject.h"
#include "core/objects/Ellipse.h"
#include "core/objects/Line.h"
#include "core/objects/Rectangle.h"
#include "core/objects/TextBox.h"

#include <QRegularExpression>
#include <QTest>

using namespace hmi;

class ObjectRegistryTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void basicObjectsAreRegistered()
    {
        const auto basics = ObjectRegistry::instance().types(ObjectCategory::BasicObjects);
        QStringList keys;
        for (const ObjectTypeInfo* info : basics)
            keys << info->key;
        QCOMPARE(keys, (QStringList{QStringLiteral("ellipse"), QStringLiteral("line"),
                                    QStringLiteral("rectangle"), QStringLiteral("textBox")}));
    }

    void entriesHaveToolboxMetadata()
    {
        for (const ObjectTypeInfo& info : ObjectRegistry::instance().types())
        {
            QVERIFY(info.displayName && *info.displayName);
            QVERIFY(info.iconPath.startsWith(u":/icons/"));
            QCOMPARE(ObjectTypeId::fromKey(info.key), info.id);
        }
    }

    void createsEachTypeWithDefaults()
    {
        for (const ObjectTypeInfo& info : ObjectRegistry::instance().types())
        {
            const auto object = info.create();
            QVERIFY(object);
            QCOMPARE(object->typeId(), info.id);
            QVERIFY(!object->id().isNull());
        }
    }

    void createdObjectsHaveDistinctIds()
    {
        const auto a = ObjectRegistry::instance().create(Rectangle::kTypeId);
        const auto b = ObjectRegistry::instance().create(Rectangle::kTypeId);
        QVERIFY(a->id() != b->id());
    }

    void unknownTypeGivesNothing()
    {
        QVERIFY(ObjectRegistry::instance().create(ObjectTypeId{"gauge"}) == nullptr);
        QVERIFY(ObjectRegistry::instance().find(ObjectTypeId{"gauge"}) == nullptr);
        QVERIFY(ObjectRegistry::instance().findByKey(u"gauge") == nullptr);
    }

    void rejectsDuplicateAndMismatchedEntries()
    {
        ObjectRegistry& registry = ObjectRegistry::instance();
        const std::size_t before = registry.types().size();

        QTest::ignoreMessage(QtWarningMsg,
                             QRegularExpression(QStringLiteral("rejected type 'rectangle'")));
        QVERIFY(!registry.add({.id = Rectangle::kTypeId,
                               .key = QStringLiteral("rectangle"),
                               .displayName = "Again",
                               .create = [] { return std::unique_ptr<HmiObject>(); }}));

        QTest::ignoreMessage(QtWarningMsg,
                             QRegularExpression(QStringLiteral("rejected type 'other'")));
        QVERIFY(!registry.add({.id = ObjectTypeId{"something"},
                               .key = QStringLiteral("other"),
                               .displayName = "Mismatch",
                               .create = [] { return std::unique_ptr<HmiObject>(); }}));

        QCOMPARE(registry.types().size(), before);
    }
};

QTEST_MAIN(ObjectRegistryTest)
#include "ObjectRegistryTest.moc"
