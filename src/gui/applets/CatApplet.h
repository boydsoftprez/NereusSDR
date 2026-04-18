// =================================================================
// src/gui/applets/CatApplet.h  (NereusSDR)
// =================================================================
//
// Source attribution (AetherSDR — GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 §5 requirements.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-18 — Ported/adapted in C++20/Qt6 for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
//                 Layout mirrors AetherSDR `src/gui/CatApplet.{h,cpp}`
//                 (serial CAT / rigctl / TCI enable rows + PTT LEDs).
//                 All controls NYI — wired in later phase.
// =================================================================

#pragma once
#include "AppletWidget.h"

class QPushButton;
class QLabel;
class QLineEdit;
class QComboBox;

namespace NereusSDR {

// CAT / rigctld / TCI control interfaces.
// NYI — Phase 3K (CAT/rigctld) + 3J (TCI) + 3-DAX (DAX/IQ).
//
// Controls:
//   1. CAT TCP enable + LEDs — QPushButton green "TCP" + 4x QLabel (A/B/C/D)
//   2. CAT PTY enable + paths — QPushButton green "PTY" + 4x QLabel paths
//   3. TCI enable + port + LED — QPushButton green "TCI" + QLineEdit port + QLabel LED
//   4. DAX enable + meters — QPushButton green "DAX" + 4x QLabel channel status
//   5. DAX IQ enable + rate — QPushButton green "IQ" + QComboBox rate
class CatApplet : public AppletWidget {
    Q_OBJECT
public:
    explicit CatApplet(RadioModel* model, QWidget* parent = nullptr);

    QString appletId()    const override { return QStringLiteral("cat"); }
    QString appletTitle() const override { return QStringLiteral("CAT / TCI"); }
    void    syncFromModel() override;

private:
    void buildUI();

    // Control 1 — CAT TCP: enable button + 4 status LEDs (A/B/C/D)
    QPushButton* m_tcpBtn        = nullptr;
    QLabel*      m_tcpLed[4]     = {};

    // Control 2 — CAT PTY: enable button + 4 path labels
    QPushButton* m_ptyBtn        = nullptr;
    QLabel*      m_ptyPath[4]    = {};

    // Control 3 — TCI: enable button + port field + status LED
    QPushButton* m_tciBtn        = nullptr;
    QLineEdit*   m_tciPort       = nullptr;
    QLabel*      m_tciLed        = nullptr;

    // Control 4 — DAX: enable button + 4 channel status labels
    QPushButton* m_daxBtn        = nullptr;
    QLabel*      m_daxStatus[4]  = {};

    // Control 5 — DAX IQ: enable button + rate combo
    QPushButton* m_iqBtn         = nullptr;
    QComboBox*   m_iqRateCombo   = nullptr;
};

} // namespace NereusSDR
