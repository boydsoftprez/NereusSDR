// no-port-check: NereusSDR-original. No upstream port.

// SPDX-License-Identifier: GPL-3.0-or-later
#include "gui/chrome/ChromeBarItems.h"

#include "gui/chrome/ChromeBarController.h"

#include <QCoreApplication>
#include <QWidget>

namespace NereusSDR {

namespace {
/// Skip-on-null so callers need no per-item guard.
void add(ChromeBarController& c, QWidget* widget, QWidget* sep, int rung,
         const QString& label)
{
    if (!widget) { return; }
    c.addItem(widget, sep, rung, label);
}
} // namespace

void registerChromeBarItems(ChromeBarController& c, const ChromeBarWidgets& w)
{
    // Rung 0: nothing else reaches these. +PAN and the panel toggle have no
    // menu equivalent at all; the safety slots have no menu, dialog or
    // applet. Reachability audit: design doc section 3.4.
    add(c, w.panButton,    nullptr, 0, QString());
    add(c, w.panelToggle,  nullptr, 0, QString());
    add(c, w.stationBlock, nullptr, 0, QString());
    add(c, w.safetyGroup,  nullptr, 0, QString());

    // psaIndicator is registered at rung 0 (never folds on width) so its
    // ~154 px (two QLabel minimumWidth pins, PsaIndicatorWidget.cpp) is
    // counted in the width budget on every PS-capable, PS-armed board --
    // omitting it entirely under-counts the budget on exactly that bench
    // (Task A8 fix round 1 finding 2). Its ARMED/unarmed state is not a
    // fold concept, so it does not decide visibility here; MainWindow
    // reports that through setItemAvailable from
    // updatePsaIndicatorVisibility(), per design doc degenerate case §7
    // "PS not armed -> hidden as today". Not on the §6 fold ladder either
    // way. Empty label, matching every other rung-0 item above: rung 0
    // never appears in foldedLabels(), so a label here would never render.
    add(c, w.psaIndicator, nullptr, 0, QString());

    // w.rxDashRow is also deliberately not registered: mode and filter
    // never fold (RxDashboard.h), so the row's own baseline width is not
    // part of the ladder, and RxDashboard is built with a Preferred size
    // policy plus an explicit floor specifically so it can absorb any
    // residual pressure once its own pills (registered individually below)
    // have already folded (RxDashboard.cpp constructor comment).

    add(c, w.systemTile, w.systemTileSep, 1,
        QCoreApplication::translate("ChromeBar", "PA / CPU"));
    add(c, w.tgxlChip, nullptr, 2,
        QCoreApplication::translate("ChromeBar", "TGXL"));

    // CAT and TCI share rung 3 so they fold as a pair, avoiding a
    // "TCI but no CAT" half-state.
    add(c, w.catIndicator, w.catSep, 3,
        QCoreApplication::translate("ChromeBar", "CAT"));
    add(c, w.tciIndicator, w.tciSep, 3,
        QCoreApplication::translate("ChromeBar", "TCI"));

    // Both chain tags share rung 4. Chain 1 is null on single-ADC SKUs.
    add(c, w.chain0, nullptr, 4,
        QCoreApplication::translate("ChromeBar", "CH"));
    add(c, w.chain1, nullptr, 4, QString());

    // Rungs 5..9, one pill each, right to left.
    static const char* const kPillNames[] = {"SQL", "APF", "NB", "NR", "AGC"};
    for (int rung = 5; rung <= 9; ++rung) {
        add(c, w.pillByRung.value(rung), nullptr, rung,
            QCoreApplication::translate("ChromeBar", kPillNames[rung - 5]));
    }

    // Rung 10, last resort: placeholders fold only after every live
    // reading has already gone.
    add(c, w.placeholderGroup, w.placeholderSep, 10,
        QCoreApplication::translate("ChromeBar", "TNF / CWX / DVK / FDX"));
}

} // namespace NereusSDR
