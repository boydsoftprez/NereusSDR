// =================================================================
// src/core/spectrum/WidebandSpectrumCache.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original. See WidebandSpectrumCache.h.
// =================================================================

#include "core/spectrum/WidebandSpectrumCache.h"

#include "core/WidebandFftEngine.h"

#include <cmath>
#include <limits>

namespace NereusSDR {

bool WidebandSpectrumCache::validAdc(int adc)
{
    return adc >= 0 && adc < kMaxSources;
}

bool WidebandSpectrumCache::validIdentity(const WidebandCaptureIdentity& identity)
{
    return identity.connectionGeneration != 0
        && identity.captureGeneration != 0
        && identity.geometryGeneration != 0;
}

bool WidebandSpectrumCache::validRate(double adcRateHz)
{
    const double nyquistHz = adcRateHz / 2.0;
    return std::isfinite(adcRateHz) && adcRateHz > 0.0
        && std::isfinite(nyquistHz) && nyquistHz > 0.0;
}

quint32 WidebandSpectrumCache::nextSourceGeneration()
{
    // One global counter makes a descriptor change unambiguous across both
    // ADC slots. Zero remains the unavailable-context sentinel.
    if (m_lastSourceGeneration == std::numeric_limits<quint32>::max()) {
        // A uint32 source generation cannot remain monotonic after wrap. This
        // cache has no valid descriptor left to mint in that impossible normal
        // lifetime; callers must rebuild its surrounding capture context.
        return 0;
    }
    return ++m_lastSourceGeneration;
}

std::optional<WidebandSourceDescriptor> WidebandSpectrumCache::configureSource(
    int adc, const WidebandCaptureIdentity& identity, double adcRateHz)
{
    if (!validAdc(adc)) {
        return std::nullopt;
    }

    SourceState& state = m_sources[adc];
    if (!validIdentity(identity) || !validRate(adcRateHz)) {
        state.descriptor.reset();
        state.latest.reset();
        return std::nullopt;
    }

    if (state.descriptor
        && state.descriptor->physicalAdcIndex == adc
        && state.descriptor->identity == identity
        && state.descriptor->adcRateHz == adcRateHz) {
        return state.descriptor;
    }

    const quint32 sourceGeneration = nextSourceGeneration();
    if (sourceGeneration == 0) {
        state.descriptor.reset();
        state.latest.reset();
        return std::nullopt;
    }

    state.descriptor = WidebandSourceDescriptor {
        .physicalAdcIndex = adc,
        .sourceGeneration = sourceGeneration,
        .adcRateHz = adcRateHz,
        .identity = identity,
    };
    state.latest.reset();
    return state.descriptor;
}

bool WidebandSpectrumCache::publish(const WidebandSpectrumFrame& frame)
{
    const int adc = frame.source.physicalAdcIndex;
    if (!validAdc(adc) || frame.producedAtNs < 0
        || frame.rawDbBins.size() != WidebandFftEngine::kOutputBins) {
        return false;
    }

    const SourceState& state = m_sources[adc];
    if (!state.descriptor || frame.source != *state.descriptor) {
        return false;
    }
    for (float db : frame.rawDbBins) {
        if (!std::isfinite(db)) {
            return false;
        }
    }
    if (state.latest && frame.producedAtNs < state.latest->producedAtNs) {
        return false;
    }

    m_sources[adc].latest = frame;
    return true;
}

std::optional<WidebandSourceDescriptor> WidebandSpectrumCache::source(int adc) const
{
    if (!validAdc(adc)) {
        return std::nullopt;
    }
    return m_sources[adc].descriptor;
}

std::optional<WidebandSpectrumFrame> WidebandSpectrumCache::latest(int adc) const
{
    if (!validAdc(adc)) {
        return std::nullopt;
    }
    return m_sources[adc].latest;
}

void WidebandSpectrumCache::invalidate(int adc)
{
    if (!validAdc(adc)) {
        return;
    }
    m_sources[adc].descriptor.reset();
    m_sources[adc].latest.reset();
}

void WidebandSpectrumCache::clear()
{
    for (SourceState& state : m_sources) {
        state.descriptor.reset();
        state.latest.reset();
    }
}

} // namespace NereusSDR
