#include "RxChannel.h"
#include "LogCategories.h"
#include "wdsp_api.h"

#include <cmath>

namespace NereusSDR {

RxChannel::RxChannel(int channelId, int bufferSize, int sampleRate,
                     QObject* parent)
    : QObject(parent)
    , m_channelId(channelId)
    , m_bufferSize(bufferSize)
    , m_sampleRate(sampleRate)
{
}

RxChannel::~RxChannel() = default;

// ---------------------------------------------------------------------------
// Demodulation
// ---------------------------------------------------------------------------

void RxChannel::setMode(DSPMode mode)
{
    int val = static_cast<int>(mode);
    if (val == m_mode.load()) {
        return;
    }

    m_mode.store(val);

#ifdef HAVE_WDSP
    // From Thetis wdsp-integration.md section 4.2
    SetRXAMode(m_channelId, val);
#endif

    emit modeChanged(mode);
}

// ---------------------------------------------------------------------------
// Bandpass filter
// ---------------------------------------------------------------------------

void RxChannel::setFilterFreqs(double lowHz, double highHz)
{
    if (m_filterLow == lowHz && m_filterHigh == highHz) {
        return;
    }

    m_filterLow = lowHz;
    m_filterHigh = highHz;

#ifdef HAVE_WDSP
    // From Thetis rxa.cs:110-111, radio.cs:603-604 — both bp1 and nbp0
    // filters must be updated together. SetRXABandpassFreqs only touches
    // bp1, which runs only when AMD/SNBA/EMNR/ANF/ANR is enabled.
    // RXANBPSetFreqs touches nbp0, the filter that runs unconditionally
    // in the SSB/CW/AM audio path. Calling only one leaves the SSB
    // bandpass stuck at nbp0's create-time default of -4150..-150
    // (LSB-shaped), which silently breaks USB, AM, and FM demod.
    SetRXABandpassFreqs(m_channelId, lowHz, highHz);
    RXANBPSetFreqs(m_channelId, lowHz, highHz);
#endif

    emit filterChanged(lowHz, highHz);
}

// ---------------------------------------------------------------------------
// AGC
// ---------------------------------------------------------------------------

void RxChannel::setAgcMode(AGCMode mode)
{
    int val = static_cast<int>(mode);
    if (val == m_agcMode.load()) {
        return;
    }

    m_agcMode.store(val);

#ifdef HAVE_WDSP
    SetRXAAGCMode(m_channelId, val);
#endif

    emit agcModeChanged(mode);
}

void RxChannel::setAgcTop(double topdB)
{
#ifdef HAVE_WDSP
    SetRXAAGCTop(m_channelId, topdB);
#else
    Q_UNUSED(topdB);
#endif
}

void RxChannel::setAgcThreshold(int dBu)
{
    if (dBu == m_agcThreshold.load()) {
        return;
    }

    m_agcThreshold.store(dBu);

#ifdef HAVE_WDSP
    // From Thetis Project Files/Source/Console/console.cs:45976-45977
    //   size = (double)specRX.GetSpecRX(0).FFTSize;  // 4096
    //   WDSP.SetRXAAGCThresh(WDSP.id(0, 0), agc_thresh_point, size, sample_rate_rx1);
    // WDSP third_party/wdsp/src/wcpAGC.c:504
    // NB: 'size' is the DSP analysis buffer size (4096, matching OpenChannel dsp_size),
    //     NOT the fexchange2 input chunk size (m_bufferSize).
    static constexpr double kDspSize = 4096.0;
    SetRXAAGCThresh(m_channelId, static_cast<double>(dBu),
                    kDspSize,
                    static_cast<double>(m_sampleRate));
#else
    Q_UNUSED(dBu);
#endif
}

void RxChannel::setAgcHang(int ms)
{
    if (ms == m_agcHang.load()) {
        return;
    }

    m_agcHang.store(ms);

#ifdef HAVE_WDSP
    // From Thetis Project Files/Source/Console/radio.cs:1056-1073
    //   WDSP.SetRXAAGCHang(WDSP.id(thread, subrx), value)
    // WDSP third_party/wdsp/src/wcpAGC.c:436
    SetRXAAGCHang(m_channelId, ms);
#else
    Q_UNUSED(ms);
#endif
}

void RxChannel::setAgcSlope(int slope)
{
    if (slope == m_agcSlope.load()) {
        return;
    }

    m_agcSlope.store(slope);

#ifdef HAVE_WDSP
    // From Thetis Project Files/Source/Console/radio.cs:1107-1124
    //   WDSP.SetRXAAGCSlope(WDSP.id(thread, subrx), value)
    // WDSP third_party/wdsp/src/wcpAGC.c:537
    SetRXAAGCSlope(m_channelId, slope);
#else
    Q_UNUSED(slope);
#endif
}

void RxChannel::setAgcAttack(int ms)
{
    if (ms == m_agcAttack.load()) {
        return;
    }

    m_agcAttack.store(ms);

#ifdef HAVE_WDSP
    // From Thetis Project Files/Source/Console/dsp.cs:116-117
    //   SetRXAAGCAttack declared; no explicit radio.cs call site (disabled in UI)
    // WDSP third_party/wdsp/src/wcpAGC.c:418
    SetRXAAGCAttack(m_channelId, ms);
#else
    Q_UNUSED(ms);
#endif
}

void RxChannel::setAgcDecay(int ms)
{
    if (ms == m_agcDecay.load()) {
        return;
    }

    m_agcDecay.store(ms);

#ifdef HAVE_WDSP
    // From Thetis Project Files/Source/Console/radio.cs:1037-1054
    //   WDSP.SetRXAAGCDecay(WDSP.id(thread, subrx), value)
    // WDSP third_party/wdsp/src/wcpAGC.c:427
    SetRXAAGCDecay(m_channelId, ms);
#else
    Q_UNUSED(ms);
#endif
}

// ---------------------------------------------------------------------------
// Noise blanker
// ---------------------------------------------------------------------------

void RxChannel::setNb1Enabled(bool enabled)
{
    m_nb1Enabled.store(enabled);
    // NB1 is applied in processIq() — no WDSP call needed here,
    // the xanbEXTF call is gated by the atomic flag.
}

void RxChannel::setNb2Enabled(bool enabled)
{
    m_nb2Enabled.store(enabled);
    // NB2 is applied in processIq() — same pattern as NB1.
}

// ---------------------------------------------------------------------------
// Noise reduction
// ---------------------------------------------------------------------------

void RxChannel::setNrEnabled(bool enabled)
{
    m_nrEnabled.store(enabled);

#ifdef HAVE_WDSP
    SetRXAANRRun(m_channelId, enabled ? 1 : 0);
#endif
}

void RxChannel::setAnfEnabled(bool enabled)
{
    m_anfEnabled.store(enabled);

#ifdef HAVE_WDSP
    SetRXAANFRun(m_channelId, enabled ? 1 : 0);
#endif
}

// ---------------------------------------------------------------------------
// Frequency shift (pan offset from VFO)
// ---------------------------------------------------------------------------

void RxChannel::setShiftFrequency(double offsetHz)
{
#ifdef HAVE_WDSP
    if (std::abs(offsetHz) < 0.5) {
        // No offset — disable shift for efficiency
        SetRXAShiftRun(m_channelId, 0);
    } else {
        // From Thetis radio.cs:1417-1418 — both calls use the same sign
        SetRXAShiftFreq(m_channelId, offsetHz);
        RXANBPSetShiftFrequency(m_channelId, offsetHz);
        SetRXAShiftRun(m_channelId, 1);
    }
#else
    Q_UNUSED(offsetHz);
#endif
}

// ---------------------------------------------------------------------------
// Channel state
// ---------------------------------------------------------------------------

void RxChannel::setActive(bool active)
{
    if (active == m_active.load()) {
        return;
    }

    m_active.store(active);

#ifdef HAVE_WDSP
    // state=1 on, state=0 off; dmode=0 for no drain, dmode=1 for drain
    SetChannelState(m_channelId, active ? 1 : 0, active ? 0 : 1);
#endif

    qCDebug(lcDsp) << "RxChannel" << m_channelId
                    << (active ? "activated" : "deactivated");
    emit activeChanged(active);
}

// ---------------------------------------------------------------------------
// Audio processing — hot path
// ---------------------------------------------------------------------------

void RxChannel::processIq(float* inI, float* inQ,
                          float* outI, float* outQ,
                          int sampleCount)
{
    if (!m_active.load()) {
        // Channel inactive — output silence
        std::memset(outI, 0, sampleCount * sizeof(float));
        std::memset(outQ, 0, sampleCount * sizeof(float));
        return;
    }

#ifdef HAVE_WDSP
    // NB1 and NB2 process raw I/Q BEFORE the main WDSP channel.
    // They operate in-place on separate I and Q buffers.
    // From Thetis wdsp-integration.md section 4.3
    if (m_nb1Enabled.load()) {
        xanbEXTF(m_channelId, inI, inQ);
    }
    if (m_nb2Enabled.load()) {
        xnobEXTF(m_channelId, inI, inQ);
    }

    // Main WDSP processing: demod, AGC, NR, ANF, filter, EQ, audio panel.
    int error = 0;
    fexchange2(m_channelId, inI, inQ, outI, outQ, &error);

    if (error != 0) {
        qCWarning(lcDsp) << "fexchange2 error on channel"
                         << m_channelId << ":" << error;
    }
#else
    // WDSP not available — output silence
    std::memset(outI, 0, sampleCount * sizeof(float));
    std::memset(outQ, 0, sampleCount * sizeof(float));
#endif
}

// ---------------------------------------------------------------------------
// Metering
// ---------------------------------------------------------------------------

double RxChannel::getMeter(RxMeterType type) const
{
#ifdef HAVE_WDSP
    // GetRXAMeter reads WDSP channel state that is only valid once
    // SetChannelState(channel, 1, 0) has been called. Reading before the
    // channel is active segfaults (seen after P1 connect in Phase 3I,
    // because the P1 path had not yet called setActive(true) on the
    // downstream RxChannel the MeterPoller was bound to). Guard here:
    // the contract is "no meter data until active".
    if (!m_active.load()) {
        return -140.0;
    }
    return GetRXAMeter(m_channelId, static_cast<int>(type));
#else
    Q_UNUSED(type);
    return -140.0;
#endif
}

} // namespace NereusSDR
