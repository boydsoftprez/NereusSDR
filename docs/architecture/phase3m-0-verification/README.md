# Phase 3M-0 PA Safety Foundation — Verification Matrix

Manual verification for the 3M-0 safety net. Run on hardware before merging
the 3M-0 PR. Recommended bench: **ANAN-G2 + Hermes-Lite 2 + Atlas (mic-jack-only)**
to cover the three SKU classes the safety net targets.

The 3M-0 controllers are inert until 3M-1a fires the first MOX. Most rows
will only show observable effects after 3M-1a lands; this matrix is the
acceptance gate for both phases. **Until 3M-1a, rows that need a real
TX go unchecked here and are tagged `[3M-1a-bench]`.**

| # | Test | Hardware | Procedure | Expected | Result |
|---|---|---|---|---|---|
| 1 | Synthetic SWR foldback unit test | none | `cd build && ctest -R '^tst_swr_protection_controller$' -V` | All 9 SWR slots PASS (per-sample foldback factor ~= limit/(swr+1) at 4 trips with windback off) | |
| 2 | Latched windback after 4 trip samples `[3M-1a-bench]` | ANAN-G2 + 50Ω dummy load with mismatch jig | TX SSB at full power into a deliberate mismatch (~3:1 SWR). Hold key for >100 ms. Watch the PA Power gauge. | After 4 consecutive 100ms samples above limit, drive multiplier drops to 0.01 (gauge collapses to ~1% of drive). Latch persists until MOX-off. | |
| 3 | Open-antenna detection `[3M-1a-bench]` | ANAN-G2, no antenna connected | Key TX at 15W carrier with antenna disconnected. | `openAntennaDetected()` asserts within 1 sample. Drive collapses to 0.01. HighSwr overlay appears. | |
| 4 | TX-inhibit user-IO pin `[3M-1a-bench]` | ANAN-G2, jumper connected to UserIO 1 | With "Update with TX Inhibit state" enabled, ground UserIO 1 (or +3.3V if reversed). | `txInhibitedChanged(true, UserIo01)` fires within 100ms. MOX rejected. Status-bar "TX INHIBIT" pill becomes visible. | |
| 5 | Watchdog enable `[3M-1a-bench]` | ANAN-G2 over network | With Network Watchdog checked (default), TX a continuous carrier, then physically pull the network cable. | Radio LED stops MOX within 1s of suppressed C&C (radio-side firmware behaviour). Note: in 3M-0, `setWatchdogEnabled(bool)` is a stub — the wire bit emit is deferred to 3M-1a. This row may not pass until 3M-1a wires the actual ChannelMaster.dll-equivalent bit position. | |
| 6 | BandPlanGuard 60m channelization | none | `cd build && ctest -R '^tst_band_plan_guard$' -V` | All 18 slots PASS. UK 11 channels + US 5 channels + JA 4.63 MHz + Europe/Australia/Spain band ranges + extended bypass + per-band rxBand/txBand prevent. | |
| 7 | Extended toggle bypass `[3M-1a-bench]` | ANAN-G2 | Setup → General → Hardware Configuration → check "Extended". Try TX at 13.5 MHz (out-of-band). | TX allowed (extended bypass per console.cs:6772). Uncheck Extended; same TX is rejected. | |
| 8 | PreventTxOnDifferentBand guard `[3M-1a-bench]` | ANAN-G2 with 2 receivers | Setup → General → Options → check "Prevent TX'ing on a different band to the RX band". RX1 on 20m, attempt to TX on 40m (or via VFO B on a different band). | MOX rejected. Status-bar TX INHIBIT visible (source: OutOfBand). | |
| 9 | Per-board PA scaling vs watt meter `[3M-1a-bench]` | ANAN-G2 + 100W dummy load + RF watt meter | Dial up 50W carrier on 14.200 MHz, read PA Power gauge against bench watt meter. Repeat at 100W. Compare to `tst_pa_scaling.cpp` reference values for ANAN_G2 (bridgeVolt=0.12, refVoltage=5.0, offset=32). | Gauge readings agree with bench meter within 5% across 25-100W range. | |
| 10 | HighSWR static overlay paints `[3M-1a-bench]` | any radio with TX, plus a deliberate mismatch | Force a high-SWR condition during TX (per row 2 or 3). | "HIGH SWR" red bold text appears in upper-center of spectrum. 6 px red border draws around the spectrum. With foldback active, "POWER FOLD BACK" appears under "HIGH SWR". | |
| 11 | RX-only SKU hard-blocks MOX entry | Hermes-Lite 2 (RX-only kit, if available) OR ANAN-G2 with chkGeneralRXOnly toggled | Setup → General → Hardware Configuration → check "Receive Only" (or use an HL2 RX-only kit so `BoardCapabilities::isRxOnlySku()` is true). Try MOX. | MOX rejected. Status-bar TX INHIBIT visible (source: Rx2OnlyRadio). | |
| 12 | PA-trip clears MOX automatically on Andromeda console | Andromeda + Ganymede 500W amp (skip on non-Andromeda) | While transmitting, induce a Ganymede CAT trip message (e.g. drive over rated, or mock by sending a CAT TripState=1 frame). | `RadioModel::paTripped()` asserts. Status-bar PA Status badge flips to "PA FAULT" (red). MOX drops automatically (`m_transmitModel.setMox(false)` per Andromeda.cs:920). | |
| 13 | Per-MAC persistence round-trip | any | Open Setup → Transmit + General. Toggle every 3M-0 control. Quit the app. Reopen. | All 15 AppSettings keys preserved exactly as set. (Automated coverage: `tst_phase3m0_persistence_audit`.) | |

## Regressions to watch

- TX path itself doesn't exist yet (3M-1a is next), so there should be **zero observable change** to the existing RX path: spectrum, waterfall, audio, VFO tuning, AGC, NR, NB, AGC, mode switching, band switching, multi-RX, FFT, panadapter — all unchanged.
- Existing setup pages (Display, Audio, CAT, Calibration, Antenna Control, etc.) — no controls changed, no defaults shifted.
- ADC overload, step attenuator, AlexController, BandStackManager — none touched by 3M-0.
- Existing 130-test suite from pre-3M-0 baseline must still pass cleanly (verified by full ctest run).

## Upstream cite traceability

Every safety controller carries verbatim Thetis cites with `[v2.10.3.13]` stamps:

- **SwrProtectionController** ports `console.cs:25933-26120` (PollPAPWR loop), `25972-25978` (SWR formula), `25989-26009` (open-antenna heuristic, `[v2.10.3.6]MW0LGE`), `26020-26057` (tune-time bypass), `26069-26075` (trip detection), `29191-29195` (UIMOXChangedFalse reset).
- **TxInhibitMonitor** ports `console.cs:25801-25839` (PollTXInhibit loop) with `//DH1KLM` per-board pin tag preserved verbatim.
- **BandPlanGuard** ports `console.cs:6770-6816` (CheckValidTXFreq), `console.cs:2643-2669` + `clsBandStackManager.cs:1467-1480` (60m channels), `clsBandStackManager.cs:1063-1083` (IsOKToTX broad-range gate, called out as a deliberate NereusSDR deviation in source comments — channelized US 60m gate is more restrictive per FCC Part 97.303(h)).
- **safety_constants** ports `console.cs:25008-25072` (computeAlexFwdPower per-board switch, including `//DH1KLM` REDPITAYA tag).
- **RadioModel::handleGanymedeTrip** ports `Andromeda.cs:855-866` + `Andromeda.cs:914-948` (CATHandleAmplifierTripMessage), with `//G8NJJ` tag preserved.
- **setWatchdogEnabled** is a stub for `NetworkIOImports.cs:197-198` (DllImport into closed-source ChannelMaster.dll); wire bit position deferred to 3M-1a per row 5.

## Commit sequence

3M-0 landed across the following commits on `feature/phase3m-0-pa-safety`:

| Task | Commits | Summary |
|---|---|---|
| 1 | `4fb4a79` + `46f246b` | BoardCapabilities — `isRxOnlySku()` + `canDriveGanymede()` |
| 2 | `2c414f8` + `fa7a001` + `bdd86e6` | BandPlanGuard + 60m channelization (3 commits incl. attribution + review fixups) |
| 3 | `921f25e` + `59443e8` + `5dfe9c5` + `33f8755` (partial) | SwrProtectionController two-stage foldback + windback latch |
| 4 | `6927f45` + `e357aab` + `33f8755` (partial) | TxInhibitMonitor 100 ms poll + 4-source dispatch |
| 5 | `ecfe93d` | `setWatchdogEnabled(bool)` stub |
| 6 | `eb37327` | `RadioModel::paTripped()` + Ganymede trip handler |
| 7 | `29fd3d8` | Per-board PA scaling + MeterPoller routing |
| 8 | `637fb20` | HighSwr static overlay |
| 9 | `48e8e35` | Setup → Transmit → SWR Protection group |
| 10 | `2cce850` | Setup → Transmit → External TX Inhibit group |
| 11 | `30c1411` | Setup → Transmit → Block-TX antennas + Disable HF PA |
| 12 | `19fcbe4` | Setup → General → Hardware Configuration |
| 13 | `4b02a6e` | Setup → General → Options → prevent-different-band |
| 14 | `fa4f9aa` | Status bar TX Inhibit + PA Status badge widgets |
| 15 | `8c2bf41` | Persistence audit (15 keys round-trip) |
| 16 | (this doc) | Verification matrix |
| 17 | (TBD) | Final integration — RadioModel owns safety controllers |

## Notes

- **Bench rows tagged `[3M-1a-bench]` are not blocking 3M-0 merge.** They form the acceptance criteria for the combined 3M-0 + 3M-1a release.
- **All non-bench rows (1, 6, 13) are automated** and gate the 3M-0 PR via CI's `ctest` step.
- **Row 5 (watchdog wire bit)** is a known stub; the wire format identification (capture or HL2 firmware analysis) is a planned 3M-1a task. **Resolved in 3M-1a Task E.5**: P1 RUNSTOP `pkt[3]` bit 7 (HL2 fw `dsopenhpsdr1.v:399-400 [@7472bd1]`); P2 deferred per pre-code §7.8.
- **Row 12 (Andromeda PA-trip)** requires Andromeda hardware + Ganymede PA; if no such hardware is available at bench time, the unit-test coverage in `tst_radio_model_pa_tripped::ganymedeTripMessage_dropsMoxIfActive` substitutes for hardware verification of the MOX-drop side effect.

---

# Phase 3M-1a TUNE-only First RF — Verification Matrix Extension

3M-1a builds on the 3M-0 safety net to fire the first MOX wire-bit on real
hardware. This section adds rows specific to the TUNE path (the only TX-on
path enabled in 3M-1a) plus the wire-byte snapshots verified in unit tests.

Recommended bench: **HL2 first** (lower-stakes smoke, RUNSTOP watchdog wire bit
verifiable, no PA chain), then **ANAN-G2** (Saturn FPGA, full P2 wire path,
includes external watt meter for power calibration).

| # | Test | Hardware | Procedure | Expected | Result |
|---|---|---|---|---|---|
| 14 | P1 MOX wire-byte snapshot | none | `cd build && ctest -R '^tst_p1_mox_wire$' -V` | All 13 cases PASS — C0 byte 3 bit 0 (0x01) emits `1` on MOX-on, `0` on MOX-off; bank-0 force-flush within ≤1 frame; HL2 firmware cross-check. | |
| 15 | P1 T/R relay wire-byte snapshot | none | `cd build && ctest -R '^tst_p1_trx_relay_wire$' -V` | All 14 cases PASS — bank 10 C3 bit 7 inverted: `1` = relay disabled (PA bypass), `0` = engaged. Codec path + legacy path both verified. | |
| 16 | P1 TX I/Q frame layout | none | `cd build && ctest -R '^tst_p1_tx_iq_wire$' -V` | All cases PASS — 16-bit signed BE samples in EP2 zones; HL2 CWX LSB-clear workaround active; SPSC ring saturation behaviour. | |
| 17 | P1 watchdog RUNSTOP byte | none | `cd build && ctest -R '^tst_p1_watchdog_wire$' -V` | All 12 cases PASS — `pkt[3]` bit 7 (RUNSTOP byte 1 bit 7); inverted (`1` = disabled, default-on writes `0`); both sendMetisStart and sendMetisStop carry the bit. | |
| 18 | P2 MOX wire-byte snapshot | none | `cd build && ctest -R '^tst_p2_mox_wire$' -V` | All 10 cases PASS — high-priority byte 4 bit 1 (0x02) for MOX; codec path (Saturn / OrionMkII) + legacy path; latent `pttOut→m_mox` bug fixed. | |
| 19 | P2 drive level wire-byte | none | `cd build && ctest -R '^tst_p2_drive_level_wire$' -V` | All 10 cases PASS — high-priority byte 345 carries `m_tx[0].driveLevel & 0xFF`; clamped [0,100]; out-of-band gate zeros byte 345 for GEN/WWV bands. | |
| 20 | P2 TX I/Q frame layout | none | `cd build && ctest -R '^tst_p2_tx_iq_wire$' -V` | All 14 cases PASS — 1444-byte frame (4-byte BE seq + 1440-byte payload), 240 samples, 24-bit signed BE I + Q (6 B/sample), float→int24 gain 8388607.0, port 1029. | |
| 21 | MoxController state machine | none | `cd build && ctest -R '^tst_mox_controller_' -V` | All 4 test files (basic, timers, phase signals, tune) PASS. State machine + 6 QTimer chains + 6 phase signals fire in correct order. | |
| 22 | RadioModel hardware-flip fan-out | none | `cd build && ctest -R '^tst_radio_model_mox_hardware_flip$' -V` | All 8 cases PASS — `onMoxHardwareFlipped` slot calls Alex routing → setMox → setTrxRelay in Thetis-§2.3 order. | |
| 23 | RadioModel TUN orchestrator | none | `cd build && ctest -R '^tst_radio_model_set_tune$' -V` | All 18 cases PASS — `setTune(true/false)` with CW→LSB/USB swap, tune-tone sign select, tune-power push, MoxController engage; cold-off guard prevents stale state restore. | |
| 24 | TxChannel WDSP wiring | none | `cd build && ctest -R '^tst_tx_channel_' -V` | All 4 test files (pipeline, tune-tone, running, wdsp-engine) PASS. 31-stage TXA pipeline; gen1 PostGen tone setup; channel ID 1. | |
| 25 | StepAttenuatorController TX-path | none | `cd build && ctest -R '^tst_step_att_tx_path$' -V` | All 12 cases PASS — `applyTxAttenuationForBand`, `shouldForce31Db` (PS-off OR CWL/CWU), HPSDR `saveRxPreampMode` / `restoreRxPreampMode`. | |
| 26 | TUN engages MOX (HL2 bench) `[bench]` | HL2 + 50Ω dummy load | Tune-Power slider at 25W (low). Press TUNE button. | TUNE button turns red. MOX status badge active. Spectrum overlay shows TX-mode tint. RUNSTOP packet on wire shows bit 7 = 0 (watchdog enabled). After release, TUNE button clears, MOX drops, watchdog state stable. | |
| 27 | TUN tone audible on HL2 `[bench]` | HL2 + dummy load + 100W watt meter | Slice in USB at 14.200 MHz. Tune-Power 25W. Press TUNE. | Watt meter shows ~25W carrier. Tone is at +cw_pitch (600 Hz USB → carrier at 14.200600 MHz). Drive level on dashboard = 25. | |
| 28 | CW→LSB swap during TUN `[bench]` | HL2 | Slice in CWL at 7.050 MHz. Press TUNE. | Slice mode briefly switches to LSB during TUN. Tone at -cw_pitch (600 Hz LSB → carrier at 7.049400 MHz). Release TUN: slice restores to CWL. | |
| 29 | Per-band tune-power persistence `[bench]` | HL2 | Set 20m tune power = 30W. Set 40m tune power = 15W. TUN on 20m (verify 30W). Switch to 40m, TUN (verify 15W). Quit + relaunch app. | Tune-Power slider on each band reads back the saved value after relaunch. | |
| 30 | ATT-on-TX activates on TUN `[bench]` | HL2 with chkATTOnTX checked | Setup → Transmit → check "ATT on TX" + "Force 31dB when PS off". TUN on 20m. | RX dashboard shows step ATT 31dB during TUN; releases on TUN-off. | |
| 31 | TUN refused without connection `[bench]` | HL2 | Disconnect radio. Press TUNE button. | Status-bar message "Power must be on to enable Tune". Button auto-unchecks. No MOX engaged. | |
| 32 | First RF on ANAN-G2 `[bench]` | ANAN-G2 + 200W dummy load + watt meter | Tune-Power 25W on 20m. Press TUNE. | Watt meter ~25W carrier on 14.200 MHz. P2 high-pri byte 4 bit 1 = 1 during MOX. P2 high-pri byte 345 = 25. TX I/Q on UDP port 1029 at 24-bit BE. | |
| 33 | ANAN-G2 ALC meter live during TUN `[bench]` | ANAN-G2 | Press TUNE at 50W. | ALC meter shows non-zero deflection during TUN. MeterPoller switches RX→TX poll set on MOX engage. | |
| 34 | ANAN-G2 SWR foldback during TUN `[bench]` | ANAN-G2 + mismatch jig (3:1 SWR) | TUN at 50W into mismatch. | SwrProtectionController trips after 4 trip-debounce samples. Drive collapses to ~1%. HighSWR overlay paints. (3M-0 row 2 + row 10 now actionable.) | |
| 35 | RX-only SKU refuses TUN `[bench]` | HL2 RX-only kit OR ANAN-G2 with chkGeneralRXOnly | Check Receive Only. Press TUNE. | TUN button auto-unchecks. Status-bar TX INHIBIT (Rx2OnlyRadio source). MOX never engages, no wire bytes emit. | |
| 36 | Reconnect cycle without leak `[bench]` | any | Connect radio → TUN → release → disconnect → reconnect → TUN → release. | TUN works identically on second connect cycle. No duplicated channel-creation logs (G.1 fixup verifies WDSP-init lambda doesn't accumulate). | |

## Regressions to watch (additional for 3M-1a)

- **RX path during MOX off** must remain unchanged: spectrum, waterfall,
  audio, VFO, AGC, NR, NB — all of 3M-0's "zero observable change" rule
  still applies when MOX is not engaged.
- **MOX-off → MOX-on → MOX-off** must leave no residual state: AlexController
  antenna selection, step attenuator, drive level, slice DSP mode all
  return to pre-TUN values.
- **`m_tunePowerByBand` persistence** must not duplicate or conflict with
  3G-13/3M-0 keys (verified by code review; keys live under
  `hardware/<mac>/tunePowerByBand/<idx>` per G.3).

## 3M-1a commit sequence

3M-1a landed on `feature/phase3m-1a-tune-only-first-rf` across these commits
(in addition to A.1, A.2 docs and B.*, C.*, D.* foundation work that
preceded the resume from `5c7015a`):

| Phase | Commits | Summary |
|---|---|---|
| E.3 | `5c7015a` | P1 setMox wire emission (C0 byte 3 bit 0) |
| E.4 | `0767d40` + `41ddf3b` | P1 setTrxRelay wire emission (bank 10 C3 bit 7) + codec polarity propagation |
| E.5 | `3425a84` + `e800c92` | P1 setWatchdogEnabled (RUNSTOP pkt[3] bit 7) + comment polish |
| E.6 | `867c727` + `0b557e9` | P2 sendTxIq SPSC ring (port 1029, 24-bit BE) + keep-in-sync comment |
| E.7 | `048f81c` + `37aaaef` | P2 setMox + drive level + latent pttOut→m_mox bug fix + cleanups |
| E.8 | `43a29e4` | P2 setWatchdogEnabled stub deferral note |
| F.1 | `1a84e58` + `5fb42ca` | RadioModel onMoxHardwareFlipped fan-out + queued-dispatch fix |
| F.2 | `f5a5d4a` + `8726aba` | StepAttenuatorController TX-path activation + threading + HPSDR setter wiring |
| F.3 | `a6fd1b9` + `fd08901` + `f2a1c09` | SwrProtectionController carry-forward TODOs + K2UE tag + clamp |
| G.1 | `7e349df` + `b156465` | RadioModel ownership of MoxController + TxChannel + TxMicRouter + lambda accumulation fix |
| G.2 | `56d17ce` + `8a4f794` | Receive-Only checkbox visibility from caps + sibling-field reset |
| G.3 | `d231a00` + `e494a86` + `15a4d5a` | TransmitModel tunePowerByBand[14] + per-MAC persistence + author tag + drop s.save() |
| G.4 | `d07b51c` + `03f7467` | RadioModel::setTune TUN orchestrator + cold-off guard |
| H.1 | `441dcbc` | SpectrumWidget MOX overlay wiring |
| H.2 | `0961301` | MeterPoller TX bindings on MOX |
| H.3 | `d11802e` | TxApplet TUN/Tune-Power/RF-Power/MOX activation |
| H.4 | `555d85d` + `48fa8d6` | PowerPaPage Power/TunePwr/ATTOnTX/ForceATT activation + QPointer + band wire + ATT tests |
| I.1 | (full ctest sweep) | 168/168 tests green |
| I.4 | (this matrix update) | Verification matrix extension |
| I.5 | (TBD) | Post-code Thetis review §2 |

---

# Phase 3M-1b Mic + SSB Voice — Verification Matrix Extension

Added 2026-04-28 as part of M.6. Manual rows tagged `[3M-1b-bench-*]` need
hardware (HL2 + ANAN-G2 dummy load + wireshark) and are deferred to JJ.
Unit-test rows are auto-checked by ctest and pass on commit `26eca01`
(221/221).

## New rows

| # | Test | Hardware | Procedure | Expected | Result |
|---|---|---|---|---|---|
| 14 | BandPlanGuard SSB-mode allow-list | none | `ctest -R '^tst_band_plan_guard_mode_allow_list$' -V` | All 20 slots pass: LSB/USB/DIGL/DIGU allowed; AM/SAM/DSB/FM/DRM rejected with "AM/FM TX coming in Phase 3M-3 (audio modes)"; CWL/CWU rejected with "CW TX coming in Phase 3M-2"; SPEC rejected. | ✅ |
| 15 | BandPlanGuard MOX rejection toast `[3M-1b-bench]` | any radio | Tune to LSB → MOX → "MOX engaged" (success). Switch to AM → click MOX → expect rejected; status-bar toast shows "AM/FM TX coming in Phase 3M-3 (audio modes)" for ~3s; TxApplet MOX button tooltip overrides to the same string. | Toast appears, tooltip overrides; m_mox stays false. | |
| 16 | VOX with mic-boost-aware threshold scaling | none | `ctest -R '^tst_mox_controller_vox_threshold$' -V` | All 14 slots pass: scaled formula `pow(10, dB/20.0) * voxGainScalar` when micBoost==true; passthrough when false. NaN sentinel forces first-call emit. | ✅ |
| 17 | VOX voice-family mode-gate | none | `ctest -R '^tst_mox_controller_vox_enabled$' -V` | All 22 assertions pass: LSB/USB/DSB/AM/SAM/FM/DIGL/DIGU enabled VOX → setVoxRun(true) emit; CWL/CWU/SPEC/DRM enabled VOX → no emit. | ✅ |
| 18 | Anti-VOX path-agnostic | none | `ctest -R '^tst_mox_controller_anti_vox$' -V` | All 18 cases pass: useVax=false → antiVoxSourceWhatRequested(false) emit; useVax=true → qCWarning + state unchanged + no emit. | ✅ |
| 19 | PTT-source dispatch (MIC/CAT/VOX/SPACE/X2) | none | `ctest -R '^tst_mox_controller_ptt_source_dispatch$' -V` | All 19 cases pass: 5 accepted sources × press/release transition with PttMode set; 2 rejected sources (CW/TCI) → qCWarning + no state change. | ✅ |
| 20 | mic_ptt extraction P1 + P2 | none | `ctest -R '^tst_mox_controller_mic_ptt_extraction$' -V` | All 18 cases pass: P1 OR-across-sub-frames bit 0 of C0; P2 raw[4] bit 0; ADC overload byte (raw[5]) does NOT cross-trigger PTT; end-to-end status frame → MoxController dispatch. | ✅ |
| 21 | TransmitModel per-MAC persistence | none | `ctest -R '^tst_transmit_model_persistence$' -V` | All 42 cases pass: 15 persisted keys round-trip; voxEnabled/monEnabled/micMute NOT persisted (safety: load to safe defaults); micGainDb defaults -6 first run; multi-MAC isolated. | ✅ |
| 22 | RadioModel HL2 force-Pc on connect | none | `ctest -R '^tst_radio_model_mic_source_hl2_force$' -V` | All 8 cases pass: HL2 caps force micSource=Pc + lock; non-HL2 unlocked; locked setter coerces Radio→Pc. | ✅ |
| 23 | RadioModel 3M-1b ownership wiring | none | `ctest -R '^tst_radio_model_3m1b_ownership$' -V` | All 15 cases pass: PcMicSource + RadioMicSource + CompositeTxMicRouter constructed on connect; 5 signal connections wired; MoxCheck callback installed. | ✅ |
| 24 | PC mic SSB out `[3M-1b-bench-HL2]` | HL2 + dummy load + USB headset/mic + test receiver | Setup → Audio → TX Input. Confirm Radio Mic radio button hidden + tooltip "Radio mic jack not present on Hermes Lite 2". Confirm PC Mic selected by default. Tune to 7.241 MHz LSB. Configure PC mic device. Run Test Mic → see VU bar move. Set Mic Gain to -6 dB. PTT via TxApplet MOX button → speak → observe SSB carrier on test receiver. Verify TxApplet TxMic + ALC meters paint live values. Confirm clean release on MOX off. | Voice on the air via test receiver; meters live; clean MOX↔Rx transitions. | |
| 25 | PC + Radio mic switching `[3M-1b-bench-G2]` | ANAN-G2 + dummy load + USB headset + radio mic | Tune to 7.241 MHz LSB. Configure PC mic (default), confirm SSB out. Switch to Radio Mic, configure 3.5mm jack default, plug radio mic, speak, confirm SSB out. | Both mic paths produce clean SSB; switching is < 1s; no MOX-locked-during-switch deadlock. | |
| 26 | Mic-jack bits wireshark cross-check `[3M-1b-bench-G2]` | ANAN-G2 + wireshark on RX/TX UDP traffic | Toggle each mic-jack control in turn (MicBoost / LineIn / MicTipRing / MicBias / MicPTT). For each, capture the next outbound P2 transmit-specific packet (port 1029) AND the next outbound P1 bank-10/bank-11 C&C frame (port 1024). | Bit positions match the deskhpsdr `new_protocol.c:1480-1502` polarity table verbatim. P1 bank-10 C2 bit 0 = mic_boost; bank-10 C2 bit 1 = line_in; bank-11 C1 bit 4 = mic_trs (inverted); bank-11 C1 bit 5 = mic_bias; bank-11 C1 bit 6 = mic_ptt (inverted). | |
| 27 | MicXlr P2 byte-50 bit 5 `[3M-1b-bench-G2]` | ANAN-G2 + XLR mic + wireshark | Switch Saturn G2 family between 3.5mm and XLR via Setup → Audio → TX Input → Radio Mic group. Capture P2 transmit-specific packet for each. | byte-50 bit 5 (0x20): set when XLR selected, clear when 3.5mm. Default-true means a fresh connection sees bit 5 set. | |
| 28 | MON enable + monitor volume `[3M-1b-bench]` | any radio + headphones | With headphones connected, enable MON via TxApplet. Set volume to 50. PTT and speak. Confirm self-audio in headphones. Move RX volume to 0 → MON audio still flows during MOX. | Self-audio audible during MOX, level scales with volume slider, independent of RX volume. Default volume on first run = 50. | |
| 29 | RX-leak fix during MOX `[3M-1b-bench]` | any radio + dummy load | With MON disabled, tune RX1 to 7.250 MHz LSB; RX2 (if available) to 14.200 MHz. Engage MOX (PTT + speak). | Active slice (RX1) RX audio silenced during MOX. Non-active slice (RX2) audio unaffected. (Was cosmetic bug in 3M-1a per row 30 carry-forward.) | |
| 30 | PTT-source dispatch bench `[3M-1b-bench]` | any radio | For each of MIC PTT (radio mic), CAT PTT (rigctl/hamlib send T command), VOX (auto-engaged on speech threshold), SPACE (UI keyboard), X2 (external TX trigger): exercise each source and confirm MoxController.pttMode reflects the right enum value, MOX engages, and TX I/Q starts flowing. | Each source independently engages + releases MOX with correct pttMode set. | |

## Carry-forward flips from 3M-1a

| Row | Change |
|---|---|
| 3M-1a "RX still plays during MOX (cosmetic)" | Flips from "deferred" → "fixed in 3M-1b". E.4 + RadioModel activeSlice gate fold-in resolves the cosmetic leak. Verified by row 29 above. |
| 3M-1a "PA telemetry meters during TUN" | Flips to "PA telemetry meters during MOX-voice TX". Same meters now exercised under voice TX (rows 24-25), not just TUN. |

## Result tracking

Rows 14, 16-23 (unit tests, 9 rows): ✅ all green on commit `26eca01` (221/221).
Rows 15, 24-30 (bench tests, 8 rows): pending JJ + hardware.

When bench rows complete, edit the Result column to `✅` with the commit SHA where the row was confirmed.

## Phase 3M-1b commit summary (added 2026-04-28 by M.6)

| Phase | Commits | Summary |
|---|---|---|
| Plan + pre-code review | `2a7a3b1` + `be2c52d` | Pre-code Thetis review + implementation plan |
| B.1-B.2 | `01fb507` + `4828d6d` | BoardCapabilities::hasMicJack + deskhpsdr provenance |
| C.1-C.5 | `c9700a7` + `9c6b4f1` + `5c29049` + `d6258b9` + `9e6fec8` + 2 fixups | TransmitModel mic gain + 8 mic-jack flags + 4 VOX + 2 anti-VOX + 2 MON props |
| D.1-D.7 | `db1fbd9` + `c900b88` + `372489c` + `33fe64b` + `940c7e7` + `24a9f2e` + `b4ad655` + 3 fixups | TxChannel mic-router + per-mode TXA + VOX/anti-VOX wrappers + stage Run + Sip1 signal + mic-mute + meters |
| E.1-E.4 | `0765bb0` + `27408aa` + `9ec4a9a` + `7ed6710` | AudioEngine pullTxMic + monitor enable/volume + slot + RX-leak gate |
| F.1-F.4 | `d39135b` + `f64a2d3` + `83f8625` + `ef588ec` + 1 fixup | PcMicSource + RadioMicSource (SPSC ring) + CompositeTxMicRouter + RadioConnection::micFrameDecoded |
| G.1-G.6 | `46d93f6` + `b3e63fb` + `4cc76a4` + `ce160da` + `c6b0712` + `dfd5393` | 6 mic-jack wire-bit setters byte-exact ported from Thetis P1 + deskhpsdr P2 |
| H.1-H.5 | `2b36db8` + `bd038ef` + `5d40a3d` + `81c6002` + `6829f5a` + `efd7ebc` | MoxController VOX + anti-VOX + PTT-source dispatch + mic_ptt extraction |
| I.1-I.5 | `7966a8d` + `647bae8` + `7816eed` + `c28adbb` + `910334e` + `2fe2c39` + `c787936` | AudioTxInputPage + per-family Radio Mic + mic gain per-board range + VOX integration |
| J.1-J.3 | `0ae8a9b` + `817b68b` + `c961690` + `318d422` + `047c762` | TxApplet Mic Gain + VOX toggle/popup + MON + mic-source badge |
| K.1-K.2 | `3c9c707` + `80359bb` | BandPlanGuard SSB-mode allow-list + MOX rejection toast/tooltip |
| L.1-L.3 | `64bb8fa` + `e3cedf8` + `0da0f2e` + `26eca01` | RadioModel ownership + per-MAC persistence + HL2 force-Pc lock |
| M.1 | (full ctest sweep) | 221/221 tests green on `26eca01` |
| M.6 | (this matrix update) | Verification matrix extended with 17 rows (3M-1b) |
| M.7 | (TBD) | Post-code Thetis review (`phase3m-1b-post-code-review.md`) |

---

# Phase 3M-1c Polish & Persistence — Verification Matrix Extension

Added 2026-04-29 as part of M.6.  Manual rows tagged `[3M-1c-bench-*]` need
hardware (HL2 + ANAN-G2 dummy load + USB headset/mic) and are deferred to
JJ.  Unit-test rows are auto-checked by ctest and pass on commit `c26358e`
(236/236).

## New rows

| # | Test | Hardware | Procedure | Expected | Result |
|---|---|---|---|---|---|
| 31 | TransmitModel two-tone properties (B.2) | none | `ctest -R '^tst_transmit_model_two_tone_properties$' -V` | All 36 cases pass: 7 default values (Freq1=700, Freq2=1900, Level=-6 dB, Power=50%, Freq2Delay=0, Invert=true, Pulsed=false) match Thetis Designer + option C decisions; round-trip + idempotent + range clamping for all 5 numeric properties; constants match Thetis-Designer-derived ranges. | ✅ |
| 32 | TransmitModel two-tone drive-power-source enum (B.3) | none | `ctest -R '^tst_transmit_model_two_tone_drive_origin$' -V` | All 16 cases pass: 3-value enum (DriveSlider=0, TuneSlider=1, Fixed=2) ports verbatim from Thetis enums.cs:456-461 [v2.10.3.13]; default DriveSlider matches console.cs:46553; toString/fromString helpers cover all values + unknown-string fallback. | ✅ |
| 33 | TransmitModel persistence covers two-tone keys (B.2 + B.3) | none | `ctest -R '^tst_transmit_model_persistence$' -V` | All 60+ cases pass: 15 mic/VOX/MON keys (Thetis column names) + 7 two-tone keys + 1 drive-power enum key all round-trip per-MAC. First-launch defaults match the F.5 default-profile table. | ✅ |
| 34 | MoxController multicast Pre/Post signals (C.2-C.4) | none | `ctest -R '^tst_mox_controller_multicast_signals$' -V` | All 9 cases pass: moxChanging fires before m_mox state change; moxChanged 3-arg fires after timer-walk completion alongside the existing moxStateChanged(bool); int rx semantic = (rx2_enabled && vfobTx) ? 2 : 1 (4 truth-table cells covered); idempotent guard suppresses both Pre and Post on same-value setMox; verbatim MW0LGE_21k8 + MW0LGE_21a author tags preserved per CLAUDE.md inline-tag rule. | ✅ |
| 35 | AudioEngine 720-sample mic block accumulator (D.1/D.2) | none | _superseded by row 53_ — D.1/D.2 deleted by the 2026-04-29 TX pump architecture redesign; tst_audio_engine_mic_block_ready dropped. See `phase3m-1c-tx-pump-architecture-plan.md` §5. | n/a (superseded) |
| 36 | TxChannel push-driven refactor (E.1) | none | `ctest -R '^tst_tx_channel_push_driven$\|^tst_tx_channel_no_zero_fill$' -V` | All cases pass: driveOneTxBlock(samples, frames) slot signature; nullptr-samples silence path drives fexchange2 anyway (TUNE-tone PostGen); mismatched-frame-count push (e.g., 720 vs m_inputBufferSize=256) rejected with qCWarning; QTimer dropped (compile-time enforced via field absence); zero-fill workaround removed; no silent frames in 30 cycles of valid input. **Updated 2026-04-29:** silence-drive timer assertion dropped — that timer was deleted by the TX pump architecture redesign; silence path now covered by row 53 (TxWorkerThread zero-fills the gap). | ✅ |
| 37 | TxChannel TXA PostGen wrapper setters (E.2-E.6) | none | `ctest -R '^tst_tx_channel_tx_post_gen_setters$' -V` | All 19 cases pass: 12 wrappers (setTxPostGenMode, setTxPostGenTTFreq1/2, setTxPostGenTTMag1/2, setTxPostGenTTPulseToneFreq1/2, setTxPostGenTTPulseMag1/2, setTxPostGenTTPulseFreq, setTxPostGenTTPulseDutyCycle, setTxPostGenTTPulseTransition, setTxPostGenRun) all callable on a default-constructed TxChannel without crash; cache-and-recall pattern preserves Thetis radio.cs:3697-3771 [v2.10.3.13] partner-value semantics for the 4 paired Freq1/Freq2 + Mag1/Mag2 setters. | ✅ |
| 38 | MicProfileManager (F.1-F.6) | none | `ctest -R '^tst_mic_profile_manager$' -V` | All 18 cases pass: load/save/delete/setActive round-trip; comma-strip TCI safety on save name; verbatim Thetis "It is not possible to delete the last remaining TX profile" warning preserved; per-MAC isolation; first-launch "Default" seed with all 23 documented values; 19 deferred factory profiles correctly NOT created. | ✅ |
| 39 | VFO Flag TX badge + Phase L routing pattern (G.1/G.2) | none | `ctest -R '^tst_vfo_display_item_tx_badge$' -V` | All 9 cases pass: setTransmitting/isTransmitting round-trip; render-time pixel sample at the badge stripe matches m_rxColour (LimeGreen) when off and m_txColour (Red) when on; custom txColour honoured; routing lambda dispatches based on rx index from MoxController moxChanged 3-arg signal (rx=1→VFO-A, rx=2→VFO-B per Thetis console.cs:29677 [v2.10.3.13] semantic). | ✅ |
| 40 | Setup → Test → Two-Tone page (H.1-H.3) | none | `ctest -R '^tst_test_two_tone_page$' -V` | All 16 cases pass: page constructs without crash; 8 controls bind bidirectionally to the 8 TransmitModel two-tone properties (5 spinboxes + 2 checkboxes + 3-button radio group); QSignalBlocker prevents Model→UI feedback loops; Defaults preset sets only Freq1=700/Freq2=1900 (other 6 properties unchanged); Stealth preset sets Freq1=70/Freq2=190; verbatim Thetis tooltips preserved on chkInvertTones + udFreq2Delay; DrivePowerSource radio group round-trips all 3 enum values. | ✅ |
| 41 | TwoToneController activation handler (I.1-I.5) | none | `ctest -R '^tst_two_tone_controller$' -V` | All 14 cases pass: power-on gate; MOX cycle-off with 200ms settle non-blocking via QTimer::singleShot (verbatim MW0LGE_21a author tag preserved); 0.49999 magnitude scaling literal preserved at TwoToneController.cpp:241; LSB/CWL/DIGL invert sign-flip; pulsed (mode=7) vs continuous (mode=1) branch with full TXPostGen setter sequences + TXPostGenTTPulseIQOut=true for pulsed; setTxPostGenRun fires last in activation, first in deactivation (after 200ms settle); BandPlanGuard rejection cleans up m_active=false via moxRejected slot; verbatim MW0LGE_22b author tag preserved on power-source switching. | ✅ |
| 42 | TxApplet profile combo + 2-TONE button (J.1/J.2) | none | `ctest -R '^tst_tx_applet_profile_combo$' -V` | All 15 cases pass: combo populated from MicProfileManager::profileNames(); selection triggers setActiveProfile via setMicProfileManager() pointer plumbing; refresh on profileListChanged + activeProfileChanged with QSignalBlocker; right-click emits txProfileMenuRequested signal; 2-TONE button toggle calls TwoToneController::setActive; controller's twoToneActiveChanged signal mirrors back to button visual (e.g., BandPlanGuard rejection un-checks the button). | ✅ |
| 43 | Setup → TX Profile editor page (J.3/J.4) | none | `ctest -R '^tst_tx_profile_setup_page$' -V` | All 16 cases pass: combo populated from MicProfileManager; Save flow with InputDialog mock + non-empty validation + overwrite confirmation; Delete flow with verbatim Thetis last-profile-guard message; focus-gated unsaved-changes Yes/No/Cancel prompt (programmatic combo changes don't trigger; user-driven dirty changes do); 23-signal dirty-flag subscription cleared on save/load. | ✅ |
| 44 | Initial-state-sync audit (K.1/K.2) | none | (no dedicated test target; covered by integration via tst_audio_engine_tx_monitor_block + tst_radio_model_3m1b_ownership) | The 2 missing pushes for AudioEngine::setTxMonitorEnabled and setTxMonitorVolume after their connect() calls in connectToRadio (mirrors the L.1 micPreamp push at 1841462) close the audit gap that was carried forward from 3M-1b. Push for monEnabled is functionally a no-op (always loads false per safety policy); push for monitorVolume is load-bearing (DOES persist; default 0.5 from audio.cs:417 [v2.10.3.13]). | ✅ |
| 45 | MicReBlocker 720→256 (L.4) | none | _superseded by row 53_ — L.4 MicReBlocker class deleted by the 2026-04-29 TX pump architecture redesign; tst_mic_re_blocker dropped. See `phase3m-1c-tx-pump-architecture-plan.md` §5. | n/a (superseded) |
| 46 | Two-tone test continuous mode `[3M-1c-bench-G2]` | ANAN-G2 + dummy load + spectrum analyser | Tune to 14.200 MHz LSB. Setup → Test → Two-Tone: confirm Freq1=700, Freq2=1900, Level=-6 dB, Power=50%, Pulsed=unchecked, Invert=checked. TxApplet 2-TONE button → engages MOX. Read spectrum analyser. | Two clean tones at +700 / +1900 Hz audio offset; no spurious products in the SSB pass-band; PA forward power ~50% of dummy-load rating; 3rd-order intermod products visible at expected −10·log10(2) below carrier. Click 2-TONE off → MOX cleanly releases. | |
| 47 | Two-tone test pulsed mode `[3M-1c-bench-G2]` | ANAN-G2 + dummy load + spectrum analyser | Same as row 46 but check Pulsed=true. Engage 2-TONE. | Pulsed envelope visible on spectrum analyser at the configured pulse rate; carrier still on Freq1+Freq2 audio tones; pulse cadence audible. | |
| 48 | Two-tone test on LSB-family invert `[3M-1c-bench-G2]` | ANAN-G2 + spectrum analyser | Tune to LSB → engage 2-TONE → check Invert=true → tones land at +Freq1/+Freq2 (audio band positive). Toggle Invert=false → tones flip to −Freq1/−Freq2 (mirrored). | Invert toggle flips spectral position of the two tones in LSB mode only. USB mode shows no change on invert toggle. | |
| 49 | Two-tone test BandPlanGuard rejection `[3M-1c-bench]` | any radio + dummy load | Tune to CW (CWL or CWU). Click 2-TONE button. | BandPlanGuard rejects the activation (toast message: "CW TX coming in Phase 3M-2"); 2-TONE button visually un-checks (controller emitted twoToneActiveChanged(false) on the moxRejected slot). | |
| 50 | Profile save/load/restart-app round-trip `[3M-1c-bench-G2]` | ANAN-G2 | Setup → TX Profile → save profile "BenchA" with non-default mic gain + VOX threshold + monitor volume. Quit app. Relaunch. Setup → TX Profile → switch to "BenchA". Verify all 23 keys restored. | All values from BenchA propagate to TransmitModel; meters reflect the loaded state; subsequent SSB transmission uses the loaded mic gain. | |
| 51 | Mic-jack flag persistence in profile `[3M-1c-bench-G2]` | ANAN-G2 | Save profile "MicJackTest" with non-default Mic_Input_Boost + Mic_XLR + Line_Input_On + Mic_Bias + Mic_TipRing. Switch to Default. Switch back to MicJackTest. Wireshark cross-check the radio-side P2 byte-50 + P1 bank-10/11 frames before and after. | Mic-jack bit positions in the wire frames change only when the profile-load completes, NOT mid-load (atomic). After load, the saved bits are visible on the wire. | |
| 52 | VFO Flag TX badge live `[3M-1c-bench]` | any radio + dummy load | Engage MOX (PTT or 2-TONE). Watch the VFO Flag widget on the panadapter. | Left-edge badge stripe colour changes from m_rxColour (LimeGreen) to m_txColour (Red) at the moment moxChanged fires (after the timer-walk completes), and back to LimeGreen at MOX off. | |
| 53 | TX pump architecture redesign — TxWorkerThread (replaces D.1/E.1/L.4) | none | `ctest -R '^tst_tx_worker_thread$' -V` | All 8 cases pass: lifecycle (construct/destruct without start; startPump-without-deps warns + stays stopped; start→isRunning==true→stop→isRunning==false); single-tick dispatch (full 256-frame bus block → 1 sendTxIq with lastN=256; empty bus → silence path zero-fills inI buffer + still drives sendTxIq; partial bus 100/256 → zero-fills bytes 100-255 in inI); real-worker cadence (≥ 2 ticks observed in 100 ms parallel-run safe lower bound); cross-thread setter race (100 setMicPreamp calls from main while pumping → no crash + final value 0.99 lands). Replaces 3M-1c rows 35 (D.1/D.2) + 45 (L.4). | ✅ |
| 54 | TX pump v3 (semaphore-wake, fexchange0, 64-block, source-first Thetis cmbuffs.c) | none | `ctest -R '^(tst_tx_worker_thread\|tst_tx_mic_source\|tst_p1_mic_extraction\|tst_p2_mic_frame)$' -V` | All passing on `def6ef7` — 13 sub-tests in `tst_tx_worker_thread` (was 11 in v2); 2 new C1 regression-trap cases (`crossThreadQueuedDelivery_micPreampSlotFires` / `voxRunLambdaFires`) verify queued cross-thread setter delivery via `QCoreApplication::sendPostedEvents(m_txChannel, 0)` at the top of every wake; live-trap verification: removing `sendPostedEvents` causes both new cases to fail with "Compared values are not the same". TxMicSource (8 cases): lifecycle + semaphore math + ring wrap + partial-block + poison release + SPSC concurrency. P1 EP6 mic16 extraction (HL2-zeros yields zero samples). P2 port-1026 132-byte frame (4-byte BE seq + 64×16-bit BE signed payload, seq-error case). 238/238 baseline + 1 new wire test (row 55) = 239/239 on `def6ef7`. Replaces row 53 (v2 QTimer-driven implementation deleted). | ✅ |
| 55 | P1 setTxDrive wire-byte (HL2 bench triage) | none | `ctest -R '^tst_p1_drive_level_wire$' -V` | All 12 cases pass: bank 10 C1 carries `qBound(0, level, 255)` from `setTxDrive(int)`; default state writes 0; mid-range / max / zero / above-range / below-range clamp on both `P1CodecStandard` (HermesII) and `P1CodecHl2` (HermesLite) paths; force-bank-10 flush flag set on every state-changing call; idempotent calls are no-ops. Bug fix: prior 3M-1a Task 7 `setTxDrive` was a no-op stub; `m_txDrive` stayed at 0 forever; HL2 / Atlas / Hermes / HermesII / Angelia / Orion all shipped with zero TX drive level → no RF on TUN or SSB.  Latent because PR #149 SSB voice bench tested only on ANAN-G2 (P2). | ✅ |
| 56 | RadioModel teardown setTxMicSource race (Codex P1) | none | (no dedicated test; covered by integration via `tst_radio_model_3m1b_ownership` + future disconnect/reconnect-cycle test) | `RadioModel::teardownConnection` previously called `p1/p2->setTxMicSource(nullptr)` directly from RadioModel's thread (main) while `m_connection` lived on `m_connThread` after the line-1842 `moveToThread` — unsynchronized cross-thread write to `m_txMicSource` racing with reads in `onReadyRead`/watchdog/`decodeMicFrame132`. Codex P1 review on PR #152.  Fix: marshal via `QMetaObject::invokeMethod(conn, lambda, Qt::BlockingQueuedConnection)` so the detach completes before `m_txMicSource->stop() + reset()` runs.  Same-thread fast-path guard for completeness (no production callsite hits it today). | ✅ |
| 57 | Two-tone level dB→linear (Codex P2) | none | (no dedicated test; covered by integration row 46 / row 47 spectrum-analyser checks) | The `twoToneLevelChanged` lambda + initial-state pushes were forwarding raw user-facing dB (e.g. -6) into `setTxPostGenTTMag*`, which expects linear amplitude in [0, 0.49999].  Codex P2 review on PR #152.  Fix: apply verbatim Thetis `0.49999 * pow(10.0, db/20.0)` per `setup.cs:11056 [v2.10.3.13]` to BOTH the level-changed lambda AND the initial-state push block.  The 0.49999 literal is preserved verbatim per CLAUDE.md "Constants and Magic Numbers".  Bench-verify on G2 spectrum analyser (rows 46 / 47). | ✅ |
| 58 | TUN engages MOX + RF on HL2 `[3M-1c-bench-HL2]` | HL2 + 50Ω dummy load + watt meter | Tune-Power slider at 25W (mid).  Tune to 7.150 MHz LSB.  Press TUNE button. | TUNE button turns red.  MOX badge active.  Watt meter reads ~25W carrier on the wire.  Bank 10 C1 byte = 25 (or scaled drive value, depending on slider mapping).  Release TUN: button clears, MOX drops, watt meter returns to ~0. | |
| 59 | SSB voice TX on HL2 `[3M-1c-bench-HL2]` | HL2 + 50Ω dummy load + watt meter + USB headset / mic + test receiver on a separate radio | Setup → Audio → TX Input.  Confirm Radio Mic radio button hidden + tooltip "Radio mic jack not present on Hermes Lite 2".  Confirm PC Mic selected by default.  Tune to 7.241 MHz LSB.  Configure PC mic device.  Run Test Mic → see VU bar move.  Set Mic Gain to -6 dB.  PTT via TxApplet MOX button → speak → observe SSB carrier on test receiver. | Voice on the air via test receiver; meters live; clean MOX ↔ Rx transitions.  No stutter.  Mid-TX mic preamp slider lands in real time (the C1 fix).  VOX button non-crashing. | |
| 60 | Two-tone test on HL2 `[3M-1c-bench-HL2]` | HL2 + dummy load + spectrum analyser | Same procedure as row 46 (G2) but on HL2.  Verifies the Codex P2 dB→linear conversion fix on a non-G2 board.  Watt meter reads ~50% of dummy-load rating; spectrum analyser shows two clean tones at +700 / +1900 Hz audio offset; 3rd-order intermod products at expected −10·log10(2) below carrier. | Two clean tones; correct magnitude (NOT muted, which is the pre-fix symptom). | |

## Carry-forward flips from 3M-1b

| Row | Change |
|---|---|
| 3M-1b row 17 (VOX voice-family mode-gate) | Stays ✅; 3M-1c didn't touch the VOX mode-gate. |
| 3M-1b row 18 (anti-VOX path-agnostic) | Stays ✅; 3M-1c didn't touch the anti-VOX path. |
| 3M-1b row 21 (TransmitModel persistence) | Expanded — 3M-1c B.1 renamed 15 keys to Thetis column names; B.2/B.3 added 8 more persisted keys. tst_transmit_model_persistence now covers 23 keys total. |
| 3M-1b row 24 (PC mic SSB out) | Carries forward — superseded by the 2026-04-29 TX pump architecture redesign. The current TX pump is `TxWorkerThread::onPumpTick` (QTimer @ 5 ms) → `AudioEngine::pullTxMic` (256-sample drain, no accumulator) → `TxChannel::driveOneTxBlock` (256 end-to-end). Bench-verify TX path still produces clean SSB at low / normal mic gain (was gravelly under the deleted bench-fix-A/B chain). |

## Result tracking

Rows 31-45 (unit tests, 15 rows): ✅ all green on commit `c26358e` (236/236
in that snapshot).  Rows 35 (D.1/D.2) and 45 (L.4) marked **superseded**
on 2026-04-29 by the TX pump architecture redesign — both class targets
were deleted; coverage moved to row 53 (TxWorkerThread).
Rows 46-52 (bench tests, 7 rows): pending JJ + hardware.
Row 53 (TxWorkerThread v2 unit test, added 2026-04-29): ✅ green on
commit `2a8099c` — 235/235 total tests passing under `-j8` parallel
runs.  (v2 implementation now superseded by row 54 v3.)
Row 54 (TX pump v3 — semaphore-wake + fexchange0 + 64-block + Stage-2
review): ✅ green on commit `def6ef7` — 13 sub-tests in
`tst_tx_worker_thread` (was 11), 238/238 total under `-j8`.
Row 55 (P1 setTxDrive wire-byte, HL2 bench triage): ✅ green on commit
`78f74a3` — 12 sub-tests, 239/239 total.
Row 56 (Codex P1 setTxMicSource teardown race): ✅ on commit `1b5a166`
— integration coverage; 239/239 total still green.
Row 57 (Codex P2 two-tone dB→linear): ✅ on commit `db6c93b` — formula
correction; 239/239 total still green; bench-verify pending row 60.
Rows 58-60 (HL2 bench tests, 3 rows): pending JJ + hardware (HL2 +
dummy load + watt meter; rows 59 / 60 also need USB mic + test
receiver / spectrum analyser respectively).

When bench rows complete, edit the Result column to `✅` with the commit SHA where the row was confirmed.

## Phase 3M-1c commit summary (added 2026-04-29 by M.6)

| Phase | Commits | Summary |
|---|---|---|
| Pre-code (A) | `81ff57e` + `a8fb2d3` + `a79a158` + `cf93ab6` + `69c4054` + `45ae619` + `4a17b05` + `81ccb7e` | Design spec + chunk 0 HL2 desk-review + 2 absorbed safety fixes + pre-code Thetis review + plan |
| B.1 | `bd9af67` | TransmitModel rename of 15 NereusSDR-original keys → Thetis column names |
| B.2 | `10098ed` | 7 TransmitModel two-tone properties (Freq1/Freq2/Level/Power/Freq2Delay/Invert/Pulsed) |
| B.3 | `ecc456b` | DrivePowerSource 3-value enum + TwoToneDrivePowerSource property |
| C.1 | `48a8234` | C.1 finalize — update 4 stale MoxController::moxChanged refs (rename was done earlier) |
| C.2-C.4 | `516682f` | MoxController multicast Pre/Post signals (moxChanging + moxChanged 3-arg) + int rx semantic |
| D.1/D.2 | `12130cf` | AudioEngine micBlockReady signal + clearMicBuffer slot + 720-sample accumulator |
| E.1 | `63d54a5` | TxChannel push-driven refactor (slot signature + drop QTimer + drop zero-fill workaround) |
| E.2-E.6 | `4be9e63` | 12 TXA PostGen wrapper setters with cache-and-recall partner-value pattern |
| F | `016c2ea` | MicProfileManager class (load/save/delete/setActive + Default seed + per-MAC isolation) |
| G | `bb51486` | VFO Flag TX badge wire-up (isTransmitting getter + render-path test + Phase L routing demo) |
| H | `1b6e1ad` | Setup → Test → Two-Tone page (8 properties × bidirectional sync + Defaults/Stealth presets) |
| I | `687e63d` | TwoToneController activation handler (I.1-I.5: full chkTestIMD_CheckedChanged port) |
| J | `d1df70d` | TxApplet 2-TONE button + profile combo + Setup TX Profile editor page |
| K | `5c4ab13` | Initial-state-sync audit (TX monitor enable + volume push gaps closed) |
| L | `6aefc6f` + `c26358e` | RadioModel cross-cutting wiring + 720→256 mic re-blocker (L.4) + L.2 fixup |
| M.1 | (full ctest sweep) | 236/236 tests green on `c26358e` |
| M.6 | (this matrix update) | Verification matrix extended with 22 rows (3M-1c) |
| M.7 | (post-code Thetis review) | Post-code review (`phase3m-1c-post-code-review.md`) |
| TX pump redesign | `2a8099c` | 2026-04-29 — D.1/E.1/L.4 + bench-fix-A/B chain replaced with TxWorkerThread per `phase3m-1c-tx-pump-architecture-plan.md`; rows 35 + 45 superseded by row 53; 235/235 tests green |
| TX pump v3 — Thetis-faithful semaphore-wake | `a344921` + `a20f2b9` + `287f673` + `e4d3dc8` + `33de2a3` + `07e2508` + `b5da0af` + `ae3c4b2` | 2026-04-29 — replaces v2 (QTimer / fexchange2 / 256-block).  TxMicSource = Thetis CMB Inbound/cm_main port (`cmbuffs.c:151-168 [v2.10.3.13]`); P1 EP6 mic16 zone extraction wired (was being skipped); P2 port-1026 132-byte mic frame parsing wired (was no-op `case 1`); 3 s LOS fallback per `network.c:655-666 [v2.10.3.13]`; fexchange0 + interleaved double + 64-block end-to-end (Thetis getbuffsize(48000) parity); VOX defensive guards on all 5 DEXP setters (`pdexp[ch]==nullptr` short-circuit until `create_dexp` is ported); RadioModel TxMicSource lifecycle + AudioEngine PC override gate per `cmaster.c:379 [v2.10.3.13]` `asioIN(pcm->in[stream])`. |
| TX pump v3 — Stage-2 review fixes | `4435523` + `b19edcf` + `c58250e` + `a323359` + `d39aa16` + `c7492b4` + `def6ef7` | 2026-04-29 — Stage-2 code review of v3 returned **Ready to merge with one Important fix**; six themed commits resolved: C1 cross-thread queued slot delivery (added `QCoreApplication::sendPostedEvents(m_txChannel, 0)` in run loop; refined to surgical TxChannel-targeted drain in `def6ef7`); I1 stale v2-era doc-comments refreshed across TxChannel.{h,cpp} + WdspEngine.cpp + AudioEngine.h; I2 PC-mic short-pull zero-fill (silence as correct degradation when PC is the chosen source); I3 `m_lastMicAt` arming at `setTxMicSource` (LOS injector arms even if no mic frame ever arrives); I6 AudioEngine public/public-slots regroup; cross-thread queued-delivery regression-trap test (2 new sub-cases verified to FAIL when `sendPostedEvents` is removed). |
| HL2 bench triage — P1 setTxDrive | `78f74a3` | 2026-04-29 — root cause of HL2 "no TUN, no TX" bench failure: `P1RadioConnection::setTxDrive(int)` was a no-op stub (`/* stub — Task 7 */`).  `m_txDrive` stayed at 0 forever; bank 10 C1 byte always wrote 0; HL2 / Atlas / Hermes / HermesII / Angelia / Orion all silently shipped with zero TX drive level → no RF on TUN or SSB voice TX.  Latent because PR #149 (3M-1b SSB voice) bench-tested only on ANAN-G2 (P2), whose own `setTxDrive` works.  Fix: store `qBound(0, level, 255)`, force bank 10 onto the wire on next frame.  12 new sub-tests in `tst_p1_drive_level_wire` (Standard + HL2 codec paths + clamp + force-flush flag).  Closes row 55; verified by row 58 / 59 / 60 bench tests. |
| Codex P1 — teardown setTxMicSource race | `1b5a166` | 2026-04-29 — Codex P1 review on PR #152.  `RadioModel::teardownConnection` was calling `setTxMicSource(nullptr)` directly on RadioModel's main thread while `m_connection` lived on its own thread (cross-thread race vs `onReadyRead`/watchdog reads).  Fix: marshal via `QMetaObject::invokeMethod(conn, lambda, Qt::BlockingQueuedConnection)` so detach completes before `m_txMicSource->stop() + reset()` runs.  Closes row 56. |
| Codex P2 — two-tone dB→linear | `db6c93b` | 2026-04-29 — Codex P2 review on PR #152.  `twoToneLevelChanged` lambda + initial-state pushes forwarded raw user-facing dB into `setTxPostGenTTMag*`, which expects linear amplitude in [0, 0.49999] (TwoToneController applies the conversion correctly at activation; lambda did not).  Symptom: muted / wrong-magnitude two-tone output mid-test.  Fix: apply verbatim `0.49999 * pow(10, db/20.0)` per `setup.cs:11056 [v2.10.3.13]` to both lambda and initial pushes.  Closes row 57; verified by row 60 spectrum-analyser bench. |

---

# Phase 3M-3a-i TX EQ + Leveler + ALC — Verification Matrix Extension

Added 2026-04-29 as part of Batch 6 (docs + cleanups).  3M-3a-i lights up
the first three stages of the 18-stage TXA processing chain — graphic 10-band
TX equalizer (with frequency edits + Nc / Mp / Ctfmode / Wintype DSP
parameters + per-band preamp), Leveler (slow-pump pre-ALC gain rider), and
ALC (fast-attack/slow-decay limiter).  Adds a 20-profile factory bank ported
verbatim from Thetis' `database.cs` mic-profile presets (Heil PR-40 SSB,
Yaeso M1, Heil HM-i, etc.) plus a modeless `TxEqDialog` editor and a
SpeechProcessorPage TX-dashboard page that cross-links to the relevant DSP
setup pages.

Manual rows tagged `[3M-3a-i-bench]` need an **ANAN-G2 + dummy load +
spectrum analyser** (none of these stages are HL2-meaningful in their own
right — HL2 doesn't run a PA chain — so HL2 bench coverage is optional).
Unit-test rows are auto-checked by ctest and pass on commit `db57ec8`
(246/246).

## New rows

| # | Test | Hardware | Procedure | Expected | Result |
|---|---|---|---|---|---|
| 61 | TxChannel TX EQ setters (Batch 1 B) | none | `ctest -R '^tst_tx_channel_eq_setters$' -V` | All cases pass: `setTxEqProfile` round-trips 10 bands × dB; `setTxEqEnabled` toggles the WDSP eq stage; `setTxEqFreqs` accepts 11-element float vector (DC + 10 band edges) and forwards to WDSP `SetTXAEQF`; Nc / Mp / Ctfmode / Wintype setters cache values and call the matching WDSP setter. | ✅ |
| 62 | TxChannel Leveler + ALC setters (Batch 1 B) | none | `ctest -R '^tst_tx_channel_leveler_alc_setters$' -V` | All cases pass: `setLevelerOn`, `setLevelerMaxGain`, `setLevelerAttack`, `setLevelerDecay`, `setLevelerHang` cover the 5 Leveler wrappers; `setAlcOn`, `setAlcMaxGain`, `setAlcAttack`, `setAlcDecay`, `setAlcHang`, `setAlcSlope`, `setAlcMaximumGain` cover the 7 ALC wrappers; defaults match Thetis `radio.cs:3960-4030 [v2.10.3.13]` (Lev MaxGain 5.0 dB / 2 ms / 500 ms / 500 ms; ALC MaxGain 3.0 dB / 2 ms / 10 ms / 500 ms / 0.0 / 4.0). | ✅ |
| 63 | TransmitModel TX EQ + Leveler + ALC properties (Batch 1 C) | none | `ctest -R '^tst_transmit_model_eq_lev_alc$' -V` | All cases pass: 10 EQ band properties (`txEqBand0..9`) round-trip; `txEqEnabled` / `txEqPreamp` / `txEqFreqs` / `txEqNc` / `txEqMp` / `txEqCtfmode` / `txEqWintype` round-trip; 5 Leveler properties + 7 ALC properties round-trip with the Thetis-derived defaults; per-MAC AppSettings keys persist; programmatic setter changes emit `*Changed` signals exactly once. | ✅ |
| 64 | MicProfileManager bundles 27 EQ/Lev/ALC keys (Batch 1 G) | none | `ctest -R '^tst_mic_profile_manager$' -V` | All cases pass — extended from 18 to N cases: profile `keyCount()` reports 50 (was 23); save/load/delete/setActive round-trip the new keys; verbatim Thetis "It is not possible to delete the last remaining TX profile" warning preserved on last-profile-delete; per-MAC isolation; first-launch "Default" seed includes all 50 documented values. | ✅ |
| 65 | RadioModel TX EQ + Lev + ALC routing (Batch 2) | none | `ctest -R '^tst_radio_model_eq_lev_alc_wiring$' -V` | All cases pass: TransmitModel property changes fan out to TxChannel via the connect-time wiring lambdas (12 EQ signals + 5 Lev signals + 7 ALC signals); initial-state push runs at connect after `m_txChannel` becomes non-null (mirrors the K.1/K.2 pattern from 3M-1c); QSignalBlocker prevents echo loops on programmatic property writes from MicProfileManager loads. | ✅ |
| 66 | TxApplet [LEV] [EQ] [PROC] toggle row (Batch 2) | none | `ctest -R '^tst_tx_applet_lev_eq_proc$' -V` | All cases pass: 3 quick-toggle buttons construct under the existing `TxApplet` row; LEV mirrors `TransmitModel::levelerOn`; EQ mirrors `txEqEnabled`; PROC mirrors `cfcOn` (placeholder until 3M-3a-ii lights up CFC); QSignalBlocker on model→UI sync; right-click on EQ launches `TxEqDialog`; tooltip text matches the Thetis Setup-IA wording verbatim. | ✅ |
| 67 | TxEqDialog scaffold + profile combo (Batch 3 + Batch 4) | none | `ctest -R '^tst_tx_eq_dialog$' -V` | All cases pass: dialog constructs with 10 vertical sliders + 10 frequency QSpinBoxes + preamp slider + Nc / Mp / Ctfmode / Wintype combos; bidirectional binding to `TransmitModel`; QSignalBlocker prevents echo; profile combo (Batch 4) populated from MicProfileManager; Save / Save-As / Delete buttons wired with verbatim Thetis last-profile-guard message; Tools menu launch + EQ-button right-click both open the dialog modeless (one instance, raised on subsequent launches). | ✅ |
| 68 | SpeechProcessorPage TX dashboard (Batch 5) | none | `ctest -R '^tst_speech_processor_page$' -V` | All cases pass: page constructs in Setup → Audio → Speech Processor slot; renders 3 status rows (TX EQ enable, Leveler enable, ALC max-gain) bound to `TransmitModel` properties; status indicator dot flips filled↔hollow on enable toggle; 3 cross-link buttons (DSP→AGC/ALC, DSP→CFC, DSP→VOX/DEXP) emit the documented `requestSetupPage(QString)` signal that SetupDialog routes to the matching page. | ✅ |
| 69 | TX EQ slider end-to-end audio `[3M-3a-i-bench]` | ANAN-G2 + dummy load + USB mic + spectrum analyser | Tune to 14.200 MHz USB.  Open TxApplet → right-click [EQ] → TxEqDialog opens.  Set band 5 (≈1 kHz) to +12 dB; leave others at 0.  Engage [EQ] toggle on TxApplet (or Enable checkbox in dialog).  PTT and speak a steady "ahh" tone (use a tone generator into the mic if cleaner).  Read spectrum analyser. | TX spectrum shows clear +12 dB boost centred at 1 kHz audio offset (i.e. at 14.201 MHz on the spectrum analyser).  Other bands within ±3 dB of flat.  Disable [EQ] — boost disappears, spectrum returns to flat speech envelope. | |
| 70 | TX EQ frequency edit `[3M-3a-i-bench]` | ANAN-G2 + dummy load + tone generator + spectrum analyser | Same setup as row 69.  In TxEqDialog, change band 5 frequency QSpinBox from 1000 Hz → 1500 Hz.  Keep band 5 at +12 dB.  PTT a continuous 1 kHz tone first, then 1.5 kHz tone. | At 1 kHz tone: no boost (band 5 has moved away).  At 1.5 kHz tone: +12 dB boost.  Confirms the freq-edit propagated through `setTxEqFreqs` to WDSP `SetTXAEQF`. | |
| 71 | Leveler Top sweep `[3M-3a-i-bench]` | ANAN-G2 + dummy load + USB mic + TX I/Q monitor (or spectrum analyser) | TxApplet [LEV] toggle on.  Setup → DSP → AGC/ALC → TX Leveler section → MaxGain slider.  Sweep MaxGain 0 → 5 → 10 → 15 dB while PTT-keying a steady speech sample. | TX I/Q monitor amplitude rises smoothly at each step (Leveler is a slow-pump gain rider; it should NOT pump on speech transients).  At 15 dB, peak speech levels reach saturation but the PA does NOT flutter / pump audibly.  No clicks or thumps. | |
| 72 | ALC MaxGain sweep `[3M-3a-i-bench]` | ANAN-G2 + dummy load + USB mic + ALC meter on screen | Setup → DSP → AGC/ALC → TX ALC section → MaxGain slider.  Sweep MaxGain 0 → 5 → 10 → 15 dB while PTT-keying speech with peaks.  Watch the ALC meter on the TxApplet. | At 0 dB: ALC barely deflects (very limited extra headroom).  At 15 dB: ALC meter shows 15 dB of gain reduction at speech peaks (matching the slider value).  No PA overdrive, no audible distortion at the speech peaks. | |
| 73 | Profile load round-trip `[3M-3a-i-bench]` | ANAN-G2 + dummy load + USB mic + Heil PR-40 (or any boom mic with characteristic shape) + spectrum analyser | Open TxApplet → right-click [EQ] → profile combo → select "Heil PR-40 SSB".  Verify 10 EQ slider positions match the values from Thetis `database.cs` factory profile (boost shape: low-cut at 100 Hz, peak around 2-3 kHz, gentle high roll).  PTT and speak. | Slider positions match the database.cs values byte-for-byte.  Spectrum analyser shows the characteristic Heil PR-40 EQ shape on TX (spoken voice amplified in the 2-3 kHz presence region). | |
| 74 | Profile save → restart-app round-trip `[3M-3a-i-bench]` | ANAN-G2 | TxEqDialog → load "Default" → modify 3 EQ bands + change Lev MaxGain to 8 dB + change ALC Slope to 1.5 → click Save.  Quit the app.  Relaunch.  Reopen TxEqDialog → verify "Default" still selected → confirm the modified values persisted. | All 50 keys round-trip cleanly via MicProfileManager + AppSettings per-MAC.  No values revert to defaults.  TX path uses the saved values (PTT + spectrum analyser confirms). | |
| 75 | Last-profile-delete guard `[3M-3a-i-bench]` | any radio (no TX needed) | TxEqDialog → Delete every profile down to one (Default).  Click Delete on the last remaining profile. | Verbatim Thetis warning surfaces in a modal: "It is not possible to delete the last remaining TX profile".  Profile is NOT deleted.  Combo still shows the one profile.  No silent failure. | |
| 76 | Cross-link buttons on SpeechProcessorPage `[3M-3a-i-bench]` | none (UI only) | Setup → Audio → Speech Processor.  Click "Open AGC/ALC settings" button → expect navigation to Setup → DSP → AGC/ALC.  Back to Speech Processor.  Click "Open CFC settings" → expect navigation to Setup → DSP → CFC (placeholder until 3M-3a-ii).  Click "Open VOX/DEXP settings" → expect navigation to Setup → DSP → VOX/DEXP. | Each click changes the active SetupDialog page to the documented destination.  No crashes if the destination page is a placeholder.  Back-button on SetupDialog returns to Speech Processor. | |
| 77 | Speech Processor live status mirror `[3M-3a-i-bench]` | none (UI only) | Open SpeechProcessorPage in Setup.  In a separate window or in the dock, open TxApplet.  Click TxApplet [EQ] toggle.  Watch the SpeechProcessorPage TX EQ status row. | Status indicator dot on SpeechProcessorPage flips filled (when [EQ] on) ↔ hollow (when [EQ] off) in real time, with no Setup-page reopen needed.  Same behaviour for Leveler enable mirror. | |

## Carry-forward flips from 3M-1c

| Row | Change |
|---|---|
| 3M-1c row 24 (PC mic SSB out) | Carries forward — 3M-3a-i now also exercises the Leveler + ALC path on the bench.  JJ's PC mic test from 3M-1c was conducted with default Leveler + ALC values (Lev MaxGain 5 dB; ALC MaxGain 3 dB), so the existing pass result still holds; 3M-3a-i now lets him *tune* those values via Setup → DSP → AGC/ALC → TX Leveler / TX ALC sections. |
| 3M-1c row 50 (Profile save / load / restart-app round-trip) | Extended — profiles now also include EQ + Lev + ALC keys (was 23 keys → now 50 keys).  Existing 23-key bench pass result still holds; row 74 above adds the new 27-key coverage on top. |

## Result tracking

Rows 61-68 (unit tests, 8 rows): ✅ all green on commit `db57ec8` (246/246
under `-j8`).
Rows 69-77 (bench tests, 9 rows): pending JJ + ANAN-G2 + dummy load +
USB mic + spectrum analyser.  Row 75 (last-profile-delete guard) and
row 76 (cross-link buttons) are pure-UI and can be verified without
hardware.

When bench rows complete, edit the Result column to `✅` with the commit SHA where the row was confirmed.

## Phase 3M-3a-i commit summary (added 2026-04-29 by Batch 6)

| Phase | Commits | Summary |
|---|---|---|
| Batch 1 B | `0165276` | TxChannel EQ + Leveler/ALC wrappers (10 EQ band + 5 Lev + 7 ALC setters; cache-and-recall partner-value pattern preserved per Thetis `radio.cs:3960-4030 [v2.10.3.13]`) |
| Batch 1 C | `e69b7f2` | TransmitModel TX EQ + Leveler + ALC properties (10 + 5 + 7 properties + per-MAC AppSettings keys) |
| Batch 1 G | `5650b80` | MicProfileManager bundles 27 new EQ/Lev/ALC keys (was 23 → now 50 keys) |
| Batch 1 cleanup | `45a2cc8` | drop unused `kProfileSubpath` const |
| Batch 2 | `88707e1` | RadioModel → TxChannel TX EQ + Lev + ALC routing (12 + 5 + 7 connect-time signal wires + initial-state push) |
| Batch 2 D | `bc16b85` | AgcAlcSetupPage adds TX Leveler + TX ALC sections |
| Batch 2 F | `45b9f3c` | TxApplet quick-toggle row [LEV] [EQ] [PROC] |
| Batch 3 A.1 | `e8b823f` | TxEqDialog scaffold — 10-band sliders + freq spinboxes + preamp + Nc / Mp / Ctfmode / Wintype |
| Batch 3 A.1 | `f8d81d6` | wire EQ right-click + Tools menu to TxEqDialog launch |
| Batch 4 A.2 | `4095b8f` | port 20 Thetis factory TX profiles verbatim from `database.cs` |
| Batch 4 A.2 | `c3639a4` | TxEqDialog adds profile combo + Save / Save-As / Delete |
| Batch 5 E | `a4d9d03` | SpeechProcessorPage rewrite as TX dashboard (3 status rows + 3 cross-link buttons) |
| Batch 6 cleanup | `db57ec8` | drop 3 unused includes (TxApplet `LogCategories.h` + MainWindow `<cmath>` + TransmitSetupPages `StyleConstants.h`) |
| Batch 6 docs (this) | (TBD) | verification matrix extension + commit summary (3M-3a-i I) |
| Batch 6 docs | (TBD) | CLAUDE.md mark 3M-3a-i Complete (pending bench), 3M-3a-ii next |

---

# Phase 3M-3a-ii CFC + CESSB + Pre-emphasis — Verification Matrix Extension

Added 2026-04-30 as part of Batch 6 (final batch) of 3M-3a-ii.  3M-3a-ii lights
up the next four stages of the 18-stage TXA chain on top of 3M-3a-i: the
Continuous Frequency Compressor (CFC), the CESSB controlled-envelope SSB
shaper, the Phase Rotator (PhRot), and the CPDR speech compressor.  Settings
surfaces include a full Setup → DSP → CFC page (Phrot / CFC / CESSB group
boxes) and a modeless `TxCfcDialog` per-band editor, plus PROC/CFC quick
toggles on the TxApplet.  Pre-emphasis (FM-mode) is deferred to a follow-up
sub-PR — the AM/FM TX path lands in 3M-3 final assembly.

Manual rows tagged `[3M-3a-ii-bench]` need an **ANAN-G2 + dummy load +
spectrum analyser** (HL2 doesn't run a PA chain so HL2 bench coverage is
optional).  Unit-test rows are auto-checked by ctest and pass on the Batch-6
final commit (251/251 tests green).

## New rows

| # | Test | Hardware | Procedure | Expected | Result |
|---|---|---|---|---|---|
| 78 | TxChannel CFC + CPDR + CESSB + PhRot setters | none | `ctest -R '^tst_tx_channel_cfc_cpdr_cessb_setters$' -V` | All cases pass: WDSP wrappers (`SetTXACFCRun`, `SetTXACFCPreComp`, `SetTXACFCPreCompProfile`, `SetTXACFCPostEqRun`, `SetTXACFCPostEqGain`, `SetTXACFCPostEqProfile`, `SetTXACompressorRun`, `SetTXACompressorGain`, `SetTXACESSBRun`) round-trip the cached partner-value pattern from Thetis `radio.cs` [v2.10.3.13]; phrot Corner / NStages / Reverse setters added in B.2. | ✅ |
| 79 | TransmitModel CFC + CPDR + CESSB + PhRot schema | none | `ctest -R '^tst_transmit_model_cfc_cpdr_cessb_phrot$' -V` | All cases pass: 4 PhRot + 4 CFC scalar + 30 CFC array + 1 blob + 2 CPDR + 1 CESSB property = 42 properties round-trip; `*Changed` signals fire exactly once on programmatic mutation; per-MAC AppSettings keys persist. | ✅ |
| 80 | RadioModel TM → TxChannel routing for CFC/CPDR/CESSB/PhRot | none | `ctest -R '^tst_radio_model_eq_lev_alc_wiring$' -V` | All cases pass — extended from EQ/Lev/ALC to also cover CFC/CPDR/CESSB/PhRot fan-out (TransmitModel signals → TxChannel WDSP setters); initial-state push runs at connect. | ✅ |
| 81 | MicProfileManager bundles 41 CFC/CPDR/CESSB/PhRot keys | none | `ctest -R '^tst_mic_profile_manager_cfc_round_trip$' -V` | All cases pass: profile `keyCount()` reports 91 (was 50); save/load round-trip the new keys for all 21 factory profiles; `CFCParaEQData` blob round-trip; boolean serialization (5 new boolean keys). | ✅ |
| 82 | CfcSetupPage (Setup → DSP → CFC) | none | `ctest -R '^tst_cfc_setup_page$' -V` | All cases pass: 3 group boxes (Phase Rotator / CFC / CESSB) construct; initial values reflect TM defaults; bidirectional binding works both directions; spinbox range clamping holds; `[Configure CFC bands…]` button emits `openCfcDialogRequested` (Batch 6 wires the dialog). | ✅ |
| 83 | TxCfcDialog (10-band CFC editor) | none | `ctest -R '^tst_tx_cfc_dialog$' -V` | All cases pass: dialog constructs with the 32-control surface (profile combo + 5 buttons + 2 globals + 30 band spins); initial values match TM CFC defaults (database.cs:4724-4768 [v2.10.3.13]); UI → TM bidirectional for all 5 setter families (precomp / post-EQ gain / freq[i] / comp[i] / post-EQ band gain[i]); Reset-to-defaults round-trips through `setActiveProfile("Default")`; echo guard prevents re-emit storms. | ✅ |
| 84 | TxApplet [LEV] [EQ] [PROC] [CFC] toggle row | none | `ctest -R '^tst_tx_applet_lev_eq_proc$' -V` | All cases pass — extended from 3 buttons to 4: PROC enabled (was disabled placeholder) + CFC button added; PROC bidirectional with `cpdrOn`; CFC bidirectional with `cfcEnabled`; CFC right-click triggers `requestOpenCfcDialog` and creates a single modeless `TxCfcDialog` instance. | ✅ |
| 85 | SpeechProcessorPage Phrot / CFC / CESSB live status | none | `ctest -R '^tst_speech_processor_page$' -V` | All cases pass — extended in 3M-3a-ii Batch 5 with Phrot / CFC / CESSB live-binding cases (rows 10-15 in tst file).  Status labels reflect TM defaults and update live on `phaseRotatorEnabledChanged` / `cfcEnabledChanged` / `cessbOnChanged` + `cpdrOnChanged`. | ✅ |
| 86 | CFC PROC + CFC quick toggles `[3M-3a-ii-bench]` | ANAN-G2 + dummy load + USB mic + spectrum analyser | TxApplet → click [PROC]; PTT and speak.  Disable [PROC]; PTT same speech volume.  Click [CFC]; PTT and speak. | With [PROC] on: speech peaks compressed (CPDR is a soft-knee dynamics stage; visible loudness lift on the spectrum's RMS without obvious distortion).  With [CFC] on: per-band compression — speech spectrum looks more "even" across the band columns (10-band CFC trims peaks per slice). | |
| 87 | TxCfcDialog per-band edits hit RF `[3M-3a-ii-bench]` | ANAN-G2 + dummy load + tone generator + spectrum analyser | TxApplet → right-click [CFC] → TxCfcDialog opens.  Set band 5 freq=1000 Hz, comp=12 dB; leave others at default.  PTT a 1 kHz tone, then a 500 Hz tone. | At 1 kHz: visible compression / peak limiting (band 5 covers 1 kHz with comp=12).  At 500 Hz: less compression (band 5 has moved away).  Confirms the spinbox edits propagate through `setCfcEqFreq` / `setCfcCompression` to WDSP `SetTXACFCPreCompProfile`. | |
| 88 | CFC dialog reuse across Setup / TxApplet `[3M-3a-ii-bench]` | none (UI only) | Setup → DSP → CFC → click [Configure CFC bands…].  Verify `TxCfcDialog` opens.  Close it.  TxApplet → right-click [CFC].  Verify the SAME instance is raised (state preserved). | The dialog's spinboxes still show whatever values were last edited.  No stale duplicate dialog.  No flicker / re-init artifact. | |
| 89 | Reset-to-factory-defaults round-trip `[3M-3a-ii-bench]` | none (UI only) | TxCfcDialog → mutate 3 per-band spinboxes (freq + comp + post-EQ) + globals.  Click [Reset to factory defaults].  Confirm the modal. | All CFC values revert to the database.cs:4724-4768 [v2.10.3.13] defaults (precomp=0, post-eq-gain=0, freq[0..9]=stock array, comp[0..9]=5, post-eq-band[0..9]=0).  Active profile selector returns to "Default". | |

## Carry-forward flips from 3M-3a-i

| Row | Change |
|---|---|
| 3M-3a-i row 66 | Was: "[LEV] [EQ] [PROC] toggle row".  Now superseded by row 84 — PROC enable + CFC addition flips PROC from disabled to bidirectionally wired and adds a 4th button. |

## Result tracking

Rows 78-85 (unit tests, 8 rows): ✅ all green at Batch-6 final commit (251/251 tests pass).
Rows 86-89 (bench tests, 4 rows): pending JJ + ANAN-G2 + dummy load + USB mic + spectrum analyser.  Rows 88-89 are pure-UI and can be verified without hardware.

## Phase 3M-3a-ii commit summary (added 2026-04-30 by Batch 6)

| Phase | Commits | Summary |
|---|---|---|
| Batch 1 B | `11fa72a` + earlier B.1 | TxChannel CFC / CPDR / CESSB / PhRot wrappers (cache-and-recall pattern preserved per Thetis `radio.cs` [v2.10.3.13]) |
| Batch 1 C | `12c1b01` | TransmitModel CFC + CPDR + CESSB + PhRot properties (42 properties, per-MAC AppSettings keys) |
| Batch 2 D | `0b587b3` | RadioModel TM → TxChannel routing (connect-time signal wires + initial-state push) for the new families |
| Batch 4 G | `2b3219e` | MicProfileManager bundles 41 new CFC/CPDR/CESSB/PhRot keys (was 50 → now 91 keys) |
| Batch 5 E | `f309661` | CfcSetupPage (Setup → DSP → CFC) — 3 group boxes (Phase Rotator / CFC / CESSB) + dashboard live status mirrors |
| Batch 6 (this) | (TBD) | TxApplet PROC enable + CFC button + TxCfcDialog modeless 10-band editor + CfcSetupPage `[Configure CFC bands…]` wiring through SetupDialog → MainWindow → TxApplet::requestOpenCfcDialog |
