# TCI Port (Phase 3J-1) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` (recommended) or `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port Thetis TCI (Transceiver Control Interface) into NereusSDR with full Thetis parity, default-enabled UI, hybrid AetherSDR Qt6 transport + Thetis-faithful protocol layer.

**Architecture:** Single TCI thread runs `TciServer` (Qt6 `QWebSocketServer` + multi-client lifecycle + 3 priority send queues + VFO coalescing thread + binary audio/IQ pipelines + sensor timers) and `TciProtocol` (transport-blind, two dispatch tables, 62 commands, 12 compat flags). Slice A/B/C/D ↔ `trx:N` mapping at protocol-layer boundary. Live-wires existing `m_tciIndicator` scaffolding at MainWindow.cpp:3179.

**Test discipline:** **Verification matrix is the load-bearing test artifact** — one CSV row per Thetis behavior, one runner asserts byte-for-byte parity. TDD only for pure-logic units (priority queue, VFO coalescer, slice mapping, dB volume math, sensor formats, ring buffer, resampler lifecycle, TX mutex). Integration tests for I/O pipelines (audio + WebSocket round-trip). Visual + smoke for UI.

**Tech Stack:** Qt6 + qt6-websockets + WDSP resampler + AppSettings (XML, NOT QSettings).

---

## Reference (every task points here)

| Resource | Location | Version |
|---|---|---|
| **Design doc** | `docs/architecture/2026-05-09-phase3j-1-tci-port-design.md` | committed `41427bb` (renamed during plan write) |
| **Thetis source** | `/Users/j.j.boyd/Thetis/Project Files/Source/Console/TCIServer.cs` | `[v2.10.3.13 @501e3f51]` |
| **AetherSDR transport ref** | `/Users/j.j.boyd/AetherSDR/src/core/TciServer.{h,cpp}` + `TciProtocol.{h,cpp}` | `[@0cd4559]` |
| **WDSP resampler** | `third_party/wdsp/resample.c` | TAPR v1.29 |
| **Sweep A** (dispatch table, 62 commands) | `.superpowers/tci-sweeps/tci-sweep-A-dispatch.md` | persisted (gitignored) |
| **Sweep B** (notifications, errors) | `.superpowers/tci-sweeps/tci-sweep-B-notifications-errors.md` | persisted (gitignored) |
| **Sweep C** (threading, transport) | `.superpowers/tci-sweeps/tci-sweep-C-threading-transport-misc.md` | persisted (gitignored) |
| **Sweep D** (state, init burst, compat) | `.superpowers/tci-sweeps/tci-sweep-D-client-state-init-compat.md` | persisted (gitignored) |
| **Sweep E** (binary, sensors) | `.superpowers/tci-sweeps/tci-sweep-E-binary-sensors.md` | persisted (gitignored) |

## Standing rules every task obeys

1. **READ → SHOW → TRANSLATE** for any Thetis-derived logic. STOP-AND-ASK if a Thetis cite cannot be located. Never infer or fabricate.
2. **Inline `// From Thetis <file>:<line> [v2.10.3.13]` cite** above every ported logic block. AetherSDR refs use `[@0cd4559]`.
3. **Byte-for-byte header copy** for every new file ported from Thetis. Multi-file ports use `// --- From [filename] ---` separators. Templates: see Phase 0 setup notes.
4. **Inline author tag preservation**: every `//MW0LGE`, `//W2PA`, `//DH1KLM`, `//G8NJJ`, `//MI0BOT`, `//-W2PA`, `//[2.10.3.13]MW0LGE` etc. preserved verbatim within ±5 lines of the cited Thetis source line.
5. **No source cites in user-visible strings**. Tooltips, button captions, dialog text, log strings stay plain English. Cites go in source comments next to the string assignment.
6. **GPG-sign every commit** (default `commit.gpgsign true`). Never `--no-gpg-sign`.
7. **Slice A/B/C/D in UI; `trx:N` in wire format**. Mapping at protocol-layer boundary.
8. **No CI-unblock stubs**. Real implementation or ASK first.
9. **Test discipline by code shape:**
   - **Plumbing handlers (62 commands)**: matrix-driven. Add row(s) to `tests/data/tci/matrix.csv`, implement handler, run matrix runner, commit. **No per-handler unit test** — the matrix is the spec.
   - **Pure logic** (queues, coalescer, mapping, math, format strings): TDD (failing test → impl → pass → commit).
   - **I/O pipelines** (audio, WebSocket, sensors): one integration test per pipeline (round-trip).
   - **UI**: `STANDARD_GUI_VERIFY` (rule 11) → commit. No unit test.
10. **Self-review for new-file ports** (only when adding a file ported from Thetis): Thetis source quoted in commit body? Author tags within ±5 lines? Cite version-stamped? Byte-for-byte header? Constants traceable to Thetis line? — Tasks that don't add a new ported file skip this checklist.
11. **`STANDARD_GUI_VERIFY` macro** (referenced by every GUI phase):
    ```bash
    # 1. Build
    cmake --build /Users/j.j.boyd/NereusSDR/.worktrees/phase3j-1-tci-server-port/build -j$(sysctl -n hw.ncpu 2>/dev/null || nproc)
    # 2. Kill + relaunch
    pkill -9 -f NereusSDR && sleep 2
    # macOS:    open /Users/j.j.boyd/NereusSDR/.worktrees/phase3j-1-tci-server-port/build/NereusSDR.app
    # Linux:    /Users/j.j.boyd/NereusSDR/.worktrees/phase3j-1-tci-server-port/build/NereusSDR &
    # Windows:  start "" "C:\path\to\worktree\build\NereusSDR.exe"
    sleep 3
    # 3. Verify binary is from this worktree
    # macOS/Linux: nm <path>/NereusSDR | grep <new-symbol-from-this-task>
    # Windows:     dumpbin /symbols <path>\NereusSDR.exe | findstr <new-symbol>
    # 4. Screenshot for visual confirmation
    # macOS:   osascript -e 'tell application "NereusSDR" to activate' && sleep 1 && screencapture -x /tmp/nereus-tci-<task>.png
    # Linux:   wmctrl -a NereusSDR && sleep 1 && gnome-screenshot -f /tmp/nereus-tci-<task>.png
    # Windows: Use Snipping Tool / Win+Shift+S → save to %TEMP%\nereus-tci-<task>.png
    # 5. Read the screenshot file via the Read tool to verify against the mockup.
    ```

---

## Phase 0: Foundation

### Pre-task setup notes (no commits, just reference)

**Header templates** for every new file (Tasks below paste the relevant one):

- **Template A** (single-source Thetis port): paste `TCIServer.cs` first 40 lines verbatim, append `// Ported from Thetis v2.10.3.13 (commit 501e3f51) — TCIServer.cs` + modification-history block per `docs/attribution/HOW-TO-PORT.md`.
- **Template B** (multi-source port): each source file's header verbatim, separated by `// --- From [filename] ---`, then modification history.
- **Template C** (NereusSDR-original, no Thetis upstream): standard NereusSDR copyright + GPLv2-or-later block + modification history. No `Ported from` line.

Capture once at session start: `sed -n '1,40p' "/Users/j.j.boyd/Thetis/Project Files/Source/Console/TCIServer.cs"` → save to local clipboard for paste.

**AppSettings keys** (17, documented in `TciProtocol.h` comment block during Task 1.1): keys table per design doc Section 10.

### Task 0.1: Add qt6-websockets dependency + NEREUS_THETIS_DIR

**Files:** `CMakeLists.txt`, `CLAUDE.md`, `CONTRIBUTING.md`, env-setup file (`.envrc` or shell profile reference)

- [ ] Add `WebSockets` to root `find_package(Qt6 REQUIRED COMPONENTS ...)` and `Qt6::WebSockets` to `target_link_libraries(NereusSDR PRIVATE ...)`.
- [ ] Add `qt6-websockets` (Arch) / `qt6-websockets-dev` (Debian/Ubuntu) to CLAUDE.md and CONTRIBUTING.md deps lines.
- [ ] **Set `NEREUS_THETIS_DIR=/Users/j.j.boyd/Thetis`** so the `verify-inline-tag-preservation.py` pre-commit hook can find Thetis source. Add to `.envrc` if direnv is in use, or document in CONTRIBUTING.md as required setup. Without this, the hook silently SKIPS (caught during the design-doc commit) and dropped author tags will not be flagged locally — only in CI.
- [ ] Verify clean build:
  ```bash
  cmake --build /Users/j.j.boyd/NereusSDR/.worktrees/phase3j-1-tci-server-port/build -j$(sysctl -n hw.ncpu 2>/dev/null || nproc)
  ```
- [ ] Commit:
  ```
  build(tci): add qt6-websockets dep + NEREUS_THETIS_DIR for tag-preservation hook
  ```

### Task 0.2: Create lcTci logging category

**Files:** `src/core/TciLogging.h`, `src/core/TciLogging.cpp`

No test (TDD theater for a 4-line constant declaration; the compiler enforces existence).

- [ ] Create `TciLogging.h`:
  ```cpp
  #pragma once
  #include <QLoggingCategory>
  Q_DECLARE_LOGGING_CATEGORY(lcTci)
  ```
- [ ] Create `TciLogging.cpp`:
  ```cpp
  #include "TciLogging.h"
  Q_LOGGING_CATEGORY(lcTci, "nereus.tci")
  ```
- [ ] Verify by building: `cmake --build .../build -j$(sysctl -n hw.ncpu 2>/dev/null || nproc)`. Build succeeds → category compiles + links.
- [ ] Commit: `feat(tci): add lcTci logging category`

---

## Phase 1: Verification matrix (load-bearing from Phase 3 onward)

### Task 1.1: Matrix.csv schema + parser + runner + tooling for README generation

**Files:**
- Create: `tests/data/tci/matrix.csv` (header row only; data rows added per phase)
- Create: `tests/tci/test_tci_matrix_runner.cpp`
- Create: `tests/tci/TestMockRadioModel.h` (minimal mock, extended per phase)
- Create: `scripts/gen-tci-matrix-readme.py` (CSV → markdown)
- Modify: `tests/CMakeLists.txt`

**Single source of truth**: `matrix.csv` is canonical. The human-facing markdown README (created in Task 1.2) is **auto-generated** by `gen-tci-matrix-readme.py` from CSV + a small front-matter file. Implementers update CSV only; README is regenerated at end-of-epic (Phase 27).

CSV schema:
```csv
command,input,expected_response,expected_notifications,thetis_cite,notes
```

`SETUP:` pseudo-prefix in `input` sets AppSettings before the actual command (multiple `SETUP:` lines + final command line all in one cell, separated by `;;`). Document this convention in the runner's docstring.

**TestMockRadioModel** starts minimal: `setVfoHz(slice, chan, hz)`, `vfoHz(slice, chan)`, `setMode(slice, mode)`, `mode(slice)`, `setMox(on)`, `mox()`, `resetToBaseline()`. Each subsequent command-family phase that needs a new accessor extends the mock and notes the addition in its commit message. Aim: ~30 accessors total by end of Phase 14.

- [ ] **Failing test** (matrix runner — passes vacuously with empty CSV; verifies file loads + parses):
  ```cpp
  #include <QtTest>
  #include <QFile>
  #include <QTextStream>
  #include "TciProtocol.h"
  #include "TestMockRadioModel.h"

  class TestTciMatrixRunner : public QObject {
      Q_OBJECT
      struct Row { QString command, input, expectedResponse, expectedNotifs, cite, notes; };
  private:
      QList<Row> loadMatrix() {
          QFile f(QStringLiteral(NEREUS_TEST_DATA_DIR "/tci/matrix.csv"));
          if (!f.open(QIODevice::ReadOnly)) { return {}; }
          QTextStream ts(&f);
          ts.readLine();   // skip header
          QList<Row> rows;
          while (!ts.atEnd()) {
              const QStringList parts = ts.readLine().split(QLatin1Char(','));
              if (parts.size() < 6) { continue; }
              rows.append({parts[0], parts[1], parts[2], parts[3], parts[4], parts[5]});
          }
          return rows;
      }
  private slots:
      void allRowsPass() {
          TestMockRadioModel mock;
          TciProtocol p(&mock);
          for (const auto& r : loadMatrix()) {
              mock.resetToBaseline();
              // SETUP: prefix handling for AppSettings preconditions
              QString actualInput = r.input;
              QStringList parts = actualInput.split(QStringLiteral(";;"));
              for (int i = 0; i < parts.size() - 1; ++i) {
                  if (parts[i].startsWith(QStringLiteral("SETUP:"))) {
                      const QString kv = parts[i].mid(6);
                      const int eq = kv.indexOf('=');
                      AppSettings::instance().setValue(kv.left(eq), kv.mid(eq + 1));
                  }
              }
              const QString actualResponse = p.handleCommand(parts.last());
              QStringList notifs;
              while (p.hasPendingNotification()) { notifs.append(p.takePendingNotification()); }
              QCOMPARE(actualResponse, r.expectedResponse);
              QCOMPARE(notifs.join(QStringLiteral("|")), r.expectedNotifs);
          }
      }
  };
  QTEST_GUILESS_MAIN(TestTciMatrixRunner)
  #include "test_tci_matrix_runner.moc"
  ```
- [ ] Create empty `tests/data/tci/matrix.csv` (header row only).
- [ ] Create `TestMockRadioModel.h` with the minimal accessor set listed above.
- [ ] Create `scripts/gen-tci-matrix-readme.py` (~30 lines: read CSV, group by family from notes column, emit markdown table). Won't be RUN until Phase 27; lives in tree from now.
- [ ] Run runner; passes vacuously.
- [ ] Commit: `test(tci): matrix.csv runner scaffold + TestMockRadioModel + readme generator`

### Task 1.2: Verification matrix README placeholder

**Files:** `docs/architecture/2026-05-09-phase3j-1-tci-port-verification/README.md`

- [ ] Create README with section headers (Commands / Notifications / Init Burst / Compat Flags / Integration Smoke / Sign-off) and a one-line note: *"This README is auto-generated by `scripts/gen-tci-matrix-readme.py` from `tests/data/tci/matrix.csv` at end-of-epic. DO NOT edit by hand mid-PR."* Integration-smoke section is hand-maintained.
- [ ] Commit: `docs(tci): verification matrix README scaffold`

---

## Phase 2: TciServer skeleton

### Task 2.1: TciServer + ClientSession + connect/disconnect lifecycle

**PORT FROM:** AetherSDR `src/core/TciServer.{h,cpp} [@0cd4559]` (transport pattern); per-client fields from Thetis `TCIServer.cs:684-790 [v2.10.3.13]`.

**READ FIRST:** AetherSDR transport files; Sweep D "Per-client state"; Sweep C "Threading model".

**Files:** `src/core/TciServer.{h,cpp}`, `src/core/TciClientSession.h`, `tests/tci/test_tci_server_lifecycle.cpp`

- [ ] **Failing test:** `start(0)` returns true → `isRunning()` true; `serverStarted` signal fires; double-start rejected; `stop()` clears running.
- [ ] Create `TciClientSession.h` with the field set from Sweep D (subset our protocol needs; see design doc Section 2.1). Apply Template C (NereusSDR-original — we condense). Cite `TCIServer.cs:684-790 [v2.10.3.13]` per group of fields.
- [ ] Create `TciServer.h` with: `start(quint16)`, `stop()`, `isRunning()`, `port()`, `clientCount()`, signals (`serverStarted/Stopped`, `clientConnected/Disconnected`, `errorOccurred`), private slots (`onNewConnection`, `onClientDisconnected`, `onTextMessageReceived`, `onBinaryMessageReceived`). Apply Template C.
- [ ] Create `TciServer.cpp` with bind logic (`QHostAddress::LocalHost` per Q7), client session table (`QHash<QWebSocket*, std::shared_ptr<ClientSession>>`), connect/disconnect handlers + log. Stub message handlers with TODO comments.
- [ ] Run test → PASS. Self-review per standing rule 10 (this task adds Thetis-derived ported headers).
- [ ] Commit: `feat(tci): TciServer + ClientSession skeleton with lifecycle`

### Task 2.2: 20-second server-driven ping with payload "Thetis"

**PORT FROM:** Thetis `TCIServer.cs:2650-2654 [v2.10.3.13]`.

**Files:** modify `TciServer.{h,cpp}`; `tests/tci/test_tci_server_ping.cpp`.

- [ ] **Failing test:** real `QWebSocket` client; `setPingIntervalMs(200)`; assert ≥ 2 pongs in 3s.
- [ ] Add `QTimer* m_pingTimer` + `setPingIntervalMs(int)` (default 20000ms). Cite `TCIServer.cs:2650-2654 [v2.10.3.13]`. On tick, iterate clients, `sock->ping(QByteArrayLiteral("Thetis"))`. Start in `start()`, stop in `stop()`.
- [ ] Run test → PASS.
- [ ] Commit: `feat(tci): server-driven 20s ping with payload "Thetis"`

---

## Phase 3: TciProtocol skeleton + dispatch

### Task 3.1: TciProtocol with two-switch dispatch + 17 AppSettings keys doc

**PORT FROM:** Thetis `TCIServer.cs:4924-5128` (60-case set switch) + `:5134-5197` (21-case query switch) `[v2.10.3.13]`. AetherSDR `TciProtocol.{h,cpp} [@0cd4559]` for seam pattern only (we deliberately diverge to two-switch).

**READ FIRST:** Sweep A full report.

**Files:** `src/core/TciProtocol.{h,cpp}`, `tests/tci/test_tci_dispatch_seam.cpp`.

- [ ] **Failing test:** unknown command → empty response, no notifications (Sweep B silent invariant); `vfo;` routes to query path; `vfo:0,0,14250000;` routes to set path.
- [ ] Create `TciProtocol.h` with `handleCommand`, `hasPendingNotification`, `takePendingNotification`, `buildInitBurst`, `sliceToTrx`/`trxToSlice`. Apply Template B (multi-source). **Add the 17 AppSettings keys documentation block** at the top of the header per design doc Section 10.
- [ ] Create `TciProtocol.cpp` with parser (split on first `:`), dispatch to `handleSetCommand`/`handleQueryCommand` (both stub `return QString()`), notification queue, slice/trx mapping (identity until Phase 6+). Cite `TCIServer.cs:4900-4924 [v2.10.3.13]` for parser.
- [ ] Run test → PASS.
- [ ] Commit: `feat(tci): TciProtocol skeleton with two-switch dispatch + AppSettings keys doc`

### Task 3.2: Wire TciServer.onTextMessageReceived → TciProtocol + silent-error invariant

**Files:** modify `TciServer.cpp`; `tests/tci/test_tci_silent_error_invariant.cpp`.

- [ ] **Failing test:** real `QWebSocket` client to running server; sends `nonexistent_command;`; waits 200ms; assert ZERO inbound messages (Sweep B invariant).
- [ ] Implement `onTextMessageReceived`: lookup session, update `lastCommand`/`lastCommandAt`, call `m_protocol->handleCommand`, send response if non-empty, drain `pendingNotification` and broadcast each to all clients.
- [ ] Run test → PASS.
- [ ] Commit: `feat(tci): wire text-frame dispatch + silent-error invariant`

---

## Phase 4: Init burst

### Task 4.1: Port sendInitialisationData (8 wrapper lines + ready)

**PORT FROM:** Thetis `TCIServer.cs:2512-2552 [v2.10.3.13]`.

**READ FIRST:** Sweep D "Init burst"; design doc Section 7 row 1.

**Files:** modify `TciProtocol.{h,cpp}`; `tests/tci/test_tci_init_burst_smoke.cpp` (one minimal smoke; full byte-for-byte assertion lives in Task 4.3 golden file).

- [ ] **Failing smoke test:** `buildInitBurst()` returns non-empty list; last entry equals `"ready;"`; first entry starts with `"protocol:"`. Three asserts only — strict-protocol assertions live in golden file.
- [ ] Implement `buildInitBurst()` per design doc Section 2.2: reads `TciEmulateExpertSDR3Protocol`, `TciEmulateSunSDR2Pro`, `TciCwluBecomesCw` from AppSettings; emits 8 wrapper lines + calls `buildInitialRadioStateLines()` (stubbed empty for now) + `ready;`. Cite each line.
- [ ] Run smoke → PASS.
- [ ] Commit: `feat(tci): port sendInitialisationData (8-line wrapper)`

### Task 4.2: Port sendInitialRadioState (~150-line burst body) + typo divergence

**PORT FROM:** Thetis `TCIServer.cs:2363-2510 [v2.10.3.13]`. **Includes init burst typo divergence** (design doc Section 7 row 1).

**READ FIRST:** Sweep D "Init burst" full enumeration.

**Files:** modify `TciProtocol.{h,cpp}`; `tests/tci/test_tci_init_burst_typo_divergence.cpp`.

- [ ] **Failing test:** burst contains BOTH `if:1,0,...;` AND `if:1,1,...;`. (We send full cross-product; Thetis duplicates `if:1,1,...;` × 2.)
- [ ] Implement `buildInitialRadioStateLines()` per design doc Section 2.2. Each `build<Foo>Line(rx, chan)` helper carries its own Thetis cite. Implementer enumerates every `send<Foo>` between TCIServer.cs:2363 and :2510 per Sweep D, ports each verbatim, preserves every author tag within ±5 lines, version-stamps every cite.
- [ ] Run test → PASS. Self-review per standing rule 10.
- [ ] Commit: `feat(tci): port sendInitialRadioState + typo divergence (design doc §7.1)`

### Task 4.3: Capture init burst golden file + CI assertion (load-bearing)

**Files:** `tests/data/tci/init_burst_anan_g2_rx1.txt`, `tests/tci/test_tci_init_burst_golden.cpp`.

- [ ] If real Thetis + ANAN-G2 available: capture via `wscat -c ws://127.0.0.1:50001`, save verbatim with file-header comment block (capture date, Thetis version, radio model, sample rate, RX2 state, divergence note for `if:1,0;`).
- [ ] If unavailable: generate synthetic golden from `TciProtocol::buildInitBurst()` mock output. Header-mark `# SYNTHETIC GOLDEN — replace before merge`. Track in PR description.
- [ ] **Test** compares our `buildInitBurst()` output to golden line-by-line for strict-protocol prefixes (`protocol:`, `trx_count:`, `channels_count:`, `modulations_list:`, `receive_only:`, `ready;`). Radio-identifying fields (`device:`, `vfo:`) compared loosely.
- [ ] Run test → PASS.
- [ ] Commit: `test(tci): init burst golden file + CI assertion`

---

## Phase 5: 12 compat flags

### Task 5.1: Wire all 12 compat flags + matrix rows

**PORT FROM:** Thetis `TCIServer.cs:6109-6286 [v2.10.3.13]`.

**Files:** modify `TciProtocol.cpp`; rows added to `tests/data/tci/matrix.csv`.

- [ ] For each of 12 flags (table per design doc Section 2.2), add a matrix row using the `SETUP:Key=Value;;command;` pattern. Example:
  ```csv
  emulate_esdr3_on,SETUP:TciEmulateExpertSDR3Protocol=True;;protocol;,protocol:ExpertSDR3\,2.0;,,TCIServer.cs:6110 [v2.10.3.13],compat flag flips wire string
  ```
- [ ] Wire each flag at its effect site. Audio/IQ/sensor flag effects deferred to Phases 11/12/14 (those phases add the matrix rows for those flag effects).
- [ ] Run matrix runner → new rows pass.
- [ ] Commit: `feat(tci): wire 12 compat flags + matrix rows`

---

## Phase 6: Command handlers — VFO family (matrix-driven)

**PORT FROM:** Thetis `TCIServer.cs` per Sweep A row by row. Commands: `vfo`, `vfo_lock`, `lock` (distinct), `vfo_limits`, `if_limits`.

**Files:** modify `TciProtocol.cpp`; rows added to `tests/data/tci/matrix.csv`.

### Task 6.1: VFO family handlers + matrix rows

- [ ] Add matrix rows for each command (set + query):
  ```csv
  vfo_set_rx0,vfo:0\,0\,14250000;,,vfo:0\,0\,14250000;,TCIServer.cs:5028 [v2.10.3.13],set+broadcast
  vfo_query_rx0,vfo;,vfo:0\,0\,14250000;,,TCIServer.cs:5168 [v2.10.3.13],query unicast
  vfo_lock_set,vfo_lock:0\,0\,true;,,vfo_lock:0\,0\,true;,TCIServer.cs:5097 [v2.10.3.13],distinct from lock
  lock_set,lock:0\,true;,,lock:0\,true;,TCIServer.cs:5095 [v2.10.3.13],drops chan arg
  vfo_limits_query,vfo_limits;,vfo_limits:0\,64000000;,,TCIServer.cs:5170 [v2.10.3.13],
  if_limits_query,if_limits;,if_limits:-96000\,96000;,,TCIServer.cs:5172 [v2.10.3.13],
  ```
- [ ] Implement handlers in `handleSetCommand`/`handleQueryCommand` switches. Each handler: parse args, validate, call `RadioModel` setter via `QMetaObject::invokeMethod(m_radioModel, ..., Qt::QueuedConnection)`, format response, enqueue notification on state change. Cite per handler. Extend `TestMockRadioModel` with any new accessors needed; note in commit message.
- [ ] Run matrix runner → new rows pass.
- [ ] Commit: `feat(tci): VFO family handlers + matrix rows`

---

## Phases 7-13: Remaining command families (matrix-driven)

Same pattern as Task 6.1: add matrix rows + implement handlers + extend mock as needed + run runner + commit. One commit per phase.

| Phase | Family | Sweep A rows | Key Thetis cites |
|---|---|---|---|
| **7** | Mode/Filter (modulation, modulations_list, rx_filter_band) + CWLU/CWU/CW transform helper | ~6 | TCIServer.cs:5036, :5174, :5045, :5180, :2148, :3873 [v2.10.3.13] |
| **8** | TRX (trx, split, dup, mute, audio_mute) | ~10 | TCIServer.cs:5040, :5060, :5062, :5070, :5072 [v2.10.3.13] |
| **9** | DSP (agc, nb, nr, anf, bin, apf, nf, sq, rit, xit, balance) — largest family | ~25 | TCIServer.cs:5048-5095 range [v2.10.3.13] |
| **10** | Audio stream (audio_stream, audio_sample_rate, audio_gain, audio_stream_samples, line_out_*). **Adds dedicated dB volume math test** (TCIServer.cs:4110-4132 — pure logic, TDD). | ~12 | TCIServer.cs:4110, :5074-5085, :5240 [v2.10.3.13] |
| **11** | IQ stream (iq_stream, iq_sample_rate, iq_start, iq_stop). Wires `TciIqSwap` + `TciAlwaysStreamIq` matrix-row effects. | ~6 | TCIServer.cs:5397, :5430, :5806 [v2.10.3.13] |
| **12** | **Stubs (Spot + CW combined)**: spot_add, spot_remove, spot_clear, spot_simulate_click (Phase 3J-2 hooks); cw_macros_speed_up/down, cw_msg (3M-2 hooks). All return `ok` + log. | 7 | TCIServer.cs:7073, :5066-5068, :5052-5054 [v2.10.3.13] |
| **13** | Bespoke `_ex` (rx_ctun_ex, tx_profile_ex, tx_profiles_ex, calibration_ex, shutdown_ex, rx_nr_enable_ex, rx_enable) | ~7 | TCIServer.cs:5126, :5095, :5134, :5176, :5158 [v2.10.3.13] |

**Phase 10 sidebar — dB volume math test (TDD, pure logic):**

- [ ] **Failing test** (`tests/tci/test_tci_volume_db.cpp`) asserts `linearToDbVolume(1.0) == 0.0`, `linearToDbVolume(0.0) == -60.0` (or whatever Thetis floor is), `dbVolumeToLinear(-6.0) ≈ 0.5012` (within 1e-3). Cite `TCIServer.cs:4110-4132 [v2.10.3.13]`.
- [ ] Implement `linearToDbVolume` + `dbVolumeToLinear` in `TciProtocol.cpp` (or small `TciVolume.h` utility). Cite per function.
- [ ] Run, PASS.

---

## Phase 14: Three priority send queues (TDD — pure logic)

### Task 14.1: TciSendQueue + drain order + bounded depth

**PORT FROM:** Thetis `TCIServer.cs:769-774` (queue declarations) + `:1645-1679` (drain order) + `:2976-3072` (send-loop body) `[v2.10.3.13]`.

**READ FIRST:** Sweep C "Threading model" → "Priority Send Queues".

**Files:** `src/core/TciSendQueue.{h,cpp}`, `tests/tci/test_tci_priority_queues.cpp`. Modify `TciServer.cpp` to use queue per-client (drop counter exposed on `ClientSession::framesDropped`; surfaced in ClientChainApplet during Phase 22).

- [ ] **Failing tests:**
  - Drain order: push Control then Binary then Urgent then Control → drain produces Urgent / Binary / Control / Control.
  - Bounded overflow: push 5 messages into capacity-3 queue → `dropCount() == 2`; drain produces last 3.
  - **Multi-thread access**: producer thread pushes 10000 messages while consumer thread drains; assert no torn reads, no UB, drop count + drained count = 10000.
- [ ] Implement `TciSendQueue.h` with three internal `QQueue<QString>` (Urgent / Binary / Control), `push(Priority, QString)`, `tryPop()`, `dropCount() const`, bounded depth + oldest-drop. Apply Template C (NereusSDR-original — we condense the C# 3-queue model).
- [ ] Wire into `ClientSession`: each session owns one queue. Modify TciServer broadcast/unicast paths to push; add per-client send-loop drain (initially single-shot timer).
- [ ] Run tests → PASS.
- [ ] Commit: `feat(tci): three priority send queues per client + thread-safe drain`

---

## Phase 15: VFO coalescing thread (TDD — pure logic)

### Task 15.1: 3-layer VFO coalescer

**PORT FROM:** Thetis `TCIServer.cs:751, 747, 1314-1381 [v2.10.3.13]`.

**READ FIRST:** Sweep C "Threading model" → "VFO Coalescing".

**Files:** `src/core/TciVfoCoalescer.{h,cpp}`, `tests/tci/test_tci_vfo_coalescer.cpp`. Modify `TciProtocol.cpp` to route VFO notifications through coalescer.

- [ ] **Failing test:** simulate 200 VFO updates within 100ms → assert ≤ 1 outbound `vfo:` per coalesce interval.
- [ ] Implement 3 layers per Sweep C: per-event one-shot timer, bounded LinkedList(10) with oldest-drop, outbound-coalesced map. Cite each layer.
- [ ] Route VFO notifications through coalescer instead of direct enqueue.
- [ ] Run test → PASS.
- [ ] Commit: `feat(tci): 3-layer VFO coalescing thread`

---

## Phase 16: Audio binary RX pipeline

### Task 16.1: SPSC ring buffer per slice (TDD — pure logic)

**Files:** `src/core/TciAudioRing.h`, `tests/tci/test_tci_audio_ring.cpp`.

- [ ] First: `grep -rn 'class.*Ring' src/core` to check if existing NereusSDR ring infrastructure can be reused. If yes, skip implementation and reuse; mention in commit.
- [ ] **Failing test:** producer + consumer threads stress 1M samples; no drops, no UB, no torn reads.
- [ ] Implement lock-free SPSC ring (or reuse).
- [ ] Run test → PASS.
- [ ] Commit: `feat(tci): lock-free SPSC audio ring buffer` (or `feat(tci): reuse <existing> for TCI audio ring`)

### Task 16.2: RxChannel post-DSP tap

**Files:** modify `src/core/RxChannel.{h,cpp}`. No test (covered by Task 16.4 integration).

**STOP-AND-ASK:** if RxChannel doesn't have a clean tap point, document the gap before refactoring.

- [ ] Add `audioFrameReady(int slice, const float* L, const float* R, int n, int srcRate)` signal post-fexchange2.
- [ ] Commit: `feat(tci): RxChannel post-DSP audio tap signal`

### Task 16.3: TciBinaryFrame encode + 5ms drain timer + WDSP resampler lifecycle

**PORT FROM:** Thetis `TCIServer.cs:5240-5337 [v2.10.3.13]`.

**Files:** `src/core/TciBinaryFrame.{h,cpp}` (Template A — pure port); modify `TciServer.{h,cpp}`; `tests/tci/test_tci_resampler_lifecycle.cpp`.

This task chunks into 3 logical sub-commits (single task, three commits — implementer decides at commit time):

- [ ] **Failing test for resampler lifecycle:** subscribe → 1 instance; second client subscribes same slice → 2 instances; first client unsubscribes → 1 instance; server stop → 0 instances.
- [ ] Sub-commit a: Implement `TciBinaryFrame::encodePayload(int rx, int sampleRate, int sampleType, int length, int streamType, int channels, const float* samples) -> QByteArray` per `TCIServer.cs:5240 [v2.10.3.13]` (64-byte LE header, 16×uint32). Cite + Template A header.
- [ ] Sub-commit b: WDSP `create_resampleFV` per-(client, slice) lifecycle. Stored in `QHash<QPair<QString,int>, void*>`. Subscribe creates, unsubscribe destroys, disconnect cleans all stale.
- [ ] Sub-commit c: 5ms drain timer on TCI thread. Drain ring → assemble per `m_audioStreamSamples` → resample if rate ≠ 48000 → encode → `sendBinaryMessage`.
- [ ] Run resampler lifecycle test → PASS.

### Task 16.4: PCM round-trip integration test

**Files:** `tests/tci/test_tci_audio_roundtrip.cpp`.

- [ ] **Test** spins up real `TciServer`, opens real `QWebSocket` client, subscribes to audio for slice 0, feeds 1s of synthetic 1kHz tone into `RxChannel::audioFrameReady`, asserts client receives ≥ 1 binary frame with `streamType==1` and decoded payload matches input within 1e-3 (resampling tolerance).
- [ ] Run test → PASS.
- [ ] Commit: `test(tci): RX audio round-trip integration`

---

## Phase 17: Audio binary TX pipeline (integration + TDD on mutex)

### Task 17.1: handleBinaryFrame inbound + modern/legacy detection + TX mutex

**PORT FROM:** Thetis `TCIServer.cs:5602-5703 [v2.10.3.13]`.

**Files:** modify `TciServer.cpp` (`onBinaryMessageReceived`); `tests/tci/test_tci_tx_mutex.cpp`; extend `test_tci_audio_roundtrip.cpp` for TX direction.

- [ ] **Failing test for TX mutex:** simulate 2 clients sending TX audio; assert client 1 frames feed `TxChannel`, client 2 silently dropped, client 2's `framesDropped` increments. Both still receive `trx:0,true;` broadcast (Sweep B finding).
- [ ] Implement `handleBinaryFrame`: parse 64-byte header, modern-vs-legacy detection per `TCIServer.cs:5628-5652 [v2.10.3.13]`, decode samples, NaN/Inf zero + clamp `[-4.0, 4.0]`, push to TX-direction SPSC ring, enforce single-client mutex via `m_txAudioActiveClient`.
- [ ] Wire TX ring drain in audio thread → `TxChannel::feedTxAudio`.
- [ ] Run tests → PASS.
- [ ] Commit: `feat(tci): TX audio binary pipeline + single-client mutex`

---

## Phase 18: IQ binary stream pipeline

### Task 18.1: rawIqData tap + wantsIQStream early-out + IQ encode + integration

**PORT FROM:** Thetis `TCIServer.cs:5397-5430 [v2.10.3.13]`.

**Files:** modify `TciServer.cpp`; `tests/tci/test_tci_iq_roundtrip.cpp` (new file — separate from audio round-trip for clean test scope).

- [ ] Tap `RadioModel::rawIqData` (existing signal for FFTEngine) on TCI thread.
- [ ] Implement `wantsIQStream(receiver)` per `TCIServer.cs:5397-5404 [v2.10.3.13]` including `AlwaysStreamIQ` flag check.
- [ ] Encode IQ binary frame: 64-byte header (always FLOAT32, always 2 channels, interleaved), apply `IQSwap` flag, broadcast to subscribed clients.
- [ ] **Integration test** (`test_tci_iq_roundtrip.cpp`): subscribe via `iq_start;`, feed synthetic I/Q via `RadioModel::rawIqData`, assert client receives binary frames with `streamType==0`. Unsubscribe via `iq_stop;`, feed more I/Q, assert no frames received (early-out works).
- [ ] Run → PASS.
- [ ] Commit: `feat(tci): IQ binary stream pipeline + IQSwap + AlwaysStreamIQ`

---

## Phase 19: Sensor manager (TDD on formats + integration on timer)

### Task 19.1: TciSensorManager + 4 wire formats + interval aggregation

**PORT FROM:** Thetis `TCIServer.cs:789, 2316, 2321, 2326, 2566, 2581 [v2.10.3.13]`.

**READ FIRST:** Sweep E "Sensor manager".

**Files:** `src/core/TciSensorManager.{h,cpp}` (Template A); `tests/tci/test_tci_sensor_formats.cpp`. Modify `TciServer.cpp` for timer wiring.

- [ ] **Failing test for formats:**
  - `formatRxSensors(0, -95.3)` → `"rx_sensors:0,-95.3;"` (cite `:2316`)
  - `formatRxChannelSensors(0, 0, -95.3)` → `"rx_channel_sensors:0,0,-95.3;"` (cite `:2321`)
  - `formatRxChannelSensorsEx(0, 0, -95.3, -97.1, -93.2)` → `"rx_channel_sensors_ex:0,0,-95.3,-97.1,-93.2;"` (cite `:2321`)
  - `formatTxSensors(0, -30.0, 12.5, 12.5, 1.10)` → `"tx_sensors:0,-30.0,12.5,12.5,1.10;"` (cite `:2326`)
- [ ] **Failing test for interval aggregation:** 3 clients request 100/200/300ms → effective 100ms.
- [ ] Implement format helpers + `MinimumRequiredRxSensorInterval` / `MinimumRequiredTxSensorInterval`.
- [ ] Wire two timers (RX 200ms default, TX 200ms MOX-gated). On tick, format + broadcast to subscribed clients.
- [ ] Run tests → PASS.
- [ ] Commit: `feat(tci): sensor manager + 4 wire formats + interval aggregation`

---

## Phase 20: GUI — Setup → Network → TCI Server page

### Task 20.1: CatTciServerPage rewrite (6 group boxes)

**PORT FROM:** Thetis `setup.designer.cs:57979-58012 [v2.10.3.13]` (grpTCIServer control list); setup.cs handlers per group.

**READ FIRST:** mockup `tci-setup-page.html`; design doc Section 2.5; existing `CatNetworkSetupPages.{h,cpp}`.

**Files:** modify `src/gui/setup/CatNetworkSetupPages.{h,cpp}`.

- [ ] Wire 6 group boxes per design (Server / Compatibility / IQ Stream / Audio Stream / Sensors / VFO Quirks). All 17 AppSettings keys bound. Plain-English Qt strings (no source cites in `setText`/`setToolTip`).
- [ ] Run `STANDARD_GUI_VERIFY` (standing rule 11) with screenshot `/tmp/nereus-tci-setup-page.png`. Confirm against mockup.
- [ ] Commit: `feat(tci): rewrite Setup → Network → TCI Server page`

---

## Phase 21: GUI — TciApplet

### Task 21.1: TciApplet skeleton + Slice A + TX + footer

**PORT FROM:** NereusSDR-original (Template C).

**READ FIRST:** mockup `tci-applet-v2.html`; existing `RxApplet.{h,cpp}` for layout pattern.

**Files:** `src/gui/applets/TciApplet.{h,cpp}`. Modify `src/gui/CMakeLists.txt`.

- [ ] Implement applet (header row + Slice A row + TX row + footer per mockup). Plain-English Qt strings.
- [ ] Run `STANDARD_GUI_VERIFY` with screenshot `/tmp/nereus-tci-applet.png`. Confirm against mockup.
- [ ] Commit: `feat(tci): TciApplet (Slice A + TX meters + gain sliders)`

---

## Phase 22: GUI — ClientChainApplet

### Task 22.1: ClientChainApplet + per-client rows + drop counter wiring

**Files:** `src/gui/applets/ClientChainApplet.{h,cpp}`. Modify `src/gui/CMakeLists.txt`.

- [ ] Implement applet per mockup `client-chain-applet.html`: per-client rows with TX badge, peer + name (User-Agent), subscription badges, last command, **drop counter (read from `TciSendQueue::dropCount()` via session)**, disconnect button. Empty state.
- [ ] Run `STANDARD_GUI_VERIFY` with screenshots `/tmp/nereus-client-chain-empty.png` and `/tmp/nereus-client-chain-3clients.png` (test client populates).
- [ ] Commit: `feat(tci): ClientChainApplet with per-client rows + drop counter`

---

## Phase 23: GUI — Live-wire m_tciIndicator + menu integration

### Task 23.1: Bottom-bar indicator + Tools menu + View submenu

**Files:** modify `src/gui/MainWindow.cpp` and `MainWindow.h`.

**READ FIRST:** mockup `banner-bottom-existing.html`; `MainWindow.cpp:3179` and `:2848`; design doc Section 2.9.

- [ ] Modify `makeIndicator()` call at MainWindow.cpp:3179 to capture inner labels via `outTop`/`outBot` out-params; store bottom label as `m_tciIndicatorBotLabel`.
- [ ] Implement `MainWindow::updateTciIndicator()` per design doc Section 8.4: 4 states (Off/On/On·N/On·N ▸TX) with colors (#404858/#6f6/#6cf/#ec6) + tooltip.
- [ ] Connect TciServer signals (`serverStarted/Stopped`, `clientConnected/Disconnected`, `txAudioActiveClientChanged` — add this signal to TciServer if absent) to `updateTciIndicator`.
- [ ] Install event filter on `m_tciIndicator` for click → open Setup → Network → TCI Server. Use existing `setProperty("isTciIndicator", true)` + `eventFilter` pattern.
- [ ] Enable existing `tciAction` at MainWindow.cpp:2848 (remove `setEnabled(false)` + NYI tooltip); wire `triggered` to the same Setup-jump.
- [ ] Add View → Network Applets ▶ submenu with TCI Server + TCI Clients toggles (default-checked); CAT/MIDI items greyed.
- [ ] Run `STANDARD_GUI_VERIFY` with 4 screenshots (Off / On / clients / TX active). Use a test client to drive states.
- [ ] Commit: `feat(tci): live-wire bottom-bar m_tciIndicator + menu integration`

---

## Phase 24: CatApplet edit + AudioTciPage flesh-out

### Task 24.1: Strip TCI button row from CatApplet

**Files:** modify `src/gui/applets/CatApplet.{h,cpp}` (~30 LOC removed).

- [ ] Remove TCI references (those moved to TciApplet).
- [ ] Run `STANDARD_GUI_VERIFY` with screenshot `/tmp/nereus-cat-applet-after.png`.
- [ ] Commit: `refactor(cat): strip TCI button row from CatApplet`

### Task 24.2: Flesh out AudioTciPage

**Files:** modify `src/gui/setup/AudioTciPage.{h,cpp}` (~60+150 LOC).

- [ ] Implement audio-side wiring per design doc Section 2.7 (sample rate per slice, format selection, master mute behavior).
- [ ] Run `STANDARD_GUI_VERIFY` with screenshot.
- [ ] Commit: `feat(tci): flesh out AudioTciPage`

---

## Phase 25: Documentation (single commit)

### Task 25.1: Update CHANGELOG, PROVENANCE, CLAUDE.md, regenerate matrix README

**Files:** `CHANGELOG.md`, `docs/attribution/PROVENANCE.md`, `CLAUDE.md`, `docs/architecture/2026-05-09-phase3j-1-tci-port-verification/README.md`.

- [ ] Add `## [Unreleased] — Phase 3J-1 TCI Server (Thetis port)` section to CHANGELOG with Added / Stubbed / Known divergences subsections (per design doc summary).
- [ ] Add PROVENANCE rows for each new ported file (TciServer, TciProtocol, TciBinaryFrame, TciSensorManager, TciClientSession) per HOW-TO-PORT.md format.
- [ ] Update CLAUDE.md: Architecture Quick Reference (new applets/components), Phase status table (3J row).
- [ ] Run `python3 scripts/gen-tci-matrix-readme.py > docs/architecture/2026-05-09-phase3j-1-tci-port-verification/README.md` to regenerate the human-facing matrix from CSV.
- [ ] Commit: `docs(tci): CHANGELOG + PROVENANCE + CLAUDE.md + regenerated matrix README`

---

## Phase 26: End-of-epic gate

### Task 26.1: Run full ctest suite

- [ ] `ctest --test-dir /Users/j.j.boyd/NereusSDR/.worktrees/phase3j-1-tci-server-port/build --output-on-failure`. ALL green. Investigate any regression.

### Task 26.2: Single comprehensive code review

- [ ] Dispatch `superpowers:code-reviewer` against full branch diff (`git diff main..HEAD`). Single review. Brief reviewer with: design doc path, divergence ledger, sweep reports as background. Focus: parity violations, missed Thetis cites, dropped author tags, GPL header gaps, security in WS handling.

### Task 26.3: Verification matrix sign-off

- [ ] Complete every row in `docs/architecture/2026-05-09-phase3j-1-tci-port-verification/README.md`. Sign-off cells filled.

### Task 26.4: Manual integration smoke

- [ ] N1MM Logger+ (Windows): connect, retune via spot click, mode change, MOX assert.
- [ ] Log4OM (Windows): connect, retune, sensor display.
- [ ] RUMlog-TCI (macOS): connect, retune, basic sensor.
- [ ] Document each in matrix README.

### Task 26.5: Open PR

- [ ] `gh pr create --title "feat(tci): port Thetis TCI Server (Phase 3J-1)" --body "..."` with summary, test-plan checklist, design + plan links, divergence summary.

---

## Self-review (writer of this plan)

- [x] **Spec coverage**: every design section maps to one or more phases.
- [x] **Test discipline alignment**: matrix-driven for plumbing handlers, TDD only for pure logic, integration for I/O, visual for UI. Aligns with `feedback_skip_per_task_review_ceremony.md` and `feedback_minimize_test_invocations.md`.
- [x] **No placeholders**: every task has actual content.
- [x] **Type consistency**: `TciServer`, `TciProtocol`, `TciClientSession`, `TciSendQueue`, `TciVfoCoalescer`, `TciBinaryFrame`, `TciSensorManager` referenced consistently.
- [x] **Cite stamps**: every Thetis cite `[v2.10.3.13]`. AetherSDR cites `[@0cd4559]`.
- [x] **Slice nomenclature**: UI Slice A/B/C/D; protocol `trx:N` via `sliceToTrx`/`trxToSlice`.
- [x] **No CI-unblock language**.
- [x] **Single end-of-epic gate** (Phase 26); no per-task reviewer ceremony.
- [x] **Adversarial review fixes applied**: dropped lcTci TDD theater (Task 0.2), single source of truth for matrix (CSV → README via script), per-line burst test → smoke only, dropped redundant pre-commit hook re-verify, added NEREUS_THETIS_DIR setup, deleted noise task, clarified TestMockRadioModel scope, merged Spot+CW stubs into one phase, single docs commit, IQ test placement clarified, self-review checklist scoped to new-file ports only, header templates in setup notes (not standalone task), STANDARD_GUI_VERIFY macro in standing rules, cross-platform GUI commands in macro.

**Plan size:** ~850 lines (down from 1,000 in matrix-driven v1, 1,800 in TDD-everywhere v0). Test files: 10. Tasks: ~25. CI runner is the load-bearing artifact.

---

*End of implementation plan.*
