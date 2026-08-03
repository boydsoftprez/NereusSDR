# TNF Design Spec: Adversarial Review Report

**Spec under review:** `/Users/j.j.boyd/NereusSDR/.claude/worktrees/tdd-test-performance-2b2c10/docs/architecture/2026-07-28-tunable-notch-filter-design.md`
**Reviewed against:** Thetis v2.10.3.15 (3759d096), AetherSDR @c6481cbf, vendored WDSP, and the current integration branch (main + PRs #306/#291/#293).

---

## 1. VERDICT

The spec needs **targeted amendments before any code is written**, not rework. Its architecture is sound: the model-canonical design with stable ids (D4), the global persistence choice (D3), the WDSP mechanism section, and the build order are all correct and survived attack. But the very first task in §12, the `RXANBPSetTuneFrequency` push, is specified with the wrong value and the wrong call site, and §11 codifies the wrong invariant as its gating test. That single defect would be frozen into the foundation before any notch UI exists to reveal it, and it fails silently in the simplest configuration (one slice, CTUN off) while misplacing every notch under CTUN or multi-slice, which is the normal post-3F case. Fix §4, §5.4, §6.2, §6.3, §10.1 and §10.2, add a bench matrix, and the rest of the document is implementable as written.

---

## 2. BLOCKERS (fix in the spec before writing code)

### B1. §4 pushes the wrong quantity into `RXANBPSetTuneFrequency`

**What is wrong.** WDSP computes `offset = b->tunefreq + b->shift` (`third_party/wdsp/src/nbp.c:192`, identical at `:225`), and NereusSDR already feeds `shift` as the slice's displacement from its stream centre (`src/core/SliceStreamAllocator.h:48` `double shiftOffsetHz{0.0};   ///< slice freq minus stream centre`; set at `SliceStreamAllocator.cpp:70` and `:137`; pushed at `src/models/RadioModel.cpp:3676`). §4 line 191-192 says the test asserts "the pushed value tracks slice frequency". If `tunefreq` is driven from slice frequency, the sum becomes `2*sliceFreq - streamCentre`.

**Decisive evidence from Thetis.** `console.cs:31921` gives the multi-RX subrx its own shift (`radio.GetDSPRX(0, 1).RXOsc = rx2_osc;`) while `console.cs:31940-31941` push the *identical* `(RX1DDSFreq + f_LO) * 1.0e6` tunefreq to both `id(0,0)` and `id(0,1)`. Two slices on one DDC, one shared tunefreq, per-slice shifts. That is exactly NereusSDR's post-3F stream topology. `RX1DDSFreq = CentreFrequency;` at `console.cs:31932` (the spec's own citation of :31931 is off by one).

**Why it matters.** `SliceStreamAllocator::placeSlice` prefers `Outcome::JoinedExisting` first (`SliceStreamAllocator.cpp:64-72`), which sets a non-zero shift by construction, and CTUN pins the DDC in-window (`RadioModel.cpp:3596-3597`, `ddcFrequencyLocked()`). Non-zero shift is the normal case, not an edge case.

**Amendment to §4.** Replace the implementation paragraph with:

> `tunefreq` is the hosting stream's centre (`SliceStreamAllocator::streamCentreHz(streamIndex)`, or `Placement::newStreamCentreHz` on a claimed or retuned stream), matching Thetis's `RX1DDSFreq`, which is `CentreFrequency` (`console.cs:31932 [v2.10.3.15]`). WDSP sums it with the shift (`nbp.c:192`), and our shift is already slice-minus-centre (`SliceStreamAllocator.h:48`), so the two terms sum to the RF frequency of the channel's baseband DC after the shift stage (`RXA.c:640` `xshift` runs before `RXA.c:645` `xnbp`). On this tree that equals the slice's demodulated RF (VFO + RIT + DIG), because NereusSDR applies no RX-side CW-pitch DDS offset; the TX/TUNE pitch handling at `RadioModel.cpp:10851-10866` does not touch RX.

Also correct the two stale in-repo cites in §4: `RXANBPSetFreqs` is at `src/core/RxChannel.cpp:436` (`:425` is `m_filterHighInt = ...`), and `RXANBPSetShiftFrequency` is at `:1483` (`:1397` is the `#else` inside `setMuted()`).

**Amendment to §11.** Rewrite `tst_notch_tune_frequency` to assert both `tunefreq == streamCentreHz(streamIndex)` and `tunefreq + shift == the slice's demodulated RF Hz`, for (a) a slice owning its stream, (b) a slice that joined an existing stream at non-zero offset, (c) after a RIT toggle, (d) after a 20 kHz to 0 return-to-centre retune.

---

### B2. §4's named call site cannot execute, and cannot supply the value

**What is wrong.** §4 says `setNotchTuneFrequency` is "called from the same code path that already issues `RXANBPSetShiftFrequency`". That path is `RxChannel::setShiftFrequency`, which opens with a value-equality early return and then gates the WDSP block on magnitude:

```cpp
// src/core/RxChannel.cpp:1468-1484
void RxChannel::setShiftFrequency(double offsetHz)
{
    if (offsetHz == m_shiftOffsetHz) { return; }   // :1470
    m_shiftOffsetHz = offsetHz;
#ifdef HAVE_WDSP
    if (std::abs(offsetHz) < 0.5) {
        SetRXAShiftRun(m_channelId, 0);            // :1479
    } else {
        SetRXAShiftFreq(m_channelId, offsetHz);
        RXANBPSetShiftFrequency(m_channelId, offsetHz);   // :1483
```

`m_shiftOffsetHz` defaults to `0.0` (`src/core/RxChannel.h:938`) and the dominant retune outcome sets `p.shiftOffsetHz = 0.0` (`SliceStreamAllocator.cpp:128`, sole-occupant `RetunedStream`, taken whenever CTUN is off). So in a non-CTUN single-slice session, the body never executes even once. Additionally, the function's only parameter is the offset; there is no absolute frequency in scope and `RxChannel` carries no absolute RF member (only `m_afGain`, `m_filterLow/High`, `m_shiftOffsetHz`).

**§6.3's fallback is dead too.** §6.3 also pushes `setNotchTuneFrequency(...)` from `activateSliceChannel`, which early-returns on an already-active channel (`RadioModel.cpp:3118-3125`) and is documented as "No-op for a slice whose channel is already live, i.e. every retune" (`RadioModel.cpp:3706`). Both spec-prescribed call sites are inert on retune.

*(One skeptic argued §4 already implies `RadioModel::bindSliceToStream` by naming the function rather than the file. The counter that settled it: §4 anchors the instruction with an explicit in-file line number, and the fallback site in §6.3 is guarded too. Even under the charitable reading, the spec's wording must change.)*

**Amendment to §4.** Name the site explicitly:

> Called unconditionally from `RadioModel::bindSliceToStream`, immediately alongside the existing `ch->setShiftFrequency(placement.shiftOffsetHz)` at `RadioModel.cpp:3676`, and re-pushed on stream retune and slice migration. Not from inside `RxChannel::setShiftFrequency`, whose value-equality early return (`RxChannel.cpp:1470`) swallows it and which has no absolute frequency to pass. `RXANBPSetTuneFrequency` is internally idempotent (`nbp.c:479` `if (tunefreq != a->tunefreq)`), so an unconditional push is free.

---

### B3. §6.3's activation hook never fires for Slice A, so the primary receiver gets neither the restored notches nor the run flag

*(Original severity blocker; both skeptics corrected to **major**. Recorded here because it is a hard prerequisite for §5.5's restore path, but it is one missing call site, not a design flaw.)*

**What is wrong.** §5.5 states cold-start restore happens "at channel-activation time (§6)", and §6.3 names `activateSliceChannel` as the hook. `connectToRadio`'s WDSP-init lambda creates channel 0, applies its state, and calls `rxCh->setActive(true)` unconditionally at `RadioModel.cpp:5365`, *before* `openRxChannelPool(...)` at `:5377`, with an in-code comment saying the ordering is deliberate (`:5370-5372`). `activateSliceChannel` then takes `if (!ch || ch->isActive()) { ... return; }` (`RadioModel.cpp:3118`).

**Why it is worse than a missing list.** Each RXA channel's notch DB is built with `master_run = 0`:

```c
// third_party/wdsp/src/RXA.c:85-93
rxa[channel].ndb.p = create_notchdb (
    0,      // master run for all nbp's
    1024);  // max number of notches
rxa[channel].nbp0.p = create_nbp (
    1,      // run, always runs
    0,      // run the notches
```

and both `calc_nbp_lightweight` (`nbp.c:190`) and `calc_nbp_impulse` (`nbp.c:222`) bypass the notch DB entirely when `fnfrun` is 0, falling through to a plain `fir_bandpass`. `RXANBPSetNotchesRun` is the only writer (`nbp.c:499`). So channel 0 is notch-*inert*, not merely empty, even for notches added live during that session. Reconnect reopens the hole: `teardownConnection` calls `m_wdspEngine->shutdown()` (`RadioModel.cpp:10175`), which destroys every RX channel (`WdspEngine.cpp:310-318`).

**Two sub-claims in the original finding are refuted, do not act on them.** (i) tunefreq is unaffected: §4 delivers it on the shift path, which runs outside `activateSliceChannel` (`RadioModel.cpp:3676`). (ii) The test-seam claim is wrong: `activateSliceChannel` does *not* early-return in unit tests. Seven test classes are `friend`s of `WdspEngine` (`WdspEngine.h:679-694`), set `engine->m_initialized = true`, and get real `RxChannel` objects; `tests/tst_stream_pool_binding.cpp:997-1017` already drives `openRxChannelPool` with slice 0 bound and asserts `QVERIFY(engine->rxChannel(a)->isActive())`. No contract-pinning signal and no pure-planner refactor are needed.

**Amendment to §6.3.** Either add `syncNotches(...)` + `setNotchesRun(globalEnabled())` to the Slice A block beside `rxCh->setAfGain(...)` at `RadioModel.cpp:5363`, or add a `syncNotchesToAllChannels()` called at the tail of `openRxChannelPool` (after `activateBoundSliceChannels()`, `RadioModel.cpp:3068`) so every open channel including id 0 is reconciled on connect and reconnect. Keep the `activateSliceChannel` hook for the later-added-slice case. Extend `tst_notch_channel_sync` to cover Slice A across a simulated reconnect using the existing friend seam.

---

## 3. MAJOR (real defects, fix in the spec)

### M1. The Thetis CW-pitch shift is neither ported nor documented as excluded

*(Original severity blocker; both skeptics corrected to **major**.)*

`AddNotch`'s third statement is `fFreqHZ += GetDSPcwPitchShiftToZero(sourceRX);` (`console.cs:40228`), returning `+CWPitch` in CWL and `-CWPitch` in CWU (`console.cs:18187-18194`). Thetis needs it because it folds the pitch into `RX1DDSFreq` (`console.cs:31901-31910`), the exact value it feeds to `RXANBPSetTuneFrequency` (`console.cs:31940`). NereusSDR does the opposite: the pitch lives in the CW filter passband (`src/models/SliceModel.cpp:1279-1284`) and no CW term appears on any slice/stream/DDC path (`grep -rn "CWPitch|cw_pitch|cwPitch" src/gui/` returns nothing). `grep -niE "\bcw\b|cwl|cwu|pitch|sidetone"` over the 646-line spec returns **zero matches**, while §2.2 hands the implementer `console.cs:40222-40280` (which contains :40228) and §10.3 makes the tag at :40230 ship-blocking under a plus/minus-5-line window that brackets it.

**Strongest supporting point, missing from the original finding:** §D6/§7 port `notchSidebandShift`, which in Thetis is deliberately paired with the CW term. `console.cs:40329-40331` sends `VFOA + middle` (middle = +cw_pitch in CWU) and `AddNotch`'s -cw_pitch cancels it back to VFOA. In NereusSDR, D6 alone is already correct; adding the CW term drags a CWU notch to baseband 0, outside the passband.

**Amendment.** Add an explicit non-port entry to §1.2 and a note in §5.4 and §8.3: `GetDSPcwPitchShiftToZero` (`console.cs:40228`) and `getCWSideToneShift` (`display.cs:8617-8622`, applied at `:8654`) are deliberately NOT ported, because they compensate for Thetis folding cw_pitch into `RX1DDSFreq`, whereas NereusSDR carries no CW offset in the DDC/slice frequency. In §8.3, drop `handleNotches`'s `cwSideToneShift` parameter rather than threading a constant zero. Add a CW case to `tst_notch_model_guards` asserting a notch added at F in CWU/CWL is stored at exactly F. Fix the §5.4 `Math.Round` cite to `console.cs:40230` (`:40232` is `//MW0LGE_21e XVTR`, which §10.3 correctly cites; the document contradicts itself).

---

### M2. `RXANBPSetShiftFrequency` is never pushed when the offset returns to zero

`setShiftFrequency`'s near-zero branch calls `SetRXAShiftRun(m_channelId, 0)`, which writes `rxa[channel].shift.p->run` (`third_party/wdsp/src/shift.c:113-116`) and never touches `NOTCHDB->shift`. `RXANBPSetShiftFrequency` is the *sole* writer of that field (`nbp.c:487-496`; `grep -n -- "->shift" nbp.c` returns only 192, 225, 491, 493), and `calc_nbp_lightweight` consumes it unconditionally with no reference to any run flag. Thetis has no such branch: `radio.cs:1419-1420` pushes both setters on every `RXOsc` change including to zero, and `grep -rn "SetRXAShiftRun"` over the entire Thetis Console tree returns nothing. The run gate is NereusSDR-original.

The zero transitions are routine and broader than the original finding listed: turning RIT off and leaving DIGU/DIGL both collapse the offset to 0.0 through the same lambda (`RadioModel.cpp:8973-8988`, wired to `ritEnabledChanged` / `ritHzChanged` / `diglOffsetHzChanged` / `diguOffsetHzChanged` / `dspModeChanged`), plus band jump (`MainWindow.cpp:1087`) and CTUN off (`:1641`, `:7304`).

This is latent today, provably: with nn=0 the offset cancels exactly (`nbp.c:110-111` then `:202-203`), and `master_run` defaults to 0. TNF is what un-hides it.

**Amendment.** Add a second bullet to §4: `SetRXAShiftFreq` and `RXANBPSetShiftFrequency` must both be called on both branches of `setShiftFrequency`, matching `radio.cs:1419-1420`, leaving `SetRXAShiftRun` as the only thing the magnitude gate controls. Extend `tst_notch_tune_frequency` with a pan-out-and-back sequence. While the file is open, fix the stale inline cite at `RxChannel.cpp:1481` (`radio.cs:1417-1418` should read `1419-1420`) and audit the sign: Thetis negates (`-value`), NereusSDR does not.

---

### M3. RIT clobbers the shift offset today, which breaks the tunefreq invariant

Not in the original finding list but surfaced during verification of B1, and it belongs in the same task. `RadioModel.cpp:8966-8988` installs an `updateShiftFrequency` lambda whose `offset` is RIT + DIG **only**; it discards `placement.shiftOffsetHz` entirely and ends with `rxCh->setShiftFrequency(offset);`. Its comment still reads "For 3G-10 (single RX, no CTUN), the shift = these two terms only", which is stale post-3F. So toggling RIT on a shifted slice clobbers the stream offset, and notches would jump.

**Amendment.** State the invariant in §4 as `tunefreq + shift == the slice's demodulated RF Hz (stream centre + slice offset + RIT + DIG)` and require the RIT/DIG lambda to compose with `placement.shiftOffsetHz` rather than replace it. Assert the sum after a RIT toggle in `tst_notch_tune_frequency`.

---

### M4. §10.1 requires only the `radio.cs` header, but §5.4 ports `console.cs` into the same file

`NotchModel` derives from two Thetis files. §5.3 takes the spatial helpers from `radio.cs`, but §5.4 ports console.cs logic directly: `NotchAdminBusy` (`console.cs:40224`), whole-Hz rounding (`:40230`), the XVTR min/max override (`:40232-40253`), the min/max constrain (`:40257`), the 10 Hz dedupe (`:40260`), the 200/100 Hz width defaults (`:40268-40269`), and the wheel-resize edge clamp (`:33317-33318`). §10.3 independently concedes console.cs author tags must survive into this work, which proves the spec knows console.cs code lands here.

The headers are materially different: `console.cs:7` credits Sizenko Alexander of Style-7, `:31-34` credits Chris Codella (W2PA), `:36` carries the `//N1GP G2E added` line. None appear anywhere in `radio.cs:1-42` (FlexRadio / Doug Wigley / Richard Samphire only).

**No gate catches it.** `verify-thetis-headers.py:70-77` checks five generic anchors. Its `check_samphire_marker` (`:281-286`) requires only the literal `MW0LGE`, which radio.cs's own dual-licensing block supplies; and `radio.cs` is not even in `SAMPHIRE_AUTHORED_SOURCES` (`:110-113`), so with radio.cs listed alone the check never fires. `verify-inline-tag-preservation.py:264-266` explicitly delegates header checking away. `check-new-ports.py`'s cure is a PROVENANCE row, not a header audit.

**Amendment to §10.1 item 2.** Stack both Thetis headers byte-for-byte in citation order with `// --- From radio.cs ---` and `// --- From console.cs ---` separators, per `CLAUDE.md:66-71` and `HOW-TO-PORT.md:31-32`, matching the live precedent at `src/core/CalibrationController.h:23` / `:70`. List both files in the THETIS-PROVENANCE row. Drop §5.5 from the justification (the spec explicitly declines to port Thetis's packed persistence string, so that cite is a divergence note, not a port). The "version + commit in the Ported from line" point is a nit: `HOW-TO-PORT.md:20-21`'s own template omits it and only 38 of 330 in-tree blocks carry a stamp.

---

### M5. §10.2 misstates provenance for `SpectrumOverlayPanel`, and the CI gate is reproducibly blocking

*(Original severity blocker; both skeptics corrected to **major**.)*

§10.2 asserts `SpectrumOverlayPanel.cpp` is "already registered in `THETIS-PROVENANCE.md`". It is not: both `.h` and `.cpp` sit in the column-2 exclusion table under the "Independently implemented" heading at `THETIS-PROVENANCE.md:393` (rows at `:403-404`), whose preamble at `:395-398` affirmatively publishes that they were "written without consulting Thetis source" and states the column-2 formatting is deliberate so the verifier skips them. `check-new-ports.py:233-256` parses column 1 only, by explicit anti-loophole design.

**Reproduced.** Copying `SpectrumOverlayPanel.cpp` and appending the exact cite D6/§7/§12-step-8 require (`// From Thetis console.cs:40313-40331 [v2.10.3.15]`) yields, in the real module: DIFF mode `[(1268, 'Source: comment citing Thetis', '// From Thetis console.cs')]`; baseline `[]`. Both `ci.yml:46` and `:58` run the script with no `continue-on-error`; `scripts/git-hooks/pre-commit:45-59` runs both modes. Because the pattern scan is whole-file (`check-new-ports.py:326-331`), every subsequent touch of the file fails too.

**Correction to the original finding:** the files ARE registered, just in the wrong index. They are column-1 rows at `aethersdr-reconciliation.md:155-156` and already carry a full AetherSDR GPLv3 header plus a Modification history block (`SpectrumOverlayPanel.h:1-38`). Full-tree mode passes; only diff mode fails.

**Recommended amendment (cheaper than migration).** Keep all Thetis-derived sideband-shift math inside `NotchModel` (which §10.1 already treats as a new attribution event) and make the `+TNF` button a pure signal emitter cited only to AetherSDR. Then §10.2 must state explicitly that `SpectrumOverlayPanel` is NOT Thetis-registered and must receive no Thetis cite. If Thetis logic really is wanted in the panel, delete the two exclusion-table rows, add real column-1 THETIS rows, and prepend the verbatim console.cs header behind a `// --- From console.cs ---` separator, in the same commit. Do not reach for `no-port-check:` (already used in 108 files under `src/`); that would leave a published false statement that a file containing ported Thetis logic was written without consulting Thetis.

**Drop the KeyboardSetupPages half.** `src/gui/setup/KeyboardSetupPages.cpp:32-70` is a 100% NYI stub (every control `setDisabled(true)`), no `ShortcutManager`/`registerAction` exists anywhere in `src/`, and every shipped shortcut is a plain `QAction::setShortcut` in the already-registered `MainWindow.cpp`. An AetherSDR cite there also fails full-tree only, not diff. What is worth raising instead: §1.1 item 7 promises an "assignable keyboard shortcut" against a disabled stub page, silently scoping in a subsystem the spec never budgets for.

Also add `MainWindow.{h,cpp}` and `DspSetupPages.{h,cpp}` to §10.2's list (both registered; documentation-only gap).

---

### M6. §10.3 omits `//NOTCH MW0LGE` (console.cs:33283), which the verifier demands for every drag-state cite

*(Original severity blocker; both skeptics corrected to **major**.)*

`console.cs:33283` is `//NOTCH MW0LGE`, a section marker one line above the drag-state block, closed by `//END NOTCH` at `:33322`. §10.3 lists only `//MW0LGE_21e` at `:33289`. Running the verifier's own extractor confirms the omitted line is the *only* required tag:

```
extract_tags_from_region(console.cs, [33284, 33297]) -> [(33283, 'MW0LGE')]
extract_tags_from_region(console.cs, [33286])        -> [(33283, 'MW0LGE')]
extract_tags_from_region(console.cs, [33287, 33288]) -> [(33283, 'MW0LGE')]
```

The listed tag cannot discharge it: `port_contains_tag` matches `\bMW0LGE\b` (`verify-inline-tag-preservation.py:303`), and `_` is a word character, so `MW0LGE_21e` yields no boundary. Reproduced end-to-end against the real script: a port file written exactly per §10.3 gives `FAIL src/SpectrumWidget.cpp:5 cites console.cs:33283 - missing //MW0LGE tag within +/-10 lines`; adding `// NOTCH MW0LGE  [original section marker from console.cs:33283]` flips it to `OK`.

**Note the local hook will not catch it.** `scripts/git-hooks/pre-commit:110-136` resolves Thetis via `../Thetis` / `../../Thetis` / `../../../Thetis` relative to cwd; from this worktree none exist, so it prints "tag-preservation SKIPPED" and the failure surfaces only in CI.

**Amendment.** Add the row `` `//NOTCH MW0LGE` | `console.cs:33283` (section marker above the drag-state block) `` and instruct placing the bare tag verbatim within plus/minus 10 lines of the drag-state cite, in addition to `//MW0LGE_21e`. Add a note that underscored variants (`MW0LGE_21e`, `MW0LGE_21k8`, `MW0LGE_21k9rc4`) never satisfy a bare-`MW0LGE` requirement, so both forms may be needed on the same cite; mark which §10.3 rows the gate actually enforces. Warn explicitly against "fixing" a failure by narrowing the cite range so the plus/minus-5 window misses :33283, which would pass CI while genuinely losing the attribution.

**Second, broader gap in the same section.** The §2.2 persistence cite `console.cs:3034-3035, 4763-4764` spans `min(line_nums)-5 .. max(line_nums)+5` (`verify-inline-tag-preservation.py:274-275`), roughly 1740 lines, generating 18 further `MW0LGE` requirements (console.cs:3358 through 4713) that §10.3 lists nowhere. Same trap on `display.cs:5046, 5095, 6505, 6530, 6741` and `TCIServer.cs:3384-3400, 1954-1960`. All are the same tag string, so one bare `MW0LGE` discharges them, but the spec should either say so or split the cites so the windows stay tight.

---

### M7. §7 / §2.2's interaction model misreads both upstreams

Four sub-defects, all verified:

**(a) The Thetis add gesture is Ctrl + RIGHT-click, not middle-click.** `console.cs:48974` opens `switch (e.Button)`; `:49614` is `case MouseButtons.Right:`; `:49629` is the `if (Common.CtrlKeyDown)` branch containing `AddNotch(dFreq, rx)` at `:49644`. The stale comment at `:49633` ("add notch from cross hair mode with middle mouse") is what the spec appears to have read. `grep -n "AddNotch("` returns only :40222 (def), :40331 (TNFAdd) and :49644. `grep -n "MouseButtons.Middle"` returns exactly one hit (`:49725`), whose body only toggles active (`:49735`) or removes on Shift (`:49731`). **D2's divergence is justified by a premise that is false**, and D2's second clause is self-defeating: on macOS Ctrl+click IS a secondary click, so Thetis's real gesture is *more* reachable there. NereusSDR's Ctrl binding is on the wheel (`SpectrumWidget.cpp:6631`), not the click, so it does not collide either.

**(b) §7's drag rows cite three bool declarations.** `console.cs:33286-33288` are `private bool m_bDraggingNotch/NotchBW/BDragginNotchBWRightSide`. The real rule is `console.cs:49037-49067`: `m_BDragginNotchBWRightSide = (dMouseVFO >= SelectedNotch.FCenter);`, then `if (nHpx - nLpx > 8)` with plus/minus-4-px edge zones, then `if (bNearEdge || Common.ShiftKeyDown) // can also hold shift drag to resize the notch`. The spec cites that range nowhere (grep for 49039/49056 returns nothing), yet §12 item 7 names "edge-drag" as a task and §11 names "edge-vs-centre discrimination" as a test.

**(c) AetherSDR's drag is one omnidirectional gesture.** `SpectrumWidget.cpp:8648-8650` starts the drag on any body pixel via `tnfAtPixel(mx)`; the triangle at `:13544-13549` (`// Center triangle (grab handle)`) is drawn and never hit-tested. `:9051-9070` sets centre from x AND width from y (`std::pow(2.0, -dy / 48.0)`, clamped 10..12000 Hz). The spec's "Drag triangle handle | Move notch centre | AetherSDR m_draggingTnfId" is wrong on both grab region and axes, and §1.2 silently drops the y-axis width gesture instead of listing it as out of scope (unlike depth and permanent).

**(d) Two incompatible hit tests are cited without saying which governs.** §2.3 cites AetherSDR's `tnfAtPixel(x, preferredId)` (nearest-centre, reverse scan, plus/minus-3-px pad, preferred-id short-circuit, `SpectrumWidget.cpp:13648-13681`); §5.3 ports Thetis's `notchSurrounding` (first-found in list order with a conditional pad, `radio.cs:4297-4322`). Neither §7 nor §11 says which one `tst_notch_hit_test` exercises.

**Amendments.** Correct §2.2 to "Add gesture (Ctrl + right-click) | console.cs:49614, 49629-49646" and re-justify D2 on grounds not yet written down (Ctrl + right-click is currently unbound in NereusSDR's SpectrumWidget). Rewrite the two drag rows against `console.cs:49037-49067 [v2.10.3.15]` including the 8 px gate, the plus/minus-4 px zone, the side-of-centre default, and the `bNearEdge || ShiftKeyDown` fallback (which forces resolving the Shift collision with §7 row 2; Alt discriminates it). Correct the AetherSDR attribution and either adopt or explicitly reject the y-axis width gesture in §1.2. State which hit test governs §7 and §11.

**Do not act on these overstatements.** Shift = 1 Hz fine wheel step is already covered: §7's wheel row cites `console.cs:33299-33321`, which contains the Shift branch at `:33305-33309`. Middle-click bypass and Shift+middle-click remove are capability-covered by §7's right-click context menu (Thetis itself exposes them via `frmNotchPopup`, `console.cs:45251-45253`). "Edge-drag is dead code for the default notch" is span-dependent, not universal: 200 Hz clears the 8 px gate below roughly 25 Hz/px. A mid-gesture hand-off cannot occur because the spec's signals carry an explicit latched `id`.

---

### M8. §8.3's undented-copy invariant names the wrong protected consumers

Three things, and the correct fix is *not* what the original finding proposed.

**The guard claim is false, and worse than "weak".** §8.3 says `tst_nf_aware_grid` and `tst_clarity_nf_grid_coexistence` guard "exactly that machinery". `tst_nf_aware_grid.cpp:47` calls `testApplyNoiseFloor(-100.0f)`, a one-line passthrough to `onNoiseFloorChanged` (`SpectrumWidget.h:786`); the Clarity test feeds `clarity.feedBins(...)`. Neither builds a spectrum frame, so `processNoiseFloor()` (called only from `updateSpectrumLinear`, `SpectrumWidget.cpp:2832`) never runs. Deeper: the NF-aware grid runs off a *different* pipeline. `MainWindow.cpp:3069-3072` feeds ClarityController from raw `FFTEngine::fftReady` bins, and `SpectrumWidget.cpp:2608-2612` states the split in-tree. A dent in `m_renderedPixels` is structurally incapable of moving the NF-aware grid, so §8.3's stated failure mode is wrong for this codebase.

**The real NereusSDR-only consumer is MaxBin.** `peakDbmInSlicePassband()` (`SpectrumWidget.cpp:2909`, scanning `m_renderedPixels` at `:2934-2936`) feeds `MainWindow.cpp:3113-3115` -> `WdspEngine::setMaxBinDbmFromSpectrum` (`WdspEngine.cpp:1430`) -> the analog S-Meter MaxBin mode shipped in v0.5.2. Thetis has no analogue: it reads MaxBin from WDSP upstream of display.cs (`console.cs:46959`, `dsp.cs:849-850`), so a Thetis visual notch structurally cannot move the meter. Ours would.

**Do NOT protect six consumers.** Thetis deliberately dents peak hold, peak blobs and the max readout on the spectrum plane: `display.cs:5256` computes `max = data[i] + fOffset` from the dented array and feeds `local_max_y` (`:5269`), the blob/IMD detector (`:5280`) and spectral peak hold (`:5337`). Only the NF accumulator reads `max_copy` (`:5259`). MW0LGE switched to the copy on the *waterfall* plane (`:6613-6615`, `:6620-6622` `//[2.10.3]MW0LGE use unmodified, not the notced data`). Implementing the original proposed list would create a fresh divergence for ActivePeakHold, PeakBlobs and the cursor readout.

**Amendment.** Dent in place, Thetis-faithful, keeping one pristine copy read only by `processNoiseFloor`. State explicitly (with the `display.cs:5269/5280/5337` cites) that ActivePeakHold, PeakBlobs and the cursor peak readout intentionally see the dent, so a later reviewer does not "fix" it. Make an explicit call on MaxBin and record it as a NereusSDR-specific divergence either way (recommend routing `peakDbmInSlicePassband()` at the undented copy, matching Thetis's effective behaviour). Drop or mark N/A the "waterfall-minimum tracking" half: NereusSDR has no per-frame waterfall minimum; `m_wfLowThreshold` is a persisted user setting (`SpectrumWidget.cpp:623`, `:2157`) and the only consumer of `m_wfRenderedPixels` is `pushWaterfallRow` (`:2880` -> `:466`). Add `nfFftBinAverageForTest()` following the `spotMarkersForTest()` convention (`SpectrumWidget.h:1103-1120`); the driver half already exists (`updateSpectrumLinear` is a public slot, `renderedPixels()` is public at `:394`). Rename the test to `tst_notch_visual_does_not_perturb_noise_floor_or_maxbin` and assert positively that ActivePeakHold/PeakBlobs DO see the dent.

---

### M9. §6.4's "needs no change" is wrong: Thetis broadcasts the global flag on every change

*(Original severity major; both skeptics corrected to **minor**. Listed here for grouping with the TCI work; treat as minor.)*

Thetis's TNF flag is global and event-driven: `console.cs:40004` fires `TNFChangedHandlers` on change, `TCIServer.cs:7690-7698` routes to every listener, and `NfChanged` sends both indices (`TCIServer.cs:1315-1320`). Thetis's `handleRxNfEnable` set branch sends *nothing* itself (`TCIServer.cs:3392-3398`). NereusSDR queues a single index unconditionally (`TciProtocol.cpp:2170-2175`) and has no wiring for a UI-originated flip, which §7 newly creates three of (`+TNF`, status-bar light, shortcut). The mechanism exists and is documented as this exact bug class (`TciProtocol.h:97-103`, "bench bug 2026-05-22"), and 47 sibling flags already use it (`TciServer.cpp`, e.g. `:845-851` APF citing `TCIServer.cs:6770`). `TCIServer.cs:6771` (TNF) is the single unported subscription, sitting between two that are ported.

**Amendment.** Add a §12 step-5 sub-task wiring `NotchModel::globalEnabledChanged` into `TciServer`'s existing broadcast block, emitting `enqueueLocalBroadcast` for rx 0 and rx 1. Then *drop* the handler's single-index push (`TciProtocol.cpp:2173-2175`) as a redundant duplicate, matching Thetis. Narrow §6.4's "This matches Thetis exactly" to the wire format. Note our handler emits unconditionally where Thetis gates on change. The "nothing consumes them" claim is literally false (`TciProtocol.cpp:761-762`) but harmless: post-repoint both indices report the same global flag, which is exactly `GetMNF` (`console.cs:52317-52326`, "// mnf enabled globally"), and both init-burst suites bind `TestMockRadioModel`, so nothing breaks.

---

### M10. §9/D7 build a new Settings page beside an existing greyed one

`SetupDialog.cpp:609` already registers `registerPage(dsp, "MNF", ...)`, and `MnfSetupPage` (`DspSetupPages.cpp:2110-2127`) is a placeholder ending in `disableGroup(mnfGrp)` (`:96-99`, "NYI guard"). "MNF" appears once in the whole spec, at line 400, referring to Thetis's `GetMNF`. Upstream, Thetis's tab is literally named MNF (`setup.designer.cs:44141` `this.tpDSPMNF.Text = "MNF";`) and `grpDSPMNF` holds exactly §9's control set plus §8.3's visual-notch toggle (`:44145-44159`: chkVisualNotch, btnVFOFreq, chkMNFAutoIncrease, btnMNFAdd/Edit/Delete/Enter/Cancel, chkMNFActive, udMNFFreq/Width/Notch). Renaming to "Notches" also cuts against the standing "match Thetis Setup IA 1:1" directive.

There is a third duplicate the finding missed: `SpectrumOverlayPanel.cpp:273-278` already carries a disabled `"MNF"` button ("Manual notch filter (NYI)"), while §1 item 6 adds `+TNF` to that same panel.

**Amendment.** Retarget D7/§9 to fill in the existing `MnfSetupPage` and keep Thetis's page name (or state why "Notches" is preferred). Retire the overlay-panel MNF stub when adding `+TNF`. Add `SetupDialog.cpp` and `DspSetupPages.{h,cpp}` to §10.2 and name the page-fill step in §12 step 9. Note in §8.3 that `chkVisualNotch` is an MNF-tab control upstream (`setup.cs:24376-24379` drives *both* `Display.ShowVisualNotch` and `MiniSpec.ShowVisualNotch`; §8.3 ports only the first).

**Two sub-claims are refuted, drop them.** `comboMNFWindow` does not exist in Thetis (0 hits repo-wide); the NereusSDR placeholder's own comment invented it, and `lstNotches` is `radio.cs:4194`'s private data list, not a control. `RXANBPSetWindow` (`nbp.c:546`) is reached only from Thetis's whole-filter `RXBandpassWindow` property (`radio.cs:1733`, paired with `SetRXABandpassWindow`), the same class §1.2 already declines for `RXANBPSetNC`. Wiring it as proposed would fabricate a control. Separately, `FilterDisplayItem::setNotchPositions` is indeed uncalled, but so are `setSpectrumData`, `setFilterEdgesRx` and `setFilterEdgesTx`: the item's entire data-feed layer is unwired (only `bindRxChannel`/`setHighResolution` are live, `DspOptionsPage.cpp:532`). That is a separate meter-binding work item, not a TNF regression.

---

### M11. §11 has no bench-verification matrix, and the two most testable pure functions are untested

**The convention.** 26 verification artifacts exist under `docs/architecture/`. The closest analogue is exact: `phase3g-rx-epic-b-verification/README.md` is the WDSP NB/NB2/SNB family, same RX-chain-with-audio-only-payload category as an NBP notch, with 13 rows including subjective audio checks. Matrices are treated as merge gates: `phase3m-3a-iv-verification/README.md:46-48` says "PR is not merged until both ANAN-G2 and HL2 columns are PASS". The design-doc precedent is direct too: `2026-05-18-pgxl-tgxl-and-analog-smeter-design.md:1572-1574` names the matrix file in the design while deferring its creation; `phase3m-3a-iv-antivox-feed-design.md:400-410` carries a full "## 11. Manual Bench Verification" section for a far smaller scope; `2026-05-26-phase3f-multi-pan-multi-slice-design.md:720` is the immediate predecessor on this branch. Note the section-number coincidence: anti-VOX's §11 is bench verification; TNF's §11 is unit tests only.

**The spec has none.** `grep -niE "bench|verification|matrix"` over the 647-line spec returns one hit, line 643, describing the base branch.

**And the payload is bench-only observable.** `RXANBPAddNotch` (`nbp.c:367`) and `RXANBPGetMinNotchWidth` (`nbp.c:598`) dereference `rxa[channel]` slots populated only by `create_rxa` (`RXA.c:86`), and the project's own convention documents the limit (`tests/tst_rxchannel_snb.cpp:65`, `tst_rxchannel_emnr.cpp:65`, `tst_rxchannel_squelch.cpp:65`: `kTestChannel = 99;  // Never opened via OpenChannel`).

**Untested pure logic.** `notchSidebandShift` (`console.cs:40281-40307`, with the `middle == 0` AM fallback at `:40294-40295`), `notchesInBandwidth` (`radio.cs:4286`, inclusive edge-overlap), and `notchSurrounding` (`radio.cs:4310`, pad applied only when `n.FWidth < nPadWidth*2`). Sign math confirms the hazard: USB 200/2800 gives +1500, LSB -2800/-200 gives -1500, AM -3000/+3000 hits the fallback for 1500. A dropped sign puts every LSB `+TNF` notch at VFO+1500, outside the passband, silent on LSB and correct on USB.

**Amendment.** Add `docs/architecture/2026-07-28-tnf-verification/README.md` with a per-radio matrix (ANAN-G2 P2, HL2 P1) covering at minimum: notch a real carrier and confirm audible removal; removal persists across a VFO retune within the band (the §4 tunefreq push); drag tracks the cut; per-notch bypass returns the carrier; master TNF off/on; band change away and back; min-width readout changes when nc changes; auto-increase widens a sub-minimum notch; notch survives a live sample-rate change; a slice added after notches inherits them; visual notch shows no dent during MOX. Reference it from §11. Add `tst_notch_sideband_shift` (table-driven, targeting the LSB sign and the AM fallback branch, not C# integer division, which matches C++ truncation anyway) and `tst_notch_spatial_helpers` (edge-overlap inclusion; `notchSurrounding` with and without padWidthHz).

**Two corrections.** "Every phase of comparable scope ships a matrix" is too broad (only 19 of 40 design docs mention one). And §11 does not skip §5.3 entirely: `notchNearFreq` is covered by §5.4's 10 Hz dedupe row, since Thetis implements that guard as `MNotchDB.NotchNearFreq(fFreqHZ, 10)` (`console.cs:40260`). Fold the exact-`deltaHz` boundary (`<` not `<=`, `radio.cs:4267`) into `tst_notch_model_guards` instead.

---

## 4. MINOR

- **§6.2 wrapper return values and `clear()` contract.** *(Original major; both skeptics corrected to minor.)* `RXANBPAddNotch` is an INSERT guarded by `notch <= b->nn && b->nn < b->maxnotches`, returning -1 with no mutation (`nbp.c:362-390`). Thetis discards the rval at all 21 call sites and is self-correcting because it reads the index back from WDSP first (`console.cs:40262-40266`) and rebuilds `MNotchDB` from channel (0,0) after every mutation (`setup.cs:17934-17954`); §5.2 discards both mechanisms and substitutes a release-compiled-out assert that a `void` wrapper cannot feed. Separately, §5.3's `clear()` has no paired signal while §6.3's fan-out is purely signal-driven, so a clear would leave every channel's notch set installed. Fix: state §5.2's recovery path (a `syncNotches()` on readback mismatch), define `clear()`'s contract (emit `notchRemoved` descending, or add `notchesReset()` handled by `syncNotches({})`), and note that unlike AetherSDR (`RadioModel.cpp:4307`) it must NOT run on disconnect under D3. Add `kMaxNotches = 1024` cited to `RXA.c:88` as completeness (unreachable in practice under the 10 Hz dedupe). Drop the per-channel-assert and identity-check sub-claims: Thetis verifies only channel (0,0) and lives with the same index-only bounds check.
- **§6.2 has no `notchTuneFrequencyHz()` accessor.** *(Original blocker; one skeptic corrected to minor, one refuted outright.)* Add the getter and set the carry outside `#ifdef HAVE_WDSP`, matching `setShiftFrequency` (`RxChannel.cpp:1468-1477`). Note the usual "pass the same value so the C++ early-return fires" hatch (`tst_rx_channel_rebuild.cpp:8-11`) does not work here, because WDSP dereferences the NULL NOTCHDB at `nbp.c:478` *before* its own equality check at `:479`. But this is not a blocker: a live gating test is already writable via the `NEREUS_BUILD_TESTS` friend seam plus `createRxChannel`'s real `OpenChannel` (`WdspEngine.cpp:377-390`; pattern at `tst_ps_feedback_channel.cpp:72,78`), and the caller-side value is observable via `RadioModel::streamCentreChanged` / `streamCentreHzForTest` (`RadioModel.h:1947`, `:2483`), which `tst_stream_pool_binding.cpp:580-583` already uses for the sibling shift push.
- **§6.2's incremental-edit rationale is imprecise, and §2.1 omits bpsnba.** *(Original major; both skeptics corrected to minor.)* `UpdateNBPFilters` (`nbp.c:345-359`) designs *two* filters: nbp0 plus `recalc_bpsnba_filter` (`snb.c:814-828`), the latter active for LSB/CWL/DIGL/USB/CWU/DIGU whenever master notch run is on (`RXA.c:934-968`), even with SNB off. Add bpsnba to §2.1. Reword §6.2 to "one `UpdateNBPFilters` where `syncNotches` is 2N of them" (`nbp.c:384`, `:435`, `:456`). Do **not** add drag throttling: Thetis pushes on every mouse-move by named design (`console.cs:49967 //MW0LGE [2.9.0.7] update on drag`, `:50019`), and does more per move than we would (`SaveNotchesToDatabase()` + `UpdateNotchDisplay()`, `:40105-40106`). The impulse-cache concern does not apply: `DspOptionsCacheImpulse` defaults False (`WdspEngine.cpp:236-237`), so `_use_cache` is 0 and nothing is inserted or evicted (`impulse_cache.c:159-167`); Thetis ships the same file with the cache defaulted ON (`radio.cs:189`, `:200`). Also note `setImpulse_fircore` is memcpy + `calc_fircore` (`firmin.c:456-460`) and never replans FFTW.
- **§8.3 cites `handleNotches` at its call site.** Re-cite as `display.cs:8644-8745 [v2.10.3.15]` (the function ends at `:8745`; `:8746` is `_joinBandEdges`, so the originally-proposed `-8790` is wrong). Add the one genuinely missing behaviour: `dentWidth = max(notch.widthHz, minNotchWidthHz()) + 20.0`, cited to `display.cs:8679-8680` ("use the min width of filter from WDSP" / "fudge factor to align better with spectrum notch"), with `kNotchDentFudgeHz = 20.0` carrying its Thetis cite per CLAUDE.md's constants rule. This is not marginal: `nbp.c:91` gives min width `2200/(nc/256)*(rate/48000)` = 275 Hz at nc=2048/48 kHz, above the spec's own 200 Hz default. Wire `minNotchWidthHz()` into the dent path, not just the §9 readout. **Do NOT port the RIT/CTUN term** (`display.cs:8650`): it compensates for Thetis's VFO-label-anchored pixel maths (`:8646`, `:8681`) plus its RIT-driven DDS retune (`console.cs:31776-31777`, `:31894-31895`); NereusSDR's axis is absolute RF (`SpectrumWidget.cpp:4025-4030`), RIT never retunes hardware (`SliceModel.h:740-741`, `RadioModel.cpp:8973-8982`), and WDSP applies shift to the passband not the notch (`nbp.c:192`), so adding it would displace every dent by rit_hz. Add a divergence note so nobody re-adds it. The `_tnf_active && n.Active` gate is not hidden: its consumption site `display.cs:4788` is already inside §8.3's cited `4733-4790` range. Separately, that range truncates the function, which ends at `:4817`; the actual dent maths (`fAttenuation`, `wL`, the `1f/pow(wL/(wL-x),1.5)` skirt) is at `4791-4816` and falls outside the cite.
- **§5.4's XVTR override.** *(Original major; both skeptics corrected to minor.)* The Thetis code is real (`console.cs:40051-40077`) but §5.4's substitute route ("routes through our existing `Band::XVTR` handling") cannot supply `tmpMin`/`tmpMax`: `Band::XVTR` is a label (`Band.cpp:126`, `:215`), `bandFromFrequency` never returns it (`Band.h:95-99`, `Band.cpp:165`), and `AlexController::xvtrActive()` is a bare bool. **Correction to the finding:** a transverter table DOES exist (`src/gui/setup/hardware/XvtrTab.cpp`, RF Start/RF End/LO Offset, persisted at `HardwarePage.cpp:150`); what is missing is a frequency-to-row lookup, a per-RX selected index, any consumer of `loOffset`, and a working round-trip (emits `"rfStart"` at `:239`, restores `"rfStartHz"` at `:290`). Behavioural impact is nil: XVTR band clicks are rejected (`RadioModel.cpp:4813-4816`) and Radio > Transverters is disabled NYI (`MainWindow.cpp:4823-4825`), so Thetis's branch is unreachable and the else-path (plain min/max) is already faithful. **The `f_LO` half is refuted:** the spec quotes those exact lines in §4 at document lines 182-185, and `f_LO` is unconditionally 0.0 absent a transverter index (`console.cs:31734-31738`). Fix: replace §5.4's sentence with an explicit deferral. Note while there that NereusSDR has no radio min/max model value at all (no frequency-range field in `BoardCapabilities.h`, no clamp in `SliceModel::setFrequency:199-217`); the only analogue is `std::clamp(hz, 100000.0, 61440000.0)` at `VfoWidget.cpp:698`/`:2840`, which happens to match Thetis's `max_freq = 61.44` (`console.cs:15552`). That is the more blocking half of the same guard row.
- **§10.1's AetherSDR block names no copyright holder.** *(Original major; one skeptic corrected to minor and refuted the licence half.)* `HOW-TO-PORT.md:36-38` rule 6 requires "the project's URL and primary author". The named template `src/models/SpotModel.h:11` says "(C) its contributors" with no author. 62 files in `src/` use the compliant `Jeremy (KK7GWY) / AetherSDR contributors` form vs 24 using the short one, and `aethersdr-reconciliation.md:72-78` prints the canonical block. §10.1 item 1 also conflicts with item 5, which routes through that same index. Fix: point item 1 at `src/gui/SpectrumOverlayPanel.cpp:12-20` for the author+URL form. **The GPL-3.0-only half is contested and should be left alone:** one skeptic found AetherSDR declares `GPL-3.0-or-later` in its AppStream metainfo (`packaging/linux/io.github.aethersdr.aethersdr.metainfo.xml:9`), twice in `THIRD_PARTY_LICENSES` (`:316`, `:374-375`), in `CMakeLists.txt:1024-1025`, in its plugin README, and as a per-file SPDX tag (`tools/generate_nr2_gamma_tables.cpp:1`); the other found instantiated or-later notices at `src/core/SpectralNR.{h,cpp}:13-19`. Both refute "no or-later grant exists anywhere in the repo". Keep `GPL-3.0-or-later` and just add the author line.

---

## 5. NOTABLE NON-FINDINGS (do not re-litigate)

**The overlay-cache / dual-layer claim was refuted on three grounds.** A reviewer argued that hooking notch hover/drag to `markOverlayDirty()` regresses the dual-layer split. It does not:

1. **Zero marginal cost.** `SpectrumWidget::mouseMoveEvent` already calls `markOverlayDirty()` unconditionally at its tail for every hover mouse-move in the GPU path (`SpectrumWidget.cpp:6491-6495`), plus per-move inside the bandwidth (`:6297`), divider (`:6309`) and pan (`:6325`) drag branches. `NEREUS_GPU_SPECTRUM` is default ON (`CMakeLists.txt:420`). Adding TNF invalidation cannot increase the count.
2. **Wrong path measured.** The 2026-05-26 bench (commit 9723002d) targeted the *timer-driven* per-frame rebuild gated at `SpectrumWidget.cpp:2852-2856` on peak hold / peak blobs / noise floor, i.e. 30 Hz unattended. It did not touch `mouseMoveEvent`. `SpectrumWidget.h:1995-1997` says the static texture carries chrome "that only changes on operator interaction", so invalidating on interaction is the designed behaviour.
3. **The AetherSDR cite is reversed.** AetherSDR's ~6 ms/frame measurement is about bare cursor-position re-bakes producing a byte-identical overlay, and the same comment explicitly exempts TNF: "TNF-hover and tune-guide-visibility changes are genuine content changes and still invalidate unconditionally." AetherSDR's hover invalidation is also edge-triggered (`m_hoveredTnfId != oldHoveredTnfId`), firing twice per traverse, not per mouse-move.

The only residue: §8.2's internal cite `SpectrumWidget.h:1889-1925` is wrong (that range is filter/VFO drag state and the HIGH-SWR/MOX block); it should read `:1991` / `:2062-2066`. That is a NereusSDR self-reference, not an upstream cite, so no attribution gate is at stake.

**Other things attacked that turned out correct, worth pinning:**

- **`RXANBPSetTuneFrequency` really is absent from the tree.** `grep -rn "RXANBPSetTuneFrequency" src/ tests/` returns nothing; `wdsp_api.h:337` declares only `RXANBPSetFreqs` and `:357` only `RXANBPSetShiftFrequency`. §4's premise is sound.
- **`Band::XVTR` really carries no frequency range**, and `bandFromFrequency` really never returns it. The header states it (`Band.h:95-99`) and `Band.cpp:165` falls through to `Band::GEN`.
- **Test executables really do compile with `HAVE_WDSP`.** `CMakeLists.txt:1124` sets it PUBLIC on `NereusSDRObjs`; `tests/CMakeLists.txt:165` links it; verified across all 1126 entries in `build/compile_commands.json`. The channel-99 convention (`tst_rxchannel_snb.cpp:65` et al.) exists because of it.
- **Notches really are inert by default per channel.** `create_notchdb(0, ...)` and `create_nbp(1, 0 /*run the notches*/, ...)` at `RXA.c:85-93`; both `calc_nbp_lightweight` (`nbp.c:190`) and `calc_nbp_impulse` (`:222`) short-circuit on `fnfrun`. Any design that forgets `RXANBPSetNotchesRun` produces a silent no-op.
- **Thetis's per-mouse-move notch push is deliberate**, not an oversight to be optimised away: `console.cs:49967` carries `//MW0LGE [2.9.0.7] update on drag`, and the mouse-up commit at `:51106-51118` is the older path it superseded.
- **`notchNearFreq` is already covered** by §5.4's 10 Hz dedupe row, because that guard *is* `MNotchDB.NotchNearFreq(fFreqHZ, 10)` (`console.cs:40260`). Do not double-scope it into a new spatial-helpers test.