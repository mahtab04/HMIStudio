#pragma once

#include "render/IGraphicSource.h"
#include "render/ISvgBackend.h"

#include <QByteArray>

#include <map>
#include <memory>
#include <mutex>

namespace hmi
{

// An SVG document. Style overrides (fill, stroke, hidden by element id) are
// applied by rewriting the document, and each styled variant is parsed once
// and kept (ARCHITECTURE.md section 14.4).
class SvgGraphic final : public IGraphicSource
{
public:
    explicit SvgGraphic(QByteArray svg);

    [[nodiscard]] bool isNull() const { return !m_plain; }
    [[nodiscard]] QSizeF naturalSize() const override;
    void render(QPainter& painter, const QRectF& target, const GraphicStyle& style) const override;
    [[nodiscard]] bool usesStyle() const override { return true; }

    // The document with the overrides applied: matching elements get the
    // fill or stroke as attributes (replacing the same keys in a `style`
    // attribute) or display="none".
    [[nodiscard]] static QByteArray applyStyle(const QByteArray& svg, const GraphicStyle& style);

private:
    [[nodiscard]] const ISvgBackend* variant(const GraphicStyle& style) const;

    QByteArray m_svg;
    std::unique_ptr<ISvgBackend> m_plain;
    mutable std::mutex m_mutex;
    mutable std::map<std::size_t, std::unique_ptr<ISvgBackend>> m_styled;
};

} // namespace hmi
