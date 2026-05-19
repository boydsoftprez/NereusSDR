# PR Draft: Phase 3P-II PGXL/TGXL + analog S-Meter port

## Title

`Phase 3P-II: PGXL/TGXL + analog S-Meter port (~80 commits, 4 phases)`

---

## Summary

This PR wires NereusSDR to the FlexRadio PowerGenius XL (PGXL) amplifier and
TeraGenius XL (TGXL) tuner over Ethernet, ports the AetherSDR analog S-Meter
widget with two Thetis-native RX meter modes (Sig Avg and Max Bin), adds full
FlexRadio API connection robustness (pairing, keepalive, auto-reconnect), and
delivers the complete operator-facing device-config UI including fault history,
TX interlock policy, tune memory management, and applet right-click navigation.
Four phases across roughly 80 commits; ~3000 LOC of production code; 28 unit
test slots across 11 test executables; 36-row bench matrix. Single PR per the
operator's direction.

---

## Highlights

### Phase 1 - PGXL/TGXL baseline (AetherSDR 1:1 port)

- `PgxlConnection`, `TgxlConnection`, `TunerModel`, `RelayBar`, `LanDiscovery`,
  `AmpApplet`, `LanScanDialog` ported from AetherSDR `[@a29ff40]`.
- `TunerApplet` rewired for TGXL; `RadioModel` owns both connections.
- Setup -> Network -> Peripherals page (PGXL + TGXL rows, manual IP, Scan LAN).
- 4 test executables, 7 slots.

### Phase 2 - Analog S-Meter port (AetherSDR widget + Thetis RX modes)

- `SMeterWidget` ported from AetherSDR `[@0cd4559]`: 180-degree needle arc,
  S-unit scale (S0 = -127 dBm, 6 dB/S-unit, animated needle 45 ms attack /
  180 ms release), configurable peak hold.
- NereusSDR-native divergences: right-click context menu replaces inline strip;
  `RxMode` expands from 2 to 4 entries (adds Sig Avg via `GetRXAMeter(RXA_S_AV)`
  and Max Bin via `SetupDetectMaxBin` / `GetDetectMaxBin`, both ported from
  Thetis `[@501e3f5]`).
- PGXL-aware 2 kW power-scale snap; standby-aware feed switch on `MainWindow`.
- 4 test executables, 10 slots.

### Phase 3 - Connection robustness

- `amplifier create`, `flexradio` pairing with graceful fallback, `keepalive
  enable`, `ping` watchdog, exponential-backoff auto-reconnect (1 / 2 / 5 / 10 /
  30 / 60 s).
- `ConnectionDiagnostics` QObject with 10 Q_PROPERTYs and 1 Hz coalesce timer.
- `SliceModel::bandChanged` wired to `PgxlConnection::setBand` so amplifier
  tracks operator band changes automatically.
- Live status labels on Peripherals page (Disconnected / Connecting / Connected /
  Connected+paired / Error).
- 6 test executables, 13 slots (20 total with Phase 1 baseline).

### Phase 4 - Advanced UI, fault log, interlock policy, applet right-click nav

- `FaultLog` (ring buffer, JSON-persisted, `likelyCause` heuristic) + 3 tests.
- `TuneMemoryStore` (per-(antenna, Band) relay cache, auto-recall on band change,
  JSON-persisted) + 3 tests.
- `TxInterlockPolicy` (Disabled / Warn / Block + grace period + SWR gate) +
  3 tests.
- `TgxlAdvancedPage` (5 sections: Identity, Hardware/antenna labels, Network,
  Diagnostics, Tune Memory Management).
- `PgxlAdvancedPage` (6 sections: Identity, Hardware, Network, Pairing,
  Diagnostics, Fault History).
- `PgxlInterlockPage` (Setup -> Transmit -> PGXL Interlock).
- `PgxlSaveRebootDialog` confirmation modal with auto-reconnect recovery path.
- `MainWindow::openSetup(QString pageKey)` navigation API.
- AmpApplet + TunerApplet right-click context menus (Open Advanced, Disconnect /
  Reconnect, Copy diagnostics, tune memory shortcuts).
- PGXL TX power cap soft-alert toast on MainWindow status bar.
- `PgxlConnection` `readSetup` / `writeSetup` / `readIfconf` / `writeIfconf` /
  `save` unit tests: 3 new executables, 8 new slots (28 total).

---

## Test coverage

28 unit test slots across 11 test executables covering:

- PgxlConnection: parse (3), pairing (3), keepalive (2), ping (2), reconnect (1),
  setup (3), ifconf (2), save (3).
- TgxlConnection: parse (4), ping (2).
- TunerModel: applyStatus (11).
- LanDiscovery: regex (3).
- SMeterWidget: scale (3), peak hold (3), context menu (2).
- WdspEngine: MaxBin WDSP wrappers (2).
- ConnectionDiagnostics (3).
- FaultLog (3).
- TuneMemoryStore (3).
- TxInterlockPolicy (3).

---

## Bench verification

36-row matrix at `docs/architecture/phase-pgxl-tgxl-smeter-verification/README.md`.
Covers PGXL telemetry, TGXL relay control, LAN discovery, S-Meter scaling and
peak hold, pairing, keepalive and ping, Save & Reboot, fault history, TX power
cap, TX interlock policy, antenna labels, tune memory, and right-click navigation.
Row 18 (HL2) is gated on the open ATT/filter safety audit (3R precedent; see
`phase3r-verification/README.md` Row 9).

---

## Out of scope (per design doc section 11)

- OpenHPSDR-native amp/tuner abstraction layer (future epic).
- Per-MAC scoping of PGXL/TGXL settings.
- Antenna Genius (AG) integration (same protocol family, separate epic).
- SO2R PGXL 3WAY variant.
- CAT-serial pairing path (`catradio` verb, requires Phase 3K rigctld).
- PGXL `message` verb (debug-only, no operator-facing value).
- Discovery of Antenna Genius devices on UDP 9007.
- PGXL chip in the bottom status bar (AetherSDR has none; no prior precedent).

---

## Follow-ups

- **HL2 ATT/filter audit gate:** Row 18 of the bench matrix unblocks once the
  HL2 audit closes. Tracked separately.
- **TGXL absolute-relay-write bench probe:** Row 34 uses a `tune start`
  placeholder; the bench session will confirm whether the firmware exposes an
  absolute-relay-write verb that enables exact position restore vs. trigger-tune
  fallback. Document the outcome in the bench matrix notes.
- **Unused-include cleanups:** several files accumulated defensive includes
  during the epic that can be pruned once the API surface stabilises. Deferred
  to a post-merge tidy pass.
- **PGXL chip in bottom status bar:** AetherSDR has no equivalent; NereusSDR
  currently adds a TGXL chip only. A PGXL chip follows the same pattern and can
  land as a one-line follow-up if operators request it.

---

🤖 Generated with [Claude Code](https://claude.com/claude-code)
