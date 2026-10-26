// =================================================================
// src/gui/meters/SpacerItem.cpp  (NereusSDR)
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
//   2026-10-26 - RX/TX colour mode ported from Thetis renderSpacer
//                 (MeterManager.cs:36544-36576 [v2.10.3.15]) with the
//                 transmit-flip fade (ContentFade::nextValue); "padding sets
//                 height" (MeterManager.cs:27627); both appended to the
//                 record. Corrected the paint comment: the gradient is a
//                 NereusSDR look, not Thetis RX/TX. J.J. Boyd (KG4VCF),
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

#include "SpacerItem.h"
#include "gui/containers/ContentFade.h"

// From Thetis MeterManager.cs:16116 — clsSpacerItem / renderSpacer (line 35010)

#include <QPainter>
#include <QLinearGradient>
#include <QStringList>

namespace NereusSDR {

// From Thetis MeterManager.cs:16121
SpacerItem::SpacerItem(QObject* parent)
    : MeterItem(parent)
{
}

void SpacerItem::applyNewItemDefaults()
{
    MeterItem::applyNewItemDefaults();
    const SpacerItemThetisDefaults& d = kSpacerThetisDefaults;
    m_rxTxColourMode    = d.rxTxColourMode;
    m_paddingSetsHeight = d.paddingSetsHeight;
    m_colour1           = QColor::fromRgb(d.colour1);
    m_colour2           = QColor::fromRgb(d.colour2);
    m_padding           = d.padding;
}

// From Thetis MeterManager.cs:27627 [v2.10.3.15]:
//   spacer.Size = new SizeF(ig.Size.Width, spacer.Padding);
// The height is the padding as a ratio of the width.
QRect SpacerItem::spacerRect(int widgetW, int widgetH) const
{
    QRect rect = pixelRect(widgetW, widgetH);
    if (m_paddingSetsHeight) {
        rect.setHeight(qRound(static_cast<double>(rect.width()) * static_cast<double>(m_padding)));
    }
    return rect;
}

// From Thetis MeterManager.cs:36553 [v2.10.3.15]
bool SpacerItem::coverFadeActive() const
{
    const bool tx = effectiveTransmitting();
    return (fadeOnRx() && !tx) || (fadeOnTx() && tx);
}

// From Thetis MeterManager.cs:36553-36560 [v2.10.3.15]
bool SpacerItem::checkFlip()
{
    if (coverFadeActive() || m_colour1 == m_colour2) {
        return false;
    }
    const bool tx = effectiveTransmitting();
    if (tx == m_moxFadeToggle) {
        return false;
    }
    m_flipFadeValue = ContentFade::kDimmedLevel;
    m_moxFadeToggle = tx;
    // The ramp counts from the next advanceMeter call.
    m_lastFlipStepMs = -1;
    m_flipRepaintPending = true;
    return true;
}

void SpacerItem::setTransmitting(bool tx)
{
    MeterItem::setTransmitting(tx);
    if (m_rxTxColourMode) {
        checkFlip();
    }
}

// From Thetis MeterManager.cs:36561-36571 [v2.10.3.15]: the flip fade
// climbs from 48 to 255 in half a second (ContentFade::nextValue with no
// fade flag set takes the same step). Thetis steps once per frame at the
// container's quickest interval; NereusSDR steps on the item's interval.
bool SpacerItem::advanceMeter(qint64 monotonicMs)
{
    if (!m_rxTxColourMode) {
        return false;
    }
    checkFlip();
    bool changed = m_flipRepaintPending;
    m_flipRepaintPending = false;
    if (coverFadeActive() || m_colour1 == m_colour2
        || m_flipFadeValue >= ContentFade::kUndimmedLevel
        || m_lastFlipStepMs < 0 || monotonicMs < m_lastFlipStepMs) {
        m_lastFlipStepMs = monotonicMs;
        return changed;
    }
    const int interval = fadeIntervalMs();
    while (monotonicMs - m_lastFlipStepMs >= interval
           && m_flipFadeValue < ContentFade::kUndimmedLevel) {
        m_flipFadeValue = ContentFade::nextValue(m_flipFadeValue, false, false,
                                                 effectiveTransmitting(), interval);
        m_lastFlipStepMs += interval;
        changed = true;
    }
    return changed;
}

// From Thetis MeterManager.cs:36575 [v2.10.3.15]:
//   getDXBrushForColour(m.MOX ? spacer.Colour2 : spacer.Colour1, mi.FadeValue)
// getDXBrushForColour (:32306-32314) replaces the colour's alpha with the
// fade value, except for a transparent colour. While the fade on RX / TX
// veil is active, MeterWidget's veil dims the item, so the colour shows
// at full alpha here.
QColor SpacerItem::rxTxColour() const
{
    QColor c = effectiveTransmitting() ? m_colour2 : m_colour1;
    if (c.alpha() == 0) {
        return c;
    }
    const bool flipFade = !coverFadeActive() && m_colour1 != m_colour2;
    c.setAlpha(flipFade ? m_flipFadeValue : ContentFade::kUndimmedLevel);
    return c;
}

// ---------------------------------------------------------------------------
// paint()
// RX/TX colour mode: from Thetis MeterManager.cs:36544-36576 [v2.10.3.15],
// renderSpacer(): colour 1 (RX) while receiving, colour 2 (TX) while
// transmitting, fading in from 48 over half a second on each MOX flip.
// Upstream inline attribution preserved verbatim:
//   :35007  //[2.10.3.5]MW0LGE note these are reverse RGB, we normally expect BGRA #289
// Gradient (RX/TX colour mode off): a NereusSDR look, not a Thetis one. If
// the colours differ, a vertical linear gradient (colour 1 top, colour 2
// bottom); otherwise a solid fill with colour 1. Records saved before the
// RX/TX colour mode existed keep it.
// Padding sets height: the spacer is width x padding tall (Thetis
// MeterManager.cs:27627); off, the spacer fills its rectangle and padding
// is kept only in the record.
// ---------------------------------------------------------------------------
void SpacerItem::paint(QPainter& p, int widgetW, int widgetH)
{
    const QRect rect = spacerRect(widgetW, widgetH);
    if (m_rxTxColourMode) {
        p.fillRect(rect, rxTxColour());
        return;
    }
    if (m_colour1 == m_colour2) {
        p.fillRect(rect, m_colour1);
    } else {
        QLinearGradient grad(rect.topLeft(), rect.bottomLeft());
        grad.setColorAt(0.0, m_colour1);
        grad.setColorAt(1.0, m_colour2);
        p.fillRect(rect, grad);
    }
}

// ---------------------------------------------------------------------------
// serialize()
// Format: SPACER|x|y|w|h|bindingId|zOrder|padding|colour1|colour2
//         |rxTxColourMode|paddingSetsHeight   (appended, 0 or 1)
// ---------------------------------------------------------------------------

// Local helper mirroring the static baseFields() in MeterItem.cpp
static QString spacerBaseFields(const MeterItem& item)
{
    return QStringLiteral("%1|%2|%3|%4|%5|%6")
        .arg(static_cast<double>(item.x()))
        .arg(static_cast<double>(item.y()))
        .arg(static_cast<double>(item.itemWidth()))
        .arg(static_cast<double>(item.itemHeight()))
        .arg(item.bindingId())
        .arg(item.zOrder());
}

// Local helper mirroring the static parseBaseFields() in MeterItem.cpp
static bool spacerParseBaseFields(MeterItem& item, const QStringList& parts)
{
    if (parts.size() < 7) {
        return false;
    }
    const QString base = QStringList(parts.mid(1, 6)).join(QLatin1Char('|'));
    return item.MeterItem::deserialize(base);
}

QString SpacerItem::serialize() const
{
    return QStringLiteral("SPACER|%1|%2|%3|%4|%5|%6")
        .arg(spacerBaseFields(*this))
        .arg(static_cast<double>(m_padding))
        .arg(m_colour1.name(QColor::HexArgb))
        .arg(m_colour2.name(QColor::HexArgb))
        .arg(m_rxTxColourMode ? 1 : 0)
        .arg(m_paddingSetsHeight ? 1 : 0);
}

// ---------------------------------------------------------------------------
// deserialize()
// Expected: SPACER|x|y|w|h|bindingId|zOrder|padding|colour1|colour2
//           [0]    [1-6]             [7]     [8]     [9]
//           |rxTxColourMode|paddingSetsHeight
//            [10]          [11]            absent = 0 (gradient, fill)
// ---------------------------------------------------------------------------
bool SpacerItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    if (parts.size() < 10 || parts[0] != QLatin1String("SPACER")) {
        return false;
    }
    if (!spacerParseBaseFields(*this, parts)) {
        return false;
    }

    bool ok = true;
    const float padding = parts[7].toFloat(&ok);
    if (!ok) {
        return false;
    }

    const QColor colour1(parts[8]);
    if (!colour1.isValid()) {
        return false;
    }

    const QColor colour2(parts[9]);
    if (!colour2.isValid()) {
        return false;
    }

    m_padding = padding;
    m_colour1 = colour1;
    m_colour2 = colour2;
    m_rxTxColourMode    = parts.size() > 10 && parts[10] == QLatin1String("1");
    m_paddingSetsHeight = parts.size() > 11 && parts[11] == QLatin1String("1");
    return true;
}

} // namespace NereusSDR
