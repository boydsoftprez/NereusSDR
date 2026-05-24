# ANAN-G2E (HermesC10) Bench Verification Matrix

**Status:** `Pending bench` (gated on receipt of actual G2E hardware).

**Source:** Spec [2026-05-21-anan-g2e-port-design.md](../2026-05-21-anan-g2e-port-design.md) §7.
Plan [2026-05-21-anan-g2e-port-plan.md](../2026-05-21-anan-g2e-port-plan.md) Phase G Task G3.

## Core verification (12 rows, from spec §7)

| # | Row | Procedure | Pass criterion | Status |
|---|---|---|---|---|
| 1 | Discovery | Power on G2E, launch NereusSDR | ConnectionPanel shows "ANAN-G2E" with board badge "HermesC10" | Pending |
| 2 | RX | Click Connect, set 14.200 MHz USB | Audio heard, FFT renders, RX2 enables and tunes | Pending |
| 3 | RX2 UI gating | Open RxApplet | No RX2 preamp combo; no RX2 stepped-att slider | Pending |
| 4 | PureSignal | Set TX to clean tone, Tools→PureSignal | PSForm opens, calc converges; peak indicator stable | Pending |
| 5 | PA gain table | Setup→PA Gain by Band | Sliders match N1GP defaults from design §3 | Pending |
| 6 | PA telemetry | Setup→PA Values | Volts, amps, temperature labels visible | Pending |
| 7 | Antenna labels | Setup→Antenna Control | RX-only labels read BYPS/EXT1/XVTR; EXT2 button reads "Rx BYPASS on Tx" | Pending |
| 8 | AutoPACalibrate hidden | Setup→PA Gain by Band | chkAutoPACalibrate not visible | Pending |
| 9 | Bypass PA Settings | Setup→PA Gain by Band | chkBypassANANPASettings visible; toggling switches gain dispatch to bypass row | Pending |
| 10 | ATT-on-TX | Setup→Transmit→PA | labelATTOnTX + spinbox render | Pending |
| 11 | MKII BPF on-air | Connect, listen on 20m and 6m | High-band Alex filters engage per band | Pending |
| 12 | WDSP init at connect | Connect to G2E, check log | WdspEngine logs setAdcSupply(33) + setLRAudioSwap(0) at connect (data carried but pure DLL call until ChannelMaster ported) | Pending |

## Documented Thetis-feature gaps (NereusSDR doesn't have these yet)

These were identified during Phases E and F. Each requires NereusSDR-architectural work that's out of scope for the G2E port -- they're tracked here for future phases.

| # | Thetis feature | Phase that surfaced | Source cite | NereusSDR landing when ported |
|---|---|---|---|---|
| G1 | VFO B split MOX sub-display gate on HPSDRHW (Hermes\|HermesII\|HermesC10) | E4 | `console.cs:32570-32576; 32599-32609 [v2.10.3.15]` | Phase 3F (multi-panadapter / VFO B / split infrastructure) |
| G2 | `SetAAudioMixStates` per-protocol DDC family dispatch (USB 4-DDC group at console.cs:27653-27664; ETH 2-DDC group at console.cs:27669-27679) | F2/F3 | `console.cs:27653-27679 [v2.10.3.15]` | When ChannelMaster is fully ported (Phase 3L) |
| G3 | P1 user I/O inhibit bit selection (ANAN_G2E uses getUserI02 bit[2] with 7000D/8000D/REDPITAYA; other boards use getUserI01 bit[1]) | F4 | `console.cs:25859-25865 [v2.10.3.15]` | When P1 user I/O parsing lands in P1RadioConnection |
| G4 | TX meter mode conditional insertion (NereusSDR statically includes "Ref Pwr" for all boards; Thetis gates it on the ANAN_G2E/ORIONMKII capable group at console.cs:14868-14882) | F6 | `console.cs:14868-14882 [v2.10.3.15]` | When per-board TX meter mode filtering is implemented |

GAP comments referencing these rows exist in the NereusSDR source at:
- `src/core/MoxController.cpp` (F2/F3 audio mix states)
- `src/core/safety/TxInhibitMonitor.cpp` (F4 P1 user I/O inhibit bit)
- `src/gui/setup/TransmitSetupPages.cpp` (F6 TX meter mode conditional)

## How to use this matrix

1. Connect an ANAN-G2E. Run NereusSDR built from `claude/anan-g2e-port` branch (or after merge from `main`).
2. Walk each row in order; mark Pass / Fail.
3. Failures → file follow-up issues; each is a candidate for hotfix or sub-PR.
4. Gap rows (G1-G4) do not block acceptance of this PR; they are tracked for future phases.
