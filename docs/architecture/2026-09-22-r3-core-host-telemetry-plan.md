# R3 Core host telemetry implementation plan

> **Execution:** run with `crew` under `cost-aware-execution`. Requirements and
> acceptance cases are binding; one whole-branch review at the end of the batch.

**Goal:** the operator can see the Core computer's CPU, memory and temperature
history in Remote Network Diagnostics, and the two-hour audio soak can record
them, without any tool on the station computer.

**Architecture:** the Core's existing 1 Hz telemetry sampler gains a small
Linux host sampler (procfs and sysfs, no new dependency) and sends the values
in the existing telemetry snapshot as optional fields, gated by a new
telemetry capability version so older GUIs and Cores keep working. The GUI
stores them in the existing telemetry history and draws them on a new "Core"
tab with the existing graph widget. Measured audio latency (R-R3-35) is not
part of this plan; it waits for the operator's choice of end points.

**Tech stack:** C++20, Qt 6, `DaemonTelemetryController`, `StationTelemetry`,
`StationServer`/`StationClient`, `TelemetryHistory`, `TimeSeriesGraphWidget`,
`RemoteDiagnosticsDialog`, Qt Test.

**Spec:** [R3 plan](2026-09-20-remote-daemon-r3-plan.md) section 4e (lines
764-834; "Core CPU/memory history follows") and section 6's soak item (lines
1289-1291: counters for CPU, memory and thermal state); requirements R-R3-32,
R-R3-33.

## Global Constraints

- Work only in `/Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR`,
  branch `codex/integrate-r2-main`, build directory `build-integration`.
- Commits: GPG-signed with hooks (`NEREUS_THETIS_DIR=/Users/j.j.boyd/Thetis`),
  never `--no-gpg-sign` or `--no-verify`, no `Co-Authored-By`, no em-dash
  characters in commit messages, docs or new operator strings. Stage explicit
  paths only. Every commit names its R-R3 IDs.
- NereusSDR-original code. Keep existing headers; new files carry the house
  header plus `// no-port-check: NereusSDR-original. <reason>`.
- Telemetry stays observational: no field grants admission, changes radio
  state or substitutes for heartbeat evidence (`StationTelemetry.h:9-11`).
  An absent value means not measured; a present zero is a measurement.
- The sampler never blocks the Core's event loop for more than reading a few
  small procfs/sysfs files once per second, never allocates per sample beyond
  the snapshot, and never runs on a real-time audio or DSP thread.
- Protocol additions are gated by negotiated minor and capability version,
  following the remote audio status pattern; a GUI or Core without the new
  version sees exactly today's telemetry.
- Non-Linux Cores (the embedded Core in the desktop app on macOS or Windows)
  send the new fields absent; nothing fakes them.
- Tests run off-screen by default. Build exact targets, then
  `ctest --test-dir build-integration -R '^(<targets>)$' --no-tests=error --output-on-failure`.
  No unfiltered suite. Never build the `NereusSDR` target. Hardware is off
  limits to implementers.

## What already exists

- `StationTelemetrySnapshot` carries `sequence`, `sampledElapsedMs`, radio and
  audio sections, all optional values (`src/core/session/StationTelemetry.h:13-40`);
  its codec is `src/core/session/StationTelemetry.cpp`.
- `DaemonTelemetryController` samples at 1 Hz (`kSamplePeriodMs = 1000`,
  `src/core/daemon/DaemonTelemetryController.h:33`) with a test seam
  `sampleNow()` and `disableAutomaticSamplingForTest()` (:45-53).
- Negotiation: `kStationTelemetrySessionProtocolMinor = 3`
  (`src/core/session/SessionMessages.h:180`), `stationTelemetryVersion`
  (`src/core/session/StationCapabilities.h:119`); `StationServer::sendTelemetry`
  and `StationClient::telemetryReceived` carry the snapshot.
- GUI: `RemoteDiagnosticsDialog` builds tabs with `buildTab` and `addGraph`
  (`src/gui/RemoteDiagnosticsDialog.cpp:143-160`), history in
  `src/gui/TelemetryHistory.{h,cpp}`, graphs in `src/gui/TimeSeriesGraphWidget.h`.
- Tests: `tst_station_telemetry`, `tst_daemon_telemetry`, `tst_remote_telemetry`,
  `tst_remote_diagnostics`, `tst_telemetry_history`, `tst_station_session`.
- The display limits plan (2026-09-22) takes protocol minor 9. If it has
  landed, this plan takes the next free minor; check `SessionMessages.h`.

## Task 1: Sample and send Core host telemetry

**Requirements:** R-R3-32, R-R3-33.

**Files:**
- Create: `src/core/daemon/HostTelemetrySampler.{h,cpp}` (Linux procfs/sysfs
  reader with an injectable root directory for tests)
- Modify: `src/core/session/StationTelemetry.{h,cpp}` (a `StationHostTelemetry`
  section and its encode/decode for both shapes),
  `src/core/daemon/DaemonTelemetryController.{h,cpp}`,
  `src/core/session/SessionMessages.h`, `src/core/session/StationCapabilities.{h,cpp}`,
  `src/core/session/StationServer.cpp`, `src/core/session/StationClient.cpp`,
  `CMakeLists.txt` (register the new source)
- Test: new `tests/tst_host_telemetry_sampler.cpp` (fixture files under a
  temporary directory), `tests/tst_station_telemetry.cpp`,
  `tests/tst_daemon_telemetry.cpp`, `tests/tst_station_session.cpp`

**Interfaces:**
- Consumes: nothing new.
- Produces:
  ```cpp
  struct StationHostTelemetry {
      std::optional<double> systemCpuPercent;   // all CPUs, 0-100
      std::optional<double> processCpuPercent;  // nereusd share of all CPUs, 0-100
      std::optional<qint64> memoryAvailableKiB; // MemAvailable
      std::optional<qint64> memoryTotalKiB;     // MemTotal
      std::optional<qint64> processResidentKiB; // VmRSS
      std::optional<double> hottestZoneCelsius;
      QString hottestZoneName;                  // empty when absent
  };
  ```
  as `StationTelemetrySnapshot::host`, and capability version
  `stationTelemetryVersion = 2`.

**Acceptance:**
- CPU percentages come from differences between two samples of `/proc/stat`
  and `/proc/self/stat` (absent on the first sample and after a reading
  error); memory from `/proc/meminfo` and `/proc/self/status`; temperature
  from every `/sys/class/thermal/thermal_zone*/temp` (millidegrees) with the
  hottest zone's `type` as its name. A missing or unreadable file makes only
  that value absent.
- Fixture tests cover two-sample CPU arithmetic, counter wrap or reset
  (absent, not negative), missing files, and a multi-zone temperature pick.
- A version 2 peer receives the host section; an older peer receives exactly
  today's telemetry (golden comparison); both decode directions work.
- On macOS and Windows the host section is absent and nothing is logged per
  sample.

**Verification:** fixture-driven unit tests plus the session negotiation
tests.
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_host_telemetry_sampler tst_station_telemetry tst_daemon_telemetry tst_station_session nereusd -j6
ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^(tst_host_telemetry_sampler|tst_station_telemetry|tst_daemon_telemetry|tst_station_session)$' --no-tests=error --output-on-failure
```
Hardware (pending, controller): on the Rock, the values match `top` and
`/sys/class/thermal` within one sample.

**Execution note (advisory):** opus.

- [ ] **Step 1:** Sampler with fixture tests.
- [ ] **Step 2:** Snapshot section, negotiation and Core wiring; run the
  commands; commit.

## Task 2: Show the Core's history in Remote Network Diagnostics

**Requirements:** R-R3-32, R-R3-33.

**Files:**
- Modify: `src/gui/RemoteDiagnosticsDialog.{h,cpp}` (a "Core" tab),
  `src/gui/TelemetryHistory.{h,cpp}` (store the host values),
  `src/gui/RemoteTelemetryController.cpp` (the periodic diagnostics log line)
- Test: `tests/tst_remote_diagnostics.cpp`, `tests/tst_telemetry_history.cpp`,
  `tests/tst_remote_telemetry.cpp`

**Interfaces:**
- Consumes: Task 1's `StationHostTelemetry`.
- Produces: no new API.

**Acceptance:**
- A "Core" tab shows three graphs with the existing widget and history
  range: "Core computer CPU" (system and NereusSDR Core as two series, in %),
  "Core computer memory" (available and used by NereusSDR Core, in MiB or GiB
  chosen like the traffic graphs choose units), and "Core computer
  temperature" (hottest sensor in degrees C, sensor name in the tooltip).
- When the Core does not send host values (older Core, or not Linux), the tab
  shows one plain line: "This Core does not report computer load." and no
  empty graphs.
- A periodic GUI diagnostics log line exists for the soak: one `qCInfo` line
  every 60 s (named constant) from `RemoteTelemetryController::sampleNow()`,
  carrying the remote audio receiver counters (including `driftRatio` and
  `startDiscardedPackets`) and the latest Core host values. No such line
  existed before this plan (the media establishment plan's Task 4 assumed one);
  this task creates it. Absent values are printed as "not measured".

**Verification:** GUI functional checks off-screen; the native look is the
operator's checkpoint.
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_remote_diagnostics tst_telemetry_history tst_remote_telemetry -j6
ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^(tst_remote_diagnostics|tst_telemetry_history|tst_remote_telemetry)$' --no-tests=error --output-on-failure
```
Hardware (pending, operator session S1): the tab on the real GUI against the
Rock.

**Execution note (advisory):** opus. Requires Task 1.

- [ ] **Step 1:** History and tab with tests; log line; run the commands;
  commit.
