# The Phone in the Workspace Look Implementation Plan

> **Execution:** run with `crew` (`/crew <this file>`) under `cost-aware-execution`.
> Requirements and acceptance cases are binding; test order and review effort follow
> the risk-based policy; UI evidence follows `ui-verification`. No review between
> tasks; one whole-branch review at the end.

**Goal:** The iPhone and iPad app and its Live Activity wear the desktop's 2026.10.1
workspace look on every screen, with Setup in workspace panels and, on a regular-width
iPad, its list beside the page.

**Architecture:** One token file in `ios/Shared/` carries the desktop's workspace
tokens by the desktop's names, with their cites, and one styles file turns them into
SwiftUI treatments (raised, glass, lit green, lit amber, keyed, danger, panel, inset,
the Setup switch). The app's existing role files (`ChromeColours` and its siblings)
keep their names and point at the tokens, so most views change without being touched;
shared controls adopt the treatments; loose literals move to roles. Setup's container
and rows are restyled once in the shared row views, and the iPad gets a split Setup
in regular width.

**Tech Stack:** Swift 6 language mode, SwiftUI (iOS 17.4 deployment target,
`ios/project.yml:11-12`), ActivityKit and WidgetKit for the Live Activity, Metal in
`NereusKit`'s `BandRenderer`, Swift Testing (`import Testing`) for unit and shot
tests, XCTest for `NereusSDRUITests`, XcodeGen, Python 3 for the repository checks.

**Spec:** [2026-10-20-phone-workspace-look-design.md](2026-10-20-phone-workspace-look-design.md)
(approved by JJ 2026-10-21, "use them and carry on", which also settled its licence
item). Every decision is settled there; this is how it gets built. Mockups:
[2026-10-20-phone-workspace-look/](2026-10-20-phone-workspace-look/). `W`, `R-IOS`
and section numbers below are the spec's.

## Global Constraints

- Read `CLAUDE.md`, `ios/README.md` and the spec first. Nothing under `src/` changes:
  the desktop is the source of the values, never a target.
- Every new Swift file starts with the two header lines the iOS provenance check
  requires (`ios/README.md:24-33`):
  `// NereusSDR for iOS: <what this file is>` then
  `// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission`.
- Licence ruling (JJ, 2026-10-21): the phone carries the desktop's Longpath-derived
  tokens and treatments. Every value taken from `src/gui/styles/WorkspacePalette.h` or
  `WorkspaceComponents.cpp` carries, on its line, the desktop name and file:line and
  the desktop's own Longpath cite exactly as the desktop writes it
  (`From Longpath src/gui/StyleConstants.h:<line> [@551576e3]`). Longpath is named in
  no other `ios/` file.
- Colors are written only in `ios/Shared/WorkspacePalette.swift`, in the role files,
  or as a kept data color (W2 and the kept list in Task 4). A view never repeats a
  value; it reads a role or a token.
- Kept, unchanged (W2): the spectrum and waterfall palettes, the band plan colors, the
  slice colors, the frequency cyan `#00e5ff`, the bandwidth cyan `#00c8ff`, the
  waterfall timestamp `#c8dcff`, the RX antenna blue `#4488ff`, the dial's index tick
  `#c8f8ff` and glow, the TX filter orange `#ff7833`, the RADE marginal `#e0e040` and
  mode `#a78bfa`, spot tier and source colors, FreeDV Reporter row tints, meter scale
  zones, the vintage and classic meter faces' scale colors, and the connection graph
  series colors.
- Red rule (W5): `kTxRed` `#c25a5c` only for transmit (the PTT's edge, the keyed PTT
  and UNKEY filled, the TX badge, the TX clock, the TX pill, the TX antenna).
  Everything else red (link lost, a gauge over range, errors, Cancel, Revoke) uses
  `kRedBg` `#7a2c2e`, `kRedText` `#f0dcdc`, `kRedBorder` / `kGaugeDanger` `#a86b6d`.
- No behavior changes. No action, command, accessibility label, accessibility
  identifier or layout changes, except Setup's layout in Tasks 7 and 8. Text sizes and
  fonts stay as they are.
- Disabled, never hidden: a control that cannot act stays in place at 0.4 opacity
  (`SetupNumberRow.greyedOpacity`, `ios/NereusApp/Setup/SetupNumberRow.swift:40`) with
  its reason shown. Gestures are extra ways in, never the only way (D114).
- Shared Mac: before building or booting a simulator, reserve `/tmp/nereus-ios-full.lock`
  with `mkdir`, record your own token, release only your own lock in an exit trap, one
  simulator at a time, `-jobs 2`, and reuse derived data (`ios/README.md:88-95`).
- Test commands (from `ios/README.md:63-79` and `.github/workflows/ios.yml:224-242`),
  with `DD` a derived-data folder in the scratchpad reused across tasks:
  - `ios/scripts/generate-project.sh`
  - `xcodebuild -project ios/NereusSDR.xcodeproj -scheme NereusSDR -destination 'platform=iOS Simulator,name=iPhone 17' -derivedDataPath "$DD" -jobs 2 build-for-testing`
  - `ios/scripts/sim-ready.sh "iPhone 17" "$DD/Build/Products/Debug-iphonesimulator/NereusSDR.app"`
  - `xcodebuild -project ios/NereusSDR.xcodeproj -scheme NereusSDR -destination 'platform=iOS Simulator,name=iPhone 17' -derivedDataPath "$DD" -collect-test-diagnostics never -parallel-testing-enabled NO -jobs 2 test-without-building -only-testing:NereusSDRTests/<Suite>`
  - Every simulator `test` or `test-without-building` passes
    `-collect-test-diagnostics never -parallel-testing-enabled NO -jobs 2`.
  - A Swift Testing case is named with its parentheses
    (`-only-testing:'NereusSDRTests/<Suite>/<case>()'`); without them xcodebuild runs
    nothing and reports success. A whole suite needs none.
  - Shot tests write PNGs only when their variable names a folder, passed with the
    `TEST_RUNNER_` prefix (`TEST_RUNNER_NEREUS_MAIN_SHOTS=<scratch dir>`). Shot folders
    are in the scratchpad, never in the repository.
  - A change under `ios/NereusKit` also runs `ios/scripts/swift-test.sh --jobs 2` (or
    `--filter <Suite>`) and the affected app suites on a simulator.
  - Repository checks: `python3 scripts/verify-ios-provenance.py` and
    `python3 -m pytest tests/compliance/ -v` (both run in `ci.yml`'s `compliance` job,
    `.github/workflows/ci.yml:114-121`, `:324-326`).
- A test that pins today's navy value is rewritten to the new value and named in the
  task's report; no valid test is weakened or deleted to get green.
- No implementer connects to a radio or a Core, installs on a device
  (`ios/scripts/device-install.sh` is the controller's or JJ's), or touches
  `~/Applications/NereusSDR Dev.app`.
- Commits: GPG-signed (never `--no-gpg-sign`; a signing failure is reported, not worked
  around). No `Co-Authored-By` trailer. No push, no PR, no public post.
- User-facing words are unchanged by this plan, except where a task gives them.
- Line numbers are from `origin/main` `9a20b6b0c`; earlier tasks move them, so find the
  code by the names given.

## What already exists

- **Desktop values (read only):** tokens in `src/gui/styles/WorkspacePalette.h`
  (`kAppBg` `:105` through `kGlassPanelRadius` `:369`, `kAmberBorder` `:271`,
  `kDspToggleBorder` `:412`, `kGaugeDanger` `:328`), treatments in
  `src/gui/styles/WorkspaceComponents.cpp` (`litGreenFill` `:243`, `litAmberFill`
  `:258`, `glassTreatment` `:1010`, the lit branches `:1292-1304`, `raisedFillOf`
  `:1710-1726`, `pressedFill` `:1728`, `glassFill` `:1735`), `shiftL` in
  `src/gui/styles/WorkspaceStyle.cpp:211`. The desktop band ground:
  `WorkspacePreview::dataBackground(..., "app-bg")` at `src/gui/SpectrumWidget.cpp:6910`,
  `:7301`, `:15652`.
- **Computed lit values** (Qt's HSL, as `shiftL` computes them; spec section 5): raised
  fill is top `shiftL(base, +16)`, `base` at 0.55, bottom `shiftL(base, -12)`. Lit
  green `#306247` / `#254d38` / `#1d3d2c`, text `#bbe1c6`, rim `#3e7e5c`, rim top
  `#48946b`. Lit amber `#4c3b16` / `#33280f` / `#20190a`, text `#d8a55f`, rim
  `#6b5426`, rim top `#83672f`. Glass rim top `#4d89c1`. Plain raised button
  `#222227` to `#141417`; pressed is the same gradient flipped.
- **Role files:** `ChromeColours` (`ios/NereusApp/Main/ChromeColours.swift`, 81 values,
  private `rgb` at `:111`), `BandColours` (`ios/NereusApp/Band/BandColours.swift`,
  `slice()` and `withAlpha()` at `:51-60`), `DialColours`
  (`ios/NereusApp/Dial/DialColours.swift`), `ActivityColours`
  (`ios/Shared/Activity/ActivityColours.swift`, read by the 11 other files in
  `ios/Shared/Activity/` and by `ios/NereusActivity/StationActivityWidget.swift`),
  `ConnectChrome` (`ios/NereusApp/Connect/ConnectChrome.swift`, internal `rgb` at
  `:29`), `ConfirmationSheetChrome` (`ios/NereusApp/Shared/ConfirmationSheetChrome.swift`),
  `SpotColours` (`ios/NereusApp/Spots/SpotColours.swift`), `FreeDVColours`
  (`ios/NereusApp/FreeDV/FreeDVColours.swift`). There is no shared `rgb`: 13 copies
  and two `Color(hex:)` (`iPad/MeterDrawing.swift:15`, private in
  `Tools/ConnectionPerformanceView.swift:438`).
- **`ios/Shared`** compiles into the app (`ios/project.yml:55`) and the
  `NereusActivity` widget (`:85`); it holds `StationActivityAttributes.swift`,
  `ActivityAssets.xcassets` and `Activity/`.
- **Chrome controls are views, not styles.** `PanelButton`
  (`ios/NereusApp/Main/RxPanel.swift:250`, styles `.blue` `.dsp` `.red` `.amber`,
  radius 4, 86 call sites in 23 files). Others: `ConnectChrome.WideButton` (`:76`),
  `RowButton` (`:145`), `Pill` (`:243`); `AccessoryChrome.OperateButton`,
  `ChoiceButton`, `GreyedButton` (`:88`, `:116`, `:155`); `AccessoryFields.SwitchRow`,
  `SmallButton`, `ActionButton` (`:151`, `:207`, `:233`); `DevicesPage.smallButton`
  (`:321`) and `Card` (`:344`); `TxEqCurveParts.StepButton`, `PlainButton` (`:757`,
  `:786`); `SupportBundlePage.CategoryButton` (`:135`); `PerformanceButtonStyle` and
  `PerformanceCard` (`ConnectionPerformanceView.swift:411`, `:424`, the only existing
  `ButtonStyle`); `RotorHoldButton` (`RotorPage.swift:211`), `SliceListButton`
  (`SliceListSheet.swift:224`), `ModMonitorButton` (`ModMonitorView.swift:377`),
  `PttButton`, `TxPill`, `LinkChip`, `SharingChip`, `SliceSwitch`, `WideChipButton`,
  `DialStepButton`, `TakeTransmitButton`, `PaProfileActionButton`, `TurnBeamButton`;
  in the Live Activity `ActivityMuteButton`, `ActivityUnkeyButton`, `ActivityLinkChip`.
  There is no custom `ToggleStyle`.
- **Setup:** one `NavigationStack(path: $router.path)` (`SetupTab.swift:57`), root
  `List` (`:58`), category `List` (`:102`), described pages built by `DescribedPage`
  (`List` at `:37`, sections `:54-83`, header with `SetupTagBadge` `:73-81`) from
  `DescribedControl` rows (`Row` `:151-153`, `row(for:)` `:157-175`, `row(_:)`
  `:178-237`: `SetupSwitchRow`, `SetupNumberRow` (a stock `Stepper`), `slider()`
  `:299-341`, `choice()` `:344-384`, `TextField` `:210-216`, `colour()` `:387-406`,
  bordered `Button` `:220-225`, `valueRow` `:249-256`, `table()` `:412-422`, panels
  `:80-94`). Reasons are footnote `.secondary` lines (`:114-119`, `unavailableRow`
  `:284-296`, `:375-381`). Other `List`s: `NavigationPage.swift:17`,
  `BatteryAndSessionsPage.swift:19`, `AudioOnThisPhonePage.swift:101`,
  `DisplayOnThisPhonePage.swift:169`, `PttButtonsPage.swift:29`,
  `RotorSetupPage.swift:50` (with `.onDelete` at `:265`), `DataUsePage.swift:49`,
  `CoreLogsPage.swift:34`, `AboutAppPage.swift:24`, `LicensesView.swift:12`,
  `CfcBandsPanel.swift:98`, `:127`; a `Form` in the gradient editor sheet
  (`DescribedPage.swift:318`). `DevicesPage` is a `ScrollView` of cards (`:28`). No
  Setup file sets a list style or a row background; the app runs
  `.preferredColorScheme(.dark)` (`RootView.swift:152`). 10 native pages
  (`SetupTree.Page`, `SetupTree.swift:18-29`) and up to 52 Core-described ones.
- **iPad:** Setup has no size-class branch. `horizontalSizeClass` is read at
  `RootView.swift:16` and `MainScreen.swift:57`, `:83`; `SetupTab` is made at
  `RootView.swift:123`; the router is opened from `RootView.swift:114`
  (`open(.displayOnThisPhone)`) and `:130` (`open(describedPage:)`).
- **NereusKit:** `BandRenderer.background` `#0A0A14`
  (`ios/NereusKit/Sources/NereusBand/BandRenderer.swift:45`, used for the clear color
  `:416`, the dim `:888`, the waterfall `:1411`, the uniforms `:1550`).
  `BandDisplaySettings.backgroundColour` (`BandDisplaySettings.swift:168`) is the user's
  Display background setting.
- **Tests that pin today's colors:** `ConnectionFlowShotTests.swift:780-786`
  (`#0D1B28`), `BandMarksShotTests.swift:125-131` (`#0F1420`), `:176-181` (`#405060`),
  `ToolsPagesTests.swift:305-318` (role identity), `SpotScreenTests.swift:418-457`
  (`ConnectChrome.pill*` identity), NereusKit `DesktopValuesTests.swift:74-77`
  (`#101520`, unchanged by this plan), `BandRendererTests.swift:203`
  (`backgroundGrey = 14`, tolerance 12).
- **Shot suites** (all in `NereusSDRTests`, each writes PNGs only when its variable is
  set): `BandFlagShotTests`, `BandMarksShotTests`, `CatalogueRangesShotTests`,
  `ConnectionFlowShotTests`, `DialShotTests`, `DiversityApprovedShotTests`,
  `DropSheetShotTests`, `FilterReasonShotTests`, `FlagButtonsShotTests`,
  `IPadShotTests` (iPad simulator only), `KeyedMeterOverlayShotTests`,
  `LiveActivityShotTests`, `LongSessionShotTests`, `MainScreenShotTests`,
  `ModMonitorShotTests`, `ModesShotTests`, `RadeFlagShotTests`, `SMeterFaceShotTests`,
  `ThreeDViewShotTests`, `ToolsPagesShotTests`, `TransmitDisplayShotTests`,
  `TuningShotTests`, `TxEqCurveShotTests`, `TxPanelApprovedShotTests`,
  `WaterfallTurnShotTests`. Variables: `NEREUS_MAIN_SHOTS`, `NEREUS_BAND_SHOTS`,
  `NEREUS_CONNECT_SHOTS`, `NEREUS_IPAD_SHOTS`, `NEREUS_CATALOGUE_SHOTS`,
  `NEREUS_DIVERSITY_SHOTS`, `NEREUS_AMMOD_SHOTS`, `NEREUS_MODES_SHOTS`,
  `NEREUS_METER_SHOTS`, `NEREUS_V3D_SHOTS`, `NEREUS_54F_SHOTS`. There is no pixel gate;
  the pictures are compared by eye with the boards.
- **UI tests:** `NereusSDRUITests` (`ios/NereusApp/UITests/`, 15 files), including
  `IPadUITests`, `RotorUITests`, `SoundPanelUITests` (finds `app.switches["soundMute"]`
  at `:34`). None covers Split View or a compact iPad.
- **Provenance check:** `scripts/verify-ios-provenance.py`, `UPSTREAM_NAMES` at
  `:64-67`, failures print `FAIL <file>: <rule>: ...` and exit 1 (`:356-361`); its tests
  are `tests/compliance/test_verify_ios_provenance.py`.
- **Docs:** the R-IOS table in `docs/architecture/2026-09-23-iphone-app-design.md`
  section 6.2 (rows `:1115-1150`, last `R-IOS-48`, columns `| ID | Requirement | Evidence |`).
  Phone images: `docs/manual/images/README.md:112-131` (each image and the test that
  made it), `docs/manual/images/capture-provenance.json`,
  `website/public/assets/img/iphone-*.png` with sources in
  `website/screenshot-sources.md:5-27`, copies in `website/public/manual/images/`.

## File Structure

**Create**

| Path | Responsibility |
| --- | --- |
| `ios/Shared/WorkspacePalette.swift` | The desktop tokens, the phone's extra steps, the computed lit values, the radii, each with its cite |
| `ios/Shared/WorkspaceStyles.swift` | `WorkspaceLook`, `WorkspaceFace`, `WorkspaceButtonStyle`, `WorkspaceSwitchStyle`, the panel and inset modifiers |
| `ios/NereusApp/Tests/WorkspacePaletteTests.swift` | Lit values, kept colors (R-IOS-54), role-to-token checks (R-IOS-49) |
| `ios/NereusApp/Tests/SetupWorkspaceShotTests.swift` | Setup pages at the default size and accessibility XL, iPhone and iPad |
| `ios/NereusApp/UITests/SetupSplitUITests.swift` | iPad regular and compact Setup, VoiceOver-visible switch, rotor swipe to delete |
| `ios/NereusApp/Setup/SetupSplitView.swift` | The iPad's list-beside-the-page Setup |
| `scripts/verify-ios-workspace-colours.py` | Fails on an old navy hex in `ios/` outside the kept list |
| `tests/compliance/test_ios_workspace_palette.py` | Swift tokens equal the desktop header; the navy check's own cases |

**Modify**

| Path | Change |
| --- | --- |
| `scripts/verify-ios-provenance.py`, `tests/compliance/test_verify_ios_provenance.py` | Longpath and OE5SOS refused except in the two workspace files |
| `.github/workflows/ci.yml` | Run the navy check beside the provenance step |
| Role files listed above | Point at the tokens per appendix A |
| The chrome control views listed above | Draw with the treatments |
| 31 view files with loose literals (Task 4 lists them) | Literals to roles |
| `ios/Shared/Activity/*` | Section 8 of the spec |
| `ios/NereusKit/Sources/NereusBand/BandRenderer.swift` | Band ground `#08080a` |
| `ios/NereusApp/Setup/*` row views and page containers, `SetupTab.swift`, `SetupRouter.swift`, `RootView.swift` | Panels, rows, large type, the iPad split |
| Tests that pin navy values | New values |
| `docs/architecture/2026-09-23-iphone-app-design.md` | R-IOS-49 to R-IOS-54 rows |
| Manual and website phone images and their provenance files | Recaptured |

## Task order and flags

Tasks run in lanes (JJ, 2026-10-21: "use as many lanes as practical"), each lane one
implementer at a time and no two lanes sharing a file: the main lane runs Tasks 1, 2, 3
and 4 in the working tree; Task 5 runs in its own worktree from the start; after Task 1,
Task 6 and then Tasks 7 and 8 run in their own worktrees; Task 9 runs last, on the merged
branch. To keep the lanes apart, `DevicesPage`'s buttons and cards moved from Task 3 to
Task 7. One simulator test run at a time across all lanes (the shared lock). The branch merges to main as a
whole, so no screen ever ships half navy. Nothing here touches the link, audio,
commands or transmit behavior; the keyed PTT and UNKEY change color only.

Calls settled in this plan, for JJ's veto (none changes a spec decision):

1. The Setup switch style is set once at the app's root, so every SwiftUI `Toggle` in
   the app (the Sound panel's mute among them) is the blue switch, not only Setup's. A
   stock green iOS switch would read as the desktop's "running" green.
2. A disabled `PanelButton` draws as the raised face at 0.4, as the spec's disabled
   rule says, instead of today's separate grey colors (`buttonOff`).
3. Setup's container (a restyled `List` or a scroll view of panels) is chosen in Task 7
   by a one-page trial against the criteria written there, and the choice is recorded
   in the ledger.
4. The navy check runs in `ci.yml`'s `compliance` job (a Linux runner, every pull
   request) rather than in `ios.yml`, whose app job holds a macOS runner for over an
   hour.
5. The release that ships this writes its CHANGELOG lines; this plan adds none.

Pending on JJ's devices (spec section 10): the lock screen, StandBy and the Dynamic
Island, listening and keyed; Setup at his own text size; the iPad in full screen and in
a narrow Split View.

## Task 1: The tokens, the treatments and the checks that hold them

**Requirements:** R-IOS-49 (token half), R-IOS-50, R-IOS-54 (test), spec sections 4, 5,
6 points 1 and 3, 11. Adds the six rows R-IOS-49 to R-IOS-54 to the iPhone design
doc's section 6.2 table, with the requirement and evidence text exactly as the spec's
section 10 table gives them.

**Files:**
- Create: `ios/Shared/WorkspacePalette.swift`, `ios/Shared/WorkspaceStyles.swift`,
  `ios/NereusApp/Tests/WorkspacePaletteTests.swift`,
  `tests/compliance/test_ios_workspace_palette.py`
- Modify: `scripts/verify-ios-provenance.py`,
  `tests/compliance/test_verify_ios_provenance.py`,
  `docs/architecture/2026-09-23-iphone-app-design.md` (six rows after `R-IOS-48`)

**Interfaces:**
- Consumes: nothing.
- Produces, in `ios/Shared/WorkspacePalette.swift`, `enum WorkspacePalette` with
  `static let` `Color` values:
  - Desktop tokens, one per line, named as the desktop name without the `k` and with a
    lower-case first letter: `appBg`, `panelBg`, `statusBarBg`, `statusBarBorder`,
    `textPrimary`, `textSecondary`, `textTertiary`, `textScale`, `textInactive`,
    `titleText`, `border`, `borderSubtle`, `borderMuted`, `insetBg`, `insetBorder`,
    `greenBg`, `liveGreen`, `amberBg`, `amberText`, `amberBorder`, `dspToggleBorder`,
    `badgeTxBg`, `txRed`, `redBg`, `redText`, `redBorder`, `gaugeDanger`,
    `glassPanelTop`, `glassPanelBot`, `glassBtnTop`, `glassBtnBot`, `glassSelTop`,
    `glassSelBot`, `glassSelBorder`, `glassSelText`. Each is written
    `static let <name> = rgb(0xRRGGBB) // k<Name>, WorkspacePalette.h:<line>; From Longpath src/gui/StyleConstants.h:<line> [@551576e3]`,
    with the Longpath part copied from that desktop line; the Python drift test reads
    this exact form.
  - Computed values (comment: the formula and `WorkspaceComponents.cpp` line):
    `litGreenTop` `#306247`, `litGreenMid` `#254d38`, `litGreenBottom` `#1d3d2c`,
    `litGreenText` `#bbe1c6`, `litGreenRim` `#3e7e5c`, `litGreenRimTop` `#48946b`,
    `litAmberTop` `#4c3b16`, `litAmberMid` `#33280f`, `litAmberBottom` `#20190a`,
    `litAmberRimTop` `#83672f`, `glassSelBorderTop` `#4d89c1`.
  - The phone's own steps (comment: spec section 13 or appendix A):
    `textBright` `#ececf0`, `accentFill` `#4a7ba8`, `accentText` `#5e8db8`,
    `tabChosen` `#8fb4d9`, `noticeWarn` `#a8853f`, `keyedRim` `#e0a3a4`.
  - `static let chipRadius: CGFloat = 7`, `static let panelRadius: CGFloat = 10`
    (`kGlassChipRadius`, `kGlassPanelRadius`), `static let raisedMidStop: CGFloat = 0.55`,
    `static let disabledOpacity = 0.4`.
  - `static func rgb(_ value: UInt32) -> Color` (internal, so the role files can use
    it for kept data colors).
- Produces, in `ios/Shared/WorkspaceStyles.swift`:
  - `enum WorkspaceLook { case raised, glass, litGreen, litAmber, keyed, danger }`
  - `struct WorkspaceFace: View` with `init(_ look: WorkspaceLook, radius: CGFloat = WorkspacePalette.chipRadius, pressed: Bool = false)`:
    the fill, the 1 point rim and the lighter top edge of that look, per the spec's
    section 5 table.
  - `func workspaceForeground(_ look: WorkspaceLook) -> Color` (text color per look).
  - `struct WorkspaceButtonStyle: ButtonStyle` with
    `init(_ look: WorkspaceLook, radius: CGFloat = WorkspacePalette.chipRadius)`: draws
    the label over `WorkspaceFace`, pressed shows the pressed face, and a disabled
    button (`@Environment(\.isEnabled)`) is the same face at
    `WorkspacePalette.disabledOpacity`.
  - `struct WorkspaceSwitchStyle: ToggleStyle`: the iOS switch shape (capsule 51 by
    31 points, round knob) with on as the glass and off as the inset; the label on the
    leading side as a stock `Toggle` lays it out.
  - `extension View { func workspacePanel(radius: CGFloat = WorkspacePalette.panelRadius) -> some View; func workspaceInset(radius: CGFloat = WorkspacePalette.chipRadius) -> some View }`.

**Acceptance:**
- `tests/compliance/test_ios_workspace_palette.py` reads
  `src/gui/styles/WorkspacePalette.h` (`constexpr auto k<Name> = "#rrggbb";`) and
  `ios/Shared/WorkspacePalette.swift`, and fails if any Swift token line's hex differs
  from the header's value for the named `k<Name>`, or if any of the 35 desktop names
  above is missing. A case changes one hex in a copy of the Swift file (in a temporary
  folder) and sees the check fail.
- `WorkspacePaletteTests`: each computed lit value equals the hex above; resolving
  `WorkspacePalette.txRed` gives RGB `(0xC2, 0x5A, 0x5C)`; `chipRadius == 7`,
  `panelRadius == 10`.
- `WorkspacePaletteTests` pins the kept colors (R-IOS-54): `BandColours.frequency`
  `#00e5ff`, `bandwidth` `#00c8ff`, `rxAntenna` `#4488ff`, `timestamp` `#c8dcff`,
  `DialColours.indexGlow` `#00e5ff`, `indexTick` `#c8f8ff`, `ChromeColours.txFilterEdge`
  `#ff7833`, and `BandColours.slice(_:)` for each slice letter against today's values.
- `WorkspaceSwitchStyle`: a `Toggle` with this style, rendered in a test host, keeps the
  `isToggle` accessibility trait and its on/off value; flipping it through its binding
  changes the face between glass and inset.
- `scripts/verify-ios-provenance.py`: a Swift file in `ios/` (outside
  `ios/Shared/WorkspacePalette.swift` and `ios/Shared/WorkspaceStyles.swift`) that names
  Longpath or OE5SOS fails with the `upstream` rule; the same words in those two files
  pass. New cases in `tests/compliance/test_verify_ios_provenance.py` cover both. The
  allowance's comment cites JJ's ruling of 2026-10-21 and the spec's section 11.
- The app builds for the simulator and the `NereusActivity` target compiles the new
  `ios/Shared` files (both compile `ios/Shared`).

**Verification:** ordinary feature (new values, no view yet reads them). Unit tests on
the simulator: `-only-testing:NereusSDRTests/WorkspacePaletteTests`. Python:
`python3 -m pytest tests/compliance/test_ios_workspace_palette.py tests/compliance/test_verify_ios_provenance.py -v`
and `python3 scripts/verify-ios-provenance.py` ("ios/ is clean"). Build both targets
with the `build-for-testing` command.

**Execution note (advisory):** opus. No prerequisites. Touches no networking, config or
device path.

- [ ] **Step 1:** Write `WorkspacePalette.swift` in the exact line form above, values
  copied from `WorkspacePalette.h` with each line's cite.
- [ ] **Step 2:** Write `WorkspaceStyles.swift` per the interface and the spec's section
  5 table, the raised gradient with three stops (top, base at 0.55, bottom).
- [ ] **Step 3:** Write the drift test and the provenance allowance with their cases.
- [ ] **Step 4:** Write `WorkspacePaletteTests`, add the six R-IOS rows, run the checks.

## Task 2: The role files point at the tokens

**Requirements:** R-IOS-49 (role half), W1, W2, W5, W7 (the switch at the root), spec
section 6 point 2, appendix A.

**Files:**
- Modify: `ios/NereusApp/Main/ChromeColours.swift`, `ios/NereusApp/Band/BandColours.swift`,
  `ios/NereusApp/Dial/DialColours.swift`, `ios/NereusApp/Connect/ConnectChrome.swift`
  (the color values only; its button views are Task 3),
  `ios/NereusApp/Shared/ConfirmationSheetChrome.swift`,
  `ios/NereusApp/Spots/SpotColours.swift`, `ios/NereusApp/FreeDV/FreeDVColours.swift`,
  `ios/NereusApp/App/RootView.swift` (`.toggleStyle(WorkspaceSwitchStyle())` at the
  root)
- Test: `WorkspacePaletteTests` (role checks added),
  `ConnectionFlowShotTests.swift:780-786`, `BandMarksShotTests.swift:125-131`,
  `:176-181`, `ToolsPagesTests.swift:305-318`

**Interfaces:**
- Consumes: `WorkspacePalette` (Task 1).
- Produces: every existing role name keeps its name and type, so no caller changes.
  Roles whose appendix A value is a treatment (for example `buttonOnBlue` "glass",
  `buttonOnGreen` "lit green", `iconOpenBackground` "glass", `sliceChosen` "glass")
  hold that treatment's flat middle color for any caller still drawing them flat
  (glass `glassSelBot`, lit green `litGreenMid`, lit amber `litAmberMid`, raised
  `glassBtnBot`); Task 3 moves those callers to the treatment.

**Acceptance:**
- Every role in appendix A resolves to the appendix's workspace value or, for a
  treatment, the flat middle color named above. `WorkspacePaletteTests` checks at
  least: `ChromeColours.bar == WorkspacePalette.statusBarBg`, `.page == .appBg`,
  `.text == .textPrimary`, `.textDim == .textSecondary`, `.accent == .accentFill`,
  `.linkLost == .redBorder`, `.gaugeRed == .gaugeDanger`, `.pttEdge == .txRed`,
  `.noticeInfo == .accentText`, `BandColours.txAntenna == .txRed`,
  `BandColours.levelHigh == .liveGreen`, `DialColours.index == .accentFill`.
- The kept colors of W2 are unchanged (Task 1's pin test still passes).
- `ConnectChrome.pillOn`, `pillStale`, `pillOff`, `pillUnknown` keep their identity
  checks in `SpotScreenTests` (their values: on `liveGreen`, stale `noticeWarn`, off
  `redBorder`, unknown `borderMuted`).
- A role the appendix does not list maps by the generator's table
  (`docs/architecture/2026-10-20-phone-workspace-look/make_board.py:51-89`), except
  that W5 wins over the table for a red that is not transmit: `ConnectChrome.bad`,
  `badBorder`, `badText`, `pillOff` and the connect screen's stop button take
  `redBg` / `redBorder` / `redText`, never `txRed`. The report lists every role the
  appendix did not give, with its new value.
- The three pixel-pinned tests look for the new values (`#0D1B28` becomes the
  workspace value of `ConnectChrome.rowFill`; `#0F1420` becomes `insetBg`; `#405060`
  becomes `#58585e`), and are named in the report.
- Every Swift `Toggle` in the app draws with `WorkspaceSwitchStyle`;
  `SoundPanelUITests` still finds `app.switches["soundMute"]` and flips it.
- No private `rgb` helper remains in a role file that now reads only tokens.

**Verification:** styling (`ui-verification`: a shared theme reaches every screen).
Unit: `WorkspacePaletteTests`, `SpotScreenTests`, `ToolsPagesTests`. Shot suites run
to a scratch folder and looked at: `MainScreenShotTests`, `BandFlagShotTests`,
`ConnectionFlowShotTests`, `BandMarksShotTests`, `DialShotTests` (iPhone 17). UI:
`-only-testing:NereusSDRUITests/SoundPanelUITests`. Screens still mix treatments and
flat fills after this task; that is expected until Task 3.

**Execution note (advisory):** opus. Needs Task 1.

- [ ] **Step 1:** Rewrite each role file's values per appendix A, reading tokens; data
  roles stay literal through `WorkspacePalette.rgb`.
- [ ] **Step 2:** Set the switch style at the root of `RootView`.
- [ ] **Step 3:** Update the pinned tests and add the role checks.

## Task 3: The shared controls draw with the treatments

**Requirements:** R-IOS-50, W3, W4, W5, W11, spec sections 5 and 9.

**Files:**
- Modify: `ios/NereusApp/Main/RxPanel.swift` (`PanelButton`),
  `ios/NereusApp/Connect/ConnectChrome.swift` (`WideButton`, `RowButton`, `Pill`),
  `ios/NereusApp/Accessories/AccessoryChrome.swift`,
  `ios/NereusApp/Accessories/AccessoryFields.swift`,
  `ios/NereusApp/Tools/TxEqCurveParts.swift` (`StepButton`, `PlainButton`),
  `ios/NereusApp/Tools/SupportBundlePage.swift` (`CategoryButton`),
  `ios/NereusApp/Tools/ConnectionPerformanceView.swift` (`PerformanceButtonStyle`,
  `PerformanceCard`), `ios/NereusApp/Accessories/RotorPage.swift` (`RotorHoldButton`),
  `ios/NereusApp/Main/SliceListSheet.swift` (`SliceListButton`),
  `ios/NereusApp/Main/ModMonitorView.swift` (`ModMonitorButton`), and the files holding
  `PttButton`, `TxPill`, `LinkChip`, `SharingChip`, `SliceSwitch`, `WideChipButton`,
  `DialStepButton`, `TakeTransmitButton`, `PaProfileActionButton`, `TurnBeamButton`,
  the toolbar's open button and the tab bar's chosen tab; the `PanelButton` call sites
  whose style changes under W3
- Test: `ToolsPagesTests.swift:305-318`, `TxPanelApprovedShotTests`,
  `FlagButtonsShotTests`, `ModesShotTests`

**Interfaces:**
- Consumes: `WorkspaceLook`, `WorkspaceFace`, `WorkspaceButtonStyle`,
  `workspacePanel`, `workspaceInset`, `WorkspacePalette` (Task 1); the roles (Task 2).
- Produces: `PanelButton.Style` keeps its four cases; lit `.blue` draws `.glass`,
  `.dsp` draws `.litGreen`, `.amber` draws `.litAmber`, `.red` draws `.keyed`; unlit
  draws `.raised`; radius `chipRadius`. No public signature changes.

**Acceptance:**
- `PanelButton` lit in each style draws the matching `WorkspaceLook` face; disabled
  draws the raised face at 0.4 (plan call 2); the amber warning dot uses `amberText`.
- W3: each `PanelButton` (and other lit control) that has a desktop counterpart takes
  the desktop's look for that control. The implementer finds each counterpart in the
  desktop source (`src/gui/`, the control's lit style) and lists every one in the
  report as phone file:line, desktop file:line, look. At least: SPEAKERS and PHONES
  are `.dsp` (lit green, as on the desktop's TX panel); MOX and TUNE are `.red`;
  mode, filter, band, AGC and antenna choices stay `.blue` (glass).
- The PTT at rest is the danger set with a `txRed` edge; keyed is `.keyed`; "on the
  air elsewhere" uses the `pttElsewhere*` roles.
- Buttons, chips and fields round at 7, panels and cards at 10; the flag keeps its own
  shape.
- The toolbar's open button is glass; the chosen tab's icon and label are `tabChosen`.
- Accessibility labels, identifiers, traits (`.isSelected` on a lit button) and actions
  are unchanged; `TxPanelUITests`, `IPadUITests` and `TabBarUITests` pass.
- `ToolsPagesTests` checks the performance buttons against the treatment instead of
  `buttonOnBlue` / `buttonOff`.

**Verification:** styling plus interaction (`ui-verification`). Unit:
`ToolsPagesTests`. UI: `TxPanelUITests`, `TabBarUITests`. Shot suites to a scratch
folder, looked at against `workspace-board.html` and `workspace-board-rest.html`:
`TxPanelApprovedShotTests`, `FlagButtonsShotTests`, `ModesShotTests`,
`ConnectionFlowShotTests`, `ToolsPagesShotTests`, `TxEqCurveShotTests`,
`ModMonitorShotTests`, `MainScreenShotTests`, `DropSheetShotTests`, magnifying one lit
green and one glass button to check the gradient and the lighter top edge are there.

**Execution note (advisory):** opus. Needs Tasks 1 and 2. The keyed PTT is the
transmit control: color only, its action and gesture paths must not move.

- [ ] **Step 1:** Rebuild `PanelButton` on `WorkspaceFace`.
- [ ] **Step 2:** Move each other chrome control listed to `WorkspaceButtonStyle`,
  `WorkspaceFace`, `workspacePanel` or `workspaceInset`.
- [ ] **Step 3:** Apply W3 call site by call site and write the counterpart list.
- [ ] **Step 4:** Update `ToolsPagesTests`, run the UI tests and the shot suites.

## Task 4: Loose literals move to roles, and the navy check

**Requirements:** R-IOS-49 (check half), W1, W2, W5, spec sections 6 point 4 and 9.

**Files:**
- Modify (chrome literals to roles or tokens; data literals kept and listed):
  `Band/FlagMenus.swift`, `Band/ForeignSliceLabel.swift`, `Band/FrequencyPad.swift`
  (`:148-153`), `Band/JumpViews.swift`, `Band/LevelCalBandLine.swift` (its `dot` is a
  band mark: kept), `Band/StackNotice.swift`, `Band/TuneStepMenu.swift`,
  `Band/VfoFlagView.swift` (`:194`, `:199`, `:483`, `:535`), `Band/WideChip.swift`,
  `Connect/FifthDeviceSheet.swift` (`:120`), `Connect/LinkLostBanner.swift`
  (`:100-154`), `Main/SliceListSheet.swift` (`:199-214`; RADE mode `#B070FF` kept),
  `Main/TxStageMeters.swift`, `Main/KeyedMeterGauge.swift`,
  `Main/DisplaySheetStackRows.swift`, `Main/ModMonitorSettingsSheet.swift`,
  `Main/ModMonitorView.swift` (chrome at `:270-282`, `:354-393`, `:448`, `:540`, `:559`,
  `:568`, `:741`, `:750`; the meter zones, scopes and vintage dial are kept),
  `Audio/SoundPanel.swift` (`:139-146`), `Shared/ValuePad.swift` (`:151-156`),
  `Shared/NoticeBanner.swift`, `Shared/ReceiverTakenOverlay.swift`,
  `Tools/TxEqCurveParts.swift` (`:16-48` chrome; plot series kept),
  `Accessories/RotorDial.swift` (`ring`, `inner`, `muted`, `staleHeading`, `stop*`
  become roles; `amber`, `target`, `arrived`, `endStop`, `beam` are the instrument's
  and kept), `iPad/ClassicMeterFace.swift` (`background`, `scale`, `caption` become
  roles; the scale colors are kept), `iPad/VintageMeterFace.swift` (`surround`) (all
  paths under `ios/NereusApp/`)
- Create: `scripts/verify-ios-workspace-colours.py`
- Modify: `tests/compliance/test_ios_workspace_palette.py` (the navy check's cases),
  `.github/workflows/ci.yml` (a step `python3 scripts/verify-ios-workspace-colours.py`
  right after the provenance step at `:114-121`)

**Interfaces:**
- Consumes: the roles (Task 2), the treatments (Task 3).
- Produces: `scripts/verify-ios-workspace-colours.py [--root PATH]`. It scans `.swift`
  files under `ios/NereusApp`, `ios/Shared` and `ios/NereusActivity` (not `Tests`,
  `UITests` or `NereusKit`) for hex colors written as `rgb(0xRR, 0xGG, 0xBB)`,
  `0xRRGGBB` and `"#RRGGBB"`, and fails, printing `FAIL <file>:<line>: <hex>`, on any
  value in its `NAVY` set unless the `KEPT` table lists that file and value with a
  reason. Exit 0 prints `ios/ chrome is in the workspace look`.
  - `NAVY` is the old chrome set, every key of the map in
    `docs/architecture/2026-10-20-phone-workspace-look/make_board.py:51-89` (copied
    into the script, not imported), plus `#0a0a14`, `#0f0f1a`, `#00b4d8`, `#0070c0`,
    `#ff4444`, `#cc2222` named again for clarity.
  - `KEPT` starts with the data colors this task keeps that happen to be in `NAVY`,
    each with the reason ("meter zone", "spot tier", and so on).

**Acceptance:**
- `python3 scripts/verify-ios-workspace-colours.py` passes on the finished tree.
- Its cases: a temporary tree with a view writing `rgb(0x1A, 0x2A, 0x3A)` fails, citing
  the file and line; the same value in a file and value listed in `KEPT` passes; a
  `Tests/` file with the value passes.
- Each kept literal in the files above is listed in the report with its reason, and the
  report confirms none of them is chrome.
- `FrequencyPad` and `ValuePad` draw the same faces (they carried identical values
  before and still do, now from roles).
- No visible change to a kept data color; the shot suites below show it.

**Verification:** styling plus a repository check. Python:
`python3 -m pytest tests/compliance/ -v`, `python3 scripts/verify-ios-workspace-colours.py`.
Shot suites to a scratch folder, looked at against the boards: `BandFlagShotTests`,
`TuningShotTests`, `ConnectionFlowShotTests`, `KeyedMeterOverlayShotTests`,
`ModMonitorShotTests`, `TxEqCurveShotTests`, `SMeterFaceShotTests`,
`ToolsPagesShotTests`, `LongSessionShotTests`, `MainScreenShotTests`. UI:
`RotorUITests`, `SpotTurnBeamUITests`.

**Execution note (advisory):** opus. Needs Task 3. Edits `.github/workflows/ci.yml`:
one added step, no other change to the workflow.

- [ ] **Step 1:** Move each chrome literal to a role or token; keep the data literals.
- [ ] **Step 2:** Write the check and its cases, add the CI step.
- [ ] **Step 3:** Run the check, the Python tests and the shot suites.

## Task 5: The band's ground

**Requirements:** spec section 6 (NereusKit paragraph), section 13 item 10, W1, W2.

**Files:**
- Modify: `ios/NereusKit/Sources/NereusBand/BandRenderer.swift` (`background` at `:45`
  and its doc comment)
- Test: NereusKit `BandRendererTests.swift` (`:203`), `DesktopValuesTests.swift`

**Interfaces:**
- Consumes: nothing (NereusKit cannot import the app's palette; the value is written
  here with its desktop cite).
- Produces: `BandRenderer.background == SIMD4<Float>(8.0 / 255, 8.0 / 255, 10.0 / 255, 1)`,
  commented as the desktop's workspace data ground `kAppBg` `#08080a`
  (`src/gui/styles/WorkspacePalette.h:105`, drawn by
  `WorkspacePreview::dataBackground(..., "app-bg")`, `src/gui/SpectrumWidget.cpp:7301`).

**Acceptance:**
- The band's clear color, its dim, the waterfall's empty rows and the shader uniform all
  use the new ground (they read `background` today).
- `scaleBackground` `#101520`, `dbmLabelColour` `#80A0B0`, the 3D scale column and
  `BandDisplaySettings.backgroundColour`'s default `#0A0A14FF` are unchanged, and
  `DesktopValuesTests` still pins `#101520`.
- `BandRendererTests`' background check (`backgroundGrey`, tolerance 12) passes with
  the new ground and is tightened to the new value of 8 if its tolerance would also have
  passed the old one; the report says which.

**Verification:** ordinary change in a frozen package's renderer. `ios/scripts/swift-test.sh --jobs 2 --filter BandRendererTests`,
then `--filter DesktopValuesTests`, then the app suites that draw the band:
`BandFlagShotTests`, `WaterfallTurnShotTests`, `ThreeDViewShotTests`, looked at.

**Execution note (advisory):** opus. Needs nothing, but runs after Task 4 so the band's
chrome and its ground land together on the branch.

- [ ] **Step 1:** Change the value and its comment.
- [ ] **Step 2:** Run the NereusKit tests and the band suites.

## Task 6: The Live Activity

**Requirements:** R-IOS-53, W5, W10, spec section 8.

**Files:**
- Modify: `ios/Shared/Activity/ActivityColours.swift`, and the views in
  `ios/Shared/Activity/` that draw the card, the keyed card, the link chip, the TX
  badge and clock, UNKEY, the speaker, Cancel and Reconnect, and the level bar
- Test: `ios/NereusApp/Tests/LiveActivityShotTests.swift`

**Interfaces:**
- Consumes: `WorkspacePalette`, `WorkspaceFace` (Task 1). `ios/Shared` is compiled into
  the widget, so the widget now reads the same values; `ActivityColours` keeps its
  role names and stops repeating the app's values.
- Produces: nothing new.

**Acceptance (each as the spec's section 8 table):**
- Card: the panel gradient at 0.92 with a `border` edge. Keyed card: `badgeTxBg` at
  0.93 with a `redBorder` edge. Lost card edge: `redBorder` at 0.7.
- Text roles: `textPrimary`, `textBright`, `textSecondary`.
- Link chip: inset, radius 7, live dot `liveGreen`.
- TX badge and clock: `badgeTxBg`, edge `txRed`, text `keyedRim`.
- UNKEY: the keyed face (`txRed` with a lighter top, rim `keyedRim`, white text).
- Speaker: raised; muted: lit amber. Cancel: danger. Reconnect: glass.
- Signal bar: `accentFill` to S9, `liveGreen` above. Time-out words: `amberText`.
- Frequency, bandwidth and slice letter unchanged. The island keyline is cyan, or
  `txRed` while keyed. StandBy's ground is `panelBg`.
- The card's corner and the island's shape are untouched.
- `LiveActivityShotTests` keeps its existing checks; any check pinning a navy value is
  rewritten and named in the report.

**Verification:** styling on a surface the system draws. Shot suite
`LiveActivityShotTests` with `TEST_RUNNER_NEREUS_MAIN_SHOTS` set, looked at against
`lock-board.html` on its three wallpapers, magnifying UNKEY and the keyed card's edge.
Device (pending, JJ): the lock screen, StandBy and the Dynamic Island, listening and
keyed, on his iPhone 18 Pro Max.

**Execution note (advisory):** opus. Needs Task 1. Builds the `NereusActivity` target
as well as the app.

- [ ] **Step 1:** Point `ActivityColours` at the tokens.
- [ ] **Step 2:** Move the card's controls to the faces per the table.
- [ ] **Step 3:** Run the shot suite and look at it.

## Task 7: Setup in the workspace panels

**Requirements:** R-IOS-51, W6, W7, W9, spec section 7.1, 7.2 and 7.4.

**Files:**
- Modify: `ios/NereusApp/Setup/SetupTab.swift`, `DescribedPage.swift`,
  `DescribedControl.swift`, `SetupSwitchRow.swift`, `SetupNumberRow.swift`,
  `SetupChoiceRow.swift`, `SetupRow.swift`, `SetupTagBadge.swift`, and each native
  page container (`NavigationPage.swift`, `BatteryAndSessionsPage.swift`,
  `AudioOnThisPhonePage.swift`, `DisplayOnThisPhonePage.swift`, `PttButtonsPage.swift`,
  `RotorSetupPage.swift`, `DataUsePage.swift`, `CoreLogsPage.swift`,
  `AboutAppPage.swift`, `LicensesView.swift`, `CfcBandsPanel.swift`, `DevicesPage.swift`,
  whose `smallButton` and `Card` also take the treatments here, moved from Task 3)
  (all under `ios/NereusApp/Setup/`)
- Create: `ios/NereusApp/Tests/SetupWorkspaceShotTests.swift`
- Test: `SetupRenderingTests`, `RotorUITests`

**Interfaces:**
- Consumes: `workspacePanel`, `workspaceInset`, `WorkspaceButtonStyle`,
  `WorkspaceSwitchStyle`, `WorkspacePalette` (Task 1).
- Produces: one Setup page container used by every page,
  `struct SetupPageContainer<Content: View>: View` (in `SetupTab.swift` or its own
  file), and one section form, `SetupPanelSection` (title, optional tag, optional note,
  rows), so Task 8 can show any page in the split's right side without change.

**Acceptance:**
- The container decision (plan call 3): build Navigation and the Rotor page both as a
  restyled `List` (hidden scroll background, panel row backgrounds, section headers in
  `titleText`) and as a scroll view of `workspacePanel` sections. Keep the one that
  (a) matches setup-board.html's "Workspace" frames (each section reads as one panel
  with a 10 point corner and a `border` edge), (b) keeps VoiceOver reading rows in
  page order, (c) keeps Dynamic Type at accessibility sizes without clipping, and
  (d) keeps the rotor page's swipe to delete. Record the choice and the evidence in the
  ledger, then apply it to every page.
- Every page: the `appBg` ground; each section a panel; section titles in `titleText`;
  Core, This phone and Both tags are chips with radius 7; the picked row of a choice
  list is glass with its check mark kept; switches are `WorkspaceSwitchStyle`; a number
  row shows its value in an inset field with raised minus and plus, and tapping the
  value still opens the number pad; a menu row is a raised button showing the value
  and the up-down chevrons; sliders have an inset track filled in `glassSelBorder`;
  footers and reasons keep their words.
- Disabled: every control kind (switch, number, slider, menu, text field, color,
  button) that cannot act is at 0.4 in place with its reason line under it. Today only
  numbers and switches dim; the others gain the 0.4.
- Large type (W9): at accessibility sizes (`dynamicTypeSize.isAccessibilitySize`), a
  number row and a menu row put the value and its buttons under the label; switches
  stay at the right.
- Kept: the back button, the large page title, swipe back, every word on every page,
  every accessibility identifier.
- `SetupWorkspaceShotTests` draws Navigation, AGC/ALC and one described page with a
  greyed row, at the default size and at accessibility XL, on the iPhone, with
  `TEST_RUNNER_NEREUS_MAIN_SHOTS` set; its checks assert the large-type layout (the
  value field's frame is below the label's).
- `RotorUITests` still deletes a rotor by swiping.

**Verification:** layout and interaction (`ui-verification`: one area, every page in
it). Unit: `SetupRenderingTests`, `SetupWorkspaceShotTests`. UI: `RotorUITests`,
`SoundPanelUITests`. Shots looked at against setup-board.html. Device (pending, JJ):
Setup at his own text size.

**Execution note (advisory):** opus. Needs Task 1; runs in its own worktree beside the
main lane. The root switch style is Task 2's: shots host each page with
`.toggleStyle(WorkspaceSwitchStyle())` as the root will, and `RootView.swift` is not
edited here. The largest task; the container trial comes first so the rest is built once.

- [ ] **Step 1:** The container trial on two pages; record the choice.
- [ ] **Step 2:** Build `SetupPageContainer` and `SetupPanelSection`; move every page to
  them.
- [ ] **Step 3:** Restyle the rows, the disabled state and the large-type layout.
- [ ] **Step 4:** Write `SetupWorkspaceShotTests`; run the tests and look at the shots.

## Task 8: Setup beside the page on the iPad

**Requirements:** R-IOS-52, W8, spec section 7.3.

**Files:**
- Create: `ios/NereusApp/Setup/SetupSplitView.swift`,
  `ios/NereusApp/UITests/SetupSplitUITests.swift`
- Modify: `ios/NereusApp/Setup/SetupTab.swift` (choose the split in regular width),
  `ios/NereusApp/Setup/SetupRouter.swift` (open a page in the split's right side),
  `ios/NereusApp/App/RootView.swift` (if the router's callers at `:114` and `:130` need
  it), `ios/NereusApp/Tests/IPadShotTests.swift` (a Setup frame)

**Interfaces:**
- Consumes: `SetupPageContainer` and `SetupPanelSection` (Task 7), `SetupRouter`.
- Produces: `SetupRouter.open(...)` keeps its signatures; on a regular-width iPad it
  selects the page in the split instead of pushing it.

**Acceptance:**
- Regular width (`horizontalSizeClass == .regular`): the Setup list on the left (the
  categories, with the open category expanded under its name and the open page marked
  in glass) and the open page on the right, its content held to a readable width; the
  iPad's tab bar stays.
- Compact width (an iPhone, or an iPad in a narrow Split View or Slide Over): one list
  with pages that slide in, exactly as after Task 7.
- Changing from regular to compact keeps the open page open.
- `RootView`'s two router calls open the right page in both widths.
- `SetupSplitUITests` on an iPad simulator: in full screen, opening a category and a
  page shows both columns; a Setup link from elsewhere opens its page on the right. It
  checks the compact path by the size class the app reports (an XCUITest cannot
  start Split View); the narrow-window check on a real iPad is JJ's.
- `IPadShotTests` draws the Setup split, looked at against setup-board.html's chosen
  iPad frame.

**Verification:** layout and navigation. UI on `iPad Pro 11-inch (M5)`:
`-only-testing:NereusSDRUITests/SetupSplitUITests`, `IPadUITests`. Shots:
`IPadShotTests` with `TEST_RUNNER_NEREUS_IPAD_SHOTS` (it runs only on an iPad
simulator). iPhone 17 regression: `SetupRenderingTests`, `RotorUITests`. Device
(pending, JJ): the iPad in full screen and in a narrow Split View.

**Execution note (advisory):** opus. Needs Task 7.

- [ ] **Step 1:** Build `SetupSplitView` on Task 7's container.
- [ ] **Step 2:** Route `SetupRouter` by width.
- [ ] **Step 3:** Write `SetupSplitUITests` and the iPad shot; run them.

## Task 9: Evidence pass and the phone images in the docs

**Requirements:** spec section 10 (shot comparison, contrast), section 12 (manual and
website images).

**Files:**
- Modify: the phone images listed in `docs/manual/images/README.md:112-131` and its
  iPad image (`:136-143`), `docs/manual/images/capture-provenance.json`, their copies in
  `website/public/manual/images/`, `website/public/assets/img/iphone-*.png`,
  `website/screenshot-sources.md`

**Interfaces:**
- Consumes: the finished branch.
- Produces: nothing for later tasks.

**Acceptance:**
- Every shot suite listed under "What already exists" runs to a scratch folder on
  `iPhone 17`, on `iPhone 18 Pro Max` for `MainScreenShotTests`,
  `TxPanelApprovedShotTests` and `SetupWorkspaceShotTests`, and on
  `iPad Pro 11-inch (M5)` for `IPadShotTests`, and passes. The report lists each
  suite's result and the folder.
- Contrast, measured from the values in `WorkspacePalette.swift` with the WCAG formula
  on `panelBg`, matches the spec's section 10 figures (primary 14.3, secondary 8.1,
  tertiary 6.2, scale 4.9, glass text on glass 5.4, lit green text 5.0, white on
  `txRed` 4.3), and no small text uses `accentFill`; the report gives each figure.
- Each manual and website phone image is recaptured from the test its README row
  names, at the same size, and the provenance files carry the new SHA-256, size and
  build; an image whose test cannot produce it is named in the report, not faked.
- `python3 scripts/verify-ios-workspace-colours.py`, `python3 scripts/verify-ios-provenance.py`
  and `python3 -m pytest tests/compliance/ -v` pass; the whole `NereusSDR` scheme's
  `test-without-building` on `iPhone 17` passes once, on the finished revision.

**Verification:** the final checks, once. The controller (not the implementer) opens
the captures beside the five boards and magnifies one glass button, one lit green
button, a Setup switch and the keyed PTT before saying the look is in. Device checks
stay pending for JJ.

**Execution note (advisory):** sonnet (captures and documentation). Needs Tasks 1 to 8.

- [ ] **Step 1:** Run the shot suites on the three simulators to scratch folders.
- [ ] **Step 2:** Measure the contrast figures.
- [ ] **Step 3:** Recapture the doc images and update their provenance files.
- [ ] **Step 4:** Run the full scheme once and the repository checks.
