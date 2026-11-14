---
name: hmi-studio-dev
description: Conventions and step-by-step recipes for working on the HMI Studio editor (C++20, Qt 6 Qt Quick with C++ QQuickItem / QQuickPaintedItem classes and thin QML). Use when adding or changing HMI objects, canvas items, gestures and tools, undo commands, panel models, QML views, tag drivers, or the project file format in this repository.
---

# HMI Studio development

Read `docs/ARCHITECTURE.md` for the design and `docs/PLAN.md` for the current
phase before starting a task. This file holds the rules and recipes that keep
new code consistent.

## Build and test

```bash
cmake --preset dev            # configure (Debug, sanitizers on Linux); needs QT_ROOT_DIR
cmake --build --preset dev
cmake --build --preset dev --target all_qmllint
ctest --preset dev            # offscreen platform and output-on-failure are set
```

`dev` expects the environment variable `QT_ROOT_DIR` to point at the Qt
kit. For a MinGW kit, a local `CMakeUserPresets.json` (ignored by Git)
inherits `dev` and pins the matching compiler and `PATH`; use its preset
name instead of `dev`.

On Windows a Qt test's console output may not reach the terminal. To see
why a test failed, run it with a file logger:
`build/dev/<Test>.exe -o result.txt,txt` (set `QT_QPA_PLATFORM=offscreen`).
Set `HMI_SCREENSHOT=<file.png>` when running `EditorShellTest` to save an
image of the shell.

A task is finished only when the build has no warnings, `qmllint` is clean
and all tests pass.

## Hard rules

1. **The model changes only inside a `QUndoCommand`.** Tools, models and
   controllers never call `setProperty`, add or remove objects directly; they
   push a command on the undo stack (`AppContext::undoStack()`, reached from
   tools through `ToolContext::undoStack()`). Selection, zoom and previews
   are editor state, not model, and change outside commands.
2. **Logic lives in C++. QML is layout.** No JavaScript functions, no
   imperative blocks beyond a single call to a C++ invokable, no state stored
   in QML. If a QML file needs an `if` or a loop, add a property or method to
   a C++ controller or model instead.
3. **`hmi_core` never includes Qt Quick** or anything from `quick/` or
   `editor/`, and `quick/` never includes `editor/`. Dependencies point
   downward only: editor → quick → core (render and comm sit between them
   from phases 5 and 6).
4. **`ObjectItem` holds no model state.** Geometry, colours and text come
   from the `HmiObject` every time; it keeps only view state (render scale,
   a tool's preview geometry until the next sync).
5. **Tools receive `PointerGesture`, never raw mouse or touch events.** Every
   interaction must work with mouse and with touch.
6. **Never switch on a concrete object type** outside that type's own files.
   Use property descriptors, the registry or a visitor.
7. **Respect the render thread.** `paint()` and `updatePaintNode()` only
   read; they never change the model, emit signals or touch other items.
   Items are created and changed on the GUI thread only.
8. **No new singletons**, in C++ or QML. Only registries filled at start-up
   may be function-local statics. Everything else is reached through
   `AppContext` and passed to QML as a typed required property of the root
   window (`setInitialProperties`), then down to panels as required
   properties. No context properties.
9. **No blocking I/O on the GUI thread.** Drivers run on the `TagEngine`
   worker thread.
10. **Every saved file has `formatVersion`.** A format change needs a
    migration function and a test with an old file.
11. **Recoverable errors return `Result<T>`** (`tl::expected`). Exceptions are
    for programming errors only.

## Code conventions

| Topic | Convention |
|---|---|
| Standard | C++20; no compiler extensions |
| Naming | `PascalCase` types, `camelCase` functions and variables, `m_` prefix for members, `k` prefix for constants |
| Files | One class per `.h`/`.cpp` pair, same name as the class; QML files `PascalCase.qml` |
| Ownership | `std::unique_ptr` for model objects; Qt parent ownership for `QObject`s and `QQuickItem`s; raw pointer or reference means non-owning |
| Headers | `#pragma once`; forward-declare where possible |
| Qt types | Qt containers and strings at Qt API boundaries, `std` containers inside the model |
| Const | `const` and `[[nodiscard]]` on every getter; `noexcept` where true |
| QML exposure | `QML_ELEMENT` for items, `QML_UNCREATABLE` for controllers and models; `Q_PROPERTY` with `NOTIFY`; `Q_INVOKABLE` for actions |
| QML typing | Typed properties, `required property` in delegates, `pragma ComponentBehavior: Bound` |
| Signals | New-style `connect` with member pointers or lambdas with a context object |
| Strings | User-visible text in `tr()` / `qsTr()`; identifiers in files are never translated |
| Comments | Explain why, not what; public classes get a one-paragraph purpose comment |
| Formatting | `.clang-format` (Allman braces, 100 columns); run it on every changed file |
| Linking `hmi_core` | Link `hmi::core`, never `hmi_core_impl`: the interface target links the archive whole so self-registering types are not dropped |
| Headers in QML modules | The generated QML type registration includes headers by file name only. A `QML_ELEMENT` class in a subfolder of a module needs that folder in the target's private include directories |
| Translation outside QObjects | Classes that are not `QObject`s (commands, model code) use `QCoreApplication::translate("hmi::<Context>", ...)`; `tr()` does not exist there |
| Types in signals | Ids and other value types used in signals have `Q_DECLARE_METATYPE` next to the signal's class |

## Recipe: add a new HMI object

Example: a `Gauge`.

1. Create `src/core/objects/Gauge.h` and `Gauge.cpp`, deriving from the
   right family base (`ElementObject` here). Only `ShapeObject` exists so
   far; the first member of a family adds its base in `src/core/model/`.
2. Declare `static constexpr ObjectTypeId kTypeId{"gauge"};` and the
   property table in `staticDescriptors()`, built with
   `withCommonDescriptors()` so the common properties come first. Reuse ids
   from `Prop.h`; add new ids there, never locally:

   ```cpp
   std::span<const PropertyDescriptor> Gauge::staticDescriptors()
   {
       static const std::vector<PropertyDescriptor> table = withCommonDescriptors(
           {120.0, 120.0},
           {
               {.id = Prop::Value, .key = "value",
                .displayName = QT_TRANSLATE_NOOP("hmi::Property", "Value"),
                .category = PropertyCategory::Behaviour, .defaultValue = 0.0,
                .bindable = true},
               {.id = Prop::Minimum, /* ... */},
           });
       return table;
   }
   ```

   `key` must hash to `id` (checked in debug builds and by the tests). Mark
   as `bindable` every property a tag should be able to drive.
3. Pass the table to the base: `Gauge(ObjectId id = ObjectId::create()) :
   ElementObject(staticDescriptors(), id)`. Implement `typeId()`, `clone()`
   (via a private copy constructor), `accept()` (and add a `visit` overload
   to `ObjectVisitor`) and `paint(QPainter&, const PaintContext&)`. Paint in
   local coordinates from `(0,0)` to `geometry().size()`. `paint()` runs on
   the render thread: read only the object's own data, use no shared
   mutable state. Skip fine detail when `context.lowDetail()`.
4. Override `hitShape()` if the object is not rectangular (line, polygon,
   pipe), so tapping empty corners does not select it.
5. Register the type at the bottom of the `.cpp`:

   ```cpp
   HMI_REGISTER_OBJECT(hmi::Gauge, "gauge",
                       QT_TRANSLATE_NOOP("hmi::ObjectRegistry", "Gauge"),
                       hmi::ObjectCategory::Elements, ":/icons/gauge.svg")
   ```

   Add the `.cpp`/`.h` to `src/core/CMakeLists.txt`. A `static_assert` in
   the macro checks that the key matches `kTypeId`.
6. Add the toolbox icon to `resources/icons/` as a single-colour 24 × 24 SVG
   drawn in black (the toolbox recolours it from `Theme`), and list it in the
   `hmi_icons` resource in `src/editor/CMakeLists.txt`.
7. Tests in `tests/core/objects/GaugeTest.cpp`, registered with
   `hmi_add_test` in `tests/CMakeLists.txt`. The checks in
   `tests/core/objects/ObjectContract.h` (`checkDefaults`, `checkClone`,
   `checkRejections`, `checkRegistered`) cover defaults, clone equality,
   constraint rejection and registration; add hit shape, painting and, from
   phase 4, a save and load round trip.
8. Do not write a new `QQuickItem` or QML for the object, and do not touch
   the toolbox, property panel or serializer. `ObjectItem` hosts every type.
   If that is not enough, the descriptor system is missing something; extend
   it instead.

## Recipe: add a C++ Qt Quick item

For canvas parts such as overlays, rulers or a new viewport feature.

1. Create the class in `src/quick/`, deriving from `QQuickPaintedItem` when
   `QPainter` drawing is simplest, or `QQuickItem` with `updatePaintNode()`
   for lines and rectangles that change every frame (grid, guides, handles).
   Build scene-graph content from rectangle and transform nodes
   (`SceneGraphShapes`), never custom geometry or materials: the software
   backend ignores those. After adding or removing nodes, call
   `SceneGraphShapes::refreshTransform(data->transformNode)` before
   returning.
2. List the files in the module's `qt_add_qml_module`. Add `QML_ELEMENT`
   only if QML creates the item (like `RulerItem`).
3. For a plain `QQuickItem` that draws, call `setFlag(ItemHasContents)`.
4. Expose configuration as `Q_PROPERTY`s; call `update()` in setters when the
   value changes, and only then.
5. In `updatePaintNode()`, use only member data copied on the GUI thread;
   reuse the old root node instead of recreating it.
6. Do not accept input in the item. Input goes through `CanvasItem` and the
   gesture path.
7. Test offscreen: create a `QQuickWindow`, add the item, grab the window and
   check pixels, or assert on item state. Offscreen tests run on the software
   renderer, so they also prove the item works without a GPU. Put the item
   away from the window origin in at least one test.

## Recipe: add a gesture

1. Add the kind to `PointerGesture::Kind`.
2. Extend the `GestureRecognizer` state machine; give mouse and touch a way
   to produce it, or document why one device cannot.
3. Thresholds (drag distance, long-press time, hit tolerance) come from
   `InputSettings`, with larger values for touch.
4. Tests in `tests/quick/GestureRecognizerTest.cpp` send `QTest` mouse and
   touch sequences (`QTest::touchEvent` with `QTest::createTouchDevice()`) to
   a `CanvasItem` in a `QQuickWindow` and check the emitted gestures.

## Recipe: add a canvas tool

1. Derive from `ITool` in `src/editor/tools/`; take a `ToolContext&` in the
   constructor (project, current screen, undo stack, selection, input
   settings, canvas).
2. Implement `handleGesture(const PointerGesture&)` and `cursor()`, and
   `activate`, `deactivate` (must drop any preview) and `keyPress` as needed.
   Hit tolerances in scene units come from `ToolContext::sceneHitTolerance()`
   and `sceneHandleHitSize()`.
3. Preview through `ICanvasView` (`setPreviewGeometry`, `setRubberBand`);
   change the model only when the gesture ends, with a single command (or a
   macro). Navigation gestures never reach tools; `ToolController` handles
   them.
4. Cancel (`DragCancel`, from Escape or a second finger landing during a
   drag) must leave the model untouched.
5. Add a value to `ToolController::Tool` and create the tool there; add a
   `<name>ToolActive` property and an invokable to `EditorController`, and a
   toolbar button in `TopBar.qml` bound to them.
6. Test with `tests/editor/FakeCanvas.h` and `GestureFactory.h`: run each
   case with `test::addDeviceRows()` so it runs as mouse and as touch.

## Recipe: add an undo command

1. Create `src/editor/commands/<Verb><Noun>Command.h/.cpp` deriving from
   `QUndoCommand`.
2. Store `Project&`, the `ScreenId` and object ids, never pointers to model
   objects, plus the old and new values. A command that removes an object
   owns it while undone.
3. `redo()` applies, `undo()` restores. The first `redo()` is called by
   `push()`, so do not apply the change before pushing.
4. For continuous edits (dragging, sliders) implement `id()` and
   `mergeWith()`.
5. Set a translated text:
   `setText(QCoreApplication::translate("hmi::Commands", "Move %n object(s)", nullptr, count))`.
6. Test in `tests/editor/CommandsTest.cpp`: build a fixture with
   `ScreenBuilder`, clone the screen, push, compare; undo, compare with the
   original; redo, compare again.

## Recipe: add a panel

1. Write the data side in C++ under `src/editor/models/`: a
   `QAbstractListModel`, `QAbstractTableModel` or `QAbstractItemModel` with
   named roles, plus invokables for actions. Edits go through `setData`,
   which pushes a command.
2. Own it in `AppContext` and pass it to `Main.qml` as a required property
   (`setInitialProperties` in `main.cpp` and `EditorShellTest`).
3. Write the QML view under `src/editor/qml/panels/`: `ListView`,
   `TableView` or `TreeView` with a delegate. Bindings and one-line calls
   only.
4. All sizes, colours and fonts come from `Theme`; rows are at least
   `Theme.touchTarget` high in comfortable density.
5. Add the panel to the `SplitView` in `Main.qml` (and, from phase 4, its
   collapse state to settings).
6. Test the model in C++ without loading any QML.

## Recipe: add a property editor type (from phase 4)

1. Add the alternative to `PropertyValue` and extend JSON conversion in
   `PropertyJson.cpp` in both directions.
2. Add an editor role value in `PropertyModel` and a QML delegate in
   `qml/panels/properties/`, selected with `DelegateChooser`.
3. The delegate calls `model.setValue(...)` only; the model creates the
   command.
4. Support the mixed-value state for multi-selection and a touch-friendly
   input (keypad, picker) in comfortable density.

## Recipe: add a tag driver (from phase 6)

1. Create `src/comm/drivers/<protocol>/` with `<Protocol>Driver` implementing
   `IDriver` and a `<Protocol>DriverFactory` implementing `IDriverFactory`.
2. Wrap the third-party client completely; no library types in public headers.
3. Parse addresses in a separate `<Protocol>Address` class with unit tests.
4. Report every failure through `Result`; reconnect with back-off inside the
   driver.
5. Register with `HMI_REGISTER_DRIVER` (added with the first driver, like
   `HMI_REGISTER_OBJECT`).
6. Add the library to `cmake/Dependencies.cmake` (FetchContent, pinned
   version and hash) and to the table in ARCHITECTURE.md with its licence.

## Recipe: change the file format (from phase 4)

1. Increase `kCurrentFormatVersion`.
2. Add `migrateV<n>ToV<n+1>(QJsonObject&)` and append it to the chain.
3. Add a sample file of the old version under `tests/data/` and a test that
   loads it.
4. Keep unknown keys when reading and write them back.

## UI and touch rules

- Every feature must be usable with touch alone and with mouse and keyboard
  alone.
- Interactive targets are at least 44 px in comfortable density; resize
  handles have a 44 px hit area even when drawn smaller.
- Nothing depends on hover, right-click or a modifier key without a touch
  alternative (long-press, toolbar button, mode toggle).
- Colours, spacing and fonts come from `Theme`; no literals in QML or items.
  The Controls palette is set from `Theme` in `Main.qml`.
- A button whose state comes from C++ binds `checked` and is not
  `checkable`: a checkable button flips `checked` on click and breaks the
  binding.
- A QML `id` must not shadow a property name of a child object
  (`qmllint` reports it).
- Every action has an accessible name; icon-only buttons set
  `Accessible.name`.
- Status and Output messages are short and state what happened and what to do.

## Before finishing a task

- [ ] Builds without warnings; `qmllint` clean
- [ ] New code has tests; all tests pass
- [ ] Model changes go through commands and undo correctly
- [ ] Tool tests run as mouse and as touch; tried on a real touch screen when
      one is available
- [ ] No logic added to QML
- [ ] Nothing in `paint()` or `updatePaintNode()` writes shared state
- [ ] Save and load still round-trips
- [ ] No upward include (core does not see quick or editor)
- [ ] clang-format applied
- [ ] ARCHITECTURE.md or this file updated if a rule or structure changed
