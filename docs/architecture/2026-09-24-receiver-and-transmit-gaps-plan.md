# Receiver and Transmit Gaps Implementation Plan

> **Execution:** run with `crew` under `cost-aware-execution`. Requirements and
> acceptance cases are binding; test order and review effort follow the risk-based
> policy. No review between tasks; one whole-branch review at the end.

**Goal:** Fix the defects in the receiver, sample-rate, PureSignal, TCI and transmit-keying
code that a read-only inventory of the multi-client groundwork found on 2026-09-24.

**Architecture:** Each task is a bounded fix inside the code that already exists. None of them
changes the session model, which the operator is redesigning for several clients at once; each
fix holds in either model.

**Tech Stack:** C++20, Qt6, WDSP, the Protocol 1 and Protocol 2 codecs, the TCI server.

**Source of the findings:** the inventory's reports in the controller's crew workspace
(`.crew/2026-09-24-receiver-and-transmit-gaps-plan/inventory-*.md`), read at integration
`93a5708e`. Line numbers below are at that commit.

## Global Constraints

- Work in the worktree, branch and build directory the controller names at dispatch.
- Commits: GPG-signed with hooks (`NEREUS_THETIS_DIR=/Users/j.j.boyd/Thetis`), never
  `--no-gpg-sign` or `--no-verify`, no `Co-Authored-By`, no em-dash characters. Stage
  explicit paths only. Every commit names the requirement or design section it implements.
- Source first. Any logic that has a Thetis equivalent is read from Thetis
  (`/Users/j.j.boyd/Thetis`, v2.10.3.15 at 3759d096) before it is written, with inline cites
  (`// From Thetis <file>:<line> [v2.10.3.15]`) and every author tag preserved. For Hermes
  Lite 2 behaviour, `/Users/j.j.boyd/mi0bot-Thetis` is authoritative. For a hardware fact
  (receiver count, rate), prefer the gateware (`/Users/j.j.boyd/n1gp-Anvelina_PROIII` at
  8e86a61) as CLAUDE.md describes. If the source cannot be found, stop and report
  NEEDS_CONTEXT; never guess a register, bit or constant.
- Nothing in a test keys a radio. A transmit change is pending on the bench until the
  operator tries it.
- Remote parity: a remote window does what a local one does, through the Core.
- Operator wording: plain user words; every new or changed string passes
  `OperatorWording::isPlain`; no source cites inside strings; "Core" for the NereusSDR
  computer.
- Tests: prefix every ctest and test binary with `QT_QPA_PLATFORM=offscreen`; build exact
  targets (test executables are EXCLUDE_FROM_ALL); run by exact name with
  `--no-tests=error`; tests labelled `realtime` run alone. Tests never open real audio
  devices. A change to wire bytes also runs `tst_p1_regression_freeze` and
  `tst_p2_regression_freeze`, and updates a baseline only in its own commit with the reason.

## What already exists

- Slices: `RadioModel::addSlice` (`src/models/RadioModel.cpp:7023`), `addSliceImpl` (:7056),
  `addSliceOnPan` (the only cap check, :7707-7720), `addSliceWithStationId` (:4478, the remote
  window's mirror of the Core's slices), `maxSlices()` (:4354). The session verb `addSlice`
  (`src/core/session/SessionCommandDispatcher.cpp:550`). `SliceStreamAllocator`
  (`src/core/SliceStreamAllocator.{h,cpp}`; `m_maxSlices` stored at :25-30, never read in
  placement at :90-170). DaemonApp's startup top-up (`src/core/daemon/DaemonApp.cpp:968-994`).
- The live sample-rate change: `RadioModel::setSampleRateLive` (:17812-18015); step 1 drains
  only channel 0 (:17866-17869), step 9 re-enables only channel 0 (:17977-17979), while the
  rate is set on every slice's channel (:17927-17931).
- PureSignal receivers: `P1CodecStandard.cpp:905-926`, `P1CodecHl2.cpp:912-932`,
  `CodecContext.h:541-586` (`PsDdcConfig`), the fixed pattern in `P2CodecHermes.cpp:248-254`.
- TCI: `TciServer.cpp:2660-2749` (the `trx` intercept and the TX-audio holder), `:157`;
  `TciProtocol.cpp:1898-2003`; the setting `TciRateLimitMsgsPerSec`
  (`src/gui/setup/CatNetworkSetupPages.cpp:285-287`), never read by `TciServer`.
- Transmit keying: `MoxController` (`src/core/MoxController.{h,cpp}`; "last setter wins" at
  h:567-570; the missing unkey subscriber at h:562-565); raw `setMox` callers
  (`TxApplet.cpp:1084-1087`, `ContainerButtonDispatcher.cpp:253`, `TwoToneController.cpp:179,
  213, 397`, the TCI shim `RadioModel.cpp:16875-16887`); `TxSliceArbiter`
  (`src/core/TxSliceArbiter.{h,cpp}`).
- Boards: `src/core/BoardCapabilities.cpp` (Angelia :472-505, Orion :534-566);
  `docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md` section 2 table.
- Tests to extend: `tst_board_capabilities_phase3f`, `tst_codec_ps_ddc_config`,
  `tst_p1_codec_standard`, `tst_p1_codec_hl2`, the `tst_mox_controller_*` family, the
  `tst_tci_*` family, the slice and allocator tests, `tst_unbuilt_features`.

## Task 1: Every path that adds a slice keeps to the slice limit

**Requirements:** Phase 3F design section 3 (the slice cap and its message, line 188);
R-R3-21 (a remote window's request is held to the Core's rules).

**Files:** Modify `src/models/RadioModel.cpp` (`addSliceImpl`, `addSlice`, `addSliceOnPan`),
`src/core/SliceStreamAllocator.{h,cpp}` if its unused cap goes, and the session verb's result
path if it needs one. Test: the existing slice-cap test, or a new
`tests/tst_slice_cap_every_path.cpp`.

**Interfaces:** Produces the same `sliceAddRejected(QString)` wording as `addSliceOnPan`
("%1 supports a maximum of %2 slices") on every path that creates a slice on the radio's own
side.

**Acceptance:**
- At the cap, a local `addSlice()` returns -1, creates nothing, and emits the cap message.
- At the cap, the session verb `addSlice` is refused with the same plain reason in its command
  result, and the Core creates no slice.
- `addSliceWithStationId` (a remote window reproducing a slice the Core already made) is not
  refused by the window's own count.
- DaemonApp's startup top-up still stops at `min(requested, maxSlices)`.
- Below the cap nothing changes. The allocator keeps no cap field that nothing reads: either
  placement reads it, or it goes.

**Verification:** ordinary bug; the verb-path test first (red before the fix).

**Execution note (advisory):** opus.

- [ ] **Step 1:** Test, fix, commit.

## Task 2: A live sample-rate change stops and restarts every slice's DSP channel

**Requirements:** the live-apply rule in CLAUDE.md ("RadioModel::setSampleRateLive (12-step
sequence ported from Thetis setup.cs:7003-7159 [v2.10.3.13])"); Phase 3F section 3 (one WDSP
channel per slice).

**Source first:** Thetis `setup.cs:7003-7159` (how the sequence treats each receiver channel)
and `cmaster.c:453-507` (`SetXcmInrate`).

**Files:** Modify `src/models/RadioModel.cpp` (`setSampleRateLive`). Test: the existing live
rate test, or a new `tests/tst_sample_rate_live_all_slices.cpp`.

**Acceptance:**
- With slices on channels 0, 1 and 2 active, every one of them is inactive before any rate is
  applied and active again afterwards, in the order Thetis uses for its channels (cited).
- A channel that was inactive before stays inactive.
- No channel's input rate changes while it is active.
- One-slice behaviour is unchanged; the existing live-rate tests pass.

**Verification:** a consequential state transition; the multi-slice test first (red: channels
1 and 2 stay active during the change).

**Execution note (advisory):** opus.

- [ ] **Step 1:** Read the Thetis sequence, test, fix, commit.

## Task 3: PureSignal feedback receivers on Protocol 1 radios

**Requirements:** Phase 3F design section 16.3.2 (the PS4 defect); the 3M-4 PureSignal work.

**Source first:** Thetis `console.cs` `UpdateDDCs` (the Protocol 1 Hermes-class branches) and
mi0bot-Thetis `console.cs` `UpdateDDCs` (the HL2).

**Files:** Modify `src/core/codec/P1CodecStandard.cpp`, `src/core/codec/P1CodecHl2.cpp`,
`src/core/codec/CodecContext.h`. Tests: `tst_p1_codec_standard`, `tst_p1_codec_hl2`,
`tst_codec_ps_ddc_config`; the P1 wire baseline if bytes change.

**Acceptance:**
- P1CodecStandard: the assignment under PureSignal with MOX matches Thetis (cited). Stream 0
  is no longer set before the PureSignal branch decides, as section 16.3.2 fixed in
  `P2CodecHermes`.
- HL2: `psFwd` and `psRev` under PureSignal with MOX match mi0bot-Thetis (cited). The code
  and the comments at `P1CodecHl2.cpp:912-932` and `CodecContext.h:558-561` agree.
- A wire byte change updates the P1 baseline in its own commit, with the reason.

**Verification:** transmit-coupled; tests first. Bench pending: PureSignal on a Hermes-class
Protocol 1 radio and on the HL2.

**Execution note (advisory):** opus.

- [ ] **Step 1:** Read both sources, test, fix, commit.

## Task 4: TCI's rate limit, a second client's transmit request, and stale comments

**Requirements:** R-R3-49 (every control a user can see does what its label says); the TCI
design (`docs/architecture/2026-05-09-phase3j-1-tci-port-design.md`).

**Source first:** Thetis's TCI server (`TCIServer.cs` or its equivalent under
`Project Files/Source/Console/`): any incoming-message rate limit, and what it does with `trx`
from a client while another client holds transmit audio.

**Files:** Modify `src/core/TciServer.cpp`, `src/core/TciProtocol.cpp` if needed,
`src/gui/setup/CatNetworkSetupPages.cpp` or the unbuilt-features list,
`src/core/TxSliceArbiter.h`, `src/models/RadioModel.h`. Tests: the `tst_tci_*` family,
`tst_unbuilt_features`.

**Acceptance:**
- **The rate limit.** If Thetis limits incoming messages, the setting does what Thetis does
  (cited), with a test. Otherwise the control is hidden in local and remote windows through
  the unbuilt-features list, with its reason.
- **A second client's `trx`.** Today the second client is refused transmit audio but still
  keys MOX (`TciServer.cpp:2694-2705` falls through to the protocol), and any client can
  unkey. Make it do what Thetis does (cited): if Thetis keys on any client's `trx`, keep that
  and say so in the log line; if Thetis refuses, refuse without keying and answer
  `trx:N,false` to that client. Test with two clients and a fake MOX.
- **Stale comments** (no behaviour change):
  - `TciServer.cpp:157` ("the Core runs none"): the Core runs a receive-only station server.
  - `TxSliceArbiter.h:46-47` ("waits for moxChanged confirmation"): it relies on a
    synchronous `setMox`.
  - `RadioModel.h:1046` ("positional"): it matches `sliceIndex()`.

**Verification:** transmit-coupled for the `trx` item; tests first.

**Execution note (advisory):** opus.

- [ ] **Step 1:** Read the Thetis TCI server, test, fix, commit.

## Task 5: Angelia and Orion sample rates and wideband ADCs

**Requirements:** CLAUDE.md's hardware-fact rules; the Phase 3F design section 2 table; the
operator's ruling (2026-09-24, "follow thetis"): these boards get Thetis's values for the
protocol they run.

**Source first:** Thetis `setup.cs` `InitAudioTab` (the per-protocol rate lists,
`setup.cs:847-853 [v2.10.3.15]`, with its `//DH1KLM` RedPitaya line);
`ChannelMaster/networkproto1.c:181-201` (no Protocol 1 wideband); `console.cs`
`wBToolStripMenuItem_Click` (`console.cs:43553-43559`, Protocol 2 wideband on ADC0) and wherever
Thetis shows or hides that menu item; mi0bot-Thetis for the HL2's 384 kHz on Protocol 1. The
pinned gateware is an OrionMKII-class build (board byte 5) and does not describe these boards;
cite it only for what it says. The first attempt's source reading is in `task-5-report.md` in
the controller's crew workspace.

**Files:** `src/core/BoardCapabilities.{h,cpp}` (per-protocol rates and wideband for a board
that runs both protocols); `src/core/SampleRateCatalog.cpp` if the rate filter changes;
`src/core/RadioDiscovery.cpp` (the 384 kHz given to every Protocol 1 reply, line 329) and what
reads it; the Radio Info tab (`RadioInfoTab.cpp`, the top rate shown); the Phase 3F design's
section 2 table and any plan that states the old values; `tests/tst_board_capabilities_phase3f.cpp`,
`tests/tst_wideband_chain_state.cpp`.

**Acceptance:**
- An ANAN-100D (Angelia) or ANAN-200D (Orion) running Protocol 1 offers 48, 96 and 192 kHz and
  no wideband. Running Protocol 2 it offers 48 to 1536 kHz and wideband as Thetis gives it
  (ADC0, unless Thetis gates the menu by model).
- Radio Info shows the top rate for the protocol the radio is running.
- The top rate taken from a Protocol 1 discovery reply follows Thetis (384 kHz for the
  RedPitaya) and mi0bot (384 kHz for the HL2), and 192 kHz for every other board.
- The invariant test becomes "no board offers wideband while running Protocol 1", over every
  row.
- The design table and the code change in the same commit, with tests of every value per
  protocol, cited.
- No other board's offered rates change: a test lists every board's offered rates per
  protocol and matches Thetis.

**Verification:** a capability table; tests of the values. Hardware pending (no Angelia or
Orion on the bench).

**Execution note (advisory):** opus.

- [ ] **Step 1:** Read the sources, test, fix, commit.

## Task 6: The Phase 3F design document says what shipped

**Requirements:** the documentation rule (a plan or design states what is true).

**Files:** `docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md`.

**Acceptance:**
- The header status says sub-epics A-G shipped and H (bench) is pending.
- The pan section describes the 9 layouts of up to 5 pans that shipped, not 5 templates.
- Section 16.2.6's `FilterChainRouter` is marked not built; the chain logic lives in
  `RadioModel::chainForStream`.
- Section 16.3.2 records Task 3's outcome.

**Verification:** a read-through.

**Execution note (advisory):** sonnet (documentation only); after Task 3.

- [ ] **Step 1:** Edit, commit.

## Task 7: Transmit keying sources follow Thetis

**Requirements:** the 3M-1 transmit work (`docs/architecture/phase3m-1a-*`,
`phase3m-1b-mic-ssb-voice-plan.md`).

**Flag:** this task changes how transmit keys and unkeys. The operator chose (2026-09-24) an
independent review of this task on its own before it merges. It runs last.

**Source first:** Thetis `console.cs`:
- `chkMOX_CheckedChanged` (the MOX button's PTT mode);
- `PollPTT` (the mic PTT keys in MIC mode, and a release unkeys only in MIC mode);
- where Thetis resets the PTT mode on unkey;
- VOX's release rule.

**Files:** `src/core/MoxController.{h,cpp}`, the raw `setMox` callers named above, and the
`tst_mox_controller_*` family.

**Acceptance:**
- Each keying source sets its PTT mode as Thetis does: the MOX button and container buttons
  (manual), two-tone, and TCI (`PttMode::Tci`, through the `RadioModel::setMox` shim).
- The mode clears on unkey as Thetis does. The missing subscriber that `MoxController.h:562-565`
  describes exists, or the header changes to what the code does.
- Releases are guarded by source as Thetis guards them: a mic release during a manual key, and
  a VOX release during a manual key, each do what Thetis does, with a test per pair.
- The existing MoxController tests pass.

**Verification:** transmit keying, high risk; tests first. Bench pending on real transmit,
with the operator's go-ahead.

**Execution note (advisory):** opus.

- [ ] **Step 1:** Read the Thetis PTT code, test, fix, commit.

## Task 8: A stopped DSP channel is fed until its stop completes

**Requirements:** the live-apply rule in CLAUDE.md; Phase 3F section 3. Found by Task 2's review
(C1) and its fix wave.

**Source first:** WDSP `channel.c` `SetChannelState` (about 280-310) and `iobuffs.c` (about
540-565) in `third_party/wdsp/src`; Thetis's own I/Q flow while a channel stops.

**Files:** `src/core/RxChannel.{h,cpp}` (`applyActive`, `processIq`,
`deactivateWithoutDrain`), `src/models/RadioModel.cpp` (`setSampleRateLive`), tests.

**Acceptance:**
- Today `RxChannel` marks itself inactive before `SetChannelState(ch, 0, 1)`, and `processIq`
  stops feeding the channel. So the drain never completes; each stop waits out WDSP's
  timeout (about 100 ms). With five slices a live rate change blocks the GUI thread about
  half a second.
- After this task, a stopping channel keeps being fed (as Thetis's I/Q keeps flowing) until
  WDSP reports the slew complete. Then it counts as inactive.
- The no-drain stop becomes safe to use, as Thetis uses it for the other channels.
- A live rate change with five slices finishes within one DSP block per channel, not
  100 ms each. The test measures and asserts the bound.
- Task 2's audio test (a channel already at the new rate stays audible) still passes.

**Verification:** a consequential state transition; tests first. Bench pending: a live rate
change with five slices on the G2.

**Execution note (advisory):** opus.

- [ ] **Step 1:** Read the WDSP source, test, fix, commit.

## Task 9: Follow-ups from the checkpoint

**Requirements:** R-R3-21 (wording), R-R3-26 (reachable listeners), R-R3-50 (licences), the
fast-test-loop rules.

**Items:**
1. **A CI build with tests off.** Add a CI step that configures and builds the app and
   `nereusd` with `NEREUS_BUILD_TESTS=OFF` (one platform is enough; Linux, reusing the
   job's ccache). A function defined only in a test-only block then fails CI, not a release
   build. The 2026-09-24 checkpoint found two: `AudioEngine::configureSpeakersConverter` and
   `HardwarePage::showAntennaTab`.
2. **`station_bind`** (the accessory listeners) reads its address the plain way, so
   `station_bind = ::` is IPv6-only. Use the same `listenAddressFor` as the remote listener,
   with a test binding `::` and connecting over 127.0.0.1 and ::1.
3. **`tst_media_transport`** does a real encrypted loopback handshake with a fixed 10 s wait,
   which misses at load 20-30. Give it the `REALTIME` option.
4. **The parked Minors from the review of Tasks 1-2:**
   - a window with no Core says "The Core supports a maximum of 5 slices" before its pool is
     sized: choose the subject by role;
   - stale comments at `MainWindow.cpp` (about 5544-5546, "1 slices"), `RadioModel.h`
     (about 3313-3314, `maxSlices()`) and `RadioModel.cpp` (about 18062's heading);
   - `tst_status_toast_preserves_bottom_bar.cpp:122`'s "1 slices" sample text.
5. **The licence check's rule 5** compares the crate notices only with
   `third_party/deepfilter/COMMIT`. Also require the pins in `setup-deepfilter.sh`
   (`DFNR_COMMIT`) and `setup-deepfilter.ps1` to agree.
6. **crunchy 0.2.2 and realfft 3.3.0** declare MIT but ship no licence file. Fetch each one's
   upstream licence text at the matching version, byte for byte, and add it to
   `deepfilternet-crates.txt`, marked as from upstream. Downloading these crates' own texts
   falls within the operator's DeepFilterNet approval of 2026-09-24.

**Verification:** each item's own test or check; the CI step read, and run once by hand in the
Linux container if possible.

**Execution note (advisory):** opus; items can be separate commits.

- [ ] **Step 1:** Each item, test, commit.

## Task 10: The TCI update gap as Thetis has it, and notices that do not depend on the build machine

**Requirements:** R-R3-49 (every control does what its label says); R-R3-50 (licences).

**Items:**
1. **The TCI update gap.** Thetis's `udTCIRateLimit` is not an incoming-message limit. It is
   the shortest gap between outgoing `vfo`, `dds` and `tx_frequency` updates to TCI apps: 0 to
   1000 ms, default 100 (TCIServer.cs:6420-6480 [v2.10.3.15]; found by Task 4).
   - NereusSDR's hidden `TciRateLimitMsgsPerSec` control was modelled as a messages-per-second
     limit and never wired.
   - Port Thetis's gap (source first, with cites and author tags), and show the control in
     Thetis's unit and default.
   - Reconcile `TciVfoCoalescer.h`'s note (which subsumed Thetis's throttle layers into the
     event loop) with the port. Its Layer 3 dedup stays.
   - A saved value in the old unit is dropped once, through the settings schema step, with a
     CHANGELOG line.
   - Test: updates to one app come no closer together than the gap; 0 sends every change.
2. **Notices that do not depend on the build machine.** The notices presets for the fetched
   libraries (portaudio, libdatachannel, libjuice, usrsctp, libsrtp) read the files one build
   tree compiled. So the committed files report out of date against a macOS arm64 tree, and
   would against a Linux or Windows tree. Collect from each library's full list of sources
   that any supported platform compiles (from its CMake lists, all platforms), so every
   machine regenerates the same file. Regenerate the committed files, and add a pytest case
   that the same inputs give the same output whichever platform's tree is given.

**Verification:** the TCI family and `tst_unbuilt_features`; the compliance pytest and the
licence check.

**Execution note (advisory):** opus.

- [ ] **Step 1:** Each item, test, commit.

## Task 11: Every Protocol 1 radio gets Thetis's receiver layout for its own model

**Requirements:** Phase 3F design section 16.3.2 (the PS4 defect); the 3M-4 PureSignal work; the
source-first rule. Found by the post-checkpoint review (its C1): `P1CodecStandard::applyDdcAssignment`
ports only Thetis's Hermes-class branch, yet the codec selector hands it every Protocol 1 model
except the HL2, the Anvelina Pro 3 and the RedPitaya (`P1RadioConnection.cpp:2310-2316`). The
same codec's `psDdcConfig` already splits by model (Hermes class, HermesII class, G2 class), so
the two halves of one codec disagree for the ANAN-10E, the ANAN-100B and the Orion/G2-class
radios on Protocol 1.

**Consumes:** the fix wave after the post-checkpoint review, which gives slice B a receiver
under PureSignal transmit on the four Hermes-class models only (HERMES, ANAN10, ANAN100,
ANAN_G2E) and leaves it unassigned on every other model. This task replaces that stopgap
with the full per-model port.

**Source first:** Thetis `console.cs` `UpdateDDCs` (every Protocol 1 branch: Hermes class,
ANAN10E/ANAN100B, and the Orion case shared by ANAN100D, ANAN200D, ORIONMKII, ANAN7000D,
ANAN8000D, ANAN_G2, ANAN_G2_1K, ANVELINAPRO3 and REDPITAYA) and `GetDDC` (their Protocol 1
cases); `ChannelMaster/networkproto1.c` `MetisReadThreadMainLoop` (the slot pairing for nddc
2, 4 and 5). Where mi0bot-Thetis differs for a non-HL2 model, cite both and say which one the
port follows and why.

**Files:** Modify `src/core/codec/P1CodecStandard.cpp` and `.h`; `P1CodecAnvelinaPro3.cpp` and
`P1CodecRedPitaya.cpp` if they must reach the Orion-class assignment; `CodecContext.h` if the
model has to travel there. Tests: a table-driven assignment test (new, or inside
`tst_p1_codec_standard`); the P1 wire baseline if bytes change. The Phase 3F design doc's
section 16.3.2 gains the per-model table.

**Acceptance:**
- An audit table first, in the report: for each model the codec serves and each combination of
  PureSignal armed, diversity, MOX and RX2 enabled, Thetis's values beside NereusSDR's: the
  P1 DDC config, enabled and synced DDCs, rates, ADC controls, the DDC under each user
  stream, the PureSignal pair and nDdc. Mismatches marked.
- `applyDdcAssignment` then gives Thetis's values for every row, cited with author tags.
  `psDdcConfig` and `applyDdcAssignment` agree on the PureSignal pair for every model.
- The table test covers every model and combination, with its expected values taken from
  Thetis (cited), and fails on today's code for every mismatch the audit found.
- The read loop's slot pairing agrees with the stream mapping for nddc 2, 4 and 5, so no user
  stream ever carries the PureSignal pair.
- A wire byte change updates the P1 baseline in its own commit, with the reason.
- HermesII (ANAN10E, ANAN100B) under PureSignal with MOX: Thetis gives `psrx = 0; pstx = 1`
  and no `rx1` or `rx2` (`console.cs:8766-8779`), so neither user stream gets a DDC. Today
  stream 0 still maps to DDC0 there, so slice A demodulates the PureSignal feedback (found by
  the fix wave after the post-checkpoint review; the same defect class as its C1). A test per
  model pins it.
- If a mismatch cannot be settled from the sources (a model whose Protocol 1 firmware Thetis
  treats differently from its enum), stop and report NEEDS_CONTEXT with both readings.

**Verification:** transmit-coupled; tests first. Bench pending for any Protocol 1 radio of
these families; the operator's HL2 has its own codec and is not affected.

**Execution note (advisory):** opus. Before Task 7, which runs last.

- [ ] **Step 1:** The audit table, the table test, the port, commit.

## Task 12: TCI apps get `if` with each VFO and centre change, as Thetis sends it

**Requirements:** R-R3-49 (every control does what its label says); the TCI design
(`docs/architecture/2026-05-09-phase3j-1-tci-port-design.md`). Found by the fix wave after the
post-checkpoint review: the live path (`TciProtocol::enqueueLocalBroadcastVfo`) sends `vfo` and
`dds` only, and `buildIfLine` is used only by the init burst, so an app's `if` goes stale after
the first tune.

**Source first:** Thetis `TCIServer.cs` `VFOChange` and `CentreChange` and the lines they send
(`TCIServer.cs:1365-1400 [v2.10.3.15]`), and whatever computes the IF offset they carry.

**Files:** `src/core/TciProtocol.{h,cpp}`; `src/core/TciServer.cpp` if the broadcast lives there;
the TCI broadcast tests and `tests/tst_tci_update_gap.cpp`.

**Acceptance:**
- A VFO change sends its `vfo` and then its `if`; a centre change sends its `dds` and then its
  `if`. Each names the receiver and channel Thetis names and carries the offset Thetis computes
  (cited, with author tags).
- Both go through the update gap on the gates the fix wave set: a centre change's `if` on the
  centre gate, a VFO change's on the VFO gate.
- The init burst and the live path build `if` with one builder.
- Tests: an app sees the new `if` after a tune inside the pan and after a pan move; the gap
  tests still pass.

**Verification:** the TCI family. Bench pending: a TCI app that reads `if`, following a tune.

**Execution note (advisory):** opus. Before Task 7, which runs last.

- [ ] **Step 1:** Read the Thetis sends, test, fix, commit.

## Task 13: The radio's TX inhibit input reaches the keying gate, as Thetis reads it

**Requirements:** the 3M-1 transmit work; the source-first rule. Found by Task 7's re-review:
Task 7's fix wave wired TX inhibit and the PA trip into every keying source, but nothing in
production asserts them. `TxInhibitMonitor::setUserIoReader`, `notifyRxOnly`,
`notifyOutOfBand` and `notifyBlockTxAntenna` have only test callers. The monitor has no poll
of its own, and `handleGanymedeTrip` has no production caller.

**Source first:** Thetis `console.cs` `PollTXInhibit` (`console.cs:25849-25887 [v2.10.3.15]`,
polled every 100 ms from power-on at `27417-27425`):
- the model gate `_useTxInhibit && Model != HPSDR`;
- Protocol 1: `!getUserI02()` for ANAN_G2E, 7000D, 8000D and RedPitaya (`//DH1KLM`,
  `//N1GP G2E added`), otherwise `!getUserI01()`;
- Protocol 2: `!getUserI05_p2()` for 7000D, 8000D, G2, G2_1K, ANVELINAPRO3 and RedPitaya,
  otherwise `!getUserI04_p2()`;
- `_reverseTxInhibit`, then `TXInhibitChangedHandlers`, then `OnTXInhibitChanged`
  (`45353-45356`), then the TXInhibit setter (`15342-15366`).

On the wire:
- Protocol 1: `networkproto1.c:336`, `user_dig_in = (C1 >> 1) & 0xf` when `C0 & 0xf8 == 0x00`;
- Protocol 2: `network.c:756`, `user_dig_in = ReadBufp[55]` in the high-priority status
  packet (the Thetis comment says byte 59; the code reads 55);
- the accessors: `netInterface.c:245-289`.

For the HL2, read mi0bot-Thetis for its own inputs. For the Ganymede PA trip, read Thetis's CAT
handling of the Ganymede message.

**Files:**
- `src/core/P1RadioConnection.cpp` and `src/core/P2RadioConnection.cpp` (parse the user
  digital inputs);
- `src/core/TxInhibitMonitor.{h,cpp}` (a per-status-frame or 100 ms reader);
- `src/models/RadioModel.cpp` (wiring, the setting's `useTxInhibit` and `reverseTxInhibit`);
- the Setup control, if Thetis shows one (source first);
- tests.

**Acceptance:**
- Each model reads the input bit Thetis reads, on each protocol, with the reverse option.
- A change reaches `TxInhibitMonitor` within one status frame or 100 ms, and every keying
  source is blocked while it holds (Task 7's gate). The P1 and P2 status parsers carry a test
  with the exact byte positions.
- The Ganymede trip reaches `handleGanymedeTrip` from its real source, or a NEEDS_CONTEXT if
  NereusSDR has none.
- A CAT or TCI request made while blocked is dropped, not held (Task 7's re-review, N3).

**Verification:** the transmit boundary, so tests come first. Bench pending with the
operator's go-ahead: a sequencer or a jumper on the radio's input.

**Execution note (advisory):** opus. After Task 7's follow-up.

- [ ] **Step 1:** Read the sources, then write the parsers, the reader and the tests.

## Task 14: Band outputs and filters follow the right slice, as Thetis and mi0bot choose them

**Requirements:** the 3M-1 transmit work, Phase 3F section 16.3.2, R-R3-49 (every control does
what its label says). Found by the whole-branch re-review's audit
(`.crew/2026-09-24-receiver-and-transmit-gaps-plan/rereview-whole-branch-report.md`, Part 2).
All five gaps predate this branch.

1. **Transmit band outputs on Protocol 1 ignore the transmitting slice.**
   - While keyed, `buildCodecContext` builds the OC byte from the RX1 stand-in's band
     (`P1RadioConnection.cpp:2582-2587`).
   - In a cross-band split this sends a carrier through the wrong band's filter.
   - On the HL2 the N2ADR board's transmit low-pass is chosen by these pins
     (`N2adrPreset.cpp:92-121`). A 7 MHz carrier selects the 30/20 m low-pass, so the second
     harmonic leaves unfiltered. A 14 MHz carrier into the 60/40 m low-pass reflects power into
     the PA.
2. **Protocol 2 sends no band outputs at all.**
   - Thetis writes them to the high-priority packet's byte 1401 (`network.c:1031`).
   - No NereusSDR Protocol 2 codec writes that byte, although Setup shows the OC Outputs tab
     for the G2.
3. **The Protocol 2 receive low-pass follows the last retuned receiver** instead of Thetis's
   rule (`P2RadioConnection.cpp:980-982`).
4. **"HPF Bypass on TX" is saved but never read** on either protocol
   (`AntennaAlexAlex1Tab.cpp:381`).
5. **The band comes from the receiver's DDC centre, not its VFO frequency.** Near a band edge
   with CTUN, the two can name different bands.

**Source first** (Thetis v2.10.3.15 at `3759d096`; mi0bot-Thetis at `c26a8a4` for the HL2):
- `Penny.cs:170-178`: receive uses `RXABitMasks[band of VFOA]`; transmit uses
  `TXABitMasks[band of VFOB]` when `VFOBTX` is set, otherwise VFOA's band.
- The callers pass `lo_band` and `lo_bandb` (`console.cs:14935-14937`, `29101-29106`
  `HdwMOXChanged`, `45950-45955`). `VFOBTX` is `chkVFOBTX` (`console.cs:39833`).
- mi0bot `Penny.cs:176-190` is the HL2 branch. `console.cs:14986-14988` is the HL2's OC band.
- `network.c:1031`: `packetbuf[1401] = (oc_output << 1) & 0xfe`.
- `console.cs:15487-15498` is the receive low-pass. It is named `UpdateAlexTXFilter` but
  runs unkeyed: the higher of RX1 and RX2 on a board with no separate RX2 front end, otherwise
  RX1.
- `console.cs:15467-15469` and `6843-6848`: `disable_hpf_on_tx` gives the HPF word 0x20
  while keyed.
- If a source cannot be found, stop with NEEDS_CONTEXT. Never guess a byte or a bit.

**Files:**
- `src/core/P1RadioConnection.{h,cpp}` (the keyed OC band from the transmit frequency; the
  band from the VFO frequency);
- `src/core/P2RadioConnection.{h,cpp}` and the Protocol 2 codecs that build the
  high-priority packet (byte 1401; the receive low-pass rule; HPF bypass while keyed);
- `src/models/RadioModel.cpp` (whatever the connections need: the transmitting slice's
  frequency, the stand-in's VFO frequency, the bypass setting);
- the Alex tab's setting reader;
- tests, including the Protocol 2 regression freeze baseline, updated in its own commit with
  the reason.

**Acceptance:**
- **Protocol 1, keyed.** The OC byte is the transmit mask for the band of the transmitting
  slice's frequency (plus XIT, as the Alex transmit low-pass already uses), whichever slice
  that is.
  - HL2 test: A on 20 m, B transmitting on 40 m gives the 40 m transmit mask (0x04 per
    `N2adrPreset.cpp`). The reverse case is also tested.
  - A Hermes test does the same.
  - Unkeyed bytes are unchanged, the HL2's two-range bypass included, which is the operator's
    hardware ruling.
- **Protocol 2.** Byte 1401 carries `(oc_output << 1) & 0xfe` on every board with OC outputs:
  unkeyed, the receive mask for the RX1 stand-in's band; keyed, the transmit mask for the
  transmitting slice's band. A test covers the G2 byte in both states.
- **Protocol 2 receive low-pass.** RX1 (the stand-in) on a board with a separate RX2 front
  end; otherwise the higher of the stand-in and the second receiver, as Thetis does. It no
  longer depends on which receiver was retuned last. Tested on the G2.
- **HPF Bypass on TX.** When set, the high-pass word is 0x20 while keyed, on both protocols.
  It is applied live, and a remote window's change reaches the Core. Tested.
- **Band from the VFO.** Every band decision above comes from the slice's VFO frequency, as
  Thetis's `BandByFreq(VFOAFreq)` does. Tested with CTUN near a band edge.
- Nothing in a test keys a real radio. The Protocol 1 freeze baseline is unchanged unless a
  state it pins is one of these fixes; say which.

**Verification:** transmit filter selection, so tests come first, red on the base commit.
Freezes on both protocols. This task gets a scoped review of its own before the operator's HL2
bench, as Task 7 did.

Bench (pending, the operator's go-ahead, dummy load, low power):
- On the HL2, key TUNE on B after A is closed, then in a cross-band split: the I/O tab shows
  the transmitting slice's mask.
- On the G2, an OC output follows the band in receive and in transmit.

**HL2 bench script (Task 14 fix wave).** The Pi 4 Core with the HL2 runs this branch; the
operator drives it from a remote window. The N2ADR filter board is fitted and enabled. Dummy
load, TUNE at the lowest power, and the operator's go-ahead before any key-down.

Where to read the band outputs:
- **In the remote window (the pass/fail reading).** Setup > Hardware > HL2 I/O: the OC strip
  (`band=`, the byte, `RX`/`TX` and seven pin lights). Setup > Hardware > OC Outputs: the
  live pin row, seven pin lights and nothing else. Both show the byte the Core's connection
  sent to the radio (`RadioModel::bandOutputsByte`), not one worked out in the window. Before
  the Core has sent one the strip shows `--` and no pins, and the row is dark.
- **Read unkeyed 0x00 on the I/O strip only.** The OC row has no byte, no `--` and no
  `RX`/`TX`, so a byte of 0x00 and a byte not yet known both leave it dark. Steps 1, 7 and 9
  (unkeyed, 0x00) are therefore read on the I/O strip; the OC row is a second witness only for
  a keyed byte with a pin lit (steps 3, 8, 9 keyed, 10 and 11).
- **On the Pi (the second witness).** The Core logs `HL2 ocByte=0xNN band=B mox=M` whenever
  the byte changes (`P1RadioConnection.cpp`, beside the OC compose):
  `sudo -n journalctl -u nereusd -f | grep --line-buffered "HL2 ocByte="`
  (the same line is in `~/.config/NereusSDR/nereussdr.log` under the service's home,
  `/var/lib/nereusd`). `band` is the band index: 3 = 40 m, 5 = 20 m; `mox=1` is keyed.
- Transmit pins on the N2ADR board (`N2adrPreset.cpp`): pin 3 (0x04) is the 60/40 m
  low-pass, pin 4 (0x08) the 30/20 m low-pass. With two filter ranges open the board is
  bypassed while receiving (0x00).

Receive:
1. Connect. Slice A on 14.074 MHz (pan 1), slice B on 7.074 MHz (pan 2). A hears 20 m FT8, B
   hears 40 m FT8, each pan shows its own band. The I/O strip reads `0X00 RX` (two ranges
   open: receive bypass); the OC row is dark.
2. Add C on 14.080 MHz and D on 7.080 MHz, then E on 14.030 MHz. Each joins the pan of its
   band and hears its own frequency; a sixth slice is refused with the slice-limit message.
3. PureSignal off. Close B, C, D and E so only A (20 m) is open. Key TUNE on A for 5 s:
   receive mutes; the I/O strip reads `0X08 TX`, the OC row lights pin 4 only, and the Pi logs
   `ocByte=0x08 band=5 mox=1`. After unkey, A's receive pins are back within a second.
4. PureSignal on. Reopen B on 7.074 MHz. Key a two-tone on A for 10 s: pan 1 keeps drawing,
   B's pan shows PS HOLD, PureSignal calibrates; after unkey B hears 40 m again.
5. Close A, keep B and D (40 m). B and D keep hearing 40 m FT8 at the same strength.
6. Tune B to 14.074 MHz: B hears 20 m at full strength.
7. Add a slice on 3.573 MHz: it hears 80 m and B 20 m; the I/O strip reads `0X00 RX` again.

Transmit (the band outputs follow the transmitting slice):
8. Close the 80 m slice and D, so only B (20 m) is open, with A closed. Hand the transmitter
   to B (TX badge on B's flag). Key TUNE on B for 3 s: the I/O strip reads `0X08 TX`,
   band 20m, the OC row lights pin 4 only; the Pi logs `ocByte=0x08 band=5 mox=1`. Normal forward power into the
   dummy load, low SWR.
9. Cross-band split, B transmitting on the lower band. Open A on 14.074 MHz (pan 1), tune B to
   7.074 MHz, the transmitter still on B. Unkeyed the I/O strip reads `0X00 RX`. Key TUNE on
   B for 3 s: the strip reads `0X04 TX`, band 40m, and the OC row lights pin 3, not pin 4; the
   Pi logs `ocByte=0x04 band=3 mox=1`.
   Normal power and SWR. Before Task 14 the wire carried pin 4 here: the 40 m carrier's second
   harmonic left through the 30/20 m low-pass. Before this fix wave the OC Outputs row showed
   pan 1's 20 m pins whatever the wire carried, so the row is now a valid keyed reading.
10. The reverse. Tune A to 7.074 MHz and B to 14.074 MHz, the transmitter on B. Key TUNE on B
    for 3 s: the strip reads `0X08 TX`, band 20m, and the OC row lights pin 4, not pin 3; the
    Pi logs `ocByte=0x08 band=5 mox=1`.
    Normal power, low SWR. Before Task 14 the 20 m carrier went into the 60/40 m low-pass:
    high SWR, power reflected into the PA.
11. Hand the transmitter back to A (7.074 MHz). Key TUNE for 3 s: the strip reads `0X04 TX`,
    band 40m, the OC row lights pin 3 only; the Pi logs `ocByte=0x04 band=3 mox=1`.

Fail, and unkey at once, if a keyed step shows another slice's band pin in the window or in
the Pi's log, if the window and the log disagree, if the SWR rises, or if the forward power
folds back.

Receive notes (Task 14 review, M1): unkeyed, the band behind the receive pins is the RX1
stand-in's VFO, not its stream centre, as Thetis takes `BandByFreq(VFOAFreq)`. The receive
byte can therefore change from what an earlier build sent in two states:
- **CTUN near a band edge.** The VFO sits in one band while the stream's centre sits in the
  next (for example a VFO at 14.010 MHz with the centre below 14.000 MHz): the pins are the
  VFO's band.
- **A slice that joined another slice's stream.** A slice opened inside an existing stream
  shares it (`JoinedExisting`) without moving its centre, so its VFO can sit up to half the
  stream bandwidth from the centre, in another band. When it is the lowest-lettered slice on
  the RX1 slot, its VFO's band chooses the pins. To see it, first set the receive sample rate
  to 192 kHz or more (Setup > Hardware > Radio Info, "Sample rate (Hz)"; 192 or 384 kHz on the
  HL2). A stream is then at least 192 kHz wide, 96 kHz either side of its centre, so C at
  13.950 MHz, 70 kHz below B, falls inside B's stream (which is centred on B when B opens it);
  at 48 or 96 kHz it does not, C does not join B's stream, and the case cannot be seen. Then, with A closed: open B at 14.020 MHz,
  then C at 13.950 MHz (outside 20 m, inside B's stream, so both show on one stream), then
  close B. The stream stays centred in 20 m, and the I/O strip shows the pins for C's band
  (GEN), not 20 m's.

**Execution note (advisory):** opus. After the whole-branch fix wave. Shares
`P1RadioConnection.cpp` and `P2RadioConnection.cpp` with nothing in flight.

- [ ] **Step 1:** Read the sources. Write the tests for all five gaps and show them red.
- [ ] **Step 2:** Protocol 1's keyed band, then the band from the VFO.
- [ ] **Step 3:** Protocol 2's byte 1401 and the receive low-pass rule, with the freeze
  baseline in its own commit.
- [ ] **Step 4:** HPF Bypass on TX on both protocols. Update the HL2 bench script with the
  transmit steps.

## Task 15: The HL2 receive-only kit and the older radios' Protocol 2 rates

**Requirements:** CLAUDE.md's hardware-fact rules; the Phase 3F design section 2 table; the
operator's rulings: every board gets Thetis's values for the protocol it runs (2026-09-24,
"follow thetis", Task 5) and Atlas, Hermes, HermesII and HL2 rows on Protocol 2 follow Thetis
too (2026-09-25, "1 follow thetis"); mi0bot-Thetis is authoritative for the Hermes Lite 2.

**Source first:** Thetis `setup.cs` `InitAudioTab`, the per-protocol rate lists
(`setup.cs:847-853 [v2.10.3.15]`, with its `//DH1KLM` line); mi0bot-Thetis for the HL2's rates on
each protocol and for how it identifies and models the HL2 receive-only kit (its discovery and
model code, and wherever it sets `HPSDRModel` for that board). Task 5's reports
(`task-5-report-2.md` in the controller's crew workspace) hold the first reading, including the
four rows pinned below Thetis with a reason.

**Files:** `src/core/BoardCapabilities.{h,cpp}` (the Protocol 2 lists for the Atlas, Hermes,
HermesII and HL2 rows; the HermesLiteRxOnly row); wherever a connect resolves a board to a model
(`HardwareProfile.cpp`, `RadioDiscovery.cpp`, and what reads the result); the Radio Info tab if
its top rate reads the model; the Phase 3F design's section 2 table and any plan that states the
old values; `tests/tst_board_capabilities_phase3f.cpp`, `tests/tst_radio_discovery_parse.cpp`,
and the hardware-profile tests.

**Acceptance:**
- On Protocol 2, the Atlas, Hermes, HermesII and HL2 rows offer Thetis's Protocol 2 list (48 to
  1536 kHz), unless mi0bot gives the HL2 a different Protocol 2 list, in which case the HL2
  follows mi0bot and the report says so with the cite. Their Protocol 1 lists do not change.
- The table test's pinned exception for these four rows is gone: it asserts that every row
  offers Thetis's list for each protocol (mi0bot's for the HL2), with no row excepted.
- An HL2 receive-only kit resolves on connect to the model mi0bot gives it, not HERMES, and on
  Protocol 1 offers the HL2's rates including 384 kHz. Everything else mi0bot keys on that model
  for the kit (receiver count, controls it hides or disables) follows from the same model, or
  the report lists what differs.
- A remote window offers the same rates as a local window on the same radio (the Core's list).
- A rate saved per radio that the new list still offers is kept; nothing saved is silently
  changed.
- The design table and the code change in the same commit, with tests of every value per
  protocol, cited.

**Verification:** a capability table and a model resolution; tests of the values.
`tst_p1_regression_freeze` and `tst_p2_regression_freeze` pass unchanged (no wire byte changes
at an existing rate). Hardware pending: none of these boards is on the bench running Protocol 2,
and there is no receive-only kit on the bench.

**Execution note (advisory):** opus.

- [ ] **Step 1:** Read Thetis and mi0bot, test red, fix the rows and the model resolution,
  commit.

## Task 16: Receive only stops every key, as Thetis's RXOnly does

**Requirements:** 3M-1 transmit (the keying gate); remote parity both ways (the operator,
2026-09-25: "we want parity no matter how i am connected"); a control that cannot run is shown
disabled with its reason, never hidden (the operator, 2026-09-25). Found by Task 15: the
`isRxOnlySku` flag is read only by two Setup screens, `TxInhibitMonitor::notifyRxOnly` has no
caller, and `GeneralOptionsPage` hides `m_chkGeneralRXOnly` (`GeneralOptionsPage.cpp:216-229`).

**Source first:** Thetis `console.cs` `RXOnly` (`console.cs:15312-15334 [v2.10.3.15]`: MOX
disabled unless SPEC or DRM, TUN, 2TONE (`// MW0LGE_21a`) and VOX disabled, MOX dropped if
keyed, Setup kept in step) and `setup.cs` `chkGeneralRXOnly_CheckedChanged`
(`setup.cs:6479 [v2.10.3.15]`) with its recovery line (`setup.cs:740`); mi0bot-Thetis for the
HL2 (Task 15's report: mi0bot models no receive-only kit, only the operator's toggle). Every
author tag preserved.

**Files:** the keying gate (`MoxController` and the admission path Task 7 and Task 13 use for
TX inhibit, so every source is refused: MOX, TUNE, two-tone, VOX, the radio's PTT, CAT, TCI);
`TxInhibitMonitor::notifyRxOnly` (wire it, or remove it if the gate has one better place);
`GeneralOptionsPage.{h,cpp}` (the checkbox shown on every radio, disabled with a reason where it
cannot change); the TX applet, the VFO flag and the container buttons for MOX, TUNE, 2TONE and
VOX (disabled with the reason while receive only is on); the `RxOnly` setting's scope
(`SettingsScope.cpp:540`) and the remote path (Setup's checkbox from a remote window, the Core's
gate, the mirrored state); the N2ADR settings migration (Task 15's concern: it covers radios
saved as a standard HL2, not the kit).

**Acceptance:**
- With receive only on, every keying source is refused through the one gate, with a plain
  reason, on a local window and on a Core; a keyed MOX drops when it is turned on, as Thetis
  does. MOX, TUNE, 2TONE and VOX show disabled with the reason (MOX follows Thetis's SPEC and
  DRM exception).
- The HL2 receive-only kit (board byte 12, `isRxOnlySku`) always runs receive only: the
  checkbox shows checked and disabled with a plain reason that the radio has no transmitter.
  This is NereusSDR's own rule (mi0bot has no kit model); say so in a comment.
- The checkbox is never hidden on any radio.
- A remote window shows the Core's receive-only state, can change it off the air exactly as a
  local window can (or, if the design scopes it per window, says in the report how the Core's
  gate and each window's controls then follow; ask with NEEDS_CONTEXT before choosing a scope
  that lets one window's setting key a radio another window has in receive only).
- The N2ADR migration covers the kit, or the report shows why it needs none.
- Tests show each refusal red first; the Task 7 and Task 13 keying tests stay green.

**Verification:** the transmit safety boundary: unit tests through the real gate, a remote
window through the session, and the regression freezes (no wire byte changes while receive only
is off). Hardware pending (an HL2 with receive only on: nothing keys; no kit on the bench). Its
own review follows, as Task 7's did.

**Execution note (advisory):** opus.

- [ ] **Step 1:** Read Thetis and mi0bot, test red, port the gate and the controls, commit.
- [ ] **Step 2:** The remote path, the kit's forced receive only and the N2ADR migration.
