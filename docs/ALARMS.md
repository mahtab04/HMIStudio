# HMI alarms

Alarms follow WinCC Unified (TIA Portal): discrete and analog alarms, alarm classes with a state machine and colours, an alarm control on screens, and acknowledgment at runtime. Sources: [alarm control](https://docs.tia.siemens.cloud/r/en-us/v21/configuring-alarms-rt-unified/configuring-the-alarm-control-rt-unified/configuring-the-alarm-control-rt-unified), [alarm states](https://docs.tia.siemens.cloud/r/en-us/v21/configuring-alarms-rt-unified/basics-rt-unified/alarm-states-rt-unified), [alarm classes](https://docs.tia.siemens.cloud/r/en-us/v21/configuring-alarms-rt-unified/basics-rt-unified/alarm-classes-rt-unified), [acknowledgment model](https://docs.tia.siemens.cloud/r/en-us/v21/configuring-alarms-rt-unified/basics-rt-unified/acknowledgment-model-rt-unified), [analog trigger](https://docs.tia.siemens.cloud/r/en-us/v21/configuring-alarms-rt-unified/configuring-alarms-rt-unified/configuring-analog-alarms-rt-unified/configure-trigger-rt-unified).

## Configuring alarms

Open the **HMI alarms** tab (next to HMI tags). It has four pages: Discrete alarms, Analog alarms, Alarm classes and Runtime settings.

**Discrete alarms** watch one bit of a trigger tag.

| Column | Meaning |
|---|---|
| ID | The alarm number, unique in the project |
| Name, Alarm text | The text may show values (see *Alarm parameters*): `Level {1} % too high` |
| Alarm parameters | Up to ten tags, separated by `;`, whose values the text shows as `{1}` to `{10}` |
| Alarm class | Appearance, acknowledgment and priority (see below) |
| Trigger tag, Trigger bit | The tag and its bit, 0 to 63 (0 for a Bool tag) |
| Trigger mode | **On rising edge**: the alarm comes when the bit goes to 1. **On falling edge**: when it goes to 0 |
| Origin, Info text | Shown in the alarm control's columns |

**Analog alarms** compare a trigger tag with a limit. Mode is **Less**, **Greater**, **Equal**, **Not equal**, **Less or equal** or **Greater or equal**, as in WinCC Unified.

**Alarm classes** set the state machine, the priority (0 to 16) and, for each of the four statuses Incoming, Incoming/Outgoing, Incoming/Acknowledged and Incoming/Outgoing/Acknowledged, the background, text colour and flashing. The predefined classes cannot be renamed, deleted or given another state machine; their colours and priority can be changed:

| Class | State machine |
|---|---|
| Alarm | Alarm with simple acknowledgment (red, flashing while incoming) |
| Warning | Alarm with simple acknowledgment (yellow) |
| Notification | Alarm without acknowledgment |
| AlarmWithReset | Alarm with acknowledgment and confirmation |
| WarningWithReset | Alarm with acknowledgment and confirmation |

Add your own classes with **Add**; a class used by alarms cannot be deleted, and renaming it renames it in its alarms. Every edit can be undone. Alarms are saved in `alarms.json`.

## Alarm parameters

As WinCC Unified's parameter fields, an alarm text shows values when the alarm changes status:

| Field | Shows |
|---|---|
| `{1}` … `{10}` | The value of the alarm's 1st … 10th alarm parameter |
| `{Level}` | The value of the tag `Level` |
| `{1:.2}` | With 2 decimals (reals show 1 by default) |
| `{1:6}` | At least 6 characters, right-aligned |
| `{1:-6}` | At least 6 characters, left-aligned |
| `{1:06.1}` | Leading zeros: `0042.5` |

The format is `[0][-][width][.decimals]` after a colon. A field naming no parameter or tag is shown as written. WinCC Unified sets these options in a dialog ("Insert parameter field"); here they are typed. Text lists are not supported.

## Runtime settings

As WinCC Unified's *Runtime settings > Alarms*:

- **Status texts** for Normal, Incoming, Incoming/Outgoing, Incoming/Acknowledged, Incoming/Outgoing/Acknowledged and Incoming/Acknowledged/Outgoing. The alarm control's Status column and the exports show them. An empty value, or the default text, means the default.
- **Shelving timeout**: seconds after which a shelved alarm is unshelved by itself; 0 keeps it shelved until unshelved. The default is 3600 (one hour). WinCC Unified's documentation does not give its default; this one is our choice.

## Statuses

| State machine | Status texts, as WinCC Unified |
|---|---|
| Without acknowledgment | Incoming while the condition holds, then Normal |
| Simple acknowledgment | Incoming; Incoming/Outgoing when it goes unacknowledged; Incoming/Acknowledged when acknowledged while active; Normal once acknowledged and gone |
| Acknowledgment and confirmation | As above, then Incoming/Outgoing/Acknowledged or Incoming/Acknowledged/Outgoing until **Single confirm**; then Normal |

An alarm that comes again while still pending comes anew (Incoming). Normal alarms are no longer pending. Every status change is logged (the newest 1000).

The log is kept across runs: the simulator writes it, per project, to `runtime/alarm-logs/` in the application's local data folder, next to the autosave folder, and the next run starts with it. Shelved and disabled alarms are not kept: they are back to normal when the runtime starts again, as WinCC Unified's.

## The alarm control

Place an **Alarm view** from the toolbox. **Columns** lists the columns in order, separated by `;`: Alarm number, Date, Time, Alarm class, Alarm text, Status, Info text, Priority, Origin. At runtime it lists the pending alarms, the newest first, in the colours of their class and status. The toolbar, as WinCC Unified's:

| Button | Function |
|---|---|
| Show active alarms | The pending alarms, shelved ones left out |
| Show logged alarms | The log, the newest first; it updates as alarms change |
| Alarm statistics - view | The statistics (below) |
| Show defined alarms | Every configured alarm with its status: enabled, shelved or disabled |
| First line, Previous line, Next line, Last line | Move the selection |
| Single acknowledgment | Acknowledges the selected alarm |
| Acknowledge visible alarms | Group acknowledgment of the visible alarms |
| Single confirm | Confirms an acknowledged, gone alarm with reset |
| Shelve alarm | Shelves the selected alarm (in the active or defined alarms) |
| Unshelve alarm | Unshelves it (in the defined alarms) |
| Disable alarm / Enable alarm | In the defined alarms |
| Export | Writes the list shown to a CSV file |
| Alarms - show current | While on, the newest alarm is selected; moving the selection turns it off |

Tapping a line selects it (and turns off Alarms - show current). Buttons that cannot act on the list or the selected line are grey. The toolbar also shows how many alarms are pending and how many await acknowledgment (shortened, or left out, when it has no room).

### Shelving

A shelved alarm keeps running through its statuses and is logged, but it is out of the active alarms, the counts and the simulator's alarm line, and it is not acknowledged by "Acknowledge visible alarms". The defined alarms show it greyed as "Shelved" (with its status while pending). It comes back when unshelved, or by itself after the shelving timeout.

### Disabling

A disabled alarm is removed from the lists and is neither raised nor logged. When enabled again it is checked: if its condition holds, it comes (Incoming).

### Alarm statistics

Computed from the log, as WinCC Unified's, per alarm that came at least once: **Frequency** (how often it came), and the sum and average, in seconds, of the time from coming to coming again (**A-A**, active-active), to going (**A-I**, active-inactive) and to its acknowledgment (**A-Ack**, active-acknowledged). Going and acknowledgments count only after a coming. Since the log is kept across runs, so are the statistics, up to the newest 1000 log entries. WinCC Unified's time range setup ("Alarm statistics - setup") is not there yet.

### Export

**Export** writes the list shown (active, logged, statistics or defined alarms) to a CSV file, UTF-8 with a byte order mark so spreadsheets read it: `ActiveAlarms_<date>_<time>.csv` and so on, in `runtime/exports/`. The simulator shows where the file went. WinCC Unified's "Export data" dialog (file name, scope) is not there; the whole list is exported.

## The simulator's alarm line

While alarms await acknowledgment, the simulator shows the newest one at the top, in its class colours, with the number of others and an **ACK** button that acknowledges it. The screen moves down while the line shows.

## The sample

The buffer-filling sample has five alarms: T-101 level high (≥ 85 %, Warning) and critical (≥ 90 %, Alarm), the high-level interlock (HighLevel, AlarmWithReset), line pressure high (> 3.5 bar, Warning) and P-101 running (Notification). Three use alarm parameters, for example "Line pressure high: {1:.2} bar at {2:.0} % level" with Pressure and Level. The 01 Overview screen has an alarm control.

## Not yet

Operator authorizations for the buttons, comments, the alarm annunciator, filters and sorting, text lists in parameter fields, the statistics time range and the export dialog are not there yet.
