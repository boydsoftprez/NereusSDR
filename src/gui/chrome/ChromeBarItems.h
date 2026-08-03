// no-port-check: NereusSDR-original. No upstream port. The banner's fold
// ladder composition, extracted from buildStatusBar so it can be tested
// without constructing MainWindow.

// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>

class QWidget;

namespace NereusSDR {

class ChromeBarController;

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

    /// Rung to pill widget, rungs 5..9 only (SQL, APF, NB, NR, AGC).
    /// Mode and filter never fold and are deliberately absent.
    QHash<int, QWidget*> pillByRung;
};

/// Single source of truth for which banner item folds at which rung.
/// Rung 0 never folds. See design doc section 6.
void registerChromeBarItems(ChromeBarController& controller,
                            const ChromeBarWidgets& widgets);

} // namespace NereusSDR
