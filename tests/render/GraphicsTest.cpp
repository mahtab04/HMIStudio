// The graphics pipeline: assets, style overrides, sources, the cache, the
// import check, and SVG rendering compared with reference images.
//
// Set HMI_UPDATE_REFERENCES=1 to write the reference PNGs next to the SVG
// files in tests/data/svg (after checking the new images by eye).

#include "core/graphics/AssetStore.h"
#include "core/graphics/GraphicStyle.h"
#include "core/model/PaintContext.h"
#include "core/objects/GraphicView.h"
#include "core/property/Prop.h"
#include "render/GraphicFactory.h"
#include "render/GraphicLibrary.h"
#include "render/RasterGraphic.h"
#include "render/SvgGraphic.h"
#include "render/SvgInspector.h"

#include <QBuffer>
#include <QFile>
#include <QImage>
#include <QPainter>
#include <QTest>

using namespace hmi;
using namespace Qt::StringLiterals;

namespace
{

QByteArray readData(const QString& name)
{
    QFile file(QFINDTESTDATA(u"../data/svg/"_s + name));
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

QByteArray pngOf(const QImage& image)
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return bytes;
}

QImage renderSource(const IGraphicSource& source, QSize size, const GraphicStyle& style = {})
{
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    source.render(painter, QRectF(QPointF(0, 0), QSizeF(size)), style);
    return image;
}

// Pixels may differ slightly between Qt versions and platforms; count the
// ones that differ by more than `tolerance` in any channel.
double differingShare(const QImage& a, const QImage& b, int tolerance = 24)
{
    if (a.size() != b.size())
        return 1.0;
    int differing = 0;
    for (int y = 0; y < a.height(); ++y)
    {
        for (int x = 0; x < a.width(); ++x)
        {
            const QRgb p = a.pixel(x, y);
            const QRgb q = b.pixel(x, y);
            if (std::abs(qRed(p) - qRed(q)) > tolerance ||
                std::abs(qGreen(p) - qGreen(q)) > tolerance ||
                std::abs(qBlue(p) - qBlue(q)) > tolerance ||
                std::abs(qAlpha(p) - qAlpha(q)) > tolerance)
                ++differing;
        }
    }
    return double(differing) / (a.width() * a.height());
}

bool near(QColor actual, QColor expected)
{
    return std::abs(actual.red() - expected.red()) < 8 &&
           std::abs(actual.green() - expected.green()) < 8 &&
           std::abs(actual.blue() - expected.blue()) < 8 &&
           std::abs(actual.alpha() - expected.alpha()) < 8;
}

} // namespace

class GraphicsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void styleParsesAndPrints()
    {
        auto style = GraphicStyle::parse(u" body: fill #3B6FD4;ring:hidden ; body: stroke red "_s);
        QVERIFY(style);
        QCOMPARE(style->overrides.size(), std::size_t(2));
        const GraphicStyle::Override* body = style->find(u"body");
        QVERIFY(body && body->fill && body->stroke);
        QCOMPARE(*body->fill, QColor(0x3B, 0x6F, 0xD4));
        QVERIFY(style->find(u"ring")->hidden);
        QCOMPARE(*GraphicStyle::parse(style->toString()), *style);
        QVERIFY(style->hash() != 0);
        QCOMPARE(GraphicStyle::parse({})->hash(), std::size_t(0));

        QVERIFY(!GraphicStyle::parse(u"body fill red"_s));
        QVERIFY(!GraphicStyle::parse(u"body: glow red"_s));
        QVERIFY(!GraphicStyle::parse(u"body: fill notacolour"_s));
    }

    void assetsAreStoredByContent()
    {
        AssetStore store;
        const QString a = store.add("same bytes", u"one.png"_s);
        const QString b = store.add("same bytes", u"two.png"_s);
        QCOMPARE(a, b);
        QCOMPARE(a.size(), 32);
        QCOMPARE(store.size(), std::size_t(1));
        QCOMPARE(store.find(a)->fileName, u"one.png"_s); // the first name is kept
        QCOMPARE(store.find(a)->suffix, u"png"_s);
        QVERIFY(!store.find(u"nope"_s));

        AssetStore other;
        other.takeFrom(store);
        QCOMPARE(other.size(), std::size_t(1));
        QCOMPARE(store.size(), std::size_t(0));
        QVERIFY(!store.addFile(u"no/such/file.png"_s));
    }

    void factoryPicksTheSource()
    {
        QImage image(4, 2, QImage::Format_ARGB32);
        image.fill(Qt::red);
        const Asset png{.id = {}, .fileName = u"a.png"_s, .suffix = u"png"_s, .data = pngOf(image)};
        auto raster = GraphicFactory::create(png);
        QVERIFY(raster);
        QCOMPARE(raster->naturalSize(), QSizeF(4, 2));
        QVERIFY(!raster->usesStyle());

        const Asset svg{
            .id = {}, .fileName = u"s"_s, .suffix = {}, .data = readData(u"shapes.svg"_s)};
        auto vector = GraphicFactory::create(svg);
        QVERIFY(vector);
        QVERIFY(vector->usesStyle());
        QCOMPARE(vector->naturalSize(), QSizeF(64, 64));

        const Asset junk{
            .id = {}, .fileName = u"x.png"_s, .suffix = u"png"_s, .data = "not an image"};
        QVERIFY(!GraphicFactory::create(junk));
    }

    void styleOverridesChangeNamedParts()
    {
        const SvgGraphic svg(readData(u"styled.svg"_s));
        QVERIFY(!svg.isNull());
        const QImage plain = renderSource(svg, {64, 64});
        QVERIFY(near(plain.pixelColor(10, 10), Qt::red));
        QVERIFY(near(plain.pixelColor(10, 50), Qt::green));

        // A fill replaces the one in the style attribute; hidden removes.
        const auto style = GraphicStyle::parse(u"body: fill #0000ff; ring: hidden"_s);
        const QImage styled = renderSource(svg, {64, 64}, *style);
        QVERIFY(near(styled.pixelColor(10, 10), Qt::blue));
        QCOMPARE(qAlpha(styled.pixel(10, 50)), 0);

        const QByteArray rewritten = SvgGraphic::applyStyle(readData(u"styled.svg"_s), *style);
        QVERIFY(rewritten.contains("display=\"none\""));
        QVERIFY(!rewritten.contains("fill:#ff0000"));
    }

    void svgMatchesReferences_data()
    {
        QTest::addColumn<QString>("name");
        for (const char* name : {"shapes", "gradient", "transform", "styled"})
            QTest::newRow(name) << QString::fromLatin1(name);
    }
    void svgMatchesReferences()
    {
        QFETCH(QString, name);
        const SvgGraphic svg(readData(name + u".svg"_s));
        QVERIFY(!svg.isNull());
        const QImage image = renderSource(svg, {64, 64});
        const QString referencePath =
            QFINDTESTDATA(u"../data/svg/"_s + name + u".svg"_s).replace(u".svg"_s, u".png"_s);

        if (qEnvironmentVariableIsSet("HMI_UPDATE_REFERENCES"))
        {
            QVERIFY(image.save(referencePath));
            QSKIP("Reference written");
        }
        QImage reference(referencePath);
        QVERIFY2(!reference.isNull(), qPrintable(u"No reference "_s + referencePath));
        reference = reference.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        QVERIFY2(differingShare(image, reference) < 0.01,
                 qPrintable(u"%1 differs from its reference"_s.arg(name)));
    }

    void inspectorFindsUnsupportedContent()
    {
        QCOMPARE(SvgInspector::unsupportedElements(readData(u"filtered.svg"_s)),
                 (QStringList{u"feGaussianBlur"_s, u"filter"_s, u"script"_s}));
        QVERIFY(SvgInspector::unsupportedElements(readData(u"shapes.svg"_s)).isEmpty());

        const QByteArray png = SvgInspector::rasterize(readData(u"filtered.svg"_s));
        const QImage image = QImage::fromData(png, "PNG");
        QCOMPARE(image.size(), QSize(40, 20));
        QVERIFY(SvgInspector::rasterize("not svg").isEmpty());
    }

    void libraryFitsAndCaches()
    {
        QCOMPARE(GraphicLibrary::fitted({0, 0, 100, 50}, {20, 20}, GraphicFit::Fit),
                 QRectF(25, 0, 50, 50));
        QCOMPARE(GraphicLibrary::fitted({0, 0, 100, 50}, {20, 20}, GraphicFit::Fill),
                 QRectF(0, -25, 100, 100));
        QCOMPARE(GraphicLibrary::fitted({0, 0, 100, 50}, {20, 20}, GraphicFit::Centre),
                 QRectF(40, 15, 20, 20));
        QCOMPARE(GraphicLibrary::fitted({0, 0, 100, 50}, {20, 20}, GraphicFit::Stretch),
                 QRectF(0, 0, 100, 50));

        AssetStore store;
        const QString id = store.add(readData(u"shapes.svg"_s), u"shapes.svg"_s);
        const GraphicLibrary library(store);
        QImage image(64, 64, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        QVERIFY(!library.draw(painter, u"unknown"_s, {0, 0, 64, 64}, GraphicFit::Fit, {}));
        QVERIFY(library.draw(painter, id, {0, 0, 64, 64}, GraphicFit::Stretch, {}));
        QVERIFY(library.draw(painter, id, {0, 0, 64, 64}, GraphicFit::Stretch, {}));
        QCOMPARE(library.cache().count(), 1); // the second draw used the cache
        painter.scale(2, 2);                  // zoomed: rendered again, sharp
        QVERIFY(library.draw(painter, id, {0, 0, 64, 64}, GraphicFit::Stretch, {}));
        QCOMPARE(library.cache().count(), 2);
        painter.end();
        QVERIFY(near(image.pixelColor(10, 10), QColor(0xd0, 0x30, 0x30)));
    }

    void graphicViewDrawsItsAsset()
    {
        AssetStore store;
        const QString id = store.add(readData(u"styled.svg"_s), u"styled.svg"_s);
        const GraphicLibrary library(store);

        GraphicView view;
        QVERIFY(view.setProperty(Prop::Width, 64.0));
        QVERIFY(view.setProperty(Prop::Height, 64.0));
        QVERIFY(view.setProperty(Prop::Asset, id));
        QVERIFY(view.setProperty(Prop::StyleOverrides, u"ring: fill #0000ff"_s));
        QImage image(64, 64, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        view.paint(painter, PaintContext{.zoom = 1.0, .editing = true, .graphics = &library});
        painter.end();
        QVERIFY(near(image.pixelColor(10, 10), Qt::red));
        QVERIFY(near(image.pixelColor(10, 50), Qt::blue));

        // Without the graphic, a placeholder is drawn instead.
        QVERIFY(view.setProperty(Prop::Asset, u"missing"_s));
        image.fill(Qt::transparent);
        QPainter again(&image);
        view.paint(again, PaintContext{.zoom = 1.0, .editing = true, .graphics = &library});
        again.end();
        QVERIFY(qAlpha(image.pixel(32, 32)) > 0);
    }
};

QTEST_MAIN(GraphicsTest)
#include "GraphicsTest.moc"
