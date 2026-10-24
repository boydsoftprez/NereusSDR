# The phone's display controls, as on the desktop: design

**Date:** 2026-10-18 to 2026-10-22
**Author:** J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code
**Status:** Written for JJ's review. Every ruling in section 3 was made by JJ between 2026-10-18 and 2026-10-22 against the board listed below. The smaller calls in section 12 are listed for veto.
**Base:** `origin/main` at `f7442e6f7`. Every `file:line` below is on that tree unless it says PR 603, which means `origin/claude/ios-workspace-look` at `6a8d8f309` (the workspace look, open and in draft).
**Applies to:** the iPhone and iPad app under `ios/`. The desktop and the Core do not change (section 6.6 lists the two facts the plan confirms on a real Core).
**Mockup (approved):** [board.html](2026-09-23-iphone-app-design/board.html), the sections headed "Display, as on the desktop":
- the sheet: one frame per tab (Spectrum, Levels, Waterfall, View, FFT) and "Press and hold the band";
- the iPad: the button column and the pill;
- the iPad with two pans: "Two pans, two pills" and "This iPad's Setup > Display";
- the phone sideways;
- Setup: "Setup > Display", the eight pages, and "Transmit Display".

The board is drawn in the navy look and draws the iPad's Setup at phone width. Both are superseded by the workspace look (section 8): the controls, rows and behavior are as drawn; the colors, corners and the iPad's Setup layout are PR 603's.

---

## 1. Goal

The desktop's Display pill (PR 443) put every display control a pan has into five tabs beside the band, with per-pan values, "Reset this pan" and a link to the matching Setup page, and Setup > Display into eight pages with a "Settings for" strip. The phone still has its own Display sheet, laid out in its own order, with one pan and a separate "On this phone" Setup page.

After this work the phone and the iPad have the desktop's Display controls: the same tabs, the same rows in the same order, the same per-pan rules, the same band menu, the same Setup pages and the same "Settings for" strip. The iPad and the sideways phone carry the desktop's button column and pill on the band, and the iPad shows as many pans as the radio allows.

## 2. Non-goals

- No change to what a display setting does. Every row draws what it draws today; this work moves rows, adds the per-pan rules the desktop has, and adds the controls the board drew.
- No change to the desktop's pill, its Setup pages or the Core. No new setup description version: version 29 (`resources/setup/display.json:2`) already carries the eight pages, the per-pan owners and the per-band dB Max and dB Min.
- No more than one pan on an iPhone, upright or sideways.
- No colors of its own. Every new screen uses the workspace look's tokens and treatments (section 8).
- The tuning, flag and slice-list designs are their own; this design only places the band menu beside them.

## 3. Decisions

| # | Decision | Ruled | Rejected |
| --- | --- | --- | --- |
| DC1 | **The toolbar's Display opens the desktop pill's five tabs:** Spectrum, Levels, Waterfall, View and FFT (`src/gui/SpectrumOverlayPanel.cpp:1621-1625`), with each tab's rows in the pill's order (`:3121`, `:3459`, `:3979`, `:4186`, `:3628`) and the pill's footer, "Reset this pan" on the left and "Setup defaults" on the right (`:1721-1747`). | JJ, 2026-10-18: "1 and bring the setting forward too". | Keeping the phone's own sheet and only renaming its rows. |
| DC2 | **The phone's own rows go in the nearest tab:** Spectrum Height under Spectrum; Look back under Waterfall; CTUN and DUP under View, in a group named "On this phone"; decimation under FFT, in the same kind of group. | Same ruling. | A sixth tab for the phone's rows. |
| DC3 | **dB Max and dB Min are kept per band on the Core, as on the desktop.** The phone reads and writes the Core's `DisplayGridMax_<band>` and `DisplayGridMin_<band>`, so the phone, the iPad and the desktop show one scale on a band. The sheet's Top and Range go; the Levels tab shows dB Max and dB Min. | JJ, 2026-10-18: "Yes, share dB Max and dB Min per band through the Core, as the desktop does." | The phone's own per-band scale (today's `bandScales`). |
| DC4 | **A pan's values follow the desktop's rules.** Each pan follows Setup's values until a row is changed in that pan's sheet; that row then shows an amber dot. Tapping the dot puts that row back to Setup's value. "Reset this pan" puts every row of the pan back. "Setup defaults" opens the Setup page for the open tab, on that pan. | JJ, 2026-10-22: "would be nice to have it behave like the desktop and the display pill work that recently landed". | The phone's switch between "this pan" and "all pans" in Setup only, with no reset on the sheet. |
| DC5 | **The FFT tab is the Core's,** one value for every device on that Core: sizing, size, Hz per bin, frame rate and window, with the readout of what the Core is running. Decimation stays this phone's. Cal Offset on the Spectrum tab is grayed with "The Core calibrates for its radio." | Drawn on the board under DC1. | Per-device FFT settings. |
| DC6 | **Press and hold on empty band for half a second opens the desktop's band menu** at the finger, with a haptic tap: Ref Level, Dyn Range, CTUN, a notch at that frequency and "Display settings", which opens the sheet (`src/gui/SpectrumOverlayMenu.cpp:79-217`). A drag or a quick tap still moves or tunes; holding a flag, a spot or a station still opens its own card. | JJ, 2026-10-18: "Yes, press and hold on empty band opens the desktop's band menu." | A menu button on the band. |
| DC7 | **The iPad has the desktop's button column at the band's top-left:** ◀, +RX, +TNF, BAND, ANT, Display, VAX and ATT, each opening its menu beside the column; Display opens the pill. Buttons are 30 points tall. The pill is never taller than the band and scrolls past that. A tap elsewhere closes an open menu. The toolbar's Display opens the same pill. | JJ, 2026-10-18: "Yes, draw the iPad with the Display controls as the desktop's floating pill beside the band." | The phone's drop sheet on the iPad. |
| DC8 | **The phone held sideways has the same column and pill,** inside the safe area. The pill is as tall as the band and scrolls. The PTT moves just right of whatever is open, and the keyed meters go beside it or above it. The band menu opens beside the finger, on the side clear of the PTT. | JJ, 2026-10-18: "Yes, draw the phone held sideways with the Display pill beside the band, as on the iPad." | The upright drop sheet, sideways. |
| DC9 | **The iPad shows more than one pan, stacked as on the desktop,** Pan 1 above Pan 2, each with its own column and pill. A pan's pill draws on that pan only. The Core's rows, the band's dB Max and dB Min, and this iPad's own rows are shared by every pan. The toolbar follows the pan last touched. | JJ, 2026-10-21: "sure go for it". | One pan on the iPad. |
| DC10 | **Slice and pan limits come from the radio.** +RX stops at the Core's slice limit; the iPad opens at most the smaller of the radio's slices and its free receivers, as the desktop does (`MainWindow::panLayoutLimitFor`, `src/gui/MainWindow.cpp:18893`). | JJ, 2026-10-21: "the pan limit should be hardware dependant not a hard arbitrary cap". | A fixed limit of two pans. |
| DC11 | **Setup > Display is the desktop's eight pages,** in its order: Spectrum, Levels, Waterfall, View, Peaks & Markers, FFT & Engine, Transmit Display, Multimeter. The "On this phone" page goes and its rows move to the page they belong on (section 7.2). | JJ, 2026-10-18: "bring the setting forward too". | Keeping "On this phone" as a ninth page. |
| DC12 | **The first five pages have the desktop's "Settings for" strip:** All pans, then one button per pan the radio allows, each naming its slices or "not open", with a dot when that pan has its own values on the page; the desktop's notes; "Reset this pan" and "Use Pan N's on all pans" acting on that page's rows (`src/gui/SetupPage.cpp:366-470`). Setup opened from its list starts on All pans; "Setup defaults" in a pan's pill opens the matching page on that pan. | JJ, 2026-10-22 (DC4's ruling). | Today's two-chip "All pans" / "Pan 1" header. |
| DC13 | **The workspace look throughout,** and on a regular-width iPad Setup's list sits on the left with the page beside it (PR 603's W8). | JJ, 2026-10-22: "make sure you have the other iphone display work pr in your consideration lots moved". | The board's navy and its phone-width iPad Setup. |

## 4. What the phone has today

- **The sheet** is a `DropSheet` drawn over the band under the toolbar (`ios/NereusApp/Main/DropSheet.swift:17-62`; the other `Main/` files below are beside it), full width less 8 points upright and 420 points wide sideways (`:24`; `MainScreen.swift:230-243`). Its rows are in the phone's own order: View (with 3D), Waterfall, Spectrum (with the FFT block, Top and Range), While transmitting, On the band, Tuning, Look back, and a link to "On this phone" (`DisplaySheet.swift:30-137`, `:255-271`). It has no "Reset this pan" and no "Setup defaults".
- **One pan.** `MainScreenModel.panNumber` is the constant 1 (`MainScreenModel.swift:20`) and `BandSubscriber.panId` is `"1"` (`BandSubscriber.swift:61`). No pan limit is read; `maxSlices` is decoded (`NereusModels/StationCatalog.swift:147`) and `effectiveMaxSlices` is shown only in `Radio/RadioAtAGlance.swift:194`.
- **Per-pan storage already exists:** a master set `phone.display.master.v29`, each pan's own values in `phone.band.display.<panId>`, `ownFields`, `reset(pan:)` and `copyOwnPictureToMaster` (`ios/NereusKit/Sources/NereusBand/BandDisplaySettingsStore.swift:15-60`, `:188`, `:364-405`), wrapped by `DisplayEditScope { pan, all }` (`MainScreenModel.swift:29-56`).
- **Setup > Display** already renders version 29's eight pages from the Core's description, after an "On this phone" page (`ios/NereusApp/Setup/SetupTree.swift:238-239`), with a two-chip "All pans" / "Pan 1" header, own-value dots and a band picker in All mode (`DescribedPage.swift:38-42`, `:122-224`).
- **dB Max and dB Min** are the phone's own, per band in `bandScales` (`NereusBand/BandDisplaySettings.swift:173-187`, `:463-519`), written by the sheet's Top and Range and the dBm arrows (`DisplaySheetModel.swift:411-424`, `:528-578`). The Core's `DisplayGridMax_<band>` is known only as a station-scoped key (`NereusMirror/SettingsScope.swift:84-87`) and is never read into the band.
- **NB Blank has no way in.** The phone stores it per pan, `waterfallBlanking` (off) and `waterfallBlankingThresholdDb` (6) (`NereusBand/BandDisplaySettings.swift:164-165`), under the keys `WaterfallBlankingEnabled` and `WaterfallBlankingThresholdDb` (`Setup/PhoneSetupKeys.swift:102-103`, `:220-226`), and version 29's Waterfall page lists both rows (`NereusMirror/SetupDescription+V29.swift`, `display.waterfall.blanking` and `display.waterfall.blankingThreshold`). The sheet has no NB row, and on JJ's phone Setup shows neither (his report, 2026-10-24).
- **No band menu.** The band has drag, single and double tap, and pinch (`Band/BandGestureLayer.swift:275-557`); the only long presses are on a spot label and a FreeDV row.
- **The iPad** has three layouts, phone, column and front panel (`iPad/IPadLayout.swift:14-36`), and the same drop sheet; it has no button column.
- **The zoom kept across launches** (PR 586) is `BandDisplaySettings.rxViewSpanHz` (`BandDisplaySettings.swift:372`), saved in the pan's own `phone.band.display.1`.

## 5. The sheet and the pill

### 5.1 One set of tabs, three containers

The five tabs and their rows are one view, `DisplayPill`, drawn in three containers:

| Device | Container | Opened by |
| --- | --- | --- |
| iPhone upright | the existing `DropSheet` over the band | the toolbar's Display; "Display settings" in the band menu |
| iPhone sideways | a pill beside the column, inside the safe area, as tall as the band | the column's Display; the toolbar's Display; the band menu |
| iPad | a pill beside its pan's column, never taller than the band | the same three |

The title names the pan: "Display Pan 1 · this phone" (or "this iPad"), as the sheet does today (`DisplaySheet.swift:24`).

### 5.2 Rows

Each tab is a table in the app of setting keys in the pill's order, each cited to the desktop function that builds that tab. Each row is drawn with the described control for its key from the Core's version 29 description (`DescribedControl`), the same control Setup draws, so a row reads the same on the sheet and in Setup and is grayed by the same `enabledWhen`. The phone's own rows (DC2) are in the same table, with the phone as owner.

| Tab | Rows (the board's frames are the reference) |
| --- | --- |
| Spectrum | Trace: detector, normalize, fill, gradient, fill alpha, averaging, average time, line width, peak hold, Spectrum Height (this phone), Cal Offset (grayed, DC5). Grid: spacing, scale text. Markers: noise floor line, bin width, peak value, cursor frequency. A link "Peaks & Markers in Setup ›". |
| Levels | Spectrum scale mode (Manual or tracking the floor), dB Max, dB Min, floor offset, keep range; waterfall level mode (five modes), Re-tune, black level, gain, noise floor offset, low, high. |
| Waterfall | Drawing: scheme, rate, opacity, NB Blank on or off, NB threshold (grayed while NB Blank is off), averaging, average time. Overlays: the five. Time: timestamp, rewind depth, then Look back (this phone). |
| View | 2D or 3D; with 3D, floor (per band), gain, span, angle, speed, slice shadow; background: on, picture, opacity. On this phone: CTUN, DUP. |
| FFT | The Core's: sizing, size, Hz per bin, the readout, frame rate, window. On this phone: decimation. |

A row a mode does not use is grayed in place with the reason the description gives. Every row of the Levels tab is grayed while transmitting, with "Transmit has its own levels, on Transmit Display", as on the desktop.

### 5.3 Behavior

- **Live:** every row draws on the band as it changes.
- **Peek:** holding a slider clears the sheet or pill away except for that row, until the finger lifts.
- **Per pan (DC4):** a change writes that pan's own value (`DisplayEditScope.pan`). A row with an own value shows the amber dot; a tap on the dot clears that one field (`ownFields`). "Reset this pan" clears every own field of the pan and is grayed, with "Pan 1 follows Setup", when there are none. Shared rows (the Core's, dB Max and dB Min, the phone's own) never show a dot.
- **Setup defaults** opens Setup > Display on the page for the open tab (Spectrum, Levels, Waterfall, View, FFT & Engine), on that pan (section 7.3).
- **Reset this pan keeps the place:** the zoom (`rxViewSpanHz`) and the band's position are not display settings and stay. Today `resetKeepingPlace` keeps each band's 3D floor, the band plan's size and the extended view but not the zoom (`ios/NereusKit/Sources/NereusBand/BandDisplayOptions.swift:242-258`); the plan adds the zoom to what it keeps.

### 5.4 dB Max and dB Min (DC3)

- The band's scale on a band is the Core's `DisplayGridMax_<band>` and `DisplayGridMin_<band>`, read from the settings mirror. A change from the Levels tab, the Levels page or the dBm arrows on the band writes the Core's key for the band shown, through the same settings proxy `CoreDisplayModel` uses (`ios/NereusApp/Main/CoreDisplayModel.swift:274`).
- The phone stops keeping its own `bandScales`. A phone's saved per-band values are not sent to the Core on upgrade; the Core's values win, as the link says (`docs/architecture/2026-09-23-station-link-v1.md:5965-5975`).
- While the Core has not answered a write, the row shows the value written, marked unconfirmed as other Core rows are (`CoreDisplayModel.isUnconfirmed`). A refusal puts the Core's value back and shows the Core's reason under the row.
- An older Core that does not describe version 29 keeps today's Top and Range rows, the phone's own, with "Needs a newer Core to share the scale" under them.

## 6. The band, the column and more than one pan

### 6.1 The band menu (DC6)

- A half-second press on the band layer that fails if the finger moves past the drag threshold, so drags and taps keep working. It fires only on empty band; the flag, spot and FreeDV station layers keep their own holds.
- A light haptic, then the menu at the finger (beside it when sideways, on the side away from the PTT), with the desktop's items in its order. Ref Level and Dyn Range move the band's shared scale (section 5.4). The notch item names the frequency under the finger and adds the notch there, as "Add a notch" in the Pan sheet does today. CTUN is the phone's. "Display settings" opens the sheet or pill.
- An item that can't be used now is grayed with its reason (for example, the notch while the Core has no notch control).

### 6.2 The column (DC7, DC8)

- Shown on the iPad and on the iPhone held sideways, at the band's top-left inside the safe area; never on the iPhone upright, which keeps the toolbar.
- ◀ folds the column to one button and back. +RX adds a slice on this pan (`addSliceOnPan`), grayed at the slice limit with "The <radio> runs <n> slices." +TNF adds a notch in the middle of the active slice's passband. BAND, ANT, VAX and ATT open the menus the phone already has for band select, the flag's antennas, the slice's VAX channel and the step attenuator, drawn beside the column. Display opens the pill.
- One menu is open at a time; a tap anywhere else closes it.
- The PTT and the keyed meters move just right of the open column, pill or menu, and back when it closes.

### 6.3 More than one pan on the iPad (DC9, DC10)

- **The limit** is `min(effectiveMaxSlices, userDdcCount)` from the Core's capabilities, the two values the desktop's remote window uses, and at least 1. The slice limit is `effectiveMaxSlices`.
- **Opening and closing a pan:** the iPad's Pan sheet gains "Add a pan" and, on Pan 2 and above, "Close this pan". "Add a pan" is grayed at the limit with "The <radio> can show <n> pans." Closing a pan keeps its own values for next time, as the desktop does.
- **Each pan is a band:** its own display subscription with its own pan key (`"1"`, `"2"`, ...), its own `BandModel`, its own entry in the own-values store and its own zoom. `DisplayQualityAllocator` already takes one intent per pan key (`ios/NereusKit/Sources/NereusMedia/DisplayQualityAllocator.swift:26`), so the frame budget is shared between pans as it is for the desktop's remote window.
- **Stacking:** pans share the band area equally, Pan 1 on top. Each has its column and its pill. A new slice from a pan's +RX starts on that pan.
- **The toolbar follows the pan last touched:** its Pan button names it, its slice letter is that pan's active slice, and its Display opens that pan's pill.
- **Shared rows:** the Core's rows, dB Max and dB Min, and the iPad's own rows (CTUN, DUP, Look back, Spectrum Height, decimation) are one value for every pan; a change on one pan's pill shows on the others.
- **A smaller radio:** if a Core reports a lower limit than the pans open (a different radio after a reconnect), the highest pans close with a notice naming the limit, and their slices are left to the Core's own rules.

### 6.4 The iPhone

An iPhone shows one pan, upright and sideways, as today. Its Setup strip lists the radio's pans with the ones after Pan 1 grayed (section 7.3).

### 6.5 Look back and the keyed state

Look back stays per device and draws on every pan. While transmitting, the Levels rows are grayed (5.2), and the transmit scale on Transmit Display is used, as today.

### 6.6 What the plan confirms on a real Core

The design needs no Core change. Two facts are confirmed against a real Core in the plan's first task, before rows are built on them:

1. A phone's write of `DisplayGridMax_<band>` reaches the desktop's pan on that band (the link's parity ruling C12 says it does).
2. A second display subscription from one device, with its own pan key, is granted and framed as the desktop remote window's second pan is.

If either is not so, the plan stops and the gap goes to the Core's owner.

The same task finds why JJ's phone shows no NB Blank row in Setup > Display > Waterfall although the description lists it: an installed build older than the page, the Core's description version, or the row read as unavailable. The fix ships with the Waterfall rows.

## 7. Setup > Display

### 7.1 The pages (DC11)

Setup > Display lists the eight pages of the Core's version 29 description, as it does today, without "On this phone". Their rows are already described; this work changes the phone's own rows and the strip.

### 7.2 Where "On this phone" goes

| Today on "On this phone" | Moves to |
| --- | --- |
| Band plan | View, "Band plan" group (the Core's) |
| The Core's FFT groups | FFT & Engine (already there) |
| Waterfall, Waterfall: more | Waterfall |
| Spectrum, Spectrum: more, Spectrum Height | Spectrum, with Spectrum Height in "Layout" (this phone) |
| Grid & Scales (Top, Bottom) | Levels, as dB Max and dB Min (the Core's, per band) |
| Grid & Scales: more | Spectrum, "Grid" and "Scales and labels" |
| Noise floor, Peaks, On the band | Peaks & Markers |
| Colors | Setup > Appearance, as on the desktop |
| View: Extended view, CTUN | View, "On this phone" |
| Transmit display, the Core's transmit display | Transmit Display; DUP joins its "Transmit scale" group |
| Reset to defaults | "Reset this pan" on each page; on All pans, the page's own reset to defaults |

Transmit Display's phone rows use dB Max and dB Min in place of Top and Range, matching the receive scale's words. Multimeter has the Core's polling delay and this phone's decimal point, units and history. FFT & Engine leaves out the desktop's thread priority and monitor rate, which have nothing to do on a phone.

### 7.3 The "Settings for" strip (DC12)

- On Spectrum, Levels, Waterfall, View and Peaks & Markers, above the first group, replacing `DisplayV29ScopeHeader`.
- **Buttons:** "All pans", then "Pan 1" to "Pan <limit>", each with a second line naming its slices ("Slice A, B") or "not open", and " •" after the name when that pan has its own values on this page, as the desktop's chips do (`SetupPage.cpp:398-440`). On an iPhone, the pans after Pan 1 are grayed with "An iPhone shows one pan."
- **Notes**, the desktop's words: on All pans, that these are what every pan starts with; on a pan with no own values, "Pan N follows All pans in every row on this page."; on a pan that is not open, "Pan N is not open now. What you set here is kept and used when you open it." (`:447-460`).
- **Actions** on a pan, acting on this page's rows only: "Reset this pan" and "Use Pan N's on all pans" (`:466`), both grayed when the pan has no own values on the page. On All pans the two are not shown, as on the desktop.
- **Levels** keeps its band picker, which edits a band's dB Max and dB Min without tuning.
- **Opening:** from the Setup list, All pans. From a pan's "Setup defaults", that pan, on the page for the open tab. The choice stays while moving between Display pages, as the desktop's `SetupDialog::setDisplayPanIndex` does (`src/gui/SetupDialog.cpp:997`).
- **Live:** a change to All pans reaches every pan that follows it at once, and a change in a pill shows in Setup while it is open (the pill design's D26 and D27).

### 7.4 The iPad

On a regular-width iPad, Setup is PR 603's split view (`SetupSplitView`, PR 603): the list on the left, the page on the right, with the strip at the top of the page. "Setup defaults" opens the page on the right through `SetupRouter`. In a compact window, pages push, as on the iPhone.

## 8. The look

This work is drawn in the workspace look of PR 603 and uses its pieces, never its own colors:

- **Tokens** from `ios/Shared/WorkspacePalette.swift` (PR 603) and **treatments** from `ios/Shared/WorkspaceStyles.swift` (PR 603): the pill and the column in the panel treatment; column buttons raised, the open one in the glass; tabs as a pick-one choice, so the open tab is in the glass; CTUN and DUP lit green when on; the amber dot in `kAmberText`; Reset this pan raised; grayed rows at 40 percent with their reason.
- **Setup** rows in `SetupPageContainer`, `SetupPanelSection`, `SetupMenuRow`, `SetupStepperRow` and `SetupSlider` (PR 603); the strip's chosen button in the glass.
- **Radii** 7 for buttons and chips, 10 for the pill, the column's frame and the menus.
- **Red** only for transmit (W5): the keyed PTT beside the column is `kTxRed`; nothing else in this design is red.

## 9. Error handling and disabled states

| Situation | What the phone does |
| --- | --- |
| A Core older than description version 29 | The sheet keeps today's rows and order, with "Needs a newer Core for the desktop's Display tabs" at the top; Setup keeps the old skeleton (`SetupTree.swift:134-137`). |
| A row the Core does not describe | The row is shown grayed with "Needs a newer Core". |
| A Core row refused or not confirmed | The Core's value comes back with the Core's reason; unconfirmed rows are marked as other Core rows are. |
| Link lost | The pill stays open with its last values; the Core's rows and dB Max and dB Min are grayed with "Not connected"; the phone's rows still work. |
| Slice limit reached | +RX grayed: "The <radio> runs <n> slices." |
| Pan limit reached | "Add a pan" grayed: "The <radio> can show <n> pans." |
| Transmitting | The Levels rows grayed: "Transmit has its own levels, on Transmit Display." |
| Large type | Setup rows follow PR 603's W9 layout; the column's buttons keep 30 points and the pill scrolls. |

## 10. Requirements and verification

Numbered after PR 603's R-IOS-49 to R-IOS-54, in the R-IOS table of [2026-09-23-iphone-app-design.md](2026-09-23-iphone-app-design.md).

| ID | Requirement | Evidence |
| --- | --- | --- |
| R-IOS-55 | The Display sheet and pill (DC1, DC2, DC4, DC5): the five tabs, each tab's rows in the desktop pill's order with the phone's rows where DC2 puts them, live on the band, the peek, the amber dot and its tap, Reset this pan and Setup defaults, Cal Offset grayed, NB Blank and its threshold on the Waterfall tab and page. | Software: each tab's key table against the desktop functions cited in DC1; own-value set, clear and reset against `BandDisplaySettingsStore`. Integration: shot tests of each tab on the iPhone, sideways and on the iPad, compared with the redrawn board; a UI test that Setup defaults opens the matching page on that pan; a UI test that NB Blank and its threshold on the sheet and in Setup both change the pan's blanking. Device: NB Blank on JJ's phone. |
| R-IOS-56 | dB Max and dB Min per band through the Core (DC3, 5.4): read, write, unconfirmed and refused states; Top and Range gone; the older-Core fallback. | Software: against `FakeStation` with and without version 29, including a refusal. Integration: a change on the phone moves the desktop's scale on that band on a real Core, and the reverse. |
| R-IOS-57 | The band menu (DC6): the half-second hold on empty band, the haptic, the items in the desktop's order, a notch at the finger's frequency, drags and taps unchanged, holds on flags, spots and stations unchanged. | Software: the gesture's hit rules on a band fixture. Integration: a UI test of the hold, the menu and each item. Device: JJ's phone, upright and sideways. |
| R-IOS-58 | The column and the pill on the iPad and the sideways iPhone (DC7, DC8): every button and its menu, one menu at a time, fold, the pill's height and scroll, the PTT's move. | Integration: `IPadShotTests` and a sideways iPhone shot test with each menu open; a UI test that the PTT is never under an open menu. Device: JJ's iPad and phone. |
| R-IOS-59 | More than one pan on the iPad (DC9, DC10, 6.3): the limit from the Core's capabilities, Add a pan and Close this pan with their grayed reasons, one subscription per pan, shared and own rows, the toolbar following the pan last touched, a lower limit after reconnecting. | Software: the limit for each capability fixture (ANAN-G2, ANAN-G2E, Hermes Lite 2, Atlas); the allocator's split between two pans. Integration: two pans against a real Core with frames on both. Device: JJ's iPad on the G2E. |
| R-IOS-60 | Setup > Display (DC11, DC12, 7): the eight pages without "On this phone", each moved row where 7.2 puts it, the strip's buttons, dots, notes and actions, the iPhone's grayed pans, the opening rules, the band picker, the iPad's split view. | Software: the page tree for a version 29 Core; the strip's text for each pan state against the desktop's strings. Integration: Setup shot tests on the iPhone and the iPad, in default and large type; a UI test that Pan 2's Setup defaults opens that page on Pan 2. |
| R-IOS-61 | The workspace look on every screen of this design (DC13, 8). | Software: PR 603's check that no Swift file outside the kept files carries an old navy hex covers the new files. Integration: the shot tests above. |

## 11. Coordination

- **PR 603 lands first.** This design builds on its Setup container, split view and styles. The plan starts after PR 603's Setup step is on main and merges main after each later step of PR 603, as PR 603's section 12 asks of open phone branches.
- **PR 586** is on main; its zoom lives in each pan's own values and is kept by Reset this pan (5.3). Pan 2 gets its own zoom the same way.
- **The board** is redrawn in the workspace look, with the iPad's Setup in the split view and the iPhone strip's grayed pans, before the plan's UI tasks, so the shot tests compare against what the app should draw. The redraw changes no control or behavior.
- **The phone design spec** gains DC1 to DC13 in its decisions and R-IOS-55 to R-IOS-61 in its table when the plan lands.
- **The parity audit** ([2026-09-23-iphone-app-display-parity.md](2026-09-23-iphone-app-display-parity.md)) is updated by the plan to say which desktop display controls the phone now matches.

## 12. Settled calls, listed for veto

These were drawn on the board or follow from it and were not asked one at a time.

1. On the board, Pan 2 is on the lower part of 40 m with slice C, and starts with two values of its own (the BlackWhite scheme, Fill off). That is the board's example only; a new pan in the app starts by following All pans.
2. The toolbar follows the pan last touched (DC9), and a new slice from a pan's +RX starts on that pan.
3. The PTT moves to just right of whatever is open, on the iPad and sideways.
4. "Reset this pan" and "Use Pan N's on all pans" are not shown on All pans, as on the desktop, even though the phone's rule is grayed rather than hidden. On a pan they are grayed when there is nothing to reset.
5. The iPad's Setup is PR 603's split view (W8), not the phone-width page the board drew.
6. A second pan is opened from the iPad's Pan sheet ("Add a pan", "Close this pan"). The board drew two pans open and no control for opening one; the desktop's is "+PAN" on its status bar.
7. On an iPhone, the strip's pans after Pan 1 are grayed with "An iPhone shows one pan", because a phone's display values are its own and an iPhone never opens them. The board drew them as live buttons.
8. The sheet's rows are drawn from the Core's description, so an older Core keeps today's sheet (section 9) rather than a half-filled pill.
9. The phone's saved per-band Top and Range are dropped on upgrade, not sent to the Core (5.4).
10. The iPad's column buttons are 30 points tall; the desktop's are smaller.

## 13. Out of scope

- More than one pan on an iPhone.
- The desktop's Pan Layout dialog and its layouts (side by side, grids). The iPad stacks pans only.
- Floating a pan into its own window.
- Display settings that neither the desktop nor the phone has today.
- The Core and the desktop, except the two confirmations in 6.6.
