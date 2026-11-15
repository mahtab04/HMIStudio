#pragma once

namespace hmi
{

class Ellipse;
class GroupObject;
class Sensor;
class Conveyor;
class Exchanger;
class Pipe;
class Motor;
class Valve;
class Pump;
class Tank;
class TouchArea;
class Clock;
class Gauge;
class Slider;
class Bar;
class SymbolicIoField;
class ListBox;
class RadioGroup;
class CheckBox;
class Switch;
class Button;
class IoField;
class GraphicView;
class Circle;
class Polygon;
class Polyline;
class Line;
class Rectangle;
class TextBox;
class UnknownObject;

// Visitor over the concrete object types, for validation, export and "where
// is this tag used". Every visit defaults to doing nothing, so a visitor
// overrides only the types it cares about. Adding a type adds one overload
// here and one accept() in the type.
class ObjectVisitor
{
public:
    ObjectVisitor() = default;
    virtual ~ObjectVisitor() = default;
    ObjectVisitor(const ObjectVisitor&) = default;
    ObjectVisitor& operator=(const ObjectVisitor&) = default;
    ObjectVisitor(ObjectVisitor&&) = default;
    ObjectVisitor& operator=(ObjectVisitor&&) = default;

    virtual void visit(Rectangle& /*object*/) {}
    virtual void visit(Ellipse& /*object*/) {}
    virtual void visit(Line& /*object*/) {}
    virtual void visit(TextBox& /*object*/) {}
    // Does not descend into the children; a visitor that needs them walks
    // GroupObject::children() itself.
    virtual void visit(Polyline& /*object*/) {}
    virtual void visit(Polygon& /*object*/) {}
    virtual void visit(Circle& /*object*/) {}
    virtual void visit(GraphicView& /*object*/) {}
    virtual void visit(IoField& /*object*/) {}
    virtual void visit(Button& /*object*/) {}
    virtual void visit(Switch& /*object*/) {}
    virtual void visit(CheckBox& /*object*/) {}
    virtual void visit(RadioGroup& /*object*/) {}
    virtual void visit(ListBox& /*object*/) {}
    virtual void visit(SymbolicIoField& /*object*/) {}
    virtual void visit(Bar& /*object*/) {}
    virtual void visit(Slider& /*object*/) {}
    virtual void visit(Gauge& /*object*/) {}
    virtual void visit(Clock& /*object*/) {}
    virtual void visit(TouchArea& /*object*/) {}
    virtual void visit(Tank& /*object*/) {}
    virtual void visit(Pump& /*object*/) {}
    virtual void visit(Valve& /*object*/) {}
    virtual void visit(Motor& /*object*/) {}
    virtual void visit(Pipe& /*object*/) {}
    virtual void visit(Exchanger& /*object*/) {}
    virtual void visit(Conveyor& /*object*/) {}
    virtual void visit(Sensor& /*object*/) {}
    virtual void visit(GroupObject& /*object*/) {}
    virtual void visit(UnknownObject& /*object*/) {}
};

} // namespace hmi
