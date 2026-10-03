// no-port-check: NereusSDR-original test.
//
// tests/tst_local_step_attenuator_band_restore.cpp
//
// R-R3-46 / R-R3-11: with a local radio, each band remembers its attenuator
// and preamp, as it does through the Core. A local RadioModel with one slice,
// a StepAttenuatorController driving a fake radio connection, and the
// wiring MainWindow uses (RadioModel::followReceiveSliceWithStepAttenuator):
// a band change restores that band's last attenuation and preamp and sends
// them to the radio; a band never visited keeps the current setting; the
// memory survives a restart for that radio; a Remote model is not wired.
// No hardware; no audio device is opened by these objects.
//
// Modification history (NereusSDR):
//   2026-09-23: created (R-R3-46, R-R3-11), by J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-23: R-R3-46 fix wave: the attenuator follows slice A's band.

#include <QtTest/QtTest>

#include <memory>

#include "core/AppSettings.h"
#include "core/ConnectionState.h"
#include "core/RadioConnection.h"
#include "core/StepAttenuatorController.h"
#include "models/Band.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

constexpr double k20mHz = 14.200e6;
constexpr double k40mHz = 7.100e6;
constexpr double k17mHz = 18.100e6;

// The radio end of the controller: records what it was sent.
class FakeRadioConnection final : public RadioConnection {
    Q_OBJECT
public:
    explicit FakeRadioConnection(QObject* parent = nullptr)
        : RadioConnection(parent)
    {
        setState(ConnectionState::Connected);
    }

    QList<int> attenuator;
    QList<bool> preamp;

    void clear()
    {
        attenuator.clear();
        preamp.clear();
    }

    void init() override {}
    void connectToRadio(const NereusSDR::RadioInfo&) override {}
    void disconnect() override {}
    void setReceiverFrequency(int, quint64) override {}
    void setTxFrequency(quint64) override {}
    void setActiveReceiverCount(int) override {}
    void setSampleRate(int) override {}
    void setAttenuator(int dB) override { attenuator.append(dB); }
    void setPreamp(bool on) override { preamp.append(on); }
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

// A local window's pieces, wired the way MainWindow wires them on connect.
struct LocalWindow {
    RadioModel model;
    StepAttenuatorController controller;
    FakeRadioConnection radio;
    SliceModel* slice{nullptr};

    LocalWindow(const QString& mac, double startHz)
    {
        controller.setTickTimerEnabled(false);
        model.setStepAttController(&controller);
        model.addSlice();
        slice = model.txBoundSlice();
        if (slice) {
            slice->setFrequency(startHz);
        }
        model.followReceiveSliceWithStepAttenuator();
        controller.setRadioConnection(&radio);
        model.syncStepAttenuatorToReceiveSlice();
        controller.loadSettings(mac);
    }

    ~LocalWindow()
    {
        controller.setRadioConnection(nullptr);
        model.setStepAttController(nullptr);
    }
};

void startFromNothing(const QString& mac)
{
    AppSettings::instance().clearHardwareValues(mac);
    AppSettings::instance().save();
}

} // namespace

class TstLocalStepAttenuatorBandRestore : public QObject {
    Q_OBJECT

private slots:
    // 20 dB on 40 m, 0 dB last used on 20 m: each band change restores and
    // sends that band's value; the preamp follows the same way.
    void bandChangeRestoresAndSendsTheBandsAttenuatorAndPreamp()
    {
        const QString mac = QStringLiteral("02:00:00:00:46:51");
        startFromNothing(mac);
        LocalWindow w(mac, k20mHz);
        QVERIFY(w.slice != nullptr);
        QCOMPARE(w.slice->band(), Band::Band20m);

        // 20 m: 0 dB, preamp off.
        w.controller.setAttenuation(0);
        w.controller.setPreampMode(PreampMode::Off);

        // 40 m (never visited): keeps 0 dB, then the operator sets 20 dB
        // and turns the preamp on.
        w.slice->setFrequency(k40mHz);
        QCOMPARE(w.slice->band(), Band::Band40m);
        QCOMPARE(w.controller.attenuatorDb(), 0);
        w.controller.setAttenuation(20);
        w.controller.setPreampMode(PreampMode::On);
        QCOMPARE(w.controller.attenuatorDb(), 20);

        // Back to 20 m: 0 dB and preamp off are restored and reach the radio.
        w.radio.clear();
        w.slice->setFrequency(k20mHz);
        QCOMPARE(w.controller.attenuatorDb(), 0);
        QCOMPARE(w.controller.preampMode(), PreampMode::Off);
        QCOMPARE(w.radio.attenuator, QList<int>{0});
        QCOMPARE(w.radio.preamp, QList<bool>{false});

        // Back to 40 m: 20 dB and preamp on are restored and sent.
        w.radio.clear();
        w.slice->setFrequency(k40mHz);
        QCOMPARE(w.controller.attenuatorDb(), 20);
        QCOMPARE(w.controller.preampMode(), PreampMode::On);
        QCOMPARE(w.radio.attenuator, QList<int>{20});
        QCOMPARE(w.radio.preamp, QList<bool>{true});
        QVERIFY(w.controller.bandRestoreToRadio());
    }

    // A band never visited keeps the current setting and nothing is sent.
    void unvisitedBandKeepsTheCurrentSetting()
    {
        const QString mac = QStringLiteral("02:00:00:00:46:52");
        startFromNothing(mac);
        LocalWindow w(mac, k40mHz);
        QVERIFY(w.slice != nullptr);
        w.controller.setAttenuation(12);
        w.controller.setPreampMode(PreampMode::On);

        w.radio.clear();
        w.slice->setFrequency(k17mHz);
        QCOMPARE(w.slice->band(), Band::Band17m);
        QCOMPARE(w.controller.attenuatorDb(), 12);
        QCOMPARE(w.controller.preampMode(), PreampMode::On);
        QVERIFY(w.radio.attenuator.isEmpty());
        QVERIFY(w.radio.preamp.isEmpty());
    }

    // The per-band memory is saved for the radio and a new window (a
    // restart) restores it on the next band change.
    void perBandMemorySurvivesARestart()
    {
        const QString mac = QStringLiteral("02:00:00:00:46:53");
        startFromNothing(mac);
        {
            LocalWindow w(mac, k20mHz);
            QVERIFY(w.slice != nullptr);
            w.controller.setAttenuation(0);
            w.slice->setFrequency(k40mHz);
            w.controller.setAttenuation(20);
            w.controller.setPreampMode(PreampMode::On);
            w.slice->setFrequency(k20mHz);
            w.controller.saveSettings(mac);
        }

        LocalWindow w(mac, k20mHz);
        QVERIFY(w.slice != nullptr);
        QCOMPARE(w.controller.attenuatorDb(), 0);
        w.radio.clear();
        w.slice->setFrequency(k40mHz);
        QCOMPARE(w.controller.attenuatorDb(), 20);
        QCOMPARE(w.controller.preampMode(), PreampMode::On);
        QCOMPARE(w.radio.attenuator, QList<int>{20});
        QCOMPARE(w.radio.preamp, QList<bool>{true});
    }

    // The transmit-bound slice's mode reaches the controller too.
    void modeFollowsTheTransmitSlice()
    {
        const QString mac = QStringLiteral("02:00:00:00:46:54");
        startFromNothing(mac);
        LocalWindow w(mac, k40mHz);
        QVERIFY(w.slice != nullptr);
        w.slice->setDspMode(DSPMode::CWU);
        QCOMPARE(w.controller.currentDspMode(), DSPMode::CWU);
        w.slice->setDspMode(DSPMode::USB);
        QCOMPARE(w.controller.currentDspMode(), DSPMode::USB);
    }

    // R-R3-46 fix wave (Thetis parity, console.cs:17325 [v2.10.3.15]): the
    // attenuator belongs to the receive ADC and follows slice A's receive
    // band (Thetis rx1_band), not the transmit slice's. A cross-band split,
    // transmitting on slice B at 20 m while listening on slice A at 40 m,
    // keeps 40 m's attenuator; the ATT-on-TX band is slice B's.
    void crossBandSplitKeepsSliceABandsAttenuator()
    {
        const QString mac = QStringLiteral("02:00:00:00:46:55");
        startFromNothing(mac);
        LocalWindow w(mac, k20mHz);
        QVERIFY(w.slice != nullptr);
        QCOMPARE(w.slice->sliceIndex(), 0);
        w.controller.setAttenuation(0);      // 20 m's memory
        w.slice->setFrequency(k40mHz);
        w.controller.setAttenuation(20);     // 40 m's

        w.model.setBoardForTest(HPSDRHW::OrionMKII);
        w.model.setConnectionStateForTest(ConnectionState::Connected);
        const int before = static_cast<int>(w.model.slices().size());
        w.model.addSlice();
        QCOMPARE(static_cast<int>(w.model.slices().size()), before + 1);
        SliceModel* const sliceB = w.model.slices().last();
        QVERIFY(sliceB != nullptr && sliceB->sliceIndex() != 0);
        sliceB->setFrequency(k20mHz);
        w.radio.clear();
        QVERIFY(w.model.requestTxHandoffToSlice(sliceB->sliceIndex()));
        QTRY_COMPARE(w.model.txBoundSlice(), sliceB);

        QCOMPARE(w.controller.currentBand(), Band::Band40m);
        QCOMPARE(w.controller.attenuatorDb(), 20);
        QVERIFY(w.radio.attenuator.isEmpty());
        QCOMPARE(w.controller.txBand(), Band::Band20m);

        // Slice B moving changes the transmit band only.
        sliceB->setFrequency(k17mHz);
        QCOMPARE(w.controller.attenuatorDb(), 20);
        QCOMPARE(w.controller.txBand(), Band::Band17m);
        // Slice A moving restores its new band's memory and sends it.
        w.slice->setFrequency(k20mHz);
        QCOMPARE(w.controller.attenuatorDb(), 0);
        QCOMPARE(w.radio.attenuator, QList<int>{0});
        QCOMPARE(w.controller.txBand(), Band::Band17m);
        w.model.setConnectionStateForTest(ConnectionState::Disconnected);
    }

    // A remote window is unchanged: the Core restores and sends, so a
    // Remote model never wires its (radio-less) controller to its slices.
    void remoteModelIsNotWired()
    {
        RadioModel remote(RadioModel::Role::Remote);
        StepAttenuatorController controller;
        controller.setTickTimerEnabled(false);
        // A Remote model keeps no controller of its own in a window; this
        // one stands in for the window's, to show nothing reaches it.
        remote.setStepAttController(&controller);
        remote.followReceiveSliceWithStepAttenuator();
        QVERIFY(!controller.bandRestoreToRadio());
        remote.syncStepAttenuatorToReceiveSlice();
        QCOMPARE(controller.currentDspMode(), DSPMode::LSB);
        remote.setStepAttController(nullptr);
    }
};

QTEST_MAIN(TstLocalStepAttenuatorBandRestore)
#include "tst_local_step_attenuator_band_restore.moc"
