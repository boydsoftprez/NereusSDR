# Station selection implementation plan

> Execute with yonder-cost-aware-execution. Reuse the approved task 4g and
> identity design §6; use risk-based tests and one consolidated boundary review.

Spec: [selection design](2026-09-22-station-selection-design.md).
Requirements: R-R3-10/16/17/21/38. The deliverable includes local radios, saved
Cores and LAN Core discovery; the first saved-target unit is not completion.
Root owns integration, build/CMake, Git and hardware coordination.

## 1. Saved Core records and startup selection

Files: new `gui/CoreTargetStore.{h,cpp}`, `tests/tst_core_target_store.cpp`;
root integrates `SettingsScope.cpp`, CMake and later `main.cpp` startup resolver.
Terra implements the bounded store alongside root's lifecycle/discovery design.

`SavedCoreTarget` contains id, label, `RemoteStationOptions connection`,
lastRadioName and lastRadioMac. `CoreTargetStore(AppSettings&)` exposes
`load(error)`, `targets()`, `target(id)`, `selectedId()`, `upsert(record,error)`,
`remove(id,error)`, `select(id,error)` and `createId()`. Constructor never writes.
All mutations validate and persist before replacing in-memory state. `local`
is reserved; other IDs match `[A-Za-z0-9_-]{1,64}`. The JSON object has version1,
selectedId and cores with exact typed known fields. Unknown version, duplicate
IDs, missing required fields, bad selection and invalid URL reject the document.
Text limits: 1 MiB document, 128 records, 512-character labels, 4096 URL, 8192
 token, 256 fingerprint and 64 MAC; these are parser bounds, not hardware limits.

- [x] Implement round-trip store, one-time exact legacy migration and rollback
  on save failure, with no credentials in messages or station settings.
- [x] Add explicit OperatorLocal classification and register source/tests.
- [x] Resolve CLI, saved target and explicit Local as coherent choices; prove
  a new CLI station URL cannot inherit another station's credentials.

Verification: actual AppSettings temporary-file round trips, independently
malformed JSON, migration/no-resurrection, multiple trust tuples, failure
rollback and SettingsProxy scope checks. No radio or native device needed.

## 2. Own and replace complete GUI sessions

Files: new GUI coordinator, `main.cpp`, `MainWindow.*` and integration tests.
Dependency: target value contract above. Root owns this integrated boundary.

Frozen lifecycle seam: `GuiSessionCoordinator` owns one `unique_ptr<MainWindow>`
and one optional `unique_ptr<SettingsProxy>`. `replace(selection, startConnection,
error)` validates URL and TX-idle before retirement; `window()`, `selection()`
and `generation()` expose current state; `shutdown()` retires it. The coordinator
emits `windowChanged` and forwards `connectionsRequested` only for the current
generation. Callers queue UI-originated replacement after the source widget's
handler returns. Selection persistence remains the selector's responsibility.

`MainWindow::ConnectionStartup::{Automatic,Deferred}` is appended to its existing
constructor with Automatic as default. Deferred constructs the complete remote
session scaffolding but neither dials nor starts local discovery/auto-connect.
`startInitialConnection()` starts that existing behavior once.
`retireForSessionSwitch()` suppresses only the normal close-to-quit action and
runs the existing close/persistence/worker-retirement path. Both close and
retirement stop remote retries before model/renderer destruction. The coordinator
keeps the old backend installed until its window is gone, detaches it before
proxy deletion, and installs a new unready proxy before a remote model exists.
During replacement it suppresses QApplication last-window quit; ordinary Close
retains its explicit orderly quit. `setConnectionPickerManaged(true)` forwards
connection entry points to the coordinator without dialing as a side effect.

- [x] Define lifecycle API and generation ownership around the actual shutdown,
  auto-connect, proxy-before-model and QApplication quit boundaries.
- [x] Replace whole window/model/proxy on target switch, preserving local mode,
  saved window placement and normal application close/quit.
- [x] Guard stale picker, retry, transport and media callbacks; refuse a switch
  that would interrupt active TX until it is explicitly unkeyed.

Verification: real window/session objects under existing discovery/device test
seams, two loopback Cores and delayed callbacks. Prove teardown order, no old
slices/cache in the new Core, no new connection before retirement, and local
backend null. Exercise GPU renderer destruction through existing pan tests.

## 3. Unified connection presentation and actions

Files: selector model/dialog, coordinator integration, existing connection/menu
entry points and station setup page. Dependencies: store and switching API.

- [x] Populate actual local radio/saved Core rows and actionable state, reusing
  existing discovery and radio-edit behavior. Add/edit/forget saved Cores with
  current URL validation and pinned TLS/token authentication.
- [x] Route title, menu, Connect and disconnected-pan entry points to selection;
  retain connected-target details/diagnostics and explicit disconnect/cancel.
- [x] Use epoch-scoped authenticated Core/radio status; mark cached information
  and show Core-online/radio-offline separately. Preserve current connection
  when merely selecting, editing, cancelling or receiving invalid input.

Verification: widget clicks/keyboard selection, empty/populated/error states,
credential-free text, layout rendering and local-radio regressions. Operator
appearance/use checkpoint remains required after installation.

## 4. Core LAN announcement and discovery

Files: new core/session discovery codec/cache/UDP classes, daemon lifecycle
integration, selector discovery provider, focused tests. Exact wire/API and
socket constants are frozen below before coding this unit.

New Nereus protocol choices (not radio/gateware constants): UDP 47910,
IPv4 group `239.255.42.99`, transient link-local IPv6 group `ff12::4e52:5344`,
hop limit 1, announce every 5 seconds, expire after 15 seconds. These are private
service-discovery choices. A manual address remains usable without multicast.

The first codec/cache unit owns `core/session/StationLanAnnouncement.{h,cpp}`
and `StationLanCache.{h,cpp}`. Wire format is strict big-endian: four ASCII
bytes `NRSC`, u8 schema=1, u8 service=1 (WSS control), u16 nonzero actual control
port, 95 ASCII bytes canonical uppercase colon-separated SHA-256 fingerprint,
u8 Core-name byte count + valid UTF-8 (1..128 bytes), u8 radio-connected (0/1),
u8 radio-name byte count + valid UTF-8 (0..128; nonempty when connected), and
17 ASCII bytes canonical uppercase colon-separated radio MAC. All-zero MAC
means unknown and is permitted only while radio is unavailable. Connected must
have an actual MAC. Reject >512 bytes, trailing bytes, malformed types/lengths,
invalid UTF-8, embedded control characters, bad fingerprint/MAC/version/service.
Offline may retain a configured radio MAC/name but UI must mark them unavailable.

`StationLanAnnouncement` contains `quint16 controlPort`, `QString fingerprint`,
`coreName`, `radioName`, `radioMac`, `bool radioConnected`; free codec functions
`encodeStationLanAnnouncement(value,error)` returning QByteArray and
`decodeStationLanAnnouncement(bytes,error)` returning optional value. Codec
errors are static text and never include input. `StationLanEndpoint` contains
announcement, `QHostAddress address`, `uint interfaceIndex`, `qint64 lastSeenMs`;
`key()` identifies fingerprint+source/interface+port, `url()` forms a WSS URL
from numeric source+port only, retaining scoped IPv6. No record carries tokens.

`StationLanCache` exposes `ingest(bytes,source,interfaceIndex,nowMs,error)`,
`expire(nowMs)`, `endpoints()`, `clear()`. Both ingest and expire report whether
visible endpoint metadata/membership changed (refreshing lastSeen alone is not a
UI change). Bound 128 Core fingerprints and 8 endpoints per fingerprint, reject
new entries beyond the cap, accept refreshes and expire before admission. Reject
null/unspecified/multicast/broadcast source; loopback is a valid explicit bench
source. For IPv6 link-local, retain source scope or set it from nonzero receiving
interface index; reject if neither exists. Cache keys include source scope.
Monotonic time is supplied by the socket owner (QElapsedTimer), never the sender.
No cache operation accesses AppSettings or grants trust.

The transport/daemon unit will use the actual successful listener bind address
and port. Qt documents Any as dual stack, AnyIPv4 as IPv4-only, AnyIPv6 as
IPv6-only ([QHostAddress](https://doc.qt.io/qt-6/qhostaddress.html#SpecialAddress-enum));
therefore a second server is not intrinsically required. Do not broaden the
operator's configured bind. Enumerate eligible interfaces for that bind/family,
remain silent for loopback-only or unavailable listeners, and verify IPv4/IPv6
with separate native evidence. The socket-owner API and Core-name configuration are frozen below.

Frozen transport API: `StationLanDiscovery(QObject*)` owns separate IPv4 and
IPv6 QUdpSockets, a monotonic clock and cache. `start(quint16 port = 47910)`
is idempotent (0 requests a private ephemeral port for tests), `stop()` closes
both sockets, cancels queued drains, clears cache; `endpoints()`, `port()`,
`lastError()` expose observations, `changed()` signals membership/metadata or
availability changes. Start succeeds when at least one family binds. Bind with
ShareAddress/ReuseAddressHint, join eligible up/running multicast non-loopback
interfaces, and refresh membership after interface changes. Manual addresses
remain available if discovery cannot bind or join. Reject oversized datagrams
before allocation; bound each drain and guard deferred continuations by socket
lifetime/generation. Expire on a one-second timer. Errors are static and contain
no packet bytes. Discovery grants no trust and starts when Connections opens.

`StationLanAnnouncer(QObject*)` exposes `update(listenerAddress, announcement)`,
`stop()`, `isActive()` and `announceNow()`. An update with invalid announcement,
unspecified/unavailable port or loopback listener stops it. Valid updates publish
immediately and every five seconds; unchanged update does not reset/flood the
schedule. Each emission enumerates current eligible interface addresses,
filters by actual listener family/address and binds the sending socket to that
exact source before multicast. Wildcard binds may use one address per interface
and family; concrete binds only that address. IPv6 source/scope and multicast
interface index are retained; both groups use hop limit one. No persistent
sending socket/worker thread or pending send survives stop. Pure helper
`stationLanListenerServesAddress(listener, source)` covers wildcard-family,
concrete and loopback rejection. Production multicast reachability remains a
native test, distinct from the loopback unicast discovery regression.

Root integrates listener state notifications, real bound-address accessor,
`core_name` (optional, bounded UTF-8; hostname fallback), daemon announcement
lifecycle, and GUI trust matching. Workers own only new transport files/tests.

- [x] Freeze bounded versioned one-way dual-stack multicast announcement and
  cache contract; preserve source-interface/IPv6 scope, TTL and identity rules.
- [x] Announce only while the configured Core listener is available; retire on
  stop/reconfigure and update radio availability. Never publish credentials.
- [x] Listen/populate/expire LAN Core rows and match saved trust without replacing
  pins or silently reassigning radios. Keep manual addresses functional.

Verification: malformed/oversized packet and cache tests, injected time,
loopback UDP and listener/start-stop generations; separate actual IPv4/IPv6 LAN
multicast/interface/reachability evidence. No announcement is trust proof.

## 5. Combined acceptance and checkpoint

- [x] Consolidated review of session/settings/credential/renderer ownership and
  discovery trust; resolve concrete findings with focused regressions.
- [x] Matching NereusSDR/nereusd/all_tests build and unfiltered full suite with
  host load recorded; preserve test skips as coverage gaps, not passed behavior.
- [x] Signed local checkpoint c28e1565, matching build identity and clean working
  tree at that checkpoint. Matching Core/GUI installation and IPv4 announcement
  receipt passed; native UI/switching and operator smoke remain below.
- [ ] Authorized installation and operator smoke: choose local radio, Rock/Saturn
  pair and a second Core where available; switch/cancel/reconnect, observe identity,
  audio/display continuity after connection and preserved local capability.

Hardware unknowns remain explicit. This does not implement R4 TX, R5 traversal
or full identity pairing, but must preserve their interfaces and roadmap scope.
