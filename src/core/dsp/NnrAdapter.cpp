// no-port-check: NereusSDR-original adapter to the vendored NNR control ABI.
// No DSP algorithm is duplicated; all values come from locked WDSP readback.
// Modification history (NereusSDR):
//   2026-09-21 — J.J. Boyd (KG4VCF), with OpenAI Codex assistance.
//   2026-09-23 : requestLimit and the applied-limit readback (R-R3-40) by
//                J.J. Boyd (KG4VCF), with Anthropic Claude Code assistance.
//   2026-09-24 : the rate refusal in operator words (R-IOS-01) by
//                J.J. Boyd (KG4VCF), with Anthropic Claude Code assistance.
#include "NnrAdapter.h"

#ifdef HAVE_WDSP
#include "nnr_compat.h"
extern "C" void SetRXANNRRun(int channel, int run);

namespace {
NereusSDR::NnrSettings settingsFrom(const NNRConfiguration& value)
{
    return {value.model_slot, value.mask_floor_db,
            static_cast<NereusSDR::NrPosition>(value.position), value.alpha,
            value.alpha_knee_db, value.tau_seconds, value.max_gain_db,
            value.attack_ms, value.release_ms};
}
}
#endif

namespace NereusSDR {

std::optional<NnrSettings> NnrAdapter::readSettings(int channelId)
{
#ifdef HAVE_WDSP
    NNRRuntimeStatus status{};
    if (GetRXANNRStatus(channelId, &status) && status.ready) {
        const auto value = settingsFrom(status.configuration);
        if (value.isValid())
            return value;
    }
#else
    Q_UNUSED(channelId);
#endif
    return std::nullopt;
}

NnrDiagnostics NnrAdapter::diagnostics(int channelId)
{
    NnrDiagnostics result;
#ifdef HAVE_WDSP
    NNRRuntimeStatus status{};
    result.available = GetRXANNRStatus(channelId, &status) != 0;
    if (!result.available) {
        result.explanation = QStringLiteral("NNR receiver is not available.");
        return result;
    }
    result.ready = status.ready != 0;
    result.running = status.running != 0;
    result.rateSupported = status.rate_supported != 0;
    // The model running, not the accepted choice: a runtime limit can hold
    // the receiver on the standard model while the choice stays premium.
    result.actualModelSlot = status.active_model_slot;
    result.appliedLimit = status.limit;
    result.requestedRun = status.requested_run != 0;
    for (int i = 0; i < 2; ++i) {
        result.modelAvailable[i] = status.model_available[i] != 0;
        result.modelSources[i] = static_cast<NnrModelSource>(status.model_source[i]);
    }
    result.dspRateHz = status.dsp_rate_hz;
    result.networkRateHz = status.network_rate_hz;
    result.delaySamples = status.delay_samples;
    if (status.dsp_rate_hz > 0)
        result.latencyMs = 1000.0 * status.delay_samples / status.dsp_rate_hz;
    result.testMode = status.test_mode;
    result.outputMode = status.output_mode;
    result.profilingAvailable = status.profiling_available != 0;
    if (!result.ready)
        result.explanation = QStringLiteral("The selected NNR model is unavailable.");
    else if (!result.rateSupported)
        result.explanation = QStringLiteral("NNR cannot run at this receiver's processing rate. Use another "
                                             "noise reduction, or change the processing rate.");
#else
    Q_UNUSED(channelId);
    result.explanation = QStringLiteral("NNR is not included in this build.");
#endif
    return result;
}

std::optional<NnrSettings> NnrAdapter::apply(int channelId, const NnrSettings& requested,
                                            QString* reason)
{
    if (reason)
        reason->clear();
    if (!requested.isValid()) {
        if (reason)
            *reason = QStringLiteral("NNR values must be finite and within their supported ranges.");
        return std::nullopt;
    }
#ifdef HAVE_WDSP
    const NNRConfiguration value{requested.modelSlot, static_cast<int>(requested.position),
        requested.maskFloorDb, requested.alpha, requested.alphaKneeDb,
        requested.tauSeconds, requested.maxGainDb, requested.attackMs, requested.releaseMs};
    NNRRuntimeStatus accepted{};
    if (ConfigureRXANNR(channelId, &value, &accepted))
        return settingsFrom(accepted.configuration);
#else
    Q_UNUSED(channelId);
#endif
    if (reason)
        *reason = QStringLiteral("The requested NNR model is not ready; no tuning was changed.");
    return std::nullopt;
}

bool NnrAdapter::setRunning(int channelId, bool enabled, QString* reason)
{
    if (reason)
        reason->clear();
#ifdef HAVE_WDSP
    const auto before = diagnostics(channelId);
    if (!before.available || (enabled && (!before.ready || !before.rateSupported))) {
        if (reason)
            *reason = before.explanation;
        return false;
    }
    SetRXANNRRun(channelId, enabled ? 1 : 0);
    const auto after = diagnostics(channelId);
    // A runtime "off" limit holds NNR off while it stays requested on; the
    // request is accepted and runs again once the limit is cleared.
    return after.running == enabled
        || (enabled && after.requestedRun && after.appliedLimit == static_cast<int>(NnrLimit::Off));
#else
    Q_UNUSED(channelId);
    if (reason)
        *reason = QStringLiteral("NNR is not included in this build.");
    return !enabled;
#endif
}

bool NnrAdapter::requestLimit(int channelId, int limit)
{
    if (!isValidNnrLimit(limit)) {
        return false;
    }
#ifdef HAVE_WDSP
    RequestRXANNRLimit(channelId, limit);
#else
    Q_UNUSED(channelId);
#endif
    return true;
}

bool NnrAdapter::setDiagnostics(int channelId, int testMode, int outputMode, QString* reason)
{
    if (reason)
        reason->clear();
    if (testMode < 0 || testMode > 2 || outputMode < 0 || outputMode > 1) {
        if (reason)
            *reason = QStringLiteral("Unsupported NNR diagnostic mode.");
        return false;
    }
#ifdef HAVE_WDSP
    if (SetRXANNRDiagnostics(channelId, testMode, outputMode))
        return true;
#else
    Q_UNUSED(channelId);
#endif
    if (reason)
        *reason = QStringLiteral("The NNR receiver is not ready.");
    return false;
}

} // namespace NereusSDR
