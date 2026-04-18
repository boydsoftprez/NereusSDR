#pragma once

// =================================================================
// src/gui/widgets/VfoModeContainers.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/dsp.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//                 Structural pattern follows AetherSDR (ten9876/AetherSDR,
//                 GPLv3).
// =================================================================

//=================================================================
// console.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems 
// Copyright (C) 2010-2020  Doug Wigley
// Credit is given to Sizenko Alexander of Style-7 (http://www.styleseven.com/) for the Digital-7 font.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Modifications to support the Behringer Midi controllers
// by Chris Codella, W2PA, May 2017.  Indicated by //-W2PA comment lines. 
// Modifications for using the new database import function.  W2PA, 29 May 2017
// Support QSK, possible with Protocol-2 firmware v1.7 (Orion-MkI and Orion-MkII), and later.  W2PA, 5 April 2019 
// Modfied heavily - Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

// Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12

/*  wdsp.cs

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013-2017 Warren Pratt, NR0V

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at  

warren@wpratt.com

*/
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

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
