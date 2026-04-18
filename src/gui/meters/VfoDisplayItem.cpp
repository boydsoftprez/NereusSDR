// =================================================================
// src/gui/meters/VfoDisplayItem.cpp  (NereusSDR)
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

#include "VfoDisplayItem.h"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>

namespace NereusSDR {

VfoDisplayItem::VfoDisplayItem(QObject* parent) : MeterItem(parent) {}

QString VfoDisplayItem::formatFrequency(int64_t hz) const
{
    const int mhz = static_cast<int>(hz / 1000000);
    const int khz = static_cast<int>((hz / 1000) % 1000);
    const int h   = static_cast<int>(hz % 1000);
    return QStringLiteral("%1.%2.%3")
        .arg(mhz, 2).arg(khz, 3, 10, QLatin1Char('0')).arg(h, 3, 10, QLatin1Char('0'));
}

int64_t VfoDisplayItem::digitWeightAt(float xFraction) const
{
    // From Thetis clsVfoDisplay: 9 digit positions left-to-right
    static constexpr int64_t kDigitWeights[] = {
        10000000, 1000000, 100000, 10000, 1000, 100, 10, 1, 1
    };
    const int digitIndex = static_cast<int>(xFraction * 9.0f);
    if (digitIndex < 0) { return kDigitWeights[0]; }
    if (digitIndex > 8) { return kDigitWeights[8]; }
    return kDigitWeights[digitIndex];
}

void VfoDisplayItem::paint(QPainter& p, int widgetW, int widgetH)
{
    const QRect rect = pixelRect(widgetW, widgetH);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Background
    p.fillRect(rect, QColor(0x0a, 0x0a, 0x18));
    p.setPen(QPen(QColor(0x1e, 0x2e, 0x3e), 1));
    p.drawRect(rect);

    // RX/TX indicator bar at left
    const QColor stateColour = m_transmitting ? m_txColour : m_rxColour;
    p.fillRect(QRect(rect.left(), rect.top(), 3, rect.height()), stateColour);

    // Frequency text (top 60%)
    const QRect freqRect(rect.left() + 6, rect.top() + 2, rect.width() - 12, rect.height() * 6 / 10);
    QFont freqFont = p.font();
    freqFont.setPixelSize(qMax(12, freqRect.height() - 4));
    freqFont.setBold(true);
    p.setFont(freqFont);
    p.setPen(m_freqColour);
    p.drawText(freqRect, Qt::AlignVCenter | Qt::AlignLeft, formatFrequency(m_frequencyHz));

    // Split indicator
    if (m_split) {
        p.setPen(m_splitColour);
        QFont splitFont = freqFont;
        splitFont.setPixelSize(freqFont.pixelSize() / 2);
        p.setFont(splitFont);
        p.drawText(freqRect, Qt::AlignVCenter | Qt::AlignRight, QStringLiteral("SPLIT"));
    }

    // Labels below frequency (bottom 40%)
    const QRect labelRect(rect.left() + 6, rect.top() + rect.height() * 6 / 10,
                          rect.width() - 12, rect.height() * 4 / 10);
    QFont labelFont = p.font();
    labelFont.setPixelSize(qMax(9, labelRect.height() - 4));
    labelFont.setBold(false);
    p.setFont(labelFont);

    const int thirdW = labelRect.width() / 3;
    p.setPen(m_modeColour);
    p.drawText(QRect(labelRect.left(), labelRect.top(), thirdW, labelRect.height()), Qt::AlignCenter, m_modeLabel);
    p.setPen(m_filterColour);
    p.drawText(QRect(labelRect.left() + thirdW, labelRect.top(), thirdW, labelRect.height()), Qt::AlignCenter, m_filterLabel);
    p.setPen(m_bandColour);
    p.drawText(QRect(labelRect.left() + 2*thirdW, labelRect.top(), thirdW, labelRect.height()), Qt::AlignCenter, m_bandLabel);
}

bool VfoDisplayItem::handleWheel(QWheelEvent* event, int widgetW, int widgetH)
{
    const QRect rect = pixelRect(widgetW, widgetH);
    const QRect freqRect(rect.left() + 6, rect.top(), rect.width() - 12, rect.height() * 6 / 10);
    if (!freqRect.contains(event->position().toPoint())) { return false; }

    const float xFrac = static_cast<float>(event->position().x() - freqRect.left()) / freqRect.width();
    const int64_t weight = digitWeightAt(xFrac);
    const int direction = (event->angleDelta().y() > 0) ? 1 : -1;
    emit frequencyChangeRequested(direction * weight);
    return true;
}

bool VfoDisplayItem::handleMousePress(QMouseEvent* event, int widgetW, int widgetH)
{
    if (event->button() != Qt::RightButton) { return false; }
    const QRect rect = pixelRect(widgetW, widgetH);
    const QRect labelRect(rect.left() + 6, rect.top() + rect.height() * 6 / 10,
                          rect.width() - 12, rect.height() * 4 / 10);
    if (!labelRect.contains(event->position().toPoint())) { return false; }

    const int thirdW = labelRect.width() / 3;
    const int relX = event->position().toPoint().x() - labelRect.left();
    if (relX > 2 * thirdW) { emit bandStackRequested(0); return true; }
    else if (relX > thirdW) { emit filterContextRequested(0); return true; }
    return false;
}

QString VfoDisplayItem::serialize() const
{
    return QStringLiteral("VFO|%1|%2|%3|%4|%5|%6|%7")
        .arg(m_x).arg(m_y).arg(m_w).arg(m_h)
        .arg(m_bindingId).arg(m_zOrder).arg(static_cast<int>(m_displayMode));
}

bool VfoDisplayItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    if (parts.size() < 7 || parts[0] != QLatin1String("VFO")) { return false; }
    m_x = parts[1].toFloat(); m_y = parts[2].toFloat();
    m_w = parts[3].toFloat(); m_h = parts[4].toFloat();
    m_bindingId = parts[5].toInt(); m_zOrder = parts[6].toInt();
    if (parts.size() > 7) { m_displayMode = static_cast<DisplayMode>(parts[7].toInt()); }
    return true;
}

} // namespace NereusSDR
