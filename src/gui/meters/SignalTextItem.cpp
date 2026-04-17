// =================================================================
// src/gui/meters/SignalTextItem.cpp  (NereusSDR)
// =================================================================
//
// Ported from multiple Thetis sources:
//   Project Files/Source/Console/MeterManager.cs
//   Project Files/Source/Console/console.cs
//
// Original Thetis copyright and license (preserved per GNU GPL,
// representing the union of contributors across all cited sources):
//
//   Thetis is a C# implementation of a Software Defined Radio.
//   Copyright (C) 2004-2009  FlexRadio Systems
//   Copyright (C) 2010-2020  Doug Wigley (W5WC)
//   Copyright (C) 2020-2026  Richard Samphire (MW0LGE) [MeterManager.cs]
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

#include "SignalTextItem.h"

// From Thetis clsSignalText (MeterManager.cs:20286-20540)

#include <QPainter>
#include <QFont>
#include <QFontMetrics>
#include <QLinearGradient>
#include <QStringList>
#include <algorithm>
#include <cmath>

namespace NereusSDR {

// ---------------------------------------------------------------------------
// Constructor
// From Thetis clsSignalText constructor (MeterManager.cs:20300+)
// ---------------------------------------------------------------------------
SignalTextItem::SignalTextItem(QObject* parent)
    : MeterItem(parent)
{
}

// ---------------------------------------------------------------------------
// setValue()
// From Thetis clsSignalText — attack/decay smoothing + peak hold logic.
// (MeterManager.cs:20353-20380)
// ---------------------------------------------------------------------------
void SignalTextItem::setValue(double v)
{
    MeterItem::setValue(v);

    const float fv = static_cast<float>(v);

    // From Thetis line 20353-20354: attack/decay exponential smoothing
    if (fv > m_smoothedDbm) {
        m_smoothedDbm = m_attackRatio * fv + (1.0f - m_attackRatio) * m_smoothedDbm;
    } else {
        m_smoothedDbm = m_decayRatio * fv + (1.0f - m_decayRatio) * m_smoothedDbm;
    }

    // From Thetis — peak hold logic (MeterManager.cs:20360+)
    // Hold for 5 frames (~500ms at 100ms poll), then decay 1dB per frame.
    if (m_smoothedDbm > m_peakDbm) {
        m_peakDbm = m_smoothedDbm;
        m_peakHoldCounter = 5;  // From NeedleItem kPeakHoldFrames = 5
    } else {
        if (m_peakHoldCounter > 0) {
            --m_peakHoldCounter;
        } else {
            m_peakDbm -= 1.0f;  // From NeedleItem kPeakDecayPerFrame = 1.0f
            if (m_peakDbm < -140.0f) {
                m_peakDbm = -140.0f;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// uvFromDbm()
// From Thetis Common.UVfromDBM
// ---------------------------------------------------------------------------
double SignalTextItem::uvFromDbm(double dbm)
{
    return std::pow(10.0, (dbm + 107.0) / 20.0);
}

// ---------------------------------------------------------------------------
// formatDbm()
// From Thetis console.cs — dBm format
// ---------------------------------------------------------------------------
QString SignalTextItem::formatDbm(float dbm) const
{
    return QString::number(static_cast<double>(dbm), 'f', 1) + QStringLiteral(" dBm");
}

// ---------------------------------------------------------------------------
// formatSUnits()
// From Thetis — S-units: 6dB per S-unit, S0=-127dBm, S9=-73dBm
// (MeterManager.cs:20410+)
// ---------------------------------------------------------------------------
QString SignalTextItem::formatSUnits(float dbm) const
{
    if (dbm <= -73.0f) {
        // S0..S9 range
        const int sUnit = std::clamp(static_cast<int>((dbm + 127.0f) / 6.0f), 0, 9);
        return QStringLiteral("S") + QString::number(sUnit);
    } else {
        // Above S9
        const int over = static_cast<int>(dbm + 73.0f);
        return QStringLiteral("S9+") + QString::number(over);
    }
}

// ---------------------------------------------------------------------------
// formatUv()
// From Thetis Common.UVfromDBM usage in clsSignalText
// ---------------------------------------------------------------------------
QString SignalTextItem::formatUv(float dbm) const
{
    const double uv = uvFromDbm(static_cast<double>(dbm));
    return QString::number(uv, 'f', 2) + QStringLiteral(" uV");
}

// ---------------------------------------------------------------------------
// formatValue()
// Dispatches to the appropriate format function based on m_units.
// ---------------------------------------------------------------------------
QString SignalTextItem::formatValue(float dbm) const
{
    switch (m_units) {
        case Units::Dbm:    return formatDbm(dbm);
        case Units::SUnits: return formatSUnits(dbm);
        case Units::Uv:     return formatUv(dbm);
    }
    return formatDbm(dbm);
}

// ---------------------------------------------------------------------------
// paint()
// From Thetis clsSignalText.renderSignalText() (MeterManager.cs:20420+)
// ---------------------------------------------------------------------------
void SignalTextItem::paint(QPainter& p, int widgetW, int widgetH)
{
    const QRect rect = pixelRect(widgetW, widgetH);
    if (rect.isEmpty()) {
        return;
    }

    // Step 1: fill background
    p.fillRect(rect, QColor(0x0a, 0x0a, 0x18));

    // Step 2: compute scaled font size
    // Scale font relative to widget height — larger widget = larger text
    const float scaledSize = m_fontSize * (static_cast<float>(rect.height()) / 100.0f);
    const float clampedSize = std::max(6.0f, scaledSize);

    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    // Step 3: main value text (centered, large)
    if (m_showValue) {
        QFont mainFont(m_fontFamily, static_cast<int>(clampedSize));
        mainFont.setBold(true);
        p.setFont(mainFont);
        p.setPen(m_colour);

        const QString text = formatValue(m_smoothedDbm);
        p.drawText(rect, Qt::AlignCenter, text);
    }

    // Step 4: units label below value in small font
    if (m_showType) {
        QString unitsLabel;
        switch (m_units) {
            case Units::Dbm:    unitsLabel = QStringLiteral("dBm");     break;
            case Units::SUnits: unitsLabel = QStringLiteral("S-Units"); break;
            case Units::Uv:     unitsLabel = QStringLiteral("uV");      break;
        }

        const float smallSize = std::max(6.0f, clampedSize * 0.4f);
        QFont smallFont(m_fontFamily, static_cast<int>(smallSize));
        p.setFont(smallFont);
        p.setPen(QColor(0x80, 0x90, 0xa0));

        // Position in lower portion of rect
        const QRect labelRect(rect.left(), rect.top() + rect.height() * 2 / 3,
                               rect.width(), rect.height() / 3);
        p.drawText(labelRect, Qt::AlignHCenter | Qt::AlignBottom, unitsLabel);
    }

    // Step 5: peak value in top-right, smaller font
    if (m_showPeakValue && m_peakHold) {
        const float peakSize = std::max(6.0f, clampedSize * 0.5f);
        QFont peakFont(m_fontFamily, static_cast<int>(peakSize));
        peakFont.setBold(true);
        p.setFont(peakFont);
        p.setPen(m_peakColour);

        const QString peakText = formatValue(m_peakDbm);
        const QRect peakRect(rect.left(), rect.top(),
                              rect.width(), rect.height() / 3);
        p.drawText(peakRect, Qt::AlignRight | Qt::AlignTop, peakText);
    }

    // Step 6: bar beneath text (if style != None)
    if (m_barStyle != BarStyle::None) {
        // Bar occupies the bottom 8px of the rect
        const int barH = std::max(4, rect.height() / 12);
        const QRect barRect(rect.left(), rect.bottom() - barH,
                            rect.width(), barH);

        // Fraction: map -140..0 dBm to 0..1
        const float fraction = std::clamp((m_smoothedDbm + 140.0f) / 140.0f, 0.0f, 1.0f);
        const int fillW = static_cast<int>(fraction * barRect.width());

        p.setPen(Qt::NoPen);

        switch (m_barStyle) {
            case BarStyle::None:
                break;

            case BarStyle::SolidFilled: {
                const QRect fillRect(barRect.left(), barRect.top(), fillW, barRect.height());
                p.fillRect(fillRect, m_colour);
                break;
            }

            case BarStyle::Line: {
                const int lineX = barRect.left() + fillW;
                p.setPen(QPen(m_markerColour, 2));
                p.drawLine(lineX, barRect.top(), lineX, barRect.bottom());
                break;
            }

            case BarStyle::GradientFilled: {
                QLinearGradient grad(barRect.left(), 0, barRect.right(), 0);
                grad.setColorAt(0.0, QColor(0x00, 0xff, 0xff));  // cyan
                grad.setColorAt(1.0, QColor(0xff, 0x00, 0x00));  // red
                p.setBrush(grad);
                const QRect fillRect(barRect.left(), barRect.top(), fillW, barRect.height());
                p.drawRect(fillRect);
                break;
            }

            case BarStyle::Segments: {
                // From Thetis clsSignalText — 20 discrete segments
                constexpr int kSegments = 20;
                const int segW = barRect.width() / kSegments;
                const int filledSegs = static_cast<int>(fraction * kSegments);
                for (int i = 0; i < kSegments; ++i) {
                    const int sx = barRect.left() + i * segW;
                    const QRect segRect(sx + 1, barRect.top(), segW - 2, barRect.height());
                    if (i < filledSegs) {
                        // Filled segment — interpolate color cyan→red
                        const float t = static_cast<float>(i) / (kSegments - 1);
                        const int r = static_cast<int>(0xff * t);
                        const int g = static_cast<int>(0xff * (1.0f - t));
                        p.fillRect(segRect, QColor(r, g, 0xff - r));
                    } else {
                        p.fillRect(segRect, QColor(0x20, 0x20, 0x30));
                    }
                }
                break;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// serialize()
// Format: SIGNALTEXT|x|y|w|h|bindingId|zOrder|units|showValue|showPeakValue|
//                    showType|peakHold|colour|peakColour|markerColour|
//                    fontFamily|fontSize|barStyle
// ---------------------------------------------------------------------------
QString SignalTextItem::serialize() const
{
    return QStringLiteral("SIGNALTEXT|%1|%2|%3|%4|%5|%6|%7|%8|%9|%10|%11|%12|%13|%14|%15")
        .arg(static_cast<double>(x()))
        .arg(static_cast<double>(y()))
        .arg(static_cast<double>(itemWidth()))
        .arg(static_cast<double>(itemHeight()))
        .arg(bindingId())
        .arg(zOrder())
        .arg(static_cast<int>(m_units))
        .arg(m_showValue ? 1 : 0)
        .arg(m_showPeakValue ? 1 : 0)
        .arg(m_showType ? 1 : 0)
        .arg(m_peakHold ? 1 : 0)
        .arg(m_colour.name(QColor::HexArgb))
        .arg(m_peakColour.name(QColor::HexArgb))
        .arg(m_markerColour.name(QColor::HexArgb))
        .arg(m_fontFamily)
        + QStringLiteral("|%1|%2")
        .arg(static_cast<double>(m_fontSize))
        .arg(static_cast<int>(m_barStyle));
}

// ---------------------------------------------------------------------------
// deserialize()
// Expected parts:
// [0]=SIGNALTEXT [1-6]=base fields [7]=units [8]=showValue [9]=showPeakValue
// [10]=showType [11]=peakHold [12]=colour [13]=peakColour [14]=markerColour
// [15]=fontFamily [16]=fontSize [17]=barStyle
// ---------------------------------------------------------------------------
bool SignalTextItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    if (parts.size() < 18 || parts[0] != QLatin1String("SIGNALTEXT")) {
        return false;
    }

    // Parse base fields (indices 1..6 -> x, y, w, h, bindingId, zOrder)
    const QString base = QStringList(parts.mid(1, 6)).join(QLatin1Char('|'));
    if (!MeterItem::deserialize(base)) {
        return false;
    }

    bool ok = true;

    const int units = parts[7].toInt(&ok);
    if (!ok || units < 0 || units > 2) { return false; }

    const int showValue = parts[8].toInt(&ok);
    if (!ok) { return false; }
    const int showPeakValue = parts[9].toInt(&ok);
    if (!ok) { return false; }
    const int showType = parts[10].toInt(&ok);
    if (!ok) { return false; }
    const int peakHold = parts[11].toInt(&ok);
    if (!ok) { return false; }

    const QColor colour(parts[12]);
    if (!colour.isValid()) { return false; }
    const QColor peakColour(parts[13]);
    if (!peakColour.isValid()) { return false; }
    const QColor markerColour(parts[14]);
    if (!markerColour.isValid()) { return false; }

    const QString fontFamily = parts[15];

    const float fontSize = parts[16].toFloat(&ok);
    if (!ok || fontSize <= 0.0f) { return false; }

    const int barStyle = parts[17].toInt(&ok);
    if (!ok || barStyle < 0 || barStyle > 4) { return false; }

    m_units         = static_cast<Units>(units);
    m_showValue     = (showValue != 0);
    m_showPeakValue = (showPeakValue != 0);
    m_showType      = (showType != 0);
    m_peakHold      = (peakHold != 0);
    m_colour        = colour;
    m_peakColour    = peakColour;
    m_markerColour  = markerColour;
    m_fontFamily    = fontFamily;
    m_fontSize      = fontSize;
    m_barStyle      = static_cast<BarStyle>(barStyle);

    return true;
}

} // namespace NereusSDR
