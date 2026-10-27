// =================================================================
// src/gui/meters/TextOverlayItem.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/MeterManager.cs
//     (clsTextOverlay, parseText, renderTextOverlay),
//     original licence from Thetis source is included below
//   Project Files/Source/Console/setup.cs
//     (btnTextOverlay_copyfonts_Click, btnTextOverlay_copyoffsets_Click),
//     original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26 - Colours at their defaults follow the workspace look when
//                 drawn (ItemColourLook.h); saved colours are unchanged.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-03 — Responsive object text and measured role fitting by J.J. Boyd
//                 (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-09-23: R-R3-13: %VALUE% shows "--" with no reading
//                 (isNoMeterReading); resolvedText1() / resolvedText2()
//                 read-only accessors. J.J. Boyd (KG4VCF), with AI-assisted
//                 transformation via Anthropic Claude Code.
//   2026-09-23: R-R3-13 fix wave: no reading applies only to
//                 receive-signal bindings (isReceiveSignalBinding); a TX
//                 meter keeps WDSP's -400 zero-power floor as a number.
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation via
//                 Anthropic Claude Code.
//   2026-10-25 - Saves and restores the text back colours and toggles after
//                 the older fields, so they survive a reload. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - Thetis RX/TX swap (renderTextOverlay) as LineMode
//                 RxTxSwap for new items, both lines kept for older
//                 records; per-line offsets, font styles, the panel's MOX
//                 flip fade (ContentFade::nextValue), auto scroll, copy
//                 font / offsets (setup.cs) and kTextOverlayThetisDefaults.
//                 Fields appended after the older ones. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-26 - RX and TX LED gates from Thetis clsTextOverlay
//                 (MeterManager.cs:20232-20310, 20546-20559): logic flags
//                 and 4-character IDs read from LedResultRegistry, appended
//                 to the record after [36]. Upstream's TX block tests the RX
//                 logic flag; NereusSDR uses the TX one. An unknown ID hides
//                 nothing. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-26 - Tokens resolve through the OverlayTokens table (Thetis
//                 parseText): reading, time and date, slice-letter, MOX and
//                 meter data source tokens, %group%, and %precis=N% for the
//                 whole line. Unknown text such as %1 stays as written.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

// --- From MeterManager.cs ---

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

// --- From setup.cs ---

//=================================================================
// setup.cs
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
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Continual modifications Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//=================================================================
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

// Modification history (NereusSDR):
//   2026-10-12 - Apply the approved workspace-only 11 px text minimum.
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-26 - User text in the record (font families) is joined, not substituted
//                 by chained QString::arg, so a "%1" or "%20" in it survives.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.

#include "gui/styles/WorkspacePreview.h"
#include "TextOverlayItem.h"
#include "ItemColourLook.h"
#include "ResponsiveText.h"

// From Thetis clsTextOverlay (MeterManager.cs:18746+)

#include "gui/containers/ContentFade.h"
#include "LedResultRegistry.h"
#include "MeterWidget.h"

#include <QPainter>
#include <QFont>
#include <QFontMetrics>
#include <QFontMetricsF>
#include <QMouseEvent>
#include <QDateTime>
#include <QStringList>
#include <algorithm>
#include <cmath>

namespace NereusSDR {

namespace {

// From Thetis MeterManager.cs:34185 [v2.10.3.15]: fontSize *= 2.1f;
// when a container is added, and not resized, 72 point font needs this fudge to get it to be 72 point
constexpr float kFontSizeFudge = 2.1f;
// plotText sizes text by the container width.
// From Thetis MeterManager.cs:37863 [v2.10.3.15]: float em_scale = containerWidth / 52f / 16f;
constexpr float kPlotTextEmWidth = 52.0f;
constexpr float kPlotTextEmSize = 16.0f;
// From Thetis MeterManager.cs:34208 [v2.10.3.15]: ScrollX * deltaTimeMS / 1000f
constexpr double kScrollMsPerSecond = 1000.0;
// From Thetis MeterManager.cs:34151 [v2.10.3.15]: mi.FadeValue = 48;
// (ContentFade::kDimmedLevel, the same 48.)
constexpr int kPanelFlipLevel = ContentFade::kDimmedLevel;
// NereusSDR: a long gap (a hidden container) catches up a few frames at
// most, as LEDItem does.
constexpr int kMaxCatchUpFrames = 16;

// getDXBrushForColour(c, nFade) replaces the alpha, except for a
// transparent colour (MeterManager.cs:32306-32315 [v2.10.3.15]).
QColor withAlpha(QColor c, int alpha)
{
    if (c.alpha() != 0) { c.setAlpha(alpha); }
    return c;
}

} // namespace

// ---------------------------------------------------------------------------
// resolveText()
// From Thetis parseText() (MeterManager.cs:20587-20716 [v2.10.3.15]), by
// way of the token table in OverlayTokens.
// ---------------------------------------------------------------------------
QString TextOverlayItem::resolveText(const QString& templateText) const
{
    if (templateText.isEmpty()) {
        return {};
    }
    return resolveOverlayTokens(templateText, overlayContext(templateText));
}

OverlayContext TextOverlayItem::overlayContext(const QString& templateText) const
{
    OverlayContext context;
    for (QObject* p = parent(); p; p = p->parent()) {
        if (auto* meter = qobject_cast<MeterWidget*>(p)) {
            context.readings = meter->lastBindingValues();
            break;
        }
    }
    context.slices = m_overlaySlices;
    for (const VfoSliceState& s : m_overlaySlices) {
        if (s.open && s.transmitting) {
            context.txSlice = QChar(u'A' + s.index);
            break;
        }
    }
    context.mox = effectiveTransmitting();
    context.nowUtc = QDateTime::currentDateTimeUtc();
    context.nowLocal = QDateTime::currentDateTime();

    // NereusSDR: %value% is the item's own reading, one decimal unless the
    // line has %precis=N%; "--" with no reading (R-R3-13).
    const int precision = overlayTokenPrecision(templateText);
    context.named.insert(QStringLiteral("value"), isNoReading(m_value)
        ? QStringLiteral("--")
        : QString::number(m_value, 'f', precision >= 0 ? precision : kValueDecimals));
    context.named.insert(QStringLiteral("group"), m_displayGroupText);
    for (auto it = m_variables.cbegin(); it != m_variables.cend(); ++it) {
        context.named.insert(it.key().toLower(), it.value());
    }
    return context;
}

bool TextOverlayItem::refreshResolvedText()
{
    const QString line1 = resolveText(m_text1);
    const QString line2 = resolveText(m_text2);
    const bool changed = line1 != m_lastResolved1 || line2 != m_lastResolved2;
    m_lastResolved1 = line1;
    m_lastResolved2 = line2;
    return changed;
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

    if (m_lineMode == LineMode::RxTxSwap) {
        paintSwap(p, widgetW, widgetH);
        return;
    }
    paintBoth(p, rect);
}

// Both lines, the look of records saved before LineMode existed.
void TextOverlayItem::paintBoth(QPainter& p, const QRect& rect)
{
    // Step 1: back panel fill — upper/lower halves may differ in color
    if (m_showBackPanel) {
        const QRect topHalf(rect.left(), rect.top(),
                            rect.width(), rect.height() / 2);
        const QRect bottomHalf(rect.left(), rect.top() + rect.height() / 2,
                               rect.width(), rect.height() - rect.height() / 2);
        p.fillRect(topHalf, ItemColourLook::effective("TextOverlay.panelBackColour1", m_panelBack1, this));
        p.fillRect(bottomHalf, ItemColourLook::effective("TextOverlay.panelBackColour2", m_panelBack2, this));
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
        QFont font1 = WorkspacePreview::readableFont(QFont(m_fontFamily1), p.device());
        font1.setBold(m_fontBold1);
        font1.setItalic(m_fontItalic1);
        font1.setUnderline(m_fontUnderline1);
        font1.setStrikeOut(m_fontStrikeout1);
        font1=fitObjectText(font1,m_fontSize1*objectTextScale(rect,{260,120}),resolved1,m_scrollX!=0?QRectF(lineRect1.left(),lineRect1.top(),1e9,lineRect1.height()):QRectF(lineRect1),true,
            m_scrollX!=0?Qt::TextSingleLine:Qt::TextWordWrap,p.device());
        p.setFont(font1);

        if (m_showTextBack1 && lineRect1.isValid()) {
            const QFontMetrics fm1(font1,p.device());
            const QRect textBound = fm1.boundingRect(lineRect1,
                                                      Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap,
                                                      resolved1);
            p.fillRect(textBound, ItemColourLook::effective("TextOverlay.textBackColour1", m_textBackColour1, this));
        }

        p.setPen(ItemColourLook::effective("TextOverlay.textColour1", m_textColour1, this));

        if (m_scrollX != 0.0f && lineRect1.isValid()) {
            // Scrolling: clip to lineRect1, draw at scrolled x position
            const QFontMetrics fm1(font1,p.device());
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
        QFont font2 = WorkspacePreview::readableFont(QFont(m_fontFamily2), p.device());
        font2.setBold(m_fontBold2);
        font2.setItalic(m_fontItalic2);
        font2.setUnderline(m_fontUnderline2);
        font2.setStrikeOut(m_fontStrikeout2);
        font2=fitObjectText(font2,m_fontSize2*objectTextScale(rect,{260,120}),resolved2,m_scrollX!=0?QRectF(lineRect2.left(),lineRect2.top(),1e9,lineRect2.height()):QRectF(lineRect2),true,
            m_scrollX!=0?Qt::TextSingleLine:Qt::TextWordWrap,p.device());
        p.setFont(font2);

        if (m_showTextBack2 && lineRect2.isValid()) {
            const QFontMetrics fm2(font2,p.device());
            const QRect textBound = fm2.boundingRect(lineRect2,
                                                      Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap,
                                                      resolved2);
            p.fillRect(textBound, ItemColourLook::effective("TextOverlay.textBackColour2", m_textBackColour2, this));
        }

        p.setPen(ItemColourLook::effective("TextOverlay.textColour2", m_textColour2, this));

        if (m_scrollX != 0.0f && lineRect2.isValid()) {
            const QFontMetrics fm2(font2,p.device());
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
// RxTxSwap: Thetis renderTextOverlay
// From Thetis MeterManager.cs:34135-34290 [v2.10.3.15]
// Thetis places the text by ratios of targetWidth * m.XRatio; an item here
// keeps its own rectangle, so those ratios are of the item's width (as
// LEDItem's placed look does), and plotText's container width is the
// widget's (as ClockItem's Thetis layout does).
// ---------------------------------------------------------------------------

// From Thetis MeterManager.cs:34146 [v2.10.3.15]
bool TextOverlayItem::coverFadeActive() const
{
    const bool tx = effectiveTransmitting();
    return (fadeOnRx() && !tx) || (fadeOnTx() && tx);
}

// From Thetis MeterManager.cs:34147-34152 [v2.10.3.15]: on a MOX flip the
// panel fade restarts at 48.
bool TextOverlayItem::checkPanelFlip()
{
    if (coverFadeActive() || m_panelBack1 == m_panelBack2) {
        return false;
    }
    const bool tx = effectiveTransmitting();
    if (tx == m_moxFadeToggle) {
        return false;
    }
    m_panelFadeValue = kPanelFlipLevel;
    m_moxFadeToggle = tx;
    return true;
}

// From Thetis MeterManager.cs:34172 [v2.10.3.15]:
//   getDXBrushForColour(m.MOX ? text_overlay.PanelBackColour2 : text_overlay.PanelBackColour1, mi.FadeValue)
// While the fade on RX / TX veil is active, MeterWidget's veil dims the
// item (Thetis's FadeValue is that same fade), so the colour shows at full
// alpha.
QColor TextOverlayItem::panelColour() const
{
    const QColor c = effectiveTransmitting() ? ItemColourLook::effective("TextOverlay.panelBackColour2", m_panelBack2, this) : ItemColourLook::effective("TextOverlay.panelBackColour1", m_panelBack1, this);
    const bool flipFade = !coverFadeActive() && m_panelBack1 != m_panelBack2;
    return withAlpha(c, flipFade ? m_panelFadeValue : ContentFade::kUndimmedLevel);
}

// The panel is the item's width wide and padding * width tall (Thetis
// sizes the item SpacerPadding high, MeterManager.cs:27565
// [v2.10.3.15]); it stops at the item's bottom edge, as LEDItem's does.
QRect TextOverlayItem::swapPanelRect(int widgetW, int widgetH) const
{
    QRect rect = pixelRect(widgetW, widgetH);
    const int height = qRound(static_cast<double>(rect.width()) * static_cast<double>(m_padding));
    rect.setHeight(std::clamp(height, 0, rect.height()));
    return rect;
}

QFont TextOverlayItem::swapFont(bool tx, const QPaintDevice* device, int widgetW) const
{
    float fontSize = tx ? m_fontSize2 : m_fontSize1;
    fontSize *= kFontSizeFudge; // when a container is added, and not resized, 72 point font needs this fudge to get it to be 72 point
    // From Thetis MeterManager.cs:37863-37864 [v2.10.3.15] (plotText)
    const double emScale = static_cast<double>(widgetW) / kPlotTextEmWidth / kPlotTextEmSize;
    QFont font(tx ? m_fontFamily2 : m_fontFamily1);
    font.setPixelSize(std::max(1, static_cast<int>(std::lround(fontSize * emScale))));
    font.setBold(tx ? m_fontBold2 : m_fontBold1);
    font.setItalic(tx ? m_fontItalic2 : m_fontItalic1);
    font.setUnderline(tx ? m_fontUnderline2 : m_fontUnderline1);
    font.setStrikeOut(tx ? m_fontStrikeout2 : m_fontStrikeout1);
    return device ? WorkspacePreview::readableFont(font, device) : WorkspacePreview::readableFont(font);
}

TextOverlayItem::SwapLine TextOverlayItem::swapLine(int widgetW, int widgetH) const
{
    SwapLine line;
    const bool tx = effectiveTransmitting();

    // show?
    if (!tx && !m_showRx) { return line; }
    if (tx && !m_showTx) { return line; }

    const QRect rect = pixelRect(widgetW, widgetH);
    const double w = rect.width();

    // Determine the text to measure and display
    line.text = resolveText(tx ? m_text2 : m_text1);
    line.font = swapFont(tx, nullptr, widgetW);

    // Calculate the text position
    const double xOffset = tx ? m_xOffset2 : m_xOffset1;
    const double yOffset = tx ? m_yOffset2 : m_yOffset1;
    double textX = rect.left() + xOffset * w;
    const double textY = rect.top() + yOffset * w;

    const double scrollOffset = tx ? m_scrollXOffset2 : m_scrollXOffset1;
    textX += scrollOffset * w;
    line.topLeft = QPointF(textX, textY);

    // Render the text
    line.fillBack = (m_showTextBack1 && !tx) || (m_showTextBack2 && tx);
    line.backColour = withAlpha(tx ? ItemColourLook::effective("TextOverlay.textBackColour2", m_textBackColour2, this) : ItemColourLook::effective("TextOverlay.textBackColour1", m_textBackColour1, this), ContentFade::kUndimmedLevel);
    line.colour = withAlpha(tx ? ItemColourLook::effective("TextOverlay.textColour2", m_textColour2, this) : ItemColourLook::effective("TextOverlay.textColour1", m_textColour1, this), ContentFade::kUndimmedLevel);
    return line;
}

void TextOverlayItem::paintSwap(QPainter& p, int widgetW, int widgetH)
{
    const QRect rect = pixelRect(widgetW, widgetH);
    const bool tx = effectiveTransmitting();
    m_measuredItemWidth = rect.width();

    // From Thetis MeterManager.cs:34167-34174 [v2.10.3.15] (render_text false)
    if (m_showBackPanel) {
        p.fillRect(swapPanelRect(widgetW, widgetH), panelColour());
    }

    SwapLine line = swapLine(widgetW, widgetH);
    double& measured = tx ? m_measuredTextWidth2 : m_measuredTextWidth1;
    if (line.text.isEmpty()) {
        measured = 0.0;
        return;
    }
    line.font = swapFont(tx, p.device(), widgetW);

    // plotText: left and top aligned at the point, measured size
    // (MeterManager.cs:37858-37920 [v2.10.3.15]).
    const QFontMetricsF fm(line.font, p.device());
    const QSizeF size = fm.boundingRect(QRectF(0, 0, 1.0e9, 1.0e9),
                                        Qt::AlignLeft | Qt::AlignTop, line.text).size();
    measured = size.width();
    const QRectF box(line.topLeft, size);

    // NereusSDR: Thetis's overlay spans the container, so text scrolled
    // past its left edge leaves the container; here it stops at the item's
    // left and right edges.
    p.save();
    p.setClipRect(QRect(rect.left(), 0, rect.width(), widgetH));
    if (line.fillBack) {
        p.fillRect(box, line.backColour);
    }
    p.setFont(line.font);
    p.setPen(line.colour);
    p.drawText(box, Qt::AlignLeft | Qt::AlignTop | Qt::TextDontClip, line.text);
    p.restore();
}

// From Thetis MeterManager.cs:34197-34252 [v2.10.3.15]. Thetis measures
// and steps in the same frame; here paint() measures and advanceMeter()
// steps. Thetis's textX is from the container's left edge, where its
// overlay starts; here it is from the item's.
bool TextOverlayItem::stepScroll(double deltaMs)
{
    // scroll?
    if (m_lineMode != LineMode::RxTxSwap || m_scrollX == 0.0f) {
        return false;
    }
    const double w = m_measuredItemWidth;
    if (w <= 0.0) {
        return false;
    }
    const bool tx = effectiveTransmitting();
    float& lineScroll = tx ? m_scrollXOffset2 : m_scrollXOffset1;
    int& scrollCount = tx ? m_scrollXCount2 : m_scrollXCount1;
    const double textWidth = tx ? m_measuredTextWidth2 : m_measuredTextWidth1;
    const double xOffset = tx ? m_xOffset2 : m_xOffset1;
    const float scrollBefore = lineScroll;
    const int countBefore = scrollCount;

    // Calculate the text position
    const double textX = (xOffset * w) + (lineScroll * w);

    if (textWidth > w && scrollCount <= 1) // cant use w as it becomes 0 when panel hidden
    {
        lineScroll += static_cast<float>(m_scrollX * deltaMs / kScrollMsPerSecond);

        if (textX < -textWidth) {
            const double newX = w - (xOffset * w);
            lineScroll = static_cast<float>(newX / w);
            ++scrollCount;
        } else if (textX <= 0.0 && scrollCount == 1) {
            lineScroll = 0.0f;
            ++scrollCount;
        }
    } else {
        lineScroll = 0.0f;
    }
    return lineScroll != scrollBefore || scrollCount != countBefore;
}

void TextOverlayItem::setTransmitting(bool tx)
{
    MeterItem::setTransmitting(tx);
    if (m_lineMode == LineMode::RxTxSwap) {
        checkPanelFlip();
    }
}

namespace {

// The 4-character ID alphabet (Thetis common.cs convertToFourChar: A-Z,
// a-z, 0-9), at most four characters; anything else is dropped.
constexpr int kLedFourCharLength = 4;

QString cleanFourChar(const QString& id)
{
    QString out;
    for (const QChar c : id) {
        const char16_t u = c.unicode();
        const bool ok = (u >= u'A' && u <= u'Z') || (u >= u'a' && u <= u'z') || (u >= u'0' && u <= u'9');
        if (ok) {
            out.append(c);
            if (out.size() == kLedFourCharLength) {
                break;
            }
        }
    }
    return out;
}

} // namespace

// From Thetis MeterManager.cs:20233-20254 [v2.10.3.15] (RXLedLogic). Thetis's
// setter has the reset commented out:
//        //if (!_rx_led_logic)
//        //{
//        //    _show_rx = true;
//        //    _rxLed = null;
//        //}
// NereusSDR resets Show RX as the TX setter does, so a gate turned off
// never leaves its line hidden.
void TextOverlayItem::setRxLedLogic(bool on)
{
    m_rxLedLogic = on;
    if (!m_rxLedLogic) {
        m_showRx = true;
    }
}

// From Thetis MeterManager.cs:20255-20276 [v2.10.3.15] (TXLedLogic)
void TextOverlayItem::setTxLedLogic(bool on)
{
    m_txLedLogic = on;
    if (!m_txLedLogic) {
        m_showTx = true;
    }
}

void TextOverlayItem::setRxFourChar(const QString& id)
{
    m_rxFourChar = cleanFourChar(id);
}

void TextOverlayItem::setTxFourChar(const QString& id)
{
    m_txFourChar = cleanFourChar(id);
}

// From Thetis MeterManager.cs:20546-20559 [v2.10.3.15] (clsTextOverlay
// Update). Thetis's TryGetValue writes false for an ID it does not hold,
// hiding the line; NereusSDR leaves the line shown for an unknown ID, so a
// gate naming an LED that is gone (or not made yet) hides nothing.
bool TextOverlayItem::refreshLedGates()
{
    const bool rxBefore = m_showRx;
    const bool txBefore = m_showTx;
    const LedResultRegistry& results = LedResultRegistry::instance();
    if (m_rxLedLogic) {
        m_showRx = results.get(m_rxFourChar).value_or(true);
    }
    // From Thetis MeterManager.cs:20552-20559 [v2.10.3.15]: upstream tests _rx_led_logic for the TX LED;
    // NereusSDR uses the TX logic flag so the TX gate follows its own setting.
    if (m_txLedLogic) {
        m_showTx = results.get(m_txFourChar).value_or(true);
    }
    return m_showRx != rxBefore || m_showTx != txBefore;
}

// One Thetis frame per update interval (Thetis renders at the
// container's quickest interval).
bool TextOverlayItem::advanceMeter(qint64 monotonicMs)
{
    // The gates follow their LEDs in either line mode; only RxTxSwap draws
    // by them.
    const bool gatesChanged = refreshLedGates();
    // Tokens such as %time_utc% change with nothing else; a line with no
    // % has none.
    const bool hasTokens = m_text1.contains(QLatin1Char('%')) || m_text2.contains(QLatin1Char('%'));
    const bool textChanged = hasTokens && refreshResolvedText();
    if (m_lineMode != LineMode::RxTxSwap) {
        return textChanged;
    }
    bool changed = checkPanelFlip() || gatesChanged || textChanged;
    const int interval = updateIntervalMs() > 0 ? updateIntervalMs() : kDefaultUpdateIntervalMs;
    // The first call, or a clock that moved backwards, only takes the time.
    if (m_lastStepMs < 0 || monotonicMs < m_lastStepMs) {
        m_lastStepMs = monotonicMs;
        return changed;
    }
    int frames = 0;
    while (monotonicMs - m_lastStepMs >= interval) {
        // From Thetis MeterManager.cs:34146-34165 [v2.10.3.15]
        if (!coverFadeActive() && m_panelBack1 != m_panelBack2) {
            // fade to take half a second (ContentFade::nextValue with no fade
            // flag set takes the 255-48 = 207 step of :34158-34164)
            const int next = ContentFade::nextValue(m_panelFadeValue, false, false,
                                                    effectiveTransmitting(), interval);
            changed = changed || next != m_panelFadeValue;
            m_panelFadeValue = next;
        }
        changed = stepScroll(interval) || changed;
        m_lastStepMs += interval;
        if (++frames >= kMaxCatchUpFrames) {
            m_lastStepMs = monotonicMs;
            break;
        }
    }
    return changed;
}

// From Thetis MeterManager.cs:34260-34285 [v2.10.3.15]: the pointer over
// the text or the item lets the current line scroll again. MeterWidget
// sends only moves inside the item.
bool TextOverlayItem::handleMouseMove(QMouseEvent* event, int widgetW, int widgetH)
{
    Q_UNUSED(event); Q_UNUSED(widgetW); Q_UNUSED(widgetH);
    if (m_lineMode == LineMode::RxTxSwap) {
        if (effectiveTransmitting()) {
            m_scrollXCount2 = 0;
        } else {
            m_scrollXCount1 = 0;
        }
    }
    return false;
}

// From Thetis setup.cs:31291-31298 [v2.10.3.15] (btnTextOverlay_copyfonts_Click)
void TextOverlayItem::copyFont1To2()
{
    // _textOverlayFont2 = new Font(_textOverlayFont1.FontFamily, _textOverlayFont1.Size, _textOverlayFont1.Style)
    m_fontFamily2     = m_fontFamily1;
    m_fontSize2       = m_fontSize1;
    m_fontBold2       = m_fontBold1;
    m_fontItalic2     = m_fontItalic1;
    m_fontUnderline2  = m_fontUnderline1;
    m_fontStrikeout2  = m_fontStrikeout1;
    m_textColour2     = m_textColour1;
    m_textBackColour2 = m_textBackColour1;
    m_showTextBack2   = m_showTextBack1;
}

// From Thetis setup.cs:29690-29694 [v2.10.3.15] (btnTextOverlay_copyoffsets_Click)
void TextOverlayItem::copyOffsets1To2()
{
    m_xOffset2 = m_xOffset1;
    m_yOffset2 = m_yOffset1;
}

// From Thetis MeterManager.cs:20123-20183 [v2.10.3.15] (clsTextOverlay())
void TextOverlayItem::applyNewItemDefaults()
{
    MeterItem::applyNewItemDefaults();
    const TextOverlayItemThetisDefaults& d = kTextOverlayThetisDefaults;
    m_textColour1     = QColor::fromRgb(d.textColour1);
    m_textColour2     = QColor::fromRgb(d.textColour2);
    m_textBackColour1 = QColor::fromRgb(d.textBackColour1);
    m_textBackColour2 = QColor::fromRgb(d.textBackColour2);
    m_panelBack1      = QColor::fromRgb(d.panelBackColour1);
    m_panelBack2      = QColor::fromRgb(d.panelBackColour2);
    m_showTextBack1   = d.showTextBack1;
    m_showTextBack2   = d.showTextBack2;
    m_showBackPanel   = d.showBackPanel;
    m_xOffset1        = d.xOffset1;
    m_yOffset1        = d.yOffset1;
    m_xOffset2        = d.xOffset2;
    m_yOffset2        = d.yOffset2;
    m_scrollX         = d.scrollX;
    m_fontFamily1     = QString::fromLatin1(d.fontFamily1);
    m_fontSize1       = d.fontSize1;
    m_fontFamily2     = QString::fromLatin1(d.fontFamily2);
    m_fontSize2       = d.fontSize2;
    // FontStyle.Regular (:20170, :20173)
    m_fontBold1 = m_fontItalic1 = m_fontUnderline1 = m_fontStrikeout1 = false;
    m_fontBold2 = m_fontItalic2 = m_fontUnderline2 = m_fontStrikeout2 = false;
    m_padding         = d.padding;
    m_lineMode        = d.lineMode;
}

// ---------------------------------------------------------------------------
// serialize()
// Format: TEXTOVERLAY|x|y|w|h|bindingId|zOrder|
//         textColour1|textColour2|panelBack1|panelBack2|
//         showBackPanel|
//         fontFamily1|fontSize1|fontBold1|
//         fontFamily2|fontSize2|fontBold2|
//         scrollX|padding|
//         <base64(text1)>|<base64(text2)>|
//         textBackColour1|textBackColour2|showTextBack1|showTextBack2|
//         lineMode|xOffset1|yOffset1|xOffset2|yOffset2|
//         italic1|underline1|strikeout1|italic2|underline2|strikeout2|
//         rxLedLogic|txLedLogic|rxFourChar|txFourChar
//
// text1/text2 are base64-encoded to avoid '|' collisions. The text back
// fields, then the line mode, offsets and font styles, follow the older
// ones, so a record saved before they existed still loads (with the
// defaults; no line mode reads as Both). Never renumber.
// ---------------------------------------------------------------------------
QString TextOverlayItem::serialize() const
{
    const QString text1B64 = QString::fromLatin1(m_text1.toUtf8().toBase64());
    const QString text2B64 = QString::fromLatin1(m_text2.toUtf8().toBase64());

    // The font families are joined, not substituted: chained .arg() would
    // read a "%1" in one as a placeholder. Field positions are unchanged.
    return QStringLiteral("TEXTOVERLAY|%1|%2|%3|%4|%5|%6"
                          "|%7|%8|%9|%10|%11")
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
        + QLatin1Char('|') + m_fontFamily1
        + QStringLiteral("|%1|%2")
        .arg(static_cast<double>(m_fontSize1))
        .arg(m_fontBold1 ? 1 : 0)
        + QLatin1Char('|') + m_fontFamily2
        + QStringLiteral("|%1|%2|%3|%4|%5|%6")
        .arg(static_cast<double>(m_fontSize2))
        .arg(m_fontBold2 ? 1 : 0)
        .arg(static_cast<double>(m_scrollX))
        .arg(static_cast<double>(m_padding))
        .arg(text1B64)
        .arg(text2B64)
        + QStringLiteral("|%1|%2|%3|%4")
              .arg(m_textBackColour1.name(QColor::HexArgb))
              .arg(m_textBackColour2.name(QColor::HexArgb))
              .arg(m_showTextBack1 ? 1 : 0)
              .arg(m_showTextBack2 ? 1 : 0)
        + QStringLiteral("|%1|%2|%3|%4|%5|%6|%7|%8|%9|%10|%11")
              .arg(static_cast<int>(m_lineMode))
              .arg(static_cast<double>(m_xOffset1))
              .arg(static_cast<double>(m_yOffset1))
              .arg(static_cast<double>(m_xOffset2))
              .arg(static_cast<double>(m_yOffset2))
              .arg(m_fontItalic1 ? 1 : 0)
              .arg(m_fontUnderline1 ? 1 : 0)
              .arg(m_fontStrikeout1 ? 1 : 0)
              .arg(m_fontItalic2 ? 1 : 0)
              .arg(m_fontUnderline2 ? 1 : 0)
              .arg(m_fontStrikeout2 ? 1 : 0)
        + QStringLiteral("|%1|%2")
              .arg(m_rxLedLogic ? 1 : 0)
              .arg(m_txLedLogic ? 1 : 0)
        // The IDs are joined, not substituted (user text); setRxFourChar
        // and setTxFourChar keep them to letters and digits, so no '|'.
        + QLatin1Char('|') + m_rxFourChar
        + QLatin1Char('|') + m_txFourChar;
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
// Optional (absent from older records):
// [22] = textBackColour1      [23] = textBackColour2
// [24] = showTextBack1        [25] = showTextBack2
// [26] = lineMode (0 Both, 1 RxTxSwap; absent = Both)
// [27] = xOffset1  [28] = yOffset1  [29] = xOffset2  [30] = yOffset2
// [31] = italic1   [32] = underline1   [33] = strikeout1
// [34] = italic2   [35] = underline2   [36] = strikeout2
// [37] = rxLedLogic  [38] = txLedLogic  [39] = rxFourChar  [40] = txFourChar
//        (absent = logic off, no ID)
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

    if (parts.size() > 22) {
        const QColor back1(parts[22]);
        if (back1.isValid()) { m_textBackColour1 = back1; }
    }
    if (parts.size() > 23) {
        const QColor back2(parts[23]);
        if (back2.isValid()) { m_textBackColour2 = back2; }
    }
    if (parts.size() > 24) {
        const int show1 = parts[24].toInt(&ok);
        if (ok) { m_showTextBack1 = (show1 != 0); }
    }
    if (parts.size() > 25) {
        const int show2 = parts[25].toInt(&ok);
        if (ok) { m_showTextBack2 = (show2 != 0); }
    }

    // A record without a field takes the look it had before the field
    // existed: both lines, no offsets, no italic / underline / strikeout.
    m_lineMode = LineMode::Both;
    if (parts.size() > 26) {
        const int mode = parts[26].toInt(&ok);
        if (ok && mode == static_cast<int>(LineMode::RxTxSwap)) { m_lineMode = LineMode::RxTxSwap; }
    }
    const auto floatAt = [&parts](int index) {
        bool fieldOk = false;
        const float v = parts.size() > index ? parts[index].toFloat(&fieldOk) : 0.0f;
        return fieldOk && std::isfinite(v) ? v : 0.0f;
    };
    const auto flagAt = [&parts](int index) {
        bool fieldOk = false;
        const int v = parts.size() > index ? parts[index].toInt(&fieldOk) : 0;
        return fieldOk && v != 0;
    };
    m_xOffset1       = floatAt(27);
    m_yOffset1       = floatAt(28);
    m_xOffset2       = floatAt(29);
    m_yOffset2       = floatAt(30);
    m_fontItalic1    = flagAt(31);
    m_fontUnderline1 = flagAt(32);
    m_fontStrikeout1 = flagAt(33);
    m_fontItalic2    = flagAt(34);
    m_fontUnderline2 = flagAt(35);
    m_fontStrikeout2 = flagAt(36);
    // From Thetis MeterManager.cs:27554-27558 [v2.10.3.15]
    m_rxFourChar = parts.size() > 39 ? cleanFourChar(parts[39]) : QString();
    m_txFourChar = parts.size() > 40 ? cleanFourChar(parts[40]) : QString();
    setRxLedLogic(flagAt(37));
    setTxLedLogic(flagAt(38));

    // Runtime state starts again.
    m_scrollXOffset1 = 0.0f;
    m_scrollXOffset2 = 0.0f;
    m_scrollXCount1  = 0;
    m_scrollXCount2  = 0;
    m_panelFadeValue = ContentFade::kUndimmedLevel;
    m_moxFadeToggle  = effectiveTransmitting();

    return true;
}

} // namespace NereusSDR
