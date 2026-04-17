// =================================================================
// src/gui/meters/TextOverlayItem.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs
//
// Original Thetis copyright and license (preserved per GNU GPL):
//
//   Thetis is a C# implementation of a Software Defined Radio.
//   Copyright (C) 2004-2009  FlexRadio Systems
//   Copyright (C) 2010-2020  Doug Wigley (W5WC)
//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified
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
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code. Structural template follows AetherSDR
//                 (ten9876/AetherSDR) Qt6 conventions.
// =================================================================

#include "TextOverlayItem.h"

// From Thetis clsTextOverlay (MeterManager.cs:18746+)

#include <QPainter>
#include <QFont>
#include <QFontMetrics>
#include <QStringList>
#include <algorithm>

namespace NereusSDR {

// ---------------------------------------------------------------------------
// resolveText()
// From Thetis parseText() (MeterManager.cs:19267-19395)
// Splits template on '%', alternating literal/token segments.
// ---------------------------------------------------------------------------
QString TextOverlayItem::resolveText(const QString& templateText) const
{
    if (templateText.isEmpty()) {
        return {};
    }

    const QStringList parts = templateText.split(QLatin1Char('%'));
    QString result;
    int precision = m_precision;

    for (int i = 0; i < parts.size(); ++i) {
        if (i % 2 == 0) {
            // Literal text — pass through unchanged
            result += parts[i];
        } else {
            // Token — case-insensitive match
            const QString token = parts[i].toUpper();

            if (token == QLatin1String("NL")) {
                // From Thetis parseText() — newline token
                result += QLatin1Char('\n');
            } else if (token.startsWith(QLatin1String("PRECIS="))) {
                // From Thetis parseText() — set decimal precision, no output
                bool ok = false;
                int p = token.mid(7).toInt(&ok);
                if (ok) {
                    precision = std::clamp(p, 0, 20);
                    m_precision = precision;
                }
            } else if (token == QLatin1String("VALUE")) {
                // Base meter value with current precision
                result += QString::number(m_value, 'f', precision);
            } else if (m_variables.contains(token)) {
                // Named variable from external registry
                result += m_variables[token];
            } else {
                // Unknown token — pass through verbatim
                result += QLatin1Char('%') + parts[i] + QLatin1Char('%');
            }
        }
    }

    return result;
}

// ---------------------------------------------------------------------------
// paint()
// From Thetis clsTextOverlay.render() (MeterManager.cs:18800+)
// Two-line text display: top half = line 1, bottom half = line 2.
// ---------------------------------------------------------------------------
void TextOverlayItem::paint(QPainter& p, int widgetW, int widgetH)
{
    const QRect rect = pixelRect(widgetW, widgetH);
    if (rect.isEmpty()) {
        return;
    }

    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    // Step 1: back panel fill — upper/lower halves may differ in color
    if (m_showBackPanel) {
        const QRect topHalf(rect.left(), rect.top(),
                            rect.width(), rect.height() / 2);
        const QRect bottomHalf(rect.left(), rect.top() + rect.height() / 2,
                               rect.width(), rect.height() - rect.height() / 2);
        p.fillRect(topHalf, m_panelBack1);
        p.fillRect(bottomHalf, m_panelBack2);
    }

    // Step 2: resolve substituted text for both lines
    const QString resolved1 = resolveText(m_text1);
    const QString resolved2 = resolveText(m_text2);

    // Step 3: layout rects
    const int pad = static_cast<int>(m_padding * static_cast<float>(rect.height()));
    const QRect lineRect1(rect.left() + pad,
                          rect.top() + pad,
                          rect.width() - 2 * pad,
                          rect.height() / 2 - 2 * pad);
    const QRect lineRect2(rect.left() + pad,
                          rect.top() + rect.height() / 2 + pad,
                          rect.width() - 2 * pad,
                          rect.height() - rect.height() / 2 - 2 * pad);

    // Step 4: draw line 1
    if (!resolved1.isEmpty()) {
        QFont font1(m_fontFamily1, static_cast<int>(m_fontSize1));
        font1.setBold(m_fontBold1);
        p.setFont(font1);

        if (m_showTextBack1 && lineRect1.isValid()) {
            const QFontMetrics fm1(font1);
            const QRect textBound = fm1.boundingRect(lineRect1,
                                                      Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap,
                                                      resolved1);
            p.fillRect(textBound, m_textBackColour1);
        }

        p.setPen(m_textColour1);

        if (m_scrollX != 0.0f && lineRect1.isValid()) {
            // Scrolling: clip to lineRect1, draw at scrolled x position
            const QFontMetrics fm1(font1);
            const int textW = fm1.horizontalAdvance(resolved1);
            p.save();
            p.setClipRect(lineRect1);

            const int drawX = lineRect1.left() + static_cast<int>(m_scrollOffset1);
            const QRect scrolledRect(drawX, lineRect1.top(),
                                     textW + lineRect1.width(),
                                     lineRect1.height());
            p.drawText(scrolledRect, Qt::AlignLeft | Qt::AlignVCenter, resolved1);
            p.restore();

            // Advance scroll offset, wrap when text has scrolled fully off left
            m_scrollOffset1 += m_scrollX;
            if (m_scrollOffset1 < -static_cast<float>(textW)) {
                m_scrollOffset1 = static_cast<float>(lineRect1.width());
            }
        } else {
            p.drawText(lineRect1, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap, resolved1);
        }
    }

    // Step 5: draw line 2
    if (!resolved2.isEmpty()) {
        QFont font2(m_fontFamily2, static_cast<int>(m_fontSize2));
        font2.setBold(m_fontBold2);
        p.setFont(font2);

        if (m_showTextBack2 && lineRect2.isValid()) {
            const QFontMetrics fm2(font2);
            const QRect textBound = fm2.boundingRect(lineRect2,
                                                      Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap,
                                                      resolved2);
            p.fillRect(textBound, m_textBackColour2);
        }

        p.setPen(m_textColour2);

        if (m_scrollX != 0.0f && lineRect2.isValid()) {
            const QFontMetrics fm2(font2);
            const int textW = fm2.horizontalAdvance(resolved2);
            p.save();
            p.setClipRect(lineRect2);

            const int drawX = lineRect2.left() + static_cast<int>(m_scrollOffset2);
            const QRect scrolledRect(drawX, lineRect2.top(),
                                     textW + lineRect2.width(),
                                     lineRect2.height());
            p.drawText(scrolledRect, Qt::AlignLeft | Qt::AlignVCenter, resolved2);
            p.restore();

            m_scrollOffset2 += m_scrollX;
            if (m_scrollOffset2 < -static_cast<float>(textW)) {
                m_scrollOffset2 = static_cast<float>(lineRect2.width());
            }
        } else {
            p.drawText(lineRect2, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap, resolved2);
        }
    }
}

// ---------------------------------------------------------------------------
// serialize()
// Format: TEXTOVERLAY|x|y|w|h|bindingId|zOrder|
//         textColour1|textColour2|panelBack1|panelBack2|
//         showBackPanel|
//         fontFamily1|fontSize1|fontBold1|
//         fontFamily2|fontSize2|fontBold2|
//         scrollX|padding|
//         <base64(text1)>|<base64(text2)>
//
// text1/text2 are base64-encoded to avoid '|' collisions.
// ---------------------------------------------------------------------------
QString TextOverlayItem::serialize() const
{
    const QString text1B64 = QString::fromLatin1(m_text1.toUtf8().toBase64());
    const QString text2B64 = QString::fromLatin1(m_text2.toUtf8().toBase64());

    return QStringLiteral("TEXTOVERLAY|%1|%2|%3|%4|%5|%6"
                          "|%7|%8|%9|%10|%11"
                          "|%12|%13|%14"
                          "|%15|%16|%17"
                          "|%18|%19"
                          "|%20|%21")
        .arg(static_cast<double>(x()))
        .arg(static_cast<double>(y()))
        .arg(static_cast<double>(itemWidth()))
        .arg(static_cast<double>(itemHeight()))
        .arg(bindingId())
        .arg(zOrder())
        .arg(m_textColour1.name(QColor::HexArgb))
        .arg(m_textColour2.name(QColor::HexArgb))
        .arg(m_panelBack1.name(QColor::HexArgb))
        .arg(m_panelBack2.name(QColor::HexArgb))
        .arg(m_showBackPanel ? 1 : 0)
        .arg(m_fontFamily1)
        .arg(static_cast<double>(m_fontSize1))
        .arg(m_fontBold1 ? 1 : 0)
        .arg(m_fontFamily2)
        .arg(static_cast<double>(m_fontSize2))
        .arg(m_fontBold2 ? 1 : 0)
        .arg(static_cast<double>(m_scrollX))
        .arg(static_cast<double>(m_padding))
        .arg(text1B64)
        .arg(text2B64);
}

// ---------------------------------------------------------------------------
// deserialize()
// Expected field indices:
// [0]  = TEXTOVERLAY
// [1]  = x      [2] = y       [3] = w        [4] = h
// [5]  = bindingId            [6] = zOrder
// [7]  = textColour1          [8] = textColour2
// [9]  = panelBack1           [10] = panelBack2
// [11] = showBackPanel
// [12] = fontFamily1          [13] = fontSize1   [14] = fontBold1
// [15] = fontFamily2          [16] = fontSize2   [17] = fontBold2
// [18] = scrollX              [19] = padding
// [20] = text1 (base64)       [21] = text2 (base64)
// ---------------------------------------------------------------------------
bool TextOverlayItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    if (parts.size() < 22 || parts[0] != QLatin1String("TEXTOVERLAY")) {
        return false;
    }

    // Parse base MeterItem fields (x, y, w, h, bindingId, zOrder)
    const QString base = QStringList(parts.mid(1, 6)).join(QLatin1Char('|'));
    if (!MeterItem::deserialize(base)) {
        return false;
    }

    bool ok = true;

    const QColor textColour1(parts[7]);
    if (!textColour1.isValid()) { return false; }
    const QColor textColour2(parts[8]);
    if (!textColour2.isValid()) { return false; }
    const QColor panelBack1(parts[9]);
    if (!panelBack1.isValid()) { return false; }
    const QColor panelBack2(parts[10]);
    if (!panelBack2.isValid()) { return false; }

    const int showBackPanel = parts[11].toInt(&ok);
    if (!ok) { return false; }

    const QString fontFamily1 = parts[12];
    const float fontSize1 = parts[13].toFloat(&ok);
    if (!ok || fontSize1 <= 0.0f) { return false; }
    const int fontBold1 = parts[14].toInt(&ok);
    if (!ok) { return false; }

    const QString fontFamily2 = parts[15];
    const float fontSize2 = parts[16].toFloat(&ok);
    if (!ok || fontSize2 <= 0.0f) { return false; }
    const int fontBold2 = parts[17].toInt(&ok);
    if (!ok) { return false; }

    const float scrollX = parts[18].toFloat(&ok);
    if (!ok) { return false; }
    const float padding = parts[19].toFloat(&ok);
    if (!ok) { return false; }

    const QString text1 = QString::fromUtf8(
        QByteArray::fromBase64(parts[20].toLatin1()));
    const QString text2 = QString::fromUtf8(
        QByteArray::fromBase64(parts[21].toLatin1()));

    // Commit parsed values
    m_textColour1   = textColour1;
    m_textColour2   = textColour2;
    m_panelBack1    = panelBack1;
    m_panelBack2    = panelBack2;
    m_showBackPanel = (showBackPanel != 0);
    m_fontFamily1   = fontFamily1;
    m_fontSize1     = fontSize1;
    m_fontBold1     = (fontBold1 != 0);
    m_fontFamily2   = fontFamily2;
    m_fontSize2     = fontSize2;
    m_fontBold2     = (fontBold2 != 0);
    m_scrollX       = scrollX;
    m_scrollOffset1 = 0.0f;
    m_scrollOffset2 = 0.0f;
    m_padding       = padding;
    m_text1         = text1;
    m_text2         = text2;

    return true;
}

} // namespace NereusSDR
