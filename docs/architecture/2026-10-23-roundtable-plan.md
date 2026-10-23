# Roundtable Implementation Plan

> **Execution:** run with `crew` (`/crew <this file>`) under `cost-aware-execution`.
> Requirements and acceptance cases are binding; test order and review effort follow
> the risk-based policy; UI evidence follows `ui-verification`. No review between
> tasks; one whole-branch review at the end.

**Goal:** Every station on a frequency shows its own raw S-meter reading of whoever is talking, with messages, named turns and a private archive, served by a new Roundtable server on the website droplet and shown on the desktop, iPhone, iPad and web.

**Architecture:** A new Python program, `nereus-roundtable` in `roundtable/`, built like `rendezvous/` (asyncio, apt packages only, SQLite, a conformance suite), speaks a numbered public protocol v1. Each Core runs one `RoundtableClient` beside `FreeDVReporterClient`, under `SpotSourceHost`, and hands the window and phone a new station link surface (`stationRoundtableVersion`). The desktop, iPhone, iPad and the server's own web pages draw the screens settled on the mockups.

**Tech Stack:** Python 3 (asyncio, `websockets`, `cryptography`, `sqlite3`, pytest); C++20 and Qt6 (QtWebSockets, QtTest, the station link and mirror, `AppletWidget`, `SpectrumWidget`, `VfoWidget`); Swift (SwiftUI, NereusKit, Swift Testing); static HTML, CSS and script served by the server; Caddy and systemd on Ubuntu 24.04.

**Spec:** [Roundtable](2026-10-23-roundtable-design.md). Every decision is settled there (decisions D1 to D25 and U1 to U14 in section 3, Claude's calls C1 to C28 in section 18); this is how it gets built. Where this plan and the spec disagree, the spec wins and the plan is the bug.

## Global Constraints

- Read `CLAUDE.md`, `CONTRIBUTING.md` and `docs/development/fast-test-loop.md` first. They bind every task.
- No upstream code is ported (spec section 17). New C++ and Swift files carry `no-port-check: NereusSDR-original.` in their header; no attribution header, no provenance row. Grid and distance math in Python is written from the Maidenhead definition and the haversine formula, never from `src/models/FreeDVStationModel.cpp` (a freedv-gui port). The identity helpers copied from `rendezvous/server/nereus_rendezvous/identity.py` are our own code.
- Every touched C++ or Swift source or test file gets a "Modification history (NereusSDR)" line in the file's existing style, for example `2026-10-23: Roundtable ... J.J. Boyd (KG4VCF), AI-assisted via Claude Code.` New files start the block. Python files start with a module docstring that says what the module does, as `rendezvous/` does.
- C++ style: no `goto`, no raw `new`/`delete` (Qt parent or `unique_ptr`), `constexpr` constants named `kPascalCase`, braces on all control flow, `m_camelCase` members, `auto` only when the type is obvious, `qCWarning(lcCategory)` for errors, no exceptions. Platform guards `Q_OS_WIN` / `Q_OS_MAC` / `Q_OS_LINUX` only.
- Rule R1: nothing under `src/core/` or `src/models/` includes a GUI header.
- Settings go through `AppSettings`, never `QSettings`. Keys are PascalCase under `Roundtable/`; booleans are the strings `"True"` / `"False"`.
- Cross-thread traffic is auto-queued signals only. `RoundtableClient` lives on the main thread with the models; it never touches the audio thread.
- Don't remove code you didn't add.
- Old peers keep their exact bytes: a peer that does not see `stationRoundtableVersion` gets no new object, stream or capability entry, and the link conformance fixtures under `tests/data/link/v1/` change only where a step names the change (capability ordinals in existing session fixtures move as the rotor PR moved them).
- The rendezvous and relay (`rendezvous/`) do not change.
- The Roundtable server keeps no address in any file or log (spec 9.5, 12.3). The log holds no message text and no station id beyond its first 6 characters.
- Python: the server runs on Ubuntu 24.04's Python 3.12 with apt `python3-websockets` and `python3-cryptography` and nothing from pip; tests also run on this Mac's `/usr/bin/python3` (3.9.6, websockets 15.0.1, cryptography 46.0.4). So: no syntax newer than 3.9 (no `match`, no `X | Y` in annotations evaluated at runtime), and every websockets API difference stays in `transport.py`, as `rendezvous/server/nereus_rendezvous/transport.py:24-38` does it.
- Python tests: `/usr/bin/python3 -m pytest roundtable/tests -q -p no:cacheprovider` (all) or `... roundtable/tests/test_<name>.py -q -p no:cacheprovider` (one file).
- American spelling and no em-dashes in all new text (comments, test names, strings, docs). No source cites in user-visible strings. Operator wording passes `OperatorWording::isPlain` where the link carries it.
- Commits are GPG-signed (`git commit -S`); never `--no-gpg-sign`. No `Co-Authored-By` trailer. If signing fails, stop and report BLOCKED.
- **Two build lanes on this Mac across every session and crew** (JJ, 2026-10-23). Configure, compile, `ctest`, `swift test`, `xcodebuild` and `capture.py` runs take a lane lock first, as `~/.config/nereus/work/pr-review-20261021/build-lane-lock.md` says: write and self-review the code first, then `mkdir ~/.config/nereus/work/build-lane.lock` (or `build-lane-2.lock`), write an owner line, run one build-and-test cycle under `nice -n 20` with `-j 6` (`xcodebuild -jobs 4`, swift-test `--jobs 2`), and remove only your own lock the moment the last command ends. Python pytest runs (seconds, no compile) need no lane, except the load test in Task 9, which takes one.
- Build dir: reuse one per lane. A new build dir is configured and compiled with the network denied, never downloading:

  ```bash
  DEPS=~/.config/nereus/work/pr-merge-review/review-deps
  ARGS=()
  for d in "$DEPS"/*-src; do
    n=$(basename "$d" -src | tr '[:lower:]' '[:upper:]')
    ARGS+=("-DFETCHCONTENT_SOURCE_DIR_${n}=${d}")
  done
  sandbox-exec -p '(version 1)(allow default)(deny network*)' \
    cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DNEREUS_BUILD_TESTS=ON \
      -DENABLE_DFNR=OFF -DFETCHCONTENT_FULLY_DISCONNECTED=ON \
      -DOPUS_URL=/Users/j.j.boyd/.config/nereus/work/deps-cache/nereus-core-pins-0368ff16/opus/opus-940d4e5-with-model.zip \
      -DOPUS_URL_HASH=SHA256=740a5220180aee546f97030118517eedafa7956fdfbc8e8432d91f2d20aac4b7 \
      "${ARGS[@]}"
  sandbox-exec -p '(version 1)(allow default)(deny network*)' nice -n 20 cmake --build build -j 6 --target <targets>
  ```

  Then grep the configure output for "Performing download step"; it must not appear.
- Build single test targets, never the whole suite by default. Run tests with `ctest --test-dir build -R '^<name>$' --output-on-failure` (ctest sets offscreen and a per-test home). A direct binary run sets `HOME`, `CFFIXED_USER_HOME` and `NEREUS_TEST_HOME` to one scratch folder, `QT_QPA_PLATFORM=offscreen`, and passes `-maxwarnings 0`.
- Swift tests: `ios/scripts/swift-test.sh --jobs 2 --filter <SuiteOrTest>`. App tests: `ios/scripts/generate-project.sh`, then `xcodebuild -project ios/NereusSDR.xcodeproj -scheme NereusSDR -destination 'platform=iOS Simulator,name=iPhone 17' -collect-test-diagnostics never -parallel-testing-enabled NO -jobs 4 test` (with `-only-testing:` for the task's tests).
- Desktop captures: `cmake --build build --target nereus_visual_preview`, then `python3 tests/visual/capture.py --out <dir> --scenes '<pattern>' --profiles '<pattern>' --jobs 4`. Each desktop task also renders at the Windows pixel density (`QT_SCALE_FACTOR=0.5` on the preview binary) and checks both against the mockups.
- A failed test is never ignored or retried away. A failure under load is a finding: record the load average and the assertion.
- Server steps (DNS, droplet changes, deploys, Caddy reloads) each need JJ's yes in chat; no task does them on its own (spec 6.2). JJ's own NereusSDR settings are off limits except read-only logs. The STRIX is read-only to agents unless JJ says yes.
- No public posts. Pushes go only to the branch below, one batched push per round, never to stay current with main.
- **Branch:** `claude/roundtable`, cut from `origin/main` by the controller, which first cherry-picks the spec and mockup commits (`2ef5e1739`, `25769a202`) and this plan's commit from `claude/nereus-freedv-reporter-chat-e7a0b9`. Lanes S (Tasks 1 to 9), C (Tasks 10 to 15) and I (Tasks 16 to 19) touch different files; the controller may run S beside C or I in separate worktrees (`claude/roundtable-server`, `claude/roundtable-apps`) merged back into `claude/roundtable` at lane ends.

## What already exists

Facts from origin/main `83a5a6bcc`; spec section 5 has the rest.

**Rendezvous, the server template** (`rendezvous/`):
- `server/nereus_rendezvous/`: `__main__` (`python3 -m nereus_rendezvous --config FILE`, stderr logging, websockets loggers silenced, SIGTERM stop; `__main__.py:2,17-68`), `config` (`Config` dataclass with defaults :28-87, `_SECTIONS` allowlist :90, `load()` refuses unknown keys :168-182, `check()` :198, `read_secret` :155 from `LoadCredential`), `clock` (`RealClock` :30-48 with `now_ms`, `wall_seconds`, `call_later`; `ManualClock(wall_start=1800000000)` :51-79 with `advance(ms)`), `transport` (websockets 10.4 vs 13+ isolation :24-38, `X-Forwarded-For` trusted from loopback only :48-67, 426 for non-upgrades :89-110), `identity` (P-256, raw signatures, ids), `limits` (rate windows; IPv6 grouped by /56 there, /64 for Roundtable per spec 9.5), `protocol` (kinds, decoding, caps; `VERSION = 1` :20), `service` (state in memory, one outbound queue and writer per connection).
- `conformance/v1/manifest.json` (`{"rendezvousVersions":[1],"fixtures":[{"id","file","kind"}]}`), kinds `crypto`, `control` (`{"from","wire","decodes"}`), `session` (`{"runs","serverSetup","steps"}` with `connect`, `from`/`to`/`message`, `advanceMs` steps; `$b64:32:name` and `$key:a:id` placeholders, grammar in rendezvous spec section 10).
- `tests/`: `conftest.py` (puts `server/` and `tests/` on `sys.path`), `runner.py` (a real service on loopback with `ManualClock`, `advanceMs` at :457-459, client header differences at :29-36), `test_sessions.py` parametrized over the manifest, `memory_probe.py`, Docker checks.
- `deploy/`: `nereus-rendezvous.service` (`Type=exec`, `PYTHONPATH=/opt/...`, `DynamicUser=yes`, loopback IP rules :52-54, memory and OOM :71-78, sandboxing :81-101), `nereus-data-use.{service,timer}` (hourly oneshot), `Caddyfile` (`protocols h1 h2`, header snippet, per-path `handle`), `caddy-override.conf`, `setup-server.sh` (idempotent, `--dry-run`, `--no-start`, 10 steps :66-89, `caddy validate` :1024-1037). `deploy.sh` rsyncs to `/opt/...` with `--dry-run`, required-file checks, a world-readable staging copy, and never restarts anything.
- CI job `rendezvous` (`.github/workflows/ci.yml:349-364`): `ubuntu-24.04`, apt `python3-websockets python3-cryptography python3-pytest`, prints versions, `/usr/bin/python3 -m pytest rendezvous/tests -q -p no:cacheprovider`.
- Wire spec shape (`docs/architecture/2026-09-23-rendezvous-v1.md`): preamble (authority, stable numbers), 1 What it does, 2 Transport, 3 Roles and the life of a connection, 4 Identity values, 5 Messages (5.1 Envelope, 5.2 Field kinds, 5.3 Kinds), 6 Flows, 7 Errors, 9 Limits, storage and logs, 10 Conformance, 11 Changing it.

**Website:** `website/deploy/Caddyfile` (global `protocols h1 h2` :13-22; `(site_headers)` CSP :28-38; `nereussdr.com` block :47-81; no file imports). `website/deploy/setup-server.sh <Caddyfile> '<SSH key>'` installs it whole (7 steps :23-36). `website/deploy.sh` rsyncs `website/public/`. `docs/manual/build_preview.py --website` renders Markdown into `website/public/`, with a route for `rendezvous/README.md` at :39-40 as the precedent for a repository Markdown file; `website/README.md:31-33` lists the regenerate steps (`build_preview.py --website`, `check_manual.py`, `test_build_preview.py`).

**Core:**
- `FreeDVReporterClient` is owned by `RadioModel` (`m_freeDvReporter`, built at `RadioModel.cpp:4431`, identity from `FreeDvReporter/*` at :4432, URL from `FreeDvReporter/ServerUrl` :4440-4447). `SpotSourceHost` is built at :4523; `setFreedvReporter` at :4571 and `setFreedvStationList` at :4579 (Local role only).
- `SpotSourceHost` (`src/core/SpotSourceHost.{h,cpp}`): source ids :198-204; `stationSources()` :243-246 returns `{dxCluster, rbn, pota, freedvReporter, pskReporter}`; `recordStreamSources()` :248; Q_PROPERTYs `<source>State` / `<source>Text` (`.h:90-107`, newest last so ordinals stay); `setFreedvReporter` (:422-446) maps client signals to `setSource(..., kOff|kConnecting|kConnected|kError)`; `restoreAutoStart(Placement)` (:487; FreeDV gated by `FreeDvAutoStart` :534-540; called with `WindowSources`, `Everything`, `StationSources` at `RadioModel.cpp:5871/5886/5892`); `connectSource` (:569); `spots.connect` verb (`SessionCommandDispatcher.cpp:1010`) to `handleSpotSources` (:1699, :4909); `freedvCallsign()` :704 and `freedvGridSquare()` :716 with `resolveCall`/`resolveGrid` :220-240; a start with neither is refused :1136-1138; `applyStationValue` :844-880 reads properties back in a remote window.
- Station link surface, following the rotor PR (#366, `a578b48ee`): capability field in `src/core/session/StationCapabilities.h` (rotor :497-502), encode in `.cpp` (sent only when > 0, :442-446), decode list :920-960 and assignment :1000/:1097; server value accessor as `StationServer::remoteRotorControlVersion()` (`StationServer.cpp:13454`, set :13668); verb rows in `SessionCommandDispatcher::verbSpecs()` (:708, shape at :1037-1044: `{"freedv.sendQsy", {arg("callsign", kUtf8), arg("frequencyHz", kInt)}, "stationFreedvVersion", 1, kRadioIdentitySessionProtocolMinor}`), routed by the else-if chain (:1702) to a handler using `hasExactlyArguments`/`findArgument`/`emitResult`; mirrored objects keyed like `kRotorKey = "rotor"` with `isRotorMessage` (`StationServer.cpp:1463-1469`), watched at :7814/:7844, gated per peer at :9611, applied inbound at :8120/:8130; the class in `kMirroredClasses` (`src/core/session/MirrorSchema.cpp` near :164), properties in `MirrorPolicy.cpp` `kEntries` (:297; rotor :990-1011), enums in `MirrorEnumDomain.cpp:185`; client side `IStationLink.h:528`, `StationClient.cpp:4008` (object binding) and :7194+ (verb senders); `StationCatalog.{h,cpp}`; `src/gui/GuiSessionCoordinator.cpp`.
- Record streams: `StationServer::setUpRecordStreams()` (`StationServer.cpp:14040`) with `m_recordStreams.emplace(name, std::make_unique<RecordStream>(name, cap))`, `upsert(id, fields)`, `remove`, `reset`, `scheduleRecordFlush()` (FreeDV :14083-14114); parameterized names are emplaced up front (`spotConsole:<source>` :14054-14057); `records.subscribe` looks names up in `m_recordStreams` (around :13288), so a stream must exist before anyone subscribes. Client subscribe example `StationClient.cpp:3093`.
- Link fixtures: `tests/data/link/v1/manifest.json` (`{"id":"session-verbs-rotor","file":"sessions/verbs-rotor.json","kind":"session","requires":{"remoteRotorControlVersion":1}}` :184), `tests/data/link/v1/surface.json` checked by `tst_link_surface_manifest` (`tests/CMakeLists.txt:10020`) and regenerated by `tst_link_surface_manifest_regen` into `NEREUS_LINK_REGEN_OUT`; `tests/LinkSurface.cpp` lists capabilities and mirrored meta-objects; conformance tests `tst_link_conformance_{control,session}`.
- Settings over the link: there is no `settings.*` verb. A settings write goes to `StationServer::applySettingsWrite` (`StationServer.cpp:8758`) and `SettingsProxyServer::applyInboundWrite` (`src/core/settings/SettingsProxyServer.cpp:260`), which refuses any key whose `classifySettingsKey(key)` is not `SettingsScope::Station` (:277). The allowlist is `src/core/settings/SettingsScope.cpp`: `kExceptions` (:247), `kPrefixes` (:307, for example `{"FreeDv", Station}` and `{"PskReporter", Station}` :440-441), `kWholeKeys` (:499); unlisted keys are OperatorLocal. Live effect after a write: `StationServer.cpp:8813`/:8972 (`RadioModel::applyRemoteFreedvSetting`, `RadioModel.cpp:6794-6830`).
- Meter and keyed state: `SliceModel::signalStrengthDbm` and `meterReadingNs` (`SliceModel.h:410-418`, :728; steady clock); `MoxController::moxStateChanged(bool)` (`MoxController.h:1498-1501`); `RadioModel::txBoundSlice()` (`RadioModel.h:3548`), `SliceModel::isTxSlice()` (:832) and `txSliceChanged(bool)` (:1577); `RadioModel::activeSlice()` (:2295); `connectedDevices` with `transmittingOn` / `listeningOn` (station link spec :3784-3812); `StationServer::authenticatedSessionCount()` (`StationServer.h:1077-1079`).
- Station key: `StationIdentity` (`src/core/security/StationIdentity.h:11-13, 104-114`, `loadOrCreate`, `publicKeySpki()`, `sign()`); loaded by `StationServer.cpp:2596` from its caller's directory (`defaultDirectory()` only when empty, :2548); a desktop's Remote Access passes the settings profile directory (`src/gui/GuiDesktopStationRuntime.cpp:89`).

**Desktop:**
- Applets: built in `MainWindow` with `panel->addApplet(...)` (Rotor `MainWindow.cpp:11236-11237`), id map :11452, `m_appletVis->registerApplet("Rotor","Rotor",true)` :11506, type ids in `ContainerContentRegistry.cpp:166` and :303. Template class `src/gui/applets/RotorApplet.{h,cpp}` (subclass of `AppletWidget`).
- Pop-out: `AppletWidget::canFloat()` (`AppletWidget.h:62`, false by default; only `ModMonitorApplet.h:81` returns true); `AppletPanelWidget::floatApplet` (`AppletPanelWidget.cpp:306`) makes an `AppletFloatingWindow` (:332) and saves its geometry. Floated pans use `PanFloatingWindow` (`PanadapterStack.cpp:502`).
- VFO flag: `VfoWidget::buildUI()` (`VfoWidget.cpp:806`) stacks row builders; `buildSnrRow` (:1114) is the hidden QLabel template; `setRadeReason` (:1325) is a line precedent.
- Spectrum foot: CPU `SpectrumWidget::paintEvent` (`SpectrumWidget.cpp:7399`) with `drawSpotMarkers` :7459, `drawFreqScale` :7482, `drawBandPlan` :7496 (at `specRect.bottom() - bandH`, :8635); GPU `renderGpuFrame` with `drawBandPlan` :15815 and `drawSpotMarkers` :15843; `markOverlayDirty()`. Both paths must draw.
- Spot Hub: `SpotHubDialog` constructor (`SpotHubDialog.cpp:360`) calls `build*Tab` in order (:410-420); `buildPskTab` :2197 (addTab :2372); constructed at `MainWindow.cpp:18339`.
- Tests: `nereus_add_test(name [sources] [EXTRA_LABELS x] [NATIVE_WINDOW] [CORE_ONLY])` (`tests/CMakeLists.txt:190`); preview scenes in `tests/visual/PreviewScenes.cpp`.

**iPhone and iPad** (`ios/`):
- Generic link clients, no per-feature code in NereusKit: `MirrorStore` (`NereusMirror/MirrorStore.swift:36`, `capabilities`, `objectKeys`), `CommandClient.invoke` (`NereusMirror/CommandClient.swift:134`), `RecordStreamClient` (`NereusMirror/RecordStreamClient.swift:24`, `want(_:backlog:by:)` :165, `unwant(_:by:)` :198), hello features in `NereusLink/LinkFeatures.swift:223-237`, fakes in `NereusKitTesting/FakeStation+FreeDV.swift`.
- Models to copy: `NereusApp/Accessories/RotorModel.swift` (`objectKey`, `capability`, `init(mirror:commands:clock:)` :249, `ToolMirrorWatch` :253, `capabilityVersion` :778, `send` :610) and `NereusApp/FreeDV/FreeDVReporterModel.swift` (capability-gated `want` :197-209, record decoding :213-215, `commands.invoke("freedv.sendQsy", …)` :701).
- Tools list `NereusApp/Tools/StationToolList.swift` (`Page` enum :27-38, `known` :107-127, `entries(...)` :153+), routing `App/ToolsTab.swift:72, 89, 273`.
- Setup > General > Navigation exists: `Setup/SetupTree.swift` (`case navigation` :20, general :114-117), `Setup/NavigationPage.swift` with `PhoneSettings` (`App/PhoneSettings.swift:14`).
- iPad column `iPad/AppletColumn.swift` (:12, body :30-55, `IPadLayout.columnWidth` :28); VFO flag `Band/VfoFlagView.swift` (:136, `FlagMetrics` :19); PTT `Main/PttButton.swift:15`.
- Conformance pins changed by any new capability: `ios/NereusKit/Tests/NereusLinkTests/LinkConformanceControlTests.swift:82-96`, `LinkSurfaceOrdinalTests.swift:29-34`, `LinkConformanceSessionTests.swift`, `MirrorConformanceTests.swift`.

**Mockups** (`docs/architecture/reporter-brainstorm/`): `mockups/board.html` (iPhone and iPad, pictures in `board-shots/`), `placement.html` (desktop), `web.html`, `archive.html`, `review.html`, `settings.html`, `ux-review.md`.

## File Structure

Create:

| Path | Responsibility |
| --- | --- |
| `docs/architecture/2026-10-23-roundtable-v1.md` | The numbered wire spec, the authority for the protocol (Task 1). |
| `roundtable/README.md` | What the server is, how to run its tests, deploy and move it. |
| `roundtable/conformance/v1/` | `manifest.json`, `crypto/`, `control/`, `sessions/`, `turns/` fixtures. |
| `roundtable/server/nereus_roundtable/__init__.py`, `__main__.py`, `config.py`, `clock.py`, `transport.py` | Program entry, config allowlist, fake and real clocks, websockets and HTTP listeners. |
| `roundtable/server/nereus_roundtable/identity.py` | P-256 SPKI, raw signatures, Roundtable ids, key ids. |
| `roundtable/server/nereus_roundtable/protocol.py` | Kinds, envelope rules, field checks, error codes. |
| `roundtable/server/nereus_roundtable/limits.py` | Per-station and per-address-group limits (9.5). |
| `roundtable/server/nereus_roundtable/grid.py` | Maidenhead to position, haversine distance. |
| `roundtable/server/nereus_roundtable/rooms.py` | Families, widths, room matching, openings, membership, generations. |
| `roundtable/server/nereus_roundtable/turns.py` | The turn engine (8.4) and per-turn reading tracks. |
| `roundtable/server/nereus_roundtable/messages.py` | Posts, deletes, names, mentions, text cleaning. |
| `roundtable/server/nereus_roundtable/store.py` | SQLite schema, WAL, every query, flush and reload. |
| `roundtable/server/nereus_roundtable/service.py` | Station connections: sign-in, dispatch, outbound queues. |
| `roundtable/server/nereus_roundtable/live.py` | `/v1/live` watchers and fan-out. |
| `roundtable/server/nereus_roundtable/http.py` | Standard-library asyncio HTTP: read API, static pages, form actions, cookies, CSRF. |
| `roundtable/server/nereus_roundtable/web.py` | Key page, web sign-in, archive data, Delete my archive, history move. |
| `roundtable/server/nereus_roundtable/moderation.py` | Reports, review actions, helpers, block, stop counting, undo, log. |
| `roundtable/server/nereus_roundtable/retention.py` | Daily text clearing, nightly snapshot, disk watch line. |
| `roundtable/server/roundtable.conf.sample` | Every config key with its default. |
| `roundtable/server/static/` | `index.html`, `archive.html`, `review.html`, `key.html`, `roundtable.css`, `roundtable.js`, `archive.js`, `review.js`, `key.js`, brand mark. |
| `roundtable/tests/` | `conftest.py`, `runner.py`, `helpers.py`, `test_*.py`, `load_probe.py`, `test_client.py` (the website-key client for R-RT-24). |
| `roundtable/deploy/` | `nereus-roundtable.service`, `nereus-roundtable-daily.{service,timer}`, `roundtable.caddy`, `caddy-override.conf`, `setup-server.sh`, `pull-backup.sh`. |
| `roundtable/deploy.sh` | rsync to the droplet, never restarts. |
| `src/core/roundtable/RoundtableWire.{h,cpp}` | Encode and decode of protocol v1 messages for the Core. |
| `src/core/roundtable/RoundtableSignalFloor.{h,cpp}` | Signal-present floor per slice (C3). |
| `src/core/roundtable/RoundtableClient.{h,cpp}` | The Core's connection, sign-in, presence, readings, hidden, AWAY, D25 gate, mentions. |
| `src/core/roundtable/RoundtableState.{h,cpp}` | The mirrored `roundtable` object (link state, first-run answered, mention counts, slice rooms). |
| `src/models/RoundtableModel.{h,cpp}` | What screens bind to: rooms, room views, drafts, mute list, actions; fed locally or from the link. |
| `src/gui/roundtable/RoundtablePanel.{h,cpp}` | The room view used docked and popped out. |
| `src/gui/applets/RoundtableApplet.{h,cpp}` | The right-column applet. |
| `src/gui/roundtable/RoundtableWindow.{h,cpp}` | Pop-out window with its three tiers. |
| `src/gui/roundtable/RoundtableFirstRunDialog.{h,cpp}` | "Roundtable is public". |
| `src/gui/roundtable/RoundtableTagLayout.{h,cpp}` | Pan tag placement and folding (pure, testable). |
| `tests/tst_roundtable_*.cpp`, `tests/data/link/v1/sessions/verbs-roundtable.json` | Core, link and desktop tests. |
| `ios/NereusApp/Roundtable/` | `RoundtableModel.swift`, `RoundtableSheet.swift`, `RoundtableRoomView.swift`, `RoundtableToolsPage.swift`, `RoundtableFirstRunSheet.swift`, `RoundtableFlagLine.swift`, `RoundtableTagLayer.swift`, `RoundtableApplet.swift`, `SwipeShortcuts.swift`. |
| `ios/NereusKit/Sources/NereusKitTesting/FakeStation+Roundtable.swift` | The fake station's Roundtable surface. |
| `ios/NereusApp/Tests/Roundtable*Tests.swift`, `ios/NereusApp/UITests/RoundtableUITests.swift` | Phone tests. |

Modify:

| Path | Change |
| --- | --- |
| `.github/workflows/ci.yml` | A `roundtable` job beside `rendezvous` (Task 2). |
| `website/deploy/Caddyfile`, `website/deploy/setup-server.sh` | `import /etc/caddy/sites/*.caddy`; the setup script makes `/etc/caddy/sites/` (Task 9). |
| `docs/manual/build_preview.py`, `website/public/roundtable/` | API page route and published fixtures (Task 9). |
| `src/core/SpotSourceHost.{h,cpp}` | `kRoundtable` source, properties appended last, setter, connect, auto-start (Task 10). |
| `src/models/RadioModel.{h,cpp}` | Own `RoundtableClient`, `RoundtableState`, `RoundtableModel`; wire slices, meter, keyed, at-radio (Task 10). |
| `src/core/settings/SettingsScope.cpp` | `{"Roundtable", Station}` prefix and the two OperatorLocal exceptions (Task 11). |
| `src/core/session/StationCapabilities.{h,cpp}`, `StationServer.{h,cpp}`, `SessionCommandDispatcher.{h,cpp}`, `MirrorSchema.cpp`, `MirrorPolicy.cpp`, `MirrorEnumDomain.cpp`, `StationCatalog.{h,cpp}`, `StationClient.{h,cpp}`, `IStationLink.h` | The link surface (Task 11). |
| `src/gui/GuiSessionCoordinator.cpp` | Feed `RoundtableModel` from the link in a window placement (Task 11). |
| `docs/architecture/2026-09-23-station-link-v1.md` | Capability, object, streams, verbs (Task 11). |
| `tests/LinkSurface.cpp`, `tests/data/link/v1/manifest.json`, `surface.json`, existing `sessions/*.json` ordinals | Link fixtures (Task 11). |
| `src/gui/SpotHubDialog.{h,cpp}`, `src/gui/MainWindow.{h,cpp}` | Roundtable tab; applet and window wiring (Tasks 12, 13). |
| `src/gui/applets/AppletWidget.h`, `AppletPanelWidget.cpp` | Pop-out hook for an applet that owns its own window (Task 13). |
| `src/gui/ContainerContentRegistry.cpp` | `{"Roundtable","roundtable"}` and `applet:roundtable` (Task 13). |
| `src/gui/SpectrumWidget.{h,cpp}`, `src/gui/widgets/VfoWidget.{h,cpp}` | Pan tags (both paint paths) and the flag line (Task 14). |
| `tests/visual/PreviewScenes.cpp` | Roundtable scenes (Tasks 12 to 14). |
| `tests/CMakeLists.txt` | Register new tests. |
| `ios/NereusKit/Sources/NereusLink/LinkFeatures.swift`, NereusKit conformance pins | Capability and ordinals (Task 16). |
| `ios/NereusApp/Band/VfoFlagView.swift`, `Main/PttButton.swift`, `Tools/StationToolList.swift`, `App/ToolsTab.swift`, `Setup/NavigationPage.swift`, `App/PhoneSettings.swift`, `iPad/AppletColumn.swift`, the panadapter view | Phone and iPad screens (Tasks 17 to 19). |

## Task 1: Wire spec v1 and conformance fixtures

**Requirements:** R-RT-1 (the document and its fixtures); spec sections 7, 8, 9, 13 as protocol text.

**Files:**
- Create: `docs/architecture/2026-10-23-roundtable-v1.md`
- Create: `roundtable/conformance/v1/manifest.json`, `crypto/*.json`, `control/*.json`, `sessions/*.json`, `turns/*.json`

**Interfaces:**
- Consumes: nothing.
- Produces:
  - The wire spec with stable section numbers in the rendezvous document's shape: preamble (what the service is, this document is the authority, numbers are stable), 1 What it does and does not do, 2 Transport (paths, frames, heartbeat), 3 Roles and the life of a connection (station, live, read API, browser), 4 Identity values (4.1 Core SPKI and the Roundtable id, 4.2 website keys and key ids, 4.3 sign-in transcript, 4.4 row refs), 5 Messages (5.1 Envelope, 5.2 Field kinds, 5.3 Station to server, 5.4 Server to station, 5.5 Live), 6 Rooms, turns and readings (the rules of spec 8 as normative text, with the C1 family table and the frequency per mode), 7 Flows (sign-in, join, talking turn, unknown turn and naming, double, resync, hidden, weblink, delete archive), 8 Errors (codes and plain reasons), 9 Limits, storage and logs, 10 Read API (every path, query and JSON shape), 11 Conformance (files, placeholders, runner), 12 Versions and changing the protocol.
  - Exact values (normative, used by every later task):
    - Id: `base32(SHA-256("NereusSDR roundtable id v1\n" || SPKI DER))[0..26]`, RFC 4648 alphabet, lowercase, no padding. Key id: same with prefix `"NereusSDR roundtable key v1\n"` over the 32 raw key bytes.
    - Sign-in transcript: `"NereusSDR roundtable hello v1\n" || host || "\n" || nonce`, `host` as ASCII bytes, `nonce` the 32 raw bytes; signature raw `r||s` (64 bytes), base64url without padding.
    - Envelope: one JSON object per text frame, `type` a string; duplicate keys are a protocol error; unknown keys ignored; unknown kind from a client closes the connection with code 4002.
    - `hello`: `{"type":"hello","v":1,"software":"NereusSDR","softwareVersion":"26.10.3","spki":"<b64url>","sig":"<b64url>"}` or `{"type":"hello","v":1,"software":"...","softwareVersion":"...","key":"<b64url>"}`.
    - `presence`: `{"type":"presence","callsign":"KG4VCF","grid":"EM73","atRadio":true,"slices":[{"id":0,"hz":14212000,"mode":"USB","filterHz":2700,"keyed":false}]}`; modes `LSB USB DIGL DIGU AM SAM FM CWL CWU FDVL FDVU`.
    - `reading`: `{"type":"reading","slice":0,"dbm":-91.5,"t":1800000000123,"present":true}` (`t` Unix ms).
    - Room id `<family>-<center Hz>`, families `lsb usb digl digu am fm cw fdvl fdvu`.
    - Generations: unsigned integers per `rooms` list and per room, starting at 1 at each `rooms` or `room` full view.
    - Error codes: `not-member`, `away`, `hidden`, `blocked`, `rate`, `too-long`, `unknown-slice`, `slice-keyed`, `bad-turn`, `not-allowed`, `not-found`, `version`, `server`; close codes 4001 sign-in failed, 4002 protocol error, 4003 replaced by a newer connection, 4004 key revoked.
  - Fixture formats copied from rendezvous with Roundtable placeholders, plus a `turns` kind: `{"runs":["service"],"setup":{"stations":[...]},"steps":[{"presence":...},{"reading":...},{"advanceMs":N},{"expectTurns":[{"kind":"talking","talker":"A","startMs":0,"endMs":null}]}]}`.

**Acceptance:**
- Every message kind in spec 9.2 and 9.3 has a section and at least one accepted and one refused control fixture.
- Crypto fixtures: for two fixed P-256 test keys, the SPKI, the Roundtable id, the rendezvous id (to show they differ), a key id, and a valid transcript signature; plus a rendezvous-prefixed signature and one naming another host, both marked invalid.
- Session fixtures cover: sign-in by SPKI, sign-in by key, a revoked key (close 4004), a second connection replacing the first (close 4003), a reading from a joining station dropped silently, a post from a non-member refused `not-member`, a post from an AWAY station refused `away`, `hidden` ending presence at once, `resync` after a skipped generation, `weblink` returning `weblinkOk`.
- Turn fixtures cover every rule in spec 8.4: talking; unknown then talking; doubled in and out with readings kept out; quiet after 3 s; same talker back under 3 s continues; 10-minute long key; digital room with two keyed members (both turns, overlap kept out) and an unknown turn; the join dwell (10 s) and keying during it.
- `manifest.json` lists every fixture with its kind; ids are unique.
- No em-dashes; American spelling; every number agrees with spec sections 8, 9 and 18.

**Verification:** documentation and fixtures (the documentation row of the risk table, but the fixtures are a contract later tasks run). A `python3 -c` check that every fixture parses as JSON with no duplicate keys and every manifest file exists; a read-through against spec sections 7 to 9. The fixtures are proved by Task 2 and Task 3, which run them.

**Execution note (advisory):** opus (it defines the protocol every other task builds on). Networking: yes, by definition; it is the interface worth an early independent look, so the controller may ask JJ whether to review it before Task 2. No prerequisites.

- [ ] **Step 1:** Write the wire spec from spec sections 7 to 9 and 13, with the exact values above.
- [ ] **Step 2:** Write the crypto fixtures with a small one-off script in the scratchpad (not committed) using `cryptography`; record the two test private keys in the fixtures as the rendezvous fixtures do.
- [ ] **Step 3:** Write the control, session and turn fixtures and the manifest.
- [ ] **Step 4:** Run the JSON check; commit.

## Task 2: Server foundation and sign-in

**Requirements:** R-RT-1 (the server's runner passes the crypto, control and sign-in session fixtures), R-RT-2, R-RT-7 (envelope, limits and address groups); spec 6.1, 7.1 to 7.3, 9.1, 9.5.

**Files:**
- Create: `roundtable/server/nereus_roundtable/{__init__,__main__,config,clock,transport,identity,protocol,limits,store,service}.py`, `roundtable/server/roundtable.conf.sample`
- Create: `roundtable/tests/{conftest,runner,helpers}.py`, `roundtable/tests/test_crypto.py`, `test_control.py`, `test_sessions.py`, `test_limits.py`, `test_config.py`, `test_store_schema.py`
- Create: `roundtable/README.md` (first version: what it is, how to run the tests)
- Modify: `.github/workflows/ci.yml` (job `roundtable`: the `rendezvous` job's steps with `roundtable/tests`)

**Interfaces:**
- Consumes: Task 1's wire spec and fixtures.
- Produces:
  - `python3 -m nereus_roundtable --config FILE`.
  - `config.Config` with at least: `ws_listen` (`["127.0.0.1:8720","[::1]:8720"]`), `http_listen` (`8721` likewise), `public_host` (`"roundtable.nereussdr.com"`), `database` (`/var/lib/nereus-roundtable/roundtable.db`), `backup_dir`, `static_dir`, `helpers` (list of station ids; JJ's is set at deploy), `trusted_proxies` (loopback), and a `[limits]` section with every value from spec 9.5. Unknown sections or keys refuse to start.
  - `clock.RealClock` / `clock.ManualClock` with `now_ms()` (monotonic), `wall_ms()` (Unix ms), `call_later(ms, fn)`; `ManualClock.advance(ms)`.
  - `identity`: `roundtable_id(spki: bytes) -> str`, `key_id(key: bytes) -> str`, `rendezvous_id(spki) -> str` (tests only, to prove difference), `verify_hello(spki, sig, host, nonce) -> bool`, `new_key() -> bytes`.
  - `protocol`: `decode(text) -> dict` raising `ProtocolError(close_code)`, `encode(obj) -> str`, `KINDS_FROM_STATION`, `ERROR_CODES`.
  - `limits.AddressGroups.group(addr: str) -> str` (IPv4 whole, IPv6 /64), `limits.Window(count, per_ms)` with `allow(key, now_ms) -> bool`.
  - `store.Store(path)`: opens SQLite in WAL mode, creates the schema of spec 6.3 (`stations`, `keys`, `rooms`, `turns`, `turn_readings`, `turn_names`, `messages`, `reports`, `review_actions`, `helpers`, `sessions`, `web_sessions`, `weblinks`, `schema_version`), with no address column anywhere; `station_upsert(id, kind, callsign, grid, now)`, `key_add(hash, station_id, now)`, `key_lookup(hash)`, `key_revoke(hash, now)`.
  - `service.Service(config, clock, store)` with `async handle_station(ws, peer_addr)`: sends `challenge`, accepts `hello`, replaces an older connection of the same id (close 4003), sends `welcome` with `stationId`, `limits`, `serverTime`, `mentions` (empty until Task 4).
  - `transport.start(service, http_app, config)` starts both listeners; non-upgrade requests on the WebSocket port get 426; `X-Forwarded-For` is trusted only from `trusted_proxies`.
  - `tests/runner.py`: `run_fixture(path)` running a real `Service` on loopback with `ManualClock`, for kinds `session` and (from Task 3) `turns`.

**Acceptance:**
- All Task 1 crypto and control fixtures pass; the sign-in, revoked-key, replace-connection and envelope session fixtures pass.
- A rendezvous-prefixed signature and a signature over another host are refused with close 4001.
- `limits`: `2001:db8:1:2::1` and `2001:db8:1:2:ffff::9` share a group; `2001:db8:1:3::1` does not; `192.0.2.1` and `192.0.2.2` do not.
- `X-Forwarded-For: 198.51.100.7` from `127.0.0.1` is used; from `203.0.113.5` it is ignored.
- A config with an unknown key exits with code 2 and a plain message.
- The schema test opens a fresh database, checks `PRAGMA journal_mode` is `wal`, and checks that no column in any table is named like `addr`, `ip` or `address`.
- The tests pass on `/usr/bin/python3` here; the CI job runs the same command.

**Verification:** authorization boundary (sign-in) and networking: the fixtures are the regression coverage, written before the service code they exercise. Commands: `/usr/bin/python3 -m pytest roundtable/tests -q -p no:cacheprovider`. CI evidence once pushed.

**Execution note (advisory):** opus. Networking and sign-in: yes. Prerequisite Task 1. Lane S.

- [ ] **Step 1:** Copy the rendezvous `clock`, `transport`, `config` and `identity` patterns (our own code) under the new package with Roundtable's names, prefixes and keys.
- [ ] **Step 2:** Write `protocol`, `limits`, `store` (schema only plus station and key queries), and the sign-in part of `service`.
- [ ] **Step 3:** Write the runner and tests; make every fixture named above pass.
- [ ] **Step 4:** Add the CI job; write the README's first version; commit.

## Task 3: Rooms, presence, readings and turns

**Requirements:** R-RT-3, R-RT-4, R-RT-5, R-RT-7 (input refusals); the server side of R-RT-6 and R-RT-12; spec 8.1 to 8.5, 9.2 to 9.4 (rooms, room, deltas, generations, read/unread, resync), 13 (restart, clock skew), 6.3 write rule.

**Files:**
- Create: `roundtable/server/nereus_roundtable/{grid,rooms,turns}.py`
- Modify: `service.py` (presence, reading, read, unread, resync dispatch; outbound `rooms`, `roomsDelta`, `room`, `delta`), `store.py` (rooms, openings, turns, turn_readings, flush, reload)
- Create: `roundtable/tests/test_grid.py`, `test_rooms.py`, `test_turns.py` (runs every `turns` fixture), `test_restart.py`, `test_readings.py`

**Interfaces:**
- Consumes: Task 2's `Service`, `Store`, `ManualClock`, limits.
- Produces:
  - `grid.position(grid: str) -> (lat, lon) | None` (2, 4 or 6 characters, square center; None if invalid); `grid.distance_km(a, b) -> float | None` (haversine, Earth radius 6371.0 km, rounded to whole km).
  - `rooms.FAMILIES`: mode to family and half-width in Hz: `LSB lsb 1500`, `USB usb 1500`, `DIGL digl 1500`, `DIGU digu 1500`, `AM am 3000`, `SAM am 3000`, `FM fm 6000`, `CWL cw 300`, `CWU cw 300`, `FDVL fdvl 1500`, `FDVU fdvu 1500`.
  - `rooms.RoomBook(clock, store)`: `place(station_id, slice) -> room_id` (nearest center in the family within the half-width, lower center on a tie, else new room at the slice's frequency), `members(room_id)`, `leave(station_id, slice_id)`, join dwell 10 000 ms or immediate on keyed, leave after 30 000 ms silent, close 600 000 ms after the last member, `generation(room_id)`, `rooms_generation()`.
  - `turns.TurnEngine(room, clock)`: `on_presence(...)`, `on_reading(...)`, `tick()`, emitting turn changes; rules exactly spec 8.4 with C4 (3 000 ms quiet, 3 000 ms same-talker rejoin), C21 (600 000 ms long key), C19 (digital).
  - `turns.Track`: one byte per second (`dBm + 160` rounded to whole dB and clamped to 0..254; 255 no reading) and one present bit per second; flushed to `turn_readings` every 30 000 ms and at turn end.
  - Outbound shapes per wire spec 5.4: `rooms` (full, generation), `roomsDelta` (at most every 2 000 ms), `room` (full view with members, readings, current turn, earlier turns of the last 30 minutes, messages left empty until Task 4), `delta`.
  - Reading rows carry the reader's callsign, row ref (random 8 characters per room opening), distance km (or null), offset Hz (`hz - center`, rounded to 10 Hz), dBm, age source time, AWAY.

**Acceptance:**
- Every `turns` fixture from Task 1 passes.
- An LSB and a USB slice on 7 200 000 Hz land in different rooms; two USB slices 1 400 Hz apart share one; 1 600 Hz apart do not; a slice exactly between two centers joins the lower.
- A room's id survives a restart: write two rooms, restart the service on the same database with `ManualClock` 5 minutes later, rooms reload with the same ids and centers; 11 minutes later they do not.
- A reading for a slice not in presence is refused `unknown-slice`; a reading on a keyed slice is refused `slice-keyed`; readings from a joining station are dropped with no error; readings over 2 a second per slice are dropped with at most one `rate` error a minute.
- A reading stamped 6 s from server time takes its arrival time; one stamped 4 s off keeps its own.
- Two slices of one station in one room: only the first listed in presence produces readings.
- Track packing: -160 dBm is byte 0, -73 dBm is 87, +94 dBm is 254, a missing second is 255; a two-minute turn of one station stores 120 + 15 bytes of tracks.
- `grid.distance_km("EM73", "FN31")` is within 1 km of the haversine distance between their square centers computed independently in the test; an invalid grid gives None and a blank distance.
- A client that skips a generation gets nothing until it sends `resync`, then a full view.

**Verification:** consequential state (the turn engine decides what is shown and archived): test-first from the Task 1 turn fixtures. `/usr/bin/python3 -m pytest roundtable/tests -q -p no:cacheprovider`.

**Execution note (advisory):** opus. Networking: protocol only. Prerequisite Task 2. Lane S.

- [ ] **Step 1:** Write `grid` and its tests.
- [ ] **Step 2:** Write `rooms` and `turns` against the turn fixtures (red first).
- [ ] **Step 3:** Wire presence and readings in `service`, the outbound views, generations and resync.
- [ ] **Step 4:** Persist openings, turns and tracks with the flush rule; reload on start; commit.

## Task 4: Messages, names, mentions and text safety

**Requirements:** R-RT-8, R-RT-11 (store side), the server side of R-RT-12 and R-RT-13; spec 8.6, 9.2 (`post`, `delete`, `name`, `report`, `mentionsSeen`, `hidden`), 9.3 (`posted`, `mention`), 11.1 message rules, 12.2, 12.4.

**Files:**
- Create: `roundtable/server/nereus_roundtable/messages.py`
- Modify: `service.py`, `store.py` (messages, names, reports intake, mention read state per station)
- Create: `roundtable/tests/test_messages.py`, `test_names.py`, `test_mentions.py`, `test_text_safety.py`, `roundtable/tests/data/text-corpus.json`

**Interfaces:**
- Consumes: Tasks 2 and 3.
- Produces:
  - `messages.clean(text) -> str | None` (NFC; control characters other than `\n` removed; None if over 500 code points or 10 lines after cleaning, or empty).
  - `messages.base_callsign(call) -> str` (upper case; prefix before `/` dropped when the part after it is longer, suffixes `/P /M /MM /AM /QRP` and any single trailing `/X` dropped).
  - `messages.mentions(text, callsigns) -> set` (whole-word, base-callsign match, case-insensitive).
  - Store: `message_add(...)` with callsign at the time and the sender's current reading of the talker; `message_delete(id, by_station)`; `turn_name(turn_id, station_id, name)` (one per station, replaceable); `name_counts(turn_id)`; `names_recent(room_id, since)`; `report_add(kind, target, reason, by)`; `mention_seen(station_id, up_to_ms)`; `unread_mentions(station_id)`.
  - The `room` view gains the last 50 messages; `GET /v1/api/room/<family>/<hz>/messages?before=<id>` arrives in Task 5.
  - `welcome.mentions` lists unread mentions since the last `mentionsSeen`.

**Acceptance:**
- A member posts; others in the room get a `delta` with the message, its callsign at the time and the sender's reading of the talker; the sender gets `posted` with the id.
- 7 posts in a minute: the 7th is refused `rate`; a 501-code-point post is refused `too-long`; an AWAY station's post is refused `away`; a non-member's `not-member`.
- Delete: the text becomes null, `deletedBySender` true, the row stays.
- Naming: a station that was a member during the unknown turn and is a member now may name it up to 24 h after; one that joined after the turn is refused `not-allowed`; one that left and is elsewhere is refused `not-member`; a talking or doubled turn is refused `bad-turn`; changing one's name replaces it; counts read `{"W4XYZ": 3, "K4ABC": 1}`; no name moves the turn into any archive (checked in Task 6's archive query, and here by asserting the `sessions` table is untouched).
- Mentions: "thanks kg4vcf/p" mentions KG4VCF; "KG4VCFX" does not; an offline station gets the mention in its next `welcome`.
- The text corpus (script tags, `javascript:` links, bidi overrides, zero-width joiners, 11 lines, combining marks) stores cleaned or refused as the corpus file says.
- After `hidden`, the station's presence ends at once (members get a `delta`), and further `reading` or `post` on that connection are refused `hidden`.

**Verification:** ordinary feature plus a safety corpus: tests with the behavior. `/usr/bin/python3 -m pytest roundtable/tests -q -p no:cacheprovider`.

**Execution note (advisory):** opus. Prerequisite Task 3. Lane S.

- [ ] **Step 1:** Write `messages` with the corpus tests.
- [ ] **Step 2:** Posts, deletes, names, reports intake and mentions in `service` and `store`.
- [ ] **Step 3:** `hidden` and `mentionsSeen`; commit.

## Task 5: Live stream, read API and HTTP

**Requirements:** R-RT-7 (live and API limits), the API part of R-RT-19; spec 9.4, 9.5 (live sockets, read API), 12.1.

**Files:**
- Create: `roundtable/server/nereus_roundtable/{live,http}.py`
- Modify: `transport.py` (HTTP listener), `service.py` (fan-out to live watchers)
- Create: `roundtable/tests/test_live.py`, `test_read_api.py`, `test_http.py`, `test_public_data.py`

**Interfaces:**
- Consumes: Tasks 2 to 4.
- Produces:
  - `/v1/live` kinds `watchBand` (`{"type":"watchBand","band":"20m"}` or `"all"`), `watch` / `unwatch` (room id, at most 4), `resync`; sends `rooms`, `roomsDelta`, `room`, `delta` exactly as stations get them, minus anything station-only.
  - `http.App(service, store, config)`: an asyncio HTTP/1.1 server from the standard library (`asyncio.start_server`, request line, headers, `Content-Length` bodies up to 16 KiB, keep-alive off), routes `GET /v1/api/rooms`, `GET /v1/api/room/<family>/<hz>`, `GET /v1/api/room/<family>/<hz>/messages?before=<id>`, `GET /v1/api/callsign/<call>`, static files from `static_dir` with the CSP header `default-src 'self'; connect-src 'self'; img-src 'self' data:; style-src 'self'; script-src 'self'; frame-ancestors 'none'; form-action 'self'`, and a route table later tasks add form actions to (`app.route(method, path, handler)`).
  - Bands from frequency: the IARU Region 2 amateur band edges, 160 m to 70 cm, in a table in `rooms.py`; anything outside is band `"other"`.

**Acceptance:**
- A live watcher of `20m` sees a room open on 14 212 000 Hz within one delta; a watcher of `40m` does not.
- A fifth `watch` is refused; a fifth live socket from one address group is closed; the 501st live socket in all is refused.
- 61 API requests in a minute from one group: the 61st gets 429.
- `test_public_data.py` walks every live and API message the fixtures produce and fails if any value equals a station id, a key id or an address.
- `/v1/api/room/usb/14212000` for a closed room returns its most recent opening, marked closed, never 404 once it existed; an unknown room returns 404.
- `/v1/api/callsign/KG4VCF` with two stations using the callsign returns two entries with their own rooms and row refs.
- Every static response carries the CSP header; no route serves a directory listing; `..` in a path returns 404.

**Verification:** ordinary feature, networking: tests with the behavior. `/usr/bin/python3 -m pytest roundtable/tests -q -p no:cacheprovider`.

**Execution note (advisory):** opus. Networking: yes. Prerequisite Task 4. Lane S.

- [ ] **Step 1:** HTTP server and static routes with their tests.
- [ ] **Step 2:** Read API.
- [ ] **Step 3:** Live watchers, limits, the public-data walk; commit.

## Task 6: Keys, web sign-in, archive data and Delete my archive

**Requirements:** R-RT-20 (server side), R-RT-22, R-RT-23; spec 6.4 (sessions), 7.3, 7.4, 11.6 (data), 13 (Delete my archive), C6, C9, C14.

**Files:**
- Create: `roundtable/server/nereus_roundtable/web.py`
- Modify: `service.py` (`weblink`, `deleteArchive`), `store.py` (sessions, web sessions, weblinks, history move, archive queries, delete), `http.py` routes
- Create: `roundtable/tests/test_keys.py`, `test_weblink.py`, `test_archive.py`, `test_delete_archive.py`

**Interfaces:**
- Consumes: Tasks 2 to 5.
- Produces:
  - Form actions: `POST /key/new` (callsign, grid; returns the key once; 3 a day per address group, counted in memory), `POST /key/signin` (key; sets the cookie), `POST /key/profile` (callsign, grid; signed in by key), `POST /key/move` (signed in by the old key; issues a new key, revokes the old, moves history in one transaction), `GET /signin/<token>` (one-time, 5 minutes; sets the cookie and redirects with `303` to `/archive`), `POST /signout-everywhere`, `POST /archive/delete` (with the confirm field).
  - Cookie `rt_session`: `Secure; HttpOnly; SameSite=Strict; Path=/; Max-Age=2592000`. Every POST checks `Origin` equals `https://<public_host>` and a per-session CSRF token field.
  - `weblink` on a station socket returns `weblinkOk` with `https://<public_host>/signin/<token>`.
  - Archive JSON for the signed-in station: `GET /v1/me/sessions`, `GET /v1/me/session/<id>` (per reader: callsign at the time, distance, watched seconds from present bits, the tracks, AWAY), `GET /v1/me/histogram`. Sessions are built from the station's talking turns only (C18: gaps under 10 minutes); unknown turns never appear, whatever their names.
  - `store.move_history(from_id, to_id, by)` (logged in `review_actions`), used by `/key/move` and by Task 7's moderator move.
  - `store.delete_archive(station_id)` per spec 13 and C9.

**Acceptance:**
- A key works once issued; the stored row holds only its SHA-256; a fourth key in a day from one group is refused.
- `/key/move` by the old key moves sessions, turns, readings, names and messages, revokes the old key (its socket closes 4004) and nothing is left under the old id; a request naming a callsign instead of a key does nothing.
- A sign-in link works once, and not after 5 minutes (ManualClock); the redirect leaves the token out of the final address.
- A POST without the CSRF token or with `Origin: https://evil.example` is refused 403.
- Sign out everywhere ends every cookie of that station.
- Archive: three talking turns 4, 6 and 20 minutes apart make two sessions; a reader whose present bits are true for 50 of 120 s shows 50 s watched; readings from a doubled turn are absent; a named unknown turn is absent.
- Delete my archive: the station's sessions and talking turns and the readings others made of them are gone; its own readings of other talkers remain under its stored callsign in their archives; its message texts are null.

**Verification:** authorization and secrets (keys, cookies, history moves): tests first for the refusals, then the flows. `/usr/bin/python3 -m pytest roundtable/tests -q -p no:cacheprovider`.

**Execution note (advisory):** opus. Secrets and authorization: yes; worth flagging for an earlier look. Prerequisite Task 5. Lane S.

- [ ] **Step 1:** Refusal tests (CSRF, origin, expired and reused links, key limits).
- [ ] **Step 2:** Keys, web sign-in, cookies.
- [ ] **Step 3:** Sessions, archive JSON, history move, Delete my archive; commit.

## Task 7: Moderation, retention and backups

**Requirements:** R-RT-10, R-RT-21 (server side); spec 6.2 (backups, disk watch), 6.4, 7.5, 11.7 (behavior), 12.2, 12.3, C12, C16, C21.

**Files:**
- Create: `roundtable/server/nereus_roundtable/{moderation,retention}.py`
- Modify: `service.py` (`notice` for blocked, not counted, report looked at), `store.py`, `http.py` routes, `rooms.py` / `turns.py` (not-counted stations ignored live and for crediting)
- Create: `roundtable/tests/test_moderation.py`, `test_retention.py`, `test_backup.py`

**Interfaces:**
- Consumes: Tasks 2 to 6.
- Produces:
  - Helper-only JSON and actions (cookie of a helper's station): `GET /v1/review/reports`, `POST /v1/review/remove`, `POST /v1/review/block` (until a time or forever), `POST /v1/review/stop-counting` (until a time or forever), `POST /v1/review/leave`, `POST /v1/review/undo` (within 600 000 ms), `POST /v1/review/helpers/add` and `/remove` (by station id), `POST /v1/review/move-history` (two station ids), `GET /v1/review/log`, `GET /v1/review/stations?callsign=` (station ids with callsigns, for picking).
  - Each action carries the report's version; an action on a report another helper changed is refused 409 with what they did.
  - `retention.daily(now)`: clears text older than 365 days (04:00 UTC by the timer), writes the disk line (database size, free disk, warning under 20% free).
  - `retention.snapshot(dir, now)`: SQLite online backup to `roundtable-YYYY-MM-DD.db`, keeps 2, skipped with a warning under 15% free disk.
  - `python3 -m nereus_roundtable --config FILE --daily` and `--snapshot` run one job and exit (for the timers).

**Acceptance:**
- Remove: the message text is null, the room shows "Message removed", the log has the helper and time; Undo within 10 minutes restores it; at 10 minutes and 1 second Undo is refused.
- Block: posts and names refused `blocked` with a `notice`; readings still accepted and shown.
- Stop counting: the station's readings and keyed state stop affecting live views and turn crediting (a keyed claim from it opens no turn); its stored rows are kept and marked; lifting it restores live counting.
- Two helpers act on one report: the second gets 409 with the first's action.
- A non-helper gets 403 on every review route; helpers are added and removed by station id only, and each change is logged.
- Retention: a message 366 days old loses its text; one 364 days old keeps it; the row, sender, time and reading stay.
- Snapshot: the file opens, its row counts equal the live database's; a third snapshot removes the oldest; a fake free-disk value of 14% skips it with a warning line.

**Verification:** consequential state (removals, blocks, deletes): tests with the behavior. `/usr/bin/python3 -m pytest roundtable/tests -q -p no:cacheprovider`.

**Execution note (advisory):** opus. Authorization: yes. Prerequisite Task 6. Lane S.

- [ ] **Step 1:** Moderation store and routes with the refusal tests.
- [ ] **Step 2:** Live effects of block and stop counting.
- [ ] **Step 3:** Retention, snapshot, disk line, the one-shot flags; commit.

## Task 8: Web pages

**Requirements:** R-RT-11 (render inert), R-RT-18 (web), R-RT-19, R-RT-20 (browser side), R-RT-21 (page); spec 11.1 (rules that apply on the web), 11.5, 11.6, 11.7, the key page (7.3), U4, U6, U11, U12, U13.

**Files:**
- Create: `roundtable/server/static/{index,archive,review,key}.html`, `roundtable.css`, `roundtable.js`, `archive.js`, `review.js`, `key.js`, the brand mark (copied from `website/public/`)
- Create: `roundtable/tests/test_pages.py`, `roundtable/tests/pages/` (headless checks)

**Interfaces:**
- Consumes: Tasks 5 to 7's routes and live stream.
- Produces: the four pages, matching `docs/architecture/reporter-brainstorm/mockups/web.html`, `archive.html` and `review.html` (the 2026.10.1 workspace look; state colors `#4caf6a` talking, `#d8a55f` unknown, `#c25a5c` doubled, `#00d4ff` you; shapes per U7), with no inline script or style (CSP) and every text inserted as text, never as HTML, except the fixed formatting set built from DOM nodes.

**Acceptance:**
- With a scripted server of 200 rooms over 8 bands: the band buttons show counts and the talking/unknown bar; picking 20m shows the band map with stems by member count; quiet rooms fold with a count; search finds a callsign that is talking, reading or posting, and a typed frequency jumps to its room; sort Frequency, Most stations, Newest activity reorder rows; each row shows frequency, mode, state, talker, time, members, the reading strip nearest first, last message, age.
- Keyboard: `/` focuses search; arrows move rooms and each shows as you land; Enter enters the room and the arrows walk earlier turns; Escape steps back room, search, band.
- A reading older than 20 s turns grey with an amber age; a room with nothing newer than 60 s turns grey; the connection line reads Live, Reconnecting, Offline since, Connecting as the test drives the socket.
- Copy link gives `https://roundtable.nereussdr.com/r/usb/14212000`, and opening it shows that room.
- The text corpus from Task 4 renders inert: no script runs (a page-level flag set by any injected script stays unset), links show their full address and only http and https are links.
- Archive with a signed-in fixture station: sessions list, a session's per-station rows with WATCHED and timelines (amber hatching for keyed-and-lost), the first 12 then "Show all 34", the hourly histogram, "Nobody reported you" for an empty session, "Message removed after a year".
- Review list with a helper fixture: oldest first with the older-than-a-day count, every action, the conflict message, the Undo countdown, the log.
- No text under 11 px and every text at least 4.5:1 against its ground, except disabled controls, measured by the check script on every page.
- Screenshots of each page at 1480 x 1000 and 390 x 844, compared by eye against the mockups and listed in the report.

**Verification:** UI: `ui-verification` decides; the headless checks are functional evidence and the screenshots are the visual evidence. Headless Chrome as the board was checked: `"/Applications/Google Chrome.app/Contents/MacOS/Google Chrome" --headless=new --disable-gpu --hide-scrollbars --window-size=W,H --screenshot=<out> <url>` against a test server started by `tests/pages/serve.py`, plus `--dump-dom` with a probe script for the functional checks. The pytest wrapper skips (with a printed reason) where Chrome is absent, so CI stays green; the checks are run here and their output is in the report.

**Execution note (advisory):** opus. No device risk. Prerequisite Task 7. Lane S.

- [ ] **Step 1:** Public page from `web.html`, on the live stream.
- [ ] **Step 2:** Archive and review pages from their mockups; the key page.
- [ ] **Step 3:** Headless checks, contrast and size check, screenshots; commit.

## Task 9: Load test, deploy files and the API page

**Requirements:** R-RT-9 (local part), R-RT-25; spec 6.2, 9.7, C10, C12.

**Files:**
- Create: `roundtable/tests/load_probe.py`, `roundtable/tests/test_deploy_files.py`
- Create: `roundtable/deploy/{nereus-roundtable.service,nereus-roundtable-daily.service,nereus-roundtable-daily.timer,nereus-roundtable-snapshot.service,nereus-roundtable-snapshot.timer,roundtable.caddy,caddy-override.conf,setup-server.sh,pull-backup.sh}`, `roundtable/deploy.sh`
- Modify: `website/deploy/Caddyfile` (one line `import /etc/caddy/sites/*.caddy` after the global block), `website/deploy/setup-server.sh` (creates `/etc/caddy/sites/` before validating), `docs/manual/build_preview.py` (route rendering `docs/architecture/2026-10-23-roundtable-v1.md` to `roundtable/api.html` and copying `roundtable/conformance/v1/` to `website/public/roundtable/conformance/v1/`), `website/public/roundtable/` (generated), `roundtable/README.md` (deploy, backups, moving)

**Interfaces:**
- Consumes: Tasks 2 to 8.
- Produces:
  - Unit: `ExecStart=/usr/bin/python3 -m nereus_roundtable --config /etc/nereus-roundtable/roundtable.conf`, `PYTHONPATH=/opt/nereus-roundtable`, `DynamicUser=yes`, `StateDirectory=nereus-roundtable`, `MemoryMax=256M`, `MemorySwapMax=0`, `OOMScoreAdjust=500`, loopback-only IP rules, and the rendezvous unit's sandbox options; no outbound network.
  - Timers: daily job `OnCalendar=*-*-* 04:00:00 UTC`; snapshot `OnCalendar=*-*-* 03:30:00 UTC`.
  - `roundtable.caddy`: the `roundtable.nereussdr.com` site block; `/v1/station*` and `/v1/live*` to `127.0.0.1:8720 [::1]:8720` with `stream_close_delay 5m`; everything else to `:8721`; `header_up X-Forwarded-For {remote_host}`; no access log.
  - `caddy-override.conf` for nereus-web: `Restart=on-failure`, `OOMScoreAdjust=-100`, `GOMEMLIMIT` as rv's.
  - `setup-server.sh`: idempotent, `--dry-run`, `--no-start`; installs Python apt packages, the unit, timers, config, `/etc/caddy/sites/roundtable.caddy`, the Caddy drop-in; validates Caddy before reloading.
  - `pull-backup.sh`: run on JJ's machine; pulls the newest snapshot over SSH, keeps 7.
  - `load_probe.py`: drives N stations in M rooms sending presence every 10 s and readings once a second, plus V live viewers, for D minutes; prints peak RSS (from `/proc/self/status` on Linux or `resource.getrusage` here) and CPU seconds per wall second of the server process.

**Acceptance:**
- `load_probe.py --stations 500 --rooms 150 --viewers 500 --minutes 30` on this Mac: the server's peak RSS stays under 256 MB and its CPU under 0.5 of one core averaged; the numbers go in the report. The run on a 1 GB droplet beside Caddy is Task 21's, pending until then.
- `test_deploy_files.py`: the unit has every key above; the Caddy file routes the two WebSocket paths to 8720 and the rest to 8721; `caddy validate` runs on the website Caddyfile with the import line and the site file when `caddy` is installed (skipped with a reason otherwise); `setup-server.sh --dry-run` makes no change and prints its steps.
- `python3 docs/manual/build_preview.py --website` writes `website/public/roundtable/api.html` and the fixtures; a test compares the published fixtures with `roundtable/conformance/v1/` byte for byte; `check_manual.py` and `test_build_preview.py` pass.
- The website's own CSP is unchanged.

**Verification:** functional wiring (a service definition, an install step): integration evidence by `caddy validate` and a dry run; the load probe is measured evidence. The load probe takes a build lane (`nice -n 20`). The live deploy is not this task.

**Execution note (advisory):** opus. Config apply on a host: the scripts are written and dry-run only; nothing runs on a server. Prerequisite Task 8. Lane S ends here.

- [ ] **Step 1:** Load probe; run it; tune only if it fails, and report what was changed.
- [ ] **Step 2:** Deploy files and their tests; website Caddyfile import and setup change.
- [ ] **Step 3:** API page and published fixtures; README; commit.

## Task 10: The Core's RoundtableClient

**Requirements:** R-RT-6 (Core side), R-RT-12, R-RT-13, R-RT-14 (Core side, D25), the Core runner of R-RT-1; spec 7.1, 7.2, 8.3, 10.1, 10.2, C3, C22/D25, C24, C25.

**Files:**
- Create: `src/core/roundtable/RoundtableWire.{h,cpp}`, `RoundtableSignalFloor.{h,cpp}`, `RoundtableClient.{h,cpp}`
- Modify: `src/core/SpotSourceHost.{h,cpp}` (source `kRoundtable = "roundtable"` appended to `stationSources()`, `roundtableState` / `roundtableText` properties appended last, `setRoundtableClient`, `connectSource`/`disconnectSource`/`restoreAutoStart` branches gated by `Roundtable/AutoStart`, default `"True"`), `src/models/RadioModel.{h,cpp}` (own and wire the client), `CMakeLists.txt` (new sources)
- Create: `tests/tst_roundtable_wire.cpp`, `tests/tst_roundtable_signal_floor.cpp`, `tests/tst_roundtable_client.cpp`; modify `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: Task 1's wire spec and fixtures (the Core test reads `roundtable/conformance/v1/` through a `NEREUS_ROUNDTABLE_CONFORMANCE_DIR` compile definition).
- Produces (namespace `NereusSDR`):
  ```cpp
  struct RoundtableSliceState { int id; qint64 hz; QString mode; int filterHz; bool keyed; };

  class RoundtableSignalFloor {
  public:
      static constexpr int kWindowMs = 60000;
      static constexpr int kWarmupMs = 10000;
      static constexpr double kPresentAboveFloorDb = 10.0;
      static constexpr double kFloorPercentile = 0.10;
      void retune(qint64 nowMs);                 // starts the warm-up, clears the window
      bool update(double dbm, qint64 nowMs);     // returns signal present
  };

  class RoundtableClient : public QObject {
      Q_OBJECT
  public:
      enum class LinkState { Off, Connecting, Live, Reconnecting, Offline, Hidden };
      Q_ENUM(LinkState)
      explicit RoundtableClient(QObject* parent = nullptr);
      void setServerUrl(const QUrl& url);          // default wss://roundtable.nereussdr.com/v1/station
      void setIdentity(StationIdentity* identity); // not owned
      void setStationInfo(const QString& callsign, const QString& grid);
      void setSlices(const QList<RoundtableSliceState>& slices); // active slice first
      void pushMeter(int sliceId, double dbm, qint64 meterReadingNs);
      void setAtRadio(bool atRadio);
      void setHidden(bool hidden);
      void setFirstRunAnswered(bool answered);
      void start();
      void stop();
      LinkState linkState() const;
      // Station actions (Task 11 routes devices' verbs here)
      void post(const QString& roomId, const QString& text, const QString& clientId);
      void deleteMessage(const QString& messageId);
      void nameTurn(const QString& turnId, const QString& name);
      void report(const QString& kind, const QString& target, const QString& reason);
      void readRoom(const QString& roomId);
      void unreadRoom(const QString& roomId);
      void markMentionsSeen(qint64 upToMs);
      void requestWebLink();
      void deleteArchive();
  signals:
      void linkStateChanged(NereusSDR::RoundtableClient::LinkState state);
      void roomsReceived(const QJsonObject& full);
      void roomsDeltaReceived(const QJsonObject& delta);
      void roomReceived(const QJsonObject& full);
      void roomDeltaReceived(const QJsonObject& delta);
      void postResult(const QString& clientId, bool ok, const QString& messageIdOrReason);
      void mentionReceived(const QJsonObject& message);
      void webLinkReady(const QUrl& url);
      void noticeReceived(const QString& text);
      void errorReceived(const QString& code, const QString& reason);
  };
  ```
- Behavior that later tasks rely on:
  - Before `setFirstRunAnswered(true)`, or while hidden, the client connects only to `/v1/live` (read-only) and sends nothing about the station (D25, 10.2). Going hidden from live sends `hidden` last, closes, and reconnects to `/v1/live`. Archive actions while hidden or unanswered open a brief station connection that sends `hello` then `weblink` or `deleteArchive` only.
  - Readings: once a second per member slice that is not keyed, only when `meterReadingNs` advanced since the last one sent; the wire time is the Core's wall clock in Unix ms at `pushMeter` time; `present` from that slice's `RoundtableSignalFloor`; `retune` on a frequency or mode change.
  - Presence on change, coalesced to at most one a second, and every 10 s.
  - Backoff 1, 2, 5, 10, 30 s, then every 30 s; heartbeat reconnect after 45 s silent.
  - Mentions: also matches its base callsign in messages of rooms it receives, so hidden stations get @N.
- `RadioModel` wiring: reads callsign from `Roundtable/Callsign`, then `User/Callsign`, then `StationCallsign`; grid from `Roundtable/GridSquare`, then `User/GridSquare`; no callsign means no start, with the source text saying why. Identity: on a Core, the `StationIdentity` that `StationServer` loaded; on an Everything desktop with Remote Access off, `StationIdentity::loadOrCreate` on the settings profile directory (the one `GuiDesktopStationRuntime.cpp:89` passes), never `defaultDirectory()`. Keyed slice from `RadioModel::txBoundSlice()` and `MoxController::moxStateChanged`. At radio: true on an Everything desktop; on a Core, true while any authenticated device session is `listening` or `transmitting`; `Roundtable/AwayChoice` `"Report"` (default) or `"Stop"`.

**Acceptance:**
- `tst_roundtable_wire`: every control fixture from Task 1 encodes or decodes as the fixture says; the crypto fixture's transcript, signed with its test key, verifies.
- `tst_roundtable_signal_floor`: a steady -120 dBm floor then -105 dBm is present after the 10 s warm-up and not during it; a carrier held at -100 dBm for 5 minutes stays present (the floor holds while present); retune clears and restarts the warm-up.
- `tst_roundtable_client` against a `QWebSocketServer` fake in the test: sign-in transcript and host; no `presence` or `reading` before the first-run answer (only `/v1/live` is opened); after the answer, presence then readings once a second; no reading while keyed; no reading when the stamp did not advance; `hidden` is the last frame before close; a hidden or unanswered client asked for the archive opens a station connection that sends only `hello` and `weblink`; backoff timings follow 1, 2, 5, 10, 30 s with a fake clock; at-radio false with `AwayChoice` `"Stop"` sends nothing.
- `SpotSourceHost`: the new properties come last; `stationSources()` ends with `roundtable`; `Roundtable/AutoStart` `"False"` keeps it off at start.
- On an Everything desktop with Remote Access off, the station id equals the one Remote Access later uses (test: load both paths on one temp profile directory and compare SPKIs).

**Verification:** networking and identity: tests first against the fixtures and the fake server. Build and run `tst_roundtable_wire`, `tst_roundtable_signal_floor`, `tst_roundtable_client` and the existing `tst_spot_source_host*` tests (find their names with `grep -n spot_source tests/CMakeLists.txt`) one at a time under a build lane.

**Execution note (advisory):** opus. Networking and the station key: yes. Prerequisite Task 1 only; can run beside Tasks 2 to 9. Lane C.

- [ ] **Step 1:** `RoundtableWire` and its fixture test.
- [ ] **Step 2:** `RoundtableSignalFloor` and its test.
- [ ] **Step 3:** `RoundtableClient` against the fake server.
- [ ] **Step 4:** `SpotSourceHost` and `RadioModel` wiring; commit.

## Task 11: Station link surface and RoundtableModel

**Requirements:** R-RT-15, R-RT-14 (one answer per station, kept on the Core); spec 10.1 (settings kept on the Core), 10.3, U5, U6 (read on one device is read on all).

**Files:**
- Create: `src/core/roundtable/RoundtableState.{h,cpp}`, `src/models/RoundtableModel.{h,cpp}`
- Modify: `src/core/session/StationCapabilities.{h,cpp}` (`int stationRoundtableVersion {0}`, sent only when > 0), `StationServer.{h,cpp}` (`stationRoundtableVersion()` returning 1; the `roundtable` object watched and gated per peer; streams; verb handling), `SessionCommandDispatcher.{h,cpp}` (verb rows and `handleRoundtable`), `MirrorSchema.cpp`, `MirrorPolicy.cpp`, `MirrorEnumDomain.cpp` (`RoundtableClient::LinkState`), `StationCatalog.{h,cpp}`, `StationClient.{h,cpp}`, `IStationLink.h`, `src/gui/GuiSessionCoordinator.cpp`, `src/core/settings/SettingsScope.cpp`, `src/models/RadioModel.{h,cpp}`
- Modify: `docs/architecture/2026-09-23-station-link-v1.md` (capability, object, streams, record kinds, verbs, settings keys), `tests/LinkSurface.cpp`, `tests/data/link/v1/manifest.json`, `surface.json`, existing `sessions/*.json` capability ordinals
- Create: `tests/data/link/v1/sessions/verbs-roundtable.json`, `tests/tst_station_roundtable_link.cpp`, `tests/tst_roundtable_model.cpp`

**Interfaces:**
- Consumes: Task 10's `RoundtableClient` and its signals.
- Produces:
  - Capability `stationRoundtableVersion` 1, appended after the existing capabilities.
  - Mirrored object `roundtable` (class `RoundtableState`): `linkState` (enum), `linkSince` (Unix ms), `firstRunAnswered` (bool), `hidden` (bool), `mentionCounts` (JSON object room id to count), `sliceRooms` (JSON object slice id to room id).
  - Record streams: `roundtableRooms` (capacity 500, record kind `room`); `roundtableRoom:<room id>` (capacity 400, kinds `member`, `reading`, `turn`, `name`, `message`). The Core emplaces a room's stream when one of its slices joins the room and when `roundtable.read` is called, before it replies; it removes a stream when no slice is in the room and no device has read it for 10 minutes.
  - Verbs (all `"stationRoundtableVersion", 1`): `roundtable.post {roomId, text, clientId}`, `roundtable.delete {messageId}`, `roundtable.name {turnId, name}`, `roundtable.report {kind, target, reason}`, `roundtable.read {roomId}` (reply carries the stream name), `roundtable.unread {roomId}`, `roundtable.setHidden {hidden}`, `roundtable.answerFirstRun {live}`, `roundtable.mute {callsign}`, `roundtable.unmute {callsign}`, `roundtable.markMentionRead {upToMs}`, `roundtable.webSignIn {}` (reply carries the link), `roundtable.deleteArchive {confirm}`.
  - `SettingsScope.cpp`: `{"Roundtable", Station}` in `kPrefixes`; `Roundtable/EarlierTurnsShown` and `Roundtable/PanelOpensOn` as OperatorLocal in `kExceptions`. Keys on the Core: `Roundtable/AutoStart`, `AwayChoice`, `HighlightMentions` (default `"True"`), `Callsign`, `GridSquare`, `FirstRunAnswer` (`""`, `"Live"`, `"Hidden"`), `Hidden`, `MuteList` (comma-separated base callsigns), `MentionsSeenMs`.
  - `RoundtableModel` (`src/models/`, a `QObject` the screens bind to, the same in every placement): `rooms()`, `room(roomId)`, `sliceRoomId(sliceId)`, `readingRoomId()`, `linkState()`, `linkSince()`, `firstRunAnswered()`, `hidden()`, `mentionCount(roomId)`, `muted(callsign)`, `draft(roomId)` / `setDraft(roomId, text)` (per window, not persisted), and actions mirroring the verbs; signals `roomsChanged()`, `roomChanged(QString roomId)`, `linkChanged()`, `firstRunChanged()`, `mentionsChanged()`, `postResult(...)`, `webLinkReady(QUrl)`. In an Everything placement `RadioModel` feeds it from `RoundtableClient` directly; in a window placement `GuiSessionCoordinator` feeds it from the mirrored object, the streams and the verbs, following how `FreeDVStationModel` is fed in each placement (`RadioModel.cpp:4579`, `SpotSourceHost::setFreedvStationList`).

**Acceptance:**
- `tst_link_surface_manifest` passes with the regenerated `surface.json`; the diff adds only the Roundtable entries and the capability ordinal moves.
- `tst_link_conformance_control` and `tst_link_conformance_session` pass, including `verbs-roundtable.json` (each verb's accepted and refused shapes; `roundtable.read` on an unknown room refused with a plain reason).
- A peer without the capability sees no `roundtable` object, no stream and no verb (existing older-peer fixtures unchanged byte for byte).
- `tst_station_roundtable_link`: a window answers first run; a second window sees `firstRunAnswered` true and the Core's settings show `FirstRunAnswer` `"Live"`; a mention read on one device clears on the other; `roundtable.read` emplaces the stream before the reply, and subscribing after it gets the room's records; `Roundtable/Callsign` written by a window is accepted, `Roundtable/PanelOpensOn` stays local.
- `tst_roundtable_model`: the same scripted rooms, fed locally and fed through a fake link, give equal `rooms()` and `room()` contents.

**Verification:** an interface other devices and versions rely on: fixtures and tests with the change, plus the older-peer fixtures as regression coverage. Build and run `tst_link_surface_manifest`, `tst_link_conformance_control`, `tst_link_conformance_session`, `tst_mirror_schema`, `tst_gui_session_coordinator`, `tst_station_roundtable_link`, `tst_roundtable_model` one at a time under a build lane; regenerate `surface.json` with `tst_link_surface_manifest_regen` and `NEREUS_LINK_REGEN_OUT`.

**Execution note (advisory):** opus. Networking: the station link, which every device depends on; worth flagging for an earlier look. Prerequisite Task 10. Lane C. Task 16 depends on it.

- [ ] **Step 1:** `RoundtableState`, capability, mirror schema and policy entries, settings scope.
- [ ] **Step 2:** Streams and verbs on the Core; the link spec text.
- [ ] **Step 3:** Client side and `RoundtableModel` in both placements.
- [ ] **Step 4:** Fixtures, `surface.json`, tests; commit.

## Task 12: Desktop Spot Hub tab and first run

**Requirements:** R-RT-14 (desktop sheet), R-RT-16 (Spot Hub tab, first run); spec 11.2 (Spot Hub tab, First run), C15, D25, U5.

**Files:**
- Create: `src/gui/roundtable/RoundtableFirstRunDialog.{h,cpp}`, `tests/tst_roundtable_spothub.cpp`
- Modify: `src/gui/SpotHubDialog.{h,cpp}` (`buildRoundtableTab` after `buildPskTab`, a `RoundtableModel*` constructor argument), `src/gui/MainWindow.cpp` (pass the model; show the first-run dialog the first time Roundtable is opened while `firstRunAnswered` is false), `tests/visual/PreviewScenes.cpp` (scenes `roundtable-spothub`, `roundtable-firstrun`, `roundtable-firstrun-hidden`)

**Interfaces:**
- Consumes: Task 11's `RoundtableModel` and settings keys.
- Produces: `RoundtableFirstRunDialog(RoundtableModel*, QWidget* parent)` with the text of spec 11.2 First run, the Hide my station switch moving the main button between Go live and Stay hidden, calling `answerFirstRun(bool live)`; a "one-time note" mode when another device answered.

**Acceptance:**
- The tab matches `docs/architecture/reporter-brainstorm/mockups/settings.html`: server group (Auto-Start, Start, connection state), Hide my station with the preview, the away choice, highlight messages that name me, callsign and grid overrides, the mute list, Archive, Delete my archive behind a confirm saying it can't be undone and that backups age out within 7 days; the per-computer group (earlier turns shown, panel opens on).
- Each Core-side control writes its `Roundtable/*` key through the model and shows the Core's value when another device changes it.
- Archive calls `webSignIn` and opens the returned link in the browser; Delete my archive calls `deleteArchive` only after the confirm.
- The first-run dialog's text is exactly the spec's, including "Others can still hear you, and may name you when you talk."; Go live sets the answer; nothing about the station is sent before it (checked through Task 10's fake server in this test).
- Captures of the three scenes at 1x and at the Windows pixel density, compared with `settings.html` and the board's first-run section; 11 px and 4.5:1 hold.

**Verification:** UI plus controls that change configuration: `ui-verification` for the captures, functional checks for each control. Build `tst_roundtable_spothub` and `nereus_visual_preview`; run the test and `capture.py --scenes 'roundtable-*' --jobs 4` under a build lane.

**Execution note (advisory):** opus. No device risk. Prerequisite Task 11. Lane C.

- [ ] **Step 1:** The tab and its functional test.
- [ ] **Step 2:** The first-run dialog and the open-time trigger.
- [ ] **Step 3:** Scenes and captures; commit.

## Task 13: Desktop panel, applet and pop-out

**Requirements:** R-RT-16 (panel, pop-out tiers and memory, keyboard), R-RT-18 (desktop); spec 11.1, 11.2 (Panel, Pop-out, Keyboard), D13, U1, U3, U4, U6, U9, U11 to U14.

**Files:**
- Create: `src/gui/roundtable/RoundtablePanel.{h,cpp}`, `src/gui/applets/RoundtableApplet.{h,cpp}`, `src/gui/roundtable/RoundtableWindow.{h,cpp}`, `tests/tst_roundtable_panel.cpp`
- Modify: `src/gui/applets/AppletWidget.h` and `AppletPanelWidget.cpp` (a hook so an applet whose `canFloat()` is true may supply its own window: `virtual QWidget* createFloatingWindow()` defaulting to nullptr, used by `floatApplet` before `AppletFloatingWindow`), `src/gui/MainWindow.cpp` (`addApplet`, id map, `registerApplet("Roundtable","Roundtable",true)`), `src/gui/ContainerContentRegistry.cpp`, `tests/visual/PreviewScenes.cpp` (scenes `roundtable-panel-*`, `roundtable-window-{narrow,medium,wide}`, `roundtable-keyed`)

**Interfaces:**
- Consumes: Task 11's `RoundtableModel`; Task 12's first-run dialog.
- Produces: `RoundtablePanel(RoundtableModel*, QWidget*)` with `followSlice(int sliceId)`, `readRoom(QString)`, `backToSlice()`; `RoundtableApplet` (follows the active slice); `RoundtableWindow` (under 600 px one column; 600 to 980 room list plus room; wider three columns 250 px, room, 320 px; Wide preset 1080 x 600; remembers size and screen under `Roundtable/WindowGeometry`, an OperatorLocal key added to `SettingsScope.cpp` `kExceptions`; closing docks the panel back).

**Acceptance:**
- Order in the panel: room header and connection line, talker, readings (Station, km, Offset, Reading, Age; nearest first; AWAY marked), earlier turns of the last 30 minutes with Name on unknown turns after they end, messages, box; one scroll with the box pinned; arrivals while scrolled up show the "N new" pill, a mention pill first; at the newest the view follows; a draft survives arrivals and room changes.
- Reading another room shows "Reading 7.249 · Tune A here · Back to my slice"; Tune A here tunes; tuning anywhere returns to following.
- Keyed: the TX line "You · 9 hear you" and readings of you take the room; earlier turns and messages wait below.
- Stale: past 20 s grey with amber age; the connection line's four states and "You are hidden"; Send off while not live, draft kept.
- Messages: counter from 400, "Not sent · Try again", Delete on own (shows "Deleted by sender"), Mute, Report, Copy on others'; muted messages fold.
- Keyboard: docked (Tab in; up and down; Enter on an unknown turn opens Name; Escape back to the slice, else out) and popped out (/, arrows, Enter, Escape steps back) as spec 11.2.
- Pop-out tiers at 560, 800 and 1200 px wide; the window reopens at its saved size and screen; closing it docks.
- Captures of every scene at 1x and the Windows pixel density, compared with `placement.html`; 11 px and 4.5:1 hold except disabled controls.

**Verification:** UI and interaction: `ui-verification`; offscreen tests drive the keyboard paths and the pop-out (geometry, not clicks, per the macOS native view tree note). Build `tst_roundtable_panel` and `nereus_visual_preview`; run under a build lane.

**Execution note (advisory):** opus. No device risk. Prerequisite Task 12. Lane C.

- [ ] **Step 1:** `RoundtablePanel` with its functional tests.
- [ ] **Step 2:** The applet and its registration.
- [ ] **Step 3:** The pop-out hook and `RoundtableWindow`.
- [ ] **Step 4:** Scenes and captures; commit.

## Task 14: Desktop pan tags and flag line

**Requirements:** R-RT-16 (tags, folds, flag line); spec 11.2 (Panadapter tags, VFO flag line), U1, U4 (offline greys the tags, connecting hides them), U7, U8, U10.

**Files:**
- Create: `src/gui/roundtable/RoundtableTagLayout.{h,cpp}`, `tests/tst_roundtable_tag_layout.cpp`, `tests/tst_roundtable_flag_line.cpp`
- Modify: `src/gui/SpectrumWidget.{h,cpp}` (draw tags in both `paintEvent` and `renderGpuFrame`, hit-testing for clicks and the +N list), `src/gui/widgets/VfoWidget.{h,cpp}` (a Roundtable line built like `buildSnrRow`), `src/gui/MainWindow.cpp` (wire model to every pan and flag), `tests/visual/PreviewScenes.cpp` (scenes `roundtable-tags`, `roundtable-tags-crowded`, `roundtable-tags-offline`, `roundtable-flag-*`)

**Interfaces:**
- Consumes: Task 11's `RoundtableModel`; Task 13's panel (a flag click opens it).
- Produces:
  ```cpp
  struct RoundtableTag { QString roomId; qint64 hz; int state; QString label; int members; bool mine; bool mentionsMe; };
  struct RoundtableTagPlacement { QRectF rect; QStringList roomIds; bool folded; };
  QList<RoundtableTagPlacement> layoutRoundtableTags(const QList<RoundtableTag>& tags,
                                                     double hzPerPixel, qint64 leftHz,
                                                     double width, double tagWidth,
                                                     double gap);
  ```
  A fold holds two or more; a lone neighbor moves aside; folds widen only if tags still don't fit; `mine` and `mentionsMe` never fold; zooming in (smaller `hzPerPixel`) opens folds.

**Acceptance:**
- Layout tests: five tags 50 Hz apart at 100 Hz per pixel fold into one +5; at 1 Hz per pixel none fold; a `mine` tag inside the cluster stays separate and the rest fold; two overlapping tags with room to slide move apart instead of folding.
- A click on a tag tunes the active slice to its room; a click on +N lists its rooms by frequency and a row tunes.
- Offline greys the tags as last known; connecting hides them; tags sit along the spectrum foot clear of the VFO flags and the band plan strip.
- Flag line: "Roundtable · 9 · K4ABC" with the state glyph, @N in cyan, "9 hear you" while keyed, the link state when not live; a click opens the panel.
- Both paint paths draw the same tags (capture the CPU and GPU scenes and compare).
- Captures at 1x and the Windows pixel density compared with `placement.html`.

**Verification:** UI with a pure layout core: unit tests for the layout, captures for the drawing. Build and run `tst_roundtable_tag_layout`, `tst_roundtable_flag_line`, then `capture.py --scenes 'roundtable-*' --jobs 4` under a build lane.

**Execution note (advisory):** opus. Touches `SpectrumWidget` paint paths (the display, not the RX path). Prerequisite Task 13. Lane C.

- [ ] **Step 1:** `layoutRoundtableTags` and its tests.
- [ ] **Step 2:** Tags in both paint paths with hit-testing.
- [ ] **Step 3:** The flag line; scenes and captures; commit.

## Task 15: Desktop integration pass

**Requirements:** R-RT-16 and R-RT-18 on the whole desktop; spec 11.1 rules across tags, flag, panel, window and Spot Hub together.

**Files:**
- Modify: whatever Tasks 12 to 14 left inconsistent (named in the report)
- Create: `tests/tst_roundtable_desktop_flow.cpp`

**Interfaces:**
- Consumes: Tasks 10 to 14.
- Produces: nothing new.

**Acceptance:**
- One scripted flow in an Everything placement against Task 10's fake server: first run (nothing sent), Go live, tune into a room by tag, see readings, post, receive a mention (flag @1, pill, clears after 2 s on screen, cleared on a second window), read another room from the pop-out list, Back to my slice, key (TX line), unkey, name the unknown turn, go hidden (NOT SENT, posting off, "You are hidden"), open Archive (one-time link requested).
- The same flow in a window placement through the station link gives the same screens.
- A capture set of every Roundtable scene, at 1x and the Windows pixel density, handed to JJ in the report with a list of what to look at.

**Verification:** UI gate for the Roundtable scenes; the flow test is functional evidence. Build and run `tst_roundtable_desktop_flow` and the Roundtable scenes under a build lane.

**Execution note (advisory):** opus. Prerequisite Task 14. Lane C ends here.

- [ ] **Step 1:** The flow test in both placements.
- [ ] **Step 2:** Fix what it finds; the capture set; commit.

## Task 16: Phone link model

**Requirements:** R-RT-15 (phone side), R-RT-17 (data); spec 10.3, 11.3 (data the screens need).

**Files:**
- Create: `ios/NereusApp/Roundtable/RoundtableModel.swift`, `ios/NereusKit/Sources/NereusKitTesting/FakeStation+Roundtable.swift`, `ios/NereusApp/Tests/RoundtableModelTests.swift`
- Modify: `ios/NereusKit/Sources/NereusLink/LinkFeatures.swift` (if the app declares a hello feature for it), `ios/NereusKit/Tests/NereusLinkTests/LinkConformanceControlTests.swift`, `LinkSurfaceOrdinalTests.swift`, `LinkConformanceSessionTests.swift`, `MirrorConformanceTests.swift` (new capability and ordinals)

**Interfaces:**
- Consumes: Task 11's surface (capability, `roundtable` object, streams, verbs, settings keys).
- Produces: `@MainActor final class RoundtableModel: ObservableObject` with `init(mirror:commands:records:clock:)`, published `rooms`, `sliceRoomId`, `readingRoomId`, `room`, `linkState`, `linkSince`, `firstRunAnswered`, `hidden`, `mentionCounts`, `drafts`; methods `post`, `delete`, `name`, `report`, `read`, `unread`, `backToSlice`, `setHidden`, `answerFirstRun`, `mute`, `unmute`, `markMentionRead`, `webSignIn() async -> URL?`, `deleteArchive`; gated on `stationRoundtableVersion` like `FreeDVReporterModel` (:197-209).

**Acceptance:**
- NereusKit conformance tests pass with the new capability and ordinals (the same fixture files Task 11 changed).
- Against `FakeStation+Roundtable`: rooms appear from `roundtableRooms`; reading another room calls `roundtable.read` then wants the returned stream; posting returns the result; a mention marked read on the phone writes `markMentionRead`; with the capability absent, the model is unavailable and nothing is wanted.

**Verification:** an interface: tests with the change. `ios/scripts/swift-test.sh --jobs 2 --filter LinkConformance` and the app test for `RoundtableModelTests` under a build lane.

**Execution note (advisory):** opus. Prerequisite Task 11. Lane I.

- [ ] **Step 1:** Conformance pins.
- [ ] **Step 2:** The fake station surface and the model with tests; commit.

## Task 17: iPhone flag line, sheet, PTT and first run

**Requirements:** R-RT-17 (flag line, sheet, PTT hold, keyed layout, dial strip, first run), R-RT-18 (phone); spec 11.1, 11.3, D17, U2, U3, U4, U5, U6, U11, U12, U14.

**Files:**
- Create: `ios/NereusApp/Roundtable/{RoundtableFlagLine,RoundtableSheet,RoundtableRoomView,RoundtableFirstRunSheet}.swift`, `ios/NereusApp/Tests/RoundtableSheetTests.swift`, `ios/NereusApp/UITests/RoundtableUITests.swift`
- Modify: `ios/NereusApp/Band/VfoFlagView.swift` (the Roundtable line), `ios/NereusApp/Main/PttButton.swift` (press-and-hold mode with the dashed ring and "Hold" while the sheet is open)

**Interfaces:**
- Consumes: Task 16's `RoundtableModel`.
- Produces: the sheet (half height: talker and readings with Offset; tap the handle for full: readings, earlier turns with Name, messages, box in one scroll), the first-run sheet, the flag line.

**Acceptance:**
- Matches `docs/architecture/reporter-brainstorm/mockups/board.html` sections band, transmitting, first run, freshness, mentions, new messages and naming, on the app's workspace look.
- PTT with the sheet open: smaller, beside the box, 24 pt clear, press and hold; with the sheet closed, tap to toggle as today (existing PTT tests unchanged).
- Keyed: gauges stay; the sheet drops below them with the TX line and readings of you; with a dial on, the sheet stops above a 92 pt strip with PTT left and the dial right (knob 76 pt), and the box steps aside keeping the draft.
- Mentions @N on the flag line; the pill jumps; clears after 2 s on screen; "N new" pill while scrolled up.
- First-run sheet text exactly as spec 11.2; nothing sent before the answer (fake station records no presence verb).
- No text under 11 pt; 4.5:1 except disabled controls.
- Simulator screenshots of each state compared with `board-shots/` and listed in the report.

**Verification:** UI: `ui-verification`; UI tests for PTT hold, the sheet heights and the first-run answer; screenshots for the look. App tests with `-only-testing:` for the new tests and the existing PTT tests, under a build lane.

**Execution note (advisory):** opus. PTT keys the radio: the hold mode must never key on a swipe or a sheet drag; flag for JJ's smoke test on his phone. Prerequisite Task 16. Lane I.

- [ ] **Step 1:** Flag line and sheet with the room view.
- [ ] **Step 2:** PTT hold mode and the keyed layouts.
- [ ] **Step 3:** First-run sheet; tests and screenshots; commit.

## Task 18: iPhone Tools page, pan tags and swipes

**Requirements:** R-RT-17 (Tools page, swipes and Setup rows, tags); spec 11.3 (Tags, Tools, Swipes), S1, U1, U10.

**Files:**
- Create: `ios/NereusApp/Roundtable/{RoundtableToolsPage,RoundtableTagLayer,SwipeShortcuts}.swift`, `ios/NereusApp/Tests/{RoundtableToolsPageTests,SwipeShortcutsTests}.swift`
- Modify: `ios/NereusApp/Tools/StationToolList.swift` (a Roundtable entry after FreeDV Reporter), `ios/NereusApp/App/ToolsTab.swift` (routing), `ios/NereusApp/Setup/NavigationPage.swift` and `ios/NereusApp/App/PhoneSettings.swift` (swipe mappings beside the tuning dial, Reset to defaults), the panadapter view (the tag layer and the edge zones)

**Interfaces:**
- Consumes: Task 16's model; Task 17's sheet.
- Produces: `enum SwipeTarget: String, CaseIterable { case off, rxPanel, txPanel, roundtableMine, roundtableAll, spotHub, tools }`; `PhoneSettings` keys `swipeLeftEdge` (default `.rxPanel`), `swipeRightEdge` (`.txPanel`), `swipeUpTabBar` (`.roundtableMine`); the tag layout ported from Task 14's rules (same cases as its tests).

**Acceptance:**
- Tools page: band chips with counts, one row per room (state, who, members, messages, @N), your slice's room marked; a row reads, never tunes.
- Tags on the phone panadapter tune on tap and fold as on the desktop (the same layout cases as Task 14).
- Swipes only on the Panadapter tab; 18 pt edge zones and the tab bar; a drag elsewhere moves the band; Off returns the edge to dragging; nothing while a sheet or panel is up; the TX panel by swipe never keys; pages slide from the swipe's side; a short drag springs back.
- Setup, General, Navigation rows with Reset to defaults lit only once a mapping changed; toolbar buttons stay RX left, TX right.
- Screenshots compared with `board-shots/3-swipes.png`, `4-tools.png`, `2d-crowded-band.png`.

**Verification:** UI and gestures: UI tests for the edge zones and the never-key rule; unit tests for the layout and settings. App tests with `-only-testing:` under a build lane.

**Execution note (advisory):** opus. The TX panel by swipe never keys: a regression test is required. Prerequisite Task 17. Lane I.

- [ ] **Step 1:** Tools page and routing.
- [ ] **Step 2:** Tag layer.
- [ ] **Step 3:** Swipes and Setup rows; tests and screenshots; commit.

## Task 19: iPad applet

**Requirements:** R-RT-17 (iPad applet, widen, flag-line scroll), R-RT-18 (iPad); spec 11.4, S2.

**Files:**
- Create: `ios/NereusApp/Roundtable/RoundtableApplet.swift`, `ios/NereusApp/Tests/RoundtableAppletTests.swift`
- Modify: `ios/NereusApp/iPad/AppletColumn.swift` (the applet second after the S-Meter, reorderable; widen state), `ios/NereusApp/Band/VfoFlagView.swift` (on iPad the line scrolls the column to the applet), `ios/NereusApp/App/PhoneSettings.swift` (iPad edge swipes default Off)

**Interfaces:**
- Consumes: Tasks 16 to 18.
- Produces: the applet with room header, connection line, talker, readings, earlier turns, messages and box; the column as one scroll with the box pinned at its foot while the applet is in view; ⇔ Widen and swipe up from the tab bar widen to room list plus room.

**Acceptance:**
- Matches `board.html`'s iPad section and `board-shots/6-ipad.png`, `6b`, `6c`, `6d`.
- With the column hidden, the iPad uses the phone's sheet; the first-run sheet is a centered form sheet.
- Edge swipes Off by default on iPad; the Setup choices turn them on, saved per device.
- iPad simulator screenshots compared and listed.

**Verification:** UI: `ui-verification`. App tests with `-only-testing:` on an iPad simulator destination (`platform=iOS Simulator,name=iPad Pro 13-inch (M5)` or the nearest installed, checked with `xcrun simctl list devices available`), under a build lane.

**Execution note (advisory):** opus. Prerequisite Task 18. Lane I ends here.

- [ ] **Step 1:** The applet in the column.
- [ ] **Step 2:** Widen, flag-line scroll, form sheet, swipe defaults; tests and screenshots; commit.

## Task 20: End to end

**Requirements:** R-RT-24, R-RT-26's software half; spec 6 (the whole path), D22 to D24.

**Files:**
- Create: `roundtable/tests/test_client.py` (a website-key client: presence, readings, keyed, post, name, report), `tests/tst_roundtable_e2e.cpp` (starts the real server with `/usr/bin/python3 -m nereus_roundtable` on loopback ports with a temp database, then two in-process `RoundtableClient`s), `roundtable/tests/test_e2e_key_client.py`

**Interfaces:**
- Consumes: Tasks 1 to 15 (and the phone model from Task 16 through the fake link, already covered there).
- Produces: nothing new.

**Acceptance:**
- Two Cores on one USB frequency: Core A keys; Core B's reading of A appears in A's room within 2 s; the turn is credited to A; A unkeys; an unknown turn opens from B's present readings and B names it; A's archive (through a web sign-in link) lists the session with B's track.
- The website-key client appears in the same room, reports, posts, names and reports exactly as a Core does, with the same limits and refusals.
- A Core with no device at the radio shows AWAY; its post is refused `away`.
- The test skips with a printed reason if `/usr/bin/python3` lacks `websockets` or `cryptography`; it is run here and its output is in the report.

**Verification:** integration evidence for the whole boundary. Build and run `tst_roundtable_e2e` under a build lane; `/usr/bin/python3 -m pytest roundtable/tests/test_e2e_key_client.py -q -p no:cacheprovider`.

**Execution note (advisory):** opus. Networking: loopback only. Prerequisites Tasks 9 and 15.

- [ ] **Step 1:** The key client.
- [ ] **Step 2:** The C++ end-to-end test; run both; commit.

## Task 21: Deploy and bench (JJ)

**Requirements:** R-RT-9 (droplet run), R-RT-26; spec 6.2, 16 step 8.

**Files:**
- Modify: `roundtable/README.md` (what was done, with the date)

**Interfaces:**
- Consumes: everything.
- Produces: a running Roundtable at `roundtable.nereussdr.com`.

**Acceptance:**
- Each step below is done only after JJ's yes in chat for that step, and its result is reported:
  1. JJ adds the `roundtable` A and AAAA records at Namecheap.
  2. `roundtable/deploy.sh --dry-run`, then the real copy to `nereus-web:/opt/nereus-roundtable/`.
  3. `website/deploy/setup-server.sh` with the import line (the website keeps serving; check `https://nereussdr.com` before and after).
  4. `roundtable/deploy/setup-server.sh --dry-run`, then the real run; JJ's station id goes into `helpers`.
  5. The load probe from JJ's Mac against the droplet at a quiet hour (500 stations, 150 rooms, 500 viewers, 10 minutes, not 30, to spare the website), watching `systemctl status` memory and `top` CPU; under 256 MB and half a vCPU.
  6. `pull-backup.sh` installed on JJ's machine; one pull verified.
- Bench (device, pending until JJ runs it): two Cores on JJ's bench on one frequency; JJ talks on one; the other's reading appears on the desktop, the phone and the public page within 2 s; AWAY shows with no device connected (R-RT-26).

**Verification:** device and live host: evidence is JJ's observation and the command output; nothing here is automated. Reports pending until each step is seen.

**Execution note (advisory):** the controller does this with JJ, not an implementer. Touches a live host and DNS: every step needs his yes.

- [ ] **Step 1:** DNS (JJ).
- [ ] **Step 2:** Deploy and setup, step by step with his yes.
- [ ] **Step 3:** Droplet load check; backup pull.
- [ ] **Step 4:** Bench test with JJ; record the result.

## Final checks

On the finished branch, once, before the whole-branch review hands over:
- `/usr/bin/python3 -m pytest roundtable/tests -q -p no:cacheprovider` and `... rendezvous/tests ...` (unchanged, still green).
- Every new C++ test and the link tests named in Task 11, one at a time under a build lane; then `ctest --test-dir build -L core` once.
- `ios/scripts/swift-test.sh --jobs 2` and the app tests for the Roundtable suites and the existing PTT suites.
- The Roundtable desktop scenes with `capture.py --verify` against the captures handed to JJ.
- The pre-commit checks on the whole branch (`scripts/verify-inline-tag-preservation.py`, `check-new-ports.py` full-tree, as the hook runs them).
- CI on the pushed branch: the new `roundtable` job and every required check.
