// no-port-check: AetherSDR-derived NereusSDR file. Three-column painted
// thumbnail grid, structurally from AetherSDR PanLayoutDialog [@c6481cb].
// Registered in docs/attribution/aethersdr-contributor-index.md:270.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/gui/PanLayoutDialog.h  (NereusSDR)
// =================================================================
//
// Ported (structurally) from AetherSDR PanLayoutDialog [@c6481cb].
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-27  J.J. Boyd / KG4VCF  Phase 3F Sub-Epic D Task 9.
//                                    5-tile visual picker for the
//                                    PanadapterStack layout templates
//                                    (1 / 2v / 2h / 12h / 2x2). Click
//                                    any tile to stash selectedLayout()
//                                    and accept(); consumers (the +PAN
//                                    bottom-bar dropdown in Task 10 and
//                                    the View > Pan Layout submenu in
//                                    Task 14) then call
//                                    PanadapterStack::applyLayout. The
//                                    AetherSDR original ships a richer
//                                    12-template catalog; NereusSDR
//                                    intentionally narrows to the 5 the
//                                    Phase 3F design settled on.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-08-02  J.J. Boyd / KG4VCF  Bottom-banner + pan-menu epic,
//                                    Task B3. Rebuilt around AetherSDR's
//                                    three-column painted thumbnail
//                                    grid (`kPanLayouts` / `LayoutThumbnail`
//                                    from Task B2), all nine layouts the
//                                    Phase 3F design settled on (design
//                                    doc §8.3), re-cited at @c6481cb.
//                                    Gated on the connected board's real
//                                    slice capacity: a layout the board
//                                    cannot host is HIDDEN rather than
//                                    greyed, with a footer line naming
//                                    the board and the count (design doc
//                                    §8.4 -- deliberate divergence from
//                                    AetherSDR, which greys and says
//                                    nothing because it relays a
//                                    capacity number from an API it does
//                                    not own). AI-assisted transformation
//                                    via Anthropic Claude Code.
// =================================================================
#pragma once

#include <QDialog>
#include <QString>
#include <QStringList>

namespace NereusSDR {

/// Three-column painted thumbnail grid, ported structurally from
/// AetherSDR PanLayoutDialog. Shows every layout in `kPanLayouts` whose
/// `panCount` fits the connected board's `maxSlices`; layouts that do not
/// fit are hidden rather than greyed, with a footer line naming the board
/// and how many were hidden (design doc §8.4).
class PanLayoutDialog : public QDialog {
    Q_OBJECT

public:
    /// maxSlices comes from BoardCapabilities for the connected radio.
    /// boardName appears in the footer when layouts are hidden.
    PanLayoutDialog(int maxSlices, const QString& currentLayoutId,
                    const QString& boardName, QWidget* parent = nullptr);
    ~PanLayoutDialog() override;

    QString selectedLayout() const { return m_selected; }

    /// Layout ids actually shown, in grid order. For tests.
    QStringList visibleLayoutIds() const { return m_visibleIds; }

    /// Footer sentence, empty when nothing was hidden.
    QString footerText() const { return m_footerText; }

private:
    void buildUi(int maxSlices, const QString& currentLayoutId,
                 const QString& boardName);

    QString     m_selected;
    QStringList m_visibleIds;
    QString     m_footerText;
};

} // namespace NereusSDR
