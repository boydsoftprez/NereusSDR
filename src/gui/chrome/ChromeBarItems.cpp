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

    // overflowChip is also rung 0: it is the ladder's OWN output surface,
    // not a foldable item, but its ~26 px must still count in the width
    // budget once it is showing, or every fold step under-frees by that
    // much right when folding starts (final-fix-wave finding 4). Its
    // ARMED state (dropped-items list non-empty) is reported through
    // setItemAvailable from the foldStateChanged handler that also feeds
    // it its content, exactly like psaIndicator above.
    add(c, w.overflowChip, nullptr, 0, QString());

    // w.rxDashRow: mode and filter never fold (RxDashboard.h), so this
    // registers the row's own non-pill residual (slice tag + mode +
    // filter + margins), NOT the row's live sizeHint(). MainWindow
    // overrides the auto-measured width immediately after
    // registerChromeBarItems() returns, via
    // RxDashboard::residualWidth() -- registering the raw sizeHint()
    // would double-count AGC (already counted at its own rung-9
    // registration below) and drift stale every time a pill toggles.
    // Previously not registered at all, which under-counted the width
    // budget by the row's own ~130-160 px (final-fix-wave finding 5;
    // design doc §5.1 invariant 2).
    add(c, w.rxDashRow, nullptr, 0, QString());

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

    // Both chain tags share rung 4. Chain 1 is constructed on every SKU;
    // single-ADC boards gate its visibility via setItemAvailable on
    // rxFilterChainCount, not by leaving this widget null.
    add(c, w.chain0, nullptr, 4,
        QCoreApplication::translate("ChromeBar", "CH"));
    add(c, w.chain1, nullptr, 4, QString());

    // Rungs 5..9, one pill each, right to left.
    static const char* const kPillNames[] = {"SQL", "APF", "NB", "NR", "AGC"};
    for (int rung = 5; rung <= 9; ++rung) {
        add(c, w.pillByRung.value(rung), nullptr, rung,
            QCoreApplication::translate("ChromeBar", kPillNames[rung - 5]));
    }

    // Rung 10, last resort: the stubs fold only after every live reading
    // has already gone.
    //
    // The band-stack dots fold at this rung too but are registered
    // separately, because they sit at the very head of the bar ahead of
    // +PAN rather than inside the group. Rung governs visibility, the
    // layout governs position, and the two are independent.
    add(c, w.bandStackLabel, nullptr, 10, QString());
    add(c, w.placeholderGroup, w.placeholderSep, 10,
        QCoreApplication::translate("ChromeBar", "Band Stack / CWX / DVK / FDX"));

    // Rung 11, folds after even the stubs. TNF was an inert NYI label when
    // this ladder was designed, and folded with the other placeholders.
    // The tunable-notch-filter work on main (#313) made it a live toggle
    // that turns amber when notches exist and are being bypassed. That
    // warning outranks a row of stubs, so TNF is the last foldable item
    // on the bar.
    add(c, w.tnfLabel, nullptr, 11,
        QCoreApplication::translate("ChromeBar", "TNF"));
}

} // namespace NereusSDR
