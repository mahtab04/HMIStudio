#include "core/objects/TextBox.h"

#include "core/model/ObjectVisitor.h"
#include "core/objects/ObjectContract.h"

#include <QTest>

using namespace hmi;

class TextBoxTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void defaults()
    {
        TextBox text;
        test::checkDefaults(text);
        QCOMPARE(text.typeId(), TextBox::kTypeId);
        QCOMPARE(text.value<QString>(Prop::Text), QStringLiteral("Text"));
        QCOMPARE(text.value<QFont>(Prop::Font).pixelSize(), 16);
        QCOMPARE(text.value<QColor>(Prop::FillColor).alpha(), 0);
    }

    void clone()
    {
        TextBox text;
        QFont font;
        font.setPixelSize(30);
        font.setBold(true);
        QVERIFY(text.setProperty(Prop::Font, font));
        QVERIFY(text.setProperty(Prop::Text, QStringLiteral("Filling Station")));
        test::checkClone(text);
        QCOMPARE(text.clone()->value<QFont>(Prop::Font), font);
    }

    void rejections()
    {
        TextBox text;
        test::checkRejections(text);
        QVERIFY(!text.setProperty(Prop::Text, 5));
        QVERIFY(!text.setProperty(Prop::HorizontalAlignment, EnumValue{3}));
    }

    void registered() { test::checkRegistered(TextBox::kTypeId, u"textBox"); }

    void alignmentFlags()
    {
        TextBox text;
        QCOMPARE(text.textFlags(), int(Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap));
        QVERIFY(text.setProperty(Prop::HorizontalAlignment, EnumValue{TextBox::AlignRight}));
        QVERIFY(text.setProperty(Prop::VerticalAlignment, EnumValue{TextBox::AlignBottom}));
        QVERIFY(text.setProperty(Prop::WordWrap, false));
        QCOMPARE(text.textFlags(), int(Qt::AlignRight | Qt::AlignBottom));
    }

    void hitShapeIsBounds()
    {
        TextBox text;
        QVERIFY(text.hitShape().contains(QPointF(1, 1)));
        QVERIFY(!text.hitShape().contains(QPointF(121, 1)));
    }

    void paintsBackgroundAndSkipsTextAtLowZoom()
    {
        TextBox text;
        QVERIFY(text.setProperty(Prop::FillColor, QColor(Qt::yellow)));
        QVERIFY(text.setProperty(Prop::Text, QStringLiteral("████████")));
        QVERIFY(text.setProperty(Prop::TextColor, QColor(Qt::black)));

        const QImage low = test::render(text, PaintContext::kLowDetailZoom / 2.0);
        for (int x = 0; x < low.width(); x += 4)
            QVERIFY(test::sameColour(low.pixel(x, 16), Qt::yellow));

        const QImage full = test::render(text);
        bool hasText = false;
        for (int x = 0; x < full.width() && !hasText; ++x)
            hasText = !test::sameColour(full.pixel(x, 16), Qt::yellow, 40);
        QVERIFY(hasText);
    }

    void visitorDispatchesToTextBox()
    {
        struct Counter : ObjectVisitor
        {
            using ObjectVisitor::visit;
            int texts = 0;
            void visit(TextBox&) override { ++texts; }
        } counter;
        TextBox text;
        text.accept(counter);
        QCOMPARE(counter.texts, 1);
    }
};

QTEST_MAIN(TextBoxTest)
#include "TextBoxTest.moc"
