// =================================================================
// src/gui/meters/LEDItem.cpp  (NereusSDR)
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

#include "LEDItem.h"

// From Thetis clsLed (MeterManager.cs:19448+)

#include <QPainter>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QPainterPath>
#include <QStringList>

namespace NereusSDR {

// From Thetis MeterManager.cs:19448 — clsLed constructor
LEDItem::LEDItem(QObject* parent)
    : MeterItem(parent)
{
}

// ---------------------------------------------------------------------------
// setValue()
// From Thetis clsLed — threshold-based LED activation logic.
// If v >= redThreshold  → red mode (always on)
// If v >= amberThreshold → amber mode (always on)
// If v >= greenThreshold → m_ledOn = true (green/true colour)
// Else                   → m_ledOn = false (false/off colour)
// ---------------------------------------------------------------------------
void LEDItem::setValue(double v)
{
    MeterItem::setValue(v);

    if (v >= m_redThreshold) {
        m_ledOn = true;   // color determined by currentColor()
    } else if (v >= m_amberThreshold) {
        m_ledOn = true;
    } else if (v >= m_greenThreshold) {
        m_ledOn = true;
    } else {
        m_ledOn = false;
    }
}

// ---------------------------------------------------------------------------
// currentColor()
// Returns the color appropriate for the current value and state.
// ---------------------------------------------------------------------------
QColor LEDItem::currentColor() const
{
    const double v = value();
    if (v >= m_redThreshold) {
        return m_redColour;
    }
    if (v >= m_amberThreshold) {
        return m_amberColour;
    }
    if (m_ledOn) {
        return m_trueColour;
    }
    return m_falseColour;
}

// ---------------------------------------------------------------------------
// paint()
// From Thetis clsLed renderLed() (MeterManager.cs:19448+)
// Renders the LED shape with optional back panel, blink, and pulsate.
// ---------------------------------------------------------------------------
void LEDItem::paint(QPainter& p, int widgetW, int widgetH)
{
    const QRect fullRect = pixelRect(widgetW, widgetH);

    // Step 1: optional back panel fill
    if (m_showBackPanel) {
        p.fillRect(fullRect, m_panelBack1);
    }

    // Step 2: compute led rect with padding inset
    const int pad = static_cast<int>(m_padding);
    const QRect ledRect = fullRect.adjusted(pad, pad, -pad, -pad);
    if (ledRect.isEmpty()) {
        return;
    }

    // Step 3: blink — toggle m_ledOn every 5 paint calls (~500ms at 100ms rate)
    // From Thetis clsLed blink logic (MeterManager.cs:19448+)
    if (m_blink) {
        ++m_blinkCounter;
        if (m_blinkCounter >= 5) {
            m_blinkCounter = 0;
            m_ledOn = !m_ledOn;
        }
    }

    // Step 4: pulsate — cycle alpha between 0.3 and 1.0
    // From Thetis clsLed pulsate logic (MeterManager.cs:19448+)
    if (m_pulsate) {
        m_pulsateAlpha += m_pulsateDir;
        if (m_pulsateAlpha <= 0.3f) {
            m_pulsateAlpha = 0.3f;
            m_pulsateDir = 0.05f;
        } else if (m_pulsateAlpha >= 1.0f) {
            m_pulsateAlpha = 1.0f;
            m_pulsateDir = -0.05f;
        }
    }

    // Step 5: determine current color, apply pulsate alpha
    QColor color = currentColor();
    if (m_pulsate) {
        color.setAlphaF(m_pulsateAlpha);
    }

    // Step 6: draw shape
    p.setRenderHint(QPainter::Antialiasing, true);

    if (m_shape == LedShape::Round) {
        if (m_style == LedStyle::Flat) {
            // From Thetis clsLed — Round/Flat: solid fill + darker border
            p.setBrush(color);
            p.setPen(QPen(color.darker(150), 1));
            p.drawEllipse(ledRect);
        } else {
            // From Thetis clsLed — Round/ThreeD: radial gradient, lighter center
            QRadialGradient grad(ledRect.center(), ledRect.width() / 2.0);
            grad.setFocalPoint(ledRect.center());
            grad.setColorAt(0.0, color.lighter(150));
            grad.setColorAt(1.0, color);
            p.setBrush(grad);
            p.setPen(QPen(color.darker(150), 1));
            p.drawEllipse(ledRect);
        }
    } else if (m_shape == LedShape::Square) {
        if (m_style == LedStyle::Flat) {
            // From Thetis clsLed — Square/Flat: solid fill + darker border
            p.fillRect(ledRect, color);
            p.setPen(QPen(color.darker(150), 1));
            p.setBrush(Qt::NoBrush);
            p.drawRect(ledRect);
        } else {
            // From Thetis clsLed — Square/ThreeD: linear gradient top-to-bottom
            QLinearGradient grad(ledRect.topLeft(), ledRect.bottomLeft());
            grad.setColorAt(0.0, color.lighter(150));
            grad.setColorAt(1.0, color);
            p.setBrush(grad);
            p.setPen(QPen(color.darker(150), 1));
            p.drawRect(ledRect);
        }
    } else {
        // From Thetis clsLed — Triangle: top-center, bottom-left, bottom-right
        QPainterPath path;
        const QPointF top(ledRect.center().x(), ledRect.top());
        const QPointF botLeft(ledRect.left(), ledRect.bottom());
        const QPointF botRight(ledRect.right(), ledRect.bottom());
        path.moveTo(top);
        path.lineTo(botLeft);
        path.lineTo(botRight);
        path.closeSubpath();
        p.setBrush(color);
        p.setPen(QPen(color.darker(150), 1));
        p.drawPath(path);
    }
}

// ---------------------------------------------------------------------------
// serialize()
// Format: LED|x|y|w|h|bindingId|zOrder|shape|style|trueColour|falseColour|
//             panelBack1|panelBack2|showBackPanel|blink|pulsate|padding|
//             greenThresh|amberThresh|redThresh
// ---------------------------------------------------------------------------

static QString ledBaseFields(const MeterItem& item)
{
    return QStringLiteral("%1|%2|%3|%4|%5|%6")
        .arg(static_cast<double>(item.x()))
        .arg(static_cast<double>(item.y()))
        .arg(static_cast<double>(item.itemWidth()))
        .arg(static_cast<double>(item.itemHeight()))
        .arg(item.bindingId())
        .arg(item.zOrder());
}

static bool ledParseBaseFields(MeterItem& item, const QStringList& parts)
{
    if (parts.size() < 7) {
        return false;
    }
    const QString base = QStringList(parts.mid(1, 6)).join(QLatin1Char('|'));
    return item.MeterItem::deserialize(base);
}

QString LEDItem::serialize() const
{
    const int shape = static_cast<int>(m_shape);   // 0=Square, 1=Round, 2=Triangle
    const int style = static_cast<int>(m_style);   // 0=Flat, 1=ThreeD

    return QStringLiteral("LED|%1|%2|%3|%4|%5|%6|%7|%8|%9|%10|%11|%12|%13|%14|%15|%16|%17")
        .arg(ledBaseFields(*this))
        .arg(shape)
        .arg(style)
        .arg(m_trueColour.name(QColor::HexArgb))
        .arg(m_falseColour.name(QColor::HexArgb))
        .arg(m_panelBack1.name(QColor::HexArgb))
        .arg(m_panelBack2.name(QColor::HexArgb))
        .arg(m_showBackPanel ? 1 : 0)
        .arg(m_blink ? 1 : 0)
        .arg(m_pulsate ? 1 : 0)
        .arg(static_cast<double>(m_padding))
        .arg(m_greenThreshold)
        .arg(m_amberThreshold)
        .arg(m_redThreshold);
}

// ---------------------------------------------------------------------------
// deserialize()
// Expected parts:
// [0]=LED [1-6]=base fields [7]=shape [8]=style [9]=trueColour [10]=falseColour
// [11]=panelBack1 [12]=panelBack2 [13]=showBackPanel [14]=blink [15]=pulsate
// [16]=padding [17]=greenThresh [18]=amberThresh [19]=redThresh
// ---------------------------------------------------------------------------
bool LEDItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    if (parts.size() < 20 || parts[0] != QLatin1String("LED")) {
        return false;
    }
    if (!ledParseBaseFields(*this, parts)) {
        return false;
    }

    bool ok = true;

    const int shape = parts[7].toInt(&ok);
    if (!ok || shape < 0 || shape > 2) {
        return false;
    }

    const int style = parts[8].toInt(&ok);
    if (!ok || style < 0 || style > 1) {
        return false;
    }

    const QColor trueColour(parts[9]);
    if (!trueColour.isValid()) { return false; }
    const QColor falseColour(parts[10]);
    if (!falseColour.isValid()) { return false; }
    const QColor panelBack1(parts[11]);
    if (!panelBack1.isValid()) { return false; }
    const QColor panelBack2(parts[12]);
    if (!panelBack2.isValid()) { return false; }

    const int showBackPanel = parts[13].toInt(&ok);
    if (!ok) { return false; }
    const int blink = parts[14].toInt(&ok);
    if (!ok) { return false; }
    const int pulsate = parts[15].toInt(&ok);
    if (!ok) { return false; }

    const float padding = parts[16].toFloat(&ok);
    if (!ok) { return false; }

    const double greenThresh = parts[17].toDouble(&ok);
    if (!ok) { return false; }
    const double amberThresh = parts[18].toDouble(&ok);
    if (!ok) { return false; }
    const double redThresh = parts[19].toDouble(&ok);
    if (!ok) { return false; }

    m_shape         = static_cast<LedShape>(shape);
    m_style         = static_cast<LedStyle>(style);
    m_trueColour    = trueColour;
    m_falseColour   = falseColour;
    m_panelBack1    = panelBack1;
    m_panelBack2    = panelBack2;
    m_showBackPanel = (showBackPanel != 0);
    m_blink         = (blink != 0);
    m_pulsate       = (pulsate != 0);
    m_padding       = padding;
    m_greenThreshold = greenThresh;
    m_amberThreshold = amberThresh;
    m_redThreshold   = redThresh;

    return true;
}

} // namespace NereusSDR
