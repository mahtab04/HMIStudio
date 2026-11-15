# hmi_core: document model, property system, object registry and hit
# testing. Links Qt Core and Gui value types only; never Qt Quick
# (ARCHITECTURE.md section 3).
add_library(hmi_core_impl STATIC
    Result.h

    io/ObjectJson.cpp
    io/ObjectJson.h
    io/ProjectSerializer.cpp
    io/ProjectSerializer.h

    model/ControlObject.h
    model/DocumentNotifier.cpp
    model/DocumentNotifier.h
    model/ElementObject.h
    model/GroupObject.cpp
    model/GroupObject.h
    model/HmiObject.cpp
    model/HmiObject.h
    model/Ids.h
    model/ObjectVisitor.h
    model/PaintContext.h
    model/Project.cpp
    model/Project.h
    model/Screen.cpp
    model/Screen.h
    model/ScreenBuilder.cpp
    model/ScreenBuilder.h
    model/ShapeObject.h
    model/SymbolObject.h
    model/UnknownObject.cpp
    model/UnknownObject.h

    objects/Ellipse.cpp
    objects/Ellipse.h
    objects/Circle.cpp
    objects/Circle.h
    objects/Line.cpp
    objects/Line.h
    objects/PaintHelpers.cpp
    objects/PaintHelpers.h
    objects/Polygon.cpp
    objects/Polygon.h
    objects/Polyline.cpp
    objects/Polyline.h
    objects/Rectangle.cpp
    objects/Rectangle.h
    objects/TextBox.cpp
    objects/TextBox.h

    property/CommonDescriptors.cpp
    property/CommonDescriptors.h
    property/DescriptorBuilders.cpp
    property/DescriptorBuilders.h
    property/Prop.h
    property/PropertyDescriptor.cpp
    property/PropertyDescriptor.h
    property/PropertyValue.cpp
    property/PropertyValue.h

    registry/ObjectRegistry.cpp
    registry/ObjectRegistry.h

    spatial/HitTester.cpp
    spatial/HitTester.h
    spatial/Snapper.cpp
    spatial/Snapper.h
)

target_include_directories(hmi_core_impl PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(hmi_core_impl PUBLIC Qt6::Core Qt6::Gui tl::expected)
set_target_properties(hmi_core_impl PROPERTIES AUTOMOC ON)
hmi_set_project_options(hmi_core_impl)

# Object types register themselves through static initializers, which a
# linker drops from a static archive when nothing references them. Consumers
# therefore link the archive whole through this interface target.
add_library(hmi_core INTERFACE)
add_library(hmi::core ALIAS hmi_core)
target_link_libraries(hmi_core INTERFACE "$<LINK_LIBRARY:WHOLE_ARCHIVE,hmi_core_impl>")
