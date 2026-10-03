# Core receive layout restoration

Requirement: R-R3-34 in [the R3 plan](2026-09-20-remote-daemon-r3-plan.md),
Task 4f. This elaborates the already authorized Core restart behavior.

Execution: yonder-cost-aware-execution. Requirements and acceptance criteria
are binding; test order and review effort follow its risk-based policy.

## Intended behavior and ownership

Restarting Core with two receivers restores both stable slice IDs, current
frequency/mode and pan bindings for the same radio. A receiver removed by the
operator remains removed even if `slice_count` is larger. A GUI reconnect is
not a request to create missing receivers. Core owns membership; the GUI
reconciles the completed station snapshot and explains rejected restoration.

A versioned per-MAC manifest is preferable to increasing `slice_count`, which
cannot represent deletions, identities or bindings. Migrating every legacy
`Slice<N>/Band...` preference would broaden the change unnecessarily. Those
preferences remain in their existing namespace; the manifest adds only the
current receive layout. Existing per-MAC/stable-ID NNR persistence is retained.

## Persistence contract

`ReceiveLayoutStore` in `src/core/ReceiveLayoutStore.{h,cpp}` is a widget-free,
side-effect-free codec and AppSettings adapter. Its stored value is compact
JSON under `hardware/<normalized-mac>/receiveLayout`:

```json
{"version":1,"radeRxOwnerId":null,"slices":[{"id":0,"panKey":"pan-0","frequencyHz":14293200,"dspMode":1},{"id":2,"panKey":"pan-1","frequencyHz":7200000,"dspMode":0}]}
```

The list order is preserved. IDs are unique and within
`WdspEngine::kMaxSliceChannels`; membership is nonempty and bounded by that
same global channel ceiling. Multiple slices can share a pan. Pan names use
the application's canonical `pan-<nonnegative-int>` spelling; they do not
encode stream or physical ADC identities. Empty pan keys on capture normalize
to `pan-0`, matching the existing default-pan behavior. Serialized descriptors
must contain explicit canonical pan names. Unknown schema versions, missing
or wrongly typed fields, fractional IDs/modes, invalid enums, nonfinite or
out-of-range frequencies, duplicate IDs and oversized manifests are rejected
as a whole. The JSON budget is 16 KiB for at most five short descriptors, a
storage guard rather than a radio packet limit.

The receive tuning admission range is 0–61.44 MHz, from the pinned Thetis
`console.cs:15540,15552` defaults (v2.10.3.15), expressed through named
`SliceModel` constants. This is not a firmware-specific capability claim or a
change to the GUI's existing 100 kHz manual-entry floor. Transverter ranges
are not implemented by this manifest. A future range capability must extend
admission explicitly rather than silently clamping saved values.

Exact adapter interfaces:

- `ReceiveSliceState { int id; QString panKey; double frequencyHz; DSPMode dspMode; }`.
- `ReceiveLayoutStore::load(const AppSettings&, const QString& mac)` returns
  `LoadResult { LoadState state; QList<ReceiveSliceState> slices; QString error;
  std::optional<int> radeRxOwnerId; }`,
  where state is `Missing`, `Loaded`, `InvalidIdentity`, or `InvalidData`.
- `ReceiveLayoutStore::stage(AppSettings&, const QString& mac,
  const QList<ReceiveSliceState>&, QString* error = nullptr,
  std::optional<int> radeRxOwnerId = std::nullopt)` validates the complete
  list before replacing the in-memory setting, and does not write to disk.
- `ReceiveLayoutStore::validate(const QList<ReceiveSliceState>&,
  QString* error = nullptr, std::optional<int> radeRxOwnerId = std::nullopt)`
  checks already-decoded descriptors using the same constraints.

The explicit `radeRxOwnerId` is the decoded receive-audio owner, independent of
active VFO and TX binding. It must name a saved RADE_U/RADE_L slice. With no
RADE slices the owner is null. With exactly one, an omitted/null owner resolves
unambiguously to that ID; load also admits the older two-key JSON root in this
unambiguous case. Multiple RADE-mode slices require an explicit valid owner.
Reject ambiguous saved audio ownership rather than select whichever slice is
first, active or TX-bound. Staging always emits the resolved owner field. This
records existing runtime ownership; it does not add concurrent RADE decoders.

MAC identity must pass `AppSettings::normalizedRadioMac`. Missing state permits
the configured-count fallback. Invalid state is distinct from missing state;
its bytes are preserved and the runtime exposes recovery guidance. Failed
staging preserves the previous value. The existing coalesced atomic
`AppSettings::save` and shutdown flush own disk commits and errors.

## Startup and resource admission

Pinned MACs can load before the station listener starts; an unspecified MAC
selects its namespace only after discovery. The listener stays responsive
while the radio is offline. Provisional membership is not persisted.

Offline hydration must not call a mode setter that starts RADE. Add a narrow
Local-model restoration path which seeds validated tuning/filter metadata
without resource work. Reconcile by updating shared IDs, creating missing IDs
and then retiring extras, so the last-slice invariant remains intact. Retirement
during this batch must bypass ordinary per-slice save/flush. Suppress manifest
writeback throughout hydration and until resource admission finishes. Never
change a model's Local/Remote role to suppress side effects.

`SliceModel::restoreFromSettings` already assigns its mode directly; unlike
`setDspMode`, it does not construct RADE. It can supply conventional preferences
for the manifest's band while offline and signal-blocked, with the manifest's
validated frequency/mode applied afterward through a dedicated seed helper.
That override must bypass the restored frequency lock and select the filter
for the manifest mode, not retain one from a different legacy mode. For an
already mirrored bootstrap slice, suppressing signals requires a full settled
state publication afterward, including ordinary AF/filter preferences. A few
tuning notifications would leave the GUI stale. The station mirror's existing
complete-snapshot path is a candidate; session/media effects must be tested
before selecting that runtime implementation.

Before committing the restored layout, check stable IDs/count against the
selected board's channel limit and validate actual stream placement. The first
slice of each distinct pan requests its own stream, preserving ordinary
new-pan semantics; subsequent slices on that pan may share. Do not silently
collapse distinct saved pans because their frequencies happen to be close.
An unsupported saved member must be refused explicitly and remain recoverable
from the original manifest; fallback must not overwrite that manifest.

Skip legacy active-A tuning recall when manifest tuning was restored. Valid
saved membership also suppresses both configured-count top-ups (initial
startup and first Connected). Same-radio in-process recovery preserves live objects rather than rereading an
older saved layout or repeating destructive startup admission. Each distinct
pan reclaims an independent DDC. Live members that have diverged beyond their
pan's other windows can reclaim another available stream without losing their
identity. RADE ownership refreshes once after the new workers are wired; the
later Connected notification does not repeat that publication.

RADE startup follows accepted stream bindings and worker attachment, restoring
the explicit saved receive-audio owner, including B when A owns TX. Other
saved RADE modes retain their mode metadata without competing for the single
receive target. The current TX-bound-only retroactive creation does not meet
this contract. Reuse existing per-slice generation/admission guards; rejected
or retired decoders cannot feed audio. If resource admission refuses the saved
owner, report that refusal; do not silently hand its role to another receiver.

Stable IDs also matter when A is absent. The current initial DSP block creates
channel 0 and seeds it from the first list entry. Restoration must instead
associate that channel with ID 0 only, keeping it inactive when no such receiver
is bound. The normal channel pool can still contain a dormant channel 0.

Core exposes a read-only restoration state and actionable detail through the
station mirror. These are distinct from disk-save errors. The GUI uses them
after snapshot reconciliation to explain missing receivers/empty pans, and
must not recreate a rejected receiver automatically.

### Passive model hydration interface

`SliceModel::restoreReceiveState(double frequencyHz, DSPMode mode)` restores
legacy preferences and then seeds manifest tuning without signals, settings
writes or decoder activation. It requires an offline Local RadioModel parent
with no initialized DSP or RX/RADE channel. The manifest bypasses a persisted
frequency lock. A compatible legacy filter remains usable when its valid
stored mode matches; otherwise only the manifest mode's filter or its existing
default is accepted.

`RadioModel::hydrateReceiveLayout(const QString& radioMac,
const ReceiveLayoutStore::LoadResult&, QString* error = nullptr)` validates
identity and the complete loaded record before reconciling objects. It also
refuses existing stream bindings. Shared IDs retain their QObject; missing
IDs are seeded before `sliceAdded`, and extras retire without persistence.
Descriptor order and active identity remain distinct. The final
`receiveLayoutHydrated` notification requests a complete settled snapshot,
not duplicate create events for retained objects.

`receiveLayoutPendingAdmission()` remains true afterward and suppresses both
scheduled and forced legacy saves. `restoredRadeReceiveOwner()` preserves the
explicit receive target independently of active/TX selection. These are
startup preparation interfaces. DaemonApp now calls them before bootstrap or
first-radio discovery; runtime admission, snapshot reseeding and writeback are
integrated in the subsequent runtime checkpoint.

### Runtime integration

Daemon startup opts into per-radio layout capture. Empty identity stays pending
without reading a guessed namespace; repeated discovery of the selected MAC
never reloads disk over live edits. Loaded membership suppresses both configured
slice-count top-ups. If every descriptor is refused, ordinary configured
fallback membership is allowed while the original record remains protected.

Admission checks stable IDs against the board's actual channel range. Each
pan receives its own stream. A later member rejoins a stream its own pan
already holds when that window covers it, and otherwise takes a new stream,
exactly as recovery places it; a window belonging to another pan never absorbs
it. (Amended after the 2026-09-22 native check: requiring later members to fit
their pan's first stream refused a 40 m RADE receiver that live use had put on
a 20 m pan, because a pan move keeps the moved receiver's own stream.) A member
is refused only for an unsupported ID or when no stream is free. Refusals name
the receiver letter, frequency and mode in plain words and end once with "Your
saved layout is kept."; an accepted restore reports no message. Invalid/degraded
records remain protected from automatic capture. Accepted layouts use the
existing coalesced atomic AppSettings save, retry/error surface and immediate
shutdown flush.

The saved RADE receive owner is activated after channels and workers exist.
Reconnect refreshes its worker generation without duplicating channel wiring.
Live capture requires an explicit owner whenever any RADE modes remain: the
store's legacy single-mode inference must not promote a different receiver
after the operator removes the current owner. That unresolved capture reports
a settings-save error and retains the previous manifest.

The station sends a full same-session snapshot after passive hydration. The
client adopts retained identities, bypasses the operator frequency lock only
for authoritative station values, and emits a separate state-snapshot signal
without repeating the session/media handshake. Real operator edits during a
reseed are coalesced until its completion marker; inbound changes never echo.
Optional restore status is reset at a new attach so an older peer cannot inherit
the previous station's warning. Core connection details retain refusal reasons
while the session is connected; the GUI also warns when restoration is refused.

The existing active receiver identity is preserved if it survives hydration.
Descriptor order and active focus are deliberately separate; the manifest does
not persist or invent a new active/TX selection.

## Implementation and verification

- [x] Add the bounded manifest codec/adapter and named receive limits. Own:
  `ReceiveLayoutStore.{h,cpp}`, the constants in `SliceModel.h`, build wiring,
  and `tests/tst_receive_layout_store.cpp`. Verify real settings-file save/load
  across new AppSettings instances, noncontiguous IDs, normalized/mismatched
  MACs, removed membership, shared pans, corrupt/unsupported schemas and
  non-destructive validation failure. The adapter alone is not runtime
  restoration. Completed with explicit RADE receive-owner preservation;
  [verification](2026-09-20-remote-daemon-r3-verification/receive-layout-store.md)
  records 41 Qt checks and the fresh 719-test full suite.
- [x] Implement side-effect-free Local-model hydration, delayed per-slice
  runtime admission and save suppression. Own: `RadioModel`, `SliceModel` and
  their focused tests. Verify no offline RADE creation, no bootstrap save,
  actual resource rejection, distinct-pan stream allocation, and the saved
  RADE receive owner's activation and retirement. Include membership
  without ID 0 and assert no phantom active channel/mixer enrollment.
  These paths pass focused native/lifecycle checks and the full run's
  receive-layout tests. The full 722/724 result retains two native capture-open
  timeouts, and hardware acceptance remains open; see the
  [runtime evidence](2026-09-20-remote-daemon-r3-verification/receive-layout-runtime.md).
- [x] Wire `DaemonApp` startup, first discovery, configured-count precedence,
  coalesced capture and immediate shutdown flush. Verify two DaemonApp
  lifetimes with an actual settings file: cfg=1 restoring two slices;
  noncontiguous IDs; deleted B staying deleted; MAC mismatch; pending offline
  listener; changed capacities; failed save retry; first-responder selection.
- [x] Mirror restoration status and reconcile GUI membership/pan explanation.
  Verify authenticated snapshot/reconnect and stale media/control retirement,
  without client-side addSlice repair. Finalize read-only state fields and UI
  consumer together; protocol compatibility must be explicit.
- [ ] Run one integrated persistence/lifecycle review, affected checks and the
  repository's required final full suite. Retain valid evidence for unchanged
  native DSP. Then perform the operator's two-pan receive-only Core restart
  checkpoint on the Rock; hardware acceptance remains pending until observed.
  The consolidated review and required unfiltered run are recorded; the two
  native input-open timeouts keep the full-suite acceptance gate open.

No public push, hardware deployment, RF/tuner actuation, pairing/interlock or
network configuration change is part of this source checkpoint.
