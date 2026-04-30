#pragma once

// =================================================================
// src/gui/widgets/OcLedStripWidget.h  (NereusSDR)
// =================================================================
//
// Ported from mi0bot-Thetis source:
//   Project Files/Source/Console/ucOCLedStrip.cs
//   (mi0bot v2.10.3.13-beta2 / @c26a8a4)
//
// Reusable N-bit LED strip — one LED per bit of a `quint8` byte.
// Used by Hl2OptionsTab's I/O Pin State group box for the output (8 LED)
// and input (6 LED) strips, and shared with Hl2IoBoardTab's status bar
// 7-pin OC indicator.  Mirrors mi0bot's `ucOCLedStrip` UserControl
// (Bits + DisplayBits properties, plus optional click-to-toggle).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-30 — New for Phase 3L HL2 Filter visibility brainstorm.
//                Reusable extraction of the inline LED strip from
//                Hl2IoBoardTab status bar (Hl2IoBoardTab.cpp:321-330).
//                J.J. Boyd (KG4VCF), with AI-assisted transformation
//                via Anthropic Claude Code.
// =================================================================
//
//=================================================================
// ucOCLedStrip.cs (mi0bot fork)
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
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
//=================================================================
// Continual modifications Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//=================================================================
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves his       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

#include <QFrame>
#include <QtGlobal>
#include <vector>

class QHBoxLayout;

namespace NereusSDR {

// Reusable 1..8 bit LED strip.  Mirrors mi0bot's ucOCLedStrip:
//   - `bits()`/`setBits()`            ↔ ucOCLedStrip.Bits
//   - `displayBits()`/`setDisplayBits()` ↔ ucOCLedStrip.DisplayBits
//   - `setInteractive(bool)`          ↔ click handler hookup (off = read-only)
//   - `pinClicked(int idx)`           ↔ ucOCLedStrip.MouseDown event
class OcLedStripWidget : public QFrame {
    Q_OBJECT
public:
    explicit OcLedStripWidget(QWidget* parent = nullptr);

    // Number of LEDs to render (1..8, default 8).
    int  displayBits() const { return m_displayBits; }
    void setDisplayBits(int bits);

    // Current bit state (lower `displayBits()` bits are rendered).
    quint8 bits() const { return m_bits; }
    void   setBits(quint8 b);

    // When true, clicking a LED emits pinClicked(idx).  When false the
    // strip is read-only.  Default = false.
    bool isInteractive() const { return m_interactive; }
    void setInteractive(bool on);

    // Per-LED tooltip override (default: "OC pin <idx+1>").
    void setLedTooltip(int idx, const QString& tooltip);

signals:
    // Emitted when the user clicks a LED in interactive mode.  `idx` is
    // 0-based.  Caller is expected to compose the matching wire-side
    // toggle (e.g. IoBoardHl2::enqueueI2c with the new mask).
    void pinClicked(int idx);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void rebuild();
    void refreshLedColors();

    QHBoxLayout*           m_row{nullptr};
    std::vector<QFrame*>   m_leds;
    quint8                 m_bits{0};
    int                    m_displayBits{8};
    bool                   m_interactive{false};
};

} // namespace NereusSDR
