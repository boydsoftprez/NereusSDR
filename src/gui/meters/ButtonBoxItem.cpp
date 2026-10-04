// =================================================================
// src/gui/meters/ButtonBoxItem.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-03 — Invalidate cached interaction frames by J.J. Boyd (KG4VCF),
//                 AI-assisted via OpenAI Codex.
//   2026-10-03 — Responsive object text and measured role fitting by J.J. Boyd
//                 (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-03 — Bounded container single-control viewport sizing by
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-09-24 - R-R3-49 / R-R3-21: a button hidden until its feature is
//                 built (the saved visibility untouched), and an unavailable
//                 button whose click says why. J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
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

#include "ButtonBoxItem.h"
#include "ResponsiveText.h"

#include <QPainter>
#include <QMouseEvent>
#include <QtMath>
#include <QVariant>

namespace NereusSDR {

ButtonBoxItem::ButtonBoxItem(QObject* parent)
    : MeterItem(parent)
{
    // From Thetis clsButtonBox: 100ms click highlight timer (AutoReset=false)
    m_clickTimer.setSingleShot(true);
    m_clickTimer.setInterval(100);
    connect(&m_clickTimer, &QTimer::timeout, this, [this]() {
        m_clickedIndex = -1;
        m_interactionDirty = true;
    });
}

ButtonBoxItem::~ButtonBoxItem() = default;

bool ButtonBoxItem::advanceMeter(qint64 monotonicMs)
{
    Q_UNUSED(monotonicMs);
    const bool changed = m_interactionDirty;
    m_interactionDirty = false;
    return changed;
}

void ButtonBoxItem::setButtonCount(int count)
{
    m_buttonCount = count;
    m_buttons.resize(count);
}

ButtonBoxItem::ButtonState& ButtonBoxItem::button(int index)
{
    return m_buttons[index];
}

const ButtonBoxItem::ButtonState& ButtonBoxItem::button(int index) const
{
    return m_buttons[index];
}

void ButtonBoxItem::setVisibleBits(uint32_t bits)
{
    m_visibleBits = bits;
    for (int i = 0; i < m_buttonCount && i < 32; ++i) {
        m_buttons[i].visible = (bits >> i) & 1;
    }
}

void ButtonBoxItem::setButtonHiddenUntilBuilt(int index, bool hidden)
{
    if (index < 0 || index >= m_buttonCount) { return; }
    m_buttons[index].hiddenUntilBuilt = hidden;
}

bool ButtonBoxItem::isButtonShown(int index) const
{
    if (index < 0 || index >= m_buttonCount || index >= m_buttons.size()) { return false; }
    return m_buttons[index].visible && !m_buttons[index].hiddenUntilBuilt;
}

void ButtonBoxItem::setButtonAvailable(int index, bool available, const QString& reason)
{
    if (index < 0 || index >= m_buttonCount) { return; }
    m_buttons[index].enabled = available;
    m_buttons[index].unavailableReason = available ? QString() : reason;
}

void ButtonBoxItem::setAllButtonsAvailable(bool available, const QString& reason)
{
    for (int i = 0; i < m_buttonCount; ++i) {
        setButtonAvailable(i, available, reason);
    }
}

bool ButtonBoxItem::isButtonAvailable(int index) const
{
    if (index < 0 || index >= m_buttonCount) { return false; }
    return m_buttons[index].enabled;
}

QString ButtonBoxItem::buttonUnavailableReason(int index) const
{
    if (index < 0 || index >= m_buttonCount) { return QString(); }
    return m_buttons[index].unavailableReason;
}

void ButtonBoxItem::setupButton(int index, const QString& text, const QColor& onColour)
{
    if (index < 0 || index >= m_buttonCount) { return; }
    m_buttons[index].text = text;
    if (onColour.isValid()) {
        m_buttons[index].onColour = onColour;
    }
}

// ============================================================================
// Layout — from Thetis clsButtonBox size calculation
// ============================================================================

QRectF ButtonBoxItem::buttonRect(int index, const QRectF& area) const
{
    if (m_columns <= 0) { return {}; }

    // Count visible buttons up to this index to determine grid position
    int visIndex = 0;
    for (int i = 0; i < index; ++i) {
        if (isButtonShown(i)) {
            ++visIndex;
        }
    }

    const int col = visIndex % m_columns;
    const int row = visIndex / m_columns;

    // From Thetis: button_width = ((1 - 0.04) / columns) - margin - border
    const float pad = 0.04f;
    const float cellW = (area.width() * (1.0f - pad)) / m_columns;
    // Reference: Thetis MeterManager.cs:39027-39042 [v2.10.3.15] lays out
    // native cells against available width and height. NereusSDR-original:
    // only a supported single OTHERBTNS cell marked by
    // the container factory/editor fits its viewport. Legacy groups retain
    // the upstream aspect ratio. Paint and hit testing share this rectangle.
    const bool singleControl = m_columns == 1 && m_visibleBits != 0
        && (m_visibleBits & (m_visibleBits - 1)) == 0
        && property("containerSingleControl").toBool();
    const float cellH = singleControl ? area.height() * (1.0f - pad) : cellW * m_heightRatio;
    const float bw = cellW - (m_margin + m_borderWidth) * area.width();
    const float bh = cellH - (m_margin + m_borderWidth) * (singleControl ? area.height() : area.width());

    const float xOff = area.x() + (pad / 2.0f) * area.width();
    const float yOff = area.y() + (pad / 2.0f) * area.height();

    return QRectF(
        xOff + col * cellW + (m_margin * area.width() / 2.0f),
        yOff + row * cellH + (m_margin * (singleControl ? area.height() : area.width()) / 2.0f),
        bw, bh
    );
}

int ButtonBoxItem::buttonAt(const QPointF& pos, int widgetW, int widgetH) const
{
    const QRectF area = pixelRect(widgetW, widgetH);
    for (int i = 0; i < m_buttonCount; ++i) {
        if (isButtonShown(i)) {
            if (buttonRect(i, area).contains(pos)) {
                return i;
            }
        }
    }
    return -1;
}

// ============================================================================
// Painting
// ============================================================================

void ButtonBoxItem::paint(QPainter& p, int widgetW, int widgetH)
{
    // FadeOnRx/FadeOnTx guard (from Thetis clsButtonBox)
    if (m_fadeOnTx && m_transmitting) { return; }
    if (m_fadeOnRx && !m_transmitting) { return; }

    const QRectF area = pixelRect(widgetW, widgetH);
    p.setRenderHint(QPainter::Antialiasing, true);

    for (int i = 0; i < m_buttonCount; ++i) {
        if (!isButtonShown(i)) { continue; }
        const QRectF rect = buttonRect(i, area);
        paintButton(p, i, rect);
    }
}

void ButtonBoxItem::paintButton(QPainter& p, int index, const QRectF& rect)
{
    const ButtonState& btn = m_buttons[index];

    // Determine fill color: clicked > hovered > on/off
    QColor fill;
    if (index == m_clickedIndex) {
        fill = btn.clickColour;
    } else if (index == m_hoveredIndex) {
        fill = btn.hoverColour;
    } else {
        fill = btn.on ? btn.onColour : btn.fillColour;
    }

    // Button body
    const float r = m_cornerRadius;
    p.setPen(QPen(btn.borderColour, m_borderWidth * rect.width()));
    p.setBrush(fill);
    p.drawRoundedRect(rect, r, r);

    // Indicator (if button is "on")
    if (btn.on) {
        paintIndicator(p, index, rect);
    }

    // Text
    if (!btn.text.isEmpty()) {
        QColor textCol = btn.on && btn.indicatorType == IndicatorType::TextIconColour
                             ? btn.onColour : btn.textColour;
        if (!btn.enabled) {
            textCol.setAlpha(80);
        }
        QFont font = p.font();
        font.setPixelSize(qMax(8, static_cast<int>(rect.height() * 0.4)));
        font.setBold(true);
        font=fitObjectText(font,font.pixelSize(),btn.text,rect,false,Qt::TextSingleLine,p.device());
        p.setFont(font);
        p.setPen(textCol);
        p.drawText(rect, Qt::AlignCenter, btn.text);
    }
}

void ButtonBoxItem::paintIndicator(QPainter& p, int index, const QRectF& rect)
{
    const ButtonState& btn = m_buttons[index];
    if (btn.indicatorType == IndicatorType::TextIconColour) { return; }

    const float iw = btn.indicatorWidth * rect.width();
    p.setPen(Qt::NoPen);
    p.setBrush(btn.onColour);

    switch (btn.indicatorType) {
    case IndicatorType::Ring:
        p.setPen(QPen(btn.onColour, iw));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(rect.adjusted(iw/2, iw/2, -iw/2, -iw/2),
                          m_cornerRadius, m_cornerRadius);
        break;
    case IndicatorType::BarLeft:
        p.drawRect(QRectF(rect.left(), rect.top(), iw, rect.height()));
        break;
    case IndicatorType::BarRight:
        p.drawRect(QRectF(rect.right() - iw, rect.top(), iw, rect.height()));
        break;
    case IndicatorType::BarBottom:
        p.drawRect(QRectF(rect.left(), rect.bottom() - iw, rect.width(), iw));
        break;
    case IndicatorType::BarTop:
        p.drawRect(QRectF(rect.left(), rect.top(), rect.width(), iw));
        break;
    case IndicatorType::DotLeft:
        p.drawEllipse(QPointF(rect.left() + iw, rect.center().y()), iw, iw);
        break;
    case IndicatorType::DotRight:
        p.drawEllipse(QPointF(rect.right() - iw, rect.center().y()), iw, iw);
        break;
    case IndicatorType::DotBottom:
        p.drawEllipse(QPointF(rect.center().x(), rect.bottom() - iw), iw, iw);
        break;
    case IndicatorType::DotTop:
        p.drawEllipse(QPointF(rect.center().x(), rect.top() + iw), iw, iw);
        break;
    case IndicatorType::DotTopLeft:
        p.drawEllipse(QPointF(rect.left() + iw*2, rect.top() + iw*2), iw, iw);
        break;
    case IndicatorType::DotTopRight:
        p.drawEllipse(QPointF(rect.right() - iw*2, rect.top() + iw*2), iw, iw);
        break;
    case IndicatorType::DotBottomLeft:
        p.drawEllipse(QPointF(rect.left() + iw*2, rect.bottom() - iw*2), iw, iw);
        break;
    case IndicatorType::DotBottomRight:
        p.drawEllipse(QPointF(rect.right() - iw*2, rect.bottom() - iw*2), iw, iw);
        break;
    default:
        break;
    }
}

// ============================================================================
// Mouse Interaction
// ============================================================================

bool ButtonBoxItem::handleMousePress(QMouseEvent* event, int widgetW, int widgetH)
{
    if (m_fadeOnTx && m_transmitting) { return false; }
    if (m_fadeOnRx && !m_transmitting) { return false; }

    const int idx = buttonAt(event->position(), widgetW, widgetH);
    if (idx < 0) { return false; }
    if (!m_buttons[idx].enabled) {
        // R-R3-21: an unavailable button takes the click (so nothing
        // beneath it acts) and says why on release; it changes nothing.
        m_unavailablePressedIndex = idx;
        return true;
    }

    // From Thetis: setupClick(false) — immediate click highlight
    m_clickedIndex = idx;
    return true;
}

bool ButtonBoxItem::handleMouseRelease(QMouseEvent* event, int widgetW, int widgetH)
{
    if (m_unavailablePressedIndex >= 0) {
        const int pressed = m_unavailablePressedIndex;
        m_unavailablePressedIndex = -1;
        if (buttonAt(event->position(), widgetW, widgetH) == pressed
            && pressed < m_buttons.size() && !m_buttons[pressed].enabled) {
            emit unavailableButtonClicked(pressed, m_buttons[pressed].unavailableReason);
        }
        return true;
    }
    if (m_clickedIndex < 0) { return false; }

    const int idx = buttonAt(event->position(), widgetW, widgetH);

    // From Thetis: setupClick(true) — start 100ms timer to clear highlight
    m_clickTimer.start();

    if (idx == m_clickedIndex && idx >= 0) {
        emit buttonClicked(idx, event->button());
    }
    return true;
}

bool ButtonBoxItem::handleMouseMove(QMouseEvent* event, int widgetW, int widgetH)
{
    const int idx = buttonAt(event->position(), widgetW, widgetH);
    if (idx != m_hoveredIndex) {
        m_hoveredIndex = idx;
        return true;  // needs repaint
    }
    return false;
}

// ============================================================================
// Serialization
// ============================================================================

QString ButtonBoxItem::serialize() const
{
    // Base fields handled by subclasses who prepend their type tag
    return QString();
}

bool ButtonBoxItem::deserialize(const QString& data)
{
    Q_UNUSED(data);
    return false;
}

} // namespace NereusSDR
