# HMIStudio

Desktop and touch editor for designing HMI screens (toolbox, canvas,
properties, tag binding, simulation), written in C++20 with Qt 6 Qt Quick.

- Design: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)
- Roadmap: [docs/PLAN.md](docs/PLAN.md)
- Target UI: [docs/HMI Editor@2x.png](docs/HMI%20Editor@2x.png)

## Status

- Phase 1, core model: `hmi_core` with strong ids, the property system,
  Rectangle, Ellipse, Line and Text box, the object registry, `Screen`,
  `Project`, `DocumentNotifier`, `ScreenBuilder` and `HitTester`.
- Phase 2, canvas and input (with the phase 0 shell): the Qt Quick editor
  window, a canvas with zoom, pan, grid, rulers and selection handles, one
  gesture path for mouse, touch and pen, and Select, Create and Pan tools. The
  toolbox supports tap-to-place and drag and drop. Every change is undoable.
- Phase 3, editing: cut, copy, paste, duplicate, delete, group, align,
  distribute, same size, z-order, nudging, rotation handle, snapping with
  guides, a context menu and a touch selection toolbar. Checked with 2,000
  objects on one screen.
- Phase 4, properties and files: a properties panel with an editor per
  value type (touch keypad for numbers), a layers panel, screen tabs, and
  projects saved as folders of JSON files (new, open, recent, save, save
  as, autosave and recovery). Settings remember the layout.
- Phase 5, objects: the full toolbox of 36 types in four sections with
  search (shapes, elements such as I/O fields, buttons, bars and gauges,
  controls, and process symbols such as tanks, pumps, valves and pipes),
  imported images and SVG with colour overrides by element id, a path tool
  for polylines, polygons and right-angled pipes, and a shared project
  style.

Run `HmiStudio` from the build folder. Mouse: click and Shift+click to
select, drag to move or rubber-band select, drag handles to resize, wheel to
pan and Ctrl+wheel to zoom. Touch: tap, long press to add to the selection,
drag to move, pinch to zoom, two fingers to pan; switch the layout to touch
density with the Input button.

## Requirements

- Qt 6.8 or newer (6.12 LTS recommended) with Qt Quick, Quick Controls, SVG and Test
- CMake 3.25+, Ninja
- MSVC 2022, GCC 13+ or Clang 16+

## Build and test

Set `QT_ROOT_DIR` to the Qt kit (for example `C:/Qt/6.12.0/msvc2022_64` or
`~/Qt/6.12.0/gcc_64`), then:

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

Tests run headless with the `offscreen` platform plugin. On Linux the `dev`
preset builds with AddressSanitizer and UBSan.

For a MinGW kit, add a `CMakeUserPresets.json` (ignored by Git) that inherits
`dev` and points `CMAKE_CXX_COMPILER` and `PATH` at the MinGW toolchain that
matches the Qt kit.
