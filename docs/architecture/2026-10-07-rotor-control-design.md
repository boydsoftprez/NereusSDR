# Rotor control design

Status: design, agreed with JJ (KG4VCF) on 2026-10-07. Mockup:
[2026-10-07-rotor-control-mockup.html](2026-10-07-rotor-control-mockup.html)
(open it in a browser; drag the desktop dial and let go to turn, or drag
the iPhone dial to select and tap Turn).

## Goal

Turn the station's antenna rotor from NereusSDR: from a Rotor applet in a
desktop container, from a Rotor page on the iPhone, and straight from a DX
spot on the pan or in Spot Hub. The first rotor is JJ's Yaesu on an Easy
Rotor Control (ERC) interface; the design covers any rotor that speaks the
Yaesu GS-232 commands or that Hamlib's `rotctld` can drive.

## Decisions (2026-10-07)

| Question | Decision |
| --- | --- |
| How the Core talks to rotors | Both: GS-232 over a serial port, and Hamlib `rotctld` over the network |
| Desktop shape | A Rotor applet. Revised the same day after comparing dials: the applet and the iPhone page use a Longpath-style control dial (after OE5SOS's `RotorDialWidget`); the Thetis `RotatorItem` stays as the meter item in custom layouts and gains drag-to-turn |
| Where it lives on the iPhone | A Rotor page under Accessories, and a Rotor row in the Tools tab that opens the same page |
| Elevation | Supported; azimuth or azimuth + elevation is set in rotor setup |
| Turn to a spot | Its own action everywhere (pan menu, Spot Hub, iPhone spot sheet), plus a "turn the beam when I tune to a spot" setting, off by default |
| Look | As in the mockup: the Longpath-style dial in NereusSDR colours (amber heading needle, dashed cyan target arrow, the travel sector, "73° to go", green on arrival, an elevation quarter gauge beside the rose on az/el rotors, rose or tape shape), Stop the only red button |
| Touch safety | The desktop turns on release: drag the dial or the meter item to a heading and let go. The iPhone selects, then confirms: a drag on the dial only selects (target colour, no turning) and a Turn button sends it; an unsent selection is dropped after 15 s (after Longpath's select-then-press). Presets, Stop, the nudge holds and "Turn beam" on a spot are one tap everywhere |

## What exists today

* `RotatorItem` (`src/gui/meters/RotatorItem.*`), ported from Thetis
  `clsRotatorItem` (MeterManager.cs:15042+), draws the compass in AZ, ELE or
  BOTH mode. It only displays. Thetis's click and drag to turn
  (`SendRotatorMessage`, MeterManager.cs:16475; the drag handling around
  MeterManager.cs:36722-37217) was not ported.
* `src/core/mmio/` has serial, TCP and UDP endpoints (Thetis MultiMeterIO).
  Thetis drives rotors by sending operator-typed templates (`%AZ%`, `%ELE%`)
  through it. That stays as it is; the rotor gets its own driver because a
  template cannot reach the iPhone, poll a heading, or refuse a command.
* Meter items already take mouse input (`MeterItem::handleMousePress` and
  friends, `MeterWidget::mousePressEvent`).
* Station accessories are Core-owned (decision of 2026-09-23, see
  [remote accessory control version 1](2026-09-23-remote-accessory-control-v1.md)):
  the PGXL, TGXL and RF2K-S each have a connection class and a
  `Station...Controller` in `src/core/`. The rotor follows that pattern.
* `CtyDatParser` resolves a callsign to a DXCC entity but drops the
  latitude and longitude columns. The station's grid square is
  `User/GridSquare` (read in `SpotSourceHost::freedvGridSquare`).

## Prior art: Longpath

Longpath (github.com/oe5sos/Longpath) is a GPL-3 fork of NereusSDR by
Martin Fischer, OE5SOS, with rotor work since 2026-08-07: a `rotctld`
client, a helper that starts `rotctld` for the operator, a curated list of
Hamlib rotor models, a "fresh position" rule (a stale heading is marked,
never shown as live), strict heading input (empty, NaN, negative and over
360 refused, not converted), a two-needle `RotorDialWidget` with rose and
tape shapes, and phone control over TCI-style commands. Its rotor
connection belongs to a window; ours belongs to the Core.

Taken (JJ, 2026-10-07), with Longpath's header and credit wherever code is
ported:

* The dial's design (plan Task 6).
* The fresh-position rule: no position reply for 1500 ms marks the heading
  stale (Longpath `RotctldClient.h:76`); windows show it muted with its age.
* Strict heading checks (Longpath `RotorPeilung.h`): not-a-number refused,
  out of range refused rather than wrapped, 360 sent as 0.
* Hamlib's own ERC driver, model 404, offered in the rotctld model list.
* The Core starting `rotctld` itself (Longpath `RotctldProcess`): it finds
  the binary (including Homebrew paths a GUI-launched process does not see),
  runs `rotctld -m <model> -r <port> -s <baud> -T 127.0.0.1 -t <listen>`
  (`RotctldProcess.cpp:138-161`), takes a free port when 4533 is held, and
  stops it on exit.

Kept from our design: the native GS-232 driver, so the ERC in GS-232 mode
needs no Hamlib install on the Core. A local reference copy is at
`../Longpath/` (pinned `551576e`, v0.6.7-1).

## Facts (sourced)

**ERC.** Made by DF9GR (Schmidt), sold in the US by Vibroplex. It talks to
the computer with the DCU-1, GS-232A or GS-232B protocols, over USB (a
virtual serial port) or RS-232. Models: ERC version 4 (azimuth), ERC-M
(azimuth, azimuth + elevation, or two azimuths), ERC-DUO (Yaesu DXA/DXC and
G/KR-5x00). Source: cq.sk article on the ERC
(https://cq.sk/en/easy-rotor-control-erc-control-of-the-antenna-rotator-from-a-computer/).

The maker's own manuals (ERC SMD USB V4.3 Instructions, sections 5, 7 and
8, and the ERC-M RS232 Kit V2.2 Instructions, from schmidt-alba.de and
vibroplex.com) add: the protocol (GS-232B, GS-232A or Hy-Gain DCU-1) and the
baud rate are chosen in the ERC's Service Tool, and the program must match
them; the port is N-8-1; the setup example uses 9600 baud and GS-232B; the
ERC-M shows its protocol on its LCD at start-up. Calibration covers rotors
with overlap (more than 360 degrees). The manuals do not list the command
set itself; they defer to the Yaesu GS-232 protocol, so the commands below
come from Hamlib. The manuals are not open source and forbid republishing,
so cite them, never copy them.

**GS-232A,** from Hamlib `rotators/gs232a/gs232a.c` (master):
commands end in CR; replies end in LF. Position read `C2`, reply
`+0aaa+0eee`. Set azimuth and elevation `Waaa eee`. Stop `S`. Move `L`, `R`,
`U`, `D`. Serial 9600 8N1, no handshake.

**GS-232B,** from Hamlib `rotators/gs232a/gs232b.c` (master): commands end
in CR; replies end in CR LF. Position read `C2`, reply `AZ=aaa EL=eee`, or
`AZ=aaa` from an azimuth-only rotor. Set `Waaa eee`. Stop `S`. Move `L`, `R`,
`U`, `D`. Hamlib also skips bare CR LF and `>` prompts as invalid replies.

**Hamlib's own ERC driver:** model 404, `ROT_MODEL_ERC =
ROT_MAKE_MODEL(ROT_ROTOREZ, 4)`, "rotators that support the DCU command set
by DF9GR" (Hamlib `include/hamlib/rotlist.h`, master). Found through
Longpath's `RotorModels.h`, checked at the source.

**rotctld,** from the Hamlib `rotctld(1)` manual: TCP, default port 4533,
one command per line ending in LF. `p` returns azimuth and elevation on two
lines; `P az el` sets both; `S` stops; `M dir speed` moves (2 up, 4 down,
8 left, 16 right; speed 1 to 100, or -1 for no change). Errors and
acknowledgements are `RPRT n` (0 is success, negative is an error).

**cty.dat,** from the CT format notes (country-files.com/cty-dat-format):
entity columns include latitude (+ north) and longitude (+ west), and a
prefix line can override them with `<lat/long>`. Our copy agrees (Japan
`36.40: -138.38`, United States `37.60: 91.87`).

**JJ's ERC, observed 2026-10-07** (serial capture on the Rock 5C Core,
`tests/data/rotor/erc-gs232b-capture-2026-10-07.log`, plan Task 2). The ERC
enumerates as an FTDI FT230X USB serial port (`/dev/ttyUSB0` on Linux).
It answers at 9600 baud 8N1 and at no other rate tried (1200 to 115200), in
GS-232B format:

* `C2` replies `AZ=302  EL=000` + CR LF: two spaces before `EL`, and an
  elevation of 000 even on this azimuth-only rotor. `C` replies `AZ=302` +
  CR LF. `B` (elevation only) gets no reply.
* `Maaa`, `Waaa eee`, `S`, `L` and `R` each answer a bare CR, about 25 ms
  after the command, and all work on the azimuth-only rotor: `M312` from 302
  stopped at 313, `W292 000` from 313 stopped at 292.
* The rotor turns about 4 to 5 degrees a second. Position replies take about
  90 ms when still and up to about 210 ms while turning.
* After `S` during an `L` or `R` move the heading coasts on by 2 to 4
  degrees, so arrival and stop logic reads the heading after it settles.
* The ERC's firmware version and Service Tool settings were not read.

## Core

New files, following the PGXL/TGXL/RF2K-S pattern:

* `RotorConnection` (`src/core/RotorConnection.*`): one class, four
  drivers behind it (GS-232A, GS-232B, a running rotctld, and rotctld
  started by the Core). Serial through
  `QSerialPort` (already used by `src/core/mmio/SerialEndpointWorker`), TCP
  through `QTcpSocket`. It reads the position on a timer (about once a
  second when still, faster while turning), reports heading, elevation,
  moving and connected, and sends set, stop and move.
* `StationRotorController` (`src/core/StationRotorController.*`): owns the
  connection, its settings, presets and faults, and serves the session
  objects and commands below. Reconnects as the other accessories do.

Behaviour:

* **Fresh or stale.** Every position reply stamps the time; 1500 ms without
  one marks the heading stale until the next reply.
* **Strict headings.** Every target is checked before it is sent (see
  Prior art).
* **rotctld started by the Core** (driver 4): a `RotctldProcess` beside the
  connection; "Hamlib's rotctld is not installed on the Core's computer."
  when the binary is missing.
* **Target and arrival.** A set command records the target; the rotor is
  "turning" until the read heading is within 1.5 degrees of the target
  (Longpath `kArrivedDeg`, `RotctldClient.cpp:30`) or stops changing, then
  "stopped".
* **Hold to nudge has a dead man.** A window's CCW or CW (or Up/Down) hold
  sends start, then repeats it while held; the Core sends stop if the repeat
  lapses or that window disconnects. A dropped phone never leaves the
  rotor turning.
* **Calibration offset and range** (rotor setup): an offset added to the
  read heading, and the rotor's range (360 or 450 degrees for rotors with
  overlap).
* **Turning while on the air is allowed** (JJ, 2026-10-07). A rotor does
  not switch RF, unlike the amp and tuner controls that wait (the
  2026-09-25 on-air rule), and Thetis does not block it either.
* **Stop wins.** Stop from any window is sent at once, ahead of anything
  queued.

### Session objects and commands

The wire contract is
[remote rotor control version 1](2026-10-07-remote-rotor-control-v1.md), in
the same style as the accessory contract, so the iPhone builds against it.
The build plan is [2026-10-07-rotor-control-plan.md](2026-10-07-rotor-control-plan.md).
In short:

* `rotor` object: the connection phase and error, the driver and its
  settings (serial port and baud, or host and port, or Hamlib model), the
  Core's serial ports, whether `rotctld` is installed, a `label` ("Easy
  Rotor Control on COM4"), axes, range, offset, the heading and elevation
  with `positionFresh`, the targets, `motion` (stopped, turning, nudging),
  presets and the last fault. The contract has the exact names and kinds.
* Commands: `setRotorTarget`, `turnRotorToCall` (the Core works out the
  bearing), `stopRotor`, `nudgeRotor` (direction, held or released),
  `configureRotor`, `disconnectRotor`, `setRotorPresets`.
* Tools catalogue (`StationCatalog`): a `rotor` entry, offered when a rotor
  is configured on this Core.
* Spots: the Core adds the short-path bearing to each spot it serves, so
  the iPhone and the desktop show the same number without their own
  cty.dat. Long path is short path + 180, worked out by the window.

### Bearing

* Extend `DxccEntity` with latitude and longitude (cty.dat columns 5 and 6,
  longitude + west converted to the usual + east), honouring the
  `<lat/long>` overrides.
* Station position from `User/GridSquare` (Maidenhead to the centre of the
  square).
* Initial great-circle bearing from station to entity. With no grid set, a
  spot has no bearing and the turn-beam actions are greyed with the reason
  ("Set your grid square in Setup to turn the beam to spots.").

## Desktop

* **Rotor dial** (`src/gui/widgets/RotorDialWidget.*`): ported from
  Longpath's `RotorDialWidget` with its header and credit, recoloured to
  NereusSDR's palette and with English operator words. Rose (default) or
  tape, switched from the dial's right-click menu. Our addition: on an
  az/el rotor, an elevation quarter gauge (0 to 90) beside the rose in the
  same style (amber needle, dashed cyan target, travel sector); Longpath
  shows elevation only as a corner readout. Dragging the dial sets the
  target and the rotor turns when the mouse is released.
* **Rotor applet** (`src/gui/applets/RotorApplet.*`, registered as
  `applet:rotor` in `ContainerContentRegistry`): status line, the dial, the
  heading readout and "to go" under it, CCW / STOP / CW, Down / Up for
  az/el, short path / long path, presets, and a "Turn to" callsign box.
  Matches the mockup.
* **Drag to turn on `RotatorItem`.** Port Thetis's drag handling and
  `SendRotatorMessage` behaviour (MeterManager.cs:16475 and around
  36722-37217) with full attribution and inline comments, but send through
  the rotor commands instead of an MMIO template. Thetis already sends on
  mouse release, not while dragging (MeterManager.cs:37207-37217), which
  matches the desktop touch rule. This is the meter item in user layouts. The port shows no elevation in "Both" mode: it draws
  `m_smoothedEle`, which nothing updates (`RotatorItem.cpp:492`); trace how
  Thetis feeds elevation and fix it in the same task.
* **No rotor:** the applet stays, controls greyed, with the reason.
* **Rotor setup** on the accessories Setup page beside the PGXL, TGXL and
  RF2K-S: driver, serial port and baud or host and port, axes, range,
  offset, presets.
* **Pan spot menu** (`SpectrumWidget`, the spot right-click menu): "Turn beam
  to CALL (330°)" under "Tune to CALL".
* **Spot Hub:** a Bearing column, and a right-click menu with Tune, Turn
  beam, Copy Callsign and Lookup on QRZ. Double-click still tunes.
* **Auto-turn setting:** "Turn the beam when I tune to a spot", off by
  default. A window preference (client-authoritative, like other UI
  preferences).

## iPhone

* `Accessories/RotorPage.swift`: the page in the mockup: status, the
  Longpath-style dial drawn in SwiftUI (the same design as the desktop
  dial), the heading readout and "to go", a touch dial (drag to select; a "Turn to N°" button under the readout sends it; a selection not sent within 15 s is dropped), large STOP between
  the nudge buttons, short / long path, preset chips, Up / Down for az/el.
  Uses `AccessoryChrome` and `AccessoryStatusLine` like the amp and tuner
  pages.
* Accessories screen: a Rotor row.
* Tools tab (`StationToolList`): a `rotor` page entry opening the same page.
* `SpotDetailsSheet`: the bearing (short and long) in the details, and a
  "Turn beam 330°" button between Tune and Close.
* The auto-turn setting, off by default, on this phone.

## Tests

* Driver tests against a fake serial port and a fake `rotctld`: each
  protocol's replies, including the ones Hamlib treats as invalid, and the
  bench capture from JJ's ERC.
* Controller tests: arrival, stop priority, the nudge dead man, a window
  disconnecting mid-nudge, reconnect, the stale heading after 1500 ms,
  strict heading refusals, and the Core starting and stopping `rotctld`.
* Desktop tests: the dial sends nothing while dragging and one target on
  release.
* Bearing tests: grid to position, known bearings, cty.dat overrides.
* Session contract tests for the `rotor` object and commands.
* iPhone unit tests for the page model (a drag sends nothing, Turn sends
  once, a selection lapses at 15 s); a UI test for the spot sheet's Turn
  beam button.
* Bench: JJ's Yaesu with the ERC, before release.

## Open questions

* The ERC's protocol and baud on the operator's unit, and its exact replies:
  answered by the bench capture (plan Task 2). Defaults until then: GS-232B
  at 9600 baud, the maker's setup example.
