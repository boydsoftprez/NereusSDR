// =================================================================
// src/core/spectrum/ExtendedSpectrumReducer.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/wbDisplay.cs:2106, 4680-4711
//   [v2.10.3.15 @3759d09] -- raw-dB display scale, positive-frequency
//   real-FFT geometry, and peak wideband display reduction.
//
// This is a literal Core extraction of SpectrumWidget::updateSpectrumLinear,
// listenableIslandPixels, and fillWidebandWings. The existing local comments
// and citations describe why the composed row goes detector -> wings -> one
// independent SpectrumAvenger. WidebandDisplayReference owns the existing
// wing-to-DDC reference; this file adds no DSP formula or ADC calibration.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-22  J.J. Boyd / KG4VCF  Remote daemon R3, Core extraction.
//                                    AI-assisted transformation via
//                                    OpenAI Codex.
// =================================================================

//=================================================================
// pandisplay.cs
//=================================================================
// PowerSDR is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2016 Doug Wigley (W5WC)
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
//=================================================================
//
// Waterfall AGC Modifications Copyright (C) 2013 Phil Harman (VK6APH)
//
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

#include "core/spectrum/ExtendedSpectrumReducer.h"

#include "core/FFTEngine.h"
#include "core/WidebandFftEngine.h"
#include "core/spectrum/SpectrumDetector.h"
#include "core/spectrum/WidebandDisplayReference.h"

#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <limits>

namespace NereusSDR {
namespace {

constexpr int kMaximumPixels = 4096;

bool isSupportedDetector(SpectrumDetectorMode detector)
{
    const int value = static_cast<int>(detector);
    return value >= static_cast<int>(SpectrumDetectorMode::Peak)
        && value < static_cast<int>(SpectrumDetectorMode::Count);
}

bool isFinitePositive(double value)
{
    return std::isfinite(value) && value > 0.0;
}

bool validConfig(const ReducerConfig& cfg)
{
    if (cfg.pixels <= 0 || cfg.pixels > kMaximumPixels
        || !std::isfinite(cfg.centreHz)
        || !std::isfinite(cfg.streamCentreHz)
        || !isFinitePositive(cfg.spanHz)
        || !isFinitePositive(cfg.sampleRateHz)
        || !isSupportedDetector(cfg.detector)
        || cfg.averageMode < -1 || cfg.averageMode > 3
        || !std::isfinite(cfg.averageAlpha)
        || cfg.averageAlpha < 0.0 || cfg.averageAlpha > 1.0) {
        return false;
    }

    // These are not merely a no-island condition. They are the display and
    // clipped-DDC edges used below, so a finite request that overflows one of
    // them is malformed before any floating-point-to-integer conversion.
    const double displayHalfSpan = cfg.spanHz / 2.0;
    const double ddcHalfSpan = cfg.sampleRateHz * (0.5 - ExtendedSpectrumReducer::kDdcClipFraction);
    const double displayLowHz = cfg.centreHz - displayHalfSpan;
    const double displayHighHz = cfg.centreHz + displayHalfSpan;
    const double ddcLowHz = cfg.streamCentreHz - ddcHalfSpan;
    const double ddcHighHz = cfg.streamCentreHz + ddcHalfSpan;
    const double hzPerPixel = cfg.spanHz / static_cast<double>(cfg.pixels);
    return std::isfinite(displayLowHz) && std::isfinite(displayHighHz)
        && std::isfinite(ddcLowHz) && std::isfinite(ddcHighHz)
        && isFinitePositive(hzPerPixel);
}

bool isSafeLinearDb(double db, double scaleDb, double& linear)
{
    const double exponent = (db - scaleDb) / 10.0;
    if (!std::isfinite(exponent)) {
        return false;
    }
    linear = std::pow(10.0, exponent);
    return std::isfinite(linear) && linear > 0.0
        && linear <= std::numeric_limits<float>::max()
        && static_cast<float>(linear) > 0.0f;
}

bool hasFiniteAvengerPower(double linear, double scale, int averageMode)
{
    // Window averaging may retain up to kMaxAverage linear-power rows.
    double accumulated = linear;
    if (averageMode == 2) {
        accumulated *= SpectrumAvenger::kMaxAverage;
    }
    const double scaled = accumulated * scale;
    return std::isfinite(accumulated) && std::isfinite(scaled) && scaled > 0.0;
}

} // namespace

std::pair<int, int> ExtendedSpectrumReducer::listenableIslandPixels(
    const ReducerConfig& cfg)
{
    if (!validConfig(cfg)) {
        return {0, -1};
    }

    // Transcribed from SpectrumWidget::listenableIslandPixels. Has to agree
    // with the clipped bin range reduce() feeds the detector, or the island's
    // signals would land at the wrong frequencies.
    const double ddcHalfSpanHz = cfg.sampleRateHz * (0.5 - kDdcClipFraction);
    const double displayLowHz = cfg.centreHz - cfg.spanHz / 2.0;
    const double displayHighHz = cfg.centreHz + cfg.spanHz / 2.0;
    const double ddcLowHz = cfg.streamCentreHz - ddcHalfSpanHz;
    const double ddcHighHz = cfg.streamCentreHz + ddcHalfSpanHz;
    const double hzPerPixel = cfg.spanHz / static_cast<double>(cfg.pixels);
    if (!std::isfinite(displayLowHz) || !std::isfinite(displayHighHz)
        || !std::isfinite(ddcLowHz) || !std::isfinite(ddcHighHz)
        || !isFinitePositive(hzPerPixel)) {
        return {0, -1};
    }

    // No overlap at all: the operator has panned far enough into a wing that
    // the DDC is off the window entirely. Say so with an empty span rather
    // than clamping both ends together, which manufactured a one-pixel island
    // at whichever edge was nearest and put DDC bins on a pixel the DDC does
    // not cover. Callers read an empty span as "the wideband plane owns every
    // pixel". Found by Codex on PR #318.
    if (ddcHighHz < displayLowHz || ddcLowHz > displayHighHz) {
        return {0, -1};
    }

    // Clamp in floating point before narrowing: remote requests may be far
    // outside the local numeric range even when finite.
    const double lastPixel = static_cast<double>(cfg.pixels - 1);
    const double firstRaw = std::floor((ddcLowHz - displayLowHz) / hzPerPixel);
    const double lastRaw = std::ceil((ddcHighHz - displayLowHz) / hzPerPixel) - 1.0;
    if (!std::isfinite(firstRaw) || !std::isfinite(lastRaw)) {
        return {0, -1};
    }
    int first = static_cast<int>(std::clamp(firstRaw, 0.0, lastPixel));
    int last = static_cast<int>(std::clamp(lastRaw, 0.0, lastPixel));
    if (last < first) {
        last = first;
    }
    return {first, last};
}

bool ExtendedSpectrumReducer::reduce(const QVector<float>& ddcBinsLinear,
                                     double ddcWindowEnb,
                                     double ddcRawDbmOffset,
                                     const QVector<float>& adcRawDb,
                                     double adcRateHz,
                                     double stationOffsetDb,
                                     double floorDbm,
                                     QVector<float>& out)
{
    // Validate every remotely supplied shape and scalar before resizing any
    // display array or narrowing a derived floating-point index.
    if (!validConfig(m_cfg)
        || ddcBinsLinear.isEmpty()
        || ddcBinsLinear.size() > FFTEngine::maximumFftSize()
        || (!adcRawDb.isEmpty()
            && adcRawDb.size() != WidebandFftEngine::kOutputBins)
        || !std::isfinite(ddcWindowEnb)
        || !isFinitePositive(adcRateHz)
        || !std::isfinite(ddcRawDbmOffset)
        || !std::isfinite(stationOffsetDb)
        || !std::isfinite(floorDbm)) {
        return false;
    }
    for (const float power : ddcBinsLinear) {
        if (!std::isfinite(power) || power < 0.0f) {
            return false;
        }
    }
    for (const float db : adcRawDb) {
        if (!std::isfinite(db)) { return false; }
    }

    const double fftWindowEnb = qMax(ddcWindowEnb, 1e-9);
    const double displayLowHz = m_cfg.centreHz - m_cfg.spanHz / 2.0;
    const double hzPerPixel = m_cfg.spanHz / static_cast<double>(m_cfg.pixels);
    const double adcBinWidthHz = adcRateHz / static_cast<double>(WidebandFftEngine::kFftSize);
    const double ddcBinWidthHz = m_cfg.sampleRateHz
        / static_cast<double>(ddcBinsLinear.size());
    const double combinedOffsetDb = ddcRawDbmOffset + stationOffsetDb;
    const double dbmScale = std::pow(10.0, combinedOffsetDb / 10.0);
    // The avenger may retain a louder prior row than the current input.
    // Bound the scale for every representable detector pixel and its ring
    // before apply(), so rejecting an extreme offset cannot alter history.
    const double maximumHistoryScale = std::numeric_limits<double>::max()
        / static_cast<double>(std::numeric_limits<float>::max())
        / SpectrumAvenger::kMaxAverage;
    const float wingReferenceDb = widebandRelativeReferenceDb(
        adcRateHz, ddcBinWidthHz, fftWindowEnb, m_cfg.detector);
    if (!std::isfinite(displayLowHz) || !isFinitePositive(hzPerPixel)
        || !isFinitePositive(adcBinWidthHz) || !isFinitePositive(ddcBinWidthHz)
        || !std::isfinite(combinedOffsetDb) || !std::isfinite(dbmScale)
        || dbmScale <= 0.0 || dbmScale > maximumHistoryScale
        || !std::isfinite(wingReferenceDb)) {
        return false;
    }

    double floorLinear = 0.0;
    if (!isSafeLinearDb(floorDbm, combinedOffsetDb, floorLinear)
        || !hasFiniteAvengerPower(floorLinear, dbmScale, m_cfg.averageMode)) {
        return false;
    }
    // applySpectrumDetector writes float pixels before SpectrumAvenger applies
    // dbmScale. Average/Sample/RMS also multiply by invEnb. Bound every DDC
    // power for both existing stages before either stateful buffer is touched.
    const bool detectorDividesOutEnb = m_cfg.detector == SpectrumDetectorMode::Average
        || m_cfg.detector == SpectrumDetectorMode::Sample
        || m_cfg.detector == SpectrumDetectorMode::RMS;
    const double detectorMultiplier = detectorDividesOutEnb
        ? 1.0 / fftWindowEnb : 1.0;
    const double largestDetectorPower = std::min(
        static_cast<double>(std::numeric_limits<float>::max()),
        std::numeric_limits<double>::max() / dbmScale
            / (m_cfg.averageMode == 2 ? SpectrumAvenger::kMaxAverage : 1));
    for (const float power : ddcBinsLinear) {
        if (static_cast<double>(power) > largestDetectorPower / detectorMultiplier) {
            return false;
        }
    }

    auto [firstBin, lastBin] = SpectrumReducer::visibleBinRange(
        ddcBinsLinear.size(), m_cfg);
    const int clip = static_cast<int>(std::floor(
        kDdcClipFraction * static_cast<double>(ddcBinsLinear.size())));
    firstBin = std::max(firstBin, clip);
    lastBin = std::min(lastBin, static_cast<int>(ddcBinsLinear.size()) - 1 - clip);
    const int sliceCount = lastBin - firstBin + 1;

    const auto [islandFirstPx, islandLastPx] = listenableIslandPixels(m_cfg);
    const int islandWidth = islandLastPx - islandFirstPx + 1;
    const bool islandVisible = sliceCount > 0 && islandWidth > 0;

    // The avenger turns its input into dBm as 10*log10(linear * scale), so a
    // wing pixel whose raw wideband reference is `db` has to go in as
    // 10^((db - rawDdcOffset)/10). SpectrumAvenger then adds rawDdcOffset and
    // the station scalar exactly once. Missing data is different: floorDbm is
    // already final display dBm, so its conversion above subtracts combinedOffset.
    if (m_linearPixels.size() != m_cfg.pixels) {
        m_linearPixels.resize(m_cfg.pixels);
    }
    for (int x = 0; x < m_cfg.pixels; ++x) {
        if (islandVisible && x >= islandFirstPx && x <= islandLastPx) {
            continue; // island belongs to the DDC plane
        }
        if (adcRawDb.isEmpty()) {
            m_linearPixels[x] = static_cast<float>(floorLinear);
            continue;
        }

        const double pixelLowHz = displayLowHz + x * hzPerPixel;
        const double pixelHighHz = pixelLowHz + hzPerPixel;
        if (!std::isfinite(pixelLowHz) || !std::isfinite(pixelHighHz)) {
            return false;
        }
        const double firstRaw = std::floor(pixelLowHz / adcBinWidthHz) - 1.0;
        const double lastRaw = std::ceil(pixelHighHz / adcBinWidthHz) - 1.0;
        const double lastAdcBin = static_cast<double>(adcRawDb.size() - 1);
        if (!std::isfinite(firstRaw) || !std::isfinite(lastRaw)
            || firstRaw > lastAdcBin || lastRaw < 0.0) {
            m_linearPixels[x] = static_cast<float>(floorLinear);
            continue;
        }
        const int firstAdcBin = static_cast<int>(std::clamp(firstRaw, 0.0, lastAdcBin));
        const int lastAdcBinIndex = static_cast<int>(std::clamp(lastRaw, 0.0, lastAdcBin));
        if (firstAdcBin > lastAdcBinIndex) {
            m_linearPixels[x] = static_cast<float>(floorLinear);
            continue;
        }

        // Peak across the bins under this pixel. Thetis's wideband analyzer
        // is configured for a peak detector too (wbDisplay.cs:4711
        // SetAnalyzer, and its per-pixel loop takes `max`), so a narrow
        // carrier survives decimation instead of averaging away.
        float peakDb = adcRawDb[firstAdcBin];
        for (int bin = firstAdcBin + 1; bin <= lastAdcBinIndex; ++bin) {
            peakDb = std::max(peakDb, adcRawDb[bin]);
        }
        double linear = 0.0;
        if (!isSafeLinearDb(static_cast<double>(peakDb) + wingReferenceDb,
                            ddcRawDbmOffset, linear)
            || !hasFiniteAvengerPower(linear, dbmScale, m_cfg.averageMode)) {
            return false;
        }
        m_linearPixels[x] = static_cast<float>(linear);
    }

    if (islandVisible) {
        const double pixPerBin = static_cast<double>(islandWidth) / sliceCount;
        const double binPerPix = 1.0 / pixPerBin;
        applySpectrumDetector(m_cfg.detector,
                              sliceCount,
                              islandWidth,
                              pixPerBin,
                              binPerPix,
                              ddcBinsLinear.constData() + firstBin,
                              m_linearPixels.data() + islandFirstPx,
                              1.0 / fftWindowEnb,
                              0.0,
                              static_cast<double>(sliceCount),
                              0.0);
    }

    if (m_avenger.numPixels() != m_cfg.pixels) {
        m_avenger.resize(m_cfg.pixels);
    }
    const QVector<double> noCorrection;
    QVector<float> candidate;
    m_avenger.apply(m_linearPixels, m_cfg.averageMode, m_cfg.averageAlpha,
                    dbmScale, noCorrection, false, 0.0, candidate);
    for (const float value : candidate) {
        if (!std::isfinite(value)) {
            return false;
        }
    }
    out = std::move(candidate);
    return true;
}

} // namespace NereusSDR
