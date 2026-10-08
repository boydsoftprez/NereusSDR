# Remote rotor control version 1

This is the wire contract for the antenna rotor: the Core owns it, and every
window (a local window in-process, the desktop remote window, the iPhone
app) reads the same `rotor` object and asks the Core to act through the
same commands. It sits beside
[remote accessory control version 1](2026-09-23-remote-accessory-control-v1.md)
and uses its wire conventions unchanged (schema, `object.create`, `delta`,
`command.invoke` / `command.result`, refusals as reason text).

Design: [2026-10-07-rotor-control-design.md](2026-10-07-rotor-control-design.md).
Plan: [2026-10-07-rotor-control-plan.md](2026-10-07-rotor-control-plan.md).
Each plan task extends this document in the same commit that adds what it
describes; until then a section here is the agreed target, not shipped
behaviour.

**Status.** Shipped with plan Task 4b: Negotiation, the `rotor` object,
Commands, Tools catalogue and Refusals. Bearings on spots shipped with plan
Task 4a. The windows' own controls (desktop and iPhone) are later plan
tasks.

## Negotiation

| Capability | Session minor | Value | Meaning |
| --- | --- | --- | --- |
| `remoteRotorControlVersion` | 11 | 1 | The Core mirrors its rotor as the `rotor` object; every command below works; spots carry a bearing |

The Core advertises 1 when it owns a rotor connection, and 0 otherwise.
Two kinds of Core own one: the headless Core, `nereusd`, always does, and so
does a desktop running its own radio, for its own windows and for any phone
it hosts. A window on a remote Core never owns one; it reads that Core's
`rotor` object, and switching a desktop to a remote Core closes its own
rotor's port first. As with the accessory
capabilities, 0 is sent by leaving the capability out (absent reads as 0);
it follows `accessoryTxVersion` in the capabilities message. A window below
minor 11, or on a Core at 0, is not sent the `rotor` object. `kSessionProtocolMinor` does not
change. A window on a Core that advertises 0 shows its rotor controls
greyed with the reason "This Core does not control a rotor. Updating the
Core may help."

## The `rotor` object

Class `RotorModel`, key `rotor`. Sent to every app at minor 11 on a Core
that advertises `remoteRotorControlVersion` 1. Every property is the
Core's to report: a write to the object is refused with "The Core reports
its rotor here. Use the rotor controls to turn it or change its setup."
and changes nothing.

On the Core the class is `NereusSDR::RotorLink::RotorModel`
(`src/models/RotorModel.h`), because `NereusSDR::RotorModel` is already the
Hamlib model list entry; the wire names a class by its short name, so it is
`RotorModel` here. Where the rotor points (`spanPositionDeg`, `travelDeg`,
`routeKnown`, `positionFresh`, `azimuthDeg`, `elevationDeg`) changes in one
delta, everything else in another. The Core looks again at its serial ports
and for `rotctld` every 5 s (both are slow to ask; `configureRotor` always
checks the ports as they are), so `serialPorts` and `rotctldAvailable` can
lag a plugged-in adapter by that long. The 5 s is this design's choice,
not a device fact.

| Property | Kind | Meaning |
| --- | --- | --- |
| `connectionPhase` | enum | The accessory connection phase table (remote accessory control version 1, "Connection phase") |
| `connectionError` | utf8 | Why the last attempt failed; empty otherwise |
| `driver` | enum | Driver table below |
| `label` | utf8 | What the status line shows, for example "Easy Rotor Control on COM4" |
| `serialPort` | utf8 | The Core's serial port for a GS-232 driver; empty for rotctld |
| `baud` | i64 | Its baud rate |
| `host` | utf8 | rotctld host; empty for a GS-232 driver |
| `port` | i64 | rotctld TCP port (4533 by default) |
| `serialPorts` | utf8 | The serial ports the Core's computer has that a rotor could be on, one per line, so a phone can offer them in setup: USB serial adapters first, then the rest, each in name order; console ports (Rockchip's `ttyFIQ*`, the Linux kernel's `console=`) are left out |
| `axes` | enum | Axes table below |
| `rangeDeg` | i64 | 360, or 450 for a rotor with overlap |
| `endStop` | enum | End stop table below |
| `spanPositionDeg` | f64 | Where the rotor is on its span, degrees clockwise from its counter-clockwise stop, 0 to `rangeDeg`; -1 when it is in the overlap band and the Core cannot yet tell which end, or with no end stop. Measured on the controller's own reading, before `offsetDeg`: the end stop is where the controller's reading stops, so the span's compass is (stop + span) modulo 360 in the controller's frame, and adding `offsetDeg` gives the heading shown |
| `travelDeg` | f64 | The signed turn still to make to the target along the route the controller will take (negative is counter-clockwise); 0 with no target. Planned on the controller's span to the target with `offsetDeg` removed (the heading the Core sends); a window planning a selection itself does the same |
| `routeKnown` | bool | False while the span position is unknown with a target set; `travelDeg` is then 0 and a window draws no route |
| `offsetDeg` | f64 | Calibration offset added to the read heading for `azimuthDeg` and removed from a target before it is sent; never applied to `spanPositionDeg`, the end stop or the overlap |
| `hamlibModel` | i64 | Hamlib rotor model for driver 4 (404 is the ERC's own driver); 0 otherwise |
| `rotctldAvailable` | bool | The Core's computer has Hamlib's `rotctld` (needed for driver 4) |
| `positionFresh` | bool | The rotor answered a position read within the last 1500 ms. False means `azimuthDeg` and `elevationDeg` are the last heard values, not live |
| `azimuthDeg` | f64 | Current compass heading after the offset, 0 to under 360 (north reads 0); -1 when unknown. Where the rotor is in the overlap is `spanPositionDeg` |
| `elevationDeg` | f64 | Current elevation, 0 to 90; -1 on an azimuth rotor or when unknown |
| `targetAzimuthDeg` | f64 | The target being turned to; -1 when none |
| `targetElevationDeg` | f64 | The elevation target; -1 when none |
| `motion` | enum | Motion table below |
| `presets` | utf8 | The presets, one per line as `name<TAB>degrees`, in the operator's order |
| `fault` | utf8 | The last fault in plain words; empty otherwise |

Off `connected`, the Core sets the headings and targets to -1 and `motion`
to stopped.

A GS-232 rotor (drivers 1 and 2) is `connecting` while its serial port is
open and no position reply has parsed, and `connected` from the first one.
With no valid reply within 3 s the Core reports the fault "The rotor
controller on <port> is not answering. Check the serial port and the baud
rate." and dials again on the reconnect schedule (bench fix, 2026-10-08).

### Enum tables

`driver`: 0 none, 1 GS-232A (serial), 2 GS-232B (serial), 3 Hamlib rotctld
already running (TCP, `host` and `port`), 4 Hamlib rotctld started by the
Core (`hamlibModel`, `serialPort`, `baud`; the Core runs `rotctld` on its own
loopback and talks to it as driver 3).

`axes`: 0 azimuth, 1 azimuth and elevation.

`endStop`: 0 none (continuous rotation), 1 north, 2 south (after Longpath
`BeamHeading::Stop`).

`motion`: 0 stopped, 1 turning to a target, 2 nudging (a window is holding
a turn button).

Values are only ever appended (the accessory contract's rule).

## Commands

Each is a `command.invoke` with exactly these arguments and wire kinds. The
answer is a `command.result`; `accepted: true` means the Core sent the
command to the rotor, not that the rotor has arrived. Windows follow
`motion` and the headings for that.

| Command | Arguments | Effect |
| --- | --- | --- |
| `setRotorTarget` | `azimuthDeg` f64, `elevationDeg` f64 (-1 to leave elevation) | Turn to a heading |
| `turnRotorToCall` | `call` utf8, `longPath` bool | The Core works out the bearing from its cty.dat and the station's grid square, then turns as `setRotorTarget` |
| `stopRotor` | none | Stop now; sent ahead of anything queued |
| `nudgeRotor` | `direction` enum (0 CCW, 1 CW, 2 down, 3 up), `active` bool | `active` true starts or keeps a hold going; false ends it |
| `configureRotor` | `driver` enum, `serialPort` utf8, `baud` i64, `host` utf8, `port` i64, `hamlibModel` i64, `axes` enum, `endStop` enum, `rangeDeg` i64, `offsetDeg` f64 | Save the setup and (re)connect; driver 0 disconnects and forgets |
| `disconnectRotor` | none | Disconnect, keeping the setup |
| `setRotorPresets` | `presets` utf8 (as the property) | Replace the presets |

**Strict headings** (after Longpath `RotorPeilung.h`). The Core checks every
heading before anything reaches the rotor: a value that is not a finite
number is refused; a value below 0 or above 360 is refused, never
wrapped (someone who sends -90 made a mistake, and 270 is a different
answer); exactly 360 means north and is sent as 0. A target is a compass
heading; on a rotor with overlap the Core, like the controller, takes the
nearer of its two span positions. A
window applies the same rule to what it reads from a text box, so an empty
box never turns the antenna to north.

**A stale heading is never shown as live** (after Longpath
`RotorController.h`, `RotctldClient.h:76`). With `positionFresh` false a
window shows the last heading muted, with "Last heard Ns ago", and never as
a live needle.

**The hold dead man.** While a turn button is held, the window repeats
`nudgeRotor` with `active` true every 250 ms. If no repeat arrives for
750 ms, or that window's session ends, the Core sends stop. These two
numbers are this design's choice, not a device fact. A hold in the other
direction while one is under way (a reversal, or two windows holding
opposite ways) sends stop only; the next repeat starts the new direction.

**Who may turn the rotor.** Any window allowed to change station
accessories (the same admission as the accessory settings commands: a
window at minor 11 on a Core that advertises the capability). Stop goes
through the same admission. No rotor command is a shared setting, so a
turn asks no other window to confirm. The
rotor turns while the radio is on the air (JJ, 2026-10-07): it switches no
RF path, unlike the amp and tuner controls that wait.

## Bearings on spots

With `remoteRotorControlVersion` 1, every spot the Core serves gains
`bearingDeg` (f64): the short-path initial great-circle bearing from the
station's grid square to the spot's cty.dat entity, 0 to under 360, or -1
when the Core has no grid square or cannot place the callsign. Long path is
`bearingDeg + 180` modulo 360, worked out by the window.

Placement (plan Task 4a): `bearingDeg` is one more field of each upsert in
the `spots` record stream, beside `dxccColour` and `dxccPriority` (the `spots` row of the link
document's record stream table), built by
`SpotSourceHost::spotRecordFields`. It is present on every spot record a
Core of this build serves, whatever the window's capabilities; a window
reads it only when the Core advertises `remoteRotorControlVersion` 1 (plan
Task 4b advertises the capability) and treats it as -1 otherwise.

- The station's grid square is the one FreeDV Reporter uses
  (`FreeDvReporter/GridSquare`, else `User/GridSquare`).
- The callsign is placed with the Core's one cty.dat table, the same one
  `turnRotorToCall` and the spot colouring use (`:/cty.dat`, a NereusCore
  resource, loaded once when the Core starts).
- The bearing is rounded to one decimal; a value that rounds to 360 is
  sent as 0.
- -1 means not known: no grid square, a grid square that cannot be read,
  a callsign cty.dat cannot place, or no cty.dat. It is never 0, which is
  north.

A remote desktop window keeps the value in its spot model
(`SpotData::bearingDeg`, -1 for its own local sources' spots).

## Tools catalogue

The Core's tools catalogue always lists `rotor` ("Rotor", station), last,
like every other tool: `offered` is true when a rotor is configured on this
Core (its driver is not none) and false without one, and the iPhone's Tools
tab shows it greyed with "No rotor is set up on this Core." Setting up or
forgetting a rotor changes `offered` and the catalogue's revision; a heading
alone does not.

## Refusals

Reason text is the identifier and is kept word for word between releases.

| Reason | When |
| --- | --- |
| "No rotor is set up on this Core." | Any turn command with driver none |
| "The rotor is not connected." | Any turn command off `connected` |
| "This rotor turns in azimuth only." | An elevation target or up/down nudge on an azimuth rotor |
| "That heading is not a number." | A heading that is NaN or infinite |
| "That heading is outside the rotor's range." | A target below 0 or above 360, or elevation outside 0 to 90 |
| "Set your grid square in Setup to turn the beam to spots." | `turnRotorToCall` with no grid square |
| "That callsign could not be placed." | `turnRotorToCall` for a call cty.dat does not resolve |
| "Hamlib's rotctld is not installed on the Core's computer." | `configureRotor` with driver 4 and `rotctldAvailable` false |
| "That serial port is not on the Core's computer." | `configureRotor` with a port not in `serialPorts` |
| "That rotor setup is not valid." | `configureRotor` of the right wire kinds whose values cannot be used: a `driver`, `axes` or `endStop` outside its table (an enum value that does not fit a 32-bit integer included), a `port` outside 1 to 65535, a `baud` of 0 or below, a `baud`, `port`, `hamlibModel` or `rangeDeg` that does not fit a 32-bit integer, a `rangeDeg` other than 360 or 450, a `hamlibModel` of 0 or below with driver 4, or an `offsetDeg` that is NaN or infinite; checked before anything reaches the controller |
| "The Core could not read this request." | Any rotor command whose arguments are missing, extra or of the wrong wire kind, or a `nudgeRotor` direction outside its table |
| "Update this app to turn the rotor on this Core." | Any rotor command from a window below minor 11 |
| "This Core does not control a rotor. Updating the Core may help." | Any rotor command to a Core that advertises 0 |

A refused command changes nothing. `configureRotor` sends `hamlibModel` to
the controller only for driver 4; for any other driver it is saved and
reported as 0.

`stopRotor` is never refused while connected.

## Core-owned settings

`AppSettings` on the Core, PascalCase keys: `Rotor/Driver`,
`Rotor/SerialPort`, `Rotor/Baud`, `Rotor/Host`, `Rotor/Port`, `Rotor/HamlibModel`, `Rotor/Axes`,
`Rotor/EndStop`, `Rotor/RangeDeg`, `Rotor/OffsetDeg`, `Rotor/Presets`.
Defaults: driver none, baud 9600, port 4533, axes azimuth, end stop north
(Longpath: "the common case"), range 360, offset 0. 9600 baud and
GS-232B follow the ERC maker's setup example (ERC SMD USB V4.3
Instructions, section 7).

The "turn the beam when I tune to a spot" choice is each window's own
preference (`Rotor/TurnOnTune`, `"False"` by default), not the Core's.

## Fixtures

- `tests/data/link/v1/surface.json`: the `rotor` key, the `RotorModel`
  class, the seven commands and `remoteRotorControlVersion` 1 (regenerated
  by `tst_link_surface_manifest_regen`).
- `tests/data/link/v1/sessions/verbs-rotor.json` (`session-verbs-rotor`,
  runs on the station and the app, in-process and over a data channel):
  every command with its own arguments and with a renamed one on a Core
  with no rotor set up, the setup values outside the tables, a serial port
  the Core lacks, the preset refusals, the presets delta and the refused
  write. The station runner's Core sees no serial ports and no `rotctld`
  there, so the object reads the same on every machine.
- The other `coreAccessories` session fixtures carry the capability, the
  `RotorModel` schema and the `rotor` object.
- The catalogue fixtures (`session-catalog-anan-g2`,
  `session-catalog-hermes-lite-2`, `session-settings-band-plan`,
  `session-verbs-radio-bound-antenna-rows`) list the `rotor` tool last,
  not offered, on a Core with no rotor set up.

## Evidence

- `tst_station_rotor_link`: every command and refusal on the wire against a
  connected fake GS-232B rotor, the minor and capability gates, the setup
  checks, `hamlibModel` for driver 4 only, a window's session ending its
  hold, the refused write, the object following the rotor, the catalogue's
  Rotor entry, and a remote window's `StationClient` mirroring the object
  and sending all seven commands.
- `tst_link_conformance_session` and the app's `LinkConformanceSessionTests`
  play `session-verbs-rotor`; `tst_link_surface_manifest` checks the surface.
