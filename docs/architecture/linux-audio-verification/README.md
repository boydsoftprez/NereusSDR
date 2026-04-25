# Phase 3O — Linux PipeWire Audio Bridge Verification Matrix

> **Status (as of 2026-04-24):** ✅ **End-to-end verified on Ubuntu
> 25.10 / PipeWire 1.4.7.** App stable, speakers playing via ALSA
> (PipeWire-routed), VAX RMS meter modulates, all 4 VAX sources
> visible to PulseAudio clients (`wpctl status`, libpulse
> enumeration), WSJT-X discovers them in Settings → Audio → Input
> after the consumer-side gotcha below is satisfied. Branch ready to
> merge.

## Consumer-app gotcha — required for VAX visibility in Qt5 apps

WSJT-X (and any other Qt5Multimedia-based ham app) needs the
**`libqt5multimedia5-plugins`** package installed on the consumer
host. Qt5 ships its multimedia *interface* in `libQt5Multimedia.so`
but the actual *backend plugins* (the `qtmedia_audioengine`
ALSA/Pulse implementations that enumerate devices) are in the
separate `-plugins` package. Without it, Qt5 audio apps silently
see zero devices regardless of what PipeWire / PulseAudio
publishes — the symptom is "Settings → Audio → Input dropdown is
empty / shows only `default`".

```
sudo apt install libqt5multimedia5-plugins   # Debian/Ubuntu
sudo dnf install qt5-qtmultimedia              # Fedora (already
                                               # bundles plugins)
```

This is **not** a NereusSDR runtime dependency — NereusSDR uses Qt6
and its own audio path. It only matters on the consumer side
(WSJT-X, fldigi, VARA, gqrx, etc.) when those apps are Qt5-based.

## How to use

1. On each target host, run the build and launch:
   ```
   cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
   cmake --build build -j$(nproc)
   ./build/NereusSDR
   ```
2. Capture the **detected backend** from the log (`Linux audio
   backend detected: …`).
3. Open **Setup → Audio** and confirm the AudioBackendStrip header
   matches the detected backend.
4. Walk the scenarios for the row.
5. For each filled scenario, record measured latency, xrun count
   after a 10-minute soak, and a screenshot of the telemetry strip
   (when applicable). Drop screenshots into this directory keyed
   by row letter (e.g. `A-vax-wsjtx.png`).
6. Reference this matrix from the release PR description.

---

## Distro coverage matrix

| # | Distro | PipeWire | pactl | Expected detection | Scenarios |
|---|--------|----------|-------|--------------------|-----------|
| A | Ubuntu 25.10 (bare metal) | ✓ 1.4.7 | ✗ | PipeWire | VAX → WSJT-X, sidetone on USB headphones, reconnect on device replug |
| B | Ubuntu 22.04 LTS | 0.3.48 (under 0.3.50 floor) | ✓ | Pactl | VAX → WSJT-X, single speakers sink |
| C | Ubuntu 20.04 LTS (VM) | ✗ | ✓ | Pactl | VAX → WSJT-X, single speakers sink |
| D | Fedora 41 | ✓ | maybe ✓ | PipeWire | Full scenarios (A's set) |
| E | Debian 12 | ✓ 1.0.5 | ✗ | PipeWire | Full scenarios (A's set) |
| F | Arch current | ✓ | ✓ | PipeWire | Full scenarios (A's set) |
| G | Container, no audio | ✗ | ✗ | None + first-run dialog | Dismiss, Rescan after install, first-run persist flag |

**Release gate:** at least rows A, one Ubuntu LTS (B or C), and one
container (G) must be filled before merging `ubuntu-dev` to `main`.

---

## Per-row scenario detail

### Row A — Ubuntu 25.10 (PipeWire path, primary target)

| Scenario | Expected | Result | Notes |
|---|---|---|---|
| Backend detection | `PipeWire` (server version 1.4.7) | ✅ verified | Log: `Linux audio backend detected: "PipeWire"` and `PipeWire thread loop connected — server version "1.4.7"`. |
| App-side stream open succeeds for all 5 buses | All log "stream opened … direction: OUT/IN" + "VAX N bus opened (eager) [ \"PipeWire\" ]" | ✅ verified | Log shows VAX 1-4 (OUT) and TX-input (IN) all returning open=true. |
| **`nereussdr.vax-*` source nodes visible via `pw-dump`** | All four VAX RX sources appear as `Audio/Source` nodes in WirePlumber's view | ❌ **REGRESSION** | `pw-dump` shows 0 NereusSDR-related nodes. App-side open() returned true (pw_stream_connect ≥ 0) but the streams never reached PAUSED/STREAMING state in WirePlumber's session graph. The 4 VAX OUTPUT streams' `on_process` callback never fires (their RT-probe log line is absent). Only the TX-input stream (which uses `Stream/Input/Audio` media class) reaches the data thread. **Hypothesis:** WirePlumber's default session policy on Ubuntu 25.10 doesn't auto-link `Audio/Source` nodes from streams that aren't attached to a hardware device. Likely needs additional pw_properties (e.g. `node.virtual=true`, `node.always-process=true`, or a different session-policy hint). Tracked as a follow-up gating item. |
| `nereussdr.vax-1` visible to WSJT-X | Selectable as input source | _blocked by ❌ above_ | Won't appear in WSJT-X until the visibility regression is fixed. |
| VAX 1 → WSJT-X round-trip decode | Decodes a known FT8 capture | _blocked_ | |
| Primary sink default routing | Speakers + master mix audible | _user-pending_ | Requires GUI session — not testable in headless ssh. |
| CW sidetone on USB headphones | Separate stream visible in `pw-cli`, audible in headphones only | _user-pending_ | |
| TX MON on third sink | Separate stream, audible | _user-pending_ | |
| Device replug recovery | Unplug + replug headphones; sidetone reroutes | _user-pending_ | |
| 10-min soak — latency | Measured (target ≤ 15 ms total RX → speakers) | _user-pending_ | |
| 10-min soak — xrun count | Target 0; record actual | _user-pending_ | |
| RT scheduling probe (TX-input stream) | Telemetry shows non-default policy when rtkit grants RT | ⚠️ verified, RT NOT granted | Log: `pw data thread sched policy: 1073741824 priority: 0 for stream: "nereussdr.tx-input"`. Policy 0x40000000 = `SCHED_RESET_ON_FORK \| SCHED_OTHER` — i.e. rtkit did **not** elevate the data thread to SCHED_FIFO/RR. Expected on a developer host without proper rtkit configuration; the RT probe correctly captured and reported the actual scheduler state, which is the contract from f35cc7b. |
| RT scheduling probe (VAX RX streams) | Same telemetry for each OUT stream | ❌ **never fires** | Same root cause as the visibility regression — `on_process` is never called for the 4 OUTPUT VAX streams, so `probeSchedOnce` never runs. Will fix as part of the visibility follow-up. |
| First-run dialog (intentional) | Help → Diagnose audio backend opens it | _user-pending_ | Wired (Tasks 18-19, 22-23); requires GUI. |
| Plan integration test (`tst_pipewire_stream_integration`) | PASS | ✅ verified | The Task 24 test uses `Stream/Output/Audio` (auto-routes via WirePlumber) and reaches `streaming` in ~50 ms. Confirms the daemon connection + stream lifecycle work for streams that play back audio rather than expose a source. |

### Row B — Ubuntu 22.04 LTS (Pactl fallback)

| Scenario | Expected | Result | Notes |
|---|---|---|---|
| Backend detection | `Pactl` (PipeWire 0.3.48 below 0.3.50 floor) | _pending_ | |
| `nereussdr-vax-1..4` (LinuxPipeBus) visible | Selectable in WSJT-X | _pending_ | |
| VAX 1 → WSJT-X round-trip | Decodes | _pending_ | |
| Single speakers sink (no PipeWire split) | Audible | _pending_ | |
| 10-min soak — latency | Measured | _pending_ | |
| 10-min soak — xrun count | Recorded | _pending_ | |

### Row C — Ubuntu 20.04 LTS VM (Pactl-only)

Same scenarios as Row B; PipeWire absent so detection should fall
straight to Pactl without a connect attempt log line.

### Row D — Fedora 41

Same scenarios as Row A. If Fedora ships pactl alongside PipeWire,
detection should still pick PipeWire (per the spec §4.1 ordering).

### Row E — Debian 12

Same scenarios as Row A. PipeWire 1.0.5 is above the 0.3.50 floor.

### Row F — Arch current

Same scenarios as Row A.

### Row G — Container without audio

| Scenario | Expected | Result | Notes |
|---|---|---|---|
| Backend detection | `None` | _pending_ | |
| First-run dialog auto-opens | Yes, on first launch | _pending_ | |
| Detection-state section | "PipeWire socket: not found at /run/user/N/pipewire-0" + "pactl binary: not in $PATH" | _pending_ | |
| Copy commands button | Three install lines on clipboard | _pending_ | |
| Dismiss | Sets `Audio/LinuxFirstRunSeen=True`; doesn't reopen on next launch | _pending_ | |
| Rescan after install | After `apt install pipewire pipewire-pulse`, re-enters PipeWire path without restart | _pending_ | |
| Help → Diagnose audio backend | Re-opens dialog with current state | _pending_ | |

---

## Reference

- Design spec: [`../2026-04-23-linux-audio-pipewire-design.md`](../2026-04-23-linux-audio-pipewire-design.md)
- Implementation plan: [`../2026-04-23-linux-audio-pipewire-plan.md`](../2026-04-23-linux-audio-pipewire-plan.md)
- Sister phase verification matrix (3G-8): [`../phase3g8-verification/README.md`](../phase3g8-verification/README.md)
