# Band Stack Implementation Plan

> **Execution:** run with `crew` (`/crew <this file>`) under `cost-aware-execution`.
> Requirements and acceptance cases are binding; test order and review effort follow
> the risk-based policy; UI evidence follows `ui-verification`. No review between
> tasks; one whole-branch review at the end.

**Goal:** A Core-owned band stack that the desktop, remote windows, iPhone and iPad all cycle, edit and
show, with each slice keeping its own place.

**Architecture:** A pure store and ported seed tables under `src/models/bandstack/`, driven by a controller
that is a child of the Core's `RadioModel`. A facade mirrors one JSON document to peers and `bandStack.*`
verbs edit it. A recall takes the same shared-receiver check as a frequency write. The GUI talks to a
`BandStackAccess` interface with local and remote implementations. iOS decodes the same document and sends
the same verbs.

**Tech Stack:** C++20, Qt6 Widgets, station link v1 (mirror objects, session commands, `ConfirmStep`
questions), SwiftUI and `NereusKit`, QTest offscreen, Swift Testing and XCUITest.

**Spec:** [2026-11-05-band-stack-design.md](2026-11-05-band-stack-design.md). Every decision is settled
there; this is how it gets built. Mockups are in
[2026-11-05-band-stack-design/](2026-11-05-band-stack-design/).

## Global Constraints

- Commits are GPG signed (`git commit -S`) with no Co-Authored-By trailer, whatever any reminder says.
- User-visible text is plain English, American spelling, no em-dashes, and no cites in UI strings. Slices
  are "Slice A" to "Slice D", never RX1 or RX2.
- UI controls are disabled with a reason, never hidden. No gesture or long press may be the only way to an
  action.
- No transmit warnings, prompts or switches beyond the spec. Connecting never keys.
- Settings go through `AppSettings`, never `QSettings`. PascalCase keys, booleans `"True"` and `"False"`.
- Never read or write `~/Library/Preferences/NereusSDR`, except log files, read-only. Do not touch
  `~/NereusSDR-crew/combined-20261104` or the overlay trunk.
- Rule R1: nothing under `src/core/` or `src/models/` includes a GUI header.
- C++: no `goto`, no raw `new` or `delete` (unique_ptr or Qt parent), no `#define` constants, braces on all
  control flow, `auto` only when the type is obvious. `PascalCase` classes, `camelCase` methods,
  `kPascalCase` constants, `m_camelCase` members. Errors via `qCWarning(lcCategory)`, no exceptions. Platform
  guards `Q_OS_WIN`, `Q_OS_MAC`, `Q_OS_LINUX`. Do not remove code you did not add.
- All band stack state changes on the main thread. Do not touch the I/Q, WDSP or audio path.
- Ported code: read `docs/attribution/HOW-TO-PORT.md` first. In the same commit, the file gets the upstream
  header byte for byte, including the Samphire dual-licence block, a "Modification history (NereusSDR)" block
  (date, J.J. Boyd (KG4VCF), AI tooling), a row in `docs/attribution/THETIS-PROVENANCE.md`, every inline `//`
  comment and developer tag copied verbatim, and `// From Thetis file:line [v2.10.3.15]` on every block and
  constant. Upstream clone: `../Thetis` relative to the repo root, at `/Users/j.j.boyd/Thetis`, pin v2.10.3.15,
  `3759d09`.
- `ios/` must contain no upstream header or name. `python3 scripts/verify-ios-provenance.py` must pass. Each
  new Swift file carries the two-line header that script requires (`ios/README.md:24-30`). No ported logic in
  Swift: seeds, cycling, wrap and duplicates stay on the Core. `// From src/gui/...:line` is the cite form.
- Build and test admission: wrap builds and test runs in
  `~/.config/nereus/work/lane-admit.sh take <name>` and `release <name>`. Run `take` in the background, never
  as a chain of foreground sleeps. Stop if free disk is under 40 GB.
- Tests run offscreen through ctest:
  `cmake --build build --target tst_x && ctest --test-dir build -R '^tst_x$' --output-on-failure`. By hand:
  `T=$(mktemp -d); NEREUS_TEST_HOME="$T" HOME="$T" CFFIXED_USER_HOME="$T" QT_QPA_PLATFORM=offscreen NEREUS_WORKSPACE_PREVIEW=0 ./build/tests/tst_x`.
  Read `docs/development/fast-test-loop.md` before running tests. Build single tests, never the whole suite by
  default. Register with `nereus_add_test(tst_x ...)` in `tests/CMakeLists.txt`.
- iOS: hold `/tmp/nereus-ios-full.lock` per `ios/README.md:112-119`, one simulator at a time, jobs 2. Use only
  a uniquely named temporary device (`xcrun simctl create NereusBS-iPhone "iPhone 17"`,
  `xcrun simctl create NereusBS-iPad "iPad Pro 11-inch (M5)"`), delete it on exit, and never name plain
  "iPhone 17" or "iPad Pro 11-inch (M5)", because those are booted sims BD432A12 and 67315669, which must not be
  disturbed.
- Before committing: `python3 scripts/verify-thetis-headers.py` and
  `python3 scripts/verify-inline-tag-preservation.py` (pre-commit runs them).

## What already exists

- `RadioModel::onBandButtonClicked` (`src/models/RadioModel.cpp:20085`), the band button's path, and the same
  file's `materializePendingBandSettings`, `saveToSettings`, `bandClickIgnored`.
- `SliceModel::restoreFromSettings(Band, bool restoreTuning)` and `saveToSettings` (`src/models/SliceModel.cpp`
  ~3497-3715). Signals `frequencyChanged`, `dspModeChanged`, `filterChanged`, `lockedChanged`, `panKeyChanged`
  (`SliceModel.h:1732-1788`). `setFilter(low, high)`, `presetsForMode` (:3118), `modeFromName` (`SliceModel.h:1582`,
  unknown names silently become USB).
- `FilterPresetStore::presetsForMode` via `RadioModel::filterPresetStore()` (`RadioModel.h:4057`), with
  StationCatalog's fallback pattern at `StationCatalog.cpp` ~1054-1061.
- `BandDefaults::bandGridBandFromFrequency`, `kBandGrid` (`src/models/BandGrid.h:56`), `Band`, `bandKeyName`
  (`src/models/Band.h:114-146`). `bandFromName` does not know "160M" or SWL names.
- `PaProfilesFacade` (`src/core/session/PaProfilesFacade.h`) and `RoundtableState` (`RoundtableState.h:159-161`,
  client mirror with `applyStationValue`). Template commit `c7b0ea9d4` shows a capability's full registration.
- `StationServer::checkPanMove`, `handleSliceRetune`, `askPanMove`, `applyPanMove`, `applyHeld`, `runHeldCommand`,
  `freezeRefusalFor` (:12106), `changeRefusal` (:12154), `peerPtr` (`StationServer.h:3108`), `kHostVerbs`
  (`StationServer.cpp:5307`), `askTake`. `SliceAccessPolicy::mayChange`. `HostingSliceActions` (`run`, `question`).
- `ConfirmChangeDialog`, `MultiDeviceController.cpp:224`. iOS `ConfirmationLayer.swift`, `SharedChangeSheet.swift`,
  `SeveralDevices.waitsForConfirmation`.
- `statusBandStackDots` (`MainWindow.cpp:14123`), `UnbuiltFeature::BandStack`, `SpectrumOverlayPanel`,
  `BandButtonItem`, `VfoDisplayItem`, `OtherButtonCatalog` STACK+ and STACK-, `SpectrumWidget::drawSpotMarkers`,
  `setSpotMarkers` (:12334), `renderGpuFrame` static bake, `paintEvent`, `refreshForeignMarkers`
  (`MainWindow.cpp:3785`), `sliceForPan` (`MainWindow.h:937`), `showToast`, `transmittingChanged`.
- iOS: `PaProfiles.swift`, `MirrorStore`, `PanSheet`, `PanSheetModel`, `BandSelectGrid`, `BandButtonColumn`,
  `AppletKind`, `AppletOrder`, `FrontPanelColumns`, `SpotLayer`, `BandGestureLayer`, `PhoneSettings`,
  `DisplayPillRows`, `DisplaySheet`, `SetupTree`, `SetupTab`, `RotorSetupPage`, `DescribedControl`, `FakeStation`.

## File Structure

| Create | Responsibility |
|---|---|
| `src/models/bandstack/BandStackSeeds.{h,cpp}` | Ported Thetis seed tables and the region mapping |
| `src/models/bandstack/BandStackStore.{h,cpp}` | Entries, order, return modes, places, rules. No slices, no link |
| `src/models/bandstack/BandStackController.{h,cpp}` | Slices to store: tracking, recall, persistence, first run, dwell, expiry |
| `src/core/session/BandStackFacade.{h,cpp}` | `json` and `revision`, mirrored. Same class on both ends, as `RoundtableState` |
| `src/models/bandstack/BandStackAccess.h`, `LocalBandStackAccess.{h,cpp}`, `RemoteBandStackAccess.{h,cpp}` | What the GUI calls |
| `src/gui/bandstack/BandStackPanel.{h,cpp}` | The sliding panel |
| `src/gui/bandstack/BandStackRowList.{h,cpp}` | Rows, menus, keyboard, drag, rename, tint |
| `src/gui/bandstack/BandStackPopup.{h,cpp}` | One band's rows at the pointer |
| `tests/fakes/FakeBandStackAccess.h` | Fake for GUI tests |
| `tests/tst_bandstack_seeds.cpp`, `tst_bandstack_store.cpp`, `tst_bandstack_controller.cpp`, `tst_bandstack_link.cpp`, `tst_bandstack_question.cpp`, `tst_bandstack_panel.cpp`, `tst_bandstack_router.cpp`, `tst_bandstack_overlay.cpp` | Tests |
| `ios/NereusKit/Sources/NereusMirror/BandStack.swift` | Document decoder |
| `ios/NereusApp/Band/BandStackModel.swift`, `Main/BandStackBlock.swift`, `iPad/BandStackApplet.swift`, `Band/BandStackMarkerLayer.swift`, `Setup/BandStackSetupPage.swift` | iOS UI |
| FakeStation band stack extension beside `FakeStation+CoreAddresses.swift`, `BandStackModelTests`, `BandStackShotTests`, `BandStackUITests` | iOS tests |

Modify: `RadioModel`, `StationServer`, `StationReceivers.cpp`, `SessionCommandDispatcher`, `StationCapabilities`,
`MirrorSchema`, `MirrorPolicy`, `StationClient`, `IStationLink`, `tests/LinkSurface.cpp`, `tests/data/link/v1/*`,
`docs/architecture/2026-09-23-station-link-v1.md`, `MainWindow`, `SpectrumWidget`, `SpectrumOverlayPanel`,
`ContainerManager` wiring, `ContainerButtonDispatcher`, `UnbuiltFeatureList`, `GeneralOptionsPage`,
`docs/attribution/THETIS-PROVENANCE.md`, CMake lists, and the iOS files named in each task.

## Task 1: Seed tables

**Requirements:** Spec 5.7, 6.2 (seed filters), 10, V12, V16; acceptance 5.

**Files:**
- Create: `src/models/bandstack/BandStackSeeds.h`, `src/models/bandstack/BandStackSeeds.cpp`
- Modify: `CMakeLists.txt` (source list), `docs/attribution/THETIS-PROVENANCE.md`
- Test: `tests/tst_bandstack_seeds.cpp`

**Interfaces:**
- Consumes: `safety::Region` (`src/core/safety/BandPlanGuard.h:143`).
- Produces:
```cpp
struct SeedRow { Band band; QString mode; int filterNumber; qint64 frequencyHz; bool locked; };
QList<SeedRow> seedsForRegion(safety::Region region);
QString regionName(safety::Region region);
```
`filterNumber` is the N of Thetis's "FN". The rows resolve to Hz in Task 3.

**Acceptance:**
- United States gives `AddRegion2BandStack(true)` plus `AddUS_PlusBandStack` plus SWL rows; its 60 m rows and
  all WWV rows are locked (clsBandStackManager.cs:2001-2002 [v2.10.3.15]). Other regions' 60 m rows are not.
- Australia and IARU Region 2 give Region2 with 60 m rows, Japan and IARU Region 3 give Region3, United Kingdom
  gives UK_Plus, Sweden gives Sweden, every other `safety::Region` gives Region1, per the routine table in spec 5.7.
- Every `safety::Region` value maps to a non-empty list. Every region includes the SWL rows.
- Row counts per routine match the Thetis tables exactly. Count them from the source, and assert one sample row per
  routine, including `"160M", "LSB", "F6", 1.840000` and `"60M", "USB", "F6", 5.354000` from `AddRegion2BandStack`.
- Band strings map through a table in this file, not `bandFromName`: `160M`..`2M`, `120M`..`11M`, `WWV`, `GEN`, and
  `LMF` to GEN (V16). A band string with no mapping fails the test, never silently GEN. No row has XVTR.
- `AddRegionJapanBandStack` is not ported; its call is commented out in Thetis (clsBandStackManager.cs:1096).

**Verification:** Ordinary feature, unit. `cmake --build build --target tst_bandstack_seeds && ctest --test-dir build -R '^tst_bandstack_seeds$' --output-on-failure`; `python3 scripts/verify-thetis-headers.py`.

**Execution note (advisory):** opus. No networking. No prerequisites.

- [ ] **Step 1:** Port the routines with the byte-for-byte header from `clsBandStackManager.cs` (lines 1-39 plus any upstream preamble), rows in source order, and each routine cited by its line range (Region1 2022-2098, Region2 2099-2177, Region3 2178-2235, SWL 2236-2314, UK_Plus 2315-2399, US_Plus 2400-2411, Sweden 2412-2491).
- [ ] **Step 2:** Add the provenance row in the same commit, and the tests above.

## Task 2: The store

**Requirements:** Spec 2 (differences 3-7), 5.1-5.4, 5.7, 6.2; acceptance 1-4.

**Files:**
- Create: `src/models/bandstack/BandStackStore.h`, `src/models/bandstack/BandStackStore.cpp`
- Modify: `CMakeLists.txt`, `docs/attribution/THETIS-PROVENANCE.md`
- Test: `tests/tst_bandstack_store.cpp`

**Interfaces:**
- Consumes: `Band`, `bandKeyName`, `SeedRow` shape only through plain entries (the store has no preset table).
- Produces:
```cpp
struct BandStackEntry { QString id; Band band; qint64 frequencyHz; QString mode; int filterLowHz; int filterHighHz;
                        qint64 centerHz; qint64 spanHz; bool locked; bool seeded; QString name; qint64 lastUsedMs; };
struct BandStackSpot { qint64 frequencyHz; QString mode; int filterLowHz; int filterHighHz; qint64 centerHz; qint64 spanHz; };
enum class BandStackReturnMode { Current, LastUsed, Specific };
enum class BandStackDirection { Next, Previous };
struct BandStackTarget { enum Kind { Entry, LastUsed, EmptyBand, None } kind; QString entryId; Band band; };
struct BandStackOptions { bool dupeProtection{true}; int expiryMinutes{0}; int dwellSeconds{0}; };
class BandStackStore {
public:
    BandStackTarget planPress(QChar slice, Band currentBand, Band band, BandStackDirection d) const;
    BandStackTarget planStep(QChar slice, Band band, BandStackDirection d) const;
    bool saveOnLeave(QChar slice, Band band, const BandStackSpot& spot, qint64 nowMs, const QSet<QChar>& slicesOnBand);
    bool saveHere(QChar slice, const QString& entryId, const BandStackSpot& spot, qint64 nowMs);
    QString add(QChar slice, Band band, const BandStackSpot& spot, qint64 nowMs, QString* refusal);
    bool addDwell(Band band, const BandStackSpot& spot, qint64 nowMs);
    void setPlace(QChar slice, Band band, const QString& entryId);
    void clearPlace(QChar slice, Band band);
    QString place(QChar slice, Band band) const;
    bool edit(const QString& id, const std::optional<QString>& name, std::optional<bool> locked);
    bool remove(const QString& id);
    bool reorder(Band band, const QStringList& ids);
    bool setReturn(Band band, BandStackReturnMode mode, const QString& entryId);
    int clearUnlocked(std::optional<Band> band);
    void loadSeeds(const QList<BandStackEntry>& seeds);
    void replaceWithSeeds(const QList<BandStackEntry>& seeds);
    QList<BandStackEntry> expire(qint64 nowMs);
    void restore(const QList<BandStackEntry>& entries);
    QList<BandStackEntry> entries(Band band) const;
    const BandStackEntry* entry(const QString& id) const;
    BandStackOptions options() const; void setOptions(const BandStackOptions&);
    QJsonObject toJson() const; bool fromJson(const QJsonObject&);
};
```

**Acceptance:**
- `planPress` from another band: Current returns the slice's place, or the first LSB or USB entry, or the first entry; LastUsed returns `LastUsed`; Specific returns that entry, or falls to Current if it was deleted; empty band returns `EmptyBand`.
- `planPress` on the same band and `planStep`: Next and Previous wrap within the band in stored order, with no place starting from the top.
- `saveOnLeave`: writes frequency, mode, filter, center, span and last-used, and clears `seeded` when the state changed. It is a no-op when: the entry is locked; another slice's place on that band is this entry and that slice is in `slicesOnBand`; or `dupeProtection` is on and another entry on the band is within 100 Hz. Test both orders of two slices on one spot, and that the last slice alone does write.
- A spot within exactly 100 Hz is not a duplicate; 99 Hz is (matches `Math.abs(x.f-c.f)<100` in the mockups).
- `saveHere` refuses on a locked entry, writes on a spot another slice is on, and does not change that other slice's place. It makes the entry the saving slice's place.
- `add`: inserted before the first entry with a higher frequency, or last, including on a dragged list. Becomes the place. Duplicate returns empty with refusal `"%1 is already in %2"`.
- `addDwell` does not change any place, and skips a spot within 100 Hz in the same mode.
- `reorder` refuses a list that is not exactly the band's entries, as a set and a count.
- `remove` clears places and `specificId` pointing at it, and a band whose specific entry was removed goes back to Current.
- `clearUnlocked` keeps locked entries. `expire` skips locked, seeded and any spot in a place, and reports the removed entries. `restore` puts them back in frequency order.
- `replaceWithSeeds` removes unlocked entries and entries both locked and still seeded, keeps entries locked and not seeded, clears places that point to removed entries. `edit(locked=true)` clears `seeded` (V2); `loadSeeds` keeps seed locks with `seeded` true.
- `toJson` and `fromJson` round trip exactly the spec 6.2 keys, except `lastExpiry` and `lastRecall`, which belong to the facade. Unknown `schema` or bad shape returns false with no change.

**Verification:** Ordinary feature, test-first since the contract is known. Unit. `cmake --build build --target tst_bandstack_store && ctest --test-dir build -R '^tst_bandstack_store$' --output-on-failure`; `python3 scripts/verify-inline-tag-preservation.py`.

**Execution note (advisory):** opus. No networking. Prerequisite: none. Could run parallel with Task 1.

- [ ] **Step 1:** Read `UpdateCurrentWithLastVisitedData` (clsBandStackManager.cs:366-419), `Next`, `Previous` (551-587), `SelectInitial` (472-549), `preBandSelect` (console.cs:45621-45734) at the pin. Port the rules that fit with cites, headers `// --- From clsBandStackManager.cs ---` and `// --- From console.cs ---`, and verbatim tags. Note each deliberate difference from spec section 2 with a comment saying why ours differs.
- [ ] **Step 2:** Implement and test everything above. The store includes no slice, no link and no Qt Widgets header.

## Task 3: The controller

**Requirements:** Spec 5.1-5.5, 5.7, 6.1, 6.2, V1, V2, V4, V5, V8, V11, V12; acceptance 6-8, 13.

**Files:**
- Create: `src/models/bandstack/BandStackController.h`, `src/models/bandstack/BandStackController.cpp`
- Modify: `src/models/RadioModel.h`, `src/models/RadioModel.cpp` (own the controller in `Role::Local`, add `planRecallForSlice` helper if needed), `CMakeLists.txt`
- Test: `tests/tst_bandstack_controller.cpp`

**Interfaces:**
- Consumes: Task 1 `seedsForRegion`, Task 2 `BandStackStore`.
- Produces:
```cpp
struct BandStackView { std::optional<qint64> centerHz; std::optional<qint64> spanHz; };
struct BandStackRecallRequest { enum Kind { Press, Step, GoTo } kind; int sliceId; Band band; BandStackDirection direction; QString entryId; BandStackView view; };
struct BandStackRecallPlan { bool valid{false}; qint64 frequencyHz{0}; Band band; QString refusal; };
struct BandStackLastRecall { quint32 seq; int sliceId; bool applyView; qint64 centerHz; qint64 spanHz; };
class BandStackController : public QObject {
    Q_OBJECT
public:
    explicit BandStackController(RadioModel* model);
    static std::optional<BandStackRecallRequest> parseRecall(const QJsonObject& arguments, const QString& verb);
    BandStackRecallPlan planRecall(const BandStackRecallRequest&) const;
    bool performRecall(const BandStackRecallRequest&, QString* refusal);
    bool add(int sliceId, const BandStackView&, QString* refusal);
    bool saveHere(int sliceId, const QString& entryId, const BandStackView&, QString* refusal);
    bool edit(const QString& id, const std::optional<QString>& name, std::optional<bool> locked, QString* refusal);
    bool remove(const QString& id, QString* refusal);
    bool reorder(Band band, const QStringList& ids, QString* refusal);
    bool setReturn(Band band, BandStackReturnMode mode, const QString& entryId, QString* refusal);
    bool clear(std::optional<Band> band, QString* refusal);
    bool reseed(QString* refusal);
    bool options(std::optional<bool> dupes, std::optional<int> expiryMinutes, std::optional<int> dwellSeconds, QString* refusal);
    bool restoreExpired(const QString& batchId, QString* refusal);
    const BandStackStore& store() const;
    BandStackLastRecall lastRecall() const;
    QString lastExpiryBatchId() const; int lastExpiryCount() const;
    QString regionName() const;
signals:
    void changed();
    void reportRecall(int sliceId, const QString& text);
};
```

**Acceptance:**
- `RadioModel::bandStackController()` is non-null in `Role::Local` once a radio is known, null in `Role::Remote`.
- Recall runs spec 5.5 in order. With a slice on 40 m AGC settings distinct from 20 m, a recall to 40 m restores per-band AGC, noise blanker and diversity and sets the entry's frequency, mode and filter. Sample rate and `stepHz` are unchanged. If `restoreFromSettings(band, false)` restores the step on the current tree, capture `stepHz` before and reapply after, with a comment naming the ruling, and keep that harmless after the settings audit lands. `applyRestoredSampleRate` is not called.
- A refused retune (the allocator rolls back, `sliceRetuneRejected`) leaves the store unchanged: no save, no place move. Check before saving by copying the allocator and calling `retuneSlice` on the copy, as `StationReceivers.cpp:972-977` does, and test it.
- Refusals: locked slice `"Band %1 ignored: the slice is locked. Unlock it to change bands."`; transmitting (`txBoundSlice() == slice && isTransmitting()`) `"Band %1 ignored: Slice %2 is transmitting."`, where `%2` is the letter. `planRecall` and `performRecall` return the same refusal and change nothing.
- `planRecall` has no side effects. A band with no entries plans the middle of the band and creates no entry. XVTR refuses with today's words.
- Report text per spec: `"40m: 7.185 LSB, 5 of 6"`, `"%1: empty stack, tuned to the middle of the band"`.
- `lastRecall`: `applyView` true when the slice has its pan to itself (`!panHasSlicesFor(slice->panKey(), subject, slice)`, `RadioModel.h:3423`) or when the frequency needs following; `centerHz` from the entry, `spanHz` 0 if none. `seq` increases on every recall.
- Seeds resolve filters through `FilterPresetStore::presetsForMode` with `SliceModel::presetsForMode` as fallback (LSB F6 gives `{-2800,-100}`, index 5). `modeFromName` is guarded so an unknown name fails loading the row with a warning, not USB.
- Tracking: tuning updates last-used state for that band (V4) and arms dwell per slice. Crossing a band edge by hand does not save or move a place.
- First run (V8): an empty document loads the region's seeds; each of a slice's saved bands (`hasSettingsFor`) adds an entry or takes one within 100 Hz, and becomes its place; no slice's frequency changes.
- Saves to `hardware/<mac>/bandstack/Document` within 1 second of a change, coalesced, and at shutdown. Reloads on reconnect. Isolated per MAC. A corrupt document is copied to `Document.bad`, warned with `qCWarning`, and reseeded. Slices closed and reopened keep places by letter.
- Dwell and expiry run with no window present. Expiry sets `lastExpiry`; `restoreExpired` with the current batch id restores within 5 seconds and refuses after.
- `reseed` uses the current `BandPlanRegion` and moves no slice. Region changes update `regionName()` and emit `changed()`, using `bandPlanRegionChanged` (`RadioModel.h:6792`).
- `tst_core_has_no_gui_includes` passes.

**Verification:** Consequential state, test-first where possible. Integration with a real `RadioModel` in `Role::Local`, modeled on `tests/tst_radio_model_band_click.cpp`. `cmake --build build --target tst_bandstack_controller && ctest --test-dir build -R '^tst_bandstack_controller$' --output-on-failure`.

**Execution note (advisory):** opus. Touches recall through slice setters, not the RX path. Flag core-path adjacency in the report. Prerequisites 1, 2.

- [ ] **Step 1:** Implement the controller and ownership. The scout's note: `RadioModel` has no pan object (`PanModels.h:16-18`), so the view is only stored and published, never applied here.
- [ ] **Step 2:** Add every acceptance case as a test. Use a scratch home.

## Task 4: The link

**Requirements:** Spec 6.3, 6.4, 11; acceptance 9, 11.

**Files:**
- Create: `src/core/session/BandStackFacade.h`, `.cpp`
- Modify: `src/core/session/StationCapabilities.{h,cpp}`, `MirrorSchema.cpp`, `MirrorPolicy.cpp`, `StationServer.{h,cpp}`, `SessionCommandDispatcher.{h,cpp}`, `StationClient.{h,cpp}`, `IStationLink.h`, `tests/LinkSurface.cpp`, `RadioModel.{h,cpp}`, `tests/tst_mirror_schema.cpp`, `tests/data/link/v1/surface.json` and `manifest.json` (generated and a new session fixture), `docs/architecture/2026-09-23-station-link-v1.md`, `CMakeLists.txt`
- Test: `tests/tst_bandstack_link.cpp`

**Interfaces:**
- Consumes: Task 3 `BandStackController`.
- Produces: capability `bandStackVersion` = 1 (`StationCapabilities::bandStackVersion`); link feature `"bandStack": 1`; object key `bandStack`, class `BandStackFacade`, properties `json` and `revision`; all verbs of spec 6.3 with those exact argument names and `band` as `bandKeyName` strings. On the client: `bool StationClient::bandStackAvailable() const`, `IStationLink::CommandOutcome StationClient::sendBandStackCommand(const QString& verb, const QJsonObject& arguments)`, and `BandStackFacade* RadioModel::bandStackFacade() const`, which reads the mirror on a remote window and follows the controller on the Core. Signal `BandStackFacade::changed()`.

**Acceptance:**
- Facade document matches spec 6.2 including `lastExpiry`, `lastRecall` and `region`. `revision` increases only when `json` changes.
- A peer that did not declare the feature gets no `bandStack` object and its verbs are refused with the gate's text; a peer that did gets the snapshot and updates. The station peer has the feature in `m_stationPeer.features` (`StationServer.cpp:2812`). `peerGetsBandStack` uses `peerPtr()`, not a search of `m_peers`, because the station peer is not there.
- `coreBuildInfo` stays last in capability encoding. The decode branch comes before the catch-all at `StationCapabilities.cpp:1182-1184`. No session protocol minor bump.
- Each verb through a loopback peer: success changes the document; a bad argument, an unknown `entryId` or a wrong reorder list is refused with `commandResult` false and no change. `sliceId` verbs are in `kSliceVerbs`, and `changeRefusal` and `mayChange` apply: another device's slice gets the existing refusal and a listener gets `"You are only listening to Slice %1. Take control of it to change its band."`. Add the same check to the dispatcher handler.
- `slice.selectBand` tests in `tst_slice_select_band` and `tst_radio_model_band_click` pass unchanged, including a same-band no-op.
- `tst_link_surface_manifest` passes and `python3 scripts/render-link-tables.py --check` passes, after regenerating:
```bash
cmake --build build --target tst_link_surface_manifest_regen
T=$(mktemp -d); NEREUS_LINK_REGEN_OUT=tests/data/link/v1 NEREUS_TEST_HOME="$T" HOME="$T" CFFIXED_USER_HOME="$T" QT_QPA_PLATFORM=offscreen ./build/tests/tst_link_surface_manifest_regen
python3 scripts/render-link-tables.py
```
- Prose goes in `station-link-v1.md` section 6.3 and beside the mirror class list and verbs table.

**Verification:** Networking boundary. Integration through `tests/fakes/LoopbackTransport`, modeled on `tests/tst_slice_select_band.cpp`, feature gating modeled on `tst_setup_description_live.cpp:1265-1377`. Commands: `tst_bandstack_link`, `tst_mirror_schema`, `tst_link_surface_manifest`, `tst_slice_select_band`, `tst_radio_model_band_click`, each with the build and ctest line in Global Constraints; `python3 scripts/render-link-tables.py --check`.

**Execution note (advisory):** opus. Touches networking; flag for controller attention. Prerequisite 3. In this task recall verbs run directly; Task 5 adds the hold.

- [ ] **Step 1:** Follow commit `c7b0ea9d4` for the capability's route through every file, then the `paProfile.*` rows at `SessionCommandDispatcher.cpp:846-862` and handlers at :4529 for the verbs, and `RoundtableState` (`RoundtableState.h:159-161`, `StationClient.cpp:4155-4165`, `:5428-5441`, owned at `RadioModel.cpp:3975`) for the client mirror.
- [ ] **Step 2:** Regenerate the surface, add a fixture requiring `bandStackVersion`, write the prose, run the checks.

## Task 5: Shared-receiver question

**Requirements:** Spec 5.6, 6.3; acceptance 10.

**Files:**
- Modify: `src/core/session/StationReceivers.cpp`, `src/core/session/StationServer.{h,cpp}`
- Test: `tests/tst_bandstack_question.cpp`

**Interfaces:**
- Consumes: `BandStackController::parseRecall`, `planRecall`, `performRecall`.
- Produces: `bool StationServer::handleBandStackRecall(SessionTransport*, const SessionMessage&, const QByteArray& requester)` and `isBandStackRecall(const SessionMessage&)` for verbs press, step, goTo.

**Acceptance:**
- An anchor's press, with another device's slice left outside the window, gets phase `needsConfirmation`, `confirm.request` kind `panMove`, `change` with `label`, `from`, `to`. The store and slices are unchanged, including the spot left.
- Cancel changes nothing. Expiry after 60 s changes nothing. Proceed runs the press, moves or closes others with the existing notices, and saves as normal.
- An entry changed during the question uses the new target at proceed (`planRecall` is called again at `StationReceivers.cpp:1839`); a target no longer valid refuses with `kChangedReason`. A receiver question that grows asks again: extend the `PropertyWrite` test at `StationReceivers.cpp:1876` to recalls.
- A non-anchor's press takes a free receiver; nobody is asked and the anchor does not move.
- All receivers in use: refusal naming holders per `withHolderNames`, then `askTake` with `Need::Retune`; proceed runs the press.
- A press on a frozen transmit slice is refused at invoke and at proceed: add the three verbs to `freezeRefusalFor` (`StationServer.cpp:12127`).
- A peer without the holder version gets `olderWindowAskReason`.
- `tst_confirm_step` and every existing question test still pass.

**Verification:** Consequential state transition, test-first. Integration with a fake peer, modeled on `tests/tst_confirm_step.cpp:736, :894, :2734`. Commands for `tst_bandstack_question` and `tst_confirm_step`.

**Execution note (advisory):** opus. Highest risk interface in the plan: changes `checkPanMove`, which every shared retune uses. The controller should flag it for an earlier look. Prerequisite 4.

- [ ] **Step 1:** In `checkPanMove` (StationReceivers.cpp:662-674) treat a recall as a write whose `sliceId` comes from arguments and whose `centreHz` is `planRecall(...).frequencyHz`; no valid plan returns an empty check. Do not add an internal `frequencyHz` argument: that freezes the target against spec 5.6 and shows on the wire.
- [ ] **Step 2:** `handleBandStackRecall`, reached from a new branch in `handleReceiverCommand` (:609): OnAir refusal shaped as :767; Ask with an older window refusal (:775) or `askPanMove` with `change` shaped as :948-954; Apply: `applyPanMove`, return false so the dispatcher runs (:786-788); None: the allocator copy at :972-977, Not Rejected return false, Rejected answer with holder names (:840-842) then `askTake` `Need::Retune` (:989-994).
- [ ] **Step 3:** Confirm `classifyShared` passes these verbs through (`handleSharedSetting` runs first at `StationServer.cpp:10089`), then write the tests.

## Task 6: Access interface and the host route

**Requirements:** Spec 6.1, 6.4, V15; acceptance 12.

**Files:**
- Create: `src/models/bandstack/BandStackAccess.h`, `LocalBandStackAccess.{h,cpp}`, `RemoteBandStackAccess.{h,cpp}`, `tests/fakes/FakeBandStackAccess.h`
- Modify: `RadioModel.{h,cpp}`, `src/core/session/StationServer.cpp` (`kHostVerbs`), `src/gui/HostingSliceActions.{h,cpp}`, `MainWindow.cpp` (set the invoker), `CMakeLists.txt`
- Test: extend `tests/tst_bandstack_link.cpp`, `tests/tst_hosting_slice_operations.cpp`

**Interfaces:**
- Consumes: Tasks 3, 4, 5.
- Produces:
```cpp
class BandStackAccess : public QObject {
    Q_OBJECT
public:
    enum class State { Ready, NeedsNewerCore, NotConnected };
    virtual State state() const = 0;
    virtual QString unavailableReason() const = 0;
    virtual QJsonObject document() const = 0;
    virtual void press(int sliceId, Band band, BandStackDirection d, const BandStackView& v) = 0;
    virtual void step(int sliceId, BandStackDirection d, const BandStackView& v) = 0;
    virtual void goTo(int sliceId, const QString& entryId, const BandStackView& v) = 0;
    virtual void add(int sliceId, const BandStackView& v) = 0;
    virtual void saveHere(int sliceId, const QString& entryId, const BandStackView& v) = 0;
    virtual void edit(const QString& id, const std::optional<QString>& name, std::optional<bool> locked) = 0;
    virtual void remove(const QString& id) = 0;
    virtual void reorder(Band band, const QStringList& ids) = 0;
    virtual void setReturn(Band band, BandStackReturnMode mode, const QString& entryId) = 0;
    virtual void clear(std::optional<Band> band) = 0;
    virtual void reseed() = 0;
    virtual void options(std::optional<bool> dupes, std::optional<int> expiryMinutes, std::optional<int> dwellSeconds) = 0;
    virtual void restoreExpired(const QString& batchId) = 0;
signals:
    void documentChanged();
    void stateChanged();
    void refused(const QString& reason);
    void reported(const QString& text);
};
BandStackAccess* RadioModel::bandStack() const;
```
`LocalBandStackAccess::setInvoker(std::function<void(const SessionMessage&)>)` routes through the server when hosting.

**Acceptance:**
- Reasons exact: `"This Core does not have band stacks yet. Updating the Core adds them."` and `"Connect to a radio to use band stacks."`.
- Remote window on a Core without the capability: `state()` is `NeedsNewerCore`, `press` does nothing and `refused` fires the reason. Band buttons continue through `onBandButtonClicked`.
- Remote sends the verbs of Task 4 with `band` as a key, carrying view values when present. `needsConfirmation` is not a refusal; the existing question path shows it.
- Local without peers: direct controller call, with `refused` on refusal.
- Hosting: band stack verbs are in `kHostVerbs`, go through `HostingSliceActions::run`, and a host press that would move a peer's slice raises the same question and dialog (test through `invokeAsStationDevice`, modeled on `tst_hosting_slice_operations.cpp:412`). A host press with no peer asks nothing. V15: other host tuning is not changed.
- `FakeBandStackAccess` records every call for GUI tests, and can emit `documentChanged` with a set document.

**Verification:** Networking boundary and integration. Commands for `tst_bandstack_link`, `tst_hosting_slice_operations`, `tst_core_has_no_gui_includes`.

**Execution note (advisory):** opus. Prerequisite 5. The models cannot include gui, so the invoker is injected from `MainWindow`.

- [ ] **Step 1:** Implement the interface and both implementations, and the injection.
- [ ] **Step 2:** Test all of the above.

## Task 7: Desktop panel host and retiring the placeholder

**Requirements:** Spec 7 (status bar, panel), V6, section 6.4; acceptance 15.

**Files:**
- Create: `src/gui/bandstack/BandStackPanel.{h,cpp}` (shell)
- Modify: `src/gui/MainWindow.{h,cpp}`, `src/gui/containers` wiring through `ContainerManager`, `src/core/UnbuiltFeatureList.{h,cpp}`, `tests/tst_unbuilt_features.cpp`, `src/gui/setup/GeneralOptionsPage.cpp` (comment at 705-706 and warning at 719, per spec 5.7), `CMakeLists.txt`
- Test: `tests/tst_bandstack_panel.cpp`

**Interfaces:**
- Consumes: `BandStackAccess`, `FakeBandStackAccess`.
- Produces: `BandStackPanel(BandStackAccess* access, QWidget* parent = nullptr)`, `void setSlice(SliceModel*)`, `void setUnavailableReason(const QString&)`, `signal closeRequested()`, `QVBoxLayout* rowsLayout()` for Task 9; `void MainWindow::bandStackPanel()` (toggle), `QString MainWindow::bandStackReason() const`, `void MainWindow::refreshBandStackAvailability()`, `void MainWindow::setBandStackAccessForTest(BandStackAccess*)`.

**Acceptance:**
- `spectrumPane` becomes an HBox of `[BandStackPanel | panColumn(VBox: m_panStack, zoomBar)]`, `spectrumPane` stays `EdgeDockArea`'s `center()`, and `setMinimumWidth(400)` moves to `panColumn`. Do not add a splitter child: `restoreSplitterState` drops saved sizes when the count differs (`ContainerManager.cpp:3149`).
- `m_dockParent` and the resize path point at `panColumn`, so free containers keep their places. Test: place a free container, open and close the panel, and it returns to the same geometry. Saved `MainSplitterSizes` and strip thicknesses are identical with the panel closed and after a round trip.
- Width 158 default, range 120 to 240, opens over 140 ms, instantly when reduced motion is on. Opens right of any left dock strip, left of the pans.
- `statusBandStackDots` is clickable via the `eventFilter` path the ☰ toggle uses (`MainWindow.cpp:17940`). "Band Stacking..." menu toggles it. Keys `BandStackPanel/Open`, `/Width`, `/GroupByBand`, `/ShowOnSpectrum` survive a relaunch and classify `OperatorLocal` in `tst_settings_scope`.
- `UnbuiltFeature::BandStack` is gone (`UnbuiltFeatureList.h:59`, `.cpp:57`, `tst_unbuilt_features.cpp:622-623`, `MainWindow.cpp:13402-13405, 14987, 15748-15749, 16575`, `MainWindow.h:416`). `grep -n "containerBandStackReason" -r src` is empty. `F::Memories` stays. No test asserts the feature hidden.
- Capability changes arrive through `stationLinkStateChanged` (`RadioModel.h:5077`) and `connectionStateChanged`, and `refreshBandStackAvailability` applies them. An older Core or no connection shows the panel greyed with the exact reasons of Task 6 and dots still visible, and the tooltip carries the reason.

**Verification:** UI layout, `ui-verification`. Functional tests plus offscreen captures of the panel open, closed and greyed in the workspace look and the classic look, read by the implementer. Use `setEnabledForTest(true)` (`tst_ampview_window.cpp:115`) and a deferred window (`tst_desktop_station_window.cpp:444`). Commands for `tst_bandstack_panel`, `tst_unbuilt_features`, `tst_settings_scope`. By-eye check of animation cost on real pans is listed for the hardware run.

**Execution note (advisory):** opus. Prerequisite 6.

- [ ] **Step 1:** Rework the pane, add the shell, wire the dots and menu, retire the placeholder.
- [ ] **Step 2:** Tests and captures.

## Task 8: Band press router and flyout

**Requirements:** Spec 5.1, 7 (flyout, container buttons, actions), V3, V7; acceptance 15.

**Files:**
- Modify: `src/gui/MainWindow.{h,cpp}`, `src/gui/SpectrumOverlayPanel.{h,cpp}`, `src/gui/containers/ContainerButtonDispatcher.cpp`, `src/gui/meters/BandButtonItem.{h,cpp}` as needed
- Test: `tests/tst_bandstack_router.cpp`, extend `tests/tst_spectrum_overlay_panel.cpp`

**Interfaces:**
- Consumes: `BandStackAccess`.
- Produces: `void MainWindow::pressBand(SliceModel* slice, Band band, Qt::KeyboardModifiers mods)`, `void bandStackNext()`, `void bandStackPrevious()`, `void bandStackAdd()`, public slots; `void bandContextRequested(const QString& bandKey, const QPoint& globalPos)` on `SpectrumOverlayPanel`; `BandStackView MainWindow::viewForSlice(SliceModel*) const`.

**Acceptance:**
- `pressBand`: without a Ready access, calls `onBandButtonClicked` unchanged. With one: same band plus Ctrl adds; same band plus Shift steps previous; anything else presses next. On Mac, Qt maps Cmd to `ControlModifier`, as spec 5.1 says. Same-band test uses `BandDefaults::bandGridBandFromFrequency`. Modifiers come from `QGuiApplication::keyboardModifiers()` in each slot.
- All paths go through it: Band menu (`MainWindow.cpp:13372, 13391, 13396`, using `activeSliceForWindow()`), flyout (`SpectrumOverlayPanel.cpp:893-896`, `MainWindow.cpp:8096-8102`), band grid (`ContainerButtonDispatcher.cpp:1328`), VFO band screen (`MainWindow.cpp` ~15947). CAT (`CatRxCommands.cpp:1638`) and `slice.selectBand` stay as they are.
- Flyout: remove `hideFlyout()` at :894; outside press still closes (`eventFilter` :572, :660). A test presses twice and the flyout is open, then clicks elsewhere and it closes.
- STACK+ and STACK- catalog actions reach `bandStackNext` and `bandStackPrevious`.
- `bandClickIgnored`'s toast shows refusals through `BandStackAccess::refused`; `reported` text shows in the status bar via `showToast`.
- View arguments come from `sliceForPan`'s `SpectrumWidget`: `centerFrequency()` and `bandwidth()`.

**Verification:** UI interaction with functional checks. `tst_bandstack_router` with the fake asserts verbs, slice and modifiers per path. Commands for it and `tst_spectrum_overlay_panel`, `tst_radio_model_band_click`, `tst_flyout_owns_the_mouse`.

**Execution note (advisory):** opus. Prerequisite 7.

- [ ] **Step 1:** Add the router and route every press site.
- [ ] **Step 2:** Flyout, STACK buttons, entry points, then tests.

## Task 9: Rows, menus, keyboard, drag

**Requirements:** Spec 7 (panel, row, band and panel menus), V6, V7; acceptance 15.

**Files:**
- Create: `src/gui/bandstack/BandStackRowList.{h,cpp}`
- Modify: `src/gui/bandstack/BandStackPanel.{h,cpp}`
- Test: extend `tests/tst_bandstack_panel.cpp`

**Interfaces:**
- Consumes: `BandStackAccess`, Task 7 panel.
- Produces: `BandStackRowList(BandStackAccess*, QWidget* parent = nullptr)`, `void setSliceId(int)`, `void setBand(std::optional<Band>)` (nullopt shows all with grouping), `signal bandContextRequested(Band, QPoint)`.

**Acceptance:**
- Rows match `desktop.html`'s `.en` (frequency, mode, filter, name, lock, flags of slices on the spot, band plan tint at the mockup's opacity), grouped with the current band open and others showing counts and lock counts. Open and closed bands follow the mockup's rule: the current band opens when it changes; a band you collapsed stays collapsed until the band changes.
- Row click calls `goTo`; double-click name renames inline and calls `edit`; drag within a band calls `reorder` with all ids on drop, using plain mouse tracking (no drag-list precedent exists in the repo; the mockup's pointer capture is the model).
- F2, Cmd or Ctrl+U, Delete, arrows, Enter, Esc per spec, while the panel has focus.
- Row menu items and order match spec 7, with Save disabled when locked or off the band, reason as tooltip (`setToolTipsVisible(true)`).
- Band menu and panel menu items match spec 7. Clear and Reset ask with the mockup's words: `"Clear the unlocked entries on %1?"`, `"Clear every unlocked entry on every band? Locked entries stay."`, `"Replace the unlocked entries with the defaults for %1? Your locked entries stay."`.
- Header: "BAND STACK", the slice flag, "on 40m", "...". "+ Add current spot" calls `add`.
- With an expiry note in the document, shows `"Removed N expired entries"` with Undo for 5 seconds, which calls `restoreExpired`.
- Listener and foreign-slice states: rows read, recall refused through `refused`.
- Styling via `WorkspacePreview::lookQss`, `roleColourName`, `statusColourName`, `kPopupMenu`; works in the classic look.

**Verification:** UI. Functional tests with the fake assert the verb for each action. Captures in both looks of an open band, menus, rename, a locked row and a drag in progress, read against `desktop.html`. Command for `tst_bandstack_panel`.

**Execution note (advisory):** opus. Prerequisite 7. Could run in a worktree parallel with Tasks 8 and 11, because the files do not overlap, except `CMakeLists.txt`.

- [ ] **Step 1:** Rows and grouping.
- [ ] **Step 2:** Menus, rename, keyboard, drag, notes, tests and captures.

## Task 10: Popup

**Requirements:** Spec 7 (popup), section 3 (right-click); acceptance 15.

**Files:**
- Create: `src/gui/bandstack/BandStackPopup.{h,cpp}`
- Modify: `src/gui/MainWindow.{h,cpp}`, `src/gui/SpectrumOverlayPanel.{h,cpp}`
- Test: extend `tests/tst_bandstack_router.cpp`

**Interfaces:**
- Consumes: `BandStackRowList`, `bandContextRequested`.
- Produces: `void BandStackPopup::showFor(SliceModel*, Band, const QPoint& globalPos)`, `void MainWindow::openBandStackPopup(SliceModel*, Band, const QPoint&)`.

**Acceptance:**
- Right-click on a flyout button (add `CustomContextMenu`), and a container band button (`ContainerWidget::bandStackRequested`), open it for that band. WWV's container button emits nothing today (`BandButtonItem.cpp` ~379): add that. VFO long press (`VfoDisplayItem.cpp:1190, 1231, 1254`) emits band 0: resolve the band from the slice's frequency.
- Same rows and menus as the panel, at the pointer, kept on screen, closes on an outside click. Uses `Qt::Popup` as `SliceChooser` does (`MainWindow.cpp:3851`). A row menu opened from it does not close it.
- Older Core or not connected: shows the reason instead of rows.

**Verification:** UI with functional checks: signal to `openBandStackPopup` with the right band on each path. A capture read against `desktop.html`. Command for `tst_bandstack_router`.

**Execution note (advisory):** opus. Prerequisites 8, 9.

- [ ] **Step 1:** Popup and the three entry points. Then tests and a capture.

## Task 11: Spectrum overlay

**Requirements:** Spec 7 (overlay), V6, V11, V17; acceptance 15.

**Files:**
- Modify: `src/gui/SpectrumWidget.{h,cpp}`, `src/gui/MainWindow.{h,cpp}`
- Test: `tests/tst_bandstack_overlay.cpp`

**Interfaces:**
- Consumes: `BandStackAccess::document()`, `sliceForPan`.
- Produces: `struct BandStackTick { QString entryId; double hz; QString label; QString tip; QColor colour; bool locked; bool active; };`, `void SpectrumWidget::setBandStackTicks(const QVector<BandStackTick>&)`, `void SpectrumWidget::setBandStackTicksHidden(bool)`, signals `bandStackTickClicked(const QString&)` and `bandStackTickContext(const QString&, const QPoint&)`.

**Acceptance:**
- Ticks draw in both `renderGpuFrame`'s static bake (near `drawSpotMarkers`, ~16936) and the CPU `paintEvent` path (~7931). No shader change. `setBandStackTicks` compares, then `markOverlayDirty()`; an equal push does not dirty the overlay (`overlayStaticDirtyForTest`, `SpectrumWidget.h:2996`). Moving the range redraws.
- Click within 4 px in the spectrum area recalls via `goTo` with the pan's active slice (`sliceForPan`); 5 px tunes as today. Other clicks are unchanged (`SpectrumWidget.cpp:15331-15366`). A slice's marker line wins over a tick under it (`:14545-14562`), V17.
- Hover shows "frequency, mode and filter, N of M, name". Right-click calls the entry menu.
- Hidden while `RadioModel::transmittingChanged` says on air, pushed to every pan.
- Off by default; on from "Show on the spectrum" in the panel menu, which persists per window. Older Core: no ticks.
- Colors match `desktop.html`'s `draw`. Saved, active and locked are distinct.

**Verification:** UI. Tests at the widget level using `drawForeignMarkersForTest`'s pattern. `NEREUS_SKIP_WITHOUT_QRHI` skips QRhi offscreen, so the GPU bake is checked by eye on a native build; list it in the hardware run. Command for `tst_bandstack_overlay`.

**Execution note (advisory):** opus. Prerequisite 8. Could run parallel with Task 9.

- [ ] **Step 1:** Ticks, hit test and hover, on-air hide, the switch. Then tests and a capture.

## Task 12: Stack number, pan rule and region prompt on desktop

**Requirements:** Spec 5.5 step 5, V1, V14, section 7 (status bar); acceptance 15, 17-19.

**Files:**
- Modify: `src/gui/MainWindow.{h,cpp}`, `src/gui/chrome/ChromeBarItems.cpp` (:104), `src/gui/setup/GeneralOptionsPage.cpp`
- Test: extend `tests/tst_bandstack_router.cpp`

**Interfaces:**
- Consumes: `BandStackAccess::document()` `lastRecall` and `region`.
- Produces: nothing other tasks need.

**Acceptance:**
- Status bar reads `"40m 5/6"` for the selected slice, `"40m -/6"` with no place, and disabled when unavailable. It folds with the status bar's existing rungs and does not break chrome width tests.
- Window controlling `lastRecall.sliceId` applies the view once per `seq` under `m_handlingBandJump`: zoom when `applyView` and `spanHz` nonzero; center via `requestStreamCentre` when click tune is on. Other windows do nothing. With the frequency off screen, centers on it. Test with a fake document on a deferred window.
- `Remote` windows apply the same rule via `followRemoteSliceFrequency`'s neighbors.
- Region: `GeneralOptionsPage` marks an edit; `MainWindow` shows, when the document's `region` next changes within 65 seconds, `"You changed the region to %1. Replace the unlocked entries with the defaults for %1? Your locked entries stay."` with **Keep my entries** (default) and **Replace**. Replace calls `reseed`. No prompt when the change came from another device, or on an older Core.

**Verification:** UI with functional checks. Command for `tst_bandstack_router`. Captures of the number and the prompt, read.

**Execution note (advisory):** opus. Prerequisites 8, 11.

- [ ] **Step 1:** Number, then pan rule, then region prompt, each with its test.

## Task 13: iOS document and link

**Requirements:** Spec 6.2, 6.3, 6.4; acceptance 9.

**Files:**
- Create: `ios/NereusKit/Sources/NereusMirror/BandStack.swift`, FakeStation band stack extension beside `FakeStation+CoreAddresses.swift`
- Modify: `ios/NereusKit/Sources/NereusLink/LinkFeatures.swift` (near :270), `ios/NereusKit/Sources/NereusKitTesting/FakeStation.swift` (`Additions` bit `1 << 27`, not in `.all`; serve verbs in the :1039 switch), `LinkConformanceSessionTests.swift:185`, `LinkSurfaceOrdinalTests.swift` (count 120 to 121), `DropSheetShotTests.swift:57-70`, `IPadShotTests.swift:379-388` as needed
- Test: `BandStackTests` in NereusKit, `FakeStationAdditionsTests`

**Interfaces:**
- Consumes: the wire contract of Task 4.
- Produces:
```swift
public struct BandStack: Equatable, Sendable {
    public static let objectKey = "bandStack"
    public static let capability = "bandStackVersion"
    public struct Entry: Equatable, Sendable, Identifiable { public let id: String; public let band: String; public let frequencyHz: Int64; public let mode: String; public let filterLowHz: Int; public let filterHighHz: Int; public let centerHz: Int64; public let spanHz: Int64; public let locked: Bool; public let seeded: Bool; public let name: String; public let lastUsedMs: Int64 }
    public enum ReturnMode: String, Sendable { case current, lastUsed, specific }
    public struct LastRecall: Equatable, Sendable { public let seq: Int; public let sliceId: Int; public let applyView: Bool; public let centerHz: Int64; public let spanHz: Int64 }
    public let revision: Int64
    public let region: String
    public let entries: [Entry]
    public let dupeProtection: Bool
    public let expiryMinutes: Int
    public let dwellSeconds: Int
    public let lastExpiryBatchId: String
    public let lastExpiryCount: Int
    public let lastRecall: LastRecall
    public func entries(band: String) -> [Entry]   // in order
    public func place(slice: String, band: String) -> String?
    public func returnMode(band: String) -> ReturnMode
    public func specificId(band: String) -> String?
    public init?(json: String, revision: Int64)
}
extension MirrorStore { public var bandStack: BandStack? { get } }
```

**Acceptance:**
- A sample document generated by the C++ facade decodes (add a fixture file shared with the Task 4 test). Malformed JSON or a bad revision decodes to nil. Unknown keys are ignored. `objectKeys` order does not matter.
- `LinkFeatures` declares `"bandStack": 1`; the exact-dictionary pin is updated.
- `python3 scripts/verify-ios-provenance.py` passes.

**Verification:** Unit. `ios/scripts/swift-test.sh --jobs 2 --filter BandStack`. Ordinals: `--filter LinkSurfaceOrdinal`.

**Execution note (advisory):** opus. Prerequisite 4. Could run parallel with the desktop tasks.

- [ ] **Step 1:** Decoder and fake, then the pins and tests.

## Task 14: iOS model

**Requirements:** Spec 5.1, 6.4, 8, 11; acceptance 16.

**Files:**
- Create: `ios/NereusApp/Band/BandStackModel.swift`
- Modify: `ios/NereusApp/Main/MainScreenModel.swift` (one per pan, in `PanControls`, :445, :472)
- Test: `ios/NereusApp/Tests/BandStackModelTests.swift`

**Interfaces:**
- Consumes: Task 13.
- Produces:
```swift
@MainActor final class BandStackModel: ObservableObject {
    enum Availability: Equatable { case ready, needsNewerCore, notConnected }
    enum Direction: String { case next, previous }
    @Published private(set) var availability: Availability
    @Published private(set) var stack: BandStack?
    @Published var note: String?
    init(store: MirrorStore, slices: BandSlicesModel, commands: CommandClient?)
    func press(bandKey: String, _ direction: Direction)
    func step(_ direction: Direction)
    func goTo(_ entryId: String)
    func add()
    func saveHere(_ entryId: String)
    func edit(_ entryId: String, name: String?, locked: Bool?)
    func delete(_ entryId: String)
    func reorder(bandKey: String, ids: [String])
    func setReturn(bandKey: String, mode: BandStack.ReturnMode, entryId: String?)
    func clear(bandKey: String?)
    func reseed()
    func options(dupeProtection: Bool?, expiryMinutes: Int?, dwellSeconds: Int?)
    func restoreExpired(batchId: String)
}
```

**Acceptance:**
- Verbs and arguments match Task 4 exactly, including `viewCenterHz` and `viewSpanHz` from the pan.
- `needsConfirmation` sets no note (guard with `SeveralDevices.waitsForConfirmation`, `SeveralDevices.swift:49`). The sheet appears through `ConfirmationLayer`; Cancel changes nothing.
- A refusal's words appear unchanged in `note`.
- Availability strings: `"This Core does not have band stacks yet. Updating the Core adds them."`, `"Connect to a radio to use band stacks."`.
- `lastRecall` for this phone's slice applies its view once per `seq`; others ignored.

**Verification:** Unit with the fake station. Build-for-testing on a temporary device, then `ios/scripts/sim-test.sh NereusBS-iPhone "$DD" -only-testing:NereusSDRTests/BandStackModelTests`.

**Execution note (advisory):** opus. Prerequisite 13.

- [ ] **Step 1:** Model and tests.

## Task 15: iOS stack block and BAND menu

**Requirements:** Spec 8 (where, controls, older Core); acceptance 16.

**Files:**
- Create: `ios/NereusApp/Main/BandStackBlock.swift`
- Modify: `PanSheet.swift` (after :39-42), `BandSelectGrid.swift`, `PanSheetModel.swift` (`selectBand` :168-177), `Band/BandButtonColumn.swift` (:777-779, `BandColumnLayout`, `width(of:)` :76)
- Test: `ios/NereusApp/Tests/BandStackModelTests.swift`, `BandButtonColumnTests`, `ScrollViewIdentifierTests`

**Interfaces:**
- Consumes: `BandStackModel`.
- Produces: `struct BandStackBlock: View { init(model: BandStackModel, width: CGFloat) }`.

**Acceptance:**
- With the capability: a lit band's tap sends `press` next; another band's tap sends `press` next, which goes to where its return mode says; without it, `selectBand` as today. Old-Core BAND menu still closes on a press.
- Block matches `phone-and-ipad.html`: "40m ▾" picker, "6 of 6", ‹ ›, "...", rows, "+ Add this spot", "Reorder". Arrows and add are disabled while viewing another band, with the reason.
- Half-second hold on a band opens its list and sends no command, via `.simultaneousGesture(LongPressGesture(minimumDuration: 0.5))` with a flag swallowing the tap; "40m ▾" is the visible way to the same list. This gesture pairing is unproven on `PanelButton`, so a UI test must exercise it.
- Row "..." menu items match spec 8. Reorder is a mode with grips only then, sends every id on Done.
- The BAND menu stays open on a press with the capability: remove `{ open = nil }` at :778 in that case. Two panels side by side at `column == 2`, one stacked on iPad. The menu's bigger size keeps the PTT clear (check `reportsBandColumnFrame`).
- The Pan sheet keeps the block under the grid on every device, per the mockup, and scrolls in `DropSheet`.
- Older Core: grid works, greyed "Needs a newer Core" row with the reason.
- Tint comes from the same band plan `BandModel` holds.

**Verification:** UI. Tests plus shots of each state read in Task 19. `sim-test.sh` for the named suites.

**Execution note (advisory):** opus. Prerequisite 14.

- [ ] **Step 1:** Block, then Pan sheet, then BAND menu.

## Task 16: iPad applet

**Requirements:** Spec 8 (iPad), section 3; acceptance 16.

**Files:**
- Create: `ios/NereusApp/iPad/BandStackApplet.swift`
- Modify: `ios/NereusApp/Roundtable/RoundtableApplet.swift` (`AppletKind`, `AppletOrder`, :17, :27, `lastPosition`), `AppletColumn.swift:142`, `FrontPanelColumns.swift`, `RoundtableAppletTests`
- Test: `IPadShotTests`, `RoundtableAppletTests`

**Interfaces:**
- Consumes: `BandStackBlock`, `BandStackModel`.
- Produces: `struct BandStackApplet: View`.

**Acceptance:**
- A Band Stack applet appears in the iPad's column with a title like `RoundtableAppletTitle` (:111), showing the block for the pan's slice. In upright layout it sits under the S-meter in the first column, per the mockup (277 wide).
- Order pins updated. An older Core shows it greyed with the reason, not gone.

**Verification:** UI. Shot and pin tests on `NereusBS-iPad`. CI targets iPhone only, so this runs locally.

**Execution note (advisory):** opus. Prerequisite 15.

- [ ] **Step 1:** Applet, then tests.

## Task 17: iOS markers and switch

**Requirements:** Spec 8 (spectrum), V6, V18; acceptance 16.

**Files:**
- Create: `ios/NereusApp/Band/BandStackMarkerLayer.swift`
- Modify: `BandGestureLayer.swift:296`, `ios/NereusApp/App/PhoneSettings.swift` (`:43, :56, :60`), `DisplayPillRows.swift:208`, `DisplayPill.swift:445`, `DisplaySheet.swift:108`
- Test: `DisplayStripTests` or a new `BandStackMarkerTests`

**Interfaces:**
- Consumes: `BandStackModel`.
- Produces: `PhoneSettings.showBandStackMarkers: Bool`, default false.

**Acceptance:**
- Chips at the spectrum foot for entries in view, in `BandLayout.spectrum`'s foot, tap sends `goTo`. Active and locked are visible.
- Switch appears in the pill's "On this phone" and the sheet's "On the band". Defaults off, survives relaunch, per device.
- Hidden while on air. An older Core shows nothing, and the switch is disabled with the reason.

**Verification:** UI and unit. `sim-test.sh` for the suite.

**Execution note (advisory):** opus. Prerequisite 15.

- [ ] **Step 1:** Layer, switch, tests.

## Task 18: iOS Setup and region prompt

**Requirements:** Spec 8 (Setup), V1, V18; acceptance 16.

**Files:**
- Create: `ios/NereusApp/Setup/BandStackSetupPage.swift`
- Modify: `ios/NereusApp/Setup/SetupTree.swift` (`Page` :22, `Skeleton` :116+), `ios/NereusApp/Setup/SetupTab.swift:195-210`, `ios/NereusApp/Setup/DescribedControl.swift` (:674, `onLateOutcome`), `MainScreenModel.swift` if the prompt lives there
- Test: `BandStackSetupTests`

**Interfaces:**
- Consumes: `BandStackModel`.
- Produces: `struct BandStackSetupPage: View`.

**Acceptance:**
- Native page, as `RotorSetupPage.swift` (`SetupTagBadge(tag: .core)` :20-23), under General after Options, with Auto-save, Auto-expiry, duplicate protection, Reset to region defaults and Clear unlocked entries. Older Core: shown greyed with the reason, not gone.
- Region prompt per spec V1: marks an edit of control `general.options.region`; on the document's `region` next changing within 65 seconds, asks with the desktop's exact words, **Keep my entries** default. Replace sends `reseed`. No prompt on a refused edit, an older Core or a change from another device.

**Verification:** UI and unit. `sim-test.sh` for the suite.

**Execution note (advisory):** opus. Prerequisite 14.

- [ ] **Step 1:** Page, prompt, tests.

## Task 19: iOS shots and UI tests

**Requirements:** Spec 8, 12; acceptance 16.

**Files:**
- Create: `ios/NereusApp/Tests/BandStackShotTests.swift`, `ios/NereusApp/UITests/BandStackUITests.swift`
- Modify: `ios/NereusApp/Shared/UITestBand.swift` (:42, :104, :306; add launch arg `-NereusBandStackFixture`)
- Test: these

**Interfaces:**
- Consumes: Tasks 15-18.
- Produces: nothing.

**Acceptance:**
- Shots written to `TEST_RUNNER_NEREUS_MAIN_SHOTS` and `TEST_RUNNER_NEREUS_IPAD_SHOTS` show: Pan sheet block, BAND menu upright and sideways, iPad applet both layouts, markers, Setup page, Reorder mode, older-Core row. Implementer reads each against `phone-and-ipad.html` and notes differences.
- UI tests: lit tap echoes `press`; hold opens the list with no command; Reorder sends all ids; menu stays open. Launch as `IPadUITests.swift:131`.

**Verification:** UI gate per `ui-verification`, run on temporary devices with the lock held, per `ios/README.md`.

**Execution note (advisory):** opus. Prerequisites 15-18.

- [ ] **Step 1:** Fixture, shots, UI tests. Run on `NereusBS-iPhone` and `NereusBS-iPad`, delete them, release the lock.

## Task 20: Final checks and hardware hand-over

**Requirements:** Spec 12; acceptance 14, 17-21.

**Files:**
- Modify: `docs/architecture/2026-11-05-band-stack-design.md` (status line, any correction the build proved necessary)
- Create: `docs/architecture/2026-11-05-band-stack-hardware-run.md`
- Test: none new

**Interfaces:**
- Consumes: all.
- Produces: the hardware run list.

**Acceptance:**
- All pass on the finished revision: `tests_core`, `tests_models` and `tests_gui` label groups, and the `session` extra label if it has a target, per `docs/development/fast-test-loop.md`, built with the matching `tests_<label>` target first; `tst_core_has_no_gui_includes`; `python3 scripts/verify-thetis-headers.py`; `python3 scripts/verify-inline-tag-preservation.py`; `python3 scripts/render-link-tables.py --check`; `python3 scripts/verify-ios-provenance.py`; `ios/scripts/swift-test.sh --jobs 2`.
- `git grep -n "containerBandStackReason\|UnbuiltFeature::BandStack"` is empty.
- The hardware run list has spec items 17-21 as scenarios, plus: animation cost on real pans, overlay in GPU rendering, right-click on real container buttons, and a hosted desktop's question with a real phone. Each states what to do and what you should see, in plain words. All are marked pending until J.J. has seen them.

**Verification:** Combined checks. Hardware pending.

**Execution note (advisory):** sonnet for the docs, opus for any failure found. Prerequisite: all.

- [ ] **Step 1:** Run the combined checks and fix what they find.
- [ ] **Step 2:** Write the hardware run list.
