# WDSP 2.10, Neural Noise Reduction, and PureSignal 3

Date: 2026-09-21

Status: Approved by the user, including the explicit persistence requirement.
Implementation has not started.

Integration target: `codex/integrate-r2-main`, inspected at
`aed2278fb7033c4b5f6e36207bc6764f6bd1bad9`. That branch remains active;
implementation must reconcile its latest accepted changes before integration.

Execution handoff after specification approval: `yonder-writing-plans`, then
`yonder-cost-aware-execution`. Apply risk-based verification and a consolidated
review at the integrated DSP/session boundary. Existing repository checks remain
binding. This document authorizes no deployment or live RF operation.

## 1. Intended result and agreed scope

Upgrade the shared DSP foundation used by the desktop application and headless
Core to TAPR WDSP 2.10. Deliver its Neural Noise Reduction (NNR) in local and
remote receive operation, and implement PureSignal 3 (PS3), including its
operator controls, calibration coordinator, diagnostics, saved corrections, and
AmpView. PS3 remote activation follows R4 transmit readiness; completing remote
microphone/PTT transport is not a prerequisite for this upgrade to integrate.

The user explicitly requires a right-click NNR menu and thoughtful access to
every available NNR and PS3 control. The UI must carry forward the useful
Nereus/Thetis interaction patterns, especially DSP parameter popups, the
PureSignal dialog, and AmpView. An enable switch and a renamed PS2 window do not
satisfy this scope.

Settings persistence is an explicit acceptance requirement, including advanced
tuning and display preferences. Normal settings must survive application/Core
restart, reconnect, and receiver recreation; persistence cannot depend on a
particular popup or dialog remaining open.

Success means the operator can discover, adjust, and inspect every supported
runtime control without editing a configuration file. Common controls remain
easy to reach; advanced and diagnostic controls remain available with clear
units, descriptions, defaults, and accepted-state readback. Hardware-owned
values are visible with their ownership explained. Removed PS2 algorithms do not
receive cosmetic replacement knobs.

The upgrade preserves existing NR choices and saved selections, CFC profile
behavior, local direct operation, and the current Core/GUI ownership boundary.
New free-curve EQ editors, broadcast FM UI, and phase-rotator optimizer UI are
separate feature scopes, although their underlying sources and changed
interfaces must be reconciled when importing WDSP 2.10.

## 2. Sources and existing contracts

Pin the vendor import to TAPR commit
`b02d5bac675dd2f33ec2bab2b339f79a597c47dd`, directory `wdsp 2.10/Source`.
Never import a moving `master` during a repeat build.

Primary upstream references:

- [Release commit](https://github.com/TAPR/OpenHPSDR-wdsp/commit/b02d5bac675dd2f33ec2bab2b339f79a597c47dd).
- [WDSP reference manual revision 2.1.0](https://github.com/TAPR/OpenHPSDR-wdsp/blob/b02d5bac675dd2f33ec2bab2b339f79a597c47dd/wdsp%202.10/WDSP_Guide__Rev_2_1_0.pdf): NNR pp. 58-60; PS3 pp. 104-110, including the AmpView figure on p. 104.
- [NNR channel implementation](https://github.com/TAPR/OpenHPSDR-wdsp/blob/b02d5bac675dd2f33ec2bab2b339f79a597c47dd/wdsp%202.10/Source/nnr.c).
- [Neural processing and parameter bounds](https://github.com/TAPR/OpenHPSDR-wdsp/blob/b02d5bac675dd2f33ec2bab2b339f79a597c47dd/wdsp%202.10/Source/nnet.c).
- [PS3 calibration and display API](https://github.com/TAPR/OpenHPSDR-wdsp/blob/b02d5bac675dd2f33ec2bab2b339f79a597c47dd/wdsp%202.10/Source/calcc.c) and [status definitions](https://github.com/TAPR/OpenHPSDR-wdsp/blob/b02d5bac675dd2f33ec2bab2b339f79a597c47dd/wdsp%202.10/Source/calcc.h).
- [Generated public API](https://github.com/TAPR/OpenHPSDR-wdsp/blob/b02d5bac675dd2f33ec2bab2b339f79a597c47dd/wdsp%202.10/Source/wdsp.h).

Repository contracts and reusable components:

- `CLAUDE.md`, `CONTRIBUTING.md`, `docs/attribution/UPSTREAM-SYNC-PROTOCOL.md`,
  `docs/attribution/WDSP-PROVENANCE.md`, and `docs/development/fast-test-loop.md`.
- [Remote architecture](2026-07-28-remote-daemon-architecture-design.md), especially Core-side PureSignal, state ownership, capabilities, and R4 TX safeguards.
- [R3 plan](2026-09-20-remote-daemon-r3-plan.md): R-R3-10 local compatibility, R-R3-21 accepted controls and receive-only gating, R-R3-25 non-actuating replay, and R-R3-31 multi-slice audio continuity.
- `src/gui/widgets/VfoWidget.cpp`, `DspParamPopup.{h,cpp}`, and
  `src/gui/setup/DspSetupPages.{h,cpp}` supply the existing NR popup/Setup pattern.
- `src/core/PureSignal.{h,cpp}`, `src/gui/PsForm.{h,cpp}`,
  `src/gui/AmpViewWindow.{h,cpp}`, `AmpViewChart.{h,cpp}`,
  `applets/PureSignalApplet.{h,cpp}`, and `PsaIndicatorWidget.{h,cpp}` supply
  the existing PS coordinator and presentation.
- `src/core/session/MirrorPolicy.cpp`, `MirrorEnumDomain.cpp`,
  `StationCapabilities.{h,cpp}`, `StationServer.cpp`, and `StationClient.cpp`
  define the session integration boundaries.

The inspected local Thetis reference is v2.10.3.15. Existing Thetis behavior
remains authoritative for retained integration logic. For new WDSP 2.10
interfaces, the pinned TAPR source and manual are authoritative; this design
does not claim an inspected Thetis PS3 UI port exists. New Nereus presentation
must be attributed as original where appropriate rather than given false
Thetis citations.

## 3. Approach and compatibility

Use WDSP 2.10 as the vendor baseline and retain a documented, reviewable set of
compatibility patches. A stock directory replacement would discard local
portability and NR3/NR4 integration. Transplanting only NNR and PS3 into 1.29
would retain a substantially mixed internal API and increase future sync work.

Before replacement, classify the current vendor differences as upstream
changes, Thetis extensions, Nereus portability/lifecycle fixes, or glue. Rebase
each required difference deliberately, recording its source and rationale.
Preserve upstream notices and follow the header-census and provenance process.

Known integration obligations include:

- NR3/RNNR and NR4/SpecBleach stages, their lifecycle, and their bandpass checks.
- The existing seven-argument CFC profile API carries Qg/Qe, while TAPR 2.10's
  same-named function takes five arguments and its internal profile model has
  changed. Preserve the established profile semantics through an explicit
  compatibility implementation; silently dropping Qg/Qe is unacceptable.
- POSIX/static-build adaptations and the integration branch's macOS semaphore
  namespace repair; PS3 adds synchronization/lifecycle paths that need a fresh
  Windows-versus-POSIX API census.
- PS channel/feedback routing, ChannelMaster-derived glue, DEXP/VOX, diversity,
  analyzer/meter extensions, wisdom initialization, and filter-cache behavior.
- All consumers of WDSP public declarations, enums, and structures. A C linker
  resolving a symbol does not establish that its argument list is correct.

Keep the application's narrow WDSP boundary, reconcile it against upstream
definitions, and add compatibility declarations explicitly. Do not blindly
replace the application header with the generated upstream header. Fresh and
existing wisdom/cache cases both need verification; report the actual vendor
version and compatibility revision in diagnostics.

## 4. Requirements

| ID | Required result |
| --- | --- |
| R-WDSP-01 | Both desktop and Core use the same pinned 2.10 baseline and documented compatibility patches. |
| R-WDSP-02 | Existing NR choices, CFC behavior, platform support, and local RX/TX behavior survive the upgrade. |
| R-NNR-01 | NNR is a distinct, per-slice NR selection in all existing selector surfaces; persisted enum identities remain stable. |
| R-NNR-02 | NNR right-click opens a slice-specific parameter popup without changing its enabled state. |
| R-NNR-03 | Every supported NNR runtime parameter in section 6 is reachable through the popup or its direct full-settings links. |
| R-NNR-04 | Standard/Premium selection, actual model availability, source, and fallback are visible; a refused switch cannot appear accepted. |
| R-NNR-05 | Remote NNR runs on Core and uses capability-gated, validated, accepted-state synchronization. |
| R-NNR-06 | Model overrides are explicit station-owned assets, applied at a controlled receiver lifecycle boundary. |
| R-NNR-07 | Diagnostic modes are clearly indicated, resettable, and cannot silently become ordinary startup processing. |
| R-PS3-01 | PS3 calibration/correction runs on the radio-owning Core path with existing feedback alignment and board routing preserved. |
| R-PS3-02 | The PS dialog exposes every current runtime control, with clear common, advanced, and diagnostic groups. |
| R-PS3-03 | Status interpretation and automatic attenuation use PS3 meanings, including separate attempted/successful counts and error bitfields. |
| R-PS3-04 | AmpView renders PS3 sample data and correction curves using the new API, counts, and phase reference. |
| R-PS3-05 | Correction save/restore handles version, validity, asynchronous results, and station ownership explicitly. |
| R-PS3-06 | The remote GUI can represent PS3 capabilities/settings/status now; actuating commands remain disabled and refused until R4 grants TX permission. |
| R-PS3-07 | Reset/off, disconnect, channel destruction, and MOX transitions preserve correction/thread lifetime invariants. |
| R-UI-PS-01 | Popup, Setup, dialog, applet, and indicator use the same accepted model state; no duplicate parameter authority. |
| R-UI-PS-02 | All controls have names, units, useful tooltips, defaults/reset behavior, keyboard access, and a usable minimum-size layout. |
| R-UI-PS-03 | A source-to-UI coverage inventory proves that no supported control/readback was accidentally omitted. |
| R-PERSIST-01 | Normal NNR selection/tuning, PS3 configuration, model-asset selections, and GUI display preferences survive clean restart, reconnect, and receiver recreation at their declared scopes. |
| R-PERSIST-02 | The radio-owning process persists accepted DSP settings, with station/radio and slice isolation; remote GUI caches cannot overwrite Core settings during connection or snapshot replay. |
| R-PERSIST-03 | Persisted preferences are distinct from commands and live status: loading settings does not replay one-shot calibration, correction restore, two-tone, PTT, or diagnostic bypass; existing supported settings migrate without loss. |
| R-PERSIST-04 | Round-trip, reset, migration, rejected-edit, missing-asset, and local/remote restart tests establish persistence through the real storage and startup paths. |
| R-VERIFY-PS-01 | Software, session/UI integration, and real hardware evidence are reported separately; pending RF evidence prevents claiming PS3 RF acceptance. |

## 5. NNR interaction design

### 5.1 Entry points and quick controls

Add an `NNR` button to the VFO DSP grid using its current compact toggle style.
There is an unused fourth cell in the NR4/DFNR/MNR row in the inspected layout;
use it without reordering the existing controls. Include NNR in `RxDashboard`
and every other NR selector. Append a new enum identity rather than renumbering
Off/NR1/NR2/NR3/NR4/DFNR/BNR/MNR.

Left-click selects NNR, or turns it off when already selected, following the
existing mutually exclusive NR contract. Preserve the independent NB/ANF/SNB
controls. RADE and other mode-specific routing keeps its established capability
rules; NNR availability follows the actual audio path.

Right-click, the keyboard context-menu action, and an accessible settings
action open the same popup. Its header identifies the bound slice and processing
host. Opening or navigating the popup never activates NNR. It remains bound to
the slice that opened it; changing the active slice cannot redirect its writes.
Deletion/disconnect retires or disables its callbacks using the current session
generation and safe QObject lifetime handling.

The compact popup shows:

1. Standard/Premium model selector and actual model state.
2. Noise suppression, with a slider and editable value in dB. Display the
   underlying mask floor and explain that a more negative value suppresses more.
3. An expandable **Advanced** section for position, alpha/knee, input
   normalization time, maximum gain, and attack/release smoothing.
4. **Diagnostics...**, **Models...**, **More Settings...**, and
   **Reset NNR tuning** actions. These open the exact corresponding section of
   Setup, retaining the same slice identity.

Use the `DspParamPopup` visual language and add only the reusable numeric and
expandable-group support required. The full NNR Setup tab presents the same
bindings with more room, including diagnostic modes and model management.
Popup and Setup numeric controls preserve fractional precision rather than
round-tripping through integer-only sliders.

### 5.2 State and defaults

Upstream normal defaults are Standard, mask floor -25 dB, post-AGC, alpha 1,
knee 10 dB, normalization tau 2 seconds, maximum gain 12 dB, smoothing 0/0 ms,
network diagnostic mode, and Q-zero output layout. NNR is off for a new
configuration until selected. Existing user NR choices remain unchanged.

Persist normal per-slice tuning through the established AppSettings/model path.
Reset tuning restores the source-backed defaults for that slice without changing
its NR selection or global model paths. Diagnostic mode/output-layout changes
are session-only, visibly marked, and reset to normal on a fresh station
session. A reconnect must not quietly replay a diagnostic bypass as ordinary NR.

## 6. NNR control and readback coverage

Ranges below are source-derived unless explicitly identified as the manual's
recommended operator range. Backend validation is required even when a widget
already constrains input. All numeric commands reject non-finite values.

| Upstream capability | UI placement and behavior | Domain/default or ownership |
| --- | --- | --- |
| `SetRXANNRRun` | NNR selection/toggle; actual processing/bypass state in status | Off/on; new-config default off |
| `SetRXANNRModel`, `GetRXANNRModel` | Model selector in popup and Setup; show actual returned selection | Slot 0 Standard, slot 1 Premium; default 0 |
| `SetRXANNRMaskFloor` | Suppression slider plus precise numeric entry | Manual recommends -50 to -10 dB; default -25 dB |
| `SetRXANNRPosition` | Advanced pre-/post-AGC selector; explain upstream's post-AGC recommendation | 0 pre, 1 post; default 1 |
| `SetRXANNRAlpha` | Advanced deep-filter alpha; reshapes gain below the knee, with 1 leaving that shaping neutral | 0-4; default 1 |
| `SetRXANNRAlphaKnee` | Advanced alpha knee / attenuation depth, positive dB | 0-40 dB; default 10 dB; preserve upstream sign convention |
| `SetRXANNRTau` | Advanced input-normalization time constant | 0.05-30 seconds; default 2 |
| `SetRXANNRMaxGain` | Advanced maximum filter gain | 0-24 dB; default 12 |
| `SetRXANNRSmooth` | Separate attack and release numeric/slider controls | Each 0-500 ms; default 0/0 |
| `SetRXANNRTestMode` | Diagnostics selector: Network, Identity, Low-pass; prominent diagnostic-state badge | Modes 0/1/2; normal 0; session-only |
| `SetRXANNRcmode` | Diagnostics output layout: duplicate real output to I/Q, or Q zero; explain that this is DSP layout, not speaker stereo pan | 0 duplicates, 1 Q zero; normal 1; session-only |
| `SetNNRModelPathSlot` | Models page: per-slot bundled model or explicit override; source/readiness/next-apply status | Process-wide paths set before receiver creation |
| `SetNNRModelPath` | Covered by slot-0 model management; no redundant conflicting path field | Alias for slot 0 |
| `getDelay_nnr`, `getRun_nnr`, readiness/model metadata accessors | Read-only diagnostics through a narrow safe WDSP adapter | Actual latency, active/bypassed state, sample rate, model readiness and identity |
| Profiling support | Diagnostic status/export when profiling is compiled in; show unavailable reason otherwise | Do not promise profiling data in builds where macros compile it out |

The upstream public API does not export every useful getter. Any added adapter
must expose bounded value snapshots under the appropriate WDSP synchronization;
do not expose internal pointers to GUI/session code. Handle partially loaded
models before calling advanced setters: several source setters assume the
selected internal objects exist.

Both embedded models ship with Core and local desktop builds. Their reference
manual latency is 51.17 ms in the documented configuration. Actual runtime
latency should be read back where supported. The manual's CPU percentages were
measured on an AMD desktop and are not Rock 5C capacity evidence.

Model overrides are loaded once at receiver creation. Choosing a new asset
therefore produces an explicit pending change and a deliberate reconnect/rebuild
operation using the established channel-lifecycle path. It does not mutate all
live channels from a file picker. An empty override selects the bundled model.
Make the default path choice explicit so the process working directory cannot
silently choose an experimental model.

## 7. PureSignal 3 interaction and control coverage

### 7.1 Dialog, applet, and context menu

Evolve `PsForm` into **PureSignal 3**. Preserve its modeless operation and the
familiar quick actions: automatic calibration, single calibration, off/reset,
two-tone, AmpView, and save/restore. Group these actions by purpose and separate
RF-producing actions from file/display actions. Right-click on the PS applet or
PS indicator offers the same common actions plus direct **Settings...**,
**AmpView...**, and **Diagnostics...** routes. Opening a menu must not toggle
PS-A or start calibration.

The persistent status area shows feedback level, correction applied state,
calibration activity, attempted and successful counts, and the latest meaningful
failure. Feedback present, corrections applied, and calibration success are
distinct states, conveyed by text as well as existing colored indicators.

Use grouped expandable areas or tabs for **Calibration**, **Timing & Feedback**,
and **Diagnostics**. Retain board-aware defaults and existing automatic/quick
attenuation where still applicable. The diagnostic view decodes status bits and
also offers the full raw 16-value snapshot for troubleshooting, clearly marking
reserved values. It exposes the source version and correction-file errors.

### 7.2 Runtime coverage matrix

| Capability | Operator surface / owner | Required behavior |
| --- | --- | --- |
| `SetPSControl`, `SetPSReset`, `SetPSMancal`, `SetPSAutomode`, `SetPSTurnon` | Coherent Off, Single, Automatic, and Use Current Correction actions | Coordinator owns valid transitions; avoid four independently inconsistent checkboxes |
| `SetPSRunCal` | Advanced collection/calibration processing enable with explicit state | Disabling it suspends `pscc` state-machine processing; an existing TXA correction can remain active. This is distinct from Off/Reset. |
| `SetPSLoopDelay` | Calibration interval / CPU trade-off in Timing | Seconds, existing UI range 0-100; new PS3 default 0; preserve saved supported values |
| `SetPSMoxDelay` | MOX settling wait in Timing | Seconds, retain existing UI range 0.1-10; new PS3 default 0.1; preserve saved supported values |
| `SetPSTXDelay` | Requested amplifier delay plus actual applied delay | Existing nanosecond UI range 0-25,000,000, 20 ns step; show returned applied value |
| `SetPSHWPeak`, `GetPSHWPeak` | Advanced hardware peak, current value, board default/reset | Positive finite value; preserve validated board/protocol scaling, not a universal copied default |
| `GetPSMaxTX` | Measured peak readback beside configured peak | Do not overwrite configuration while merely polling |
| `PSSaveCorr`, `PSRestoreCorr` | Saved-correction manager plus quick Save/Restore | Show pending/completed/error; restore can activate correction and requires actuation permission |
| `GetPSInfo` | Persistent status plus complete decoded/raw diagnostics | Use PS3 field semantics and bit masks |
| `GetPSDisp` | AmpView and its diagnostics | New bounded sample/curve snapshot described below |
| `SetPSFeedbackRate` | Display actual feedback rate and originating board/stream configuration | Core sets it to match the two sample streams; not an unrelated user override |
| `SetPSMox` | Display actual TX/MOX state; normal PTT/TX controller owns it | Never offer an independent checkbox that falsifies actual TX state |
| `pscc` and feedback routing | Read-only TX/RX stream identity, rate/alignment/continuity diagnostics where measured | Core owns sample-correlated processing; no raw-I/Q round trip through GUI |
| Existing auto-/quick-attenuation and feedback controls | Calibration/feedback group and applet shortcuts | Retain host behavior after adapting attempt-count/status interpretation |
| Existing two-tone settings and measurement toggle | Two-tone action and direct route to existing tone controls | Preserve settings and measurement surfaces; every TX entry point retains permission/interlock checks |

All aliases are accounted for through a single semantic control. No functioning
public control may be omitted merely because the old dialog lacked it.

PS3 removes `SetPSPinMode`, `SetPSMapMode`, `SetPSStabilize`, `SetPSPtol`, and
`SetPSIntsAndSpi`. Remove their live widgets and calls, and preserve old saved
settings only as migration/rollback data. The manual specifically says PIN,
MAP, and TINT no longer correspond to adjustments in the new implementation.

The coverage inventory also records compile-time algorithm constants, neural
weights, raw tensor/allocator interfaces, and processing/lifecycle primitives as
implementation-owned. Exposing complete runtime controls does not turn those
internals into unvalidated live tuning parameters. These exclusions must be
visible in the inventory with reasons; newly discovered real runtime tunables
are added to the appropriate UI group rather than silently excluded.

### 7.3 Correct status interpretation

For the pinned PS3 source, `info[5]` counts successful calibrations and
`info[7]` counts attempted calibrations. Retained automatic-attenuation logic
must observe an actual new attempt, including unsuccessful attempts; it cannot
keep treating PS2's old counter semantics as current.

Decode builder/solution status values as bitfields. In particular, the
over-drive indication is the corresponding bit in `info[6]`, not only equality
with the integer 2. Include `info[12]` file read/write status from the source,
even though the manual's shorter status list does not enumerate it. Avoid
inventing success from a nonzero feedback level or from a queued request.

The MOX-off notification precedes TXA channel shutdown, as required by the
manual. Correction worker shutdown, queue retirement, rate changes, and channel
destruction must work on macOS, Linux/ARM64, and Windows without surviving
callbacks into freed channels.

Off/Reset must take precedence over the advanced processing-enable state. The
coordinator must allow the engine to complete the stop transition and verify
correction is off; queuing a reset behind a permanently suspended `pscc` state
machine is not a successful stop. Cover this interaction explicitly.

## 8. AmpView

Carry forward the existing modeless AmpView window, dark chart style, reference
line, Show Gain, Phase Zoom, Low Res, and On Top controls. Use responsive layouts
instead of preserving fixed toolbar gaps that clip at the minimum window size.
Add a legend with per-series visibility and clear sample/curve identification;
retain readable magnitude/gain and phase axes. Right-click exposes the same
display choices and a route back to PS settings, with no calibration side effect.

The view consumes a value snapshot with:

- Measured input magnitudes `x`, output magnitudes `ym`, and phase components
  `yc`/`ys` with `nsamps_out` samples.
- Independent magnitude-correction coordinates `xm_cor`/`ym_cor` and
  angle-correction coordinates `xa_cor`/`ya_cor`, with `cpts_out` points.
- `phs_ref_deg_out`, source channel/session identity, and freshness metadata.

Preserve the phase reference when deriving measured phase, following the
upstream convention. The correction angles are already degrees. Stop evaluating
the old PS2 cubic-coefficient buffers; the new curve coordinates are the source
of truth. Document the source-backed plotting transform and verify it against
known snapshots and the manual's figure.

`GetPSDisp` takes no caller capacities. The WDSP adapter must allocate sufficient
storage before the call using the pinned implementation's maximum sample and
curve sizes, then validate returned counts before constructing the bounded
application snapshot. The guide describes 4096 samples and 512 curve points;
verify allocation bounds against the source, not just a count returned after a
possible overwrite. Keep buffer internals out of Qt widgets and wire messages.

Publish AmpView snapshots only while subscribed/visible, with a bounded cadence
and latest-value backpressure. Remote transport uses bounded display snapshots,
not the calibration sample feed. Closing the window, switching sessions, or
disconnecting retires the subscription; stale graphs are visibly stale/cleared.

## 9. Core/session architecture and file ownership

```text
NNR popup / Setup -> SliceModel accepted settings -> station owner
  local:  RxChannel -> WDSP RXA/NNR -> existing audio path
  remote: authenticated command -> Core SliceModel -> RxChannel -> WDSP
                                      -> mixed/encoded audio -> GUI

PS dialog / applet -> typed PS command -> Core PureSignal coordinator
  aligned radio feedback -> pscc -> PS3 calibration -> TXA correction
  coordinator/status + bounded AmpView snapshot -> GUI presentation
```

Add capabilities for NNR support/model availability, WDSP/PS algorithm version,
and the PS display/control schema. Do not infer them solely from the radio's
existing `pureSignalPresent` flag. An older Core may support PS2 or no NNR.
Unknown enum values and unsupported commands are refused cleanly, and the GUI
explains unavailable controls without overwriting preferences.

Normal NNR tuning is slice-scoped accepted state. Custom model configuration is
station/process scoped. PS configuration, correction assets, and live status
belong to the radio-owning Core; display preferences and window geometry belong
to the GUI. Extend the existing allowlisted mirror/settings mechanisms, with
typed actions for operations that must not replay as ordinary properties.

Provide a narrow station-owned file workflow for custom NNR models and PS3
corrections: list/select, import/export, identity/status, and pending apply.
Remote selection identifies a Core asset rather than passing a desktop path as
if it were meaningful on Core. Bounded transfer, validation, and atomic
publication use the authenticated session and explicit station storage paths.
Resolve filenames internally before calling WDSP's fixed-size path APIs;
validate encoded byte lengths before the call. File selection/import alone
does not load a model into live channels or activate a correction.

PS2 correction files remain intact. Detect and refuse incompatible files with
a useful version message; do not reinterpret them as PS3 data or promise an
unverified automatic conversion. PS3 saves use the upstream format with
application-side version/identity bookkeeping. Verify round-trip behavior and
surface asynchronous file errors. Reconnect/snapshot replay never reapplies a
correction file as an actuation command.

With `txPermitted=false`, remote clients can inspect PS3 support and authorized
read-only diagnostics, and manage non-actuating preferences/assets. Commands
that enable calibration, apply/restore correction, generate tones, or key TX
remain disabled in UI and rejected by Core. Off/reset remains available as a
stop operation for the controlling session. R4 adds its transmit authorization,
watchdog, starvation, and handoff acceptance before granting operational PS3
commands. This upgrade does not widen the existing receive-only checkpoint.

### 9.1 Explicit persistence contract

Use the repository's `AppSettings` infrastructure and existing stable radio and
slice identity conventions; do not add a parallel `QSettings` store. In local
operation the desktop owns DSP settings. In remote operation the Core owns DSP
settings and assets, and the GUI persists only its presentation preferences.
Core settings must be loaded and saved in headless startup/shutdown paths too;
a desktop close handler cannot be the daemon's persistence mechanism.

| Setting class | What survives restart | Owner/scope |
| --- | --- | --- |
| NNR receive configuration | NR selection, Standard/Premium selection, mask floor, position, alpha, knee, normalization tau, maximum gain, attack and release | Radio-owning process; existing radio/slice settings identity |
| NNR model overrides | Bundled/custom choice and validated asset identity for each slot; pending versus applied identity remains truthful | Station/process, because upstream model paths are process-global |
| PS3 configuration | Supported timing/delay, hardware-peak override, automatic/quick attenuation preferences, desired automatic-calibration preference, and normal processing preference | Radio-owning process; existing per-radio PS settings identity |
| PS3 correction assets | Saved files and metadata/selection, without implicitly applying the selected file | Station-owned asset store, with radio/version compatibility metadata |
| GUI presentation | PS dialog/AmpView geometry, On Top, expanded sections, existing Show Gain/Phase Zoom/Low Res, and new series visibility choices | GUI-local AppSettings; no DSP commands during restoration |
| Commands, measurements, and diagnostic overrides | No automatic replay of single calibration, correction apply/restore, off/reset, two-tone/PTT, counters, measured peaks, live MOX/correction state, NNR TestMode or output-layout override | Transient state; diagnostics return to their documented normal defaults |

Persist automatic-calibration intent separately from operational arming. Local
startup retains the established coordinator-controlled behavior and its normal
readiness checks. Remote reconnect or loading a saved preference cannot bypass
R4 permission, synthesize a new calibration action, or key the transmitter.
The UI shows saved intent and actual activity distinctly. A saved correction
selection is likewise not authority to restore/apply it.

Save only validated, accepted DSP configuration. A rejected remote edit must
not become the value restored on next start. On reconnect, the Core snapshot
wins over stale GUI values. Preserve fractional values through storage and
reapplication; disabled features retain their tuning. Apply model-path choices
before receiver creation, and reapply saved tuning after channel recreation
before enabling NNR. Do not create defaults by overwriting an existing record
before it has been loaded.

Use the existing settings-save scheduling and durable flush mechanisms; ensure
accepted changes are saved without opening or closing a settings window and
flush pending saves on orderly shutdown. A missing/corrupt model asset must
produce a visible availability/fallback state without silently erasing the
saved custom selection. Old-Core negotiation similarly leaves unsupported
preferences intact. Reset writes the documented defaults back to the same
scope and survives a subsequent restart; it does not reset other slices or
station-wide assets when only tuning was requested.

Migration preserves existing NR enum identities, supported PS2 timing/feedback
settings, and existing AmpView choices. Retain removed PS2-only keys as inactive
rollback data. Invalid stored values use the same validation/default rules as
live edits and expose any material fallback. Every coverage-inventory row
declares its settings key, scope, load/save path, and whether it is transient.

## 10. UI behavior and coverage discipline

Maintain one control inventory with upstream symbol, semantic parameter,
source location, type/unit/domain/default, lifecycle, model property or command,
UI entry point, persistence owner, local/remote capability, and verification.
Sections 6-7 are its initial required rows. Reconcile the final inventory against
the pinned public header and relevant implementation-level controls before
calling the UI complete. Each row must identify a real working surface or an
explicit implementation/hardware-owned exclusion.

Controls show accepted values after backend clamping/refusal. Pending changes
and rejected commands have visible outcomes; delayed acknowledgements cannot
overwrite a newer edit. Opening windows, restoring geometry, applying snapshots,
and refreshing diagnostics are non-actuating operations.

Use slider-plus-spinbox controls for continuous tuning, numeric units beside
fields, contextual help for uncommon terms, and reset actions scoped to the
current slice/group. Keep scroll-wheel protection and button-default behavior
consistent with repository conventions. Advanced groups may start collapsed,
but remain clearly discoverable and keyboard accessible. Diagnostic modes have
a visible badge even after their settings window is closed.

Verify actual Qt layouts at the existing minimum window sizes and supported
display scaling. Inspect popup anchoring near screen edges, expanded groups,
long status text, unavailable capabilities, and reconnect while dialogs are
open. Native Qt interaction/screenshot evidence is the appropriate layer here;
Yonder's web-page scripts and browser-specific requirement IDs do not apply to
this repository.

## 11. Verification and acceptance

### Software and source evidence

- Verify the pinned vendor inventory, retained patches, public signatures,
  attribution/header census, and current platform build requirements.
- Run meaningful NNR signal/lifecycle checks with actual WDSP: model selection,
  unsupported-model refusal, effective settings, rate constraints, repeat
  enable/disable, multi-slice isolation, finite output, and existing NR behavior.
- Verify each advanced setting reaches its intended backend with correct units
  and accepted-state readback. Include alpha/knee defaults, smoothing bounds,
  unavailable model objects, model-switch/reset behavior, and diagnostic reset.
- Verify PS3 status decoding and automatic attenuation on successful and failed
  attempts, combinations of failure bits, MOX/reset ordering, correction file
  failure and round-trip, display bounds/phase convention, and worker teardown.
- Characterize retained CFC profile behavior, including non-default Qg/Qe,
  before selecting its compatibility implementation; compare resulting DSP
  output/response after integration.
- Extend relevant existing tests such as `tst_puresignal_coordinator`,
  `tst_tx_channel_ps_setters`, `tst_wdsp_ps_smoke`, `tst_ampview_window`, and
  feedback/DDC routing tests. Actual linked WDSP evidence complements model
  fixtures; neither replaces the other.
- Build the affected test targets before running them. Use focused checks
  during development and the repository's required full suite on the combined
  final revision, plus Linux/macOS/Windows CI and Linux ARM64 build coverage.

### Session and UI integration evidence

- Exercise NNR from the real popup and Setup page in local mode and against a
  Core session; observe accepted state and processed audio, not only widget
  presence. Editing slice B must not retune slice A's NNR.
- Show every coverage row working or its documented capability/ownership state.
  Verify reset, fractional values, model fallback, file pending/error state,
  diagnostic badges, keyboard context menus, and minimum-size/scaled layouts.
- Verify older-Core negotiation, missing capability, refusal, disconnect,
  reconnect, session replacement, slice deletion, and stale AmpView snapshots.
- Test receive-only refusal at Core independently of disabled widgets, including
  correction restore, automatic calibration, and two-tone entry points.
- Verify remote snapshot/persistence replay cannot arm PS3 or activate diagnostic
  NNR modes. Local automatic-calibration intent follows the existing readiness-
  gated coordinator path. Migration preserves existing selections/settings.
- Exercise actual save/reload and process startup for local desktop and headless
  Core: set non-default fractional NNR/PS3 values with the feature off, restart,
  reconnect/recreate channels, and check both widgets and applied DSP values.
  Repeat with two slices and two radio/station identities to detect leakage.
- Change a remote setting, receive acceptance, restart Core and reconnect a
  stale GUI; the accepted Core value must win. Refuse an invalid/unauthorized
  edit and verify it was not saved. Reset a group, restart, and verify only that
  group's defaults changed. Cover settings migration, absent/corrupt assets,
  and a clean first run without touching real user configuration in tests.

### Hardware and operator evidence

- Receive with Standard and Premium on the Rock 5C and desktop, measuring per-
  slice CPU/memory, audio continuity, and end-to-end latency against the same
  pre-upgrade workload. Exercise multi-slice reception with the current remote
  display/audio load and retain the R3 sustained-session acceptance.
- Arrange an operator listening comparison on representative strong/weak speech
  and noise, including NNR off and both models. Listening evidence establishes
  usability; it does not substitute for bounded numerical/lifecycle checks.
- For local PS3 RF acceptance, use an explicitly arranged supported-radio and
  feedback bench. Record radio/protocol/band/power/feedback setup, calibration
  convergence, applied correction, failure recovery, AmpView, and measured
  distortion with PS off/on. A successful software smoke test is not an IMD
  improvement claim. Do not create a deliberately overdriven RF condition to
  test a status label; inject recorded/synthetic status for that UI case.
- Report untested radio families/protocols as pending hardware evidence. R4
  owns remote transmit/PS3 RF operation and its fault/reconnect/handoff tests.

## 12. Delivery boundaries and handoff

The implementation plan should sequence: vendor/compatibility baseline; NNR
engine and controls; PS3 coordinator/display/files; session integration; combined
verification and hardware acceptance. These are reviewable steps within the
agreed combined-build objective, not separate alternative products.

This specification is held on a separate design branch to avoid racing the
active integration task's source, tests, builds, or hardware. Incorporate the
approved artifact into the integration work at a deliberate Git boundary.

Planning must resolve source-level details before affected production edits:
the complete vendor delta inventory, the CFC compatibility mechanism, newly
required POSIX primitives, exact typed session messages, asset-size bounds
derived from formats, and the phase plotting transform. Their acceptance
contracts are fixed above; guessed implementations do not satisfy them.

There is no claimed Rock 5C NNR performance result or PS3 RF result yet. The
written spec and subsequent implementation plan each receive the review
required by the selected brainstorming workflow before production work begins.
