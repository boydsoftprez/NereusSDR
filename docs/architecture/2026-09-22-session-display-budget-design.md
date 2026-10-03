# Core / GUI session display budget

Status: the operator approved the recommendation and proposed behavior on
September 22 after clarification of the term "budget". This means performance
capacity for displays, not a usage allowance. Production limits remain subject
to measurement; this document alone implements no allocation or pacing.

Execution: yonder-cost-aware-execution under the existing implementation goal.
Use its risk-based verification and one consolidated integration review.

## Purpose and existing decisions

Keep multiple docked and floating pans usable on a constrained Core or link,
without letting display demand consume the audio/control allocation. The GUI
divides Core's advertised capacity among logical panes; Core independently
admits requests and enforces actual output. The operator can see requested,
accepted and delivered quality and why a pane was reduced or suspended.

This implements the display-budget portion of parent architecture §§4.5,
9.2, 9.4 and 9.5 and R3 Task 6. Preserve R-R3-03/08/09/20/28/30/35:
independent audio transport, shared FFT ownership, pane lifetime, wideband
coverage, session recovery, widget retirement and honest telemetry. It does
not close the parent hardware-capacity, PerfMonitor, codec accuracy,
two-hour listening or packet-size acceptance requirements by itself.

Existing decisions remain binding: logical pane membership rather than
`isVisible()`, latest producer frame rather than a catch-up queue, independent
trace/waterfall planes, GUI-owned view geometry, Core-owned shared FFTs, and
explicit retirement. Allocated output pixel count is independent of widget
width: Core may send fewer samples without resizing the pane. Desired quality
continues to use the existing width-derived request and responds to layout
changes; this adds no separate user pixel setting. A budget adjustment must not
tune the radio, change a slice, alter CTUN or wipe painted waterfall history.

## Alternatives and recommendation

1. **GUI allocation plus Core admission and persistent pacing — recommended.**
   The GUI can explain each pane's quality, while Core remains authoritative
   even if the client sends invalid or rapidly changing requests.
2. **Admission alone.** Too weak: `SpectrumEndpoint::configure()` resets frame
   cadence. Replacing a 1-FPS endpoint on a shared 60-FPS source can repeatedly
   obtain a fresh first frame without increasing its nominal reservation.
3. **Core silently reduces every endpoint.** Bounds traffic, but leaves the
   GUI unaware of accepted quality and cannot implement GUI focus priority.

Use independent application-byte and spectrum-output-sample ceilings. A
sample-unit ceiling expresses the parent's aggregate pixel-rate requirement;
it is not an estimate of FFT CPU, GPU work or overall system capacity.

## Capacity descriptor and accounting

Introduce a shared `DisplayBudgetLimits` value and a checked reservation
calculator under `src/core/session/media/DisplayBudget.{h,cpp}`. Core owns the
value; advertisement and enforcement consume the same validated descriptor.

| Field | Meaning |
| --- | --- |
| `applicationBytesPerSecond` | Total attempted display-channel application bytes per second, including spectrum and PS3; excludes RTP audio, control and encrypted/wire overhead |
| `spectrumSampleUnitsPerSecond` | Spectrum trace, waterfall and optional wide-plane samples produced per second, counted separately even when two planes have equal dimensions |
| `generation` | Nonzero serial descriptor generation within a station session |

For a plane with `n` samples, the current display codec's worst case is
`A(0)=0`, `A(n)=3+5*ceil(n/128)+n` for `n>0`. The frame header is 42 bytes.
For requested pixels `P`, target FPS `F`, and wide-span factor `G`, use:

```text
D = 768 if G > 1, otherwise 0
frameBytes = 42 + 2*A(P) + A(D)
frameSampleUnits = 2*P + D
reservedBytesPerSecond = F * frameBytes
reservedSampleUnitsPerSecond = F * frameSampleUnits
```

The codec selects the cheapest of its supported block sizes; its 128-sample
absolute representation bounds delta frames and is attained by a keyframe.
The current endpoint maximum is 9,361 bytes and 8,960 sample units per frame.
These are format bounds, not measured sustainable operating rates.

Charge the request before source geometry is available. A later bin-count
clamp may reduce actual output but does not implicitly refund the reservation.
Neither `framesPerLine` nor an unchanged waterfall row removes its transmitted
plane. Shared-source FFT cost is not added once per endpoint: retain the
existing maximum size/FPS aggregation per `(streamIndex, tier)` and its source
activation/refusal boundary.

Use checked integer arithmetic. Capability values use Int64 wire entries and
must be positive, complete and within the exact JSON integer range. Unknown
fields stay forward compatible; a partial or invalid budget descriptor must
not be mistaken for an authoritative zero or an unlimited advertised budget.

## Agreed GUI quality policy

Favor the active pan. Preserve all requested qualities when they fit. On
pressure, lower background FPS first, then background pixel counts; only then
lower the active pan's FPS and pixel count. Within a background phase, reduce
one unit per pane in stable pan-ID order, repeating until both budgets fit or
every affected pane reaches its floor. Recompute from current intent after
each focus/layout/geometry/limit change so capacity recovery restores quality.

Minimum useful quality is `min(requestedPixels,256)` by
`min(requestedFps,10)`. The 256×10 value comes from the parent's illustrative
background-pane example and is an **operator-approved UX floor**, not a capacity
measurement or a replacement for the user's normal settings. At most eight
endpoints and bounded pixel/FPS ranges keep the pure allocation calculation
bounded; no iteration sends a network request.

If all floors do not fit, suspend background endpoints in reverse stable
pan-ID order and recalculate the remaining panes. Keep the active pane first.
If even its floor cannot fit, suspend that display with an explicit capacity
reason. Suspension keeps the pane and receive slice; it releases only the
display endpoint. Frozen history must be visibly identified as paused rather
than presented as live. A user switching focus causes a fresh allocation.

Do not silently remove a requested 3D wide plane or independent waterfall
plane to fit. Preserve the requested FFT resolution, crop, detectors and
averaging. Recompute frames-per-line from the existing requested waterfall
period and allocated FPS using the current `ceil(periodMs*fps/1000)` rule.
This first allocation policy does not claim to implement the parent's later
PerfMonitor waterfall/degradation steps.

## Core admission and actual output enforcement

An endpoint owns one reservation for its latest accepted request. Replacement
uses `currentTotal-oldReservation+newReservation`. Validate budget, source
configuration and wideband demand transactionally: refusal retains the old
endpoint, source ownership, reservation and usable context. Unsubscribe retires
the reservation with the endpoint. Radio loss can retire production, but
cannot mint send credit within the same authenticated station session.

Admission is necessary but insufficient. Maintain monotonic-clock byte and
spectrum-sample credit at session scope, surviving endpoint replacement,
context/keyframe changes, source reconfiguration, radio production restart
and media-peer replacement within that epoch. A new authenticated epoch
receives fresh credit once. Credit remains bounded by the fixed burst capacities
below. Decreasing a limit changes future refill; increasing it does not
immediately refill the bucket.

Spectrum sample capacity/initial credit is one maximum legal spectrum frame,
8,960 units. Global byte capacity is one maximum legal display message,
65,536 bytes; the larger PureSignal chunks determine this bound.
For byte rate `C`, capacity `M`, and every interval `t`, attempted payload
must obey `bytes <= C*t+M`; apply the corresponding inequality to spectrum
sample output. Use checked fixed-point refill, capped before multiplication
can overflow; non-advancing clock readings never earn credit.

Check worst-frame credit before consuming/reducing/encoding a candidate. If
insufficient, retain its latest input and let a newer producer frame replace
it. Do not advance codec sequence or delta history for a throttled frame.
Debit actual encoded bytes and actual plane sample counts before the transport
call. An encoding exceeding the verified request bound is an invariant failure
and must not be sent. Never refund a nonempty send attempt: a false transport
return can already mean buffered bytes. Retain existing force-next-keyframe
behavior without retrying the failed sequence.

Audio capture, encoding and `sendRtp()` never consult these display buckets.
This protects allocation separation; real CPU and link contention still require
the hardware acceptance below.

## PureSignal display sharing

The same display channel carries `PS3D` as well as spectrum `NSDC` frames.
Reuse `Ps3Snapshot`'s public sample/correction maxima and `Ps3DisplayCodec`'s
64-byte header and 65,536-byte chunk limit. Four sample arrays and four
correction arrays of doubles give `32*(S+K)` payload bytes. At the existing
maxima `S=4096`, `K=512`, a full snapshot is 147,648 application bytes in three
chunks. The existing 100 ms producer cadence therefore reserves 1,476,480
bytes/s and 30 sender slots/s while remote Amp View is subscribed; zero when
it is not. These are format/cadence facts, not a selected machine capacity.
Keep local-only Amp View demand outside the remote reservation.

Spectrum admission plus PS3 must fit the common byte limit and the current
one-message-per-5-ms structural sender envelope (200 messages/s). PS3 is not
charged to the spectrum sample ceiling: its arrays are a different workload.
Its codec capability remains separate from display-budget capability.

Pace display with a global byte bucket at the budget's byte limit and one
maximum display message (65,536 bytes) of burst, plus class buckets for
spectrum and PS3. The global bucket bounds every burst: all display traffic
still obeys `L*t + 65,536`.

- PS3 is paced at its reservation (one worst-case snapshot per 100 ms poll)
  with one whole worst-case snapshot (147,648 bytes) of burst. With room for
  only one chunk its bucket filled while a chunk waited for the sender and
  lost that credit: it slipped one 5 ms tick per cycle and delivered 28.6 of
  its 30 chunks a second, about one snapshot in twenty overtaken by the next.
  The rate is unchanged, so the reservation still bounds it.
- Spectrum is paced to the budget less PS3's reservation while remote Amp
  View is subscribed (the whole byte budget when it is not), and to the
  budget's sample ceiling, with one worst-case spectrum frame of burst; never
  below the admitted spectrum charge. Pacing to the admitted charge refilled
  exactly one frame per 5 ms tick at the computed ceiling, so a tick that
  came early lost its frame and eight wide pans received about three
  quarters of what legacy mode sends. Every class still stays within the
  limits, and each endpoint's own cadence keeps it to its admitted frame
  rate.

Maintain all credit across subscription changes; changing a class's rate
cannot refill it. Each 5 ms tick sends at most one message and alternates
which class it tries first; it tries the other class in the same tick when
the first has no eligible work, so PS3 cannot starve the active pan.

Spectrum picks its endpoint by the session's mode:

- A budget session sends the endpoint whose held frame stops being worth
  sending first (earliest deadline). Admission keeps the planned load within
  the sender's 200 messages a second, so only the order decides whether each
  pan gets its planned rate. At the computed ceiling the app plans exactly
  200 (the active pan at 60 fps, seven more at 20), and round robin let the
  background pans, which fall due on the same source frame, push the active
  pan's frame out one time in three (46.6 of 60 fps). The deadline is
  counted in whole source frames, so endpoints of one source that fall due
  together tie exactly; a tie goes to the faster endpoint (the app plans its
  active pan fastest), then to round-robin order.
- A session without a budget (an older app) keeps endpoint round robin.
  Nothing limits what it asks for, so it can ask for more than the sender
  carries, and there earliest deadline starves pans on different sources: a
  pan whose source frame falls due earlier keeps winning, and eight pans on
  two sources received 60/60/0/0/60/20/0/0 fps. Round robin gives each its
  even share (25 fps each of the 200).

Target FPS remains a target, not a delivery guarantee; hardware acceptance
must check starvation and responsiveness.

Pin a PS3 snapshot once its first chunk is attempted. While completing it,
retain only one replacing latest snapshot. Before the first chunk, a newer
snapshot may replace it. Failure drops the remaining chunks without retry or
credit refund. This bounds storage to current plus latest and lets a paced
multi-chunk snapshot complete instead of being replaced forever.

Admission must precede the station's `setRemoteAmpViewSubscribed()` mutation.
The GUI records requested PS3 display intent, reduces spectrum allocations and
waits for their outcomes before submitting `ps3.subscribeDisplay`. A refusal
keeps the previous accepted PS3 state and reports its reason. If the fixed
PS3 reservation alone exceeds the available byte/slot limit, report that it
cannot fit without changing any pan or station subscription. Core rechecks
every PS3 command independently, including commands from an older GUI.

## Allocation protocol and GUI lifecycle

Reserve the next available protocol minor (7 at `c794c52e`) and capability
`remoteDisplayBudgetVersion=1`. Add the complete limits descriptor to station
capabilities. Recheck that minor before implementation; do not reuse an
unrelated feature's gate. All updated clients and Core use one formula module.

Add an immediate `allocation-result` for subscribe and unsubscribe with
`connectionId`, `endpointId`, nonzero request `revision`, `accepted`, `reason`,
current `budgetGeneration`, `acceptedRevision` (zero when no endpoint remains),
and retained endpoint charge fields `applicationBytesPerSecond`,
`spectrumSampleUnitsPerSecond` and `messagesPerSecond`. Charge excludes PS3,
whose accepted subscription is advertised separately. All numbers are exact
integers within the shared bounds; no endpoint means revision and all charges
are zero.
Subscribe's later `context` is still required before decoding its new media;
an allocation result is not a rendering context or proof of a working source.
New-version unsubscribe includes a revision. Duplicate operations return their
bounded cached outcome without reconfiguring a source. Stale operations cannot
remove or resolve a newer endpoint revision. Wire endpoint IDs increase within
a media peer and are not reused after unsubscribe. Core keeps live operation
records and at most 64 recent nonlive records; a high-water ID rejects obsolete
unknown IDs after cache eviction. This is a memory bound, not a capacity
measurement. Replies always describe the current retained reservation.

GUI state separates desired request, pending request, accepted reservation and
accepted render context. For each allocation generation:

1. Freeze a complete fitting target from logical pane intent and active pan.
2. Send retirements and replacements that reduce reservations.
3. Wait for their matching results. Recalculate headroom from accepted state;
   never release capacity merely because an unsubscribe was sent.
4. Send increases/new endpoints only when they fit that confirmed headroom.
5. Commit quality display from accepted outcomes; wait for the matching context
   before replacing the decoder. A later intent supersedes the target, not
   the resource ledger for operations already sent.

Temporary reduced display quality during focus handoff is acceptable;
audio continues. A refused reduction retains the old charge and
blocks dependent increases. A refused increase keeps the last accepted
quality, or leaves a new pane explicitly unstreamed. Do not retry the same
refusal on every subscription poll. Geometry changes keep their existing
staleness guards; an allocation-only renewal preserves painted history.

Retire widget bindings before any send that can synchronously close/delete
the controller. Pending retirements keep only a bounded resource tombstone,
never a widget pointer or callback. Outcomes are checked against current
station epoch, peer/connection identity, endpoint and request revision. A
superseded result may reconcile outstanding resource accounting, but cannot
install stale geometry or mutate a replacement widget. Clear the ledger on
authenticated session retirement and allocate anew after snapshot readiness.

The acknowledgment deadline is 10 seconds, injected in tests. Expiry
exposes a stalled allocation and blocks dependent increases; it never assumes
resources were released or disconnects healthy audio automatically. Existing
explicit Connect/Disconnect recovery remains available. A late valid result
reconciles its operation and replans current intent. This deadline is a new
control policy choice, not a claimed measurement of the network.

## Runtime limits, compatibility and configuration

Core installs one validated descriptor before the station listener opens.
Add daemon configuration keys for display application bytes/s and spectrum
sample units/s together with their actual production consumers and sample-file
documentation. Both are required to enable this descriptor. Reject a partial,
zero, negative or malformed explicitly supplied pair as a configuration error;
never silently fall back to unlimited output from an invalid requested cap.
An absent pair preserves legacy behavior and explicitly advertises no budget
support until a measured configuration is selected. This is an intermediate
compatibility state, not completion of Task 6 or a default capacity claim.

Normal reception in this compatibility state has no pan-overlay notice. The
absence of a descriptor is diagnostic information, not an operator failure.
Log it when the media connection becomes ready. Retain visible status for
actual reduced, paused, stalled or refused displays. This presentation correction
follows the September 22 operator report that the capability terminology was
unhelpful and obscured the operating display.

A future/current effective-limit publisher changes the descriptor generation
atomically for advertisement and enforcement. A decrease takes effect at the
sender immediately. Allow genuinely reducing replacements even while retained
reservations exceed the new total; refuse increases until the ledger fits.
The GUI receives the new descriptor and runs the same reduction-first process.
This interface supports PerfMonitor; implementing the monitor and proving its
reaction thresholds remains a separate parent requirement.

Old GUI requests keep their old message shapes. When Core has configured
limits, it still enforces them and uses existing rejection messages for an
over-budget request; protocol downgrade cannot bypass the cap. Old clients
cannot automatically redistribute quality. A new GUI connected to an older
Core retains the existing endpoint behavior and reports that aggregate budget
support is unavailable. Do not advertise effective limits that Core ignores.

## Verification and deployment evidence

- Pure calculator/allocation tests: format-bound arithmetic, zero/invalid
  inputs, checked overflow, both resource ceilings, fitting all requests,
  focus changes, stable ties, floors/suspension and full restoration. Exercise
  both 2D and 3D, narrow-bin clamps and several endpoints sharing one FFT.
- Pure pacer tests with an injected clock: exact boundary, fractional refill,
  long-idle burst bound, backward time, decreasing/increasing limits and
  non-refunded failed attempts. A token algorithm test alone is insufficient.
- Production-controller integration: repeatedly renew a 1-FPS/4096-pixel
  endpoint sharing a 60-FPS source. Inspect actual transport attempts across
  intervals and prove both bounds survive reconfiguration. Verify latest-only
  delivery, decoder/keyframe recovery and continued RTP with display exhausted.
- GUI/Core integration: delayed/refused reductions, duplicate/stale/missing
  replies, retirement during send, endpoint-ID reuse, focus changes mid-phase,
  reconnect, source failure/rollback and new lower limits. Check the real
  allocator feeds subscribe values and does not merely update diagnostics.
- UI verification: requested/accepted/delivered quality and capacity reasons
  remain truthful; a budget-only change does not wipe history or move CTUN.
  Float/dock is still the same logical endpoint. Old-peer behavior is explicit.
- Build affected test targets before focused runs; then matching GUI/Core and
  `all_tests`, followed by one unfiltered full suite. Consolidate protocol,
  lifecycle, pacing and source-capacity review at the integrated boundary.
- Hardware acceptance remains open: measure full-span/noise FFT4096, deep
  zoom, both tiers, 2D/3D, one/four/floating pans and shared-source panes on the
  Rock 5C; repeat the floor obligation on Pi 4 Model B Rev 1.5 8GB. Measure
  actual wire overhead separately. Select and record sustainable byte/sample
  limits from these runs, with audio/control health and thermal behavior.
  Follow with the agreed receive-only operator smoke and two-hour audio soak.

## Source map

- `src/gui/RemoteMediaController.cpp`: requested quality, stable pane intent,
  revision/context handling and reentrant retirement.
- `src/core/session/media/DaemonMediaController.cpp`: transactional source
  replacement, delayed context, current send tick and PS3 priority.
- `src/core/session/media/SpectrumEndpoint.cpp`: actual plane bounds and the
  cadence reset that admission alone cannot constrain.
- `src/core/session/media/DisplayCodec.cpp`: 42-byte header, per-plane prefix,
  adaptive block encoding and absolute keyframe bound.
- `src/core/daemon/DaemonConfig.*`, `DaemonApp.cpp`,
  `src/core/session/StationCapabilities.*`, `StationServer.*`, `StationClient.*`:
  configuration, single limits source, capability negotiation and lifecycle.
- Parent: [remote daemon architecture](2026-07-28-remote-daemon-architecture-design.md).
  Work tracking: [R3 plan](2026-09-20-remote-daemon-r3-plan.md), Task 6.
