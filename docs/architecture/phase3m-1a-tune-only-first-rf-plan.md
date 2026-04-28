# Phase 3M-1a — TUNE-only First RF: Implementation Plan

**Status:** ready for execution.
**Date:** 2026-04-25.
**Branch:** `feature/phase3m-1a-tune-only-first-rf`.
**Base:** `origin/feature/phase3m-0-pa-safety` @ `159bf16`.
**Pre-code review:** `docs/architecture/phase3m-1a-thetis-pre-code-review.md`.
**Master design:** `docs/architecture/phase3m-tx-epic-master-design.md` §5.1
(with corrections per pre-code review §10).
**Risk profile:** HIGH — first MOX byte / first RF on the air.

---

## 0. Brainstorm-locked decisions

Three decisions from the planning conversation, all baked into this plan:

1. **Watchdog wire-bit research = research-bounded.** Read open-source
   alternatives first, port what's found, defer with documented blocker
   for what isn't. **Outcome:** P1 watchdog FOUND in HL2 firmware
   (RUNSTOP byte 1 bit 7). P2 watchdog NOT found in open-source — stub
   stays, TODO comment updated, tracking issue filed. See pre-code
   review §7.5 + §7.8.
2. **Risk mitigation = no extra gate.** Trust 3M-0's safety nets
   (`SwrProtectionController`, `BandPlanGuard`, `TxInhibitMonitor`,
   PA Status badge, status-bar TX Inhibit indicator). No
   `NEREUS_ENABLE_TX` compile flag. No first-launch acknowledgment
   dialog. Per `feedback_dont_ask_to_stop`, JJ has full bench oversight
   and will stop if anything looks off.
3. **Bench-test cadence = pre-merge gate.** Branch implementation runs
   to green unit tests, then **HL2 bench pass**, then **G2 bench pass**
   (in that order — HL2 first as the lower-stakes smoke), THEN PR
   opens. PR review happens on already-validated code. Verification
   matrix lives at the tail of the plan with two row-states per row
   (HL2: passed pre-merge / G2: passed pre-merge).

---

## 1. Workflow shape

**Single branch, single PR.** All commits on
`feature/phase3m-1a-tune-only-first-rf`.

**Commit shape:**
- **Commit 1:** Pre-code Thetis review (`phase3m-1a-thetis-pre-code-review.md`).
- **Commit 2:** This plan (`phase3m-1a-tune-only-first-rf-plan.md`).
- **Commits 3-N:** TDD task commits (one per task, see §3).
- **Commit N+1:** Verification matrix update (3M-1a rows).
- **Commit N+2:** Post-code review §2 appended to the pre-code review
  document, before PR open.
- **PR open:** drafted in chat first per `feedback_no_public_posts_without_review`,
  opened only on explicit "post it" sign-off.

**Per-task discipline (rigid TDD):**
1. State the pre-code review section being ported.
2. Write the failing test first (or for source-first ports: state cite,
   write the failing assertion that captures the cited behaviour).
3. Implement to green.
4. Run the relevant `ctest` subset locally before commit.
5. GPG-sign the commit (no `--no-gpg-sign`; per `feedback_gpg_sign_commits`).
6. After each commit, run a two-stage subagent review (spec compliance +
   code quality). Fresh subagent per task — no shared state.
7. Resolve review feedback (if any) via fixup commit on the same task,
   then advance.

**Inline cite stamp:** `[v2.10.3.13]` (Thetis tag) or `[@<shortsha>]`
when no tagged release applies. The verifier script
(`scripts/verify-inline-tag-preservation.py`) runs in the pre-commit
hook chain — `NEREUS_THETIS_DIR=/Users/j.j.boyd/Thetis` must be set in
the shell before each commit.

**Codex review patterns to watch (from PR #139):**
- **P1: signal multi-emit.** When one Qt setter calls another, both
  emit a shared signal → subscribers fire twice. Subscribe at the
  per-sample boundary signal (e.g., `MoxController::txStateChanged`),
  not individual setters.
- **P2: idempotent-guard placement.** Safety-critical effects must
  execute BEFORE the `if (m_x == newX) return` idempotent guard. A
  repeated transition can't skip the safety effect. Order is:
  *(safety effect) → (state transition) → (boundary signal emit) →
  (idempotent guard for next call)*.

---

## 2. Files

**New (5):**
- `src/core/MoxController.{h,cpp}`
- `src/core/TxChannel.{h,cpp}`
- `src/core/TxMicRouter.{h,cpp}` (interface only; sources stubbed)
- `src/core/PttSource.h` (enum + helpers)

**Modified (~13):**
- `src/core/WdspEngine.{h,cpp}` — `createTxChannel` / `destroyTxChannel` /
  `setPureSignalSource` (PS API stub) + 31-stage TXA construction.
- `src/core/RadioModel.{h,cpp}` — own `MoxController` + `TxChannel` +
  `TxMicRouter`; wire Receive Only visibility from `caps.isRxOnlySku`.
- `src/models/TransmitModel.{h,cpp}` — MoxController integration,
  per-band `tunePower_by_band[14]`.
- `src/core/RadioConnection.{h,cpp}` — `virtual void sendTxIq()`,
  `virtual void setMox(bool)` (already declared), `virtual void setTrxRelay(bool)` (added
  in E.1), `virtual void setWatchdogEnabled(bool)` (already declared).
  (NereusSDR base class already declares `setMox`/`setWatchdogEnabled`; new method
  `setTrxRelay` added in E.1; the `*Bit` shorthand in early plan drafts was renamed
  for consistency.)
- `src/core/P1RadioConnection.{h,cpp}` — TX I/Q in EP2 zones; MOX byte 3
  bit 0; T/R relay byte 6 bit 7; tune-power; watchdog RUNSTOP byte 1
  bit 7.
- `src/core/P2RadioConnection.{h,cpp}` — TX I/Q on port 1029; high-pri
  byte 4 bit 1 MOX; high-pri byte 345 drive level; watchdog state-track
  stub (P2 wire bit deferred per §7.8).
- `src/core/safety/SwrProtectionController.{h,cpp}` — 2 TODOs resolved:
  `alex_fwd > alex_fwd_limit` floor + `tunePowerSliderValue ≤ 70`
  override.
- `src/gui/SpectrumWidget.{h,cpp}` — Display.MOX overlay slot;
  `setTxAttenuatorOffsetDb` slot.
- `src/gui/meters/MeterPoller.{h,cpp}` — TX-side bindings observed live
  during MOX (3M-0 already routed; data flows now).
- `src/gui/applets/TxApplet.{h,cpp}` — TUN / Tune-Power / RF-Power /
  MOX deep-wired; out-of-phase controls hidden (mic / EQ / proc /
  CFC / DEXP / VOX / 2-TONE / PS-A).
- `src/gui/setup/TransmitSetupPages.cpp` — Power & PA page's
  `setOutputPower` becomes effective; per-band tune-power column wired;
  ATTOnTX + ForceATTwhenPSAoff toggles.
- `src/gui/setup/GeneralOptionsPage.cpp` — Receive Only checkbox
  visibility callback wired (`setReceiveOnlyVisible(bool)`).
- `src/core/StepAttenuatorController.{h,cpp}` — TX-path entry points
  (`applyTxAttenuationForBand` already there from 3G-13;
  `saveRxPreampMode` / `restoreRxPreampMode` helpers added for HPSDR
  variant; `shouldForce31Db(dspMode, isPsOff)` predicate).

**Tests (new):** ~16 new test files, listed inline per task.

---

## 3. TDD task list

Tasks numbered as `<phase>.<index>`. Each is one commit. Phase letters
match brainstorm Section 4.

### Phase A — Pre-code documentation (already in flight)

| # | Task | Commit type | Notes |
|---|---|---|---|
| A.1 | Pre-code Thetis review document | docs | Already authored; commits as part of this branch. |
| A.2 | TDD implementation plan (this file) | docs | Already authored; commits as part of this branch. |

### Phase B — MoxController + PttSource enum (5 tasks)

Builds the state machine without firing any hardware. Pure logic, fully
unit-testable.

| # | Task | Tests | Cite |
|---|---|---|---|
| B.1 | `PttMode` enum (Thetis verbatim, 9 values: NONE, MANUAL, MIC, CW, X2, CAT, VOX, SPACE, TCI — order matches `enums.cs:346-359 [v2.10.3.13]`, with `FIRST = -1` and `LAST` sentinels per Thetis) + helpers (`pttModeLabel(PttMode)`, `pttModeFromString(QString)`). New file `src/core/PttMode.h`. **Disambiguation:** the existing NereusSDR-native `PttSource` enum (`src/core/PttSource.h`, 3P-H) is **separate** — it tracks UI-level PTT origin for the Diagnostics page (includes `Tune`/`TwoTone` concepts that aren't Thetis PTTMode values). `PttMode` is the radio-level Thetis mode used by `MoxController`; both coexist. | `tst_ptt_mode.cpp` (new file; existing `tst_ptt_source.cpp` stays as-is) | Pre-code §1.5 + Thetis `enums.cs:346-359 [v2.10.3.13]` |
| B.2 | `MoxController` skeleton: `MoxState` enum (Rx, RxToTxRfDelay, RxToTxMoxDelay, Tx, TxToRxKeyUpDelay, TxToRxBreakIn, TxToRxFlush), `setMox(bool)` slot with idempotent-guard-AFTER-safety-effect ordering (Codex P2), `m_mox` and `m_pttSource` fields | `tst_mox_controller_basic.cpp` (state transitions, no timers yet) | Pre-code §1.4 |
| B.3 | `MoxController` 6 `QTimer` chains (`rfDelay 30 ms`, `moxDelay 10 ms`, `spaceDelay 0 ms`, `keyUpDelay 10 ms`, `pttOutDelay 20 ms`, `breakInDelay 300 ms`); state-driven advance via timer slots | `tst_mox_controller_timers.cpp` (uses `QTest::qWait` to drive simulated time; verifies cadence) | Pre-code §1.3 |
| B.4 | `MoxController` 6 phase signals (`txAboutToBegin()`, `hardwareFlipped()`, `txReady()`, `txAboutToEnd()`, `txaFlushed()`, `rxReady()`); emit at correct state-machine boundaries; subscribers attach at boundary signal not individual setters (Codex P1) | `tst_mox_controller_phase_signals.cpp` (signal-spy ordering) | Pre-code §1.4 |
| B.5 | `MoxController::setTune(bool)` slot — drives MOX through `setMox(true)`, sets `_manual_mox = true`, `_current_ptt_mode = PTTMode.MANUAL`; on `setTune(false)` clears flags and drives `setMox(false)` | `tst_mox_controller_tune.cpp` (TUN engages MOX, TUN-off releases MOX) | Pre-code §3.2 |

### Phase C — TxChannel + WDSP API (4 tasks)

Builds the WDSP TX channel with the 31-stage pipeline and TUNE
configuration.

| # | Task | Tests | Cite |
|---|---|---|---|
| C.1 | `WdspEngine::createTxChannel()` / `destroyTxChannel()` API; channel ID = `WDSP.id(1, 0)`; channel parameters (96 kHz DSP rate, type 1 = TX, slewup/slewdown 10 ms, block-on-output) | `tst_wdsp_engine_tx_channel.cpp` (channel created with correct params; idempotent destroy) | Pre-code §8.4 |
| C.2 | `TxChannel` skeleton: 31-stage TXA pipeline build via `create_txa()`; verify all 31 stages constructed; default Run states match Thetis (panel/micmeter/eqmeter/lvlrmeter/cfcmeter/bp0/compmeter/alc/alcmeter/sip1/calcc/outmeter ON; rest OFF) — 12 ON / 19 OFF, total 31 stages per `wdsp/TXA.c:31-479 [v2.10.3.13]` | `tst_tx_channel_pipeline.cpp` (introspect each stage's `Run` attribute) | Pre-code §8.1 |
| C.3 | `TxChannel::setTuneTone(bool on, double freqHz, double magnitude)` — writes to gen1 (PostGen) via `SetTXAPostGenMode/Mag/Freq/Run`; mode=0 sine, freq=±cw_pitch (sign per current DSP mode — caller passes signed freq), magnitude = `kMaxToneMag = 0.99999f` (Thetis `console.cs:29954 [v2.10.3.13]` — preserve inline `// why not 1?  clipping?` comment per GPL attribution rule) | `tst_tx_channel_tune_tone.cpp` (PostGen state after on/off; sign-handling via parametrized inputs) | Pre-code §3.5, §8.3 |
| C.4 | `TxChannel::setRunning(bool)` — calls `WDSP.SetChannelState(WDSP.id(1, 0), running ? 1 : 0, running ? 0 : 1)`; activates the 10 active stages in 3M-1a (rsmpin / bp0 / alc / gen1 / uslew / cfir / rsmpout / sip1 / alcmeter / outmeter) | `tst_tx_channel_running.cpp` (channel state transition; idempotent) | Pre-code §8.1 (3M-1a active stages column) |

### Phase D — TxMicRouter interface stub (1 task)

| # | Task | Tests | Cite |
|---|---|---|---|
| D.1 | `TxMicRouter` interface (header only): pure-virtual `int pullSamples(float* dst, int n)`; concrete `NullMicSource` (returns zero-padded samples) for 3M-1a TUNE path; concrete `PcMicSource` and `RadioMicSource` deferred to 3M-1b with `// TODO [3M-1b]` markers | `tst_tx_mic_router.cpp` (NullMicSource emits zero-padding; interface satisfies WDSP TX-input contract) | Master design 5.1.1 |

### Phase E — Wire format (P1 + P2): 7 tasks

The highest-stakes phase. Each wire-bit task gets a wire-byte snapshot
test that compares the outbound frame byte-by-byte to a golden vector.

| # | Task | Tests | Cite |
|---|---|---|---|
| E.1 | `RadioConnection` (base): `virtual void sendTxIq(const float* iq, int n) = 0`; `virtual void setMox(bool)` (already declared, 3M-0); `virtual void setTrxRelay(bool)` (new, E.1); `virtual void setWatchdogEnabled(bool)` (already declared, 3M-0). Wires the dispatch chain. | `tst_radio_connection_tx_iface.cpp` (base class compiles; subclasses must override; signal/slot wiring) | Master design 5.1.4 |
| E.2 | `P1RadioConnection::sendTxIq()` — write TX I/Q to EP2 frame zones in the 1032-byte Metis frame; integration with existing `sendCmdHighPriority` outbound cadence | `tst_p1_tx_iq_wire.cpp` (wire-byte snapshot with hand-fed I/Q vector) | Pre-code §7.10 (P1 row), Thetis EP2 layout in NetworkIO.cs |
| E.3 | `P1RadioConnection::setMox(bool)` — sets/clears C0 byte 3 bit 0 (`0x01`); idempotent at the field level; runs the safety-effect-first idempotent-guard pattern (write the bit, then guard the next call) | `tst_p1_mox_wire.cpp` (wire-byte snapshot: MOX=true → byte 3 has bit 0 set; MOX=false → bit 0 cleared) + cross-confirm against HL2 firmware decoding | Pre-code §7.1 |
| E.4 | `P1RadioConnection::setTrxRelay(bool)` — sets/clears C3 byte 6 bit 7 (`0x80`); inverted semantic (`1=disabled`); only writes `1` when PA explicitly disabled; otherwise `0` (relay engaged) | `tst_p1_trx_relay_wire.cpp` (wire-byte snapshot for both PA-enabled and PA-disabled states) | Pre-code §7.2 |
| E.5 | `P1RadioConnection::setWatchdogEnabled(bool)` — RUNSTOP packet byte 1 bit 7; inverted semantic (`1=disabled`, default-on writes `0`); resolves `// TODO [3M-1a]` at `P1RadioConnection.cpp:854` | `tst_p1_watchdog_wire.cpp` (wire-byte snapshot for enabled/disabled states; cite refresh in source comment to point at HL2 firmware finding) | Pre-code §7.5 |
| E.6 | `P2RadioConnection::sendTxIq()` — write TX I/Q to UDP port 1029 in 60-sample frames (288-byte payload per frame; existing P2 frame-builder pattern from 3A) | `tst_p2_tx_iq_wire.cpp` (wire-byte snapshot of frame layout + sequence numbering) | Pre-code §7.10 (P2 rows) |
| E.7 | `P2RadioConnection::setMox(bool)` + `composeCmdTx` mox flag — writes high-priority byte 4 bit 1 (`0x02`) for MOX; high-priority byte 345 (`drive_level & 0xFF`) for tune power; UDP port 1027 | `tst_p2_mox_wire.cpp` + `tst_p2_drive_level_wire.cpp` (wire-byte snapshots) | Pre-code §7.6, §7.7 |
| E.8 | `P2RadioConnection::setWatchdogEnabled(bool)` — state-tracking stub; updates the existing `// TODO [3M-1a]` comment at `P2RadioConnection.cpp:560` to cite pre-code review §7.8 (P2 wire bit deferred); files a tracking issue (`gh issue create --title "P2 watchdog wire-bit research"`) and links from the comment | `tst_p2_watchdog_state_track.cpp` (state stored, no wire emission; idempotent) | Pre-code §7.8 |

### Phase F — Hardware flip + safety (3 tasks)

| # | Task | Tests | Cite |
|---|---|---|---|
| F.1 | `RadioModel::onMoxHardwareFlipped(bool isTx)` slot — connects `MoxController::hardwareFlipped` to `AlexController::applyAntennaForBand(currentBand(), isTx)` AND `RadioConnection::setMox(isTx)` AND `RadioConnection::setTrxRelay(isTx)`. Order matters: Alex routing FIRST, then MOX wire bit, then T/R relay (matches Thetis HdwMOXChanged step order — see pre-code §2.3) | `tst_radio_model_mox_hardware_flip.cpp` (signal-spy verifies emit order: applyAntennaForBand → setMox → setTrxRelay) | Pre-code §2.3, §2.5 |
| F.2 | `StepAttenuatorController` TX-path activation: subscribe to `MoxController::hardwareFlipped(isTx)`; on `isTx=true` call `applyTxAttenuationForBand(currentBand)` plus `shouldForce31Db()` predicate (PS-off OR CWL/CWU); on `isTx=false` restore RX-band ATT. HPSDR variant (Atlas/Hermes-original) uses `saveRxPreampMode` / `restoreRxPreampMode` helpers | `tst_step_att_tx_path.cpp` (force-31 trigger logic; HPSDR vs non-HPSDR branches) | Pre-code §6.2-§6.4 |
| F.3 | `SwrProtectionController` 2 TODOs: (a) `alex_fwd > alex_fwd_limit` floor (5 W default; 2× power-slider for ANAN-8000D); (b) `tunePowerSliderValue ≤ 70` override on the tune-bypass block. Both pure source-first ports from Thetis cites already in TODO comments | `tst_swr_protection_tune_floors.cpp` (3 cases: alex_fwd < 5W bypass; tune-power ≤ 70 bypass; both pass-through to existing logic) | Pre-code §7.9, Thetis `console.cs:26020-26057` + `:26067` |

### Phase G — Integration (4 tasks)

| # | Task | Tests | Cite |
|---|---|---|---|
| G.1 | `RadioModel` owns `MoxController` + `TxChannel` + `TxMicRouter`; wires phase-signal subscribers; ensures parent ownership for Qt cleanup; threading: MoxController on main thread, TxChannel on audio thread, TxMicRouter on audio thread | `tst_radio_model_tx_ownership.cpp` (RAII ownership; thread-affinity verification with `QObject::thread()`) | Master design 5.1.4 |
| G.2 | `RadioModel::onActiveRadioChanged(BoardCapabilities caps)` — fires `setReceiveOnlyVisible(caps.isRxOnlySku)` on `GeneralOptionsPage`; emits via existing connection-state-changed signal pipeline | `tst_general_options_rx_only_visibility.cpp` (HL2-RX-only kit shows checkbox; standard HL2 hides it; ANAN hides it) | Master design 5.1.4 carry-forward; `BoardCapabilities.cpp:652` for HL2-RX-only |
| G.3 | `TransmitModel::tunePowerByBand[14]` — `std::array<int, 14>`, default 50 W per band; AppSettings persistence per-MAC under `hardware/<mac>/tunePowerByBand/<band-index>`; setter/getter + `setTunePowerForBand(Band, int)` / `tunePowerForBand(Band)`; `MoxController::setTune(true)` calls `transmitModel().tunePowerForBand(currentBand())` and pushes to `RadioConnection::setDriveLevel(int)` | `tst_transmit_model_tune_power.cpp` (per-band default; per-MAC round-trip; `setTune` reads correct band slot) | Pre-code §4.4 |
| G.4 | TUNE function port: `MoxController::setTune(true)` integrates the full Thetis path — power-on gate, `_tuning = true`, lock meter mode to `tune_meter_tx_mode`, set TX PostGen tone (`TxChannel::setTuneTone`), CW→LSB/USB swap (save `old_dsp_mode`, restore on TUN-off), `_current_ptt_mode = PTTMode.MANUAL`, `_manual_mox = true`. ATU async + 2-TONE pre-stop + Apollo deferred to 3M-3/3M-6 with `// TODO` markers | `tst_tune_function_port.cpp` (CW→LSB swap on TUN; mode restore on TUN-off; meter-mode lock + restore; power-off gate rejects TUN) | Pre-code §3 |

### Phase H — UI activation (4 tasks)

| # | Task | Tests | Cite |
|---|---|---|---|
| H.1 | `SpectrumWidget::setMoxOverlay(bool)` slot — TX-mode color scheme via `TxFilterColor` / `DisplayFilterTxColor` / `TxDisplayBackgroundColor` swap; `setTxAttenuatorOffsetDb(float)` shifts dBm scale; gates on existing `DrawTXFilter` flag for the TX filter outline | `tst_spectrum_widget_mox_overlay.cpp` (color swap on/off; offset propagation) | Pre-code §5 |
| H.2 | `MeterPoller` TX-side bindings observed live during MOX state — verify (a) Fwd/Rev/SWR/ALC bindings paint values when MOX is true (3M-0 already routed; this task just confirms data flow under TXChannel state); (b) RX-side bindings stop painting (no dual-meter race) | `tst_meter_poller_tx_live.cpp` (signal-spy on each TX binding when MOX true; expected 0 emissions when MOX false) | Master design 5.1.1 |
| H.3 | `TxApplet`: TUN button → `MoxController::setTune(toggle)`; Tune-Power slider → `TransmitModel::setTunePowerForBand(currentBand, value)`; RF-Power slider → `TransmitModel::setRfPower(value)` (passive in 3M-1a; activates in 3M-1b); MOX button → `MoxController::setMox(toggle)`. Hide out-of-phase controls (Mic Gain, Profile, PROC/LEV/EQ/CFC/DEXP/VOX, 2-TONE, PS-A) per master design 5.1.2 | `tst_tx_applet_3m1a_wiring.cpp` (button toggles drive correct slots; out-of-phase controls hidden) | Master design 5.1.2 |
| H.4 | `TransmitSetupPages` Power & PA page: per-band tune-power column wired to `TransmitModel::setTunePowerForBand`; ATTOnTX checkbox wired to `m_bATTonTX`; ForceATTwhenPSAoff checkbox wired to `_forceATTwhenPSAoff`. Both checkboxes were 3M-0 surface stubs; this task makes them effective | `tst_transmit_setup_pages_3m1a.cpp` (per-band edits round-trip; checkbox edits propagate to controllers) | Pre-code §6.1 + master design 5.1.2 |

### Phase I — Verification (3 tasks)

| # | Task | Description |
|---|---|---|
| I.1 | Unit-test sweep + build green: `cmake --build build && ctest --output-on-failure -j$(sysctl -n hw.ncpu)`. All 140+ tests pass (existing 140 from 3M-0 baseline + ~16 new from 3M-1a). No regressions. | Verification step, no commit. |
| I.2 | **HL2 bench test** (Hermes Lite 2 + dummy load): connect HL2; launch app; tune to a 40m band slot; set Tune Power = 1 W; click TUN; observe ~1 W carrier on dummy load (verify with HL2's built-in PA telemetry); click TUN off; observe clean release with no audible click; verify SWR meter reading in `[1.0, 2.0]` band; verify ATT-on-TX = 31 dB during TUN (CW path forces it); verify Display.MOX overlay paints; verify SwrProtectionController stays out of foldback for normal SWR. Document in verification matrix row `[3M-1a-bench-HL2]`. Commit: `test(verify): 3M-1a HL2 bench pass — TUN carrier, meters, foldback`. |
| I.3 | **G2 bench test** (ANAN-G2 + dummy load): same sequence with G2 hardware; observe ~25 W tune carrier on the band-current TX antenna (Alex routing exercised); verify `applyAntennaForBand(isTx=true)` writes the correct register sequence (cross-check `txAnt[band]` setting); verify ATT-on-TX changes preamp/att appropriately; verify Display.MOX overlay; verify foldback floor (`alex_fwd > 5W`) prevents false trips during ramp-up. Document in verification matrix row `[3M-1a-bench-G2]`. Commit: `test(verify): 3M-1a G2 bench pass — TUN carrier, Alex routing, foldback floors`. |
| I.4 | Verification matrix update at `docs/architecture/phase3m-0-verification/README.md` — add ~7 new rows (per brainstorm Section 5) and flip ~5 carry-forward rows from "deferred" to passing. Commit: `docs(3m-1a): verification matrix update`. |
| I.5 | Post-code review §2 appended to `phase3m-1a-thetis-pre-code-review.md` — for each surface §1-§9 + §0.1, one paragraph: did the impl match the pre-code transcription? Note any deltas. Codex P1/P2 patterns checked. Bench results summary. Commit: `docs(3m-1a): post-code Thetis review §2`. |

---

## 4. Estimated task count and ordering

**Total tasks:** 31 (A.1, A.2, B.1-B.5, C.1-C.4, D.1, E.1-E.8, F.1-F.3,
G.1-G.4, H.1-H.4, I.1-I.5).

**Strict ordering** for dependent layers:
- Phase A → B → C → D (skeleton/foundations; can be re-ordered within
  phase but not across).
- Phase E depends on D (TxMicRouter interface) and C.4 (TxChannel running)
  for the integration test paths in E.2 / E.6, but E.1, E.3, E.4, E.5,
  E.7, E.8 can run in parallel after E.1 lands.
- Phase F depends on B (MoxController phase signals), E (wire bits), and
  3G-13's existing StepAttenuatorController.
- Phase G depends on B + C + F; G can run in parallel within itself
  after G.1 (RadioModel ownership) lands.
- Phase H depends on G (the MoxController/TransmitModel signals must
  exist before UI binds).
- Phase I is gated on all impl tasks green.

**Per-task review cadence:** subagent-driven-development manages the
two-stage review per task. Expect one review checkpoint per task in
chat. Fresh subagent per task; no shared state.

---

## 5. Verification matrix delta

Updates to `docs/architecture/phase3m-0-verification/README.md`. New
rows (~7) and carry-forward flips (~5) per brainstorm Section 5:

**New rows:**
- `[3M-1a-bench-HL2]` MOX wire bit P1 (RUNSTOP byte 1 bit 7 watchdog +
  C0 byte 3 bit 0 MOX) — bench observed.
- `[3M-1a-bench-G2]` MOX wire bit P2 (high-pri byte 4 bit 1 + byte 345
  drive level) — bench observed.
- `[3M-1a]` TX I/Q frame format (P1 EP2 zones) — wire-byte snapshot test.
- `[3M-1a]` TX I/Q frame format (P2 port 1029) — wire-byte snapshot test.
- `[3M-1a-bench-G2]` First Alex TX activation
  `applyAntennaForBand(isTx=true)` — bench observed per `txAnt[band]`.
- `[3M-1a-bench]` ATT-on-TX timing — preamp/att change across MOX
  boundary; force-31 in CW or PS-off.
- `[3M-1a-bench]` Tune carrier audible cleanliness — 5 ms uslew ramp,
  no click on TUN release.
- `[3M-1a]` Foldback floor (`alex_fwd > 5W`) — unit test.
- `[3M-1a]` Foldback override (`tunePowerSliderValue ≤ 70`) — unit test.

**Carry-forward 3M-0 rows that flip from deferred to passing:**
- `[3M-0]` SwrProtectionController — was inert, now wired (with the 2
  TODOs resolved in F.3).
- `[3M-0]` BandPlanGuard — was inert, now wired (3M-0 implementation
  active during MOX).
- `[3M-0]` TxInhibitMonitor — was inert, now wired.
- `[3M-0]` PA Status badge — was inert, now reflects real PA state from
  meters during MOX.
- `[3M-0]` Status-bar TX Inhibit indicator — was inert, now reflects
  guard state.
- `[3M-0]` PA Telemetry meters — was inert, now show live Fwd/Rev/SWR/
  ALC during MOX.

---

## 6. Rollback strategy

Per Section 5 of brainstorm:

- **Pre-merge:** "don't merge yet" — the bench-test gate is the safety
  belt. Branch can keep iterating with rebase/fixup commits before PR
  opens.
- **Post-merge:** `git revert` on the merge commit. PR description must
  call out explicitly that 3M-1a enables RF emission. There is no
  compile-time switch.
- **If a wire-format bug is found post-merge:** hot-fix PR (forward
  patch preferred over revert if bug is scoped); 3M-1b waits until 3M-1a
  is healthy.

---

## 7. PR shape (drafted in chat first)

One PR titled `feat(tx): Phase 3M-1a TUNE-only First RF`. Body covers:
- Architecture summary (5 new files + 13 modified files)
- Three locked brainstorm decisions
- Risk profile callout (HIGH — first MOX byte / first RF on the air)
- HL2 + G2 bench results (✅ with brief log excerpts; commit hashes
  for `[3M-1a-bench-HL2]` / `[3M-1a-bench-G2]` matrix rows)
- Verification-matrix delta (link)
- Pre-code review link (`phase3m-1a-thetis-pre-code-review.md`)
- Resolved carry-forward TODOs (4 listed; outcomes per pre-code §7)
- Receive Only checkbox visibility wiring note
- Codex P1/P2 patterns checked

Per `feedback_no_public_posts_without_review`: body drafted in chat
first; opened only on explicit "post it" sign-off.

Per `feedback_public_post_signature`: PR description ends with
`J.J. Boyd ~ KG4VCF`. No "73 de" preamble. No Claude line.

---

## 8. References

- **Pre-code Thetis review:**
  `docs/architecture/phase3m-1a-thetis-pre-code-review.md`
  (12 sections; the source-of-truth for behaviour during impl)
- **Master design:**
  `docs/architecture/phase3m-tx-epic-master-design.md` §5.1
  (with corrections per pre-code review §10)
- **3M-0 verification matrix:**
  `docs/architecture/phase3m-0-verification/README.md`
- **CLAUDE.md** — source-first protocol, attribution rules, Thetis
  cite versioning grammar
- **CONTRIBUTING.md** — coding conventions, GPG-signing, pre-commit
  hooks
- **Codex review patterns from PR #139:**
  P1 (signal multi-emit) + P2 (idempotent-guard placement)

---

**End of plan.**
