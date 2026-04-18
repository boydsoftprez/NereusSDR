// =================================================================
// src/gui/meters/ClockItem.cpp  (NereusSDR)
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

#include "ClockItem.h"
#include <QPainter>
#include <QDateTime>

namespace NereusSDR {

ClockItem::ClockItem(QObject* parent) : MeterItem(parent)
{
    m_updateTimer.setInterval(1000);
    m_updateTimer.start();
}

void ClockItem::paint(QPainter& p, int widgetW, int widgetH)
{
    const QRect rect = pixelRect(widgetW, widgetH);
    p.setRenderHint(QPainter::Antialiasing, true);

    const int halfW = rect.width() * 48 / 100;
    const int gap = rect.width() * 4 / 100;
    const QRect localRect(rect.left(), rect.top(), halfW, rect.height());
    const QRect utcRect(rect.left() + halfW + gap, rect.top(), halfW, rect.height());

    const QDateTime now = QDateTime::currentDateTime();
    const QDateTime utcNow = now.toUTC();

    const QString timeFmt = m_show24Hour ? QStringLiteral("HH:mm:ss") : QStringLiteral("hh:mm:ss AP");
    const QString dateFmt = QStringLiteral("ddd d MMM yyyy");

    const int timeSize = qMax(10, rect.height() / 3);
    const int dateSize = qMax(8, rect.height() / 5);
    const int typeSize = qMax(7, rect.height() / 6);

    auto drawClock = [&](const QRect& r, const QDateTime& dt, const QString& typeLabel) {
        int yOff = r.top();
        if (m_showType) {
            QFont typeFont = p.font();
            typeFont.setPixelSize(typeSize);
            p.setFont(typeFont);
            p.setPen(m_typeTitleColour);
            p.drawText(QRect(r.left(), yOff, r.width(), typeSize + 2), Qt::AlignCenter, typeLabel);
            yOff += typeSize + 2;
        }
        QFont timeFont = p.font();
        timeFont.setPixelSize(timeSize);
        timeFont.setBold(true);
        p.setFont(timeFont);
        p.setPen(m_timeColour);
        p.drawText(QRect(r.left(), yOff, r.width(), timeSize + 2), Qt::AlignCenter, dt.toString(timeFmt));
        yOff += timeSize + 2;
        QFont dateFont = p.font();
        dateFont.setPixelSize(dateSize);
        dateFont.setBold(false);
        p.setFont(dateFont);
        p.setPen(m_dateColour);
        p.drawText(QRect(r.left(), yOff, r.width(), dateSize + 2), Qt::AlignCenter, dt.toString(dateFmt));
    };

    drawClock(localRect, now, QStringLiteral("Local"));
    drawClock(utcRect, utcNow, QStringLiteral("UTC"));
}

QString ClockItem::serialize() const
{
    return QStringLiteral("CLOCK|%1|%2|%3|%4|%5|%6|%7|%8")
        .arg(m_x).arg(m_y).arg(m_w).arg(m_h)
        .arg(m_bindingId).arg(m_zOrder)
        .arg(m_show24Hour ? 1 : 0).arg(m_showType ? 1 : 0);
}

bool ClockItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    if (parts.size() < 7 || parts[0] != QLatin1String("CLOCK")) { return false; }
    m_x = parts[1].toFloat(); m_y = parts[2].toFloat();
    m_w = parts[3].toFloat(); m_h = parts[4].toFloat();
    m_bindingId = parts[5].toInt(); m_zOrder = parts[6].toInt();
    if (parts.size() > 7) { m_show24Hour = parts[7].toInt() != 0; }
    if (parts.size() > 8) { m_showType = parts[8].toInt() != 0; }
    return true;
}

} // namespace NereusSDR
