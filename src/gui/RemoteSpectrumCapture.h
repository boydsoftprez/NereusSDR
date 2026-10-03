#pragma once
// =================================================================
// src/gui/RemoteSpectrumCapture.h (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original. Immutable GUI-side capture metadata
// carried alongside decoded rows through delayed presentation and folding.
// No wire fields are added. An unavailable DDC sourceGeneration stays zero.
// Modification history (NereusSDR):
//   2026-10-02: J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
// =================================================================

#include <memory>

namespace NereusSDR {

struct SpectrumEndpointContext;

class RemoteSpectrumCapture {
public:
    RemoteSpectrumCapture() = default;
    RemoteSpectrumCapture(const SpectrumEndpointContext& context,
                          double sourceCentreHz, double sourceSampleRateHz);
    const SpectrumEndpointContext& context() const;
    bool operator==(const RemoteSpectrumCapture& other) const;

    double sourceCentreHz{0.0};
    double sourceSampleRateHz{0.0};

private:
    // Keeping the context opaque here preserves the widget header's Core
    // forward-declaration boundary. Every queue copy shares a frozen value.
    std::shared_ptr<const SpectrumEndpointContext> m_context;
};

} // namespace NereusSDR
