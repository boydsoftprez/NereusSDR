# Phase 3F Bench Results: ANAN-G2 (Saturn)

Sub-Epic H Task 2 deliverable. Records what the G2 bench sessions actually
found, so findings survive between sessions instead of living only in commit
subjects.

**SKU:** ANAN-G2 / Saturn (2 ADC, 7 DDC, user DDC2-6, maxSlices 5)
**Host:** macOS (Apple Silicon)
**Branch:** `feature/phase3f-sub-epic-a-foundation`

## Session log

| Session | Date | Build at | Outcome |
|---|---|---|---|
| 1 | 2026-06-03 | `d564a772` | Bugs 1-3 found + fixed same session |
| 2 | 2026-07-24 | `f2bfd383` | Bugs 4-7 found. Root cause: data plane never wired. |
| 3 | 2026-07-25 | (desk) | Sub-Epic I implemented. Bugs 4-7 closed. Bench re-run pending. |

---

## Session 1 (2026-06-03): fixed

| # | Symptom | Root cause | Fix |
|---|---|---|---|
| 1 | Slice B's VFO flag landed on Slice A's pan, stacked on the top pan | `sliceAdded` handler picked the target `SpectrumWidget` from a transitional dynamic property and fell back to the active pan; no migration path when a slice's pan changed later | `a6ae08e8`: `spectrumForSlice()` + `panKey` migration, ported from AetherSDR `MainWindow::spectrumForSlice` |
| 2 | A flag could not be closed once opened | `VfoWidget::m_closeBtn` existed but was never wired to `removeSlice` | `cf31c157`: wire close button, hide it on Slice A |
| 3 | No way to switch which slice the RX applet targets | No per-slice tab row | `f2bfd383`: per-slice RX applet tab row + active-slice switching |

---

## Session 2 (2026-07-24): findings, all closed in Session 3

All four findings below share **one root cause**: Phase 3F landed the
multi-pan/multi-slice UI layer and the supporting components, but the data
plane that carries samples from a second DDC through to a second pan and a
second audio path was never connected.

| # | Symptom | Root cause | Status |
|---|---|---|---|
| 4 | Second pan never animates | Exactly one `FFTEngine(0)` exists (`MainWindow.cpp:1735`), statically connected at construction to `activeSpectrumWidget()` (`MainWindow.cpp:1838`), which resolves to pan 0 permanently. That `connect` is the only call site of `updateSpectrumLinear` in the tree. Pan 1+ has a fully built `SpectrumWidget` that is never handed a frame. | Fixed (Session 3) |
| 5 | Slice C sits on 20 m and will not leave | `SliceModel::m_frequency{14225000.0}` (`SliceModel.h:916`) is the ctor default, 14.225 MHz USB. Nothing assigns a frequency to a new slice, and `wireSliceSignals()` binds only `m_activeSlice`'s `frequencyChanged` to hardware, at connect time. Slice B+ never reach `ReceiverManager::setReceiverFrequency`. | Fixed (Session 3) |
| 6 | Slice C produces no audio | `RadioModel::addSlice()` creates a `SliceModel` and wires UI signals only. No `createReceiver()`, no DDC mapping, no WDSP channel. One RX channel is ever created (`createRxChannel(0, ...)`), and `RxDspWorker.cpp:216` hardcodes `rxChannel(0)`. `setActiveSlice()` only flips a bool. | Fixed (Session 3) |
| 7 | Radio is never told to enable a second DDC | `RadioModel::invokeCodecDdcAssignment()` (`RadioModel.cpp:10889`) builds the 5-slice config, runs the per-board codec, and pushes wire bytes to `P2RadioConnection::applyDdcAssignment()`. It has **zero callers**. The whole Sub-Epic B codec layer is unreachable. | Fixed (Session 3) |

### Supporting dead-code findings

- `FFTRouter::onFftFrame` is never called from `src/` (only `tests/tst_fft_router_fanout.cpp`), and `fftFrameForPan` is never connected. The topology map is populated at `MainWindow.cpp:1619` and never read.
- That same line registers `slice->ddcIndex()`, which is always `-1`. `SliceModel::setDdcIndex()` has zero callers in the tree.

### Latent corruption risk (not yet operator-visible)

`RxDspWorker::processIqBatch(int receiverIndex, ...)` accepts a receiver
index and ignores it: one accumulation buffer pair, one hardcoded
`rxChannel(0)`. **If a second DDC starts streaming before this is fixed, its
samples are appended into Slice A's accumulator and corrupt Slice A's
audio.** Any work that enables a second DDC must fix this first.

---

## Session 3 (2026-07-25): Sub-Epic I closes Bugs 4-7

Desk implementation, not a bench run. All four Session 2 findings are addressed
in code with unit coverage; the bench re-run is what converts them to verified.

| # | Symptom | Fixed by | Commit |
|---|---|---|---|
| 4 | Second pan never animates | One `FFTEngine` per DDC stream, and frames dispatched to every pan subscribed to that stream via the (previously dead) `FFTRouter` topology | `f6a81666`, `7daadd6e` |
| 5 | Slice C sits on 20 m and will not leave | New slices seed frequency and mode from the active slice instead of the 14.225 MHz ctor default, and every slice's tuning now reaches hardware through `bindSliceToStream` rather than only `m_activeSlice` at connect time | `7d589d10` |
| 6 | Slice C produces no audio | Per-stream accumulation with per-slice fan-out through each slice's own WDSP channel, and `rxBlockReady(sliceIdx, ...)` instead of a hardcoded 0 | `f8241703` |
| 7 | Radio is never told to enable a second DDC | `invokeCodecDdcAssignment` finally has a caller, the codec is indexed by stream rather than slice, and each stream's DDC is routed back to its logical receiver | `f2820e60`, `740deb06` |

### Additional defects found and fixed while implementing

| Symptom | Why it mattered | Commit |
|---|---|---|
| Noise blanker mutated the shared I/Q chunk in place | Once slices share a stream, one slice's blanker corrupts what every later slice sees. Upstream keeps one `ANB` / `NOB` per `_rcvr`. | `2588be39` |
| `fexchange2` can return leaving output buffers untouched | Hoisting the per-slice output buffers would have leaked stale audio on flush and restart transitions. | `f8241703` |
| Hermes-class boards asserted DDC2 on the first VFO turn | `P2CodecOrionMkII` was selected by `default:` for Hermes / HermesII / HermesC10, whose primary DDC is 0. Would have killed RX on a G2E. | `133c30bd`, `60ecfccc` |
| Sole-occupant retune reset the stream to the connection default rate | Silently discarded any per-stream width the operator had set. | `b18cc391` |
| `SliceModel::receiverIndex` became a stale duplicate of `streamIndex` | Three sites read it as the logical receiver index; it was never updated on migration and never set for Slice B+. | `414c164f` |

---

## Matrix rows unblocked by Sub-Epic I

These were untestable in Session 2 because the feature had no implementation.
They are now implemented and awaiting a bench run:

- Row 1 (slice creation up to maxSlices)
- Row 2 (slice removal restores chain BPF)
- Rows 3-8 (per-slice sample rate 48 k through 1536 k)
- Row 9 (slice band change / per-band memory load) for Slice B+
- Rows 10-11 (AetherSDR overlay: same-band and cross-band slices)
- Rows 12-14 (antenna routing) for Slice B+
- Row 15 (TxSliceArbiter handoff): a second stream now exists to hand off to
- Rows 48-60 (new, added by Sub-Epic I)

Rows covering Slice A only (single-slice regression, Alex policy on chain 0,
click-in-island retune) were valid throughout and are unaffected.

### Bench walk to run first

Superseded by the Session 4 walkthrough below, which reflects what the code
actually does after the adversarial-review fix pass. The steps here assumed
ADC distribution, which is still specification-only.

---

## What shipped and works

Confirmed on the bench across both sessions:

- Multi-pan layout templates and pan persistence
- Per-slice VFO flags: creation, pan routing, close button, bidirectional
  state sync with their `SliceModel`
- Per-slice RX applet tab row and active-slice switching
- Single-slice (Slice A) operation, unchanged from v0.5.2

---

## Session 4 (2026-07-25): adversarial review and fix pass

A falsification pass against Thetis, deskhpsdr and the Anvelina Pro III FPGA
gateware produced 16 findings that survived independent refutation, six of them
critical. All six are fixed, plus several found while fixing them. Suite 556/556.

Still desk work. Nothing below has touched a radio.

### Fixed since Session 3

| Defect | Commit |
|---|---|
| TX low-pass selected from the last RX retune, not the transmit frequency | `46e5390d`, `f3e2f53f` |
| WDSP channel-id collision: RX pool claimed the TX channel's id | `f0a632f4` |
| Pooled RX channels opened but never activated (slices B+ silent) | `30a2efae` |
| Codec's DDC enable mask overwritten by a receiver count | `8b02c5a5` |
| `applyDdcAssignment` mutating connection state from the GUI thread | `40c14144` |
| Wrong filter ladder on Saturn-class boards (80/60/40/15 m) | `3a2d7038` |
| `TxSliceArbiter` never establishing an initial binding | `1f536cd1` |
| Filter-policy changes never reaching the wire on their own trigger | `b7bd01bf` |
| `setWidebandEnabled` written from the GUI thread | `0f7e3055` |
| TX mic-source attach unmarshalled on the cold-start retry | `824c2b78` |
| WIDE badge never lit | `00ab9522` |
| Pan status overlay showing hardcoded placeholders | `0896b4f3` |
| Status-overlay badges responding to clicks on pan-0 only | `a976fc46` |
| P1 stream rate change never reaching the wire (HL2) | `ddf42130` |
| RxDashboard bound by list position rather than slice id | `2f63f59e` |
| Active pan id left naming a destroyed pan after its pan closed | `d190c580` |

### What to expect on a G2, honestly

**Should work now:**

- Two pans animating independently, each fed by its own DDC stream
- Slices B+ audible (WDSP channel activated, mixer slot registered, per-slice fan-out)
- Per-slice tuning reaching hardware
- Correct band-pass filter on every band, including 80/60/40/15 m which were
  wrong before `3a2d7038` even on a single slice
- TX low-pass following the transmit frequency rather than the last RX retune
- WIDE badge lighting on any pan whose chain is bypassed, with a tooltip naming
  the conflicting ranges and what to do about it
- Pan status overlay showing that pan's real slice letter, frequency, mode and CH tag,
  with its badges clickable on every pan rather than only pan-0
- Filter Policy changes taking effect immediately instead of at the next VFO tick

**Expected to be imperfect, by design:**

- **Every slice lands on ADC0.** ADC distribution is specification-only (design
  doc §16); `setAdcForReceiver` is still called once. So on a G2, two slices in
  different filter ranges share one chain and that chain bypasses, where the
  hardware could have given each its own filter. The WIDE badge will correctly
  report this. This is the single biggest gap between current behaviour and §16.
- The TX pill is only hit-testable on a pan whose slice is already TX-bound, so
  it reads as a state indicator rather than a way to grab TX from another slice.
  Use the flag's TX button for the handoff.
- **The active pan cannot be changed.** `PanadapterApplet::activated` is declared
  ("emitted on any click within applet") but never emitted -- there is no
  `mousePressEvent` or `eventFilter` on the applet -- so nothing ever calls
  `PanadapterStack::setActivePan` after the first pan is created. Consequence
  for the bench: **"Add slice on active pan" (Ctrl+R) and "Float active pan..."
  always target pan-0**, whichever pan you are actually working in. Use the
  layout templates to add pans (that path passes explicit `pan-N` ids and is
  unaffected) and the per-slice flag controls for everything else.
  `closeRequested(panId)` is unemitted in the same way, so a pan cannot be
  closed from the pan itself. AetherSDR wires all three: `PanadapterApplet.cpp:629`
  emits `activated` from `eventFilter` on `MouseButtonPress`, `MainWindow.cpp:12964`
  connects it straight to `setActivePan`, and `MainWindow.cpp:12076` auto-activates
  the pan owning a slice when that slice becomes active. Left for maintainer
  review because it is UX behaviour, not a defect with one correct answer.
- A refused TX handoff is silent. `TxSliceArbiter::handoffBlocked` is emitted but
  has no production consumer, so if the arbiter declines a handoff the flag
  button appears to do nothing.
- Slice B+ come up with default NR / SNB / APF / squelch / pan, because those
  remain active-slice-only from Sub-Epic A. Slice B will sound different from A.
- Anti-VOX cancellation references Slice A's audio only. Upstream mixes all
  receivers (`cmaster.c:372 [v2.10.3.15]`); the aamix port is in flight.

### Walkthrough

1. **Single-slice regression first.** Connect, Slice A only. Audio, spectrum,
   S-meter, tuning. This is the path most at risk from the epic and the one
   worth the most attention. Also worth an ear on 40 m and 15 m specifically:
   the preselector was choosing the wrong filter there until `3a2d7038`.
2. **Same-range pair.** Add Slice B at 14.180 while A is at 14.225. Both inside
   11.0 to 22.0 MHz, so one filter serves both. Expect: both audible, both flags
   on one pan, one active DDC, **no WIDE badge**.
3. **Cross-range pair.** Retune B to 7.150. Expect: B claims a second DDC and a
   second pan animates on 40 m, both slices audible. Because both streams sit on
   ADC0 today, expect the chain to bypass and **WIDE to appear on both pans**.
   Under §16 with ADC distribution this case would keep both filters; it does
   not yet.
4. **Overlay check.** Each pan's status strip should show its own slice letter,
   frequency and mode, not `A / 0.000 / USB`.
5. **Filter policy.** Click the WIDE badge on pan-0, choose Force band, Apply.
   The preselector should change immediately, without needing a VFO nudge.
6. **Removal.** Close Slice A while B exists. B must keep working and the radio
   must not go silent (this was a real defect until `30a2efae`). Then close a
   whole pan: whatever survives must still take Ctrl+R without the app
   targeting the pan that just went away (`d190c580`).
7. **Do not transmit with two slices on different bands until step 1 has been
   confirmed clean.** The TX low-pass fix is unverified on hardware, and the
   failure mode it corrects was full power through a filter for the wrong band.

### If something is wrong

The most informative single observation is whether a symptom appears with one
slice or only with two. Everything in this epic that broke, broke on the second
slice; the single-slice path was byte-identical throughout.

### One extra step for the HL2 run

Protocol 1 carries a single rate for the whole radio, so a per-slice rate
request is a radio-wide change there. Until this pass, the client moved its
allocator window, WDSP channel rates, drain chunk sizes and FFT bin math to the
new rate while the radio kept sending the old one, and the VFO flag's
"Sample rate >" submenu is a direct route into that path. On the HL2, after the
G2 walkthrough:

- Set a rate from a flag's "Sample rate >" submenu with two slices up. Both
  flags must show the new rate, both pans must keep animating, and audio must
  stay clean. A rate that lands on the client but not the radio shows up as
  audio that survives but sounds wrong, and a spectrum whose bins sit at the
  wrong frequencies.
- Repeat with the Setup rate combo to confirm the two entry points agree; they
  now run the same 12-step `setSampleRateLive` sequence.

---

## Session 5 (2026-07-26): driven smoke run on a live ANAN-G2

First session where the multi-pan path was actually exercised against hardware
rather than reasoned about. Driven through the UI (automation), screenshots
checked at each step, three defects found and fixed in the loop.

### Found and fixed

| Defect | Symptom on the bench | Commit |
|---|---|---|
| Pan display window never moved to its stream | Second pan rendered a saturated (solid red) waterfall, and its scale showed a different band than its own flag | `6c5fc58c` |
| Pan display span not clamped to stream width | Same saturation at the window edges: a 192 kHz default span over a 48 kHz DDC leaves three quarters of the window outside the data | `6c5fc58c` |
| `setVfoFrequency` only ever pushed to the ACTIVE pan | Any other pan kept a 0 Hz marker, so its flag was positioned off the left edge and the pan drew its `<\| 0.0000` off-screen-VFO chevron. Indistinguishable from "the flag was never created", and the reason scrolling a dead pan appeared to conjure one | `6c5fc58c` |
| `PanadapterApplet::activated` never emitted | No way to change the active pan, so "Add slice on active pan" always targeted pan-0 | `f4329373` |

### Verified working on the G2

- Two pans, Slice A on 40 m and Slice B on 20 m: both flags placed, both
  waterfalls live, `ddcEnable=12 nDdc=2` on the wire.
- Band-changing a slice migrates it to its own DDC and its pan follows.
- Three-pan `12h` layout: A / B / C, correct letters, all three live, the
  auto-added slice landing on the newly created pan.
- Removing a slice leaves the remaining slices running and does not silence
  the radio.
- `CH 0: BYPASS (multi-band: 40m + 20m)` reported correctly while both streams
  share ADC0.

### Human smoke-test checklist (SUPERSEDED — see Session 6)

The Session 5 list below predates the Session 6 fix pass. Roughly half its
rows now have different expectations (per-pan strips exist, `+RX` works,
click-to-tune works on every pan, the WIDE badge is visible). Kept for the
record; run the Session 6 list instead.

### Human smoke-test checklist

Run in order. Each row is one observable; stop and report at the first failure
rather than continuing, because later rows assume the earlier ones.

| # | Action | Expect |
|---|---|---|
| 1 | Connect, Slice A only | Audio, spectrum, S-meter, tuning all normal. This is the path most at risk from the epic. |
| 2 | Tune A to 40 m, then 15 m | Correct preselector on both. These two bands were selecting the wrong filter until `3a2d7038`. |
| 3 | `+PAN` -> `2v` | Two pans. Pan-0 keeps Slice A and stays live. |
| 4 | Click pan-1, then `+PAN` -> `Slice B` | Flag B appears **on pan-1**, pan-1's scale matches A's band, waterfall live. B is seeded on A's frequency, so sharing one DDC here is correct. |
| 5 | With B selected, `Band` -> `HF` -> `20m` | Pan-1's scale follows to 20 m, waterfall stays live, a second DDC appears. No `<\| 0.0000` chevron at pan-1's left edge. |
| 6 | Check the bottom bar | `CH 0` reports `BYPASS (multi-band: 40m + 20m)`. Expected: every slice is still on ADC0 (§16 ADC distribution is specification-only). |
| 7 | Check both pans' status overlays | Each shows its OWN slice letter, frequency and mode. |
| 8 | `+PAN` -> `12h` | Three pans, letters A / B / C, all live, the new slice on the new pan. |
| 9 | Close Slice C (x on its flag) | C disappears from the RX applet; A and B keep working; radio does not go silent. |
| 10 | Close a whole pan | A surviving pan takes `Ctrl+R` without targeting the pan that just went away. |
| 11 | Audio check on each slice | Each slice is independently audible. |
| 12 | **Do not transmit** with two slices on different bands until row 1 is clean | TX low-pass fix is still unverified on hardware; the failure it corrects was full power through the wrong band's filter. |

### Known-imperfect, do not file as new

- Every slice lands on ADC0; cross-band pairs share a chain and it bypasses.
- The per-pan `+RX` button is a disabled `NYI` stub and its `addRxClicked`
  signal has no consumer. Use `+PAN` -> `Slice N` instead.
- A refused TX handoff is silent (`TxSliceArbiter::handoffBlocked` has no
  consumer).
- Slice B+ come up with default NR / SNB / APF / squelch.
- A pan whose slice was removed keeps rendering its stream rather than going
  dark.
- `Cannot create receiver: at maximum 4` at startup on a 5-stream pool: a fifth
  slice would have no receiver behind it.

---

---

## Session 6 (2026-07-26): driven fix pass, 18 defects

Driven through the UI against a live ANAN-G2, screenshot-verified at each
step. Nearly every defect was one root shape: **a subsystem wired once to
whatever `activeSpectrumWidget()` returned at startup, or to `m_activeSlice`.**
`connect()` binds the OBJECT, not the expression, so it captured pan-0 / Slice A
permanently and never re-resolved.

### Fixed and verified on hardware

| # | Defect | Commit |
|---|---|---|
| 1 | P1 stream rate never reached the wire (HL2) | `ddf42130` |
| 2 | RxDashboard bound by list position, not slice id | `2f63f59e` |
| 3 | Active pan id left naming a destroyed pan | `d190c580` |
| 4 | Three `Qt::UniqueConnection` lambda connects that never connected | `4fb68621` |
| 5 | Slice letters read A, A, A | `f4329373` |
| 6 | Panadapters could not be made active (no click-to-activate) | `f4329373` |
| 7 | Pan display window never followed its stream (red waterfall) | `6c5fc58c` |
| 8 | Pan VFO marker stuck at 0 Hz, flag positioned off-screen | `6c5fc58c` |
| 9 | Mouse input dead on every pan but pan-0 | `0ca75c34` |
| 10 | Interaction signals bound to pan-0's object only | `0ca75c34` |
| 11 | Only pan-0 had a control strip | `0ca75c34` |
| 12 | Flag removal orphaned its close / lock / rec / play buttons | `0ca75c34` |
| 13 | Status strip invisible (no `sizeHint`), so WIDE looked unwired | `6a2f2584` |
| 14 | Band plan drew on pan-0 only | `6a2f2584` |
| 15 | Status strip overlapped the dBm range arrows | `8face243` |
| 16 | Spot remove / hover, disconnect click, DDC centre, stale waterfall | `867945b7` |
| 17 | Floated pan froze (`QRhiWidget: No QRhi`) | `5da4beaa` |

### Landed but NOT verified on hardware

| Defect | Commit | Bench check |
|---|---|---|
| Per-slice S-meter (slices B+ had none) | `9c70e153` | two slices, both flag level bars move independently |
| Per-slice DSP controls (65 handlers were Slice A only, all writing `rxChannel(0)`) | `ea2ccbc0` | change AGC or NR on B: A unaffected, B actually changes |
| Alex per-chain grouping key pinned to ADC0 while the wire routed to ADC1 (D1), plus the missing chain-count gate (D4) | this commit | bench rows 15-16 |

The DSP one is 970 lines through the live control path and the suite does not
cover per-slice DSP behaviour. Green means it compiles and does not regress
what IS covered. Treat it as unproven until the bench row passes.

### Bench checklist

Run in order; stop at the first failure, since later rows assume the earlier
ones.

| # | Action | Expect |
|---|---|---|
| 1 | Connect, Slice A only | Audio, spectrum, S-meter, tuning all normal. Highest-risk path. |
| 2 | Tune A to 40 m, then 15 m | Correct preselector on both (wrong until `3a2d7038`). |
| 3 | `+PAN` -> `2v` | Two pans, each with its own left strip (`+RX / BAND / ANT / Display`). |
| 4 | Click pan-1's spectrum | Its slice tunes. Pointer does NOT need to be over the flag. |
| 5 | `+RX` on **pan-1's own strip** | New slice appears **on pan-1**, flag placed, live waterfall. |
| 6 | Band-change that slice to 20 m | Pan-1 follows to 20 m, stays live, a second DDC appears. |
| 7 | Both pans' status strips | Show `CH 0` plus `WIDE`, clear of the dBm range arrows. |
| 8 | Bottom bar | `CH 0: BYPASS (multi-band: 40m + 20m)`. **Correct** — one antenna reaches one ADC. |
| 9 | **S-meter, unverified** | Both flags' level bars move, independently. |
| 10 | **AGC/NR on Slice B, unverified** | B changes; **A's audio is unaffected**. The one to watch. |
| 11 | Audio on each slice | Each independently audible. |
| 12 | Close a slice, then close a pan | Survivors keep working; no leftover button columns; radio not silent. |
| 13 | `+PAN` -> `12h` | Three pans, letters A / B / C, all live. |
| 14 | **Do not transmit** on two bands until row 1 is clean | TX low-pass fix is unverified on hardware. |
| 15 | **ADC routing, unverified.** With rows 5-6 up (two slices on two bands), **click the second slice's flag to select it**, then set its antenna to **RX2 / EXT1**. Selecting first is not optional: the antenna is held per band and the resulting label is synced onto whichever slice is active. | That slice's DDC moves to **ADC1**, so the two slices no longer share a preselector. Its pan's `WIDE` pill clears and its `CH` pill reads `CH 1`. The bottom bar shows both chains filtered and neither saying `BYPASS`: `CH 0: 40m` and `CH 1: 20m`. The first pan is unchanged. Needs a real feed on the RX2 jack to HEAR anything, but the filter decision is observable without one. |
| 16 | Set that same slice's antenna back to **ANT1** | Both slices share chain 0 again. `WIDE` returns on both pans, both `CH` pills read `CH 0`, and the bottom bar returns to `CH 0: BYPASS (multi-band: 40m + 20m)`. `CH 1` goes back to its idle text. |
| 17 | **SAME band. KNOWN FAIL, added 2026-07-30.** Two slices on the **same** band at the same dial frequency. Select the second slice's flag, then set its antenna to **EXT1**. This row exists because rows 15-16 use two DIFFERENT bands, and that hides the defect: on different bands the per-band antenna store hands each slice its own slot, and the differing frequencies put them on separate streams. | **Expected:** that slice moves to chain 1, its pill reads `CH 1`, bottom bar reads `CH 0: 80m` and `CH 1: 80m`. **Actual today:** both pills stay `CH 0`, `CH 1` reads idle. The selection applies for roughly 2.6 s (`setAlexRxBpf adc0=-1 adc1=8`) and is then reverted by the sibling slice (`rxOnly=2` followed by `rxOnly=0`). Two root causes, both diagnosed, neither fixed: the antenna is stored per band rather than per slice, and `SliceStreamAllocator` places slices by frequency alone so it co-hosts a pair whose antennas have just made co-hosting impossible. Full writeup in `docs/architecture/2026-07-30-per-slice-antenna-adc-routing.md`. Do not file as new. |

### Known-imperfect, do not file as new

- **Rows 15-16 need the G2, not the G2E.** The G2E (HermesC10) has one ADC, so
  it routes through `P2CodecHermes` and is pinned to ADC0 by construction. Two
  slices on two bands will always bypass there, and no antenna pick can change
  it. Not a defect.
- **`CH 1` in the bottom bar only appears on a two-chain SKU.** It is gated on
  `BoardCapabilities::rxFilterChainCount >= 2`, which is the ANAN-G2 and the
  OrionMkII family. On an ANAN-100D or ANAN-200D there are two ADCs but one
  preselector in front of both, so there is no second chain to report and the
  indicator stays hidden. Not a defect either.
- **Float active pan**: the floated pan renders, but the pan left behind goes
  black. Pulling a widget out of the QSplitter costs its siblings their QRhi
  context. Diagnosed, not fixed; likely needs to stop reparenting the
  QRhiWidget at all.
- Every slice lands on ADC0, so cross-band pairs share a chain and it bypasses.
  Hardware on a single antenna; see the G2 vs G2E table above.
- Zoom replan is pan-0 only: zoom works everywhere, but only pan-0 gains FFT
  resolution.
- 38 one-shot `activeSpectrumWidget()->set...` pushes remain pan-0 only
  (spot markers, cal offset, TX filter range, clarity, noise-floor attack,
  step size, display FPS).
- `Cannot create receiver: at maximum 4` at startup on a 5-stream pool: a fifth
  slice would have no receiver behind it.
- A pan whose slice was removed keeps rendering its stream rather than going
  dark.
- A refused TX handoff is silent (`TxSliceArbiter::handoffBlocked` unconsumed).
- Slice B+ still come up with default NR / SNB / APF / squelch values.

---

## Session 7 (2026-07-30): chip closeout and five Codex review rounds

No new bench evidence. This session closed the three chips spawned off the
multi-pan work, then worked five rounds of automated review on PR #293. What
changed is listed here so the bench rows below have something to point at.

| Change | Commit | Bench row |
|---|---|---|
| NB1 / SNB tuning sliders wrote `rxChannel(0)` regardless of which slice was selected | chip, see PR | 18 |
| TCI `rx_volume` never broadcast per slice; APF / BIN were stub arrays in `RadioModel` rather than reaching the slice | chips, see PR | 24 |
| Synchronized DDC1 was set in the P2 enable mask on top of `SyncEnable`, on both the PS and the diversity path | `71151176` | 21, 22 |
| Hermes-class PS branch left `streamDdc[0] = 0`, so the PS feedback DDC and the user stream both claimed DDC0 | `904e675f` | n/a on G2 (Saturn path) |
| `psPaused` was never set in production; only tests drove it | see PR | 21 |
| Wideband request was last-writer-wins across slices sharing a chain, and was not recomputed when a slice was removed | `a3da778e` | 20 |
| Shrinking the pan layout orphaned slices on deleted pans; only one of the three layout entry points had the fix | `d14bbbd4`, `a3da778e` | 19 |
| Expanding after a shrink created new slices instead of reusing the co-hosted ones, hitting `maxSlices` and stranding a surplus | `39f994be` | 19 |
| ANAN-100D and ANAN-200D declared Protocol 1 while advertising `widebandAdcs = 2`, letting extended view bypass the preselector for a stream that never arrives | `39f994be` | n/a on G2 (P2 board) |

Suite at `39f994be`: 572/572, build exit 0, built through the `all_tests`
target. Green here means the change compiles and does not regress what is
covered. The rows below are the part the suite cannot reach.

### Bench checklist, session 7

Rows 1-17 above still apply and should be run first; these extend them. Row 17
is a known fail with a written diagnosis, so it is expected to fail and is not
a stopping point.

| # | Action | Expect |
|---|---|---|
| 18 | **Per-slice noise blanker.** Two slices on the same band, co-hosted. Select Slice B, move the NB threshold and slope sliders. | The sliders act on the chain B is actually on. Note that co-hosted slices genuinely share one blanker (`_rcvr.panb` is per receiver, master matrix row 51), so A's audio changing here is expected. What must NOT happen is B's slider moving a chain B is not on, which is what it did before. |
| 19 | **Layout shrink then expand.** `+PAN` -> `2x2`, add slices until four pans each hold one. Then `+PAN` -> `1`. Then back to `2x2`. | Shrinking: no slice is orphaned on a deleted pan, and the survivors keep audio. Expanding: the four slices you already had **spread back out**, one per pan. You should NOT see a cap-reject toast, and you should NOT end up with three empty pans and a surplus slice. Both directions were broken separately. |
| 20 | **Wideband across two slices on one chain.** Two slices sharing a chain. Zoom one out into wideband, then zoom the OTHER back in. Then remove the slice that is still asking for wideband. | Wideband stays on while ANY slice on that chain still wants it, and drops when the last one stops asking or is removed. Previously the second slice's zoom-in turned it off underneath the first, and removing a slice left it stuck on. |
| 21 | **PureSignal with more than one slice.** Slice A plus at least one more, then run PS calibration. | PS acquires as it does single-slice. The other slices' streams pause during PS and **resume on their own** afterwards, without a manual retune. Check the bottom bar reports the DDC count you expect and not one extra. |
| 22 | **Diversity with more than one slice.** Ctrl+Shift+D, enable diversity on Slice A, with Slice B live on another band. | Diversity behaves as it does single-slice, and Slice B keeps its own stream. The sync DDC must not show up as a user stream. |
| 23 | **Two-pan simultaneous listen.** Two slices on two bands, both unmuted, both with real signal. | Both audible at once, independently. This is the headline of the whole epic and it has never been confirmed by ear. |
| 24 | **TCI, if you have a client handy.** Connect a TCI client, change `rx_volume` on Slice B, and toggle APF and BIN on Slice B. | The client sees a per-slice volume broadcast, and APF / BIN reach that slice rather than being swallowed. Skip if no client is at hand; this is the lowest-consequence row. |
| 25 | **RX audio after TX or TUNE.** Key TUNE briefly, unkey. | RX audio returns on every slice. There is an open chip for a report that it does not; this row is to confirm whether that reproduces on current HEAD. |

### What the bench cannot settle

- Row 17's antenna defect is diagnosed and NOT fixed on this branch, by
  decision. It gets its own branch. See
  `docs/architecture/2026-07-30-per-slice-antenna-adc-routing.md`.
- The Protocol 1 wideband correction only affects ANAN-100D and ANAN-200D,
  neither of which is on this bench. It is covered by an invariant test over
  `BoardCapsTable::all()` instead.
- The Hermes-class PureSignal DDC0 fix lands on the G2E, not the G2. The G2
  runs the Saturn codec, which inherits OrionMkII.

---

## Next

Data-plane completion is tracked as Sub-Epic I. See
`docs/architecture/2026-07-24-phase3f-sub-epic-i-data-plane-plan.md`.
Sub-Epic H Tasks 3-4 (HL2, G2E, HermesII bench runs) and Task 9 (release PR)
stay blocked until Sub-Epic I lands, because the multi-slice rows are the
bulk of the matrix.
