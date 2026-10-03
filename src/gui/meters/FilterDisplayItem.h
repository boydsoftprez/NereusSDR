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
//   2026-09-26 - R-R3-49 (remote-window parity Task 16): the
//                 high-resolution curve can come from a Core's bins
//                 (setFilterResponseBins) in a remote window, resampled as
//                 the local channel's is. J.J. Boyd (KG4VCF), AI-assisted
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
#include <QImage>
#include <QVector>
#include <vector>

namespace NereusSDR {

class RxChannel;

// From Thetis clsFilterItem (MeterManager.cs:16852+)
// Mini passband spectrum/waterfall display with filter edge markers.
class FilterDisplayItem : public MeterItem {
    Q_OBJECT

public:
    // From Thetis FIDisplayMode enum (MeterManager.cs:16865)
    enum class DisplayMode { Panadapter, Waterfall, Panafall, None };

    // From Thetis FIWaterfallPalette enum
    enum class WaterfallPalette { Enhanced, Spectran, BlackWhite, LinLog, LinRad, LinAuto, Custom };

    // From Thetis MeterManager.cs:43362 [v2.10.3.15] MiniSpec.PIXELS.
    static constexpr int kSpectrumPixels = 1024;

    explicit FilterDisplayItem(QObject* parent = nullptr);

    void setDisplayMode(DisplayMode m) {
        if (m_displayMode == m) { return; }
        m_displayMode = m;
        emit displayModeChanged();
    }
    DisplayMode displayMode() const { return m_displayMode; }

    // Spectrum data feed (called externally, e.g., by FFTEngine)
    void setSpectrumData(const float* bins, int count);
    // A delivered analyzer frame owns its RF geometry and accepted count.
    // Waterfall history advances on a delivered row, never on a paint.
    void presentFrame(const QVector<float>& traceDbm,
                      const QVector<float>& waterfallDbm,
                      double centreHz, double spanHz, bool transmit,
                      bool waterfallAdvance);
    void clearFrame();
    bool frameAvailable() const { return m_frameAvailable; }
    int framePixels() const { return int(m_spectrumData.size()); }
    int tracePeakPixelForTest() const;
    double frameCentreHz() const { return m_frameCentreHz; }
    double frameSpanHz() const { return m_frameSpanHz; }
    bool frameTransmitting() const { return m_frameTransmitting; }
    const QImage& waterfallImageForTest() const { return m_waterfallImage; }
    int rxLowPixelForTest() const { return m_rxLow; }
    int rxHighPixelForTest() const { return m_rxHigh; }

    // Filter edges (in accepted-frame pixels)
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
    void setWaterfallLowColour(const QColor& color) { m_waterfallLowColour = color; }
    void setCustomWaterfallGradient(const QVector<QColor>& rx,
                                    const QVector<QColor>& tx) {
        m_customRxGradient = rx;
        m_customTxGradient = tx;
    }

    // Spectrum range
    void setSpecGridRange(float minDb, float maxDb) { m_specMinDb = minDb; m_specMaxDb = maxDb; }
    float specMinDb() const { return m_specMinDb; }
    float specMaxDb() const { return m_specMaxDb; }

    // ── High-resolution filter characteristics (Task 4.4) ─────────────────────
    // When enabled, paintFilterEdges overlays the actual computed FIR magnitude
    // curve from RxChannel::filterResponseMagnitudes() in addition to the edge
    // markers.  The change takes effect on the next paint frame.
    void setHighResolution(bool h);
    bool highResolution() const { return m_highResolution; }

    // Bind the RxChannel whose filterResponseMagnitudes() provides the FIR curve.
    // Non-owning — caller is responsible for clearing before the channel is destroyed.
    void bindRxChannel(RxChannel* channel);
    // R-R3-49 (parity Task 16): a remote window has no channel; it sets the
    // Core's filter response bins (magnitudes, DC to Nyquist) here, and the
    // curve is resampled from them as RxChannel::filterResponseMagnitudes
    // resamples its own. Used only while no channel is bound. Empty: none.
    void setFilterResponseBins(const QVector<double>& binMagnitudes)
    { m_filterResponseBins = binMagnitudes; }
    void setRfFilterResponse(const QVector<double>& magnitudesDb,
                             double startHz, double stepHz, double referenceHz);
    void setRfMarkers(double rxLowHz, double rxHighHz, double txLowHz,
                      double txHighHz, const QVector<double>& notchCentresHz);
    const QVector<double>& filterResponseBinsForTest() const { return m_filterResponseBins; }

    Layer renderLayer() const override { return Layer::OverlayDynamic; }
    void paint(QPainter& p, int widgetW, int widgetH) override;
    QString serialize() const override;
    bool deserialize(const QString& data) override;

signals:
    void displayModeChanged();

private:
    void paintSpectrum(QPainter& p, const QRect& rect);
    void paintWaterfall(QPainter& p, const QRect& rect);
    void paintFilterEdges(QPainter& p, const QRect& rect);
    // High-res FIR magnitude curve overlay (Task 4.4, design Section 4D).
    // Called from paintFilterEdges() when m_highResolution, with a channel
    // or (parity Task 16) the Core's bins.
    void paintHighResolutionFilterCurve(QPainter& p, const QRect& rect);
    void paintNotches(QPainter& p, const QRect& rect);
    QColor dbToWaterfallColor(float db) const;

    DisplayMode m_displayMode{DisplayMode::Panafall};
    WaterfallPalette m_waterfallPalette{WaterfallPalette::Enhanced};

    // Spectrum data in the accepted RF window.
    std::vector<float> m_spectrumData;
    bool m_frameAvailable{false};
    bool m_frameTransmitting{false};
    double m_frameCentreHz{0.0};
    double m_frameSpanHz{0.0};

    // Waterfall: rolling image, shift down 1 row per frame
    QImage m_waterfallImage;
    int m_waterfallFrameCount{0};
    int m_waterfallFrameInterval{4}; // update every Nth frame

    // Filter edges (pixel positions 0-511)
    int m_rxLow{-1};
    int m_rxHigh{-1};
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
    float  m_autoWaterfallMinDb{-140.0f};
    float  m_autoWaterfallMaxDb{-40.0f};
    QColor m_waterfallLowColour{Qt::black};
    QVector<QColor> m_customRxGradient;
    QVector<QColor> m_customTxGradient;

    // High-resolution filter characteristics (Task 4.4)
    bool      m_highResolution{false};
    RxChannel* m_rxChannel{nullptr};  // non-owning
    QVector<double> m_filterResponseBins;  // parity Task 16: the Core's
    QVector<double> m_rfFilterResponseDb;
    double m_rfFilterStartHz{0.0};
    double m_rfFilterStepHz{0.0};
    double m_rfFilterReferenceHz{0.0};
};

} // namespace NereusSDR
