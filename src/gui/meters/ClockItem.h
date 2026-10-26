#pragma once

// =================================================================
// src/gui/meters/ClockItem.h  (NereusSDR)
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
//   2026-10-25 - publishedReadings(): time_utc, time_loc, date_utc and
//                 date_loc as Thetis builds them; injectable time source.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - thetisTime / thetisAmPm / thetisDate and a public
//                 currentLocalTime(), shared with the composite clock face.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - Layout (NereusSDR / Thetis renderClock), background fill
//                 colour and enable, applyNewItemDefaults() and
//                 kClockThetisDefaults: new clocks take Thetis's look,
//                 saved clocks keep theirs. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
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

#include "MeterItem.h"
#include <QColor>
#include <QDateTime>
#include <QPointF>
#include <QRectF>
#include <QVector>
#include <functional>

namespace NereusSDR {
// Dual UTC/Local time display. From Thetis clsClock (MeterManager.cs:14075+).
class ClockItem : public MeterItem {
    Q_OBJECT
public:
    explicit ClockItem(QObject* parent = nullptr, bool ownTimer = true);

    // How the clock lays out its text. NereusSDR (0, the constructor's and
    // every older record's) stacks title, time and date centred in each
    // half; Thetis (1, new items) is renderClock's large time with a corner
    // title. From Thetis MeterManager.cs:40474-40537 [v2.10.3.15]
    enum class Layout { NereusSDR = 0, Thetis = 1 };
    void setLayout(Layout l) { m_layout = l; }
    Layout layout() const { return m_layout; }

    // Background fill behind each half (Thetis AddClock's two clsSolidColour
    // items). Off for older records.
    void setBackColour(const QColor& c) { m_backColour = c; }
    QColor backColour() const { return m_backColour; }
    void setBackEnabled(bool on) { m_backEnabled = on; }
    bool backEnabled() const { return m_backEnabled; }

    // New items (makeEntry) take kClockThetisDefaults.
    void applyNewItemDefaults() override;

    // One run of text the Thetis layout draws: its top left in widget
    // pixels, font size in pixels (plotText's em-scaled size) and colour.
    struct TextRun {
        QString text;
        QPointF topLeft;
        double  fontSize{0.0};
        QColor  colour;
    };
    // The runs the Thetis layout draws, in paint order.
    QVector<TextRun> thetisRuns(int widgetW, int widgetH) const;
    // The background halves for the current layout, in widget pixels.
    QVector<QRectF> backRects(int widgetW, int widgetH) const;

    void setShow24Hour(bool v) { m_show24Hour = v; }
    bool show24Hour() const { return m_show24Hour; }
    void setShowType(bool v) { m_showType = v; }
    bool showType() const { return m_showType; }

    void setTimeColour(const QColor& c) { m_timeColour = c; }
    QColor timeColour() const { return m_timeColour; }
    void setDateColour(const QColor& c) { m_dateColour = c; }
    QColor dateColour() const { return m_dateColour; }
    void setTypeTitleColour(const QColor& c) { m_typeTitleColour = c; }
    QColor typeTitleColour() const { return m_typeTitleColour; }

    Layer renderLayer() const override { return Layer::OverlayDynamic; }
    void paint(QPainter& p, int widgetW, int widgetH) override;
    bool advanceMeter(qint64 monotonicMs) override;
    QString serialize() const override;
    bool deserialize(const QString& data) override;

    // time_utc, time_loc, date_utc, date_loc.
    // From Thetis MeterManager.cs:31065-31099 [v2.10.3.15]
    QHash<QString, QVariant> publishedReadings() const override;

    // Local time source; the clock and its readings use it. Unset (or an
    // empty function) means QDateTime::currentDateTime(). Tests inject a
    // fixed time.
    void setTimeSource(std::function<QDateTime()> source) { m_timeSource = std::move(source); }
    // The time source's local time (or QDateTime::currentDateTime()).
    QDateTime currentLocalTime() const;

    // Thetis's clock strings, in the C locale. 24 hour is HH:mm:ss; 12 hour
    // is h:mm:ss with am / pm apart (thetisAmPm). The date is
    // ddd d MMM yyyy. From Thetis MeterManager.cs:40489-40511 [v2.10.3.15]
    static QString thetisTime(const QDateTime& dt, bool show24Hour);
    static QString thetisAmPm(const QDateTime& dt);
    static QString thetisDate(const QDateTime& dt);

private:
    bool m_show24Hour{true};
    bool m_showType{true};
    QColor m_timeColour{0xc8, 0xd8, 0xe8};
    QColor m_dateColour{0x80, 0x90, 0xa0};
    QColor m_typeTitleColour{0x70, 0x80, 0x90};
    Layout m_layout{Layout::NereusSDR};
    QColor m_backColour{32, 32, 32};
    bool m_backEnabled{false};
    qint64 m_lastClockSecond{-1};
    std::function<QDateTime()> m_timeSource;
};

// New CLOCK items (makeEntry) take Thetis's clock: the clsClock
// constructor (MeterManager.cs:15407-15421 [v2.10.3.15]) and AddClock's
// two background halves (MeterManager.cs:25815-25859 [v2.10.3.15]).
// Thetis has no setup control for the font; NereusSDR draws with it and
// does not save it. Where Trebuchet MS is not installed, Qt's usual font
// fallback picks the face.
struct ClockItemThetisDefaults {
    const char*       fontFamily;
    float             fontSize;
    QRgb              timeColour;
    QRgb              dateColour;
    QRgb              typeTitleColour;
    bool              show24Hour;
    bool              showType;
    QRgb              backColour;
    bool              backEnabled;
    ClockItem::Layout layout;
};
inline constexpr ClockItemThetisDefaults kClockThetisDefaults{
    "Trebuchet MS",             // _fontFamily = "Trebuchet MS" (:15409)
    18.0f,                      // _fontSize = 18f (:15411)
    qRgb(255, 165, 0),          // _timeColour = Color.Orange (:15413)
    qRgb(128, 128, 128),        // _dateColour = Color.Gray (:15414)
    qRgb(128, 128, 128),        // _typeTitleColour = Color.Gray (:15415)
    true,                       // _24HourClock = true (:15416)
    true,                       // _showType = true (:15412)
    qRgb(32, 32, 32),           // sc.Colour = Color.FromArgb(32, 32, 32) (:25825, :25833)
    true,                       // AddClock adds both halves (:25821-25835)
    ClockItem::Layout::Thetis,  // renderClock (:40474-40537)
};
} // namespace NereusSDR
