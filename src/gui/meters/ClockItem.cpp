// =================================================================
// src/gui/meters/ClockItem.cpp  (NereusSDR)
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
//   2026-10-03 — Responsive object text and measured role fitting by J.J. Boyd
//                 (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-10-25 - Saves and restores the time, date and title colours after
//                 the older fields, so they survive a reload. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-25 - publishedReadings(): time_utc, time_loc, date_utc and
//                 date_loc as Thetis builds them; paint and readings share
//                 an injectable time source. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-10-26 - The Thetis time, am / pm and date strings are public
//                 statics (thetisTime, thetisAmPm, thetisDate) so the
//                 composite clock face draws them too. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-26 - Ported renderClock as the Thetis layout (layout 1, new
//                 items) beside the NereusSDR layout (0, older records);
//                 background halves from AddClock with an enable flag;
//                 applyNewItemDefaults() takes kClockThetisDefaults. The
//                 layout, back colour and enable follow the colours in the
//                 record. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
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
#include "ItemColourLook.h"
#include "ResponsiveText.h"
#include <QPainter>
#include <QDateTime>
#include <QFont>
#include <QFontMetricsF>
#include <QLocale>
#include <algorithm>
#include <cmath>

namespace NereusSDR {

ClockItem::ClockItem(QObject* parent, bool ownTimer) : MeterItem(parent)
{
    Q_UNUSED(ownTimer); // Compatibility argument; all clocks use the shared presentation cadence.
}

bool ClockItem::advanceMeter(qint64 monotonicMs)
{
    const qint64 second=monotonicMs/1000;
    const bool changed=second!=m_lastClockSecond;
    m_lastClockSecond=second;
    return changed;
}

namespace {

// Thetis layout (renderClock). From Thetis MeterManager.cs:40474-40537 [v2.10.3.15]
// From Thetis MeterManager.cs:40495, :40507 [v2.10.3.15]: if (sLoc.Length == 7) fPadLoc = w * 0.03f;
constexpr int kThetisPadTimeLength = 7;
constexpr float kThetisTimePad = 0.03f;
// From Thetis MeterManager.cs:40515 [v2.10.3.15]: plotText("local", x + (w * 0.01f), y + (h * 0.03f), ...)
constexpr float kThetisLocalTitleX = 0.01f;
// From Thetis MeterManager.cs:40516 [v2.10.3.15]: plotText("utc", x + (w * 0.52f), y + (h * 0.03f), ...)
constexpr float kThetisUtcTitleX = 0.52f;
constexpr float kThetisTitleY = 0.03f;
// From Thetis MeterManager.cs:40522 [v2.10.3.15]: new SharpDX.RectangleF(x + (w * 0.12f), y + (h * 0.02f), w, h)
constexpr float kThetisTimeX = 0.12f;
constexpr float kThetisTimeY = 0.02f;
// The UTC half sits w * 0.52f to the right. From Thetis MeterManager.cs:40527, :40534 [v2.10.3.15]
constexpr float kThetisUtcOffsetX = 0.52f;
// From Thetis MeterManager.cs:40523 [v2.10.3.15]: clk.FontSize * 1.9f
constexpr float kThetisTimeFontScale = 1.9f;
// From Thetis MeterManager.cs:40525 [v2.10.3.15]: rct.X + (w * 0.228f), rct.Y + (h * 0.285f), clk.FontSize * 0.8f
constexpr float kThetisAmPmX = 0.228f;
constexpr float kThetisAmPmY = 0.285f;
constexpr float kThetisAmPmFontScale = 0.8f;
// From Thetis MeterManager.cs:40532-40533 [v2.10.3.15]: x + (w * 0.132f), y + (h * 0.6f), clk.FontSize * 0.9f
constexpr float kThetisDateX = 0.132f;
constexpr float kThetisDateY = 0.6f;
constexpr float kThetisDateFontScale = 0.9f;
// plotText sizes text by the container width.
// From Thetis MeterManager.cs:37863 [v2.10.3.15]: float em_scale = containerWidth / 52f / 16f;
constexpr float kPlotTextEmWidth = 52.0f;
constexpr float kPlotTextEmSize = 16.0f;
// AddClock's pad around the background halves and the clock.
// From Thetis MeterManager.cs:22583 [v2.10.3.15]: _fPadX = 0.004f;
constexpr float kAddClockPadX = 0.004f;
// AddClock's half: 0.5f - _fPadX - (_fPadX * 0.5f) wide, the second at
// 0.5f + (_fPadX * 0.5f); the clock is 1f - _fPadX * 2f wide at _fPadX.
// From Thetis MeterManager.cs:25823-25824, :25831-25832, :25840-25841 [v2.10.3.15]
constexpr float kAddClockHalf = 0.5f;

// NereusSDR layout: each half 48% wide with a 4% gap.
constexpr int kNereusHalfPercent = 48;
constexpr int kNereusGapPercent = 4;

} // namespace

void ClockItem::applyNewItemDefaults()
{
    MeterItem::applyNewItemDefaults();
    const ClockItemThetisDefaults& d = kClockThetisDefaults;
    m_timeColour      = QColor::fromRgb(d.timeColour);
    m_dateColour      = QColor::fromRgb(d.dateColour);
    m_typeTitleColour = QColor::fromRgb(d.typeTitleColour);
    m_show24Hour      = d.show24Hour;
    m_showType        = d.showType;
    m_backColour      = QColor::fromRgb(d.backColour);
    m_backEnabled     = d.backEnabled;
    m_layout          = d.layout;
}

QVector<QRectF> ClockItem::backRects(int widgetW, int widgetH) const
{
    const QRect rect = pixelRect(widgetW, widgetH);
    if (m_layout == Layout::NereusSDR) {
        const int halfW = rect.width() * kNereusHalfPercent / 100;
        const int gap = rect.width() * kNereusGapPercent / 100;
        return {QRectF(rect.left(), rect.top(), halfW, rect.height()),
                QRectF(rect.left() + halfW + gap, rect.top(), halfW, rect.height())};
    }
    // The clock spans both halves, so place them relative to its width.
    const double clockSpan = 1.0 - kAddClockPadX * 2.0f;
    const double halfW = (kAddClockHalf - kAddClockPadX - (kAddClockPadX * 0.5f)) / clockSpan;
    const double secondX = (kAddClockHalf + (kAddClockPadX * 0.5f) - kAddClockPadX) / clockSpan;
    const double w = rect.width();
    return {QRectF(rect.left(), rect.top(), w * halfW, rect.height()),
            QRectF(rect.left() + w * secondX, rect.top(), w * halfW, rect.height())};
}

QVector<ClockItem::TextRun> ClockItem::thetisRuns(int widgetW, int widgetH) const
{
    const QRect rect = pixelRect(widgetW, widgetH);
    const double x = rect.left();
    const double y = rect.top();
    const double w = rect.width();
    const double h = rect.height();
    const double emScale = double(widgetW) / kPlotTextEmWidth / kPlotTextEmSize;
    const double fontSize = kClockThetisDefaults.fontSize;

    const QDateTime now = currentLocalTime();
    const QDateTime utcNow = now.toUTC();

    const QString sUtc = thetisTime(utcNow, m_show24Hour);
    const QString sUtcAmPm = m_show24Hour ? QString() : thetisAmPm(utcNow);
    double fPadUtc = 0;
    if (!m_show24Hour && sUtc.length() == kThetisPadTimeLength) { fPadUtc = w * kThetisTimePad; }

    const QString sLoc = thetisTime(now, m_show24Hour);
    const QString sLocAmPm = m_show24Hour ? QString() : thetisAmPm(now);
    double fPadLoc = 0;
    if (!m_show24Hour && sLoc.length() == kThetisPadTimeLength) { fPadLoc = w * kThetisTimePad; }

    const QString sUtcDate = thetisDate(utcNow);
    const QString sLocDate = thetisDate(now);

    QVector<TextRun> runs;
    const auto plot = [&](const QString& text, double px, double py, double size, const QColor& colour) {
        runs.append({text, QPointF(px, py), size * emScale, colour});
    };

    if (m_showType) {
        plot(QStringLiteral("local"), x + (w * kThetisLocalTitleX), y + (h * kThetisTitleY), fontSize, ItemColourLook::effective("Clock.typeTitleColour", m_typeTitleColour, this));
        plot(QStringLiteral("utc"), x + (w * kThetisUtcTitleX), y + (h * kThetisTitleY), fontSize, ItemColourLook::effective("Clock.typeTitleColour", m_typeTitleColour, this));
    }

    //time
    double rctX = x + (w * kThetisTimeX);
    const double rctY = y + (h * kThetisTimeY);
    plot(sLoc, rctX + fPadLoc, rctY, fontSize * kThetisTimeFontScale, ItemColourLook::effective("Clock.timeColour", m_timeColour, this));
    if (!m_show24Hour) {
        plot(sLocAmPm, rctX + (w * kThetisAmPmX), rctY + (h * kThetisAmPmY), fontSize * kThetisAmPmFontScale, ItemColourLook::effective("Clock.timeColour", m_timeColour, this));
    }

    rctX = x + (w * kThetisTimeX) + (w * kThetisUtcOffsetX);
    plot(sUtc, rctX + fPadUtc, rctY, fontSize * kThetisTimeFontScale, ItemColourLook::effective("Clock.timeColour", m_timeColour, this));
    if (!m_show24Hour) {
        plot(sUtcAmPm, rctX + (w * kThetisAmPmX), rctY + (h * kThetisAmPmY), fontSize * kThetisAmPmFontScale, ItemColourLook::effective("Clock.timeColour", m_timeColour, this));
    }

    //date
    const double dateY = y + (h * kThetisDateY);
    plot(sLocDate, x + (w * kThetisDateX), dateY, fontSize * kThetisDateFontScale, ItemColourLook::effective("Clock.dateColour", m_dateColour, this));
    plot(sUtcDate, x + (w * kThetisDateX) + (w * kThetisUtcOffsetX), dateY, fontSize * kThetisDateFontScale, ItemColourLook::effective("Clock.dateColour", m_dateColour, this));
    return runs;
}

void ClockItem::paint(QPainter& p, int widgetW, int widgetH)
{
    const QRect rect = pixelRect(widgetW, widgetH);
    p.setRenderHint(QPainter::Antialiasing, true);

    if (m_backEnabled) {
        for (const QRectF& half : backRects(widgetW, widgetH)) { p.fillRect(half, ItemColourLook::effective("Clock.backColour", m_backColour, this)); }
    }

    if (m_layout == Layout::Thetis) {
        // plotText: left and top aligned at the run's point, in the
        // em-scaled size (DirectWrite sizes are device-independent pixels).
        // From Thetis MeterManager.cs:37858-37920 [v2.10.3.15]
        QFont font(QString::fromLatin1(kClockThetisDefaults.fontFamily));
        for (const TextRun& run : thetisRuns(widgetW, widgetH)) {
            if (run.text.isEmpty() || run.fontSize <= 0.0) { continue; }
            font.setPixelSize(std::max(1, int(std::lround(run.fontSize))));
            const QFontMetricsF fm(font, p.device());
            const QRectF box(run.topLeft, QSizeF(fm.horizontalAdvance(run.text), fm.height()));
            p.setFont(font);
            p.setPen(run.colour);
            p.drawText(box, Qt::AlignLeft | Qt::AlignTop | Qt::TextDontClip, run.text);
        }
        return;
    }

    const int halfW = rect.width() * kNereusHalfPercent / 100;
    const int gap = rect.width() * kNereusGapPercent / 100;
    const QRect localRect(rect.left(), rect.top(), halfW, rect.height());
    const QRect utcRect(rect.left() + halfW + gap, rect.top(), halfW, rect.height());

    const QDateTime now = currentLocalTime();
    const QDateTime utcNow = now.toUTC();

    const QString timeFmt = m_show24Hour ? QStringLiteral("HH:mm:ss") : QStringLiteral("hh:mm:ss AP");
    const QString dateFmt = QStringLiteral("ddd d MMM yyyy");

    const int timeSize = qMax(10, rect.height() / 3);
    const int dateSize = qMax(8, rect.height() / 5);
    const int typeSize = qMax(7, rect.height() / 6);

    auto drawClock = [&](const QRect& r, const QDateTime& dt, const QString& typeLabel) {
        const double total=timeSize+dateSize+(m_showType?typeSize:0);
        double top=r.top();
        const auto role=[&](const QString& text,double requested,const QColor& ink,bool bold) {
            const double height=r.height()*requested/total;
            const QRectF box(r.left(),top,r.width(),height);
            QFont font=p.font(); font.setBold(bold);
            p.setFont(fitObjectText(font,requested,text,box,false,Qt::TextSingleLine,p.device())); p.setPen(ink);
            p.drawText(box,Qt::AlignCenter,text); top+=height;
        };
        if(m_showType) { role(typeLabel,typeSize,ItemColourLook::effective("Clock.typeTitleColour", m_typeTitleColour, this),false); }
        role(dt.toString(timeFmt),timeSize,ItemColourLook::effective("Clock.timeColour", m_timeColour, this),true);
        role(dt.toString(dateFmt),dateSize,ItemColourLook::effective("Clock.dateColour", m_dateColour, this),false);
    };

    drawClock(localRect, now, QStringLiteral("Local"));
    drawClock(utcRect, utcNow, QStringLiteral("UTC"));
}

QDateTime ClockItem::currentLocalTime() const
{
    return m_timeSource ? m_timeSource() : QDateTime::currentDateTime();
}

namespace {

// From Thetis MeterManager.cs:31093-31094 [v2.10.3.15]
constexpr const char* kPublishedDateFormat = "ddd d MMM yyyy";

} // namespace

// Thetis formats with .NET custom format strings. "HH:mm:ss" matches Qt;
// .NET "h" is the 12-hour hour (1-12), which Qt's "h" is not without an
// AP marker, so the 12-hour hour is computed here.
// From Thetis MeterManager.cs:31073-31091 [v2.10.3.15]
// The clock face draws the same strings (renderClock).
// From Thetis MeterManager.cs:40489-40511 [v2.10.3.15]
QString ClockItem::thetisTime(const QDateTime& dt, bool show24Hour)
{
    const QLocale c = QLocale::c();
    if (show24Hour) {
        return c.toString(dt, QStringLiteral("HH:mm:ss"));
    }
    int hour12 = dt.time().hour() % 12;
    if (hour12 == 0) {
        hour12 = 12;
    }
    return QString::number(hour12) + c.toString(dt, QStringLiteral(":mm:ss"));
}

QString ClockItem::thetisAmPm(const QDateTime& dt)
{
    return dt.time().hour() >= 12 ? QStringLiteral("pm") : QStringLiteral("am");
}

QString ClockItem::thetisDate(const QDateTime& dt)
{
    return QLocale::c().toString(dt, QString::fromLatin1(kPublishedDateFormat));
}

namespace {

QString publishedTime(const QDateTime& dt, bool show24Hour)
{
    if (show24Hour) {
        return ClockItem::thetisTime(dt, true);
    }
    return ClockItem::thetisTime(dt, false) + ClockItem::thetisAmPm(dt);
}

} // namespace

QHash<QString, QVariant> ClockItem::publishedReadings() const
{
    const QDateTime now = currentLocalTime();
    const QDateTime utcNow = now.toUTC();

    // From Thetis MeterManager.cs:31096-31099 [v2.10.3.15]
    QHash<QString, QVariant> readings;
    readings.insert(QStringLiteral("time_utc"), publishedTime(utcNow, m_show24Hour));
    readings.insert(QStringLiteral("date_utc"), thetisDate(utcNow));
    readings.insert(QStringLiteral("time_loc"), publishedTime(now, m_show24Hour));
    readings.insert(QStringLiteral("date_loc"), thetisDate(now));
    return readings;
}

QString ClockItem::serialize() const
{
    return QStringLiteral("CLOCK|%1|%2|%3|%4|%5|%6|%7|%8")
        .arg(m_x).arg(m_y).arg(m_w).arg(m_h)
        .arg(m_bindingId).arg(m_zOrder)
        .arg(m_show24Hour ? 1 : 0).arg(m_showType ? 1 : 0)
        // Colours follow the older fields, so a record saved before they
        // existed still loads (with the default colours).
        + QStringLiteral("|%1|%2|%3")
              .arg(m_timeColour.name(QColor::HexArgb))
              .arg(m_dateColour.name(QColor::HexArgb))
              .arg(m_typeTitleColour.name(QColor::HexArgb))
        // Layout (12), back colour (13) and back enable (14) follow, so an
        // older record loads with the NereusSDR layout and no fill.
        + QLatin1Char('|') + QString::number(static_cast<int>(m_layout))
        + QLatin1Char('|') + m_backColour.name(QColor::HexArgb)
        + QLatin1Char('|') + QString::number(m_backEnabled ? 1 : 0);
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
    const auto readColour = [&](int index, QColor& colour) {
        if (index >= parts.size()) { return; }
        const QColor parsed(parts[index]);
        if (parsed.isValid()) { colour = parsed; }
    };
    readColour(9, m_timeColour);
    readColour(10, m_dateColour);
    readColour(11, m_typeTitleColour);
    if (parts.size() > 12) {
        m_layout = parts[12].toInt() == static_cast<int>(Layout::Thetis) ? Layout::Thetis : Layout::NereusSDR;
    }
    readColour(13, m_backColour);
    if (parts.size() > 14) { m_backEnabled = parts[14].toInt() != 0; }
    return true;
}

} // namespace NereusSDR
