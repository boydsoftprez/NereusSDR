// =================================================================
// src/gui/widgets/OcLedStripWidget.cpp  (NereusSDR)
// =================================================================
//
// Ported from mi0bot-Thetis source:
//   Project Files/Source/Console/ucOCLedStrip.cs
//   (mi0bot v2.10.3.13-beta2 / @c26a8a4)
//
// See OcLedStripWidget.h for the full design + scope rationale.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-30 — New for Phase 3L HL2 Filter visibility brainstorm.
//                Reusable extraction of inline status-bar LED strip.
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

#include "OcLedStripWidget.h"

#include <QHBoxLayout>
#include <QMouseEvent>

namespace NereusSDR {

OcLedStripWidget::OcLedStripWidget(QWidget* parent)
    : QFrame(parent)
{
    m_row = new QHBoxLayout(this);
    m_row->setContentsMargins(0, 0, 0, 0);
    m_row->setSpacing(2);
    rebuild();
}

void OcLedStripWidget::setDisplayBits(int bits)
{
    const int clamped = qBound(1, bits, 8);
    if (clamped == m_displayBits) { return; }
    m_displayBits = clamped;
    rebuild();
}

void OcLedStripWidget::setBits(quint8 b)
{
    if (b == m_bits) { return; }
    m_bits = b;
    refreshLedColors();
}

void OcLedStripWidget::setInteractive(bool on)
{
    m_interactive = on;
    setCursor(on ? Qt::PointingHandCursor : Qt::ArrowCursor);
}

void OcLedStripWidget::setLedTooltip(int idx, const QString& tooltip)
{
    if (idx < 0 || idx >= static_cast<int>(m_leds.size())) { return; }
    m_leds[idx]->setToolTip(tooltip);
}

void OcLedStripWidget::rebuild()
{
    // Clear any existing LEDs.
    for (auto* led : m_leds) {
        m_row->removeWidget(led);
        led->deleteLater();
    }
    m_leds.clear();

    m_leds.reserve(m_displayBits);
    for (int i = 0; i < m_displayBits; ++i) {
        auto* led = new QFrame(this);
        led->setFixedSize(10, 10);
        led->setFrameShape(QFrame::StyledPanel);
        led->setToolTip(tr("OC pin %1").arg(i + 1));
        led->installEventFilter(this);
        m_leds.push_back(led);
        m_row->addWidget(led);
    }
    m_row->addStretch();
    refreshLedColors();
}

void OcLedStripWidget::refreshLedColors()
{
    for (int i = 0; i < static_cast<int>(m_leds.size()); ++i) {
        const bool on = (m_bits >> i) & 0x01;
        m_leds[i]->setStyleSheet(on
            ? QStringLiteral("QFrame { background: #44ff44; border: 1px solid #88ff88; "
                             "border-radius: 5px; }")
            : QStringLiteral("QFrame { background: #222; border: 1px solid #555; "
                             "border-radius: 5px; }"));
    }
}

bool OcLedStripWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (m_interactive && event->type() == QEvent::MouseButtonRelease) {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            for (int i = 0; i < static_cast<int>(m_leds.size()); ++i) {
                if (m_leds[i] == watched) {
                    emit pinClicked(i);
                    return true;
                }
            }
        }
    }
    return QFrame::eventFilter(watched, event);
}

} // namespace NereusSDR
