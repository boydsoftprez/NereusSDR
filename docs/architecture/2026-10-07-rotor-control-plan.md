# Rotor Control Implementation Plan

> **Execution:** run with `crew` under `cost-aware-execution`. Acceptance cases are binding;
> test order follows the risk-based policy. A whole-branch review closes the plan before it
> reaches the operator's rotor.

**Goal:** The operator turns the station's antenna rotor from NereusSDR: a Rotor applet in a desktop
container, a Rotor page on the iPhone (under Accessories and in the Tools tab), and "Turn beam"
from a spot on the pan, in Spot Hub and in the iPhone's spot sheet. First rotor: the operator's
Yaesu on an Easy Rotor Control (ERC). Any GS-232A, GS-232B or Hamlib `rotctld` rotor works the same way.

**Architecture:** The Core owns the rotor, as it owns the PGXL, TGXL and RF2K-S
(`RotorConnection` for the four drivers, `StationRotorController` for state, settings, presets
and the hold dead man). Every window is a thin client on the `rotor` object and commands of
[remote rotor control version 1](2026-10-07-remote-rotor-control-v1.md). Bearings are the
Core's: cty.dat positions and the station's grid square, served on each spot.

**Tech Stack:** C++20, Qt6 (`QSerialPort`, `QTcpSocket`), the station link (`StationServer`,
`StationClient`), the meter and applet system, SwiftUI and `NereusKit` on the iPhone.

**Design and decisions:** [2026-10-07-rotor-control-design.md](2026-10-07-rotor-control-design.md)
(agreed 2026-10-07: both protocol families; a Rotor applet and iPhone page built on a
Longpath-style dial in NereusSDR colours, with the Thetis compass kept as the meter item and
gaining drag-to-turn; Accessories plus Tools on the iPhone; elevation as a setup option, with an
elevation gauge beside the rose; turn-to-spot as its own action with an auto-turn setting off
by default; turning allowed on the air; the desktop turns on release, the iPhone selects then
confirms with a 15 s expiry). Mockup:
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
- Longpath reference: `../Longpath/`, pinned `551576e` (v0.6.7-1); cite it as
  `// From Longpath <file>:<line> [@551576e]`. Never pull it mid-task.
- The Longpath ports (Tasks 3 and 6) follow HOW-TO-PORT.md as for any upstream: Longpath's
  file header byte for byte with its modification history, a NereusSDR modification-history
  line, `// From Longpath src/gui/widgets/RotorDialWidget.cpp:NNNN [@shortsha]` cites, inline
  comments kept verbatim (they are German; keep them), a new
  `docs/attribution/LONGPATH-PROVENANCE.md` row. User-facing strings are rewritten in English
  operator words; NereusSDR's palette replaces Longpath's.
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
shows, and the replies to `C`, `C2`, `Maaa`, `Waaa eee`, `S`, `L`, `R` on the operator's
azimuth-only rotor. Save the log under `tests/data/rotor/` as a fixture with the date and ERC firmware
version, and record the findings in the design's "Facts" section. Task 3b's driver tests replay
it.

## Task 3a: Rotor rules: headings, routes and the overlap, model list

Pure logic, no I/O. Starts the Longpath provenance file.

**Files:**
- Create: `src/core/RotorHeading.h` (strict heading checks, after Longpath `RotorPeilung.h`)
- Create: `src/core/RotorRoute.{h,cpp}` (ported from Longpath `BeamHeading::plan` and `Stop`,
  extended to the overlap: span position from the stop, the nearer of two span positions,
  signed travel; and the span tracker that follows modulo-360 replies by continuity, as the
  design's "End stops and overlap" says)
- Create: `src/core/RotorModels.h` (the curated Hamlib model list, ERC 404 among them; model
  numbers checked against Hamlib's `rotlist.h`, not copied on trust)
- Create: `docs/attribution/LONGPATH-PROVENANCE.md` (one row per ported file; follow the shape
  of the existing provenance files)
- Test: `tst_rotor_heading`, `tst_rotor_route`

**Acceptance:**
- Not-a-number, infinite and out-of-range headings are refused, never wrapped; 360 is north and
  is sent as 0; an empty text box is refused, never north.
- Route and overlap, from the full-range capture with a south stop and range 450: 183 to 010
  travels +187; 292 to 000 travels +68; 301 to 180 travels -121; replaying the capture's
  clockwise run from 183 (span 3) to the stop gives span position 449 at the reply `AZ=269`,
  446 degrees of travel. A first reply inside the overlap band gives span position -1 and
  `routeKnown` false until a reply outside the band. With no end stop the route is the shorter
  way. With a north stop and range 360, 350 to 010 travels -340.
- The offset applies to the reported heading and is removed from a target.

## Task 3b: `RotorConnection` and `RotctldProcess`

**Files:**
- Create: `src/core/RotorConnection.{h,cpp}` (GS-232A, GS-232B over `QSerialPort`; `rotctld`
  over `QTcpSocket`; position polling about once a second when still and faster while turning;
  set, stop, move; uses Task 3a's tracker for the span position)
- Create: `src/core/RotctldProcess.{h,cpp}` (ported from Longpath `RotctldProcess`: find the
  binary including Homebrew paths, the `-m -r -s -T 127.0.0.1 -t` arguments, a free port when
  4533 is held, restart, stop on exit)
- Modify: `docs/attribution/LONGPATH-PROVENANCE.md` (the `RotctldProcess` row)
- Test: `tst_rotor_connection` (each driver against a fake port and a fake `rotctld`, including
  Hamlib's invalid replies and both Task 2 captures replayed), `tst_rotctld_process`

**Acceptance:**
- Each driver reads azimuth (and elevation on az/el) and sends set, stop and move in exactly the
  cited formats; GS-232B parses `AZ=302  EL=000` (two spaces) and `AZ=302`, and treats the bare
  CR acknowledgement, a bare CR LF and `>` as no position.
- No position reply for 1500 ms sets `positionFresh` false; the next reply sets it true.
- Driver 4 starts `rotctld` with the chosen model, port and baud, and stops it on disconnect and
  on exit; with no `rotctld` installed it refuses with the document's reason.
- Stop is written ahead of anything queued.

## Task 3c: `StationRotorController` in the Core

**Files:**
- Create: `src/core/StationRotorController.{h,cpp}` (settings under `Rotor/*`, presets,
  target and arrival, stop priority, the hold dead man of 250 ms repeats and a 750 ms lapse,
  reconnect as the other accessories do, the Core's serial port list)
- Modify: the Core's accessory start-up, where the PGXL, TGXL and RF-Kit controllers are made
- Test: `tst_station_rotor_controller`

**Acceptance:**
- Arrival within 1.5 degrees of the target, or when the heading stops changing, sets motion
  stopped.
- Stop jumps the queue. A lapsed hold, or its window's session ending, sends stop.
- Turning works while the radio is on the air.
- Settings persist with `AppSettings` under the contract's keys and defaults.

## Task 4a: cty.dat on the Core, and bearings on spots

**Files:**
- Modify: the build's resources (move `cty.dat` into the Core's resources with a
  `Q_INIT_RESOURCE`, as the band plans were moved), so `nereusd` has it; load it once on the
  Core and share that one parser with the rotor controller and the spot stream; the desktop
  keeps working from the same single copy
- Modify: the spot stream the Core serves (`bearingDeg`, short path, -1 with no grid or no
  placeable callsign), `StationClient` and the desktop spot model (carry it)
- Modify: `docs/architecture/2026-10-07-remote-rotor-control-v1.md` (fix the spot field
  placement), the link document's tables, `surface.json` with its regen target
- Test: a Core test that a spot from a placeable callsign carries the same bearing Task 1
  computes and -1 with no grid; a test that `nereusd`'s Core resolves a callsign; the link
  manifest test

**Acceptance:**
- On the headless Core, `turnRotorToCall` places a callsign (it refused every call before).
- A spot from a placeable callsign carries the same bearing Task 1 computes; -1 with no grid.
- Only one cty.dat is loaded per process.

## Task 4b: The link: the `rotor` object, commands and tools catalogue

**Files:**
- Modify: `StationServer` and the accessory command handling (the `rotor` object and the seven
  commands, the refusals word for word, mirrored from Task 3c's controller), `StationCatalog`
  (`rotor` entry when configured), `StationClient` (send the commands, mirror the object)
- Modify: `docs/architecture/2026-10-07-remote-rotor-control-v1.md` (mark the sections shipped;
  add the reason "That rotor setup is not valid." for an unknown driver, axes or end stop, or a
  port outside 1 to 65535), the link document's tables, `surface.json`
- Test: Core command tests for every command and refusal, conformance fixtures for all three
  runners, `tst_link_surface_manifest`

**Acceptance:**
- `remoteRotorControlVersion` is 1 on a Core that owns a rotor connection, 0 otherwise.
- Every refusal in the document is produced, with no change applied.
- Only windows admitted to change station accessories can turn, configure or stop the rotor;
  a window's session ending ends its hold.
- `hamlibModel` is sent to the controller as 0 unless the driver is 4.

## Task 5: Drag to turn on the compass (`RotatorItem`)

**Files:**
- Modify: `src/gui/meters/RotatorItem.{h,cpp}` (port Thetis's click and drag handling and
  `SendRotatorMessage`'s behaviour, MeterManager.cs:16475 and the drag code around
  36722-37217, through `MeterItem::handleMouse*`; the target marker in amber; send through a
  rotor interface rather than an MMIO template)
- Fix: elevation in "Both" mode (`m_smoothedEle` is never updated; trace Thetis's elevation
  feed first, then match it)
- Test: an offscreen `RotatorItem` test: a drag sets the expected target; a press in the centre
  stop circle stops; elevation shows the value set

**Acceptance:**
- In a user meter layout, dragging the compass and letting go turns the rotor (nothing is sent
  while dragging, as in Thetis); with no rotor it does nothing and shows no target.
- Attribution and inline comments as the Global Constraints say.

## Task 6: The rotor dial and the Rotor applet

**Files:**
- Create: `src/gui/widgets/RotorDialWidget.{h,cpp}` (ported from Longpath, see Global
  Constraints: rose and tape shapes, amber heading needle, dashed target arrow, travel sector
  along the Core's predicted route (`travelDeg`, the long way when the stop forces it), the end
  stop marked on the rim, no sector while `routeKnown` is false,
  arrival tolerance and green on arrival, an elevation quarter gauge beside the rose on az/el
  rotors (our addition, see the design), drag to aim and turn on release, a stale
  heading drawn muted with its age; shape choice saved with `AppSettings`)
- Create: `src/gui/applets/RotorApplet.{h,cpp}` (status line, the dial, heading readout and "to
  go" under it, CCW / STOP / CW, Down / Up on az/el, short and long path, presets, "Turn to"
  callsign box; as in the mockup)
- Modify: `ContainerContentRegistry` (`applet:rotor`, "Rotor")
- Test: an offscreen applet test for each state in the mockup (azimuth turning, az/el stopped,
  no rotor greyed with its reason, a Core too old)

**Acceptance:**
- Matches the mockup. Stop is the only red control. The hold buttons send the dead-man repeats
  and stop on release.
- Dragging the dial sends nothing until the mouse is released, then one `setRotorTarget`.

## Task 7: Rotor setup on the desktop

**Files:**
- Modify: the accessories Setup page beside the PGXL, TGXL and RF2K-S (driver, the Core's serial
  ports and baud, or host and port; axes; end stop; range; offset; presets editor), sending
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
- Create: `ios/NereusApp/Accessories/RotorPage.swift` and its model (the Longpath-style dial in
  SwiftUI, matching the desktop dial, including the predicted route and end stop; a drag only selects, drawn in the target colour, and a "Turn to N°"
  button sends it, with an unsent selection dropped after 15 s; presets, Stop and the nudge holds
  are one tap; STOP between the nudge buttons; short and long path; preset chips; Up and
  Down on az/el; the rotor setup card with the Core's serial ports)
- Modify: the Accessories screen (a Rotor row), `StationToolList` (`rotor` page entry and its
  greyed reason), `NereusKit` (the `rotor` object and commands)
- Test: page model unit tests (a drag sends nothing; Turn sends `setRotorTarget` once; a
  selection lapses at 15 s; a sent target never lapses); a UI test that the Tools row and the
  Accessories row open the same page

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

The operator turns the Yaesu from the applet, the compass in a meter layout, the iPhone page,
the pan menu, Spot Hub and the spot sheet; holds and releases the nudge buttons; checks that an iPhone drag does nothing until Turn is
tapped and that an unsent selection lapses after 15 s; drops the
phone's Wi-Fi mid-hold and confirms the rotor stops; and checks the heading against the
controller's dial.
