# HMI Editor — Implementation Plan

Companion to ARCHITECTURE.md (Qt Quick, C++-first). Each phase ends with
something you can run and show. Durations are rough estimates for one
experienced C++ developer working full-time; they are not measured and will
move.

## Overview

| Phase | Result | Estimate |
|---|---|---|
| 0 | Repository, build, CI, themed Qt Quick shell | 1.5 weeks |
| 1 | Core model and property system with tests | 2 weeks |
| 2 | Canvas items, gestures, select / move / resize with mouse and touch | 5 weeks |
| 3 | Undo/redo, copy/paste, group, align, snapping, touch toolbar | 4 weeks |
| 4 | Property panel, layers, save and load | 4 weeks |
| 5 | Full toolbox: elements, process symbols, graphics | 4 weeks |
| 6 | Tags, bindings, simulator, Simulate mode | 4 weeks |
| 7 | Events, screen navigation, alarms and trend controls | 4 weeks |
| 8 | Real drivers: OPC UA, then S7 and Modbus | 3 weeks |
| 9 | Runtime player and deployment package | 2 weeks |
| 10 | Polish: library, templates, validation, installer | ongoing |

Phases 0–4 give a working drawing editor (about 16 weeks). Phases 5–7 make
it an HMI editor (about 12 more). Compared with a Widgets design, phases 2–4
are longer because selection, hit testing and docking are built here, and
phase 9 is shorter because the runtime reuses the editor's Qt Quick items.

## Phase 0 — Foundation

- Repository with the directory layout from ARCHITECTURE.md.
- CMake presets for Windows (MSVC) and Linux (GCC), C++20, warnings as errors.
- `qt_add_qml_module` for `hmi_quick` and the editor's QML; `QML_ELEMENT`
  registration working for one dummy item.
- Dependency fetching for tl::expected and the test framework.
- clang-format, clang-tidy, `qmllint`, `.editorconfig`.
- CI: build and run tests on both platforms (offscreen platform plugin).
- `main.cpp` with `QGuiApplication`, `QQmlApplicationEngine`, `AppContext`.
- `Main.qml`: top bar, `SplitView` with empty left, centre, right and bottom
  panels, status bar.
- `Theme` (C++ `QObject` exposed to QML): colours, spacing, fonts, and the
  compact / comfortable density switch.

Done when: a clean checkout builds with one command on both platforms and
opens the themed shell; switching density resizes the controls.

## Phase 1 — Core model

- Ids (`ObjectId`, `PropertyId`, `ObjectTypeId`) as strong types with `<=>`.
- `PropertyValue`, `PropertyDescriptor`, constraints and validation.
- `HmiObject` base class with `paint()` and `hitShape()`; `Rectangle`,
  `Ellipse`, `Line`, `TextBox`.
- `ObjectRegistry` with self-registration and toolbox metadata.
- `Screen`, `Project`, `DocumentNotifier`.
- `clone()` for all types; `ScreenBuilder` for test fixtures.
- `HitTester` with tolerance parameter.

Done when: unit tests cover create, clone, property validation, notification
signals and hit testing; `hmi_core` links without Qt Quick.

Status: done. Build scaffolding from phase 0 (presets, warnings as errors,
dependency fetching, clang-format, clang-tidy) came with it; the Qt Quick
shell, `AppContext` and `Theme` from phase 0 are still open. CI is deferred
until the application is further along: aqtinstall cannot yet install Qt 6.12
on Windows, and the Conan route (local branch `wip/conan-ci`) is unfinished.

## Phase 2 — Canvas and input

Canvas items (C++):

- `CanvasItem` with zoom and pan state, screen-to-scene mapping.
- `ScreenItem` creating and removing `ObjectItem`s from notifier signals.
- `ObjectItem : QQuickPaintedItem` calling `HmiObject::paint()`; geometry,
  rotation and `z` from the model.
- `GridItem` and `RulerItem`.
- `SelectionOverlay` with eight handles drawn at constant screen size.
- Texture re-render when zoom settles; texture size cap.

Input:

- `GestureRecognizer`: tap, double tap, long-press, drag, pinch, two-finger
  pan, wheel, from mouse, touch and pen; emits `PointerGesture`.
- `ITool`, `ToolController`.
- `SelectTool`: select, add to selection, rubber band, move, resize with
  live preview.
- `CreateTool`: drag from toolbox, or pick tool then drag or tap on canvas.
- `PanTool` and the always-available pan and zoom gestures.

Shell:

- `ToolboxModel` and the toolbox `GridView`; drag from toolbox to canvas with
  mouse and with touch.
- `EditorController` with zoom, current tool and cursor position properties.

Done when: with a mouse and, separately, with only a touch screen, you can
place the four basic shapes, select, move and resize them, pan with two
fingers and pinch-zoom from 10% to 800% with sharp results after the zoom
settles. Tool tests pass with both synthetic mouse and touch gestures.

Status: implemented on `feat/phase-2-canvas`, together with the phase 0 shell
(`main.cpp`, `AppContext`, `Theme` with the density switch, `Main.qml`). The
add, move and resize commands from phase 3 came early, because tools may only
change the model through commands. Gesture, tool and canvas tests pass
offscreen with synthetic mouse and touch input; trying it on a real touch
screen is still open.

## Phase 3 — Editing operations

- `QUndoStack` in `AppContext`; `AddObjectCommand`, `RemoveObjectsCommand`,
  `MoveObjectsCommand`, `ResizeObjectCommand`, `SetPropertyCommand`,
  `ReorderCommand`.
- Rule from here on: the model changes only inside commands.
- Cut, copy, paste, duplicate through `clone()` and a JSON clipboard format.
- `GroupObject`, group and ungroup.
- Align, distribute, same size, bring forward and send backward.
- Snapping strategies: grid, object edges and centres; `GuideOverlay`.
- Rotation handle.
- Keyboard: arrows nudge, delete, Ctrl+A, standard shortcuts.
- Touch: floating selection toolbar (delete, duplicate, group, order, nudge
  buttons), long-press context menu, multi-select mode toggle.
- Performance and memory check with 2,000 objects on one screen; apply the
  fallbacks from ARCHITECTURE.md section 14.5 if needed.

Done when: every operation undoes and redoes to an identical model (tested),
all operations are reachable without a keyboard, and the 2,000-object screen
pans and zooms smoothly within the memory budget you set.

## Phase 4 — Properties, layers, persistence

- `PropertyModel` built from the selection's descriptors, with mixed-value
  state for multi-selection; `setData` pushes `SetPropertyCommand`; command
  merging during continuous edits.
- QML delegates per value type: number (with keypad in touch density), text,
  colour (`ColorDialog`), font, enum, bool, point, size.
- `LayerModel` and `TreeView`: rename, reorder by drag, lock, hide.
- `ProjectSerializer`: save and load the project folder, `formatVersion`,
  first migration test, unknown data preserved.
- New, open, save, save as (`FileDialog` / `FolderDialog`), recent projects,
  unsaved-changes prompt.
- Autosave snapshot and recovery on next start.
- Screen management: add, rename, duplicate, delete, tabs for open screens.
- Panel collapse and sizes saved in settings.

Done when: a project with several screens survives save, close, reopen with
a byte-identical second save, and every property can be edited by touch.

## Phase 5 — Full object set

Each object is a descriptor table, a `paint()`, a `hitShape()` if not
rectangular, and a registration, per the recipe in SKILL.md.

| Batch | Objects |
|---|---|
| 5a Shapes | Polyline, Polygon, Circle, Graphic view |
| 5b Elements | I/O field, Button, Switch, Check box, Radio group, List box, Symbolic I/O field, Bar, Slider, Gauge, Clock, Touch area |
| 5c Process symbols | Tank, Pump, Valve, Motor, Pipe (with `PipeTool` for routed segments), Exchanger, Conveyor, Sensor |

Graphics pipeline, built with the Graphic view object in 5a (ARCHITECTURE.md
section 14):

- `AssetManager`: import into `assets/` by content hash, remove unused assets.
- `IGraphicSource` with `RasterGraphic` and `SvgGraphic` on `QtSvgBackend`;
  add `Qt6::Svg` to the build.
- `GraphicCache` (`QImage`) keyed by asset, pixel size, device pixel ratio
  and style.
- `SvgInspector` import check with the warn / rasterize choice.
- `GraphicStyle` overrides by SVG element id, made bindable in phase 6.
- Test set of SVG files compared against reference PNGs.
- Optional second SVG backend behind a CMake option.

Controls (alarm view, trend, f(x) trend, screen window, browser, media
player, recipe view, diagnostics) are added here as static placeholders with
their properties; live behaviour comes in phase 7.

- HMI style definitions (colours, fonts) shared across objects.

Done when: all 36 toolbox entries can be placed, edited, saved and reloaded.

## Phase 6 — Tags and simulation

- `Tag`, `TagDatabase`, `TagModel` with filter proxy, tags `TableView` with
  add, edit, CSV import and export.
- `Binding` and transforms: direct, linear scale, range map, expression.
- Binding UI in the property panel: tag picker with search, transform editor.
- "Where used" for tags through a visitor; warn before deleting a used tag.
- `IDriver`, `TagEngine` with worker thread and coalesced updates.
- `SimulatorDriver`: ramp, sine, random, toggle and manual values per tag.
- Play mode on `CanvasItem`: `BindingEvaluator` drives properties from tag
  values; live values column in the tags panel.

Done when: the filling-station demo screen animates its tank level, pump
state and values from simulated tags.

## Phase 7 — Behaviour

- Events and actions: SetTag, ToggleTag, IncrementTag, OpenScreen, ShowPopup,
  AcknowledgeAlarm; action list editor.
- `RuntimeInputHandler`: taps and drags operate buttons, switches, sliders
  and I/O fields in play mode, with touch and mouse.
- Screen navigation and popup windows; `ScreenWindow` control.
- Alarm definitions (limit and discrete), alarm engine, live alarm view and
  banner.
- Trend view with history buffer; f(x) trend.
- Validation pass: unbound required properties, missing tags, touch targets
  smaller than 44 px or overlapping, objects outside the screen; results in
  the Output panel.
- Optional: scripting through `QJSEngine` behind a `RunScript` action.

Done when: a three-screen demo project can be operated end to end in
Simulate mode by touch, including an alarm being raised and acknowledged.

## Phase 8 — Real connections

- Connection editor (type, address, rack/slot or endpoint, cycle time).
- OPC UA driver (open62541): connect, browse, subscribe, write; tag import
  by browsing the server.
- S7 driver (Snap7): DB, M, I, Q addresses, polling optimized by block.
- Modbus TCP driver.
- Connection status in the status bar; quality flag per tag shown on objects.
- Reconnect with back-off; all driver errors reported through `Result`.

Done when: the demo runs against a real or simulated PLC (PLCSIM Advanced or
an OPC UA demo server) for 24 hours without leaks or stalls.

## Phase 9 — Runtime and deployment

- `hmi_runtime` executable: minimal QML window hosting `ScreenItem` in play
  mode, full screen, no editor code linked.
- Start-up screen, screen cache, on-screen keyboard for input fields.
- Deploy action: package project plus runtime, copy to a target over SSH or
  to a folder.
- Test on the target panel hardware: frame rate, memory, touch response.
- User login and access levels on objects, if required.

Done when: a project built in the editor runs on a separate touch device
without the editor installed.

## Phase 10 — Polish (pick by need)

- User library of reusable grouped objects; faceplates with interface
  properties.
- Screen templates and master layers (header, navigation).
- Multi-language texts.
- Find and replace for tags and texts.
- Export screen as PNG or PDF.
- Installer (Qt Installer Framework or MSI) and Linux AppImage.
- User documentation.

## Working order inside each phase

1. Model and tests in `hmi_core` first.
2. Commands that change the model.
3. C++ items, tools, models and controllers that use the commands.
4. The QML view last, as layout only.
5. Serialization, then a round-trip test.
6. Try the feature with touch only before calling it done.
7. Update SKILL.md if a new convention appeared.

## First week checklist

- [ ] Install Qt 6.12 LTS (or 6.8+) with Qt Quick, Quick Controls, Quick
      Dialogs and SVG; CMake 3.25+; a C++20 compiler.
- [ ] Create the repository and commit the three documents under `docs/`.
- [ ] Top-level CMake with the targets as empty libraries.
- [ ] One `QQuickPaintedItem` subclass registered with `QML_ELEMENT`,
      painting a rectangle, shown from `Main.qml`.
- [ ] CI workflow building both platforms.
- [ ] One trivial test passing in CI.
- [ ] A touch screen or tablet available for testing from phase 2 onward.
