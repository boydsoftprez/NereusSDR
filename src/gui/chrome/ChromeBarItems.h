// no-port-check: NereusSDR-original. No upstream port. The banner's fold
// ladder composition, extracted from buildStatusBar so it can be tested
// without constructing MainWindow.

// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>

class QWidget;

namespace NereusSDR {

class ChromeBarController;

/// Width of a reserved safety slot (design §4.5).
///
/// Two sizes, not one. PA and TX carry two or three glyphs and want a
/// narrow, stable footprint the operator learns by position. The ADC
/// overload badge carries real content -- "ADC0/1/2" over "OVERLOAD" --
/// and forcing it into the same box is what produced the clipped
/// "/ERLO/" seen on a bench. Retiring the TX-inhibit pill freed a whole
/// slot, so the alarm now gets a slot sized to it and the group is still
/// narrower than the four uniform slots it replaces.
///
/// Both MainWindow::buildStatusBar and
/// tests/tst_mainwindow_status_bar_safety.cpp read these, so a slot and
/// its fit assertion cannot drift apart.
inline constexpr int kSafetySlotWidthPx = 60;
/// Sized to the overload badge's widest real state: a three-ADC alarm.
inline constexpr int kOverloadSlotWidthPx = 88;

/// Every widget the banner registers. Any member may be null; registration
/// skips nulls. In production every SKU currently constructs chain1
/// unconditionally (MainWindow::buildStatusBar) and gates its visibility
/// through ChromeBarController::setItemAvailable on
/// BoardCapabilities::rxFilterChainCount, not through leaving this field
/// null -- a null chain1 here is a defensive case this struct still
/// supports (see nullWidgetsAreSkippedNotCrashed), not a real-world one.
struct ChromeBarWidgets {
    QWidget* panButton{nullptr};
    QWidget* panelToggle{nullptr};
    QWidget* stationBlock{nullptr};
    QWidget* safetyGroup{nullptr};
    QWidget* psaIndicator{nullptr};
    QWidget* overflowChip{nullptr};

    QWidget* systemTile{nullptr};
    QWidget* systemTileSep{nullptr};
    QWidget* tgxlChip{nullptr};
    QWidget* catIndicator{nullptr};
    QWidget* catSep{nullptr};
    QWidget* tciIndicator{nullptr};
    QWidget* tciSep{nullptr};
    QWidget* chain0{nullptr};
    QWidget* chain1{nullptr};
    QWidget* rxDashRow{nullptr};
    QWidget* placeholderGroup{nullptr};
    QWidget* placeholderSep{nullptr};
    /// Band-stack dots. Lead the bar positionally but fold at rung 10 with
    /// the other stubs; rung governs visibility, layout governs position.
    QWidget* bandStackLabel{nullptr};
    /// TNF indicator. Live since #313, so it folds at rung 11, after the
    /// stubs, because its amber state warns that notches are bypassed.
    QWidget* tnfLabel{nullptr};

    /// Rung to pill widget, rungs 5..9 only (SQL, APF, NB, NR, AGC).
    /// Mode and filter never fold and are deliberately absent.
    QHash<int, QWidget*> pillByRung;
};

/// Single source of truth for which banner item folds at which rung.
/// Rung 0 never folds. See design doc section 6.
void registerChromeBarItems(ChromeBarController& controller,
                            const ChromeBarWidgets& widgets);

} // namespace NereusSDR
