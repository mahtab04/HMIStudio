#include "core/model/HmiStyle.h"

#include "core/io/ProjectSerializer.h"
#include "core/model/Project.h"
#include "core/model/ScreenBuilder.h"
#include "core/objects/Gauge.h"
#include "core/objects/IoField.h"
#include "core/objects/Motor.h"
#include "core/objects/Slider.h"
#include "core/property/Prop.h"

#include <QTemporaryDir>
#include <QTest>

using namespace hmi;
using namespace Qt::StringLiterals;

class HmiStyleTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void appliesOnlyToStyledProperties()
    {
        HmiStyle style;
        style.face = QColor(0x10, 0x20, 0x30);
        style.line = QColor(0x40, 0x50, 0x60);
        style.text = QColor(0x70, 0x80, 0x90);
        style.accent = QColor(0xA0, 0xB0, 0xC0);
        style.fontFamily = u"Courier"_s;

        IoField field;
        QVERIFY(style.applyTo(field));
        QCOMPARE(field.value<QColor>(Prop::FillColor), style.face);
        QCOMPARE(field.value<QColor>(Prop::LineColor), style.line);
        QCOMPARE(field.value<QColor>(Prop::TextColor), style.text);
        QCOMPARE(field.value<QFont>(Prop::Font).family(), u"Courier"_s);
        QCOMPARE(field.value<QFont>(Prop::Font).pixelSize(), 16); // size kept
        QVERIFY(!style.applyTo(field));                           // nothing left to change

        Slider slider;
        QVERIFY(style.applyTo(slider));
        QCOMPARE(slider.value<QColor>(Prop::AccentColor), style.accent);

        // The gauge's red needle and the motor's white badge text are not
        // style colours.
        Gauge gauge;
        style.applyTo(gauge);
        QCOMPARE(gauge.value<QColor>(Prop::AccentColor), QColor(0xDC, 0x26, 0x26));
        QCOMPARE(gauge.value<QColor>(Prop::FillColor), style.face);
        Motor motor;
        style.applyTo(motor);
        QCOMPARE(motor.value<QColor>(Prop::TextColor), QColor(Qt::white));
    }

    void emptyFontFamilyKeepsTheObjects()
    {
        IoField field;
        const QString family = field.value<QFont>(Prop::Font).family();
        HmiStyle{}.applyTo(field);
        QCOMPARE(field.value<QFont>(Prop::Font).family(), family);
    }

    void roundTripsThroughJsonAndProjectFiles()
    {
        HmiStyle style;
        style.accent = QColor(1, 2, 3);
        style.fontFamily = u"Arial"_s;
        QCOMPARE(HmiStyle::fromJson(style.toJson()), style);
        QCOMPARE(HmiStyle::fromJson({}), HmiStyle{}); // missing: defaults

        Project project(u"P"_s);
        (void)project.insertScreen(ScreenBuilder(u"S"_s).build());
        project.setStyle(style);
        QTemporaryDir dir;
        QVERIFY(ProjectSerializer::save(project, dir.path()));
        auto loaded = ProjectSerializer::load(dir.path());
        QVERIFY(loaded);
        QCOMPARE((*loaded)->style(), style);
    }
};

QTEST_MAIN(HmiStyleTest)
#include "HmiStyleTest.moc"
