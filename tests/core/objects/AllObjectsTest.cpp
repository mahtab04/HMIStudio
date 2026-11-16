// The object contract (ObjectContract.h) for every registered toolbox type,
// so a new type is checked as soon as it registers: defaults, clone,
// rejections, painting, hit shape, and a JSON round trip.
//
// Set HMI_OBJECT_SHEET=<file.png> to save every type at its default size
// on one sheet, for a visual check.

#include "core/io/ObjectJson.h"
#include "core/model/HmiObject.h"
#include "core/objects/ObjectContract.h"
#include "core/registry/ObjectRegistry.h"

#include <QPainter>
#include <QTest>

using namespace hmi;
using namespace Qt::StringLiterals;

Q_DECLARE_METATYPE(hmi::ObjectTypeId)

class AllObjectsTest : public QObject
{
    Q_OBJECT

private:
    static void addTypeRows()
    {
        QTest::addColumn<ObjectTypeId>("type");
        for (const ObjectTypeInfo& info : ObjectRegistry::instance().types())
        {
            if (info.inToolbox)
                QTest::newRow(qPrintable(info.key)) << info.id;
        }
    }

private Q_SLOTS:
    void contract_data() { addTypeRows(); }
    void contract()
    {
        QFETCH(ObjectTypeId, type);
        const ObjectTypeInfo* info = ObjectRegistry::instance().find(type);
        test::checkRegistered(type, info->key);
        auto object = ObjectRegistry::instance().create(type);
        test::checkDefaults(*object);
        test::checkRejections(*object);
        test::checkClone(*object);
        QVERIFY(info->displayName && *info->displayName);
        QVERIFY(info->iconPath.startsWith(u":/icons/"));
    }

    // Every type draws something at its default size. (Callers save and
    // restore the painter around paint(), so a type may leave it changed.)
    void paints_data() { addTypeRows(); }
    void paints()
    {
        QFETCH(ObjectTypeId, type);
        const auto object = ObjectRegistry::instance().create(type);
        const QSize size = object->geometry().size().toSize().expandedTo(QSize(1, 1));
        QImage image(size, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        object->paint(painter, PaintContext{.zoom = 1.0});
        painter.end();

        bool drawn = false;
        for (int y = 0; y < image.height() && !drawn; ++y)
        {
            for (int x = 0; x < image.width() && !drawn; ++x)
                drawn = qAlpha(image.pixel(x, y)) > 0;
        }
        QVERIFY2(drawn, "nothing was painted");

        // Painting at low detail and at a deep zoom must not fail either.
        QPainter again(&image);
        object->paint(again, PaintContext{.zoom = 0.2});
        object->paint(again, PaintContext{.zoom = 8.0});
    }

    // The hit shape lies within the bounds (plus a stroke) and is not empty.
    void hitShapeFitsBounds_data() { addTypeRows(); }
    void hitShapeFitsBounds()
    {
        QFETCH(ObjectTypeId, type);
        const auto object = ObjectRegistry::instance().create(type);
        const QPainterPath shape = object->hitShape();
        QVERIFY(!shape.isEmpty());
        const QRectF bounds(QPointF(0, 0), object->geometry().size());
        QVERIFY(bounds.adjusted(-10, -10, 10, 10).contains(shape.boundingRect()));
    }

    void jsonRoundTrip_data() { addTypeRows(); }
    void jsonRoundTrip()
    {
        QFETCH(ObjectTypeId, type);
        auto object = ObjectRegistry::instance().create(type);
        QVERIFY(object->setProperty(Prop::Name, QStringLiteral("saved")));
        auto back = ObjectJson::read(ObjectJson::write(*object));
        QVERIFY(back);
        QCOMPARE((*back)->id(), object->id());
        QVERIFY((*back)->hasSameProperties(*object));
    }

    // Process symbols with a stretch mode are pictures (as in WinCC symbol
    // libraries): twice the size draws the same picture twice as large, and
    // a rectangle of another shape keeps the proportions unless "Stretch" is
    // chosen.
    void scaledSymbolsKeepTheirLook_data()
    {
        QTest::addColumn<ObjectTypeId>("type");
        for (const ObjectTypeInfo& info : ObjectRegistry::instance().types())
        {
            const auto object = info.create(ObjectId::create());
            if (info.inToolbox && findDescriptor(object->descriptors(), Prop::StretchMode))
                QTest::newRow(qPrintable(info.key)) << info.id;
        }
    }

    // Resized evenly, every type looks the same, only larger: no part keeps
    // a fixed size while the rest grows (a switch track capped at 28 px in a
    // large rectangle). Types whose content keeps its size on purpose are
    // left out: fields and lists keep their font size, as in WinCC, lists
    // and tables show more rows when taller, and a shape's line width (a
    // pipe's diameter) is a property that resizing does not change, nor are
    // the numbers of a bar's or slider's scale.
    void growsAsOneWhenScaledEvenly_data()
    {
        QTest::addColumn<ObjectTypeId>("type");
        const QStringList fixedContent{u"ioField"_s,
                                       u"listBox"_s,
                                       u"alarmView"_s,
                                       u"recipeView"_s,
                                       u"diagnosticsView"_s,
                                       u"browser"_s,
                                       u"symbolicIoField"_s,
                                       u"screenWindow"_s,
                                       u"graphicView"_s,
                                       u"textBox"_s,
                                       u"button"_s,
                                       u"line"_s,
                                       u"polyline"_s,
                                       u"polygon"_s,
                                       u"rectangle"_s,
                                       u"ellipse"_s,
                                       u"circle"_s,
                                       u"pipe"_s,
                                       u"bar"_s,
                                       u"slider"_s};
        for (const ObjectTypeInfo& info : ObjectRegistry::instance().types())
        {
            if (info.inToolbox && !fixedContent.contains(info.key))
                QTest::newRow(qPrintable(info.key)) << info.id;
        }
    }
    void growsAsOneWhenScaledEvenly()
    {
        QFETCH(ObjectTypeId, type);
        auto object = ObjectRegistry::instance().create(type);
        // Glyphs are hinted differently at each size; compare the drawing.
        for (const PropertyId text : {Prop::Text, Prop::Items, Prop::OnText, Prop::OffText})
        {
            if (findDescriptor(object->descriptors(), text))
                QVERIFY(object->setProperty(text, QString()));
        }
        const QSizeF design = object->geometry().size().expandedTo(QSizeF(1, 1));
        const auto render = [&](double scale)
        {
            (void)object->setProperty(
                Prop::Width, object->geometry().width() > 0 ? design.width() * scale : 0.0);
            (void)object->setProperty(
                Prop::Height, object->geometry().height() > 0 ? design.height() * scale : 0.0);
            const QSize pixels = (design * scale).toSize();
            QImage image(pixels, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setRenderHint(QPainter::TextAntialiasing);
            object->paint(painter, PaintContext{});
            painter.end();
            return image;
        };
        const QImage small = render(1.0);
        const QImage large =
            render(2.0).scaled(small.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        int differing = 0;
        for (int y = 0; y < small.height(); ++y)
        {
            for (int x = 0; x < small.width(); ++x)
            {
                const QRgb a = small.pixel(x, y);
                const QRgb b = large.pixel(x, y);
                if (std::abs(qAlpha(a) - qAlpha(b)) > 64 || std::abs(qGreen(a) - qGreen(b)) > 64)
                    ++differing;
            }
        }
        QVERIFY2(differing < small.width() * small.height() / 20,
                 qPrintable(QStringLiteral("%1 of %2 pixels differ")
                                .arg(differing)
                                .arg(small.width() * small.height())));
    }
    void scaledSymbolsKeepTheirLook()
    {
        QFETCH(ObjectTypeId, type);
        auto object = ObjectRegistry::instance().create(type);
        const QSizeF design = object->geometry().size();
        // Glyphs are hinted differently at each size; compare the drawing.
        if (findDescriptor(object->descriptors(), Prop::Text))
            QVERIFY(object->setProperty(Prop::Text, QString()));
        const auto render = [&](QSizeF size)
        {
            // Sizes from the default are always valid.
            (void)object->setProperty(Prop::Width, size.width());
            (void)object->setProperty(Prop::Height, size.height());
            QImage image(size.toSize(), QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            painter.setRenderHint(QPainter::Antialiasing);
            object->paint(painter, PaintContext{});
            painter.end();
            return image;
        };
        const QImage small = render(design);
        const QImage large =
            render(design * 2.0)
                .scaled(small.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        int differing = 0;
        for (int y = 0; y < small.height(); ++y)
        {
            for (int x = 0; x < small.width(); ++x)
            {
                const QRgb a = small.pixel(x, y);
                const QRgb b = large.pixel(x, y);
                if (std::abs(qAlpha(a) - qAlpha(b)) > 64 || std::abs(qGreen(a) - qGreen(b)) > 64)
                    ++differing;
            }
        }
        // Edges blur differently; the picture itself must be the same.
        QVERIFY2(differing < small.width() * small.height() / 25,
                 qPrintable(QStringLiteral("%1 pixels differ").arg(differing)));

        // A wide, short rectangle: the picture keeps its proportions, centred.
        QVERIFY(object->setProperty(Prop::Width, design.width() * 4.0));
        QVERIFY(object->setProperty(Prop::Height, design.height() * 2.0));
        const QRectF kept = object->hitShape().boundingRect();
        QCOMPARE(kept.height(), design.height() * 2.0);
        QCOMPARE(kept.width(), design.width() * 2.0);
        QCOMPARE(kept.center(), QPointF(design.width() * 2.0, design.height()));

        // Stretched, it fills the rectangle.
        QVERIFY(object->setProperty(Prop::StretchMode, EnumValue{1}));
        const QRectF stretched = object->hitShape().boundingRect();
        QVERIFY(stretched.width() > design.width() * 3.9);
        QCOMPARE(stretched.height(), design.height() * 2.0);
    }

    void contactSheet()
    {
        const QString path = qEnvironmentVariable("HMI_OBJECT_SHEET");
        if (path.isEmpty())
            QSKIP("Set HMI_OBJECT_SHEET to save the sheet");
        constexpr int kCell = 180;
        constexpr int kColumns = 6;
        std::vector<const ObjectTypeInfo*> types;
        for (const ObjectTypeInfo& info : ObjectRegistry::instance().types())
        {
            if (info.inToolbox)
                types.push_back(&info);
        }
        const int rows = int((types.size() + kColumns - 1) / kColumns);
        QImage sheet(kColumns * kCell, rows * kCell, QImage::Format_ARGB32_Premultiplied);
        sheet.fill(QColor(0xE6, 0xE8, 0xEB));
        QPainter painter(&sheet);
        painter.setRenderHint(QPainter::Antialiasing);
        for (std::size_t i = 0; i < types.size(); ++i)
        {
            const auto object = types[i]->create(ObjectId::create());
            const QSizeF size = object->geometry().size();
            const QPointF cell(double(i % kColumns) * kCell, double(i / kColumns) * kCell);
            painter.save();
            painter.translate(
                cell + QPointF((kCell - size.width()) / 2.0, (kCell - 20 - size.height()) / 2.0));
            object->paint(painter, PaintContext{});
            painter.restore();
            painter.setPen(Qt::black);
            painter.drawText(QRectF(cell.x(), cell.y() + kCell - 20, kCell, 18), Qt::AlignCenter,
                             types[i]->key);
        }
        painter.end();
        QVERIFY(sheet.save(path));
    }

    // Set HMI_RESIZE_SHEET=<file.png> to see each toolbox type at its
    // default size, then resized out of proportion with "Keep proportions",
    // then with "Stretch".
    void resizeSheet()
    {
        const QString path = qEnvironmentVariable("HMI_RESIZE_SHEET");
        if (path.isEmpty())
            QSKIP("Set HMI_RESIZE_SHEET to save the sheet");
        std::vector<const ObjectTypeInfo*> types;
        for (const ObjectTypeInfo& info : ObjectRegistry::instance().types())
        {
            if (info.inToolbox)
                types.push_back(&info);
        }
        constexpr double kRow = 200.0;
        QImage sheet(1100, int(kRow * types.size()), QImage::Format_ARGB32_Premultiplied);
        sheet.fill(QColor(0xE6, 0xE8, 0xEB));
        QPainter painter(&sheet);
        painter.setRenderHint(QPainter::Antialiasing);
        for (std::size_t i = 0; i < types.size(); ++i)
        {
            auto object = types[i]->create(ObjectId::create());
            const QSizeF design = object->geometry().size();
            // Wider and much taller, as when only the height is dragged up.
            const QSizeF resized(std::min(design.width() * 2.1, 380.0),
                                 std::min(design.height() * 3.2, 180.0));
            const QRectF cells[] = {QRectF(10, 10, design.width(), design.height()),
                                    QRectF(260, 10, resized.width(), resized.height()),
                                    QRectF(680, 10, resized.width(), resized.height())};
            for (int c = 0; c < 3; ++c)
            {
                (void)object->setProperty(Prop::Width, cells[c].width());
                (void)object->setProperty(Prop::Height, cells[c].height());
                if (findDescriptor(object->descriptors(), Prop::StretchMode))
                    (void)object->setProperty(Prop::StretchMode, EnumValue{c == 2 ? 1 : 0});
                painter.save();
                painter.translate(cells[c].topLeft() + QPointF(0, kRow * double(i)));
                painter.setPen(QPen(QColor(0x4C, 0x8D, 0xFF), 1.0, Qt::DashLine));
                painter.drawRect(QRectF(QPointF(0, 0), cells[c].size()));
                object->paint(painter, PaintContext{});
                painter.restore();
            }
        }
        painter.end();
        QVERIFY(sheet.save(path));
    }
};

QTEST_MAIN(AllObjectsTest)
#include "AllObjectsTest.moc"
