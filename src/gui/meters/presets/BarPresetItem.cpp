// =================================================================
// src/gui/meters/presets/BarPresetItem.cpp  (NereusSDR)
// =================================================================
//
// Ported from multiple Thetis sources (3 flavours) and NereusSDR
// ItemGroup factories (15 flavours). See the .h for the full
// attribution banner. The Thetis MeterManager.cs header is
// reproduced verbatim below for the 3 Thetis-sourced flavours
// (Alc / AlcGain / AlcGroup).
//
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

// =================================================================
// Modification history (NereusSDR):
//   2026-04-20 — Reimplemented in C++20/Qt6 for NereusSDR by
//                J.J. Boyd (KG4VCF), with AI-assisted transformation
//                via Claude Opus 4.7. Thetis-sourced flavour
//                constants cite AddALCBar / AddALCGainBar /
//                AddALCGroupBar @501e3f5 byte-for-byte; NereusSDR-
//                local flavour constants cite the matching
//                ItemGroup::create*BarPreset() factory (which in
//                turn chain-cites Thetis AddMicBar / AddCompBar /
//                AddEQBar / AddLevelerBar / AddCFCBar / AddADCBar /
//                AddADCMaxMag).
// =================================================================

#include "gui/meters/presets/BarPresetItem.h"
#include "gui/ColorSwatchButton.h"   // colorToHex / colorFromHex
#include "gui/meters/MeterPoller.h"  // MeterBinding::*

#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QPen>
#include <QtGlobal>

namespace NereusSDR {

// =================================================================
// Constructor — default Custom flavour, everything at zero.
// =================================================================
BarPresetItem::BarPresetItem(QObject* parent)
    : MeterItem(parent)
{
    // Neutral Custom default; caller must invoke a configureAs* or
    // supply their own (bindingId, min, max, label) via setters or
    // configureAsCustom().
    setBindingId(-1);
}

// =================================================================
// Central helper — sets the common fields from a flavour config.
// =================================================================
void BarPresetItem::setCommon(Kind k, int binding, double minV,
                              double maxV, const QString& label,
                              const QColor& barColor,
                              double redThreshold)
{
    m_kind         = k;
    setBindingId(binding);
    m_minValue     = minV;
    m_maxValue     = maxV;
    m_label        = label;
    m_barColor     = barColor;
    m_redThreshold = redThreshold;
}

// =================================================================
// Per-flavour configureAs* — each method hard-codes the flavour's
// canonical defaults. The plan stores binding, min, max, label, bar
// colour, and red-threshold; all of them round-trip through
// serialize()/deserialize() so the Task 11 editor can override any
// field without needing a new flavour.
// =================================================================

// -------- Thetis-sourced flavours (3) --------
// Red-threshold for ALC-family bars follows the Thetis convention:
// the midpoint calibration key (0 dBfs) marks the visual red zone.

void BarPresetItem::configureAsAlc()
{
    // From Thetis MeterManager.cs:23326 [@501e3f5] — AddALCBar header
    // From Thetis MeterManager.cs:23347-23349 [@501e3f5] — ScaleCalibration
    //   -30 -> 0 ; 0 -> 0.665 ; 12 -> 0.99
    // From Thetis MeterManager.cs:23345 [@501e3f5] — historyColour
    //   LemonChiffon(128)  (preserved in the ItemGroup::buildBarRow
    //   layer; BarPresetItem exposes barColour only)
    setCommon(Kind::Alc,
              MeterBinding::TxAlc,
              -30.0, 12.0,
              QStringLiteral("ALC"),
              QColor(Qt::white),
              /*redThreshold=*/0.0);
}

void BarPresetItem::configureAsAlcGain()
{
    // From Thetis MeterManager.cs:23412 [@501e3f5] — AddALCGainBar header
    // From Thetis MeterManager.cs:23433-23435 [@501e3f5] — ScaleCalibration
    //   0 -> 0 ; 20 -> 0.8 ; 25 -> 0.99
    setCommon(Kind::AlcGain,
              MeterBinding::TxAlcGain,
              0.0, 25.0,
              QStringLiteral("ALC Gain"),
              QColor(Qt::white),
              /*redThreshold=*/20.0);
}

void BarPresetItem::configureAsAlcGroup()
{
    // From Thetis MeterManager.cs:23473 [@501e3f5] — AddALCGroupBar header
    // From Thetis MeterManager.cs:23494-23496 [@501e3f5] — ScaleCalibration
    //   -30 -> 0 ; 0 -> 0.5 ; 25 -> 0.99
    setCommon(Kind::AlcGroup,
              MeterBinding::TxAlcGroup,
              -30.0, 25.0,
              QStringLiteral("ALC Group"),
              QColor(Qt::white),
              /*redThreshold=*/0.0);
}

// -------- NereusSDR-local flavours (15) --------
// Each setCommon() below cites its backing ItemGroup factory; where
// that factory itself chain-cites Thetis, the Thetis cite is
// repeated on the line above setCommon() to preserve the chain.

void BarPresetItem::configureAsMic()
{
    // From ItemGroup::createMicPreset — ItemGroup.cpp:857
    // (chain-cites Thetis AddMicBar:23025-23027)
    setCommon(Kind::Mic,
              MeterBinding::TxMic,
              -30.0, 12.0,
              QStringLiteral("Mic"),
              QColor(Qt::white),
              /*redThreshold=*/0.0);
}

void BarPresetItem::configureAsComp()
{
    // From ItemGroup::createCompPreset — ItemGroup.cpp:867
    // (chain-cites Thetis AddCompBar:23681+)
    setCommon(Kind::Comp,
              MeterBinding::TxComp,
              -30.0, 12.0,
              QStringLiteral("COMP"),
              QColor(Qt::white),
              /*redThreshold=*/0.0);
}

void BarPresetItem::configureAsCfc()
{
    // From ItemGroup::createCfcBarPreset — ItemGroup.cpp:992
    // (chain-cites Thetis AddCFCBar:23534+)
    setCommon(Kind::Cfc,
              MeterBinding::TxCfc,
              -30.0, 12.0,
              QStringLiteral("CFC"),
              QColor(Qt::white),
              /*redThreshold=*/0.0);
}

void BarPresetItem::configureAsCfcGain()
{
    // From ItemGroup::createCfcGainBarPreset — ItemGroup.cpp:1001
    // (chain-cites Thetis AddCFCGainBar:23620+)
    setCommon(Kind::CfcGain,
              MeterBinding::TxCfcGain,
              0.0, 30.0,
              QStringLiteral("CFC-G"),
              QColor(Qt::white),
              /*redThreshold=*/0.0);
}

void BarPresetItem::configureAsLeveler()
{
    // From ItemGroup::createLevelerBarPreset — ItemGroup.cpp:956
    // (chain-cites Thetis AddLevelerBar:23179+)
    setCommon(Kind::Leveler,
              MeterBinding::TxLeveler,
              -30.0, 12.0,
              QStringLiteral("LEV"),
              QColor(Qt::white),
              /*redThreshold=*/0.0);
}

void BarPresetItem::configureAsLevelerGain()
{
    // From ItemGroup::createLevelerGainBarPreset — ItemGroup.cpp:965
    // (chain-cites Thetis AddLevelerGainBar:23265+)
    setCommon(Kind::LevelerGain,
              MeterBinding::TxLevelerGain,
              0.0, 30.0,
              QStringLiteral("LEV-G"),
              QColor(Qt::white),
              /*redThreshold=*/0.0);
}

void BarPresetItem::configureAsAgc()
{
    // From ItemGroup::createAgcBarPreset — ItemGroup.cpp:921
    // (Thetis renderScale AGC_AV: generalScale(.., -125, 125, 25, 25))
    setCommon(Kind::Agc,
              MeterBinding::AgcAvg,
              -125.0, 125.0,
              QStringLiteral("AGC"),
              QColor(Qt::white),
              /*redThreshold=*/0.0);
}

void BarPresetItem::configureAsAgcGain()
{
    // From ItemGroup::createAgcGainBarPreset — ItemGroup.cpp:930
    // (Thetis renderScale AGC_GAIN: generalScale(.., -50, 125, 25, 25))
    setCommon(Kind::AgcGain,
              MeterBinding::AgcGain,
              -50.0, 125.0,
              QStringLiteral("AGC-G"),
              QColor(Qt::white),
              /*redThreshold=*/0.0);
}

void BarPresetItem::configureAsPbsnr()
{
    // From ItemGroup::createPbsnrBarPreset — ItemGroup.cpp:939
    setCommon(Kind::Pbsnr,
              MeterBinding::PbSnr,
              0.0, 60.0,
              QStringLiteral("PBSNR"),
              QColor(Qt::white),
              /*redThreshold=*/0.0);
}

void BarPresetItem::configureAsEq()
{
    // From ItemGroup::createEqBarPreset — ItemGroup.cpp:947
    // (chain-cites Thetis AddEQBar:23091+)
    setCommon(Kind::Eq,
              MeterBinding::TxEq,
              -30.0, 12.0,
              QStringLiteral("EQ"),
              QColor(Qt::white),
              /*redThreshold=*/0.0);
}

void BarPresetItem::configureAsSignalBar()
{
    // From ItemGroup::createSignalBarPreset — ItemGroup.cpp:876
    setCommon(Kind::SignalBar,
              MeterBinding::SignalPeak,
              -140.0, 0.0,
              QStringLiteral("Signal"),
              QColor(Qt::white),
              /*redThreshold=*/0.0);
}

void BarPresetItem::configureAsAvgSignalBar()
{
    // From ItemGroup::createAvgSignalBarPreset — ItemGroup.cpp:887
    setCommon(Kind::AvgSignalBar,
              MeterBinding::SignalAvg,
              -140.0, 0.0,
              QStringLiteral("Signal Avg"),
              QColor(Qt::white),
              /*redThreshold=*/0.0);
}

void BarPresetItem::configureAsMaxBinBar()
{
    // From ItemGroup::createMaxBinBarPreset — ItemGroup.cpp:895
    setCommon(Kind::MaxBinBar,
              MeterBinding::SignalMaxBin,
              -140.0, 0.0,
              QStringLiteral("Max Bin"),
              QColor(Qt::white),
              /*redThreshold=*/0.0);
}

void BarPresetItem::configureAsAdcBar()
{
    // From ItemGroup::createAdcBarPreset — ItemGroup.cpp:903
    // (chain-cites Thetis AddADCBar:21740+)
    setCommon(Kind::AdcBar,
              MeterBinding::AdcAvg,
              -140.0, 0.0,
              QStringLiteral("ADC"),
              QColor(Qt::white),
              /*redThreshold=*/0.0);
}

void BarPresetItem::configureAsAdcMaxMag()
{
    // From ItemGroup::createAdcMaxMagPreset — ItemGroup.cpp:912
    // (chain-cites Thetis AddADCMaxMag:21638-21640)
    setCommon(Kind::AdcMaxMag,
              MeterBinding::AdcPeak,
              0.0, 32768.0,
              QStringLiteral("ADC Max"),
              QColor(Qt::white),
              /*redThreshold=*/25000.0);
}

// -------- Custom caller-parameterised flavour --------
void BarPresetItem::configureAsCustom(int bindingId, double minV,
                                      double maxV, const QString& label)
{
    // From ItemGroup::createCustomBarPreset — ItemGroup.cpp:1010
    // Pass-through: no Thetis provenance.
    setCommon(Kind::Custom,
              bindingId,
              minV, maxV,
              label,
              QColor(Qt::white),
              /*redThreshold=*/0.0);
}

// =================================================================
// Enum <-> string
// =================================================================
QString BarPresetItem::kindString() const
{
    switch (m_kind) {
    case Kind::Mic:          return QStringLiteral("Mic");
    case Kind::Alc:          return QStringLiteral("Alc");
    case Kind::AlcGain:      return QStringLiteral("AlcGain");
    case Kind::AlcGroup:     return QStringLiteral("AlcGroup");
    case Kind::Comp:         return QStringLiteral("Comp");
    case Kind::Cfc:          return QStringLiteral("Cfc");
    case Kind::CfcGain:      return QStringLiteral("CfcGain");
    case Kind::Leveler:      return QStringLiteral("Leveler");
    case Kind::LevelerGain:  return QStringLiteral("LevelerGain");
    case Kind::Agc:          return QStringLiteral("Agc");
    case Kind::AgcGain:      return QStringLiteral("AgcGain");
    case Kind::Pbsnr:        return QStringLiteral("Pbsnr");
    case Kind::Eq:           return QStringLiteral("Eq");
    case Kind::SignalBar:    return QStringLiteral("SignalBar");
    case Kind::AvgSignalBar: return QStringLiteral("AvgSignalBar");
    case Kind::MaxBinBar:    return QStringLiteral("MaxBinBar");
    case Kind::AdcBar:       return QStringLiteral("AdcBar");
    case Kind::AdcMaxMag:    return QStringLiteral("AdcMaxMag");
    case Kind::Custom:       return QStringLiteral("Custom");
    }
    return QStringLiteral("Custom");
}

BarPresetItem::Kind BarPresetItem::kindFromString(const QString& s,
                                                   Kind fallback)
{
    if (s == QLatin1String("Mic"))          { return Kind::Mic; }
    if (s == QLatin1String("Alc"))          { return Kind::Alc; }
    if (s == QLatin1String("AlcGain"))      { return Kind::AlcGain; }
    if (s == QLatin1String("AlcGroup"))     { return Kind::AlcGroup; }
    if (s == QLatin1String("Comp"))         { return Kind::Comp; }
    if (s == QLatin1String("Cfc"))          { return Kind::Cfc; }
    if (s == QLatin1String("CfcGain"))      { return Kind::CfcGain; }
    if (s == QLatin1String("Leveler"))      { return Kind::Leveler; }
    if (s == QLatin1String("LevelerGain"))  { return Kind::LevelerGain; }
    if (s == QLatin1String("Agc"))          { return Kind::Agc; }
    if (s == QLatin1String("AgcGain"))      { return Kind::AgcGain; }
    if (s == QLatin1String("Pbsnr"))        { return Kind::Pbsnr; }
    if (s == QLatin1String("Eq"))           { return Kind::Eq; }
    if (s == QLatin1String("SignalBar"))    { return Kind::SignalBar; }
    if (s == QLatin1String("AvgSignalBar")) { return Kind::AvgSignalBar; }
    if (s == QLatin1String("MaxBinBar"))    { return Kind::MaxBinBar; }
    if (s == QLatin1String("AdcBar"))       { return Kind::AdcBar; }
    if (s == QLatin1String("AdcMaxMag"))    { return Kind::AdcMaxMag; }
    if (s == QLatin1String("Custom"))       { return Kind::Custom; }
    return fallback;
}

// =================================================================
// paint() — simple bar-row render. Layout mirrors buildBarRow():
//   top 45% strip — label
//   bottom 55%    — horizontal bar + red-threshold zone
// =================================================================
void BarPresetItem::paint(QPainter& p, int widgetW, int widgetH)
{
    const QRect rect = pixelRect(widgetW, widgetH);
    if (rect.width() <= 0 || rect.height() <= 0) {
        return;
    }

    p.save();
    p.fillRect(rect, m_backdropColor);

    // Label in the top 45% strip.
    const QRect titleRect(rect.x(), rect.y(), rect.width(),
                          rect.height() * 45 / 100);
    p.setPen(QColor(Qt::red));  // Thetis meter-title colour (buildBarRow)
    QFont f = p.font();
    f.setBold(true);
    p.setFont(f);
    p.drawText(titleRect, Qt::AlignCenter, m_label);

    // Bar region in the bottom 55%.
    const QRect barRect(rect.x() + rect.width() * 2 / 100,
                        rect.y() + rect.height() * 45 / 100,
                        rect.width() * 96 / 100,
                        rect.height() * 50 / 100);

    // Seed marker position at minValue for a stable smoke-paint path.
    // Thetis seeds BarItem.Value to the first calibration key (the
    // minimum); NereusSDR mirrors that seed for the preview render.
    const double span = qMax(m_maxValue - m_minValue, 1e-6);
    const double seed = m_minValue;
    const float  normX = static_cast<float>((seed - m_minValue) / span);

    // Draw the bar baseline (line-style, matching BarStyle::Line).
    const int baseY = barRect.y() + barRect.height() / 2;
    QPen pen(m_barColor);
    pen.setWidthF(qMax(1.0, barRect.height() * 0.08));
    p.setPen(pen);
    p.drawLine(barRect.x(), baseY,
               barRect.x() + static_cast<int>(barRect.width() * normX),
               baseY);

    // Optional red zone above redThreshold. Matches the Thetis
    // convention (see BarItem::setRedThreshold in buildBarRow).
    if (m_redThreshold > m_minValue && m_redThreshold < m_maxValue) {
        const float normR =
            static_cast<float>((m_redThreshold - m_minValue) / span);
        const int rX = barRect.x() + static_cast<int>(barRect.width() * normR);
        p.setPen(QPen(QColor(Qt::red), pen.widthF()));
        p.drawLine(rX, barRect.y(), rX, barRect.y() + barRect.height());
    }
    p.restore();
}

// =================================================================
// Serialization — flavour + overrides as compact JSON.
// =================================================================
QString BarPresetItem::serialize() const
{
    QJsonObject o;
    o.insert(QStringLiteral("kind"),          QStringLiteral("BarPreset"));
    o.insert(QStringLiteral("flavor"),        kindString());
    o.insert(QStringLiteral("x"),             static_cast<double>(m_x));
    o.insert(QStringLiteral("y"),             static_cast<double>(m_y));
    o.insert(QStringLiteral("w"),             static_cast<double>(m_w));
    o.insert(QStringLiteral("h"),             static_cast<double>(m_h));
    o.insert(QStringLiteral("bindingId"),     bindingId());
    o.insert(QStringLiteral("minValue"),      m_minValue);
    o.insert(QStringLiteral("maxValue"),      m_maxValue);
    o.insert(QStringLiteral("label"),         m_label);
    o.insert(QStringLiteral("barColor"),      ColorSwatchButton::colorToHex(m_barColor));
    o.insert(QStringLiteral("backdropColor"), ColorSwatchButton::colorToHex(m_backdropColor));
    o.insert(QStringLiteral("redThreshold"),  m_redThreshold);
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

bool BarPresetItem::deserialize(const QString& data)
{
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }
    const QJsonObject o = doc.object();
    if (o.value(QStringLiteral("kind")).toString() != QStringLiteral("BarPreset")) {
        return false;
    }

    // Flavour first — applies canonical defaults; any subsequent
    // blob fields override.
    const QString flavor = o.value(QStringLiteral("flavor")).toString();
    const Kind k = kindFromString(flavor, m_kind);
    switch (k) {
    case Kind::Mic:          configureAsMic();          break;
    case Kind::Alc:          configureAsAlc();          break;
    case Kind::AlcGain:      configureAsAlcGain();      break;
    case Kind::AlcGroup:     configureAsAlcGroup();     break;
    case Kind::Comp:         configureAsComp();         break;
    case Kind::Cfc:          configureAsCfc();          break;
    case Kind::CfcGain:      configureAsCfcGain();      break;
    case Kind::Leveler:      configureAsLeveler();      break;
    case Kind::LevelerGain:  configureAsLevelerGain();  break;
    case Kind::Agc:          configureAsAgc();          break;
    case Kind::AgcGain:      configureAsAgcGain();      break;
    case Kind::Pbsnr:        configureAsPbsnr();        break;
    case Kind::Eq:           configureAsEq();           break;
    case Kind::SignalBar:    configureAsSignalBar();    break;
    case Kind::AvgSignalBar: configureAsAvgSignalBar(); break;
    case Kind::MaxBinBar:    configureAsMaxBinBar();    break;
    case Kind::AdcBar:       configureAsAdcBar();       break;
    case Kind::AdcMaxMag:    configureAsAdcMaxMag();    break;
    case Kind::Custom:
        // Custom has no intrinsic defaults; let the blob fields
        // populate binding/min/max/label below.
        m_kind = Kind::Custom;
        break;
    }

    setRect(static_cast<float>(o.value(QStringLiteral("x")).toDouble(m_x)),
            static_cast<float>(o.value(QStringLiteral("y")).toDouble(m_y)),
            static_cast<float>(o.value(QStringLiteral("w")).toDouble(m_w)),
            static_cast<float>(o.value(QStringLiteral("h")).toDouble(m_h)));

    setBindingId(o.value(QStringLiteral("bindingId")).toInt(bindingId()));
    m_minValue     = o.value(QStringLiteral("minValue")).toDouble(m_minValue);
    m_maxValue     = o.value(QStringLiteral("maxValue")).toDouble(m_maxValue);
    m_label        = o.value(QStringLiteral("label")).toString(m_label);
    m_redThreshold = o.value(QStringLiteral("redThreshold")).toDouble(m_redThreshold);

    auto loadColor = [&](const QString& key, QColor& dst) {
        const QString hex = o.value(key).toString();
        if (hex.isEmpty()) { return; }
        const QColor c = ColorSwatchButton::colorFromHex(hex);
        if (c.isValid()) { dst = c; }
    };
    loadColor(QStringLiteral("barColor"),      m_barColor);
    loadColor(QStringLiteral("backdropColor"), m_backdropColor);
    return true;
}

} // namespace NereusSDR
