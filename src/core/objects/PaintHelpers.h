#pragma once

#include "core/property/PropertyValue.h"

#include <QColor>
#include <QPainterPath>
#include <QPen>
#include <QPolygonF>
#include <QRectF>

class QPainter;

namespace hmi
{

class HmiObject;

// Small drawing helpers shared by the object types' paint() and
// hitShape(). They read only the object passed in, so they are safe on the
// render thread.
namespace PaintHelpers
{

// The object's line colour and width as a pen; NoPen for width 0.
[[nodiscard]] QPen linePen(const HmiObject& object);

// Points stored relative to the bounds (0..1) in local coordinates.
[[nodiscard]] QPolygonF toLocal(const PointList& relative, QSizeF size);

// The area within `width` / 2 of `path`, at least one unit wide, so thin
// lines can still be hit.
[[nodiscard]] QPainterPath strokeArea(const QPainterPath& path, double width);

// Text in the object's font and text colour, clipped to `rect`.
void drawText(QPainter& painter, const HmiObject& object, const QRectF& rect, int flags,
              const QString& text);

// A flat face with an outline: fields, lists, panels.
void drawFace(QPainter& painter, const QRectF& rect, const QColor& fill, const QColor& line,
              double radius);

// A raised, glossy face: buttons and the drop-down button of a field.
void drawRaisedFace(QPainter& painter, const QRectF& rect, const QColor& fill, const QColor& line,
                    double radius);

// A linear scale along `axis` (a line in local coordinates, from the
// minimum to the maximum): five numbered divisions with minor ticks.
// Ticks point away from `towards`; labels sit beyond the ticks.
void drawScale(QPainter& painter, const QLineF& axis, QPointF outward, double tickLength,
               double minimum, double maximum, const QColor& color, const QFont& font, bool labels);

// Placeholder frame for objects whose content only exists at runtime
// (controls, touch areas): a dashed outline with a title in the corner.
void drawPlaceholder(QPainter& painter, const QRectF& rect, const QString& title,
                     const QColor& background);

// For controls with a label beside them (switch, check box, radio group):
// scales the painter by the object's height relative to its design height,
// so the control and its text grow together, and returns the size to lay
// out in. Extra width is room for the label.
[[nodiscard]] QSizeF scaleToHeight(QPainter& painter, const HmiObject& object);

// Normalised position of `value` between `minimum` and `maximum`, 0..1.
[[nodiscard]] double fraction(double value, double minimum, double maximum);

} // namespace PaintHelpers

} // namespace hmi
