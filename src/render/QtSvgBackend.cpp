#include "render/QtSvgBackend.h"

#include <QPainter>

namespace hmi
{

std::unique_ptr<ISvgBackend> createSvgBackend()
{
    return std::make_unique<QtSvgBackend>();
}

bool QtSvgBackend::load(const QByteArray& svg)
{
    const std::scoped_lock lock(m_mutex);
    return m_renderer.load(svg) && m_renderer.isValid();
}

QSizeF QtSvgBackend::naturalSize() const
{
    const std::scoped_lock lock(m_mutex);
    const QSizeF size = m_renderer.defaultSize();
    return size.isEmpty() ? m_renderer.viewBoxF().size() : size;
}

void QtSvgBackend::render(QPainter& painter, const QRectF& target) const
{
    const std::scoped_lock lock(m_mutex);
    m_renderer.render(&painter, target);
}

} // namespace hmi
