// no-port-check: AetherSDR-derived NereusSDR file. 5-tile visual layout
// picker is adapted structurally from AetherSDR PanLayoutDialog
// [@0cd4559]. NereusSDR ships the 5 templates that the Phase 3F design
// settled on (1 / 2v / 2h / 12h / 2x2) rather than the AetherSDR catalog
// of 12. Registered in docs/attribution/aethersdr-reconciliation.md.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/gui/PanLayoutDialog.h  (NereusSDR)
// =================================================================
//
// Ported (structurally) from AetherSDR PanLayoutDialog [@0cd4559].
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
// =================================================================
#pragma once

#include <QDialog>
#include <QString>

namespace NereusSDR {

/// 5-tile visual layout picker. Ported structurally from AetherSDR
/// PanLayoutDialog. Tiles: "1" (Single), "2v" (Stacked), "2h"
/// (Side-by-Side), "12h" (Wide + 2 with WIDEBAND badge), "2x2" (Grid).
class PanLayoutDialog : public QDialog {
    Q_OBJECT
public:
    explicit PanLayoutDialog(QWidget* parent = nullptr);
    ~PanLayoutDialog() override;

    QString selectedLayout() const { return m_selected; }

private:
    QString m_selected;
};

} // namespace NereusSDR
