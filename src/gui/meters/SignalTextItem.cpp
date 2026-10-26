// =================================================================
// src/gui/meters/SignalTextItem.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26 - Fix wave round 2 (R2): a click box choice clears the
//                 entry's overrides with the new record, so a rebuild keeps it.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - Fix wave R2: units set on this item (its click box or the
//                 editor) stay when the global unit setting changes; an
//                 item with none set follows it. Appended record field 26.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - Thetis signal text options (renderSignalTextDisplay,
//                 MeterManager.cs:40538-40628): the PK / AVG / FFT type
//                 glyph, the uV and dBm sub lines, the background color, the
//                 peak value from the history peak without peak hold,
//                 history and ignore history, and kSignalTextThetisDefaults.
//                 The record appends its fields; a saved record keeps its
//                 look. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-25 - VHF S9 rule per slice: S-unit text reads S9 at -93 dBm
//                 when the slice is at or above 30 MHz (aboveS9Frequency).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-03 — Responsive object text and measured role fitting by J.J. Boyd
//                 (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-09-23: R-R3-13: no reading (isNoMeterReading) shows "--" with the
//                 unit and drops the smoothed value and peak to the -140 dBm
//                 floor; valueText() / peakValueText() read-only accessors.
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation via
//                 Anthropic Claude Code.
//   2026-09-23: R-R3-13 fix wave: no reading applies only to
//                 receive-signal bindings (isReceiveSignalBinding); a TX
//                 meter keeps WDSP's -400 zero-power floor as a number.
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation via
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

//=================================================================
// console.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems 
// Copyright (C) 2010-2020  Doug Wigley
// Credit is given to Sizenko Alexander of Style-7 (http://www.styleseven.com/) for the Digital-7 font.
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
// Modifications to support the Behringer Midi controllers
// by Chris Codella, W2PA, May 2017.  Indicated by //-W2PA comment lines. 
// Modifications for using the new database import function.  W2PA, 29 May 2017
// Support QSK, possible with Protocol-2 firmware v1.7 (Orion-MkI and Orion-MkII), and later.  W2PA, 5 April 2019 
// Modfied heavily - Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
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

// Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12

// Modification history (NereusSDR):
//   2026-10-12 - Apply the approved workspace-only 11 px text minimum.
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.

#include "gui/styles/WorkspacePreview.h"
#include "SignalTextItem.h"
#include "ResponsiveText.h"
#include "gui/containers/ContainerContentRegistry.h"

// From Thetis clsSignalText (MeterManager.cs:20286-20540)

#include <QPainter>
#include <QFont>
#include <QFontMetrics>
#include <QJsonObject>
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
    m_configNotifier = new ContentConfigNotifier(this);
}

void SignalTextItem::setOwnUnits(Units u)
{
    const bool changed = !m_unitsSetHere || m_units != u;
    m_units = u;
    m_unitsSetHere = true;
    // The choice is saved with the layout. The record now holds the item's
    // whole state (the editor's overrides were applied when it was built),
    // so the overrides go: kept, they would win again on the rebuild and
    // undo the click. Geometry comes from the entry's canvas rect, and the
    // reading from its context, so neither is lost.
    if (changed) {
        emit m_configNotifier->configEdited(
            QJsonObject{{QStringLiteral("legacyRecord"), serialize()},
                        {QStringLiteral("overrides"), QJsonObject{}}});
    }
}

void SignalTextItem::followGlobalUnits()
{
    m_unitsSetHere = false;
    m_units = unitsFor(unitMode());
}

// ---------------------------------------------------------------------------
// setValue()
// From Thetis clsSignalText — attack/decay smoothing + peak hold logic.
// (MeterManager.cs:20353-20380)
// ---------------------------------------------------------------------------
void SignalTextItem::setValue(double v)
{
    MeterItem::setValue(v);

    // NereusSDR (R-R3-13): no reading.  The text shows "--" (valueText),
    // the bar falls to its -140 dBm floor and the peak hold clears.
    if (isNoReading(v)) {
        m_smoothedDbm = -140.0f;
        m_peakDbm = -140.0f;
        m_peakHoldCounter = 0;
        m_history.clear();
        return;
    }

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

    appendHistory();
}

// ---------------------------------------------------------------------------
// History
// From Thetis clsSignalText.Update / HistoryDuration / ClearHistory
// (MeterManager.cs:21677-21701, 21732-21771) [v2.10.3.15]
// ---------------------------------------------------------------------------
int SignalTextItem::historyIntervalMs() const
{
    return updateIntervalMs() > 0 ? updateIntervalMs() : kDefaultUpdateIntervalMs;
}

void SignalTextItem::appendHistory()
{
    if (m_ignoringNext <= 0) {
        // signal history
        m_history.append(m_smoothedDbm); // adds to end of the list
        const int numberToRemove = static_cast<int>(m_history.size())
            - (m_historyDurationMs / historyIntervalMs());
        // the list is sized based on delay
        if (numberToRemove > 0) { m_history.remove(0, numberToRemove); } // remove the oldest, the head of the list
    } else {
        --m_ignoringNext;
    }
}

void SignalTextItem::setHistoryDurationMs(int ms)
{
    m_historyDurationMs = std::clamp(ms, kMinBarHistoryDurationMs, kMaxBarHistoryDurationMs);
}

void SignalTextItem::setIgnoreHistoryMs(int ms)
{
    m_ignoreHistoryMs = std::clamp(ms, kMinBarIgnoreHistoryMs, kMaxBarIgnoreHistoryMs);
}

void SignalTextItem::clearHistory()
{
    m_history.clear();
    m_ignoringNext = m_ignoreHistoryMs / historyIntervalMs();  // ignore next N readings to make up 2 second of ignore
}

float SignalTextItem::maxHistory() const
{
    if (m_history.isEmpty()) { return m_smoothedDbm; }
    return *std::max_element(m_history.cbegin(), m_history.cend());
}

float SignalTextItem::peakValueDbm() const
{
    return m_peakValueNeedsPeakHold ? m_peakDbm : maxHistory();
}

// ---------------------------------------------------------------------------
// applyNewItemDefaults()
// From Thetis MeterManager.cs:23001-23062, 21644-21676 [v2.10.3.15]
// ---------------------------------------------------------------------------
void SignalTextItem::applyNewItemDefaults()
{
    MeterItem::applyNewItemDefaults();
    const SignalTextItemThetisDefaults& d = kSignalTextThetisDefaults;
    m_backgroundColour  = QColor::fromRgb(d.backgroundColour);
    setBindingId(d.bindingId);
    m_attackRatio       = d.attackRatio;
    m_decayRatio        = d.decayRatio;
    m_historyDurationMs = d.historyDurationMs;
    m_ignoreHistoryMs   = d.ignoreHistoryMs;
    m_subMarkerColour   = QColor::fromRgb(d.subMarkerColour);
    m_showValue         = d.showValue;
    m_showPeakValue     = d.showPeakValue;
    m_colour            = QColor::fromRgb(d.fontColour);
    m_fontSize          = d.fontSize;
    m_peakColour        = QColor::fromRgb(d.peakValueColour);
    m_peakHold          = d.peakHold;
    m_showSubMarker     = d.showSubMarker;
    m_showType          = d.showType;
    m_typeColour        = QColor::fromRgb(d.typeColour);
    // Thetis's type is the reading glyph, and its peak value needs no peak
    // hold (renderSignalTextDisplay, MeterManager.cs:40550, 40603).
    m_typeStyle              = TypeStyle::ReadingGlyph;
    m_peakValueNeedsPeakHold = false;
}

// ---------------------------------------------------------------------------
// Type glyph
// From Thetis renderSignalTextDisplay (MeterManager.cs:40550-40578)
// [v2.10.3.15]
// ---------------------------------------------------------------------------
QString SignalTextItem::readingTypeGlyph(int bindingId)
{
    switch (bindingId) {
        case MeterBinding::SignalPeak:   return QStringLiteral("PK");
        case MeterBinding::SignalAvg:    return QStringLiteral("AVG");
        case MeterBinding::SignalMaxBin: return QStringLiteral("FFT");
        default:                         return {};
    }
}

void SignalTextItem::paintReadingTypeGlyph(QPainter& p, const QRectF& rect, int bindingId,
                                           const QColor& colour, const QString& fontFamily,
                                           double fontPixels)
{
    const QString text = readingTypeGlyph(bindingId);
    if (text.isEmpty() || rect.isEmpty()) {
        return;
    }
    // From Thetis MeterManager.cs:40554-40555 [v2.10.3.15]
    constexpr double kGlyphDiagonal = 450.0;
    constexpr double kGlyphStroke = 1.5;
    // From Thetis MeterManager.cs:40567 [v2.10.3.15]
    constexpr double kCircleRadiusStrokes = 2.5;

    const double x = rect.left();
    const double y = rect.top();
    const double w = rect.width();
    const double h = rect.height();

    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    // signal type symbol top right
    const double fDiag = std::sqrt((w * w) + (h * h));
    const double fScale = fDiag / kGlyphDiagonal;
    const double fStrokeWidth = kGlyphStroke * fScale;
    p.setPen(QPen(colour, fStrokeWidth));
    p.setBrush(Qt::NoBrush);
    switch (bindingId) {
        case MeterBinding::SignalPeak:
            // arrow
            p.drawLine(QPointF(x + (w * 0.95), y + (h * 0.09)), QPointF(x + (w * 0.98), y + (h * 0.09)));
            p.drawLine(QPointF(x + (w * 0.95), y + (h * 0.2)), QPointF(x + (w * 0.965), y + (h * 0.1)));
            p.drawLine(QPointF(x + (w * 0.98), y + (h * 0.2)), QPointF(x + (w * 0.965), y + (h * 0.1)));
            break;
        case MeterBinding::SignalAvg: {
            // circle
            const double circ_rad = fStrokeWidth * kCircleRadiusStrokes;
            p.drawEllipse(QPointF(x + (w * 0.965), y + (h * 0.16)), circ_rad, circ_rad);
            break;
        }
        case MeterBinding::SignalMaxBin:
            // line
            p.drawLine(QPointF(x + (w * 0.95), y + (h * 0.2)), QPointF(x + (w * 0.965), y + (h * 0.1)));
            p.drawLine(QPointF(x + (w * 0.98), y + (h * 0.2)), QPointF(x + (w * 0.965), y + (h * 0.1)));
            break;
        default:
            break;
    }

    // The text right-aligned at x + w * 0.94, from y + h * 0.025
    // (plotText ... bAlignRight true).
    const QRectF textBox(x, y + (h * 0.025), w * 0.94, std::max(1.0, h * 0.15));
    QFont font(fontFamily);
    font.setPixelSize(std::max(1, static_cast<int>(std::lround(fontPixels))));
    p.setFont(fitObjectText(font, fontPixels, text, textBox, false, Qt::TextSingleLine, p.device()));
    p.setPen(colour);
    p.drawText(textBox, Qt::AlignRight | Qt::AlignTop, text);
    p.restore();
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
    // VHF S9 rule: at or above the S9 frequency S9 is -93 dBm
    // (MeterItem::s9Dbm), so the reading moves 20 dB up the -73 dBm scale
    // below.
    dbm += kS9DbmBelowS9Frequency - s9Dbm(m_aboveS9Frequency);
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
// noReadingLabel() / valueText() / peakValueText()
// NereusSDR (R-R3-13): "--" keeps the unit each format shows today.
// ---------------------------------------------------------------------------
QString SignalTextItem::noReadingLabel() const
{
    switch (m_units) {
        case Units::Dbm:    return QStringLiteral("-- dBm");
        case Units::SUnits: return QStringLiteral("--");
        case Units::Uv:     return QStringLiteral("-- uV");
    }
    return QStringLiteral("-- dBm");
}

QString SignalTextItem::valueText() const
{
    return isNoReading(m_value) ? noReadingLabel() : formatValue(m_smoothedDbm);
}

QString SignalTextItem::peakValueText() const
{
    return isNoReading(m_value) ? noReadingLabel() : formatValue(peakValueDbm());
}

// Sub lines: Thetis draws the dBm and uV of the value, and of the peak.
// From Thetis MeterManager.cs:40592-40601, 40617-40626 [v2.10.3.15]
QString SignalTextItem::subDbmText() const
{
    return isNoReading(m_value) ? QStringLiteral("-- dBm") : formatDbm(m_smoothedDbm);
}

QString SignalTextItem::subUvText() const
{
    return isNoReading(m_value) ? QStringLiteral("-- uV") : formatUv(m_smoothedDbm);
}

QString SignalTextItem::peakSubDbmText() const
{
    return isNoReading(m_value) ? QStringLiteral("-- dBm") : formatDbm(peakValueDbm());
}

QString SignalTextItem::peakSubUvText() const
{
    return isNoReading(m_value) ? QStringLiteral("-- uV") : formatUv(peakValueDbm());
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
    p.fillRect(rect, m_backgroundColour);

    // Step 2: compute scaled font size
    // Scale font relative to widget height — larger widget = larger text
    const float scaledSize = m_fontSize * (static_cast<float>(rect.height()) / 100.0f);
    const float clampedSize = std::max(6.0f, scaledSize);

    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    // Rows: the peak on top, the value, then the sub line and the units
    // label. Without the sub line the rows are thirds, as a saved record
    // drew them.
    const bool peakShown = peakValueShown();
    const bool unitsLabel = m_showType && m_typeStyle == TypeStyle::UnitsLabel;
    const int auxRows = int(peakShown) + int(unitsLabel) + int(m_showSubMarker);
    const double rowH = m_showSubMarker
        ? rect.height() / static_cast<double>(std::max(3, auxRows + 1))
        : rect.height() / 3.0;

    // Step 2b: the reading type glyph, top right (Thetis ShowType)
    if (m_showType && m_typeStyle == TypeStyle::ReadingGlyph) {
        // From Thetis MeterManager.cs:40563 [v2.10.3.15] — FontSize * 0.32
        constexpr float kGlyphTextRatio = 0.32f;
        paintReadingTypeGlyph(p, QRectF(rect), bindingId(), m_typeColour, m_fontFamily,
                              std::max(6.0f, clampedSize * kGlyphTextRatio));
    }

    // Step 3: main value text (centered, large)
    if (m_showValue) {
        QFont mainFont = WorkspacePreview::readableFont(QFont(m_fontFamily, static_cast<int>(clampedSize)), p.device());
        mainFont.setBold(true);
        p.setFont(mainFont);
        p.setPen(m_colour);

        const QString text = valueText();
        const double top=peakShown?(m_showSubMarker?rowH:rect.height()/3.0):0;
        const double bottom=m_showSubMarker
            ? rect.height()-rowH*(1+int(unitsLabel))
            : (unitsLabel?rect.height()*2/3.0:rect.height());
        const QRectF main(rect.left(),rect.top()+top,rect.width(),bottom-top);
        p.setFont(fitObjectText(mainFont,clampedSize,text,main,true,Qt::TextSingleLine,p.device()));
        p.drawText(main,Qt::AlignCenter,text);
    }

    // Step 3b: the uV and dBm sub line under the value, in the sub color
    // From Thetis MeterManager.cs:40592-40601 [v2.10.3.15] — FontSize * 0.55
    constexpr float kSubTextRatio = 0.55f;
    if (m_showSubMarker) {
        const float subSize = std::max(6.0f, clampedSize * kSubTextRatio);
        const QFont subFont = WorkspacePreview::readableFont(QFont(m_fontFamily, static_cast<int>(subSize)), p.device());
        const double subTop = rect.top() + rect.height() - rowH * (1 + int(unitsLabel));
        const QRectF left(rect.left() + rect.width() * 0.02, subTop, rect.width() * 0.47, rowH);
        const QRectF right(rect.left() + rect.width() * 0.51, subTop, rect.width() * 0.47, rowH);
        p.setPen(m_subMarkerColour);
        const QString dbm = subDbmText();
        const QString uv = subUvText();
        p.setFont(fitObjectText(subFont, subSize, dbm, left, true, Qt::TextSingleLine, p.device()));
        p.drawText(left, Qt::AlignLeft | Qt::AlignVCenter, dbm);
        p.setFont(fitObjectText(subFont, subSize, uv, right, true, Qt::TextSingleLine, p.device()));
        p.drawText(right, Qt::AlignRight | Qt::AlignVCenter, uv);
    }

    // Step 4: units label below value in small font
    if (unitsLabel) {
        QString unitsText;
        switch (m_units) {
            case Units::Dbm:    unitsText = QStringLiteral("dBm");     break;
            case Units::SUnits: unitsText = QStringLiteral("S-Units"); break;
            case Units::Uv:     unitsText = QStringLiteral("uV");      break;
        }

        const float smallSize = std::max(6.0f, clampedSize * 0.4f);
        QFont smallFont = WorkspacePreview::readableFont(QFont(m_fontFamily, static_cast<int>(smallSize)), p.device());
        p.setFont(smallFont);
        p.setPen(m_typeColour);

        // Position in lower portion of rect
        const QRectF labelRect = m_showSubMarker
            ? QRectF(rect.left(), rect.top() + rect.height() - rowH, rect.width(), rowH)
            : QRectF(QRect(rect.left(), rect.top() + rect.height() * 2 / 3,
                           rect.width(), rect.height() / 3));
        p.setFont(fitObjectText(smallFont,smallSize,unitsText,labelRect,true,Qt::TextSingleLine,p.device()));
        p.drawText(labelRect, Qt::AlignHCenter | Qt::AlignBottom, unitsText);
    }

    // Step 5: peak value in top-right, smaller font
    if (peakShown) {
        const float peakSize = std::max(6.0f, clampedSize * 0.5f);
        QFont peakFont = WorkspacePreview::readableFont(QFont(m_fontFamily, static_cast<int>(peakSize)), p.device());
        peakFont.setBold(true);
        p.setFont(peakFont);
        p.setPen(m_peakColour);

        const QString peakText = peakValueText();
        if (!m_showSubMarker) {
            const QRect peakRect(rect.left(), rect.top(),
                                  rect.width(), rect.height() / 3);
            p.setFont(fitObjectText(peakFont,peakSize,peakText,peakRect,true,Qt::TextSingleLine,p.device()));
            p.drawText(peakRect, Qt::AlignRight | Qt::AlignTop, peakText);
        } else {
            // With the sub line, Thetis draws the peak's dBm left, the peak
            // centred and its uV right, all in the peak value color.
            // From Thetis MeterManager.cs:40603-40626 [v2.10.3.15] — FontSize * 0.35
            constexpr float kPeakSubTextRatio = 0.35f;
            const double third = rect.width() / 3.0;
            const QRectF left(rect.left() + rect.width() * 0.02, rect.top(), third - rect.width() * 0.02, rowH);
            const QRectF centre(rect.left() + third, rect.top(), third, rowH);
            const QRectF right(rect.left() + 2 * third, rect.top(), third - rect.width() * 0.02, rowH);
            p.setFont(fitObjectText(peakFont,peakSize,peakText,centre,true,Qt::TextSingleLine,p.device()));
            p.drawText(centre, Qt::AlignHCenter | Qt::AlignVCenter, peakText);
            const float subSize = std::max(6.0f, clampedSize * kPeakSubTextRatio);
            const QFont subFont = WorkspacePreview::readableFont(QFont(m_fontFamily, static_cast<int>(subSize)), p.device());
            const QString dbm = peakSubDbmText();
            const QString uv = peakSubUvText();
            p.setFont(fitObjectText(subFont, subSize, dbm, left, true, Qt::TextSingleLine, p.device()));
            p.drawText(left, Qt::AlignLeft | Qt::AlignVCenter, dbm);
            p.setFont(fitObjectText(subFont, subSize, uv, right, true, Qt::TextSingleLine, p.device()));
            p.drawText(right, Qt::AlignRight | Qt::AlignVCenter, uv);
        }
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
//                    |typeStyle|typeColour|showSubMarker|subMarkerColour
//                    |backgroundColour|peakValueNeedsPeakHold|historyMs
//                    |ignoreHistoryMs|unitsSetHere
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
        .arg(static_cast<int>(m_barStyle))
        + QStringLiteral("|%1|%2|%3|%4|%5|%6|%7|%8")
        .arg(static_cast<int>(m_typeStyle))
        .arg(m_typeColour.name(QColor::HexArgb))
        .arg(m_showSubMarker ? 1 : 0)
        .arg(m_subMarkerColour.name(QColor::HexArgb))
        .arg(m_backgroundColour.name(QColor::HexArgb))
        .arg(m_peakValueNeedsPeakHold ? 1 : 0)
        .arg(m_historyDurationMs)
        .arg(m_ignoreHistoryMs)
        + QStringLiteral("|%1").arg(m_unitsSetHere ? 1 : 0);
}

// ---------------------------------------------------------------------------
// deserialize()
// Expected parts:
// [0]=SIGNALTEXT [1-6]=base fields [7]=units [8]=showValue [9]=showPeakValue
// [10]=showType [11]=peakHold [12]=colour [13]=peakColour [14]=markerColour
// [15]=fontFamily [16]=fontSize [17]=barStyle
// [18..25] (optional; a record without them keeps the look it had):
// typeStyle, typeColour, showSubMarker, subMarkerColour, backgroundColour,
// peakValueNeedsPeakHold, historyMs, ignoreHistoryMs
// [26] (optional) unitsSetHere: 1 keeps the record's units when the global
// unit setting changes. A record without it follows the global setting: it
// cannot show that someone chose its units, since every record writes a
// unit and the global setting overwrote it at each change.
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

    // Appended fields. A missing or malformed field takes the value a
    // record without it painted with.
    const auto flag = [&parts](int i, bool fallback) {
        if (i < parts.size() && (parts[i] == QLatin1String("0") || parts[i] == QLatin1String("1"))) {
            return parts[i] == QLatin1String("1");
        }
        return fallback;
    };
    const auto colourAt = [&parts](int i, const QColor& fallback) {
        if (i < parts.size()) {
            const QColor c(parts[i]);
            if (c.isValid()) { return c; }
        }
        return fallback;
    };
    const auto integer = [&parts](int i, int fallback) {
        bool okInt = false;
        const int v = i < parts.size() ? parts[i].toInt(&okInt) : 0;
        return okInt ? v : fallback;
    };
    const int typeStyle = integer(18, static_cast<int>(TypeStyle::UnitsLabel));
    m_typeStyle = typeStyle == static_cast<int>(TypeStyle::ReadingGlyph)
        ? TypeStyle::ReadingGlyph : TypeStyle::UnitsLabel;
    m_typeColour             = colourAt(19, QColor(0x80, 0x90, 0xa0));
    m_showSubMarker          = flag(20, false);
    m_subMarkerColour        = colourAt(21, QColor(0x80, 0x80, 0x80));
    m_backgroundColour       = colourAt(22, QColor(0x0a, 0x0a, 0x18));
    m_peakValueNeedsPeakHold = flag(23, true);
    setHistoryDurationMs(integer(24, 2000));
    setIgnoreHistoryMs(integer(25, 2000));
    m_unitsSetHere = flag(26, false);

    return true;
}

} // namespace NereusSDR
