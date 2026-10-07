# CAT setup from a connected desktop: design and plan

Date: 2026-10-07. Branch `codex/thetis-cat-design` (PR #340).

## Decision

JJ, 2026-10-06, answering how a headless Core's CAT is set up: "From the
client". In this PR, the CAT setup pages work from a connected desktop the
same way other Core settings do: channels, ports, slice binding and the log,
applied on the Core.

Today a remote window greys every CAT control with "CAT runs on the computer
running the Core. Set it up there." (`src/gui/setup/CatLiveSetupPages.cpp:35`),
and `CatService` does nothing outside the Local role (`CatService.cpp:129,191,595,646`).
On `nereusd` that leaves CAT with no setup surface at all.

## Precedent: the Core's own TCI server

The closest existing path is the Core TCI server group on the TCI Server page.
Every step below copies it.

| Step | TCI precedent |
| --- | --- |
| Page reads a mirrored model, sends typed commands | `CatNetworkSetupPages.cpp:283-307,463-561` |
| Client proxy | `IStationLink.h:443-454`; `StationClient.cpp:6623-6644`; hello feature `StationClient.cpp:892` |
| Wire | `SessionMessageKind::CommandInvoke` with typed `MirrorUpdate` arguments; answer is `CommandResult` (accepted, reason) |
| Core gate | `StationServer.cpp:5585-5617` (minor and capability); `runInvoke` `:8797-8901` |
| Verb table, routing, handler | `SessionCommandDispatcher.cpp:738-757,1387-1393,3344-3384` |
| Apply on the Core | `RadioModel::setStationTciSettingsForStation` (`RadioModel.h:3624`) to `StationTciController::setSettings` |
| Echo to every window | `StationServer.cpp:7471` `m_mirror->watch("stationTci", model)`; writes refused `:7749` |
| Schema and policy | `MirrorSchema.cpp:122`; Outbound in `MirrorPolicy.cpp` |
| Snapshot on connect | object.create after capabilities (`tests/data/link/v1/sessions/verbs-station-tci-settings.json`) |
| Client registers the key only when offered | `StationClient.cpp:3791`; values via `StationTciModel::applyStationValue` (`:4957`) |
| Capability field | `StationCapabilities.h:388`, encode `.cpp:461-463`, decode `:889-890`; per peer `StationServer.cpp:9640-9645,12991` |
| List data | the `tciClients` record stream (`StationServer.cpp:12616,13383`; `StationClient.cpp:2925`) |
| Tests | `tests/tst_station_tci_server.cpp` (`:289,359,432`), `tests/MultiDeviceHarness.h`, link fixtures and `tst_link_surface_manifest(_regen)` |

The generic settings mirror (`SettingsProxy`) is the wrong fit: `CatService`
checks across channels when it applies a channel (duplicate serial devices,
the PTT port) and captures slice incarnations, and the live state is not a
setting.

## Design

### On the Core

* **`StationCatModel`** (`src/models/StationCatModel.h/.cpp`, Core export, no
  GUI includes): the mirrored object `stationCat`, read-only to every window.
  Properties, each a UTF-8 JSON text so one change is one delta:
  * `global`: every `CatGlobalConfig` field, plus `aiActive` (the run-only AI
    state from `autoInformationActive()`) and `pttState`.
  * `channel1` .. `channel4`: every `CatEndpointConfig` field, the binding as
    slice ids with `primaryValid` / `secondaryValid` (the bound incarnation is
    still live on the Core), and live status: `state`, per-transport state
    text (TCP, serial, PTY, rigctld), bound address and port for TCP and
    rigctld, TCP and rigctld client counts, PTY path.
  * `platform`: what the Core's computer can do: `serial` (built with Qt serial
    port), `pty` (macOS or Linux), `markSpaceParity` (not macOS),
    `oneAndHalfStop` (Windows only), `serialDevices` (the Core computer's
    `QSerialPortInfo::availablePorts()` system locations, refreshed when the
    page asks, see `refreshStationCatDevices`).
  * `lastTest`: `{requestId, channel, command, reply, accepted}` of the most
    recent tester command, so the window that sent it can show the reply.
* **Controller**: on the Core (Local role, which is both `nereusd` and a
  desktop running its own radio), a small publisher fills `StationCatModel`
  from `CatService` signals (`configurationChanged`, `globalConfigurationChanged`,
  `channelStateChanged`, `transportStateChanged`, `clientCountChanged`,
  `rigctldClientCountChanged`, `ptyPathChanged`, `pttStateChanged`,
  `autoInformationChanged`) and on slice add or remove (binding validity).
* **Verbs** (CommandInvoke, answered by CommandResult):
  * `setStationCatChannel {channel: Int64, config: utf8 JSON}`: the Core
    resolves each bound slice id to its live incarnation at apply time, then
    calls `CatService::reconfigureChannel`. An id the channel already holds
    keeps its incarnation unless the config carries `primaryRebind` or
    `secondaryRebind` true (the operator picked that slice in the selector). Refusal reason: the page's text,
    "Configuration refused: check address, port, format and exclusive device
    assignment."
  * `setStationCatGlobal {config: utf8 JSON}`: `CatService::reconfigureGlobal`.
    Refusal: "Configuration refused: check PTT source, sampled inputs and
    device assignment."
  * `testStationCatCommand {requestId: Int64, channel: Int64, command: utf8}`:
    `CatService::testCommand`, the same refusals for keying and calibration as
    the local tester; the reply comes back as the result's `reply` value
    and lands in `lastTest` for other windows.
  * `refreshStationCatDevices {}`: re-reads the Core computer's serial ports
    into `platform`, at most once a second.
* **Log**: record stream `catLog`, capacity 10000 (the log window's own
  `kMaximumEntries`, `CatLogWindow.cpp:18`), entries `{channel, inbound,
  text, time}` from `CatService::messageLogged`. A window subscribes when its
  CAT Log window opens, with a backlog of 10000 so earlier lines show, and
  unsubscribes when it closes.
* **Gates**: capability `stationCatVersion` 1 and the hello feature
  `stationCat` 1; the Core sends the object and the stream only to a peer that
  declared it and refuses the verbs from one that did not, with the reason
  the TCI path uses for an older peer. Who may change it: any authenticated,
  paired session, as for the Core's TCI server. No on-air refusal: the local
  page applies changes while transmitting, and `CatService` already releases
  a reconfigured session's PTT; parity with the local window wins.
* **Versions**: follow how `stationTciSettings` was added; bump the session
  minor only if that path did, and cite it either way. Product, settings
  schema and wire versions stay independent.

### On the client

* **`CatControl`** (`src/core/cat/CatControl.h`, no GUI includes): what the
  pages, the applet, the log window and the status bar need, with one local
  and one remote implementation.
  * Reads: `channelConfig(n)`, `channelStatus(n)` (the live fields above),
    `globalConfig()`, `pttState()`, `platform()`, `available()` and
    `unavailableReason()` (link down, older Core).
  * Writes: `reconfigureChannel(n, config, callback(accepted, reason))`,
    `reconfigureGlobal(config, callback)`, `testCommand(channel, bytes,
    callback(reply))`, `refreshDevices()`.
  * Signals: `changed()`, `logged(channel, inbound, bytes)`.
  * `LocalCatControl` wraps `CatService` and answers synchronously, so the
    local window's behavior is unchanged.
  * `RemoteCatControl` reads `StationCatModel` and sends the verbs through
    `IStationLink`; the binding it sends is slice ids only.
  * `RadioModel::catControl()` returns the one for the role.
* **Pages, applet, log window, status bar**: read and write through
  `catControl()` in both roles. In a remote window:
  * The device list, PTY availability and serial choices come from
    `platform`, not from the window's own computer.
  * A slice binding shows "Invalid binding" from `primaryValid` /
    `secondaryValid`.
  * The localReason note and its greying are removed. A control is disabled
    only while `available()` is false, with `unavailableReason()`.
  * The status bar's "On the Core" CAT indicator shows the Core's counts.

## Tasks

1. **Core side.**
   * Work: `StationCatModel`, the publisher, the capability and hello feature,
     the four verbs and their handlers, the `catLog` stream, schema and policy,
     the link fixtures (regenerated with `tst_link_surface_manifest_regen`),
     and rows in `docs/architecture/2026-09-23-station-link-v1.md` and the
     capability table in `docs/architecture/2026-09-23-remote-accessory-control-v1.md`.
   * Tests: publish from `CatService` changes, each verb applied and refused,
     binding resolved to the live incarnation, an older peer gets neither the
     object nor the verbs, the stream delivers log lines, and a real Core over
     `MultiDeviceHarness` round-trips a channel change.
2. **Client and GUI.**
   * Work: `CatControl` with both implementations, `IStationLink` and
     `StationClient` additions, the mirror key registered when offered, and
     `RadioModel::catControl()`. Rewire `CatLiveSetupPages.cpp`,
     `CatApplet.cpp`, `CatLogWindow` and the status bar.
   * Tests: a `RadioModel(Role::Remote)` page drives the Core over the
     harness; the tester reply comes back; the log window fills from the
     stream; local behavior is unchanged (the existing `tst_cat_setup`,
     `tst_cat_applet` and the CAT suites pass). Also run
     `tst_operator_wording_sweep`, `tst_no_placeholder_marks`,
     `tst_station_reason_wording` and `tst_core_has_no_gui_includes`.
3. **Phone and conformance.**
   * Work: the regenerated link fixtures must keep the iOS NereusKit package
     tests passing (`swift test` in `ios/NereusKit`, offline). The phone keeps
     CAT Control greyed until it has a page.
