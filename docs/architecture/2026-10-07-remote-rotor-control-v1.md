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

## Negotiation

| Capability | Session minor | Value | Meaning |
| --- | --- | --- | --- |
| `remoteRotorControlVersion` | 11 | 1 | The Core mirrors its rotor as the `rotor` object; every command below works; spots carry a bearing |

The Core advertises 1 when it owns a rotor connection (the headless Core,
`nereusd`, always does), and 0 otherwise. `kSessionProtocolMinor` does not
change. A window on a Core that advertises 0 shows its rotor controls
greyed with the reason "This Core does not control a rotor. Updating the
Core may help."

## The `rotor` object

Class `RotorModel`, key `rotor`. Sent to every app. Every property is the
Core's to report.

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
| `serialPorts` | utf8 | The serial ports the Core's computer has, one per line, so a phone can offer them in setup |
| `axes` | enum | Axes table below |
| `rangeDeg` | i64 | 360, or 450 for a rotor with overlap |
| `offsetDeg` | f64 | Calibration offset added to the read heading |
| `azimuthDeg` | f64 | Current heading after the offset, 0 to 360 (or to 450 within overlap); -1 when unknown |
| `elevationDeg` | f64 | Current elevation, 0 to 90; -1 on an azimuth rotor or when unknown |
| `targetAzimuthDeg` | f64 | The target being turned to; -1 when none |
| `targetElevationDeg` | f64 | The elevation target; -1 when none |
| `motion` | enum | Motion table below |
| `presets` | utf8 | The presets, one per line as `name<TAB>degrees`, in the operator's order |
| `fault` | utf8 | The last fault in plain words; empty otherwise |

Off `connected`, the Core sets the headings and targets to -1 and `motion`
to stopped.

### Enum tables

`driver`: 0 none, 1 GS-232A (serial), 2 GS-232B (serial), 3 Hamlib rotctld
(TCP).

`axes`: 0 azimuth, 1 azimuth and elevation.

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
| `configureRotor` | `driver` enum, `serialPort` utf8, `baud` i64, `host` utf8, `port` i64, `axes` enum, `rangeDeg` i64, `offsetDeg` f64 | Save the setup and (re)connect; driver 0 disconnects and forgets |
| `disconnectRotor` | none | Disconnect, keeping the setup |
| `setRotorPresets` | `presets` utf8 (as the property) | Replace the presets |

**The hold dead man.** While a turn button is held, the window repeats
`nudgeRotor` with `active` true every 250 ms. If no repeat arrives for
750 ms, or that window's session ends, the Core sends stop. These two
numbers are this design's choice, not a device fact.

**Who may turn the rotor.** Any window allowed to change station
accessories (the same admission as the accessory settings commands). The
rotor turns while the radio is on the air (JJ, 2026-10-07): it switches no
RF path, unlike the amp and tuner controls that wait.

## Bearings on spots

With `remoteRotorControlVersion` 1, every spot the Core serves gains
`bearingDeg` (f64): the short-path initial great-circle bearing from the
station's grid square to the spot's cty.dat entity, 0 to 360, or -1 when
the Core has no grid square or cannot place the callsign. Long path is
`bearingDeg + 180` modulo 360, worked out by the window. Plan Task 4 fixes
the exact field placement against the spot stream's existing shape and
records it here.

## Tools catalogue

The Core's tools catalogue lists `rotor` ("Rotor", station) when a rotor is
configured on this Core. Without one it is not offered, and the iPhone's
Tools tab shows it greyed with "No rotor is set up on this Core."

## Refusals

Reason text is the identifier and is kept word for word between releases.

| Reason | When |
| --- | --- |
| "No rotor is set up on this Core." | Any turn command with driver none |
| "The rotor is not connected." | Any turn command off `connected` |
| "This rotor turns in azimuth only." | An elevation target or up/down nudge on an azimuth rotor |
| "That heading is outside the rotor's range." | A target below 0 or above `rangeDeg`, or elevation outside 0 to 90 |
| "Set your grid square in Setup to turn the beam to spots." | `turnRotorToCall` with no grid square |
| "That callsign could not be placed." | `turnRotorToCall` for a call cty.dat does not resolve |
| "That serial port is not on the Core's computer." | `configureRotor` with a port not in `serialPorts` |

`stopRotor` is never refused while connected.

## Core-owned settings

`AppSettings` on the Core, PascalCase keys: `Rotor/Driver`,
`Rotor/SerialPort`, `Rotor/Baud`, `Rotor/Host`, `Rotor/Port`, `Rotor/Axes`,
`Rotor/RangeDeg`, `Rotor/OffsetDeg`, `Rotor/Presets`. Defaults: driver none,
baud 9600, port 4533, axes azimuth, range 360, offset 0. 9600 baud and
GS-232B follow the ERC maker's setup example (ERC SMD USB V4.3
Instructions, section 7).

The "turn the beam when I tune to a spot" choice is each window's own
preference (`Rotor/TurnOnTune`, `"False"` by default), not the Core's.

## Fixtures

Plan Task 4 adds conformance fixtures for the object, every command and
every refusal, and the rows in `tests/data/link/v1/surface.json`.
