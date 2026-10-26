#pragma once

// =================================================================
// src/gui/meters/SpacerItem.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-10-26 - RX/TX colour mode (Thetis renderSpacer: colour 1 while
//                 receiving, colour 2 while transmitting, fading in from 48
//                 on each transmit flip) and "padding sets height"
//                 (height = width x padding); both appended to the record,
//                 absent = off, so saved spacers keep the gradient.
//                 kSpacerThetisDefaults for new items. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
// =================================================================

/*  MeterManager.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2026 Richard Samphire MW0LGE

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

mw0lge@grange-lane.co.uk
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

#include "MeterItem.h"
#include <QColor>

namespace NereusSDR {

class SpacerItem : public MeterItem {
    Q_OBJECT
public:
    explicit SpacerItem(QObject* parent = nullptr);

    // From Thetis MeterManager.cs:16143 — Colour1 / Colour2 / Padding
    void setColour1(const QColor& c) { m_colour1 = c; }
    QColor colour1() const { return m_colour1; }

    void setColour2(const QColor& c) { m_colour2 = c; }
    QColor colour2() const { return m_colour2; }

    void setPadding(float p) { m_padding = p; }
    float padding() const { return m_padding; }

    // RX/TX colour mode. On: colour 1 while receiving, colour 2 while
    // transmitting, fading in from 48 on each transmit flip (Thetis
    // renderSpacer, MeterManager.cs:36544-36576 [v2.10.3.15]). Off (a
    // record saved before the option, and the NereusSDR look): a vertical
    // gradient from colour 1 to colour 2.
    void setRxTxColourMode(bool on) { m_rxTxColourMode = on; }
    bool rxTxColourMode() const { return m_rxTxColourMode; }

    // Padding sets height. On: the spacer is drawn width x padding tall
    // from its top edge (Thetis MeterManager.cs:27627 [v2.10.3.15],
    // spacer.Size = (width, Padding)). Off: it fills its rectangle.
    void setPaddingSetsHeight(bool on) { m_paddingSetsHeight = on; }
    bool paddingSetsHeight() const { return m_paddingSetsHeight; }

    // The rectangle the spacer paints, in pixels.
    QRect spacerRect(int widgetW, int widgetH) const;
    // The colour shown now in RX/TX colour mode, with its alpha replaced by
    // the flip fade (Thetis getDXBrushForColour replaceAlpha).
    QColor rxTxColour() const;
    // RX/TX colour mode flip fade: 48..255; 255 is fully shown.
    int flipFadeValue() const { return m_flipFadeValue; }

    void setTransmitting(bool tx) override;
    bool advanceMeter(qint64 monotonicMs) override;
    void applyNewItemDefaults() override;

    Layer renderLayer() const override { return Layer::Background; }
    void paint(QPainter& p, int widgetW, int widgetH) override;
    QString serialize() const override;
    bool deserialize(const QString& data) override;

private:
    // The transmit state the colour follows; a foreign session's entry
    // stays receiving, as its fade does (MeterItem::advanceFade).
    bool effectiveTransmitting() const { return transmitting() && !foreignSession(); }
    // Thetis do_cover_fade (MeterManager.cs:36553 [v2.10.3.15]).
    bool coverFadeActive() const;
    // Restarts the flip fade at 48 when the transmit state changed since
    // the last flip (MeterManager.cs:36554-36560 [v2.10.3.15]).
    bool checkFlip();

    // From Thetis MeterManager.cs:16123 — default Color.FromArgb(32, 32, 32)
    QColor m_colour1{0x20, 0x20, 0x20};
    QColor m_colour2{0x20, 0x20, 0x20};
    // From Thetis MeterManager.cs:16126 — default 0.1f
    float  m_padding{0.1f};
    bool   m_rxTxColourMode{false};
    bool   m_paddingSetsHeight{false};

    // Runtime only (not saved): Thetis MOXFadeToggle / FadeValue.
    bool   m_moxFadeToggle{false};
    int    m_flipFadeValue{255};
    qint64 m_lastFlipStepMs{-1};
    bool   m_flipRepaintPending{false};
};

// New spacers (makeEntry) take Thetis's spacer.
// From Thetis MeterManager.cs:17441-17447 [v2.10.3.15] (clsSpacerItem
// constructor). Thetis always shows the RX/TX colour, so new spacers turn
// the mode on, and Thetis sizes every spacer by its padding (:27627).
struct SpacerItemThetisDefaults {
    bool  rxTxColourMode;
    bool  paddingSetsHeight;
    QRgb  colour1;
    QRgb  colour2;
    float padding;
};
inline constexpr SpacerItemThetisDefaults kSpacerThetisDefaults{
    true,                       // renderSpacer RX/TX colour (:36544-36576)
    true,                       // spacer.Size = (width, Padding) (:27627)
    qRgb(32, 32, 32),           // _colour1 = Color.FromArgb(32, 32, 32) (:17443)
    qRgb(32, 32, 32),           // _colour2 = Color.FromArgb(32, 32, 32) (:17444)
    0.1f,                       // _padding = 0.1f (:17446)
};

} // namespace NereusSDR
