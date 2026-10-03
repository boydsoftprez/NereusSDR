# Remote accessory control version 1

This is the wire contract for R-R3-47 and R-R3-22: the Power Genius XL
(PGXL), the Tuner Genius XL (TGXL) and the RF-Kit RF2K-S talk to the Core
only, and every window (the desktop remote window, the planned iPhone app, a
local window in-process) reads the same Core objects and asks the Core to act
through the same commands. It sits beside
[remote media control version 1](2026-09-20-remote-media-control-v1.md) and
[remote notch control version 1](2026-09-23-remote-notch-control-v1.md), and
uses the existing session envelope (schema, object snapshots, property
deltas, `property.write` / `property.result`, `command.invoke` /
`command.result`).

The plan that builds it is
[2026-09-23-r3-core-owned-accessories-plan.md](2026-09-23-r3-core-owned-accessories-plan.md).
Each of its tasks extends this document in the same commit that adds what it
describes. This revision covers Tasks 1 to 6: the read-only `amplifier` and
`rfkit` status objects, everything the Core already serves for its
accessories (the `tuner` object, the 4O3A fields, and the TGXL and 4O3A
commands), the Core-owned PGXL (identity before admission, pairing only
after it, the `configurePgxl`, `disconnectPgxl` and
`setPgxlConnectionSettings` commands, and the receive-only refusals of the
amplifier's and tuner's operate controls), the Core-owned RF2K-S (identity
from `/info` before admission, its interface, antenna and tuner rows, the
`configureRfKit`, `disconnectRfKit` and `setRfKitEnabled` commands), band
follow for both amplifiers, and the Core's own TCI server on the station
network (the `stationTci` object and the `setStationTci` command), and the
Core's accessory records and settings (the `accessoryData` object: fault
history for all three devices, connection counters, the transmit interlock
policy, the Power Genius output limit and its alert, tune memory and
antenna names; the `setTxInterlockPolicy`, `setPgxlPowerCap` and
`clearAccessoryFaults` commands), and the Power Genius's and Tuner
Genius's own settings (the `accessorySettings` object and the commands the
Core sends to the device as a local window's Advanced page does: name, the
amp's bias, fan and LED, network, Save & Reboot and Revert). A later
revision (plan
[2026-09-24-remote-tgxl-controls-plan.md](2026-09-24-remote-tgxl-controls-plan.md),
R-R3-49) adds the Tuner Genius's antenna, operate and bypass from a window
(`remoteTgxlControlVersion` 2: `setTgxlAntenna`, `setTgxlOperate` and
`setTgxlBypass`), refused while the radio is on the air. The remote-window
parity plan's Task 8 (R-R3-49) adds the rest of a local window's Tuner
Genius at `remoteTgxlControlVersion` 4: the relay nudges (`moveTgxlRelay`),
the Core's own Scan LAN (`scanTgxlLan`) and the Peripherals row's address
saved without Connect (`setTgxlAddress`). Its Task 9 (R-R3-49) does the
same for the Power Genius at `remotePgxlControlVersion` 4: OPERATE and
STANDBY (`setPgxlOperate`), the Core's own Scan LAN (`scanPgxlLan`) and
the address saved without Connect (`setPgxlAddress`). Its Task 10
(R-R3-49) does the same for the RF-Kit RF2K-S at
`remoteRfKitControlVersion` 4: OPERATE and STANDBY (`setRfKitOperate`),
ANT 1 to 4 (`setRfKitAntenna`), TCI mode (`setRfKitTciMode`) and the
address saved without Connect (`setRfKitAddress`), and at
`accessoryDataVersion` 2 the Core's RF-Kit connection counts on
`accessoryData`.

**The on-air rule, by what a control does** (parity mini-round, the
operator's rulings of 2026-09-25, the same in local and remote windows).
What switches the amp or the tuner waits while the radio is on the air:
the Tuner Genius antenna, operate, bypass and relays, the Power Genius
OPERATE and STANDBY, and the RF-Kit OPERATE and STANDBY, ANT 1 to 4 and
TCI mode. STANDBY waits too: switching the amp's relays under RF is what
the rule guards, and the operator unkeys first. What only listens or saves
does not wait: `scanTgxlLan`, `scanPgxlLan`, `setTgxlAddress`,
`setPgxlAddress` and `setRfKitAddress` are taken on the air (before this
round a Core refused them there with the on-air reason; a window written
for that still works, it simply never sees that refusal now). The version
numbers do not change.

## Wire conventions

A non-Qt client needs only these rules.

- Every mirrored object is announced once per class by a `schema` message
  (`{"type":"schema","class":...,"fields":[{"name","kind","ordinal"}]}`),
  then sent whole by `object.create` (`{"type":"object.create","key",
  "class","properties":[{"name","kind","ordinal","value"}]}`), and changed
  by `delta` (`{"type":"delta","key","properties":[...]}`). The first burst
  ends with `{"type":"snapshot.complete"}`.
- Match properties by `name`. The `ordinal` is the property's position in
  its class for this build and is not stable across builds.
- Wire kinds: `bool`; `i64` (a whole number); `f64` (a number; values the
  Core reads as 32-bit floats arrive with their float rounding, for example
  1.399999976158142 for 1.4); `utf8` (text); `enum` (a whole number from a
  fixed table in this document).
- Enum values are fixed. New values are only appended. A client that meets a
  value it does not know ignores that one value and keeps the one it had
  (the desktop app refuses it and logs it), and keeps working.
- A client drops, without error, an object key or property name it does not
  know. That is how older apps survive newer Cores.
- A delta carries every property that shares one change notice on the Core,
  so it may repeat values that did not change. Apply each as a plain value.
- Units are in the property name where there is one: `W` watts, `C` degrees
  Celsius, `V` volts, `A` amperes. `swr` is a ratio (1.0 is a perfect match).

## Negotiation

Capabilities arrive in the `capabilities` message after authentication. The
session protocol minor (`kSessionProtocolMinor`) does not change for this
contract; each feature has its own version.

| Capability | Session minor | Value | Meaning |
| --- | --- | --- | --- |
| `remoteTgxlConfigVersion` | 4 or later | 1 | The Core owns the TGXL: `configureTgxl` and `disconnectTgxl` work |
| `remoteFourO3AControlVersion` | 4 or later | 1 | `setFourO3AEnabled` works |
| `remotePgxlControlVersion` | 11 | 1 | The Core mirrors its PGXL as the read-only `amplifier` object |
| `remotePgxlControlVersion` | 11 | 2 | Also: `configurePgxl`, `disconnectPgxl` and `setPgxlConnectionSettings` work, and the Core identifies and pairs the PGXL itself |
| `remoteRfKitControlVersion` | 11 | 1 | The Core mirrors its RF2K-S as the read-only `rfkit` object |
| `remoteRfKitControlVersion` | 11 | 2 | Also: the interface, antenna, tuner and band-follow rows of `rfkit`; `configureRfKit`, `disconnectRfKit` and `setRfKitEnabled` work, and the Core identifies the RF2K-S itself |
| `remoteRfKitControlVersion` | 11 | 3 | Also: `resetRfKitError` works, and the Core applies a window's `RfKit_AutoReconnect` and `RfKit_PollIntervalMs` station settings to its amp's connection at once, so the RF-Kit page's settings, antenna names and Reset amp error work from a remote window |
| `remoteRfKitControlVersion` | 11 | 4 | Also: `setRfKitOperate` (OPERATE or STANDBY), `setRfKitAntenna` (ANT 1 to 4), `setRfKitTciMode` (the amp in TCI mode) work whenever the radio is not on the air, and `setRfKitAddress` (the address saved without dialling) works on the air too (see "Operating the RF-Kit amplifier") |
| `stationTciVersion` | 11 | 2 | Version 1: the Core's read-only `stationTci` object and `setStationTci`. Version 2: four server options, the `tciClients` stream, option changes and client disconnect |
| `accessoryDataVersion` | 11 | 1 | The Core mirrors its accessory records and settings as the read-only `accessoryData` object, and `setTxInterlockPolicy`, `setPgxlPowerCap` and `clearAccessoryFaults` work |
| `accessoryDataVersion` | 11 | 2 | Also: the RF-Kit's connection counts on `accessoryData` (`rfkitConnectedSinceMs`, `rfkitPollsOk`, `rfkitPollsFailed`, `rfkitReconnectCount`, `rfkitLastPollMs`) |
| `accessoryDataVersion` | 11 | 3 | Also: the RF-Kit's average response time over its last ten polls on `accessoryData` (`rfkitRttAvgMs`), as a local window's RF-Kit page and Copy diagnostics show it |
| `accessoryTxVersion` | 11 | 1 | The Core accepts the nine transmit-coupled amplifier, tuner and RF-Kit commands listed below. Every refusal carries a code, text and fix. |
| `remotePgxlControlVersion` | 11 | 3 | Also: the amp's own settings. The `pgxl*` properties of the read-only `accessorySettings` object, and `setPgxlName`, `setPgxlHardware`, `setPgxlNetwork`, `savePgxlSettings` and `readPgxlSettings` work |
| `remotePgxlControlVersion` | 11 | 4 | Also: `setPgxlOperate` (OPERATE or STANDBY), `scanPgxlLan` (the Core listens for Power Genius announcements) and `setPgxlAddress` (the address saved without dialling): OPERATE and STANDBY work whenever the radio is not on the air, the scan and the address on the air too (see "Operating the Power Genius" and "Scanning for the Power Genius and its saved address") |
| `remoteTgxlControlVersion` | 11 | 1 | The tuner's own settings: the `tgxl*` properties of `accessorySettings`, and `setTgxlName`, `setTgxlNetwork`, `saveTgxlSettings` and `readTgxlSettings` work |
| `remoteTgxlControlVersion` | 11 | 2 | Also: the tuner's antenna, operate and bypass. `setTgxlAntenna`, `setTgxlOperate` and `setTgxlBypass` work whenever the radio is not on the air (see "Switching the Tuner Genius") |
| `remoteTgxlControlVersion` | 11 | 3 | Also: `setTgxlOperate` with `on` true puts the tuner in OPERATE whole, bypass off and operate on from the one command, so STANDBY to OPERATE is one request |
| `remoteTgxlControlVersion` | 11 | 4 | Also: `moveTgxlRelay` (a relay nudge), `scanTgxlLan` (the Core listens for Tuner Genius announcements) and `setTgxlAddress` (the address saved without dialling): the nudge works whenever the radio is not on the air, the scan and the address on the air too (see "Switching the Tuner Genius" and "Scanning for the Tuner Genius and its saved address") |

- The Core advertises `remotePgxlControlVersion` as 4,
  `remoteRfKitControlVersion` as 4, `remoteTgxlControlVersion` as 4 and
  the other TGXL and 4O3A versions (`remoteTgxlConfigVersion`,
  `remoteFourO3AControlVersion`) as 1 when it owns its accessories (the
  headless Core, `nereusd`, always does), and all of them as 0 otherwise. A
  Core built before the remote TGXL controls plan says 1 for
  `remoteTgxlControlVersion`: the tuner's own settings, but not its
  antenna, operate or bypass, which stay greyed in a remote window. A
  Core built before the fix wave of that plan says 2: `setTgxlOperate`
  with `on` true sends only `operate=1`, so from STANDBY a window sends
  `setTgxlBypass` false and then `setTgxlOperate` true. A Core built
  before the parity plan's Task 8 says 3: no relay nudges, no Scan LAN at
  the Core and no saved address, so a window keeps the relay bars with its
  transmit reason, Scan LAN off and a typed address kept for Connect. A
  Core built before the parity plan's Task 9 says 3 for the PGXL: no
  OPERATE or STANDBY, no Scan LAN at the Core and no saved address, so a
  window keeps OPERATE greyed with its older reason, Scan LAN off and a
  typed address kept for Connect. A Core built before the parity plan's
  Task 10 says 3 for the RF2K-S: no OPERATE or STANDBY, no antenna, no
  TCI mode from a window and no saved address, so a window keeps OPERATE
  and ANT greyed with its older reason, "Set amp to TCI mode" off and a
  typed address kept for Connect. A
  Core built between Tasks 2 and 5 says 2 for the PGXL and sends no
  `remoteTgxlControlVersion`: the amp's and tuner's own settings are not
  offered there. A Core built before the fix wave says 2 for the RF2K-S:
  no `resetRfKitError`, and the RF-Kit page's settings and names stay
  unchangeable in a remote window. A Core built between Tasks 1 and 3 says 1 for the
  RF2K-S (and, before Task 2, for the PGXL): the object, no commands. An app
  treats 1 as "readings only" and 2 or more as "readings and commands".
- `stationTciVersion` is 1 on a Core that runs its station TCI server
  (`nereusd` always does; the server itself is on only while the station's
  TCI switch is on), 0 otherwise.
- `accessoryDataVersion` is 2 on a Core that owns its accessories (the same
  condition as `remotePgxlControlVersion` 2), 0 otherwise. A Core built
  before the parity plan's Task 10 says 1: no RF-Kit connection counts, so
  a window's RF-Kit Live diagnostics says the Core keeps them. The five
  counts come last in the object, so the older properties keep their
  ordinals.
- `remotePgxlControlVersion`, `remoteRfKitControlVersion`,
  `stationTciVersion`, `accessoryDataVersion` and then
  `remoteTgxlControlVersion` travel last in the
  minor-11 block of the capabilities message, after `hpsdrModel`,
  `radioProtocol`, `radioAddress` and `radioHardwareVersion`, and only to an
  app that agreed minor 11. An app below minor 11 receives exactly the
  capabilities, objects and deltas it received before: none of the five
  entries, and no `amplifier`, `rfkit`, `stationTci`, `accessoryData` or
  `accessorySettings` schema, object or delta. (`remoteTgxlConfigVersion`
  keeps its place and value in the older block.) A minor-11 app built before `accessoryData`
  drops its schema, object and deltas as an unknown key (see Wire
  conventions) and keeps reading the fault history from the settings
  snapshot as before; one built before `accessorySettings` drops that
  object the same way.
- An app that sees version 0, or no entry, shows no Power Genius or RF-Kit
  readings from this Core and says so (see Window behaviour).
- An app that sees `remoteTgxlControlVersion` below 2 (or no entry) does
  not offer the Tuner Genius's antenna, operate and bypass on this Core:
  the desktop app keeps them greyed with its transmit reason (see "What
  waits for remote transmit"), and says "This Core does not let this app
  switch the Tuner Genius. Updating the Core may help." if asked anyway.
- An app that sees `remoteTgxlControlVersion` below 4 does not offer the
  relay nudges, Scan LAN at the Core or the saved address on this Core,
  and says "This Core does not let this app move the Tuner Genius relays,
  scan for it or save its address. Updating the Core may help." if asked
  anyway (Scan LAN's tooltip: "This Core does not scan for a Tuner Genius
  for this app. Updating the Core may help.").
- An app that sees `remotePgxlControlVersion` below 4 does not offer the
  amp's OPERATE and STANDBY, Scan LAN at the Core or the saved address on
  this Core, and says "This Core does not let this app put the Power
  Genius in operate or standby, scan for it or save its address. Updating
  the Core may help." if asked anyway (the applet's OPERATE keeps
  "Amplifier control is not available from a remote window.", the
  Power Genius tab's Operate the receive-only reason, and Scan LAN's
  tooltip says "This Core does not scan for a Power Genius for this app.
  Updating the Core may help.").
- An app that sees `remoteRfKitControlVersion` below 4 does not offer the
  RF-Kit amplifier's OPERATE and STANDBY, antennas, TCI mode or the saved
  address on this Core, and says "This Core does not let this app put the
  RF-Kit amplifier in operate or standby, switch its antenna or TCI mode,
  or save its address. Updating the Core may help." if asked anyway (the
  applet's OPERATE and ANT keep "Amplifier control is not available from a
  remote window.", and "Set amp to TCI mode" says "The Core puts the
  amplifier in TCI mode itself while the Core's TCI server is on.").
- An app that sees `remotePgxlControlVersion` below 3 (or
  `remoteTgxlControlVersion` 0, or no entry) does not offer to change the
  amp's (or tuner's) own settings on this Core and says why: "This Core does
  not let this app change the Power Genius's own settings. Updating the
  Core may help." (or "the Tuner Genius's").
- An app that sees `accessoryDataVersion` 0, or no entry, shows the
  accessory records it can read from the settings snapshot, and does not
  offer to change the interlock policy, the output limit or the fault
  history on this Core (see Window behaviour).

## Connection phase

`tuner`, `amplifier` and `rfkit` share one connection-state shape and one
phase table, `connectionPhase`:

| Value | Name | Meaning |
| --- | --- | --- |
| 0 | `disabled` | The station has this accessory switched off |
| 1 | `disconnected` | Switched on, not connected, no attempt running |
| 2 | `discovering` | Waiting for the device's LAN discovery announcement |
| 3 | `connecting` | Opening the connection |
| 4 | `identifying` | Connected, checking the device is what was configured |
| 5 | `retrying` | Lost or refused; the Core will try again by itself |
| 6 | `connected` | Admitted; readings are live |
| 7 | `error` | Stopped with a reason in `connectionError`; no retry |

```
disabled --switch on--> disconnected --configure--> discovering --> connecting
    --> identifying --> connected
connected --drop--> retrying --> connecting ...      (automatic retry on)
connected --drop--> disconnected                     (automatic retry off)
any --failure--> error ; any --switch off--> disabled ; any --disconnect--> disconnected
```

Which phases each device reports today:

- TGXL: all eight. `discovering` and `identifying` are the Core's identity
  check (a TunerGenius or TunerGeniusXL discovery announcement from the same
  address and port, and the same serial in the tuner's own info reply).
- PGXL: `disabled`, `disconnected`, `connecting`, `identifying`,
  `retrying`, `connected`, `error`. `identifying` is the Core's identity
  check (a `PowerGeniusXL` discovery announcement from the same address and
  port, and the same serial in the amp's own `info` reply; see "How the
  Core identifies the PGXL"). The PGXL, like the TGXL, never reports
  `discovering`: the discovery listen runs inside `identifying`.
- RF2K-S: `disabled`, `disconnected`, `connecting`, `retrying`,
  `connected`, `error`. `connecting` covers the Core's identity check: a REST
  amp has no separate identifying step, since the `/info` reply that proves
  the address answers is the one that identifies it (see "How the Core
  identifies the RF2K-S").

**Accepted is not connected.** A command's `command.result` with
`accepted: true` means the Core took the request. Whether the device is now
connected is only ever said by `connectionPhase` becoming `connected`. A
window shows the request as pending until the phase moves, and shows
`connectionError` in user words if it ends at `error` or `retrying`.

## The `tuner` object (TGXL)

Class `TunerModel`, key `tuner`. Sent to every app. Every property is the
Core's to report.

| Property | Kind | Meaning |
| --- | --- | --- |
| `connectionPhase` | enum | Connection phase (table above) |
| `configuredHost` | utf8 | The address the Core dials |
| `configuredPort` | i64 | Its TCP port (9010 by default) |
| `connectionError` | utf8 | Why the last attempt failed; empty otherwise. Diagnostic text, shown in user words |
| `deviceModel` | utf8 | The model the device reported |
| `deviceSerial` | utf8 | Its serial number |
| `deviceVersion` | utf8 | Its firmware version |
| `deviceNickname` | utf8 | Its nickname |
| `relayC1`, `relayL`, `relayC2` | i64 | Matching network relay positions, 0 to 255 |
| `isOperate` | bool | In operate (not standby) |
| `isBypass` | bool | Bypassed |
| `isTuning` | bool | A tune cycle is running |
| `antennaA` | i64 | Selected antenna on a 3x1 tuner, as the tuner numbers it |
| `hasAntennaSwitch` | bool | The tuner is a 3x1 with an antenna switch |
| `isPresent` | bool | Admitted and reporting |
| `hasDirectConnection` | bool | Connected to the Core |
| `tgxlIp` | utf8 | The tuner's address while connected |
| `fwdPower` | f64 | Forward power through the tuner, W |
| `swr` | f64 | SWR ratio at the tuner |

Off `connected`, the Core clears the relays, operate, bypass, tuning,
antenna and meters (1.0 SWR, 0 W).

### Switching the Tuner Genius

With `remoteTgxlControlVersion` 2 a window switches the Core's tuner with
three commands (see Commands). The Core applies each through its own
tuner model, the command slots a local window's Tuner Genius applet
drives, so the tuner gets exactly the local window's line, framed
`C<seq>|<command>`:

| Command | Sent to the tuner | Local applet control |
| --- | --- | --- |
| `setTgxlAntenna` `port` | `activate ant=<port>` | ANT 1, ANT 2, ANT 3 |
| `setTgxlOperate` `on` | `on` true: `bypass=0` then `operate=1` (at version 3; `operate=1` alone at 2). `on` false: `operate=0` | OPERATE button (STANDBY to OPERATE, BYPASS to STANDBY) |
| `setTgxlBypass` `on` | `bypass=1` or `bypass=0` | OPERATE button (OPERATE to BYPASS, STANDBY to OPERATE) |
| `moveTgxlRelay` `relay`, `direction` (version 4) | `tune relay=<relay> move=<move>`, `move` 1 or -1 (`TgxlConnection::adjustRelay`, from AetherSDR's) | Mouse wheel on the C1 (`relay` 0), L (1) or C2 (2) bar |

The tuner takes operate and bypass as separate lines (`TunerModel::setOperate`
and `setBypass`, from AetherSDR's `TunerModel`), so the tuner cannot take
them as one. At version 3 the Core makes STANDBY to OPERATE one command
instead: after its one check it sends `bypass=0` and `operate=1` back to
back, with nothing else able to run between them, so a key cannot leave
the tuner half-changed. Below 3 a window sends two commands, and a key
between them leaves the tuner with bypass off and still in standby.

`accepted: true` means the line left for the tuner. The window's buttons
follow the tuner's report on this object (`antennaA`, `isOperate`,
`isBypass`), never the click, and its relay bars follow `relayC1`,
`relayL` and `relayC2`, never the wheel. A relay nudge moves one matching
relay one step. None of them keys a transmitter or starts a
tune, so a receive-only Core takes them (operator ruling of 2026-09-24).
Each waits while the radio is on the air (operator decision D60): the Core
refuses it while its MOX (from any source, a hardware PTT included), TUNE
or two-tone test is on, and until its MOX controller has finished handing
back to receive (about 30 ms after MOX clears), and a window
disables its buttons with the reason while the Core reports the radio
keyed (`transmitting` on the `radio` object, below), TUNE on the
`transmit` object or the two-tone test on `pureSignal`. TUNE (the
autotune) and the tune-memory recall on a band change still wait for
remote transmit, because they start a tune and put a tune carrier on the
air. A relay nudge keys nothing: from `remoteTgxlControlVersion` 4 it
waits only while the radio is on the air, as the switches do.

### Scanning for the Tuner Genius and its saved address

With `remoteTgxlControlVersion` 4 a window's Setup > CAT & Network > 4O3A >
Peripherals row does at the Core what a local window's row does at its own
computer:

- `scanTgxlLan` (no arguments): the Core listens for Tuner Genius
  announcements (`TunerGenius` and `TunerGeniusXL`, the two the Core
  admits) on its station network for the local Scan LAN dialog's own
  window, three seconds, and then answers. Its `command.result` carries
  `values` with one entry, `devicesJson` (utf8): a JSON array, one object
  per device heard, `{"address","port","model","serial","nickname"}`
  (`port` a number, the others text), `[]` when none. Listening sends
  nothing to any device. The answer comes once, when the window ends; an
  answer due to an earlier app connection is not sent.
  The Core runs one scan per device at a time: a scan asked for while one
  is listening joins it and gets the same answer when it ends. It only
  listens, so it is taken while the radio is on the air too.
- `setTgxlAddress` (`host` utf8, `port` i64): the Core saves
  `TGXL_ManualIp` and `TGXL_ManualPort` for its radio without dialling,
  with `configureTgxl`'s address checks and reasons. The `tuner` object's
  `configuredHost` and `configuredPort` take the saved address while the
  Core is not connecting to or connected to a tuner (a running connection
  keeps showing its own); `connectionPhase` does not change. It is taken
  while the radio is on the air too: saving switches nothing.
  A blank `host` (empty after trimming, with a port from 1 to 65535) is saved as blank, as a local window's blank Host is: it stops auto-connect, because the Core dials a saved address only when it is not blank, and `configuredHost` shows the blank.

## The `amplifier` object (PGXL)

Class `AmplifierModel`, key `amplifier`, `remotePgxlControlVersion` 1.
Read-only: every property is the Core's to report.

| Property | Kind | Meaning |
| --- | --- | --- |
| `connectionPhase` | enum | Connection phase (table above) |
| `configuredHost` | utf8 | The address the Core last dialled |
| `configuredPort` | i64 | Its TCP port (9008 by default) |
| `connectionError` | utf8 | Why the last attempt failed; empty otherwise |
| `deviceModel` | utf8 | The product in the amp's LAN discovery announcement, `PowerGeniusXL`; empty before the identity check. After a refused identity, the product that was found (for example `TunerGenius`) |
| `deviceSerial` | utf8 | The serial in the amp's `info` reply (for example `10-200/24-0046`); empty before |
| `deviceVersion` | utf8 | The version in the amp's connect banner and `info` reply, for example `3.8.9` |
| `deviceNickname` | utf8 | The nickname in the amp's discovery announcement; empty before |
| `present` | bool | The Core has a reading from the amp on the current connection. False: the values below are the last ones read and are not live |
| `state` | enum | The amp's state (table below) |
| `deviceState` | utf8 | The amp's own state word, as sent |
| `operate` | bool | Operating: `state` is `idle`, `operate`, `transmitA` or `transmitB` |
| `transmitting` | bool | Keyed: `state` is `transmitA` or `transmitB` |
| `forwardPowerW` | f64 | Peak forward power while transmitting, W; 0 otherwise |
| `swr` | f64 | SWR ratio while transmitting, capped at 99; 1.0 otherwise |
| `temperatureC` | f64 | Heat-sink temperature, degrees C |
| `mainsVoltageV` | f64 | Mains voltage, V |
| `drainCurrentA` | f64 | Drain current, A |
| `efficiencyText` | utf8 | The amp's efficiency label (MEffA), as sent, for example `off` |
| `bandFollow` | enum | Whether the amp follows the radio's band (table in "Band follow"): `off` while not connected, `waiting` until the pairing is answered, `following` once paired |

`state`:

| Value | Name | The amp's word |
| --- | --- | --- |
| 0 | `unknown` | None yet, or a word this Core does not know |
| 1 | `powerUp` | `POWERUP` |
| 2 | `standby` | `STANDBY` |
| 3 | `idle` | `IDLE` (operating, ready to transmit) |
| 4 | `operate` | `OPERATE` |
| 5 | `transmitA` | `TRANSMIT_A` |
| 6 | `transmitB` | `TRANSMIT_B` |
| 7 | `fault` | Any word beginning `FAULT` |

### Operating the Power Genius

With `remotePgxlControlVersion` 4, `setPgxlOperate` (`on` bool) puts the
Core's amp in operate (true) or standby (false). The Core sends its amp the
local applet's own line through its `PgxlConnection`, framed
`C<seq>|<command>`:

| Command | Line the amp receives | Local control |
| --- | --- | --- |
| `setPgxlOperate` `on` | `operate=1` (true) or `operate=0` (false) | The Power Genius applet's OPERATE button; a remote window's Setup > CAT & Network > 4O3A > PowerGenius XL > Operate |

(The line is the one the amp takes, from the bench capture of 2026-05-19:
a bare `operate` or `standby` is refused by the amp.) `accepted: true`
means the line left for the amp. The window's buttons follow the amp's
report on this object (`state`, `deviceState`, `operate`), never the
click. Operating the amp keys nothing (it amplifies only when the radio
transmits), so a receive-only Core takes it (operator decision D53,
ruling 7.8). It waits while the radio is on the air (operator decision
D60): the Core refuses it while its MOX (from any source, a hardware PTT
included), TUNE or the two-tone test is on, and through the hand-back to
receive, and a window disables its buttons with the reason while the Core
reports the radio on the air. It is also refused while the Core is not
connected to the amp. Nothing reaches the amp on a refusal. PGXL standby
around a TGXL tune still waits for remote transmit.

### Scanning for the Power Genius and its saved address

With `remotePgxlControlVersion` 4 a window's Setup > CAT & Network > 4O3A >
Peripherals Power Genius row does at the Core what a local window's row
does at its own computer, as for the Tuner Genius:

- `scanPgxlLan` (no arguments): the Core listens for Power Genius
  announcements (`PowerGeniusXL`, the one product the Core admits) on its
  station network for three seconds, and then answers. Its
  `command.result` carries `values` with one entry, `devicesJson` (utf8):
  a JSON array, one object per device heard,
  `{"address","port","model","serial","nickname"}` (`port` a number, the
  others text), `[]` when none. Listening sends nothing to any device. The
  answer comes once, when the window ends; an answer due to an earlier app
  connection is not sent.
  The Core runs one scan per device at a time: a scan asked for while one
  is listening joins it and gets the same answer when it ends. It only
  listens, so it is taken while the radio is on the air too.
- `setPgxlAddress` (`host` utf8, `port` i64): the Core saves
  `PGXL_ManualIp` and `PGXL_ManualPort` for its radio without dialling,
  with `configurePgxl`'s address checks and reasons (the 4O3A switch is
  not an address check: saving dials nothing). The `amplifier` object's
  `configuredHost` and `configuredPort` take the saved address while the
  Core is not connecting to or connected to an amp (a running connection
  keeps showing its own); `connectionPhase` does not change. It is taken
  while the radio is on the air too: saving switches nothing.
  A blank `host` (empty after trimming, with a port from 1 to 65535) is saved as blank, as a local window's blank Host is: it stops auto-connect, because the Core dials a saved address only when it is not blank, and `configuredHost` shows the blank.

### How the Core reads the PGXL

The Core converts the amp's status lines with one function
(`applyPgxlStatus`, `src/core/PgxlStatusGauges.cpp`); a local window runs the
same function in-process. A status line's key it does not carry keeps its
last value.

| Amp key | Unit on the amp | Property |
| --- | --- | --- |
| `peakfwd` | dBm | `forwardPowerW` = 10^(dBm/10) / 1000 |
| `swr` | signed dB return loss (negative on a good match) | `swr`: with G = 10^(RL/20), (1 + G) / (1 - G); 99 when RL >= 0 dB or G >= 0.999. -24.5 dB is 1.13 |
| `temp` | degrees C | `temperatureC` |
| `vac` | V | `mainsVoltageV` |
| `id` | A | `drainCurrentA` |
| `state` | word | `deviceState`, `state`, `operate`, `transmitting` |
| `meffa` | label | `efficiencyText` |

`peakfwd` and `swr` are hold values on the amp: they keep the last transmit
peak. The Core takes them only while `transmitting`, and a state line
outside transmit sets `forwardPowerW` to 0 and `swr` to 1.0.

### How the Core identifies the PGXL

A connect banner (`V3.8.9`) says only that the peer speaks the Genius
protocol; a Tuner Genius sends one too, and the amp's `info` reply has no
model. So on every dial, before anything else is sent:

1. The Core opens TCP to the configured address (`connecting`), reads the
   `V` banner and sends only `info` (`identifying`).
2. It listens for LAN discovery on UDP 9008 and 9010 for three seconds and
   takes the announcement whose address and receiving port are the
   connection's, heard only from the station network (see "Where the Core
   accepts station devices"). Captured from the real amp:
   `PowerGeniusXL ip=192.168.109.235 v=3.8.9 serial=10-200/24-0046 nickname=PowerGeniusXL`
3. The amp's `info` reply, captured:
   `R<seq>|0|serial=10-200/24-0046  version=3.8.9 protocol=1.0 mains=240`
   (key=value pairs, no leading word, two spaces after the serial).
4. Admitted only when the announced product is exactly `PowerGeniusXL` and
   the two serials are equal. Then, and only then, the Core pairs the amp
   (`amplifier create`, `flexradio ampslice=... ptt=LAN`, `keepalive enable`;
   operator decision of 2026-09-23: pair automatically once the Core has
   confirmed it is a real Power Genius) and follows the band after the amp
   accepts the pairing.

Anything else ends at `error` (and `retrying` when automatic retry is on)
with a reason, having been sent nothing but `info`: another product at the
address, no matching announcement in the window, a serial mismatch, an
`info` reply with no serial or an error code, or no answer within five
seconds. Replacing the address or disconnecting in any phase cancels the
attempt, its discovery listen and any pending retry: the old address is
never dialled again.

## Band follow

`amplifier` and `rfkit` each carry `bandFollow`, one fixed table:

| Value | Name | Meaning |
| --- | --- | --- |
| 0 | `off` | PGXL: not connected. RF2K-S: the TCI server it follows is off |
| 1 | `waiting` | PGXL: connected, not paired yet (or the pairing was refused). RF2K-S: the TCI server is on and the amp is not connected to it; `bandFollowAddress` and `bandFollowPort` say what to enter on the amp |
| 2 | `following` | The amp follows the radio's band |
| 3 | `thisComputerOnly` | RF2K-S: the TCI server accepts only apps on its own computer, so the amp cannot reach it |

The PGXL follows the band once it is paired: the Core sends
`flexradio ampslice=<slice> serial=<radio serial> band=<Hz>` on each band
change (200 ms debounce), and nothing before the pairing reply arrives.

The RF2K-S follows as a TCI app of the Core's station TCI server (see "The
`stationTci` object"): it reads only `vfo:` and `split_enable:`
(2026-05-24-rfkit-rf2ks-applet-design.md section 6.2). `following` means a
TCI app is connected from the amp's configured address (an amp configured
by host name never matches and reads `waiting`; the address line is still
right). While the station's TCI server is on, the Core switches an admitted
amp into TCI mode through its web interface (`PUT /operational-interface`
`{"operational_interface":"TCI"}`), once when band follow starts (the first
admission of that amp in the Core's run while the switch is on, or the
switch turned on), and only when the amp reports another interface; not
again after a link blip or a reconnect, so an operator who switches it back
on the amp's panel is not fought. The TCI server's address is entered on the amp's
own touchscreen (the REST call carries no address).

The lines a window shows, in user words: "Band follow: following the
radio"; "Band follow: enter <address>, port <port> as the TCI server on the
amplifier."; "Band follow: off. Turn on the TCI server so the amplifier can
follow the radio."; "Band follow: the TCI server accepts only apps on its
own computer, so the amplifier cannot reach it."; for the PGXL "Band
follow: waiting for the Power Genius to pair with the radio." and "Band
follow: off while the Power Genius is not connected."

## The `rfkit` object (RF2K-S)

Class `RfKitModel`, key `rfkit`, `remoteRfKitControlVersion` 1 (the
properties down to `currentA`) and 2 (the rest). Read-only.

| Property | Kind | Meaning |
| --- | --- | --- |
| `connectionPhase` | enum | Connection phase (table above) |
| `configuredHost` | utf8 | The amp's address |
| `configuredPort` | i64 | Its HTTP port (8080 by default) |
| `connectionError` | utf8 | Why the last attempt failed; empty otherwise |
| `deviceModel` | utf8 | The amp's `device` field from `/info`, for example `RF2K-S` |
| `deviceSerial` | utf8 | Empty; the amp's `/info` carries none |
| `deviceVersion` | utf8 | `G<GUI>C<controller>` from `/info`, for example `G200C267` |
| `deviceNickname` | utf8 | The amp's `custom_device_name` |
| `present` | bool | The Core has a `/power` reading on the current connection. False: the values below are not live |
| `operate` | bool | `/operate-mode` is `OPERATE` |
| `forwardPowerW` | f64 | `/power` forward, W |
| `reflectedPowerW` | f64 | `/power` reflected, W |
| `swr` | f64 | `/power` SWR ratio |
| `temperatureC` | f64 | `/power` temperature, degrees C |
| `voltageV` | f64 | `/power` supply voltage, V |
| `currentA` | f64 | `/power` current, A |
| `operationalInterface` | utf8 | `/operational-interface`, as sent: `TCI`, `UDP`, `CAT` or `UNIV`; empty before the first reading |
| `antennaPresentMask` | i64 | `/antennas`: bit N-1 set for each internal antenna N (1 to 4) the amp lists |
| `antennaDisabledMask` | i64 | `/antennas`: bit N-1 set for each internal antenna N the amp lists as `DISABLED` |
| `activeAntennaNumber` | i64 | `/antennas/active` number; 0 before the first reading |
| `activeAntennaExternal` | bool | The active antenna is an external one (no internal antenna is active) |
| `tunerMode` | enum | `/tuner` mode (table below) |
| `tunerSetup` | utf8 | `/tuner` setup, as sent, for example `LC` |
| `tunerInductanceNh` | i64 | `/tuner` L, nH |
| `tunerCapacitancePf` | i64 | `/tuner` C, pF |
| `tunerFrequencyKhz` | i64 | `/tuner` tuned frequency, kHz; 0: not tuned |
| `tunerSegmentKhz` | i64 | `/tuner` segment size, kHz |
| `bandFollow` | enum | "Band follow" table |
| `bandFollowAddress` | utf8 | The TCI server address to enter on the amp while `bandFollow` is `waiting` (or the one it follows); empty otherwise |
| `bandFollowPort` | i64 | That server's port; 0 while the server is off |

`tunerMode`:

| Value | Name | The amp's word |
| --- | --- | --- |
| 0 | `unknown` | None yet, or a word this Core does not know |
| 1 | `bypass` | `BYPASS` |
| 2 | `manual` | `MANUAL` |
| 3 | `autoTuning` | `AUTO_TUNING` (a tune is running) |
| 4 | `auto` | `AUTO` |

### How the Core identifies the RF2K-S

The Core dials `http://<configuredHost>:<configuredPort>/info` and counts
the amp as connected only if the reply's `device` field is `RF2K-S`
(`{"device":"RF2K-S","software_version":{"GUI":200,"controller":267},
"custom_device_name":"KG4VCF"}` on firmware G200C267; design doc
2026-05-24-rfkit-rf2ks-applet-design.md section 6.1, from the amp's
swagger and a live probe). Only then does it poll the other paths. A
reply that names another product is refused with a reason in
`connectionError`, recorded as a fault, and never retried: a different
device at the address will not become an RF2K-S. The `/info` refresh every
ten poll cycles checks it again; a device that starts naming another
product is dropped the same way. A reply that names no device at all
proves nothing either way, so it counts as a failed answer (as does no
answer at all): retried with the connection's backoff (1 s doubling to
60 s) while automatic retry is on (`RfKit_AutoReconnect`), and otherwise
stopped at `error`; on an admitted amp it is one failed poll (three in a
row drop the link and retry, as for any failure), never a refusal.

RF2K-S reasons in `rfkit`.`connectionError` (already in user words):

| Reason | When |
| --- | --- |
| "The device at this address is not an RF-Kit RF2K-S amplifier. It reports itself as <device>." | `/info` named another device |
| "The device at this address did not say it is an RF-Kit RF2K-S amplifier." | Kept for older apps; a Core from the fix wave on retries an `/info` that names no device instead |
| "The RF-Kit amplifier did not answer at this address." | No answer and automatic retry off |

### Operating the RF-Kit amplifier

With `remoteRfKitControlVersion` 4 a window switches the Core's admitted
RF2K-S. The Core sends the amp the REST request a local window's control
sends, through its own connection (`Rf2ksConnection`):

| Command | Request the amp receives | Local control |
| --- | --- | --- |
| `setRfKitOperate` `on` | `PUT /operate-mode` `{"operate_mode":"OPERATE"}` (true) or `{"operate_mode":"STANDBY"}` (false) | The RF-Kit applet's OPERATE button |
| `setRfKitAntenna` `port` | `PUT /antennas/active` `{"number":<port>,"type":"INTERNAL"}` | The RF-Kit applet's ANT 1 to ANT 4 |
| `setRfKitTciMode` | `PUT /operational-interface` `{"operational_interface":"TCI"}` | Setup > RF-Kit > RF2K-S > "Set amp to TCI mode" |

`accepted: true` means the request left for the amp. The window's controls
follow the amp's report on this object (`operate`, `activeAntennaNumber`,
`operationalInterface`), never the click. `setRfKitAntenna` takes an
internal antenna 1 to 4; once the amp has listed its antennas
(`antennaPresentMask`), one it does not list or lists as disabled
(`antennaDisabledMask`) is refused and nothing is sent, as a local
window's applet leaves that button off. None of them keys anything (the
amp amplifies only when the radio transmits), so a receive-only Core takes
them (operator decision D53, ruling 7.8). They wait while the radio is on
the air (operator decision D60): the Core refuses them while its MOX (from
any source, a hardware PTT included), TUNE or the two-tone test is on, and
through the hand-back to receive, and a window disables its controls with
the reason while the Core reports the radio on the air. They are also
refused while the Core has not admitted an amp. Nothing reaches the amp on
a refusal. The Core's own TCI switch still puts the amp in TCI mode once
when band follow starts (see "Band follow"); `setRfKitTciMode` is the
page's button, whenever the operator presses it.

`setRfKitAddress` (`host` utf8, `port` i64) saves `RfKit_ManualIp` and
`RfKit_ManualPort` for the Core's radio without dialling, with
`configureRfKit`'s address checks and reasons (the RF-Kit switch is not an
address check: saving dials nothing). The `rfkit` object's
`configuredHost` and `configuredPort` take the saved address while the
Core is not connecting to or connected to an amp (a running connection
keeps showing its own); `connectionPhase` does not change. The next
Connect, or the switch turned on, dials it. It is taken while the radio
is on the air too, as a local window's Save is: saving switches nothing.
A blank `host` (empty after trimming, with a port from 1 to 65535) is saved as blank, as a local window's blank Host is: it stops auto-connect, because the Core dials a saved address only when it is not blank, and `configuredHost` shows the blank.

## The `stationTci` object and the station TCI server

Class `StationTciModel`, key `stationTci`, `stationTciVersion` 1.
Read-only; the switch changes only through `setStationTci`.

| Property | Kind | Meaning |
| --- | --- | --- |
| `enabled` | bool | The station's TCI switch (kept on the Core) |
| `port` | i64 | The server's TCP port (50001 by default, as the app's own) |
| `listening` | bool | The server accepts connections |
| `stationAddress` | utf8 | The station network address it listens on; empty when it listens only on the Core's own computer |
| `error` | utf8 | Why it could not listen, as the system said; empty otherwise |
| `emulateExpertSdr3`, `emulateSunSdr2Pro` | bool | Version 2: protocol and device identity reported to a new TCI app |
| `cwluBecomesCw` | bool | Version 2: whether CWL/CWU is reported as CW |
| `sendInitialState` | bool | Version 2: whether a new app receives the initial radio state |

At version 2, `tciClients` is a record stream with at most 64 entries.
Each record id is a stable connection id. Its fields are `id`, `name`,
`address`, `subscriptions` (an array of audio, I/Q or sensor names),
`transmitting` (bool) and `lastCommand` (string). The Core sends changes
as record upserts and removals. A window subscribes after the snapshot
and clears its list when the session ends. The Core's bind remains set by
`station_bind` in `nereusd.conf`; the desktop page shows it read-only.

The Core runs the app's existing TCI server on its own radio model, so a TCI
app at the station (the RF2K-S first) hears the Core's radio: the init
burst, `vfo:` as the Core's slices move, `split_enable:` (always false:
NereusSDR has no split) and the rest of the protocol a local window's server
speaks. It listens where every station listener does (see "Where the Core
accepts station devices"): the station network, and always the Core's own
computer (127.0.0.1), so apps there reach it.

It transmits for no app until remote transmit: the init burst says
`receive_only:true` and `tx_enable:<rx>,false`; `trx:<rx>,true` touches no
MOX, takes no transmit audio lock and is answered `trx:<rx>,false`; transmit
audio frames are dropped. The Core listens on the station address and on
its own computer separately: what binds is kept and served, and an
address another program holds is retried while the switch is on, after
1, 2, 5 and 10 seconds and then every 30 seconds, without stopping the
server (a new `setStationTci` tries at once). `listening` is true while
any address serves, `stationAddress` names the station address only
while it serves, and `error` names the blocked one: "Another program on
the Core's computer is using port <port>, so apps there cannot reach the
Core's TCI server. The Core keeps trying." or "Another program is
using port <port> at the Core's address <address>, so devices on the
radio's network cannot reach the Core's TCI server. The Core keeps
trying." (both: "... on the Core's computer and at the Core's address
<address>, so the Core's TCI server cannot start. ..."). The RF-Kit keeps band
follow while the station address serves. The Core logs one line when an
address is first blocked and one when every address serves again, not
one per try. Nor does it let an app change the Core's
transmit configuration: `tx_profile_ex:<name>`, `xit_enable:<rx>,<bool>`
and `xit_offset:<rx>,<hz>` change nothing, are not broadcast, and are
answered to the asking app with the value the Core keeps (queries answer
as usual). The Core logs the plain reason "Apps cannot transmit through
the station's TCI server until remote transmit is ready." for each and
never puts it on the TCI wire.

The TCI compatibility settings (`TciEmulateExpertSDR3Protocol`,
`TciEmulateSunSDR2Pro` and the other `Tci` keys) are not seeded on the
Core: the server reads them from the Core's own settings, where no window
writes them, so it runs on their defaults (the two emulation keys read
True, as in a fresh window).

## Where the Core accepts station devices

One rule for every listener the Core opens for station devices: the
SmartSDR API listener on TCP 4992 (the Power Genius and Tuner Genius
connect there), the Power Genius and Tuner Genius discovery on UDP 9008 and
9010, and the station TCI server. A desktop window without a Core sets none
of this and binds as it always has.

- **The station network** is `station_bind` in `nereusd.conf` when set: an
  address of the Core's computer (another network), or `0.0.0.0` for every
  network. Empty, it is the network that holds the radio: the Core's
  address on the radio's subnet. The older name `station_tci_bind` (which
  covered the TCI server alone) is still read when `station_bind` is empty
  or absent; `packaging/nereusd.conf.sample` documents only `station_bind`.
- **The Core's own computer** (127.0.0.1) is always accepted as well, so an
  app or amplifier program on that computer reaches it. `0.0.0.0` already
  covers it.
- **Before a radio connects** (and with no `station_bind`), only the Core's
  own computer is accepted.
- **When the radio's address changes** every listener moves to the new
  network: a running listener restarts on the new addresses, and a device
  connected on the old network is dropped and reconnects.
- **TCP listeners** (4992 and the TCI server) listen on those addresses
  only. A station address the Core's computer does not have is a failed
  listen (`fourO3AListenerError`, `stationTci.error`), never a quiet
  fallback to the Core's computer alone.
- **Discovery sockets** still open on every address, because the Power
  Genius and Tuner Genius announce by broadcast and a socket bound to one
  address hears no broadcast. The Core ignores any announcement whose
  sender, or whose announced address, is not on the station network or the
  Core's own computer, so a device on another network is never identified
  or admitted. When this Power Genius (Tuner Genius) was heard only from
  another network (an announcement from its configured address or with the
  serial its own `info` reply gave; another amp's announcement does not
  count), or the configured address itself is off the station network,
  the refusal in `connectionError` says so and how to
  allow it: "The Power Genius at <address> is on a different network from
  the radio, and the Core accepts amplifiers and tuners only on the
  radio's network. To allow it, set station_bind in the Core's
  configuration file to the Core's address on that network (or 0.0.0.0 for every network), then
  restart the Core." (or the Tuner Genius).

The FlexRadio discovery beacon (UDP 4992, which lets a Power Genius or Tuner
Genius find the station) runs only while the radio's 4O3A switch is on and
`PGXL_BroadcastDiscovery` is `True`: with 4O3A off nothing listens on 4992,
so nothing is announced. This holds for a desktop window too. On the Core
the beacon announces the station network address, where 4992 listens
(with `station_bind = 0.0.0.0` it finds its own address as a desktop
window's does).

## The `accessoryData` object

Class `AccessoryDataModel`, key `accessoryData`, `accessoryDataVersion` 1
(the properties down to `rfkitAntenna4Label`) and 2 (the RF-Kit's
connection counts, last). Read-only: a window changes the policy, the output limit and a fault history
only through the commands below. The properties are grouped by change
notice: a delta carries every property of the group that changed, so a
fault list is not resent when a counter moves.

| Property | Kind | Meaning |
| --- | --- | --- |
| `faultRevision` | i64 | Moves by one each time any of the three fault lists changes on the Core. Compare only within one connection: it starts again when the Core restarts |
| `pgxlFaults`, `tgxlFaults`, `rfkitFaults` | utf8 | Each device's fault history: a JSON array of up to 10 fault records, newest first (see "The fault record"); `[]` when empty |
| `pgxlConnectedSinceMs`, `tgxlConnectedSinceMs` | i64 | When the Core's connection to the amp (tuner) started, ms since 1970 UTC on the Core's clock; 0 while not connected. A window works the uptime out from its own clock |
| `pgxlLastRttMs`, `tgxlLastRttMs` | i64 | The last measured response time, ms; 0 before any |
| `pgxlKeepaliveMissed`, `tgxlKeepaliveMissed` | i64 | Response checks that went unanswered |
| `pgxlReconnectCount`, `tgxlReconnectCount` | i64 | Automatic retries the Core has made |
| `pgxlFramesIn`, `pgxlFramesOut`, `tgxlFramesIn`, `tgxlFramesOut` | i64 | Lines received from and sent to the device |
| `pgxlBytesIn`, `pgxlBytesOut`, `tgxlBytesIn`, `tgxlBytesOut` | i64 | Bytes received and sent |
| `pgxlLastFrameMs`, `tgxlLastFrameMs` | i64 | When the last line arrived, ms since 1970 UTC; 0 before any |
| `pgxlFaultsSession`, `tgxlFaultsSession` | i64 | Fault state lines seen on this connection (the Tuner Genius has none) |
| `interlockMode` | enum | The transmit interlock mode (table below) |
| `interlockGraceMs` | i64 | Milliseconds after the amp enters operate during which the SWR check is skipped, 0 to 30000 |
| `interlockSwrGateEnabled` | bool | The SWR check is on |
| `interlockSwrGateMax` | f64 | The SWR ratio above which the check acts, 1.0 to 10.0 |
| `powerCapEnabled` | bool | The Power Genius output limit is on |
| `powerCapW` | i64 | The limit, W, 100 to 2000 |
| `powerCapExceeded` | bool | The amp's peak forward power is above the limit now |
| `powerCapAlertText` | utf8 | The last alert in user words, for example "Power Genius output 1600 W is above the 1500 W limit."; empty before any |
| `powerCapAlertCount` | i64 | Moves by one each time the output passes the limit (again, after it was back at or below it). It follows `powerCapAlertText` in the delta |
| `tuneMemory` | utf8 | The Tuner Genius tune memory: a JSON array, sorted by band then antenna, of `{"antenna":1-3,"band":"20m","c1":0-255,"l":0-255,"c2":0-255,"savedAtMs":...}` (`band` is the app's band key: `160m` to `6m`, `GEN`, `WWV`, `XVTR` and the broadcast bands) |
| `autoTuneMemoryRecall` | bool | The Core recalls the memory on a band or antenna change (the recall itself is a tune and waits for remote transmit on a receive-only Core) |
| `tgxlAntenna1Label` to `tgxlAntenna3Label` | utf8 | The Tuner Genius antenna names; empty means the default "ANT N" |
| `rfkitAntenna1Label` to `rfkitAntenna4Label` | utf8 | The RF-Kit antenna names; empty means the default |
| `rfkitConnectedSinceMs` | i64 | When the Core's connection to the RF2K-S was admitted, ms since 1970 UTC on the Core's clock; 0 while not connected (version 2) |
| `rfkitPollsOk`, `rfkitPollsFailed` | i64 | REST requests to the amp that were answered, and that failed, since the Core started (version 2) |
| `rfkitReconnectCount` | i64 | Automatic retries the Core has scheduled for the current address (version 2) |
| `rfkitLastPollMs` | i64 | When the amp last answered, ms since 1970 UTC; 0 before any (version 2) |
| `rfkitRttAvgMs` | i64 | The amp's average response time over its last ten polls, in ms; 0 before any (version 3) |

The RF-Kit counts are the Core's `Rf2ksConnection` counters, which move
with every REST request (several a second); the Core reads them once a
second and when the amp connects or drops, so a window hears at most one
delta a second for them.

`interlockMode`:

| Value | Name | Meaning |
| --- | --- | --- |
| 0 | `disabled` | The interlock never holds back transmit |
| 1 | `warn` | Transmit goes ahead; the operator is warned when the amp is present but not operating, or the SWR check trips |
| 2 | `block` | Transmit is refused in those cases |

**The interlock is enforced on the Core.** The Core evaluates the policy
on every transmit request, with its own amp state and SWR
(`MoxController`, unchanged). A window views and changes the policy only;
a change is applied by the Core, saved there, takes effect from the next
transmit request, and comes back to every window on `accessoryData`. A
change never keys anything. While a Core is receive-only it transmits for
nobody, so the policy has nothing to act on until remote transmit; it is
kept and shown so the station is set up when it does. Who may change it:
the window connected to the Core (the Core serves one window at a time and
the newest connection takes over, until remote transmit brings asking
first).

**The power-cap alert is raised on the Core.** From each Power Genius
status line with a peak forward power, while the limit is on: above the
limit raises one alert (`powerCapExceeded` true, a new
`powerCapAlertText`, `powerCapAlertCount` up by one); at or below it
re-arms (`powerCapExceeded` false). A window shows the alert when the count
moves while `powerCapExceeded` is true, and not for the count it finds when
it first attaches. A local window runs the same code in process.

**Counters.** The Core counts on its own connection to each device for as
long as it runs (a local window counts on its own connection the same way).
A remote window shows the Core's counters and never its own: its accessory
connections stay idle.

## The `accessorySettings` object

Class `AccessorySettingsModel`, key `accessorySettings`, sent to an app at
minor 11 when the Core offers `remotePgxlControlVersion` 3 or
`remoteTgxlControlVersion` 1. Read-only: the settings the Power Genius and
the Tuner Genius keep themselves, as the Core last heard them from the
device (a Revert read, or a change the device took), and the device's last
answer to a window's request. A window changes them only through the
commands below. Two change notices: a delta carries every `pgxl*` property
when the amp's side changed, every `tgxl*` one when the tuner's did.

| Property | Kind | Meaning |
| --- | --- | --- |
| `pgxlNickname`, `tgxlNickname` | utf8 | The device's name; empty until the Core has heard it |
| `pgxlBiasMode` | utf8 | `ClassA` or `ClassAB`; empty until heard |
| `pgxlFanMode` | utf8 | `Auto`, `Quiet` or `Continuous`; empty until heard |
| `pgxlLedIntensity` | i64 | 0 to 100; -1 until heard |
| `pgxlNetworkKnown`, `tgxlNetworkKnown` | bool | The four network values below are the device's (it reported or took them) |
| `pgxlDhcp`, `tgxlDhcp` | bool | The device takes its address from DHCP |
| `pgxlAddress`, `pgxlNetmask`, `pgxlGateway`, `tgxlAddress`, `tgxlNetmask`, `tgxlGateway` | utf8 | Its fixed address, netmask and gateway (may be empty) |
| `pgxlAnswer`, `tgxlAnswer` | utf8 | The device's last answer in user words (table below); show this |
| `pgxlAnswerAccepted`, `tgxlAnswerAccepted` | bool | False when the device did not take the request, or went offline first |
| `pgxlAnswerCount`, `tgxlAnswerCount` | i64 | Moves by one with each new answer; it follows the answer and its acceptance in the delta |

The Tuner Genius has no bias, fan or LED setting; there are no `tgxl`
properties for them. When the Core's address, radio or 4O3A switch for a
device changes, the values are forgotten (the last answer stays).

**What the Core sends the device.** Each command is sent as exactly the
line a local window's Advanced page sends through its own connection
(`PgxlAdvancedPage.cpp`, `TgxlAdvancedPage.cpp`; the wire forms are
`PgxlConnection.cpp`'s and `TgxlConnection.cpp`'s, "From FlexRadio wiki
spec" and design section 6.4), framed `C<seq>|<command>`:

| Command | Sent to the device | Local page control |
| --- | --- | --- |
| `setPgxlName`, `setTgxlName` | `setup nickname=<name>` | Identity: Nickname (on edit) |
| `setPgxlHardware` `biasMode` | `setup bias=a` (`ClassA`) or `setup bias=ab` (`ClassAB`) | Hardware: Bias Mode |
| `setPgxlHardware` `fanMode` | `setup fan=auto`, `setup fan=quiet` or `setup fan=continuous` | Hardware: Fan Mode |
| `setPgxlHardware` `ledIntensity` | `setup led=<0-100>` | Hardware: LED Intensity |
| `setPgxlNetwork`, `setTgxlNetwork` | `ifconf address=<ip> netmask=<mask> gateway=<gw> dhcp=<true\|false>` | Network: Apply Network Settings |
| `savePgxlSettings`, `saveTgxlSettings` | `save` | Save & Reboot |
| `readPgxlSettings`, `readTgxlSettings` | `setup read`, then `ifconf read` | Revert |

The Core matches the device's `R<seq>|<code>|<body>` answer by its
sequence. Code 0 is taken: the value is published, and a read's body
(`nickname=`, `bias=`, `fan=`, `led=`; `dhcp=`, `ip=`, `netmask=`,
`gateway=`, the fields the local page reads) replaces the Core's values.
Any other code is not taken and nothing changes. Beside each change the
Core saves the setting the local page saves (`PGXL_Nickname`,
`PGXL_BiasMode`, `PGXL_FanMode`, `PGXL_LedIntensity`, `TGXL_Nickname`, and
`TGXL_Nickname` from a Tuner Genius read, as the local page does). None of
these commands keys a transmitter, puts the amp in operate, tunes or
changes an antenna.

Answers (`<device>` is "Power Genius" or "Tuner Genius"):

| When | `...Answer` | `...AnswerAccepted` |
| --- | --- | --- |
| A request left for the device | "Sent to the `<device>`. Waiting for its answer." | true |
| Name taken, or not | "The `<device>` took the new name." / "The `<device>` did not take the new name." | true / false |
| Bias, fan or LED taken, or not | "The Power Genius took the new setting." / "The Power Genius did not take the new setting." | true / false |
| Network taken, or not | "The `<device>` took the new network settings." / "... did not take the new network settings." | true / false |
| Save acknowledged, or not | "The `<device>` is saving its settings and restarting." / "The `<device>` did not save its settings." | true / false |
| Revert read answered, or not | "The `<device>` sent its settings." then "The `<device>` sent its network settings." / "... did not send its settings." / "... did not send its network settings." | true / false |
| The device went away with a request waiting | "The `<device>` went offline before it answered." | false |
| No answer within 10 seconds of the request (the Core's clock; a later answer to it is ignored) | "The `<device>` did not answer. Try again." | false |

**Asking first.** A window asks before it sends a network change or Save &
Reboot, and sends nothing without a yes. Save & Reboot asks the local
window's own question, word for word ("Sending `save` will persist your
configuration to flash and reboot the PGXL. ...", titled "Save & Reboot
PGXL" or "Save & Reboot TGXL"). Before new network settings, local and
remote windows alike (operator decision of 2026-09-24) ask, titled "Apply
Network Settings", in words true in both: "The Power Genius will switch to
these network settings. If NereusSDR cannot reach it afterwards, enter its
new address for the Power Genius on the Peripherals page and connect
again." (or the Tuner Genius); nothing is sent, local or remote, without a
yes. A remote window's Network section shows the same words; the local
page's section keeps its own warning, which names Scan LAN (not offered in
a remote window). The iPhone app asks the same question. Before asking, either window checks the
setting as the Core does (see Refusals) and shows the reason instead.

## The 4O3A, RF-Kit and transmit fields on the `radio` object

Class `RadioModel`, key `radio`, sent to every app.

| Property | Kind | Direction | Meaning |
| --- | --- | --- | --- |
| `fourO3AEnabled` | bool | Core to window | The station's 4O3A switch (PGXL, TGXL and the SmartSDR listener) for its radio |
| `fourO3AListening` | bool | Core to window | The Core's SmartSDR listener (TCP 4992) is accepting connections |
| `fourO3AListenerError` | utf8 | Core to window | Why the listener could not start; empty otherwise |
| `rfKitEnabled` | bool | Core to window | The station's RF-Kit switch for its radio. Changed by `setRfKitEnabled`; a raw write is refused (see Refusals) |
| `transmitting` | bool | Core to window | The Core's radio is keyed: true from the moment the Core's MOX controller starts a key from any source (its MOX button, a hardware PTT, CAT, TCI, TUNE or two-tone) until its hand-back to receive ends. A raw write is refused |

`transmitting` is what a window reads for "on the air". The `transmit`
object's `mox` is not: the Core writes that only while it has no MOX
controller. An older Core does not send `transmitting`, so a window reads
it as false there and the Core's own refusal still holds. Today's Core is
receive-only and refuses every key, so it stays false until remote
transmit.

## Commands

Task 42 adds `amp.operate {}`, `amp.standby {}`, `tuner.tune {}`,
`tuner.operate {on: bool}`, `tuner.bypass {on: bool}`,
`tuner.antenna {port: i64}` (1 to 3), `rfkit.operate {}`,
`rfkit.standby {}` and `rfkit.antenna {port: i64}` (1 to 4), all at
minor 11 with `accessoryTxVersion` 1. The station checks session transmit
admission before sending any device command. An idle holder is included in
the shared-setting confirmation for the non-carrier actions; another
device's change waits while that holder transmits. `tuner.tune` follows
the transmit holder's keying rules, taking unheld transmit first. The
RF2K-S tuner's own controls remain unavailable until its firmware accepts
commands; use its front panel.

The older switch commands `setPgxlOperate`, `setTgxlOperate`,
`setTgxlBypass`, `setTgxlAntenna`, `setRfKitOperate` and
`setRfKitAntenna` use the same session transmit admission. Their existing
wire arguments and capability declarations remain available for older apps,
but an app without remote transmit permission now gets a plain refusal
before any shared-setting question or device action. A local Core window
continues to operate its own admitted accessories through the local model.

Each is a `command.invoke` with exactly the arguments shown, in exactly
these wire kinds. The answer is a `command.result`; `accepted: true` means
the Core took the request (see "Accepted is not connected").

| Verb | Arguments | Needs | Effect |
| --- | --- | --- | --- |
| `configureTgxl` | `host` (utf8), `port` (i64, 1 to 65535) | minor 4, `remoteTgxlConfigVersion` 1 | Saves the TGXL address for the Core's radio and starts connecting |
| `disconnectTgxl` | none | minor 4, `remoteTgxlConfigVersion` 1 | Cancels an attempt or closes the connection; the address stays |
| `setFourO3AEnabled` | `enabled` (bool) | minor 4, `remoteFourO3AControlVersion` 1 | Turns the station's 4O3A switch on or off for its radio |
| `configurePgxl` | `host` (utf8), `port` (i64, 1 to 65535) | minor 11, `remotePgxlControlVersion` 2 | Saves the PGXL address for the Core's radio and starts connecting and identifying (see "How the Core identifies the PGXL") |
| `disconnectPgxl` | none | minor 11, `remotePgxlControlVersion` 2 | Cancels an attempt or closes the connection in any phase; nothing is redialled; the address stays |
| `setPgxlConnectionSettings` | `autoReconnect` (bool), `keepaliveSec` (i64, 1 to 3600), `pingSec` (i64, 0 to 3600; 0 is off) | minor 11, `remotePgxlControlVersion` 2 | Saves the three station-wide PGXL connection settings on the Core and applies them to the running connection: automatic retry off drops a pending retry (the phase becomes `disconnected`), a running keepalive takes the new interval, the Core pings the amp every `pingSec` while connected |
| `setRfKitEnabled` | `enabled` (bool) | minor 11, `remoteRfKitControlVersion` 2 | Turns the station's RF-Kit switch on or off for its radio. On with a saved address dials it through the identity check; off stops everything (`disabled`) |
| `configureRfKit` | `host` (utf8), `port` (i64, 1 to 65535) | minor 11, `remoteRfKitControlVersion` 2 | Saves the RF2K-S address for the Core's radio and starts connecting and identifying (see "How the Core identifies the RF2K-S") |
| `disconnectRfKit` | none | minor 11, `remoteRfKitControlVersion` 2 | Cancels an attempt or closes the connection in any phase; nothing is redialled; the address and the switch stay |
| `resetRfKitError` | none | minor 11, `remoteRfKitControlVersion` 3 | Sends the admitted amp the request the local RF-Kit page's "Reset amp error state" button sends: `POST /error/reset` on its REST interface (`Rf2ksConnection::resetError`). It clears the amp's error; it does not operate the amp, change its antenna or key anything |
| `setRfKitOperate` | `on` (bool) | minor 11, `remoteRfKitControlVersion` 4 | Puts the amp in operate (true) or standby (false) (see "Operating the RF-Kit amplifier"). Keys nothing |
| `setRfKitAntenna` | `port` (i64, 1 to 4) | minor 11, `remoteRfKitControlVersion` 4 | Switches the amp to that internal antenna (see "Operating the RF-Kit amplifier") |
| `setRfKitTciMode` | none | minor 11, `remoteRfKitControlVersion` 4 | Puts the amp in TCI mode, as the RF-Kit page's "Set amp to TCI mode" |
| `setRfKitAddress` | `host` (utf8), `port` (i64, 1 to 65535) | minor 11, `remoteRfKitControlVersion` 4 | Saves the RF2K-S address for the Core's radio without dialling |
| `setStationTci` | `enabled` (bool), `port` (i64, 1024 to 65535) | minor 11, `stationTciVersion` 1 | Saves the station's TCI switch and port on the Core and starts or stops its station TCI server. The Core keeps them across window sessions, other apps connecting and restarts |
| `setStationTciOptions` | `emulateExpertSdr3`, `emulateSunSdr2Pro`, `cwluBecomesCw`, `sendInitialState` (bool) | minor 11, `stationTciVersion` 2 | Saves the Core's four TCI compatibility and initial-state options for new clients; changes no radio setting |
| `disconnectStationTciClient` | `id` (utf8) | minor 11, `stationTciVersion` 2 | Closes only the named app on the Core's station TCI server; an unknown id is refused |
| `setStationTciSettings` | one or more of `rateLimitMs` (i64, 0 to 1000; 0 sends every change), `cwBecomesCwuAbove10mhz` (bool), `iqSwap` (bool), `alwaysStreamIq` (bool), `audioBlockSamples` (i64, 100 to 2048), `txChannel` (i64: 0 Left, 1 Right, 2 Both), `rxSensorIntervalMs` (i64, 30 to 1000), `txSensorIntervalMs` (i64, 30 to 1000), `forgetRx2VfoBOnDisconnect`, `useRx1VfoaForRx2Vfoa`, `copyRx2VfobToVfoa` (bool) | minor 11, `stationTciSettingsVersion` 1 (a peer that declared `stationTciSettings` 1) | Saves the rest of the TCI Server page's settings for the Core's own server, under the page's keys (`TciRateLimitMs`, `TciCwBecomesCwuAbove10mhz`, `TciIqSwap`, `TciAlwaysStreamIq`, `TciAudioStreamSamples`, `TciTxChannel` as its text, `TciRxSensorIntervalMs`, `TciTxSensorIntervalMs` and the three VFO quirk keys), all or nothing; a value out of range, a wrong kind or an unknown name is refused in plain words. The rate limit and "always stream IQ" reach the running server at once, the others when it next reads them. Refused while the radio is on the air. The same eleven are `stationTci`'s read-only properties for that peer; the page's defaults (100, false, true, false, 2048, 2, 200, 200, false, false, true) until changed |
| `setTxInterlockPolicy` | `mode` (i64, the `interlockMode` value 0 to 2), `graceMs` (i64, 0 to 30000), `swrGateEnabled` (bool), `swrGateMax` (f64, 1.0 to 10.0) | minor 11, `accessoryDataVersion` 1 | Sets the whole transmit interlock policy on the Core, which saves it and enforces it from the next transmit request. Keys nothing |
| `setPgxlPowerCap` | `enabled` (bool), `watts` (i64, 100 to 2000) | minor 11, `accessoryDataVersion` 1 | Sets the Power Genius output limit on the Core, which saves it and raises the alert from then on |
| `clearAccessoryFaults` | `device` (utf8: `pgxl`, `tgxl` or `rfkit`) | minor 11, `accessoryDataVersion` 1 | Empties that device's fault history on the Core (and in its settings) |
| `setPgxlName` | `name` (utf8; no line breaks or tabs; trimmed) | minor 11, `remotePgxlControlVersion` 3 | Sends the amp its new name (see "The `accessorySettings` object") and saves it on the Core |
| `setPgxlHardware` | exactly one of `biasMode` (utf8: `ClassA`, `ClassAB`), `fanMode` (utf8: `Auto`, `Quiet`, `Continuous`), `ledIntensity` (i64, 0 to 100) | minor 11, `remotePgxlControlVersion` 3 | Sends the amp that one hardware setting and saves it on the Core. The amp applies it after Save & Reboot |
| `setPgxlNetwork`, `setTgxlNetwork` | `dhcp` (bool), `address`, `netmask`, `gateway` (utf8: empty, or four numbers from 0 to 255 separated by dots; with `dhcp` false, `address` and `netmask` are required and `gateway` is empty or on the address's network) | minor 11, `remotePgxlControlVersion` 3 / `remoteTgxlControlVersion` 1 | Sends the device its network settings |
| `savePgxlSettings`, `saveTgxlSettings` | none | minor 11, `remotePgxlControlVersion` 3 / `remoteTgxlControlVersion` 1 | Sends `save`: the device keeps its settings and restarts (about 20 seconds offline; the Core reconnects as for any drop) |
| `readPgxlSettings`, `readTgxlSettings` | none | minor 11, `remotePgxlControlVersion` 3 / `remoteTgxlControlVersion` 1 | Revert: asks the device for its settings and network settings |
| `setPgxlOperate` | `on` (bool) | minor 11, `remotePgxlControlVersion` 4 | Puts the amp in operate (true) or standby (false) (see "Operating the Power Genius"). Keys nothing |
| `scanPgxlLan` | none | minor 11, `remotePgxlControlVersion` 4 | The Core listens for Power Genius announcements for three seconds and answers with `values` `devicesJson` (see "Scanning for the Power Genius and its saved address") |
| `setPgxlAddress` | `host` (utf8), `port` (i64, 1 to 65535) | minor 11, `remotePgxlControlVersion` 4 | Saves the PGXL address for the Core's radio without dialling |
| `setTgxlName` | `name` (as `setPgxlName`) | minor 11, `remoteTgxlControlVersion` 1 | Sends the tuner its new name and saves it on the Core |
| `setTgxlAntenna` | `port` (i64, 1 to 3) | minor 11, `remoteTgxlControlVersion` 2 | Switches the tuner to that antenna (see "Switching the Tuner Genius") |
| `setTgxlOperate` | `on` (bool) | minor 11, `remoteTgxlControlVersion` 2 | Puts the tuner in operate (true) or standby (false). At version 3, true also takes it out of bypass, in the same command |
| `setTgxlBypass` | `on` (bool) | minor 11, `remoteTgxlControlVersion` 2 | Bypasses the tuner (true) or takes it out of bypass (false) |
| `moveTgxlRelay` | `relay` (i64: 0 C1, 1 L, 2 C2), `direction` (i64: -1 or 1) | minor 11, `remoteTgxlControlVersion` 4 | Moves that one matching relay one step down or up (see "Switching the Tuner Genius"). Keys nothing |
| `scanTgxlLan` | none | minor 11, `remoteTgxlControlVersion` 4 | The Core listens for Tuner Genius announcements for three seconds and answers with `values` `devicesJson` (see "Scanning for the Tuner Genius and its saved address") |
| `setTgxlAddress` | `host` (utf8), `port` (i64, 1 to 65535) | minor 11, `remoteTgxlControlVersion` 4 | Saves the TGXL address for the Core's radio without dialling |

For these, `accepted: true` means the request left for the device; the
device's answer arrives on `accessorySettings` (for `setTgxlAntenna`,
`setTgxlOperate`, `setTgxlBypass` and `moveTgxlRelay`, on `tuner`; for
`setPgxlOperate`, on `amplifier`; for `setRfKitOperate`, `setRfKitAntenna`
and `setRfKitTciMode`, on `rfkit`). `scanTgxlLan` and `scanPgxlLan` are
accepted with their answer; `setTgxlAddress`, `setPgxlAddress` and
`setRfKitAddress` mean the address was saved. The desktop app shows a
refusal of one of them on the Advanced page that sent it (never the slice
notice).

## Refusals

A refusal is a `command.result` with `accepted: false`, or a
`property.result` with `accepted: false`, and a `reason`. There are no
numeric codes: the reason text is the identifier. Its exact wording is kept
from release to release, because older apps compare some of it exactly; an
app shows it through its own user-word translation and keeps the raw text in
its log.

The desktop app shows the Core's refusal of any accessory command in this
document (the Power Genius, Tuner Genius and RF-Kit commands, the
interlock, output limit and fault history commands, `setStationTci` and
`setFourO3AEnabled`) on its own accessory route, never as a slice notice.
A refusal of a request sent by the Power Genius, Tuner Genius or RF-Kit
page shows on that page only (with the Core's values kept) if that page
is still open and on screen when the refusal arrives; if Setup was
closed meanwhile, or the link to the Core dropped, it shows as a notice
instead, so no refusal is lost. Any other
accessory refusal (from the interlock page, the Peripherals and 4O3A
pages, an applet, the TCI switch) shows as a notice, and the interlock
page also reloads the Core's policy. An unrelated slice refusal does not
touch those pages.

Property writes:

| Write | Reason |
| --- | --- |
| Any property of `amplifier` | "The Core reports the amplifier's readings. They cannot be changed from this app." |
| Any property of `rfkit` | "The Core reports the RF-Kit amplifier's readings. They cannot be changed from this app." |
| `fourO3AEnabled`, `fourO3AListening`, `fourO3AListenerError` on `radio` | Refused with a diagnostic reason; use `setFourO3AEnabled` |
| `rfKitEnabled` on `radio` (what every app before this contract sent) | "Update this app to turn the RF-Kit amplifier on or off on this Core." The Core's switch stays |
| `transmitting` on `radio` | "The Core sets this itself; it cannot be changed from here." Nothing keys |
| Any property of `stationTci` | "The Core reports its TCI server here. Turn it on or off with this app's TCI switch." |
| Any property of `accessoryData` | "The Core keeps the amplifier and tuner records and settings. Change them from this app's Setup pages." |
| Any property of `accessorySettings` | "The Core reports the amplifier's and tuner's own settings. Change them from this app's Setup pages." |
| `operate` on `amplifier`, on a receive-only Core (every `nereusd` today) | "Operating the station's amplifier or tuner waits for remote transmit. This station is receive-only." |
| `tuner` telemetry (everything except the three below) | "TunerModel::<name> is hardware telemetry TunerModel only learns from the tuner itself; there is no remote-write path" |
| `tuner` `isOperate`, `isBypass`, `antennaA`, on a receive-only Core | "Operating the station's amplifier or tuner waits for remote transmit. This station is receive-only." Nothing changes on the Core and nothing is sent to the tuner. A current app uses `setTgxlOperate`, `setTgxlBypass` and `setTgxlAntenna` instead |

Commands:

| Verb | Reason |
| --- | --- |
| `configureTgxl`, `disconnectTgxl` below minor 4 | "Remote TGXL configuration requires a newer station protocol." |
| `configurePgxl`, `disconnectPgxl`, `setPgxlConnectionSettings` below minor 11 | "Update this app to set up the Power Genius on this Core." |
| `configurePgxl` with other arguments | "invalid host or port argument" |
| `disconnectPgxl` with arguments | "disconnectPgxl takes no arguments" |
| `setPgxlConnectionSettings` with other arguments | "setPgxlConnectionSettings requires an autoReconnect boolean and keepaliveSec and pingSec whole numbers" |
| `configurePgxl`, `disconnectPgxl`, `setPgxlConnectionSettings` on a Core that does not own its accessories | "This Core cannot change its amplifier and tuner settings." |
| `configurePgxl` with no radio | "Connect Core to a radio before configuring its PGXL." |
| `configurePgxl` with 4O3A off | "Enable 4O3A on Core before connecting the PGXL." |
| `configurePgxl` with a bad address | "Enter a valid PGXL IP address or hostname and TCP port 1 to 65535." |
| `setPgxlConnectionSettings` out of range | "Enter a keepalive of 1 to 3600 seconds and a ping of 0 to 3600 seconds." |
| `configureRfKit`, `disconnectRfKit`, `setRfKitEnabled` below minor 11 | "Update this app to set up the RF-Kit amplifier on this Core." |
| `resetRfKitError` below minor 11 | "Update this app to reset the RF-Kit amplifier's error on this Core." |
| `resetRfKitError` on a Core that does not own its accessories | "This Core cannot reset its RF-Kit amplifier's error." |
| `resetRfKitError` with arguments | "The request to reset the RF-Kit amplifier's error was not understood." |
| `resetRfKitError` with no amp admitted | "The Core is not connected to the RF-Kit amplifier." |
| `resetRfKitError` from an app whose Core lacks `remoteRfKitControlVersion` 3 (the app's own words, nothing sent) | "This Core does not let this app reset the RF-Kit amplifier's error. Updating the Core may help." |
| `configureRfKit` with other arguments | "invalid host or port argument" |
| `disconnectRfKit` with arguments | "The disconnect request for the RF-Kit amplifier was not understood." |
| `setRfKitEnabled` with other arguments | "The request to turn the RF-Kit amplifier on or off was not understood." |
| `configureRfKit`, `disconnectRfKit`, `setRfKitEnabled` on a Core that does not own its accessories | "This Core cannot change its amplifier and tuner settings." |
| `configureRfKit` with no radio | "Connect the Core to a radio before setting up its RF-Kit amplifier." |
| `setRfKitEnabled` with no radio | "Connect the Core to a radio before turning its RF-Kit amplifier on or off." |
| `configureRfKit` with the RF-Kit switch off | "Turn on the RF-Kit amplifier on the Core before connecting it." |
| `configureRfKit` with a bad address | "Enter the RF-Kit amplifier's IP address or host name, and a port from 1 to 65535." |
| `setStationTci` below minor 11 | "Update this app to turn the station's TCI server on or off." |
| `setStationTci` on a Core without a station TCI server | "This Core has no TCI server for the station." |
| `setStationTci` with other arguments | "The request to turn the station's TCI server on or off was not understood." |
| `setStationTci` with a port outside 1024 to 65535 | "Choose a TCI port from 1024 to 65535." |
| `setTxInterlockPolicy`, `setPgxlPowerCap`, `clearAccessoryFaults` below minor 11 | "Update this app to change the station's amplifier and tuner settings on this Core." |
| `setTxInterlockPolicy`, `setPgxlPowerCap`, `clearAccessoryFaults` on a Core that does not own its accessories | "This Core cannot change its amplifier and tuner settings." |
| `setTxInterlockPolicy` with other arguments | "The request to change the transmit interlock was not understood." |
| `setTxInterlockPolicy` out of range | "Choose an interlock mode, a grace period of 0 to 30000 ms and an SWR limit from 1.0 to 10.0." |
| `setPgxlPowerCap` with other arguments | "The request to change the Power Genius output limit was not understood." |
| `setPgxlPowerCap` out of range | "Choose an output limit from 100 to 2000 W." |
| `clearAccessoryFaults` with other arguments, or another device | "The request to clear the fault history was not understood." |
| `setPgxlName`, `setPgxlHardware`, `setPgxlNetwork`, `savePgxlSettings`, `readPgxlSettings` below minor 11 | "Update this app to change the Power Genius's own settings on this Core." |
| `setTgxlName`, `setTgxlNetwork`, `saveTgxlSettings`, `readTgxlSettings` below minor 11 | "Update this app to change the Tuner Genius's own settings on this Core." |
| Any of the nine on a Core that does not own its accessories | "This Core cannot change its amplifier and tuner settings." |
| Any of the nine while the Core is not connected to the device (or has not admitted it) | "The Core is not connected to the Power Genius." / "The Core is not connected to the Tuner Genius." |
| `setPgxlName`, `setTgxlName` with other arguments | "The request to rename the Power Genius was not understood." (or Tuner Genius) |
| `setPgxlName`, `setTgxlName` with a line break or tab | "Enter a name without line breaks or tabs." |
| `setPgxlHardware` with no argument, two, or another | "The request to change the Power Genius hardware was not understood." |
| `setPgxlHardware` out of range | "Choose Class A or Class AB.", "Choose Auto, Quiet or Continuous." or "Choose an LED brightness from 0 to 100." |
| `setPgxlNetwork`, `setTgxlNetwork` with other arguments | "The request to change the Power Genius network settings was not understood." (or Tuner Genius) |
| `setPgxlNetwork`, `setTgxlNetwork` with an address that is not four numbers from 0 to 255 | "Enter each address as four numbers from 0 to 255 separated by dots." |
| `setPgxlNetwork`, `setTgxlNetwork` with `dhcp` false and no address or no netmask | "Without DHCP, enter an address and a netmask." |
| `setPgxlNetwork`, `setTgxlNetwork` with `dhcp` false and a netmask that is not ones then zeros, is all zeros, or is 255.255.255.255 | "Enter a netmask such as 255.255.255.0." |
| `setPgxlNetwork`, `setTgxlNetwork` with `dhcp` false and an address a device cannot use (0.0.0.0, 127.x, multicast, 240 and above, 255.255.255.255, or the subnet's network or broadcast address, except on a 255.255.255.254 link) | "Enter an address the device can use on your network." |
| `setPgxlNetwork`, `setTgxlNetwork` with `dhcp` false and a gateway off the address's network, equal to it, or at the subnet's network or broadcast address | "Enter a gateway on the same network as the address, or leave it empty." |
| `savePgxlSettings`, `saveTgxlSettings` with arguments | "The request to save and restart the Power Genius was not understood." (or Tuner Genius) |
| `readPgxlSettings`, `readTgxlSettings` with arguments | "The request to read the Power Genius settings was not understood." (or Tuner Genius) |
| `setTgxlAntenna`, `setTgxlOperate`, `setTgxlBypass` below minor 11 | "Update this app to switch the Tuner Genius on this Core." |
| `setTgxlAntenna`, `setTgxlOperate`, `setTgxlBypass` on a Core that does not own its accessories | "This Core cannot change its amplifier and tuner settings." |
| `setTgxlAntenna` with other arguments | "The request to switch the Tuner Genius antenna was not understood." |
| `setTgxlOperate` with other arguments | "The request to put the Tuner Genius in operate or standby was not understood." |
| `setTgxlBypass` with other arguments | "The request to bypass the Tuner Genius was not understood." |
| `setTgxlAntenna` with a port outside 1 to 3 | "Choose Tuner Genius antenna 1, 2 or 3." |
| `setTgxlAntenna`, `setTgxlOperate`, `setTgxlBypass` while the radio is on the air (MOX, TUNE or two-tone, or the hand-back to receive after MOX) | "The radio is on the air. Try again when it stops." |
| `setTgxlAntenna`, `setTgxlOperate`, `setTgxlBypass` while the Core has not admitted a tuner | "The Core is not connected to the Tuner Genius." |
| `setTgxlAntenna` on a tuner with no antenna switch | "This Tuner Genius has no antenna switch." |
| `setTgxlAntenna`, `setTgxlOperate`, `setTgxlBypass` from an app whose Core lacks `remoteTgxlControlVersion` 2 (the app's own words, nothing sent) | "This Core does not let this app switch the Tuner Genius. Updating the Core may help." |
| `moveTgxlRelay`, `scanTgxlLan`, `setTgxlAddress` below minor 11 | "Update this app to switch the Tuner Genius on this Core." |
| `moveTgxlRelay`, `scanTgxlLan`, `setTgxlAddress` on a Core that does not own its accessories | "This Core cannot change its amplifier and tuner settings." |
| `moveTgxlRelay` with other arguments, a `relay` outside 0 to 2 or a `direction` other than -1 or 1 | "The request to move a Tuner Genius relay was not understood." |
| `scanTgxlLan` with arguments | "The request to scan for a Tuner Genius was not understood." |
| `setTgxlAddress` with other arguments | "The request to save the Tuner Genius address was not understood." |
| `moveTgxlRelay` while the radio is on the air (MOX, TUNE or two-tone, or the hand-back to receive after MOX) | "The radio is on the air. Try again when it stops." |
| `moveTgxlRelay` while the Core has not admitted a tuner | "The Core is not connected to the Tuner Genius." |
| `setTgxlAddress` with no radio | "Connect the Core to a radio before setting up its Tuner Genius XL." |
| `setTgxlAddress` with a host that is not blank and not an IP address or host name, or a port outside 1 to 65535 | "Enter the Tuner Genius XL's IP address or host name, and a port from 1 to 65535." |
| `setPgxlOperate`, `scanPgxlLan`, `setPgxlAddress` below minor 11 | "Update this app to switch the Power Genius on this Core." |
| `setPgxlOperate`, `scanPgxlLan`, `setPgxlAddress` on a Core that does not own its accessories | "This Core cannot change its amplifier and tuner settings." |
| `setPgxlOperate` with other arguments (or `on` not a bool) | "The request to put the Power Genius in operate or standby was not understood." |
| `scanPgxlLan` with arguments | "The request to scan for a Power Genius was not understood." |
| `setPgxlAddress` with other arguments | "The request to save the Power Genius address was not understood." |
| `setPgxlOperate` while the radio is on the air (MOX, TUNE or two-tone, or the hand-back to receive after MOX) | "The radio is on the air. Try again when it stops." |
| `setPgxlOperate` while the Core is not connected to the amp | "The Core is not connected to the Power Genius." |
| `setPgxlOperate` while the amp is still switching from an earlier operate command (not `on` false while an `operate=1` is unconfirmed; iPhone app plan Task 77 fix round 4) | "The amplifier is still switching. Try again in a moment." |
| `setPgxlOperate` while a Tuner Genius cycle runs on the Core (its standby wait, tune carrier and restore) or the Core's tuner reports its sweep (iPhone app plan Task 77 fix round 3) | "The tuner is tuning. Try again when it finishes." |
| `setPgxlAddress` with no radio | "Connect the Core to a radio before setting up its Power Genius." |
| `setPgxlAddress` with a host that is not blank and not an IP address or host name, or a port outside 1 to 65535 | "Enter the Power Genius's IP address or host name, and a port from 1 to 65535." |
| `setRfKitOperate`, `setRfKitAntenna`, `setRfKitTciMode`, `setRfKitAddress` below minor 11 | "Update this app to switch the RF-Kit amplifier on this Core." |
| `setRfKitOperate`, `setRfKitAntenna`, `setRfKitTciMode`, `setRfKitAddress` on a Core that does not own its accessories | "This Core cannot change its amplifier and tuner settings." |
| `setRfKitOperate` with other arguments (or `on` not a bool) | "The request to put the RF-Kit amplifier in operate or standby was not understood." |
| `setRfKitAntenna` with other arguments (or `port` not an i64) | "The request to switch the RF-Kit amplifier's antenna was not understood." |
| `setRfKitAntenna` with a port outside 1 to 4 | "Choose RF-Kit amplifier antenna 1, 2, 3 or 4." |
| `setRfKitTciMode` with arguments | "The request to put the RF-Kit amplifier in TCI mode was not understood." |
| `setRfKitAddress` with other arguments | "The request to save the RF-Kit amplifier address was not understood." |
| `setRfKitOperate`, `setRfKitAntenna`, `setRfKitTciMode` while the radio is on the air (MOX, TUNE or two-tone, or the hand-back to receive after MOX) | "The radio is on the air. Try again when it stops." |
| `setRfKitOperate`, `setRfKitAntenna`, `setRfKitTciMode` while the Core has not admitted an amp | "The Core is not connected to the RF-Kit amplifier." |
| `setRfKitAntenna` for an antenna the amp lists as disabled, or does not list once it has listed its antennas | "This antenna is not available on the RF-Kit amplifier." |
| `setRfKitAddress` with no radio | "Connect the Core to a radio before setting up its RF-Kit amplifier." |
| `setRfKitAddress` with a host that is not blank and not an IP address or host name, or a port outside 1 to 65535 | "Enter the RF-Kit amplifier's IP address or host name, and a port from 1 to 65535." |
| `setRfKitOperate`, `setRfKitAntenna`, `setRfKitTciMode`, `setRfKitAddress` from an app whose Core lacks `remoteRfKitControlVersion` 4 (the app's own words, nothing sent) | "This Core does not let this app put the RF-Kit amplifier in operate or standby, switch its antenna or TCI mode, or save its address. Updating the Core may help." |
| `setPgxlOperate`, `scanPgxlLan`, `setPgxlAddress` from an app whose Core lacks `remotePgxlControlVersion` 4 (the app's own words, nothing sent) | "This Core does not let this app put the Power Genius in operate or standby, scan for it or save its address. Updating the Core may help." |
| `moveTgxlRelay`, `scanTgxlLan`, `setTgxlAddress` from an app whose Core lacks `remoteTgxlControlVersion` 4 (the app's own words, nothing sent) | "This Core does not let this app move the Tuner Genius relays, scan for it or save its address. Updating the Core may help." |
| `setFourO3AEnabled` below minor 4 | "Remote 4O3A control requires a newer station protocol." |
| `configureTgxl` with other arguments | "invalid host or port argument" |
| `disconnectTgxl` with arguments | "disconnectTgxl takes no arguments" |
| `setFourO3AEnabled` with other arguments | "setFourO3AEnabled requires exactly one enabled boolean argument" |
| `configureTgxl`, `disconnectTgxl` on a Core that does not own its accessories | "This Core cannot change its amplifier and tuner settings." |
| `configureTgxl` with no radio | "Connect Core to a radio before configuring its TGXL." |
| `configureTgxl` with 4O3A off | "Enable 4O3A on Core before connecting the TGXL." |
| `configureTgxl` with a bad address | "Enter a valid TGXL IP address or hostname and TCP port 1–65535." |
| `setFourO3AEnabled` with no radio | "Connect Core to a radio before changing its 4O3A integration." |
| Any refusal without its own reason | "The Core did not switch the Tuner Genius.", "TGXL configuration was refused", "TGXL disconnect was refused", "4O3A master change was refused", "PGXL configuration was refused", "PGXL disconnect was refused", "PGXL settings change was refused", "The Core did not set up the RF-Kit amplifier.", "The Core did not disconnect the RF-Kit amplifier.", "The Core did not change its RF-Kit amplifier switch.", "The Core did not change its TCI server.", "The Core did not change the transmit interlock.", "The Core did not change the Power Genius output limit.", "The Core did not clear the fault history." |

The Core checks every network request itself: it is the only gate for the
iPhone app and other apps, whatever a window asks first. A local window's
Advanced page and a remote window's page make the same check before they
ask or send, and show the same words under the Network section.

The desktop app does not send a command its Core did not offer. It shows
"The station does not support remote TGXL configuration.", "The station
does not support remote PGXL configuration.", "The station does not
support remote 4O3A control.", "This Core does not offer RF-Kit amplifier
setup to this app.", "This Core has no TCI server for the station.",
"This Core does not share its amplifier and tuner settings with this app."
or "This Core does not let this app change the Power Genius's own settings.
Updating the Core may help." (or the Tuner Genius's) instead.

PGXL identity reasons in `amplifier`.`connectionError` (diagnostic text; an
app shows them in its own words):

| Reason | When |
| --- | --- |
| "Expected PowerGeniusXL at the connected endpoint; observed <product> (serial <serial>)." | The announcement at the address names another product |
| "No matching PGXL discovery announcement for <address>:<port>. Check the amplifier address, port and station LAN discovery." | No announcement from the address in the three-second window |
| "PGXL identity serial mismatch: expected <announced>, observed <info>" | The two serials differ |
| "PGXL native info failed with code <hex>" | The `info` reply carried an error code |
| "PGXL native info omitted a nonempty serial" | The `info` reply had no serial |
| "PGXL native identity timed out" | No `info` reply within five seconds |
| "PGXL discovery approval timed out for serial <serial>" | The `info` reply came, the matching announcement did not, within five seconds |

## Core-owned settings

The Core keeps every accessory setting. A window changes one only through a
command above (or, for the settings not yet behind a command, not at all
from a remote window). Keys are the Core's `NereusSDR.settings` keys.

Per radio, under `hardware/<mac>/peripherals/`:

| Key | Value | Changed by |
| --- | --- | --- |
| `FourO3A_Enabled` | `True` / `False` | `setFourO3AEnabled` |
| `TGXL_ManualIp`, `TGXL_ManualPort` | text, whole number | `configureTgxl`, and `setTgxlAddress` (saved without dialling) |
| `PGXL_ManualIp`, `PGXL_ManualPort` | text, whole number | `configurePgxl`, and `setPgxlAddress` (saved without dialling) |
| `RfKit_Enabled` | `True` / `False` | `setRfKitEnabled` |
| `RfKit_ManualIp`, `RfKit_ManualPort` | text, whole number | `configureRfKit`, and `setRfKitAddress` (saved without dialling) |

Station-wide, behind `setStationTci`: `StationTci_Enabled` (`True` /
`False`, default `False`) and `StationTci_Port` (default 50001). Where the
server listens on the station network is `station_bind` in `nereusd.conf`
(older name `station_tci_bind`; empty: the Core's address on the radio's
subnet), the same for every station listener (see "Where the Core accepts
station devices").

Station-wide, behind `setPgxlConnectionSettings`: `PGXL_AutoReconnect`
(`True` / `False`, default `True`), `PGXL_KeepaliveSec` (default 30),
`PGXL_PingSec` (default 0 on the Core, off: the amp's reply to `ping` has
never been captured).

Station-wide, behind `setTxInterlockPolicy`: `PGXL_TxInterlockMode`
(`Disabled`, `Warn`, `Block`), `PGXL_TxInterlockGraceMs`, `PGXL_TxSwrGate`
(`True` / `False`), `PGXL_TxSwrGateMax`. Behind `setPgxlPowerCap`:
`PGXL_PowerCapEnabled`, `PGXL_PowerCapW`. Behind `clearAccessoryFaults`
(and written by the Core as it records faults): `PGXL_FaultHistory`,
`TGXL_FaultHistory`, `RfKit_FaultHistory`. All mirrored on `accessoryData`.
The Core writes its settings file within half a second of each of these
changes (one write for a burst), not only at a clean stop, so a power loss
at the station keeps them.

Station-wide, mirrored on `accessoryData` and changed by the desktop app as
station settings: `TGXL_Ant1_Label` to `TGXL_Ant3_Label`,
`RfKit_Ant1_Label` to `RfKit_Ant4_Label`, `TGXL_TuneMemory_Ant<N>_Band<B>`
(one compact JSON object per memory), `TGXL_AutoTuneMemoryRecall`. A
settings write of any key in this section (or the three above) from a
window reaches the Core's live objects at once (the interlock policy is
reloaded, the output limit and the object are refreshed, a fault history
written by an app that predates `clearAccessoryFaults` is reloaded), so an
older app's change is not held back until the Core restarts. Version 1 has
no command for the names and the tune memory: an app without station
settings shows them read-only.

Station-wide, saved by the Core beside the device commands (see "The
`accessorySettings` object"): `PGXL_Nickname` (`setPgxlName`),
`PGXL_BiasMode`, `PGXL_FanMode`, `PGXL_LedIntensity` (`setPgxlHardware`),
`TGXL_Nickname` (`setTgxlName`, and a Tuner Genius read). The network
settings live only in the device.

Station-wide, the Power Genius tab's Pairing & Band Source section: there is
no device command behind it (the local page only saves the settings, which
the Core reads when it next pairs the amp), so the desktop app changes
`PGXL_PairAttempt` (`True` / `False`), `PGXL_TxAnt` (`ANT1` / `ANT2`) and
`PGXL_FlexAmpSlice` (`A` / `B`) as station settings, as it changes the
antenna names; they take effect on the Core at the next Power Genius
connection.

Station-wide, not yet behind a command: `PGXL_BroadcastDiscovery`,
`PGXL_BroadcastNickname`, `PGXL_FlexRadioSerial`, `PGXL_AntMap`,
`PGXL_PairModel`, `PGXL_DiscoveryModel`, `TGXL_AutoReconnect`,
`TGXL_KeepaliveSec`.

Station-wide, the RF-Kit page's connection settings (with
`remoteRfKitControlVersion` 3): `RfKit_AutoReconnect` (`True` / `False`)
and `RfKit_PollIntervalMs` (250 to 5000; the connection clamps it). The
desktop app's Save writes them, with the four antenna names, as station
settings, and the Core applies them to its amp's connection at once, as
the local page's Save does. The amp's address goes with Connect
(`configureRfKit`).

## The fault record

The Core records every accessory's faults and keeps each device's last 10,
newest first, in its settings key (`PGXL_FaultHistory`,
`TGXL_FaultHistory`, `RfKit_FaultHistory`), written to its settings file
within half a second of each fault or clear, so the history survives a Core
restart, including one after a power loss. The same JSON array travels in `accessoryData` (`pgxlFaults`,
`tgxlFaults`, `rfkitFaults`); a window shows a new fault as soon as the
Core records it.

What the Core records:

| Device | `state` | When | `text` |
| --- | --- | --- | --- |
| PGXL | the amp's word, for example `FAULT` | A state word beginning `FAULT` after one that did not | "The Power Genius reported a fault." plus " Likely cause: high SWR." / " Likely cause: the amplifier was too hot." / " Likely cause: too much drive from the radio." when the readings point to one |
| TGXL | `link` | A connection the Core had admitted drops (never an operator's disconnect) | "The Tuner Genius stopped answering." |
| TGXL | `connection` | An attempt ends at an error (for example another device answering at the address), once per outage: the retries after it record nothing until the tuner is admitted again or the address changes, and a drop already recorded as `link` is that outage's one fault | "The Core could not connect to the Tuner Genius." |
| RF2K-S | `link` | The admitted amp stops answering | "The RF-Kit amplifier stopped answering." |
| RF2K-S | `identity` | `/info` names another device, or none | the identity reason (see "How the Core identifies the RF2K-S") |
| RF2K-S | `interface` | The amp reports a new error on its operating interface | "The RF-Kit amplifier reported a problem with how it follows the radio." |

```json
{"whenMs":1790000000000,"device":"pgxl","state":"FAULT","text":"The Power Genius reported a fault. Likely cause: high SWR.","detail":"","fwdAtFaultW":1820.0,"swrAtFault":2.85,"tempAtFaultC":78.0,"likelyCause":"SWR trip"}
```

| Field | Meaning |
| --- | --- |
| `whenMs` | When the Core saw it, milliseconds since 1970 UTC |
| `device` | `pgxl`, `tgxl` or `rfkit` |
| `state` | The amp's word, or the kind of problem (table above) |
| `text` | What happened, in user words; show this |
| `detail` | What the device or its connection said, as sent (the Tuner Genius connection reason, the RF-Kit interface error); may be empty. For a log or a details view, not a headline |
| `fwdAtFaultW` | PGXL: forward power at the fault, W (from the amp's `fwd`, dBm); 0 otherwise |
| `swrAtFault` | PGXL: SWR ratio at the fault (from the amp's return loss); 0 otherwise |
| `tempAtFaultC` | PGXL: temperature at the fault, degrees C; 0 otherwise |
| `likelyCause` | PGXL: `SWR trip`, `Overtemp`, `Drive too high` or `Unknown`; empty otherwise |

Records saved before this revision carry no `device`, `text` or `detail`;
the Core fills `device` from the key and `text` from `state` and
`likelyCause` when it reads them.

## What waits for remote transmit

Nothing in this contract keys a transmitter, operates an amplifier or
starts a tune carrier. The Tuner Genius's antenna, operate and bypass are
the one exception to "wait for remote transmit": they key nothing, so from
`remoteTgxlControlVersion` 2 a window switches them whenever the radio is
not on the air (operator ruling of 2026-09-24; see "Switching the Tuner
Genius"). From version 4 the relay nudges join them: a nudge keys nothing
and waits only while the radio is on the air. From
`remotePgxlControlVersion` 4 the Power Genius's OPERATE and STANDBY join
them too (`setPgxlOperate`, see "Operating the Power Genius"): operating
the amp keys nothing. From `remoteRfKitControlVersion` 4 the RF2K-S's
OPERATE and STANDBY and its antenna choice join them as well
(`setRfKitOperate`, `setRfKitAntenna`, see "Operating the RF-Kit
amplifier"). Until remote transmit, the Core refuses or does not offer:

- PGXL standby around a TGXL tune. A write of `amplifier` `operate` is
  refused with the receive-only reason above (a current app sends
  `setPgxlOperate`, which carries the on-the-air check); below
  `remotePgxlControlVersion` 4, PGXL OPERATE and STANDBY as well.
- TGXL TUNE (autotune) and tune-memory recall on a band change (each
  starts a tune, a tune carrier on the air).
- RF2K-S OPERATE and STANDBY, antenna choice, below
  `remoteRfKitControlVersion` 4. (The Core does put an admitted RF2K-S into
  TCI mode once when band follow starts; that chooses where the amp reads
  the radio's frequency from and keys nothing. Its error reset works from
  a remote window with `remoteRfKitControlVersion` 3, by operator ruling
  in the fix wave: it clears the amp's error and operates nothing.) RF2K-S
  TUNE and BYPASS stay hidden in every window: the amp's firmware has no
  request for them.
- Transmit over the Core's station TCI server (see "The `stationTci`
  object").
- Enforcement of the interlock policy, and the MOX RF-flow gate. These stay
  on the Core and are unchanged; viewing and changing the policy
  (`setTxInterlockPolicy`) comes before remote transmit and keys nothing.
- Tune-memory recall on a band change (a tune); the memory itself and its
  switch are readable and kept on the Core.

A `property.write` of `tuner` `isOperate`, `isBypass` or `antennaA` is
refused on a receive-only Core with the receive-only reason, before it can
reach the tuner (until Task 2 it reached `TunerModel::applyMirroredValue`, an
R2 path, whatever the policy). It stays refused: a current app switches
them with `setTgxlOperate`, `setTgxlBypass` and `setTgxlAntenna`, which
carry the on-the-air check.

Pairing is not operating: the Core pairs an admitted PGXL
(`flexradio ... ptt=LAN`) automatically, by operator decision of
2026-09-23. It keys nothing; the Core's own transmit refusals still apply.

A remote window shows these controls disabled: on a Core below
`remotePgxlControlVersion` 4, the Power Genius tab's Operate button with
the receive-only reason and the Power Genius applet's OPERATE with
"Amplifier control is not available from a remote window."; on a Core
below `remoteRfKitControlVersion` 4, the RF-Kit applet's OPERATE and
antenna buttons with that reason and the RF-Kit page's "Set amp to TCI
mode" button (the Core sets TCI mode itself while the station's TCI
server is on); and the Tuner Genius applet's TUNE
button with its transmit-permission reason (and its relay bars on a Core
below `remoteTgxlControlVersion` 4, its OPERATE and ANT buttons too below
2). The
RF-Kit page's "Reset amp error state" works from a remote window with
`remoteRfKitControlVersion` 3 (`resetRfKitError`, by operator ruling in
the fix wave): it clears the amp's error and operates nothing.

## Window behaviour

**One on-the-air rule in both windows** (group B fix wave, M5; the
operator's ruling 2026-09-25). A local window's Power Genius OPERATE and
STANDBY (the applet and the PowerGenius XL tab), RF-Kit OPERATE and
antennas, and Tuner Genius relays, ANT and OPERATE follow the rule a remote
window's do: while the radio is on the air (MOX, TUNE, two-tone, or the
hand-back to receive after MOX) each is disabled with "The radio is on the
air. Try again when it stops." (`RadioModel::onAirReason()`, the one
sentence both windows use), and a press that gets through anyway sends
nothing to this computer's accessory (`RadioModel::stationOnAirRefusal`,
the Core's own check). TUNE keeps its transmit gate. `tst_remote_peripherals`
(`localWindowAmpAndTunerSwitchesWaitOnTheAir`) checks each keyed and
unkeyed; the remote window's cases check the same in a remote window.
Parity mini-round (the operator's rulings of 2026-09-25): a local
window's RF-Kit "Set amp to TCI mode" joins them (it switches the amp).
Its buttons grey while `RadioModel::isCoreOnAir()` is true, which can
read false a moment before the Core's own check stops refusing (the
hand-back to receive); a local click refused then is never dropped
silently: `RadioModel::refuseLocalAccessorySwitchOnAir` sends it out on
`accessoryRequestRefused` with the same sentence a remote window's Core
sends back, and the window shows it as it shows the Core's
(`localClickRefusedAsTheRadioUnkeysSaysWhy`). Scan LAN, Host, Port and
Save wait in neither window.

A window reads `amplifier` and `rfkit` only while the Core offers them:

- Filled on attach from the `object.create` values, and updated by every
  delta. `false`, `0` and `standby` are values and are shown as such.
- The desktop applets show the gauges once `present` has been true, and keep
  the last values when it goes false.
- When the link to the Core is lost, the last values stay and a line says
  "Core disconnected. Power Genius readings are stale." (or "RF-Kit
  readings"). On a Core that does not offer the object: "This Core does not
  report its Power Genius to this app. Updating the Core may help." (or
  "its RF-Kit amplifier").
- A remote window never opens a connection to an accessory. A local window
  reads the same objects, fed by its own connections, and shows no stale
  line.
- With `remotePgxlControlVersion` 2, the desktop remote window's
  Peripherals page Power Genius row and its 4O3A Power Genius tab ask the
  Core (`configurePgxl`, `disconnectPgxl`, `setPgxlConnectionSettings`) and
  show the Core's `amplifier` phase, identity and reason. The button says
  Connect, Cancel (while `connecting`, `identifying` or `retrying`) or
  Disconnect. LAN scanning is offered remotely from version 4 (below). Below 2 the row and tab
  say "This Core does not offer Power Genius XL control to this app." A
  local window keeps its own connection and its existing row.
- With `remoteRfKitControlVersion` 2, the desktop remote window's RF-Kit
  page (CAT & Network > RF-Kit) asks the Core: its switch sends
  `setRfKitEnabled` and follows the Core's `rfKitEnabled`; Connect sends
  `configureRfKit` with the page's address; Disconnect sends
  `disconnectRfKit`; the status line is the Core's `rfkit` phase and
  identity. The RF-Kit applet's Disconnect or Reconnect asks the Core the
  same way. Below 2 the page says "This Core does not offer RF-Kit
  amplifier setup to this app." and changes nothing. With
  `remoteRfKitControlVersion` 3 the page's automatic retry, poll interval
  and four antenna names show the Core's values (following another
  window's change to them, except a field the operator has changed and
  whose saved value the Core has not yet echoed back) and Save writes them
  as station settings; Reset amp error state sends `resetRfKitError`, and a
  refusal shows in the status line. Below 3 those controls are shown,
  unchangeable, with "This Core does not let this app change these
  settings. Updating the Core may help."
- With `remoteRfKitControlVersion` 4 the desktop remote window's RF-Kit
  applet OPERATE sends `setRfKitOperate` (on from standby, off from
  operate) and ANT 1 to ANT 4 send `setRfKitAntenna`, while the Core is
  connected to the amp and the radio is off the air; OPERATE shows the
  amp's reported state and the lit ANT the amp's `activeAntennaNumber`
  from the `rfkit` object, not the click, and neither uses this computer's
  own connection. An ANT the amp lists as disabled, or does not list, is
  disabled with "This antenna is not available on the RF-Kit amplifier."
  The RF-Kit page's "Set amp to TCI mode" sends `setRfKitTciMode` and the
  amp's `operationalInterface` follows. While the Core reports the radio
  on the air, OPERATE, ANT and "Set amp to TCI mode" are disabled with
  "The radio is on the air. Try again when it stops." (the page's Host,
  Port and Save stay live: they only save); while the Core is not connected to the amp, OPERATE, ANT and
  "Set amp to TCI mode" with "The Core is not connected to the RF-Kit
  amplifier." A refusal of the page's request shows in its status line;
  one of the applet's as a notice. The page's Save also keeps a Host or
  Port that differs from the Core's on the Core (`setRfKitAddress`,
  nothing dialled); Connect still sends `configureRfKit`. With
  `accessoryDataVersion` 2 the page's Live diagnostics shows the Core's
  counts (polls answered and failed, reconnects, connected since, last
  poll), and the applet's Copy diagnostics to clipboard copies the Core's
  connection: the `rfkit` object's address, version, operate, interface
  and error and `accessoryData`'s `rfkit*` counts, never this computer's
  idle connection. Below 2 Live diagnostics says "The Core keeps the
  amplifier's connection counts." A local window's RF-Kit page shows the
  same readings from its own connection (with its response time), and its
  Copy diagnostics adds the operate, interface, reconnect, connected-since
  and last-poll lines
  (parity runs both ways; the Core does not send the response time, so a
  remote window does not show it). A local window's OPERATE, ANT and "Set
  amp to TCI mode" keep sending this computer's own requests, with no
  on-the-air check, as before. RF-Kit TUNE and BYPASS stay hidden in every
  window.
- The amp page and applet show the band-follow line (see "Band follow"):
  the RF-Kit page and applet from `rfkit`, the Power Genius applet and the
  4O3A page's General tab from `amplifier`, in local and remote windows.
- One TCI switch and one port (operator decision of 2026-09-23). In a
  window connected to a Core with `stationTciVersion` 1, the switch and
  port on CAT & Network > TCI Server are the Core's station switch and
  port: the page shows them (following a change made from another window
  or the phone, and keeping this computer's `TciServerEnabled` and
  `TciServerPort` in step), and changing them sends `setStationTci`. The
  Core keeps its own copy, so its server keeps running for the amp when
  the window closes or another app connects. At connect the Core's stored
  switch wins and the window's switch follows it; a Core that has no
  stored station switch yet (no `StationTci_Enabled` in its settings, as
  after the upgrade on a computer that ran the window with TCI on) takes
  the window's switch and port instead, so apps there keep TCI. Until
  this link's settings snapshot arrives the window decides nothing (a
  previous Core's settings, from earlier in the same run, do not count). The window reads the Core's
  change only once all its properties have arrived, and after sending a
  change it waits for the Core to report that same switch and port before
  following again, so it never flips back to a stale value. That wait
  ends as soon as the request is over whatever became of it: the Core's
  answer to `setStationTci` (accepted or refused), the link going down or
  coming up, or a new connection; the window then follows the Core's
  current switch and port. The window's
  own server follows the switch only when the Core runs on another
  computer; on the Core's own computer the window runs no server while
  connected, and apps there use the Core's (it listens on that computer
  too). The page adds "Also at the station: <address>, port <port>" while
  the Core's server listens ("The station's TCI server is not running."
  while it is on and not listening), or "The Core on this computer serves
  TCI apps here, port <port>." on the Core's computer. With the link to
  the Core down the switch shows the last known state and the page says
  nothing about the Core; on the Core's computer the window starts no
  server (there is no radio there to serve), and on another computer its
  server follows the switch. With an older Core (or none) the window's own
  server follows the switch as it always has.
- With `stationTciVersion` 2, the TCI applet, bottom indicator, clients
  applet, Setup status and log distinguish this window's server from the
  Core's. The clients applet subscribes to `tciClients` and can disconnect
  one Core client by id. Setup shows the Core's bind and port read-only and
  sends its four compatibility and initial-state options through
  `setStationTciOptions`; this window's options remain local. The Core
  refuses option changes and client disconnects while the radio is on the
  air. With `stationTciSettingsVersion` 1 (JJ's ruling of 2026-09-28; the
  window declares `stationTciSettings`), the group also shows and changes
  the rest of the page's settings for the Core's server through
  `setStationTciSettings`, one at a time; the ones the page hides as not
  built yet are hidden there too. An older Core leaves these controls visible but disabled with a
  plain reason. The TCI applet's Enable Server saves the switch through
  `TciSwitch`, including its Core request, so a link event does not erase
  the operator's choice.
- A local window's RF-Kit band follow is worked out from its own TCI server
  the same way.
- With `accessoryDataVersion` 1 the desktop remote window's 4O3A page
  shows the Core's records and changes them through the Core. The General
  tab's interlock section shows the Core's policy and sends
  `setTxInterlockPolicy` with the whole policy on each change; a refusal
  puts the Core's policy back and shows the reason. The Power Genius tab
  adds the output limit (`setPgxlPowerCap`), the Core's counters and the
  fault history (Clear All sends `clearAccessoryFaults`; each row's tooltip
  is its `text`). The Tuner Genius tab shows the Core's antenna names,
  tune memory, counters and fault history (When, What happened; the
  `detail` in the tooltip; Clear All through the Core); a name or memory
  changed there is written as a station setting and comes back on
  `accessoryData`. Without `accessoryDataVersion` the interlock section, the output limit
  and Clear All are shown unchangeable with "This Core does not share its
  transmit interlock with this app. Updating the Core may help." (or its
  Power Genius or Tuner Genius records), and the Tuner Genius tab stays
  closed with "This Core does not share its Tuner Genius records with this
  app."
- With `remotePgxlControlVersion` 3 (and `remoteTgxlControlVersion` 1) the
  Power Genius (and Tuner Genius) tab has every section a local window's
  Advanced page has, and each control works: Identity (the name; firmware,
  serial, state and, for the amp, MeFFA from `amplifier`, for the tuner the
  variant and state from `tuner`), Hardware (bias, fan, LED, output limit),
  Network, Pairing & Band Source, Diagnostics, Fault History, Revert and
  Save & Reboot. The device settings go to the Core as the commands above
  (one per change; the LED once the slider is let go); network changes and
  Save & Reboot ask first (see "Asking first"); the device's answer shows
  above the sections, and the values it reports or takes fill the fields;
  a refusal shows there too and the Core's values stay. Apply, Revert and
  Save & Reboot are enabled only while the Core is connected to the device
  (Save & Reboot after a change, as in a local window). The pairing
  settings are written as station settings. Below those versions the
  device's own controls are shown unchangeable with the reason (see
  Negotiation); the pairing settings, the output limit, the counters and
  the fault history work as before.
- With `remoteTgxlControlVersion` 2 the desktop remote window's Tuner
  Genius applet enables ANT 1, 2 and 3 and OPERATE while the Core reports
  the radio off the air. ANT sends `setTgxlAntenna`; OPERATE cycles
  OPERATE, BYPASS, STANDBY from the Core's reported state, as a local
  click does, sending `setTgxlBypass` and `setTgxlOperate`; at version 3,
  STANDBY to OPERATE is the one `setTgxlOperate` true. The highlighted
  antenna and the button's label follow the `tuner` object. While the Core
  reports the radio keyed (`radio` `transmitting`), TUNE (`transmit`) or
  the two-tone test (`pureSignal` `twoToneOn`) the buttons are disabled
  with "The radio is on the air. Try
  again when it stops." A refusal shows as a notice. TUNE keeps the
  transmit-permission reason (and the relay bars below version 4). With
  the link down, or below 2, ANT and OPERATE are greyed with that reason
  too. A local window is unchanged.
- With `remoteTgxlControlVersion` 4 the desktop remote window's Tuner
  Genius applet also lets the mouse wheel move the Core's relays: a wheel
  step on the C1, L or C2 bar sends `moveTgxlRelay` for that relay while
  the Core is connected to the tuner and the radio is off the air, and the
  bars show `relayC1`, `relayL` and `relayC2` as the tuner reports them.
  While the Core reports the radio on the air the bars do not scroll and
  say "The radio is on the air. Try again when it stops." In local and
  remote windows the applet's right-click menu offers Recall tune memory
  (it copies the stored values into the bars and sends nothing; a remote
  window's store holds the Core's tune memory) and Open TGXL Advanced...,
  which opens Setup at CAT & Network > 4O3A > Tuner Genius XL (Open PGXL
  Advanced... opens the Power Genius XL tab and the interlock entry the
  General tab). Copy diagnostics to clipboard in a remote window copies
  the Core's connection: the `tuner` object's address, identity and error
  and `accessoryData`'s `tgxl*` counters, never this computer's idle
  connection.
- With `remoteTgxlControlVersion` 4 the desktop remote window's Peripherals
  row for the Tuner Genius offers Scan LAN: it sends `scanTgxlLan` and
  lists the devices the Core heard (Model, IP, Port, Serial, Nickname; the
  Core does not report the firmware version); a double-click fills Host and
  Port and sends them with `setTgxlAddress`. A Host or Port typed without
  Connect is sent with `setTgxlAddress` when editing finishes, and when
  Setup closes (or the row's tab is left) with an edit still unsent; the
  Core keeps it for its radio, so the next window shows it, and nothing is
  dialled. A refusal shows as a notice. Scan LAN, Host and Port stay live
  while the radio is on the air (they only listen or save).
  Below version 4 Scan LAN stays off with "This Core does not scan for a
  Tuner Genius for this app. Updating the Core may help." and a typed
  address is only sent by Connect.
- With `remotePgxlControlVersion` 4 the desktop remote window's Power
  Genius applet OPERATE button sends `setPgxlOperate` (on from standby,
  off from operate) while the Core is connected to the amp and the radio
  is off the air; it shows the amp's reported state (OPERATE or STANDBY)
  from the `amplifier` object, not the click, and never uses this
  computer's own connection. The Operate button on Setup > CAT & Network >
  4O3A > PowerGenius XL (the remote window's tab) does the same: it reads Operate
  while the amp is in standby and Standby while it operates. While the
  Core reports the radio on the air both are disabled with "The radio is
  on the air. Try again when it stops."; while the Core is not connected
  to the amp, with "The Core is not connected to the Power Genius."; while
  the Core's tuner (`tuner` `isTuning`) sweeps, with "The tuner is
  tuning. Try again when it finishes." (iPhone app plan Task 77 fix round
  3; a local window also waits through the Core's whole cycle). With the
  amp in FAULT both read Standby and send `setPgxlOperate` off. A
  refusal shows as a notice. A local window's PowerGenius XL tab has the
  same Operate button beside its state badge (operator decision
  2026-09-25: the same controls however the window is connected). It
  sends the local applet's own line, `operate=1` or `operate=0`, through
  that computer's own connection to the amp, reads Operate or Standby from
  the amp's report, and is disabled with "The Power Genius is not
  connected." while that computer is not connected to the amp. Like the
  local applet's OPERATE it has no on-the-air rule; the link is not
  involved.
  Copy diagnostics to clipboard in a remote window copies the Core's
  connection: the `amplifier` object's address, identity, state and error
  and `accessoryData`'s `pgxl*` counters, never this computer's idle
  connection.
- With `remotePgxlControlVersion` 4 the desktop remote window's Peripherals
  row for the Power Genius offers Scan LAN (`scanPgxlLan`; the table lists
  the Power Genius devices the Core heard) and keeps a typed Host or Port
  on the Core (`setPgxlAddress`) exactly as the Tuner Genius row does
  above, with its own words: "This Core does not scan for a Power Genius
  for this app. Updating the Core may help." below version 4.
- Every window (local or remote) shows the power-cap alert as a five-second
  notice when `powerCapAlertCount` moves while `powerCapExceeded` is true;
  a remote window's applet antenna names follow the Core's.

## Fixtures

`tests/fixtures/accessories/` holds files a client's conformance suite can
load:

- `amplifier.jsonl` and `rfkit.jsonl`: one session message per line, as the
  Core sends them: the `schema`, the `object.create` after the captured
  readings (PGXL `R1|0|state=OPERATE temp=42.5 vac=240 fwd=1480.0 swr=2.1`
  and the setup reply with `meffa=off`; RF2K-S `/info`, `/power` and
  `/operate-mode` replies, with `/operational-interface`, `/antennas` and
  `/antennas/active`), `snapshot.complete`, then deltas (PGXL transmitting
  at 60 dBm with -24.5 dB return loss, then standby, then band follow
  `following`; RF2K-S standby with an idle `/power`, then a `/tuner`
  reading, TCI mode, antenna 2 and band follow `waiting` with an address).
- `accessoryData.jsonl`: the `accessoryData` object as the Core sends it:
  the `schema`, the `object.create` with a Power Genius fault (the captured
  fault line's readings), the interlock policy `block`, the output limit,
  one tune memory and two antenna names, `snapshot.complete`, then deltas
  (a Tuner Genius `link` fault, the output going over the limit, the Power
  Genius counters, and from `accessoryDataVersion` 2 the RF-Kit's counts
  after one failed poll and the retry it schedules).
- `accessorySettings.jsonl`: the `accessorySettings` object as the Core
  sends it: the `schema`, the `object.create` with the amp's settings and
  its answer to a Revert, `snapshot.complete`, then deltas (the amp taking
  a new name; the tuner's network settings).
- `enums.json`: the `connectionPhase`, `amplifierState`, `bandFollow`,
  `rfkitTunerMode` and `interlockMode` tables and the five read-only
  refusal texts.

`tst_station_accessory_state` regenerates the four `.jsonl` files from the
objects and fails if they differ, parses them, applies them to a window's
objects, and checks `enums.json` against the enums. After a deliberate
contract change, run it once with `NEREUS_WRITE_ACCESSORY_FIXTURES=1` to
rewrite the fixtures, and update this document in the same commit.

## Evidence

- `tst_station_accessory_state`: the conversion from captured lines, the
  connection phases over a loopback socket, the RF2K-S replies, what a
  current, an older and a non-owning Core sends, the read-only refusals, and
  the fixtures.
- `tst_remote_peripherals`: a remote window's Power Genius and RF-Kit applets
  over the in-process loopback (filled on attach, updated, false and zero
  shown, stale on Core loss, no accessory connection opened), the older-Core
  line, and a local window showing the same values as before through the
  moved conversion.
- `tst_station_session`, `tst_display_budget_contract`: the capability
  entries' place, and an older app's capabilities byte for byte.
- `tst_station_pgxl_controller`: the captured discovery and `info` reply
  admit a PGXL, which is then paired and follows the band; a Tuner Genius,
  a serial mismatch and a silent peer are never admitted and are sent only
  `info`; cancelling or switching off in every phase never redials; A
  replaced by B while A is identifying leaves only B; the connection
  settings are saved and applied; a radio's saved address is dialled
  through the identity check.
- `tst_pgxl_connection_reconnect`: the owned retry timer (replaced or
  cancelled addresses never redialled, a fresh socket per retry, a late
  callback from a replaced attempt cannot act, automatic retry off drops a
  pending retry).
- `tst_remote_peripherals`: the remote Power Genius row and tab through the
  link, end to end through the Core over the loopback, and the receive-only
  refusals of the tuner's and amplifier's operate controls (nothing
  changes, nothing is sent to the tuner).

- `tst_station_rfkit_controller`: an RF2K-S naming itself in `/info` is
  admitted and only then connected; another device, or none, is refused,
  recorded as a fault and never retried; no answer retries (or says why with
  retry off); cancelling while identifying never admits; the Core puts the
  amp in TCI mode once when band follow starts while the station's TCI
  server is on, not otherwise, not when it already is, and not again after
  a link blip (again only when the switch is turned on again); the Core's configure and switch
  rules; a radio's saved address dialled through the identity check.
- `tst_rf2ks_connection_lifecycle`, `tst_rf2ks_connection_parse`: identity
  admission on the wire, a device changing mid-session, the local default
  unchanged, the retry and failure reports, the reported device and the
  interface faults.
- `tst_rfkit_radiomodel_enabled`, `tst_rfkit_page_master_gate`: the switch
  has no raw write, a remote window holds the Core's value and never
  switches it itself, the Core runs it through its controller; the remote
  page asks the Core and dials nothing; the band-follow line.
- `tst_station_tci_server`: where the Core listens (the station network
  facing the radio, the override, this computer), the switch kept on the
  Core across restarts, a TCI app at the station hearing receive-only,
  `split_enable:` and `vfo:` as the Core's slice moves, transmit refused,
  the RF-Kit's band follow over the server, the one switch driving both
  servers, a window following the Core's switch and port changed by
  another window (the whole change at once, in the wire's property
  order), no window server on the Core's computer while connected, the
  Core retrying a listener that could not start with its plain reason,
  the station network served while a third program holds the port on the
  Core's computer (the RF-Kit's band follow up, the reason naming the
  blocked address, that computer taken on the next retry with no stop
  and start), and the TCI page's line. `tst_remote_peripherals`: on the Core's
  computer the window runs none, the phone turns the Core's switch on,
  the Core serves apps on its computer and on a station address, and the
  TCI page shows the Core's switch and port.
- `tst_tci_tx_mutex`: the station server's transmit refusal on the wire,
  and its refusal of TX profile and XIT changes (nothing applied or
  broadcast, the kept value to the asking app, the reason off the wire).
- `tst_smartsdr_api_listener_bind`: the one station rule on a Core with two
  networks (the radio's network, the override, every address, this computer
  only before a radio, a move when the radio moves); the 4992 listener on
  the station address and this computer only, a missing station address
  refused rather than narrowed, the move dropping the old connection; the
  Core's model applying the rule to 4992 and the TCI server alike; a
  desktop window's listener binding as before.
- `tst_lan_discovery_regex`: the Core hears Power Genius and Tuner Genius
  announcements only from the station network (sender and announced
  address), this computer only before a radio, the override; a desktop
  window hears every one.
- `tst_station_pgxl_controller`: an announcement from another network
  admits nothing; the same from the station network admits the amp.
- `tst_flex_radio_discovery_broadcaster`: no FlexRadio beacon with 4O3A
  off, on and off with the switch and with a radio's saved switch, off with
  its own setting off; on the Core it announces the station address.
- `tst_daemon_config`: `station_bind`, the older `station_tci_bind` read
  beneath it, and a value that is not an address refused.
- `tst_fault_log`: every record names its device and says what happened in
  plain words, older records read with both, a window's copy is replaced
  from the Core's list without saving, and a fault the Core recorded is
  there after the Core restarts from its settings file.
- `tst_tx_interlock_policy`: the Core reloads the policy when a window
  writes its settings, a remote window holds the Core's policy without
  saving it, and `setTxInterlockPolicy` is applied and enforced on the
  Core, mirrored on `accessoryData`, and refused in user words out of range.
- `tst_station_accessory_state`: `accessoryData` only to a current app on a
  Core that owns its accessories, read-only, its fixture, the
  `interlockMode` table; the Tuner Genius faults the Core records (a drop,
  an attempt that ends at an error, never an operator's disconnect); the
  power-cap alert raised once per time over the limit, re-armed, and
  following a window's settings write.
- `tst_remote_peripherals`: over the loopback, faults raised on the Core
  appear in a remote window's pages without a reconnect and are cleared
  through the Core; the counters are the Core's, not the window's; the
  output limit is set through the Core and its alert reaches the window;
  names and tune memory are the Core's; the interlock policy changed from a
  remote window takes effect on the Core (which refuses to transmit), is
  shown by that window and by one connecting later, and a request out of
  range changes nothing and is shown in user words; an older Core leaves
  the section unchangeable with the reason.
- `tst_station_pgxl_controller`, `tst_tgxl_station_identity`: a window's
  requests for the amp's (tuner's) own settings reach a fake device on a
  loopback socket as exactly the commands above; the device's answers
  (taken, not taken, a read's values, `saving`) are published in user
  words; bad values are refused before anything is sent; nothing reaches a
  device the Core has not admitted; a request waiting when the device goes
  says so; a new scope forgets the values; nothing operates, bypasses or
  switches an antenna.
- `tst_station_accessory_state`: `remotePgxlControlVersion` 3 (4 from parity Task 9) and
  `remoteTgxlControlVersion` 1 last in the block, neither to an older app
  or from a Core that does not own its accessories; `accessorySettings`
  read-only and its fixture; the nine commands refused below minor 11, on
  a non-owning Core, malformed, and with no device, in user words.
- `tst_remote_peripherals`: over the loopback, the same clicks on a remote
  window's Power Genius (Tuner Genius) tab and on a local window's page
  reach their devices as the same commands (the remote one once per
  change); network changes and Save & Reboot ask first in the local
  words and send nothing on a no; the device's answers and values show on
  the page; a Core refusal arrives on its own route (not the slice notice)
  and changes nothing; Apply and Revert wait for the Core's connection; the
  window opens no connection to the device; an older Core leaves the
  controls unchangeable with the reason.
- `tst_remote_peripherals`: the RF-Kit set up, switched and disconnected
  from a remote window through the Core over the loopback, its rows reaching
  the applet, a raw `rfKitEnabled` write refused, the Power Genius's
  band-follow line local and remote, and the one TCI switch turning the
  Core's station server on and off, which keeps running when the window
  goes and another connects.
- `tst_remote_peripherals` (fix wave): a remote window's RF-Kit page
  works every control: its Reset amp error reaches the Core's amp as
  `POST /error/reset`, the same request a local page's button sends to its
  own amp (both fakes record it); refused with no amp admitted, on the
  accessory route; Save sends automatic retry, poll interval and the four
  names over the link as station settings (through the window's settings
  proxy to the Core's store), which the Core applies to its connection on
  arrival, and the page follows a change made on the Core by another
  window;
  below `remoteRfKitControlVersion` 3 the controls stay unchangeable with
  a plain reason. `tst_station_rfkit_controller`: the Core's reset reaches
  only an admitted amp. `tst_station_accessory_state`: `resetRfKitError`
  refused below minor 11, on a non-owning Core, with arguments and with no
  amp, in user words.

- `tst_tgxl_station_identity` (R-R3-49): a window's antenna, operate and
  bypass reach the Core's (fake) tuner as the local applet's lines
  (`activate ant=2`, `bypass=0` then `operate=1` from one `setTgxlOperate`
  true, `bypass=1`, `operate=0`) on a
  receive-only Core; refused with nothing sent on a Core that does not own
  its accessories, with no tuner admitted (and before admission), with a
  port outside 1 to 3, on a tuner with no antenna switch, and while the
  Core's MOX or TUNE is on: the transmit model's latches, a hardware PTT
  press and the two-tone test through the Core's MOX controller, and its
  TX to RX handover after MOX clears (the Core's receive-only MOX
  pre-check lifted to stand in for a Core that can transmit); the model
  reports the tuner's answer, not the request.
- `tst_station_accessory_state` (R-R3-49): `remoteTgxlControlVersion` 3 (4 from parity Task 8);
  the three commands refused below minor 11, on a non-owning Core, with no
  tuner, on the air, malformed and out of range, in plain words.
- `tst_remote_peripherals` (R-R3-49): a remote window's Tuner Genius applet
  over the loopback switches the Core's tuner antenna and OPERATE (the fake
  tuner records the lines), follows the tuner's report and not the click,
  disables ANT and OPERATE with the reason while the Core's MOX is on,
  keyed through its MOX controller by a MOX click and by the radio's PTT
  input, and read from `transmitting` (the Core's MOX pre-check is lifted
  to stand in for a Core that can transmit)
  (a request sent anyway refused with it on the accessory route, nothing
  reaching the tuner), keeps TUNE greyed, and falls back to the transmit
  reason with the link down; an older Core leaves them greyed and sends
  nothing. From STANDBY one OPERATE click is one `setTgxlOperate` true on a
  Core at 3 (`bypass=0` reaches the tuner once) and `setTgxlBypass` false
  then `setTgxlOperate` true on a Core at 2.
- `session-verbs-tgxl-control` (`tests/data/link/v1/sessions/`): the three
  commands right and wrong on a Core with no tuner, and the antenna command
  with port 4 ("Choose Tuner Genius antenna 1, 2 or 3."), run by the
  station and app conformance runners. The on-air refusal is not in it:
  the station's runner has no way to put its Core on the air (its setup
  has no key for it, and a receive-only Core refuses every key), so the
  unit tests above cover it.
- Parity Task 8 (R-R3-49, `remoteTgxlControlVersion` 4):
  `tst_tgxl_station_identity` checks each relay nudge reaches the (fake)
  tuner as `tune relay=<relay> move=<move>` on a receive-only Core and
  keys nothing; the nudge, the scan and the saved address refused with
  nothing sent on a non-owning Core, the nudge on the air (the MOX and
  TUNE latches and a hardware PTT through the Core's MOX controller) while
  the scan and the address are taken there (parity mini-round), and the
  nudge with no tuner and out of range; the scan answering with the Tuner Genius
  announcements it heard and not a Power Genius's; the address checked,
  saved, shown on `tuner` once the Core is disconnected, and not dialled.
  `tst_station_accessory_state` checks version 4 and the three commands'
  wire refusals. `tst_remote_peripherals` checks over the loopback that
  the relay bars' wheel moves the Core's relays and the bars follow the
  tuner, stop with the on-air reason, and a request sent anyway is
  refused; that Setup's Scan LAN lists what the Core heard and a pick,
  a finished edit and Setup closing each keep the address on the Core
  (a later window reads it) with nothing dialled, and Scan LAN and the
  address stay live on the air and are taken there (parity mini-round);
  that Recall tune memory and Open TGXL Advanced work in a remote window
  and Copy diagnostics carries the Core's counters; and that the Advanced
  and interlock entries open their 4O3A tab in local and remote windows.
  `session-verbs-tgxl-relays` invokes the three commands right and wrong
  (the scan's `devicesJson` matched as any text, since a real tuner on the
  test computer's network may answer); it requires version 4, so an app at
  version 2 or 3 still runs `session-verbs-tgxl-control`.
- Parity Task 9 (R-R3-49, `remotePgxlControlVersion` 4):
  `tst_pgxl_station_control` checks on a loopback amp that
  `setPgxlOperate` sends exactly `operate=1` and `operate=0` on a
  receive-only Core and keys nothing, and that the model follows the amp's
  report; the three commands refused with nothing sent on a non-owning
  Core, OPERATE on the air (the MOX and TUNE latches, a hardware PTT, the
  two-tone test and the hand-back to receive, through the Core's MOX
  controller) while the scan and the address are taken there (parity
  mini-round), OPERATE with no amp; the scan answering with the Power
  Genius announcements it heard and not a Tuner Genius's; the address
  checked, saved, shown on `amplifier` once the Core is disconnected, and
  not dialled. `tst_station_accessory_state` checks version 4 and the
  three commands' wire refusals. `tst_remote_peripherals` checks over the
  loopback that the applet's OPERATE and the 4O3A tab's Operate reach the
  amp and follow its report, wait on the air with the reason (a request
  sent anyway refused, nothing reaching the amp) and never emit the local
  toggle; that Copy diagnostics carries the Core's counters; and that the
  Peripherals row's Scan LAN lists the Power Genius the Core heard and a
  pick, a finished edit and Setup closing keep the address on the Core,
  with nothing dialled, taken on the air too (parity mini-round); and that
  a local window's
  PowerGenius XL tab Operate sends `operate=1` and `operate=0` to its own
  loopback amp, follows the amp's report and is disabled with its reason
  while not connected. `session-verbs-pgxl-control`
  invokes the three commands right and wrong (the scan's `devicesJson`
  matched as any text).
- Parity Task 10 (R-R3-49, `remoteRfKitControlVersion` 4,
  `accessoryDataVersion` 2): `tst_rfkit_station_control` checks against an
  in-process HTTP amp that records every request that `setRfKitOperate`,
  `setRfKitAntenna` and `setRfKitTciMode` send exactly the local applet's
  and page's `PUT` requests on a receive-only Core and key nothing, and
  that the `rfkit` object follows the amp's report; the four refused with
  nothing sent on a non-owning Core, the three switches on the air (the
  MOX and TUNE latches, a hardware PTT, the two-tone test and the hand-back
  to receive, through the Core's MOX controller) while the address is
  taken there (parity mini-round), with no amp admitted, for an antenna outside
  1 to 4 and for one the amp lists as disabled or does not list; the
  address checked, saved, shown on `rfkit` once the Core is disconnected
  (and with the switch off), and not dialled; and the connection counts
  reaching `accessoryData` on their own within a second.
  `tst_station_accessory_state` checks versions 4 and 2, the four
  commands' wire refusals and the counts in `accessoryData.jsonl`.
  `tst_remote_peripherals` checks over the loopback that the applet's
  OPERATE and ANT and the page's "Set amp to TCI mode" reach the amp and
  follow its report, never emitting the local signals; that an antenna the
  amp does not offer is disabled with its reason; that Save keeps a
  changed Host and Port on the Core with nothing dialled; that Live
  diagnostics and Copy diagnostics carry the Core's counts; that the
  switches wait on the air with the reason (a request sent anyway refused,
  nothing reaching the amp) while Host, Port and Save are taken there
  (parity mini-round); that an older Core leaves them greyed and the
  window asks nothing; and that a local window's page shows the
  connected-since and last-poll readings and its TCI button still sends
  this computer's own request. `session-verbs-rfkit-control` invokes the
  four commands right and wrong on the static station; it requires version
  4, so an app at version 2 or 3 still runs `session-verbs-rfkit`.
- Group B fix wave (I1): the three address commands save a blank host.
  `tst_tgxl_station_identity`, `tst_pgxl_station_control` and
  `tst_rfkit_station_control` check the blank is saved and shown, and
  that the Core's next start then dials nothing; `tst_remote_peripherals`
  checks a remote window's blank Host reaches the Core with no refusal;
  `session-verbs-tgxl-relays`, `session-verbs-pgxl-control` and
  `session-verbs-rfkit-control` each save a blank host (its delta shows
  the blank) and then the address again.

Hardware evidence is pending for the operator checkpoint: ANT 1, 2 and 3,
OPERATE and BYPASS switched on the real Tuner Genius from the Rock's
remote window (R-R3-49), C1 nudged and the LAN scanned from a remote
window with the local applet on the Core's computer showing the same relay
(parity Task 8), the real PGXL put in operate and back in standby from the
Rock's remote window with the radio idle and the LAN scanned for it
(parity Task 9), OPERATE, ANT 1 to 4 and "Set amp to TCI mode" on a real
RF2K-S from a remote window with the radio idle, the saved address and the
Core's counts in Live diagnostics (parity Task 10, when an RF2K-S is
available), and readings from the
real PGXL and RF2K-S reaching a remote window and the iPhone app; identity
and pairing with the real PGXL on the Core (the discovery announcement and
`info` reply used here are the real amp's, captured on 2026-05-19 and
2026-05-20 in `captures/`); the real RF2K-S admitted by its `/info`, put in
TCI mode by the Core, and following band changes made from the remote
window and from the iPhone app through the Core's station TCI server (the
RF2K-S pointed at the Rock); the station server, the 4992 listener and the
accessory discovery bound on the Rock's station network and not its other
network, and the FlexRadio beacon announcing the station address there; a real Power Genius fault, a Tuner Genius drop and an RF-Kit
interface error reaching a remote window and the iPhone app from the Rock,
a Reset amp error from the Rock's remote window clearing a real RF2K-S
error,
and the Rock's fault history after a restart; the power-cap alert from a
real transmit through the Power Genius (with remote transmit); and, only
with the operator's go-ahead because it changes the devices' own settings,
a name change and a Save & Reboot on the real Power Genius and Tuner Genius
from the Rock's remote window. None of the device replies the settings
tests use has been captured: the `setup read` reply follows the design
doc's section 6.4 (verbatim from the FlexRadio wiki: `nickname= fan=
meffa= led=`), and its `bias=` is unobserved; the `ifconf read` reply uses
`dhcp=` (0 or 1) and `ip=`, the keys the local Tuner Genius page's parser
reads, where the design doc documents `address=` and `dhcp=false` (the
Core, like the local page, reads `ip=`: which one the devices send is part
of the pending evidence); the refusal code 50000015 is the one a real
Power Genius sent when it refused `amplifier create` (bench note of
2026-05-21), and a refusal of a settings command has not been observed.
