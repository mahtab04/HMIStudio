# HMI tags and PLC monitoring

Use an HMI tag as the named value that screen objects bind to. Each tag has an HMI data type and belongs to a tag table. An internal tag holds its value in the HMI. An external tag selects a named PLC connection and an address within that PLC.

This workflow follows WinCC Unified's distinction between internal and external tags and its use of named tag tables for organization. Connection definitions belong to the project and have stable IDs, so renaming a connection preserves its tag assignments. [Siemens tag basics](https://docs.tia.siemens.cloud/r/en-us/v21/configuring-tags-rt-unified/basics-rt-unified/basics-of-tags-rt-unified), [Siemens tag tables](https://docs.tia.siemens.cloud/r/en-us/v20/configuring-tags-rt-unified/basics-rt-unified/overview-of-hmi-tag-tables-rt-unified)

1. Open **HMI tags** and choose **Connections** to configure a PLC endpoint.
2. Choose the driver, host, and its rack/slot, CIP route, or Modbus unit ID. Give the connection a recognizable name such as `LinePLC`.
3. Choose **Add** and enter the HMI tag name, data type, tag table, connection, and PLC address. Use **Internal** for a local HMI value.
4. Return to a screen tab and assign the tag to an object's Process property or another bindable property in the properties panel.
5. Use **Simulation settings** to configure start values and ramp, sine, random, or toggle test signals. The normal simulation action uses those settings for all tags.
6. Use **Monitor PLCs** to read configured PLC addresses. The monitor uses a copy of the screen, and PLC values appear in the Value column. Monitoring is read only in this implementation.

The tag workspace expands across the window. The table selector filters a named tag table. Search also matches addresses and connection names. Changes to tags and connections use the document's undo stack. Cycle is the requested polling interval in milliseconds, from 100 to 60,000. Acquisition is best effort and shares the communication worker across tags.

| Driver | Native library | Address examples | Scalar mapping |
|---|---|---|---|
| Siemens S7 | Snap7 | `DB1.DBX0.0`, `DB1.DBB1`, `DB1.DBW2`, `DB1.DBD4` | Bool uses DBX, Int uses byte/16-bit signed word/32-bit signed double word, Real uses 32-bit IEEE DBD |
| Allen-Bradley Logix | libplctag | `Motor.Running`, `Motor.Speed`, `Program:Main.Counter` | Bool uses BOOL, Int uses DINT, Real uses REAL |
| Modbus TCP | libplctag | `co0`, `di0`, `hr0`, `ir0` | Bool uses a coil/input bit, Int uses a signed 16-bit register, Real uses two registers with the library's default big-endian byte/word order |

String tags work internally and in simulation. External monitoring currently supports the scalar subset in this table. Configure the HMI type to match the PLC storage type. Arrays, structures, strings, selectable Modbus word order, native symbol browsing, PLC writes, and automatic reconnection backoff remain extension work.

For Snap7, place the matching `snap7` shared library beside the application or choose its absolute path in Connections.

Loading a native library runs its code, and a project file can come from anyone. So a library path stored in a project is loaded only after it was chosen with **Browse** in Connections on this computer; the choice is remembered in the local application settings, not in the project. A path that is only typed, or that arrives with a project from elsewhere, reports "loaded only after you choose it with Browse" in the Quality tooltip until it is chosen. The library beside the application is always allowed. Windows uses `snap7.dll` and Linux commonly uses `libsnap7.so`. The initial adapter reads absolute global DB locations. S7-1200/1500 access depends on the CPU configuration, including compatible PUT/GET access and non-optimized DB layout. [Snap7 client compatibility](https://snap7.sourceforge.net/snap7_client.html)

For Allen-Bradley and Modbus, use the matching `plctag` shared library, such as `plctag.dll` or `libplctag.so`, or select its absolute path. The Logix adapter supplies `plc=controllogix` and the configured CIP route. Modbus addresses are zero based, so `hr0` denotes the first holding register. A Real occupies two consecutive registers. Native binaries must match the application's architecture. [libplctag project](https://github.com/libplctag/libplctag), [native API](https://github.com/libplctag/libplctag/wiki/API), [driver attributes](https://github.com/libplctag/libplctag/wiki/Tag-String-Attributes)

Quality reports **Offline**, **Simulated**, **Waiting**, **Good**, or **Bad**. Missing connections and invalid addresses have configuration messages. The first active driver error is shown above the table, and cell tooltips expose the applicable error. Failed reads clear the tag engine's current value. Objects retain their last successfully applied property until a valid value arrives. PLC connection errors are retried on subsequent acquisition turns. Stop and start monitoring after changing native library settings or tag configuration to take a new runtime snapshot.

CSV export includes `table`, `connection`, `address`, and `acquisitionMs` in addition to the existing columns. Connections are exported by name. On import, names matching configured connections are resolved to their stable IDs. Configure the destination connections before importing external tags. An unresolved connection remains visible as missing. Existing CSV files with the earlier columns still import. The CSV schema is HmiStudio's schema, and vendor-native TIA/Studio 5000 export formats require a dedicated importer.

Project formats 2 and 3 store connection definitions in `project.json` and the additional tag settings in `tags.json`. Versions 0 and 1 load with the Default tag table, a 500 ms acquisition interval, and their existing simulation settings. The native libraries are loaded dynamically and are supplied separately.

OPC UA is a useful next integration for PLCs or gateways exposing an OPC UA server. Ignition uses the same broad separation between device connections and tags and can connect through OPC UA services. An OPC UA adapter can fit the existing worker/driver boundary, but requires endpoint security, certificates, subscriptions, quality mapping, and node browsing. These are still to be implemented here. [Ignition device drivers](https://www.docs.inductiveautomation.com/docs/8.3/ignition-modules/opc-ua/opc-ua-drivers), [open62541 client documentation](https://www.open62541.org/doc/master/client.html)

The native adapter tests load a deterministic shared-library fixture and cover address validation, native ABI calls, S7 byte order, scalar conversion, acquisition intervals, missing libraries, failed reads, and read-only behavior. Validation against physical PLC hardware is still required for deployment.

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
| Alarm view, Trend view, f(x) trend, Recipe view, Diagnostics view, Screen window, Browser, Media player | Existing display placeholders; no new tag input behavior | Their full runtime services remain future work |

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

Input accepts direct bindings and invertible linear scaling. Range maps, expression transforms and zero-span scales are display only and report an error on input. Invalid numbers, non-finite numbers, missing tags and out-of-range values are rejected with a visible explanation. The I/O field is currently numeric; String tags remain editable in the tag table. Slider movement writes continuously; cancelling ends movement without rolling back values already written. All native PLC adapters remain read only, so these interactions are available in **Simulation**, not **Monitor PLCs**.

Tags named by event actions take part in Where used ("Click event"), delete warnings, renaming and undo, including objects inside groups. Format 4 stores events under `"events": {"click": [...], "press": [...], "release": [...]}` in the screen files. Format 3 files are converted on load: a Set, Toggle or Increment operation becomes a Click action, and a Momentary operation becomes Set true on Press and Set false on Release.

## Buffer-filling training project

The built-in sample and `examples/BufferFilling.hmiproj` are an operable transfer station with separated command and feedback tags:

- **Start / Stop** set and clear `RunCommand`. `PumpRunning` is simulator feedback, driving the pump, valve, line and status text.
- **Auto** fills between 30% and 80% while the run command is enabled; downstream demand drains the tank while filling pauses. With Auto off the request runs continuously until the high-level interlock pauses it at 90%.
- **Hold to jog** uses `Jog`; releasing it removes the jog request. An active Start request continues independently. The high-level interlock also inhibits jogging.
- **FlowSetpoint** is shared by the editable I/O field, slider and gauge. The -5/+5 and 50 L/min preset buttons write the same tag. `Flow` is the separately simulated actual flow with a short response lag; pressure follows actual flow.
- **Level**, **Flow**, **Pressure**, **Temperature**, **PumpRunning** and **HighLevel** are generated measurements. Changing their start values is useful for test scenarios; writing a measurement during this simulation is overwritten by the process model.

The saved `simulationProfile: "filling-station"` explicitly selects `FillingStationDriver`; an empty profile uses the normal independent per-tag generators. The profile's tag names form its fixed demo interface. Keep those names when customizing this sample, or remove the profile to use ordinary tags. Saved profile behavior survives closing and reopening the project. PLC monitoring always selects the configured native adapters instead.

This is a deterministic training model of a pump and buffer tank, with bounded time steps and reset state on restart. It is not a PLC program, an alarm service or a safety function. No physical outputs are written.
