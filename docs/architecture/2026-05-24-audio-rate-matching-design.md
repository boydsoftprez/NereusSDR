# Audio rate matching: source-first port of Thetis `rmatch` for speakers + VAX

**Date:** 2026-05-24
**Author:** J.J. Boyd (KG4VCF), with AI-assisted analysis via Anthropic Claude Code
**Status:** Design + plan, pre-implementation. Revised 2026-05-24 after Codex second-opinion review.
**Thetis baseline:** v2.10.3.15 @ 3759d096
**WDSP baseline:** TAPR v1.29 (vendored at `third_party/wdsp/`)

---

## 1. Problem statement (simplest terms)

NereusSDR has three correlated symptoms reported during ANAN-G2 bench testing on macOS:

1. **Display ahead of audio.** The spectrum, waterfall, and S-meter are consistently a noticeable amount ahead of what you hear. The gap varies over time.
2. **Audio replays multiple times.** During intermittent lag spikes the audio momentarily stutters or appears to replay a short chunk.
3. **Spectrum / waterfall lag.** The display occasionally jerks or stalls briefly.

All three trace to a single root cause: the radio's sample clock and the host audio device's sample clock are independent, drift past each other constantly, and we have no mechanism to track that drift. NereusSDR's current speakers ring is hardcoded at `48000 * 2` floats = exactly 1 second of stereo audio ([PortAudioBus.cpp:209-215](../../src/core/audio/PortAudioBus.cpp)) with no overrun protection, no rate matching, and no latency target. It absorbs drift silently until it overflows, at which point fresh samples stomp on unread ones in the modulo-wrapping ring and the listener hears scrambled audio.

The same root cause applies to VAX (NereusSDR's virtual audio cable subsystem for digital-mode 3rd-party apps): the 3rd-party app's audio clock also drifts past WDSP's clock. Today VAX inherits the same kind of unbuffered overrun behavior.

## 2. How Thetis solves this

Thetis uses an adaptive rate-matching resampler called `rmatchV` that lives inside WDSP. It watches its internal ring fill level over time and continuously adjusts a variable-rate resampler so the ring stays near its midpoint.

Key reference files (Thetis v2.10.3.15):

- `../Thetis/Project Files/Source/ChannelMaster/ivac.c` — Thetis's complete VAC bridge. Each IVAC instance owns two `rmatch` handles (one per direction) plus an `aamix` mixer. Lines 33-53 (`create_resamps`) compute ring size as `2 * rate * latency_seconds`. Lines 196-263 (`CallbackIVAC`) drive the resampler from the PortAudio callback. The mono-to-stereo upconvert pattern at lines 211-252 is the model for VAX TX with mono mic sources.
- `../Thetis/Project Files/Source/ChannelMaster/ivac.h` — public API.
- `../Thetis/Project Files/Source/Console/audio.cs:1645,1725,1835` — three audio entry points: `StartAudioIVAC(0)` for VAC1, `StartAudioIVAC(1)` for VAC2, `NetworkIO.StartAudioNative()` for the speakers. The speakers path leads into compiled `ChannelMaster.dll` (closed-source binary, not in the Thetis repo). We cannot byte-for-byte port Thetis's speakers handling. We can only port what Thetis does for VAC, then apply the same algorithm to our speakers path.

The rmatch algorithm itself is in WDSP source:

- `third_party/wdsp/src/rmatch.c` (736 lines) — implementation.
- `third_party/wdsp/src/rmatch.h` (159 lines) — public API.
- `third_party/wdsp/src/varsamp.c` (251 lines) — variable-rate sampler used internally by rmatch.

Both files are vendored in NereusSDR but neither is currently used. The WDSP CMake glob `src/*.c` should pull them into the static archive automatically; this must be verified with `nm` before any wrapper code is written.

## 3. What changes vs Thetis

| Thetis behavior | NereusSDR equivalent | Why different |
|---|---|---|
| Two separate IVAC instances (VAC1 + VAC2), two full Setup UI panels | One rate-matcher per output bus + one for VAX TX; no per-cable UI duplication | NereusSDR's VAX is one subsystem with 4 RX channels and 1 TX endpoint, not two independent cables. UI parity with VAC1/VAC2 split is explicitly out of scope. |
| User-tunable ring latency 0-240 ms, default 120 ms | Default = backend's lowest stable latency, clamped to internal sane floor. Existing `DeviceCard` buffer-size control stays as advanced per-device override. | Decision F8 (2026-05-24): default to lowest, expose existing knob as advanced override. |
| Windows-only PortAudio + ASIO | Cross-platform: PortAudio (speakers, all platforms) + CoreAudioHal (macOS VAX) + LinuxPipeBus + PipeWireBus (Linux VAX) | NereusSDR is a cross-platform port. |
| `aamix` mixer inside IVAC switching between RX audio and TX monitor based on MOX/MON | VAX taps RX path before MasterMixer; always feeds RX audio regardless of MOX state | Decision F6 (2026-05-24): drop Thetis VAX TX-monitor parity. 3rd-party apps see RX audio at all times, even during MOX. Documented divergence. |
| Single PortAudio callback per IVAC drives `xrmatchOUT` | Per-backend driver: PortAudio audio callback for speakers; PipeWire `onProcess` callback for PipeWire-native VAX; small wallclock timer threads for CoreAudioHal and LinuxPipeBus | Three of the four backends do not have a callback we own; PipeWire does. |

## 4. Adversarial review history

This design absorbed two rounds of adversarial review. The remaining text reflects findings already incorporated; this section is the audit trail.

**Round 1 (internal Claude agent, 2026-05-24):** 10 findings, of which 4 (rmatch is callback-driven; no length arg on `xrmatchIN/OUT`; mono mics need upconvert; HAL plugin shm ring is fixed at 2 s) caused the initial "universal RmatchAudioBus decorator" plan to be re-shaped into per-backend integration.

**Round 2 (Codex second-opinion review, 2026-05-24):** 10 findings, of which 7 are accepted and incorporated into this revision:

- **F1 (wallclock drives wrong clock for VAX, partial fix):** acknowledged as known limitation. Wallclock-driven rmatch on CoreAudioHal / LinuxPipeBus removes the radio-vs-host drift (the larger, ppm-scale drift tied to the radio crystal) but cannot close the loop on host-vs-consumer drift without downstream ring-fill feedback. Closing that loop is a follow-up epic.
- **F2 (PipeWire is callback-driven):** accepted. PipeWire-native VAX uses `PipeWireStream::onProcessOutput/Input` directly; no timer thread.
- **F3 (one TX endpoint not four):** accepted. Verified at [AudioEngine.cpp:520-540](../../src/core/AudioEngine.cpp). Total rmatch instances = 1 speakers + 4 VAX RX + 1 VAX TX = 6.
- **F4 (silence pre-fill is harmful):** accepted. Verified at [rmatch.c:139-140](../../third_party/wdsp/src/rmatch.c). `calc_rmatch` already initializes `n_ring = rsize/2`, `iin = rsize/2`, `iout = 0`. Pre-filling pushes the ring off its intended midpoint. Pre-fill step deleted from plan.
- **F5 (paFloat64 is not a local switch):** accepted. Verified at [PortAudioBus.cpp:381-389](../../src/core/audio/PortAudioBus.cpp). The callback hard-casts to `float*` and the ring is `std::vector<float>`. Stay at `paFloat32`; do float↔double conversion inside the `RateMatcher` wrapper.
- **F6 (MasterMixer does not replace aamix for VAX TX monitor):** product decision (2026-05-24): drop Thetis parity. VAX always feeds RX audio regardless of MOX.
- **F7 (VAX TX scheduler conflicts with TxWorkerThread):** accepted. Put rmatch inside `pullVaxTxMic()` (or behind a new `TxMicSource` abstraction that the existing pump calls), not as a parallel consumer.
- **F8 (no-knob is wrong, existing DeviceCard control already exposes it):** product decision (2026-05-24): default to lowest stable latency; keep existing buffer-size control as advanced per-device override. Default value computed from the backend's lowest-stable hint; user can override if needed.
- **F9 (setter sleep claim overstated):** accepted. Classify rmatch setters by sleep behavior. `setRMatchFeedbackGain` and `forceRMatchVar` do not sleep; use them on the live handle for non-structural tweaks. Only destroy/recreate for structural changes (rate, ringsize, in/out size).
- **F10 (provenance / attribution):** accepted. New §13 added to this doc enumerating what each new file inherits from Thetis vs WDSP vs original glue, with header requirements.

## 5. Mission (revised)

Make the audio you hear from the radio stay in lockstep with the spectrum, waterfall, and S-meter at the lowest latency the platform can stably deliver, on the speakers path AND every VAX channel (4 RX + 1 TX), with no NEW user-facing controls but preserving the existing advanced per-device buffer-size override.

## 6. UI surfaces audit

Full sweep of files under `src/gui/` that touch audio:

| Surface | File | What it does today | Change |
|---|---|---|---|
| Setup → Audio → Devices | [src/gui/setup/AudioDevicesPage.h](../../src/gui/setup/AudioDevicesPage.h) | Pick speakers device, mic device | None |
| Setup → Audio → VAX | [src/gui/setup/AudioVaxPage.cpp](../../src/gui/setup/AudioVaxPage.cpp) | Enable channels 1-4, transport selection | None to controls. Optionally add read-only "Effective latency: X ms" diagnostic row. |
| Setup → Audio → TX Input | [src/gui/setup/AudioTxInputPage.h](../../src/gui/setup/AudioTxInputPage.h) | Mic device, gain | None |
| Setup → Audio → TCI | [src/gui/setup/AudioTciPage.h](../../src/gui/setup/AudioTciPage.h) | TCI audio routing | None |
| Setup → Audio → Advanced | [src/gui/setup/AudioAdvancedPage.cpp](../../src/gui/setup/AudioAdvancedPage.cpp) | (mostly placeholder today) | Optional read-only diagnostics panel: per-output underflow / overflow count + current variance estimate. Pure observability. |
| Setup → Audio → Backend strip | [src/gui/setup/AudioBackendStrip.h](../../src/gui/setup/AudioBackendStrip.h) | Pick PortAudio / Native CoreAudio / PipeWire per VAX channel | None |
| Setup → Audio → DeviceCard | [src/gui/setup/DeviceCard.cpp:260-276](../../src/gui/setup/DeviceCard.cpp) | Per-device buffer-size control | **Stays.** Default value now reads from backend's lowest-stable hint when the user has not set a value. User override still wins. |
| VAX first-run dialog | [src/gui/VaxFirstRunDialog.h](../../src/gui/VaxFirstRunDialog.h) | macOS HAL plugin install prompt | None |
| VAX Linux first-run dialog | [src/gui/VaxLinuxFirstRunDialog.cpp](../../src/gui/VaxLinuxFirstRunDialog.cpp) | pactl / pipewire setup prompt | None |
| VaxApplet (Container #0) | [src/gui/applets/VaxApplet.cpp](../../src/gui/applets/VaxApplet.cpp) | Per-channel RX gain / mute / TX gain / level meters | None to controls. Optional tiny corner indicator turning yellow when that channel's rmatch is hitting sustained overruns. |
| MasterOutputWidget | [src/gui/widgets/MasterOutputWidget.h](../../src/gui/widgets/MasterOutputWidget.h) | Master volume + mute | None |
| VfoWidget | [src/gui/widgets/VfoWidget.h](../../src/gui/widgets/VfoWidget.h) | Per-slice VAX channel selector | None |
| DigitalApplet / PhoneCwApplet / CatApplet / TxApplet | various | References VAX for TX routing | None |
| TitleBar | [src/gui/TitleBar.cpp](../../src/gui/TitleBar.cpp) | Status badges | Optional sustained-overflow badge. Deferrable. |
| SpectrumOverlayPanel | [src/gui/SpectrumOverlayPanel.cpp](../../src/gui/SpectrumOverlayPanel.cpp) | Audio level indicators | None |

**Net UI impact:** zero new user-facing controls. DeviceCard buffer-size control behavior subtly changes (default value now driven by lowest-stable hint instead of a hardcoded constant). Up to two new read-only diagnostic surfaces, both optional and deferrable.

## 7. Architecture

### 7.1 Where the rate matcher sits

```
                    +---------------------------------------+
WDSP output     --> | RateMatcher (rmatch.c wrapper)        | --> bus transport
(radio clock)       | - per-direction, per-bus              |     (device clock OR
                    | - calc_rmatch native ring init        |      wallclock-driven OR
                    | - watches ring fill, adjusts var      |      PipeWire onProcess)
                    +---------------------------------------+
```

One `RateMatcher` instance per (bus, direction). Total instances when fully active:
- Speakers: 1 (RX direction only)
- VAX RX: 4 (one per channel)
- VAX TX: 1 (single TxInput endpoint, not per-channel)
- **Total: 6**

### 7.2 Pull / push driver per transport

| Bus | RX driver | TX driver |
|---|---|---|
| `PortAudioBus` (speakers, all platforms) | PortAudio audio callback | n/a |
| `PortAudioBus` (mic input, where used) | n/a | PortAudio audio callback |
| `CoreAudioHalBus` (macOS VAX RX, channels 1-4) | Wallclock timer thread, one per active channel | n/a |
| `CoreAudioHalBus` (macOS VAX TX) | n/a | Pulled by `TxWorkerThread::pullVaxTxMic()` via new `TxMicSource` abstraction; rmatch lives inside the source, not as a parallel consumer |
| `LinuxPipeBus` (Linux VAX RX, channels 1-4, pactl-pipe backend) | Wallclock timer thread, one per active channel | n/a |
| `LinuxPipeBus` (Linux VAX TX, pactl-pipe backend) | n/a | Same TxMicSource pattern as CoreAudioHal TX |
| `PipeWireBus` (Linux VAX RX, channels 1-4, native PipeWire backend) | `PipeWireStream::onProcessOutput` callback (real-time, no extra thread) | n/a |
| `PipeWireBus` (Linux VAX TX, native PipeWire backend) | n/a | `PipeWireStream::onProcessInput` callback drives rmatch directly; result fed to TxWorkerThread via the same TxMicSource abstraction |

Wallclock timer threads use `std::chrono::steady_clock` + `std::this_thread::sleep_until` at the nominal block interval (block_size_frames / sample_rate). Each thread is idle ~99% of the time. Maximum 4 timer threads active simultaneously (when all 4 CoreAudioHal or LinuxPipeBus RX channels are enabled).

### 7.3 Acknowledged partial fix on VAX (Codex F1)

Wallclock-driven rmatch removes the radio-vs-host drift (typically the larger ppm-scale drift tied to the radio crystal). It does NOT close the loop on host-vs-consumer drift between our shm/FIFO write rate and the 3rd-party app's read rate. Closing that loop requires downstream feedback (HAL plugin reporting shm read position, FIFO drain rate measurement, etc.) and is deferred to a follow-up epic.

For PipeWire-native, the `onProcess` callback IS driven by the consumer's clock, so PipeWire VAX gets the full fix.

The bench tests in §8 should distinguish "PipeWire fixed" from "CoreAudioHal/LinuxPipeBus partial fix" when reporting results.

### 7.4 Thread shutdown contract

Timer threads must terminate cleanly on disconnect, app quit, and on per-channel disable. Each thread checks a `std::atomic<bool> m_running` flag inside its sleep loop. `AudioEngine::stop()` clears the flag and `join`s each thread. The `RateMatcher` destructor runs after the timer thread join completes, so `xrmatchIN/OUT` are guaranteed not to race with `destroy_rmatchV`.

PipeWire `onProcess` callbacks run on the daemon's RT thread; lifetime is bounded by the `pw_stream` lifetime which is owned by `PipeWireStream`. RateMatcher destruction happens in `PipeWireStream`'s destructor under the stream's own lock.

### 7.5 Disabled VAX channels

When a VAX channel is disabled in Setup → Audio → VAX, no timer thread (or `RateMatcher`) is created for it. Enabling a channel later constructs both; disabling joins the thread and destroys the matcher. This adds lifecycle complexity but saves up to 4 idle threads when no VAX is in use.

### 7.6 Float to double conversion

`IAudioBus::push` takes `const char*` (float32 bytes today). `rmatch` requires `double*`. The conversion happens inside the `RateMatcher` wrapper, once per push and once per pull. Cost is one pass over the block. At the worst case (5 outputs simultaneously active at 1024-frame blocks), this is roughly 5 × 1024 × 2 × 2 = ~20k float conversions per WDSP block, well under 1% of CPU. The conversion runs on whichever thread called push/pull, which is either the DSP thread (push) or the audio/timer thread (pull); never the GUI thread.

## 8. Implementation plan

Each step lands as a single GPG-signed commit. Order matters: each step depends only on the steps above it.

1. **Pre-flight: verify rmatch is in the linked WDSP archive.** Run `nm` on macOS, Linux, Windows builds for symbols `create_rmatchV`, `xrmatchIN`, `xrmatchOUT`, `destroy_rmatchV`, `getRMatchDiags`. If absent, add `rmatch.c` + `varsamp.c` explicitly to `third_party/wdsp/CMakeLists.txt`. No code changes if symbols already present.

2. **Fix `_aligned_free` macro trailing semicolon** in `third_party/wdsp/linux_port.h`. Wrap in `do { } while(0)`. Drive-by; ship regardless of rest of epic.

3. **Build the `RateMatcher` C++ wrapper class.** Owns one `create_rmatchV` handle. Constructor takes (in_size, out_size, nom_inrate, nom_outrate, ringsize, initial_var). **No silence pre-fill** (Codex F4: `calc_rmatch` already initializes the ring at midpoint). Exposes `push(const float* in)` (float→double, then `xrmatchIN`), `pull(float* out)` (`xrmatchOUT`, then double→float), `diags()` returning underflow / overflow / variance / ringsize / nring. Setter helpers classified by sleep behavior (Codex F9): light setters (`setRMatchFeedbackGain`, `forceRMatchVar`) callable on live handle; structural setters (rate, ringsize, in/out size) require destroy/recreate. Unit-tested with synthetic input streams against known nominal rates and known drift, on a non-UI thread to avoid the rmatch internal sleep on reconfig.

4. **Force `framesPerBuffer` to match `rmatch.outsize`** in `Pa_OpenStream` for the speakers stream. The 256-sample default in `PortAudioBus::open()` becomes whatever the speakers rmatch was constructed with.

5. **Integrate `RateMatcher` into speakers `PortAudioBus`.** Stay at `paFloat32` (Codex F5: switching to paFloat64 corrupts the existing callback contract). `push(const char* bytes, qint64)` feeds the rmatch's `push`. The PortAudio callback's output-mode branch calls `RateMatcher::pull` instead of reading the old `48000*2` ring directly. The old ring shrinks or is removed; rmatch's internal ring takes over.

6. **Default DeviceCard buffer-size control to the lowest-stable hint** (Decision F8). When the user has not explicitly set a buffer size for a device, the default value comes from `Pa_GetDeviceInfo(device)->defaultLowOutputLatency` (PortAudio) or the platform equivalent. User-set values still win.

7. **Build `BusPullScheduler`** for wallclock-driven RX transports (CoreAudioHal, LinuxPipeBus). A `QThread`-subclass or `std::thread` wrapper that wakes at `block_size_frames / sample_rate` intervals using `std::this_thread::sleep_until`, calls `RateMatcher::pull`, hands the block to `bus->push()`. One scheduler per active VAX RX channel. Clean shutdown via `std::atomic<bool> m_running` + `join`.

8. **Integrate `RateMatcher` into `PipeWireStream::onProcessOutput`** for PipeWire-native VAX RX (Codex F2). The PipeWire callback IS the puller; no timer thread for this backend.

9. **Build `TxMicSource` abstraction for VAX TX** (Codex F7). The existing `TxWorkerThread::pullVaxTxMic` already pulls from the VAX bus; the new abstraction lets us slot rmatch in without adding a parallel consumer. One `RateMatcher` instance for the single VAX TX endpoint. Mono-to-stereo upconvert before `rmatch.push` (Round-1 finding F4, verified pattern in ivac.c:211-252).

10. **Wire schedulers and sources into `AudioEngine::start()` and `AudioEngine::stop()`** per the lifecycle contract in §7.4. Tie scheduler create/destroy to `setVaxEnabled(channel, bool)` per §7.5.

11. **Surface diagnostics.** Add `getRMatchDiags`-derived stats to `AudioEngine`. Log per-output underflow / overflow / variance on disconnect at `qCInfo` level. Optional debug-level periodic log (every 30 s) during a session. Optional read-only Setup → Audio → Advanced diagnostics panel.

12. **Bench cycle 1: speakers only.** Connect to ANAN-G2, listen for the display-vs-audio gap (expect it to drop to under 30 ms). Listen for clean RX over a long session (>30 min). Trigger a CPU stall (open a heavy menu, drag a window quickly) and confirm the audio recovers cleanly. Watch `getRMatchDiags` for sustained overflow / underflow indicating tuning issues.

13. **Bench cycle 2: VAX RX on macOS (CoreAudioHal).** Enable VAX channel 1, route RX audio to WSJT-X. Run for >30 min. Acknowledge expected partial fix (§7.3): drift between our shm-write cadence and WSJT-X's read cadence remains.

14. **Bench cycle 3: VAX TX on macOS.** Route WSJT-X TX through VAX. Send a series of WSPR / FT8 transmissions. Verify the radio's TX timing matches WSJT-X's expectations (decodes from monitoring receivers land on expected slots).

15. **Bench cycle 4: VAX on Linux PipeWire-native.** Same as cycles 2-3 but on Linux with PipeWire backend. This is the only VAX path that gets the FULL fix (real consumer callback); compare against macOS results to validate the partial-fix framing.

16. **Bench cycle 5: VAX on Linux pactl-pipe.** Compare to PipeWire-native. Expect partial-fix behavior similar to macOS CoreAudioHal.

17. **Update CHANGELOG.md** with the new behavior. Document the macOS VAX partial-fix caveat and the connect-time startup behavior.

## 9. Risk register

| Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|
| rmatch.c not in the archive | Low | Medium | Step 1 pre-flight verifies. |
| 3-second startup glitch audible | Medium | Medium | Bench cycle 1 measures. Mitigation if needed: fork rmatch.c to shorten `startup_delay`. No ring-state tricks (Codex F4). |
| Timer thread jitter on macOS / Linux under load | Medium | Medium | `sleep_until` is sub-ms accurate on modern macOS / Linux. If insufficient, switch to a dedicated real-time-priority audio thread. |
| Mono mic upconvert missed in VAX TX path | Low | High | Step 9 explicitly covers; unit test in step 3. |
| Per-channel enable / disable lifecycle bugs | Medium | Medium | Step 10 + clean shutdown contract in §7.4. Test enabling / disabling under load. |
| HAL plugin 2-second shm ring caps macOS VAX latency floor | High (existing) | Low (out of scope) | Documented as known ceiling. Separate follow-up epic. |
| Pre-existing `_aligned_free` macro bug bites in step 1 | Low | Low | Step 2 fixes it preemptively. |
| Codex F1 partial-fix framing turns out to be unacceptable on macOS VAX bench | Medium | Medium | Documented as known limitation. Closing the loop requires HAL plugin shm read-position feedback; separate epic. |
| User overrides DeviceCard buffer to a value rmatch cannot handle | Low | Medium | Clamp user input to a sane range. Document the new "default = lowest-stable hint" behavior in tooltips. |
| TxMicSource refactor breaks existing TX pump | Medium | High | Step 9 includes a test pass on TUNE + SSB TX with VAX disabled to confirm baseline TX behavior is preserved. |

## 10. Open decision points

None remaining. F6 and F8 decisions captured in §3 and §4.

## 11. Out of scope

- Shrinking the macOS HAL plugin shm ring (separate epic, requires `.driver` rebuild + re-sign + notarization).
- Replacing pactl-pipe with PipeWire-native on Linux as the default backend (separate scope decision).
- Closing the host-vs-consumer drift loop on CoreAudioHal and pactl-pipe (Codex F1 follow-up).
- Porting Thetis's `aamix` mixer and the IVAC TX-monitor-during-MOX switching (Codex F6, decision: drop parity).
- VAC1 / VAC2 dual-instance UI (NereusSDR uses unified VAX subsystem).

## 12. Acceptance criteria

- Display-vs-audio gap on speakers path drops from ~1000 ms worst-case to <50 ms steady-state on ANAN-G2.
- No audible "audio replays" symptom during normal operation or CPU stalls.
- VAX RX/TX on **PipeWire-native** with WSJT-X runs for >1 hour with no WSJT-X clock drift warnings (full-fix path).
- VAX RX/TX on **macOS CoreAudioHal** runs for >1 hour with improved (but not eliminated) drift vs the pre-rmatch baseline (partial-fix path).
- `getRMatchDiags` shows variance settling within 30 seconds of connect.
- Zero new user-facing controls in Setup pages. Existing DeviceCard buffer-size control behavior preserved (default value now driven by lowest-stable hint).
- All existing audio + VAX tests still pass.
- TUNE + SSB TX work identically with VAX disabled (TxMicSource refactor regression gate).
- Clean shutdown: no leaked threads on disconnect or quit.

## 13. Provenance / attribution requirements (Codex F10)

Every new C++ file or modified file in this epic must classify its content per the CLAUDE.md source-first protocol. Three categories:

**A. NereusSDR-original glue (no upstream attribution required, but `// no-port-check:` marker needed for the verifier).**
- `RateMatcher` C++ wrapper class (header + impl). It calls rmatch's public C API but contains no translated logic.
- `BusPullScheduler` timer-thread class.
- `TxMicSource` abstraction.
- `AudioEngine` integration glue.

**B. Thetis-derived logic (full ivac.c byte-for-byte header copy + `// From Thetis ChannelMaster/ivac.c:NN [v2.10.3.15]` inline cites per ported line).**
- The mono-to-stereo upconvert in `TxMicSource` (translates from ivac.c:211-252).
- The ring-size formula `2 * rate * latency_seconds` (translates from ivac.c:35-36).
- Any other place where ivac.c logic is translated into C++.

**C. WDSP wrapper (links against vendored WDSP source; no separate attribution beyond WDSP's existing TAPR/GPLv2 header).**
- Direct calls to `create_rmatchV`, `xrmatchIN`, `xrmatchOUT`, `destroy_rmatchV`, `getRMatchDiags` from `RateMatcher`.
- No translation; only C-API consumption.

**Verification:** `scripts/verify-thetis-headers.py`, `scripts/check-new-ports.py`, and `scripts/verify-inline-tag-preservation.py` run in pre-commit. The PR will not land if any Category B file lacks the byte-for-byte ivac.c header copy.

**Filename convention:** new files use NereusSDR-original names (`RateMatcher.h`, `BusPullScheduler.h`, `TxMicSource.h`). The class names do not include "IVAC" or "rmatch" in their public API; those are implementation details.

---

## Appendix A: Key code references

- [src/core/audio/PortAudioBus.cpp:209-215](../../src/core/audio/PortAudioBus.cpp) — current 1-second hardcoded ring (the bug surface).
- [src/core/audio/PortAudioBus.cpp:305-321](../../src/core/audio/PortAudioBus.cpp) — current push, no overrun protection.
- [src/core/audio/PortAudioBus.cpp:372-415](../../src/core/audio/PortAudioBus.cpp) — paCallback, reads `r < w` then silence.
- [src/core/audio/PortAudioBus.cpp:380-389](../../src/core/audio/PortAudioBus.cpp) — Codex F5 evidence: hard-cast to `float*`, ring is `std::vector<float>`.
- [src/core/AudioEngine.cpp:520-540](../../src/core/AudioEngine.cpp) — Codex F3 evidence: singular `makeVaxTxBus()`, one TxInput endpoint.
- [src/core/AudioEngine.cpp:928-1097](../../src/core/AudioEngine.cpp) — `rxBlockReady`, the hot path that pushes to all output buses.
- [src/core/AudioEngine.cpp:1292-1297](../../src/core/AudioEngine.cpp) — `setMasterMuted` flush path (regression-test target).
- [src/core/IAudioBus.h](../../src/core/IAudioBus.h) — bus interface (no changes proposed).
- [src/core/audio/CoreAudioHalBus.h:69](../../src/core/audio/CoreAudioHalBus.h) — macOS HAL shm contract (out-of-scope ceiling).
- [src/core/audio/PipeWireStream.cpp:150-158, 305-312, 349-384, 396-434](../../src/core/audio/PipeWireStream.cpp) — Codex F2 evidence: PipeWire `onProcessOutput/Input` callbacks.
- [src/core/TxWorkerThread.cpp:341-376, 516-519, 748-758](../../src/core/TxWorkerThread.cpp) — Codex F7 evidence: TX pump architecture.
- [src/gui/setup/DeviceCard.cpp:260-276](../../src/gui/setup/DeviceCard.cpp) — Codex F8 evidence: existing buffer-size control (stays).
- [third_party/wdsp/src/rmatch.h:117-139](../../third_party/wdsp/src/rmatch.h) — rmatch public API.
- [third_party/wdsp/src/rmatch.c:130-172](../../third_party/wdsp/src/rmatch.c) — `calc_rmatch`. Codex F4 evidence: line 139 `n_ring = rsize/2` (native half-full init).
- [third_party/wdsp/src/rmatch.c:514](../../third_party/wdsp/src/rmatch.c) — hardcoded 3-second `startup_delay`.
- [third_party/wdsp/src/rmatch.c:537-545, 596-604, 690-696, 490-498](../../third_party/wdsp/src/rmatch.c) — Codex F9 evidence: setter sleep classification.
- [third_party/wdsp/linux_port.h](../../third_party/wdsp/linux_port.h) — `_aligned_free` macro bug location.
- `../Thetis/Project Files/Source/ChannelMaster/ivac.c:33-263` [v2.10.3.15] — Thetis reference usage.
- `../Thetis/Project Files/Source/ChannelMaster/ivac.c:145-168, 598-667` [v2.10.3.15] — Codex F6 evidence: `xvacOUT` + `aamix` mixer dispatching RX audio vs TX monitor on MOX.
- `../Thetis/Project Files/Source/Console/audio.cs:865-907, 1596-1599` [v2.10.3.15] — Codex F8 evidence: Thetis exposes latency knobs.

## Appendix B: Why the original "universal RmatchAudioBus decorator" plan was wrong

The first-pass plan proposed a single `RmatchAudioBus : IAudioBus` decorator that wraps any inner bus and rate-matches uniformly. Round-1 adversarial review (Finding F1) demonstrated this is structurally impossible: 3 of 4 backends have no audio callback we own (consumers live in other processes). The revised plan keeps rmatch as a per-bus component but uses different drivers per transport: PortAudio's own callback for speakers, PipeWire's `onProcess` callback for PipeWire-native VAX, and dedicated timer threads for CoreAudioHal and LinuxPipeBus.

Round-2 (Codex) reduced the VAX TX scope from 4 instances to 1 (the bus is singular), corrected the PipeWire framing (it IS callback-driven), removed the harmful silence pre-fill (calc_rmatch already initializes correctly), reverted the paFloat64 optimization (not a local switch), and surfaced the partial-fix nature of wallclock-driven rmatch on the non-callback backends.

J.J. Boyd ~ KG4VCF
