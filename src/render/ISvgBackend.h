#pragma once

#include <QByteArray>
#include <QRectF>
#include <QSizeF>

#include <memory>

class QPainter;

namespace hmi
{

// Which library draws SVG (Strategy, ARCHITECTURE.md section 14.2). Qt SVG
// is the default; a second backend for files Qt SVG cannot draw may be
// added behind a CMake option.
class ISvgBackend
{
public:
    ISvgBackend() = default;
    virtual ~ISvgBackend() = default;
    ISvgBackend(const ISvgBackend&) = delete;
    ISvgBackend& operator=(const ISvgBackend&) = delete;
    ISvgBackend(ISvgBackend&&) = delete;
    ISvgBackend& operator=(ISvgBackend&&) = delete;

    // Parses the document; false if it is not usable SVG.
    virtual bool load(const QByteArray& svg) = 0;
    // Size from the document's width, height or view box.
    [[nodiscard]] virtual QSizeF naturalSize() const = 0;
    virtual void render(QPainter& painter, const QRectF& target) const = 0;
};

// Creates the backend in use.
[[nodiscard]] std::unique_ptr<ISvgBackend> createSvgBackend();

} // namespace hmi
