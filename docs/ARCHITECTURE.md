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
   and animated objects stay smooth on panel hardware. Without a usable GPU
   (virtual machines, remote desktop, some panels) Qt Quick falls back to its
   software renderer; everything in this design must also draw there
   (section 7.1).
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
hmi_editor (exe) → hmi_editor_lib (controllers + QML shell) ─┐
hmi_runtime (later) ─────────────────────────────────────────┤
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
| `hmi_core` | Project, Screen, HmiObject hierarchy, property system, tags, bindings, events, serialization, object registry, hit testing. Built as the static `hmi_core_impl`; consumers link the `hmi::core` interface target, which links it whole so self-registering types are kept | Core, Gui |
| `hmi_render` | Graphic sources (raster, SVG), the SVG backend strategy, the image cache, the SVG import check; implements core's `IGraphicProvider` (section 14). Object painting itself stays in `hmi_core` | Gui, Svg |
| `hmi_comm` | `IDriver`, `SimulatorDriver`, later OPC UA, S7, Modbus drivers; `TagEngine`; `BindingEvaluator` (play mode) | Core (Network with the first PLC driver) |
| `hmi_quick` | `CanvasItem`, `ScreenItem`, `ObjectItem`, overlays, gesture recognizer; static QML module `HmiStudio.Quick` | Quick |
| `hmi_editor_lib` | `AppContext`, `Theme`, controllers, panel models, undo commands, tools and the QML shell; static QML module `HmiStudio.Editor`, so tests link it without the app | Quick, QuickControls2, Svg; the `QtQuick.Dialogs` and `QtQml.Models` QML modules |
| `hmi_editor` | The executable: `main.cpp` only; output name `HmiStudio` | as above |
| `hmi_runtime` | Full-screen player (phase 9), links `hmi_quick` without editor code | Quick |
| `tests` | One Qt Test executable per test class | Test |

`hmi_core` must stay free of Qt Quick so it can be tested headless. The
static QML modules are imported in `main.cpp` (and in tests that load QML)
with `Q_IMPORT_QML_PLUGIN(HmiStudio_QuickPlugin)` and
`Q_IMPORT_QML_PLUGIN(HmiStudio_EditorPlugin)`; their build output goes to
`build/<preset>/qml/HmiStudio/...` so `qmllint` can resolve them.

## 4. Directory layout

```
hmi-studio/
├─ CMakeLists.txt
├─ CMakePresets.json
├─ cmake/
├─ src/
│  ├─ core/
│  │  ├─ model/              # Project, Screen, HmiObject, family bases, GroupObject,
│  │  │                      # UnknownObject, HmiStyle
│  │  ├─ graphics/           # AssetStore, GraphicStyle, IGraphicProvider
│  │  ├─ objects/            # one file pair per object type
│  │  ├─ property/           # PropertyValue, PropertyDescriptor
│  │  ├─ tags/               # Tag, TagDatabase, Binding, Dynamization
│  │  ├─ events/             # Action, EventHandler
│  │  ├─ io/                 # ObjectJson, ProjectSerializer and its migrations
│  │  ├─ spatial/            # HitTester, Snapper and snap strategies
│  │  └─ registry/           # ObjectRegistry (factory)
│  ├─ render/                # GraphicLibrary, GraphicCache, GraphicFactory,
│  │                         # RasterGraphic, SvgGraphic, QtSvgBackend, SvgInspector
│  ├─ comm/
│  │  ├─ IDriver.h
│  │  ├─ SimulatorDriver.*   # ramp, sine, random, toggle, manual
│  │  ├─ TagEngine.*         # worker thread, coalesced updates
│  │  ├─ BindingEvaluator.*  # play copy of a screen driven by tags
│  │  └─ drivers/            # later: opcua/, s7/, modbus/
│  ├─ quick/                 # C++ Qt Quick items
│  │  ├─ CanvasItem.*        # viewport: zoom, pan, input
│  │  ├─ ICanvasView.h       # what tools may ask of the canvas
│  │  ├─ ScreenItem.*        # one screen, parent of ObjectItems
│  │  ├─ ObjectItem.*        # QQuickPaintedItem per HmiObject
│  │  ├─ overlays/           # SelectionOverlay, GuideOverlay, GridItem,
│  │  │                      # RulerItem, SelectionGeometry, SceneGraphShapes
│  │  └─ input/              # GestureRecognizer, PointerGesture, InputSettings
│  └─ editor/
│     ├─ app/                # main.cpp, AppContext, EditorController,
│     │                      # EditActions, DocumentController, AppSettings, Theme
│     ├─ tools/              # ITool, ToolController, ToolContext, SelectTool,
│     │                      # CreateTool, PathTool, PanTool
│     ├─ commands/           # QUndoCommand subclasses
│     ├─ models/             # SelectionModel, ToolboxModel, ToolboxDragController,
│     │                      # PropertyModel, LayerModel, ScreenListModel; later TagModel
│     └─ qml/                # Main.qml, panels/, controls/
├─ tests/                    # core/, quick/, editor/, mirroring src/
├─ resources/icons/          # SVG icons: two-tone toolbox icons, single-colour tool icons
├─ docs/                     # ARCHITECTURE.md, PLAN.md, HMI Editor@2x.png
└─ .claude/skills/hmi-studio-dev/SKILL.md   # rules and recipes
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
    [[nodiscard]] QRectF geometry() const;            // from x, y, width, height
    [[nodiscard]] QTransform sceneTransform() const;  // rotation about the centre
    // name, rotation, visibility, lock state; later bindings and events
};
```

`paint()` takes a `QPainter`, so the object does not know it is hosted in
Qt Quick. `ObjectItem` (a `QQuickPaintedItem`) supplies the painter.

Each type passes its static descriptor table to the `HmiObject` constructor,
which stores the values in table order. While an object belongs to a
`Screen` inside a `Project`, `setProperty()` emits
`DocumentNotifier::propertyChanged`; a detached object (fixture, undo
snapshot, clipboard, group child) changes silently. Z-order is the object's index in its
`Screen` (index 0 at the bottom), not a property. `PropertyId` and
`ObjectTypeId` are
compile-time FNV-1a hashes of their file keys; `ObjectId` and `ScreenId` are
UUIDs so they stay unique across paste and projects.

Hierarchy (36 toolbox types since phase 5):

```
HmiObject
├─ ShapeObject      Line, Polyline, Polygon, Ellipse, Circle, Rectangle, TextBox,
│                   GraphicView
├─ ElementObject    IoField, Button, Switch, CheckBox, RadioGroup, ListBox,
│                   SymbolicIoField, Bar, Slider, Gauge, Clock, TouchArea
├─ ControlObject    AlarmView, TrendView, FxTrendView, ScreenWindow, Browser,
│                   MediaPlayer, RecipeView, DiagnosticsView
├─ SymbolObject     Tank, Pump, Valve, Motor, Pipe, Exchanger, Conveyor, Sensor
├─ GroupObject      Composite: owns child HmiObjects
└─ UnknownObject    placeholder for a type this version does not know
```

Each type is a descriptor table, a `paint()`, a `hitShape()` when it is not
rectangular, and a registration. `Describe::` builders
(`core/property/DescriptorBuilders.h`) and `PaintHelpers`
(`core/objects/PaintHelpers.h`) keep each type's file down to its own
properties and drawing. Controls are static placeholders in the editor;
their live behaviour comes in phase 7. Objects that are invisible at
runtime (touch area, an empty graphic view) draw a marked outline while
`PaintContext::editing` is set.

How a type responds to resizing follows WinCC and other HMI editors:

- Pictures: types with a "Stretch mode" property (pump, valve, motor, heat
  exchanger, conveyor, sensor, gauge, clock) are drawn at their design
  size (the type's default size) and scaled to the rectangle, keeping
  proportions (centred, the default) or stretched
  (`HmiObject::designTransform`). Lines and text scale with the picture.
- Controls with a label beside them (switch, check box, radio group) scale
  with the height (`PaintHelpers::scaleToHeight`): control and label grow
  together, and extra width is room for the label. Slider knobs and
  drop-down arrows grow with the thickness.
- Fields, buttons, bars, lists, tables and shapes fill their rectangle and
  keep their font size and line width, which are properties; lists and
  tables show more rows when taller. Tank and pipe lay themselves out in
  their rectangle (a taller tank is a taller vessel; a pipe follows its
  points).

No part may keep a fixed size while the rest grows; `AllObjectsTest`
draws each type at twice its size and compares.

The registry records how a type is placed (`Placement`): by dragging a
rectangle, or point by point (polyline, polygon) or with right-angled
segments (pipe), which `ToolController` gives to `PathTool`.

`HmiStyle` is the project's shared look: field, outline, text and accent
colours and a font family, saved in `project.json`. Descriptors whose
meaning matches carry a `StyleRole` (a gauge's red needle does not); new
objects take the style when placed, "Apply project style" restyles a
selection, and no type has style code of its own.

`GroupObject` keeps its children in its own coordinates, exactly as they
were when grouped, and maps the rectangle around them onto its geometry.
Moving, resizing or rotating a group therefore changes only the group's
properties, never the children's, which keeps undo exact.
`ungroupedChildren()` bakes the group's transform into copies of the
children. `HmiObject` carries the composite interface (`children()`,
`setChildren()`, `ungroupedChildren()`), so commands and the serializer
never test for the group type. Groups are registered as an internal type:
files and the clipboard can contain them, the toolbox does not offer them.

`ObjectJson` writes objects as JSON from their descriptors (section 10).
The clipboard uses it under the MIME type `application/x-hmistudio-objects`,
and `ProjectSerializer` uses it for screen files.

An object, a screen and the project each keep the part of their file this
version does not understand (`preservedJson()`: unknown keys, unknown
properties) and write it back unchanged. An object of an unknown type is
loaded as an `UnknownObject`: it has the common properties, so it can be
selected, moved and deleted, draws a marked placeholder, and keeps its
whole entry.

`Project::replaceContents()` swaps in a loaded project while the `Project`
object, and so its `DocumentNotifier`, stays the same: everything holding a
`Project&` (tool context, commands, models) keeps working after open, new
and recovery. It emits `projectReset`, and destroys the old screens only
after every view has moved to the new ones.

### 5.2 Property system

Properties are data, not hand-written getters, so the property panel,
serializer and tag binding all work from the same description.

```cpp
using PropertyValue = std::variant<bool, int, double, QString, QColor,
                                   QPointF, QSizeF, QFont, TagRef, EnumValue>;

// True when T is one of the variant's alternatives. A requires-expression on
// std::get<T> does not work here: std::get with a wrong type is a hard error.
template <typename T>
concept PropertyType = detail::IsAlternative<T, PropertyValue>::value;

struct PropertyDescriptor {
    PropertyId    id;
    const char*   key;          // identifier in files; hashes to id
    const char*   displayName;  // translated in context "hmi::Property"
    const char*   category;     // Layout, Appearance, Behaviour ...
    PropertyValue defaultValue;
    Constraint    constraint;   // min/max, enum options, read-only (advice for editors)
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
| State | Canvas tools (`SelectTool`, `CreateTool`, `PathTool`, `PanTool`) behind `ITool`; `ToolController` holds the current one. `GestureRecognizer` is also a small state machine. |
| Observer | Qt signals from `DocumentNotifier`, `TagEngine`, `QUndoStack`; QML bindings observe controller properties. |
| Builder | `ScreenBuilder` assembles screens for templates and test fixtures fluently (a `ProjectBuilder` when projects need it). |
| Strategy | Snapping (grid, object edges, guides), binding transforms, SVG backend, export formats. |
| Visitor | `ObjectVisitor` for validation, export, "where is this tag used". |
| Adapter | Each driver adapts a third-party client library to `IDriver`; `GestureRecognizer` adapts mouse, touch and pen to one `PointerGesture`. |
| Facade | `TagEngine` hides drivers and threading; `EditorController` is the single object QML talks to for editing actions and display state. |
| Memento | Autosave: `DocumentController` writes the modified project to an autosave folder at an interval; it is offered for recovery if the application did not exit normally. |
| Model/View | `QAbstractItemModel` subclasses feed QML `ListView` and `TableView`. |
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
├─ GridItem : QQuickItem           screen background and grid lines
├─ ScreenItem : QQuickItem         one Screen; scaled and translated by CanvasItem
│  └─ ObjectItem : QQuickPaintedItem   one per HmiObject, z from its index
├─ GuideOverlay : QQuickItem       snap guides while moving or resizing
└─ SelectionOverlay : QQuickItem   frames, eight handles, rotation handle,
                                   rubber band

RulerItem ×2 : QQuickPaintedItem   QML siblings of the canvas, bound to it
                                   through their `canvas` property
```

`CanvasItem` creates its children in C++ in this order, so the grid lies
between the screen background and the objects. It exposes the selection's
view rectangle (`selectionBounds`) and whether a tool preview is running
(`previewActive`); QML places the size label and the touch selection
toolbar from them, and `EditorController` formats the label text.

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
- Tools preview by moving and turning `ObjectItem`s
  (`ICanvasView::setPreviewGeometry`, `setPreviewRotation`) and drawing on
  the overlays (`setRubberBand`, `setGuides`); the model changes once,
  through a command, when the gesture ends. During a resize preview the item
  stretches the model's drawing to the preview size.
- `SelectionGeometry` computes frames, handle positions, handle hits,
  the rotation handle and resize results (opposite edge fixed, also when
  rotated). The overlay and
  `SelectTool` both use it, so what is drawn is what can be hit.
- When an object is removed, `ScreenItem` detaches its `ObjectItem` before
  deleting it, so the render thread never paints a destroyed object.
- Only `CanvasItem` accepts input. Object items are not interactive in edit
  mode; that keeps all interaction logic in tools rather than spread over
  items.

### 7.2 Input: mouse, touch and pen through one path

```
QQuickItem events (mouse, touch, wheel, key)
        │
        ▼
GestureRecognizer  →  PointerGesture { kind, device, view and scene positions,
        │                             start, delta, scaleDelta, angleDelta,
        │                             modifiers, afterLongPress }
        ▼
ToolController  →  navigation (pinch, pan, wheel) for every tool
                →  current ITool  →  preview on items/overlay  →  QUndoCommand at the end
```

`CanvasItem` calls `setAcceptTouchEvents(true)` and overrides `touchEvent`,
the mouse event handlers, `wheelEvent` and the key handlers, and adds scene
positions to each gesture. Holding Space turns a left drag into a pan.
Escape first cancels a gesture in progress (`DragCancel`) and reaches the
tool only when nothing is in progress. Tools never see raw events, only
gestures, so each tool works with every input device. The recognizer tracks
each finger by point id, because positions of stationary touch points are
not reliable.

| Gesture | Mouse and keyboard | Touch |
|---|---|---|
| Select | Click | Tap |
| Add to or remove from selection | Shift+click | Tap with multi-select mode on, or long-press the object |
| Rubber-band select | Drag on empty area; dragging right selects enclosed objects, dragging left also touched ones | Long-press on empty area, then drag |
| Move object | Drag object | Drag object |
| Resize | Drag handle | Drag handle (44 px touch target) |
| Rotate | Drag the rotation handle; Shift for 15° steps | Drag the rotation handle |
| Pan | Wheel, Space+drag, middle button, Pan tool | Two-finger drag; one-finger drag on empty area; Pan tool |
| Zoom | Ctrl+wheel; toolbar − and +; Ctrl+0 fit; Ctrl+1 100 % | Pinch, centred between the fingers; toolbar |
| Context menu | Right-click | "More" button in the selection toolbar; long-press is used for multi-select |
| Place new object | Drag from toolbox, or click a tile then click or drag on canvas | Drag from toolbox, or tap a tile then tap or drag on canvas |
| Cancel | Escape | A second finger landing during a drag |
| Nudge | Arrow keys (Shift: 10 units); consecutive nudges are one undo step | Arrow buttons in the selection toolbar |
| Snap | On by default (top bar toggle); Alt suspends it while dragging | Toggle in the top bar |

Thresholds come from `InputSettings`, in view pixels:

| | Mouse | Touch | Pen |
|---|---|---|---|
| Drag starts after | 4 | 12 | 6 |
| Hit tolerance | 3 | 12 | 5 |
| Handle hit square | 12 | 44 | 20 |

A long press is 500 ms (touch and pen only; the mouse uses right-click). A
double tap must follow within 400 ms and 24 px.

Snapping (`Snapper`, Strategy pattern) moves an edge or centre of the
dragged rectangle onto the grid or onto another object's or the screen's
edges and centres, within 6 view pixels for the mouse and 12 for touch.
Object targets win ties with the grid and draw guides. Targets are collected
and sorted once when a drag starts, so each move is a binary search.
Rotation snaps to 45° within 3°, and to 15° steps with Shift.

When the rotation handle and a resize handle are both under a finger, the
nearer one wins.

Touch additions outside the canvas: a floating selection toolbar (delete,
duplicate, group or ungroup, bring forward, send backward, nudge arrows and
"more" for the context menu) above the selection, shown in the touch
layout, undo and redo always visible, numeric fields that open an on-screen keypad, and all controls at
least 44 px high when the UI is in touch density. `Theme` exposes a density
setting (compact for mouse, comfortable for touch): comfortable when a touch
screen is attached at start-up, switchable with the Input button in the top
bar, and saved in the settings.

Hit testing: `HitTester` in `hmi_core` walks the objects of a screen from
top to bottom and tests `hitShape()` with a tolerance that depends on the
device (larger for touch). A spatial index is added only if profiling at
2,000 objects shows a need.

### 7.3 Panels

Every panel is a C++ model plus a short QML view.

| Panel | C++ | QML view |
|---|---|---|
| Toolbox | `ToolboxModel` (from `ObjectRegistry`), `ToolboxDragController` (fed by each tile's `DragHandler`; mouse and touch) | `GridView` of tiles; not flickable, so dragging a tile places an object; a scroll bar scrolls |
| Properties | `PropertyModel`: rows from the descriptors every selected object shares, with a mixed flag, or the screen's name, size and background when nothing is selected; `setValue` and friends push `SetPropertyCommand` or `ChangeScreenCommand`; edits between `beginEdit` and `endEdit` merge into one step | `ListView` with sections per category and a `DelegateChooser` on the `editor` role (number with steppers and a touch keypad, text, colour, font, enum, bool, point, size) |
| Layers | `LayerModel`: a flat list, topmost first, with expandable groups shown by depth | `ListView`; a drag handle reorders, so the rest of the row still scrolls the list by touch |
| Screens | `ScreenListModel`; actions on `EditorController` | Tab buttons over the canvas with a rename, duplicate and delete menu |
| Tags | `TagModel`, `TagFilterProxy` | `TableView` |
| Output | `MessageModel` filled by `ValidationVisitor` | `ListView` |

`EditorController` exposes actions as invokables (`undo`, `redo`,
`selectTool`, `panTool`, `createTool(key)`, zoom actions, and the editing
actions `cut`, `copy`, `paste`, `duplicate`, `deleteSelection`, `group`,
`ungroup`, `nudge`, `align`, `distribute`, `matchSize`, `reorder`; later
`simulate`) and display state as properties (`canUndo`, `canGroup`,
`canPaste` and the other `can…` flags, `zoomText`, `cursorText`,
`selectionText`, `sizeText`, one `…ToolActive` bool per tool,
`createType`). The editing actions live in `EditActions`, a plain C++ class
that each pushes one undo step, so keyboard shortcuts, the context menu and
the touch toolbar run the same code. Text is formatted in C++ so QML only binds. `Main.qml` lays
the panels out in a `SplitView` and sets the Controls palette from `Theme`.
`AppSettings` (QSettings) keeps the density, which panels are shown, the
split sizes and the recent projects.

`DocumentController` handles new, open, save, save as, recent projects,
the unsaved-changes question, autosave and recovery. Actions that would
lose changes ask first: it emits `confirmRequested` and continues on
`saveChanges`, `discardChanges` or `cancelPending`. It asks QML for file
names with `openDialogRequested` and `saveAsDialogRequested`; QML only
shows the dialogs (`DocumentDialogs.qml`). "Modified" is the undo stack
not being clean.

### 7.4 Simulation mode and runtime

Simulate (`SimulationController`) works like TIA Portal's Runtime
Simulator: `TagEngine` starts on the project's tags, `BindingEvaluator`
makes a play copy of the current screen in a project of its own, and a
separate simulator window shows that copy in a `RuntimeView` (a
`ScreenItem` fitted into the window, without editor overlays). The
evaluator sets the copy's properties from tag values through each
binding's transform; the copy's own `DocumentNotifier` reports the
changes, so `ObjectItem`s repaint exactly as when editing, while the
document, the undo stack and saved files never see a simulated value. The
editor stays editable; Restart takes a new copy. Closing the window
stops the simulation. From phase 7, input in play mode
goes to `RuntimeInputHandler`, which maps taps and drags to object events
(button press, slider drag).

The runtime executable is the same `RuntimeView`, `ScreenItem` and
`ObjectItem` inside a minimal QML window, with no editor code linked.

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
    using Publish = std::function<void(const QString& tag, const TagValue& value)>;
    virtual ~IDriver() = default;
    virtual Result<void> start(std::span<const Tag> tags) = 0;
    virtual void stop() = 0;
    virtual Result<void> write(const QString& tag, const TagValue& value) = 0;
    // Every TagEngine tick: report the values that changed.
    virtual void poll(double seconds, const Publish& publish) = 0;
};
```

Connection settings and addresses (`Tag::connection`, `Tag::address`) are
read by the driver from the tags it is started with; PLC drivers from
phase 8 may extend the interface with subscriptions.

- Drivers run on a worker thread owned by `TagEngine` and are polled every
  `TagEngine::kTickMs` (50 ms). The GUI thread never blocks on I/O.
- Values reach the GUI thread through queued calls and are announced at
  most every `TagEngine::kPublishMs` (66 ms, about 15 Hz) as one
  `valuesChanged` with the names that changed, so a fast PLC cannot flood
  repaint. A write from the GUI is shown at once and sent to the driver.
- `TagValue` is a `std::variant` of nothing, bool, integer, real and text;
  `Tag::converted` turns any value into the tag's type.
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
| Tests | Qt Test | LGPL | Qt Quick Test only if QML-level tests become necessary |
| JSON | `QJsonDocument` | part of Qt | No extra dependency needed |
| SVG rendering | Qt SVG | part of Qt (LGPL) | Default backend, see section 14 |
| SVG rendering, fuller standard support | lunasvg or resvg | MIT / MPL 2.0 | Optional second backend |
| On-screen keyboard | Qt Virtual Keyboard | GPL/commercial | Only if the OS keyboard is not enough; check licence first |

Libraries are fetched with CMake `FetchContent` in `cmake/Dependencies.cmake`
(today only tl::expected, pinned by version and hash). Qt comes from the Qt
installer. Conan was tried for CI only, because ConanCenter has prebuilt Qt
6.12 binaries; that work is parked on the local branch `wip/conan-ci`.
vcpkg was ruled out: it builds Qt from source and was one minor version
behind when checked. Qt Quick, Qt Quick Controls and Qt SVG are LGPL, which
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

- JSON, pretty-printed, keys sorted, so diffs are readable and saving an
  unchanged project writes byte-identical files.
- `project.json` lists the screen files in screen order. Screen files are
  named `screens/<nn>_<name>.json`; renamed or deleted screens' old files
  are removed on save. Files the project does not list (the user's own
  files) are never touched. `tags.json` holds the tag database and is
  always written; a project without one has no tags. Bindings are stored
  on their objects in the screen files, as `"bindings": [{property, tag,
  transform: {kind, …parameters}}]`.
- Every file carries `formatVersion`. `ProjectSerializer` runs a chain of
  migration functions from the file's version to the current one and
  refuses files from a newer version. Version 0 is a file without
  `formatVersion`, from before the format was versioned.
- Files are written with `QSaveFile` (temporary file and rename), screens
  first and `project.json` last.
- Objects serialize as `{ "type": "tank", "id": "...", "props": { ... },
  "children": [...] }` driven by descriptors, so a new object type needs no
  serializer code. Colours are `"#AARRGGBB"`, points and sizes two-number
  arrays, fonts `QFont::toString()`, enums the option key.
- Unknown types, properties and keys are kept and written back unchanged
  (section 5.1).

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

- Warnings as errors: `/W4 /permissive-` or `-Wall -Wextra -Wpedantic
  -Wshadow -Wnon-virtual-dtor -Woverloaded-virtual`
  (`cmake/CompilerWarnings.cmake`).
- `Q_ASSERT` stays on in Debug builds even with a release-only Qt kit, where
  CMake 4.2+ (policy CMP0199) would otherwise define `QT_NO_DEBUG`
  (`QT_FORCE_ASSERTS` for Debug in `cmake/CompilerWarnings.cmake`).
- AddressSanitizer and UBSan on the Linux debug preset.
- clang-format (Allman braces), clang-tidy and `qmllint` (the `all_qmllint`
  target) run locally for now.
- Every module has unit tests. Commands are tested for do, undo, redo leaving
  the model identical. Tools are tested by feeding synthetic
  `PointerGesture`s, once as mouse and once as touch.
- The gesture recognizer and canvas items are tested offscreen in a
  `QQuickWindow` with `QTest` mouse and touch helpers and pixel checks. The
  offscreen platform uses the software renderer, so these tests also cover
  machines without a GPU.
- `EditorShellTest` loads the real `Main.qml`, fails on any warning that
  names a `.qml` file, and saves a screenshot when `HMI_SCREENSHOT` is set.
- CI (Windows and Linux on every push) is deferred until the application is
  further along; see PLAN.md.

## 13. Main risks

| Risk | Mitigation |
|---|---|
| Scope: a WinCC-class editor is years of work | PLAN.md delivers a usable editor with a small object set first |
| The canvas (selection, hit testing, handles, gestures) is all custom code | It is the largest item in phases 2 and 3, with its own tests; logic sits in `hmi_core` and tools, not in items |
| One texture per `QQuickPaintedItem` uses a lot of GPU memory on large screens | Measured at 2,000 objects (`LargeScreenTest`); only objects near the view render above 1:1 (section 14.5) |
| Blurry objects after zooming, since painted items are textures | Re-render at the new scale when zoom settles (section 14.5) |
| Logic leaking into QML over time | Rules in section 2.1, `qmllint`, review checklist in SKILL.md |
| Fine editing with fingers is imprecise | Larger hit tolerance, numeric fields, nudge buttons, zoom-to-selection |
| S7-1500 access needs PUT/GET enabled or OPC UA | Make OPC UA the first real driver |
| An imported SVG uses features Qt SVG cannot draw | Import check, optional second backend, rasterize fallback (section 14.3) |
| No GPU on the target, so Qt Quick uses the software renderer, which ignores custom geometry nodes | Overlays use rectangle and transform nodes only; offscreen tests run on that renderer |

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
hmi_core
AssetStore              the project's assets in memory, thread-safe
 └─ Asset               id (content hash), original file name, suffix, bytes
GraphicStyle            overrides by SVG element id (fill, stroke, hidden)
IGraphicProvider        draw(painter, asset id, target, fit, style); reached
                        through PaintContext::graphics

hmi_render
GraphicLibrary          IGraphicProvider: decodes each asset once, renders it
                        at the pixel size it is shown at, caches the result
IGraphicSource          render(QPainter&, QRectF target, const GraphicStyle&)
 ├─ RasterGraphic       QImageReader, keeps the decoded QImage
 └─ SvgGraphic          holds the SVG bytes, renders through ISvgBackend
ISvgBackend             Strategy: which library draws SVG
 └─ QtSvgBackend        QSvgRenderer
GraphicCache            QImage cache keyed by (asset id, pixel size, style
                        hash); the pixel size includes zoom and device pixel
                        ratio; least recently used first out of a 64 MiB budget
GraphicFactory          picks RasterGraphic or SvgGraphic from the content
SvgInspector            import check and rasterizer
```

- `GraphicView` stores only an asset id, a fit mode (stretch, fit, fill,
  natural size) and style overrides; `PaintContext::graphics` draws it.
  Without a graphic it draws a placeholder.
- `AssetStore` keys an imported file by its content hash, so an image used
  on ten screens is stored once. The project serializer writes only assets
  some object uses, as `assets/<hash>.<suffix>`, and reports an asset file
  whose content no longer matches its hash.
- The cache holds `QImage`, not `QPixmap`, because painting happens on the
  render thread. `AppContext` owns the `GraphicLibrary`; the canvas hands it
  to every `ObjectItem`.
- A second SVG backend (for example LunaSVG behind a CMake option) has not
  been needed yet; `ISvgBackend` is the place for it.

### 14.3 Unsupported SVG content

On import, `SvgInspector` scans the file for elements the backend cannot
draw (for example `<filter>`, `<foreignObject>`, `<script>`). The
properties panel then asks: import anyway, or rasterize to a PNG at the
graphic's natural size.

### 14.4 Dynamic SVG symbols

- **Colour and visibility by element id.** A `GraphicStyle` holds
  overrides, written in the graphic view's "styleOverrides" property as
  `body: fill #3B6FD4; alarm-ring: hidden`. `SvgGraphic` applies them by
  rewriting the document with `QXmlStreamReader`/`Writer` (an attribute
  replaces the same key in a `style` attribute), keeps up to 32 styled
  variants parsed, and the cache keys images by the style hash. Bindings
  will write into the property in phase 6, so a tag can drive any named
  part of an imported symbol.
- **Per-element rendering.** `QSvgRenderer::render(painter, elementId,
  bounds)` can draw one named element, so a part could have its own
  rotation; not used yet.

Built-in process symbols stay as C++ painter code because they need
continuous animation (tank level, bar fill).

### 14.5 Rendering quality and speed in Qt Quick

A `QQuickPaintedItem` paints into a texture that the scene graph then
composites. Consequences and how they are handled:

- **Sharpness when zooming.** While a pinch or wheel zoom is in progress the
  existing textures are scaled by the GPU (fast, slightly soft). When the
  zoom settles, `ObjectItem` sets its texture size to item size × zoom ×
  device pixel ratio and repaints, so the result is sharp.
- **Texture size limit.** The texture side is capped at 4096 px
  (`ObjectItem::kMaxTextureSide`); beyond that an item stays at the cap and
  is scaled.
- **Repaint cost.** Only items whose model changed call `update()`. Panning
  and zooming move textures without repainting anything.
- **Memory with many objects.** Only objects within 256 view pixels of the
  view get textures above 1:1 when the view settles; the rest render at
  scene size. Panning restarts the settle timer, so objects brought into
  view sharpen 150 ms after the view stops. `LargeScreenTest` (2,000
  objects, 1280 × 800, software renderer) measured, before and after:
  object textures at 800 % 293 MiB → 11 MiB (budget 64 MiB); at 100 %
  5 MiB; pan and zoom frames about 15 ms; selecting all 2,000 objects
  1.5 ms and a 19 ms frame (the overlay draws one frame around a selection
  of more than 200 objects); a hit test about 1 ms. If a later object type
  needs more, two further fallbacks remain: draw basic shapes with
  scene-graph nodes instead of textures (keeping the painted path for the
  software renderer), or evaluate Qt Canvas Painter (Qt 6.12).
- **Antialiasing.** `setAntialiasing(true)` on painted items; overlay lines
  are axis-aligned one-pixel rectangles snapped to whole pixels.
- **Level of detail.** `PaintContext` carries the current zoom; below a
  threshold objects skip small text and tick marks.
- **Icons.** Toolbar and panel icons are single-colour SVGs recoloured from
  the theme. Toolbox icons are two-tone filled SVGs (blue body, light
  outline, green, red or yellow only where they carry meaning) drawn in
  their own colours, larger than toolbar icons, in the spirit of the TIA
  Portal toolbox but drawn for this project.

### Runtime operator input

`HmiObject::runtimeBehavior()` advertises semantic capabilities; object-local
`runtimeValueAt()` maps local positions to slider values or selector rows.
`RuntimeInputHandler` in `quick/` interprets `PointerGesture` and keyboard
input without switching on concrete types. It writes only through the
`TagEngine` validated input API. `Transform::inverse()` permits direct and
invertible linear bindings; arbitrary ranges and expressions remain read only.
`BindingEvaluator` fans out tag updates through nested groups and announces
changes on their containing top-level item. The runtime sets `editing=false`
on `ScreenItem`/`ObjectItem`, while the editor retains its normal presentation.

`hmi_quick` depends on `hmi_comm` for reusable runtime input; `hmi_comm` still
knows only core model types. `SimulationController` owns the handler and drops
its screen reference, pending dialog and held command before replacing the
play copy or stopping the engine. Button operations are ordinary descriptor
properties and use existing property commands and JSON serialization.

### Events and actions

Every `HmiObject` has an `EventTable`: an `ActionList` for each of Click,
Press and Release (`core/model/Events.h`). Actions are plain data (Set tag,
Toggle tag, Increment tag, Open screen), saved by `ObjectJson` under
`"events"`; unknown actions and event shapes are kept for newer versions.
The editor edits them through `EventModel` (the Events tab) and
`SetActionsCommand`. At runtime `RuntimeInputHandler` runs the lists in
order through `TagEngine::writeChecked`; Release also runs when a press is
cancelled. Open screen is a signal, queued to `SimulationController`, which
replaces the play screen after the list has finished. Show popup gives the
popup screen a second `BindingEvaluator` play copy and `RuntimeInputHandler`
in the controller; the simulator window shows it in its own `RuntimeView`
above the dimmed screen, which takes no input while the popup is open. `TagUsage` and
`ChangeTagCommand` include tags named by actions.

The explicitly saved `Project::simulationProfile()` chooses ordinary tag
simulation or the buffer-filling training model. `FillingStationDriver` runs
on the existing worker boundary, never opens a device connection and resets
its process state on restart. It belongs to the sample project, so it lives
in `editor/app/` next to it, not in `hmi_comm`. Native PLC monitoring remains
read only, and loads a native library named by a project only from a path
the user chose on this computer (`AppSettings::trustedLibraries`).

Tag minimum and maximum are the simulation and display range. They limit
operator input only for tags with `limitInput`, like the optional limits of
a WinCC tag.
