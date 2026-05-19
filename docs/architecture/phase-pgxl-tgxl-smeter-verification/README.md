# Phase 3P-II PGXL/TGXL + analog S-Meter bench verification

**Goal:** Confirm PowerGenius XL amplifier telemetry, TeraGenius XL tuner
control, analog S-Meter port, connection robustness, and advanced UI all
work end-to-end on real hardware against a live PGXL and TGXL unit over
Ethernet.

**Status:** Matrix drafted; bench rows pending. All rows must pass before
the Phase 3P-II PR is tagged as bench-verified, with the explicit exception
of Row 18 (HL2 ATT/filter audit gate).

**Design authority:** `docs/architecture/2026-05-18-pgxl-tgxl-and-analog-smeter-design.md`
Section 10.2 (Bench verification matrix).

**Plan:** `docs/architecture/2026-05-18-pgxl-tgxl-and-analog-smeter-plan.md`

**Branch:** `claude/jolly-golick-11c3c3`

## Coverage

36 rows across PGXL telemetry, TGXL relay control, LAN discovery, S-Meter
scaling + peak hold, pairing, keepalive + ping, save and reboot, fault
history, TX power cap, TX interlock policy, antenna labels, tune memory,
and right-click navigation. Row 18 is gated on HL2 ATT/filter audit closure
(3R precedent).

Unit tests added during Phases 1-4 verify the code paths mechanically;
bench rows exercise the on-device behaviour that only live hardware can
confirm.

## How to use

1. Build and launch a Phase 3P-II release candidate from branch
   `claude/jolly-golick-11c3c3` (or the merged equivalent).
2. Connect a known-good PGXL amplifier and TGXL tuner to the same Ethernet
   subnet as the NereusSDR host. Connect the radio (ANAN-G2 recommended for
   primary bench) to a dummy load and power meter.
3. For each row, follow the reproducer steps in order. Tick the matching
   status box when the expected behaviour is observed; if the observed
   behaviour differs, file a GitHub issue, link it from the row, and tick
   Failed.
4. Sign off the file at the bottom once every non-deferred row is Passed.

## Tester sign-off legend

Each row carries one of:

- `[ ] Untested`
- `[x] Passed YYYY-MM-DD by NAME (callsign)`
- `[x] Failed YYYY-MM-DD by NAME (callsign), issue: #N`
- `[~] Deferred (see row text)` for rows explicitly gated on follow-up work

---

## Matrix

| Row | Status | Scenario | Bench notes |
|-----|--------|----------|-------------|
| 1 | [ ] | Scan LAN happy path: PGXL + TGXL on subnet; Scan finds both within 3 s. | |
| 2 | [ ] | Scan LAN empty subnet: dialog reports "no devices found" cleanly. | |
| 3 | [ ] | Manual IP, correct host + port: Connect succeeds; Peripherals row turns green; AmpApplet / TunerApplet populate. | |
| 4 | [ ] | Manual IP, wrong port: Connect fails; row status shows "Error: Connection refused". | |
| 5 | [ ] | Manual IP, wrong host: Connect fails after socket timeout; row status shows "Error: Network unreachable" or similar. | |
| 6 | [ ] | PGXL telemetry live: keying the radio shows the AmpApplet FwdPower/SWR/Temp gauges moving; OPERATE button green. | |
| 7 | [ ] | TGXL telemetry live: TUNE button starts tune cycle; relay bars move; post-tune SWR captured into button. | |
| 8 | [ ] | OPERATE / BYPASS / STANDBY cycle on TGXL via single cycle button: state changes round-trip through TunerModel and persist on the device. | |
| 9 | [ ] | Manual relay step via mousewheel: each wheel tick advances the relay by 1 in the indicated direction. | |
| 10 | [ ] | Antenna 1/2/3 switch on TGXL 3x1: clicking each button activates the corresponding antenna on-device and updates the green-highlight on the UI. | |
| 11 | [ ] | S-Meter PGXL-aware scaling: with PGXL absent, scale is 120 W barefoot; with PGXL in STANDBY, scale stays barefoot and feed comes from radio FWDPWR; with PGXL in OPERATE, scale snaps to 2 kW and feed switches to PGXL peakfwd. | |
| 12 | [ ] | S-Meter peak hold: Fast/Medium/Slow decay rates produce the expected dB/s falloff; Reset zeroes the peak. | |
| 13 | [ ] | Auto-connect on launch: with Manual IPs configured, launching NereusSDR auto-connects to PGXL and TGXL within ~2 seconds of `MainWindow::onRadioConnected`. | |
| 14 | [ ] | TX mode / RX mode (Signal / Signal Peak / Max Bin) and peak-hold settings persist across app restart. | |
| 15 | [ ] | Right-click context menu: right-click on S-Meter shows the four submenus; current TX/RX/Decay selections are ticked; firing an action updates the widget behavior and the AppSettings key in one step. | |
| 16 | [ ] | Max Bin RX mode: tune to a slice with a clearly-strongest carrier inside the passband; switch RX mode to Max Bin; needle tracks the carrier level (often higher than the SMeter reading). Changing the filter low/high cut updates the detector window within ~100 ms; the reading follows the new window. | |
| 17 | [ ] | Max Bin smoothing: when the strongest carrier is interrupted, the reading decays at the Thetis tau=0.5s rate, not instantly. | |
| 18 | [~] | HL2 row: same as rows 6, 7, 11, 16 but on HL2. Gated on HL2 ATT/filter audit closure. | Deferred - see Notes |
| 19 | [ ] | PGXL pairing: `flexradio ampslice=A serial=NereusSDR-... txant=ANT1 ptt=LAN active=1` is sent on connect. Assert PGXL response (accept or reject) is logged and reflected in the Peripherals row status string. | |
| 20 | [ ] | PGXL keepalive: simulate a network blip (unplug ethernet for 10 s); observe NereusSDR detects within `3 * PGXL_KeepaliveSec`, marks disconnected, and auto-reconnects when network returns. | |
| 21 | [ ] | PGXL ping RTT: live PGXL connection shows non-zero RTT in PGXL Advanced -> Diagnostics; pull the cable and observe `pingTimedOut` increments `keepaliveMissed`. | |
| 22 | [ ] | PGXL save & reboot: trigger Save & Reboot from the Advanced page; confirm the modal; observe the connection drops, the amp reboots, NereusSDR auto-reconnects within 30 s, and the pairing flow re-runs cleanly. | |
| 23 | [ ] | PGXL bias mode / fan mode / LED / nickname edits: change each in PGXL Advanced, click Save & Reboot, observe the new values persist post-reboot via `readSetup`. | |
| 24 | [ ] | PGXL `ifconf address` write: change the PGXL static IP from `.43` to `.44`, click Save & Reboot, observe NereusSDR uses Scan LAN to rediscover and reconnect. | |
| 25 | [ ] | PGXL fault history: drive the amp into a synthetic FAULT (high-SWR test); observe a row appended to Fault history with correct `likelyCause`; Clear All works; entries persist across app restart. | |
| 26 | [ ] | PGXL TX power cap: set cap to 1000 W; key the radio at >1000 W; toast appears; TX continues (soft alert only). | |
| 27 | [ ] | TX interlock Disabled: out-of-box default; TX proceeds regardless of PGXL state. | |
| 28 | [ ] | TX interlock Warn: PGXL in STANDBY; key the radio; toast appears; TX proceeds. | |
| 29 | [ ] | TX interlock Block: PGXL in STANDBY; key the radio; toast appears; TX is denied; MOX does not engage. | |
| 30 | [ ] | TX interlock grace period: set to 5000 ms; key the radio during the grace window; policy does not fire even if PGXL not in OPERATE. | |
| 31 | [ ] | TX interlock SWR gate: enable, max=2.0; key into a high-SWR load; policy denies (Block) or warns (Warn) accordingly. | |
| 32 | [ ] | TGXL antenna labels: set ANT 1 label to "80 m dipole"; observe TunerApplet button text updates; persists across restart. | |
| 33 | [ ] | TGXL tune memory save: tune up on 20 m / ANT 1; click "Save current tune memory" in the TunerApplet right-click; observe TuneMemoryStore entry; restart NereusSDR and observe persistence. | |
| 34 | [ ] | TGXL tune memory auto-recall: enable `TGXL_AutoTuneMemoryRecall`; switch from 20 m to 40 m and back; observe the stored 20 m relay positions get restored (or a fresh tune triggers if absolute-write isn't supported). | |
| 35 | [ ] | Right-click AmpApplet -> "Open PGXL Advanced..." navigates to the Setup dialog at the right page. Same for TunerApplet -> "Open TGXL Advanced...". | |
| 36 | [ ] | Right-click on either applet -> "Copy diagnostics" puts a JSON blob on the clipboard with all the ConnectionDiagnostics fields. | |

---

## Notes

- **Row 18 (HL2)** is gated on the open ATT/filter safety audit, matching the
  3R precedent (see `docs/architecture/phase3r-verification/README.md` Row 9).
  Initial Phase 3P-II merge may ship without this row's green tick; document as
  a Known Limitation and close in a fast-follow once the HL2 audit is signed off.

- **Tasks 91-94** added unit tests for `readSetup` / `writeSetup` /
  `readIfconf` / `writeIfconf` / `save` lifecycle on both `PgxlConnection` and
  `TgxlConnection`. Bench rows 22-24 exercise the on-device side of those same
  code paths.

- **Row 34** ("tune memory auto-recall") uses a `tune start` placeholder pending
  bench confirmation of an absolute-relay-write verb. The design doc (section
  4.8) notes a bench caveat: if the TGXL firmware exposes an absolute-relay-
  write command, auto-recall can restore the exact stored position; otherwise
  a fresh tune cycle is triggered and the stored values serve as the comparison
  baseline. Mark Passed for either outcome; note which path was observed.

- **Row 21** is marked partial in the design spec: the Diagnostics panel portion
  (RTT display) is fully testable with a live connection; the keepaliveMissed
  increment on cable-pull requires simulating a network blip as described in
  Row 20.

---

## Sign-off

Verification owner: J.J. Boyd (KG4VCF). Phase 3P-II PR is not tagged as
bench-verified until every non-deferred row above is Passed and Row 18 either
carries a green tick or a tracking issue documenting the HL2 audit gate. Failed
rows must each carry a GitHub issue number linked from the row. Deferred rows
must carry a tracking issue or follow-up plan.
