// =================================================================
// src/gui/applets/CwxApplet.h  (NereusSDR)
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
//                 Port of AetherSDR `src/gui/CwxPanel.{h,cpp}` (CW text
//                 entry + WPM + message-slot buttons). Renamed to
//                 CwxApplet in NereusSDR. All controls NYI.
// =================================================================

#pragma once
#include "AppletWidget.h"

class QPushButton;
class QTextEdit;
class QSlider;
class QLabel;

namespace NereusSDR {

// CW keyer message composer and memory keyer.
// NYI — Phase 3I-2 (CW TX, sidetone, firmware keyer, QSK/break-in).
//
// Controls:
//   1. Text input field     — QTextEdit, 60px height, dark styled
//   2. Send button          — QPushButton "Send" (non-toggle)
//   3. Speed override       — QSlider (5..60 WPM) + inset "20"
//   4. Memory buttons       — QPushButton x8 ("M1"-"M8"), 2 rows of 4
//   5. Repeat toggle+interval — QPushButton green "Rpt" + QSlider (1..60s) + inset
//   6. Keyboard-to-CW toggle — QPushButton green "KB→CW"
class CwxApplet : public AppletWidget {
    Q_OBJECT
public:
    explicit CwxApplet(RadioModel* model, QWidget* parent = nullptr);

    QString appletId()    const override { return QStringLiteral("cwx"); }
    QString appletTitle() const override { return QStringLiteral("CW Keyer"); }
    void    syncFromModel() override;

private:
    void buildUI();

    // Control 1 — text input
    QTextEdit*   m_textEdit      = nullptr;
    // Control 2 — send button
    QPushButton* m_sendBtn       = nullptr;

    // Control 3 — speed override slider (5..60 WPM)
    QSlider* m_speedSlider       = nullptr;
    QLabel*  m_speedValue        = nullptr;

    // Control 4 — memory buttons M1-M8 (2 rows of 4)
    QPushButton* m_memBtn[8]     = {};

    // Control 5 — repeat toggle + interval slider
    QPushButton* m_repeatBtn     = nullptr;
    QSlider*     m_repeatSlider  = nullptr;
    QLabel*      m_repeatValue   = nullptr;

    // Control 6 — keyboard-to-CW toggle
    QPushButton* m_kbCwBtn       = nullptr;
};

} // namespace NereusSDR
