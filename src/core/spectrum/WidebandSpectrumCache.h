// =================================================================
// src/core/spectrum/WidebandSpectrumCache.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original. Latest-frame store for physical ADC
// wideband spectra. Source identity belongs to Core capture lifecycle; this
// class deliberately has no hardware, clock, GUI, or freshness policy.
// =================================================================
#pragma once

#include <QVector>
#include <QtGlobal>

#include <array>
#include <optional>

namespace NereusSDR {

struct WidebandCaptureIdentity {
    quint64 connectionGeneration {0};
    quint64 captureGeneration {0};
    quint64 geometryGeneration {0};

    bool operator==(const WidebandCaptureIdentity&) const = default;
};

struct WidebandSourceDescriptor {
    int physicalAdcIndex {-1};
    quint32 sourceGeneration {0};
    double adcRateHz {0};
    WidebandCaptureIdentity identity;

    bool operator==(const WidebandSourceDescriptor&) const = default;
};

struct WidebandSpectrumFrame {
    WidebandSourceDescriptor source;
    qint64 producedAtNs {0};
    QVector<float> rawDbBins;
};

/// Holds only the latest valid raw wideband FFT row for each physical ADC.
/// Consumers decide whether a valid row is too old for their use case.
class WidebandSpectrumCache
{
public:
    static constexpr int kMaxSources = 2;

    std::optional<WidebandSourceDescriptor> configureSource(
        int adc, const WidebandCaptureIdentity& identity, double adcRateHz);
    bool publish(const WidebandSpectrumFrame& frame);
    std::optional<WidebandSourceDescriptor> source(int adc) const;
    std::optional<WidebandSpectrumFrame> latest(int adc) const;
    void invalidate(int adc);
    void clear();

private:
    struct SourceState {
        std::optional<WidebandSourceDescriptor> descriptor;
        std::optional<WidebandSpectrumFrame> latest;
    };

    static bool validAdc(int adc);
    static bool validIdentity(const WidebandCaptureIdentity& identity);
    static bool validRate(double adcRateHz);
    quint32 nextSourceGeneration();

    std::array<SourceState, kMaxSources> m_sources;
    quint32 m_lastSourceGeneration {0};
};

} // namespace NereusSDR
