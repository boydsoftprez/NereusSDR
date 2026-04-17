#pragma once

// =================================================================
// src/gui/widgets/TriBtn.h  (NereusSDR)
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
//   2026-04-16 — Ported/adapted in C++20/Qt6 for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
//                 `TriBtn` ported from AetherSDR
//                 `src/gui/VfoWidget.cpp:97-129`.
// =================================================================

#include <QPushButton>

namespace NereusSDR {

// TriBtn — fixed-size 22×22 button that paints a filled directional triangle.
// Ported from AetherSDR src/gui/VfoWidget.cpp:97-129.
// Used for RIT/XIT zero buttons, FM offset step controls, CW pitch steppers.
class TriBtn : public QPushButton {
    Q_OBJECT
public:
    enum Dir { Left, Right };
    explicit TriBtn(Dir dir, QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* ev) override;

private:
    Dir m_dir;
};

} // namespace NereusSDR
