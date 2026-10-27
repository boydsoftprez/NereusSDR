// no-port-check: NereusSDR-original. The CW keyer controller: it reads the
// keyer gate's inputs from the models, decides which break-in modes the
// radio may use, and composes the CW wire state the connection sends. The
// formulas it calls live in CwWireState (cited there); no upstream logic is
// ported here.
// =================================================================
// src/models/CwKeyerController.h  (NereusSDR)
// =================================================================
//
// R-CW-05 to R-CW-07, R-CW-10, R-CW-13, R-CW-16, R-CW-20 and R-CW-22
// (availability), R-CW-38, D16. RadioModel owns one, pushes wireState() to
// the connection when it changes and once on connect, and feeds it the
// radio's key status (RadioConnection::radioKeyStatus) and every stop.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26: CW transmit, the CW keyer controller (R-CW-05, R-CW-06,
//               R-CW-07, R-CW-10, R-CW-13, R-CW-16, R-CW-20, R-CW-22,
//               R-CW-38, D16). J.J. Boyd (KG4VCF), AI-assisted via
//               Claude Code.
// =================================================================

#pragma once

#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>

#include "core/CwTypes.h"
#include "core/NereusCoreExport.h"
#include "core/codec/CwWireState.h"
#include "core/safety/CwKeyerGate.h"
#include "core/safety/TxRefusal.h"

namespace NereusSDR {

class RadioModel;
class SliceModel;

// R-CW-20: why QSK is not offered, in the order the rule checks them.
inline constexpr char kCwQskNeedsP2Reason[] =
    "QSK needs a radio on the newer network protocol.";
inline constexpr char kCwQskBoardReason[] = "This radio can't do QSK.";
inline constexpr char kCwQskFirmwareReason[] = "QSK needs radio firmware 1.7 or later.";
inline constexpr char kCwQskAntennaReason[] =
    "QSK needs the same antenna for receive and transmit.";
inline constexpr char kCwQskTransverterReason[] = "QSK is off on transverter bands.";
inline constexpr char kCwQskHl2Reason[] = "The Hermes Lite 2 can't do QSK.";
// R-CW-22 (D16): Semi and QSK while an amplifier interlock is armed.
inline constexpr char kCwAmpInterlockBreakInReason[] =
    "NereusSDR switches the amplifier, so the radio can't key it between characters. "
    "Use Off, or key the amplifier from the radio's PTT out.";

// R-CW-20: Orion and Orion MkII do QSK from firmware 17 (D2).
inline constexpr int kCwQskMinOrionFirmware = 17;

class NEREUS_CORE_EXPORT CwKeyerController : public QObject {
    Q_OBJECT

public:
    explicit CwKeyerController(RadioModel& radio, QObject* parent = nullptr);

    bool keyerAllowed() const { return m_result.allowed; }
    TxRefusal refusal() const { return m_result.refusal; }
    // The break-in mode the radio uses, after the QSK (R-CW-20) and D16
    // (R-CW-22) fallbacks.
    CwBreakInMode effectiveBreakInMode() const { return m_effectiveMode; }
    bool qskAvailable() const { return m_qskReason.isEmpty(); }
    QString qskUnavailableReason() const { return m_qskReason; }
    bool semiAvailable() const { return m_semiReason.isEmpty(); }
    QString semiUnavailableReason() const { return m_semiReason; }   // D16
    CwWireState wireState() const { return m_wire; }

    // Re-evaluates now: for an input with no change signal of its own
    // (RadioModel::setStationRadioChangeUnderway).
    void reevaluate();

    // The radio's key status (RadioConnection::radioKeyStatus), on the
    // main thread. A stop latch clears once the key is released.
    void onRadioKeyStatus(bool dot, bool dash, bool keyOut);
    bool radioKeyOut() const { return m_keyOut; }

    // An emergency stop or the TX time-out (R-CW-10): the keyer stays off
    // until the radio reports dot, dash and key-out all up. With the key
    // already up it clears at once, as MoxController::latchStopAllTx does
    // for a source no longer held.
    void latchStop();
    bool isStopLatched() const { return m_stopLatched; }

    // Test seam: how many times the inputs were evaluated.
    int evaluationCountForTest() const { return m_evaluations; }

signals:
    void keyerStateChanged();      // allowed, refusal, effective mode or availability changed
    void wireStateChanged(const NereusSDR::CwWireState& s);

private:
    void wireSlices();
    void wireSlice(SliceModel* slice);
    void evaluate();
    safety::CwKeyerGateInputs gateInputs() const;
    QString qskReasonNow(bool interlockArmed) const;
    // The transverter state: NereusSDR has no transverter transmit path yet
    // (UnbuiltFeature::Transverters), so no band is one. The QSK rule is
    // written against this so it applies when transverters land.
    bool txOnTransverterBand() const { return false; }
    bool interlockArmed() const;

    RadioModel& m_radio;
    QList<QPointer<SliceModel>> m_wiredSlices;

    safety::CwKeyerGateResult m_result;
    CwBreakInMode m_effectiveMode{CwBreakInMode::Off};
    QString m_qskReason;
    QString m_semiReason;
    CwWireState m_wire;

    bool m_dot{false};
    bool m_dash{false};
    bool m_keyOut{false};
    bool m_stopLatched{false};
    // D16: an armed interlock granted for the key now on (MOX in Off).
    bool m_interlockGranted{false};
    int m_evaluations{0};
};

} // namespace NereusSDR
