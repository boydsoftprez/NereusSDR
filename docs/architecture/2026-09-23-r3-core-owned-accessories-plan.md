# R3 Core-owned amplifiers and tuner implementation plan

> **Execution:** run with `crew` under `cost-aware-execution`. Requirements and
> acceptance cases are binding; one whole-branch review at the end of the batch.
> Runs in the lane the controller names at dispatch, after the lane B carry and after
> the receiver audio plan's Task 4 (both touch the TCI server and its Setup pages).

**Goal:** the Power Genius XL, the Tuner Genius XL and the RF-Kit RF2K-S talk to the
Core only. The Core connects, checks each device's identity, keeps it connected,
follows the band, records its faults, and relays status, meters and faults to every
client; clients change connection settings and the interlock policy through
documented session commands. The desktop remote window and the iPhone app are thin
clients of that one API. Transmit-coupled actions (Operate, Tune, relay and antenna
changes, interlock enforcement) are named here and built with remote transmit.

**Architecture:** each accessory gets a Core-side controller with the lifecycle the
Tuner Genius already has (identity before admission, cancellable retries, a
connection phase), a mirrored read-only status object, and typed session commands
gated by a per-feature capability version. Station logic that lives in the desktop
window today (gauge conversions, fault capture, the power-cap alert, diagnostics)
moves to the Core; applets and Setup pages become views plus commands in both local
and remote windows. The Core also runs the app's existing TCI server on the station
network so the RF-Kit can follow the band, switched by the same TCI switch and port
as the app and remembered on the Core. One accessory control document describes the
objects, fields, units, fixed enum values, commands and refusals for non-Qt clients.

**Tech stack:** C++20, Qt 6, Qt Test (off-screen), `PgxlConnection`,
`TgxlConnection`, `StationTgxlController`, `Rf2ksConnection`, `SmartSdrApiListener`,
`LanDiscovery`, `TunerModel`, `TxInterlockPolicy`, `FaultLog`, `TuneMemoryStore`,
`ConnectionDiagnostics`, `TciServer`, `StationServer`, `SessionCommandDispatcher`,
`MirrorSchema`, `MirrorPolicy`, `StationCapabilities`, `DaemonApp`.

**Spec:** operator direction of 2026-09-23 (accessories talk to the Core only; the
Core relays both ways; thin clients; a documented API for the iPhone app); operator
answers of 2026-09-23: pair the Power Genius automatically, but only after the Core
confirms it is a real Power Genius; the Core runs its own TCI server on the station
network for devices like the RF-Kit, controlled by the one TCI switch and port and
remembered on the Core; the TCI page shows "Also at the station: <address>, port
<port>"; the amp page and applet show the band-follow state. Controller decisions:
the 4992 listener and accessory discovery bind to the station network only
(configurable in `nereusd.conf`); the newest connection keeps control of the
accessories until remote transmit brings ask-first. R3 plan requirements R-R3-22,
R-R3-25 and new R-R3-47, R-R3-48 (added to the R3 plan's requirement table with this
plan). The iPhone app design (branch `claude/nereussdr-iphone-app-5fb988`,
`docs/architecture/2026-09-23-iphone-app-design.md`) requires a written, versioned link
and a conformance suite before remote transmit. Scout notes with file:line:
`/Users/j.j.boyd/.config/nereus/work/r3-core-owned-accessories-scout-2026-09-23.md`.

## Global Constraints

- Work in the worktree, branch and build directory the controller names at dispatch.
- Commits: GPG-signed with hooks (`NEREUS_THETIS_DIR=/Users/j.j.boyd/Thetis`), never
  `--no-gpg-sign` or `--no-verify`, no `Co-Authored-By`, no em-dash characters. Stage
  explicit paths only. Every commit names its R-R3 IDs.
- Protocol: per-feature capability versions (`remotePgxlControlVersion`,
  `remoteRfKitControlVersion`, `stationTciVersion`, `accessoryDataVersion`), never a
  change to `kSessionProtocolMinor`. Enum values on the wire are fixed and only
  appended to. Older apps and older Cores keep today's behaviour; goldens stay
  byte-for-byte.
- Receive-only guard (R-R3-25): nothing in this plan keys a transmitter, operates an
  amplifier, starts a tune carrier or changes a relay or antenna on a transmit path.
  Those actions stay refused on the Core with a plain reason until remote transmit.
- A remote window never opens an accessory socket; accessory lifecycles on the Core
  outlive client sessions.
- Operator wording: plain user words; every new string passes
  `OperatorWording::isPlain`; Core reasons go through `OperatorReasonText`.
- Local (direct radio) operation keeps its behaviour; where station logic moves out
  of `MainWindow`, local windows use the same Core-side code in-process.
- Tests: prefix every ctest and test binary with `QT_QPA_PLATFORM=offscreen`. Build
  exact targets, then `ctest -R '^(...)$' --no-tests=error --output-on-failure`. No
  unfiltered suite. Never build the `NereusSDR` target. No hardware; device evidence
  (identity replies, band follow on the real amps and tuner) stays pending for the
  operator checkpoint.

## What already exists

- The Core already creates and connects all three accessories from per-radio
  settings (scout A1); the Tuner Genius has a Core controller with identity checks,
  cancellable timers and a mirrored `tuner` object, plus `configureTgxl`,
  `disconnectTgxl` and `setFourO3AEnabled` verbs (scout A4, B7-B9).
- Power Genius: no identity check, automatic pairing on every connect, an
  uncancellable retry, status parsing split between the Core and `MainWindow` (scout A2).
- RF-Kit: dialled at radio connect, no identity check, no band on a headless Core,
  `faultObserved` never emitted, `rfKitEnabled` written raw (scout A5, B11).
- Station logic in the window: gauge conversions, the power-cap alert, diagnostics,
  fault history reloaded only on the first snapshot, the interlock policy loaded only
  at startup (scout C13-C15).
- Document templates: `docs/architecture/2026-09-20-remote-media-control-v1.md` and
  `docs/architecture/2026-09-23-remote-notch-control-v1.md`.

## Task 1: Status objects and the accessory control document

**Requirements:** R-R3-47, R-R3-22.

**Files:**
- Create: `docs/architecture/2026-09-23-remote-accessory-control-v1.md` (negotiation;
  the `tuner`, `amplifier` and `rfkit` objects and the 4O3A fields with units and
  fixed enum values; the connection-phase state machine; "accepted" is not
  "connected"; commands, argument kinds, refusal codes and texts; Core-owned settings
  keys; the fault record; which commands wait for remote transmit; evidence), fixture
  files the iPhone conformance suite can load
- Modify: `src/core/session/MirrorSchema.cpp`, `src/core/session/MirrorPolicy.cpp`,
  `src/core/session/StationServer.cpp` (read-only `amplifier` and `rfkit` objects with
  the Tuner Genius's connection-state shape plus meters in W, SWR ratio, degrees C, V,
  A), one Core-side conversion function for the Power Genius gauges moved out of
  `src/gui/MainWindow.cpp`, `src/gui/applets/AmpApplet.cpp` and `src/gui/applets/Rf2ksApplet.cpp` (read
  the models in local and remote windows)
- Test: `tests/tst_station_accessory_state.cpp`, `tests/tst_remote_peripherals.cpp`, new
  status cases from captured S/R lines and REST JSON already in the repository

**Acceptance:**
- In a remote window the Power Genius and RF-Kit gauges fill on attach, update, show
  false and zero values truthfully, and show a stale state when the Core is lost; the
  window opens no accessory socket.
- Local mode shows the same values as today through the moved conversion function.
- The document covers every object, field, command and refusal this plan adds; its
  fixtures parse in a test.

**Verification:**
```sh
cmake --build <build dir> --target tst_station_accessory_state tst_remote_peripherals -j6
QT_QPA_PLATFORM=offscreen ctest --test-dir <build dir> -R '^(tst_station_accessory_state|tst_remote_peripherals)$' --no-tests=error --output-on-failure
```

**Execution note (advisory):** opus.

- [ ] **Step 1:** Objects, conversion move and applet reads with tests.
- [ ] **Step 2:** The control document and fixtures; commit.

## Task 2: The Power Genius belongs to the Core

**Requirements:** R-R3-47, R-R3-22, R-R3-25.

**Files:**
- Create: `src/core/StationPgxlController.{h,cpp}` (the Tuner Genius controller's shape)
- Modify: `src/core/PgxlConnection.{h,cpp}` (a member retry timer with generation checks
  replacing the static single-shot; the connect path that gave invalid-descriptor
  errors on the Rock replaced by the Tuner Genius's working pattern), `src/models/RadioModel.cpp`
  (pairing only after the Core has admitted the device by discovery announcement plus
  the same serial in its own reply), `src/core/session/SessionCommandDispatcher.cpp`,
  `src/core/session/StationServer.cpp`, `src/core/session/StationClient.cpp` and
  `src/core/session/IStationLink.h` (`configurePgxl(host, port)`, `disconnectPgxl()`, connection
  settings: auto-reconnect, keepalive, ping interval), `src/gui/setup/CatNetworkSetupPages.cpp`
  (the Peripherals Power Genius row), `src/gui/setup/FourO3APage.cpp` (the Power Genius
  tab as a view plus commands), the control document
- Test: new `tests/tst_station_pgxl_controller.cpp`, `tests/tst_pgxl_connection_reconnect.cpp`,
  `tests/tst_remote_peripherals.cpp`

**Acceptance:**
- A Tuner Genius (or anything else) answering at the Power Genius address is never
  admitted and receives no pairing command; a real Power Genius (fixture discovery
  plus matching serial) is admitted and then paired; band follow works after pairing.
- Disabling or reconfiguring in any phase never redials the old address; replacing A
  with B while A is pending leaves only B.
- A remote window connects, disconnects and configures the Power Genius through the
  Core; Operate stays refused with the transmit reason.

**Verification:**
```sh
cmake --build <build dir> --target tst_station_pgxl_controller tst_pgxl_connection_reconnect tst_remote_peripherals -j6
QT_QPA_PLATFORM=offscreen ctest --test-dir <build dir> -R '^(tst_station_pgxl_controller|tst_pgxl_connection_reconnect|tst_remote_peripherals)$' --no-tests=error --output-on-failure
```
Hardware (pending, operator checkpoint): identity and pairing with the real Power
Genius; a capture of its discovery announcement and info reply.

**Execution note (advisory):** opus. Requires Task 1.

- [ ] **Step 1:** Controller, identity-before-pairing, cancellable retry with tests.
- [ ] **Step 2:** Verbs, capability, Setup views, document; commit.

## Task 3: The RF-Kit belongs to the Core, and the Core serves TCI at the station

**Requirements:** R-R3-47, R-R3-48, R-R3-22, R-R3-25.

**Files:**
- Modify: `src/core/Rf2ksConnection.{h,cpp}` (identity from `/info` before counting
  as connected; faults emitted), a Core-side RF-Kit controller beside the Power Genius
  one, `src/core/session/SessionCommandDispatcher.cpp`, `StationServer.cpp`,
  `StationClient.cpp`, `src/core/session/IStationLink.h` (`configureRfKit(host, port)`,
  `disconnectRfKit()`, `setRfKitEnabled(bool)`; `rfKitEnabled` becomes outbound only
  and raw writes are refused with a plain reason), `src/core/daemon/DaemonApp.cpp`
  (the Core runs the existing `TciServer` on its own radio model, bound to the station
  network, on the station TCI setting), the station TCI setting (enabled and port,
  stored on the Core) written by the app's one TCI switch and port
  (`src/gui/setup/CatNetworkSetupPages.cpp` TCI Server page, `src/gui/setup/AudioTciPage.cpp`),
  the TCI page's information line "Also at the station: <address>, port <port>", the
  RF-Kit page and applet's band-follow line ("Band follow: following the radio", or
  the address to enter on the amp), the Core switching the amp into TCI mode through
  its web interface when band follow is on, the control document
- Test: new `tests/tst_station_rfkit_controller.cpp`, `tests/tst_station_tci_server.cpp`,
  `tests/tst_remote_peripherals.cpp`, `tests/tst_tci_tx_mutex.cpp` (transmit over the
  Core's TCI refused until remote transmit), `tests/tst_rf2ks_connection_lifecycle.cpp`,
  `tests/tst_rf2ks_connection_parse.cpp`, `tests/tst_rfkit_radiomodel_enabled.cpp`,
  `tests/tst_rfkit_page_master_gate.cpp`

**Acceptance:**
- The RF-Kit is admitted only after its `/info` identifies it; enable, disable and
  configure work from a remote window through the Core; a raw `rfKitEnabled` write is
  refused.
- Turning the app's TCI switch on starts the window's server (as today) and the
  Core's station server on the same port; the Core keeps its switch when the window
  closes and when another app connects; turning it off stops both.
- A TCI client on the station network (the test's stand-in for the amp) receives the
  Core's `vfo:` and `split_enable:` updates as the Core's slice changes; transmit
  requests over the Core's TCI are refused with a plain reason until remote transmit.
- If the Core runs on the same computer as the window, one server runs and apps on
  that computer use it.
- The TCI page shows the station address line; the amp page and applet show the
  band-follow line with the right state.

**Verification:**
```sh
cmake --build <build dir> --target tst_station_rfkit_controller tst_station_tci_server tst_remote_peripherals tst_tci_tx_mutex tst_rf2ks_connection_lifecycle tst_rf2ks_connection_parse tst_rfkit_radiomodel_enabled tst_rfkit_page_master_gate -j6
QT_QPA_PLATFORM=offscreen ctest --test-dir <build dir> -R '^(tst_station_rfkit_controller|tst_station_tci_server|tst_remote_peripherals|tst_tci_tx_mutex|tst_rf2ks_connection_lifecycle|tst_rf2ks_connection_parse|tst_rfkit_radiomodel_enabled|tst_rfkit_page_master_gate)$' --no-tests=error --output-on-failure
```
Hardware (pending, operator checkpoint): the RF2K-S pointed at the Rock follows band
changes made from the remote window and from the iPhone app.

**Execution note (advisory):** opus. Requires Task 1 and the receiver audio plan's
Task 4.

- [ ] **Step 1:** RF-Kit controller, identity, verbs and refusals with tests.
- [ ] **Step 2:** The Core's station TCI server and the one switch, with tests.
- [ ] **Step 3:** Page and applet lines, document; commit.

## Task 4: Faults, diagnostics, interlock settings and the power-cap alert live on the Core

**Requirements:** R-R3-47, R-R3-22.

**Files:**
- Modify: `src/core/FaultLog.{h,cpp}` (a mirrored fault list with a revision; Tuner
  Genius fault capture; RF-Kit faults), `src/core/ConnectionDiagnostics.{h,cpp}`
  (counters carried in the status objects), `src/core/TxInterlockPolicy.{h,cpp}` (a typed
  live command to view and change the policy, mirrored back; enforcement unchanged and
  still on the Core), the power-cap alert computed on the Core and shown by windows,
  `src/core/TuneMemoryStore.{h,cpp}` and antenna labels readable remotely, the Advanced
  pages (`src/gui/setup/PgxlAdvancedPage.cpp`, `TgxlAdvancedPage.cpp`,
  `PgxlInterlockPage.cpp`) as views plus commands, the control document
- Test: `tests/tst_fault_log.cpp`, `tests/tst_tx_interlock_policy.cpp`,
  `tests/tst_remote_peripherals.cpp`, `tests/tst_station_accessory_state.cpp`

**Acceptance:**
- A fault raised on the Core appears in a connected window without a reconnect, with
  its time, device and plain text; the history survives a Core restart.
- Changing the interlock policy from a remote window takes effect on the Core at once
  and every window shows the new policy; the transmit refusal still happens on the Core.
- The power-cap alert appears in any connected window when the Core raises it.
- Diagnostics pages show the Core's counters, not the window's.

**Verification:**
```sh
cmake --build <build dir> --target tst_fault_log tst_tx_interlock_policy tst_remote_peripherals tst_station_accessory_state -j6
QT_QPA_PLATFORM=offscreen ctest --test-dir <build dir> -R '^(tst_fault_log|tst_tx_interlock_policy|tst_remote_peripherals|tst_station_accessory_state)$' --no-tests=error --output-on-failure
```

**Execution note (advisory):** opus. Requires Task 1.

- [ ] **Step 1:** Fault list, diagnostics and interlock command with tests.
- [ ] **Step 2:** Power-cap alert, tune memory and labels, pages, document; commit.

## Task 5: Accessory listeners only on the station network

**Requirements:** R-R3-47, R-R3-22.

**Files:**
- Modify: `src/core/SmartSdrApiListener.{h,cpp}` and `src/core/LanDiscovery.{h,cpp}`
  (bind to the station-facing interface: the one on the radio's subnet by default,
  overridable in `nereusd.conf`), `src/models/RadioModel.cpp` (the FlexRadio discovery
  beacon follows the 4O3A switch, not only its own setting),
  `src/core/daemon/DaemonConfig.{h,cpp}` and `packaging/nereusd.conf.sample`
- Test: `tests/tst_daemon_config.cpp`, a new `tests/tst_smartsdr_api_listener_bind.cpp`,
  `tests/tst_lan_discovery_regex.cpp`, `tests/tst_flex_radio_discovery_broadcaster.cpp`

**Acceptance:**
- On a Core with two networks, the 4992 listener, the accessory discovery sockets and
  the Core's station TCI server accept connections only on the station network by
  default; the override selects another interface; local desktop behaviour unchanged.
- With the 4O3A switch off, no FlexRadio discovery beacon is sent.

**Verification:**
```sh
cmake --build <build dir> --target tst_daemon_config tst_smartsdr_api_listener_bind tst_lan_discovery_regex tst_flex_radio_discovery_broadcaster -j6
QT_QPA_PLATFORM=offscreen ctest --test-dir <build dir> -R '^(tst_daemon_config|tst_smartsdr_api_listener_bind|tst_lan_discovery_regex|tst_flex_radio_discovery_broadcaster)$' --no-tests=error --output-on-failure
```

**Execution note (advisory):** opus. Independent of Tasks 2-4.

- [ ] **Step 1:** Station-interface binding, config key and beacon gate with tests;
  commit.

## Task 6: The amp's and the tuner's own settings from a remote window

Added 2026-09-24 after Task 4: a remote window's Power Genius and Tuner Genius
Advanced pages show what the Core keeps, but not the devices' own settings, which
need the Core to talk to the device. The operator's rule is remote parity: a control a
local window offers is never left disabled in a remote one.

**Requirements:** R-R3-47, R-R3-22.

**Files:**
- Modify: `src/core/StationPgxlController.{h,cpp}` and the Core's Tuner Genius
  controller (each device setting as a typed request the Core sends to the device, with
  the device's answer returned), `src/core/session/SessionCommandDispatcher.cpp`,
  `src/core/session/StationServer.cpp`, `src/core/session/StationClient.cpp`,
  `src/core/session/IStationLink.h` (one verb per setting group: the Power Genius's
  Hardware, Network, Pairing & Band Source, Save & Reboot and Revert; the Tuner Genius's
  Network, Save & Reboot and Revert; and any other control on the two Advanced pages a
  remote window cannot use yet), `src/core/session/StationCapabilities.{h,cpp}` (a
  raised control version for each device, so older windows see today's wire),
  `src/gui/setup/PgxlAdvancedPage.cpp` and `src/gui/setup/TgxlAdvancedPage.cpp` (in a
  remote window the controls send the verbs and show the Core's answer; in a local
  window unchanged), the control document
- Test: `tests/tst_station_pgxl_controller.cpp`, the Tuner Genius controller's test,
  `tests/tst_remote_peripherals.cpp`, a page test for each Advanced page in a remote
  window

**Acceptance:**
- In a remote window every control on the Power Genius and Tuner Genius Advanced pages
  works as it does in a local window: the request goes to the Core as a typed verb, the
  Core sends the device the same command the local page sends today (a fake device in
  the tests records the bytes), and the page shows the device's answer and its new
  values from the Core's status. A device refusal or a missing device reaches the
  window as plain words.
- Network changes and Save & Reboot ask the same confirmation a local window asks
  before anything is sent.
- None of these verbs keys the radio or puts the amp in operate; the receive-only
  Core's refusals (Task 2) are unchanged.
- A window whose Core does not offer the raised version keeps today's behaviour and
  says why a control is unavailable, in plain words.

**Verification:** the tests above built and run by exact name, offscreen. Hardware
(pending, operator checkpoint, and only with the operator's go-ahead because it changes
the devices' own settings): a name change and a Save & Reboot on the real Power Genius
and Tuner Genius from the Rock's remote window.

**Execution note (advisory):** opus. After Task 4. Touches settings that can take a
device off its network (Network, Save & Reboot): a candidate for an earlier independent
review; the operator decides.

- [ ] **Step 1:** Core requests and verbs for each device setting, with fake-device
  tests; commit.
- [ ] **Step 2:** The Advanced pages in a remote window, the version gate and the
  control document; commit.
