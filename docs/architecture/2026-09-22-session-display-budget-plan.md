# Session display budget implementation plan

> Execution: yonder-cost-aware-execution. Requirements and acceptance criteria
> are binding; test order and review effort follow the Yonder risk-based policy.

**Goal:** wire the agreed active-pan policy to an advertised, independently
enforced Core display limit without disrupting audio or pane identity.

**Spec:** [session display budget design](2026-09-22-session-display-budget-design.md).
Parent requirements: architecture §§4.5/9.2/9.4/9.5 and R3 Task 6;
R-R3-03/08/09/20/28/30/35/37. Operator approved the recommendation and proposed
behavior on September 22; continue under the existing implementation mandate.

**Architecture:** one shared accounting contract, Core-owned reservation and
pacing state, GUI-owned quality allocation, and reliable allocation outcomes.
**Stack:** existing C++20/Qt6, display codec, media transport and settings;
no new dependency, thread or radio/DSP algorithm.

## Constraints and review focus

- Measured production byte/sample limits remain hardware evidence, never test
  constants. Configuration support with no enabled measured limit is not a
  completed deployment or capacity claim.
- Preserve independent trace/waterfall/wide planes, shared FFT ownership,
  latest-only spectrum input, CTUN/zoom/history behavior and logical panes.
- Counts cover all display-channel bytes including PS3; Opus remains independent.
  They do not claim encrypted wire bandwidth or actual CPU consumption.
- Budget rejection, missing acknowledgment and stale callbacks must not release
  unknown capacity, erase a newer widget or produce automatic retry loops.
- A failed replacement preserves accepted resources. Runtime limit decreases
  allow reductions while refusing increases until the ledger fits again.
- Source-first/provenance and signed-commit hooks remain binding. No public
  push, deployment, GUI relaunch or hardware actuation is part of this source
  increment. The existing operator smoke checkpoints remain pending.

Review the integrated boundary once for (1) replacement churn defeating limits,
(2) failed/reentrant sends changing lifetimes, (3) PS3 partial-frame starvation,
(4) older peers or malformed descriptors bypassing accounting, and (5) source
rollback and quality changes disrupting audio/history. Tests below cover each.

## 1. Shared accounting and monotonic pacing

**Files:** new `src/core/session/media/DisplayBudget.{h,cpp}` and
`tests/tst_display_budget.cpp`; root owns registration in both CMake files.

**Interfaces (namespace NereusSDR):**

```cpp
struct DisplayBudgetLimits {
    quint64 applicationBytesPerSecond = 0;
    quint64 spectrumSampleUnitsPerSecond = 0;
    quint32 generation = 1;
    bool isValid() const;
    bool operator==(const DisplayBudgetLimits&) const = default;
};
struct DisplayBudgetCharge {
    quint64 applicationBytesPerSecond = 0;
    quint64 spectrumSampleUnitsPerSecond = 0;
    quint32 messagesPerSecond = 0;
    bool operator==(const DisplayBudgetCharge&) const = default;
};
struct SpectrumDisplayCost {
    DisplayBudgetCharge charge;
    quint32 maximumFrameBytes = 0;
    quint32 maximumFrameSampleUnits = 0;
};
std::optional<SpectrumDisplayCost> spectrumDisplayCost(int pixels, int fps,
                                                     bool includeWidePlane);
DisplayBudgetCharge ps3DisplayCharge();
std::optional<DisplayBudgetCharge> sumDisplayCharges(
    const QList<DisplayBudgetCharge>& charges);
bool displayChargeFits(const DisplayBudgetLimits&, const DisplayBudgetCharge&);
```

Expose source-derived shared constants for the 5 ms sender interval, 200
messages/s, maximum spectrum frame, maximum PS3 chunk and existing 100 ms PS3
poll interval. Derive PS3 size from existing `Ps3Snapshot`/`Ps3DisplayCodec`
public bounds; do not port or duplicate vendor algorithms.

`DisplayBudgetPacer` exposes `bool beginSession(quint64 epoch, limits, nowNs)`,
`void endSession()`, `bool update(limits, spectrumCharge, ps3Enabled, nowNs)`,
`bool canSpendSpectrum(bytes,samples,nowNs)`,
`bool spendSpectrum(bytes,samples,nowNs)`, `bool canSpendPs3(bytes,nowNs)` and
`bool spendPs3(bytes,nowNs)`. Byte/sample arguments are quint64, time qint64.
Reject epoch zero, invalid limits/charges and backwards or repeated epoch
initialization without changing state; ending a session does not permit that
epoch to mint a fresh burst. Only a new epoch initializes full burst credit.

- [x] Implement checked costs/sums and structural slot admission.
- [x] Implement global-byte, spectrum-byte, PS3-byte and spectrum-sample
  credit; refill under old rates before changing rates. Fixed burst capacities
  never depend on repeated subscriptions. Spending is atomic across buckets.
- [x] Prevent spending for an unconfigured/inactive class. Preserve credits
  across all updates, including PS3 disable/re-enable and zero spectrum demand.
- [x] Verify actual codec keyframe/chunk lengths against the formulas and
  interval bounds against recorded nonempty send costs, including long idle,
  limits near integer range, backwards clocks and failed-attempt accounting.

**Verification:** deterministic calculator/pacer tests, plus codec output as
independent evidence. Terra is suitable for this bounded unit; it can run
alongside the lead's configuration/protocol work after these interfaces freeze.
Focused calculator/pacer test passed September 22; real-controller and hardware
enforcement evidence remains in tasks 3 and 5.

## 2. Configure and negotiate one authoritative descriptor

**Files:** `DaemonConfig.{h,cpp}`, `DaemonApp.cpp`, `packaging/nereusd.conf.sample`,
`StationCapabilities.{h,cpp}`, `SessionMessages.h`, `StationClient.{h,cpp}`,
`StationServer.{h,cpp}`, corresponding config/capability/session tests.
**Dependency:** task 1 values; advertised support is enabled only with task 3.

- [x] Parse optional paired keys `display_application_bytes_per_second` and
  `spectrum_sample_units_per_second`. An explicitly malformed/partial pair
  fails validation; absent keys preserve legacy behavior with no claimed cap.
- [x] Pass one descriptor before listening. Server owns the current validated
  value; controller enforcement reads that same owner. Generation advances for
  effective-limit changes and is advertised atomically with both values.
- [x] Allocate protocol minor 7 and `remoteDisplayBudgetVersion=1`, checking
  current source before allocating. Client exposes optional validated limits
  and observes changes; invalid/partial/duplicate typed fields cannot produce
  a usable descriptor. Older peers preserve their shapes and explicit fallback.
- [x] Advertise current remote PS3 accepted reservation separately so the GUI
  can subtract it without guessing station state; retain independent codec gates.

**Verification:** actual configuration consumers, sample/parser-key parity,
new/new and old/new negotiated sessions, first-client race, malformed signed
and oversized integers, descriptor changes and stale-epoch callbacks.

## 3. Enforce at Core's real admission and send boundary

**Files:** `DaemonMediaController.{h,cpp}`, `StationServer.{h,cpp}`,
`SessionCommandDispatcher.{h,cpp}`, `PureSignalSessionFacade.*` only as needed
for accepted-subscription notification; daemon media/PS3 integration tests.
**Dependencies:** task 1 and server descriptor interface from task 2.

- [x] Keep accepted reservation accounting transactional with existing source
  and wideband replacement/rollback. Apply configured caps to older clients too.
- [x] Return immediate revisioned allocation results; retain the later render
  context contract. Revisioned unsubscribe and duplicate/stale operations must
  not release a newer reservation. Results report current retained charge.
- [x] Install the injected monotonic pacer at the production sender. Hold the
  latest input until credit exists, debit before transport, and never refund a
  failed attempt. Maintain the epoch boundary across production/peer changes.
- [x] Admit remote PS3 before mutating the facade. Replace unconditional PS3
  priority with class scheduling and bounded current-plus-latest chunk state.
- [x] Preserve RTP capture/encoding/send independently and report refusals.

**Verification:** real-controller replacement-abuse regression, exact combined
byte/sample interval bounds, all lifecycle resets, source rollback, PS3 enable
refusal, delayed multipart completion/failure, and RTP under exhausted display
credit. Use injected time/transport and actual codecs, not native radio guesses.
Sol is suitable; shared server/schema edits must be serialized with task 2.

## 4. Allocate GUI quality and acknowledge resource transitions

**Files:** new `src/gui/RemoteDisplayAllocator.{h,cpp}`, `RemoteMediaController.*`,
`StationClient.*` PS3 intent routing, existing pane status/telemetry presentation,
and focused allocator/media/controller/widget integration tests.
**Dependencies:** exact capability/outcome shapes and task 3's authoritative state.

- [x] Implement the pure active/background policy in the design, using stable
  pan IDs and explicit intent; include accepted/requested PS3 display demand.
- [x] Separate desired, pending and accepted allocation/render state. Send
  reductions first, wait for revisioned outcomes, then fitting increases.
  Coalesce changing focus/geometry without discarding outstanding accounting.
- [x] Mediate remote PS3 intent before its existing station command: reduce
  spectrum first when needed, then request PS3. Core refusal remains decisive.
- [x] Preserve widget retirement/reentrancy guards; keep bounded resource-only
  tombstones and reject stale epochs/contexts. Do not reset CTUN or history.
- [x] Show accepted quality, delivered rate and reduction/suspension reason.
  Handle the injected 10-second acknowledgment deadline without assuming a
  release or interrupting healthy audio; existing explicit reconnect remains.

**Verification:** four/floating/shared-source panes, stable ties, restore on
headroom return, PS3 coexistence, delayed/refused/missing replies, focus changes
mid-phase, retirement during send, old peer fallback and real graph/status text.

## 5. Integrated verification and hardware acceptance

- [x] One consolidated review of the combined protocol/pacing/lifecycle change;
  resolve actionable findings and rerun affected checks.
- [x] Build `NereusSDR`, `nereusd` and `all_tests`, then unfiltered
  `ctest --test-dir build-integration -j6 --output-on-failure`. Record host load.
- [x] Save signed local checkpoint `fc129a39` with required hooks.
- [ ] Refresh and verify the matching binaries’ post-checkpoint build tag.
- [ ] When installation/hardware access is authorized, measure the design's
  Rock 5C workloads and select enabled production values from evidence. Repeat
  the independent Pi 4 floor obligation; do not infer it from Rock results.
- [ ] Receive-only operator smoke: one/four/floating pans, change active pan,
  tune/zoom, close/reopen and reconnect. Expect smooth audio, retained history,
  explained quality changes and restoration. Complete the separate two-hour
  listening/thermal/packet-size checks before declaring capacity complete.

## September 22 software evidence

Shared accounting, configuration/negotiation, Core enforcement and GUI allocation
are implemented. Eleven focused targets passed; Core's controller passed all
31 Qt cases, including source-update failure and retirement recovery. GUI's
controller passed all 23 Qt cases after its PS3 fixture was corrected to create
byte pressure and wait for the queued recovery acknowledgment. Those fixes did
not change production limits or weaken the required restoration behavior.

The consolidated source review found and resolved four defects: a reservation
left behind when a pane retired during subscribe; missing authoritative release
on source retirement; retained allocation after a failed source update; and an
inconsistent replay result after retirement. Regression cases cover each. No
blocking source finding remains. Matching `NereusSDR`, `nereusd` and `all_tests` built successfully. The
unfiltered full suite passed **730/730 in 286.68 seconds**; signed checkpoint
and hardware acceptance are recorded separately.
See [verification evidence](2026-09-20-remote-daemon-r3-verification/display-capacity.md).
