# HMI Editor — Architecture

Desktop and touch editor for designing HMI screens (toolbox, canvas,
properties, tag binding, simulation), written in C++20 with Qt 6 and Qt Quick.

## 1. Assumptions

These are decisions made without your input. Change any of them and the rest
of the document still holds unless noted.

| Assumption | Effect if wrong |
|---|---|
| The editor must work with touch as well as mouse and keyboard | This drives the Qt Quick choice in section 2 |
| The editor is the first product; a panel runtime comes later | The runtime reuses the same Qt Quick items (section 7.4) |
| Targets are Windows and Linux, including touch screens and tablets | Android or embedded Linux need packaging work only |
| One developer | Phases in PLAN.md are sized for one person |
| Third-party libraries are allowed | Section 9 lists them; all are replaceable |
| First PLC connections are a built-in simulator, then OPC UA, then Siemens S7 | Driver order in PLAN.md changes |

## 2. UI technology decision

**Decision: Qt Quick, written C++-first.** The canvas and every HMI object
are C++ classes derived from `QQuickItem` and `QQuickPaintedItem`. QML is
used only as a thin layout layer for the application shell and panels, with
no logic in it.

Why Qt Quick:

1. Touch is a first-class input: multi-touch events, pinch and flick reach
   items directly, and Qt Quick Controls are built for fingers.
2. Rendering is GPU-accelerated through the scene graph, so zooming, panning
   and animated objects stay smooth on panel hardware.
3. The editor and the later runtime share one technology, so Simulate mode
   and the runtime are the same code.

What this costs compared with Widgets and Graphics View, so it is planned for:

| Lost ready-made feature | Replacement built in this project |
|---|---|
| `QGraphicsScene` selection, hit testing, rubber band | `HitTester` in `hmi_core` plus `SelectTool` (section 7.2) |
| Item transforms and z-order handling | `ObjectItem` position, size, rotation and `z` set from the model |
| `QDockWidget` docking | `SplitView` layout with collapsible panels |
| Widget property editors | Qt Quick Controls delegates chosen by a C++ model |

`QUndoStack` is still available, since in Qt 6 it lives in QtGui.

### 2.1 What "less QML, more C++" means here

| Layer | Language | Contents |
|---|---|---|
| Model, commands, tools, drivers | C++ | Everything in sections 5, 6 and 8 |
| Canvas and HMI objects | C++ | `QQuickItem` / `QQuickPaintedItem` subclasses, registered with `QML_ELEMENT` |
| Panel data and behaviour | C++ | `QAbstractItemModel` subclasses and controller `QObject`s with `Q_PROPERTY` and `Q_INVOKABLE` |
| Shell and panel layout | QML | Window, `SplitView`, toolbars, list and table delegates, text inputs |

Rules for the QML files:

- Declarative only: property bindings and calls to C++ controllers. No
  JavaScript functions, no state kept in QML.
- No QML file creates or edits model data. A button calls
  `editor.undo()`, a text field calls `properties.setValue(row, text)`.
- If a QML file grows past about 150 lines or needs an `if`, the logic moves
  into C++.

Building text fields, scrolling lists and tables as hand-written
`QQuickItem`s is possible but poor value: Qt Quick Controls already provide
keyboard, touch, IME and accessibility handling. So controls come from QML,
behaviour from C++.

**Qt version:** Qt 6.12 LTS is the current long-term release (September
2026); the design needs nothing newer than Qt 6.8. Compilers: MSVC 2022,
GCC 13+ or Clang 16+ for full C++20 including `std::format`.

## 3. Module layout

Each module is a CMake target. Nothing may depend upward.

```
hmi_editor (app: controllers + QML shell) ──┐
hmi_runtime (later) ────────────────────────┤
                                            ▼
                    hmi_quick (QQuickItem classes: canvas, object items)
                                            │
              hmi_render (QtGui, QtSvg)     │      hmi_comm (QtCore, QtNetwork)
                     │                      │              │
                     └──────────────────────┴──────────────┘
                                            ▼
                         hmi_core (QtCore, QtGui value types only)
```

| Target | Contents | Qt modules |
|---|---|---|
| `hmi_core` | Project, Screen, HmiObject hierarchy, property system, tags, bindings, events, serialization, object registry, hit testing | Core, Gui |
| `hmi_render` | Painter helpers, symbol drawing, graphic loading and caching (section 14) | Gui, Svg |
| `hmi_comm` | `IDriver`, simulator, OPC UA, S7, Modbus drivers, `TagEngine` | Core, Network |
| `hmi_quick` | `CanvasItem`, `ScreenItem`, `ObjectItem`, overlays, gesture recognizer; a QML module (`qt_add_qml_module`) | Quick |
| `hmi_editor` | `main.cpp`, `AppContext`, controllers, panel models, undo commands, tools, QML shell, theme | Quick, QuickControls2, QuickDialogs2 |
| `hmi_runtime` | Full-screen player (phase 9), links `hmi_quick` without editor code | Quick |
| `tests` | Unit tests per module | Test, QuickTest |

`hmi_core` must stay free of Qt Quick so it can be tested headless.

## 4. Directory layout

```
hmi-studio/
├─ CMakeLists.txt
├─ CMakePresets.json
├─ cmake/
├─ src/
│  ├─ core/
│  │  ├─ model/              # Project, Screen, HmiObject, GroupObject
│  │  ├─ objects/            # one file pair per object type
│  │  ├─ property/           # PropertyValue, PropertyDescriptor
│  │  ├─ tags/               # Tag, TagDatabase, Binding, Dynamization
│  │  ├─ events/             # Action, EventHandler
│  │  ├─ io/                 # ProjectSerializer, migrations
│  │  ├─ spatial/            # HitTester, snapping
│  │  └─ registry/           # ObjectRegistry (factory)
│  ├─ render/
│  ├─ comm/
│  │  ├─ drivers/            # simulator/, opcua/, s7/, modbus/
│  │  └─ TagEngine.*
│  ├─ quick/                 # C++ Qt Quick items
│  │  ├─ CanvasItem.*        # viewport: zoom, pan, input
│  │  ├─ ScreenItem.*        # one screen, parent of ObjectItems
│  │  ├─ ObjectItem.*        # QQuickPaintedItem per HmiObject
│  │  ├─ overlays/           # SelectionOverlay, GuideOverlay, GridItem, RulerItem
│  │  └─ input/              # GestureRecognizer, PointerGesture
│  └─ editor/
│     ├─ app/                # main.cpp, AppContext, EditorController
│     ├─ tools/              # SelectTool, CreateTool, PipeTool, PanTool
│     ├─ commands/           # QUndoCommand subclasses
│     ├─ models/             # ToolboxModel, PropertyModel, LayerModel, TagModel
│     └─ qml/                # Main.qml, panels/, controls/, Theme.qml
├─ tests/
├─ resources/
└─ docs/                     # ARCHITECTURE.md, PLAN.md, SKILL.md
```

## 5. Core model

Ownership is a strict tree held with `std::unique_ptr`:

```
Project
├─ TagDatabase          → Tag*
├─ Connection*          (PLC connections)
└─ Screen*
   └─ HmiObject*        (GroupObject owns children)
```

Model classes are plain C++ classes, not `QObject`s. This keeps them light
(a screen can hold thousands of objects) and copyable through `clone()`.
Change notification goes through one `QObject` per document, the
`DocumentNotifier`, which emits `objectAdded`, `objectRemoved`,
`propertyChanged(ObjectId, PropertyId)` and similar signals.

### 5.1 HmiObject

```cpp
class HmiObject {
public:
    virtual ~HmiObject() = default;

    [[nodiscard]] virtual ObjectTypeId typeId() const noexcept = 0;
    [[nodiscard]] virtual std::unique_ptr<HmiObject> clone() const = 0;   // Prototype, same id
    [[nodiscard]] std::unique_ptr<HmiObject> cloneWithNewId() const;      // paste, duplicate
    [[nodiscard]] std::span<const PropertyDescriptor> descriptors() const noexcept;

    [[nodiscard]] std::optional<PropertyValue> property(PropertyId id) const;
    template <PropertyType T> [[nodiscard]] const T& value(PropertyId id) const;
    Result<void> setProperty(PropertyId id, const PropertyValue& value);  // validates, notifies

    virtual void paint(QPainter& painter, const PaintContext& ctx) const = 0;
    [[nodiscard]] virtual QPainterPath hitShape() const;                  // default: bounds
    virtual void accept(ObjectVisitor& visitor) = 0;                      // Visitor

    [[nodiscard]] ObjectId id() const noexcept;
    [[nodiscard]] QRectF geometry() const noexcept;
    // rotation, z-order, name, visibility, lock state, bindings, events
};
```

`paint()` takes a `QPainter`, so the object does not know it is hosted in
Qt Quick. `ObjectItem` (a `QQuickPaintedItem`) supplies the painter.

Each type passes its static descriptor table to the `HmiObject` constructor,
which stores the values in table order. While an object belongs to a
`Screen` inside a `Project`, `setProperty()` emits
`DocumentNotifier::propertyChanged`; a detached object (fixture, undo
snapshot, clipboard) changes silently. `PropertyId` and `ObjectTypeId` are
compile-time FNV-1a hashes of their file keys; `ObjectId` and `ScreenId` are
UUIDs so they stay unique across paste and projects.

Hierarchy:

```
HmiObject
├─ ShapeObject      Line, Polyline, Polygon, Ellipse, Rectangle, TextBox, GraphicView
├─ ElementObject    IoField, Button, Switch, CheckBox, RadioGroup, ListBox,
│                   SymbolicIoField, Bar, Slider, Gauge, Clock, TouchArea
├─ ControlObject    AlarmView, TrendView, FxTrendView, ScreenWindow, Browser,
│                   MediaPlayer, RecipeView, DiagnosticsView
├─ SymbolObject     Tank, Pump, Valve, Motor, Pipe, Exchanger, Conveyor, Sensor
└─ GroupObject      Composite: owns child HmiObjects
```

### 5.2 Property system

Properties are data, not hand-written getters, so the property panel,
serializer and tag binding all work from the same description.

```cpp
using PropertyValue = std::variant<bool, int, double, QString, QColor,
                                   QPointF, QSizeF, QFont, TagRef, EnumValue>;

template <typename T>
concept PropertyType = requires { std::get<T>(std::declval<PropertyValue>()); };

struct PropertyDescriptor {
    PropertyId    id;
    const char*   key;          // identifier in files; hashes to id
    const char*   displayName;  // translated in context "hmi::Property"
    const char*   category;     // Layout, Appearance, Behaviour ...
    PropertyValue defaultValue;
    Constraint    constraint;   // min/max, enum options, read-only
    bool          bindable;     // can a tag drive it at runtime?
};

Result<PropertyValue> validate(const PropertyDescriptor&, const PropertyValue&);
```

`validate()` checks the type (an `int` is widened for a `double` property),
range, finiteness, enum membership and colour validity.
`withCommonDescriptors()` prepends the properties every object has (name,
x, y, width, height, rotation, visible, locked). Property ids that mean the
same thing across types (`Prop::FillColor`) are shared in `Prop.h`, so the
property panel can merge them across a mixed selection.

Each object type declares a static table of descriptors. `ObjectItem`, the
property panel and the serializer never switch on the concrete class.

### 5.3 Tags, dynamization, events

| Concept | Meaning |
|---|---|
| `Tag` | Name, data type, connection, address, scaling, limits |
| `Binding` | Links one bindable property to a tag, with a transform |
| `Transform` | Direct, linear scale, range map (value ranges → colour/visibility), expression |
| `EventHandler` | Trigger (press, release, value change, screen loaded) plus a list of `Action`s |
| `Action` | SetTag, ToggleTag, IncrementTag, OpenScreen, ShowPopup, AcknowledgeAlarm, RunScript |

Transforms and actions are small polymorphic classes created through
registries, so new ones do not touch existing code.

## 6. Design patterns and where each one is used

| Pattern | Use in this project |
|---|---|
| Factory Method / registry | `ObjectRegistry` maps `ObjectTypeId` to a creator plus toolbox metadata. Types register themselves at start-up. Same idea for `DriverRegistry`, `ActionRegistry`, `TransformRegistry`. |
| Abstract Factory | `IDriverFactory` creates a driver and its matching address parser and configuration model for one protocol. |
| Prototype | `HmiObject::clone()` for copy, paste, duplicate and the user library. |
| Composite | `GroupObject`; group, ungroup and nested transforms. |
| Command | Every model change is a `QUndoCommand`. Multi-step edits use `beginMacro`. |
| State | Canvas tools (`SelectTool`, `CreateTool`, `PipeTool`, `PanTool`) behind `ITool`; `ToolController` holds the current one. `GestureRecognizer` is also a small state machine. |
| Observer | Qt signals from `DocumentNotifier`, `TagEngine`, `QUndoStack`; QML bindings observe controller properties. |
| Builder | `ScreenBuilder` and `ProjectBuilder` assemble templates and test fixtures fluently. |
| Strategy | Snapping (grid, object edges, guides), binding transforms, SVG backend, export formats. |
| Visitor | `ObjectVisitor` for validation, export, "where is this tag used". |
| Adapter | Each driver adapts a third-party client library to `IDriver`; `GestureRecognizer` adapts mouse, touch and pen to one `PointerGesture`. |
| Facade | `TagEngine` hides drivers and threading; `EditorController` is the single object QML talks to for commands. |
| Memento | Project snapshot for autosave and crash recovery. |
| Model/View | `QAbstractItemModel` subclasses feed QML `ListView`, `TableView` and `TreeView`. |
| Singleton | Used sparingly: registries filled at start-up only, as function-local statics. |

On Singleton: registries are safe as singletons because they are filled once
and then read-only. Document state, settings, the undo stack and the tag
engine are not singletons; they live in an `AppContext` created in `main()`.
QML reaches them as typed required properties of the root window, set with
`QQmlApplicationEngine::setInitialProperties()` and passed down to each
panel as required properties. Unlike context properties this keeps every
access visible to `qmllint` and the QML compiler; there are no QML singletons
with hidden state.

## 7. Editor application

### 7.1 Canvas item tree

```
CanvasItem : QQuickItem            viewport; owns zoom and pan; receives all input
├─ GridItem : QQuickItem           one-pixel rectangle nodes
├─ ScreenItem : QQuickItem         one Screen; scaled and translated by CanvasItem
│  └─ ObjectItem : QQuickPaintedItem   one per HmiObject, in z-order
├─ SelectionOverlay : QQuickItem   frame, handles, rotation handle, size label
├─ GuideOverlay : QQuickItem       snap guides, rubber band
└─ RulerItem ×2 : QQuickPaintedItem
```

- `ScreenItem` listens to `DocumentNotifier` and creates or removes
  `ObjectItem`s. It is the Qt Quick counterpart of a scene.
- `ObjectItem` holds a non-owning pointer to its `HmiObject`. Its
  `paint(QPainter*)` calls `HmiObject::paint()`. Position, size, rotation and
  `z` are copied from the model whenever the notifier reports a change. It
  stores no state of its own.
- Overlays are siblings above the screen, drawn in viewport coordinates so
  handles keep a constant on-screen size at any zoom.
- Grid and overlays are built only from rectangle and transform nodes
  (`SceneGraphShapes`), because the software scene graph backend, used where
  no GPU is available, ignores custom geometry nodes. After rebuilding its
  nodes an overlay marks its item transform node dirty: the software
  renderer otherwise places nodes added to an already rendered subtree with
  a stale transform.
- Tools preview by moving `ObjectItem`s (`ICanvasView::setPreviewGeometry`)
  and drawing on the overlay; the model changes once, through a command, when
  the gesture ends.
- Only `CanvasItem` accepts input. Object items are not interactive in edit
  mode; that keeps all interaction logic in tools rather than spread over
  items.

### 7.2 Input: mouse, touch and pen through one path

```
QQuickItem events (mouse, touch, wheel, key)
        │
        ▼
GestureRecognizer  →  PointerGesture { kind, scenePos, delta, scale, modifiers, device }
        │
        ▼
ToolController  →  current ITool  →  preview on items/overlays  →  QUndoCommand on release
```

`CanvasItem` calls `setAcceptTouchEvents(true)` and overrides `touchEvent`,
the mouse event handlers, `wheelEvent` and `keyPressEvent`. Tools never see
raw events, only gestures, so each tool works with every input device.

| Gesture | Mouse and keyboard | Touch |
|---|---|---|
| Select | Click | Tap |
| Add to selection | Shift+click | Tap with multi-select mode on, or long-press then tap |
| Rubber-band select | Drag on empty area | Long-press on empty area, then drag |
| Move object | Drag object | Drag object |
| Resize, rotate | Drag handle | Drag handle (handles have a 44 px touch target) |
| Pan | Space+drag, middle button | Two-finger drag; one-finger drag on empty area |
| Zoom | Ctrl+wheel | Pinch, centred between the fingers |
| Context menu | Right-click | Long-press on object |
| Place new object | Drag from toolbox, or click tool then drag on canvas | Drag from toolbox, or tap tool then tap canvas |
| Nudge | Arrow keys | Arrow buttons in the selection toolbar |

Touch additions outside the canvas: a floating selection toolbar (delete,
duplicate, group, bring forward) near the selection, undo and redo always
visible, numeric fields that open an on-screen keypad, and all controls at
least 44 px high when the UI is in touch density. `Theme` exposes a density
setting (compact for mouse, comfortable for touch) chosen from the primary
input device and changeable by the user.

Hit testing: `HitTester` in `hmi_core` walks the objects of a screen from
top to bottom and tests `hitShape()` with a tolerance that depends on the
device (larger for touch). A spatial index is added only if profiling at
2,000 objects shows a need.

### 7.3 Panels

Every panel is a C++ model plus a short QML view.

| Panel | C++ | QML view |
|---|---|---|
| Toolbox | `ToolboxModel` (from `ObjectRegistry`), `ToolboxDragController` | `GridView` of tiles |
| Properties | `PropertyModel` built from the selection's descriptors; `setData` pushes `SetPropertyCommand` | `ListView` with a delegate chosen by value type |
| Layers | `LayerModel` (tree) | `TreeView` |
| Tags | `TagModel`, `TagFilterProxy` | `TableView` |
| Output | `MessageModel` filled by `ValidationVisitor` | `ListView` |

`EditorController` exposes actions (`undo`, `redo`, `copy`, `align…`,
`simulate`) and state (`canUndo`, `zoom`, `currentTool`) as properties and
invokables. `Main.qml` lays the panels out in a `SplitView`; panels can be
collapsed and their sizes are saved in settings.

### 7.4 Simulation mode and runtime

Simulate switches `CanvasItem` to play mode: tools are disabled, input goes
to `RuntimeInputHandler`, which maps taps and drags to object events
(button press, slider drag). `TagEngine` starts, `BindingEvaluator` drives
properties from tag values and `ObjectItem`s repaint.

The runtime executable is the same `ScreenItem` and `ObjectItem` in play
mode inside a minimal QML window, with no editor code linked.

### 7.5 Threads in Qt Quick

- Qt Quick renders on a separate render thread. `QQuickPaintedItem::paint()`
  is called from that thread while the GUI thread is blocked, so reading the
  model inside `paint()` is safe; changing it there is forbidden.
- Items are created, moved and updated only from the GUI thread, followed by
  `update()`.
- Custom scene-graph code (`updatePaintNode`) touches only data copied in
  during the sync phase.

## 8. Communication and threading

```cpp
class IDriver {
public:
    virtual ~IDriver() = default;
    virtual Result<void> connect(const ConnectionConfig&) = 0;
    virtual void disconnect() = 0;
    virtual Result<TagValue> read(const TagAddress&) = 0;
    virtual Result<void> write(const TagAddress&, const TagValue&) = 0;
    virtual void subscribe(std::span<const TagAddress>, UpdateCallback) = 0;
};
```

- Drivers run on a worker thread owned by `TagEngine`. The GUI thread never
  blocks on network I/O.
- Updates reach the GUI through queued signals and are coalesced to about
  10–20 Hz, so a fast PLC cannot flood repaint.
- `Result<T>` is `tl::expected<T, Error>` (C++20 has no `std::expected`).
  Exceptions are used only for programming errors.

## 9. Third-party libraries

Check each licence against how you intend to ship before adopting it; the
licence column is from memory and should be confirmed on the project page.

| Purpose | Library | Licence (verify) | Notes |
|---|---|---|---|
| Error handling | tl::expected | CC0 | Header only |
| Logging | spdlog | MIT | Or `QLoggingCategory` with no dependency |
| Expressions in bindings | exprtk | MIT | Alternative: `QJSEngine` if scripts are wanted |
| OPC UA client | open62541 | MPL 2.0 | Qt OPC UA is GPL/commercial only |
| Siemens S7 | Snap7 | LGPL 3 | Covers S7-300/400/1200/1500 with PUT/GET enabled |
| Modbus TCP/RTU | Qt SerialBus or libmodbus | LGPL | |
| Tests | Qt Test and Qt Quick Test, or Catch2 for core | LGPL / BSL | |
| JSON | `QJsonDocument` | part of Qt | No extra dependency needed |
| SVG rendering | Qt SVG | part of Qt (LGPL) | Default backend, see section 14 |
| SVG rendering, fuller standard support | lunasvg or resvg | MIT / MPL 2.0 | Optional second backend |
| On-screen keyboard | Qt Virtual Keyboard | GPL/commercial | Only if the OS keyboard is not enough; check licence first |

Dependencies are fetched with CMake `FetchContent` or vcpkg manifest mode;
pick one and keep it. Qt Quick, Qt Quick Controls and Qt SVG are LGPL, which
permits closed-source use with dynamic linking, or commercial.

## 10. Project file format

A project is a folder, which works well with Git:

```
MyLine.hmiproj/
├─ project.json        # name, target device, format version
├─ tags.json
├─ connections.json
├─ screens/
│  ├─ 01_overview.json
│  └─ 02_filling.json
└─ assets/             # imported images
```

- JSON, pretty-printed, stable key order, so diffs are readable.
- Every file carries `formatVersion`. `ProjectSerializer` runs a chain of
  migration functions from the file's version to the current one.
- Objects serialize as `{ "type": "tank", "id": "...", "props": { ... },
  "bindings": [...], "events": [...] }` driven by descriptors, so a new
  object type needs no serializer code.
- Unknown types and properties are kept and written back unchanged.

## 11. C++20 usage

| Feature | Where |
|---|---|
| Concepts | `PropertyType`, driver and visitor constraints |
| `std::span` | Passing descriptor tables and address lists |
| Ranges and views | Filtering selections, searching objects and tags |
| `<=>` and defaulted `==` | Ids, addresses, value types |
| Designated initializers | Descriptor tables, configuration structs |
| `constexpr` / `consteval` | Compile-time type ids from string hashes |
| `std::format` | Log and status messages |
| `enum class`, `[[nodiscard]]`, `std::unique_ptr`, `std::variant` | Everywhere |

Not used: C++20 modules (tooling with moc is still awkward) and coroutines.

## 12. Quality

- Warnings as errors: `/W4 /permissive-` or `-Wall -Wextra -Wpedantic`.
- AddressSanitizer and UBSan on the Linux debug preset.
- clang-format, clang-tidy and `qmllint` in CI.
- `hmi_core` and `hmi_comm` have unit tests. Commands are tested for do,
  undo, redo leaving the model identical. Tools are tested by feeding
  synthetic `PointerGesture`s, once as mouse and once as touch.
- Canvas items are tested offscreen with `QQuickWindow` and `QTest` touch
  and mouse helpers.
- CI builds Windows and Linux and runs tests on every push.

## 13. Main risks

| Risk | Mitigation |
|---|---|
| Scope: a WinCC-class editor is years of work | PLAN.md delivers a usable editor with a small object set first |
| The canvas (selection, hit testing, handles, gestures) is all custom code | It is the largest item in phases 2 and 3, with its own tests; logic sits in `hmi_core` and tools, not in items |
| One texture per `QQuickPaintedItem` uses a lot of GPU memory on large screens | Measure at 2,000 objects in phase 3; fallbacks in section 14.5 |
| Blurry objects after zooming, since painted items are textures | Re-render at the new scale when zoom settles (section 14.5) |
| Logic leaking into QML over time | Rules in section 2.1, `qmllint`, review checklist in SKILL.md |
| Fine editing with fingers is imprecise | Larger hit tolerance, numeric fields, nudge buttons, zoom-to-selection |
| S7-1500 access needs PUT/GET enabled or OPC UA | Make OPC UA the first real driver |
| An imported SVG uses features Qt SVG cannot draw | Import check, optional second backend, rasterize fallback (section 14.3) |

## 14. Graphics and image rendering

| Kind | Examples | How it is drawn |
|---|---|---|
| Built-in objects | Rectangle, gauge, tank, pump, valve | C++ `QPainter` code in the object's `paint()`, hosted by a `QQuickPaintedItem` |
| Raster graphics | PNG, JPEG, BMP, GIF, WebP | `QImageReader` → `QImage`, drawn through the same painter |
| Vector graphics | Imported SVG, symbol libraries | Qt SVG (`QSvgRenderer`) painting into the same painter |

### 14.1 SVG in Qt

Qt handles SVG through the separate **Qt SVG** module. Add it with
`find_package(Qt6 REQUIRED COMPONENTS Svg)` and link `Qt6::Svg`.

- `QSvgRenderer::render(QPainter*, QRectF)` draws a file at any size, so it
  fits `HmiObject::paint()` directly.
- The module targets the SVG Tiny 1.2 static profile. Shapes, paths,
  gradients, transforms, text and groups work. Recent Qt 6 releases added
  more of full SVG 1.1, but complex CSS, scripts and some filter effects are
  not covered. Check the Qt SVG documentation for the version you install.
- Files exported as "plain SVG" or "SVG Tiny" render reliably.

### 14.2 Classes

```
AssetManager            owns the project's assets/ folder
 └─ Asset               id (content hash), original file name, kind

IGraphicSource          render(QPainter&, QRectF target, const GraphicStyle&)
 ├─ RasterGraphic       QImageReader, keeps the decoded QImage
 └─ SvgGraphic          holds the SVG bytes, renders through ISvgBackend

ISvgBackend             Strategy: which library draws SVG
 ├─ QtSvgBackend        default, QSvgRenderer
 └─ LunaSvgBackend      optional, enabled by a CMake option

GraphicCache            QImage cache keyed by
                        (asset id, pixel size, device pixel ratio, style hash)
GraphicFactory          picks RasterGraphic or SvgGraphic from the file type
```

- `GraphicViewObject` stores only an asset id and fit mode. It asks
  `AssetManager` for the `IGraphicSource` when painting.
- `AssetManager` copies an imported file into `assets/` under its content
  hash, so an image used on ten screens is stored once.
- The cache holds `QImage`, not `QPixmap`, because painting happens on the
  render thread.

### 14.3 Unsupported SVG content

On import, `SvgInspector` scans the file for elements the active backend
cannot draw (for example `<filter>`, `<foreignObject>`, `<script>`) and
offers: import anyway, use the second backend if built in, or rasterize to
PNG at a chosen resolution.

### 14.4 Dynamic SVG symbols

- **Colour and visibility by element id.** `SvgGraphic` parses the file once
  with `QDomDocument`. A `GraphicStyle` holds overrides such as
  `{"body": fill #3B6FD4}` or `{"alarm-ring": hidden}`, applied before
  rendering and cached under the style hash. Bindings write into
  `GraphicStyle`, so a tag can drive any named part of an imported symbol.
- **Per-element rendering.** `QSvgRenderer::render(painter, elementId,
  bounds)` draws one named element, so a part can have its own rotation.

Built-in process symbols stay as C++ painter code because they need
continuous animation (tank level, bar fill).

### 14.5 Rendering quality and speed in Qt Quick

A `QQuickPaintedItem` paints into a texture that the scene graph then
composites. Consequences and how they are handled:

- **Sharpness when zooming.** While a pinch or wheel zoom is in progress the
  existing textures are scaled by the GPU (fast, slightly soft). When the
  zoom settles, `ObjectItem` sets its texture size to item size × zoom ×
  device pixel ratio and repaints, so the result is sharp.
- **Texture size limit.** The texture size is capped (for example 4096 px);
  beyond that an item stays at the cap and is scaled.
- **Repaint cost.** Only items whose model changed call `update()`. Panning
  and zooming move textures without repainting anything.
- **Memory with many objects.** If the 2,000-object test shows a problem,
  three fallbacks exist, in order of effort: lower the texture scale for
  off-screen items and release textures of items outside the viewport; draw
  basic shapes (rectangle, line, ellipse) with scene-graph geometry nodes in
  `updatePaintNode` instead of textures; evaluate Qt Canvas Painter, the
  GPU-accelerated painter added in Qt 6.12, as a replacement painter.
- **Antialiasing.** `setAntialiasing(true)` on painted items; overlay lines
  are axis-aligned one-pixel rectangles snapped to whole pixels.
- **Level of detail.** `PaintContext` carries the current zoom; below a
  threshold objects skip small text and tick marks.
- **Icons.** Toolbox and toolbar icons are single-colour SVGs recoloured from
  the theme.
