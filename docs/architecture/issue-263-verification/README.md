# Issue #263 — ANAN-10E / Hermes-class P2 Connection Verification

> **Status:** Bench matrix drafted; awaiting verification on real
> hardware (ANAN-10E with community Protocol 2 gateware).
>
> Reporter: nubbyless (issue #263). Symptom: NereusSDR detects the
> radio, shows "Connected" for a few seconds, then disconnects.
> Works fine in Thetis and ZeusSDR.
>
> Root cause: `P2RadioConnection::connectToRadio` and
> `P2CodecOrionMkII::applyPureSignalDdcConfig` only carried the
> G2-class (2-ADC) branch of `Thetis console.cs UpdateDDCs()`. On
> the wire, NereusSDR was telling a 1-ADC HermesII board to "enable
> DDC2", which that gateware does not stream. The 2 s connect
> watchdog fired and the connection dropped. The fix ports the
> HermesII (8451-8521) and Hermes (8378-8449) branches of
> UpdateDDCs into the P2 codec, picks the per-board primary DDC at
> connection time, and routes the receiver-to-DDC mapping in
> RadioModel accordingly.

## How to use

1. Build: `cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo && cmake --build build`.
2. Launch: `open build/NereusSDR.app` (macOS) / run `build/NereusSDR` (Linux/Win).
3. For each row below, perform the described action against the test radio,
   confirm the expected result, attach packet captures + screenshots to
   the PR thread when applicable.

| # | Radio | Action | Expected | Notes |
|---|---|---|---|---|
| 1 | **ANAN-10E** (1-ADC HermesII, community P2 gateware) | Discover + Connect | Connection holds. Spectrum + waterfall display real I/Q. No "connect watchdog fired" log line. | This is the issue-#263 regression case. Was failing at 2 s pre-fix; should pass post-fix. |
| 2 | ANAN-10E | Tune VFO across 80m, 40m, 20m, 15m, 10m | Spectrum re-centers; signal strength changes per band. Alex HPF/LPF bits update on each tune (visible via `setAntennaRouting` log). | Validates `setReceiverFrequency` writes to DDC0 (the new primary), not DDC2. |
| 3 | ANAN-10E | Cycle sample rate 48 / 96 / 192 kHz | Each rate accepted, spectrum bandwidth updates, no disconnect. | `setSampleRate` writes `m_rx[0].samplingRate`; was previously hardcoded on `m_rx[2]` indirectly. |
| 4 | ANAN-10E | Connect / Disconnect / Reconnect 3x | Each cycle clean; no `m_rx[2]` stale state confuses the second connect. | `m_rx[primaryDdc].frequency == 0` guard now keys off the right slot. |
| 5 | ANAN-10E | PCAP the P2 CmdRx packet during steady-state RX. Verify byte 7 = 0x01 (DDC0 enabled), byte 18-19 = wire rate (192/96/48), byte 30-31 = 0 (DDC2 rate unused). | Wire matches Thetis P2 wire for the same radio. | Compare against a Thetis pcap from the same radio if available. |
| 6 | ANAN-100B (1-ADC HermesII, same gateware family) | Connect | Connection holds; same per-band tune behavior. | Same code path as ANAN-10E; if available, exercises the second SKU on the HermesII branch. |
| 7 | **ANAN-10 / ANAN-100** (1-ADC Hermes-class, community P2 gateware) | Connect | Connection holds; DDC0 primary as per Thetis console.cs:8378-8449. | Exercises the new `psDdcConfigHermesClass` P2 branch. May be theoretical — community P2 firmware for Hermes is rare. |
| 8 | **ANAN-G2** (2-ADC OrionMkII/Saturn) | Connect + cycle modes (LSB/USB/CW/AM/FM) | Behavior unchanged from v0.5.1. RX1 on DDC2 as before. | Regression check — the G2 branch was factored into `psDdcConfigG2Class` byte-identically. The existing `tst_p2_regression_freeze` test locks this against the pre-refactor JSON baseline. |
| 9 | ANAN-G2 | PureSignal MOX TX | PS still works as in v0.5.1. | Regression check on the G2 PS-MOX path. |
| 10 | ANAN-10E | PureSignal MOX TX (if user has PS-capable wiring) | **DEFERRED / OBSERVATIONAL.** Codec emits Thetis-source-faithful bytes (`cntrl1=4` for HermesII PS-MOX). Whether the community gateware honors them is unverified. Capture wire bytes; if a future bench shows divergence, add an empirical override mirroring the 2026-05-09 P1 fix in `P1CodecStandard.cpp::psDdcConfigHermesIIClass`. | The P1 wire-observed `cntrl1=0` override does NOT mechanically apply to P2: P1 wire reads `P1_adc_cntrl` (separate variable, networkproto1.c:519), while P2 wire reads `prn->rx[i].rx_adc` which IS the path SetADC_cntrl1 writes. So Thetis P2 wire = source value. Detailed rationale in inline comment in `P2CodecOrionMkII.cpp::psDdcConfigHermesIIClass`. |
| 11 | ANAN-10E | Disconnect via radio power-off (not via UI) | Connection ends; auto-reconnect armed; reconnect resumes cleanly when radio comes back up. | No DDC2-vs-DDC0 confusion across the auto-reconnect path. |

## Build / unit test status (already verified before bench)

- `tst_codec_ps_ddc_config`: 35 cases pass, including 9 new HermesII / Hermes P2 wire-byte assertions.
- `tst_p2_codec_orionmkii`: unchanged, all pass.
- `tst_p2_codec_saturn`: unchanged, all pass.
- `tst_p2_regression_freeze`: pre-refactor Saturn/Orion/OrionMkII byte-for-byte baseline still locks; G2 branch was factored without semantic change.
- Full ctest: 471/471 pass on this branch.

## Files touched

- `src/core/codec/P2CodecOrionMkII.h` — new per-board PsDdcConfig helpers + dispatch table doc.
- `src/core/codec/P2CodecOrionMkII.cpp` — `applyPureSignalDdcConfig` now dispatches on HPSDRModel; added `psDdcConfigG2Class` (refactor of existing logic), `psDdcConfigHermesClass`, `psDdcConfigHermesIIClass` (new ports of Thetis console.cs:8378-8521).
- `src/core/codec/IP2Codec.h` — interface doc updated to reflect per-model dispatch.
- `src/core/P2RadioConnection.h` — `primaryRxDdcForBoard(HPSDRHW)` static helper.
- `src/core/P2RadioConnection.cpp` — `connectToRadio` seeds `m_rx[primaryDdc]` (was hardcoded `m_rx[2]`).
- `src/models/RadioModel.cpp` — `onConnectionInfo` chooses per-board primary DDC for `setDdcMapping`.
- `tests/tst_codec_ps_ddc_config.cpp` — 9 new HermesII / Hermes P2 wire-byte test cases.
- `docs/attribution/THETIS-PROVENANCE.md` — P2CodecOrionMkII row updated with the new console.cs port.
