# hmi_core: document model, property system, object registry and hit
# testing. Links Qt Core and Gui value types only; never Qt Quick
# (ARCHITECTURE.md section 3).
add_library(hmi_core_impl STATIC
    Result.h

    graphics/AssetStore.cpp
    graphics/AssetStore.h
    graphics/GraphicStyle.cpp
    graphics/GraphicStyle.h
    graphics/IGraphicProvider.h

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
    model/HmiStyle.cpp
    model/HmiStyle.h
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
    objects/GraphicView.cpp
    objects/GraphicView.h
    objects/IoField.cpp
    objects/IoField.h
    objects/Button.cpp
    objects/Button.h
    objects/Switch.cpp
    objects/Switch.h
    objects/CheckBox.cpp
    objects/CheckBox.h
    objects/RadioGroup.cpp
    objects/RadioGroup.h
    objects/ListBox.cpp
    objects/ListBox.h
    objects/SymbolicIoField.cpp
    objects/SymbolicIoField.h
    objects/Bar.cpp
    objects/Bar.h
    objects/Slider.cpp
    objects/Slider.h
    objects/Gauge.cpp
    objects/Gauge.h
    objects/Clock.cpp
    objects/Clock.h
    objects/TouchArea.cpp
    objects/TouchArea.h
    objects/Tank.cpp
    objects/Tank.h
    objects/Pump.cpp
    objects/Pump.h
    objects/Valve.cpp
    objects/Valve.h
    objects/Motor.cpp
    objects/Motor.h
    objects/Pipe.cpp
    objects/Pipe.h
    objects/Exchanger.cpp
    objects/Exchanger.h
    objects/Conveyor.cpp
    objects/Conveyor.h
    objects/Sensor.cpp
    objects/Sensor.h
    objects/AlarmView.cpp
    objects/AlarmView.h
    objects/RecipeView.cpp
    objects/RecipeView.h
    objects/DiagnosticsView.cpp
    objects/DiagnosticsView.h
    objects/TrendView.cpp
    objects/TrendView.h
    objects/FxTrendView.cpp
    objects/FxTrendView.h
    objects/ScreenWindow.cpp
    objects/ScreenWindow.h
    objects/Browser.cpp
    objects/Browser.h
    objects/MediaPlayer.cpp
    objects/MediaPlayer.h
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
