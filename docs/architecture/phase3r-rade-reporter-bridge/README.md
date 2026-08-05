# Phase 3R-bridge: FreeDV RADE rx_report upload bridge — bench matrix

Bench verification rows for `FreeDVRadeReporterBridge`, the Path B
(RADE sync-only, empty-callsign) rx_report uploader ported from
freedv-gui `src/main.cpp:1971-1996` [@77e793a].

Path A (callsign-decoded via EOO text channel) is already wired
through `RadioModel::onRadeTextDecoded` and is exercised by row 3.

## Setup

1. Set Setup → Reporting → FreeDV Reporter → Callsign + Grid Square +
   Reporting Enabled = on. Restart the app if Reporting was off at
   launch.
2. Tune the active slice to a known RADE bench frequency
   (FreeDV Reporter Tools → 14130 kHz is the canonical test slot).
3. Mode → RADE.
4. Open Tools → FreeDV Reporter (Ctrl+Shift+R) so you can watch your
   own row's "Last RX SNR" column.
5. In a second tab, open https://qso.freedv.org and find your callsign.

## Verification rows

| # | Scenario | Expected | Notes |
|---|---|---|---|
| 1 | Cold launch, RADE mode, no RF signal | No `rx_report` packets logged. `nereus.freedv.rade.bridge` debug shows "RADE sync -> false" or no sync events. | Sync gate works. |
| 2 | RADE syncs on a beacon (no callsign yet) | ~1 `rx_report` per second logged at `lcSpots` debug level, mode "RADEV1", empty callsign, snr ≈ live SNR. qso.freedv.org "Last RX SNR" column on own row populates within 2 s. | Path B operational. This is the primary fix. |
| 3 | Beacon transmits its callsign via EOO | Immediate `rx_report` with callsign + "RADEV1" + snr from `RadioModel::onRadeTextDecoded` (Path A, pre-existing). Path B continues 1 Hz cadence between EOO frames. | Path A regression check. |
| 4 | Setup → Reporting → Reporting Enabled = off mid-sync | Both paths go silent within 100 ms. `nereus.freedv.rade.bridge` debug shows "Reporting disabled". qso.freedv.org dashboard row's "Last RX SNR" eventually expires. | Reporting-disabled gate works. |
| 5 | Engage MOX while synced | Path B stops within 100 ms of MOX engaging. Release MOX → Path B resumes on the next tick. | TX gate works (no self-reporting our own carrier). |
| 6 | Pull the network cable / kill DNS | Reporter `isConnected()` goes false; bridge becomes idle (no emits, no crashes). Reconnect → bridge resumes. | Reporter-disconnected gate + reconnect-survival. |
| 7 | Switch mode from RADE to USB | Bridge stops ticking entirely (sync goes false on RADE channel destroy). No `rx_report` packets while in USB. | Mode-change cleanup. |
| 8 | Reload the qso.freedv.org page, find own callsign | "Last RX SNR" column shows the rounded int matching what the bridge sent on the last tick. | Server-side round-trip. |
| 9 | Open in-app Tools → FreeDV Reporter dialog, find own callsign | Own row's SNR column populated (server echoes our `rx_report` back as an inbound `rx_report` event keyed by our sid). Negative SNR shows e.g. "-3"; positive shows "+12". | Inbound parsing already works (per `FreeDVReporterClient::onRxReport`); this row confirms the bidirectional loop. |

## Smoke commands

```bash
# Enable bridge debug logging (host-side):
QT_LOGGING_RULES="nereus.freedv.rade.bridge=true;nereus.spots=true" \
  ./build/NereusSDR.app/Contents/MacOS/NereusSDR

# Tail the rolling log file for Path B emits:
grep -E "RADE sync|Reporting (en|dis)abled|sendRxReport|rxReportEmitted" \
  ~/Library/Preferences/NereusSDR/nereussdr.log | tail -30

# Unit test (must pass):
ctest --test-dir build -R tst_freedv_rade_reporter_bridge -V
```

## Pass/fail recording

Tick each row as it's verified. File an issue with the row number
and `nereussdr.log` excerpt if any row fails on bench.

| # | Pass | Date | Operator | Notes |
|---|---|---|---|---|
| 1 |   |   |   |   |
| 2 |   |   |   |   |
| 3 |   |   |   |   |
| 4 |   |   |   |   |
| 5 |   |   |   |   |
| 6 |   |   |   |   |
| 7 |   |   |   |   |
| 8 |   |   |   |   |
| 9 |   |   |   |   |
