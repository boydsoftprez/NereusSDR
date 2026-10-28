# The Phone's Display Controls, as on the Desktop: Implementation Plan

> **Execution:** run with `crew` (`/crew <this file>`) under `cost-aware-execution`.
> Requirements and acceptance cases are binding; test order and review effort follow
> the risk-based policy; UI evidence follows `ui-verification`. No review between
> tasks; one whole-branch review at the end.

**Goal:** The iPhone and iPad get the desktop's Display controls: the pill's five tabs
with its rows, per-pan values with Reset this pan and Setup defaults, dB Max and dB Min
per band shared through the Core, the band menu on a press and hold, the button column
and pill on the iPad and the sideways iPhone, more than one pan on the iPad up to the
radio's limit, and Setup > Display as the desktop's eight pages with the "Settings for"
strip.

**Architecture:** One `DisplayPill` view draws the five tabs from tables of setting
keys in the desktop pill's order, each row drawn with the same `DescribedControl` Setup
uses, and sits in three containers (the upright drop sheet, the sideways pill, the
iPad pill). Per-pan values stay in the existing `BandDisplaySettingsStore`, now keyed
by a `PanIdentity` that maps the phone's store key (`"1"`, `"2"`) to the Core's pan
key (`"pan-0"`, `"pan-1"`). The band's scale moves from the phone's `bandScales` to
the Core's `DisplayGridMax_<band>` and `DisplayGridMin_<band>` through the settings
proxy `CoreDisplayModel` already uses. The iPad stacks one band (model, slices,
subscriber) per open pan, sharing one display allocator.

**Tech Stack:** Swift 6 language mode, SwiftUI (iOS 17.4 deployment target,
`ios/project.yml:11-12`), `NereusKit` (Swift package: `NereusBand`, `NereusMedia`,
`NereusMirror`, `NereusModels`, `NereusKitTesting`), Swift Testing (`import Testing`)
for unit and shot tests, XCTest for `NereusSDRUITests`, XcodeGen, Python 3 for the
repository checks. The desktop and Core sources under `src/` are read, never changed.

**Spec:** [2026-10-22-phone-display-controls-design.md](2026-10-22-phone-display-controls-design.md)
(approved by JJ 2026-10-24, "all sugestions look good onward"). Every decision is
settled there; this is how it gets built. Board:
[board.html](2026-09-23-iphone-app-design/board.html), the sections headed "Display, as
on the desktop". `DC`, `R-IOS` and section numbers below are the spec's.

## Global Constraints

- Read `CLAUDE.md`, `ios/README.md` and the spec first. Nothing under `src/` changes:
  the desktop and the Core are the reference, never a target (spec section 13).
- Every new Swift file starts with the two header lines the iOS provenance check
  requires (`ios/README.md:24-33`):
  `// NereusSDR for iOS: <what this file is>` then
  `// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission`.
- A value or string taken from the desktop carries its `src/` file:line in a comment
  on its line. User-facing strings carry no cites
  (`feedback_no_cites_in_user_strings`); the cite goes in the comment above.
- Colors and treatments only from `ios/Shared/WorkspacePalette.swift` and
  `ios/Shared/WorkspaceStyles.swift` (PR 603): the pill and column in
  `workspacePanel`, column buttons `WorkspaceButtonStyle(.raised)`, the open button
  and the open tab `.glass`, CTUN and DUP on `.litGreen`, the own-value dot
  `WorkspacePalette.amberText`, Reset this pan `.raised`. Radii 7 for buttons and chips
  (`WorkspacePalette.chipRadius`), 10 for the pill, the column frame and menus
  (`WorkspacePalette.panelRadius`). Red only for transmit (`txRed`). No new hex
  literal in any view; `python3 scripts/verify-ios-workspace-colours.py` stays green.
- Disabled, never hidden: a control that cannot act stays in place at
  `WorkspacePalette.disabledOpacity` (0.4) with its reason shown under or beside it.
  Gestures are extra ways in, never the only way: every band-menu item is also
  reachable from the pill, the column or the Pan sheet.
- User-facing words are the spec's and the desktop's, exactly as written there. Where
  this plan quotes a string, that string is used.
- Mac build lanes (JJ, 2026-10-22 and 2026-10-23): at most two agents configure,
  compile or test on this Mac at a time. Write and self-review all code first, then
  take a lane: `for L in build-lane.lock build-lane-2.lock; do mkdir /Users/j.j.boyd/.config/nereus/work/$L 2>/dev/null && echo "<task> $(date +%T)" > /Users/j.j.boyd/.config/nereus/work/$L/owner && break; done`.
  Hold one lane for one build-and-test cycle, `nice -n 20`, and release it
  (`rm -rf` only the lock you hold) the moment the last command ends. Rules:
  `/Users/j.j.boyd/.config/nereus/work/pr-review-20261021/build-lane-lock.md`.
- Inside a held lane, the iOS lock too (`ios/README.md:88-94`): `mkdir
  /tmp/nereus-ios-full.lock`, record your own token, release only your own lock in an
  exit trap, one simulator at a time, `-jobs 2`, reuse derived data.
- Test commands (from `ios/README.md:62-78`), with `DD` a derived-data folder in the
  scratchpad reused across tasks:
  - `ios/scripts/generate-project.sh`
  - `nice -n 20 xcodebuild -project ios/NereusSDR.xcodeproj -scheme NereusSDR -destination 'platform=iOS Simulator,name=iPhone 17' -derivedDataPath "$DD" -jobs 2 build-for-testing`
  - `ios/scripts/sim-ready.sh "iPhone 17" "$DD/Build/Products/Debug-iphonesimulator/NereusSDR.app"`
  - `nice -n 20 xcodebuild -project ios/NereusSDR.xcodeproj -scheme NereusSDR -destination 'platform=iOS Simulator,name=iPhone 17' -derivedDataPath "$DD" -collect-test-diagnostics never -parallel-testing-enabled NO -jobs 2 test-without-building -only-testing:NereusSDRTests/<Suite>`
  - iPad runs use `name=iPad Pro 11-inch (M5)`; UI tests use
    `-only-testing:NereusSDRUITests/<Class>/<method>`.
  - A Swift Testing case is named with its parentheses
    (`-only-testing:'NereusSDRTests/<Suite>/<case>()'`); without them xcodebuild runs
    nothing and reports success.
  - Shot tests write PNGs only when their variable names a folder, passed with the
    `TEST_RUNNER_` prefix (`TEST_RUNNER_NEREUS_MAIN_SHOTS=<scratch dir>`,
    `TEST_RUNNER_NEREUS_IPAD_SHOTS=<scratch dir>`). Shot folders are in the
    scratchpad, never in the repository. There is no pixel gate: the pictures are
    compared by eye with the board.
  - A change under `ios/NereusKit` also runs
    `nice -n 20 ios/scripts/swift-test.sh --jobs 2 --filter <Suite>` and the affected
    app suites on a simulator.
  - Repository checks: `python3 scripts/verify-ios-provenance.py`,
    `python3 scripts/verify-ios-workspace-colours.py` and
    `python3 -m pytest tests/compliance/ -v`.
- No implementer connects to a radio or a Core, installs on a device
  (`ios/scripts/device-install.sh` is the controller's or JJ's), or touches
  `~/Applications/NereusSDR Dev.app` or JJ's settings. Real-Core and device steps are
  the controller's, each with JJ's yes.
- A test that pins today's behavior this plan changes (Top and Range, the "On this
  phone" page, the two-chip scope header) is rewritten to the new behavior and named
  in the task's report; no valid test is weakened or deleted to get green.
- Commits: GPG-signed (never `--no-gpg-sign`; a signing failure is reported, not worked
  around). No `Co-Authored-By` trailer. No push, no PR, no public post.
- Line numbers are from `origin/main` `b85e4127d`; earlier tasks move them, so find the
  code by the names given.

## What already exists

- **The sheet:** `OpenSheet {pan, display, slices}` (`ios/NereusApp/Main/DropSheet.swift:7`),
  `DropSheet` (`:17-62`, sideways width 420 at `:24`), `SheetCaption`, `SheetNote`,
  `SheetGrid` (`:74`, `:91`, `:109`). Opened by `Toolbar.swift:274`
  (`sheetButton(.display…)`), bound to `MainScreen.sheet` (`MainScreen.swift:53`),
  drawn in the overlay at `MainScreen.swift:230-243`; `dropped(.display)`
  (`:293-302`) builds `DisplaySheet(model: main.display, core: main.coreDisplay){ openSetup() }`,
  and `openSetup` is `setupRouter.open(.displayOnThisPhone)` (`App/RootView.swift:116-118`).
  `DisplaySheet.swift`: title `:23-24`, rows `:30-137`, `fft` `:144-194`, Top and
  Range `:78-82`, DUP `:83-95`, CTUN `:113-119`, Look back `:120-132`, the "More
  display options in Setup ›" link `:254-270`.
- **The sheet's model:** `DisplaySheetModel` (`Main/DisplaySheetModel.swift`), scale
  writes `:370-424` (`dragScale` `:375`, `writeScale` `:411-424`, which writes
  `txGridTopDbm` / `txGridRangeDb` while keyed) and `:528-578` (`arrowStepDb` 10
  `:532`, `nudgeTop` `:542`, `setTop` `:562`, `setRange` `:568`).
  `Band/DbmScaleArrows.swift:28-44` calls `nudgeTop` / `nudgeFloor`.
- **Per-pan scope:** `MainScreenModel` (`Main/MainScreenModel.swift`): `panNumber = 1`
  `:20`, `DisplayEditScope { pan, all }` `:29-30`, `displayBandChoice` `:33`,
  `displayEditingSettings` `:37-42`, `ownDisplayFields` `:43`, `resetDisplayPicture`
  `:46`, `copyOwnDisplayPictureToMaster` `:51`, the band wiring `:165-171`,
  `keepSettings` `:189-193`, `focusPan` `:300`, `changeDisplay(selectedFields:)`
  `:341-369`.
- **The store:** `BandDisplaySettingsStore`
  (`ios/NereusKit/Sources/NereusBand/BandDisplaySettingsStore.swift`): keys `:15`,
  `:24-27` (`phone.display.master.v29`, `phone.band.display.<panId>`), `sharedFields`
  `:39`, `pictureFields` `:50` (excludes `rxViewSpanHz`), `settings(forPan:)` `:188`,
  `setSettings(_:forPan:includingPicture:)` `:277`, `masterSettings(forPan:)` `:315`,
  `setMasterSettings(_:fields:)` `:321`, `ownFields(forPan:among:) -> Set<String>`
  `:364`, `resetPicture(forPan:fields:)` `:370`, `copyOwnPictureToMaster(forPan:fields:)`
  `:379`, `reset(pan:)` `:405` (no callers). `BandDisplaySettings.swift`: blanking
  `:164-165`, scale and `bandScales` `:173-184`, `rxViewSpanHz` `:372`, `enterBand`
  `:463`, `keepBandScale` `:509`. `BandDisplayOptions.resetKeepingPlace` `:242-258`,
  called only from "Reset to defaults" on "On this phone"
  (`Setup/DisplayOnThisPhonePage+More.swift:307`).
- **Setup:** `SetupTree.swift`: Display skeleton `:134-138`, version 29 order with
  "On this phone" first `:238-239`, `withStackPage` `:174-194` (the View page's id is
  `display.threeD`, `stackPageId` `:167`), `route(toDescribed:)` `:288-293`, the
  Appearance category `:142`. `SetupRouter.swift`: `open(_ page:)` `:20`,
  `open(describedPage:) -> Bool` `:27-31`. `DescribedPage.swift`:
  `DisplayV29ScopeHeader` `:120-163`, `DisplayV29BandHeader` `:167-189`,
  `DisplayV29Control` `:192-236` (the own-value dot in `accentFill` at `:222`), header
  gating `:38-42`, rows `:54-81`. `DescribedControl.swift`: state from
  `dispatcher.state(of:)` `:96`, reasons `:114-119`, row kinds `:157-176`. Gray-out
  logic: `NereusKit/Sources/NereusMirror/SetupControlDispatcher.swift` (`state`
  `:145-162`, `unavailableReason` `:442`, phone keys `:566-580`). PR 603's containers
  in `SetupPageContainer.swift`: `SetupPageContainer` `:17`, `SetupPanelSection` `:55`,
  `SetupMenuRow` `:426`, `SetupStepperRow` `:608`, `SetupSlider` `:670`
  (`onEditingChanged` `:682`); `SetupSplitView` (`SetupSplitView.swift:18`).
  `DisplayOnThisPhonePage.swift` body `:168-186`, title "Display" `:195`, its
  "Waterfall" group `:264` (no NB Blank). `PhoneSetupKeys.swift`:
  `value(forPhoneKey:)` `:78-178` (the description's `DisplayGridMax` and
  `DisplayGridMin` phone bindings read `scaleTopDbm` / `scaleBottomDbm` at
  `:136-139`), `set` `:181`, `displayFields` / `pictureFields` `:533-551`,
  `displayFieldNames` `:552-653`, `receiveLevelsKeys` and the transmit reason
  `:688-705`, `unavailableReason` `:707`.
- **The description:** version 29 is frozen in
  `NereusKit/Sources/NereusMirror/SetupDescription+V29.swift` and served from
  `resources/setup/display.json`. Its Waterfall page `display.waterfallDefaults` lists
  `display.waterfall.blanking` and `display.waterfall.blankingThreshold` (the
  threshold `enabledWhen {phone: WaterfallBlankingEnabled, oneOf:[true]}`);
  `legacy/display-v28.json` has neither. dB Max and dB Min are `binding:{phone:
  "DisplayGridMax"/"DisplayGridMin"}`, owner `station`, `perBand`.
  `SetupDescription.bandName(_:)` (`NereusMirror/SetupDescription.swift:273-287`)
  names bands 0 to 13 and 27 and returns nil for 14 to 26.
- **The Core's settings:** `CoreDisplayModel` (`Main/CoreDisplayModel.swift`):
  `set(_:to:)` `:274` → `write` `:504` → `startWrite` `:531` →
  `settingsProxy.writeBound(...)`; `isUnconfirmed(_:)` `:208`; refusals land in one
  model-wide `@Published note` (`:109`, `receive` `:607-617`). `SettingsProxyClient`
  (`NereusMirror/SettingsProxyClient.swift`): `values` `:37`, `unconfirmedKeys` `:44`,
  `value(_:)` `:112`, `isUnconfirmed` `:121`, `write` / `writeBound` `:187` / `:195`.
  Station scope for `DisplayGridMax_*`: `NereusMirror/SettingsScope.swift:84-87`.
  Desktop key: `"DisplayGridMax_" + bandKeyName(b)` (`src/models/PanadapterModel.cpp:116`),
  with `bandKeyName` the band label (`src/models/Band.cpp:216-220`); band numbers on
  the link are the desktop's `Band` values, 14 to 26 the short-wave broadcast bands
  (`docs/architecture/2026-09-23-station-link-v1.md:1107-1111`, `src/models/Band.h:129-142`).
- **Pans:** `BandSubscriber.panId = "1"` (`Main/BandSubscriber.swift:61`), builds a
  `DisplaySubscription` (`:515-555`) sent by `MediaControlClient.subscribe(_:)`
  (`NereusMedia/MediaControlClient.swift:2179`). The wire `subscribe` carries no pan
  key: endpoints are keyed by `sliceId` (`NereusMedia/DisplaySubscription.swift:7`;
  link `:7138`; `maxDisplayEndpoints` 8 at `:7578`). Each `BandSubscriber` owns its own
  `DisplayQualityAllocator` (`:102`) and sends one intent (`:321-334`);
  `DisplayQualityAllocator` (`NereusMedia/DisplayQualityAllocator.swift:26`),
  `Intent(panId:pixels:fps:includesWidePlane:waterfallPeriodMs:active:extras:)`
  `:92-115`, `update(intents:)` `:212`, `allocate(budget:intents:)` `:244` (rejects
  duplicate pan ids `:255`), `Allocation.quality(forPan:)` `:160`. `BandSlicesModel`
  filters by `panKey` (`Band/BandSlicesModel.swift:1198`; `Entry.panKey` `:36`), which
  nothing sets today. Capabilities `effectiveMaxSlices` and `userDdcCount` are read
  raw from the mirror's capability values (`Radio/RadioAtAGlance.swift:194`,
  `Tools/ConnectionPerformanceModel.swift:1101-1107`); the Core sends them at
  `src/core/session/StationCapabilities.cpp:303-305`, from each board's `maxSlices` and `userDdcCount` in `src/core/BoardCapabilities.cpp`. The desktop's limit:
  `MainWindow::panLayoutLimitFor` (`src/gui/MainWindow.cpp:18953`),
  `qMin(maxSlices(), userStreamCount())`.
- **Slices and pans:** `PanSheetModel` (`Main/PanSheetModel.swift`): `addSliceVerb =
  "addSliceOnPan"` `:47`, `addSlice()` `:144-149` with `panId: panKey`, `panKey`
  default `"pan-0"` `:61`, `:95`, from the active slice `:239-240`; the notch
  `notch.addAtSlice` (sliceId only) gated on `notchControlVersion >= 2` `:48-52`,
  `:152-157`. The Core's verbs: `addSliceOnPan {panId}`
  (`SessionCommandDispatcher.cpp:726`, handler `:2624-2656`), `notch.add {sliceId,
  centreHz, widthHz}` (`:1094`, never sent by the phone today). A slice's `panKey` is a
  two-way property (`MirrorPolicy.cpp:280-287`, `:334`). Desktop layout:
  `RadioModel::rehomeSlicesToPans` (`src/models/RadioModel.cpp:35902`),
  `spreadSlicesOntoEmptyPans` (`:35966`, "a pan holding a single slice is never
  raided"), `MainWindow::applyPanLayout` (`MainWindow.cpp:18683`).
- **The Pan sheet:** `PanSheet` (`Main/PanSheet.swift:11`), title `"Pan \(MainScreenModel.panNumber)"`
  `:17`, Add a slice here `:36`, Add a notch `:40-42`, private `bandGrid` `:65-85`,
  Extended view `:92-108`; opened by `Toolbar.swift:269-270` and shown at
  `MainScreen.swift:295-296`.
- **The band's gestures:** `BandGestureLayer.swift`: drag `:275` with
  `DragGesture(minimumDistance: TuneGestures.dragThresholdPoints)` `:516` (6 points,
  `NereusBand/TuneGestures.swift:90`), taps on `touchSurface` `:487-495`, `taps`
  `:536-551`, `pinch` `:556`; flags draw over the touch surface `:254-256`; menus sit
  in an overlay `:276-292`. Other holds: `SpotLayer.swift:92` (`holdSeconds = 0.45`
  `:22`), `FreeDVStationRow.swift:25`. Haptics: only `Dial/DialHaptics.swift:95-97`.
- **The desktop's band menu** (`src/gui/SpectrumOverlayMenu.cpp`): "Display Range"
  `:131`, Ref Level `:137` (-160 to 20), Dyn Range `:154` (20 to 160), "Tuning"
  `:170`, "CTUN (independent pan)" `:174`, "Notch" `:189`, "Add notch here" `:194`
  (a notch of `NotchModel::kDefaultNotchWidthHz` 200 Hz, `src/models/NotchModel.h:213`,
  via `SpectrumWidget.cpp:13385` and `MainWindow::onNotchCreateRequested`),
  "Display settings..." `:216`.
- **The desktop's column** (`src/gui/SpectrumOverlayPanel.cpp`): ◀ `:455` / `:815`,
  +RX `:474`, +TNF `:489`, BAND `:500`, ANT `:508`, Display `:519`, VAX `:527`, ATT
  `:535`. **The pill:** tabs `:1638-1642`; Spectrum `:3152`, Levels `:3490`, FFT
  `:3659`, Waterfall `:4018`, View `:4233`; footer `:1738-1770` (Reset `:1763`, "Setup
  defaults →" `:1770`). **Setup's strip:** `src/gui/SetupPage.cpp:366-470` (All-pans
  note "A change here goes to every pan." `:451`); `SetupDialog::setDisplayPanIndex`
  (`src/gui/SetupDialog.cpp:997`).
- **The iPad and sideways:** `IPadLayout` (`iPad/IPadLayout.swift:14-36`: `.phone`,
  `.column` with the 320 point `AppletColumn` on the right, `.frontPanel`).
  `MainScreen.swift`: `sideways = width > height` `:82`, `IPadLayout.arrangement` `:83`,
  branches `:85-114`, safe area only on the phone branch `:90-91`,
  `.ignoresSafeArea(edges: sideways ? .horizontal : [])` `:247`. PTT
  `MainScreen.swift:177-180` (`PttButton.inset` 14, `bottomInset` 12, `diameter` 76,
  `PttButton.swift:20-24`). `KeyedOverlay.swift:46-66`. Menus: ANT is
  `FlagAntennaMenu` (`Band/FlagMenus.swift:224`); VAX is the private `FlagVaxSection`
  (`FlagMenus.swift:617`) inside `FlagPanel`; ATT is in `FrontEndSection`
  (`Modes/FrontEndSection.swift:15`, step attenuator `:52-66`); BAND is `PanSheet`'s
  private grid.
- **FakeStation** (`NereusKit/Sources/NereusKitTesting/FakeStation.swift`):
  `init(fixture:…withoutCapabilities:)` `:318`, `capabilities(_:adding:removing:)`
  `:732`, `settings.write` routing `:935` answered only for `BandPlanName`
  (`FakeStation+BandPlan.swift:84-90`), `refuseNext(_:reason:)` `:891`, `takeRefusal`
  `:919`, `publishSetup(_:json:revision:)` (`FakeStation+Setup.swift:78`). No
  `addSliceOnPan` or `notch.add` handler. The default fixture
  `tests/data/link/v1/sessions/connect-connectable.json` has `effectiveMaxSlices` 5
  and `userDdcCount` 2.
- **Tests:** app suites in `ios/NereusApp/Tests`: `DisplaySheetTests`,
  `DisplayMoreTests`, `CoreDisplayTests`, `TransmitDisplayTests`, `DropSheetShotTests`
  (`NEREUS_MAIN_SHOTS`), `PhoneSetupKeysV29Tests` (blanking `:154-169`),
  `SetupDescribedPagesTests`, `SetupRenderingTests`, `SetupSplitTests`,
  `SetupWorkspaceShotTests`, `IPadShotTests` (`NEREUS_IPAD_SHOTS`),
  `MainScreenShotTests` (`NEREUS_MAIN_SHOTS`; the sideways screen 874 by 402 at `:55`,
  `:127`). NereusKit: `NereusBandTests/DisplayV29ScopeTests`,
  `DisplayV29FoundationTests`, `BandDisplaySettingsTests`. UI tests in
  `ios/NereusApp/UITests`: `PanelDragUITests.swift:77-113`, `SetupSplitUITests.swift:84-106`
  (launch arguments `-NereusShowBand`, `-NereusFlagsOnBand`, `-NereusRotorFixture`
  `:157`), `IPadUITests.swift` (`:21`, `:65`). DEBUG launch fixtures follow
  `App/UITestDiversityFixture.swift`.
- **Docs:** the R-IOS table in `docs/architecture/2026-09-23-iphone-app-design.md`
  (last row R-IOS-54 at `:1156`); the parity audit
  `docs/architecture/2026-09-23-iphone-app-display-parity.md`.

## File Structure

**Create**

| Path | Responsibility |
| --- | --- |
| `ios/NereusApp/Main/PanIdentity.swift` | A pan's number, its store key and its Core pan key; the pan and slice limits from the Core's capabilities |
| `ios/NereusApp/Main/CoreBandScale.swift` | dB Max and dB Min per band on the Core: read, write, unconfirmed, refused, the older-Core fallback |
| `ios/NereusApp/Main/DisplayPill.swift` | The five tabs, the footer, the peek; one view for all three containers |
| `ios/NereusApp/Main/DisplayPillRows.swift` | Each tab's rows as a table of setting keys in the desktop pill's order, with the phone's rows |
| `ios/NereusApp/Setup/DisplaySettingsStrip.swift` | The "Settings for" strip: buttons, dots, notes, actions |
| `ios/NereusApp/Band/BandMenu.swift` | The band menu and its half-second hold |
| `ios/NereusApp/Band/BandButtonColumn.swift` | The button column and its menus beside the band (named so it is not confused with `AppletColumn`) |
| `ios/NereusApp/Main/BandSelectGrid.swift` | The band grid, out of `PanSheet`, for the Pan sheet and the column's BAND |
| `ios/NereusApp/Main/PanStackModel.swift` | The open pans, their bands, the shared allocator, Add a pan and Close this pan |
| `ios/NereusApp/Main/PanStack.swift` | The iPad's stacked pans, each with its column and pill |
| `ios/NereusKit/Sources/NereusKitTesting/FakeStation+Display.swift` | FakeStation answers for `DisplayGridMax_*` / `DisplayGridMin_*` writes, `addSliceOnPan`, `notch.add`, slice `panKey` writes |
| `ios/NereusApp/Tests/PanIdentityTests.swift` | Keys and limits for each capability fixture |
| `ios/NereusApp/Tests/CoreBandScaleTests.swift` | R-IOS-56 against FakeStation |
| `ios/NereusApp/Tests/DisplayPillTests.swift` | Tab tables against the desktop, own values, reset, Setup defaults, peek |
| `ios/NereusApp/Tests/DisplayStripTests.swift` | The strip's text for each pan state |
| `ios/NereusApp/Tests/BandMenuTests.swift` | The hold's hit rules and the items |
| `ios/NereusApp/Tests/PanStackTests.swift` | Add and close, slice moves, shared and own rows, a lower limit |
| `ios/NereusApp/UITests/DisplayPillUITests.swift` | Setup defaults opens the page on that pan; NB Blank on the sheet and in Setup; the band menu |
| `ios/NereusApp/App/UITestDisplayFixture.swift` | DEBUG launch argument `-NereusDisplayV29Fixture` (a version 29 description, two pans' slices), only if the existing fixtures cannot give them |

**Modify**

| Path | Change |
| --- | --- |
| `ios/NereusApp/Main/MainScreenModel.swift` | `DisplayEditScope` keyed by `PanIdentity`; the pill's per-pan actions; one pan becomes the pan stack's first |
| `ios/NereusApp/Main/MainScreen.swift`, `Toolbar.swift`, `DropSheet.swift` | The pill in the sheet; the column and pill sideways and on the iPad; the PTT's move; the toolbar following the pan last touched |
| `ios/NereusApp/Main/DisplaySheet.swift`, `DisplaySheetModel.swift`, `Band/DbmScaleArrows.swift` | The scale through `CoreBandScale`; the old sheet kept for a Core below version 29 |
| `ios/NereusApp/Main/CoreDisplayModel.swift` | A refusal reason per key |
| `ios/NereusApp/Main/BandSubscriber.swift`, `Band/BandSlicesModel.swift` | A pan identity per subscriber; the shared allocator; the pan's slices |
| `ios/NereusApp/Main/PanSheet.swift`, `PanSheetModel.swift` | "Pans": Add a pan, Close this pan; the grid from `BandSelectGrid`; the pan's own key |
| `ios/NereusApp/Band/BandGestureLayer.swift` | The hold that opens the band menu |
| `ios/NereusApp/Band/FlagMenus.swift`, `Modes/FrontEndSection.swift` | VAX and the step attenuator usable as menus beside the column |
| `ios/NereusApp/Setup/SetupTree.swift`, `SetupRouter.swift`, `DescribedPage.swift`, `PhoneSetupKeys.swift`, `DisplayOnThisPhonePage*.swift`, `App/RootView.swift` | The eight pages without "On this phone"; the strip; opening a page on a pan |
| `ios/NereusKit/Sources/NereusBand/BandDisplaySettings.swift`, `BandDisplaySettingsStore.swift`, `BandDisplayOptions.swift` | `bandScales` retired; the zoom kept by the page reset |
| `ios/NereusKit/Sources/NereusMirror/SetupDescription.swift` | Names for bands 14 to 26 |
| Existing tests named in each task | The new behavior |
| `docs/architecture/2026-09-23-iphone-app-design.md` | DC1 to DC13 and R-IOS-55 to R-IOS-61 |
| `docs/architecture/2026-09-23-iphone-app-display-parity.md` | What the phone now matches |

## Task order and flags

Task 1 is the controller's and runs first; nothing is built on the two Core facts until
it reports. Then two lanes (JJ's limit is two build lanes): the main lane runs Tasks 2,
3, 4, 6, 7, 8 and 9 in the working tree; Task 5 runs in its own worktree after Task 2
and is merged back before Task 6, which uses its router and strip. Task 10 runs last on
the finished branch. No two lanes share a file: Task 5 owns `Setup/*`, `App/RootView.swift`
and `PhoneSetupKeys.swift` except the two dB Max and dB Min bindings, which Task 3
changes first. Nothing here touches the audio path, transmit or the link's wire format;
Task 9 opens a second display subscription, which is the one change near the receive
display path, so the controller may ask for an earlier look at its interfaces.

Calls settled in this plan, for JJ's veto (none changes a spec decision):

1. The Spectrum tab also carries Fill color and Trace color, and the View tab BG Color,
   because DC1 puts the desktop pill's rows in its order and the desktop has them
   (`SpectrumOverlayPanel.cpp:3190`, `:3225`, `:4310-4397`); the board left them out.
2. The band menu's "Add notch here" sends the Core's `notch.add` at the finger's
   frequency with the desktop's 200 Hz width, as the desktop's menu does; the Pan
   sheet's and +TNF's notch stays `notch.addAtSlice`. Spec 6.1's sentence that the menu
   adds it "as Add a notch in the Pan sheet does today" is corrected in the spec.
3. A refused Core write shows the Core's reason under that row (spec 5.4 and 9), so
   `CoreDisplayModel` keeps a reason per key beside its model-wide note.
4. The short-wave broadcast bands (14 to 26) get the desktop's names, so their scale is
   shared too; today the phone has no name for them.
5. The phone keeps store key `"1"` for Pan 1, so its saved values carry over; Pan 2 is
   `"2"`, the Core's `"pan-1"`.
6. "Reset this pan" uses the store's `reset(pan:)`, which already leaves the zoom; the
   All-pans page reset uses `resetKeepingPlace`, which gains the zoom (spec 5.3).

Pending on JJ's devices (spec section 10): NB Blank on his phone; the band menu upright
and sideways; the column, menus and PTT on his iPad and phone; two pans on his iPad on
the G2E; the scale moving between the phone and the desktop on a real Core.

## Task 1: Confirm the Core's two facts and find the missing NB Blank row

**Requirements:** spec section 6.6 (both facts and the NB Blank cause). No code.

**Files:**
- Create: nothing in the repository. Findings go in the crew ledger.

**Interfaces:**
- Consumes: nothing.
- Produces: three findings in the ledger, each with its evidence: (a) a peer's
  `settings.write` of `DisplayGridMax_<band>` reaches another peer's pan on that band;
  (b) a second display subscription from one connection, on a slice of another pan, is
  granted and framed; (c) why JJ's Setup shows no NB Blank row. If (a) or (b) is not
  so, the plan stops and the controller reports to JJ and the Core's owner.

**Acceptance:**
- (a) and (b) are each shown first in the Core's source with file:line (the
  `settings.write` handler and its fan-out to other peers for a station-scoped key; the
  media controller's `subscribe` handling of a second `endpointId` on one connection,
  within `maxDisplayEndpoints` 8, and whether two slices on one receiver share one
  subscription as `tests/tst_fft_router_topology.cpp:36` suggests), then by the
  existing station tests run once on a build lane:
  `tst_display_shared_settings_all_pans`, `tst_fft_router_topology`,
  `tst_daemon_media_controller` (each with `QT_QPA_PLATFORM=offscreen` and the test
  HOME variables from `feedback_tests_offscreen`).
- On a real Core, with JJ's yes and on the Core he names (the bench Anvelina at
  192.168.109.66 unless he says otherwise; never his own window's settings): two
  desktop remote windows on one Core, one changing dB Max on 20 m, the other's 20 m
  scale moving within a second; and one remote window opening Pan 2, with the Core's
  log showing two display endpoints for that connection and frames on both. The ledger
  records what was seen.
- (c) names the cause from evidence, not guesswork. The leading candidate from the
  code: the sheet's "More display options in Setup ›" opens the native "On this phone"
  page, titled "Display", whose "Waterfall" group has no NB Blank
  (`App/RootView.swift:117`, `DisplayOnThisPhonePage.swift:195`, `:264`); the others
  are a Core serving Display below version 29 (`legacy/display-v28.json` has no NB
  rows) or an installed build older than `fec187798`. The controller asks JJ one
  question (how he reached the page, and the build shown in Setup > About) and reads
  his Core's `setupDescriptionVersion` from its log. Task 4's and Task 5's NB Blank
  acceptance closes whichever cause it is.

**Verification:** discovery. Source reading plus existing station tests (one build
lane, offscreen); the real-Core observation is the controller's with JJ's yes.

**Execution note (advisory):** the controller, not an implementer (it needs a Core and
a question to JJ). Touches a Core only as a client; changes nothing on it. Blocks Tasks
3 and 9; Task 2 may start beside it.

- [ ] **Step 1:** Read the Core's settings fan-out and display subscribe paths; note file:line.
- [ ] **Step 2:** Run the three station tests on a build lane; record results.
- [ ] **Step 3:** With JJ's yes, the two-window check on the named Core; record it.
- [ ] **Step 4:** Ask JJ the NB Blank question; read his Core's description version; record the cause.

## Task 2: Pan identity, the limits and the test station's answers

**Requirements:** DC10, spec 6.3 (the limit, the keys), 9 (slice and pan limits); the
foundations R-IOS-56, R-IOS-57 and R-IOS-59 test against.

**Files:**
- Create: `ios/NereusApp/Main/PanIdentity.swift`,
  `ios/NereusKit/Sources/NereusKitTesting/FakeStation+Display.swift`,
  `ios/NereusApp/Tests/PanIdentityTests.swift`
- Modify: `ios/NereusApp/Main/MainScreenModel.swift` (`DisplayEditScope`),
  `ios/NereusKit/Sources/NereusMirror/SetupDescription.swift` (bands 14 to 26), the
  callers of `DisplayEditScope` and their tests
- Test: `PanIdentityTests`, NereusKit `NereusKitTestingTests` (the new handlers),
  `NereusMirrorTests` (band names)

**Interfaces:**
- Consumes: nothing.
- Produces:
  - `struct PanIdentity: Hashable, Sendable, Comparable { let number: Int }` with
    `var storeKey: String` (`"\(number)"`), `var corePanKey: String`
    (`"pan-\(number - 1)"`), `init?(corePanKey: String)`, `static let first`
    (number 1), `var title: String` (`"Pan \(number)"`).
  - `enum PanLimits` with `static func panCount(capabilities: [String: MirrorValue]) -> Int`
    (`max(1, min(effectiveMaxSlices, userDdcCount))`, either missing counts as the
    other, both missing gives 1) and
    `static func sliceCount(capabilities: [String: MirrorValue]) -> Int?`
    (`effectiveMaxSlices`), reading the values as `ConnectionPerformanceModel.swift:1101-1107`
    does, with the desktop cite `MainWindow.cpp:18953`.
  - `MainScreenModel.DisplayEditScope` becomes `enum DisplayEditScope: Hashable { case all; case pan(PanIdentity) }`;
    every use of today's `.pan` becomes `.pan(.first)`, with no behavior change.
  - `SetupDescription.bandName(_:)` names 14 to 26 `"120m"`, `"90m"`, `"61m"`,
    `"49m"`, `"41m"`, `"31m"`, `"25m"`, `"22m"`, `"19m"`, `"16m"`, `"14m"`, `"13m"`,
    `"11m"` (`src/models/Band.h:129-142`, `Band.cpp:216-220`).
  - In `FakeStation+Display.swift`: `settings.write` for a key starting
    `DisplayGridMax_` or `DisplayGridMin_` is applied and echoed as the Core does;
    `func refuseNextSetting(_ key: String, reason: String)` makes the next write of
    that key refused with that reason and the old value restored;
    `addSliceOnPan {panId}` adds a slice with that `panKey`, up to
    `effectiveMaxSlices`, then refuses with the Core's reason; `notch.add {sliceId,
    centreHz, widthHz}` records the notch and answers ok; a slice `panKey` property
    write moves the slice. `var notchesAdded: [(sliceId: String, centreHz: Double, widthHz: Double)]`
    lets tests read what was sent.

**Acceptance:**
- `PanLimits.panCount` from capability values built with each board's numbers in
  `src/core/BoardCapabilities.cpp` (there are no per-radio capability fixtures in
  `tests/data/link/v1`): ANAN-G2 (`kSaturn`, 5 slices, 5 user DDCs `:1158`, `:1159`)
  gives 5; ANAN-G2E (`kHermesC10`, 5 and 4, `:810`, `:825`) gives 4; Hermes Lite 2
  (`kHermesLite`, 5 and 2, `:950`, `:951`) gives 2; Atlas (`kAtlas`, 3 and 3, `:349`,
  `:350`) gives 3; the default `connect-connectable.json` (5 and 2) gives 2. Values
  without `userDdcCount` give `effectiveMaxSlices`; values without either give 1.
- `PanIdentity(corePanKey: "pan-1")?.number == 2`; `PanIdentity(corePanKey: "x") == nil`;
  `PanIdentity.first.storeKey == "1"`, so the store's `phone.band.display.1` is still
  Pan 1's.
- `bandName(14) == "120m"`, `bandName(26) == "11m"`, `bandName(27) == "2m"`,
  `bandName(28) == nil`.
- FakeStation: a `DisplayGridMax_20m` write of `-40` is echoed with `-40`; after
  `refuseNextSetting("DisplayGridMax_20m", reason: "r")` the next write is refused
  with `"r"` and the value stays; `addSliceOnPan` past the limit is refused;
  `notch.add` is recorded with its three values.
- Every existing test that used `.pan` passes unchanged in meaning.

**Verification:** ordinary feature plus test infrastructure. NereusKit:
`swift-test.sh --jobs 2 --filter NereusKitTestingTests` and `--filter NereusMirrorTests`;
app: `PanIdentityTests`, `DisplaySheetTests`, `DisplayMoreTests` on `iPhone 17`.

**Execution note (advisory):** opus. No prerequisites; may run beside Task 1. Touches
no networking (FakeStation is test-only).

- [ ] **Step 1:** `PanIdentity` and `PanLimits`, with their tests.
- [ ] **Step 2:** `DisplayEditScope` with a pan; callers moved to `.pan(.first)`.
- [ ] **Step 3:** Names for bands 14 to 26.
- [ ] **Step 4:** FakeStation's display answers and their tests.

## Task 3: dB Max and dB Min per band through the Core

**Requirements:** R-IOS-56, DC3, spec 5.4, section 9 rows 1 to 4, section 12 item 9.

**Files:**
- Create: `ios/NereusApp/Main/CoreBandScale.swift`,
  `ios/NereusApp/Tests/CoreBandScaleTests.swift`
- Modify: `ios/NereusApp/Main/CoreDisplayModel.swift` (reason per key),
  `DisplaySheetModel.swift` (`dragScale`, `setTop`, `setRange`, `nudgeTop`,
  `nudgeFloor` through `CoreBandScale` while not keyed), `Band/DbmScaleArrows.swift`,
  `Setup/PhoneSetupKeys.swift` (the `DisplayGridMax` / `DisplayGridMin` bindings
  `:136-139` read and write the Core's per-band key),
  `ios/NereusKit/Sources/NereusBand/BandDisplaySettings.swift` (`bandScales` no longer
  read or written; the saved value is dropped on load), the band's scale source in
  `Band/BandModel.swift` or wherever the band reads `scaleTopDbm` / `scaleBottomDbm`
- Test: `CoreBandScaleTests`, `CoreDisplayTests`, `DisplaySheetTests`,
  NereusKit `BandDisplaySettingsTests`

**Interfaces:**
- Consumes: `SetupDescription.bandName(_:)` (Task 2), FakeStation's display answers
  (Task 2), `CoreDisplayModel`'s settings proxy.
- Produces:
  - `@MainActor final class CoreBandScale: ObservableObject` with
    `static func maxKey(band: Int) -> String?` (`"DisplayGridMax_" + bandName`),
    `static func minKey(band: Int) -> String?`,
    `var isShared: Bool` (the Core describes Display at version 29),
    `func range(band: Int) -> (maxDbm: Double, minDbm: Double)?`,
    `func set(maxDbm: Double, minDbm: Double, band: Int)`,
    `func isUnconfirmed(band: Int) -> Bool`,
    `func refusal(band: Int) -> String?`.
  - `CoreDisplayModel.refusalReason(_ key: String) -> String?`, set when a write of
    that key is refused, cleared by the next accepted value of that key.

**Acceptance:**
- Against FakeStation at version 29: on 20 m the band draws `DisplayGridMax_20m` /
  `DisplayGridMin_20m` from the mirror; changing to 40 m draws 40 m's keys; a write
  from the Levels row, the dBm arrows (10 dB, `arrowStepDb`) or a scale drag sends the
  20 m key and shows the written value with the unconfirmed mark until the echo;
  `refuseNextSetting("DisplayGridMax_20m", reason: "r")` then a write puts the Core's
  value back and `refusal(band: 5) == "r"` shows under the row.
- While keyed, the arrows and drag still write `txGridTopDbm` / `txGridRangeDb`, as
  `writeScale` does today; nothing is sent to the Core's grid keys.
- Below version 29: the sheet keeps Top and Range on the phone's own scale with
  "Needs a newer Core to share the scale" under them; no `DisplayGridMax_` write is
  sent.
- A phone with saved `bandScales` from an older build: after upgrade nothing is sent
  to the Core and each band shows the Core's scale (section 12 item 9).
- Link lost: the scale rows are grayed with "Not connected".
- SWL: on band 15 (90 m) the keys are `DisplayGridMax_90m` / `DisplayGridMin_90m`.

**Verification:** risky interface (a shared value written by several surfaces through
the Core). Test-first for `CoreBandScale` against FakeStation. App suites
`CoreBandScaleTests`, `CoreDisplayTests`, `DisplaySheetTests`; NereusKit
`--filter BandDisplaySettingsTests`. Integration on a real Core (R-IOS-56: the phone
moves the desktop's scale on that band and the reverse) is the controller's, with JJ's
yes, after Task 4 gives the Levels row; pending until then.

**Execution note (advisory):** opus. Needs Tasks 1 (fact a) and 2. Writes a
station-scoped setting through the existing proxy; no new verb. Main lane.

- [ ] **Step 1:** `CoreBandScale` and the reason per key, test-first against FakeStation.
- [ ] **Step 2:** The band, the arrows, the drag and the sheet read and write it; the keyed path unchanged.
- [ ] **Step 3:** The two description bindings point at the Core's key; `bandScales` retired and dropped on load.
- [ ] **Step 4:** The older-Core fallback and the link-lost gray.

## Task 4: The pill's five tabs in the sheet

**Requirements:** R-IOS-55 (tabs, rows, order, live, Cal Offset, NB Blank), DC1, DC2,
DC5, spec 5.1, 5.2, section 9 rows 1, 2, 5, 7, section 12 item 8.

**Files:**
- Create: `ios/NereusApp/Main/DisplayPill.swift`, `DisplayPillRows.swift`,
  `ios/NereusApp/Tests/DisplayPillTests.swift`
- Modify: `ios/NereusApp/Main/MainScreen.swift` (`dropped(.display)` draws
  `DisplayPill` at version 29, `DisplaySheet` below it), `DropSheet.swift` if the pill
  needs its own width rules, `Tests/DropSheetShotTests.swift` (one shot per tab)
- Test: `DisplayPillTests`, `DropSheetShotTests`

**Interfaces:**
- Consumes: `CoreBandScale` (Task 3) for the Levels tab's dB Max and dB Min,
  `PanIdentity` (Task 2), `DescribedControl` and `SetupControlDispatcher`.
- Produces:
  - `enum DisplayPillTab: String, CaseIterable { case spectrum, levels, waterfall, view, fft }`
    with `var title: String` ("Spectrum", "Levels", "Waterfall", "View", "FFT") and
    `var setupPageId: String` (the version 29 page ids for Spectrum, Levels,
    Waterfall, View (`display.threeD`) and FFT & Engine).
  - `enum DisplayPillRow: Equatable { case group(String); case setting(key: String); case phone(PhoneRow); case link(title: String, pageId: String) }`
    and `enum PhoneRow { case spectrumHeight, lookBack, ctun, dup, decimation }`.
  - `enum DisplayPillRows { static func rows(_ tab: DisplayPillTab) -> [DisplayPillRow] }`.
  - `struct DisplayPill: View` with
    `init(pan: PanIdentity, device: String, tab: Binding<DisplayPillTab>, main: MainScreenModel, core: CoreDisplayModel, onSetupDefaults: @escaping (DisplayPillTab, PanIdentity) -> Void)`.
    `device` is "this phone" or "this iPad" for the title "Display Pan 1 · this phone".

**Acceptance:**
- Each tab's table lists the desktop's setting keys in the order the desktop function
  builds them, cited per tab: Spectrum (`SpectrumOverlayPanel.cpp:3152`):
  `DisplaySpectrumDetector`, `DisplayDispNormalize`, `DisplayPanFill`,
  `DisplayGradientEnabled`, `DisplayFillColor`, `DisplayFftFillAlpha`,
  `DisplaySpectrumAveraging`, `DisplaySpectrumAverageTimeMs`, `DisplayTraceColor`,
  `DisplayLineWidth`, `DisplayPeakHoldEnabled`, `DisplayPeakHoldResetMs`, Spectrum
  Height (this phone), `DisplayCalOffset`; Grid: `DisplayGridEnabled`,
  `DisplayFreqGridSpacing`, `DisplayFreqScaleFontPx`; Markers: `DisplayShowNoiseFloor`,
  `DisplayShowBinWidth`, `DisplayShowPeakValueOverlay`, `DisplayShowCursorFreq`; the
  link "Peaks & Markers in Setup ›". Levels (`:3490`): `SpectrumScaleMode`,
  `DisplayGridMax`, `DisplayGridMin`, `DisplayNFOffsetGridFollow`,
  `DisplayMaintainNFAdjustDelta`; `WaterfallLevelsMode`, Re-tune, `DisplayWfBlackLevel`,
  `DisplayWfColorGain`, `WaterfallAGCOffsetDb`, `DisplayWfLowLevel`,
  `DisplayWfHighLevel`. Waterfall (`:4018`): `DisplayWfColorScheme`,
  `DisplayWfUpdatePeriodMs`, `DisplayWfOpacity`, `WaterfallBlankingEnabled`,
  `WaterfallBlankingThresholdDb`, `DisplayWaterfallAveraging`,
  `DisplayWaterfallAverageTimeMs`; Overlays: `WaterfallStopOnTx`,
  `DisplayShowRxFilterOnWaterfall`, `DisplayShowTxFilterOnRxWaterfall`,
  `DisplayShowRxZeroLine`, `DisplayShowTxZeroLine`; Time: `DisplayWfTimestampPos`,
  `DisplayWaterfallHistoryMs`, Look back (this phone). View (`:4233`):
  `DisplaySpectrumRenderMode`, `Display3DFloor`, `Display3DGain`, `Display3DSpan`,
  `Display3DAngle`, `Display3DSpeed`, `Display3DSliceShadow`; Background:
  `DisplayBackgroundImage`, `DisplayBackgroundEnabled`, `DisplayBackgroundOpacity`,
  `DisplayBackgroundColor`; On this phone: CTUN, DUP. FFT (`:3659`):
  `DisplayFftSizingMode`, `DisplayFftSize`, `DisplayHzPerBinTarget`, the readout,
  `DisplaySpectrumFps`, `DisplayFftWindow`; On this phone: decimation. A test compares
  each table with this list; a key the version 29 description does not bind is a test
  failure that names it.
- A `.setting` row draws with `DescribedControl` for the description control bound to
  that key, so it reads and grays as Setup's does: the NB threshold is grayed with the
  description's reason while NB Blank is off and editable when it is on; Cal Offset is
  grayed with "The Core calibrates for its radio."; every Levels row is grayed while
  transmitting with "Transmit has its own levels, on Transmit Display."
- The tabs are a pick-one choice with the open tab in the glass; the title reads
  "Display Pan 1 · this phone".
- Live: changing a row changes the band's settings for that pan at once (the store's
  value for `pan.storeKey` changes in the same run loop turn).
- NB Blank: turning it on from the Waterfall tab sets `waterfallBlanking` for Pan 1,
  and the threshold row becomes editable.
- Link lost: the pill stays open with its last values; the Core's rows (the FFT tab,
  dB Max and dB Min) are grayed with "Not connected"; the phone's rows still work.
- Below version 29, `dropped(.display)` draws today's `DisplaySheet` with "Needs a
  newer Core for the desktop's Display tabs" at the top; a row the Core does not
  describe is grayed with "Needs a newer Core".
- Shots: one `DropSheetShotTests` frame per tab on `iPhone 17`, compared by eye with the
  board's five frames.

**Verification:** UI feature (`ui-verification`: one surface, five states). Unit
`DisplayPillTests`; shots `DropSheetShotTests` to a scratch folder; the controller
opens them beside the board before calling the tab layout done.

**Execution note (advisory):** opus. Needs Tasks 2 and 3. Main lane.

- [ ] **Step 1:** The row tables and their test against the desktop's order.
- [ ] **Step 2:** `DisplayPill` with the tabs and rows, live on the band.
- [ ] **Step 3:** The sheet draws it at version 29 and keeps today's sheet below.
- [ ] **Step 4:** Shots of the five tabs.

## Task 5: Setup > Display as the desktop's eight pages, with the strip

**Requirements:** R-IOS-60, DC11, DC12, spec 7.1 to 7.4, section 12 items 4, 5, 7.

**Files:**
- Create: `ios/NereusApp/Setup/DisplaySettingsStrip.swift`,
  `ios/NereusApp/Tests/DisplayStripTests.swift`
- Modify: `ios/NereusApp/Setup/SetupTree.swift` (no "On this phone" at version 29;
  the older skeleton keeps it), `SetupRouter.swift` (a pan to open a page on),
  `DescribedPage.swift` (the strip replaces `DisplayV29ScopeHeader`; the own-value dot
  in `WorkspacePalette.amberText`), `PhoneSetupKeys.swift` (the phone's rows placed per
  7.2), `DisplayOnThisPhonePage*.swift` (kept only for a Core below version 29),
  `App/RootView.swift` (the sheet's Setup link opens the matching page),
  `ios/NereusKit/Sources/NereusBand/BandDisplayOptions.swift` (`resetKeepingPlace`
  keeps `rxViewSpanHz`), `Tests/SetupWorkspaceShotTests.swift`,
  `Tests/PhoneSetupKeysV29Tests.swift`, `Tests/SetupDescribedPagesTests.swift`,
  `UITests/SetupSplitUITests.swift` (`moreDisplayOptions` now lands on a version 29 page)
- Test: `DisplayStripTests`, `PhoneSetupKeysV29Tests`, `SetupDescribedPagesTests`,
  `SetupSplitTests`, `SetupWorkspaceShotTests`

**Interfaces:**
- Consumes: `PanIdentity`, `PanLimits` and `DisplayEditScope` (Task 2).
- Produces:
  - `SetupRouter.displayScope: MainScreenModel.DisplayEditScope` (default `.all`),
    kept while moving between Display pages, and
    `func open(describedPage: String, scope: MainScreenModel.DisplayEditScope) -> Bool`.
  - `struct DisplaySettingsStrip: View` with
    `init(pageId: String, scope: Binding<MainScreenModel.DisplayEditScope>, panLimit: Int, isPhone: Bool, slicesByPan: [PanIdentity: [String]], ownFieldCount: (PanIdentity) -> Int, reset: (PanIdentity) -> Void, useOnAll: (PanIdentity) -> Void)`.
  - `enum DisplayStripText` with the strip's strings as functions of the pan state, so
    the tests read them.

**Acceptance:**
- At version 29 Setup > Display lists, in order: Spectrum, Levels, Waterfall, View,
  Peaks & Markers, FFT & Engine, Transmit Display, Multimeter; no "On this phone".
  Below version 29 the old skeleton (`SetupTree.swift:134-137`) is unchanged.
- Every row of today's "On this phone" page is reachable where spec 7.2 puts it: a
  test lists `DisplayOnThisPhonePage`'s keys and finds each on its page and group
  (Spectrum Height in Spectrum's "Layout (this phone)"; Top and Bottom as Levels' dB
  Max and dB Min; CTUN and Extended view in View's "On this phone"; DUP in Transmit
  Display's "Transmit scale"; Colors in Setup > Appearance; the Band plan in View's
  "Band plan"). Transmit Display's phone rows say dB Max and dB Min, not Top and
  Range. FFT & Engine has no thread priority or monitor rate. Multimeter has the Core's
  polling delay and this phone's decimal point, units and history.
- The strip on Spectrum, Levels, Waterfall, View and Peaks & Markers, above the first
  group: "All pans", then "Pan 1" to "Pan <limit>", each with a second line
  ("Slice A, B" or "not open"), and " •" after the name when that pan has own values
  on the page. Notes, the desktop's words (`SetupPage.cpp:447-460`): on All pans "A
  change here goes to every pan." (`:451`); on a pan with none, "Pan N follows All pans
  in every row on this page."; on a pan not open, "Pan N is not open now. What you set
  here is kept and used when you open it." Actions on a pan: "Reset this pan" and "Use
  Pan N's on all pans", grayed when the pan has no own values on the page. On All pans
  both stay in place, grayed, with "Pick a pan to reset it." On an iPhone, the pans
  after Pan 1 are grayed with "An iPhone shows one pan." `DisplayStripTests` checks
  each string for each state against these.
- "Reset this pan" on a page clears only that page's own fields for that pan; "Use Pan
  N's on all pans" copies only that page's own fields to All pans.
- Levels keeps its band picker, editing a band's dB Max and dB Min through
  `CoreBandScale` without tuning (if Task 3 has not merged yet in this lane's
  worktree, the picker is wired against Task 3's interface after the merge, before
  this task reports).
- Opening: from the Setup list, All pans; `open(describedPage:scope:)` with `.pan(p)`
  opens that page with `p` chosen; the choice stays across Display pages.
- The sheet's Setup link (and Task 6's "Setup defaults") opens the version 29 page
  for the open tab, never "On this phone".
- On a regular-width iPad the page opens on the right of `SetupSplitView` with the
  strip at its top; in a compact window it pushes.
- The page reset on All pans (`resetKeepingPlace`) keeps the zoom; a test sets
  `rxViewSpanHz`, resets, and finds it unchanged.
- Shots: `SetupWorkspaceShotTests` gains the strip on All pans, on Pan 1 with own
  values and on Pan 2 not open, on `iPhone 17` and `iPad Pro 11-inch (M5)`, in default
  and accessibility XL type.

**Verification:** layout and navigation (`ui-verification`). Unit
`DisplayStripTests`, `PhoneSetupKeysV29Tests`, `SetupDescribedPagesTests`,
`SetupSplitTests`; shots on both simulators; UI
`SetupSplitUITests` on the iPad. NereusKit `--filter BandDisplaySettingsTests` for the
reset.

**Execution note (advisory):** opus. Needs Task 2. Runs in its own worktree beside
Tasks 3 and 4 (second build lane), merged back before Task 6. Owns `Setup/*`,
`App/RootView.swift` and `PhoneSetupKeys.swift` except Task 3's two bindings: it
starts after Task 3's commit to `PhoneSetupKeys.swift`, or merges it in before editing
that file.

- [ ] **Step 1:** The page tree without "On this phone"; each moved row placed, with the reachability test.
- [ ] **Step 2:** The strip and its text tests; the router's scope.
- [ ] **Step 3:** Page reset and copy per page; the zoom kept.
- [ ] **Step 4:** The iPad split and the shots.

## Task 6: Per-pan values on the pill: the dot, Reset this pan, Setup defaults, the peek

**Requirements:** R-IOS-55 (the amber dot and its tap, Reset this pan, Setup defaults,
the peek), DC4, spec 5.3.

**Files:**
- Modify: `ios/NereusApp/Main/DisplayPill.swift` (the footer, dots, peek),
  `MainScreenModel.swift` (own fields, reset and setup-defaults for a `PanIdentity`),
  `MainScreen.swift` (the peek hides the container except the held row),
  `App/RootView.swift` (Setup defaults opens through `SetupRouter.open(describedPage:scope:)`)
- Create: `ios/NereusApp/UITests/DisplayPillUITests.swift`, and
  `ios/NereusApp/App/UITestDisplayFixture.swift` only if no existing launch fixture
  gives a version 29 description
- Test: `DisplayPillTests`, `DisplayPillUITests`

**Interfaces:**
- Consumes: `DisplayPill` (Task 4), `SetupRouter.open(describedPage:scope:)` and
  `DisplayPillTab.setupPageId` (Tasks 4, 5), `BandDisplaySettingsStore.ownFields`,
  `resetPicture(forPan:fields:)`, `reset(pan:)`.
- Produces:
  - `MainScreenModel.ownDisplayFields(for: PanIdentity) -> Set<String>`,
    `clearOwnDisplayField(_ field: String, pan: PanIdentity)`,
    `resetDisplay(pan: PanIdentity)`.
  - An environment value `displayPeek: Binding<String?>` (the held row's key) that
    `DisplayPill` sets from a slider's editing state and the containers read.

**Acceptance:**
- A change on the pill writes Pan 1's own value; that row shows the amber dot
  (`WorkspacePalette.amberText`); a tap on the dot clears that one field and the row
  shows Setup's value. Shared rows (the Core's, dB Max and dB Min, the phone's own)
  never show a dot.
- "Reset this pan" clears every own field of the pan, keeps `rxViewSpanHz` and the
  band position, and is grayed with "Pan 1 follows Setup" when there are none.
- "Setup defaults" on the Waterfall tab of Pan 1 opens Setup > Display > Waterfall
  with Pan 1 chosen in the strip (UI test). Each tab opens its page: Spectrum,
  Levels, Waterfall, View, FFT & Engine.
- Peek: while a slider row is held, the sheet (or pill) draws only that row; on lift,
  the whole container returns. A test drives the editing state and checks what is
  drawn.
- A change in the pill shows in Setup while it is open, and a change to All pans in
  Setup reaches a pan that follows it at once.
- UI test: NB Blank turned on from the sheet's Waterfall tab shows on in Setup >
  Display > Waterfall for Pan 1, and the reverse; both change the pan's blanking.

**Verification:** UI feature with per-pan state. Unit `DisplayPillTests`; UI
`DisplayPillUITests` on `iPhone 17`. Device: NB Blank on JJ's phone (pending; the
controller installs with JJ's yes).

**Execution note (advisory):** opus. Needs Tasks 4 and 5 (merged). Main lane.

- [ ] **Step 1:** Own values, the dot and its tap, Reset this pan.
- [ ] **Step 2:** Setup defaults to the matching page on that pan.
- [ ] **Step 3:** The peek.
- [ ] **Step 4:** The UI tests, with a DEBUG fixture only if needed.

## Task 7: The band menu

**Requirements:** R-IOS-57, DC6, spec 6.1.

**Files:**
- Create: `ios/NereusApp/Band/BandMenu.swift`, `ios/NereusApp/Tests/BandMenuTests.swift`
- Modify: `ios/NereusApp/Band/BandGestureLayer.swift` (the hold),
  `ios/NereusApp/Main/PanSheetModel.swift` (a `notch.add` at a frequency),
  `UITests/DisplayPillUITests.swift` (the hold and each item)
- Test: `BandMenuTests`, `DisplayPillUITests`

**Interfaces:**
- Consumes: `CoreBandScale` (Task 3) for Ref Level and Dyn Range; `DisplayPill`'s
  opener (Task 4); FakeStation's `notch.add` (Task 2).
- Produces:
  - `struct BandMenuHit { static let holdSeconds: Double = 0.5; static func isEmptyBand(at point: CGPoint, ...) -> Bool }`
    (the rule that a point is clear of flags, spots, stations and the scale strip).
  - `PanSheetModel.addNotch(atHz: Double)` sending `notch.add {sliceId, centreHz,
    widthHz: 200}` (`NotchModel.h:213`), available when `notchControlVersion >= 1`.
  - `struct BandMenu: View` with
    `init(frequencyHz: Double, band: Int, scale: CoreBandScale, pan: PanSheetModel, ctun: Binding<Bool>, openDisplay: @escaping () -> Void)`.

**Acceptance:**
- A press of 0.5 s on empty band, moving less than `TuneGestures.dragThresholdPoints`
  (6 points), opens the menu at the finger after a light haptic
  (`UIImpactFeedbackGenerator(style: .light)`, as `DialHaptics` uses). A movement past
  6 points before 0.5 s is a drag and tunes as today; a tap and a double tap act as
  today; a pinch zooms as today. A hold on a flag, a spot label (0.45 s) or a FreeDV
  row opens their own card, never the band menu. `BandMenuTests` checks the hit rules
  on a band fixture with a flag, a spot and empty band.
- Items in the desktop's order with its words: "Display Range" with Ref Level (-160 to
  20) and Dyn Range (20 to 160), which move the band's shared scale through
  `CoreBandScale`; "Tuning" with "CTUN (independent pan)"; "Notch" with "Add notch
  here" and the frequency under the finger; "Display settings..." opening the sheet or
  pill. Ref Level is dB Max; Dyn Range is dB Max minus dB Min.
- "Add notch here" sends `notch.add` with the frequency under the finger and 200 Hz;
  with no notch control on the Core it is grayed with the reason the Pan sheet gives
  today.
- Sideways, the menu opens beside the finger on the side away from the PTT.
- UI test: hold on empty band, the menu shows its four groups; each item does its
  action against the fixture.

**Verification:** gesture and interaction. Unit `BandMenuTests`; UI on `iPhone 17`
upright and sideways. Device: JJ's phone, upright and sideways (pending).

**Execution note (advisory):** opus. Needs Tasks 3 and 4. Sends a new verb to the Core
(`notch.add`), already in the Core's dispatcher; no networking change. Main lane.

- [ ] **Step 1:** The hold and its hit rules, test-first.
- [ ] **Step 2:** The menu and its items.
- [ ] **Step 3:** `notch.add` at the finger.
- [ ] **Step 4:** The UI test.

## Task 8: The button column and the pill, on the iPad and the sideways iPhone

**Requirements:** R-IOS-58, DC7, DC8, spec 6.2, 6.4, section 9 row 6 and the large-type
row, section 12 items 3 and 10.

**Files:**
- Create: `ios/NereusApp/Band/BandButtonColumn.swift`,
  `ios/NereusApp/Main/BandSelectGrid.swift`
- Modify: `ios/NereusApp/Main/MainScreen.swift` (the column at the band's top-left
  inside the safe area, on the iPad and sideways; the pill beside it; the PTT and
  keyed meters moving right of whatever is open), `PanSheet.swift` (its grid from
  `BandSelectGrid`), `Band/FlagMenus.swift` (`FlagVaxSection` usable on its own),
  `Modes/FrontEndSection.swift` (the step attenuator usable on its own),
  `Main/KeyedOverlay.swift`, `Tests/IPadShotTests.swift`,
  `Tests/MainScreenShotTests.swift` (sideways with each menu open),
  `UITests/IPadUITests.swift` (the PTT never under an open menu)
- Test: `IPadShotTests`, `MainScreenShotTests`, `IPadUITests`

**Interfaces:**
- Consumes: `DisplayPill` (Tasks 4, 6), `PanLimits.sliceCount` (Task 2),
  `PanSheetModel.addSlice()` and `notch.addAtSlice` (today's +TNF), `FlagAntennaMenu`.
- Produces:
  - `enum BandColumnMenu: Hashable { case band, antenna, display, vax, attenuator }`.
  - `struct BandButtonColumn: View` with
    `init(pan: PanIdentity, open: Binding<BandColumnMenu?>, folded: Binding<Bool>, ...)`
    and a preference `BandColumnFrameKey` carrying the frame of the column plus
    whatever is open, which `MainScreen` reads to place the PTT and keyed meters.
  - `struct BandSelectGrid: View` (the grid `PanSheet` draws today, unchanged in look).

**Acceptance:**
- Shown on the iPad and on the iPhone held sideways, at the band's top-left inside the
  safe area; never on the iPhone upright.
- Buttons, top to bottom: ◀, +RX, +TNF, BAND, ANT, Display, VAX, ATT, each 30 points
  tall with radius 7; the open one in the glass. ◀ folds to one button and back.
- +RX sends `addSliceOnPan` with the pan's `corePanKey`; at the slice limit it is
  grayed with "The <radio> runs <n> slices." (the radio's name as the Core reports it).
  +TNF sends `notch.addAtSlice` for the active slice. BAND opens `BandSelectGrid`, ANT
  `FlagAntennaMenu`, VAX the VAX channel choice, ATT the step attenuator, each in a
  panel beside the column; Display opens `DisplayPill`.
- One menu open at a time; a tap anywhere else closes it.
- The pill is never taller than the band and scrolls past that; sideways it is as
  tall as the band.
- The PTT and the keyed meters sit just right of the open column, pill or menu, and
  return when it closes; never under an open menu (UI test on the iPad, each menu).
- Large type: the buttons keep 30 points; the pill scrolls.
- Shots: `IPadShotTests` with each menu open and the pill on each tab;
  `MainScreenShotTests` sideways with the column, the pill and one menu; compared by
  eye with the board's "The iPad" and "The phone sideways" frames.

**Verification:** layout and interaction (`ui-verification`). Shots on
`iPad Pro 11-inch (M5)` and `iPhone 17` sideways; UI `IPadUITests`. Device: JJ's iPad
and phone (pending).

**Execution note (advisory):** opus. Needs Tasks 6 and 7 (`MainScreen.swift` is shared,
so it runs after them in the main lane).

- [ ] **Step 1:** `BandSelectGrid` out of `PanSheet`; VAX and ATT usable as menus.
- [ ] **Step 2:** `BandButtonColumn` and its menus.
- [ ] **Step 3:** The column and pill in `MainScreen`, with the PTT's and meters' move.
- [ ] **Step 4:** Shots and the UI test.

## Task 9: More than one pan on the iPad

**Requirements:** R-IOS-59, DC9, DC10, spec 6.3, section 9 row 7, section 12 items 1,
2, 6.

**Files:**
- Create: `ios/NereusApp/Main/PanStackModel.swift`, `ios/NereusApp/Main/PanStack.swift`,
  `ios/NereusApp/Tests/PanStackTests.swift`
- Modify: `ios/NereusApp/Main/MainScreenModel.swift` (the first pan's band from the
  stack), `BandSubscriber.swift` (a `PanIdentity`; intents through the shared
  allocator), `Band/BandSlicesModel.swift` (each pan's `panKey`), `PanSheet.swift` and
  `PanSheetModel.swift` (title from the pan; the "Pans" pair), `Toolbar.swift` (follows
  the pan last touched), `MainScreen.swift` (the iPad draws `PanStack`),
  NereusKit `BandDisplaySettingsStore.swift` (the iPad's own rows in `sharedFields`
  if any are missing), `Tests/IPadShotTests.swift` (two pans), `UITests/IPadUITests.swift`
- Test: `PanStackTests`, NereusKit `NereusMediaTests` (the allocator's split),
  `IPadShotTests`, `IPadUITests`

**Interfaces:**
- Consumes: `PanIdentity`, `PanLimits` (Task 2); `BandButtonColumn`, `DisplayPill`
  (Tasks 4, 6, 8); FakeStation's `addSliceOnPan` and `panKey` writes (Task 2).
- Produces:
  - `@MainActor final class PanStackModel: ObservableObject` with
    `@Published private(set) var open: [PanIdentity]`,
    `@Published var lastTouched: PanIdentity`,
    `var limit: Int`, `func addPanState() -> (enabled: Bool, reason: String?)`,
    `func addPan()`, `func closePanState(_ pan: PanIdentity) -> (enabled: Bool, reason: String?)`,
    `func closePan(_ pan: PanIdentity)`, `@Published private(set) var status: String?`
    (the column's status line), and `func band(for pan: PanIdentity) -> PanBand`.
  - `struct PanBand` holding that pan's `BandModel`, `BandSlicesModel` and
    `BandSubscriber`.
  - `final class SharedDisplayAllocator` owning one `DisplayQualityAllocator` and
    taking every pan's intent in one `update(intents:)`.

**Acceptance:**
- The limit is `PanLimits.panCount`; "Add a pan" opens the next pan below the last and
  is grayed at the limit with "The <radio> can show <n> pans.", the number written
  as a word as the board writes it (Hermes Lite 2: "The Hermes Lite 2 can show two
  pans."). +RX's "The <radio> runs <n> slices." writes its number the same way.
- A new pan takes a spare slice from a pan holding more than one (the desktop's
  `spreadSlicesOntoEmptyPans`, `RadioModel.cpp:35966`: a pan with a single slice is
  never raided), by writing that slice's `panKey`; with none spare it sends
  `addSliceOnPan` with the new pan's `corePanKey`. The status line names the slice
  that moved or was added.
- "Close this pan" is grayed on Pan 1 with "Pan 1 stays open."; on Pan 2 it closes the
  pan, writes `panKey "pan-0"` for each of its slices (the desktop's
  `rehomeSlicesToPans`, `:35902`), keeps Pan 2's own values in the store for next
  time, and says "Pan 2 closed. Slice C moved to Pan 1, …" naming the slices.
- Each open pan has its own subscription on a slice of its own pan, its own
  `BandModel` from `settings(forPan: pan.storeKey)`, its own zoom; the shared allocator
  splits the frame budget across both intents (`NereusMediaTests`: two intents, one
  budget, both get frames, neither exceeds its share).
- Pans share the band area equally, Pan 1 on top, each with its column and pill; a
  pill draws on its own pan only. A new slice from a pan's +RX starts on that pan.
- The toolbar's Pan button names the pan last touched, its slice letter is that pan's
  active slice, and its Display opens that pan's pill.
- Shared rows (the Core's, dB Max and dB Min, CTUN, DUP, Look back, Spectrum Height,
  decimation) changed on Pan 2's pill show on Pan 1's; an own row does not.
- After a reconnect to a Core reporting a lower limit, the highest pans close with a
  notice naming the limit; nothing is written to their slices.
- An iPhone never opens a second pan.
- Shots: `IPadShotTests` with two pans, each pill open in turn, compared with the
  board's "Two pans, two pills".

**Verification:** risky interface (a second display subscription; a shared frame
budget; slice moves sent to the Core). Test-first for `PanStackModel` against
FakeStation; NereusKit `--filter DisplayQualityAllocatorTests`
(`ios/NereusKit/Tests/NereusMediaTests/DisplayQualityAllocatorTests.swift`); shots and UI on `iPad Pro 11-inch (M5)`. Integration (two
pans against a real Core with frames on both) and device (JJ's iPad on the G2E) are
the controller's with JJ's yes; pending until seen.

**Execution note (advisory):** opus. Needs Tasks 1 (fact b) and 8. Opens a second
media subscription and writes slice properties: the controller may want an earlier
look at `PanStackModel` and `SharedDisplayAllocator` before the rest is built on them.
Main lane.

- [ ] **Step 1:** `SharedDisplayAllocator` and one subscriber per pan, test-first.
- [ ] **Step 2:** `PanStackModel`: limit, add, close, slice moves, status.
- [ ] **Step 3:** `PanStack` on the iPad; the toolbar following the pan last touched.
- [ ] **Step 4:** Shared and own rows across pans; the lower-limit notice; shots and UI tests.

## Task 10: Docs, the evidence pass and the final checks

**Requirements:** spec section 10 (all R-IOS rows' evidence), 11 (the design doc and
the parity audit), R-IOS-61.

**Files:**
- Modify: `docs/architecture/2026-09-23-iphone-app-design.md` (DC1 to DC13 in its
  decisions; R-IOS-55 to R-IOS-61 after R-IOS-54, with the requirement and evidence
  text exactly as the spec's section 10 gives them),
  `docs/architecture/2026-09-23-iphone-app-display-parity.md` (which desktop display
  controls the phone now matches)

**Interfaces:**
- Consumes: the finished branch.
- Produces: nothing for later tasks.

**Acceptance:**
- The seven rows are in the R-IOS table with the spec's text; DC1 to DC13 are in the
  phone design's decisions.
- The parity audit says, row by row, which desktop pill and Setup > Display controls
  the phone now has.
- Every shot suite this plan touched (`DropSheetShotTests`, `SetupWorkspaceShotTests`,
  `MainScreenShotTests`, `IPadShotTests`) runs to a scratch folder on `iPhone 17`
  and `iPad Pro 11-inch (M5)` and passes; the report lists each suite and folder.
- `python3 scripts/verify-ios-workspace-colours.py`,
  `python3 scripts/verify-ios-provenance.py` and `python3 -m pytest tests/compliance/ -v`
  pass; `ios/scripts/swift-test.sh --jobs 2` passes; the whole `NereusSDR` scheme's
  `test-without-building` on `iPhone 17` passes once, on the finished revision.

**Verification:** the final checks, once. The controller opens the captures beside the
board's frames before saying the screens match. Device and real-Core checks stay
pending for JJ, listed in the report.

**Execution note (advisory):** sonnet (documentation and captures). Needs Tasks 1 to 9.

- [ ] **Step 1:** The design doc's decisions and R-IOS rows; the parity audit.
- [ ] **Step 2:** The shot suites on both simulators.
- [ ] **Step 3:** The repository checks, NereusKit's tests and the full scheme once.
