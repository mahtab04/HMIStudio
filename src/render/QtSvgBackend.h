#pragma once

#include "render/ISvgBackend.h"

#include <QSvgRenderer>

#include <mutex>

namespace hmi
{

// SVG through Qt SVG (QSvgRenderer), the SVG Tiny 1.2 static profile plus
// what recent Qt versions add (ARCHITECTURE.md section 14.1).
class QtSvgBackend final : public ISvgBackend
{
public:
    bool load(const QByteArray& svg) override;
    [[nodiscard]] QSizeF naturalSize() const override;
    void render(QPainter& painter, const QRectF& target) const override;

private:
    // QSvgRenderer is not re-entrant; one render at a time.
    mutable std::mutex m_mutex;
    mutable QSvgRenderer m_renderer;
};

} // namespace hmi
