# hmi_render: turns project assets into pictures (ARCHITECTURE.md section
# 14): raster and SVG sources, the SVG backend strategy, the image cache and
# the import check. Implements hmi_core's IGraphicProvider; knows nothing
# about Qt Quick or the editor.
find_package(Qt6 REQUIRED COMPONENTS Svg)

add_library(hmi_render STATIC
    GraphicCache.cpp
    GraphicCache.h
    GraphicFactory.cpp
    GraphicFactory.h
    GraphicLibrary.cpp
    GraphicLibrary.h
    IGraphicSource.h
    ISvgBackend.h
    QtSvgBackend.cpp
    QtSvgBackend.h
    RasterGraphic.cpp
    RasterGraphic.h
    SvgGraphic.cpp
    SvgGraphic.h
    SvgInspector.cpp
    SvgInspector.h
)

target_include_directories(hmi_render PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(hmi_render PUBLIC hmi::core Qt6::Gui Qt6::Svg)
hmi_set_project_options(hmi_render)
