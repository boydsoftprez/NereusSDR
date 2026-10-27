// no-port-check: NereusSDR-original. See CwKeyerController.h.
// =================================================================
// src/models/CwKeyerController.cpp  (NereusSDR)
// =================================================================
//
// Modification history (NereusSDR):
//   2026-10-26: CW transmit, the CW keyer controller (R-CW-05, R-CW-06,
//               R-CW-07, R-CW-10, R-CW-13, R-CW-16, R-CW-20, R-CW-22,
//               R-CW-38, D16). J.J. Boyd (KG4VCF), AI-assisted via
//               Claude Code.
//   2026-10-26: CW transmit early fix wave: C1, I2 and M4 (see the
//               header). J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// =================================================================

#include "models/CwKeyerController.h"

#include <algorithm>

#include "core/HpsdrModel.h"
#include "core/MoxController.h"
#include "core/RadioDiscovery.h"
#include "core/SmartSdrApiListener.h"
#include "core/TxInterlockPolicy.h"
#include "core/TxSliceArbiter.h"
#include "core/WdspTypes.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

namespace NereusSDR {

CwKeyerController::CwKeyerController(RadioModel& radio, QObject* parent)
    : QObject(parent)
    , m_radio(radio)
{
    const auto again = [this]() { evaluate(); };

    // R-CW-06: the signals the transmit checks follow today.
    connect(&m_radio, &RadioModel::sliceAdded, this, [this](int) {
        wireSlices();
        evaluate();
    });
    connect(&m_radio, &RadioModel::sliceRemoved, this, again);
    connect(&m_radio, &RadioModel::activeSliceChanged, this, again);
    if (TxSliceArbiter* arbiter = m_radio.txSliceArbiter()) {
        connect(arbiter, &TxSliceArbiter::txBoundSliceChanged, this, again);
    }
    connect(&m_radio, &RadioModel::receiveOnlyStationPolicyChanged, this, again);
    connect(&m_radio, &RadioModel::rxOnlyChanged, this, again);
    connect(&m_radio, &RadioModel::txInhibitedChanged, this, again);
    connect(&m_radio, &RadioModel::paTrippedChanged, this, again);
    connect(&m_radio, &RadioModel::radioLinkDownChanged, this, again);
    connect(&m_radio, &RadioModel::connectionStateChanged, this, again);
    connect(&m_radio, &RadioModel::currentRadioChanged, this, again);
    // The three band-plan settings, which AppSettings does not signal.
    connect(&m_radio, &RadioModel::bandPlanRegionChanged, this, again);
    connect(&m_radio, &RadioModel::extendedTransmitChanged, this, again);
    connect(&m_radio, &RadioModel::preventTxOnDifferentBandChanged, this, again);

    // The interlock: the amplifier's presence, operate state and SWR, which
    // RadioModel's own earlier connections hand to MoxController first, and
    // the policy's settings.
    connect(&m_radio, &RadioModel::amplifierChanged, this, again);
    connect(&m_radio, &RadioModel::ampStateChanged, this, again);
    connect(&m_radio, &RadioModel::ampMetersChanged, this, again);
    if (TxInterlockPolicy* policy = m_radio.txInterlockPolicy()) {
        connect(policy, &TxInterlockPolicy::changed, this, again);
    }
    // D16: whether an amplifier interlock is armed, and its grant for the
    // key now on. The grant can arrive inside txAboutToBegin (a fast
    // acknowledgement), so it is cleared only when that key ends.
    // Early fix wave I2: the grant belongs to the key now on. MOX is on from
    // the moment the key is made (MoxController sets it before
    // txAboutToBegin), so a grant that arrives with MOX off is for a key
    // that has already ended (the listener sends it from a 30 ms timer that
    // an unkey does not cancel) and is ignored, as the voice path drops a
    // stale queued step by its key generation.
    if (SmartSdrApiListener* listener = m_radio.smartSdrListener()) {
        connect(listener, &SmartSdrApiListener::interlockedAmpChanged, this, again);
        connect(listener, &SmartSdrApiListener::interlockGranted, this, [this](const QString&) {
            const MoxController* mox = m_radio.moxController();
            if (mox == nullptr || !mox->isMox()) {
                return;
            }
            m_interlockGranted = true;
            evaluate();
        });
    }
    if (MoxController* mox = m_radio.moxController()) {
        connect(mox, &MoxController::txAboutToEnd, this, [this]() {
            m_interlockGranted = false;
            evaluate();
        });
        connect(mox, &MoxController::moxStateChanged, this, [this](bool on) {
            if (on) {
                return;
            }
            m_interlockGranted = false;
            evaluate();
        });
    }

    // The composition's inputs (R-CW-07, R-CW-13, R-CW-38).
    TransmitModel& tm = m_radio.transmitModel();
    connect(&tm, &TransmitModel::cwKeyerSpeedChanged, this, again);
    connect(&tm, &TransmitModel::cwKeyerModeChanged, this, again);
    connect(&tm, &TransmitModel::cwKeyerWeightChanged, this, again);
    connect(&tm, &TransmitModel::cwReversePaddlesChanged, this, again);
    connect(&tm, &TransmitModel::cwStrictSpacingChanged, this, again);
    connect(&tm, &TransmitModel::cwBreakInModeChanged, this, again);
    connect(&tm, &TransmitModel::cwBreakInDelayMsChanged, this, again);
    connect(&tm, &TransmitModel::cwKeyDownDelayMsChanged, this, again);
    connect(&tm, &TransmitModel::cwKeyUpDelayMsChanged, this, again);
    connect(&tm, &TransmitModel::cwSidetoneChanged, this, again);
    connect(&tm, &TransmitModel::cwRadioSidetoneChanged, this, again);
    connect(&tm, &TransmitModel::monitorVolumeChanged, this, again);
    connect(&m_radio, &RadioModel::cwPitchChanged, this, again);
    connect(&m_radio, &RadioModel::radioSpeakerMutedChanged, this, again);

    wireSlices();
    evaluate();
}

void CwKeyerController::reevaluate()
{
    evaluate();
}

void CwKeyerController::onRadioKeyStatus(bool dot, bool dash, bool keyOut)
{
    m_dot = dot;
    m_dash = dash;
    m_keyOut = keyOut;
    if (m_stopLatched && !dot && !dash && !keyOut) {
        m_stopLatched = false;
        evaluate();
    }
}

void CwKeyerController::latchStop(bool sendingStretch)
{
    if (m_stopLatched) {
        return;
    }
    // Early fix wave C1: a stop between the elements of a QSK stretch, with
    // the paddle up, latches too; the next element would key again.
    if (!radioKeyDown() && !(sendingStretch && m_result.allowed)) {
        return;
    }
    m_stopLatched = true;
    evaluate();
}

void CwKeyerController::wireSlices()
{
    m_wiredSlices.removeAll(QPointer<SliceModel>());
    for (SliceModel* slice : m_radio.slices()) {
        if (slice != nullptr && !m_wiredSlices.contains(QPointer<SliceModel>(slice))) {
            wireSlice(slice);
        }
    }
}

void CwKeyerController::wireSlice(SliceModel* slice)
{
    m_wiredSlices.append(QPointer<SliceModel>(slice));
    const auto again = [this]() { evaluate(); };
    // Every slice, not only the transmit slice: "prevent transmitting on a
    // different band" compares the transmit slice with its device's active
    // slice (RadioModel::bandPlanCheckForTxSlice).
    connect(slice, &SliceModel::frequencyChanged, this, again);
    connect(slice, &SliceModel::dspModeChanged, this, again);
    connect(slice, &SliceModel::xitEnabledChanged, this, again);
    connect(slice, &SliceModel::xitHzChanged, this, again);
    // R-CW-20: the antenna rule.
    connect(slice, &SliceModel::rxAntennaChanged, this, again);
    connect(slice, &SliceModel::txAntennaChanged, this, again);
}

bool CwKeyerController::interlockArmed() const
{
    const SmartSdrApiListener* listener = m_radio.smartSdrListener();
    return listener != nullptr && listener->hasInterlockedAmp();
}

safety::CwKeyerGateInputs CwKeyerController::gateInputs() const
{
    safety::CwKeyerGateInputs in;
    const SliceModel* tx = m_radio.txBoundSlice();
    in.hasTxSlice = tx != nullptr;
    in.txSliceInCw = tx != nullptr
        && (tx->dspMode() == DSPMode::CWL || tx->dspMode() == DSPMode::CWU);
    if (in.hasTxSlice && in.txSliceInCw) {
        // The carrier alone (R-CW-05, Thetis console.cs:6778-6813
        // [v2.10.3.15]); BandPlanGuard ignores the filter for CW anyway.
        in.bandPlan = m_radio.bandPlanCheckForTxSlice(/*ignoreFilter=*/true);
    }
    in.receiveOnly = m_radio.receiveOnlyTxOperationsBlocked() || m_radio.isRxOnly();
    in.txInhibited = m_radio.isTxInhibited();
    in.paTripped = m_radio.paTripped();
    in.radioLinkDown = m_radio.isRadioLinkDown();
    in.radioChangeUnderway = m_radio.stationRadioChangeUnderway();
    in.stopLatched = m_stopLatched;
    if (const MoxController* mox = m_radio.moxController()) {
        in.interlockRefusal = mox->interlockRefusal().text;
    }
    in.interlockArmed = interlockArmed();
    in.interlockGranted = m_interlockGranted;
    return in;
}

QString CwKeyerController::qskReasonNow(bool armed) const
{
    // R-CW-20, in its order. Early fix wave M4: the Hermes Lite 2 first, so
    // an HL2 (a Protocol 1 radio) is told it by name rather than the
    // protocol or board reason.
    const RadioInfo& info = m_radio.currentRadioInfo();
    const HPSDRHW board = info.boardType;
    if (board == HPSDRHW::HermesLite || board == HPSDRHW::HermesLiteRxOnly) {
        return QString::fromLatin1(kCwQskHl2Reason);
    }
    if (info.protocol != ProtocolVersion::Protocol2) {
        return QString::fromLatin1(kCwQskNeedsP2Reason);
    }
    const bool orion = board == HPSDRHW::Orion || board == HPSDRHW::OrionMKII;
    const bool saturn = board == HPSDRHW::Saturn || board == HPSDRHW::SaturnMKII;
    if (!orion && !saturn) {
        return QString::fromLatin1(kCwQskBoardReason);
    }
    if (orion && info.firmwareVersion < kCwQskMinOrionFirmware) {
        return QString::fromLatin1(kCwQskFirmwareReason);
    }
    if (const SliceModel* tx = m_radio.txBoundSlice()) {
        if (tx->txAntenna() != tx->rxAntenna()) {
            return QString::fromLatin1(kCwQskAntennaReason);
        }
    }
    if (txOnTransverterBand()) {
        return QString::fromLatin1(kCwQskTransverterReason);
    }
    if (armed) {
        return QString::fromLatin1(kCwAmpInterlockBreakInReason);
    }
    return QString();
}

void CwKeyerController::evaluate()
{
    ++m_evaluations;

    const safety::CwKeyerGateInputs in = gateInputs();
    const safety::CwKeyerGateResult result = safety::evaluateCwKeyerGate(in);

    const QString qskReason = qskReasonNow(in.interlockArmed);
    const QString semiReason = in.interlockArmed
        ? QString::fromLatin1(kCwAmpInterlockBreakInReason) : QString();

    TransmitModel& tm = m_radio.transmitModel();
    const int selected = std::clamp(tm.cwBreakInMode(), static_cast<int>(CwBreakInMode::Off),
                                    static_cast<int>(CwBreakInMode::Qsk));
    CwBreakInMode mode = static_cast<CwBreakInMode>(selected);
    // R-CW-20: QSK not allowed falls back to Semi; R-CW-22 (D16): Semi not
    // allowed falls back to Off.
    if (mode == CwBreakInMode::Qsk && !qskReason.isEmpty()) {
        mode = CwBreakInMode::Semi;
    }
    if (mode == CwBreakInMode::Semi && !semiReason.isEmpty()) {
        mode = CwBreakInMode::Off;
    }

    CwWireState wire;
    wire.enable = result.allowed;
    wire.breakIn = result.allowed
        && (mode == CwBreakInMode::Semi || mode == CwBreakInMode::Qsk);
    wire.keyerMode = tm.cwKeyerMode();
    wire.speedWpm = tm.cwKeyerSpeed();
    wire.weight = tm.cwKeyerWeight();
    wire.strictSpacing = tm.cwStrictSpacing();
    wire.reversePaddles = tm.cwReversePaddles();
    wire.sidetoneLevel = cwRadioSidetoneLevel(tm.cwSidetone(), tm.cwRadioSidetone(),
                                              m_radio.radioSpeakerMuted(), mode,
                                              tm.monitorVolume());
    wire.sidetoneFreqHz = m_radio.cwPitchHz();
    wire.rfDelayMs = cwRfDelayMs(tm.cwKeyDownDelayMs(), tm.cwKeyerSpeed());
    wire.hangMs = cwHangMs(mode, tm.cwBreakInDelayMs(), tm.cwKeyUpDelayMs());
    wire.qskTxAttenuation = result.allowed && mode == CwBreakInMode::Qsk;
    applyCwWireClamps(wire);

    const bool stateChanged = result.allowed != m_result.allowed
        || !(result.refusal == m_result.refusal) || mode != m_effectiveMode
        || qskReason != m_qskReason || semiReason != m_semiReason;
    const bool wireChanged = !(wire == m_wire);

    m_result = result;
    m_effectiveMode = mode;
    m_qskReason = qskReason;
    m_semiReason = semiReason;
    m_wire = wire;

    if (stateChanged) {
        emit keyerStateChanged();
    }
    if (wireChanged) {
        emit wireStateChanged(m_wire);
    }
}

} // namespace NereusSDR
