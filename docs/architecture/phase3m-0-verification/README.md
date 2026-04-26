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
- **Row 5 (watchdog wire bit)** is a known stub; the wire format identification (capture or HL2 firmware analysis) is a planned 3M-1a task.
- **Row 12 (Andromeda PA-trip)** requires Andromeda hardware + Ganymede PA; if no such hardware is available at bench time, the unit-test coverage in `tst_radio_model_pa_tripped::ganymedeTripMessage_dropsMoxIfActive` substitutes for hardware verification of the MOX-drop side effect.
