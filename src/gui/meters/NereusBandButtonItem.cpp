// =================================================================
// src/gui/meters/NereusBandButtonItem.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//
// Duplicate of src/gui/meters/BandButtonItem, the NereusSDR port of Thetis
// clsBandButtonBox. The band set, clicks, and band-stack request are that
// port. The button face and the choosable text and background colours are
// NereusSDR-original.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-22 - Nereus-styled duplicate of BandButtonItem. Brian Sutton,
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

#include "NereusBandButtonItem.h"

#include "ResponsiveText.h"
#include "gui/StyleConstants.h"
#include "gui/styles/WorkspacePalette.h"
#include "gui/styles/WorkspacePreview.h"
#include "gui/styles/WorkspaceStyle.h"

#include <QLinearGradient>

#include <utility>

namespace NereusSDR {
namespace {

QColor fromHex(const char* hex)
{
    return QColor(QString::fromLatin1(hex));
}

QColor shifted(const QColor& color, int delta)
{
    return QColor(Workspace::shiftL(color, delta));
}

// Same radius rule as the PureSignal button: round(height * 7 / 26).
qreal pureSignalRadius(const QRectF& rect)
{
    const qreal scaled = qRound(rect.height() * 7.0 / 26.0);
    const qreal limit = qMin(rect.width(), rect.height()) / 2.0;
    return qBound(1.0, scaled, limit);
}

void strokeRoundedBorder(QPainter& painter, const QRectF& box, qreal radius,
                         const QColor& side, const QColor& top, const QColor& bottom)
{
    painter.save();
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(side, 1.0));
    painter.drawRoundedRect(box, radius, radius);

    painter.setClipRect(QRectF(box.left(), box.top(), box.width(), radius + 1.0),
                        Qt::IntersectClip);
    painter.setPen(QPen(top, 1.0));
    painter.drawRoundedRect(box, radius, radius);
    painter.restore();

    painter.save();
    painter.setBrush(Qt::NoBrush);
    painter.setClipRect(QRectF(box.left(), box.bottom() - radius - 1.0, box.width(), radius + 1.0),
                        Qt::IntersectClip);
    painter.setPen(QPen(bottom, 1.0));
    painter.drawRoundedRect(box, radius, radius);
    painter.restore();
}

} // namespace

NereusBandButtonItem::NereusBandButtonItem(QObject* parent)
    : BandButtonItem(parent)
{
    applyChrome();
}

QColor NereusBandButtonItem::lighten(const QColor& color)
{
    return shifted(color, 12);
}

void NereusBandButtonItem::applyChrome()
{
    for (int i = 0; i < buttonCount(); ++i) {
        ButtonState& face = button(i);
        face.textColour = m_textColor;
        face.fillColour = m_backgroundTop;
        face.offColour = m_backgroundBottom;
        face.hoverColour = lighten(m_backgroundTop);
        face.borderColour = fromHex(Workspace::kGlassEdgeLight);
        face.onColour = m_selectedTop;
        face.clickColour = m_backgroundBottom;
    }
}

void NereusBandButtonItem::setButtonTextColor(const QColor& color)
{
    if (!color.isValid() || color == m_textColor) {
        return;
    }
    m_textColor = color;
    applyChrome();
}

void NereusBandButtonItem::setBackgroundTopColor(const QColor& color)
{
    if (!color.isValid() || color == m_backgroundTop) { return; }
    m_backgroundTop = color;
    applyChrome();
}

void NereusBandButtonItem::setBackgroundBottomColor(const QColor& color)
{
    if (!color.isValid() || color == m_backgroundBottom) { return; }
    m_backgroundBottom = color;
    applyChrome();
}

void NereusBandButtonItem::setSelectedTopColor(const QColor& color)
{
    if (!color.isValid() || color == m_selectedTop) { return; }
    m_selectedTop = color;
    applyChrome();
}

void NereusBandButtonItem::setSelectedBottomColor(const QColor& color)
{
    if (!color.isValid() || color == m_selectedBottom) { return; }
    m_selectedBottom = color;
    applyChrome();
}

void NereusBandButtonItem::paintButton(QPainter& painter, int index, const QRectF& rect)
{
    const ButtonState& face = button(index);
    const bool pressed = index == pressedButton();
    const bool selected = face.on;

    QColor top = selected ? m_selectedTop : m_backgroundTop;
    QColor bottom = selected ? m_selectedBottom : m_backgroundBottom;
    QColor ink = m_textColor;
    if (!face.enabled) {
        top = bottom = QColor(Style::kDisabledBg);
        ink = QColor(Style::kDisabledText);
    } else if (index == hoveredButton() && !pressed && !selected) {
        top = lighten(top);
        bottom = lighten(bottom);
    }
    // A press turns the PureSignal gradient over, the same way for both faces.
    if (pressed && face.enabled) {
        std::swap(top, bottom);
    }

    // 1px rim, drawn on the half-pixel so it matches a stylesheet border.
    const QRectF box = rect.adjusted(0.5, 0.5, -0.5, -0.5);
    const qreal radius = pureSignalRadius(box);
    QLinearGradient fill(box.topLeft(), box.bottomLeft());
    fill.setColorAt(0.0, top);
    fill.setColorAt(1.0, bottom);
    painter.setPen(Qt::NoPen);
    painter.setBrush(fill);
    painter.drawRoundedRect(box, radius, radius);

    QColor side = fromHex(Workspace::kBorder);
    QColor edgeTop = fromHex(Workspace::kGlassEdgeLight);
    QColor edgeBottom = fromHex(Workspace::kGlassEdgeDark);
    if (!face.enabled) {
        side = edgeTop = edgeBottom = QColor(Style::kDisabledBorder);
    } else if (selected) {
        // Checked PureSignal button: green rim, lighter along the top.
        const QColor rim = shifted(fromHex(Workspace::kDspToggleBorder), 16);
        side = edgeBottom = rim;
        edgeTop = shifted(rim, 16);
    }
    if (pressed && face.enabled) {
        std::swap(edgeTop, edgeBottom);
    }
    strokeRoundedBorder(painter, box, radius, side, edgeTop, edgeBottom);

    if (face.text.isEmpty()) {
        return;
    }
    QFont font = WorkspacePreview::readableFont(painter.font(), painter.device());
    font.setPixelSize(WorkspacePreview::readableTextPixelSize(
        qMax(8, static_cast<int>(rect.height() * 0.4))));
    font.setBold(true);
    font = fitObjectText(font, font.pixelSize(), face.text, rect, false,
                         Qt::TextSingleLine, painter.device());
    painter.setFont(font);
    painter.setPen(ink);
    painter.drawText(rect, Qt::AlignCenter, face.text);
}

QString NereusBandButtonItem::serialize() const
{
    const QString fields = QStringLiteral("NEREUSBAND|%1|%2|%3|%4|%5|%6|%7|%8|%9")
        .arg(m_x).arg(m_y).arg(m_w).arg(m_h)
        .arg(m_bindingId).arg(m_zOrder)
        .arg(columns()).arg(activeBand()).arg(visibleBits());
    return fields + QLatin1Char('|') + m_textColor.name(QColor::HexArgb)
        + QLatin1Char('|') + m_backgroundTop.name(QColor::HexArgb)
        + QLatin1Char('|') + m_backgroundBottom.name(QColor::HexArgb)
        + QLatin1Char('|') + m_selectedTop.name(QColor::HexArgb)
        + QLatin1Char('|') + m_selectedBottom.name(QColor::HexArgb);
}

bool NereusBandButtonItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    if (parts.size() < 7 || parts[0] != QLatin1String("NEREUSBAND")) {
        return false;
    }
    m_x = parts[1].toFloat();
    m_y = parts[2].toFloat();
    m_w = parts[3].toFloat();
    m_h = parts[4].toFloat();
    m_bindingId = parts[5].toInt();
    m_zOrder = parts[6].toInt();
    if (parts.size() > 7) { setColumns(parts[7].toInt()); }
    if (parts.size() > 8) { setActiveBand(parts[8].toInt()); }
    if (parts.size() > 9) { setVisibleBits(parts[9].toUInt()); }
    if (parts.size() > 10) {
        const QColor text(parts[10]);
        if (text.isValid()) { m_textColor = text; }
    }
    if (parts.size() > 11) {
        const QColor top(parts[11]);
        if (top.isValid()) { m_backgroundTop = top; }
    }
    if (parts.size() > 12) {
        const QColor bottom(parts[12]);
        if (bottom.isValid()) { m_backgroundBottom = bottom; }
    }
    if (parts.size() > 13) {
        const QColor top(parts[13]);
        if (top.isValid()) { m_selectedTop = top; }
    }
    if (parts.size() > 14) {
        const QColor bottom(parts[14]);
        if (bottom.isValid()) { m_selectedBottom = bottom; }
    }
    applyChrome();
    return true;
}

} // namespace NereusSDR
