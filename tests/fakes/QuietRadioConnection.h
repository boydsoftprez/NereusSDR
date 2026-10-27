// no-port-check: NereusSDR-original.
// =================================================================
// tests/fakes/QuietRadioConnection.h  (NereusSDR)
// =================================================================
//
// A radio connection that reads Connected and does nothing: a test Core's
// radio for the CW keyer and the CWX sender (CW transmit, R-CW-48). It
// opens no socket, and nothing it is told reaches a radio.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26: CW transmit, remote CW (R-CW-48, R-CW-49). J.J. Boyd
//               (KG4VCF), AI-assisted via Claude Code.
// =================================================================

#pragma once

#include "core/RadioConnection.h"

namespace NereusSDR::Test {

class QuietRadioConnection final : public RadioConnection {
public:
    explicit QuietRadioConnection(QObject* parent = nullptr)
        : RadioConnection(parent)
    {
        setState(ConnectionState::Connected);
    }

    void init() override {}
    void connectToRadio(const NereusSDR::RadioInfo&) override {}
    void disconnect() override {}
    void setReceiverFrequency(int, quint64) override {}
    void setTxFrequency(quint64) override {}
    void setActiveReceiverCount(int) override {}
    void setSampleRate(int) override {}
    void setAttenuator(int) override {}
    void setPreamp(bool) override {}
    void setTxDrive(int) override {}
    void sendTxIq(const float*, int) override {}
    void setWatchdogEnabled(bool) override {}
    void setAntennaRouting(AntennaRouting) override {}
    void setMox(bool) override {}
    void setTrxRelay(bool) override {}
    void setMicBoost(bool) override {}
    void setLineIn(bool) override {}
    void setMicTipRing(bool) override {}
    void setMicBias(bool) override {}
    void setLineInGain(int) override {}
    void setUserDigOut(quint8) override {}
    void setPuresignalRun(bool) override {}
    void setMicPTTDisabled(bool) override {}
    void setMicXlr(bool) override {}
};

} // namespace NereusSDR::Test
