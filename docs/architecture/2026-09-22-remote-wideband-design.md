# Remote wideband extended-pan contract

Status: Task 4b implementation design, expanding the already-authorized R3
receive-parity scope. Source baseline: software `55e7d49f`. Hardware acceptance
is pending; the Rock became unreachable after the recovery installation.

Execution: yonder-cost-aware-execution. Requirements and acceptance criteria
are binding; test order and review effort follow the Yonder risk-based policy.

## Intended result and references

R-R3-20 restores the existing extended-view toggle and zoom gestures over a
remote Core: the listenable DDC island stays at its true RF position and ADC
survey data fills the surrounding wings. Zoom out, release, zoom in and wing
tuning must behave like the local implementation. Core owns capture, receive
filter policy and level normalization. The GUI renders accepted rows and
preserves history. No transmit or tuner actuation is introduced.

Binding references:

- [R3 plan](2026-09-20-remote-daemon-r3-plan.md), R-R3-04/08/09/10/11/14/18/19/20
  and Task 4b.
- [Local wideband plan](2026-05-26-phase3f-sub-epic-f-wideband-plan.md).
- [Parent architecture](2026-07-28-remote-daemon-architecture-design.md), sections
  9.4/9.5. The later R-R3-20 explicitly brings its formerly deferred ADC source
  into scope.
- [Display codec v1](2026-09-20-display-codec-v1.md).

## Source findings that constrain the implementation

`FftTier::Wide` is a full **DDC** FFT. The codec's optional `wideDbm` plane is
an off-screen crop from that DDC for 3D history. Neither is an ADC survey.

`WidebandFftEngine` captures 16,384 real samples, Hann windows them, zero-pads
to 65,536 and publishes 32,768 positive-frequency bins after dropping DC.
Bin spacing is ADC rate / 65,536; noise bandwidth uses the 16,384 capture
length and window ENB. Several comments still say 8,192; the implementation
and its executable size assertion already use the larger output. The output
is raw `10*log10(|X|^2)`, not antenna-calibrated dBm.

The local widget performs three separate operations:

1. Refer the real FFT peak to its Hann coherent gain.
2. Match the ADC noise bandwidth to the DDC bin reference. Peak/Rosenfell
   retain DDC window ENB; Average/Sample/RMS remove it in their detector.
3. Apply the existing station RX meter/display scalar after composition.

The first two are Nereus's existing relative display normalization, including
its known limitation that unresolved narrow carriers in the survey have a
different reference from resolved DDC carriers. They are not a new absolute
per-ADC calibration. `DaemonMediaController` currently obtains the scalar
from `RadioModel::rxMeterOffsetDb()` for every DDC frame. This work preserves
that established reference explicitly; it must not invent ADC1 calibration
or numerically compensate for a physical BPF.

The existing ADC geometry rate is the fixed 122,880,000 Hz reference used by
Thetis `wideband.Designer.cs:237-243` and `wbDisplay.cs:4511-4519`
(`v2.10.3.15`), also used at Nereus `RadioModel` engine construction. No P2
capture-rate field or later codec-rate update exists. Expose that configured
engine value through Core; do not substitute a DDC sample rate or describe it
as measured/negotiated. The context records its basis. Hardware RF placement
still requires the Saturn check before acceptance.

The local island clips 4% from each DDC skirt. Both detector cropping and
pixel geometry use that same clip. A view completely outside the island is
valid: ADC data owns every pixel. Trace and waterfall each compose their
linear row before their independent averaging stage. 3D receives that exact
waterfall row plus its existing optional DDC-only wider history.

Physical ADC and filter chain are distinct identities. Existing model APIs
`sliceAdcIndex` and `sliceChainIndex` expose them, but the current capture-enable
push passes a chain index to P2's ADC bit. The implementation must separate
capture demand by ADC from bypass demand by chain. A two-ADC/one-chain board
is the required regression case.

## Selected representation

Keep one display endpoint per visible pan. Add an explicit optional ADC
source to that endpoint's authenticated context. Core composes its DDC island
and ADC wings **before** per-plane averaging and sends the existing reduced
trace/waterfall planes. Codec v1's pixel-row meaning and bounded framing stay
unchanged; no raw ADC array crosses the link and no existing plane is renamed.

Alternatives considered:

- A separate ADC endpoint/codec would preserve independently arriving planes,
  but would move composition/averaging back into the GUI or change their
  order. It also adds a second set of pixels, pairing and loss recovery for
  each pan. That is unnecessary for local parity.
- Reusing `wideDbm` as ADC data would corrupt its DDC-only RF interpretation
  and 3D history. It is rejected.

The ADC source remains separately identified, generated and demand-managed
inside Core. Composition does not merge source lifetimes. A source or
calibration-basis change invalidates the composite context and codec predictor.
Ordinary changes of the existing RX meter scalar keep the current per-frame
DDC policy; they do not independently erase painted history.

## Core reference seam: first implementation checkpoint

Add `src/core/spectrum/WidebandDisplayReference.{h,cpp}` containing:

```cpp
float widebandFftNormalisationDb();
float widebandBandwidthNormalisationDb(double adcRateHz,
    double ddcBinWidthHz, double ddcWindowEnb, SpectrumDetectorMode detector);
float widebandRelativeReferenceDb(double adcRateHz,
    double ddcBinWidthHz, double ddcWindowEnb, SpectrumDetectorMode detector);
```

Move the existing arithmetic and explanations verbatim out of
`SpectrumWidget`; retain its public wrappers for callers/tests. The helper
contains no widget, session, hardware or settings dependencies. Inputs are
the already-validated source/window state. No DSP constants, defaults or
local rendering behavior change. `widebandRelativeReferenceDb` is deliberately
not named absolute calibration. A later endpoint adds Core's station scalar
once, and the remote renderer continues skipping its local scalar.

This checkpoint is independently useful and does not require changes to the
parallel WDSP/NNR/PS3 lifecycle work.

## Negotiation and context

Use a new named session-minor feature gate, allocated against the serially
integrated protocol constants. DSP/WDSP controls now occupy minor 5, so the
wideband gate must follow it; do not reuse the earlier minor-4 baseline. Advertise
`remoteWidebandDisplayVersion = 1` only with the implemented media controller.
Older peers retain the strict current subscription/context shape and DDC
zoom ceiling.

For the new feature, subscription adds an `extendedView` permission boolean.
Core resolves the slice's actual ADC, chain and configured geometry rate; the GUI cannot
choose another ADC by forging those numbers. Accepted context adds:

```text
wideband: {
  version: 1,
  available: boolean,
  active: boolean,
  physicalAdcIndex: integer,
  filterChainIndex: integer,
  sourceGeneration: nonzero uint32 when active, otherwise 0,
  adcRateHz: finite positive number when available,
  geometryRateBasis: "thetisLocalReference",
  lowHz: 0,
  highHz: adcRateHz / 2,
  levelReference: "localWingRelativeWithStationRxOffset"
}
```

Unavailable sources omit source-specific values rather than publishing
guessed geometry. `available` means Core has validated the concrete source
configuration and can accept extended zoom; it does not claim streaming.
`active` means the accepted view requires wings and owns capture demand.
This separation allows a first zoom without enabling BPF bypass merely
because the preference is on. Pending first capture paints no invented wing
signals. Activation, deactivation and source changes mint a fresh endpoint
context generation before any row with changed meaning is sent.

Connection UUID/session epoch, endpoint ID, subscription revision and context
generation retain their existing guards. ADC source generation is additional
identity, not a replacement for any guard. Radio teardown's existing atomic
wideband epoch remains checked both before FFT and before owner-thread
publication. A new local publication descriptor must attach physical ADC,
source generation, configured ADC geometry rate and monotonic production time
before the media controller consumes it. Invalid rate disables the offer.

## Composition, lifecycle and controls

Implement an `ExtendedSpectrumReducer` using the existing local pixel/FFT
clipping, peak-wing reduction and per-plane `SpectrumAvenger` order. Explicit
inputs include the two source geometries, raw ADC bins, DDC linear bins/window
reference, source generations, plane detector/averaging and missing-data floor.
Use the requested RF window for extended contexts; never stretch the clipped
DDC crop across the entire pan. Clamp numeric ranges before integer conversion.

Keep the DDC raw FFT offset and station scalar as separate reducer inputs.
Wing linear input removes only the DDC raw offset, then the shared final scale
adds that offset plus the station scalar. Removing an already calibrated DDC
offset from the wing would accidentally cancel the station scalar. Test a
nonzero scalar on both ADC identities and distinct trace/waterfall detectors.

Keep one latest ADC frame per physical source, not one queued copy per pan.
Reject obsolete publication generations on ADC mapping, capture disable,
radio replacement or source configuration change. First capture and source
loss are represented explicitly. Stale capture-age policy is a hardware
observation dependency: measure the configured P2 burst interval and missed
frame behavior before choosing a timeout; do not substitute DDC cadence.

Core holds remote capture requests by endpoint lifetime, ORed with local
slice demand. Enable the physical ADC mask independently of Alex chain bypass.
Two pans sharing a source keep it enabled until the last consumer retires.
Hide/remove/rebind/session retirement/radio loss release remote requests;
recovery must not resurrect old-session requests. New mirrored status reports
effective capture/filter state, not client intent.

The Core ownership seam is `RadioModel::acquireWidebandDemand(sliceId)`,
`setWidebandDemandActive(token, active)` and `releaseWidebandDemand(token)`.
Acquisition returns an inactive, nonzero owner token bound to the actual slice
object. Core resolves that slice's current published ADC and filter chain;
callers cannot supply either routing index. Multiple owners and the existing
local slice request are ORed into separate physical capture and chain bypass
arrays. Tokens retire on slice removal, radio loss and teardown; token numbers
are never reused during the model's lifetime. Deactivation hides wings without
destroying an endpoint; release is idempotent and retires its token.

Reconciliation retries after reentrant filter observers change ownership,
before sending an obsolete capture snapshot. Physical ADC mask changes remain
marshalled to the P2 connection thread. The endpoint controller must acquire,
activate and release these tokens at the context/lifetime boundaries above;
the model seam alone does not establish remote endpoint integration.

The GUI uses accepted source availability for the existing zoom ceiling:
`max(ddcRate, adcRate / 2)` only when permission and source availability hold.
Rejected/unavailable contexts retain the existing DDC fallback. An in-flight
zoom retires live planes but preserves painted RF history, as ordinary remote
zoom already does. Wing release uses the existing DDC-retune command path;
ordinary pan dragging and C-Tune dragging remain distinct. Command rejection
restores the last accepted geometry and respects cohost slice constraints.

## Bounds and remaining integration dependencies

Composition reuses the same accepted pixel count/FPS and existing three-plane
codec limits (4,096 samples/plane, DDC 3D row capped at 768, 16 KiB logical
frame). A wider RF view does not add a fourth plane or second stream of display
packets. Hidden endpoints must retire their demand.

There is currently **no explicit session-wide display allocator**. The
8-endpoint admission cap and round-robin sender are not that allocator.
R3 Task 6 must advertise/enforce a total budget, allocate focused/background
quality, and measure one/four/floating-pan cases. Its allocation applies to
these composite rows unchanged. Do not mark R-R3-08 or full Task 4b hardware
acceptance complete before that work passes.

The configured libdatachannel MTU is 1,000 bytes; codec message size is not
wire datagram size. Record actual encrypted UDP sizes, including overhead,
for keyframes and deltas. A failing capture is an implementation defect to
resolve before hardware acceptance, not a reason to relax the datagram cap.
Reuse the existing [installed display evidence](2026-09-20-remote-daemon-r3-verification/README.md#installed-display-checkpoint-ea55d24a):
its 20-second Saturn run carried 1,024 trace + 1,024 waterfall + 768 DDC-wide
samples at 15 fps with a maximum IPv4 packet of 969 bytes. That establishes
this measured profile, not total-session, four-pan or SRTP acceptance. Broaden
the capture for materially different combined workloads; do not discard the
valid baseline or substitute codec byte counts for the new observation.

## Ordered implementation and verification

1. **Shared normalization (this checkpoint).** Add the Core reference seam,
   route local wrappers through it, correct stale FFT-shape documentation.
   Preserve existing extended-wing tests. Add a real windowed FFT amplitude
   fixture and detector/rate comparisons against the existing local path.
   Build the affected targets before running them; run the no-GUI Core guard.
2. **Source descriptor and demand.** Expose the existing configured reference
   rate; gate availability using live P2 capability and published ADC mapping.
   Add tagged publication and separately aggregated ADC
   mask/chain bypass. Exercise ADC1 behind chain0, two consumers, last release,
   ADC remap and stale before/after-FFT callbacks. Coordinate RadioModel edits
   with the serial WDSP integration. Observe real P2 burst cadence when the
   Rock returns; rate/freshness uncertainties remain an explicit dependency.
3. **Core endpoint and negotiated context.** Implement the reducer and new
   feature gate together, retaining v1 wire rows. Cover all five detectors,
   distinct trace/waterfall averaging, clipped/no island, below DC/beyond
   Nyquist, first/missing/stale ADC data, malformed fields and old-peer fallback.
4. **GUI parity.** Admit the context, restore existing zoom/wing gestures,
   and retain 2D/3D RF history. Exercise context replacement during a gesture,
   command rejection, cohost change, hidden/removed pans and reconnect. The
   optional 3D wide plane must remain DDC-only while its exact row has wings.
5. **Combined acceptance.** One integrated risk review; focused corrections
   and the repository full suite against the combined source. Integrate the
   Task 6 budget before claiming four-pan parity. Native ARM build/install,
   real Saturn capture and receive-only filter transitions, local/remote
   gesture comparison, network sizes/rates, Rock load and mixed-audio soak.
   Operator observation is required for the hardware acceptance, not inferred
   from unit tests or a successful install.

No new dependency, DSP algorithm, default tuning behavior, NAT strategy,
transmit permission or menu redesign is part of this contract.

## Checkpoint evidence

- [x] Read and compare the current local contract, physical identities,
  normalization, capture-rate provenance and 2D/3D ownership.
- [x] Record the composite-row representation and strict compatibility gate;
  identify the existing Task 6 allocator gap explicitly.
- [x] Extract `WidebandDisplayReference` and delegate the local wrappers.
  Correct stale FFT-shape documentation; transform constants are unchanged.
- [x] Build `tst_wideband_display_reference`, `tst_wideband_fft_engine`,
  `tst_extended_pan_wings` and `tst_core_has_no_gui_includes` with `-j2`.
  All four pass (0.86 seconds). The new fixture verifies actual Hann-windowed
  FFT output at full and quarter amplitude, plus all five detector references
  when ADC and DDC rates change independently. Existing local wing behavior
  remains covered. Build-start load: 6.96 / 9.39 / 7.86.
- [x] Retire incomplete P2 capture bursts at a changed ADC enable bit and
  every connection-generation boundary. The regression first emitted one
  obsolete frame after disable/re-enable and after disconnect; it now emits
  none until a fresh seq=0 burst. Duplicate enable and another ADC's toggle
  preserve live capture. Also retire accumulator state before notifying direct
  observers: a reentrant next-burst start previously emitted the old row twice.
  Five freshly built focused targets pass (6.04 seconds): P2 enable bytes,
  frame accumulator, established silence, thread marshalling and wideband
  worker ownership. This fixes partial packet assembly; tagged FFT/media
  source-generation retirement remains the next distinct boundary.
- [x] Extract `ExtendedSpectrumReducer` for composed Core rows, retaining
  the existing local detector, clipped DDC island, ADC peak wings and separate
  plane averaging. A pixel-by-pixel characterization matches the existing
  widget across all five detectors with the DDC island visible and absent.
  Additional checks cover station calibration once on both regions, absent
  ADC data, independent trace/waterfall histories, recursive ADC averaging
  and malformed geometry/power. An extreme finite calibration initially
  corrupted retained averaging even when its row was rejected; the regression
  now confirms rejection leaves both output and subsequent history intact.
  Five freshly built focused targets pass (1.45 seconds): the extended and
  ordinary reducers, local wing characterization, shared reference and the
  no-GUI Core guard. Run-start load: 5.05 / 6.50 / 7.44. This reducer is not yet
  connected to remote endpoints; these checks do not establish wire or
  hardware parity.
- [x] Tag P2 assembler publication with a retained per-ADC atomic capture
  identity. Actual enable transitions retire that ADC; connection boundaries
  and destruction retire every ADC. Duplicate enable and unrelated ADC changes
  preserve identity. RadioModel checks the tag before FFT and again after its
  owner-thread hop without dereferencing a deleted connection. The real daemon
  fixture first published an obsolete row after disable/re-enable; it now
  rejects rows queued before FFT and queued after FFT, then accepts a fresh
  capture. Direct observer reentrancy and retained tokens after destruction
  are covered. Five freshly built focused targets pass (18.14 seconds): P2
  enable/tagging, established silence, marshalling, actual daemon recovery and
  wideband worker lifetime. This establishes capture-to-model retirement.
- [x] Add an owner-thread, bounded latest-frame cache for each physical ADC.
  The descriptor carries connection, capture and geometry identity, a nonzero
  source generation and the configured ADC rate. P2 timestamps a completed
  capture before dispatch; worker delay cannot make an old capture appear new.
  Immutable geometry snapshots reject captures queued across a rate change,
  including a change back to the original rate. Invalid shape, nonfinite bins,
  obsolete identity and backwards timestamps cannot replace a valid latest row.
  Availability and cache reads check retained capture tokens even before an
  asynchronous retirement notification reaches the model. Legacy local rows
  retain their existing shape and relative level reference.
  Seven freshly built focused targets pass (51.14 seconds; build-start load
  1.03 / 1.39 / 2.63). They cover the cache, FFT geometry, P2 tagging, daemon
  recovery, worker lifetime, marshalling and the no-GUI Core guard. Consolidated
  source/lifetime review found one reentrant teardown gap: notification could
  briefly expose the retiring source's availability. A regression reproduced
  the failure; both source tokens now retire before either notification.
  The two rebuilt correction targets pass (50.38 seconds): actual daemon
  recovery and wideband worker lifetime. Private evidence:
  `r3-wideband-source-tests.log` and
  `r3-wideband-source-retirement-{red,green}-{build,test}.log`.
- [x] Rebuild the matching `NereusSDR`, `nereusd` and `all_tests` targets,
  then run the unfiltered suite: **723/725 executables passed in 312.86
  seconds** (build-start load 1.38 / 1.34 / 1.90). The two failures are the
  unchanged native microphone-open timeouts in `tst_port_audio_bus` and
  `tst_audio_engine_speakers_live_reconfig`, both at 120 seconds, also present
  at the preceding receiver-layout checkpoint. Source/cache and actual daemon
  recovery checks pass. This records a completed verification run, not a
  passing full-suite gate. No timeout or test was weakened or excluded; the
  separate R-R3-36 native capture-startup gap remains open. Private evidence:
  `r3-wideband-source-final-{build,tests,load}.log`.
- [x] Separate physical-ADC capture from filter-chain bypass and add inactive
  Core endpoint-owner leases, ORed with local slice demand. The synthetic
  two-ADC/one-chain regression first sent capture mask `1` for a slice on
  ADC1; it now sends `2` while bypassing only chain 0. Source mapping and
  ownership remain distinct: removing one consumer preserves the others,
  remapping follows the published ADC, and removal/reconnect cannot reuse an
  old token. Direct filter observers cannot restore a released request from
  an older reconciliation snapshot. Existing P2 worker marshalling remains
  in force and now explicitly checks ADC1.
  Eight matching focused targets pass (51.38 seconds), including local chain
  aggregation, owner lifetimes, P2 marshalling, actual Alex policy wire pushes,
  pan badges, real daemon recovery, worker lifetime and the no-GUI Core guard.
  The daemon teardown observer also attempts acquisition and reactivation
  while the old P2 object is still live; both are refused. Private evidence:
  `r3-wideband-demand-red-test.log` and
  `r3-wideband-demand-focused-{build,tests}.log`. An additional two-ADC test
  passes in both release orders: only the retiring ADC's bit clears, and
  their shared filter chain stays bypassed until both owners release.
  Its matching target passes in 0.73 seconds
  (`r3-wideband-demand-shared-adcs-{build,tests}.log`). The consolidated
  independent review found no concrete defects in the scoped demand/lifetime
  changes. The matching combined GUI/Core/all-tests build succeeds. The
  unfiltered suite passes 724/726 executables in 307.92 seconds, with 12 inner
  Qt skips and the same two native microphone-open timeouts as the source
  checkpoint: `tst_port_audio_bus` (`openInputSucceedsOnDefaultDevice`) and
  `tst_audio_engine_speakers_live_reconfig` (`setTxInputConfigEmitsSignal`).
  Build-start load was 1.27 / 1.43 / 2.00. This is not a passing full-suite
  gate; R-R3-36 remains open. No tests were excluded or weakened. Private
  evidence: `r3-wideband-demand-final-{build,tests,load}.log`.
- [x] Wire demand-owner lifetimes into negotiated Core endpoints. Session
  minor 6 and `remoteWidebandDisplayVersion=1` gate the new subscription and
  strict nested descriptor; legacy peers retain their 19-field context.
  Core composes its current ADC row with the DDC island before independent
  trace/waterfall averaging, applies the station scalar once, and retains
  DDC-only optional 3D wide rows. Permission alone does not enable capture;
  endpoint replacement, last removal, remap and session retirement release
  their own leases. Applied software capture state supplies a real descriptor
  before the first ADC burst, including a capture enabled while Connecting.
  Idempotent application neither advances the capture epoch nor sends an
  extra CmdGeneral; it is not a hardware acknowledgement.
- [x] Admit negotiated contexts in the GUI, preserving the existing zoom
  controls and 2D/3D RF history. Available source geometry raises the zoom
  ceiling; active state must agree with granted permission and accepted RF
  coverage. Full retirement clears the offer without changing the saved
  preference. Remote accepted state does not emit local hardware intent.
- [x] Complete one integrated requirements/lifetime review and its corrections.
  The review found inconsistent GUI context admission and endpoint references
  retained across synchronous transport closure. Reproducing display-send
  tests failed because the peer was destroyed on its own send stack. Peer
  deletion is now deferred; control/display return paths revalidate identities
  and re-find endpoints. Context and noise-floor closure also retire capture,
  while a fresh media peer can reuse the endpoint number. GUI regressions
  cover invalid permission/coverage and valid inactive/active painting.
  No DSP, FFT normalization or hardware policy was changed to resolve tests.
- [ ] Complete native/hardware acceptance and Task 6's total-session allocator.
  Real Saturn burst cadence still determines freshness policy; no TTL has
  been guessed. Observe RF placement, receive-only BPF transitions, shared
  pans, wing tuning, 2D/3D gestures, packet sizes/rates, Rock load and mixed
  audio. Current source is not deployed; installed software remains `55e7d49f`.

Twelve freshly built focused targets pass in 50.03 seconds (run-start load
1.23 / 1.53 / 1.82): endpoint composition, daemon media and audio sessions,
P2 capture application, actual daemon recovery, GUI control/rendering, station
negotiation, strict descriptor codec, physical demand/chain lifetimes, local
wings and the no-GUI Core guard. Evidence:
`r3-wideband-endpoint-review-{build,tests,load}.log`,
`r3-wideband-close-{red-test,build,tests}.log`, and
`r3-wideband-gui-admission-build.log`.

The matching `NereusSDR`, `nereusd` and `all_tests` build succeeds, including
WDSP integration. The unfiltered suite passes **725/727 executables in 313.08
seconds**, with 12 inner Qt skips. Build-start load: 1.72 / 1.63 / 1.84;
run-start load: 8.35 / 4.66 / 3.04. The same two native microphone-open
120-second timeouts remain: `tst_port_audio_bus::openInputSucceedsOnDefaultDevice`
and `tst_audio_engine_speakers_live_reconfig::setTxInputConfigEmitsSignal`.
No test was excluded, weakened or given a longer timeout. This is a completed
verification run, not a passing full-suite gate; R-R3-36 remains open.
Evidence: `r3-wideband-endpoint-final-{build,tests,load}.log`.
Automated endpoint coverage does not establish full R3 or hardware acceptance.
