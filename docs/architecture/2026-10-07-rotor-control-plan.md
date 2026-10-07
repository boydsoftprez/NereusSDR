# Rotor Control Implementation Plan

> **Execution:** run with `crew` under `cost-aware-execution`. Acceptance cases are binding;
> test order follows the risk-based policy. A whole-branch review closes the plan before it
> reaches the operator's rotor.

**Goal:** The operator turns his antenna rotor from NereusSDR: a Rotor applet in a desktop
container, a Rotor page on the iPhone (under Accessories and in the Tools tab), and "Turn beam"
from a spot on the pan, in Spot Hub and in the iPhone's spot sheet. First rotor: his Yaesu on
an Easy Rotor Control (ERC). Any GS-232A, GS-232B or Hamlib `rotctld` rotor works the same way.

**Architecture:** The Core owns the rotor, as it owns the PGXL, TGXL and RF2K-S
(`RotorConnection` for the three drivers, `StationRotorController` for state, settings, presets
and the hold dead man). Every window is a thin client on the `rotor` object and commands of
[remote rotor control version 1](2026-10-07-remote-rotor-control-v1.md). Bearings are the
Core's: cty.dat positions and the station's grid square, served on each spot.

**Tech Stack:** C++20, Qt6 (`QSerialPort`, `QTcpSocket`), the station link (`StationServer`,
`StationClient`), the meter and applet system, SwiftUI and `NereusKit` on the iPhone.

**Design and decisions:** [2026-10-07-rotor-control-design.md](2026-10-07-rotor-control-design.md)
(agreed 2026-10-07: both protocol families, applet around the existing compass, Accessories plus
Tools on the iPhone, elevation as a setup option, turn-to-spot as its own action with an
auto-turn setting off by default, turning allowed on the air). Mockup:
[2026-10-07-rotor-control-mockup.html](2026-10-07-rotor-control-mockup.html).

## Global Constraints

- CLAUDE.md and CONTRIBUTING.md bind: `AppSettings` (never `QSettings`), no raw new/delete,
  braces on all control flow, `qCWarning` for errors, `Q_OS_*` guards, Rule R1 (nothing in
  `src/core/` includes a GUI header).
- Protocol facts come from the sources the design cites (Hamlib `gs232a.c`, `gs232b.c`,
  `rotctld(1)`; the ERC maker's manuals; the cty.dat format notes). Hamlib is cited for facts,
  not ported. The ERC manuals are cited, never copied. Where the sources leave a gap (the
  azimuth-only set command and replies on the operator's ERC), stop and use the bench capture
  from Task 2.
- The Thetis drag-to-turn port (Task 5) follows HOW-TO-PORT.md: the MeterManager.cs header and
  Samphire dual-licence block byte for byte (already on `RotatorItem`), a modification-history
  line, `// From Thetis MeterManager.cs:NNNN [v2.10.3.15]` cites, every inline comment kept,
  `scripts/verify-inline-tag-preservation.py` clean.
- The link: every new object, command, capability and refusal goes in the remote rotor control
  document and the link document's tables; `tests/data/link/v1/surface.json` changes only with
  its regen target; `python3 scripts/render-link-tables.py --check` passes;
  `kSessionProtocolMinor` does not change.
- User-facing strings in plain operator words (`OperatorWording::isPlain`); no em dash; no
  cites in user strings.
- Disabled, never hidden: with no rotor, a Core too old, or no grid square, controls stay
  visible, greyed, with the reason.
- Tests run with `QT_QPA_PLATFORM=offscreen`; build and run single named tests. No test opens a
  real serial port or turns a real rotor.
- Commits GPG-signed with hooks; never `--no-verify` or `--no-gpg-sign`; no `Co-Authored-By`;
  explicit pathspecs.

## Task 1: Bearings from cty.dat and the grid square

**Files:**
- Modify: `src/core/CtyDatParser.{h,cpp}` (latitude and longitude on `DxccEntity`, longitude
  converted from cty.dat's + west to + east; `<lat/long>` prefix overrides honoured)
- Create: `src/core/GreatCircle.{h,cpp}` (Maidenhead to position at the square's centre, 4 or 6
  characters; initial great-circle bearing; long path as short + 180)
- Test: `tst_great_circle`, additions to the cty.dat parser test

**Acceptance:**
- Known pairs give known bearings within 1 degree (fixtures computed from a cited reference
  calculator, recorded in the test with the source).
- An override line wins over the entity's position.
- A bad or empty grid gives "no bearing", never 0.

## Task 2: Bench capture from the operator's ERC

**Owner:** the operator, with the controller. No code.

Record, with a serial logger on the ERC's port: the protocol and baud the ERC's Service Tool
shows, and the replies to `C`, `C2`, `Maaa`, `Waaa eee`, `S`, `L`, `R` on his azimuth-only
rotor. Save the log under `tests/data/rotor/` as a fixture with the date and ERC firmware
version, and record the findings in the design's "Facts" section. Task 3's driver tests replay
it.

## Task 3: `RotorConnection` and `StationRotorController`

**Files:**
- Create: `src/core/RotorConnection.{h,cpp}` (GS-232A, GS-232B over `QSerialPort`; `rotctld`
  over `QTcpSocket`; position polling about once a second when still and faster while turning;
  set, stop, move)
- Create: `src/core/StationRotorController.{h,cpp}` (settings under `Rotor/*`, presets,
  target and arrival, stop priority, the hold dead man of 250 ms repeats and a 750 ms lapse,
  reconnect as the other accessories do, the Core's serial port list)
- Modify: the Core's accessory start-up, where the PGXL, TGXL and RF-Kit controllers are made
- Test: `tst_rotor_connection` (each driver against a fake port and a fake `rotctld`, including
  Hamlib's invalid replies and the Task 2 capture), `tst_station_rotor_controller`

**Acceptance:**
- Each driver reads azimuth (and elevation on az/el) and sends set, stop and move in exactly the
  cited formats.
- Stop jumps the queue. A lapsed hold, or its window's session ending, sends stop.
- The offset applies to the reported heading and is removed from a target.
- Turning works while the radio is on the air.

## Task 4: The link: `rotor` object, commands, spot bearings, tools catalogue

**Files:**
- Modify: `StationServer` and the accessory command handling (the `rotor` object and the seven
  commands, the refusals word for word), the spot stream (`bearingDeg`), `StationCatalog`
  (`rotor` entry when configured), `StationClient` (send the commands, mirror the object)
- Modify: `docs/architecture/2026-10-07-remote-rotor-control-v1.md` (fix the spot field
  placement; mark the sections shipped), the link document's tables, `surface.json`
- Test: Core command tests for every command and refusal, conformance fixtures for all three
  runners, `tst_link_surface_manifest`

**Acceptance:**
- `remoteRotorControlVersion` is 1 on a Core that owns a rotor connection, 0 otherwise.
- Every refusal in the document is produced, with no change applied.
- A spot from a placeable callsign carries the same bearing Task 1 computes; -1 with no grid.

## Task 5: Drag to turn on the compass (`RotatorItem`)

**Files:**
- Modify: `src/gui/meters/RotatorItem.{h,cpp}` (port Thetis's click and drag handling and
  `SendRotatorMessage`'s behaviour, MeterManager.cs:16475 and the drag code around
  36722-37217, through `MeterItem::handleMouse*`; the target marker in amber; send through a
  rotor interface rather than an MMIO template)
- Test: an offscreen `RotatorItem` test: a drag sets the expected target; a press in the centre
  stop circle stops

**Acceptance:**
- In a user meter layout, dragging the compass turns the rotor; with no rotor it does nothing
  and shows no target.
- Attribution and inline comments as the Global Constraints say.

## Task 6: The Rotor applet

**Files:**
- Create: `src/gui/applets/RotorApplet.{h,cpp}` (status line, heading readout, the compass as a
  `MeterWidget` hosting a `RotatorItem`, CCW / STOP / CW, Down / Up on az/el, short and long
  path, presets, "Turn to" callsign box; as in the mockup)
- Modify: `ContainerContentRegistry` (`applet:rotor`, "Rotor")
- Test: an offscreen applet test for each state in the mockup (azimuth turning, az/el stopped,
  no rotor greyed with its reason, a Core too old)

**Acceptance:**
- Matches the mockup. Stop is the only red control. The hold buttons send the dead-man repeats
  and stop on release.

## Task 7: Rotor setup on the desktop

**Files:**
- Modify: the accessories Setup page beside the PGXL, TGXL and RF2K-S (driver, the Core's serial
  ports and baud, or host and port; axes; range; offset; presets editor), sending
  `configureRotor` and `setRotorPresets`
- Test: a setup page test in a local and a remote window

**Acceptance:**
- A remote window lists the Core's serial ports, not its own computer's.
- A refusal shows on the page while it is open, as a notice otherwise (the accessory rule).

## Task 8: Turn beam from spots on the desktop, and auto-turn

**Files:**
- Modify: `src/gui/SpectrumWidget.cpp` (the spot right-click menu: "Turn beam to CALL (330°)"
  under "Tune to CALL")
- Modify: `src/gui/SpotHubDialog.cpp` (a Bearing column; a right-click menu with Tune, Turn
  beam, Copy Callsign, Lookup on QRZ; double-click still tunes)
- Modify: the window's preferences ("Turn the beam when I tune to a spot", `Rotor/TurnOnTune`,
  off by default)
- Test: menu and column tests; auto-turn on and off

**Acceptance:**
- Without a rotor or a bearing, Turn beam is shown greyed with the reason.
- With auto-turn off, tuning never moves the rotor.

## Task 9: The iPhone Rotor page

**Files:**
- Create: `ios/NereusApp/Accessories/RotorPage.swift` and its model (the touch compass: drag to
  set, release to go; STOP between the nudge buttons; short and long path; preset chips; Up and
  Down on az/el; the rotor setup card with the Core's serial ports)
- Modify: the Accessories screen (a Rotor row), `StationToolList` (`rotor` page entry and its
  greyed reason), `NereusKit` (the `rotor` object and commands)
- Test: page model unit tests; a UI test that the Tools row and the Accessories row open the
  same page

**Acceptance:**
- Matches the mockup. A held nudge sends the dead-man repeats; leaving the page or the app going
  to the background ends the hold.

## Task 10: Turn beam from the iPhone's spot sheet, and auto-turn

**Files:**
- Modify: `ios/NereusApp/Spots/SpotDetailsSheet.swift` (bearing short and long in the details;
  "Turn beam 330°" between Tune and Close), the spot model (`bearingDeg`), the phone's settings
  (auto-turn, off by default)
- Test: a UI test for the Turn beam button, greyed with no rotor or no bearing

## Task 11: Bench verification

**Owner:** the operator. The controller deploys the Core and the desktop build and installs the
phone build.

The operator turns his Yaesu from the applet, the compass in a meter layout, the iPhone page,
the pan menu, Spot Hub and the spot sheet; holds and releases the nudge buttons; drops the
phone's Wi-Fi mid-hold and confirms the rotor stops; and checks the heading against the
controller's dial.
