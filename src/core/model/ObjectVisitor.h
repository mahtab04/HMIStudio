#pragma once

namespace hmi
{

class Ellipse;
class GroupObject;
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
    virtual void visit(GroupObject& /*object*/) {}
    virtual void visit(UnknownObject& /*object*/) {}
};

} // namespace hmi
