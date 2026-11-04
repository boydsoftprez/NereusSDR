# Existing bar faces and container defaults — approved implementation contract

JJ approved adapting Brian Sutton's PR #731 into a selectable face on existing
bar meters. JJ chose the user-facing names Classic and Modern. The existing
Classic face remains the default. Modern is the optional presentation, retaining
the existing internal Instrument value. Container defaults and individual
overrides are approved below.
No new catalog IDs, meter sources, calibration, thresholds, defaults or DSP work.

## Design

Store `face` (`Classic`, `Instrument`, or `Container`) in the existing BarPreset
JSON record, with `config.meterFace` holding the container Classic/Instrument
default. New catalog bars follow Container; missing face on old records is
explicit Classic. Runtime resolution never overwrites the saved policy.
Switching the face changes presentation only, preserving the live readings and
smoothing/history state, type ID, binding, label, calibration, settings and shared
copy/paste path. Missing `face` renders Classic; older readers preserve the key
through the existing `m_unknown = record` round trip.

The selected bar's Appearance and behavior panel gains a Face selector before
Label, with Use container setting (resolved name), Classic and Modern. The
container toolbar has Meter face and Use for all meters. Modern honors the existing controls (caption/title, values, history,
markers, fill style, scale/fill colors, timing and power limit). It formats values
through the existing signal-unit and power/SWR conventions. Tick locations come
from calibratedPosition and the existing tick lists; SWR's high point stays 3.0,
and signal high color begins at the configured existing threshold. Narrow faces
clip to their own bounds and suppress colliding tick captions.

PureSignalFeedbackMeter currently uses a 12px rounded track, 4px center fill,
independent range band and marker. Extract only their parameterized native Qt
painting operations into a small shared helper. Preserve its exact geometry,
colors, range boundaries, numeric captions and behavior. Existing Thetis headers,
comments, citations and constants stay intact. Credit Brian Sutton for the
original proposal/implementation, and J.J. Boyd with OpenAI Codex for this actual
adaptation; no claim about Brian's AI tools.

## Dependency-aware execution

1. Add focused regressions for face persistence/in-place state, registry identity,
   editor choice/caption, configuration copy/paste, rendering bounds and thresholds.
   Run the new tests against the existing implementation to establish failure.
2. Replace PR #731's new classes/IDs with BarPreset face state and selection;
   preserve existing configuration and copy/paste semantics.
3. Implement Instrument using the existing scales, dynamics and controls; extract
   the shared drawing helper and keep PureSignal behavior byte-for-byte equivalent
   at the call sites.
4. Build only the focused targets in a private build with the current compiler;
   run their ctest cases. Capture real rendered Classic/Instrument examples and
   inspect them, including narrow and small faces. Root owns the broader gate.
5. Hand back uncommitted files, evidence and limitations. No app/radio/profile
   access, commits, pushes, comments or release actions.

## Initial face-only verification receipt (2026-11-03)

- Private build: `/Users/j.j.boyd/.config/nereus/builds/meter-face-20261103`,
  Apple Clang `21.0.0.21000401`, fresh PCH, RelWithDebInfo, CPU spectrum backend.
  Cached dependency sources and the SHA-256-verified pinned Opus model were reused.
- Pre-adaptation RED: actual old reader retained `face=Instrument` and unknown
  data; four regressions failed as expected (live-state reset, missing face
  default/editor and Instrument track). Responsive endpoint-caption regression
  also failed before the fix.
- Final focused build and ctest: `tst_meter_instrument_face`,
  `tst_meter_power_swr_bars`, `tst_container_picker_catalog`,
  `tst_container_copy_paste`, `tst_puresignal_refresh_state`: 5/5 passed.
  PureSignal's opt-in `renderSyntheticUiWhenRequested` capture case was skipped
  because `NEREUS_PS_REFRESH_CAPTURE_DIR` was unset; the feedback renderer was
  independently captured by the new test. The new test has 14 passing cases including init/cleanup, all existing bar
  record identities, shared paste, visible controls, signal units, SWR/signal
  threshold pixels, smoothing continuity and tiny-face clipping/painter state.
- Actual production-renderer PNGs: `captures-before/` and `captures-after/`
  in the private build. Ten Classic captures and the existing PureSignal
  feedback capture are byte-identical before/after. Workspace and classic-look
  captures include 320x72 and 160x48, plus the actual Face editor.
- Inspected actual Instrument screenshots: full signal endpoint captions,
  collision-suppressed intermediate captions and explicit small-value elision.
  The existing preferred/minimum size remains 260x72; tiny examples exercise
  graceful clipping rather than a change to that sizing contract.
- Existing upstream license block in PureSignalInstruments.h verified
  byte-for-byte; existing Thetis comments, citations and calibration constants
  retained. Original Brian Sutton Git author remains in the pending merge.

Classic remains the default under the final approved container/override design.
No radio/device or real-user profile was accessed. Native GPU presentation and
full-suite/CI verification remain with the integration owner. The container host
continues its existing Apply/reconcile lifecycle; this change preserves the
record identity and the bar object's face-only dynamics, not all-container
widget lifetime across Apply. No index/commit/push/release actions were taken.

## Approved container defaults and overrides (2026-11-03)

JJ approved Classic/Modern naming and both container and individual controls.
Modern retains the stored `Instrument` value. Classic remains the default.

- Container `config.meterFace` is Classic or Instrument (absent means Classic).
- Bar `face` is Classic, Instrument, or Container. Missing face is legacy
  explicit Classic; new catalog bars use Container. No schema-version change.
- Resolve Container at presentation time through a separate runtime container
  input on BarPresetItem; never flatten the policy when capturing/saving.
- The real dialog gains Meter face and Use for all meters. Individual Face has
  Use container setting (resolved name), Classic, Modern. All changes remain in
  the draft/live preview until Apply; Cancel discards them.
- Use for all meters resets only valid face-capable bar overrides to Container;
  analog meters, unrelated entries and opaque fields remain intact.
- Follow policies resolve in the destination after move/copy/import. Explicit
  faces remain explicit. Existing Instrument records remain compatible.
- Container-default changes reuse existing live/preview bars where their hosts
  already allow reuse, invalidating painting without resetting readings/history.

Execution: first reproduce regressions for new-bar Follow policy, mixed legacy
and explicit records, container resolution, unchanged live dynamics, real dialog
preview/Apply/Cancel and batch override reset; then implement bar runtime policy,
host/preview/editor resolution, and real draft controls. Verify focused targets
only, plus actual themed dialog captures. Root owns independent review and CI.

## Final container/override verification receipt

The group controls and individual inheritance choice are now production code;
the capture harness no longer inserts proposal controls or renames selector
items. Actual group-choice events update the draft and preview.

- RED established against the face-only implementation: new bars were explicit
  Classic and the container face control did not exist.
- Nine freshly built focused executables passed: meter instrument face,
  container settings workflow, container preview, container content host,
  container document codec, picker catalog, copy/paste, meter record
  serialization and power/SWR bars. Ctest: 9/9, 23.56 seconds, no skipped cases.
- The final face regression passed all 17 cases after adding batch-reset
  Apply/reload and unsupported opaque-entry preservation assertions. Its final
  capture run passed in 1.60 seconds. No broader repeat was necessary because
  only that test harness changed after the nine-target run.
- Covered: new Follow records, old records with no face in either properties
  or legacyRecord, explicit overrides, destination move/copy via the actual
  portable-entry codec, saved policy/unknown data round trips, live object and
  reading retention on container-default change, actual individual choices,
  draft preview/Cancel/Apply and Use for all meters save/reload. Analog and
  unsupported entries remain byte-for-byte equal as document entries.
- Final source's ten Classic face PNGs and PureSignal feedback PNG remain
  byte-identical to the pre-adaptation captures (11/11). Upstream license
  blocks remain byte-for-byte intact; git diff --check passes.
- Production-control captures: private build `group-face-captures/`; final
  individual face captures: `captures-final/`; logs: `evidence/`.

The full dialog captures use the real manager/store/editSession/preview path,
real application Fusion/dark/baseline/workspace bootstrap and real Show theming.
They use an isolated synthetic workspace, no operational MainWindow or user
profile, no radio/audio source and no running poller. Synthetic numerical values
are injected directly into actual preview bars. Popup images combine the actual
Qt popup and dialog pixels at their mapped positions, excluding the OS frame.
Native GPU/CI verification and integration remain with the parent owner.
