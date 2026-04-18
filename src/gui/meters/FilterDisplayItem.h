#pragma once

// =================================================================
// src/gui/meters/FilterDisplayItem.h  (NereusSDR)
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
#include <QImage>
#include <vector>

namespace NereusSDR {

// From Thetis clsFilterItem (MeterManager.cs:16852+)
// Mini passband spectrum/waterfall display with filter edge markers.
class FilterDisplayItem : public MeterItem {
    Q_OBJECT

public:
    // From Thetis FIDisplayMode enum (MeterManager.cs:16865)
    enum class DisplayMode { Panadapter, Waterfall, Panafall, None };

    // From Thetis FIWaterfallPalette enum
    enum class WaterfallPalette { Enhanced, Spectran, BlackWhite, LinLog, LinRad, LinAuto, Custom };

    static constexpr int kSpectrumPixels = 512; // From Thetis MiniSpec.PIXELS

    explicit FilterDisplayItem(QObject* parent = nullptr);

    void setDisplayMode(DisplayMode m) { m_displayMode = m; }
    DisplayMode displayMode() const { return m_displayMode; }

    // Spectrum data feed (called externally, e.g., by FFTEngine)
    void setSpectrumData(const float* bins, int count);

    // Filter edges (in pixels, 0-511 range)
    void setFilterEdgesRx(int low, int high) { m_rxLow = low; m_rxHigh = high; }
    void setFilterEdgesTx(int low, int high) { m_txLow = low; m_txHigh = high; }

    // Notch positions (in pixels)
    void setNotchPositions(const std::vector<int>& positions) { m_notchPositions = positions; }

    // Colors
    void setDataLineColour(const QColor& c) { m_dataLineColour = c; }
    QColor dataLineColour() const { return m_dataLineColour; }
    void setDataFillColour(const QColor& c) { m_dataFillColour = c; }
    QColor dataFillColour() const { return m_dataFillColour; }
    void setEdgesColourRX(const QColor& c) { m_edgesColourRX = c; }
    QColor edgesColourRX() const { return m_edgesColourRX; }
    void setEdgesColourTX(const QColor& c) { m_edgesColourTX = c; }
    QColor edgesColourTX() const { return m_edgesColourTX; }
    void setNotchColour(const QColor& c) { m_notchColour = c; }
    QColor notchColour() const { return m_notchColour; }
    void setMeterBackColour(const QColor& c) { m_meterBackColour = c; }
    QColor meterBackColour() const { return m_meterBackColour; }
    void setTextColour(const QColor& c) { m_textColour = c; }
    QColor textColour() const { return m_textColour; }

    void setFillSpectrum(bool f) { m_fillSpectrum = f; }
    bool fillSpectrum() const { return m_fillSpectrum; }
    void setPadding(float p) { m_padding = p; }
    float padding() const { return m_padding; }
    void setWaterfallPalette(WaterfallPalette pal) { m_waterfallPalette = pal; }
    WaterfallPalette waterfallPalette() const { return m_waterfallPalette; }

    // Spectrum range
    void setSpecGridRange(float minDb, float maxDb) { m_specMinDb = minDb; m_specMaxDb = maxDb; }
    float specMinDb() const { return m_specMinDb; }
    float specMaxDb() const { return m_specMaxDb; }

    Layer renderLayer() const override { return Layer::OverlayDynamic; }
    void paint(QPainter& p, int widgetW, int widgetH) override;
    QString serialize() const override;
    bool deserialize(const QString& data) override;

private:
    void paintSpectrum(QPainter& p, const QRect& rect);
    void paintWaterfall(QPainter& p, const QRect& rect);
    void paintFilterEdges(QPainter& p, const QRect& rect);
    void paintNotches(QPainter& p, const QRect& rect);
    QColor dbToWaterfallColor(float db) const;

    DisplayMode m_displayMode{DisplayMode::Panafall};
    WaterfallPalette m_waterfallPalette{WaterfallPalette::Enhanced};

    // Spectrum data (512 bins)
    std::vector<float> m_spectrumData;

    // Waterfall: rolling image, shift down 1 row per frame
    QImage m_waterfallImage;
    int m_waterfallFrameCount{0};
    int m_waterfallFrameInterval{4}; // update every Nth frame

    // Filter edges (pixel positions 0-511)
    int m_rxLow{100};
    int m_rxHigh{400};
    int m_txLow{-1};  // -1 = don't show
    int m_txHigh{-1};

    // Notch positions
    std::vector<int> m_notchPositions;

    // Colors (from Thetis clsFilterItem defaults MeterManager.cs:17012+)
    QColor m_dataLineColour{0x32, 0xcd, 0x32};       // LimeGreen
    QColor m_dataFillColour{0x32, 0xcd, 0x32, 0x40}; // LimeGreen alpha
    QColor m_edgesColourRX{0xff, 0xff, 0x00};         // Yellow
    QColor m_edgesColourTX{0xff, 0x00, 0x00};         // Red
    QColor m_notchColour{0xff, 0xa5, 0x00};           // Orange
    QColor m_meterBackColour{0x00, 0x00, 0x00};       // Black
    QColor m_textColour{0xff, 0xff, 0xff};             // White
    bool   m_fillSpectrum{true};
    float  m_padding{0.02f};
    float  m_specMinDb{-140.0f};
    float  m_specMaxDb{-40.0f};
};

} // namespace NereusSDR
