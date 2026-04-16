#include "SliceModel.h"

#include "Band.h"
#include "core/AppSettings.h"

#include <algorithm>

namespace NereusSDR {

SliceModel::SliceModel(QObject* parent)
    : QObject(parent)
{
}

SliceModel::~SliceModel() = default;

// ---------------------------------------------------------------------------
// Frequency
// ---------------------------------------------------------------------------

void SliceModel::setFrequency(double freq)
{
    // 3G-10 S2.9: client-side lock guard. When locked, setFrequency is a
    // no-op — prevents accidental tuning. The hardware VFO is not changed.
    if (m_locked) { return; }
    if (!qFuzzyCompare(m_frequency, freq)) {
        m_frequency = freq;
        emit frequencyChanged(freq);
    }
}

// ---------------------------------------------------------------------------
// Demodulation mode
// ---------------------------------------------------------------------------

void SliceModel::setDspMode(DSPMode mode)
{
    bool modeChanged = (m_dspMode != mode);
    m_dspMode = mode;

    // Apply default filter for the new mode
    // From Thetis console.cs:5180-5575 — InitFilterPresets, F5 per mode
    auto [low, high] = defaultFilterForMode(mode);
    bool filterChanged = (m_filterLow != low || m_filterHigh != high);
    m_filterLow = low;
    m_filterHigh = high;

    if (modeChanged) {
        emit dspModeChanged(mode);
    }
    if (filterChanged) {
        emit this->filterChanged(m_filterLow, m_filterHigh);
    }
}

// ---------------------------------------------------------------------------
// Bandpass filter
// ---------------------------------------------------------------------------

void SliceModel::setFilterLow(int low)
{
    if (m_filterLow != low) {
        m_filterLow = low;
        emit filterChanged(m_filterLow, m_filterHigh);
    }
}

void SliceModel::setFilterHigh(int high)
{
    if (m_filterHigh != high) {
        m_filterHigh = high;
        emit filterChanged(m_filterLow, m_filterHigh);
    }
}

void SliceModel::setFilter(int low, int high)
{
    if (m_filterLow != low || m_filterHigh != high) {
        m_filterLow = low;
        m_filterHigh = high;
        emit filterChanged(m_filterLow, m_filterHigh);
    }
}

// ---------------------------------------------------------------------------
// AGC
// ---------------------------------------------------------------------------

void SliceModel::setAgcMode(AGCMode mode)
{
    if (m_agcMode != mode) {
        m_agcMode = mode;
        emit agcModeChanged(mode);
    }
}

// ---------------------------------------------------------------------------
// Tuning step
// ---------------------------------------------------------------------------

void SliceModel::setStepHz(int hz)
{
    if (m_stepHz != hz && hz > 0) {
        m_stepHz = hz;
        emit stepHzChanged(hz);
    }
}

// ---------------------------------------------------------------------------
// Gains
// ---------------------------------------------------------------------------

void SliceModel::setAfGain(int gain)
{
    gain = std::clamp(gain, 0, 100);
    if (m_afGain != gain) {
        m_afGain = gain;
        emit afGainChanged(gain);
    }
}

void SliceModel::setRfGain(int gain)
{
    gain = std::clamp(gain, 0, 100);
    if (m_rfGain != gain) {
        m_rfGain = gain;
        emit rfGainChanged(gain);
    }
}

// ---------------------------------------------------------------------------
// Antenna selection
// ---------------------------------------------------------------------------

void SliceModel::setRxAntenna(const QString& ant)
{
    if (m_rxAntenna != ant) {
        m_rxAntenna = ant;
        emit rxAntennaChanged(ant);
    }
}

void SliceModel::setTxAntenna(const QString& ant)
{
    if (m_txAntenna != ant) {
        m_txAntenna = ant;
        emit txAntennaChanged(ant);
    }
}

// ---------------------------------------------------------------------------
// Slice state
// ---------------------------------------------------------------------------

void SliceModel::setActive(bool active)
{
    if (m_active != active) {
        m_active = active;
        emit activeChanged(active);
    }
}

void SliceModel::setTxSlice(bool tx)
{
    if (m_txSlice != tx) {
        m_txSlice = tx;
        emit txSliceChanged(tx);
    }
}

// ── Phase 3G-10 Stage 1 stubs (DSP state, Stage 2 wires to RxChannel) ──

void SliceModel::setLocked(bool v)
{
    if (m_locked != v) {
        m_locked = v;
        emit lockedChanged(v);
    }
}

void SliceModel::setMuted(bool v)
{
    if (m_muted != v) {
        m_muted = v;
        emit mutedChanged(v);
    }
}

void SliceModel::setAudioPan(double pan)
{
    // qFuzzyCompare is undefined when either arg is 0.0; use the subtraction-to-zero pattern.
    if (qFuzzyIsNull(m_audioPan - pan)) {
        return;
    }
    m_audioPan = pan;
    emit audioPanChanged(pan);
}

void SliceModel::setSsqlEnabled(bool v)
{
    if (m_ssqlEnabled != v) {
        m_ssqlEnabled = v;
        emit ssqlEnabledChanged(v);
    }
}

void SliceModel::setSsqlThresh(double dB)
{
    // qFuzzyCompare is undefined when either arg is 0.0; use the subtraction-to-zero pattern.
    if (qFuzzyIsNull(m_ssqlThresh - dB)) {
        return;
    }
    m_ssqlThresh = dB;
    emit ssqlThreshChanged(dB);
}

void SliceModel::setAmsqEnabled(bool v)
{
    if (m_amsqEnabled != v) {
        m_amsqEnabled = v;
        emit amsqEnabledChanged(v);
    }
}

void SliceModel::setAmsqThresh(double dB)
{
    // qFuzzyCompare is undefined when either arg is 0.0; use the subtraction-to-zero pattern.
    if (qFuzzyIsNull(m_amsqThresh - dB)) {
        return;
    }
    m_amsqThresh = dB;
    emit amsqThreshChanged(dB);
}

void SliceModel::setFmsqEnabled(bool v)
{
    if (m_fmsqEnabled != v) {
        m_fmsqEnabled = v;
        emit fmsqEnabledChanged(v);
    }
}

void SliceModel::setFmsqThresh(double dB)
{
    // qFuzzyCompare is undefined when either arg is 0.0; use the subtraction-to-zero pattern.
    if (qFuzzyIsNull(m_fmsqThresh - dB)) {
        return;
    }
    m_fmsqThresh = dB;
    emit fmsqThreshChanged(dB);
}

void SliceModel::setAgcThreshold(int dBu)
{
    if (m_agcThreshold != dBu) {
        m_agcThreshold = dBu;
        emit agcThresholdChanged(dBu);
    }
}

void SliceModel::setAgcHang(int ms)
{
    if (m_agcHang != ms) {
        m_agcHang = ms;
        emit agcHangChanged(ms);
    }
}

void SliceModel::setAgcSlope(int dB)
{
    if (m_agcSlope != dB) {
        m_agcSlope = dB;
        emit agcSlopeChanged(dB);
    }
}

void SliceModel::setAgcAttack(int ms)
{
    if (m_agcAttack != ms) {
        m_agcAttack = ms;
        emit agcAttackChanged(ms);
    }
}

void SliceModel::setAgcDecay(int ms)
{
    if (m_agcDecay != ms) {
        m_agcDecay = ms;
        emit agcDecayChanged(ms);
    }
}

void SliceModel::setRitEnabled(bool v)
{
    if (m_ritEnabled != v) {
        m_ritEnabled = v;
        emit ritEnabledChanged(v);
    }
}

void SliceModel::setRitHz(int hz)
{
    if (m_ritHz != hz) {
        m_ritHz = hz;
        emit ritHzChanged(hz);
    }
}

void SliceModel::setXitEnabled(bool v)
{
    if (m_xitEnabled != v) {
        m_xitEnabled = v;
        emit xitEnabledChanged(v);
    }
}

void SliceModel::setXitHz(int hz)
{
    if (m_xitHz != hz) {
        m_xitHz = hz;
        emit xitHzChanged(hz);
    }
}

void SliceModel::setNb2Enabled(bool v)
{
    if (m_nb2Enabled != v) {
        m_nb2Enabled = v;
        emit nb2EnabledChanged(v);
    }
}

void SliceModel::setEmnrEnabled(bool v)
{
    if (m_emnrEnabled != v) {
        m_emnrEnabled = v;
        emit emnrEnabledChanged(v);
    }
}

void SliceModel::setSnbEnabled(bool v)
{
    if (m_snbEnabled != v) {
        m_snbEnabled = v;
        emit snbEnabledChanged(v);
    }
}

void SliceModel::setApfEnabled(bool v)
{
    if (m_apfEnabled != v) {
        m_apfEnabled = v;
        emit apfEnabledChanged(v);
    }
}

void SliceModel::setApfTuneHz(int hz)
{
    if (m_apfTuneHz != hz) {
        m_apfTuneHz = hz;
        emit apfTuneHzChanged(hz);
    }
}

void SliceModel::setBinauralEnabled(bool v)
{
    if (m_binauralEnabled != v) {
        m_binauralEnabled = v;
        emit binauralEnabledChanged(v);
    }
}

void SliceModel::setFmCtcssMode(int mode)
{
    if (m_fmCtcssMode != mode) {
        m_fmCtcssMode = mode;
        emit fmCtcssModeChanged(mode);
    }
}

void SliceModel::setFmCtcssValueHz(double hz)
{
    // qFuzzyCompare is undefined when either arg is 0.0; use the subtraction-to-zero pattern.
    if (qFuzzyIsNull(m_fmCtcssValueHz - hz)) {
        return;
    }
    m_fmCtcssValueHz = hz;
    emit fmCtcssValueHzChanged(hz);
}

void SliceModel::setFmOffsetHz(int hz)
{
    if (m_fmOffsetHz != hz) {
        m_fmOffsetHz = hz;
        emit fmOffsetHzChanged(hz);
    }
}

void SliceModel::setFmTxMode(FmTxMode mode)
{
    if (m_fmTxMode == mode) { return; }
    m_fmTxMode = mode;
    emit fmTxModeChanged(mode);
}

void SliceModel::setFmReverse(bool v)
{
    if (m_fmReverse != v) {
        m_fmReverse = v;
        emit fmReverseChanged(v);
    }
}

void SliceModel::setDiglOffsetHz(int hz)
{
    if (m_diglOffsetHz == hz) { return; }
    m_diglOffsetHz = hz;
    emit diglOffsetHzChanged(hz);
}

void SliceModel::setDiguOffsetHz(int hz)
{
    if (m_diguOffsetHz == hz) { return; }
    m_diguOffsetHz = hz;
    emit diguOffsetHzChanged(hz);
}

void SliceModel::setRttyMarkHz(int hz)
{
    if (m_rttyMarkHz != hz) {
        m_rttyMarkHz = hz;
        emit rttyMarkHzChanged(hz);
    }
}

void SliceModel::setRttyShiftHz(int hz)
{
    if (m_rttyShiftHz != hz) {
        m_rttyShiftHz = hz;
        emit rttyShiftHzChanged(hz);
    }
}

// ---------------------------------------------------------------------------
// Per-mode default filter presets
// ---------------------------------------------------------------------------

// Porting from Thetis console.cs:5180-5575 — InitFilterPresets, F5 per mode.
//
// Filter low/high are in Hz relative to the carrier frequency.
// LSB: negative offsets (passband below carrier)
// USB: positive offsets (passband above carrier)
// AM/SAM/DSB: symmetric around carrier
// CW: centered on cw_pitch (600 Hz from Thetis display.cs:1023)
// DIGU: centered on digu_click_tune_offset (1500 Hz from Thetis console.cs:14636)
// DIGL: centered on -digl_click_tune_offset (-2210 Hz from Thetis console.cs:14671)
std::pair<int, int> SliceModel::defaultFilterForMode(DSPMode mode)
{
    // From Thetis display.cs:1023
    static constexpr int kCwPitch = 600;
    // From Thetis console.cs:14636
    static constexpr int kDiguOffset = 1500;
    // From Thetis console.cs:14671
    static constexpr int kDiglOffset = 2210;

    switch (mode) {
    case DSPMode::LSB:
        // From Thetis console.cs:5207 — F5: -3000 to -100
        return {-3000, -100};
    case DSPMode::USB:
        // From Thetis console.cs:5249 — F5: 100 to 3000
        return {100, 3000};
    case DSPMode::DSB:
        // From Thetis console.cs:5543 — F5: -3300 to 3300
        return {-3300, 3300};
    case DSPMode::CWL:
        // From Thetis console.cs:5375 — F5: -(cw_pitch+200) to -(cw_pitch-200)
        return {-(kCwPitch + 200), -(kCwPitch - 200)};
    case DSPMode::CWU:
        // From Thetis console.cs:5417 — F5: (cw_pitch-200) to (cw_pitch+200)
        return {kCwPitch - 200, kCwPitch + 200};
    case DSPMode::FM:
        // FM filters are dynamic in Thetis (from deviation + high cut).
        // Default deviation=5000, so use ±8000 as reasonable default.
        // From Thetis console.cs:7559-7565
        return {-8000, 8000};
    case DSPMode::AM:
        // From Thetis console.cs:5459 — F5: -5000 to 5000
        return {-5000, 5000};
    case DSPMode::DIGU:
        // From Thetis console.cs:5333 — F5: (offset-500) to (offset+500)
        return {kDiguOffset - 500, kDiguOffset + 500};
    case DSPMode::SPEC:
        // SPEC mode: passthrough, wide filter
        return {-5000, 5000};
    case DSPMode::DIGL:
        // From Thetis console.cs:5291 — F5: -(offset+500) to -(offset-500)
        return {-(kDiglOffset + 500), -(kDiglOffset - 500)};
    case DSPMode::SAM:
        // From Thetis console.cs:5501 — F5: -5000 to 5000
        return {-5000, 5000};
    case DSPMode::DRM:
        // DRM: wide filter similar to AM
        return {-5000, 5000};
    }
    // Fallback
    return {100, 3000};
}

// ---------------------------------------------------------------------------
// Mode name utilities
// ---------------------------------------------------------------------------

QString SliceModel::modeName(DSPMode mode)
{
    switch (mode) {
    case DSPMode::LSB:  return QStringLiteral("LSB");
    case DSPMode::USB:  return QStringLiteral("USB");
    case DSPMode::DSB:  return QStringLiteral("DSB");
    case DSPMode::CWL:  return QStringLiteral("CWL");
    case DSPMode::CWU:  return QStringLiteral("CWU");
    case DSPMode::FM:   return QStringLiteral("FM");
    case DSPMode::AM:   return QStringLiteral("AM");
    case DSPMode::DIGU: return QStringLiteral("DIGU");
    case DSPMode::SPEC: return QStringLiteral("SPEC");
    case DSPMode::DIGL: return QStringLiteral("DIGL");
    case DSPMode::SAM:  return QStringLiteral("SAM");
    case DSPMode::DRM:  return QStringLiteral("DRM");
    }
    return QStringLiteral("USB");
}

DSPMode SliceModel::modeFromName(const QString& name)
{
    if (name == QLatin1String("LSB"))  return DSPMode::LSB;
    if (name == QLatin1String("USB"))  return DSPMode::USB;
    if (name == QLatin1String("DSB"))  return DSPMode::DSB;
    if (name == QLatin1String("CWL"))  return DSPMode::CWL;
    if (name == QLatin1String("CWU"))  return DSPMode::CWU;
    if (name == QLatin1String("FM"))   return DSPMode::FM;
    if (name == QLatin1String("AM"))   return DSPMode::AM;
    if (name == QLatin1String("DIGU")) return DSPMode::DIGU;
    if (name == QLatin1String("SPEC")) return DSPMode::SPEC;
    if (name == QLatin1String("DIGL")) return DSPMode::DIGL;
    if (name == QLatin1String("SAM"))  return DSPMode::SAM;
    if (name == QLatin1String("DRM"))  return DSPMode::DRM;
    return DSPMode::USB;
}

// ---------------------------------------------------------------------------
// Per-slice-per-band persistence (Phase 3G-10 Stage 2 — S2.P)
// ---------------------------------------------------------------------------
//
// Key layout:
//   Per-band DSP: Slice<N>/Band<key>/<Field>   (varies by band)
//   Session state: Slice<N>/<Field>             (band-agnostic)
//
// <N> comes from m_sliceIndex. <key> comes from bandKeyName(band).

namespace {

// Build the per-band prefix string, e.g. "Slice0/Band20m/".
QString bandPrefix(int sliceIndex, Band band)
{
    return QStringLiteral("Slice%1/Band%2/")
               .arg(sliceIndex)
               .arg(bandKeyName(band));
}

// Build the session-state prefix string, e.g. "Slice0/".
QString slicePrefix(int sliceIndex)
{
    return QStringLiteral("Slice%1/").arg(sliceIndex);
}

// Boolean → AppSettings canonical string.
QString boolStr(bool v) { return v ? QStringLiteral("True") : QStringLiteral("False"); }

} // namespace

void SliceModel::saveToSettings(Band band)
{
    auto& s = AppSettings::instance();
    const QString bp = bandPrefix(m_sliceIndex, band);
    const QString sp = slicePrefix(m_sliceIndex);

    // ── Per-band DSP state ────────────────────────────────────────────────────
    s.setValue(bp + QStringLiteral("Frequency"),    m_frequency);
    s.setValue(bp + QStringLiteral("AgcThreshold"), m_agcThreshold);
    s.setValue(bp + QStringLiteral("AgcHang"),      m_agcHang);
    s.setValue(bp + QStringLiteral("AgcSlope"),     m_agcSlope);
    s.setValue(bp + QStringLiteral("AgcAttack"),    m_agcAttack);
    s.setValue(bp + QStringLiteral("AgcDecay"),     m_agcDecay);
    s.setValue(bp + QStringLiteral("FilterLow"),    m_filterLow);
    s.setValue(bp + QStringLiteral("FilterHigh"),   m_filterHigh);
    s.setValue(bp + QStringLiteral("DspMode"),      static_cast<int>(m_dspMode));
    s.setValue(bp + QStringLiteral("AgcMode"),      static_cast<int>(m_agcMode));
    s.setValue(bp + QStringLiteral("StepHz"),       m_stepHz);

    // ── Session state (band-agnostic) ─────────────────────────────────────────
    s.setValue(sp + QStringLiteral("Locked"),     boolStr(m_locked));
    s.setValue(sp + QStringLiteral("Muted"),      boolStr(m_muted));
    s.setValue(sp + QStringLiteral("RitEnabled"), boolStr(m_ritEnabled));
    s.setValue(sp + QStringLiteral("RitHz"),      m_ritHz);
    s.setValue(sp + QStringLiteral("XitEnabled"), boolStr(m_xitEnabled));
    s.setValue(sp + QStringLiteral("XitHz"),      m_xitHz);
    s.setValue(sp + QStringLiteral("AfGain"),     m_afGain);
    s.setValue(sp + QStringLiteral("RfGain"),     m_rfGain);
    s.setValue(sp + QStringLiteral("RxAntenna"),  m_rxAntenna);
    s.setValue(sp + QStringLiteral("TxAntenna"),  m_txAntenna);
}

void SliceModel::restoreFromSettings(Band band)
{
    auto& s = AppSettings::instance();
    const QString bp = bandPrefix(m_sliceIndex, band);
    const QString sp = slicePrefix(m_sliceIndex);

    // ── Per-band DSP state ────────────────────────────────────────────────────
    // Each key: if absent, leave the current SliceModel default unchanged.

    if (s.contains(bp + QStringLiteral("Frequency"))) {
        setFrequency(s.value(bp + QStringLiteral("Frequency")).toDouble());
    }
    if (s.contains(bp + QStringLiteral("AgcThreshold"))) {
        setAgcThreshold(s.value(bp + QStringLiteral("AgcThreshold")).toInt());
    }
    if (s.contains(bp + QStringLiteral("AgcHang"))) {
        setAgcHang(s.value(bp + QStringLiteral("AgcHang")).toInt());
    }
    if (s.contains(bp + QStringLiteral("AgcSlope"))) {
        setAgcSlope(s.value(bp + QStringLiteral("AgcSlope")).toInt());
    }
    if (s.contains(bp + QStringLiteral("AgcAttack"))) {
        setAgcAttack(s.value(bp + QStringLiteral("AgcAttack")).toInt());
    }
    if (s.contains(bp + QStringLiteral("AgcDecay"))) {
        setAgcDecay(s.value(bp + QStringLiteral("AgcDecay")).toInt());
    }
    if (s.contains(bp + QStringLiteral("DspMode"))) {
        // Set mode WITHOUT applying the default filter — filter follows below.
        // We must update m_dspMode before reading FilterLow/FilterHigh so
        // the final setFilter call is not superseded by setDspMode's default.
        DSPMode mode = static_cast<DSPMode>(
            s.value(bp + QStringLiteral("DspMode")).toInt());
        // Directly assign mode without calling setDspMode() (which also
        // resets the filter). Emit the signal manually to keep observers in sync.
        if (m_dspMode != mode) {
            m_dspMode = mode;
            emit dspModeChanged(mode);
        }
    }
    if (s.contains(bp + QStringLiteral("FilterLow")) &&
        s.contains(bp + QStringLiteral("FilterHigh"))) {
        setFilter(s.value(bp + QStringLiteral("FilterLow")).toInt(),
                  s.value(bp + QStringLiteral("FilterHigh")).toInt());
    }
    if (s.contains(bp + QStringLiteral("AgcMode"))) {
        setAgcMode(static_cast<AGCMode>(
            s.value(bp + QStringLiteral("AgcMode")).toInt()));
    }
    if (s.contains(bp + QStringLiteral("StepHz"))) {
        setStepHz(s.value(bp + QStringLiteral("StepHz")).toInt());
    }

    // ── Session state (band-agnostic) ─────────────────────────────────────────

    if (s.contains(sp + QStringLiteral("Locked"))) {
        setLocked(s.value(sp + QStringLiteral("Locked")).toString() == QLatin1String("True"));
    }
    if (s.contains(sp + QStringLiteral("Muted"))) {
        setMuted(s.value(sp + QStringLiteral("Muted")).toString() == QLatin1String("True"));
    }
    if (s.contains(sp + QStringLiteral("RitEnabled"))) {
        setRitEnabled(s.value(sp + QStringLiteral("RitEnabled")).toString() == QLatin1String("True"));
    }
    if (s.contains(sp + QStringLiteral("RitHz"))) {
        setRitHz(s.value(sp + QStringLiteral("RitHz")).toInt());
    }
    if (s.contains(sp + QStringLiteral("XitEnabled"))) {
        setXitEnabled(s.value(sp + QStringLiteral("XitEnabled")).toString() == QLatin1String("True"));
    }
    if (s.contains(sp + QStringLiteral("XitHz"))) {
        setXitHz(s.value(sp + QStringLiteral("XitHz")).toInt());
    }
    if (s.contains(sp + QStringLiteral("AfGain"))) {
        setAfGain(s.value(sp + QStringLiteral("AfGain")).toInt());
    }
    if (s.contains(sp + QStringLiteral("RfGain"))) {
        setRfGain(s.value(sp + QStringLiteral("RfGain")).toInt());
    }
    if (s.contains(sp + QStringLiteral("RxAntenna"))) {
        setRxAntenna(s.value(sp + QStringLiteral("RxAntenna")).toString());
    }
    if (s.contains(sp + QStringLiteral("TxAntenna"))) {
        setTxAntenna(s.value(sp + QStringLiteral("TxAntenna")).toString());
    }
}

// One-shot migration of the legacy flat key format (VfoFrequency, VfoDspMode,
// etc.) to the new per-slice-per-band namespace. Called once at startup before
// restoreFromSettings(). If the legacy key is absent the function is a no-op.
void SliceModel::migrateLegacyKeys()
{
    auto& s = AppSettings::instance();

    if (!s.contains(QStringLiteral("VfoFrequency"))) {
        return; // Nothing to migrate.
    }

    // Derive the band from the persisted frequency.
    double freq = s.value(QStringLiteral("VfoFrequency"), 14225000.0).toDouble();
    Band band = bandFromFrequency(freq);

    // Slice 0 — the only slice that can have legacy data.
    const QString bp = bandPrefix(0, band);
    const QString sp = slicePrefix(0);

    // Per-band DSP — migrate each key that exists.
    s.setValue(bp + QStringLiteral("Frequency"), freq);

    if (s.contains(QStringLiteral("VfoDspMode"))) {
        s.setValue(bp + QStringLiteral("DspMode"),
                   s.value(QStringLiteral("VfoDspMode")));
    }
    if (s.contains(QStringLiteral("VfoFilterLow"))) {
        s.setValue(bp + QStringLiteral("FilterLow"),
                   s.value(QStringLiteral("VfoFilterLow")));
    }
    if (s.contains(QStringLiteral("VfoFilterHigh"))) {
        s.setValue(bp + QStringLiteral("FilterHigh"),
                   s.value(QStringLiteral("VfoFilterHigh")));
    }
    if (s.contains(QStringLiteral("VfoAgcMode"))) {
        s.setValue(bp + QStringLiteral("AgcMode"),
                   s.value(QStringLiteral("VfoAgcMode")));
    }
    if (s.contains(QStringLiteral("VfoStepHz"))) {
        s.setValue(bp + QStringLiteral("StepHz"),
                   s.value(QStringLiteral("VfoStepHz")));
    }

    // Session state — migrate each key that exists.
    if (s.contains(QStringLiteral("VfoAfGain"))) {
        s.setValue(sp + QStringLiteral("AfGain"),
                   s.value(QStringLiteral("VfoAfGain")));
    }
    if (s.contains(QStringLiteral("VfoRfGain"))) {
        s.setValue(sp + QStringLiteral("RfGain"),
                   s.value(QStringLiteral("VfoRfGain")));
    }
    if (s.contains(QStringLiteral("VfoRxAntenna"))) {
        s.setValue(sp + QStringLiteral("RxAntenna"),
                   s.value(QStringLiteral("VfoRxAntenna")));
    }
    if (s.contains(QStringLiteral("VfoTxAntenna"))) {
        s.setValue(sp + QStringLiteral("TxAntenna"),
                   s.value(QStringLiteral("VfoTxAntenna")));
    }

    // Remove all legacy flat keys.
    s.remove(QStringLiteral("VfoFrequency"));
    s.remove(QStringLiteral("VfoDspMode"));
    s.remove(QStringLiteral("VfoFilterLow"));
    s.remove(QStringLiteral("VfoFilterHigh"));
    s.remove(QStringLiteral("VfoAgcMode"));
    s.remove(QStringLiteral("VfoStepHz"));
    s.remove(QStringLiteral("VfoAfGain"));
    s.remove(QStringLiteral("VfoRfGain"));
    s.remove(QStringLiteral("VfoRxAntenna"));
    s.remove(QStringLiteral("VfoTxAntenna"));
}

} // namespace NereusSDR
