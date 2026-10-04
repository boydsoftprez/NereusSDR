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
//                 (serial CAT / rigctld enable rows + PTT LEDs).
//                 All controls NYI — wired in later phase.
//   2026-05-10 — Phase 24 (Task 24.1): stripped TCI button row; TCI
//                 controls now live in TciApplet (Phase 21, 0b615a7).
// =================================================================

#pragma once
#include "AppletWidget.h"
#include <QPointer>

class QPushButton;
class QLabel;
class QComboBox;

namespace NereusSDR {

class CatService;

// CAT / rigctld control interfaces.
// Live Thetis CAT; rigctld and VAX/IQ retain their separate delivery boundaries.
// TCI controls live in TciApplet (Phase 21).
//
// Controls:
//   1. CAT TCP enable + LEDs — QPushButton green "TCP" + 4x QLabel (A/B/C/D)
//   2. CAT PTY enable + paths — QPushButton green "PTY" + 4x QLabel paths
//   3. VAX enable + meters — QPushButton green "VAX" + 4x QLabel channel status
//   4. VAX IQ enable + rate — QPushButton green "IQ" + QComboBox rate
class CatApplet : public AppletWidget {
    Q_OBJECT
public:
    explicit CatApplet(RadioModel* model, QWidget* parent = nullptr);

    QString appletId()    const override { return QStringLiteral("cat"); }
    QString appletTitle() const override { return QStringLiteral("CAT"); }
    void    syncFromModel() override;

private:
    void buildUI();
    QPointer<CatService> m_service;
    bool m_localHost{false};

    // Control 1 — CAT TCP: enable button + 4 status LEDs (A/B/C/D)
    QPushButton* m_tcpBtn        = nullptr;
    QLabel*      m_tcpLed[4]     = {};

    // Control 2 — CAT PTY: enable button + 4 path labels
    QPushButton* m_ptyBtn        = nullptr;
    QLabel*      m_ptyPath[4]    = {};

    // Control 3 — VAX: enable button + 4 channel status labels
    QPushButton* m_vaxBtn        = nullptr;
    QLabel*      m_vaxStatus[4]  = {};

    // Control 5 — VAX IQ: enable button + rate combo
    QPushButton* m_iqBtn         = nullptr;
    QComboBox*   m_iqRateCombo   = nullptr;
};

} // namespace NereusSDR
