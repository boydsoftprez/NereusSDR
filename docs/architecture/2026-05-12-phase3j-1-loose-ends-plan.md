# Phase 3J-1 TCI Server Port — Loose-Ends Plan

**Date:** 2026-05-12
**Author:** J.J. Boyd ~ KG4VCF (with AI-assisted analysis via Anthropic Claude)
**Status:** Plan — not yet executed
**Predecessor:** PR #229 commit `ccb1c5fb` (WSJT-X TCI TX audio end-to-end + live status indicator)

## Scope

Catalog of work items that remain after the WSJT-X bench unblocked TX audio
end-to-end. Each item is independently scoped, source-first-cited, and has
an explicit verification plan. Sequencing is grouped into phases so we can
ship a coherent slice at each checkpoint.

The umbrella PR #229 is currently bench-verified for the most-common TCI
client (WSJT-X 2.7.x over HL2 / Protocol 1). Phase 3J-1 was originally
scoped as "TCI server port" — these are the remaining items that bring it
to operational completeness for the broader TCI ecosystem (ESDR3, SunSDR,
JTDX, FlDigi-TCI, Log4OM, N1MM-TCI) and that close UX gaps the user
flagged during bench.

## Items

### Item 1 — Setup → CAT/Network/TCI: bind-interface dropdown (Step B)

**Goal:** Replace the read-only `127.0.0.1` label with a `QComboBox` that
enumerates the host's network interfaces and lets the operator choose:

- "Loopback only (127.0.0.1)" — default
- "Any IPv4 interface (0.0.0.0)" — exposes server to LAN
- One row per detected non-loopback IPv4 interface (e.g. `en0  192.168.1.50  (Wi-Fi)`)
- "Any IPv6 interface (::)"
- "IPv6 loopback (::1)"
- One row per detected IPv6 interface

**Source-first reads:**

- Thetis `Setup.cs:22410-22473` — `txtTCIServerBindIPPort_TextChanged` + `updateTCIPort()`. Validates `IP:port` text field with `IPAddress.TryParse`; red background on invalid; stores into `console.TCIip` (string) + `console.TCIport` (int). NereusSDR diverges on UX (dropdown instead of free-text per user decision; see CLAUDE.md `feedback_source_first_ui_vs_dsp.md` — source-first governs DSP/radio only, Qt widgets are NereusSDR-native), but persisted format (string IP + int port) is unchanged.
- Qt `QNetworkInterface::allInterfaces()` for enumeration.

**Scope (files touched):**

- `src/core/TciServer.h/.cpp`
  - Add `bool start(QHostAddress addr, quint16 port)` overload
  - Keep existing `bool start(quint16 port)` as wrapper passing `QHostAddress::LocalHost` for back-compat with the 17 existing TCI tests
- `src/gui/setup/CatNetworkSetupPages.h/.cpp`
  - Replace `m_bindIpLabel` (QLabel) with `m_bindCombo` (QComboBox)
  - On page construct, enumerate via `QNetworkInterface::allInterfaces()`; filter to UP + non-PointToPoint + has-address; populate with `userData = ip-string`
  - Restore selection from `TciServerBindAddress` AppSetting (or first row = Loopback)
  - On `currentIndexChanged`, write the userData IP back to AppSetting
  - Emit `tciServerEnableToggled(bool on, QHostAddress addr, quint16 port)` — bumped signature
- `src/gui/SetupDialog.h/.cpp`
  - Bumped signature on the forwarder signal
- `src/gui/MainWindow.cpp`
  - `tciServerEnableToggled` consumer receives both address and port, calls `m_tciServer->start(addr, port)`
- `AppSettings` — new key `TciServerBindAddress` (string, default `"127.0.0.1"`)

**Risks / open questions:**

- IPv6: some users may have IPv6 disabled — guard with `QHostAddress::protocol()` check on enumeration.
- Interface enumeration cost: `QNetworkInterface::allInterfaces()` does a syscall — call once at page construct, not on every show.
- LAN exposure has no auth — TCI protocol is unauthenticated. Add a tooltip on the combobox warning: "Binding to a non-loopback address exposes the TCI server to your network. TCI has no authentication; only enable on trusted networks."
- The 17 existing TCI tests use `server.start(0)` for ephemeral-port assignment — they're already compatible with the 1-arg overload.

**Verification:**

- Unit: extend `tst_tci_server_lifecycle` with `start(QHostAddress::Any, 0)` accept-from-loopback assertion (we can't easily test non-loopback in CI but Any binding should accept loopback connections).
- Unit: new `tst_tci_bind_address_persistence` — round-trip the setting through AppSettings + page construct/destruct.
- Bench (manual): bind to `0.0.0.0`, connect from a 2nd machine on the LAN, confirm Test CAT works. Document in `docs/architecture/2026-05-09-phase3j-1-tci-port-verification/README.md`.

**Effort estimate:** ~150 LOC + 1 new test + 1 AppSettings key. 1-2 hours.

---

### Item 2 — TciLogWindow dialog (Step C)

**Goal:** Wire the Setup → CAT/Network/TCI "Show Log..." button (currently
`setEnabled(false)`) to a real dialog that shows live TCI server message
history. Auto-scroll, pause, clear, filter-by-direction.

**Source-first reads:**

- Thetis `Setup.cs:22562-22566` — `btnShowLog_Click` opens `console.ShowTCILog()`.
- Thetis `console.cs` `ShowTCILog()` implementation (locate the form class).
- Existing NereusSDR diagnostic UI patterns — check `RadioStatusPage` / `ConnectionQualityPage` for shared widget conventions.

**Scope (files touched):**

- New `src/gui/setup/TciLogWindow.{h,cpp}` — QDialog subclass with:
  - `QPlainTextEdit` (read-only, monospace, dark-theme styled) — main log view
  - `QCheckBox` "Auto-scroll" (default on)
  - `QPushButton` "Pause"
  - `QPushButton` "Clear"
  - `QComboBox` "Filter: All / In / Out"
  - Circular ring buffer of last 10,000 entries (struct: timestamp, direction, peer, text)
  - `void appendEntry(QString dir, QString peer, QString text, qint64 epochMs)` slot
- `src/core/TciServer.h/.cpp`
  - New signal `void messageLogged(QString direction, QString peer, QString text, qint64 epochMs)`
  - Emit from `onTextMessageReceived` (direction `"in"`)
  - Emit from each `sendTextMessage` call site in the drain loop (direction `"out"`)
  - Binary frames logged as `"out: binary streamType=N length=M to peer"` (header summary only — no payload bytes in log; avoids flooding for TX_CHRONO at 47 per second)
- `src/gui/setup/CatNetworkSetupPages.cpp`
  - Enable "Show Log..." button when server is running (already done in our latest commit via `m_tciServerRunning`)
  - Click handler: lazy-construct `TciLogWindow` as a child of `qApp->activeWindow()`, connect signals, `show()` + `raise()`
- `AppSettings` — new keys:
  - `TciLogWindowAutoScroll` (bool, default True)
  - `TciLogWindowGeometry` (QByteArray) — restore window position/size

**Risks / open questions:**

- Performance: TX_CHRONO fires ~47 frames/sec. If logged, that's a constant noise floor in the log. Mitigation: TX_CHRONO frames coalesced into one "TX_CHRONO active" indicator line that updates count rather than appending; or excluded entirely from log with tooltip "TX_CHRONO timing frames suppressed."
- Cross-thread: the signal emits from the TCI server's thread (main thread for text, varies for binary path?). Use `Qt::QueuedConnection` for the window slot so emit never blocks the server.
- Window lifetime: outlive Setup dialog — owned by `MainWindow`, not by `SetupDialog`. Open via lazy-construct on first click; persistent thereafter for the app lifetime.
- Search/filter live updates with growing buffer can cost a lot — use `QPlainTextEdit::setMaximumBlockCount(10000)` for trim, keep filter as a separate proxy view if implemented.

**Verification:**

- Unit: `tst_tci_log_window` — emit 100 signals, verify window shows entries, auto-scroll works, clear empties buffer.
- Bench: open log during a WSJT-X session, see the trx/audio_start/modulation/vfo dance in real time.

**Effort estimate:** ~250 LOC (new file) + ~30 LOC of TciServer signal plumbing + 1 test. 2-3 hours.

---

### Item 3 — RadioModel Q_INVOKABLE long tail [CLOSED 2026-05-12]

**Closeout summary:** 56 production shims added in `RadioModel.h/.cpp`
covering VFO lock, mute (per-slice + global), filter, AGC mode/gain,
squelch, RIT/XIT, balance, CTUN, NB/NR/ANF, RX enable, AF/MON linear
volumes, IQ sample rate, audio stream config, TX profile, and 5
calibration getters.  Real-state shims route to SliceModel
Q_PROPERTYs (locked / muted / filterLow/High / agcMode / agcThreshold /
ssqlEnabled / ssqlThresh / audioPan / ritEnabled/Hz / xitEnabled/Hz /
nbMode / activeNr) or to `RadioModel::activeSlice()` for radio-global
RIT/XIT.  Stub shims (CTUN / rxBin / rxApf / rxNf / rxEnable / rxAnf /
afLinear / monLinear / iqSampleRate / audio-stream config) store and
return the last-set value via small per-slice arrays or RadioModel
fields, keeping the operator's "I set X to 7" expectation true even
when the underlying WDSP feature isn't wired yet.  Calibration getters
return 0.0 ("no calibration applied") until a CalibrationModel lands.
TX profile shims route to MicProfileManager via setActiveProfile +
activeProfileName + profileNames.

**Verification:** new `tst_tci_radio_model_shims` (18 category-level
tests) invokes each shim category via `QMetaObject::invokeMethod` and
asserts the underlying model state changed -- same dispatch path
TciProtocol uses in production.  Out-of-range-slice safety also pinned
(silently no-ops, no segfault).  Matrix parity against TestMockRadio
Model remains covered by `tst_tci_matrix_runner`.

**Original goal (kept for history):**
Provide working production paths for the ~67 named methods that
`TciProtocol.cpp` calls via `QMetaObject::invokeMethod(m_radio, "...", ...)`.
Today only the WSJT-X minimum is wired: `setMox`/`mox`, `setVfoHz`/`vfoHz`,
`setMode`/`mode`, `setSplit`/`split`. The remaining names silently no-op
against the real `RadioModel` (they only pass against `TestMockRadioModel`,
which is what the 80+ matrix test rows exercise).

**Source-first reads:**

Per-shim citation pattern. Most are 1:1 mappings to existing `SliceModel`
Q_PROPERTYs that just need `RadioModel`-level routing through `sliceAt(rx)`.
A few require new state.

Enumerated by `grep -nE 'invokeMethod' src/core/TciProtocol.cpp` (see PR
#229 history for the full list). Categories:

| Category | Methods | Existing state | Cite |
|---|---|---|---|
| AGC mode/gain | `setAgcMode`, `agcMode`, `setAgcGain`, `agcGain` | SliceModel Q_PROPERTY exists | Thetis `TCIServer.cs:3997-4031` |
| Squelch | `setSqlEnable`, `sqlEnable`, `setSqlLevel`, `sqlLevel` | SliceModel `ssqlEnabled`/`ssqlThresh` close but not exact | Thetis `TCIServer.cs:4192-4230` |
| RIT/XIT | `setRitEnable`, `ritEnable`, `setRitOffset`, `ritOffset`, XIT same | SliceModel `ritEnabled`/`ritHz` exists | Thetis `TCIServer.cs:4032-4096` |
| DSP toggles | `setRxNb`, `setRxBin`, `setRxApf`, `setRxNf`, `setRxAnf`, `setRxNr` + getters | SliceModel has all | Thetis `TCIServer.cs:3960-3996` |
| Balance | `setRxBalance`, `rxBalance` | SliceModel `audioPan` is close | Thetis `TCIServer.cs:4097-4119` |
| Mute | `setRxMute`, `rxMute`, `setGlobalMute`, `globalMute` | SliceModel `muted` exists; global is new | Thetis `TCIServer.cs:4120-4151` |
| Filter band | `setFilterBand` | SliceModel `filterLow`/`filterHigh` exists | Thetis `TCIServer.cs:4366-4413` |
| TX profile | `setTxProfile`, `txProfile`, `txProfilesList` | MicProfileManager exists | Thetis `TCIServer.cs:4413-4445` |
| Audio config | `setAudioSampleRate`, `audioSampleRate`, `setAudioStreamChannels`, `setAudioStreamSamples`, `setAudioStreamSampleType` | Server-side, lives in TciServer | Thetis `TCIServer.cs:5012-5085` |
| Volume | `setAfLinear`, `afLinear`, `setMonLinear`, `monLinear` | SliceModel `afGain` (int 0-100) — need linear getter | Thetis `TCIServer.cs:4152-4191` |
| CTUN | `setRxCtun`, `rxCtun` | Slice/Pan state | Thetis `TCIServer.cs:4267-4291` |
| RX enable | `setRxEnable`, `rxEnable` | New — second-RX state | Thetis `TCIServer.cs:4231-4250` |
| Calibration | `calibrationDisplay/Meter/SixMeter/TxDisplay/Xvtr` (getters only) | New — calibration state stub | Thetis `TCIServer.cs:4446-4486` |
| VFO lock | `setVfoLock`, `vfoLock`, `setLock`, `lock` | SliceModel `locked` exists | Thetis `TCIServer.cs:4292-4322` |

**Scope (files touched):**

- `src/models/RadioModel.h/.cpp` — ~67 new Q_INVOKABLE shims, mostly 2-5 lines each routing to `sliceAt(rx)` or `m_transmitModel`
- For categories that need new state (`globalMute`, calibration getters, `rxEnable` for slice 2): minimal stub returning sensible defaults until the underlying feature is wired
- `src/models/RadioModel.cpp` constructor: ensure no signals need re-wiring

**Risks / open questions:**

- Some shims have side effects beyond the obvious (e.g., `setRxCtun` retunes the DDC; `setFilterBand` triggers WDSP setFilterFreqs). These already work via the SliceModel Q_PROPERTY WRITE path — exposing via Q_INVOKABLE just provides an additional entry point.
- The matrix-runner tests (`tst_tci_matrix_runner`) already assert byte-for-byte parity against `TestMockRadioModel` for all 80+ rows. Adding production shims doesn't break these tests — they test the protocol layer, not the model.
- A real RadioModel test for each shim (not just mock) would be ideal but is out of scope for Phase 3J-1 closeout — defer to a follow-up "TCI production-path coverage" plan.

**Verification:**

- Unit: each shim category gets one test in `tst_tci_radio_model_shims.cpp` (new) — invoke via `QMetaObject::invokeMethod` against a real RadioModel, verify the underlying SliceModel property changed.
- Bench: connect ESDR3 / N1MM / Log4OM and confirm RIT/XIT/AGC/SQL controls work.

**Effort estimate:** ~350 LOC (mostly mechanical), ~200 LOC test. 4-6 hours.

---

### Item 4 — Per-band-per-mode `LastFilter` persistence [CLOSED 2026-05-12]

**Closeout summary:** new `bandModePrefix(slice, band, mode)` helper
produces a `Slice<N>/Band<B>/Mode<M>/` key namespace.  `setDspMode` now
saves the current filter under (currentBand, OLD mode) before the mode
swap and restores from (currentBand, NEW mode) after, falling back to
`defaultFilterForMode` only when no persisted value exists.
`saveToSettings(band)` writes filter to both the legacy `(band)/` path
(backward-compat with code that reads it directly) and the new
`(band, mode)/` path; `restoreFromSettings(band)` prefers the
(band, mode) path with legacy fallback.  Schema-stable: existing
pre-Item-4 settings files keep working via the legacy fallback;
forward writes go to both paths.  No version bump needed.

DIGU/DIGL F1 (3 kHz) workaround from the 3J-1 bench fix is reverted to
Thetis F5 (1.2 kHz) defaults now that persistence remembers the
operator's first widening.  Matches upstream `console.cs:5286 / 5328
[v2.10.3.13]` byte-for-byte.

**Verification:** new `tst_slice_filter_per_band_per_mode_persistence`
covers (a) mode-change roundtrip within a single band: USB 200/2900 ->
DIGU defaults -> DIGU 1400/1700 -> USB 200/2900 restored -> DIGU
1400/1700 restored; (b) unset-mode fallback to `defaultFilterForMode`;
(c) per-band isolation: same mode on 20m and 40m maintains separate
filter memories.  35/35 slice + TCI tests pass.

**Original goal (kept for history):**
When the user adjusts the filter for a band+mode combination,
persist that filter. When a mode change occurs (whether from UI or from
TCI `modulation:N,M;`), restore the persisted filter for the current
band+mode instead of slamming a hardcoded default. Mirrors Thetis's
`preset[m].LastFilter` machinery.

This removes the F1-default workaround we currently apply in
`SliceModel::defaultFilterForMode` for DIGU/DIGL.

**Source-first reads:**

- Thetis `console.cs:14653-14671` — `rx1_filters[(int)DSPMode.DIGU].SetFilter(f, low, high, name)` + `preset[m].LastFilter = Filter.F5;` initialization pattern.
- Thetis `console.cs` — `RX1Filter` setter (locate) — applies `LastFilter` on mode change.
- Existing NereusSDR per-band-per-slice persistence: `SliceModel::saveToSettings(band)` / `restoreFromSettings(band)` — already persists filterLow/filterHigh per-band. We need to extend with mode dimension.

**Scope (files touched):**

- `src/models/SliceModel.h/.cpp`
  - `saveToSettings(band, mode)` — writes filter to band+mode slot
  - `restoreFromSettings(band, mode)` — reads filter from band+mode slot
  - `setDspMode(mode)` — on mode transition, save filter under (current band, OLD mode), restore filter for (current band, NEW mode) — instead of applying `defaultFilterForMode`
  - `frequencyChanged` signal handler in RadioModel — on band crossing, save filter under (OLD band, current mode), restore for (NEW band, current mode)
- `AppSettings` — key namespace `Slice<N>/Band<B>/Mode<M>/FilterLow` + `FilterHigh`
- `src/models/SliceModel.cpp::defaultFilterForMode` — keep as fallback when no persisted value exists; revert DIGU/DIGL workaround back to Thetis F5 default since persistence now handles the long-term value

**Risks / open questions:**

- Schema migration: existing per-band keys (no mode dimension) shouldn't be lost. Migration path: on first launch with new schema, copy existing `Slice<N>/Band<B>/FilterLow` to `Slice<N>/Band<B>/Mode<USB>/FilterLow` (USB is the typical pre-3J-1-port default mode).
- Rapid mode changes via TCI: existing `scheduleSettingsSave()` coalesces writes — no extra throttling needed.
- Test coverage: 14 bands × 11 modes = 154 distinct slots. Spot-check tests on a few representative combinations; full round-trip on band-mode crossings.

**Verification:**

- Unit: `tst_slice_filter_per_band_per_mode_persistence` (new) — set filter on USB+20m, switch to DIGU+20m, switch back, verify USB filter restored.
- Unit: schema migration test — pre-populate v6-shaped keys, run AppSettings load, verify v7-shaped keys exist.
- Bench: WSJT-X on 20m DIGU sets a wider filter via manual click-tune, restart NereusSDR, verify filter restored on next launch.

**Effort estimate:** ~150 LOC + 1 test + schema migration. 3-4 hours.

---

### Item 5 — `tx_stream_audio_buffering` honored

**Goal:** The Setup → CAT/Network/TCI → Audio Stream → "TX stream buffering" spinbox should drive the value emitted in the init burst's `tx_stream_audio_buffering:N;` line. Currently hardcoded `50`.

**Source-first reads:**

- Thetis `TCIServer.cs` — `m_txStreamAudioBufferingMs` field
- Existing AudioTciPage UI (if it has the control already)

**Scope:**

- `src/core/TciProtocol.cpp::buildInitBurst` — read `AppSettings::value("TciTxStreamBufferingMs", 50)` instead of hardcoded `50`
- `src/gui/setup/CatNetworkSetupPages.cpp` (or `AudioTciPage`) — Setup spinbox writes to `TciTxStreamBufferingMs`
- New AppSettings key: `TciTxStreamBufferingMs` (int, default 50, range 0..500)

**Verification:**

- Unit: change setting, build init burst, assert line value matches.
- Bench: optional — exercise via WSJT-X and observe whether different values affect WSJT-X behavior.

**Effort estimate:** ~30 LOC. 30 minutes.

---

### Item 6 — CW pitch from AppSettings

**Goal:** `SliceModel::defaultFilterForMode` uses `static constexpr int kCwPitch = 600` for CW filter center. This should read from AppSettings (`CWPitch` key — already used elsewhere in the codebase for the local CW path).

**Source-first reads:**

- Thetis `console.cs::CWPitch` property setter — fires `RX1Filter` reset for CWL/CWU modes
- NereusSDR existing CW pitch read sites — grep for `CWPitch` to find existing pattern

**Scope:**

- `src/models/SliceModel.cpp::defaultFilterForMode` — replace `kCwPitch` constant with `AppSettings::value("CWPitch", 600).toInt()`
- Probably already has setter elsewhere — just unify the read

**Verification:**

- Unit: `tst_slice_filter_cw_pitch_from_settings` — set CWPitch to 800, call defaultFilterForMode(CWL), assert range is `[-1000, -600]` (instead of `[-800, -400]`).
- Bench: tune to a CW signal, change CW pitch in setup, verify filter recenters.

**Effort estimate:** ~15 LOC. 15 minutes.

---

### Item 7 — Settings-purge investigation [CLOSED 2026-05-12]

**Investigation outcome (commit pending):**
There is NO key-purge mechanism in the NereusSDR codebase.  Audit of
`AppSettings::load` / `save` / `setValue` / `remove` and all
`m_settings.remove(` callers (5 total) found that every removal path
is explicitly scoped: `remove(key)` is per-key API, `forgetRadio` is
`hardware/<mac>/`, `clearSavedRadios` is `radios/`, `clearHardware
Values` is `hardware/<mac>/`.  None of these can touch top-level keys
like `TciEmulateSunSDR2Pro`.  The user's memory note appears to refer
to a removed code path or a misperception.

The "keys vanished" symptom was almost certainly "keys were never
persisted in the first place".  Pre-fix code read the keys via
`value(..., "False")` and got the default; without a `setValue` write
the keys never reached `m_settings`, so `save()` never wrote them, so
the next launch saw no keys and returned the default again.  The
seed-on-construct fix at `TciServer.cpp:107-114` writes both keys to
`m_settings` on first launch, after which they persist.

**Pinned via regression test:** `tst_app_settings_arbitrary_key_
persistence` — 4 subtests covering arbitrary user key round-trip,
TciEmulate* round-trip, empty-string-value round-trip (catches
"empty == absent" parser quirks), and confirmation that scoped removal
paths (`forgetRadio` / `clearHardwareValues`) don't touch unrelated
top-level keys.  Any future regression that adds a filter to
`AppSettings::save` or `load` will break this test.

**Original goal (kept for history):**
Identify the mechanism by which the user's pre-edited
`TciEmulateSunSDR2Pro` and `TciEmulateExpertSDR3Protocol` keys vanished
from the settings file. Our seed-on-construct workaround in
`TciServer::TciServer` mitigates the symptom but doesn't address the root
cause — other settings keys could be affected.

The user's memory note "NereusSDR purges unknown keys on save" wasn't
substantiated by my Phase 3J-1 code audit — `AppSettings::save()` writes
`m_settings` directly with no filter. Either:

(a) The memory note was about a different code path that's since been
removed.

(b) Some other component (`SettingsHygiene`, a UI-driven reset, or a
migration path) is removing keys.

(c) The keys never made it into `m_settings` at load time (e.g., load
silently skipped them due to a parser quirk).

**Source-first reads:** N/A (NereusSDR-original).

**Scope (investigation, then fix):**

- `src/core/AppSettings.cpp::load()` — instrument with a diagnostic that
  lists keys-loaded-but-not-in-known-schema, see if anything obvious is
  filtered
- `src/core/SettingsHygiene.cpp` — confirm scope is `hardware/<mac>/...`
  only
- `git log` on AppSettings.cpp + SettingsHygiene.cpp — see if any past
  commit explicitly added a key-purge mechanism
- Search for `m_settings.remove(` callers in production code
- Reproduce: pre-edit a known-good test setting, launch the app, exit
  cleanly, verify it persists. If it doesn't, instrument the save path.

**Verification:**

- Add `tst_app_settings_arbitrary_key_persistence.cpp` — write an unknown
  key, save, reload, assert key still present.

**Effort estimate:** 2-4 hours investigation + maybe 1-2 hours fix.

---

## Phasing / sequencing

### Phase 1 (next session, ~5-6 hours of focused work)

| Item | Effort | Bench |
|---|---|---|
| **Item 1** — Bind-interface dropdown | 1-2h | LAN test required |
| **Item 2** — TciLogWindow | 2-3h | Manual UI test |
| **Item 5** — `tx_stream_audio_buffering` honored | 30m | None |
| **Item 6** — CW pitch from settings | 15m | None |

Output: PR #229 grows by Items 1+2+5+6, becomes the "Phase 3J-1 closeout"
PR. Status indicator (already done) + bind dropdown + log window + the two
small honored-setting fixes give us a polished operator-facing TCI panel.

### Phase 2 (follow-up PR after #229 merges)

| Item | Effort | Bench |
|---|---|---|
| **Item 3** — Q_INVOKABLE long tail | 4-6h | ESDR3/N1MM/Log4OM tests |
| **Item 7** — Settings purge investigation | 2-4h+fix | Round-trip test |

Output: New PR titled "Phase 3J-1 follow-up: TCI production-path coverage
+ AppSettings persistence audit". Opens up TCI for clients beyond WSJT-X.

### Phase 3 (separate follow-up — larger architectural change)

| Item | Effort | Bench |
|---|---|---|
| **Item 4** — Per-band-per-mode LastFilter persistence | 3-4h | Filter restore on band crossing |

Output: New PR. Removes the F1 DIGU/DIGL workaround we have today.
Requires its own design doc on schema migration since 14 bands × 11 modes
× 4 slices is a substantial settings-namespace change.

## Open questions

1. **LAN exposure tooltip wording (Item 1):** what level of warning is
   appropriate? "TCI has no authentication" is factually correct but may
   alarm casual users binding to LAN for a single shack PC. Recommend:
   one-line tooltip on the combobox, not a modal warning.

2. **Log window scope (Item 2):** should we log binary frame *contents*
   (first N bytes of payload) or just headers? Recommend: headers only,
   with a "verbose" checkbox for binary payload hex dump (off by default
   — TX_CHRONO would drown the window otherwise).

3. **Q_INVOKABLE shim test discipline (Item 3):** unit per shim, or one
   sweep test that hits every method via the matrix runner against a real
   RadioModel? Recommend: matrix runner extension — already tests every
   protocol command, just point it at a real RadioModel instead of the
   mock.

4. **Settings purge scope (Item 7):** if we find the root cause, do we
   fix it in this PR or defer to a separate hygiene PR? Recommend: defer
   to a hygiene PR — fixing AppSettings affects every NereusSDR feature,
   not just TCI.

## Done criteria for Phase 3J-1 closeout

- [ ] All seven items above land or are explicitly deferred to a tracked
      follow-up issue.
- [ ] WSJT-X bench verification matrix in
      `docs/architecture/2026-05-09-phase3j-1-tci-port-verification/README.md`
      updated with results from each item that has a bench step.
- [ ] CI green on macOS / Linux / Windows.
- [ ] All 17+ TCI tests + new tests pass.
- [ ] PR #229 description updated to reflect closeout scope.
- [ ] CHANGELOG.md entry for v0.4.x mentioning TCI Server reaches
      operational parity.

## NereusSDR divergences from Thetis (documented)

Several deliberate divergences accumulated in this work. For PR review
clarity:

1. **Bind UX**: dropdown (NereusSDR) vs `IP:port` text field (Thetis) —
   per CLAUDE.md `feedback_source_first_ui_vs_dsp.md`, control-surface UX
   is NereusSDR-native; persisted value format unchanged.

2. **SunSDR2PRO emulation default**: True (NereusSDR) vs False (Thetis) —
   NereusSDR is not Thetis; the SunSDR2PRO identifier is the safer
   default for the most-common TCI clients.

3. **DIGU/DIGL hardcoded default filter**: F1 (NereusSDR) vs F5 (Thetis)
   — temporary; reverts to Thetis F5 once Item 4 (per-band-per-mode
   LastFilter persistence) lands.

4. **TX_CHRONO timing**: monotonic-accumulator + 5ms poll (AetherSDR
   pattern) — chosen over a fixed-period QTimer to avoid the 1.6%
   timer drift that Thetis tolerates and that warps WSJT-X FT8 tones.
