// =================================================================
// src/gui/AntennaPopupBuilder.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original file. Consolidates capability-gated
// antenna popup menu construction. Logic derived from existing per-widget
// ad-hoc constructors in VfoWidget.cpp and RxApplet.cpp; no direct Thetis
// port. BoardCapabilities and SkuUiProfile flags come from Thetis ports in
// their respective files.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-02 — Written for NereusSDR by J.J. Boyd (KG4VCF), with
//                AI-assisted implementation via Anthropic Claude Code.
//                Per docs/superpowers/plans/2026-05-01-ui-polish-cross-surface.md §B3.
//   2026-09-27 - The Special section follows rxOutOnTxPresent, the BYPS
//                gate (R-IOS-06, R-IOS-27). J.J. Boyd (KG4VCF), with
//                AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#pragma once

#include <QMenu>
#include <QString>
#include <QStringList>

#include "core/BoardCapabilities.h"
#include "core/SkuUiProfile.h"

namespace NereusSDR {

// Capability-gated antenna popup / label factory.
//
// Used by VfoWidget header, RxApplet antenna buttons, and
// SpectrumOverlayPanel ANT flyout — all three share this builder so popup
// contents stay in sync.
//
// Design: docs/superpowers/plans/2026-05-01-ui-polish-cross-surface.md §B3.
//
// Popup structure (RX mode):
//   [Main TX/RX]  ANT1 .. ANTn   (up to antennaInputCount, max 3)
//   [RX only]     sku.rxOnlyLabels[0..2]   (only when rxOnlyAntennaCount > 0)
//   [Special]     "RX out on TX"            (only when rxOutOnTxPresent)
//
// Popup structure (TX mode):
//   [Main TX/RX]  ANT1 .. ANTn   (RX-only and bypass sections omitted)
//
// No-Alex boards (hasAlex = false):
//   Empty popup — antenna routing is not available and buttons should be
//   hidden by the calling widget (see RxApplet::setBoardCapabilities and
//   VfoWidget::setBoardCapabilities).
//
// kPopupMenu stylesheet: the caller is responsible for setting the
// kPopupMenu stylesheet on the QMenu BEFORE calling populate(), per the
// Phase 3P-I-a T22 invariant enforced by tst_popup_style_coverage.
//
class AntennaPopupBuilder
{
public:
    enum class Mode { RX, TX };

    // Populate `menu` with antenna actions gated by the board's capabilities
    // and SKU-level UI profile.
    //
    // - In Mode::RX: shows Main TX/RX (ANT1-N) + RX-only (sku.rxOnlyLabels
    //   when rxOnlyAntennaCount > 0) + Special bypass (when rxOutOnTxPresent).
    // - In Mode::TX: shows Main TX/RX only. RX-only and bypass omitted.
    //
    // The currently-active antenna (`current`) is shown checked.
    // Each action's data() carries the antenna identifier as QString.
    //
    // Note: caller must set kPopupMenu stylesheet on `menu` before calling
    // this function (enforced by tst_popup_style_coverage).
    static void populate(QMenu* menu,
                         const BoardCapabilities& caps,
                         const SkuUiProfile& sku,
                         Mode mode,
                         const QString& current);

    // Returns the ordered list of antenna labels (no separators) for a given
    // capability + SKU + mode. Useful for QComboBox widgets that don't use a
    // QMenu directly (e.g. SpectrumOverlayPanel ANT flyout combos).
    static QStringList labels(const BoardCapabilities& caps,
                              const SkuUiProfile& sku,
                              Mode mode);
};

}  // namespace NereusSDR
