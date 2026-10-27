// =================================================================
// src/gui/meters/FadeCoverItem.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26 - Colours at their defaults follow the workspace look when
//                 drawn (ItemColourLook.h); saved colours are unchanged.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - The 500 ms fade ramp (MeterItem's fade, Thetis fade())
//                 in place of the instant on/off; the cover inflated by
//                 max(2 px, 1%) as renderFadeCover does; the "use container
//                 background" option (record field 11). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-25 - setTxState keeps the state; updateActive applies it when
//                 the Fade on RX / TX flags change or load. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
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

#include "FadeCoverItem.h"
#include "ItemColourLook.h"
#include "gui/containers/ContentFade.h"

// From Thetis MeterManager.cs:7665 — clsFadeCover / renderFadeCover (line 36292)

#include <QPainter>
#include <QLinearGradient>
#include <QStringList>
#include <QWidget>

#include <algorithm>

namespace NereusSDR {

// From Thetis MeterManager.cs:7667 — clsFadeCover constructor
// ZOrder = int.MaxValue (always rendered on top of all other items)
FadeCoverItem::FadeCoverItem(QObject* parent)
    : MeterItem(parent)
{
    // From Thetis MeterManager.cs:7671 — ZOrder = int.MaxValue
    setZOrder(std::numeric_limits<int>::max());
}

// ---------------------------------------------------------------------------
// Transmit state
// From Thetis MeterManager.cs:7887-7888 — FadeOnRx/FadeOnTx guard logic.
// Thetis: items skip rendering when "if (FadeOnRx && !MOX) return;"
//         This means FadeOnRx items are visible during TX (MOX=true),
//         and FadeOnTx items are visible during RX (MOX=false).
// NereusSDR: setTxState is MeterItem::setTransmitting. The cover dims in
// over 500 ms (MeterItem::advanceFade, Thetis fade() MeterManager.cs:
// 33153-33177 [v2.10.3.15]):
//   - FadeOnRx: shown when receiving
//   - FadeOnTx: shown when transmitting
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// paint()
// From Thetis MeterManager.cs:36292 — renderFadeCover()
// Thetis fills the rect with BackgroundColour at nFade alpha.
// NereusSDR: fills pixelRect with colour1 (or gradient to colour2) at m_alpha.
// ---------------------------------------------------------------------------
void FadeCoverItem::paint(QPainter& p, int widgetW, int widgetH)
{
    //clsFadeCover fc = (clsFadeCover)mi;

    // From Thetis MeterManager.cs:37835 [v2.10.3.15]
    const int nFade = ContentFade::kUndimmedLevel - fadeValue();
    if (nFade <= 0) {
        return;
    }

    // From Thetis MeterManager.cs:37837-37843 [v2.10.3.15]
    // increase size slightly as indictator lines stroke width of 3 will be outside bounds
    const QRectF rect = ContentFade::coverRect(QRectF(pixelRect(widgetW, widgetH)));

    if (m_useContainerBackground) {
        // From Thetis MeterManager.cs:37844 [v2.10.3.15]: the container's
        // background colour at alpha nFade.
        const QWidget* host = nullptr;
        for (QObject* o = parent(); o != nullptr && host == nullptr; o = o->parent()) {
            host = qobject_cast<const QWidget*>(o);
        }
        QColor fill = ContentFade::coverColour(host);
        fill.setAlpha(nFade);
        p.fillRect(rect, fill);
        //_renderTarget.DrawRectangle(rectFC, getDXBrushForColour(System.Drawing.Color.YellowGreen, nFade));
        return;
    }

    // NereusSDR: the colour 1 / colour 2 gradient at m_alpha when fully
    // dimmed, ramping with the fade.
    const int dimRange = ContentFade::kUndimmedLevel - ContentFade::kDimmedLevel;
    const int alphaInt = static_cast<int>(m_alpha * 255.0f) * std::min(nFade, dimRange) / dimRange;

    if (m_colour1 == m_colour2) {
        QColor fill = ItemColourLook::effective("FadeCover.colour1", m_colour1, this);
        fill.setAlpha(alphaInt);
        p.fillRect(rect, fill);
    } else {
        QColor c1 = ItemColourLook::effective("FadeCover.colour1", m_colour1, this);
        QColor c2 = ItemColourLook::effective("FadeCover.colour2", m_colour2, this);
        c1.setAlpha(alphaInt);
        c2.setAlpha(alphaInt);
        QLinearGradient grad(rect.topLeft(), rect.bottomLeft());
        grad.setColorAt(0.0, c1);
        grad.setColorAt(1.0, c2);
        p.fillRect(rect, grad);
    }
}

// ---------------------------------------------------------------------------
// serialize()
// Format: FADECOVER|x|y|w|h|bindingId|zOrder|colour1|colour2|alpha|flags
//   flags = (fadeOnRx ? 2 : 0) | (fadeOnTx ? 1 : 0)
// ---------------------------------------------------------------------------

static QString fadeCoverBaseFields(const MeterItem& item)
{
    return QStringLiteral("%1|%2|%3|%4|%5|%6")
        .arg(static_cast<double>(item.x()))
        .arg(static_cast<double>(item.y()))
        .arg(static_cast<double>(item.itemWidth()))
        .arg(static_cast<double>(item.itemHeight()))
        .arg(item.bindingId())
        .arg(item.zOrder());
}

static bool fadeCoverParseBaseFields(MeterItem& item, const QStringList& parts)
{
    if (parts.size() < 7) {
        return false;
    }
    const QString base = QStringList(parts.mid(1, 6)).join(QLatin1Char('|'));
    return item.MeterItem::deserialize(base);
}

QString FadeCoverItem::serialize() const
{
    const int flags = (m_fadeOnRx ? 2 : 0) | (m_fadeOnTx ? 1 : 0);
    // Field 11 (NereusSDR): use the container background. A record saved
    // without it reads as off.
    return QStringLiteral("FADECOVER|%1|%2|%3|%4|%5|%6")
        .arg(fadeCoverBaseFields(*this))
        .arg(m_colour1.name(QColor::HexArgb))
        .arg(m_colour2.name(QColor::HexArgb))
        .arg(static_cast<double>(m_alpha))
        .arg(flags)
        .arg(m_useContainerBackground ? 1 : 0);
}

// ---------------------------------------------------------------------------
// deserialize()
// Expected: FADECOVER|x|y|w|h|bindingId|zOrder|colour1|colour2|alpha|flags
//           [0]       [1-6]             [7]     [8]     [9]    [10]
// ---------------------------------------------------------------------------
bool FadeCoverItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    if (parts.size() < 11 || parts[0] != QLatin1String("FADECOVER")) {
        return false;
    }
    if (!fadeCoverParseBaseFields(*this, parts)) {
        return false;
    }

    const QColor colour1(parts[7]);
    if (!colour1.isValid()) {
        return false;
    }

    const QColor colour2(parts[8]);
    if (!colour2.isValid()) {
        return false;
    }

    bool ok = true;
    const float alpha = parts[9].toFloat(&ok);
    if (!ok) {
        return false;
    }

    const int flags = parts[10].toInt(&ok);
    if (!ok) {
        return false;
    }

    m_colour1   = colour1;
    m_colour2   = colour2;
    m_alpha     = alpha;
    m_fadeOnRx  = (flags & 2) != 0;
    m_fadeOnTx  = (flags & 1) != 0;
    // Field 11 (NereusSDR): use the container background; absent is off.
    m_useContainerBackground = parts.size() > 11 && parts[11] == QLatin1String("1");
    return true;
}

} // namespace NereusSDR
