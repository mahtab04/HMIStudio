# HMI alarms

Alarms follow WinCC Unified (TIA Portal): discrete and analog alarms, alarm classes with a state machine and colours, an alarm control on screens, and acknowledgment at runtime. Sources: [alarm control](https://docs.tia.siemens.cloud/r/en-us/v21/configuring-alarms-rt-unified/configuring-the-alarm-control-rt-unified/configuring-the-alarm-control-rt-unified), [alarm states](https://docs.tia.siemens.cloud/r/en-us/v21/configuring-alarms-rt-unified/basics-rt-unified/alarm-states-rt-unified), [alarm classes](https://docs.tia.siemens.cloud/r/en-us/v21/configuring-alarms-rt-unified/basics-rt-unified/alarm-classes-rt-unified), [acknowledgment model](https://docs.tia.siemens.cloud/r/en-us/v21/configuring-alarms-rt-unified/basics-rt-unified/acknowledgment-model-rt-unified), [analog trigger](https://docs.tia.siemens.cloud/r/en-us/v21/configuring-alarms-rt-unified/configuring-alarms-rt-unified/configuring-analog-alarms-rt-unified/configure-trigger-rt-unified).

## Configuring alarms

Open the **HMI alarms** tab (next to HMI tags). It has three pages.

**Discrete alarms** watch one bit of a trigger tag.

| Column | Meaning |
|---|---|
| ID | The alarm number, unique in the project |
| Name, Alarm text | The text may show tag values: `Level {Level} % too high` |
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

## Statuses

| State machine | Status texts, as WinCC Unified |
|---|---|
| Without acknowledgment | Incoming while the condition holds, then Normal |
| Simple acknowledgment | Incoming; Incoming/Outgoing when it goes unacknowledged; Incoming/Acknowledged when acknowledged while active; Normal once acknowledged and gone |
| Acknowledgment and confirmation | As above, then Incoming/Outgoing/Acknowledged or Incoming/Acknowledged/Outgoing until **Single confirm**; then Normal |

An alarm that comes again while still pending comes anew (Incoming). Normal alarms are no longer pending. Every status change is logged (the newest 1000).

## The alarm control

Place an **Alarm view** from the toolbox. **Columns** lists the columns in order, separated by `;`: Alarm number, Date, Time, Alarm class, Alarm text, Status, Info text, Priority, Origin. At runtime it lists the pending alarms, the newest first, in the colours of their class and status. The toolbar, as WinCC Unified's:

| Button | Function |
|---|---|
| Show active alarms / Show logged alarms | The pending alarms, or the log |
| First line, Previous line, Next line, Last line | Move the selection |
| Single acknowledgment | Acknowledges the selected alarm |
| Acknowledge visible alarms | Group acknowledgment of the visible alarms |
| Single confirm | Confirms an acknowledged, gone alarm with reset |
| Alarms - show current | While on, the newest alarm is selected; moving the selection turns it off |

Tapping a line selects it (and turns off Alarms - show current). The toolbar also shows how many alarms are pending and how many await acknowledgment.

## The simulator's alarm line

While alarms await acknowledgment, the simulator shows the newest one at the top, in its class colours, with the number of others and an **ACK** button that acknowledges it. The screen moves down while the line shows.

## The sample

The buffer-filling sample has five alarms: T-101 level high (≥ 85 %, Warning) and critical (≥ 90 %, Alarm), the high-level interlock (HighLevel, AlarmWithReset), line pressure high (> 3.5 bar, Warning) and P-101 running (Notification). The 01 Overview screen has an alarm control.

## Not yet

Alarm suppression and shelving, alarm statistics, export, user-configurable status texts, alarm parameters other than `{Tag}` values in the text, and the alarm log's persistence across runs are not there yet.
