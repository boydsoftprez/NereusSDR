// Ported from Thetis MeterManager.cs [v2.10.3.15].
// Modification history (NereusSDR):
//   2026-10-26 - Thetis bar colour set: scale low and high colours, show
//                 marker, peak hold and peak value colours, solid and
//                 segment low and high colours, average (sub) marker colour
//                 and show, show title. New items take
//                 kBarPresetThetisDefaults (applyNewItemDefaults); saved
//                 records without the keys paint as before. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - setSecondaryValue: a second data source variable feeds the
//                 average marker. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-25 - setAboveS9Frequency overrides MeterItem's (VHF S9 rule
//                 per slice). J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-04 — Selected RX source identity and RX-only presentation reset by
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
// 2026-10-02 — Native complete faces by J.J. Boyd (KG4VCF), with AI-assisted
// transformation via OpenAI Codex.
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

#pragma once
#include "gui/meters/MeterItem.h"
#include "gui/meters/MeterDynamics.h"
#include <QJsonObject>
#include <QFont>
#include <QRgb>
#include <array>
namespace NereusSDR {
class BarPresetItem : public MeterItem {
    Q_OBJECT
public:
    explicit BarPresetItem(QObject* parent = nullptr);
    void configureAsMic();
    void configureAsAlc();
    void configureAsCustom(int bindingId, double minV, double maxV, const QString& label);
    QString typeId() const;
    static QStringList variants();
    bool configureVariant(const QString& flavor);
    // New items (makeEntry) take the kBarPresetThetisDefaults row for their
    // flavor; a custom bar also takes kCustomBarThetisDefaults.
    void applyNewItemDefaults() override;
    void setAboveS9Frequency(bool above) override { MeterItem::setAboveS9Frequency(above); if(m_aboveS9!=above) { m_aboveS9=above; m_scaleCache={}; m_staticDirty=true; } }
    Layer renderLayer() const override { return Layer::Background; }
    bool participatesIn(Layer layer) const override;
    void paint(QPainter&, int, int) override;
    void paintForLayer(QPainter&, int, int, Layer) override;
    QSet<int> readingBindings() const override;
    void pushBindingValue(int, double) override;
    void setValue(double value) override { pushBindingValue(bindingId(), value); }
    void setSecondaryValue(int index, double value) override;
    void setBindingUnavailable(int, const QString&) override;
    bool advanceMeter(qint64) override;
    bool takeStaticPresentationChange() override { const bool dirty=m_staticDirty; m_staticDirty=false; return dirty; }
    void resetForTxTransition(bool) override;
    void resetRxSource() override;
    QString serialize() const override;
    bool deserialize(const QString&) override;
    QJsonObject configuration() const;
    bool applyConfiguration(const QJsonObject&);
    double calibratedPosition(double) const;
    double primaryValue() const { return m_primary.value(); }
    double averageValue() const { return m_average.value(); }
    double peakValue() const { return m_primary.maxHistory(); }
    int preferredFaceHeight() const { return m_rowHeight; }
    int preferredRowHeight() const { return m_rowHeight; }
    QSize minimumFaceSize() const { return QSize(260,m_rowHeight); }
    double minimumHistory() const { return m_primary.minHistory(); }
    bool hasPrimaryReading() const { return m_primary.hasReading(); }
    bool hasAverageReading() const { return m_average.hasReading(); }
private:
    void configureDynamics();
    // Today's paint for the colour-set keys; a saved record without a key
    // keeps it.
    void resetColourSet();
    QString m_flavor = QStringLiteral("Custom"), m_label;
    int m_secondary = -1;
    double m_minimum = -30, m_maximum = 12, m_redThreshold = 0;
    QColor m_marker = Qt::yellow, m_background{32,32,32}, m_title{169,169,169};
    QColor m_historyColor{255,0,0,128};
    bool m_showHistory = true, m_peakHold = false, m_showValue = true, m_showPeakValue = true;
    QString m_style = QStringLiteral("Line"), m_units = QStringLiteral("dB");
    int m_interval = 100, m_historyMs = 2000, m_ignoreMs = 2000, m_rowHeight = 72;
    MeterDynamics m_primary, m_average;
    QJsonObject m_unknown;
    bool m_staticDirty = true;
    QColor m_lowFill = Qt::white;
    double m_calMinimum = -30, m_calMaximum = 12;
    double m_attack = .8, m_release = .1, m_middle = 0, m_middlePosition = .665;
    bool m_aboveS9 = false;
    QList<double> m_major{-20,-10,0,4,8,12}, m_minor{-25,-15,-5,2,6,10};
    QImage m_scaleCache;
    QRect m_scaleRect;
    QFont m_scaleFont;
    // Colour set (MeterManager.cs:28272-28315 [v2.10.3.15]). An invalid
    // optional colour draws today's colour for that part.
    QColor m_scaleLow = Qt::white, m_scaleHigh = Qt::red, m_subColor{169,169,169};
    QColor m_peakHoldColor, m_peakValueColor, m_segLowColor, m_segHighColor;
    bool m_showMarker = true, m_showSub = true, m_showTitle = true;
};

// New bar presets take their Thetis builder's values, over the clsBarItem
// constructor (MeterManager.cs:21284-21302), the clsScaleItem constructor
// (MeterManager.cs:16118-16124) and the builders' clsSolidColour backdrop.
struct BarPresetThetisDefaults {
    const char* flavor;
    QRgb markerColour;        // cb.MarkerColour (constructor Yellow)
    QRgb historyColour;       // cb.HistoryColour
    QRgb segLowColour;        // cb.Colour (constructor Red)
    QRgb segHighColour;       // cb.ColourHigh (constructor Orange)
    QRgb peakHoldColour;      // cb.PeakHoldMarkerColour (constructor Red)
    QRgb peakValueColour;     // cb.PeakValueColour (constructor Red)
    QRgb subColour;           // sub bar cb.MarkerColour (DarkGray)
    QRgb scaleLowColour;      // cs.LowColour (constructor White)
    QRgb scaleHighColour;     // cs.HighColour (constructor Red)
    QRgb backgroundColour;    // sc.Colour
    bool showHistory;         // cb.ShowHistory
    bool peakHold;            // cb.PeakHold (constructor false)
    bool showMarker;          // cb.ShowMarker (constructor true)
    bool showSub;             // sub bar ShowMarker (constructor true)
    bool showTitle;           // cs.ShowType
    const char* style;        // cb.Style
};
namespace BarPresetColours {
inline constexpr QRgb kYellow = qRgb(0xff, 0xff, 0x00);          // Color.Yellow
inline constexpr QRgb kOrange = qRgb(0xff, 0xa5, 0x00);          // Color.Orange
inline constexpr QRgb kRed = qRgb(0xff, 0x00, 0x00);             // Color.Red
inline constexpr QRgb kWhite = qRgb(0xff, 0xff, 0xff);           // Color.White
inline constexpr QRgb kDarkGray = qRgb(0xa9, 0xa9, 0xa9);        // Color.DarkGray
inline constexpr QRgb kCadetBlue = qRgb(0x5f, 0x9e, 0xa0);       // Color.CadetBlue
inline constexpr QRgb kDarkCyan = qRgb(0x00, 0x8b, 0x8b);        // Color.DarkCyan
inline constexpr QRgb kBackground = qRgb(32, 32, 32);            // Color.FromArgb(32, 32, 32)
inline constexpr QRgb kRedHistory = qRgba(0xff, 0x00, 0x00, 128);          // FromArgb(128, Red)
inline constexpr QRgb kCornflowerHistory = qRgba(0x64, 0x95, 0xed, 128);   // FromArgb(128, CornflowerBlue)
inline constexpr QRgb kPurpleHistory = qRgba(0x80, 0x00, 0x80, 128);       // FromArgb(128, Purple)
inline constexpr QRgb kLemonChiffonHistory = qRgba(0xff, 0xfa, 0xcd, 128); // FromArgb(128, LemonChiffon)
inline constexpr QRgb kPaleTurquoiseHistory = qRgba(0xaf, 0xee, 0xee, 128);// FromArgb(128, PaleTurquoise)
inline constexpr QRgb kPeachPuffHistory = qRgba(0xff, 0xda, 0xb9, 128);    // FromArgb(128, PeachPuff)
inline constexpr QRgb kVioletHistory = qRgba(0xee, 0x82, 0xee, 128);       // FromArgb(128, Violet)
}
inline constexpr std::array<BarPresetThetisDefaults, 19> kBarPresetThetisDefaults{{
    // From Thetis MeterManager.cs:22822-22900 [v2.10.3.15] (AddSMeterBarSignal, shared builder)
    {"Signal", BarPresetColours::kYellow, BarPresetColours::kRedHistory, BarPresetColours::kCadetBlue, BarPresetColours::kOrange, BarPresetColours::kRed, BarPresetColours::kRed, BarPresetColours::kDarkGray, BarPresetColours::kWhite, BarPresetColours::kRed, BarPresetColours::kBackground, true, false, true, true, true, "Line"},
    // From Thetis MeterManager.cs:22826 [v2.10.3.15] (AddSMeterBarSignalAvg, shared builder)
    {"SignalAvg", BarPresetColours::kYellow, BarPresetColours::kRedHistory, BarPresetColours::kCadetBlue, BarPresetColours::kOrange, BarPresetColours::kRed, BarPresetColours::kRed, BarPresetColours::kDarkGray, BarPresetColours::kWhite, BarPresetColours::kRed, BarPresetColours::kBackground, true, false, true, true, true, "Line"},
    // From Thetis MeterManager.cs:22830 [v2.10.3.15] (AddSMeterBarMaxBin, shared builder)
    {"SignalMaxBin", BarPresetColours::kYellow, BarPresetColours::kRedHistory, BarPresetColours::kCadetBlue, BarPresetColours::kOrange, BarPresetColours::kRed, BarPresetColours::kRed, BarPresetColours::kDarkGray, BarPresetColours::kWhite, BarPresetColours::kRed, BarPresetColours::kBackground, true, false, true, true, true, "Line"},
    // From Thetis MeterManager.cs:22940-22983 [v2.10.3.15] (AddADCMaxMag)
    {"AdcMax", BarPresetColours::kOrange, BarPresetColours::kCornflowerHistory, BarPresetColours::kRed, BarPresetColours::kOrange, BarPresetColours::kRed, BarPresetColours::kRed, BarPresetColours::kDarkGray, BarPresetColours::kWhite, BarPresetColours::kRed, BarPresetColours::kBackground, true, false, true, true, true, "Line"},
    // From Thetis MeterManager.cs:23063-23133 [v2.10.3.15] (AddADCBar)
    {"Adc", BarPresetColours::kOrange, BarPresetColours::kCornflowerHistory, BarPresetColours::kRed, BarPresetColours::kOrange, BarPresetColours::kRed, BarPresetColours::kRed, BarPresetColours::kDarkGray, BarPresetColours::kWhite, BarPresetColours::kRed, BarPresetColours::kBackground, true, false, true, true, true, "Line"},
    // From Thetis MeterManager.cs:23151-23195 [v2.10.3.15] (AddPBSNRBar)
    {"PbSnr", BarPresetColours::kYellow, BarPresetColours::kVioletHistory, BarPresetColours::kDarkCyan, BarPresetColours::kOrange, BarPresetColours::kRed, BarPresetColours::kRed, BarPresetColours::kDarkGray, BarPresetColours::kWhite, BarPresetColours::kRed, BarPresetColours::kBackground, false, true, true, true, true, "Segments"},
    // From Thetis MeterManager.cs:23222-23266 [v2.10.3.15] (AddAGCGainBar)
    {"AgcGain", BarPresetColours::kYellow, BarPresetColours::kVioletHistory, BarPresetColours::kDarkCyan, BarPresetColours::kOrange, BarPresetColours::kRed, BarPresetColours::kRed, BarPresetColours::kDarkGray, BarPresetColours::kWhite, BarPresetColours::kRed, BarPresetColours::kBackground, true, false, true, true, true, "Line"},
    // From Thetis MeterManager.cs:23284-23356 [v2.10.3.15] (AddAGCBar)
    {"Agc", BarPresetColours::kYellow, BarPresetColours::kVioletHistory, BarPresetColours::kDarkCyan, BarPresetColours::kOrange, BarPresetColours::kRed, BarPresetColours::kRed, BarPresetColours::kDarkGray, BarPresetColours::kWhite, BarPresetColours::kRed, BarPresetColours::kBackground, true, false, true, true, true, "Line"},
    // From Thetis MeterManager.cs:23374-23448 [v2.10.3.15] (AddCustomBar)
    {"Custom", BarPresetColours::kYellow, BarPresetColours::kVioletHistory, BarPresetColours::kDarkCyan, BarPresetColours::kOrange, BarPresetColours::kRed, BarPresetColours::kRed, BarPresetColours::kDarkGray, BarPresetColours::kWhite, BarPresetColours::kRed, BarPresetColours::kBackground, true, false, true, true, true, "Line"},
    // From Thetis MeterManager.cs:24327-24396 [v2.10.3.15] (AddMicBar)
    {"Mic", BarPresetColours::kYellow, BarPresetColours::kRedHistory, BarPresetColours::kRed, BarPresetColours::kOrange, BarPresetColours::kRed, BarPresetColours::kRed, BarPresetColours::kDarkGray, BarPresetColours::kWhite, BarPresetColours::kRed, BarPresetColours::kBackground, true, false, true, true, true, "Line"},
    // From Thetis MeterManager.cs:24414-24484 [v2.10.3.15] (AddEQBar)
    {"Eq", BarPresetColours::kYellow, BarPresetColours::kCornflowerHistory, BarPresetColours::kRed, BarPresetColours::kOrange, BarPresetColours::kRed, BarPresetColours::kRed, BarPresetColours::kDarkGray, BarPresetColours::kWhite, BarPresetColours::kRed, BarPresetColours::kBackground, true, false, true, true, true, "Line"},
    // From Thetis MeterManager.cs:24502-24570 [v2.10.3.15] (AddLevelerBar)
    {"Leveler", BarPresetColours::kYellow, BarPresetColours::kPurpleHistory, BarPresetColours::kRed, BarPresetColours::kOrange, BarPresetColours::kRed, BarPresetColours::kRed, BarPresetColours::kDarkGray, BarPresetColours::kWhite, BarPresetColours::kRed, BarPresetColours::kBackground, true, false, true, true, true, "Line"},
    // From Thetis MeterManager.cs:24588-24631 [v2.10.3.15] (AddLevelerGainBar)
    {"LevelerGain", BarPresetColours::kYellow, BarPresetColours::kPurpleHistory, BarPresetColours::kRed, BarPresetColours::kOrange, BarPresetColours::kRed, BarPresetColours::kRed, BarPresetColours::kDarkGray, BarPresetColours::kWhite, BarPresetColours::kRed, BarPresetColours::kBackground, true, false, true, true, true, "Line"},
    // From Thetis MeterManager.cs:24649-24717 [v2.10.3.15] (AddALCBar)
    {"Alc", BarPresetColours::kYellow, BarPresetColours::kLemonChiffonHistory, BarPresetColours::kRed, BarPresetColours::kOrange, BarPresetColours::kRed, BarPresetColours::kRed, BarPresetColours::kDarkGray, BarPresetColours::kWhite, BarPresetColours::kRed, BarPresetColours::kBackground, true, false, true, true, true, "Line"},
    // From Thetis MeterManager.cs:24735-24778 [v2.10.3.15] (AddALCGainBar)
    {"AlcGain", BarPresetColours::kYellow, BarPresetColours::kLemonChiffonHistory, BarPresetColours::kRed, BarPresetColours::kOrange, BarPresetColours::kRed, BarPresetColours::kRed, BarPresetColours::kDarkGray, BarPresetColours::kWhite, BarPresetColours::kRed, BarPresetColours::kBackground, true, false, true, true, true, "Line"},
    // From Thetis MeterManager.cs:24796-24839 [v2.10.3.15] (AddALCGroupBar)
    {"AlcGroup", BarPresetColours::kYellow, BarPresetColours::kLemonChiffonHistory, BarPresetColours::kRed, BarPresetColours::kOrange, BarPresetColours::kRed, BarPresetColours::kRed, BarPresetColours::kDarkGray, BarPresetColours::kWhite, BarPresetColours::kRed, BarPresetColours::kBackground, true, false, true, true, true, "Line"},
    // From Thetis MeterManager.cs:24857-24925 [v2.10.3.15] (AddCFCBar)
    {"Cfc", BarPresetColours::kYellow, BarPresetColours::kPaleTurquoiseHistory, BarPresetColours::kRed, BarPresetColours::kOrange, BarPresetColours::kRed, BarPresetColours::kRed, BarPresetColours::kDarkGray, BarPresetColours::kWhite, BarPresetColours::kRed, BarPresetColours::kBackground, true, false, true, true, true, "Line"},
    // From Thetis MeterManager.cs:24943-24986 [v2.10.3.15] (AddCFCGainBar)
    {"CfcGain", BarPresetColours::kYellow, BarPresetColours::kPaleTurquoiseHistory, BarPresetColours::kRed, BarPresetColours::kOrange, BarPresetColours::kRed, BarPresetColours::kRed, BarPresetColours::kDarkGray, BarPresetColours::kWhite, BarPresetColours::kRed, BarPresetColours::kBackground, true, false, true, true, true, "Line"},
    // From Thetis MeterManager.cs:25004-25072 [v2.10.3.15] (AddCompBar)
    {"Comp", BarPresetColours::kYellow, BarPresetColours::kPeachPuffHistory, BarPresetColours::kRed, BarPresetColours::kOrange, BarPresetColours::kRed, BarPresetColours::kRed, BarPresetColours::kDarkGray, BarPresetColours::kWhite, BarPresetColours::kRed, BarPresetColours::kBackground, true, false, true, true, true, "Line"},
}};
// A new custom bar's range, units and title (the setup defaults Thetis
// reads when it applies a custom bar).
struct CustomBarThetisDefaults { double minimum, maximum, high; const char* units; const char* title; };
// From Thetis MeterManager.cs:28251-28257 [v2.10.3.15]
inline constexpr CustomBarThetisDefaults kCustomBarThetisDefaults{0.0, 10.0, 7.5, "?", "Title"};
}
