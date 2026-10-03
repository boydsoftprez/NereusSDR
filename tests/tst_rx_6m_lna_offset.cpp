// =================================================================
// tests/tst_rx_6m_lna_offset.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test file. The Thetis citations below
// document the upstream behaviour exercised; no C# is translated here. The
// logic under test (RadioModel::rx6mGainOffsetDb) cites console.cs
// txtVFOAFreq_LostFocus and RXCalibrationOffset [v2.10.3.15].
//
// The RX1 6 m LNA gain offset (Setup > Calibration, Rx1 6m LNA) enters the
// station's receive calibration (rxMeterOffsetDb) as its negative, on 6 m,
// on a radio with an Alex, when the 6 m LNA is in circuit:
//   From Thetis console.cs:31754-31771 [v2.10.3.15] (txtVFOAFreq_LostFocus, //DH1KLM)
//   and console.cs:21062-21067 (RXCalibrationOffset(1) adds _rx1_6m_gain_offset).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-28 - Written by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QScopeGuard>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/ConnectionState.h"
#include "core/HpsdrModel.h"
#include "core/RadioDiscovery.h"
#include "core/StepAttenuatorController.h"
#include "models/Band.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

namespace {

const QString kMac = QStringLiteral("AA:BB:CC:DD:6E:01");

// A local model of the given radio, on the given receive band, with its
// saved settings under kMac.
struct Bench {
    RadioModel model;
    StepAttenuatorController att;

    Bench(HPSDRModel radio, Band band)
    {
        model.setHpsdrModelForTest(radio);
        RadioInfo info;
        info.macAddress = kMac;
        info.boardType = boardForModel(radio);
        model.setLastRadioInfoForTest(info);
        model.setConnectionStateForTest(ConnectionState::Connected);
        att.setTickTimerEnabled(false);
        model.setStepAttController(&att);
        att.setBand(band);
    }
    ~Bench() { model.setStepAttController(nullptr); }

    void setFlag(const char* key, bool on)
    {
        AppSettings::instance().setHardwareValue(
            kMac, QString::fromLatin1(key), on ? QStringLiteral("True") : QStringLiteral("False"));
    }
};

// The station offset without the 6 m term: the same radio on 20 m.
double offsetOn20m(HPSDRModel radio)
{
    Bench b(radio, Band::Band20m);
    return b.model.rxMeterOffsetDb();
}

} // namespace

class TstRx6mLnaOffset : public QObject {
    Q_OBJECT

private slots:
    void init() { AppSettings::instance().clearHardwareValues(kMac); }
    void cleanup() { AppSettings::instance().clearHardwareValues(kMac); }

    void onSixMetersTheRx1OffsetLowersTheCalibration()
    {
        const double base = offsetOn20m(HPSDRModel::ANAN7000D);
        Bench b(HPSDRModel::ANAN7000D, Band::Band6m);
        // RX1_6mGainOffset = -RX6mGainOffset_RX1, 13 dB by default.
        QCOMPARE(b.model.rx6mGainOffsetDb(), -13.0);
        QCOMPARE(b.model.rxMeterOffsetDb(), base - 13.0);
        b.model.calibrationControllerMutable().setRx1_6mLnaOffset(10.0);
        QCOMPARE(b.model.rxMeterOffsetDb(), base - 10.0);
        // The Rx2 value does not enter the RX1 calibration.
        b.model.calibrationControllerMutable().setRx2_6mLnaOffset(20.0);
        QCOMPARE(b.model.rxMeterOffsetDb(), base - 10.0);
    }

    void offSixMetersThereIsNoOffset()
    {
        Bench b(HPSDRModel::ANAN7000D, Band::Band20m);
        QCOMPARE(b.model.rx6mGainOffsetDb(), 0.0);
    }

    void theLnaDisabledOnRxOrTheHpfBypassedRemovesIt()
    {
        {
            Bench b(HPSDRModel::ANAN7000D, Band::Band6m);
            b.setFlag("alex/master/disable6mLnaOnRx", true);
            QCOMPARE(b.model.rx6mGainOffsetDb(), 0.0);
        }
        AppSettings::instance().clearHardwareValues(kMac);
        {
            Bench b(HPSDRModel::ANAN7000D, Band::Band6m);
            b.setFlag("alex/master/hpfBypass", true);
            QCOMPARE(b.model.rx6mGainOffsetDb(), 0.0);
        }
    }

    // The 7000D / 8000D / AnvelinaPro3 / G2 / G2 1K / Red Pitaya group reads
    // the BPF1 6 m row's bypass (bpf1_6bp_bypass); the others the Alex HPF
    // 6 m row's (alex6bphpf_bypass).
    void theBandPassBoardsReadTheBpf1SixMeterBypass()
    {
        Bench b(HPSDRModel::ANAN_G2, Band::Band6m);
        b.setFlag("alex/hpf/6mBP/enabled", true);
        QCOMPARE(b.model.rx6mGainOffsetDb(), -13.0);
        b.setFlag("alex/bpf1/6mBP/enabled", true);
        QCOMPARE(b.model.rx6mGainOffsetDb(), 0.0);
    }

    void theOtherAlexBoardsReadTheHpfSixMeterBypass()
    {
        Bench b(HPSDRModel::ANAN100D, Band::Band6m);
        QCOMPARE(b.model.rx6mGainOffsetDb(), -13.0);
        b.setFlag("alex/bpf1/6mBP/enabled", true);
        QCOMPARE(b.model.rx6mGainOffsetDb(), -13.0);
        b.setFlag("alex/hpf/6mBP/enabled", true);
        QCOMPARE(b.model.rx6mGainOffsetDb(), 0.0);
    }

    void anan10AndRadiosWithoutAnAlexHaveNone()
    {
        for (const HPSDRModel radio : {HPSDRModel::ANAN10, HPSDRModel::ANAN10E,
                                       HPSDRModel::HERMESLITE}) {
            Bench b(radio, Band::Band6m);
            QCOMPARE(b.model.rx6mGainOffsetDb(), 0.0);
        }
    }

    void aChangeIsAnnounced()
    {
        Bench b(HPSDRModel::ANAN7000D, Band::Band6m);
        QSignalSpy spy(&b.model, &RadioModel::rxMeterOffsetChanged);
        b.model.calibrationControllerMutable().setRx1_6mLnaOffset(5.0);
        QCOMPARE(spy.size(), 1);
        QCOMPARE(spy.last().at(0).toDouble(), b.model.rxMeterOffsetDb());
    }

    // A remote window's change reaches the Core's calibration at once.
    void aWindowsSavedChangeReachesTheCore()
    {
        Bench b(HPSDRModel::ANAN7000D, Band::Band6m);
        const double before = b.model.rxMeterOffsetDb();
        QSignalSpy spy(&b.model, &RadioModel::rxMeterOffsetChanged);
        const QString key = QStringLiteral("hardware/%1/cal/rx1_6mLna").arg(kMac);
        AppSettings::instance().setValue(key, QStringLiteral("3"));
        b.model.scheduleRemoteHardwareApply(key);
        QTRY_COMPARE(spy.size(), 1);
        QCOMPARE(b.model.rxMeterOffsetDb(), before + 13.0 - 3.0);
        // And a later one, once the Core's PA table is already in place.
        AppSettings::instance().setValue(key, QStringLiteral("8"));
        b.model.scheduleRemoteHardwareApply(key);
        QTRY_COMPARE(spy.size(), 2);
        QCOMPARE(b.model.rxMeterOffsetDb(), before + 13.0 - 8.0);
    }

    // A remote window adds no offset of its own: the Core's readings and
    // frames already carry it.
    void aRemoteWindowAddsNone()
    {
        RadioModel remote(RadioModel::Role::Remote);
        remote.setHpsdrModelForTest(HPSDRModel::ANAN7000D);
        QCOMPARE(remote.rx6mGainOffsetDb(), 0.0);
        QCOMPARE(remote.rxMeterOffsetDb(), 0.0);
    }
};

QTEST_MAIN(TstRx6mLnaOffset)
#include "tst_rx_6m_lna_offset.moc"
