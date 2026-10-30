# Roundtable Frequency-First Implementation Plan

> Execute with Crew and cost-aware-development routing. One consolidated review after implementation, one correction wave. JJ approved the complete three-surface board and authorized implementation on 2026-10-30; no new design approval gate.

**Goal:** Implement the approved people-at-frequency Roundtable on desktop, iPhone and public website for the combined release.

**Architecture:** Keep existing RF room/turn/archive machinery internal. Add negotiated frequency views across RF rooms, exact public slice presence and immutable sender contexts. Share station preferences and preserve legacy clients.

**Tech Stack:** Python asyncio/SQLite, C++20/Qt6 station link and native desktop, SwiftUI/NereusKit, existing server static web app.

**Spec:** docs/architecture/2026-10-29-roundtable-frequency-first/{README.md,board-source.html,review.md}; retained docs/architecture/2026-10-23-roundtable-design.md applies except where the approved board supersedes room-based interaction.

## Global Constraints

- Read CLAUDE.md, CONTRIBUTING.md and docs/development/fast-test-loop.md. AppSettings only; original source attribution and dated modification histories; signed commits and normal hooks.
- No DSP/RX-path changes, radio keying, device/profile/settings changes or deployment as part of source implementation. Preserve hidden/unanswered live-reader privacy and all existing text/moderation/archive safeguards. No phone Archive control.
- Exact contexts are integer Hz and existing mode family. Chat eligibility is same family and absolute offset <=5000 Hz, pairwise across RF rooms. No transitive grouping.
- Negotiation: optional frequencyVersion:1 through known hello/welcome and live watchBand/rooms envelopes. No new kind before negotiation. Legacy v1 kinds/fixtures/signing transcript stay valid. Unsupported servers are identified honestly.
- New server postAt requires slice,hz,family,text,clientId. Validate against current accepted exact presence; stale context refuses and preserves draft. Legacy post unchanged but store exact accepted sender context for new messages. Never invent legacy Hz from room center.
- readAt(hz,family) / live watchAt(hz,family) reuse room/delta payloads marked view:frequency, with separate registry. Exact directory SUMMARY.stations contains every Placement, including slice, sourceRoom, transient ref, Hz, family and public identity/state. Member refs are scoped by sourceRoom; turn/message IDs global.
- Station link version2 only for roundtable2 peers. Add roundtable.postAt(sliceId,hz,family,text,clientId), roundtable.readAt(hz,family) and roundtableFrequency:<family>-<hz> records. Preserve version1 peers.
- Path metadata: nullable bearingDeg integer0..359 from identified talker to reporter; unknown/doubled/missing/coincident direction blank. Snapshot endpoint data for retained last-talker reports. Roundtable/DistanceUnits defaults mi, option km; Roundtable/ShowBearing defaults True. Convert only display using km/1.609344; sort by canonical distance.
- Desktop/iPhone show people at exact Hz, cochannel callsign stacks, explicit last talker and Heard by/dBm/age. Reports join reading.turn. Preserve received history with sender frequency/time after tuning; draft/retry keyed by slice/family/Hz. Public browse never tunes; authenticated follow authority derives from actual station presence with existing cookie/CSRF protections.
- Build admission uses current ~/.config/nereus/work/pr-review-20261021/build-lane-lock.md and lane-admit.sh, not superseded lock counts. No downloads. Focused checks per task; final combined checks once after actual source changes. No unchanged rerun loops or tolerance weakening.

## Review Focus

- A/B/C chain 0/5000/10000 Hz remains pairwise; +/-5000 included and +/-5001 excluded across RF room boundaries.
- Exact duplicate callsigns and cochannel multi-slice presence retain transient identity, sourceRoom/ref scope and geometric clustering.
- Tune/retry races preserve original draft context and refuse stale sends; hidden/away/offline cannot publish.
- Last-talker readings and paths retain correct endpoints after unkey/leave; doubled/unknown never manufacture attribution.
- Reconnect/resync, deletion/moderation, first-run privacy, actual-exposure mentions and old-peer compatibility remain intact.

### Task 1: Server frequency contract and persistence

**Files:** roundtable/server/nereus_roundtable/{protocol,rooms,service,store,turns,web,http,grid}.py and focused roundtable/tests; static files excluded.
**Interfaces:** Produces negotiated directory, frequency room/delta views and guarded postAt exactly as Global Constraints.
- [ ] Pin boundary, stale context, old-peer, deletion and privacy cases with deterministic clock/client tests.
- [ ] Implement additive schema upgrade with legacy unknown values blank; frequency queries and view resync; directed live/retained bearing.
- [ ] Run focused server tests, then server suite once for final server revision; report commands/exits and limitations.

### Task 2: Core model and station link

**Files:** src/core/roundtable/*, src/models/RoundtableModel.*, station capability/client/server/dispatcher, IStationLink and shared record/schema APIs, NereusKit link/record declarations and conformance documentation.
**Interfaces:** Consumes Task1 contract; produces version2 verbs, records, typed exact presence/frequency APIs, station preferences and context-keyed history/drafts for UI.
- [ ] Add negotiated wire and station-link regression cases preserving old v1 bytes.
- [ ] Implement matching builders/validators, capability-gated actions, frequency streams and reconnect/resync.
- [ ] Build matching targets before focused tests; retain actual verification evidence.

### Task 3: Desktop surface

**Files:** src/gui/roundtable/*, spectrum tag integration, Roundtable applet/window, SpotHub settings; corresponding GUI tests/captures.
**Interfaces:** Consumes Task2 APIs; no shared model edits without ownership transfer.
- [ ] Implement exact people/cochannel stack, frequency chat, correct report join, retained history/drafts and compact mi/degreeT path.
- [ ] Add controls for shared preferences and preserve read/tune authority and all approved message/consent states.
- [ ] Build focused GUI targets and inspect actual rendered controls at normal and Windows density.

### Task 4: iPhone surface

**Files:** ios/NereusApp/Roundtable/* and corresponding app model/UI tests and fake fixtures.
**Interfaces:** Consumes Task2 version2 contract; shared NereusKit declarations remain controller-owned.
- [ ] Implement frequency subscription/action/context model, exact tags/stacks, reporter join and path controls.
- [ ] Preserve keyboard/full/half/landscape layouts, retained history/drafts/failure, consent/offline/away gating and no Archive.
- [ ] Run matching Swift/app checks and inspect actual product renders; archive/TestFlight only after integrated acceptance.

### Task 5: Website surface

**Files:** roundtable/server/static/{index.html,roundtable.js,roundtable.css,archive.html,archive.js}, applicable page tests.
**Interfaces:** Consumes Task1 negotiated public frequency data and server-authorized owner-follow endpoints.
- [ ] Replace room-first presentation with exact people/frequency views and explicit attributed report rows.
- [ ] Implement mi/km/bearing display, read-only browse vs authenticated follow and retained/earlier history.
- [ ] Run page tests and inspect real served UI; preserve helper authorization and no synthetic login state.

### Task 6: Integrated acceptance and delivery

- [ ] One consolidated requirements/code review across final source, focused correction wave and scoped recheck.
- [ ] Build all_tests then final suite once on combined C++ source; required Swift/server/CI floors, review actual renders and cross-surface functional integration.
- [ ] Signed commits and reviewable PR(s); merge only required accepted heads. Produce combined desktop/phone builds and operator checklist; upload TestFlight using existing authorization. Live station restart requires current idle authorization. Final release tag remains pending actual acceptance.

## Parallel release work

PR697 audio observer correction, CW first-edge passive diagnostic and PR705 package diagnosis have separate owned checkouts/reports. They do not alter Roundtable contracts or justify loosening release gates. PR706 fork CI approved once after source diff inspection. PR339 and698 remain deferred drafts.
