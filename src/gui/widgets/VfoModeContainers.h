#pragma once
// =================================================================
// src/gui/widgets/VfoModeContainers.h  (NereusSDR)
// =================================================================
//
// Ported from multiple Thetis sources:
//   Project Files/Source/Console/console.cs
//   Project Files/Source/Console/dsp.cs
//
// Original Thetis copyright and license (preserved per GNU GPL,
// representing the union of contributors across all cited sources):
//
//   Thetis is a C# implementation of a Software Defined Radio.
//   Copyright (C) 2004-2009  FlexRadio Systems
//   Copyright (C) 2010-2020  Doug Wigley (W5WC)
//   Copyright (C) 2013-2017  Warren Pratt (NR0V) [dsp.cs]
//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified
//   Copyright (C) 2017-2019  Chris Codella (W2PA) — console.cs / setup.cs / NetworkIO.cs inline mods
//   Copyright (C) 2018-2025  Laurence Barker (G8NJJ) — Andromeda / Aries / Saturn / ANAN-G2
//   Copyright (C) 2020-2025  Joe (WD5Y) — console.cs UI tweaks / mute / ForeColor
//
//   This program is free software; you can redistribute it and/or
//   modify it under the terms of the GNU General Public License
//   as published by the Free Software Foundation; either version 2
//   of the License, or (at your option) any later version.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
// Dual-Licensing Statement (applies ONLY to Richard Samphire MW0LGE's
// contributions — preserved verbatim from Thetis LICENSE-DUAL-LICENSING):
//
//   For any code originally written by Richard Samphire MW0LGE, or for
//   any modifications made by him, the copyright holder for those
//   portions (Richard Samphire) reserves the right to use, license, and
//   distribute such code under different terms, including closed-source
//   and proprietary licences, in addition to the GNU General Public
//   License granted in LICENCE. Nothing in this statement restricts any
//   rights granted to recipients under the GNU GPL.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Synthesized in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code. Combines logic from the Thetis sources
//                 listed above.
// =================================================================

// NereusSDR native mode container widgets; Qt skeleton patterns informed
// by AetherSDR src/gui/VfoWidget.cpp:996-1300 (mode sub-widget blocks).
// DSP behavior bound to S1.6 SliceModel stub API — Thetis-authoritative
// values; no WDSP forwarding yet (Stage 2).

#include <QWidget>
#include <QPointer>
#include <QSpinBox>
#include <QPushButton>

class QLabel;

namespace NereusSDR {

class SliceModel;
class GuardedComboBox;
class ScrollableLabel;
class TriBtn;

// ── FmOptContainer ────────────────────────────────────────────────────────
// Three-row widget for FM-mode options:
//   Row 1: CTCSS tone mode combo + CTCSS tone value combo (Hz)
//   Row 2: Repeater offset spinbox (kHz)
//   Row 3: TX direction (Low / Simplex / High) + Reverse toggle
//
// Binds to SliceModel: fmCtcssMode, fmCtcssValueHz, fmOffsetHz,
//   fmTxMode (FmTxMode::Low/Simplex/High), fmReverse.
class FmOptContainer : public QWidget {
    Q_OBJECT
public:
    explicit FmOptContainer(QWidget* parent = nullptr);
    void setSlice(SliceModel* s);
    void syncFromSlice();  // reads slice state into widgets; safe if m_slice is null

private:
    void buildUi();

    QPointer<SliceModel> m_slice;

    GuardedComboBox* m_toneModeCmb{nullptr};
    GuardedComboBox* m_toneValueCmb{nullptr};
    QSpinBox*        m_offsetKhzSpin{nullptr};  // FM repeater offset in kHz
    QPushButton*     m_txLowBtn{nullptr};       // "Low" repeater direction (TX below RX)
    QPushButton*     m_simplexBtn{nullptr};     // no repeater offset
    QPushButton*     m_txHighBtn{nullptr};      // "High" repeater direction (TX above RX)
    QPushButton*     m_revBtn{nullptr};         // reverse-listen toggle
};

// ── DigOffsetContainer ────────────────────────────────────────────────────
// Single-row widget: TriBtn(−) + ScrollableLabel + TriBtn(+).
// Routes read/write through dspMode(): DIGL → diglOffsetHz,
// DIGU → diguOffsetHz (Thetis uses separate per-sideband offsets).
class DigOffsetContainer : public QWidget {
    Q_OBJECT
public:
    explicit DigOffsetContainer(QWidget* parent = nullptr);
    void setSlice(SliceModel* s);
    void syncFromSlice();  // reads slice state into widget; safe if m_slice is null

private:
    void buildUi();
    int  currentOffsetHz() const;   // routes by dspMode() to digl or digu
    void applyOffset(int hz);       // routes setter the same way

    QPointer<SliceModel> m_slice;

    ScrollableLabel* m_offsetLabel{nullptr};
    TriBtn*          m_minusBtn{nullptr};
    TriBtn*          m_plusBtn{nullptr};
};

// ── RttyMarkShiftContainer ────────────────────────────────────────────────
// Two sub-groups side by side: Mark (frequency) and Shift (spacing).
//   Mark sub-group:  TriBtn(−) + ScrollableLabel + TriBtn(+)  — step 25 Hz
//   Shift sub-group: TriBtn(−) + ScrollableLabel + TriBtn(+)  — step 5 Hz
//
// Binds to SliceModel: rttyMarkHz (default 2295), rttyShiftHz (default 170).
// Step constants from AetherSDR VfoWidget.cpp; these are UX choices, not DSP
// constants, so they are native NereusSDR.
class RttyMarkShiftContainer : public QWidget {
    Q_OBJECT
public:
    explicit RttyMarkShiftContainer(QWidget* parent = nullptr);
    void setSlice(SliceModel* s);
    void syncFromSlice();  // reads slice state into widgets; safe if m_slice is null

private:
    void buildUi();

    QPointer<SliceModel> m_slice;

    ScrollableLabel* m_markLabel{nullptr};
    ScrollableLabel* m_shiftLabel{nullptr};
    TriBtn*          m_markMinus{nullptr};
    TriBtn*          m_markPlus{nullptr};
    TriBtn*          m_shiftMinus{nullptr};
    TriBtn*          m_shiftPlus{nullptr};
};

}  // namespace NereusSDR
