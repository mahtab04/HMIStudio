# HMI tags and PLCs

A tag is a named value that screen objects bind to. An **internal** tag lives in the HMI (simulated while you test). A **PLC tag** names a PLC and an address in it; the runtime reads it from the PLC and writes it there when an operator changes it. This is the model most HMIs share (WinCC Unified's internal and external tags, Ignition's devices and tags, FactoryTalk's device shortcuts), kept to the few fields a working connection needs.

## Connecting a PLC in three steps

1. **HMI tags > Connections.** Enter a name, choose the **PLC type** and type its **IP address**, then **Test** (it checks that the PLC answers on the network) and **Save**.

   | PLC type | Port | Defaults |
   |---|---|---|
   | Siemens S7-1200 / S7-1500 | 102 | rack 0, slot 1 |
   | Siemens S7-300 / S7-400 | 102 | rack 0, slot 2 |
   | Modbus TCP | 502 | unit 1 |
   | Allen-Bradley (EtherNet/IP) | 44818 | CIP route 1,0 |

   A PLC on another port: add it to the address, `127.0.0.1:1102`. Rack and slot, the CIP route, the Modbus unit and the driver library wait under **More settings**; the defaults fit most PLCs.
2. **Add tags** with that PLC as their connection and an address. The data type follows the address:

   | PLC | Address | Data type |
   |---|---|---|
   | Siemens S7 | `DB1.DBX0.0`, `M0.0`, `I0.0`, `Q0.1` (a bit) | Bool |
   | | `DB1.DBB1`, `DB1.DBW2`, `MB3`, `MW10`, `IW64`, `QW0` | Int (byte unsigned, word signed) |
   | | `DB1.DBD4`, `MD20` | Real (choose Int for a DInt) |
   | Modbus TCP | `co0` (coil), `di0` (input) | Bool |
   | | `hr0` (holding register), `ir0` (input register) | Int; a Real uses two registers |
   | Allen-Bradley | `Motor.Running`, `Motor.Speed`, `Program:Main.Counter` | as the PLC's BOOL, DINT, REAL |

   S7 addresses may be written as TIA Portal does, with `%` (`%MW10`), and with German letters (`E`, `A`). Modbus addresses are zero based.
3. **Simulate** (F5). PLC tags are read every acquisition cycle (Cycle, default 500 ms) and written at once when an operator sets them; the value shows once the PLC has it. Internal tags are simulated as before. To test without a PLC, check **Simulate PLC tags** in HMI tags: PLC tags are then simulated like internal ones.

The Quality column reports **Simulated**, **Waiting** (no value from the PLC yet), **Good** or **Bad**, with the reason in its tooltip and the first problem above the table. A PLC that does not answer is retried on the next cycles. Changes to tags and connections are undoable; stop and start the runtime to use them.

## Testing without hardware

- **A test S7 PLC on this computer**: `tools/s7_test_plc.py` runs an S7 server ([python-snap7](https://github.com/gijzelaerr/python-snap7)) with DB1 and the markers, and changes a few values. Run `pip install python-snap7`, then `python tools/s7_test_plc.py`, add a PLC *Siemens S7-1200 / S7-1500* at `127.0.0.1:1102`, and tags `Level` (DB1.DBD0, a ramp), `Running` (DB1.DBX4.0, toggles), `Setpoint` (DB1.DBW6) and `Start` (M0.0); the script prints what the HMI writes. Port 102 needs administrator rights, hence 1102.
- **Siemens S7-PLCSIM Advanced** (licensed) simulates an S7-1500 that clients reach over TCP/IP; allow PUT/GET in its CPU.
- **Modbus TCP**: any Modbus TCP server or simulator, or a soft PLC such as OpenPLC.

## Driver libraries

The runtime talks to S7 PLCs through [Snap7](https://snap7.sourceforge.net/) and to Modbus TCP and Allen-Bradley PLCs through [libplctag](https://github.com/libplctag/libplctag), loaded when needed. Put `snap7.dll` and `plctag.dll` (Linux: `libsnap7.so`, `libplctag.so`), built for the same architecture as the application, beside the application. They are not part of this repository.

Loading a library runs its code, and a project file can come from anyone. So a library path stored in a project (More settings > Driver library) is loaded only after it was chosen with **Browse** on this computer; the choice is remembered in the local application settings, not in the project. The library beside the application is always allowed.

S7-1200/1500 CPUs answer only with **PUT/GET access** allowed (CPU properties > Protection & Security) and read **non-optimized** data blocks (block access); markers, inputs and outputs need neither. [Snap7 client compatibility](https://snap7.sourceforge.net/snap7_client.html)

## More

The tag workspace fills the window. The table selector filters a named tag table; search also matches addresses and connection names. Cycle is the acquisition interval in milliseconds, from 100 to 60,000.

String tags work internally and in simulation; PLC tags are Bool, Int or Real. Arrays, structures and strings on PLCs, Modbus word order, browsing the PLC's tags, OPC UA, and reconnection back-off are still to come.

CSV export includes `table`, `connection`, `address` and `acquisitionMs`. Connections are exported by name; on import, names matching configured connections are resolved to their ids, so configure the connections before importing PLC tags. An unknown connection stays visible as missing. The CSV schema is HMI Studio's own; TIA Portal or Studio 5000 exports need a dedicated importer.

Connections are stored in `project.json`, tag settings in `tags.json`. Older projects load unchanged.

The driver tests use a deterministic shared-library fixture: addresses, reads and writes, byte order, conversions, acquisition intervals, ports, missing libraries and failed reads. Validation against physical PLC hardware is still required for deployment.

## Runtime operation

Press **F5** to run the current screen. Bind the same process tag to every control or display that should share a value. Input writes the tag engine; bindings then update all linked objects, including children inside transformed groups. Runtime changes affect a play copy and never enter document undo or saved design values.

The implemented distinction follows Siemens' [overview of operable elements](https://docs.tia.siemens.cloud/r/en-us/v20/operating-unified-pc-rt-unified/elements-rt-unified/overview-of-elements-rt-unified) and its [set-value example](https://docs.tia.siemens.cloud/r/en-us/v20/configuring-screens-rt-unified/overview-of-screen-objects-rt-unified/elements-rt-unified/examples-rt-unified/example-set-values-rt-unified). Button operations are a focused subset of the documented [tag system functions](https://docs.tia.siemens.cloud/r/en-us/v20/simatic-visualization-architect/reference/supported-system-functions-for-faceplates). The [tutorial supplied in the conversation](https://www.solisplc.com/tutorials/wincc-unified) demonstrates shared values; the official documentation defines the input/display distinction.

| Object | Runtime interaction | Configure in Properties |
|---|---|---|
| I/O field | Tap to enter a number; Apply or Enter commits, Cancel discards | Process > Tag; Mode = Input or Input and output. Output mode is read only |
| Slider | Tap or drag along the track, horizontally or vertically; arrow keys adjust | Process > Tag; Minimum and Maximum |
| Switch, Check box | Toggle a Boolean tag | Process > Tag |
| Radio group, List box | Select an entry; Up/Down changes the selection | Process > Tag; Entries; tag stores the zero-based selected index |
| Symbolic I/O field | Open the selector and choose an entry | Process > Tag; Entries; tag stores the zero-based selected index |
| Button, Touch area | Run the actions of their Click, Press and Release events | Events tab: actions per event |
| Any object | Objects with event actions can be clicked, pressed and released too | Events tab |
| Gauge, Bar, Tank | Display a numerical tag | Process > Tag; display limits where available |
| Pump, Motor, Conveyor, Valve, Pipe | Display running/open/flowing Boolean state | Process > Tag |
| Sensor, Exchanger, Clock | Display their available bindable properties; no operator tag writes | Property link buttons where offered |
| Rectangle, Ellipse, Circle, Line, Polyline, Polygon, Text box, Graphic view | Display tag-driven appearance/text through bindable properties; no operator tag writes | Property link buttons |
| Alarm view | Lists the alarms raised by tag values; acknowledgment and confirmation by touch | See `docs/ALARMS.md` |
| Trend view, f(x) trend, Recipe view, Diagnostics view, Screen window, Browser, Media player | Existing display placeholders; no new tag input behavior | Their full runtime services remain future work |

### Events and actions

Select an object and open the **Events** tab of the properties panel. Each object has three events, as in WinCC: **Click** (pressed and released on the object), **Press** and **Release**. Each event runs a list of actions, in order:

| Action | Fields | What it does |
|---|---|---|
| Set tag | Tag, Value | Writes the value (`true`, `0`, `42.5` ...) |
| Toggle tag | Tag | Inverts a Bool tag |
| Increment tag | Tag, Step | Adds the step; a negative step decreases |
| Open screen | Screen | Shows another screen of the project in the simulator; tags keep their values |
| Show popup | Screen | Shows a screen as a popup over the current one, in a frame with its name and a close button; the screen behind is dimmed and takes no input until the popup closes |
| Close popup | - | Closes the popup (for a Close button inside the popup) |

For a Start button add **Set tag** on Click with the command tag and `true`; for Stop the same tag with `false`. For a hold-to-run button add **Set tag** `true` on **Press** and `false` on **Release**. Release actions also run when the press ends outside the object or is cancelled (Escape, a second finger, focus loss, simulation stop or restart), so a held command never sticks. A failing action is reported and the next ones still run. Open screen, Show popup and Close popup take effect after the rest of the list has run. A popup is an ordinary screen, usually smaller (the sample's "P-101 details" is 720 × 420); its objects, bindings and events work as on any screen, and an Open screen action from inside it closes it. Opening another screen, restarting or stopping also closes the popup.

Mouse and touch use the same gesture path. Tab selects an operating element, Enter/Space activates it, arrows operate sliders and row selectors, and Escape cancels input. Numeric entry has an on-screen keypad. Touch area outlines and hidden objects are omitted in the runtime. Hidden objects cannot be operated.

A tag's **Minimum** and **Maximum** are its simulation and display range. They limit operator input only when **Limit input** is Yes (in the table's simulation settings view), like the optional limits of a WinCC tag; the sample's `FlowSetpoint` has it on, so the operator cannot set more than 100 L/min.

Input accepts direct bindings and invertible linear scaling. Range maps, expression transforms and zero-span scales are display only and report an error on input. Invalid numbers, non-finite numbers, missing tags and out-of-range values are rejected with a visible explanation. The I/O field is currently numeric; String tags remain editable in the tag table. Slider movement writes continuously; cancelling ends movement without rolling back values already written. On PLC tags these interactions write the PLC.

Tags named by event actions take part in Where used ("Click event"), delete warnings, renaming and undo, including objects inside groups. Format 4 stores events under `"events": {"click": [...], "press": [...], "release": [...]}` in the screen files. Format 3 files are converted on load: a Set, Toggle or Increment operation becomes a Click action, and a Momentary operation becomes Set true on Press and Set false on Release.

## Buffer-filling training project

The built-in sample and `examples/BufferFilling.hmiproj` are an operable transfer station with separated command and feedback tags:

- **Start / Stop** set and clear `RunCommand`. `PumpRunning` is simulator feedback, driving the pump, valve, line and status text.
- **Auto** fills between 30% and 80% while the run command is enabled; downstream demand drains the tank while filling pauses. With Auto off the request runs continuously until the high-level interlock pauses it at 90%.
- **Hold to jog** uses `Jog`; releasing it removes the jog request. An active Start request continues independently. The high-level interlock also inhibits jogging.
- **FlowSetpoint** is shared by the editable I/O field, slider and gauge. The -5/+5 and 50 L/min preset buttons write the same tag. `Flow` is the separately simulated actual flow with a short response lag; pressure follows actual flow.
- **Level**, **Flow**, **Pressure**, **Temperature**, **PumpRunning** and **HighLevel** are generated measurements. Changing their start values is useful for test scenarios; writing a measurement during this simulation is overwritten by the process model.

The saved `simulationProfile: "filling-station"` explicitly selects `FillingStationDriver`; an empty profile uses the normal independent per-tag generators. The profile's tag names form its fixed demo interface. Keep those names when customizing this sample, or remove the profile to use ordinary tags. Saved profile behavior survives closing and reopening the project. PLC tags (with a connection) always go to their PLCs unless Simulate PLC tags is checked.

This is a deterministic training model of a pump and buffer tank, with bounded time steps and reset state on restart. It is not a PLC program or a safety function; its HMI alarms (`docs/ALARMS.md`) are operator information only. No physical outputs are written.
