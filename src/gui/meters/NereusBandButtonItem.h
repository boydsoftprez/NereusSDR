#pragma once

// =================================================================
// src/gui/meters/NereusBandButtonItem.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//
// Duplicate of src/gui/meters/BandButtonItem, the NereusSDR port of Thetis
// clsBandButtonBox. The band set, clicks, and band-stack request are that
// port. Each face is the PureSignal applet's raised button: a vertical
// gradient of two choosable colours. The resting pair defaults to the
// PureSignal button, and the selected pair defaults to its checked green.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-22 - Nereus-styled duplicate of BandButtonItem. Brian Sutton,
//                 AI-assisted via xAI Grok.
//   2026-10-22 - Resting and selected faces use the PureSignal button
//                 gradients, each a choosable pair. Brian Sutton,
//                 AI-assisted via xAI Grok.
//   2026-10-22 - Corner radius and border match the PureSignal button.
//                 Brian Sutton, AI-assisted via xAI Grok.
//   2026-10-24 - Default colors from NereusButtonFace.h, shared with the
//                 RX Filter copy. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
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

#include "BandButtonItem.h"
#include "NereusButtonFace.h"

namespace NereusSDR {

class NereusBandButtonItem : public BandButtonItem {
    Q_OBJECT

public:
    explicit NereusBandButtonItem(QObject* parent = nullptr);

    void setButtonTextColor(const QColor& color);
    QColor buttonTextColor() const { return m_textColor; }

    void setBackgroundTopColor(const QColor& color);
    void setBackgroundBottomColor(const QColor& color);
    QColor backgroundTopColor() const { return m_backgroundTop; }
    QColor backgroundBottomColor() const { return m_backgroundBottom; }

    void setSelectedTopColor(const QColor& color);
    void setSelectedBottomColor(const QColor& color);
    QColor selectedTopColor() const { return m_selectedTop; }
    QColor selectedBottomColor() const { return m_selectedBottom; }

    QString serialize() const override;
    bool deserialize(const QString& data) override;

protected:
    void paintButton(QPainter& painter, int index, const QRectF& rect) override;

private:
    void applyChrome();
    static QColor lighten(const QColor& color);

    // The Nereus button face defaults (NereusButtonFace.h).
    QColor m_textColor{QColor(NereusButtonFace::kText)};
    QColor m_backgroundTop{QColor(NereusButtonFace::kRestingTop)};
    QColor m_backgroundBottom{QColor(NereusButtonFace::kRestingBottom)};
    QColor m_selectedTop{QColor(NereusButtonFace::kSelectedTop)};
    QColor m_selectedBottom{QColor(NereusButtonFace::kSelectedBottom)};
};

} // namespace NereusSDR
