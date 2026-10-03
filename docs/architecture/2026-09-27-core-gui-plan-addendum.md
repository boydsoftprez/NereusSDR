# Core/GUI plan addendum: every gap found, with its ruling or question

JJ's goal (2026-09-27), from `landing-for-phone.md`: "land the Core/GUI work on main as one
PR passing ci.yml (Linux, macOS, Windows) and ios.yml where it applies, after finishing the
parity plan, the Core station tasks the phone depends on, R5, and a plan addendum of every gap
found." This document is that addendum. It is kept for the whole effort, not just this one
push: see the last section for how it grows.

Each entry: what the operator sees or would see, where it was found, the evidence, the ruling
(verbatim quote and date, or an open question with options and a recommendation), its build
status, and the plan task or requirement ID it belongs to.

JJ clarified the acceptance bar on 2026-09-28: working desktop features from
before the Core/GUI split must retain parity, and omissions become addenda in
this effort. The scout compares main `86a1b20d` (which has no daemon target or
`DaemonApp`) with integration `11a1173a`, tracing actual behavior rather than
counting controls. Historical placeholders are identified separately; a source
audit does not substitute for matching local/remote operating checks.

## Open questions (JJ has not ruled)

### G-50: Phone Setup has no counterpart for desktop-local services and file actions

- Evidence: the TCI Server page has twenty local controls/actions plus status,
  separate from four implemented Core TCI toggles. Diagnostics Export All/Import
  All operate on a desktop settings file; Logs Refresh/Clear operate on the local
  view. The phone controller confirms that no phone-native TCI server or local
  file/log action contract exists. Relabeling these as Core actions would change
  their meaning.
- Ruling: OPEN. JJ was asked whether the phone should show these as unavailable
  with an explanation, retaining Core TCI and Core diagnostics, or whether phone
  equivalents should be built in this effort.
- Status: the exact inventory and disabled wording are recorded in the Setup
  lane. The first Core description slice publishes 35 controls across four pages
  in General, Test and Core TCI; coverage remains explicitly partial. (Corrected 2026-09-28:
  "Diagnostics remains unpublished" is stale. Its Settings Validation page is published as the
  closed version 3 panel in signed `9fad480b`, `resources/setup/diagnostics.json`; see G-97.
  The desktop-local file and log actions this entry asks about remain unpublished and the
  question stays OPEN.) Setup-aware peers receive its read-only
  object; older peers remain unchanged. Integrated app/Core build and seven
  session/Setup checks passed, followed by repaired full surface and session
  conformance checks (five targets, 35.68 s). Omitted controls and empty pages
  do not count as completed parity. General Region is active behind
  transmitSettingsVersion 9 with a live off-air gate; Extended remains
  unavailable pending its separate migration ruling.
- Plan: iPhone D16, Core Setup-description tasks 43-46 and phone renderer task 58.

### G-04: ATT on TX changes take effect at the next key, not at once

Thetis applies a step-attenuator-on-TX change immediately (`console.cs:19078`). NereusSDR
applies it only the next time the operator keys.

- Found: 2026-09-25/26, parity Task 31.
- Evidence: `nereus-parity/.../progress.md:111`: "Task 31: minor (deferred): ATT on TX toggled
  while keyed takes effect at the next key; Thetis at once (console.cs:19078)."
- Ruling: OPEN (recorded as a deferred minor, no operator ruling on record).
- Status: open.
- Plan: parity Task 31 (A11, R-R3-49).

### G-05: MOX release 10 ms after the drain can cut a stall's backlog at the end of an over

If the network send ring stalls near the end of a transmission, releasing MOX 10 ms after the
drain can cut off the tail of what was queued instead of waiting for it to actually go out.

- Found: 2026-09-27, P2 TX review.
- Evidence: `nereus-lane-b/.../progress.md:426`: "P2 TX: questions for JJ: (a) hold MOX until
  the send ring drains (bounded) so a stall's backlog is not cut at the end of an over."
- Ruling: OPEN. Recommendation (from the ledger): hold MOX until the send ring drains, bounded
  (i.e. wait for the drain, but with a ceiling so a stuck link cannot hold MOX forever).
- Status: open.
- Plan: needs an ID (P2 TX send thread work; no R-IOS/R-R3 tag recorded for this item).

### G-06: On the Rock with three receivers, TX threads have no fast core while keyed

The Rock (3 receivers) does not currently give the transmit threads a fast CPU core while
keyed, unlike the receive-side thread placement work already done.

- Found: 2026-09-27, P2 TX review.
- Evidence: `nereus-lane-b/.../progress.md:426`: "(b) on the Rock with three receivers, let TX
  take a fast core while keyed."
- Ruling: OPEN.
- Status: open.
- Plan: needs an ID.

### G-07: Protocol 1 TX ring stays at 84 ms; a stall over ~64 ms can overflow it

- Found: 2026-09-27, P2 TX review (carried, not new to this pass).
- Evidence: `nereus-lane-b/.../progress.md:426`: "Carried: P1 ring still 84 ms (stall > ~64 ms
  can overflow); clock drift only over many-minute overs; the unkey line has no dedicated
  test."
- Ruling: OPEN.
- Status: the P1 ring question is open. The unkey-line test part is built (corrected
  2026-09-28): `unkeyLineCarriesTheMicrophonePathsLatency` (`tests/tst_daemon_media_controller.cpp:613`,
  `:4418`) from signed `d5849337d` checks the unkey line's latency, ring fill and silence-shed
  fields. That this is the unkey line the ledger meant is inferred, not confirmed.
- Plan: needs an ID.

### G-08: The Core keeps a dead rendezvous path open up to 60 s; the service gives up in 20-40 s

When the path to the rendezvous service dies, the Core takes up to 60 seconds to notice, while
the service itself gives up in 20-40 seconds. A phone reconnecting through rv can be stuck
waiting on the slower side.

- Found: 2026-09-27, Task 28 tail.
- Evidence: `nereus-lane-b/.../progress.md:430`: "Task 28 tail: question for JJ (low): should
  the Core give up on a dead rv path ~20 s sooner (matching the service) so the phone's reach
  returns faster."
- Ruling: OPEN (marked low priority by the implementer).
- Status: open.
- Plan: R-IOS-16 (station Task 28).

### G-10: The Pi's and Rock's explicit `audio_bitrate = 24000` config lines need JJ's yes to change

JJ approved 48 kbps full-band Opus as the new default, but the Pi 4's and Rock's installed
config files each set `audio_bitrate = 24000` explicitly, which overrides the new default. An
install would need to either change that line or leave those two Cores at the old rate.

- Found: 2026-09-26, Opus 48k task dispatch.
- Evidence: `nereus-lane-b/.../progress.md:367`: "the Rock's and Pi's explicit audio_bitrate =
  24000 lines need JJ's yes to change at install."
- Ruling: OPEN (a config edit, needs JJ's yes per the standing rule on device installs).
- Status: the 48 kbps default itself is built and merged (commit `e324e2bb`); the two
  installed Cores' config files are unchanged pending JJ's answer.
- Plan: R-R3-21, R-IOS-09.

## In progress

### G-16: Other config-file keys that may override every start (audit)

Beyond `sample_rate_hz` (G-17 below), `audio_device` and `audio_bitrate` may have the same
"overwrites the saved choice on every start" problem. The continuation audit found one
remaining settings overwrite: an explicit audio_device replaces the saved speaker choice.
Audio bitrate is a runtime encoder policy, with no separate saved choice found to overwrite.

- Found: 2026-09-27, controller's own audit alongside the sample-rate fix.
- Evidence: brief's own seed list, corroborated by the sample-rate fix's commit message
  (`043b8cfb`) treating `sample_rate_hz` as the first instance of the pattern.
- Ruling: OPEN for whether explicit audio_device should seed only an absent speaker choice,
  like sample_rate_hz. No radio config file has been edited.
- Status: source audit complete in DaemonApp::applyConfigToSettings and setupRemoteSession.
  applyConfigToSettings has only sample-rate seeding and the audio-device overwrite; bitrate
  goes directly to DaemonMediaHub::setAudioTargetBitrate. Regression/behavior change awaits
  the audio-device ruling.
- Plan: R-R3-49 (small-followups lane).

## Queued

### G-20: TGXL TRANSMITTING hold (~400 ms) so the amplifier never sweeps without carrier

When a network device's TUNE turns on while the TGXL amplifier is still switching, the display
should hold "TRANSMITTING" for up to about 400 ms so the antenna tuner never sweeps with no
carrier present.

- Found: 2026-09-2x, PGXL/TGXL capture analysis (read-only).
- Evidence: `nereus-lane-b/.../progress.md:401`: "QUEUED (low priority): hold TRANSMITTING for
  a network device's tune on while the amp is still switching, at most ~400 ms, so the TGXL
  never sweeps without carrier (SmartSdrApiListener.cpp:429)."
- Ruling: queued as low priority; no operator ruling needed beyond the queue placement itself.
- Status: queued, not started.
- Plan: needs an ID.

## Ruled and built (record with commits)

### G-01: Spectrum decimation range cited to Thetis is wider than Thetis allows

The Rendering setup page lets an operator pick a decimation step from 1 to 32. Thetis's own
control tops out at 16, so NereusSDR's range is wider than the source it was ported from.

- Found: 2026-09-27, trunk catalogue-ranges task, lane B.
- Evidence: `nereus-lane-b/.../progress.md:434`: "the decimation range 1-32 is cited to Thetis,
  which allows 1-16 (setup.designer.cs:33834 [v2.10.3.15]); cite correction sent back to the
  implementer; the range itself is a question for JJ (low)."
- Ruling: JJ approved on 2026-09-27: "Match Thetis: 1–16 (recommended)".
- Status: implementation in `codex/display-decimation-parity`; the UI, catalogue, local FFT
  engines and Core media request validation share the same bounds. Boundary regression
  first failed because 17 was accepted. Signed implementation `87f59425b` passed five focused
  tests including link conformance in 45.43 s; the integrated trunk passed the same
  five targets in 41.99 s. Generated protocol tables and diff checks pass.
  A September 28 read-only phone scout found its range, request encoder and
  subscriber clamp still permit 1-32 at phone `3c5771c82`. The exact source
  finding was delivered to the phone owner for correction under this same
  ruling. Core remains 1-16; phone-side acceptance is not yet verified.
  (Re-sorted 2026-09-28 into "Ruled and built" for the Core side, `87f59425b` being an
  ancestor of the trunk; the phone's clamp remains the phone owner's work.)
- Plan: R-IOS-06, R-IOS-27 (catalogue ranges task).

### G-02: Several noise-reduction ranges and new-slice defaults differ from Thetis

The desktop's NR2, NR4 and new-slice NR4 controls use different numeric ranges and starting
values than Thetis's own dialogs. An operator moving between the two apps would see different
numbers for what should be the same control.

- Found: 2026-09-27, catalogue-ranges scout and task, lane B.
- Evidence: `nereus-lane-b/.../progress.md:440`: "desktop NR values that differ from Thetis
  v2.10.3.15 (NR2 Factor/Rate 0-30 vs 0-100; NR4 Rescale 0-20 vs 0-12; NR4 SNRthresh -30..0 vs
  -10..+10; new-slice NR4 Smoothing 65/Whitening 2/Algo 2 vs 0/0/Algo 1). Recommend correcting
  to Thetis, as NR1 was."
- Ruling: JJ approved matching Thetis on 2026-09-27 in the Core/GUI Codex continuation:
  "Match Thetis (recommended)". Match the NR2 Factor/Rate range and fractional steps,
  NR4 Rescale/SNR threshold ranges, and new-slice Smoothing/Whitening/algorithm defaults.
  Preserve saved operator choices; this is not a settings reset.
- Status: NR1 corrected and built (commit `d6d96eaa`, in trunk); NR2/NR4 implementation
  merged into trunk as signed `edfd220bb` (implementation `b9bedc3c5`). New control/default/save
  regressions, catalogue checks and both session transports pass. Existing saved choices
  remain intact; catalogue fixtures advertise the same ranges as the controls.
- Plan: R-IOS-06, R-IOS-27.

### G-03: A local window's receive waterfall never takes the display calibration

A remote window's waterfall rows arrive already calibrated from the Core. A local window's
receive waterfall does not apply the same calibration, so the two show slightly different
colours for the same signal.

- Found: 2026-09-25/26, parity Task 31.
- Evidence: `nereus-parity/.../progress.md:110`: "Task 31: question for JJ: a local window's
  receive waterfall never takes the display calibration (Thetis adds it; a remote window's
  rows arrive calibrated from the Core), so local and remote differ; fixing it moves every
  operator's receive colours."
- Ruling: JJ approved on 2026-09-27: "Apply calibration in both windows (recommended)".
- Status: signed implementation `f7af7860f` applies local receive calibration once before
  colours, threshold tracking and 3D history. Six focused tests passed in 7.83 s, including
  positive/negative offsets and remote/TX no-double-calibration checks. The integrated
  trunk passed the same six targets in 7.64 s.
  The colour shift is explicitly approved.
- Plan: parity Task 31 (A11, R-R3-49).

### G-11: Protocol 2 sample rates for Atlas, Hermes, HermesII and HL2 sit below Thetis's range

Thetis allows these boards 48 kHz to 1536 kHz under Protocol 2 firmware; NereusSDR's current
range is narrower for the same boards.

- Found: 2026-09-24/25, gaps review.
- Evidence: `~/.config/nereus/work/checkpoint-next-operator-list.md:184`: "JJ QUEUE: Protocol 2
  rates for Atlas, Hermes, HermesII and HL2 kept below Thetis's 48-1536 kHz (Hermes-class
  radios can run P2 firmware): follow Thetis? (from the gaps review)."
- Ruling (corrected 2026-09-28; the earlier "OPEN" missed it): JJ answered on 2026-09-25,
  "1 follow thetis", as recorded in `2026-09-24-receiver-and-transmit-gaps-plan.md` Task 15:
  Atlas, Hermes, HermesII and HL2 rows on Protocol 2 follow Thetis too; mi0bot-Thetis is
  authoritative for the HL2.
- Status: built in signed `8d65d0ac7` (an ancestor of the trunk). The Atlas, Hermes, HermesII
  and HL2 rows offer all six rates, 48 to 1536 kHz (`src/core/BoardCapabilities.cpp:330`, `:393`,
  `:462`, `:935`), each citing "the operator's ruling of 2026-09-25" (`:324`, `:387`, `:456`,
  `:925`); `sampleRatesFor` trims each row to the protocol in use.
- Plan: receiver-and-transmit-gaps plan Task 15.

### G-12: TCI and MMIO Setup pages keep protocol-facing names (WebSocket, endpoint, binding)

JJ's standing rule is plain operator wording with no protocol jargon, but the TCI Server and
MMIO setup pages still use "WebSocket," "endpoint," and "binding," because those are the
features' own names.

- Found: 2026-09-23, checkpoint wording review.
- Evidence: `~/.config/nereus/work/checkpoint-next-operator-list.md:67`: "Question queued for
  JJ: the TCI and MMIO Setup pages keep their feature names (WebSocket, endpoint, binding):
  keep or reword?"
- Ruling (corrected 2026-09-28): decided by the operator on 2026-09-24. Signed `5e9f58b70`
  records "the words the operator approved on 2026-09-24"; no verbatim quote of his answer was
  found in the sources read for this correction. Words another program shows stay as it shows
  them (TCI, TCP, UDP, JSON, XML, port, IP address, MMIO in brackets); NereusSDR's own jargon is
  said plainly.
- Status: built in signed `5e9f58b70` (an ancestor of the trunk). The TCI page reads "Listen
  on:" (`src/gui/setup/CatNetworkSetupPages.cpp:224`); the MMIO window is "Meter Data Sources
  (MMIO)" with "Data sources" (`src/gui/containers/MmioEndpointsDialog.cpp:116`, `:135`). The
  wording sweep (`tests/tst_operator_wording_sweep.cpp`) fails if endpoint, binding, bind
  interface or WebSocket return in text a user reads there. Still awaiting the operator's
  look: the wording the implementer chose beyond the approved list
  (`checkpoint-next-operator-list.md:99`: the TCI listen-address choices and tooltips, the
  network-card entry format, the meter editor labels and similar); that review item is
  unchanged by this correction.
- Plan: R-R3-21, R-R3-48.

### G-13: The Connections window says "Your stations" / "Stations on this network" while the app says "Core"

- Found: 2026-09-23, checkpoint wording review.
- Evidence: `~/.config/nereus/work/checkpoint-next-operator-list.md:68`: "Operator review item
  (wording): the Connections window says 'Your stations' and 'Stations on this network' while
  the rest of the app says 'Core'; keep 'station' (a Core with its radio) or change to
  'Core'?"
- Ruling (corrected 2026-09-28): the operator decided on 2026-09-24 that user text calls the
  computer you connect to "the Core" (phone design D43's wording rule, set by JJ on 2026-09-24;
  signed `831a60100`: "Operator decision of 2026-09-24"); "station" stays only in its ham sense.
- Status: built in signed `831a60100` (an ancestor of the trunk). The Connections window reads
  "Cores on this network" and "Your Cores" (`src/gui/ConnectionSelector.cpp:234`, `:236`), with
  "No Cores found on this network." and "No saved Cores."
- Plan: R-R3-21.

### G-14: Older windows finding a full Core receive a retryable refusal

- Ruling: JJ's 2026-09-24 board v50/v51 decisions are recorded in the phone crew ledger and
  phone design D66. With four devices or no free receiver, an older window is refused with
  "The Core is full. Update NereusSDR to take a device's place, or try again later."
  Nobody already connected is disturbed. The phone controller confirmed these sources;
  the Core lead read the ruling directly.
- Status: built in signed `43e8140a` and integrated by `dd42fc56f`.
  Older clients receive the retryable full-Core refusal without displacing an
  admitted device. The integrated admission/session verification and subsequent
  lifetime corrections are recorded under G-53.
- Plan: R-IOS-30, R-IOS-31.

### G-15: Away devices come first in the fifth-device choice

- Ruling: phone design section 5.9, kept by JJ on board v50/v51, explicitly puts an away
  device first. D55's idle-longest ordering remains for connected candidates, and D64
  protects the desktop hosting the Core.
- Status: built in signed `43e8140a` and integrated by `dd42fc56f`.
  `DeviceSessionRegistry::replacementCandidates` sorts away devices first;
  `awayIncumbentIsOfferedFirstAndReplaced` verifies the offered device and actual
  replacement. G-53 records the integrated verification and remaining fixture limit.
- Plan: R-IOS-30, R-IOS-31.

### G-17: Config-file `sample_rate_hz` overwrote the saved rate every start

Every Core start or install re-applied the config file's `sample_rate_hz` over the per-radio
saved rate, so a rate chosen from a window was thrown away at the next restart.

- Found: 2026-09-27, controller's own review.
- Ruling (JJ, 2026-09-27, "your recommendation"): `nereus-lane-b/.../progress.md:445`: "Ruling
  (JJ 2026-09-27, 'your recommendation'): the Core's config-file sample_rate_hz is a starting
  value only; a saved per-radio rate wins across restarts and installs."
- Evidence of the fix: commit `043b8cfb`, "Seed the Core's sample rate from the config file
  only when none is saved" (worktree `nereus-small`, branch `codex/conf-rate-seed`).
- Status: built and integrated as equivalent signed implementation `40766db6b`,
  confirmed an ancestor of trunk `11a1173a` during the 2026-09-28 scout. The old
  `043b8cfb` hash itself is not an ancestor; that does not mean the fix is absent.
  `DaemonApp::applyConfigToSettings` preserves a saved per-radio rate; its tests
  cover saved, initial, absent and unsupported rates. The separate audio-device
  overwrite remains open in G-16.
- Plan: R-R3-49.

### G-18: Tests failing under load, including "the data channel did not open"

A cluster of tests fail only under shared-machine load (contention, timing-sensitive setup,
libdatachannel races), not in isolation. A dedicated lane is fixing them one at a time.

- Found: throughout the effort (recurring FINDING across lane-b and parity ledgers).
- Evidence: worktree `nereus-r2-integration`, branch `codex/flaky-tests`, 11 commits ahead of
  `codex/checkpoint-b` as of this writing (e.g. "Wait for the Core's receive lane before
  reading its DSP results in tst_remote_dsp_info," "Keep a remote description before
  libdatachannel's ICE agent takes it," "Hold the test data channel pair's offer candidates
  until the answer is kept"); trunk head's own non-realtime run
  (`nereus-lane-b/.../progress.md:444`) shows "tst_tci_remote_window lossless, three
  data-channel rows 'did not open'," passing below load 25.
- Ruling basis: JJ requires a test that fails under load to be reported with its
  cause and a suggested fix. This batch includes a real libdatachannel race as
  well as test setup repairs; it is not all test-only hardening.
- Status: the original batch is integrated in signed merge `02d0a4faf`,
  confirmed an ancestor of trunk `11a1173a` during the 2026-09-28 scout. This
  closes the stale “not yet merged” status, not every remaining load finding.
  Later load failures retain their own cause, fix and verification requirements;
  timeout increases alone are not an accepted resolution.
- Plan: R-R3-49.

### G-19: TX monitor plays only on the transmitting device

- Found: 2026-09-26, parity lane.
- Ruling (JJ, 2026-09-26, "no" to the Core's own speakers also playing):
  `nereus-parity/.../progress.md:94`: "MON ruling (JJ 2026-09-26, 'no' to the Core's speakers
  also playing): while a remote device holds transmit its MON plays only on that device; the
  Core's local monitor stays quiet; a local window at the Core that holds transmit still hears
  it."
- Evidence of the build: `nereus-parity/.../progress.md:113`, Task 32 "complete with concerns
  (opus, medium; 7bc095a8 G; MON level after Opus decode within 0.2 dB (main) / 0.07 dB
  (headphones))."
- Status: built and integrated. Git confirms `7bc095a8` is an ancestor of current trunk;
  the earlier handoff describing it as queued is stale. Hardware acceptance remains distinct
  from the synthetic monitor-level evidence above.
- Plan: parity Task 32.

### G-21: Absent hardware hidden; controls that exist but cannot run stay disabled with a reason

- Ruling (JJ, 2026-09-26): `nereus-lane-b/.../progress.md:394`: "JJ 2026-09-26: controls for
  hardware the radio lacks are hidden; controls that exist but can't run are disabled with a
  reason."
- Built: commit `96eeecd8`, "Send the transmit ranges, the noise-reduction controls and the
  board's relays in the Core's catalogue," whose message states the catalogue gives
  `board.rx1Preamp` and `board.relays` "for an app to hide what the radio lacks, as the desktop
  does." Confirmed an ancestor of the trunk head `6c3f543d`.
- Plan: R-IOS-06, R-IOS-27.

### G-22: DFNR, MNR and BNR shown disabled with a reason, never hidden (and BNR's button removed)

- Ruling (JJ, 2026-09-25): `nereus-lane-b/.../progress.md:208`: "JJ 2026-09-25 on DFNR: 'why
  cant dfnr run? Not a fan of disappearing buttons but rather disabled. Also a big fan of the
  dfnr just working.'" And on BNR, `nereus-lane-b/.../progress.md:245`: "JJ 2026-09-25 on BNR
  (NVIDIA noise removal, in no build): option 2, take its button out ('2 for bnr we can look at
  that again later')."
- Built: commit `6812ed66`, "Show DFNR, MNR and BNR disabled with the reason, never hidden";
  commit `7e4f3480`, "Take the BNR button out, as the operator decided." Both confirmed
  ancestors of the trunk head.
- Plan: R-R3-49, Sub-epic C-1.

### G-23: DFNR loaded lazily at first selection, not at channel construction

Every channel used to build a DeepFilterNet3 instance at construction (loading an 8 MB model,
about 250 ms, five channels per connect), so any build with the DFNR model spent over a second
per connect. It now loads only at a channel's first DFNR selection.

- Found: 2026-09-25, lane D follow-up.
- Ruling: controller ruling (not a direct JJ quote, but a design decision recorded and acted
  on): "create the DFNR instance lazily at its first selection, on the receive lane... a small
  task in the tx lane right after Task 31."
- Built: `src/core/RxChannel.cpp:37-40` (comment): "2026-09-25 - R-R3-39, Sub-epic C-1: the
  DeepFilterNet3 instance is built at a channel's first DFNR selection, on the receive lane,
  not in the constructor." Present in the trunk head's checked-out tree.
- Plan: R-R3-39, Sub-epic C-1.

### G-24: Opus at 48 kbps full-band for every mode, no FEC

- Ruling (JJ, 2026-09-26): `nereus-lane-b/.../progress.md:367`: "JJ 2026-09-26: Opus at 48 kbps
  full-band for every mode ('48kbps seems thin enough for everything')." FEC was approved the
  same day, then reversed after measurement: `nereus-lane-b/.../progress.md:379`: "JJ
  2026-09-26: Opus 48 kbps full-band, NO FEC (option 3 after the measurement)," because FEC
  forces SILK/hybrid and kills audio above 8 kHz at 48k.
- Built: commit `e324e2bb`, "Merge the Core's 48 kbps full-band Opus default into the trunk."
  Confirmed an ancestor of the trunk head.
- Plan: R-R3-21, R-IOS-09.

### G-25: Display presented on the audio's playout clock, with blended gap rows

Watching the waterfall over a WAN link with real jitter stuttered independently of the audio.
JJ asked that audio and waterfall stay in sync.

- Ruling (JJ, 2026-09-26): `nereus-lane-b/.../progress.md:381`: "JJ 2026-09-26: audio and
  waterfall must stay in sync; present the display on the audio's playout clock with blended
  gap rows ('yes on your recommendation assuming you have thought about this carefully')."
- Built: commit `83ac32ca`, "Merge the remote display playing in step with its audio into the
  trunk," carrying `6662a48a` ("Present a remote window's display on its audio's clock and ride
  a stalling link") and `f59cc8ee`. Confirmed an ancestor of the trunk head.
- Plan: R-R3-21, R-R3-08.

### G-26: Display duplex (DUP) off by default, with a View menu item in both windows

- Ruling (JJ, 2026-09-26, on question Q5): `nereus-parity/.../progress.md:92`: "Q5 ruled by JJ
  (2026-09-26, 'yes for your DUP recommendation'): DUP off by default as in Thetis, with a
  'Display duplex (DUP)' View menu item in both windows; Task 31 stands as written."
- Built: commit `17cd4dc5`, "Show the receiver while transmitting with display duplex (DUP) in
  both windows." Confirmed an ancestor of the trunk head.
- Plan: parity Task 31 (A11, R-R3-49).

### G-27: Radio change goes through the Core's own restart, and windows reconnect by themselves

Rather than build a live radio-swap path, the Core restarts itself when its radio is changed;
connected windows are told why and reconnect on their own.

- Ruling (JJ, 2026-09-26, "go with your recommendation"): `nereus-parity/.../progress.md:88`:
  "JJ ruled 2026-09-26 ('go with your recommendation'): option (c) keep the run restart,
  windows reconnect by themselves with a plain radio-change reason; fix I1 (control socket) and
  I2 (answer/notices/end reason flushed before teardown)."
- Built: commit `f355f7da`, "Merge parity Tasks 19 and 21 (spots and the Core's radio from a
  window) into the trunk," carrying `9c9e4546` ("Change the Core's radio from a remote
  window"), `87af370f` ("Tell every app why the Core restarts for a radio change and keep its
  console"), `96c7786b`, `a7e04997`, and `2ad4cce3`. Confirmed an ancestor of the trunk head.
- Plan: R-IOS-18, R-R3-38, R-R3-49.

### G-28: Relay quota raised to 8 allocations per Core, 128 in total

- Ruling (JJ, 2026-09-26): `nereus-rendezvous/.../progress.md` line 55: "Relay quota raise
  approved by JJ (8 per Core, 128 total)." Superseded by a later sizing ruling the same day,
  recorded in the same ledger: "Rulings (JJ 2026-09-26): 2000 Cores; ... (user-quota 4, 64
  slots, ...)" for the dedicated server, then raised again: `nereus-lane-b/.../progress.md`
  and the rendezvous ledger both confirm "coturn user-quota 8, total 128" as the value
  deployed and carried into the trunk.
- Built: commit `bb2a3d16` ("Raise the relay allowance to 8 per Core and 128 slots in total"),
  merged into the trunk as commit `5036485a` ("Merge the rendezvous relay quota (8 per Core,
  128 total) into the trunk"). Confirmed an ancestor of the trunk head.
- Plan: R-IOS-16.

### G-29: Local and remote windows both get an Operate button for the Power Genius XL tab

Only the remote window's 4O3A PowerGenius tab had an Operate button; the local tab had none.

- Ruling (JJ, 2026-09-25): `nereus-parity/.../progress.md:45`: "Task 9: ruling amended by JJ
  2026-09-25 ('1 add it to the local tab we want parity no matter how i am connected'): the
  local window's PowerGenius XL tab (PgxlAdvancedPage) gets an Operate button too... Parity
  runs both ways: a control a remote window has, a local window gets without asking."
- Built: commit `1eb12fc4`, "Operate the Power Genius from a local window's PowerGenius XL
  tab." Confirmed an ancestor of the trunk head.
- Plan: parity Task 9.

### G-30: What switches the amplifier or tuner is blocked on the air in both windows; what only listens or saves is allowed in both

- Ruling (JJ, 2026-09-25, on group B finding M5): `nereus-parity/.../progress.md:54`: "JJ
  2026-09-25 on group B M5: '1 block them in both windows': a local window's PGXL
  Operate/Standby, RF-Kit Operate/antenna and TGXL relay moves get the remote on-air rule,
  disabled with the reason while keyed."
- Built: commit `b9d475bd`, "the on-air rule by what a control does in both windows: amp/tuner
  switches incl. RF-Kit TCI mode held; Scan LAN and address saves taken on the air; a local
  click in the unkey window shows the reason" (per `nereus-parity/.../progress.md:63`).
  Confirmed an ancestor of the trunk head.
- Plan: group B fix wave, parity plan.

### G-31: Wideband on the second ADC (ADC1) is kept, as NereusSDR's own identity choice, for the G2 and other two-ADC P2 radios

Thetis only enables ADC0 on every board; NereusSDR offers two ADCs on boards that have them.

- Ruling (JJ, 2026-09-25): `~/.config/nereus/work/checkpoint-next-operator-list.md:201`:
  "ANSWERED: wideband on the second ADC stays for the G2 and the other two-ADC Protocol 2
  radios ('1 that was a feature we worked on as part of our own identity')."
- Status: no code change required (kept as-is; the ruling is to not follow Thetis here). A
  checkpoint bench item was added: on the G2, zoom slice B's pan (the second ADC) out past its
  receiver's bandwidth and check the wide edges fill in.
- Plan: needs an ID (raised in the gaps review; not a plan task on its own).

## Approved continuation work

### G-09: Restrictive networks use JJ's approved layered connection plan

- Evidence/ruling: lane-B crew progress records JJ on 2026-09-27: "Yes build the layerd
  plan if this is the ideal way to handle naturalversal, given everything we know".
  The accepted plan uses direct wss, rendezvous ICE, the rendezvous WebSocket relay as a
  low-priority ICE candidate, direct-wss media fallback, fast failure detection and system
  proxy support. Transmit deadline behavior must be measured on TCP fallback paths.
- Status: R5 implementation and Linux/macOS traversal verification are in progress. The
  earlier measurement document's pending choice is superseded by this recorded approval.
- Plan: R-IOS-16, R-IOS-08; R5 remote access.

## Continuation findings and verification, 2026-09-27

### G-32: Slice publication and incoming DTLS records could race initialization

- Evidence: the resumed half-merge published an atomic slice audio view and applied the
  DTLS MTU before incoming records. Signed trunk merge `29ce3594f` contains both fixes.
- Ruling: JJ explicitly requested finishing this merge and its required verification.
- Status: application and all test targets built; focused checks passed. The full baseline
  completed in two recorded segments (865 entries, then the 83 interrupted/unstarted entries).
  Combined result: 944/948 passed. The four findings below remain tracked independently.
- Plan: R-R3-49; Core station prerequisites for phone media.

### G-33: Load exposes test setup races and unresolved timing failures

- Evidence: the baseline failed the control heartbeat setup, fake-radio meter startup,
  pairing helper startup and transmit event-loop gap checks. The heartbeat test enabled its
  100 ms deadline before the handshake completed. The fake radio stopped producing during
  synchronous DSP initialization. The latter now runs on its own thread; neither production
  watchdog was weakened. Pairing startup now reports phase timings without exposing wire data.
- Ruling: JJ: "A test that fails only under load is a finding: bring JJ its cause and a
  suggested fix." The earlier instruction also rejects merely extending the audio-clock limit.
- Status: integrated load-fix build passed; focused run passed 14/16 entries in 307.21 s.
  Both remote-audio entries failed (14 failing rows total) at observed load up to about 140.
  Evidence includes source/speaker timer delays and receiver worker wake gaps up to 201.5 ms.
  These are open findings, not waived tests. Separate independently paced source/device
  measurement is needed to distinguish harness starvation from receiver scheduling.
  Signed diagnostics `944311ba` are integrated as `2ebb270`: they capture source lateness,
  device/worker wake gaps and first excess concealment without a later finite-source timeout
  hiding it. The exact integrated case passed in 4.098 s; this does not close the load finding.
  Pairing's original slow phase and
  the 25.81 ms transmit timer gap (25 ms bound) remain unresolved. A standalone passing run
  does not close either finding. Suggested next investigation: phase measurements for pairing
  and a same-load unkeyed timer baseline plus key-call timing for transmit.
- Audio-clock evidence: unchanged simulated-hour coverage passed in 177.19 s in the baseline
  remainder. A separate measured run used about 105 CPU seconds; about 98 were in production
  audio push/resampling. Test tone generation used about 4 seconds. Existing limits retained.
- Plan: R-R3-49; load-failure work.

### G-34: Relay cleanup must retain the actual ICE socket and its local route

- Evidence: PeerConnection close returns before asynchronous ICE teardown; a fixed delay
  cannot prove that an old agent stopped using its local route. Shared phone patches retain
  the TURN agent through bounded release processing and any outstanding resolver. Core adds
  an optional lifetime owner, released only after actual agent destruction, then releases the
  route on its Qt thread. Overlapping media uses distinct UUID routes and a bounded count.
- Ruling: implementation under JJ's explicit authorization to finish R5 and Core phone
  prerequisites. No new operator behavior ruling is inferred. Existing identity checks and
  the transmit watchdog remain required.
- Status: signed implementation `50fcf932` built and verified in trunk: lifetime, rendezvous
  and provenance checks passed 4/4 in 67.90 s. Phone both-end interop passed all seven tests
  plus three release repeats with a staged copy of the verified helper. Signed trunk
  `c2b00a54` had a built Rock package that passed the isolated 15-second startup check
  before the later deployment.
  The subsequently verified `1f3cc251` Rock package is installed: the Core is active with
  no restarts, registered with the service, and `/etc/nereusd.conf` retains its measured
  hash (`core-gui-rock-1f3cc251-health.log`). An earlier build was found to have skipped
  vendor patches despite a successful tool exit. That evidence was withdrawn. The patch
  helper now rejects skipped application and materialized hashes are checked; that earlier
  skipped-patch build was never deployed.
- Plan: R5 remote access; phone relay cleanup and replacement.

### G-35: Pairing confirmation can be deleted before the socket drains

- Evidence: phone finding `46f01fca` identified a Core connection deleted immediately after
  sending pair.confirm. A Core regression reproduces premature deletion with queued output,
  including server destruction. The original proposed hunk also needed to register the closed
  handler before calling close, since a transport can signal closure synchronously.
- Ruling: ordinary correctness fix within JJ's authorized Core station work; no change to
  pairing trust, permissions, or timing policy is proposed.
- Status: signed implementation `ecb939955` integrated into trunk as `40244520c`. The regression first
  failed for delayed close and server destruction; all three closure cases pass after the fix.
  Session and pairing suites pass both in the lane (2/2, 26.07 s) and on the integrated
  working tree (2/2, 25.35 s). Only Core code is included; the phone's final-frame receive
  change remains phone-owned.
- Plan: R-IOS-08; pairing interoperability.

### G-36: Late packets from a retired media peer bypass duplicate filtering

- Evidence: R5 load testing reproduced a duplicate packet after replacement. The overlap
  filter can become inactive immediately at promotion while the old peer remains alive for
  two seconds. A second replacement can also overwrite the retiring peer owner.
- Ruling: correctness repair within JJ's approved R5 replacement work; no watchdog or test
  tolerance change. Preserve the current connection while retirement completes.
- Status (corrected 2026-09-28): built in `d82ee3428` (an ancestor of the trunk). Filtering
  continues until the retired peer stops (`src/gui/RemoteMediaController.cpp:3020-3037`), another
  replacement waits while a peer is retiring (`:2866`) and retries afterward (`:3098-3101`).
  Deterministic regressions: `lateOldRtpAfterPromotionNeverReachesTheJitterQueue` and
  `anotherMoveWaitsForRetirementAndReleasesTheOldPeer` (`tests/tst_media_replace.cpp:832`,
  `:892`). Remaining: no recorded verification under load.
- Plan: R5 media replacement and recovery.

### G-37: Local TCI I/Q labels every receiver as stream zero at 192 kHz

- Evidence: the raw I/Q audit found the local TCI producer used only untagged stream zero
  and a fixed 192 kHz rate despite the radio's actual binding and sample rate.
- Ruling: existing local/remote parity requirement applies in both directions. Preserve
  samples without resampling and report the actual supported receiver rate.
- Status: local `4da887a0` and remote `67d53723` plus lifetime correction `8b10acaa`
  are integrated. Receiver identity/rate, bounded ordered transport, budget admission,
  one-debit backpressure and reentrant peer retirement pass integrated focused tests.
  The 60-second sequence and TX-watchdog checks use simulated time; they are not
  physical-radio or wall-clock load acceptance. Remote TCI audio resampling remains
  a separate unfinished threading item.
- Plan: remote-window parity TCI raw I/Q.

### G-38: Settings reset labels and remote hygiene commands disagree with behavior

- Evidence: local Reset to defaults only repairs invalid board settings; Forget removes
  per-MAC settings. Existing station.forgetRadio instead removes a saved radio entry and
  refuses the current radio, so it cannot implement remote hygiene parity.
- Ruling: OPEN for reset semantics. JJ has been asked whether Reset should reset all
  saved settings for the radio or retain repair behavior under a clearer label.
- Status: capability-gated validate/forget hygiene operations from `ce40a2c` are
  integrated, with Core-owned MAC validation and paired-device/on-air mutation gates.
  Reset remains disabled remotely pending the ruling. Integrated app/Core build and
  20 focused tests passed (79.27 s), including setup, hygiene and link conformance.
- Plan: remote-window parity Setup diagnostics and preferences.

### G-39: Restrictive-network relay stress test falsely unkeys simulated TUNE

- Evidence: Linux isolated relay test with 2% loss and 150 ms RTT tripped the transmit
  watchdog 4.882 s after simulated TUNE began, after 403 ms without a keepalive. The session
  still reported connected. The accepted-gap histogram omitted the missing interval and
  the harness incorrectly printed a passing summary. This was synthetic; no radio keyed.
- Ruling: JJ's existing requirement applies: diagnose load failures and bring the cause
  and suggested fix. The 400 ms safety deadline remains unchanged.
- Original diagnosis: the original tests used the control-only keepalive
  fallback; a corrected production-follower harness also reproduced 402-403 ms false unkeys
  on web relay at 2% and 3% loss and direct WSS at 2%, each with 150 ms RTT. A bounded
  in-memory monotonic trace on the 2% web case captured client keepalive sequences 66-70
  accepted locally every about 100 ms and tag-2 WebSocket enqueue/send continuing with
  sampled backlog zero, while Core channel decode last accepted seq 66 at trip minus 402 ms
  and Core relay tag-2 receive stopped at trip minus 318 ms. The missing stage is after
  client `sendBinaryMessage` and before Core `RelayLeg::onMessage`. Service forwarding
  versus socket/TCP buffering or retransmission remains unproved. Control and media share
  the same TCP floor, so sending a duplicate over control has no independent deadline
  guarantee. No watchdog adjustment or production behavior change was made in that
  investigation. The old green accepted-gap summary is withdrawn; an absent interval
  is a failure.
- Status (2026-09-28): superseded by G-55 and G-70, which carry the built fix and its
  verification; this entry is kept as the investigation record.
- Current status: the subsequent bounded relay trace and independent authenticated
  watch implementation are recorded under G-55 and G-70. The actual Python-service
  acceptance now passes the four predeclared impaired-network rows plus the
  continuous second-receiver row, with the original 100 ms cadence and 400 ms safety
  cutoff. G-55 retains the exact measurements and scenario limits; this supersedes
  the earlier investigation-only status without erasing its failures. Installed
  service and live-phone checks remain separate from those isolated results.
- Plan: R5 restrictive-network transmit deadline.

### G-40: Code-only rendezvous reconnect stops at an already paired device

- Evidence: JJ's build 7 phone reached the Rock through pairing but received the
  existing-key refusal before SPAKE. Returning to a saved entry connected, while
  saved private addresses still delayed the intended address-free workflow.
- Ruling: JJ said the operator should enter the pairing code and nothing else,
  without managing saved IP addresses. This is part of the code-only objective.
- Status: Core correction verified by the full pairing test (35 Qt rows, 13.39 s)
  and three integrated session/readings/log regressions (43.42 s): complete code proof confirms an existing
  key without replacing its record; removal during the exchange invalidates it.
  Mailboxes remain pairing-only. The phone owns authenticated upsert and ordinary
  rendezvous reconnect without waiting for stale saved private addresses. The verified
  `1f3cc251` Core is now installed and healthy on Rock, registered to the service, with
  `/etc/nereusd.conf` unchanged by hash (`core-gui-rock-1f3cc251-health.log`). The phone's
  automatic initial-race and later path-switch work remains separately open.
- Plan: Core station pairing and R5 code-only access.

### G-41: Audio reset headphone wording conflicts with default-off behavior

- Evidence: remote parity acceptance B6.10 says reset reopens headphones, but
  AudioEngine::resetAudioSettings removes Headphones/Enabled and closes the headphone
  output to restore its default-off state.
- Ruling: OPEN on whether reset should preserve enabled headphones. Existing behavior
  is preserved while the independent immediate audio-output rebuild work proceeds.
- Status: Setup lane reports the discrepancy; no invented new headphone default.
- Plan: remote-window parity Setup diagnostics and preferences.

### G-42: Region and extended-transmit controls do not yet drive the Core TX gate

- Evidence: General Options writes Region as a string (`GeneralOptionsPage.cpp`), while
  the transmit gate reads numeric `BandPlanRegion` (`RadioModel.cpp`). The saved
  `ExtendedTxAllowed` and cross-band values are OperatorLocal in `SettingsScope`; the
  Core gate currently uses false constants instead of those values. A stale saved
  Extended=true activation and split RX/TX band policy need a source-based safety review.
  `BandPlanGuard::bandRangesFor` had TODOs for most country tables and fell back
  to US ranges; a stale comment calling the guard inert is false because MOX already
  invokes it. Thetis `Console.cs` 6780-6812 also checks non-CW transmit filter edges
  through `CheckValidTXFreq`, while Nereus's sole band-plan guard call passes carrier
  frequency only; `pushTxModeAndBandpass` uses positive audio-space filter values before
  TxChannel mode conversion. Filter-edge policy is another unresolved safety subgap.
- Ruling: existing parity work authorizes making these controls functional, but the
  precise safety policy and settings ownership migration remain OPEN. Do not import
  an operator-local value into the Core gate without that ruling.
- Status: country ranges from signed `7ffcc419` are integrated (corrected 2026-09-28: `7ffcc419`
  itself is not an ancestor of the trunk; the integrated commit is `e400bc464`, "Complete Thetis
  country transmit ranges", whose `BandPlanGuard.{h,cpp}` and `tst_band_plan_guard.cpp` changes
  match `7ffcc419`'s line for line, plus this addendum's own edit): all 24 supported
  regions and 264 HF ranges independently match Thetis. Integrated app/Core build and
  both band-plan and TX-frequency suites pass (1.11 s). Unknown enum values fail closed.
  The filter-edge gate now follows Thetis: signed TX-chain filter edges for voice/
  digital modes, carrier-only TUNE/CW, with XIT included. Malformed or out-of-range
  stored regions are rejected before enum conversion. New regressions reproduced
  eleven failures before the fix; four guard/filter suites pass and the corrected
  XIT suite passes with explicit whole-passband expectations. Core region writes
  now reject malformed/out-of-range values and both writes/removals wait for RX.
  Changes affecting another transmit holder use shared confirmation, with the
  on-air check repeated at proceed. Seven network regressions failed before this
  fix; the final catalog and confirmation suites pass 2/2 (12.17 s), including
  all 24 accepted regions and four confirmation write/remove cases. Region is now
  wired to numeric BandPlanRegion locally and on remote Core versions advertising
  transmitSettingsVersion 9. It disables while on air and rejects stale UI edits;
  invalid saved values show no selection until an explicit choice. Legacy Region
  text is preserved. Eight affected GUI/capability/session suites pass (42.38 s,
  host load 5.28/5.49/6.31); the app/Core builds and generated protocol-table check
  pass. Extended-transmit migration still awaits JJ's ruling. Thetis's current `Init60mChannels` has only UK, US and
  default cases, so missing extra country-channel arrays are not established; Nereus's
  explicit UK/Japan channelization is a native exception. The isolated parity lane is only
  disabling misleading interim UI and recording this gap. Ganymede and DisableHFPA remain
  the absent-producer classifications under G-21;
  the alleged missing TX-inhibit reader was a false positive, since production
  `attachRadioInput` is wired.
- Ruling (JJ, 2026-09-28 night): Extended transmit is one Core-owned station
  setting, default off; any old saved `ExtendedTxAllowed` is ignored and never
  turns it on; changing it needs transmit permission and is refused while anyone
  is transmitting; every device shows the same state. "Prevent TX on a different
  band" becomes a Core setting too, default off; the Thetis filter-edge check
  stays; every refusal gives a plain reason on every device.
- Settings migration (Extended): the Core's setting is a new key,
  `ExtendedTransmit` (`SettingsScope::Station`, "True"/"False", absent = off),
  read by `RadioModel::installBandPlanMoxCheck` at every key. The old
  `ExtendedTxAllowed` stays `OperatorLocal`, is never read and is not copied
  forward on any computer or Core, so the operator ticks Extended again. A
  device's write or removal needs the station transmit gate's permission and is
  refused on the air; `transmitSettingsVersion` 12 offers it and General
  Options' Extended box (local window, remote window, phone Setup description)
  shows and changes the Core's value. No transverter transmit path exists yet,
  so Thetis's `tx_xvtr_index > -1` bypass has no counterpart.
- Settings migration (Prevent TX on a different band, 2026-09-29): the key
  name stays `PreventTxOnDifferentBandToRx` and moves from `OperatorLocal` to
  `SettingsScope::Station` ("True"/"False", absent = off, Thetis's default at
  console.cs:20843 [v2.10.3.15]). Following the Extended precedent, nothing is
  copied between computers: the Core's own saved value carries forward (it can
  only restrict transmit), and a remote computer's old value is no longer read.
  Writes take the same gates as `ExtendedTransmit`; `transmitSettingsVersion`
  14 offers it and the box is shown (never hidden) in the local window, remote
  windows and the phone Setup description. The comparison changes with JJ's
  ruling: Thetis compares the TX VFO's band with RX1's and only in split
  (console.cs:29451-29465 [v2.10.3.15]); a station has no split, so the Core
  refuses a key only when the transmitting slice is not the device's active
  (listening) slice and is on a different band from it. A slice parked on
  another band does not block a key on the active slice, and other devices'
  slices never count. It runs after
  the mode allow-list and before the US 60 m and band edge checks, as in Thetis,
  and keeps the band plan refusal code with a sentence naming both bands.
  Thetis's CWX StopEverything on this refusal and its `!calibrating` condition
  have no counterpart yet.
- Plan: remote-window parity Setup and transmit gate safety.

### G-43: Transmit frequency guard omits XIT offset

- Evidence: `RadioModel::installBandPlanMoxCheck` passes the transmit-bound
  slice's dial frequency to `BandPlanGuard::checkMoxAllowed`, while the hardware
  transmit path uses `txFrequencyForSlice(slice)` including XIT. Thetis
  `Console.cs` lines 29440-29450 applies XIT before `CheckValidTXFreq`
  at line 29486.
- Ruling: the existing source-faithful transmit guard objective authorizes this narrow
  fix, without a broader Extended/cross-band policy change.
- Status: signed fix `d83b4e88c` is integrated: the guard uses the actual TX carrier
  including enabled XIT for both frequency and band. The integrated transmit-frequency
  test passed in the 20-test setup integration run; no physical transmission was used.
- Plan: transmit guard parity and safety.

### G-44: Saved legacy control-channel result can permanently suppress recovery

- Evidence: station-link sections 6.3 and 21.1, `CoreTargetStore`, and
  `StationClient::newRacer` retain a saved `controlChannelVersion` of 0 and
  skip the service rung on every later race. An upgraded Core can therefore
  remain unreachable through rendezvous after a stale direct address fails.
- Ruling: JJ approved automatic direct/manual/rendezvous recovery. The lead's
  bounded implementation contract is a persisted, authenticated negative
  observation fresh for five minutes; older records without a timestamp,
  expired/future timestamps, and unparseable timestamps are unknown for
  discovery. A real network-generation change invalidates the observation
  once, without clearing it on every retry. A fresh authenticated 0 suppresses
  the service rung until expiry or network change; authenticated 1 clears the
  negative timestamp. Failed or unanswered probes do not renew 0. Full
  same-identity code pairing invalidates the negative observation. Existing
  identity/certificate checks, single authenticated race winner, bounded
  deadlines and retries, and `relayAllowed` remain in force.
- Status: desktop implementation `59edfcfa` and shared link documentation `31536085`
  are integrated. Store, selector, GUI controller, remote controls and path-race suites
  pass on the integrated tree. No identity checks were weakened. Authenticated-zero
  end-to-end recording remains a fixture coverage limit: current bound Core fixtures
  advertise 1. Phone implementation and real automatic-recovery acceptance remain
  open. This lead contract does not assert a separate JJ ruling on the five-minute value.
- Plan: automatic direct/manual/rendezvous recovery.

### G-45: Full-suite build omitted the slice audio race executable

- Evidence: the immutable Linux checkpoint registered `tst_slice_audio_view_race`
  after creating `all_tests` and label aggregates. CTest found it, but the full
  build never produced it, so the suite reported Not Run.
- Ruling: JJ's requirement to run the required tests authorizes correcting build
  coverage; no deadline or assertion changes are needed.
- Status: registration moved before aggregate creation. Regenerated Ninja graph
  includes the executable in all_tests, tests_core and tests_models; the freshly
  built integrated race test passes (0.93 s). The Linux continuation will build
  this target explicitly against its immutable checkpoint and record its result.
  The later Linux run found the same ordering error in three hosting tests:
  binary discovery, service management and the desktop hosting runtime. Their
  registrations now precede both aggregates; all three rebuilt macOS suites
  pass (1.16 s). Corrected Linux full-suite coverage remains to be verified.
- Status (corrected 2026-09-28): built and covered on Linux. In the trunk every registration
  precedes the aggregates (`tests/CMakeLists.txt:9038` race test, `:9042-9046` the three hosting
  tests, `all_tests` at `:9063`, label aggregates after it). The immutable Linux `5c4ca88d` full
  run built and ran all four: `tst_slice_audio_view_race` passed (2.10 s),
  `tst_station_binary_locator` and `tst_station_service_manager` passed, and
  `tst_gui_desktop_station_runtime` ran and failed on its own assertions, the fixture defect
  fixed under G-112 (`48764cd41`, `d49ef4e48`). Log: core-gui-linux-5c4ca88d-build-and-test.log.
- Plan: required full-suite verification.

### G-46: Container filter display lacks slice-correct spectrum and overlays

- Evidence: `FilterDisplayItem::setSpectrumData` has no production caller. Existing
  FIR delivery is real, but broadcasts slice A's curve to all containers; filter
  edge and notch setters also have no callers. Thetis MiniSpec uses 1024 pixels
  and an independent RF window; the item's 512-pixel attribution is incorrect.
  Existing remote pan frames have their own window and cannot be copied blindly.
- Ruling: JJ's complete remote-window parity objective includes finishing this
  feature. Exact mini endpoint, detector and overlay implementation remains a lead
  source-based design decision; no separate JJ ruling is claimed.
- Status: built. Source contract records per-container slice identity,
  stream geometry, dBm reduction, lifetime and remote grant requirements. The
  temporary hidden classification is not completion of this feature. Core now
  negotiates a mini display role only with declaring peers and applies TX takeover
  only to the actual transmitting slice. Surface/conformance/session checks and
  the full TX-display suite pass. GUI and independent local/remote TX analyzer
  wiring are now implemented in signed `29ebbf388`, including panel header/body
  fanout and a separate TX analyzer. The integrated app/Core build and nine
  focused suites passed (74.86 seconds). The source-queue overflow fix in G-63
  now permits production visibility in both local and remote containers. The
  production-gate tests cover rendering and the Add menu without test overrides.
  All ten affected integrated suites passed (71.57 seconds) with the gate open.
  Physical radio acceptance remains separate from the automated checks.
  Checked 2026-09-28: the gate is open in source (`src/gui/UnbuiltFeatures.cpp:273`, opened by
  `8bad71253`). The enum comment at `src/gui/UnbuiltFeatures.h:125` still says the gate awaits
  loaded FFT startup acceptance; that code comment is stale and needs a code change, not an
  addendum change.
- Plan: remote-window parity container meters and filter display.

### G-47: Expected transmit silence falsely restarts relay media

- Evidence: the desktop RemoteMediaController diagnoses a relayed audio stall after
  three seconds without RTP while speakers are wanted. The Core intentionally gates
  the TX-bound receive stream during MOX/TUNE, so legitimate silence triggers media
  teardown. The seeded synthetic TUNE run decoded audio before keying, then lost its
  receiver during TUNE even though the Core sent audio again after unkeying. The
  fixture did not connect the recovery signal, explaining why it stayed stopped.
- Ruling: JJ requires load failures to be diagnosed and fixed. The lead's correction
  keeps the existing receive-stall deadline but excludes authenticated keyed/tuning/
  transmit-tail states; a true return to receive starts a fresh interval if real RTP
  previously armed it. Unrelated state updates must not extend that interval.
- Status: built in signed lane commits `fa5e9f46` and `5aa1b55c`, integrated with
  fresh media-tunnel, remote-controller and TX-watchdog suites passing 3/3 (65.41 s).
  Synthetic TUNE coverage proves media/display survival, continued tunnel heartbeat
  tracking, audio resumption and genuine post-unkey stall recovery despite unrelated
  state updates. The independent 400 ms transmit watchdog and earlier false-unkey
  finding stay unchanged. The phone controller has no equivalent audio-only stall
  timer and recorded this requirement for its upcoming relay implementation.
- Plan: R5 media recovery and load verification.

### G-48: Linux fixtures assume the developer machine's environment

- Evidence: checkpoint verification exposed an absolute nice-level request that
  requires privilege when the runner is already at nice 10; a socket-path test
  conflating Qt's isolated config directory with the packaged service directory;
  and FreeDV checks passing iterators from distinct temporary event lists. The
  path-racer fixture also assumes a 192.168.1 subnet and an unanswered localhost
  connection survives until asynchronous name resolution completes.
- Ruling: JJ requires failures under real load to be explained and fixed. These
  test-fixture corrections preserve production behavior and existing deadlines.
- Status: signed `710c2970` repairs the first three causes. Fresh macOS tests pass
  3/3; an isolated Linux checkpoint plus the exact patch also passes 3/3. The
  path-racer fixture now uses numeric loopback, a controlled ephemeral TCP listener
  and explicit lookup completion; fresh macOS (21.07 s) and isolated Linux patched
  checkpoint (14.44 s) suites pass. Earlier Linux environment repairs
  supplied a non-loopback internal interface for ICE and existing offline Python
  service dependencies. Phase-specific passes are not a green current-branch suite.
- Plan: cross-platform verification for the whole Core/GUI PR.

### G-49: Supported CPU spectrum renderer did not compile

- Evidence: macOS Qt's offscreen platform cannot create QRhi. Building the supported
  QPainter renderer exposed `m_visibleBinCount` declared only under the GPU guard,
  although the shared trace query and MOX/context reset methods reference it.
- Ruling: JJ's requirement to finish and verify cross-platform display parity covers
  this compile repair. No display design or spectrum calculations change.
- Status: signed lane `aa9b26bf` moves the zero-initialized count outside the guard.
  Fresh CPU capture fixtures pass and the actual images were inspected by both Core
  and phone leads. The CPU painter still draws from rendered pixels and leaves this
  GPU count zero. Fresh integrated GPU builds and the two existing display/window
  suites pass (32.89 s); opt-in captures remain CPU-only. The synthetic full-window
  capture injects trace data; full-window media delivery, shared view, DUP, XIT and
  hardware/RF acceptance remain open.
- Plan: remote-window transmit display parity and verification.

### G-51: Remote TCI audio processing can fall behind under variable host load

- Evidence: a paired 30-second test with eight TCI clients per receiver at 384 kHz
  stereo Float32 accepted and decoded all 7,500 input packets per receiver, but one
  run saw 96/102 ms worker wake gaps and discarded about 2.31/2.35 million client
  history frames. Another run on the same implementation had no history loss.
- Ruling: JJ requires failures at real load to be diagnosed, not hidden by relaxed
  deadlines or waiting for the machine to quiet down.
- Status: OPEN investigation. Four alternating yield/no-yield runs under concurrent
  builds all retained input packets and history; both variants had roughly 5-6 ms
  maximum wake gaps. Removing the explicit scheduler yield is not an established
  explanation for the severe run. The bounded worker implementation and lifetime
  tests continue separately; these passing repeats do not erase that finding.
  The worker migration is now integrated with eight-client admission, bounded
  conversion quanta/history/mailboxes and two-result UI ticks. Lead review also
  fixed the timer caller continuing after a send/notice retired its server; a
  real timer regression covers both notice and notice-clear destruction. Fresh
  app/Core builds and four TCI/VAX/controller suites pass (65.01 s). This verifies
  functional integration, not resolution of the severe load stall.
  A subsequent bounded 30-second baseline/load pair measured CPU and wall
  time inside each conversion quantum. Loaded maxima of 8.51/7.62 ms included
  at least 8.32/7.39 ms not executing on the worker thread; all 7,500 packets
  per receiver were decoded without history loss. This narrows measurement
  toward scheduling or blocking but neither reproduces nor explains the
  earlier severe stall. Temporary instrumentation was restored byte-for-byte;
  the rebuilt app/Core and both full restored TCI suites pass (19.62 s).
- Plan: remote-window TCI audio parity and real-load acceptance.

### G-52: Catalog capability-order fixture missed two integrated appended fields

- Evidence: the full catalog suite expected its tail to end at
  `mediaRelayRoutingVersion`, although integrated Core capabilities also append
  `remoteIqVersion` and `txModMonitorVersion`. The wire fixtures already covered
  both; this separate catalog test had not been included in those focused runs.
- Ruling: ordinary verification repair under JJ's complete-and-test instruction.
  Preserve the exact order assertion and add the actual appended fields.
- Status: corrected fixture passes in the full catalog suite. Integration also
  corrected the station-session append/old-peer omission checks and the surface
  manifest's misplaced txModMonitor.reset command. Regeneration was checked to
  change only command order, with identical command contents and every other
  section unchanged. Production wire order is unchanged; no assertion or deadline
  was removed.
- Plan: integrated Core protocol verification.

### G-53: Admission cleanup could outlive the Core server

- Evidence: fifth-device review found raw server captures in shared admission/drop
  cleanup and accesses after callbacks that can retire the server. The new registry
  changed signal, incumbent session-end send and slice removal make these paths
  relevant to replacement. The loopback send observer also needed a lifetime guard.
- Ruling: covered by JJ's instruction to finish and verify every gap; preserve
  admission and transmit ownership rules while stopping cleanup after destruction.
- Status: fifth-device admission, the original 60-second question, 180-second away
  place, cancellation, stale-answer refresh and reserved replacement are integrated.
  QPointer guards cover registry notification, admission/drop cleanup and slice
  release; three added regressions destroy the server at admission, incumbent end
  and slice removal. Ten integrated device/session/conformance targets passed;
  the sole surface-order failure was repaired and both the full fifth-device and
  surface suites then passed (7.30 s, load 7.11/8.55/7.86). App/Core builds passed.
  PlaceFreed's 180-second case is covered by Core integration; its wire fixture
  awaits a runner that can defer the initially opened 30-second pre-auth connection.
- Plan: Core device admission and multi-device lifecycle.

### G-54: Noise-reduction Setup stayed bound to the previously selected receiver

- Evidence: NR1-4, DFNR and MNR controls captured the active slice when Setup
  opened. Selecting another receiver could leave edits targeting the old one.
- Ruling: JJ's goal requires finishing every remote-window parity task and
  building every discovered gap. The existing selected-receiver contract applies;
  this is its implementation repair, without a new operator-policy decision.
- Status: NR/ANF rebuilds its controls on selection changes, destroys the old
  gesture widgets and connections, retains the selected tab, and disables edits
  without a receiver. Integrated app/Core build and six Setup/NR suites passed
  (2.70 s), including A-to-B edits, removal fallback and old-widget destruction.
  The AGC/ALC page now also rebuilds on selected-receiver changes. DSP
  descriptions cover nine pages with their implemented scalar controls; composite
  NNR, CFC and Filter Presets work remains explicitly incomplete. TNF now has
  an explicitly negotiated v2 table with existing Core edit commands; v1 clients
  retain their scalar controls. App/Core build and five integrated suites passed
  (32.37 s), including wire version fitting and shared-edit confirmation. The
  phone's generic Setup renderer remains unbuilt.
- Plan: remote-window selected-receiver parity and Core DSP Setup descriptions.

### G-55: TCP relay loss can delay transmit keepalives beyond the safety cutoff

- Evidence: a seeded 3% loss, 150 ms RTT harness run stopped a 25-second
  simulated TUNE at 15.082 seconds. Client-to-front frames stalled for 376.748 ms;
  service forwarding took about 0.24 ms. Core first received the next burst
  400.441 ms after its last validated heartbeat, already beyond the 400 ms limit.
  TCP retransmission/head-of-line delay is the strongest inference; packet-specific
  retransmission was not captured. The later 2.5 ms callback delay is secondary.
- Ruling: JJ requires the cause and a suggested fix for load failures. The existing
  400 ms safety cutoff remains unchanged. The later independent, authenticated
  watch contract is implemented under the approved R5 completion scope (G-70).
- Original diagnosis and rejected alternatives are recorded in
  `~/.config/nereus/work/core-gui-r5-loss-diagnosis-report.md`; no production change
  or deadline relaxation. Follow-up diagnostic cadence trials found two failures
  in three 50 ms samples; three 25 ms samples passed 25 seconds each with maximum
  received gaps of 249/277/282 ms. These short stochastic samples do not establish
  an acceptance rate. The override was restored exactly afterward; production
  remains 100 ms. The longer 25 ms experiment also failed all four predeclared 60-second
  rows at 3% loss/75 ms each way: both relay and direct WSS. Trips occurred at
  55.310, 59.714, 20.733 and 1.456 seconds into requested TUNE, with 401-403 ms
  silence. Client production continued. A timer-only fix is rejected; separately
  delivered authenticated liveness needs an explicit contract and verification.
  Those failures establish the need for the separately delivered watch.
  A separate diagnostic used two independent TLS sockets at the unchanged 100 ms
  cadence for three 60-second samples. Every individual socket exceeded 400 ms;
  combined newer-sequence delivery had maximum observed gaps of 273/354/152 ms.
  This supports investigating independent delivery, not a product-fix claim: it
  omitted application authentication, audio, relay framing and startup/end gaps.
  Evidence and required protocol constraints are in
  `~/.config/nereus/work/core-gui-r5-independent-flow-probe.md`.
- Current status: built and verified with the actual Python rendezvous/relay
  services in isolated Linux namespaces (`9406d54f`, `5c051110`). At unchanged
  100 ms cadence and 400 ms cutoff, the four predeclared direct-WSS/web-relay
  rows (seeds 20261011 and 20261013, 3% loss, 150 ms RTT) each passed once:
  Core-keyed intervals 59964/60140/60033/60000 ms, maximum accepted heartbeat
  gaps 324/321/352/317 ms, and zero watchdog trips. The checker requires
  59900 ms Core coverage for these client-timed rows, explicit release, audible
  receive recovery and continuing keyed display. This is bounded scenario
  evidence, not a statistical reliability guarantee. A separate second-receiver
  row proves continuous receive audio while logically keyed: 6000/6000 audible
  10 ms blocks, 60/60 audible seconds, 604 accepted heartbeats, maximum gap
  279 ms, and a 60340 ms Core-keyed interval. Its timing correction is G-80.
  The strengthened checker also verifies each watch socket differs from that
  endpoint's media socket; replay against saved evidence passed all five accepted
  rows. Original failed evidence is retained. Old-service watch-version-zero
  fallback passed both transports. App/Core and nine rebuilt focused suites
  pass together (63.80 s, load 5.42/4.56/5.07). No radio or deployed service was
  used or changed; live-phone and installed-service acceptance remains separate.
- Plan: R5 direct-WSS/web-relay transmit under impaired networks.

### G-56: PureSignal continuously occupied the display send window

- Evidence: full media verification produced zero spectrum frames across 24
  congested-window cycles despite fresh FFT frames. The IQ integration had changed
  the sender to try PureSignal first every time, bypassing prior alternation.
- Ruling: JJ requires diagnosing load failures and fixing plan gaps. Restore the
  existing fairness contract; no deadline or assertion change is needed.
- Status: built. The sender alternates PureSignal and spectrum with fallback,
  preserves bounded IQ work and checks lifetime/peer/epoch after each callback.
  The original regression, ten related cases and all 86 daemon checks pass
  (full suite 37.75 s). App/Core/helper rebuild and TX-display suite pass.
- Plan: shared display bandwidth, PureSignal and raw-IQ integration.

### G-57: One protocol test failed while opening its encrypted connection

- Evidence: accessory conformance had 212 passing cases and one spots fixture
  failing before playback in DataChannelBridge start/open (15-second bound).
  The message does not distinguish immediate start failure from handshake timeout.
  Adjacent cases and a full rerun passed, but that does not establish a cause.
- Ruling: JJ requires cause and suggested fix for load failures; no test deadline
  increase or assertion waiver is authorized.
- Status: OPEN cause. Signed fda40c1e adds stage diagnostics for both starts,
  open callbacks and failure callbacks, with elapsed time and the unchanged
  15-second bound. Three bounded lane conformance runs passed without reproducing
  the failure; that is not a fix. First-failure logs remain preserved.
  A later full data-channel suite also failed in
  `aWholeSessionRunsOverTheChannel` before the offerer opened (49 cases passed,
  one failed, load 18.87/38.90/30.75). Signed `46c984d9` preserves the same
  15-second bound and adds bounded library logs, elapsed/open/failure state and
  safe fixture cleanup. Three predetermined full-suite observations and the
  final diagnostic-lifetime run passed; the last took 14.60 s. The original
  failure remains in `core-gui-export-owned-cancel-ctest.log`; passing reruns do
  not establish a cause. The focused source audit found that the fixture did
  not record description/candidate acceptance, but did not establish this as
  the cause. Signed `c0260823` adds bounded acceptance/state events and fixed
  protocol-stage markers without raw SDP, candidates or certificates. Its
  50-case run passed (14.14 s), followed by the six-suite trunk integration
  (44.26 s). The production deadline is unchanged; the cause remains OPEN.
  A later eleven-suite parallel PA-readout check reproduced the failure in
  `session-verbs-tgxl`: both starts succeeded, neither side opened or failed,
  and the unchanged bound expired after 15,001 ms. Ten suites passed. The
  failing row later passed alone in 0.52 s and the full conformance suite
  passed alone in 65.26 s; those results do not close the load failure.
  The original parallel log and stage state are retained for investigation.
  The conformance join now also captures the existing safe fixed library
  stages and structural description/candidate acceptance events, with bounded
  shared lifetime and the same 15-second deadline. Its failure assertion
  includes the evidence and current load, so Qt's exhausted warning allowance
  cannot hide it. No connection material is retained. The rebuilt conformance,
  data-channel and station suites pass (42.87 s; initial load
  12.96/14.88/14.42), without reproducing the failure. This improves the next
  observation; the cause and production repair remain open.
- Plan: integrated protocol reliability under real host load.

### G-58: Setting-backed toggles need explicit boolean string encoding

- Evidence: DSP cache and some General settings readers require the exact strings
  True/False; a JSON boolean can be stored as lowercase true/false and read as off.
  Five General toggles and two DSP cache toggles need round-trip verification.
- Ruling: accurate Setup parity is required by JJ's goal. The lead selected an
  explicit descriptor encoding, preserving existing settings semantics. The phone
  team confirmed the exact True/False contract; this is an implementation choice
  under the authorized goal, not a separate operator ruling.
- Status: Core encoding built and verified. All seven controls use exact
  valueEncoding; malformed/partial/unknown mappings are rejected. Integrated tests
  write both directions through SettingsProxyServer, verify persisted strings and
  receive-only, desktop/phone timeout, watchdog and cache readers. Six focused
  suites pass (3.25 s), as does the app/Core build. Phone renderer work remains
  phone-owned; this does not claim that UI is complete.
- Plan: Core Setup descriptions and phone renderer contract.

### G-59: Direct phone tests needed an explicit loopback-only Core fixture

- Evidence: the existing helper's --listen option bound every interface. The
  phone team's direct fallback tests require a listener restricted to this Mac.
- Ruling: authorized Core/phone integration testing; preserve the existing default
  and add an explicit test-only binding option.
- Status: built. --listen-loopback binds IPv4 127.0.0.1 and requires a valid port.
  Four invalid-argument checks and live socket inspection passed with synthetic
  media. The immutable helper core-helper-5068cfca was packaged, its hashes, signature
  and bundled runtime libraries verified, and delivered to the phone chat. Old
  fixtures remain preserved. No RF or radio configuration was involved.
- Plan: real Core/phone direct-connection interoperability.

### G-60: CFC non-ten-band layouts do not apply per-band edits

- Evidence: TxCfcDialog::pushCfcProfileToModel returns after the global gains
  when either curve has other than ten points. The visible five- and eighteen-band
  layouts therefore do not apply their per-band edits to the transmit model.
  RadioModel also forwards empty Q vectors despite the visible per-band Q controls.
  Thetis saves CFC as two curve JSON objects separated by `<SEP>` inside its
  compressed envelope (frmCFCConfig.cs, ConfigData). The current remote validator
  instead uses the single-curve TX EQ decoder, so it cannot accept that paired
  Thetis CFC payload. The saved opaque blob is not applied by RadioModel either.
- Ruling: JJ requires every gap found in this effort to be built. No separate
  ruling on CFC behavior has been requested or received; preserve the intended
  Thetis-derived controls while completing their Core apply path.
- Status: BUILT in signed `efbd90b`, integrated with the current Core and GUI.
  The paired codec preserves independent compression/post-EQ frequency grids,
  applies five/ten/eighteen bands and both Q arrays, and retains opaque saved
  data with the legacy fallback. Ten-band compatibility edits preserve the
  paired curves; incompatible old-width edits are refused. Profile restoration
  and direct settings reload apply one final coherent CFC state, including
  enabled flags; nested curve edits retain the newest value. The integrated
  app/Core build and all five focused suites passed (7.28 seconds). Tests use
  a TX-channel seam, with no physical RF or audio-device acceptance claimed.
- Plan: complete DSP Setup parity and live Core application.

### G-61: Phone transmit Setup needs the desktop's on-air lock

- Evidence: remote desktop Setup disables transmit settings while the Core is
  on air, while the Core accepts some ordinary live transmit-setting edits.
  Initial Transmit/Audio descriptions therefore allowed more than that window.
- Ruling: JJ answered "Lock Setup controls while transmitting (recommended)".
- Status: built and verified. All 27 new transmit-related Setup controls require
  offAir; the Core descriptor validators reject a missing gate. Missing or stale
  txState disables them; keyed, tuning, twoTone and txEnding must all be false.
  Integrated app/Core build and four focused suites passed (5.17 s). Receive
  controls are outside this discrepancy.
  (2026-09-28) Documentation fix `26e89c507` on the trunk: the Setup description document's V7
  Appearance note now says only controls carrying the offAir gate lock while keyed, plus the
  session freshness rule; the source has no whole-Setup on-air lock (a remote window disables
  only transmit settings, `MainWindow::transmitSettingsPermitted`).
- Plan: Transmit/Audio Setup parity and consistent phone behavior.

### G-62: Background-service management needs reliable state and duration handling

- Evidence: review of the first packaging implementation found a Windows running
  check tied to English status text, no explicit unlimited task execution time,
  and stop treating a failed status query as an already stopped service. On macOS,
  writing a new plist while a stopped definition remains loaded can restart the
  old executable/profile arguments.
- Ruling: JJ's goal includes a Core that keeps running after the desktop closes.
  No separate operator choice has been requested or received for these defects;
  the proposed fixes preserve that requirement and report query errors honestly.
- Status: built and integrated. Numeric Windows state, unlimited runtime,
  query-error handling and idle macOS definition refresh are covered. Pending
  states are refused explicitly rather than reported stopped. The Debian workflow
  also uses proper source-control metadata and verified dependency extraction.
  Integrated app/Core/helper build, two focused suites (1.01 s), strict macOS
  bundle signature and bundled daemon help invocation passed. Linux/Windows
  release artifacts and notarization still need their workflow verification.
  No real service or radio configuration was changed.
- Plan: station packaging and background-service manager.

### G-63: Mini-display tests missed the initial receive-context deadline under load

- Evidence: the mini-display lane observed two five-second receive-setup waits
  fail before transmit assertions. Later full runs passed under concurrent builds,
  and a further instrumented failure located the delay before the first FFT.
  The session was ready at 53 ms and grants arrived at 56 ms; at 4592 ms
  the source had published no FFT and completed no I/Q handoff after 174
  synthetic submissions, with 86 dropped inputs. A handoff is counted after
  feedIQ returns, so this does not distinguish a blocked first feed from
  a worker that has not begun draining its queue.
- Ruling: JJ requires the cause and a suggested fix for failures under load;
  increasing the deadline or treating a rerun as a fix is not authorized.
- Status: reproduced and fixed. Two 2052-float packets submitted before a worker
  drain exceeded the 4096-float queue and discarded both inputs, matching the
  observed near-one-drop-per-pair counts. A deterministic paused-worker test
  failed on the old implementation. Overflow now discards the older pending
  samples, retains the newest valid whole packet, and marks the FFT discontinuity.
  The regression also compares output against a clean reference after a prior
  partial FFT, proving samples are not joined across the gap. Oversized and
  inactive inputs remain rejected; queue bounds and deadlines are unchanged.
  The synthetic fixture now feeds shared DDCs once rather than once per slice.
  Integrated source/TX-display suites passed (9.65 seconds); stage diagnostics
  remain available for any distinct future stall. The original log had no worker
  stack, so this does not exclude unrelated stalls or claim hardware acceptance.
- Plan: mini-spectrum display integration and tests at real load.

### G-64: The standalone Core package omits its DeepFilterNet model

- Evidence: the DeepFilterNet model install rule in `CMakeLists.txt` belongs only
  to the default component; `cmake --install --component nereusd` excludes it.
  The proposed Pi image also disables DFNR because its prebuilt Rust archive has
  no verified Pi 4 CPU baseline. The existing setup script prefers that archive.
- Ruling: JJ's complete station objective authorizes correcting packaging. Lead
  implementation decision: preserve the desktop install, add the daemon model
  component, and build the image dependency from pinned source with explicit
  Pi 4 CPU flags. No separate operator request to omit DFNR is claimed.
- Status: implemented in signed `043508ce`. A separate daemon-component rule
  preserves the desktop install. The Pi workflow now requires DFNR from pinned
  source/toolchain with an audited dependency lock and explicit ARMv8-A flags;
  it checks the model and compiler definition rather than silently disabling it.
  Six integrated stage checks passed, including execution of the actual CMake
  install rules for both daemon and desktop components; shell lint and workflow
  structure checks passed. Linux arm64 artifact construction and device boot
  remain unverified. The Rock's installed model was separately staged and
  hash-verified; this packaging finding does not claim it is missing.
- Plan: station binary packaging and small-computer image.

### G-65: Independent heartbeats can outlive a delayed transmit-release command

- Evidence: deterministic tests of `RemoteTransmitClient` and `RemoteTxWatchdog`
  hold primary command delivery while permitting media heartbeats. The old key
  remains alive through 600 ms after MOX/program release with VOX armed, or after
  MOX release followed by an immediate new key request with VOX off. Three safety
  assertions fail; seventeen existing/characterization cases pass. This is a
  modeled transport split, not an observed RF event.
- Ruling: JJ requires diagnosing load failures and preserving the safety cutoff.
  Lead safety decision: pause ALL heartbeat paths while any off command is being
  dispatched or awaiting its own accepted Core result. A command number does not
  prove enqueue: the current transport API can silently drop a send. Failed or
  refused releases remain fenced until session reset and reject new on requests
  with reconnect guidance. An accepted result proves delivery, not that a newer
  key is off. An unrelated accepted TUNE-off cannot discharge a failed MOX release.
  This supersedes the earlier primary-only heartbeat proposal.
- Status: built in signed worker `b8ea3c19` and integrated with an additional
  callback regression: a release during one heartbeat callback must prevent a
  fallback send in the same tick. That regression failed before the added gate.
  The integrated app/Core build and both focused suites passed (12.04 s), including
  client/watchdog failures with both heartbeat paths enabled and actual Core
  tuner-TUNE acknowledgement routing. Callback destruction is guarded as well.
  The 100 ms cadence and 400 ms cutoff are unchanged. This is deterministic and
  loopback evidence; restrictive-network and hardware acceptance remain open.
- Plan: remote transmit safety and restrictive-network liveness.

### G-66: Web relay limits still assume only two device sessions per Core

- Evidence: `nereus_relay/config.py` and `relay.conf.sample` default to two
  sessions per station and eight physical connections per address. The older
  rendezvous design section 12.5 explicitly specifies two, while the newer
  several-devices design requires four devices and a fifth-device replacement
  handshake. TURN's allocation quota is a different limit and does not fix this.
  Proposed independent watch sockets would also consume physical connection
  capacity, even when attached to the same logical session.
- Ruling: JJ's complete Core station and R5 objective includes the approved
  four-device and replacement behavior. Lead implementation must size bounded
  relay admission for that contract, including temporary replacement paths;
  no permission to change a deployed service is inferred from this finding.
- Status: built bounded primary and auxiliary admission. Nine logical sessions
  permit four current paths, four reconnect introductions and a fifth-device
  question; thirty-six physical connections allow both primary and watch pairs
  to share one address group. The global sixteen-session cap is unchanged.
  Watch sockets require the same live primary pair, station, session and expiry;
  they never create a logical session or extend its idle lifetime. Primary end
  or replacement retires both watch legs. Separate bounded watch queues and
  rates also charge the existing aggregate control budget.
  Rendezvous negotiates purpose-separated watch grants only when explicitly
  enabled and both peers declare version 1. Its default remains disabled;
  enabled service hello uses protocol version 2. Old peers retain ordinary
  version-1 primary grants. The integrated Python suite passed 478 tests
  (23.03 seconds), including real loopback forwarding with service-issued grants.
  C++ service negotiation is also built, disabled by default: explicit opt-in
  and a version-2-or-newer service declaring watch version 1 are required. Unknown
  future watch versions and older services retain ordinary primary behavior;
  reconnect clears negotiated state. The integrated app/Core build and full
  rendezvous client suite passed (64.42 seconds), with strict optional-field,
  station/client negotiation and old-service reconnect regressions.
  The C++ watch-purpose RelayLeg is built as a separate outer WebSocket with
  one loopback UDP source, tag 3 only, no media/routed claims and a bounded
  16-frame/8192-byte queue (`218dd70e`). Its integrated app/Core build and both
  relay suites passed (11.66 seconds); ending the watch leaves primary forwarding
  intact. A dedicated watch DTLS adapter is also built (`ff06a5ea`):
  one reliable ordered binary channel, bounded 33-byte frames, separate peer,
  no media or ordinary candidate trickle, and owned relay-loopback candidates
  only. Real DTLS tests verify the presented Core certificate and raw watch
  frames; the integrated app/Core build and four transport suites passed
  (19.95 seconds). A further real-RelayLeg test (`9c68e699`) uses four separate
  outer WSS sockets and two real DTLS watch peers through a local protocol
  player. It verifies raw attach/ack/heartbeat frames, the actual Core digest
  distinct from the relay TLS certificate, tag-3 forwarding and source retirement.
  The integrated full data-channel suite passed (14.09 seconds). The relay
  player does not replace production Python service or loss-test acceptance.
  No deployed service was changed.
  The primary transport now retains its negotiated watch grant before the RV
  introduction is retired. It permits new watch admission only on that exact
  live WebSocket-relay primary with both relay legs present and an unexpired
  grant. Admission expiry does not invalidate an already admitted watch;
  primary close clears the context. The app/Core build and four relevant suites
  passed (68.66 seconds), including expiry, missing peer and selected-route
  boundaries. Core/client owner integration subsequently landed in `698615768`
  and `899e9b742`; `9406d54f` enables explicit opt-in at both RV owners.
  The service default remains disabled and older-service fallback remains intact.
  The actual-service fixed-loss acceptance and simultaneous second-receiver
  audio evidence are recorded under G-55. Live-service deployment is still open.
- Plan: several-device capacity and restrictive-network relay access.

### G-67: Releasing an offline Core must preserve receiver edits and saved layouts

- Evidence: receiver saves are intentionally suppressed before radio resources
  are admitted, or while a saved layout is protected. Unconditionally capturing
  fallback receivers during handover would overwrite the saved layout; releasing
  after a real offline edit would discard that unsaved edit.
- Ruling: JJ requires preserving uncommitted work and completing safe station
  handover. Lead implementation preserves the existing storage policy: release
  unchanged offline state only after an explicit startup baseline, preserve its
  saved layout byte-for-byte, and refuse release while receiver edits remain
  unsaved. No new pending-layout format or operator policy is inferred.
- Status: built in signed `1bebe127`. The Core retains the model and profile lock
  on a refused save, with an explicit reason and retry path. Retune, gain, receiver
  add/remove and ownership edits are tracked, including disconnected receivers.
  The integrated app/Core build and twelve focused tests passed (20.09 s).
  Desktop-to-background handover and live-device acceptance remain open.
  (corrected 2026-09-28) The hosting and reverse-handover wiring is now built: startup
  reclaims the profile from the background Core (`src/main.cpp:262-273`, from `1bebe1274`),
  shutdown hands it back to the background service (`src/main.cpp:300-345`), the session
  coordinator installs the desktop Core (`src/gui/GuiSessionCoordinator.cpp:152`, `:186-212`;
  `ad5a3f878`), the background Core's control socket accepts the release
  (`src/server_main.cpp:206`), and the loopback round trip is tested
  (`tests/tst_station_handover_roundtrip.cpp`, `b22029c58`). Only live acceptance remains.
- Plan: station profile ownership and radio handover.

### G-68: Capability-order test assumed newly added fields did not exist

- Evidence: `tst_station_devices` found original capability entries by offsets
  from the end of the list. Appending remote IQ, transmit modulation monitor and
  accessory transmit capabilities made those offsets refer to different entries.
- Ruling: within JJ's instruction to diagnose failures and suggest their actual
  fix, the lead preserves the wire order and corrects the test's stale indexing.
- Status: signed implementation `fc58c3ad` checks the original contiguous block
  from its named first entry and verifies newer entries follow in order. No
  production capability order was changed. The integrated app/Core build and
  six focused suites passed (27.62 seconds).
  The full Linux run at `9f87b4a9` exposed the same stale-tail assumption
  in `tst_display_extras`: its `size()-18` location now names a later field.
  The lead corrected that test to locate the original named block and still
  check every entry's contiguous order. The explicitly rebuilt full display
  extras suite passes (2.83 s). No production ordering changed.
  The same full run found matching tail assumptions in the Core log and
  pairing suites. Both now anchor their original contiguous blocks by name;
  their explicitly rebuilt complete suites pass together (10.66 s).
  Signed `a85c0cd1`, integrated in `f7e2f3287`, fixes the same assumption in
  transmit settings, band selection and display-budget compatibility tests.
  Four integrated suites pass (5.22 s), preserving the exact older-minor
  descriptor bytes and strict ordering/types of the newer fields.
- Plan: several-device capability compatibility.

### G-69: Desktop shutdown could close ingress before ending transmit

- Evidence: the hosting controller's stop-during-start path called `quiesce`
  before `stopAllTx`. A focused regression observed listener closure before the
  transmit-stopped notification. A separate callback test also reproduced a
  crash when listener shutdown deleted the controller on its own call stack.
- Ruling: the approved desktop-hosting plan explicitly requires ending transmit
  before shutdown. Lead correction preserves that order during startup as well
  as ordinary operation and retains the host until synchronous callbacks return.
- Status: signed implementations `987eb996` and `f7fd0b0b` fix callback lifetime
  and order. The formerly failing order test, normal running-host order and
  reentrant deletion tests pass in the integrated app/Core build and six focused
  suites (27.62 seconds). These are in-process models without RF or a live window
  relaunch. MainWindow hosting and reverse handover wiring remain open.
  (corrected 2026-09-28) The hosting and reverse-handover wiring is now built: startup
  reclaims the profile from the background Core (`src/main.cpp:262-273`, from `1bebe1274`),
  shutdown hands it back to the background service (`src/main.cpp:300-345`), the session
  coordinator installs the desktop Core (`src/gui/GuiSessionCoordinator.cpp:152`, `:186-212`;
  `ad5a3f878`), the background Core's control socket accepts the release
  (`src/server_main.cpp:206`), and the loopback round trip is tested
  (`tests/tst_station_handover_roundtrip.cpp`, `b22029c58`). Only live acceptance remains.
- Plan: desktop hosting and station handover.

### G-70: Auxiliary watch callbacks and route declarations need strict lifetime bounds

- Evidence: new owner-deletion regressions reproduced a crash when watch
  acknowledgement, eligibility, delivery or retirement callbacks destroyed the
  Core helper. Review also found direct-watch eligibility checked that Core was
  listening without checking the primary's actual transport, so a relay/DTLS
  primary could be offered an unsupported direct route.
- Ruling: lead implementation under JJ's unchanged transmit-safety requirement
  must invalidate bindings before callbacks, check owner/generation afterward,
  and advertise only a route currently implemented for that primary. This does
  not change the 100 ms cadence or 400 ms cutoff.
- Status: signed `f00b61e2` guards callback lifetime, primary drop and path moves.
  Root integration additionally restricts direct watch eligibility to WSS
  primaries. The real paired two-socket Core handshake, accepted heartbeats and
  replay refusal passed on trunk. The integrated app/Core build and nine
  focused suites passed (46.10 seconds) at concurrent lane load.
  The direct client helper also verifies the fresh actual TLS certificate before
  sending its ticket and guards synchronous socket errors that delete or replace
  the helper (`d8f14ac1`). Its integrated app/Core build and two focused suites
  passed (4.17 seconds). Actual StationClient direct-WSS wiring is now built
  (`2673e991`): paired authentication and snapshot precede ticket requests,
  actual primary authority/pin are reused, immutable generation and session epoch
  correlate replies, and every retirement clears the auxiliary. Lead review
  added guards for synchronous ticket-send and helper-close callbacks. One
  existing timer sends the same sequence/epoch on auxiliary and media-or-primary,
  retaining every release fence. The integrated app/Core build and six suites
  passed (29.31 seconds), including real paired WSS heartbeat delivery, primary
  close and reconnection, plus a DTLS primary rejecting a claimed direct route.
  The same client helper now accepts a dedicated watch DTLS offerer (`a18f349b`),
  verifies its actual Core certificate before sending any ticket, and preserves
  the bounded ack/frame/backlog and generation checks. Wrong-pin real-DTLS tests
  observed zero ticket bytes. The integrated app/Core build and three suites
  passed (17.36 seconds), including all direct WSS checks. Core relay ownership
  is built in `2e75bf64` and `a63d210a`: pending construction is bounded and
  revoked on primary replacement, path move, device revocation or the ten-second
  deadline. An attached watch survives admission-grant expiry while its primary
  remains valid, without permitting new admission. Lead integration rebuilt
  app/Core and six named suites; all passed (35.13 seconds) at concurrent load
  6.19/5.95/5.43. The watch uses real DTLS and relay sockets in a local protocol
  fixture. Client relay ownership is built in `81362264` and `c795e810`:
  the actual paired StationClient owns offer/result correlation, the separate
  relay leg and pinned DTLS attachment, release-safe heartbeat copying and
  retirement/retry. Active watch readiness survives admission-grant expiry;
  new admission does not. Its integrated app/Core build and seven named suites
  passed (39.67 seconds) under concurrent load 9.17/9.21/7.35. The client test
  uses a bounded command responder alongside real paired authentication, so
  that initial test did not establish production Python RV or loaded-loss
  acceptance. Subsequent actual-service namespace runs are recorded in G-55:
  four impaired-network rows and a second-receiver row passed with the unchanged
  heartbeat cadence and cutoff. Installed-service and live-phone acceptance
  remain separate.
- Plan: independent transmit watch and restrictive-network liveness.

### G-71: Hosted desktop TCI must preserve ownership through callbacks and receiver remaps

- Evidence: lead review found the new accepted-program-key path continued after
  synchronous keying callbacks without rechecking the client or server. The
  worker confirmed that risk and that queued receive audio survives ownership
  remaps. Review also found a logical receiver number entering a new broadcast
  function that expects a physical slice number.
- Ruling basis: JJ's standing instruction is “each gap is built as part of this
  effort.” These corrections preserve the already approved station-owned TCI
  receiver and program-key policy; they introduce no new operator preference.
- Status: built in signed implementation `46835b63` with lead integration
  corrections. Program key requests recheck lifetime and ownership after
  callbacks, receive remaps retire queued audio without blocking the DSP
  producer, and broadcasts map physical owned slices to logical receivers.
  Lead regressions additionally reproduced crashes when key release destroyed
  the server: mode changes continued on a deleted object, and socket-close
  processing lost its internal TCP object during parent destruction. Callers
  now recheck lifetime and defer socket/listener destruction until the close
  stack unwinds. All three stop/disable/disconnect reproductions pass. The
  integrated app/Core build and nine TCI suites passed (8.95 seconds) under
  concurrent lane load. Desktop runtime activation and live-radio acceptance
  remain open; no RF was used.
  (corrected 2026-09-28) Desktop runtime activation is now built: the session coordinator
  installs the desktop Core (`src/gui/GuiSessionCoordinator.cpp:152`, `:186-212`; `ad5a3f878`).
  Only live-radio acceptance remains.
- Plan: desktop hosting and several-device ownership.

### G-72: An open Rename dialog could bypass the transmit Setup lock

- Evidence: a focused test opened Remote Access Rename, changed the live state
  to transmitting while the dialog was open, then accepted the name. The new
  page emitted the rename despite its controls being disabled.
- JJ's ruling: “Lock Setup controls while transmitting (recommended).” This
  applies to an action completing after a dialog has already opened as well.
- Status: built. The page rechecks its lifetime, current permission and original
  name when the dialog returns. The formerly failing test and all page actions
  pass in the integrated app build (0.52 seconds); four offscreen states were
  rendered and inspected. Action signal blockers now end before callbacks and
  operator labels use plain text. Real hosting/service/device action wiring is
  still in progress, so page presentation alone does not complete the plan.
  (corrected 2026-09-28) That wiring is now built in `fb325954d`: the Remote Access page's
  run, keep-running, start-with-computer, rename, revoke, add-device and key-backup actions
  reach the desktop runtime (`src/gui/GuiDesktopStationRuntime.cpp:432-445`, handlers from
  `:258`), covered by `tests/tst_gui_desktop_station_runtime.cpp`. Only live acceptance remains.
- Plan: Remote Access controls and Setup transmit locking.

### G-73: A late heartbeat could renew an already expired transmit watch

- Evidence: lead review found that heartbeat receipt updated the last-heard
  time without checking the existing deadline. When a busy event loop dispatches
  socket input before its overdue timer, a late heartbeat could keep a key on.
  An injected-clock regression reproduced this on the session, media and
  independent watch paths; nine expired-arrival rows failed before the fix.
- Ruling basis: JJ requires findings under load to have a cause and a fix, and
  every discovered gap to be built in this effort. This enforces the existing
  approved rule of stopping after more than 400 ms without a valid heartbeat;
  it changes neither that deadline nor the 100 ms send interval.
- Status: built. Receipt checks elapsed time before accepting any packet and
  stops an expired watch once; packets exactly at the deadline still count.
  The regression and four adjacent suites passed after the app/Core rebuild
  under concurrent lane load. This deterministically reproduces delayed timer
  dispatch, not a measured guarantee about OS scheduling or physical RF release.
  Restrictive-network acceptance and live-radio verification remain open.
- Plan: remote transmit link-loss safety and restrictive-network liveness.

### G-74: Remote settings export and import omit the Core's settings

- Evidence: the working desktop at `86a1b20d` exported and imported its settings
  XML in `ExportImportConfigPage`. At `11a1173a`, those handlers still copy only
  `AppSettings::instance().filePath()` on the window's computer. They neither
  export the Core's state nor restore it through a Core command. The frozen
  parity scout found this, and the lead verified the handlers and existing
  operator ruling. The disabled historical File > Profiles placeholder is not
  evidence of an additional working feature lost in the split.
- Ruling basis: the remote-window parity plan's recorded Q1 decision says
  “Settings import and export in a remote window carry both computers' settings,
  and the Core applies its part through a radio reconnect.” The original
  conversation confirms this was the previous controller's interpretation of
  JJ's standing parity rule, not a direct JJ answer. JJ's current instruction
  includes parity gaps in this effort; G-50 does not waive desktop remote
  backup/restore. The plan's Q1 option says to refuse while another device is
  connected, but its recommendation also says to use the shared-setting confirm
  step. JJ was asked on 2026-09-28 to choose between requiring disconnection or
  naming the affected devices, confirming, and reconnecting them. That specific
  multi-device behavior remains OPEN; restore is always refused on the air.
- Status: the bounded two-part backup codec and local settings import/export
  primitives are built in signed `be9ce291`. They preserve local window storage
  independently of a settings proxy, validate XML before mutation, and publish
  imported maps only after atomic disk save succeeds. The app/Core and nine
  focused integration suites pass (63.80 s), including malformed input, arbitrary
  settings keys and write-failure preservation. Network transfer, owner shutdown,
  user interface and the pending multi-device policy remain open. The session's
  1 MiB incoming message limit requires bounded chunking for a 16 MiB XML part.
  The binary transfer primitives are now built in `5fd79f1e`/`9ba6519a`:
  owned immutable snapshots, 256 KiB chunks, exact offsets/length/checksum and
  preserved outputs on errors. Lead review caught borrowed-memory aliasing;
  its regression failed before the owning-copy fix and passes afterward. The
  integrated app/Core and three focused suites pass (7.60 s, load
  12.50/25.25/27.82). External authentication, session ownership, expiry and
  global limits are implemented in signed `46c984d9` and verified in trunk:
  app/Core and eight explicitly rebuilt suites pass together (28.84 s, observed
  load 42.87/34.46/28.92). The paired session export is read-only and checks
  ownership, typed envelopes, final XML and integrity before publication.
  The window export is built in signed `e1c9647f`: remote export atomically
  saves both window and paired Core XML in one `.nereus-settings` bundle;
  local export retains XML. The integrated app/Core build and nine named
  suites pass (5.16 s), including cancellation, replacement, invalid replies
  and destination write failure. Remote import remains unavailable pending
  the multi-device restore ruling and its implementation.
  Export must carry both parts in one file;
  import must validate and restore them through their respective owners, apply
  the Core part through the approved reconnect path, and enforce the plan's
  transmit and other-device restrictions. Failure must preserve existing
  settings. Acceptance needs a real remote round trip, persistence after
  reconnect, refusal while busy, and invalid/failed-import preservation.
- Plan: remote-window parity B6.1 and approved Q1.

### G-75: A refused desktop Core start left a listener retry alive

- Evidence: the desktop runtime's blocked-port regression reproduced a failed
  Run action leaving a StationHost alive with a scheduled listener retry. The
  page reported the Core as off and its saved Run preference remained false,
  yet the retained host could start listening after the port became available.
- Ruling basis: the existing desktop-hosting acceptance requires that with the
  switch off nothing listens. Stopping the retained host on failed startup
  implements that approved requirement; it introduces no new operator choice.
- Status: built in the signed desktop runtime through `25750342` and verified
  in trunk integration. Failed Run and failed preference restoration retire the
  host and report that it is off, avoiding a later listener retry in that run.
  A rejected new Run request does not save a successful Run preference. The
  integrated runtime suite and the initial fifteen-suite integration run pass.
- Plan: desktop hosting and the Remote Access page.

### G-76: Desktop receiver controls still followed another device's active receiver

- Evidence: an actual MainWindow regression assigns receiver A to the desktop
  and B to a phone that holds transmit, making B the Core's active receiver.
  Triggering desktop ANF changes B instead of A. Source review also found the
  dashboard using the Core's active receiver and pan controls accepting a
  foreign receiver as their target.
- Ruling basis: the approved hosting-desktop design requires its window to
  operate its own receivers and read its own active receiver. JJ's current
  parity instruction includes these gaps in this effort.
- Status: built in signed `5dfb27690` and verified in trunk integration. The
  actual-window regression covers menus, dashboard, pan and container controls
  with a foreign transmit holder and desktop A-to-C selection. Core station-level
  active-receiver semantics remain intact. The later Setup audit found G-79,
  which is tracked separately and remains in progress.
- Plan: hosting-desktop receiver ownership and remote-window parity.

### G-77: Radio replacement could start during the return from transmit to receive

- Evidence: the integrated coordinator checked MOX/Tune booleans but omitted
  the full station on-air gate. An actual local-window regression reached
  `TxToRxFlush` with both MOX booleans false and the station gate still active;
  `canReplace()` incorrectly allowed retirement. The failure was reproduced
  before replacing that predicate. Two-tone is also covered by the full gate.
- Ruling basis: JJ's existing transmit safety and radio-handover requirements
  prohibit retiring an on-air radio. This restores that requirement without
  changing the transmit sequence or cutoff.
- Status: correction built; the focused regression passed after failing before
  the change. The deferred radio-change callback uses this same replacement
  gate before answering success. Final app/Core build and six focused
  integration suites pass (10.86 s; load 3.71/4.14/6.16). The independent
  lifecycle reviewer found the original issues and verified their corrections.
- Plan: desktop Core hosting and safe radio handover.

### G-78: Reopening the desktop ignored a radio selected from the phone

- Evidence: the desktop startup used saved auto-connect flags and lastConnected,
  while the background Core used StationRadioChoice and radio_mac. A regression
  with old auto-connect A and saved Core choice B reproduced an empty target;
  the old startup would then attempt A and overwrite the confirmed Core choice.
- Ruling basis: the approved shared Core radio choice survives desktop/background
  handover and follows the existing StationRadios choice order.
- Status: correction built. Hosted startup now seeds the saved/configured target,
  resolves the completed discovery through StationRadios, and waits when the
  chosen radio is absent or the available radios are ambiguous. Legacy local
  auto-connect remains the non-hosted path. Five focused cases pass (3.637 s):
  saved choice, configured choice, missing choice, ambiguous radios, and the
  sole available radio. Tests cancel before opening radio/audio sockets.
  Review also found a transient failed attempt stopped retrying. The correction
  retires failed I/O, retains same-radio receiver edits, and retries discovery;
  an intentional Disconnect cancels recovery, including late failure callbacks.
  A sixth regression now observes the real retry against loopback only and then
  verifies manual cancellation. All six rows pass (1.839 s). The first retry
  assertion incorrectly counted one-time WDSP initialization; the log showed
  a real retry, so the test now observes its connection instead. No production
  timeout was changed. Final app/Core build and six focused integration suites
  pass (10.86 s; load 3.71/4.14/6.16). Independent source review found no remaining
  defect in the corrected retry and intentional-disconnect boundary.
- Plan: desktop Core hosting and persistent radio selection.

### G-79: Hosted desktop Setup still selected another device's receiver

- Evidence: source inspection found SetupDialog's DSP factories still use the
  shared RadioModel active receiver and its global selection signal. A phone
  holding transmit can make B globally active while the desktop selects A/C;
  changing the desktop selection then emits no global active-receiver change.
  AGC, NR/ANF, NB/SNB, CW APF, squelch and TNF paths can consequently remain on B.
  Existing tests cover global selection changes and desktop menus, not this
  hosted Setup case. This is distinct from G-54 and extends G-76's audit.
- Ruling basis: JJ's approved hosted-window ownership rule requires the window's
  controls to operate its own selected receiver; the station-level transmit
  holder selection must remain unchanged.
- Status: built in signed `9650e167d` and verified in trunk integration. Setup
  now carries the window's receiver selector and ownership-change notification.
  An actual-window AGC edit failed before the fix; A-to-C edits, TNF Add, flag
  shortcuts and no-owned-receiver refusal now pass with B remaining globally
  active. The app/Core build and seven focused suites pass (4.24 s). The merge
  retains both the Remote Access binder and receiver selector. No DSP values or
  station-level active-receiver semantics changed.
- Plan: hosted desktop receiver ownership and Setup parity.

### G-80: Relay acceptance measured requested time instead of Core transmit time

- Evidence: the separate second-receiver row at seed 20261015 held its client
  command for 60001 ms, but the Core was keyed for only 59851 ms. The ON and OFF
  commands took 234 ms and 84 ms respectively; that 150 ms difference explains
  the failed 59900 ms Core-coverage assertion. Watchdog and media checks passed.
- Ruling basis: JJ requires explaining load failures and fixing their cause.
  Measure the test's hold from the authenticated Core state showing keyed/TUNE,
  while preserving the production cadence, safety cutoff and coverage assertion.
- Status: test-only correction built in `5c051110`. One predetermined rerun of
  the same seed passed: client hold 60001 ms after observed Core ON, Core keyed
  60340 ms, zero trips and continuous second-receiver audio. A 65 s fixture
  release fallback remains bounded; it does not alter Core safety behavior.
  The original failure and corrected artifacts are both retained under
  `build-r5-linux/watch-acceptance/second-rx` and `second-rx-corrected`.
- Plan: R5 impaired-network acceptance with simultaneous receive audio.

### G-81: Full phone diagnostics lack radio age, live UDP port and ADC observations

- Evidence: the phone team's full Network Diagnostics audit finds no numeric
  Core-to-radio connection age, no live radio UDP base/control port, and no
  measured per-ADC overload status in telemetry versions 1-5. Session sample age
  starts when a client joins, so cannot stand in for radio uptime. The local
  diagnostics ADC label is a placeholder; positive-only `adcOverflow` signals
  cannot establish a measured clear. P2's actual outbound base also differs
  from a saved RadioInfo port when configured by a test.
- Ruling basis: JJ approved the phone's full Network Diagnostics scope, recorded
  in its plan commit `7361316de`; the phone team owns the interface. The lead
  supplies truthful optional Core observations, with no Core counter-reset RPC.
- Status: built in signed `749360b0e` and verified in trunk. The version-6
  contract uses the unreleased minor-11 extension: monotonic model-owned radio
  age, owner-thread live base port, and bounded status-bit observations for the
  board's ADCs. Clear requires an explicit zero bit; stale/unobserved status is
  unavailable. Counts track observed overload transitions since this connection,
  survive client joins, and reset on radio reconnect. No transmit or attenuation
  behavior changes. Root rebuilt app/Core and all 13 targeted suites, then
  ran them serially: 13/13 passed in 48.98 s (load 54.49/21.03/18.78).
  Coverage includes compatibility, malformed fields, stale replies, reconnect
  fencing and actual P1/P2 parser bits. Lead review corrected a test that
  compared a fresh asynchronous connection age to an old frozen age; it now
  uses the enclosing monotonic timer, without relaxing product deadlines.
- Plan: Core support for the phone's approved full diagnostics parity.

### G-82: Refused handover leaves the still-owning Core unreachable

- Evidence: the real-process handover fixture's unadmitted, no-radio variant
  allowed a paired client to adopt a fallback receiver. G-67 correctly refused
  release because that ownership edit could not yet be saved. However, release
  had already closed the station listener and disabled radio recovery. The Core
  retained its process, model and profile lock, but its clients could not return.
  Source tracing confirms no resume path; the original failure is preserved.
- Ruling basis: preserve JJ's unsaved-work rule and G-67's refusal. The lead's
  fix restores client access and the original radio-recovery behavior after a
  refusal before model destruction, keeping the same unsaved model and lock.
  It must never clear dirty flags, discard edits or resume an old transmit key.
  A failure after model destruction remains closed with an explicit retry path.
- Status: built in signed `28a6cc6c`. The Core reopens its listener around the
  retained model, restores previously enabled discovery, and defers recovery
  until an in-progress radio connection unwinds. The coordinator fences console
  changes and duplicate releases during recovery. A real paired client reconnects
  after two refusals; saved layout, edited state, identity and profile lock are
  retained. A separate post-teardown write-failure test verifies continued fencing
  and explicit retry. Root integration rebuilt app/Core and all three named
  daemon/handover suites: 3/3 passed (22.96 s, load 16.69/15.52/21.13).
  The normal admitted-board round trip in `b22029c58` uses three real owner
  processes; neither fixture covers live hardware or the actual OS service manager.
- Plan: reliable station ownership and recoverable Core handover.

### G-83: Core release build hides a production method behind the test guard

- Evidence: the signed `9f87b4a9` Rock package failed with tests disabled because
  DaemonApp::stationServer() was declared inside NEREUS_BUILD_TESTS. Production
  console and radio-switch code calls it and its definition is unconditional.
  Test-enabled app/Core builds therefore missed the missing declaration.
- Ruling basis: JJ requires verified installable Core builds. Move the production
  declaration outside the test-only observer block; retain all actual test hooks
  behind their existing guard. No runtime behavior or deadline changes.
- Status: the declaration is corrected. The actual DaemonApp.cpp compiles with
  NEREUS_BUILD_TESTS undefined and without a precompiled header (exit 0).
  The integrated app/Core build and three backup/handover suites pass (7.60 s,
  load 12.50/25.25/27.82). Signed `99245937` subsequently built with tests
  disabled for both Rock and Pi 4; CPU/ISA, dependency and model checks passed.
  Both isolated, network-disabled startup checks reached `nereusd started`
  and remained running for the 15-second observation. Full Linux suite
  verification and safe installation timing remain outstanding. The failed
  original package and compiler output remain
  recorded in `core-gui-rock-9f87b4a9-build.log`.
- Plan: production Core packaging on the Rock and Pi 4.

### G-84: Rock package reports the previous integration branch name

- Evidence: the Rock build kit hard-codes `codex/integrate-r2-main` in its build
  tag even when packaging signed `codex/checkpoint-b` source. The commit hash is
  correct, but the branch shown in source information is stale. The Pi kit already
  reads a branch field from its source manifest.
- Ruling basis: JJ's diagnostic/source information must describe the build that
  was actually installed. Record the source branch in the signed-source package
  manifest and have the Rock kit read it, matching the existing Pi behavior.
- Status: local packaging tools corrected and syntax checked. Both signed
  `99245937` packages now configure with `codex/checkpoint-b@99245937`, matching
  their manifests. Inspection found that neither daemon embeds this tag at all;
  G-86 records the separate product-side gap. These build-kit files live under
  `~/.config/nereus/work/`, outside the product repository. No radio configuration
  or installed binary has changed.
- Plan: truthful Core build provenance in diagnostics.

### G-85: Backup completion can outlive its client or disturb unrelated messages

- Evidence: lead review of the uncommitted paired-export lane found that a cancel
  send and export-completion signal could delete or replace StationClient before
  the caller continued accessing it. Session teardown also emitted completion
  before fencing new work. The new export size check rejected every large incoming
  message during an export, including unrelated traffic allowed by the existing
  session limit.
- Ruling basis: JJ requires complete, reliable parity. Fence retired sessions
  before callbacks, guard lifetime and session identity after callbacks, and
  apply export-specific bounds only to export replies. Preserve the existing
  session limits and unrelated traffic.
- Status: repaired in signed `46c984d9` and verified in trunk integration. After one
  worker correction left callback ordering unsettled, the lead took over the
  bounded repair. Three regressions failed before the repair and passed after:
  unrelated large request/result text no longer changes export classification,
  and reconnect from completion observes cleared old mirrors. Teardown retires
  work before notification; lifetime guards protect callbacks; strict command
  IDs are checked before narrowing. Cancellation names its operation, so an old
  page cannot cancel newer work. Eight explicitly rebuilt integrated suites
  pass with the app/Core build (28.84 s); the earlier seven-suite lane run also
  passed (33.96 s). The
  later cancellation run's separate encrypted-channel startup failure remains
  OPEN under G-57. GUI review also found synchronous-cancel reentry, hidden-page
  retirement and model changes during dialogs/requests. Those corrections are
  built in signed `e1c9647f` and pass the nine-suite integration (5.16 s),
  including deletion of the selected model inside the export request.
  No affected code is installed.
- Plan: complete settings export, G-74.

### G-86: The Core daemon never receives its source build identity

- Evidence: both signed `99245937` Core packages have the correct build-tag
  cache value, but neither daemon binary contains it. The generated header is
  private to the desktop executable; only `main.cpp` initializes BuildIdentity.
  `server_main.cpp` never reads it, and Core sessions do not currently advertise
  the source revision. A correct package manifest is not proof of an embedded
  or remotely reported build identity.
- Ruling basis: JJ approved full diagnostics and About source information.
  The lead will make the actual daemon report its product/source identity and
  provide optional, backward-compatible metadata for connected clients. The
  generated header must stay private to executables to avoid rebuilding the
  shared Core and every test each time Git HEAD changes.
- Status: built and verified in this integration. Both executable entry points
  receive the private generated tag. The daemon's `--build-info` and `--version`
  return before configuration, settings or profile-lock work; their output
  matches the generated tag and project version and leaves an isolated home
  empty. Authenticated, opted-in minor-11 clients receive bounded optional
  `coreBuildInfo` capabilities; malformed or duplicate metadata is absent,
  old clients retain their existing contract, and reconnect clears old identity.
  The app and daemon build and five named suites pass (25.84 s), including
  Unicode byte bounds, negotiation and replacement. Signed `21bbbd8e` packages
  built with tests disabled for both Rock and Pi 4; CPU/ISA, dependency and
  model checks passed. Each packaged executable reports product `0.5.2` and
  source `codex/checkpoint-b@21bbbd8e`, leaves the isolated CLI home empty,
  and passes the network-disabled startup observation. The Rock package is
  staged without restarting JJ's active phone test; Rock timing remains pending.
  After JJ enabled his VPN, the intended Pi 4 at `10.0.252.47` was positively
  identified and upgraded from the manifest-matched `0368ff16` to `21bbbd8e`.
  Its installed binary/library/model hashes match the verified package, actual
  `--build-info` reports the expected source, and the service is active with
  zero restarts. It registered with the remote-access service, reconnected to
  the HL2 at the unchanged 384 kHz rate, and received its first EP6 frame.
  `/etc/nereusd.conf` is byte-for-byte unchanged. A root-only rollback copy
  retains the previous installation. Physical iPhone reception and rendezvous
  connection remain live acceptance checks; this install does not claim them.
- Plan: truthful Core build provenance and phone About support.

### G-87: Hardware and PA descriptions omit built desktop controls and readouts

- Evidence: a source scout compared the constructed desktop Hardware and PA
  pages with the Core's published Setup resources. Hardware had no published
  category; PA has no mirrored category property. Existing local/remote desktop
  controls therefore cannot be rendered by the phone from the Core description.
  The generic readout kind alone is insufficient: current semantic validation
  admits only the DSP notch-width readout. Some hardware controls already have
  scalar mirrors; band matrices, profiles, calibration and I/O actions need
  additional typed contracts. Historical hidden XVTR/bandwidth placeholders are
  not working desktop controls and are not fabricated as completed pages.
- Ruling basis: JJ requires the Core station dependencies for the phone and
  working desktop parity; each missing built control remains in this effort.
  Preserve actual board/model visibility, values, apply paths and transmit
  restrictions. Partial publication is useful progress, not page completion.
- Status: seven Antenna/ALEX scalar descriptions are built in signed
  `8c6bf3133`/`8dbdb14b`, with trunk app/Core and nine named integration suites
  passing (5.16 s). They use existing mirrored
  writes, exact desktop IDs/text and Core board/model projection, including
  same-board model changes; the worker's three focused suites passed in 4.39 s.
  A second scout mapped all 13 PA Values rows: five have direct read-only
  mirrors (calibrated forward/reflected power, SWR, two raw ADC counts), while
  derived watts/volts, combined overload, PA telemetry and peak/min/reset require
  further work. Five direct PA readout descriptions are built in signed
  `7e294608`, reviewed and verified in trunk: app/Core plus six explicitly
  rebuilt suites pass (44.26 s), including encrypted transport and session
  conformance. The worker's five focused suites also passed (41.71 s).
  The phone's generic Setup renderer is
  separate unfinished phone work, so publication does not establish phone
  page parity. Band antenna/OC/filter tables, connected-radio scalar settings,
  PA profile lifecycle and calibration/I/O actions remain incomplete; their
  existing source maps are retained for implementation.
  The existing ANAN-G2E PA bypass checkbox is described in signed `ed7f6548`:
  exact desktop text, SKU projection and the existing version-6/off-air
  settings gate, including the receive-only Core exception. No Core authority
  or schema ordinal changed. Lead review and the app/Core build plus fourteen
  named integration suites pass (11.13 s). This remains a partial PA page.
  Drive is now described in signed `9af39d292`: a readout of the selected
  `transmit.power` value, not measured RF output, with its existing version-1
  capability gate. It is the sixth PA readout, remains visible while keyed,
  and sends no write. Lead review and the combined app/Core build plus four
  explicitly rebuilt Setup/native PA suites pass (6.39 s; load before run
  10.36/9.30/9.08). Signed `69ecac959` now appends three outbound Core-scaled
  readings: raw forward watts and forward/reverse ADC volts, gated by
  `txReadingsVersion: 2`. They reuse existing hardware scaling and reset with
  the session. Native and published PA values, same-count model changes and
  client write refusal are covered. Root reviewed the implementation and the
  combined app/Core build plus twelve full named suites pass (50.37 s;
  starting load 20.16/18.00/15.08). This pass does not resolve the earlier
  intermittent connection-open finding under G-57. Combined ADC overload,
  peak/min/reset and the remaining PA actions are still open.
  Signed `e72fa12e` and lifetime follow-up `d9dc53cb` add authenticated,
  connected-radio-bound single-band RX/TX antenna commands. Capability
  withdrawal and confirmation revalidate the same radio and session; existing
  authority, transmit and blocked-port restrictions remain. Root app/Core and
  ten explicitly rebuilt suites pass (76.76 s; load 62.62/36.75/23.26).
  Signed 7102a610 includes the V6 antenna table descriptions and the
  late-connection correction; they are no longer an unfinished Core table.
  Signed `2954472b` adds PA Current and DC Voltage readouts from the existing
  optional Core telemetry, with exact amps/supply-volts board projection and
  no fabricated zero for an absent field. They require Setup V5 and telemetry
  V4. Root app/Core and eleven explicitly rebuilt suites pass (55.01 s; load
  8.03/15.18/18.17). The later raw power/voltage work above is already
  integrated. A refreshed source audit confirms eleven of thirteen physical
  PA Values rows are described. Temperature and combined ADC overload remain,
  plus profile/calibration/I/O contracts. Peak/min and their reset controls
  are per-window presentation, not missing Core measurement/reset commands.
  Report core-gui-pa-hardware-remaining-scout.md records the native overload
  latch versus remote instantaneous discrepancy and actual phone parser/UI
  limitations; publication alone still does not establish phone page parity.
- Plan: Core Hardware Config/PA Setup description dependencies in the phone plan
  and remote-window Hardware/PA parity.

### G-88: Networking source notes omit applied retirement patches

- Evidence: the phone integration audit found that the desktop licence/source
  notes list the older remote-description and DTLS-MTU changes, while the actual
  CMake patch list also applies numbered libjuice and libdatachannel retirement
  patches. The shipped source pointers did not describe those modifications.
- Ruling basis: the repository requires accurate upstream attribution and
  modification notices; no new operator behavior is proposed.
- Status: corrected the licence README and both library modification notes to
  match the actual patch files and affected sources. Original licence texts
  and upstream notices remain intact. The licence checker passes for 21
  libraries and 45 files. Subsequent packages must carry the corrected notes;
  the already-built `21bbbd8e` packages predate this documentation correction.
- Plan: complete source provenance for Core and phone networking dependencies.

### G-89: Backup write-failure test assumes an unprivileged POSIX runner

- Evidence: the GUI export test made its destination directory mode 0500 and
  skipped Windows. A probe in the actual Linux verification container, which
  runs as root, successfully wrote into that directory, so the setup would not
  reliably exercise failure on that runner.
- Ruling basis: JJ requires testing at real conditions and correcting causes.
  Exercise a genuine atomic-write failure without changing production limits.
- Status: the test now replaces the selected destination with a nonempty
  directory while the asynchronous export is pending. It verifies the failure
  message, untouched directory contents and preserved previous backup, without
  an operating-system skip. All nine macOS integration suites pass (5.16 s).
  Full Linux and Windows verification of this change remains outstanding.
  The full Linux run also exposed this assumption in the handover save-failure
  fixture. Signed `3c07c3f9` replaces both pre/post-teardown injections with a
  directory at the settings-file destination, preserving the original file and
  profile lock without Windows skips. App/Core and all seven integrated window,
  accessory and handover suites pass (44.93 s). The pre-stop case recovers normal
  commands; the post-stop case stays fenced; both retain ownership and retry.
  Linux uid-0 and Windows execution remain outstanding.
  (corrected 2026-09-28) Linux uid-0 execution is recorded: the immutable Linux `5c4ca88d`
  full run, in the root verification container this entry describes and containing
  `3c07c3f9`, passed `tst_settings_backup_export` (2.57 s), `tst_station_handover` and
  `tst_station_handover_roundtrip`. Windows execution remains outstanding.
- Plan: reliable settings export and portable CI evidence.

### G-90: HL2 option values are stored but never applied to hardware

- Evidence: the source scout traced nine desktop HL2 options through their
  per-radio saved keys and Core reload. The model explicitly describes wire
  emission as deferred; production radio code never consumes these values.
  P1 emits fixed PTT hang and TX latency values. This predates the Core/GUI
  split, so it is an existing incomplete feature rather than lost working
  desktop behavior. The two timing rows are currently hidden.
- Ruling: OPEN on extending this parity effort to the unbuilt hardware behavior.
  Recommendation: preserve truthful visibility, correct the misleading PS Sync
  wording, and implement the actual supported HL2 behavior before advertising
  these options to a phone. Merely publishing saved values is insufficient.
- Status: bounded read-only audit complete, with exact pinned mi0bot source
  paths, current setting authority and radio-identity fences recorded. CL2 and
  the external reference use clock-chip I2C sequences; reset and timing use
  P1 banks, audio swap changes outgoing samples, and Band Volts/PS Sync reuse
  ADC control bits. Upstream says "Disable PS Sync" and supports fractional
  CL2 MHz; the existing Nereus labels/storage differ. Lead scope and semantic
  decisions remain open; no hardware effect or new permission is inferred.
- Plan: honest Hardware Setup parity and phone dependencies, G-87.

### G-91: Linux verification container has no usable ICE interface

- Evidence: the full `9f87b4a9` suite runs in a network-disabled Docker
  container. Inspection shows only loopback carrying IP traffic. The pinned
  libjuice gatherer deliberately excludes loopback and link-local addresses;
  ICE configuration and rendezvous tests therefore report zero candidates,
  followed by data-channel and watch-relay connection failures/timeouts.
- Ruling basis: JJ requires causes and suggested fixes for real-load failures.
  Restore the test fixture's network prerequisite without changing production
  ICE selection, weakening assertions or increasing time limits.
- Status: source and actual runner interface state inspected. The next
  verification runner will have a private internal Docker network interface,
  with no external routing. The current run is preserved unchanged so its
  remaining independent findings can be collected. A bounded comparison using
  the exact same signed-source Linux binaries passed both previously failing
  ICE-gathering cases (464 ms) on the private internal interface; no code or
  deadline changed. The original run finished with 951/991 passing; forty
  nonpassing entries are individually classified in the progress evidence.
  A full app/Core/all-tests rebuild and run of signed `3a2c73fa` is underway
  on the private interface, with no test exclusions or changed limits.
  A separate namespace preflight passed with NET_ADMIN/SYS_ADMIN capabilities
  for the isolated traversal fixture; the old runner lacked those capabilities.
  The exact audio-backoff binary also passed with the interface (12.528 s;
  measured waits 2002/4004/4011 ms) and reproduced failure without it (46.903 s).
  With no direct candidates, the fallback tunnel legitimately starts session
  recovery after three seconds of silence, interrupting the direct-media
  backoff sequence. The remedy is the required interface, not longer limits.
  This does not resolve G-57's separate macOS
  intermittent startup failure on a machine with usable interfaces.
- Plan: full Linux verification and R5 network acceptance.

### G-92: Hydration test treats automatic read-only validation as a settings edit

- Evidence: the full Linux run rejects a reconnect command beyond record
  subscription. A focused diagnostic identified `station.validateSettings`,
  which reads settings and refreshes reported issues without writing preferences.
- Ruling basis: preserve JJ's requirement that stale window preferences never
  overwrite Core settings, while permitting the implemented read-only check.
- Status: signed `e51f8659` permits exactly validation and record subscription
  in that assertion; other commands and every property write remain rejected.
  App/Core and fourteen integrated suites pass (11.13 s). No product behavior
  or timeout changed, and no load-dependent cause is claimed.
- Plan: settings ownership and reconnect verification.

### G-93: Remote audio-reset test predates functioning local VAX outputs

- Evidence: the remote Audio Advanced test expected zero VAX rebuild signals,
  although approved parity B6.10 requires rebuilding those outputs in both
  windows. The remote router and feeder now feed local VAX buses; the dedicated
  reset test already verifies their reconstruction.
- Ruling basis: preserve the approved B6.10 behavior and protect Core settings.
- Status: the final signed `e51f8659` changes the stale page test to require
  all four rebuild notifications and retirement of the previous output. It
  retains assertions that Core DSP settings and outgoing writes are untouched.
  Production AudioEngine is unchanged. Both audio suites and twelve other
  integrated suites pass (11.13 s), after the app/Core build.
- Plan: remote-window audio reset and VAX parity.

### G-94: New hosting diagnostics call the Core a station

- Evidence: the full Linux wording sweep reports user-visible hosting, service,
  handover and Setup messages using "station" for the serving computer.
- Ruling basis: the standing operator wording rule calls that computer Core.
  G-13's Connections headings are decided and built (see G-13; corrected 2026-09-28).
- Status: signed `e51f8659` corrects those messages. Installed service names,
  scripts, wire keys and G-13 headings are preserved; exact installed-service
  strings have documented sweep exceptions. The sweep and hosting tests pass
  in the fourteen-suite integration (11.13 s).
- Plan: plain operator wording and desktop Core hosting.

### G-95: New accessory commands lose the Core's refusal in a desktop window

- Evidence: after correcting old receive-only test fixtures to use genuinely
  paired, permitted devices, the Core correctly refused `amp.operate` while
  keyed or while the tuner was sweeping. StationClient classified only the
  legacy accessory verbs, so the new refusal missed `accessoryRequestRefused`
  and fell through to the receiver error route.
- Ruling basis: JJ requires working remote accessory parity and truthful
  refusals; preserve every existing permission and on-air restriction.
- Status: signed `e770593f` maps the nine existing amp/tuner/RF-Kit command names
  to their actual accessory error route. All three lane suites pass (30.09 s);
  app/Core and seven integrated suites pass (44.93 s). PGXL paired regression
  coverage proves exactly one accessory refusal and no receiver error. Dormant
  tuner/RF-Kit aliases were source-reviewed; existing legacy paths remain tested.
  Accepted-path fixtures use proper device permissions; denied and legacy
  coverage remain. No Core authority is relaxed and no RF is emitted.
- Plan: remote accessory control and shared-setting confirmation.

### G-96: The new Remote Access page removed its Connections entry

- Evidence: the real-window harness cannot find `remoteStationConnections`.
  The new page retains its compatibility signal, and SetupDialog/MainWindow
  still route it, but the page has no button that emits it. Other Radio menu
  entries do not restore the expected Setup entry.
- Ruling basis: JJ requires existing working window entry points to retain
  parity. Opening Connections is navigation, not a Core configuration write.
- Status: signed `c3048c81` restores Connections in a This window section through
  the existing signal path. App/Core and seven integrated suites pass (44.93 s),
  including the full real-window harness. Coverage proves it opens the managed
  picker without dialing, stays available during hosting locks and preserves
  intentional disconnect. No real operator window was relaunched.
- Plan: remote-window connection entry points and Core hosting integration.

### G-97: Phone Settings Health has a protocol but no Setup description

- Evidence: `setup.diagnostics` is empty and its service test requires that. The
  independent minor-11 `settingsHygieneVersion:1` protocol already validates and
  forgets current-radio settings, but ordinary descriptor bindings cannot carry
  its session-bound current MAC and asynchronous bounded issue list.
- Ruling basis: JJ includes existing remote desktop parity and phone Core
  dependencies in this effort. Existing G-38 reset semantics and G-50 local
  file/log questions remain OPEN and are not changed by this repair.
- Recommendation: publish a closed Settings Health panel descriptor backed by
  the existing typed commands, with current-session/MAC matching, malformed
  reply rejection, paired/off-air Forget confirmation and truthful unavailable
  reasons. Keep Reset unavailable. Avoid a general command/result scripting
  language for this one established operation.
- Status: signed `9fad480b` publishes the closed V3 panel, with exact desktop
  labels, existing command semantics and strict schema validation. App/Core and
  twelve integrated suites pass (62.78 s), including real paired snapshot and
  capability gates, older-version projection, desktop label parity and full
  link conformance. V1/V2 clients keep their prior empty Diagnostics response.
  Phone renderer and lifecycle implementation remain separate phone work; Core
  publication alone is not phone parity. Reset and local file/log rulings remain
  unchanged.
- Plan: phone Setup description and remote Settings Validation parity.

### G-98: Audio compatibility tests reject independently negotiated start fields

- Evidence: two audio tests hide one older audio capability but still advertise
  mini-display, remote IQ, media tunnel and relay routing. Production correctly
  includes those negotiated fields; old assertions expected only two or three
  keys. The documented start contract and Core decoder agree with production.
- Ruling basis: JJ requires compatibility verification with real audio coverage;
  correct stale expectations without changing production behavior or deadlines.
- Status: signed `8949ca83` checks the exact negotiated start and audio-control
  shapes, including literal two-key start for minor 7. Existing decoded-audio,
  mute, reconnect and context checks remain. App/Core and full integrated
  remote-audio session suite pass (44.39 s). The Linux minor-7 peer-open failure
  is separately confounded by G-91 and awaits the corrected-network run.
- Plan: remote audio compatibility and meaningful CI evidence.

### G-99: Mirror-policy test omits the published Setup class

- Evidence: production includes `SetupDescription` in its mirrored classes,
  but the test inventory omits it. This reports both a class-list mismatch and
  false stale-policy entries for the published Setup properties.
- Ruling basis: JJ requires complete verification of the actual Core surface;
  restore test coverage rather than suppressing policy checks.
- Status: signed `a85c0cd1` adds the actual metaobject to the fixture. It also
  corrects three remaining capability fixtures under G-68, preserving exact
  original field order, kinds and older-minor wire equality. App/Core and all
  four integrated suites pass (5.22 s); no older-client capability leak was
  observed and no production wire changed. Updated Linux execution remains.
  (corrected 2026-09-28) Linux execution is recorded: the immutable Linux `5c4ca88d` full run
  contains `a85c0cd1` and passed `tst_mirror_schema` and `tst_link_surface_manifest`. Windows
  remains.
- Plan: complete mirror-policy and older-peer compatibility verification.

### G-100: Core refusal wording and its source scanner have drifted

- Evidence: three transmit-watch close/refusal messages expose internal terms.
  The reason scanner also mistakes a compared wire verb and a media helper's
  slice ID for operator text, and lacks exact new forwarding/site records.
- Ruling basis: preserve plain operator messages and meaningful future-source
  checking without changing refusal, authority or timing behavior.
- Status: signed `851846c0` corrects the three messages, traces individual
  forwarded reasons to their scanned sources and adds scanner regressions for
  real bad text. Root corrected a lane verification naming gap by rebuilding
  the actual changed watch suite as well as both other watch suites. App/Core
  and all twelve integration suites pass (62.78 s). Wire names, reason codes,
  permissions and deadlines remain unchanged.
- Plan: operator wording and complete Core refusal verification.

### G-101: Linux build repeatedly rejects its shared precompiled header

- Evidence: the signed `3a2c73fa` build reports that the Core precompiled header
  cannot be reused for GUI sources because `RTC_STATIC` is not defined there.
  CMake explicitly configures the GUI and test targets to reuse that header.
  The compiler falls back to parsing headers normally; this is build overhead,
  not evidence of a slow runtime test or a correctness failure.
- Ruling basis: JJ requires cost-aware work and understanding slow verification.
  Align the intended compile environment or use a compatible header cache,
  preserving static-library linkage and platform-specific visibility semantics.
- Status: signed `bcaf54cd` separates the library and test header donors,
  preserving existing consumer definitions, linking, PIC/PIE and warning flags.
  Exact GCC commands compile real Core, GUI and both kinds of test source with
  `-Werror=invalid-pch`, and report that the intended header cache was used.
  The lane's Mac app/Core build and two representative suites pass. Root
  app/Core and nine explicitly rebuilt suites pass (58.15 s; observed load
  10.05/10.69/14.27). Full Linux and Windows verification remain.
  Measured source-compile probes improved, but are not a full CI speed claim.
  The older immutable Linux build was deliberately stopped at 1810/2799
  compile steps before tests, retaining its log and cache. Its replacement
  will use the verified integrated source; no test result is claimed for it.
  The actual CMake-generated replacement at `10b796751` exposed another
  mismatch: PipeWire's public `-D_REENTRANT` compile option reaches Core, GUI
  and tests but neither donor. GCC again rejects the cache. The earlier
  manually constructed probes missed the generated-target mismatch. The
  cause is established from pkg-config and actual compile commands. Both
  donors now receive the same optional PipeWire flags. The original generated
  Core command fails with `-Werror=invalid-pch`; after the correction, both
  CMake-generated donors and four actual Core/GUI/test source commands pass
  and report cache use. The Mac app/Core build and two representative suites
  also pass. No full Linux success or runtime-test speed improvement is claimed.
  (checked 2026-09-28) The immutable Linux `5c4ca88d` full build contains `bcaf54cd` and
  `10b796751` and its log shows no header-cache rejection, but that build did not enable
  `-Winvalid-pch`, so the silence does not prove the cache was used. Linux reuse stays
  unverified; Windows remains.
- Plan: reliable, efficient full-suite builds across CI platforms.

### G-102: Trace and Fill colour is changed live but not saved

- Evidence: the Display/Appearance scout traced the built Colors & Theme
  swatch to `SpectrumWidget::setFillColor()`. It schedules a settings save,
  but `loadSettings()` and `saveSettings()` omit `m_fillColor`. The other nine
  built appearance swatches have per-pan saved keys. Reopening the window
  therefore loses this choice.
- Ruling basis: JJ requires working desktop parity and every discovered gap
  to be built in this effort. This is missing persistence in an existing
  control, with no change to its colour or rendering defaults.
- Status: built in signed `dc113a25`. `DisplayFillColor` uses the existing
  Qt `#AARRGGBB` format, pan suffix and load-time pan-zero fallback. Lead review
  and the integrated app/Core build plus six explicitly rebuilt display/Setup
  suites pass (4.59 s; observed load before the run 89.89/47.60/29.83).
  Tests cover alpha, independent overrides and invalid/default handling through
  actual widget setters and save/load paths. As with the existing settings,
  saving a pan writes all its current values; it can make inherited values
  explicit. No property-by-property save policy change is claimed.
  Implementation inspection also found that the shared colour reader bypasses
  the existing pan-zero fallback. The documented July 30 behavior says that
  untouched pans follow pan zero until given their own values. The fix restores
  that behavior for the shared per-pan colour reader too; global
  Peak colours remain global. The scout's initial claim that those existing
  swatches already inherited was incorrect.
- Plan: Display/Appearance parity and the phone's local colour descriptions.

### G-103: Display and Appearance have no published Setup descriptions

- Evidence: `StationCatalog` publishes eleven display controls and preset
  palette stops, but neither `resources/setup/display.json` nor
  `appearance.json` exists. The source scout mapped the built desktop pages,
  Core settings, local per-pan and per-band keys, and subscription fields.
  Catalogue controls do not substitute for Setup pages or settle phone-local
  persistence. Colours currently use three explicit formats: catalogue
  `#RRGGBB`, SpectrumWidget `#AARRGGBB`, and Peak swatches `#RRGGBBAA`.
- Ruling basis: JJ includes every Core dependency of the phone plan and built
  desktop parity in this effort. Publish exact existing behavior and ownership;
  do not describe hidden placeholders as working features. His accepted
  decimation range is 1-16, with calibration applied once in both windows.
- Status: signed `f2b24cf6` publishes the first fourteen Core-owned controls.
  The lane's app/Core build and five complete suites pass (24.80 s), with a
  strengthened paired-session fixture also passing. Root app/Core and nine
  explicitly rebuilt suites pass (58.15 s), including complete link conformance.
  Normalize has its exact V4 detector dependency; V1-V3 receive the
  other eleven controls. Native TX Display availability stays unchanged.
  Signed `04a6da23` and compatibility follow-up `b0bf9d264` also publish all ten
  built Colors & Theme swatches with exact native labels/tooltips and explicit
  RGBA defaults in V4. Root app/Core and eleven full named suites pass
  (55.01 s; load 8.03/15.18/18.17). Signed `e65100dd4` adds the three built
  S-meter style controls in negotiated Appearance V7: face, peak hold and
  decay. Their closed phone dispatch identities map to the phone's existing
  typed model, as its owner confirmed. No Core write authority is added.
  V1-V3 retain ten swatches without defaults; V4-V6 retain the previous
  Appearance V4 shape; V7 adds only the three new controls. Root app/Core and
  all seven rebuilt Setup, settings-scope and S-meter suites pass (9.12 s).
  Signed `12979352` adds seven existing RX subscription controls in Display
  V8: spectrum/waterfall detector, averaging and time, plus decimation 1-16.
  The phone owner confirmed all seven dispatch aliases map to its current
  typed per-pan settings; they are not new keys or Core write permissions.
  V1-V7 retain their exact 11/14-control projections and prior category
  versions. Root reviewed the implementation and rebuilt app/Core plus all
  five named Setup, catalogue and settings-scope suites: 5/5 pass (7.00 s).
  Signed `70ede33f2`, accepted in trunk `47d16744`, adds the eight existing
  Spectrum Rendering and Waterfall Display controls in Display V9, for 29
  controls. V1-V8 retain their exact 11/14/21-control projections. Root
  reviewed the actual changes and rebuilt app/Core plus all eleven affected
  complete suites, passing in 109.71 seconds at load 10.52/11.60/11.54.
  Evidence: core-gui-display-rendering-description-report.md and
  core-gui-header-display-root-ctest.log. The four existing Waterfall Overlays
  toggles are built in signed 982b8d29d and integrated at this checkpoint as
  Display V10, with 33 controls and unchanged V1-V9 projections. Root reviewed
  the actual diff, rebuilt app/Core and all three complete Setup suites plus
  the station wording audit; all four passed at starting load
  18.36/14.17/10.07. Native defaults and phone bindings are traced in
  core-gui-display-next-contract-scout.md. Implementation and root evidence:
  core-gui-display-waterfall-overlay-description-report.md and
  core-gui-final-handoff-root-proof.json.
  The phone's TX-filter overlay default differs from the desktop's actual
  persisted-load default; its owner has been asked to align absent/default
  behavior while preserving explicit user settings and check TX geometry.
  The phone renderer is still phone-owned work. Other local Display controls,
  reset/copy actions and remaining derived/contextual controls remain open.
  The phone owner confirmed that literal PascalCase binding names
  map to its existing typed per-pan/current-band model and that explicit
  `#RRGGBBAA` colours suit its boundary conversions. The phone model already
  exists in its checkout; absence in the Core checkout did not mean unbuilt
  phone rendering. These are agreed implementation details, not a claimed
  separate JJ ruling. Discrete FFT sliders use a negotiated version-4 ordered
  numeric option contract matching StationCatalog, with actual sizes as values
  and no arbitrary intermediate sizes; older projections omit them. The RX
  quartet retains its existing subscription relationship. TX analyzer settings
  apply live to the Core analyzer, without an invented RX subscription field.
  This clarifies the plan's overly broad subscription label. Remaining local
  controls, readouts, active-band grid context, 3D page and actions still need
  exact publication and phone checks. The ten Colors and Theme swatches and
  three Meter Styles controls are now described; Reset Colors remains open.
  Partial publication does not close this gap.
- Plan: phone Display/Appearance description and renderer dependencies, and
  remote-window parity.

### G-104: Disconnect capability refresh can invalidate its peer iterator

- Evidence: lead review of the new antenna capability withdrawal found a
  direct iteration over `m_peers` while calling synchronous `sendText()`.
  A receiving callback can close a session and erase that peer. Existing
  broadcast helpers already copy their destinations for this reason.
- Ruling basis: preserve session lifetime and truthful capability withdrawal;
  this is a concrete implementation defect, with no new operator policy.
- Status: signed `d9dc53cb` copies guarded transport and unique session
  identities, revalidates before each send, and stops after Core destruction.
  The regression first reproduced a replacement peer receiving two capability
  messages instead of one, then passed with the correction. Root app/Core and
  all ten affected suites pass (76.76 s; load 62.62/36.75/23.26), including
  disconnect/replacement coverage and both session conformance modes.
- Plan: radio-bound antenna commands and multi-device disconnect safety.

### G-105: New Setup metadata is exposed to older strict parsers

- Evidence: phone review of accepted `10b796751` found that the Display
  resource adds `default` metadata and `decimals` on a decimal control, while
  the prior format permits decimal formatting only on readouts. V1-V3
  projections keep these new fields, so the strict phone parser rejects them.
  The pending Appearance colour defaults have the same compatibility issue.
- Ruling basis: preserve negotiated older grammar and exact native behavior;
  this is a protocol compatibility repair, not permission to loosen parsing.
- Status: built in signed `b0bf9d264`. New Display/Appearance defaults and
  decimal-control precision stay in V4; older projections retain their eleven
  Display and ten Appearance controls without that metadata. Existing readout
  precision remains. A controlled regression failed on the V1 default leak,
  then passed after the repair. Serialized paired V1-V5 coverage and the exact
  shapes pass, as do all eleven root integration suites (55.01 s) after the
  app/Core build. The format document and both native/descriptor Hz/bin
  tooltips are corrected. The phone has not advertised Setup, so no
  shipped-client outage is claimed; its parser import remains phone-owned.
- Plan: complete Setup descriptions and strict phone compatibility.

### G-106: An antenna description filtered by live readiness can stay missing after connection

- Evidence: lead review of the V6 antenna tables found that their per-peer
  projection used the current row-edit capability instead of the peer's
  declared row feature. A paired loopback regression reproduced it: sign in
  before the same Hermes radio connects, receive scalars without tables,
  then connect and receive row capability 1 but no table. The description
  bytes/revision did not change, so no mirror delta was emitted.
- Ruling basis: JJ requires complete working desktop/phone dependencies.
  The existing contract omits tables for peers without the feature; a peer
  that declares it may receive the supported table while its live capability
  keeps editing unavailable. Transient readiness must gate edits, not remove
  the static description. No new authority or stale-radio permission follows.
- Status: signed `7102a610` corrects the projection and is integrated through
  signed `68245ce22`. Paired regression coverage verifies offline admission,
  same-radio connection, row editing, capability withdrawal and reconnect.
  The merged worker app/Core build and all four full Setup/row-command suites
  pass (30, 36, 15 and 10 tests). Root reviewed the change and reused that
  tested merge. Combined trunk app/Core and all twelve rebuilt antenna, PA,
  Setup and session suites pass (50.37 s; starting load 20.16/18.00/15.08).
  Existing Core identity/confirmation checks remain.
- Plan: Hardware antenna description and session lifecycle parity.

### G-107: Desktop pairing still requires an address instead of accepting only the RV code

- Evidence: the running verified Mac preview at signed `5c4ca88d` exposes
  Add a Core by code, but its dialog requires a Core address and its
  controller always calls direct `pairByCode`. The existing Core client
  already implements `pairByCodeFromAnywhere`. Its authenticated mailbox
  result has an identity and label with no address; the desktop currently
  turns that into an invalid URL, and its saved-target validation requires
  an address. Merely changing the button would not complete the flow.
- JJ's ruling: on 2026-09-28 he made code-only desktop RV pairing the number
  one priority because he wants to test it. The normal app flow must require
  only the pairing code, with connection details handled automatically as
  in the iPhone app. This supersedes further Setup publication as the next
  implementation priority. He then requested a mockup before implementation
  and explicitly approved the suggestion and mockup on 2026-09-28. The normal
  dialog shows the code alone, with direct-address entry available as an
  optional path; the surrounding Connections layout stays as shown.
- Required behavior: Connections > Add a Core by code accepts the code
  alone, uses the existing secure mailbox exchange, saves the verified Core
  identity under its name and connects through the existing path selection.
  Direct access is preferred when available and relay fallback stays
  automatic. Saved pairing survives network/address changes and ordinary
  reconnect/relaunch. An address remains an optional direct path. Missing,
  invalid or unavailable service targets must never fall back to starting
  a local Core. Existing trust and certificate checks remain mandatory.
  V3 saved-target migration preserves existing trust and the old document
  bytes initially. Subsequent forgets and edits also remove or update matching
  older records, as the existing V1 compatibility rule requires, so downgrading
  cannot restore forgotten credentials. Older lists gain no new V3 records;
  addressless records cannot be represented there. Failed saves restore every
  prior value. This narrows the lead's original blanket rollback-snapshot
  proposal after inspecting the existing credential-removal invariant.
- Status: implemented in signed `e09474320` and reviewed by the Astra lead
  for identity validation, credential migration and shutdown lifetime. Root
  app/Core and all nine rebuilt pairing, target-store, connection and route
  suites pass at actual load (67.50 s). Real mailbox fixtures cover code-only
  pairing, an addressless saved Core, authenticated WebRelay connection,
  relaunch/reconnect and same-identity re-pairing. Cancel, service refusal,
  retry and shutdown cannot accidentally start a local Core. The previous
  frozen preview remains under JJ's control. Signed `7d9292190` is now
  packaged separately, with deep strict code-signature validation and a
  manifest proving the GUI/private Core build identity and file hashes.
  JJ's standing relaunch approval is pending for that ready replacement;
  his actual Core pairing test remains to be performed.
- Plan: approved rendezvous desktop-client flow and remote-window parity.

### G-108: App wording mixes American and British spellings

- Evidence: the current UI includes Colour, centre, colours, behaviour,
  Normalise and recognised in labels, tooltips and status messages. Several
  of these strings are also published in the Core's Setup descriptions.
- JJ's ruling: on 2026-09-28 he requested American English throughout the app,
  giving color and center as examples, while explicitly preserving attribution
  and other people's comments.
- Required behavior: use American spelling in authored app presentation text
  and matching Setup descriptions and expected results. Preserve existing
  code comments, verbatim upstream/legal text, proper names, stable APIs,
  settings keys, wire/schema fields and exact reason-matching keys. Where a
  protocol key contains British spelling, change its displayed text only.
- Status: authored wording and matching Setup descriptions implemented in
  signed `50be8c9a8`. Root compared all 21 changed C++ comment streams with
  their originals: byte-identical. The eight JSON edits change only labels
  and tooltips. Integrated app/Core build and all eight rebuilt affected
  suites pass (14.47 s), including native/description parity. Six remaining
  literals in the pairing-owned files are included with signed `e09474320`
  and its nine-suite root verification. The expanded audit found five more
  authored Setup help strings: signed `bfb09fd05` changes parameterised and
  greys to American spelling, with five worker suites passing (5.43 s);
  root app/Core and all eight rebuilt integration suites pass (28.85 s).
  Compatibility keys, existing comments and verbatim source text remain intact. The phone owner has the same ruling
  for phone-owned presentation strings.
- Plan: consistent desktop/Core/phone-facing wording without compatibility or
  attribution changes.

### G-109: The wording test does not inventory the new antenna refusal paths

- Evidence: American English verification passed seven of eight suites, but
  `tst_station_reason_wording` reported five unlisted forwarding expressions
  in StationServer, StationSharedSettings and SessionCommandDispatcher. The
  new radio-bound antenna guards pass on existing human-readable reasons;
  the test's explicit source inventory had not been extended for those paths.
- Ruling basis: JJ requires discovered failures to be investigated and fixed.
  This deterministic source-inventory failure is independent of spelling and
  machine load; no wording exemption or scanner relaxation is needed.
- Status: fixed in signed `5e1edb587`, with five exact forwarding entries
  whose reason-producing functions are already scanned. Root reviewed those
  functions and the test inventory. The rebuilt full wording suite passes
  alone (6.15 s) and in the eight-suite trunk integration (14.47 s total).
  Existing coverage floors, production behavior and comments remain unchanged.
- Plan: keep all Core refusal messages covered by the operator-wording check.

### G-110: Tunneled media is counted again as control traffic

- Evidence: the phone team asked about the accounting boundary. Source review
  confirms WebSocketTransport increments its session payload counters for
  binary media-tunnel frames as well as text. StationClient and
  SwitchableTransport forward those counters unchanged; RemoteTelemetryController
  then adds the media display/RTP counters. A tunneled packet is therefore
  included in both terms, with tunnel/encryption bytes mixed into the control
  series. SessionTransport's documented UTF-8 boundary and the September 21
  telemetry design explicitly exclude media from the control term.
- Ruling basis: JJ requires discovered gaps to be recorded and built. Restore
  the documented payload accounting, retaining actual binary delivery and all
  existing connection behavior. This is a source-supported defect, not a
  claimed measurement of JJ's current connection.
- Status: signed `60d6cfe93` removes the two binary increments while retaining
  real bidirectional binary delivery and exact UTF-8 text counts. The real
  loopback regression first failed on seven binary bytes counted as text,
  then passed; worker app/Core and all three affected full suites pass
  (28.79 s). Root reviewed the actual changes; integrated app/Core and
  all eight rebuilt traffic/Setup/wording suites pass (28.85 s), after
  the priority pairing preview was packaged. Dedicated DataChannelTransport
  watch binary is separate and unchanged. The phone owner has the boundary.
- Plan: accurate Core/window bandwidth graphs and phone diagnostic dependencies.

### G-111: Newer payload channels are absent from the desktop traffic total

- Evidence: LibDataChannelMediaTransport's observable traffic contains display
  and RTP counts only. Its separate transmit-keepalive and raw-IQ channels
  have no corresponding payload counters; RemoteTelemetryController sums only
  control text, display and RTP. The dedicated transmit-watch transport has
  its own counters, but the controller's selected-session snapshot does not
  aggregate that separate path. Thus the current total does not cover all
  newer application channels. No live traffic discrepancy has been measured.
- Ruling basis: JJ's complete parity and diagnostic goal includes truthful
  accounting. Preserve the documented application-payload boundary: count each
  observed/submitted payload once, excluding encryption, tunnel envelopes,
  protocol framing and network overhead. No delivery guarantee can be inferred
  from a submission counter, and unavailable observations must remain absent.
- Status: signed `1723c726c` adds media transmit-keepalive/raw-IQ counters.
  Real encrypted-channel regressions first observed missing counts, then
  passed with exact payload counts, restart resets and valid IQ arrival
  counted before the existing bounded-queue failure. Root reviewed production
  and tests; app/Core and all eight rebuilt media/portability suites pass
  (28.80 s). Auxiliary-watch commits db05be910 and e8fb6d402 are integrated
  after root review and app/Core plus all six rebuilt watch/telemetry/
  diagnostics suites passing (36.70 s). A real paired watch opens and retires
  between graph samples; its 33-byte attach and validated 2-byte ACK survive
  exactly once, and a same-primary retry adds its own counts. An explicit
  test clock disables automatic sampling; production keeps its one-second
  timer. No wire, permission, deadline, RF or delivery guarantee changed.
  Keep this separate from G-110's WebSocket overlap and physical network use.
- Plan: complete Core/window application traffic observations for R5 and IQ.

### G-112: Cross-platform test fixtures assume one service manager and one ICE path

- Evidence: the immutable Linux `5c4ca88d` full run failed the disconnected
  Setup page inventory, stale-negative route ranking, and three desktop
  background-service assertions. The new combined export button legitimately
  depends on Core access; a Linux service-status query is a read operation;
  loopback ICE selection has floor rank; root can bypass a chmod write block.
- Ruling basis: JJ requires failures to be explained and fixed at actual load.
  Preserve production behavior and test the intended contracts explicitly.
- Status: signed `48764cd41` fixes the three test fixtures and passes app/Core
  plus four affected Mac suites (23.25 s). Lead review caught a narrowed
  service-action assertion; signed `d49ef4e48` now allows only the exact
  read-only systemctl query, rejecting all other platform commands. The full
  runtime suite passes, followed by root app/Core and all eight rebuilt
  integration suites (28.80 s). The rebuilt immutable 383984e5 Linux run
  confirms the affected gating, path-racer and desktop-runtime suites pass.
  Its separate relay-return failure is recorded in G-120. The
  completed 996-entry Linux run retains these failures against its older
  immutable source; it passed 989 entries and failed seven.
- Plan: cross-platform Core/GUI verification.

### G-113: Audio status test requires acceptance of a legitimately superseded reply

- Evidence: the Linux full run failed to observe an accepted RadioOffline
  audio context. A controlled Mac reproduction mirrors the disconnected
  radio before the already-published reply reaches the GUI. The trace shows
  Core revision 1/generation 2 with radio-offline, GUI request revision 2,
  Core revision 2/generation 3 with client-disabled, then delivery of both
  replies. The GUI correctly rejects the older revision; the previous test
  wrongly required its acceptance regardless of message order.
- Ruling basis: JJ requires a cause and fix, without increasing deadlines.
  Keep the current revision guard and prove both publication and current
  window status through the actual Core/GUI session.
- Status: signed `8dec4aec5` test-only correction verifies Core emits the valid explanation,
  the GUI receives it, the newer reply matches the latest request, the window
  shows RadioOffline, and a deliberately superseded reply is never accepted.
  Ordinary delivery and controlled mirror-first rows preserve forged-context
  refusal and reconnect/audio recovery checks. The controlled old assertion
  failed (16.98 s); after correction app/Core and the full audio-session
  suite pass (47.62 s). The rebuilt immutable 383984e5 Linux run confirms
  the full remote-audio-session suite passes (46.67 s). No production
  audio behavior, deadline or tolerance changed.
- Plan: truthful audio status and load-sensitive protocol verification.

### G-114: Lossless remote VAX level differs during the loaded Linux test

- Evidence: immutable Linux `5c4ca88d` reported remote slice-B amplitude
  0.0898856 versus local 0.0950019, exceeding the existing 0.0005 lossless
  tolerance. The fixture samples independently paced local and remote buffers
  after a fixed frame offset. Its current log lacks render underrun and
  phase-continuity evidence. A later mixed-stream row passing does not
  explain the failure.
- Ruling basis: JJ requires the cause and suggested fix at actual load.
  Preserve the amplitude contract and measure delivery and playback boundaries
  before choosing a production or fixture repair.
- Status: signed `ecbcbe90` fixes a fixture defect reproduced by delaying the operator's
  route until two seconds had already been rendered. The old assertion failed
  at 0.0374726 versus 0.0949995: playback first became audible at frame 107584,
  after the fixed 48000-frame skip, with zero feeder drops, trims or restarts.
  The test now captures equal fixed three-second local/remote windows after
  playback begins, retaining the original one-second settling interval,
  amplitude tolerance and all later silence/discontinuities. Both single and
  mixed-slice delayed-route regressions pass, followed by app/Core and the
  full VAX suite (45.12 s). No production behavior or deadlines changed.
  The original Linux run lacked this trace, so its exact sample history is
  not retrospectively established; Linux confirmation of the repair remains
  pending. Evidence: core-gui-vax-startup-{probe-red,green-focused,green-ctest}.log.
  (corrected 2026-09-28) Linux confirmation is recorded: the `ec28eeca` Linux focused run
  (which contains `ecbcbe90`) passed `tst_remote_vax_feeder` in 133.64 s, 30 of 30 suites
  (core-gui-linux-ec28eeca-focused-build-and-test-graph-fixed.log). Windows remains.
- Plan: local/remote VAX audio parity under real load.

### G-115: Docker's read-only network settings prevent traversal setup

- Evidence: the Linux full run's traversal harness exits before any scenario
  when its private network namespace writes disable_ipv6. Read-only inspection
  confirms `/proc/sys` is mounted read-only despite NET_ADMIN and SYS_ADMIN.
- Ruling basis: JJ requires the actual traversal scenarios to run. Correct
  this disposable test environment, preserving isolation and all assertions.
- Status: a separate network-isolated disposable container with Docker's
  systempaths=unconfined option successfully created a private network
  namespace, set/read its IPv6 switch and removed it. This proves the setup
  prerequisite only. The next run also exposed a missing /dev/net/tun;
  an explicit device mapping passed the private namespace/TUN/nft probe.
  With both corrections, the unchanged immutable 5c harness passed all eight
  peer scenarios. Its first full-session helper then crashed (G-116).
  The completed older full run retains its setup failure. No host network
  settings, live service or radio were changed.
- Plan: complete Linux remote-access traversal verification.

### G-116: Linux traversal Core helper crashes before session registration

- Evidence: after the corrected isolated container passed eight peer traversal
  scenarios, its first session-direct Core helper exited with SIGSEGV before
  writing its registration ID. The retained log ends after successful
  PortAudio initialization; this is not proof that PortAudio caused the crash.
  Harness cleanup removed its per-scenario logs, leaving no stack trace.
- Ruling basis: JJ requires causes and fixes for failures at actual load.
  Preserve the original deadlines and full-session acceptance criteria.
- Status: source/artifact cause established in the retained independent
  reproduction. The old EXCLUDE_FROM_ALL helper allocates 8104 bytes for
  RadioModel, while the matching current Core requires 8136. The crash is
  in QObjectPrivate::connectImpl from RadioModel construction. The root's
  local all_tests build omitted this helper, leaving a stale binary beside
  fresh libraries. CI's separate traversal job explicitly builds the helper.
  Correct the local build recipe/dependency coverage, rebuild a matching
  helper/runtime and rerun full-session traversal before closing this gap.
  Evidence: core-gui-traversal-5c-triage-report.md. No live service, radio or
  host network mutation occurred.
- Matching-build verification: the rebuilt helper and frozen 383984e5
  libraries pass session-direct, including authenticated registration. The
  full run also passes direct/relay/media, IPv6, tunnel and path-upgrade
  scenarios before reaching a separate relay-release failure (G-122).
  Its later watch-deadline scenario could not create its report directory
  because the root mounted the build read-only. Supply the harness's existing
  DEADLINE_OUT setting with a writable scratch path before resuming those
  scenarios. Both all_tests and tests_traversal now depend on the helper
  when Linux traversal is enabled. The actual registration/aggregate CMake
  blocks were configured in an isolated Ninja graph probe: both targets
  include the helper when enabled, and the disabled configuration remains
  opt-in. The test-registration verifier passes. The scratch full Linux
  recipe also names the helper explicitly. Logs:
  traversal-build-aggregate-probe/verification.log,
  traversal-383984e5-matched-session-direct/console.log
  and traversal-383984e5-matched-full/console.log.
- Follow-up verification: with DEADLINE_OUT directed to writable scratch,
  both web-relay and direct-WebSocket watch-deadline scenarios passed their
  original 60-second synthetic-key checks at both 2% and 3% loss with 75 ms
  delay. No false watchdog stop occurred. These are isolated fake-radio
  tests, not physical RF or a claim of continuous keyed receive audio.
  Evidence: traversal-383984e5-matched-web-relay-deadline and
  traversal-383984e5-matched-direct-wss-deadline. The full generated Linux
  aggregate graph is now verified in the actual generated ec28eeca Linux
  build: both all_tests and tests_traversal include the matching helper.
  The signed snapshot built app/Core and helper, then passed all 30 affected
  suites in 191.75 seconds. Evidence:
  core-gui-linux-ec28eeca-focused-build-and-test-graph-fixed.log.
- Plan: complete R5 full-session Linux traversal verification.

### G-117: Desktop connection header does not identify the active route

- Evidence: the running signed preview `7d929219` reports connection state,
  traffic and timing, but not the selected direct/relay path or IP family.
  Control and media may use different paths; RV discovery alone does not
  establish either route. The source scout identifies the actual transport
  peer and selected ICE pair as the evidence required for each channel.
- JJ's ruling: put this information where "Core connected" appears and show
  a mockup first. JJ rejected the first mockup's stacked header and inaccurate
  surrounding app. Preserve the existing single-row header and app appearance.
- Further ruling: "keep the traffic, audio, core to radio, core round trip
  in the header and for the drop down for the rest thats nice". Retain these
  four measurements in the single existing header row; the connection
  drop-down holds the route, IP family and addresses, with separate control
  and audio/display paths. Preserve the existing app appearance and controls.
- Status: layout approved with that correction. The revised mockup keeps
  all four measurements visible while its drop-down is open; verified at
  desktop width with keyboard activation. JJ then confirmed "this header
  area look ok." Source inspection is complete; implementation needs a
  current media-path snapshot and truthful tunnel endpoint metadata as well
  as the compact header/popup wiring. Do not infer a relayed connection from RV discovery or expose
  a tunnel's loopback shim as the internet peer. Clear stale path details on
  reconnect and identify unavailable facts honestly. Core-only route
  snapshots are signed in 717de0552, with the worker app/Core build and
  seven affected full suites passing in 181.08 seconds. Root reviewed the
  actual implementation and tests. Signed trunk merge 47d16744 accepts the
  Core metadata together with Display V9 after an app/Core build and all
  11 affected suites passed in 109.71 seconds at actual load. GUI wiring is
  signed in 018f97399 and corrected in 9ec154d20 after independent Sol 6 high
  review found muted/unavailable audio states were lost from the visible
  header. The correction preserves typed audio status and the single-row
  fit. Root reviewed the final changes and integrated them in signed
  9a4971798 after rebuilding app/Core and all five affected complete suites,
  passing in 77.03 seconds at load 4.48/5.25/6.79. Evidence:
  core-gui-header-final-root-{proof.json,build.log,ctest.log}. Native popup
  interaction remains unverified; offscreen tests cover keyboard activation,
  current routes, stale clearing, width, mute and unavailable presentation.
- Plan: remote-window connection visibility and R5 operator diagnostics.

### G-118: Slice ownership, takeover and dead-session release are unclear

- Evidence: JJ sees this Mac twice and cannot identify which session owns
  which slice or how to take/release one. Source confirms same-ID admission
  replaces the older transport, while separate identities can share a name;
  the live duplicate rows have not yet been identified. Paired link loss
  retains slices for a 180-second reconnect grace period. The connected-device
  list is read-only. Remote Take is offered only when an add/retune is blocked;
  the hosting desktop bypasses that chooser entirely.
- JJ's ruling: approved handing over the existing slice intact: "Yes, hand
  over the existing slice (recommended)". Take control preserves letter,
  color, frequency, mode and filter, tells the former controller it no longer
  controls that slice, and leaves transmit control as a separate action.
  JJ also approved "Yes, shared listening with one controller (recommended)":
  another paired phone/desktop can choose Listen in, adjust its own volume
  and mute, and must Take control before changing shared frequency, mode or
  filter. JJ approved release: "Keep it for listeners; otherwise close it
  (recommended)". Keep a released slice playing for remaining listeners and
  available for Take control; with no users, close it and free resources,
  including the last physical Core slice. This changes the old minimum-one
  rule. JJ also approved "Keep listening after handoff (recommended)": the
  former controller retains audio as a listener and sees the new controller.
  JJ approved "Clear transmit selection on handoff (recommended)": an idle
  slice handoff clears that slice's transmit selection, requiring the new
  controller to select it explicitly. Taking an actually transmitting slice
  is blocked until transmission stops. JJ approved "Release after three
  minutes (recommended)": after the reconnect grace expires, remove the
  absent client's control/listening claim, keep slices for other listeners
  and make them available to control, or close them if nobody remains.
  JJ said the bottom area seems good. Its relationship to flags, RX/TX
  applets and multiple pans remains under discussion. The proposed bottom RX
  badge opens an all-slice chooser, separate from TX selection. A focused
  existing-style mockup is prepared as nereus-slice-chooser-review.html;
  its Listen, Take control and Release interactions and narrow-width layout
  were checked. This is a design preview, not built app behavior.
- JJ's window rulings (2026-09-28), quoted exactly as the slice crew ledger records them,
  each followed by the lead's recorded reading. They settle what the bottom-area approval left
  open (flags, RX/TX applets, multiple pans) and the TX applet letter idea below:
  - U1 "1 your recommendation but maybe not a floating pan but a layout that fits in the
    single window": an unseen slice goes into the main window: an existing main-window pan
    showing it comes forward, else an empty main-window pan, else the main window grows to a
    pan layout that fits in the single window, or asks for a named destination; placing a
    slice never opens a new floating pan. Lead confirmation (2026-09-28): ask only when no
    larger single-window layout fits; growing the layout to place a slice adds no new slice
    to any other empty pan.
  - U2 "1 your recommendation": a slice already showing in a floating pan brings that floater
    to the front and switches the main window's RX to it; nothing moves, no second copy.
  - U3 "1 your recommendation": a pan background click keeps today's display-only meaning
    (keyboard/scroll focus); slices are chosen by flag or tab.
  - U4 "1 your recommendation": same-pan stacking keeps today's rule, the selected slice's
    flag on top; listened slices keep their color with "Listening · controlled by ..." text.
  - U5 "1 your recommendation": the existing AF slider and mute on any slice (controlled or
    listened) change only what this device hears; labeled "Your volume" on a listened slice;
    nobody's volume or mute changes anyone else's audio, the controller's included. So the
    controller's AF must become per-device too (plan Task 6). Lead ruling (2026-09-28,
    settles Q2 vs U5): U5 wins; a listener's audio never depends on the controller's AF,
    including AF at zero (feed taken before the controller's AF; the controller's own path
    unchanged).
  - U6 "1 your wording sounds fine": flag text "You control" (menu: Release); "Listening ·
    controlled by <device>" (menu: Take control, Stop listening; tuning disabled with
    "<device> controls this slice"); "TX" as today, red on air.
  - U7 "stop listening to slices, its confusinf to hear slices you have no visual referance
    to": a listened slice that loses its pan in a layout change stops being listened to on
    this device (with a plain notice); a device hears only slices it can see. Controlled
    slices keep today's rehoming into a remaining pan. This supersedes the design's earlier
    "replacing or hiding a view retains listening until explicit leave" and its RX-applet
    tabs for hidden joined slices.
  - U8 "2 but for only slices tgat are activatyed show in the applet": the TX applet shows a
    row of slice letter buttons, only for slices active on this device (read: slices this
    device controls, the only ones it may transmit on); pressing one selects it for transmit
    through the existing `tx.setTxSlice` behavior, the same for every client (ruling 8.10:
    while keyed it unkeys, then moves). Correction (lead, 2026-09-28): an earlier record here
    added "idle only; refused on air"; that was the lead's addition, not JJ's ruling, and is
    withdrawn.
- Status (2026-09-28): RULED. The six policies, the bottom area and U1 to U8 are settled and
  recorded in `2026-09-28-slice-control-and-listening-design.md`; the implementation plan is
  `2026-09-28-slice-control-and-listening-plan.md` (17 tasks, committed `2c0c5566e`, lead
  rulings Q1 to Q17). In progress: plan Task 1 (slice incarnation and control revision) is
  complete on lane `codex/slice-access` in signed `efb398303`, not yet in the trunk. Nothing
  else from this entry is built. The earlier status below is kept as history.
- Earlier status (before 2026-09-28's rulings): OPEN. Recommend visible slice-specific actions and explicit owner,
  this-window, hosting and away labels, preserving confirmations that name
  every affected listener and the protection for an on-air transmit slice.
  A release must not revoke pairing. Current Take frees capacity by closing
  affected slices and applying the new request; a proposed Take over action
  that preserves an existing slice's tuning and identity is a distinct,
  now-approved operation. Settle its remaining edge cases and release semantics before
  implementing a new action. Read-only evidence is retained
  in core-gui-slice-ownership-reconnect-scout.md.
- Further discussion: JJ suggested showing every existing slice letter in
  the TX applet, with a foreign-slice click showing information and a
  takeover action, but explicitly said this is not yet the chosen design.
  (Settled 2026-09-28 by U8 above: only this device's controlled slices
  appear in the TX applet row.)
  Compare slice transfer, slice release and freeing a shared hardware
  receiver before implementing. Receive ownership must remain distinct
  from selecting or taking transmit. A read-only investigation of the
  actual duplicate desktop identities could not identify the live row IDs;
  the duplicate-name cause remains unproven.
- JJ then asked how the bottom banner's active-slice indicator fits the
  flow, and for matching iPhone/desktop behavior when switching, taking,
  releasing, reaching capacity or sharing a slice. The existing banner
  follows this window's active receive slice; transmit selection is separate.
  Current media mixes include only owned slices; the now-approved Listen in
  action requires new audio subscription support with one tuning controller.
  It is not existing capability or permission for multiple writers.
- JJ explicitly requested a deep usability and completeness scout of creation,
  ownership, release and takeover across desktop and iPhone. Sol 6 high
  completed the source audit of actual screens, commands, capacity, audio,
  reconnect and failure recovery; report core-gui-slice-experience-deep-scout.md.
  Root reviewed its findings against StationReceivers.cpp. The hosting
  desktop lacks a capacity Take entry; a desktop empty-state message can
  wrongly blame another device after deliberate self-release; phone close
  and active-selection refusals are log-only; a final physical Core slice
  cannot be closed even though a client may lose its final own slice. These
  are separate gaps in reachability, feedback and release policy. Same-slice
  listening and intact transfer are absent. Recommended common inventory
  and explicit actions remain design input except for JJ's subsequent
  approvals of intact Take control and one-controller shared listening above.
  Existing stale-confirmation and on-air protections are present. The
  suspected failed-allocation victim loss is tracked separately below.
- Plan: several-devices UX and local/remote ownership parity.

### G-119: Slice markers and RX badges do not consistently use Aether colors

- Evidence: flags and slice tabs use the established A cyan, B magenta,
  C green, D yellow palette. SpectrumWidget::drawSliceMarker hardcodes cyan
  for every own slice, and the RX applet's current-letter badge remains blue.
  The hosting desktop also hides foreign flags without populating the
  documented dashed, owner-labeled foreign markers; hidden flags still
  contribute own-style marker geometry. These surfaces are unchanged between
  preview `7d929219` and trunk `4a6b7b05`.
- JJ's ruling: "we have stable colors selected like in the iphone we used
  the aether colors". Preserve that established palette and slice identity.
  Ownership is separate from the slice color.
- Status: own-marker and current-badge repair integrated from signed
  561dd89c. The app/Core build and eight affected full offscreen suites passed
  together in the trunk (2.58 s), including painted B/C/D cues and existing
  E color fallback. Existing receive-filter fill and alpha settings are
  retained. Hosting foreign-marker repair from signed 53b916dd now passes
  the trunk app/Core build and all four affected complete suites (4.74 s,
  starting load 5.88/9.29/8.88). It uses authoritative ownership, presence
  and TX state for the existing dashed owner-labeled markers, suppressing
  foreign own-style marker, 3D shadow and offscreen-arrow cues. A real
  raster regression exposed the separate arrow defect during lead review.
  No new Take/Release behavior is authorized by this repair, and the running
  preview has not been updated with it. Evidence:
  core-gui-host-markers-root-build.log and
  core-gui-host-markers-root-ctest.log.
  Evidence: core-gui-slice-colors-root-build.log and
  core-gui-slice-colors-root-ctest.log.
- Plan: several-devices visual parity and truthful ownership presentation.

### G-120: Linux service fixture expects relay removal on a loopback ICE path

- Evidence: immutable Linux 383984e5 built app/Core and 23 affected targets;
  22/23 named suites passed. The rendezvous service row expected no Core media
  relay but received one. A Core log shows 127.0.0.1:49081. The test checks
  non-relayed, while mediaIceFor also excludes loopback-shim paths; the
  current shim classifier classifies all 127.* paths as shims. The exact
  selected pair at the later failing assertion was not logged, so a path
  change or late candidate classification is not excluded.
- Ruling basis: JJ requires a cause and fix for failures at actual load;
  preserve relay cleanup and reachability policy, with no deadline increase.
- Status: explicit locally admitted CandidateSource endpoint provenance
  from signed d77cb3e1 passes the combined trunk app/Core build and all ten
  complete affected suites (112.81 s, starting load 18.11/18.52/13.31).
  Real loopback ICE no longer becomes a shim merely by address or a remote
  foundation string. The selected endpoint must match a successfully
  admitted candidate from this transport's own source. Lead review also
  found that an old source callback could affect a restarted peer; weak
  exact-lease guards fix that reproduced lifetime defect. The real service
  fixture reads both current authenticated inner transports, with epoch
  and lifetime checks, and preserves ALLOCATED 2 and RELEASED 2.
  Root logs: core-gui-ice-provenance-root-build.log and
  core-gui-ice-provenance-root-ctest.log. Matching signed ec28eeca Linux
  app/Core/helper built and all 30 affected complete suites passed in
  191.75 seconds, including the original rendezvous service suite (69.88 s).
  Evidence: core-gui-linux-ec28eeca-focused-build-and-test-graph-fixed.log.
  The original failing selected pair was not recorded, so this does not
  prove every contributor to that historical failure.
  Evidence: core-gui-linux-relay-return-scout.md and
  core-gui-linux-383984e5-focused-build-and-test.log (116.99 s).
- Plan: real-service R5 session and relay-release verification.

### G-121: Full slice flags and the current RX badge stop labeling after D

- Evidence: VfoWidget::setSliceIndex and RxApplet::setSliceIndex update
  badge text only for IDs zero through three. The Core's stable slice ID
  maps to a letter beyond D as well, and tabs/foreign labels use that
  mapping. A real slice E can therefore retain A or a previous badge letter.
- Ruling basis: the approved several-devices design requires Core-assigned
  slice letters to agree across clients. JJ reiterated that the existing
  Aether palette must be preserved; no new color or identity policy is needed.
- Status: integrated from signed 2bba1a3a after 561dd89c. The D-to-E-to-B
  regression failed on the original stale D badge, then passed with all
  eight affected full suites in the combined trunk (2.58 s). Both badge
  setters preserve the Core's exact existing letter mapping and color
  fallback. No ownership mutation or new Take/Release action is included.
- Plan: truthful slice identity throughout the desktop window.

### G-122: Real Linux relayed-session teardown returns only one reservation

- Evidence: the matching frozen 383984e5 traversal run connects the real
  relayed session successfully, then counts one returned reservation where
  the harness requires two. The later relayed-media scenario returns all
  four reservations and passes. This is separate from the loopback-shim
  classification finding; no cause has yet been established.
- Ruling basis: JJ requires the cause and a suggested fix for failures at
  actual load. Preserve the two-reservation release assertion and deadlines.
- Status: OPEN investigation. The full console is retained under
  traversal-383984e5-matched-full; its original per-scenario Core log was
  overwritten by later scenarios and its cleanup did not preserve coturn's
  log. The focused session-relayed run with identical verified helper/library
  hashes returned both allocations and passed, with coturn and session logs
  now retained under traversal-383984e5-matched-session-relayed. One passing
  retry does not establish the earlier failure's cause. No live rendezvous
  service or radio was changed.
- Bounded follow-up: five focused runs returned both allocations, and a
  replay of the original ten-scenario prelude also returned both. The
  prelude failed a separate IPv6 assertion (G-123). The original one-return
  failure remains unexplained; these passes do not close it. Coturn and
  session logs are retained in the release-probe and release-prelude
  directories, with core-gui-relay-release-bounded-probe.json summarizing
  the five focused runs. No deadline or release assertion was relaxed.
- Plan: R5 relay cleanup and real-service traversal acceptance.

### G-123: Dual-stack traversal sometimes selects IPv4 despite IPv6 acceptance

- Evidence: the unchanged ipv6-both scenario in the matching 383984e5
  release-prelude run connects and echoes at 139 ms using the IPv4 pair
  198.51.100.6 to 198.51.100.10, then fails its requirement for an IPv6 pair.
  The preceding full run passed that requirement. The scenario enables
  routed IPv6 alongside IPv4 NAT and denies relay; connectivity alone is
  insufficient to satisfy its existing acceptance criterion.
- Ruling basis: JJ requires causes and suggested fixes for failures at
  actual load, without weakening assertions or waiting for a quiet machine.
- Status (corrected 2026-09-28): test defect repaired and accepted in signed trunk merge
  `4c0e0e585` (`ea7494545` plus root review fix `de4c657d0`); see "Accepted test correction"
  below. The separate connected-but-no-echo run is G-127. History of the investigation:
  preserve the selected-pair evidence and
  investigate candidate availability, nomination and observation timing.
  A source read shows the peer reports its path on its first echoed message;
  that alone does not establish the failure's cause. Evidence:
  traversal-383984e5-matched-release-prelude/console.log.
- Diagnostic follow-up: a separately hashed helper linked to the read-only
  ec28eeca Linux libraries now retains verbose candidate/nomination logs and
  observes the selected path for ten seconds after its original first-echo
  result. The original acceptance assertion is unchanged. Its first run
  selected IPv6 host candidates at 135 ms and remained IPv6 at 10047 ms,
  passing the assertion. This does not reproduce or close the intermittent
  IPv4 failure. Evidence: traversal-ec28eeca-ipv6-observation-1 and
  ipv6-diagnostic-ec28eeca/artifact-hashes.json.
- Reproduced cause on ec28eeca: the first bounded extra diagnostic run
  echoed over IPv4 srflx at 97 ms, then selected and nominated IPv6 before
  the 10170 ms observation. Both IPv6 candidates were already exchanged.
  The trace shows the IPv4 connectivity check succeeding first, followed by
  the higher-priority IPv6 check and nomination. The immediate family
  assertion samples a valid intermediate ICE state. The existing
  rendezvous IPv6 unit test already waits up to ten seconds for preference.
  Root reviewed the trace and assigned a test-only correction that observes
  both selected IPv6 endpoints within that window and the unchanged outer
  deadline. IPv4-only failure and delayed-IPv6 success must be verified.
  This establishes the new reproduction's cause, not the older untraced
  run's exact history. Report: core-gui-ipv6-nomination-probe-report.md.
- Accepted test correction: signed trunk merge 4c0e0e585 includes ea7494545
  plus root review fix de4c657d. The helper retains first-echo timing and
  observes the same peer for the existing preference window; only ipv6-both
  opts in. Controlled IPv4-first then IPv6, bounded IPv4-only rejection,
  ordinary IPv4 and normal dual-stack checks passed. Root caught that the
  harness masked helper exit status, added connected/error checks, proved
  the error and disconnected cases failed before/fixed after, and reran
  delayed IPv6 successfully. Matching trunk helper build passed. Evidence:
  core-gui-ipv6-preference-fix-report.md and the root-acceptance-delayed run.
  The first-echo observation defect is repaired. A separate connected but
  no-echo run remains an open finding below; successful retries do not
  explain it.
- Plan: R5 IPv6 path preference and truthful route reporting.

### G-124: Bottom banner can retain a slice after the window loses it

- Evidence: MainWindow::refreshActiveSlicePresentation binds a null slice
  when a hosting window has no owned slice, but updates its letter only
  for a non-null slice. RxDashboard::bindSlice returns on null without
  clearing the previous letter or readings. The general dashboard rebind
  lambda also returns before clearing the binding when no active slice
  exists. This can misrepresent an old slice as the current one.
- Ruling basis: the agreed per-device active-slice and truthful ownership
  presentation require the banner to describe a currently owned slice.
  JJ specifically asked how that banner identifies the active slice.
- Status: built in signed 9925ed9a and integrated with the current networking
  base. The dashboard clears its old letter, readings and click targets,
  forwards empty bindings and guards slice destruction. The worker reproduced
  both stale display and the destroyed-slice crash before the fix. Root
  reviewed the diff, rebuilt app/Core and five affected complete suites, all
  passing in 3.04 seconds at starting load 6.78/12.61/13.35. Logs are
  core-gui-empty-banner-root-{build,ctest}.log. No new picker, takeover
  behavior or ownership policy is included. Matching signed ec28eeca Linux
  app/Core and all 30 affected suites, including banner/chrome, passed in
  191.75 seconds; log core-gui-linux-ec28eeca-focused-build-and-test-graph-fixed.log.
- Plan: truthful active-slice presentation and empty-window recovery.

### G-125: Confirmed capacity Take may close a slice without fulfilling the request

- Evidence: the read-only deep slice scout traced proceedTakeReceiver and
  proceedTakeSlice: both call closeForTake before applyHeld, then tellTaken
  regardless of whether the held operation succeeded. No rollback spans
  those steps. A source-derived example fills the configured HL2 two user
  DDCs and five slice slots: four compatible A-owned slices share one DDC,
  C owns one on the other, and B has none. Taking one A slice frees a slice
  slot but no DDC for B's new pan, so creation can fail after removal.
  The following fake-radio regression now proves this case; no physical
  radio result is claimed.
- Ruling basis: JJ requires complete, usable slice handling and causal fixes
  for discovered bugs. A failed action must preserve existing slice work.
  No new ownership or sharing policy is authorized by this repair.
- Status: reproduced by Sol 6 high on signed ec28eeca in the existing
  confirmation suite using the configured HL2 fake-radio pool. Confirmed
  Take returned accepted=false with the receiver-full reason; A lost slice
  0 and received both object.destroy and a Take notice, while B gained no
  slice. The invariant that a failed action preserves A's IDs failed.
  The bounded Add placement preflight and confirmation repair is signed
  in 6848a1d44 after root diff/test review. App/Core built, all 74 confirm
  cases passed, and the 91 complete session suites passed in 243.26 s at
  actual load. Evidence: core-gui-slice-take-atomicity-fix-report.md and
  core-gui-slice-atomicity-session-tests.log. It checks the copied allocator after exactly the confirmed
  removals, and offers the receiver chooser when one slice cannot free the
  required receiver. Existing ownership, affected-set, expiry and on-air
  guards remain intact. This is not a general rollback transaction. Review
  also identified PanMove and Take-back paths that close victims before a
  failing or partial replay. Both now have deterministic original-source
  RED regressions and bounded preflight repairs in signed c54147ae9. The
  planner checks carried frequencies and the whole saved group's slice/DDC
  capacity before closing exactly the shown victims, and rechecks their
  controller identities. The actual new-stream rate is shared with RadioModel.
  Worker verification: 78 confirmation cases and nine complete affected
  suites passed; the broader 91-suite session run passed 90 and found one
  missing wording-origin declaration. Signed test-only cae94f7c adds a real
  ReceiverPlanner wording scan and exact forwarding declaration; its full
  wording suite passes without changing product messages. Root app/Core and
  ten affected complete suites passed in 72.89 seconds at load 10.31/8.15/7.09;
  the final integrated wording suite also passed. Evidence:
  core-gui-slice-move-restore-report.md and core-gui-slice-move-restore-root-proof.json.
  This remains OPEN for broader transaction safety: direct callbacks during
  closing and non-capacity restoration failures can still leave a partial
  result. Abnormal stale settings differing from a locked saved frequency
  are not repaired by the capacity preflight. Keep new intact-transfer UX
  separate; neither preflight establishes general rollback.
- Plan: reliable several-client capacity operations and recovery.

### G-126: Pan layout changes also allocate or move station slices

- Evidence: MainWindow::applyPanLayout rehomes slices when shrinking,
  spreads them across empty pans when growing, then populatePanSlices
  attempts addSliceOnPan for every empty pane. showPanLayoutDialog caps
  layouts by user hardware receiver count because a new independent pan
  requests another receiver. These assumptions describe owned slices;
  shared listening also needs to show a receiver already used by another
  device without creating a new slice or taking tuning authority.
- JJ's request: the bottom chooser looks good, but its effects on flags,
  applets and multiple open pans are unclear. Explain and review the whole
  flow before changing the UI. Approval of the bottom area alone does not
  settle these behaviors.
- Earlier status (before 2026-09-28's rulings): OPEN design. Root recommends keeping per-window visual placement,
  per-device receive selection and Core tuning authority separate. Selecting
  an already displayed slice should focus its existing pan; a view change
  must not silently move another slice's tuning or take ownership. How to
  show an unseen receiver and how layout changes offer new slices need to
  be part of the complete design. The Sol 6 medium scout has completed its
  trace against signed 6848a1d44. Pan-background selection currently changes
  display focus only; a flag or RX tab changes window RX focus. Spectrum
  tuning targets the emitting pan's own selected slice. RX controls and
  flag audio controls currently write shared state, so listened flags need
  explicit read-only tuning and independent local volume/mute. TX applet
  bindings also use active RX in places and must be audited before allowing
  a listened RX to become selected. None of these findings grants TX rights.
  Report: core-gui-multi-pan-slice-flow-scout.md. Real offscreen two-pan Qt
  captures informed the full-window interactive proposal delivered to JJ as
  nereus-multi-pan-slice-flow.html. Preview checks cover joined flags/RX tabs,
  independent TX, read-only listening, intact handoff and explicit destination
  for an unseen slice without silently releasing hidden audio. The proposal
  and remaining placement questions are recorded in
  2026-09-28-slice-control-and-listening-design.md. No new layout policy or
  shared-listener UI has been implemented or approved.
- JJ's rulings (2026-09-28; quoted in full with readings under G-118): U1 "1 your
  recommendation but maybe not a floating pan but a layout that fits in the single window"
  (an unseen slice goes into the main window, growing it to a single-window layout or asking
  for a named destination, never a new floating pan); U2 "1 your recommendation" (a floating
  slice's floater comes forward, nothing moves); U3 "1 your recommendation" (background click
  stays display-only); U7 "stop listening to slices, its confusinf to hear slices you have no
  visual referance to" (a listened slice that loses its pan stops being listened to on this
  device, with a notice; controlled slices keep today's rehoming). U7 supersedes the proposal's
  retained hidden audio above.
- Status (2026-09-28): RULED, not built. Plan Task 16 of
  `2026-09-28-slice-control-and-listening-plan.md` carries acceptance written from these
  rulings.
- Plan: complete shared-slice UX across panes, flags and applets.

### G-127: Connected IPv6 traversal can finish without an echoed payload

- Evidence: the first normal dual-stack verification run for the IPv6
  observation correction reached a connected IPv6 host pair, but returned
  echoed=false at 90410 ms with No echo in time. Station and RV logs confirm
  connection/introduction; they do not record payload send/receive or verbose
  ICE transitions. Later successful runs cannot identify this run's cause.
  Evidence: ipv6-preference-fix-verification/normal/console.log and retained
  station/client/rendezvous logs.
- Ruling basis: JJ requires failures at actual load to be investigated at
  their cause without increasing time limits or dismissing intermittent runs.
- Earlier status (before the cause was found): OPEN causal investigation. Preserve the failed run. Add bounded
  diagnostic payload/transport observations to reproduce whether readiness,
  send, delivery or echo stalled, then repair the proved cause. The existing
  ninety-second deadline remains unchanged. A scratch helper traced ready,
  payload submission, receipt and echo against the matching frozen Linux
  libraries. Three bounded attempts passed at 189/133/81 ms, each showing
  the whole 60000-byte payload in both directions. They did not reproduce
  or explain the retained failure. The channel is unordered with zero
  retransmits, so ICE connection alone cannot prove payload delivery.
  Evidence and the next boundary probe are preserved in
  core-gui-ipv6-no-echo-probe-report.md. No production fix is claimed.
- Cause (found 2026-09-28): a product bug in `LibDataChannelMediaTransport::drainCallbacks()`,
  not a lost fragment. Under load one 2 ms drain on the answering station could collect the
  newly opened display channel together with the first 60000-byte message on it. The drain
  delivered the message first and settled readiness last, so the station's echo handler ran
  while the transport was not yet ready, `submitDisplay()` returned Refused, and the echo was
  never sent; the client then reported "No echo in time." at the unchanged 90 s deadline.
- Fix: signed `94366ef0c` settles readiness (and emits `ready()`) before delivering any display,
  raw I/Q or audio data from the same drain
  (`src/core/session/media/LibDataChannelMediaTransport.cpp:1602-1619`); signed `ac5d803a9`
  narrows the ordering contract to display, I/Q and audio (transmit keepalives are not ordered
  against ready) in `IMediaTransport.h` and `2026-09-20-remote-media-control-v1.md`. Both are in
  the trunk through signed merge `136634fc7`. No wire change; the harness assertion and the 90 s
  deadline are unchanged.
- Evidence: deterministic test `readyPrecedesMessagesThatArriveWithIt`
  (`tests/tst_media_transport.cpp:1085`) failed 5 of 5 before the fix and passed 5 of 5 after.
  On the Linux traversal harness, the frozen library had 3 no-echo failures in 190 valid
  attempts, each tracing message received while not ready, echo Refused, then ready; the fixed
  library (the frozen objects relinked with the one fixed file) had 0 in 160 attempts, under
  real and generated load. Report: core-gui-ipv6-no-echo-cause-report.md; artifacts in
  ipv6-no-echo-cause/.
- Remaining: the retained original run had no payload trace, so its exact history cannot be
  re-derived; its signature (connected IPv6 host pair, station connected, no echo at 90 s)
  matches all three reproductions. The Linux fix run used the de4c657 sources (drain code
  identical to the committed one) with one relinked file, not a full Linux build of the branch.
  By design a single unreliable 60000-byte message can still be lost on a truly lossy path;
  a future lossy fixture would need a retry or a smaller message.
- Status (2026-09-28): cause found and fixed in the trunk (`94366ef0c`, `ac5d803a9`, merged in
  `136634fc7`).
- Plan: reliable R5 direct IPv6 media transport and traversal acceptance.

### G-128: Pi upgrade rollback does not restore its saved daemon settings

- Evidence: the private pi4-docker/pi-files/upgrade-core-pi4.sh copies the
  daemon .config directory into rollback storage, but its rollback function
  restores binaries/unit/config-file/DFNR without restoring that directory.
  It also takes the settings copy before stopping Core. A failed new Core
  that changes persisted settings could leave old binaries with new state.
- Ruling basis: JJ authorizes verified Core installation while preserving
  operator radio configuration and uncommitted work. Rollback must preserve
  the previous runtime state and retain failure evidence.
- Status: repaired in the private Pi helper and verified before installing
  signed checkpoint 47d16744. Root's full-script isolated fixture reproduced
  the old start/health-failure rollback leaving new settings, then passed
  seven corrected cases: success, start failure, health failure, snapshot
  failure, restore-copy failure, restore-rename failure and rollback-stop
  failure. Failed upgrade state is retained, and incomplete rollback does
  not restart Core. Evidence: pi4-rollback-verification/green-failure-gates.log.
  The successful live upgrade used helper SHA256
  5c3243ee401c8fa80ea5765d453c0b60a66ba7e07cdb2d2d1228890dbdf41adb,
  preserved /etc/nereusd.conf and left a root-only stopped-service settings
  snapshot. See core-gui-pi4-47d16744-install-report.md. The equivalent private
  Rock helper is also repaired and root-reviewed; its full-script isolated
  fixture passes 15 cases including incomplete file/model restoration,
  daemon reload failure and the existing nonzero-stop policy. Evidence:
  rock-rollback-verification/README.md. No live Rock install was performed.
- Plan: verified Core deployment and recoverable installation.

### G-129: Private radio installer omits bundled RNNoise runtime models

- Evidence: after the successful Pi upgrade to 47d16744, full package-manifest
  verification found Default_large.bin and Default_small.bin absent from
  /usr/local/share/NereusSDR/models/rnnoise. Both are in the verified package.
  CMake installs them for nereusd and ModelPaths resolves NR3 defaults there;
  the private upgrade helper copied only the DFNR model. The package also
  contains an optional diagnostic benchmark, which is not a runtime dependency.
- Ruling basis: JJ authorized verified Core installation with all planned
  functionality. Required runtime models belong in that installation.
- Status: live Pi repaired from the verified archive, with both model hashes
  checked before and after installation. All packaged runtime files now
  match; only the explicitly named benchmark is excluded. Core had already
  cached NR3 unavailable at startup, so root then checked recorded MOX false
  and completed one service restart with both models present. Final PID2394
  is active with zero automatic restarts and unchanged radio config; RV and
  HL2 EP6 reconnected and the journal explicitly loads Default_large.bin.
  Both private installer corrections are complete and root-reviewed: Pi
  passes eleven full-script cases and Rock passes fifteen, including existing
  models, first installation, missing files and partial copy failure. Reports:
  pi4-model-assets-verification/README.md and rock-rollback-verification/README.md.
  Rock remains under JJ's active-test hold. Live Pi evidence:
  core-gui-pi4-47d16744-model-repair.log, core-gui-pi4-47d16744-model-restart.log
  and core-gui-pi4-47d16744-install-report.md.
- Plan: complete NR3 parity in deployed Core builds.

### G-130: RADE end-of-over callsigns are neither sent nor decoded

- Evidence (source read 2026-09-28 at trunk `136634fc7`): on receive,
  `RadeChannel::processIq` drops every end-of-over frame (`src/core/RadeChannel.cpp:566-572`:
  "For I2 we drop the EOO frame"). The `m_textChannel` member (`src/core/RadeChannel.h:372`) is
  never constructed and nothing emits `rxTextDecoded`, so the connection at
  `src/models/RadioModel.cpp:12625` never fires and no speaker callsign reaches the VFO flag or
  the decoded-callsign report. On transmit, nothing in `src/` calls `rade_tx_eoo`. `RadeText`
  wraps radae_nopy's raw 7-bit ASCII helpers, not the text format FreeDV sends on the air, so
  even a wired path would not interoperate with FreeDV stations. CLAUDE.md's RADE lines
  overstate this.
- Ruling (JJ, 2026-09-28, "1 your recomendation"): RADE end-of-over callsigns are built in this
  Core/GUI PR.
- Status (2026-09-28): ruled; in progress. A lane is dispatched (brief
  `claude-lane-rade-eoo-brief.md`: send and decode, interoperable with FreeDV V1, a vendored RADE
  revision that does not force the V2 weights into the build, and a transmit tail that respects
  every unkey path). Its branch `codex/rade-eoo` has no commits yet. Nothing is built.
- Plan: 3R RADE requirements (the lane's commits carry the RADE requirement IDs from the 3R
  plan).

### G-131: The two relay link-loss-while-keyed rows have no test infrastructure

- Evidence: `tst_tx_link_loss_each_path` skips its through-TURN and relay-floor rows
  (`tests/tst_tx_link_loss_each_path.cpp:453`, `:465`). Signed `ac5d803a9` replaced the stale
  "not built yet" skip text with the real gap: the relay transports exist, but nothing can cut a
  live relayed path while a key is held. `tests/tools/fake_turn_server.py` has no control to
  stop relaying an allocation; the `StandInRelay` is private to `tst_relay_session.cpp` and
  forwards until destroyed; this file's session (`RemoteAudioSessionHarness`) runs over the
  loopback link, not through the rendezvous service; and the Docker traversal harness keys over
  its paths only to measure false stops, never severing a path with the key held.
  Report: core-gui-ipv6-no-echo-cause-report.md (2026-09-28 follow-up section).
- Ruling basis: no new ruling is needed. The two rows are the relayed paths' existing
  unkey-on-link-loss checks in that test; they stay skipped with the true reason until the
  infrastructure exists, and their assertions do not change.
- Status (2026-09-28): OPEN, not started. Needed: a keyable Core and window session through the
  service, a sever control added to the fake TURN server, and the stand-in relay moved to a
  shared test header with its own sever control; then enable the two rows.
- Plan: R5 restrictive-network transmit deadline (with G-55 and G-70).

## How this addendum is kept

New gaps are appended here as they are found, each with its own `G-` number (next available
number, not reused). A ruling is recorded only when a source quotes the operator's words or
otherwise plainly records his decision; short of that, the entry stays marked OPEN with the
options and a recommendation, never a guessed ruling. Status is updated in place as work lands:
"queued" becomes "in progress" when a lane is dispatched, and "in progress" becomes "ruled and
built" with the landing commit hash once it is in the trunk (`codex/checkpoint-b` or its
successor). Entries are re-sorted (open questions first, then in progress, then queued, then
ruled-and-built) whenever the addendum is revisited, so JJ always sees what still needs him at
the top.
