#pragma once

// =================================================================
// src/gui/meters/SignalTextItem.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26 - Fix wave R2: units set on this item (its click box or the
//                 editor) stay when the global unit setting changes; an
//                 item with none set follows it. Appended record field 26.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-09-23: R-R3-13: no reading (isNoMeterReading) shows "--" with the
//                 unit and drops the smoothed value and peak to the -140 dBm
//                 floor; valueText() / peakValueText() read-only accessors.
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation via
//                 Anthropic Claude Code.
//   2026-10-26 - Thetis signal text options: PK / AVG / FFT type glyph and
//                 type color, the uV and dBm sub lines and their color, the
//                 background color, a peak value that shows without peak
//                 hold (the most recent history peak), history and ignore
//                 history, and kSignalTextThetisDefaults for new items. A
//                 saved record keeps its look. J.J. Boyd (KG4VCF),
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

#include "MeterItem.h"
#include "MeterPoller.h"
#include <QColor>
#include <QRectF>
#include <QVector>

namespace NereusSDR {

class ContentConfigNotifier;

// From Thetis clsSignalText (MeterManager.cs:20286+)
// Large text signal display with S-units/dBm/uV format switching.
class SignalTextItem : public MeterItem {
    Q_OBJECT

public:
    // From Thetis clsSignalText.Units enum (MeterManager.cs:20288)
    enum class Units { Dbm, SUnits, Uv };

    // From Thetis clsSignalText.BarStyle enum (MeterManager.cs:20294)
    enum class BarStyle { None, Line, SolidFilled, GradientFilled, Segments };

    explicit SignalTextItem(QObject* parent = nullptr);

    void setUnits(Units u) { m_units = u; }
    Units units() const { return m_units; }

    // Task 3.2 — unit-mode fan-out: override base setUnitMode() to sync the
    // Thetis-faithful per-item Units enum when the MultimeterPage combo
    // broadcasts to all MeterItems via ContainerManager::forEachMeterItem().
    // MeterUnit ↔ Units mapping: S↔SUnits, dBm↔Dbm, uV↔Uv (1:1).
    // An item whose units were set on it keeps them (fix wave R2); the
    // base unit mode still records the global setting, for Follow global.
    void setUnitMode(MeterUnit u) override {
        MeterItem::setUnitMode(u);
        if (m_unitsSetHere) { return; }
        m_units = unitsFor(u);
    }
    static Units unitsFor(MeterUnit u) {
        switch (u) {
            case MeterUnit::S:   return Units::SUnits;
            case MeterUnit::uV:  return Units::Uv;
            default:             return Units::Dbm;
        }
    }

    // Units set on this item, through its click box or the editor: kept
    // when the global unit setting changes, and saved (record field 26).
    // A live item's choice reaches its entry through its config notifier.
    void setOwnUnits(Units u);
    // Back to the global unit setting (the last one broadcast).
    void followGlobalUnits();
    bool unitsSetHere() const { return m_unitsSetHere; }

    void setShowValue(bool s) { m_showValue = s; }
    bool showValue() const { return m_showValue; }

    void setShowPeakValue(bool s) { m_showPeakValue = s; }
    bool showPeakValue() const { return m_showPeakValue; }

    void setShowType(bool s) { m_showType = s; }
    bool showType() const { return m_showType; }

    void setPeakHold(bool p) { m_peakHold = p; }
    bool peakHold() const { return m_peakHold; }

    void setColour(const QColor& c) { m_colour = c; }
    QColor colour() const { return m_colour; }

    void setPeakValueColour(const QColor& c) { m_peakColour = c; }
    QColor peakValueColour() const { return m_peakColour; }

    void setMarkerColour(const QColor& c) { m_markerColour = c; }
    QColor markerColour() const { return m_markerColour; }

    void setFontFamily(const QString& f) { m_fontFamily = f; }
    QString fontFamily() const { return m_fontFamily; }

    void setFontSize(float s) { m_fontSize = s; }
    float fontSize() const { return m_fontSize; }

    void setBarStyleMode(BarStyle s) { m_barStyle = s; }
    BarStyle barStyleMode() const { return m_barStyle; }

    // What Show type draws. UnitsLabel (a saved record's) is the units
    // under the value; ReadingGlyph is Thetis's PK / AVG / FFT symbol for
    // the reading (renderSignalTextDisplay, MeterManager.cs:40550-40578
    // [v2.10.3.15]).
    enum class TypeStyle { UnitsLabel, ReadingGlyph };
    void setTypeStyle(TypeStyle s) { m_typeStyle = s; }
    TypeStyle typeStyle() const { return m_typeStyle; }

    // Thetis TypeColour: the type glyph, or the units label.
    void setTypeColour(const QColor& c) { m_typeColour = c; }
    QColor typeColour() const { return m_typeColour; }

    // Thetis ShowSubMarker: the uV and dBm sub lines, in the sub color
    // (Thetis HistoryColour), beside the value and the peak.
    void setShowSubMarker(bool s) { m_showSubMarker = s; }
    bool showSubMarker() const { return m_showSubMarker; }
    void setSubMarkerColour(const QColor& c) { m_subMarkerColour = c; }
    QColor subMarkerColour() const { return m_subMarkerColour; }

    // The fill behind the text (Thetis's solid colour under the group).
    void setBackgroundColour(const QColor& c) { m_backgroundColour = c; }
    QColor backgroundColour() const { return m_backgroundColour; }

    // True (a saved record's): the peak value shows only with peak hold
    // and reads the held peak. False (Thetis): Show peak value alone shows
    // it, reading the history peak (MaxHistory).
    void setPeakValueNeedsPeakHold(bool on) { m_peakValueNeedsPeakHold = on; }
    bool peakValueNeedsPeakHold() const { return m_peakValueNeedsPeakHold; }
    bool peakValueShown() const
    {
        return m_showPeakValue && (m_peakHold || !m_peakValueNeedsPeakHold);
    }

    // Thetis HistoryDuration / IgnoreHistoryDuration. Clamped to
    // kMinBarHistoryDurationMs..kMaxBarHistoryDurationMs and
    // kMinBarIgnoreHistoryMs..kMaxBarIgnoreHistoryMs.
    void setHistoryDurationMs(int ms);
    int historyDurationMs() const { return m_historyDurationMs; }
    void setIgnoreHistoryMs(int ms);
    int ignoreHistoryMs() const { return m_ignoreHistoryMs; }
    void clearHistory() override;
    int ignoringReadings() const { return m_ignoringNext; }
    int historySize() const { return static_cast<int>(m_history.size()); }
    float maxHistory() const;

    // The smoothed reading the value line shows.
    float displayValue() const { return m_smoothedDbm; }

    // Thetis's type glyph text for a reading: "PK" for SignalPeak, "AVG"
    // for SignalAvg, "FFT" for SignalMaxBin, empty for any other.
    static QString readingTypeGlyph(int bindingId);
    // Draws that glyph (text and symbol) in the top right of `rect`, as
    // Thetis's renderSignalTextDisplay does. Nothing for other readings.
    static void paintReadingTypeGlyph(QPainter& p, const QRectF& rect, int bindingId,
                                      const QColor& colour, const QString& fontFamily,
                                      double fontPixels);

    // The sub line texts: dBm and uV for the smoothed value and for the
    // peak the peak value shows; "--" with the unit with no reading.
    QString subDbmText() const;
    QString subUvText() const;
    QString peakSubDbmText() const;
    QString peakSubUvText() const;

    // New items (makeEntry) take kSignalTextThetisDefaults.
    void applyNewItemDefaults() override;

    void setValue(double v) override;

    // The main and peak text paint() draws; "--" with the unit when there
    // is no reading (R-R3-13).
    QString valueText() const;
    QString peakValueText() const;

    Layer renderLayer() const override { return Layer::OverlayDynamic; }
    void paint(QPainter& p, int widgetW, int widgetH) override;
    QString serialize() const override;
    bool deserialize(const QString& data) override;

private:
    // From Thetis console.cs — format helpers
    QString formatDbm(float dbm) const;
    QString formatSUnits(float dbm) const;
    QString formatUv(float dbm) const;
    QString formatValue(float dbm) const;
    QString noReadingLabel() const;
    float peakValueDbm() const;
    int historyIntervalMs() const;
    void appendHistory();

    // From Thetis Common.UVfromDBM
    static double uvFromDbm(double dbm);

    Units    m_units{Units::Dbm};
    // Fix wave R2: units set on this item. False (and every record saved
    // before the field) follows the global unit setting.
    bool     m_unitsSetHere{false};
    ContentConfigNotifier* m_configNotifier{nullptr};
    bool     m_showValue{true};
    bool     m_showPeakValue{false};
    bool     m_showType{true};
    bool     m_peakHold{false};
    QColor   m_colour{0xff, 0x00, 0x00};         // Red (from Thetis line 20333)
    QColor   m_peakColour{0xff, 0x00, 0x00};      // Red (from Thetis line 20335)
    QColor   m_markerColour{0xff, 0xff, 0x00};    // Yellow (from Thetis line 20334)
    QString  m_fontFamily{QStringLiteral("Trebuchet MS")};
    float    m_fontSize{20.0f};
    BarStyle m_barStyle{BarStyle::None};

    // Smoothing (from Thetis line 20353-20354)
    float m_attackRatio{0.8f};
    float m_decayRatio{0.2f};
    float m_smoothedDbm{-140.0f};
    float m_peakDbm{-140.0f};
    int   m_peakHoldCounter{0};

    // Appended options. These starting values paint as a saved record did
    // before the options existed; new items take kSignalTextThetisDefaults.
    TypeStyle m_typeStyle{TypeStyle::UnitsLabel};
    QColor    m_typeColour{0x80, 0x90, 0xa0};        // the units label's ink
    bool      m_showSubMarker{false};
    QColor    m_subMarkerColour{0x80, 0x80, 0x80};   // Gray
    QColor    m_backgroundColour{0x0a, 0x0a, 0x18};
    bool      m_peakValueNeedsPeakHold{true};
    // From Thetis MeterManager.cs:21647-21648 [v2.10.3.15]
    int       m_historyDurationMs{2000};
    int       m_ignoreHistoryMs{2000};
    int       m_ignoringNext{0};
    QVector<float> m_history;
};

// New SIGNALTEXT items (makeEntry) and new meter.signalText presets take
// Thetis's signal text. From Thetis MeterManager.cs:23001-23062
// [v2.10.3.15] (AddSMeterBarText) and the clsSignalText constructor
// :21644-21676.
struct SignalTextItemThetisDefaults {
    QRgb  backgroundColour;
    int   bindingId;
    float attackRatio;
    float decayRatio;
    int   historyDurationMs;
    int   ignoreHistoryMs;
    bool  showHistory;
    QRgb  subMarkerColour;
    bool  showValue;
    bool  showPeakValue;
    QRgb  fontColour;
    float fontSize;
    QRgb  peakValueColour;
    bool  peakHold;
    bool  showSubMarker;
    bool  showType;
    QRgb  typeColour;
};
inline constexpr SignalTextItemThetisDefaults kSignalTextThetisDefaults{
    qRgb(32, 32, 32),               // sc.Colour = Color.FromArgb(32, 32, 32) (:23012)
    MeterBinding::SignalAvg,        // Reading.AVG_SIGNAL_STRENGTH (:23021)
    0.8f,                           // cst.AttackRatio = 0.8f (:23023)
    0.2f,                           // cst.DecayRatio = 0.2f (:23024)
    4000,                           // cst.HistoryDuration = 4000 (:23026)
    2000,                           // _msIgnoreHistoryDuration = 2000 (:21648)
    false,                          // cst.ShowHistory = false (:23027)
    qRgb(0x80, 0x80, 0x80),         // cst.HistoryColour = Color.Gray (:23028)
    true,                           // cst.ShowValue = true (:23029)
    true,                           // cst.ShowPeakValue = true (:23030)
    qRgb(0xff, 0xff, 0x00),         // cst.FontColour = Color.Yellow (:23031)
    56.0f,                          // cst.FontSize = 56f (:23032)
    qRgb(0xff, 0x00, 0x00),         // _peakValueColour = Color.Red (:21655)
    false,                          // _peakHold = false (:21656)
    true,                           // _showSubMarker = true (:21658)
    true,                           // _showType = true (:21667)
    qRgb(0x80, 0x80, 0x80),         // _typeColor = Color.Gray (:21668)
};

} // namespace NereusSDR
