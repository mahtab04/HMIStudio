#pragma once

#include "core/model/HmiObject.h"

#include <QTransform>

namespace hmi
{

// Family base for process symbols (tank, pump, valve, motor, pipe, heat
// exchanger, conveyor, sensor), drawn with C++ painter code so their state
// (level, running, open) can animate smoothly.
//
// Symbols with a "stretch mode" property are pictures, as in the symbol
// libraries of WinCC and other HMI editors: they are drawn at their design
// size (the type's default size) and scaled to the object's rectangle,
// either keeping their proportions (centred) or stretched to fill it. So a
// pump stays a pump at every size, and its text scales with it. Types
// without the property (tank, pipe) lay themselves out in the rectangle.
class SymbolObject : public HmiObject
{
public:
    enum StretchMode : int
    {
        KeepProportions = 0,
        Stretch = 1,
    };

    // Whether the type is drawn at its design size and scaled.
    [[nodiscard]] bool isScaledPicture() const;

    // The coordinates paint() and hitShape() work in: the design size for
    // scaled pictures, else the object's own size.
    [[nodiscard]] QSizeF designSize() const;

    // Maps design coordinates to local coordinates (identity for types that
    // are not scaled pictures).
    [[nodiscard]] QTransform designTransform() const;

    // The design rectangle, mapped; types refine it with their own shape.
    [[nodiscard]] QPainterPath hitShape() const override;

protected:
    using HmiObject::HmiObject;
};

} // namespace hmi
