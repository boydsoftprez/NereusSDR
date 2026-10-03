// =================================================================
// src/gui/AntennaPopupBuilder.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original file. See AntennaPopupBuilder.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-02 — Written for NereusSDR by J.J. Boyd (KG4VCF), with
//                AI-assisted implementation via Anthropic Claude Code.
//                Per docs/superpowers/plans/2026-05-01-ui-polish-cross-surface.md §B3.
//   2026-09-27 - "RX out on TX" follows the BYPS gate (rxOutOnTxPresent:
//                the relay and the product's RX out on TX; R-IOS-06,
//                R-IOS-27). J.J. Boyd (KG4VCF), with AI-assisted
//                implementation via Anthropic Claude Code.
// =================================================================

#include "AntennaPopupBuilder.h"

namespace NereusSDR {

void AntennaPopupBuilder::populate(QMenu* menu,
                                   const BoardCapabilities& caps,
                                   const SkuUiProfile& sku,
                                   Mode mode,
                                   const QString& current)
{
    if (!menu) { return; }

    auto addAntAction = [menu, &current](const QString& label) {
        QAction* act = menu->addAction(label);
        act->setData(label);
        act->setCheckable(true);
        act->setChecked(label == current);
    };

    // No Alex hardware → no antenna switching available.
    // The calling widget should already hide the button via
    // setBoardCapabilities(), but return gracefully if called.
    if (!caps.hasAlex && !caps.hasAlex2) {
        return;
    }

    // Section: Main TX/RX
    menu->addSection(QStringLiteral("Main TX/RX"));
    const int mainCount = qMin(caps.antennaInputCount, 3);
    for (int i = 1; i <= mainCount; ++i) {
        addAntAction(QStringLiteral("ANT%1").arg(i));
    }

    // Section: RX only — shown only in RX mode when the board has RX-only
    // inputs (rxOnlyAntennaCount > 0). Labels come from SkuUiProfile so
    // they match the SKU-specific naming (RX1/RX2/XVTR vs EXT2/EXT1/XVTR
    // vs BYPS/EXT1/XVTR — from Thetis setup.cs:19832-20375 [v2.10.3.13]).
    if (mode == Mode::RX && caps.rxOnlyAntennaCount > 0) {
        menu->addSection(QStringLiteral("RX only"));
        const int rxOnlyCount = qMin(static_cast<int>(sku.rxOnlyLabels.size()),
                                     caps.rxOnlyAntennaCount);
        for (int i = 0; i < rxOnlyCount; ++i) {
            const QString& lbl = sku.rxOnlyLabels[static_cast<size_t>(i)];
            if (!lbl.isEmpty()) {
                addAntAction(lbl);
            }
        }
    }

    // Section: Special — RX-bypass relay, shown only in RX mode, on the
    // VFO flag's BYPS gate: the board's relay and the product's RX out on
    // TX (rxOutOnTxPresent). The relay alone offered it on a G2, whose
    // BYPS button stays hidden.
    if (mode == Mode::RX && rxOutOnTxPresent(caps, sku)) {
        menu->addSection(QStringLiteral("Special"));
        addAntAction(QStringLiteral("RX out on TX"));
    }
}

QStringList AntennaPopupBuilder::labels(const BoardCapabilities& caps,
                                        const SkuUiProfile& sku,
                                        Mode mode)
{
    QMenu tempMenu;
    populate(&tempMenu, caps, sku, mode, QString());
    QStringList out;
    for (QAction* a : tempMenu.actions()) {
        if (!a->isSeparator() && a->data().isValid()) {
            const QString label = a->data().toString();
            if (!label.isEmpty()) {
                out << label;
            }
        }
    }
    return out;
}

}  // namespace NereusSDR
