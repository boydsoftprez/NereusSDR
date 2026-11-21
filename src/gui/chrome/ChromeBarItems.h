// no-port-check: Longpath-original. No upstream port. The banner's fold
// ladder composition, extracted from buildStatusBar so it can be tested
// without constructing MainWindow.

// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>

class QWidget;

namespace Longpath {

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
/// Der Sende-Platz. Seit dem 2026-08-20 steht dort "ON AIR" statt "TX"
/// (MainWindow, Begruendung beim Badge) — mit Punkt-Symbol rund 74 px,
/// und der 60-px-Platz schnitt das R ab: auf dem Foto des Betreibers
/// vom 2026-09-17 stand rechts unten "ON AI". Der Pruefstand hatte es
/// nicht gefangen, weil er noch "TX" mass. 78 war am Mac gemessen; die
/// Linux-Schrift setzt "ON AIR" mit Symbol auf 81 (CI, 2026-09-21),
/// daher 84 -- der Platz muss die breiteste Plattform fassen.
inline constexpr int kTxSlotWidthPx = 84;
/// Der Mikrofon-Platz (2026-11-20): "NO MIC" mit Punkt-Symbol, gleich
/// lang wie "ON AIR" -- darum dieselbe Breite. Steht zwischen ADC OVL und
/// ON AIR, matt solange das PC-Mikrofon liefert (oder gar nicht gewaehlt
/// ist), in Bernstein, wenn es ausfaellt.
inline constexpr int kMicSlotWidthPx = 84;

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

    /// Die vier Sendeschalter (Betreiber-Entscheidung 2026-08-18,
    /// Fussleisten-Entwurf Zuschnitt A). Je Schalter ein Eintrag, weil
    /// sie verschieden wichtig sind: PS faellt zuerst (Sprosse 13), MOX
    /// zuletzt (16). Ein Sender, den man nicht abschalten kann, ist das
    /// schlechteste Ende einer Faltung.
    QWidget* txMox{nullptr};
    QWidget* txVox{nullptr};
    QWidget* txTune{nullptr};
    QWidget* txPs{nullptr};

    /// Rung to pill widget, rungs 5..9 only (SQL, APF, NB, NR, AGC).
    /// Mode and filter never fold and are deliberately absent.
    QHash<int, QWidget*> pillByRung;
};

/// Single source of truth for which banner item folds at which rung.
/// Rung 0 never folds. See design doc section 6.
void registerChromeBarItems(ChromeBarController& controller,
                            const ChromeBarWidgets& widgets);

} // namespace Longpath
