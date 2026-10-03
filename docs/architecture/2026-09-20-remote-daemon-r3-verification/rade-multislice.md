# R-R3-31: RADE on the second receiver

## Report and reproducing evidence

The operator reported that RADE-U on the second pan did not work and appeared
to mute the first pan. At signed baseline `d9c7bce1`, RadioModel published one
RadeChannel pointer without its owning slice ID, and RxDspWorker used that
pointer only for slice 0. Selecting RADE on B therefore redirected A into B's
decoder and suppressed A's ordinary audio.

The reproducing test creates two real WDSP RX channels, assigns separate
stream routes and audio buses, and lets both ordinary slices join the actual
master mixer before selecting B. On the faulty route it reported:

```text
Aordinary=0 Bordinary=96 radeAfterA=44 radeAfterB=44
masterBefore=1 masterAfter=1
Totals: 2 passed, 1 failed, 0 skipped
```

This was a routing/mix assertion failure, not a build failure. An earlier
fixture attempt omitted WDSP initialization and was corrected before accepting
the RED evidence. The test uses the repository's synchronous real WDSP init
seam, which skips wisdom generation rather than substituting DSP behavior.

## Repair contract

Publish the owning slice and a generation to the worker. QObject lifetime and
codec ownership remain on the control thread; decoded speech returns to the
DSP worker before it enters AudioEngine. Old I/Q, old decoded output and old
destroy callbacks cannot target a replacement slice/worker/decoder.

Nereus's master mixer waits for all enrolled producers and has no timeout.
The previous RADE wrapper suppressed output while the decoder was unsynced,
based on an obsolete timer-driven-audio assumption. Restore Aether's existing
same-sized quiet output when the decoder accumulator is short:
`src/core/RADEEngine.cpp:496–504` at
`0dea0dd7d73e25a40c8c01d46873af5834e23921`. A failed or warming decoder remains
withdrawn until current output exists. No DSP/codec parameters, mix timeout,
Opus profile or transmit behavior are changed by this repair.

## Verification status

The meaningful regression above is captured and the implementation is complete.
All six affected test executables pass after correcting two independent review
findings: an unkey edge could re-enrol a warming RADE target, and synthetic late
speech made the replacement-worker acceptance assertion vacuous. The regression
now exercises the actual software MOX edge without transmitting, proves ordinary
A continues after unkey, and requires new codec speech, B audio and master-mix
progress after replacement. Fresh resamplers get a bounded 256-input-block
warm-up window; missing output remains a failure with per-stage counts.

The full build (`all_tests` and `nereusd`) passed. The final unfiltered suite
passed **685/685 executables in 148.40 seconds**, with eleven pre-existing inner
Qt skips and none in the new regression. An earlier run passed 684/685 but the
two-hour simulated audio-clock test reached its 180-second timeout during a
concurrent separate build. The final run had no competing build and passed the
unchanged clock test in 148.38 seconds. No tests were excluded or weakened.

Matching native Core and GUI installation is complete at signed `dd2a9ebf`.
Hardware two-pan/RADE listening acceptance failed with stuttering on A; investigation continues below.

The first native build of `aed2278f` found a compile-only configuration defect:
the moved `attachDspWorkerForTest` definition lacked the test-build guard already
present on its declaration. The following corrective checkpoint restores that
guard without changing runtime behavior. The failed native build was never
installed; the native build and matching GUI are regenerated after correction.

Focused build/run targets are `tst_rade_rx_multislice_routing`,
`tst_rade_channel`, `tst_audio_engine_rade`, `tst_rade_channel_model_wiring` and
`tst_stream_pool_binding`, plus affected mixer/lifecycle coverage. Final checks
build `all_tests` and `nereusd` before running the full test suite.

Receive-only hardware acceptance: keep A in ordinary receive while B enters
RADE-U, including without synchronization; confirm A continues, then restore B
to ordinary receive. Exercise slice/pan replacement and reconnect without
reviving old audio. A valid on-air RADE decode requires a real decodable signal
and is separate from the no-sync continuity check. No RF or tuner actuation is
part of this checkpoint.

## Matching installation, September 21, 21:11

The corrected native build (`NEREUS_BUILD_TESTS=OFF`), staged installation,
dependency checks and all 72 packaged source hashes passed. The installed
Core library SHA-256 is
`7c1b2c6356ade7ed64973df0ba5edce6ad9f42af1e3b69a8f768229f8615ee4c`.
The prior installation is retained at
`/var/lib/nereus-build/rollback-d9c7bce1-before-dd2a9ebf/`.
Core PID 10969 was active with zero service restarts after installation.

The matching GUI's executable tag, both private library UUIDs and strict/deep
bundle signature were verified before opening saved profile `radxa_5c_r3`
at `.106:50055`. GUI PID 12088 authenticated to ANAN-G2 Saturn at 21:11:49.
The initial speaker-open delay caused a packet backlog and one audio-context
recovery; encrypted display and Opus reception then resumed. The Core sample
at about 30 seconds showed approximately 48,000 source frames and 25 encoded/
accepted packets per second, no encoder/send errors and twelve source drops.
These are short observations, not a soak pass. The GUI window title confirmed
`dd2a9ebf`; a fresh screenshot was unavailable, so no new visual waterfall or
meter acceptance is inferred from that observation.

The operator has been asked to keep A in ordinary receive while selecting
RADE-U on B. The answer and real decodable-signal acceptance remain pending.

## Live cadence failure, September 21, 21:16–21:19

The operator reported that the ordinary SSB receiver stutters and cuts in/out
while B uses RADE-U. This fails the two-receiver acceptance check despite the
routing/lifecycle tests passing. With RADE active, Core's one-second summaries
showed approximately 39,000–44,000 mixed source frames and 20–23 encoded packets,
below the required 48,000 frames and 25 packets. There were no encoder failures
or transport rejections, and most intervals had no source-drop events. The GUI
repeatedly underflowed and restarted audio contexts at about one-second intervals;
its packet delivery owner-thread delay was generally 0–5 ms.

B left RADE at 21:19:06. Core immediately resumed approximately 48,000 source
frames per second and retained the same audio context for the following twelve
seconds. This correlates the deficit with the RADE path; the exact sample-loss
cause and the operator's USB comparison remain under investigation. The earlier
regression proves routing and eventual output, not sustained sample conservation.
Telemetry installation is held while this cadence defect is investigated.

The operator subsequently clarified that leaving RADE mode on **both** receivers
resolves the audible issue. The source audit finds a 256-frame (5.33 ms at
48 kHz) mixer ring for the actual 64-frame DSP blocks. Ordinary audio arrives
synchronously, while RADE returns through a queued DSP/control/DSP round trip;
a delayed decoder burst can overflow the ordinary producer's ring. Thetis
`cmaster.c:159–168,297–306` at `v2.10.3.15` instead configures 4096 frames for
both RX and anti-VOX mixers independently of block size. A deterministic
sample-conservation regression and a bounded-capacity correction are next.
The longer-capacity ring is not a prefill delay; readiness still drains as soon
as all enrolled producers have samples.

The deterministic mixer regression fails on the installed implementation:
32 delayed 64-frame blocks drain only **256/2048 frames**; the uneven burst
sequence drains **1728/2048**. Both enroll the actual two-member mixer first
and mark input blocks so the green check verifies their exact paired content,
not just an eventual push. The correction gives each ring at least 4096 frames
while retaining the larger-block rule, immediate drainage, explicit lifecycle
withdrawal and drop-oldest behavior beyond its bound. Focused/full verification
and a fresh hardware test follow; no sustained repair is claimed yet.

The cadence correction passes all six affected test executables (7.10 seconds),
then the complete `all_tests`/`nereusd` build and **692/692 executables in
154.93 seconds**, with eleven pre-existing inner Qt skips and no new skips.
The lead verified the small capacity change against the upstream source and
reviewed the frame-count/content regressions. Both verbatim upstream licence
headers are preserved; their original trailing spaces are the only diff
whitespace findings. Matching native/GUI installation and the repeated live
RADE test remain the next gate.

## Installed cadence correction and remaining playback interruptions

Signed `0b408427` was installed in matching Core and GUI builds at 21:42 on
September 21. The native production build and 109 source-overlay hashes pass.
Installed NereusCore SHA-256 is
`f91a7c05b4fe86a5f55d83fa0dab0b921e60c846428cfc09d8a2765e0d8f0925`;
rollback is `/var/lib/nereus-build/rollback-dd2a9ebf-before-0b408427/`.
Core PID 14142 was active with zero service restarts. The saved GUI profile,
endpoint and pairing were retained; executable/private library identities and
strict/deep signing matched the checkpoint before launch.

Live observation showed A at 14.2932 MHz USB and B at 7.177 MHz RADE-L, with
RADE indicating sync at one observation. Core context 39 produced 4,832,960
source frames in 100.686 seconds (about 48,000/s), 2487 encoded/accepted packets,
30 source-drop events and no encoder errors or send rejections. Source-frame
telemetry counts valid ingress before bridge contention drops; it is not proof
that every frame reached the encoder. Codec sync is not listening acceptance
for decoded speech.

The original mixer sample deficit is no longer observed, but playback still
underflows. At 21:52:46 a context restarted after 175.206 seconds, with 4338
accepted packets, 40 PLC blocks, one late packet, a 133 ms maximum arrival gap
and a 10 ms maximum worker wake gap. Further interruptions followed. Packet
receipt gaps are generally much larger than the GUI owner-thread drain delay.
The operator's clarification that leaving RADE on both receivers resolves the
problem remains the comparison condition. Sender timing and an actual receiver
burst-arrival regression are under investigation. The arrival-plus-80-ms jitter
clock and existing long-duration clock-drift invariants remain unchanged.

### Sender capture and receiver regression

A passive 24.277-second capture on the Rock's wired `end1` interface contained
607 outgoing audio RTP packets (PT 111). Their median spacing was 40.109 ms,
maximum spacing 109.967 ms, and fifteen gaps were at least 80 ms. Every sequence
increment was one; timestamp steps were 1920 except one 3840 step. Thus this
capture proves uneven timing before the network hop, without claiming that
all later client gaps originate on Core or that a particular codec call causes
them. The exact sender scheduling cause remains open.

The new actual-Opus receiver regression uses a 128-frame independent device
clock and repeated arrival intervals `17 × 40, 100, 0, 20 ms`, preserving an
average 25 packets/s. It fails before its intentional missing packet: at
865 ms it had accepted 22 packets, decoded 18, concealed none and underflowed,
with four packets still queued, a 98 ms maximum arrival gap, 4 ms maximum worker
wake gap and unchanged 1.0 resampler ratio. This reproduces restart from bounded
burst timing without source loss or a stalled GUI worker.

The correction under development permits only the exact expected, already
received packet to leave its hold when decoded PCM cannot satisfy the next
speaker refill. It retains the normal arrival-plus-80-ms release path, original
missing-packet deadlines, queue bounds and continuous resampler state. It does
not force early PLC, grow latency or change resampler feedback. Verification
and hardware acceptance are pending.

The demand-release correction passes the unchanged real-receiver burst/loss
regression and all seven affected executable checks in 9.77 seconds. Its precise
native-block readiness test also passes: eight 480-frame pulls leave 480 frames,
which cannot satisfy the next 512-frame native pull; adding a real input block
restores readiness without an underflow. The complete `all_tests`/`nereusd` build and unfiltered **692/692** desktop
executables pass in 128.73 seconds, including both one-hour simulated clock
directions. Eleven pre-existing inner Qt skips remain; the new regressions have
none. Independent review prompted stronger exact packet/PLC assertions and an
explicit comparison of existing future-packet loss deadlines. Both affected
executables pass after those test/comment refinements (9.43 seconds): all 99
valid packets are admitted and decoded, with exactly one deliberate PLC and no
restart or matcher error. The comparison confirms that the future-packet loss
anchor behaves identically before/after early release; the API comment now
precisely identifies the preserved empty-queue deadline. No production behavior
changed after the full gate. Matching installation and repeated RADE listening
remain pending.

## Demand-release installation, 200d2a0e

Matching signed Core and GUI `200d2a0e` were installed at 22:22. All 115 source
hashes and the native tests-off build/stage pass. Installed Core SHA-256 is
`81dc65540024580dc3c339e780fdb773c66d99164c1af3818e4319eb66b50fc1`.
Rollback is `/var/lib/nereus-build/rollback-0b408427-before-200d2a0e/`.
The GUI executable/private libraries and strict/deep signature match; the saved
profile and pairing are unchanged. Initial device-open backlog caused one
arrival-queue restart, then playback resumed in context 3. Core's restart
restored only A. B was recreated through the actual GUI, and the operator
continued tuning it in ordinary LSB. The new RADE comparison is pending; neither
a RADE listening pass nor sustained remote-audio acceptance is claimed.
