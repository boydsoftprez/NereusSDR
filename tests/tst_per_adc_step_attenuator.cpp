// no-port-check: NereusSDR-original test.
// =================================================================
// tests/tst_per_adc_step_attenuator.cpp  (NereusSDR)
// =================================================================
//
// R-R3-46 / R-R3-11: a step attenuator per receive ADC, as Thetis keeps
// RX1's and RX2's and sends each to the ADC that receiver uses
// (console.cs RX1AttenuatorData / RX2AttenuatorData, GetADCInUse), and a
// receive offset per ADC (console.cs RXOffset(rx)). Slice A carries RX1's
// value; the other ADC in use carries its own, following the band of the
// lowest-numbered slice on it; slices on one ADC share its value; linked
// diversity forces one value on both.
//
// The bench bug this pins: with 20 dB on ADC0 (slice A), a slice on EXT1
// (ADC1) read 20 dB too high, and ADC1's attenuator byte was never sent.
//
// Receive side only: nothing here keys a radio or opens a socket to one.
// =================================================================
// Modification history (NereusSDR):
//   2026-09-28: original test for NereusSDR by J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-29: Level Cal fix wave: RX2's preamp mode cases, by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-30: Level Cal 2: every 1 dB step of RX2's step attenuator on
//               the wire per protocol and model, and the HPSDR's two
//               Mercury states. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-30: Level Cal 2 review: RX2 above its 0-31 dB field never
//               wraps on either protocol. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-30: Level Cal 2 re-review: a stored RX2 value above 31 loads
//               and restores as 31. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/ConnectionState.h"
#include "core/HpsdrModel.h"
#include "core/P1RadioConnection.h"
#include "core/P2RadioConnection.h"
#include "core/RadioConnection.h"
#include "core/StepAttenuatorController.h"
#include "core/StepAttenuatorFacade.h"
#include "models/Band.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "OperatorWording.h"

#include <memory>

using namespace NereusSDR;

namespace {

constexpr double k20mHz = 14.200e6;
constexpr double k40mHz = 7.100e6;
constexpr double k17mHz = 18.100e6;

// CmdHighPriority step attenuator bytes (Thetis network.c CmdHighPriority).
constexpr int kAdc0AttByte = 1443;
constexpr int kAdc1AttByte = 1442;

using Sends = QList<QPair<int, int>>;

// Records every per-ADC attenuator send.
class RecordingConnection final : public RadioConnection {
    Q_OBJECT
public:
    explicit RecordingConnection(QObject* parent = nullptr)
        : RadioConnection(parent)
    {
        setState(ConnectionState::Connected);
    }

    Sends sends;
    QList<bool> preamp;
    QList<bool> rx2Preamp;

    void init() override {}
    void connectToRadio(const NereusSDR::RadioInfo&) override {}
    void disconnect() override {}
    void setReceiverFrequency(int, quint64) override {}
    void setTxFrequency(quint64) override {}
    void setActiveReceiverCount(int) override {}
    void setSampleRate(int) override {}
    void setAttenuator(int dB) override { sends.append({0, dB}); }
    void setAttenuatorForAdc(int adc, int dB) override { sends.append({adc, dB}); }
    void setPreamp(bool on) override { preamp.append(on); }
    void setRx2Preamp(bool on) override { rx2Preamp.append(on); }
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

void startFromNothing(const QString& mac)
{
    AppSettings::instance().clearHardwareValues(mac);
    AppSettings::instance().save();
}

// A controller with its settings loaded for `mac` (band memory live).
void loadController(StepAttenuatorController& ctrl, const QString& mac)
{
    startFromNothing(mac);
    ctrl.setTickTimerEnabled(false);
    ctrl.setMinAttenuation(0);
    ctrl.setMaxAttenuation(31);
    ctrl.loadSettings(mac);
}

// Thetis BUFLEN, the CmdHighPriority packet length.
constexpr int kHighPriorityLen = 1444;

QByteArray highPriority(const P2RadioConnection& conn)
{
    quint8 buf[kHighPriorityLen] = {};
    conn.composeCmdHighPriorityForTest(buf);
    return QByteArray(reinterpret_cast<const char*>(buf), kHighPriorityLen);
}

int byteAt(const QByteArray& buf, int index)
{
    return static_cast<quint8>(buf.at(index));
}

// An ANAN-G2 (two ADCs) on Protocol 2 with a step attenuator following
// slice A, and slices A and B bound to streams 0 and 1.
struct G2Station {
    P2RadioConnection conn;
    RadioModel model;
    StepAttenuatorController ctrl;
    SliceModel* sliceA{nullptr};
    SliceModel* sliceB{nullptr};

    explicit G2Station(const QString& mac)
    {
        // The ANAN-G2's own codec on the connection, so the model's DDC
        // assignment runs and puts a slice on the ADC its antenna feeds.
        conn.setBoardForTest(HPSDRHW::Saturn);
        model.injectConnectionForTest(&conn);
        model.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
        RadioInfo info;
        info.protocol = ProtocolVersion::Protocol2;
        model.setLastRadioInfoForTest(info);
        model.configureStreamPool(4, 4, 192000);
        loadController(ctrl, mac);
        model.setStepAttController(&ctrl);
        sliceA = model.sliceById(model.addSlice());
        sliceB = model.sliceById(model.addSlice());
        if (sliceA) { sliceA->setFrequency(k20mHz); }
        if (sliceB) { sliceB->setFrequency(k40mHz); }
        model.followReceiveSliceWithStepAttenuator();
        ctrl.setRadioConnection(&conn);
    }

    ~G2Station()
    {
        ctrl.setRadioConnection(nullptr);
        model.setStepAttController(nullptr);
    }

    // Each slice on the ADC given, the way the operator does it: an
    // RX-only input (EXT1) puts a slice on ADC1, ANT1 on ADC0
    // (P2CodecOrionMkII's antenna-to-ADC rule, run by the model's own
    // DDC assignment).
    // The antenna is kept per band and synced onto the active slice, so
    // each slice is selected before its antenna is picked, as the operator
    // does.
    void route(int adcA, int adcB)
    {
        pick(sliceA, adcA);
        pick(sliceB, adcB);
    }
    void pick(SliceModel* slice, int adc)
    {
        model.setActiveSlice(slice->sliceIndex());
        slice->setRxAntenna(adc == 1 ? QStringLiteral("EXT1") : QStringLiteral("ANT1"));
    }
};

} // namespace

class TstPerAdcStepAttenuator : public QObject {
    Q_OBJECT

private slots:
    // The bench bug: 20 dB on ADC0 (slice A), slice B on EXT1 (ADC1) with
    // no attenuation of its own reads with 0 dB of offset, not 20.
    void sliceOnAdc1ReadsItsOwnAdcsOffset()
    {
        G2Station s(QStringLiteral("02:00:00:00:ad:01"));
        QVERIFY(s.sliceA && s.sliceB);
        QVERIFY(s.sliceA->streamIndex() == 0 && s.sliceB->streamIndex() == 1);
        s.route(0, 1);
        QCOMPARE(s.model.sliceAdcIndex(s.sliceA->sliceIndex()), 0);
        QCOMPARE(s.model.sliceAdcIndex(s.sliceB->sliceIndex()), 1);
        QCOMPARE(s.ctrl.rx1Adc(), 0);
        QCOMPARE(s.ctrl.rx2Adc(), 1);

        s.ctrl.setAttenuation(20);
        const double cal = s.model.rxMeterCalOffsetDb();
        QCOMPARE(s.model.rxMeterOffsetDbForSlice(s.sliceA->sliceIndex()), cal + 20.0);
        QCOMPARE(s.model.rxMeterOffsetDbForSlice(s.sliceB->sliceIndex()), cal);
        // Each stream's spectrum the same way.
        QCOMPARE(s.model.rxMeterOffsetDbForStream(s.sliceA->streamIndex()), cal + 20.0);
        QCOMPARE(s.model.rxMeterOffsetDbForStream(s.sliceB->streamIndex()), cal);
        // Slice A's reading (RX1's) is unchanged.
        QCOMPARE(s.model.rxMeterOffsetDb(), cal + 20.0);

        // ADC1's own attenuation moves only slice B's offset.
        s.ctrl.setRx2Attenuation(12);
        QCOMPARE(s.model.rxMeterOffsetDbForSlice(s.sliceA->sliceIndex()), cal + 20.0);
        QCOMPARE(s.model.rxMeterOffsetDbForSlice(s.sliceB->sliceIndex()), cal + 12.0);
    }

    // ADC1 receives its own attenuator byte; ADC0 keeps slice A's.
    void adc1GetsItsOwnByteOnTheWire()
    {
        G2Station s(QStringLiteral("02:00:00:00:ad:02"));
        QVERIFY(s.sliceA && s.sliceB);
        s.route(0, 1);
        s.ctrl.setAttenuation(20);
        s.ctrl.setRx2Attenuation(12);
        QByteArray hp = highPriority(s.conn);
        QCOMPARE(byteAt(hp, kAdc0AttByte), 20);
        QCOMPARE(byteAt(hp, kAdc1AttByte), 12);

        // Through the slice's ADC: B's edit lands on ADC1 only.
        s.ctrl.setAttenuationForAdc(s.model.sliceAdcIndex(s.sliceB->sliceIndex()), 7);
        hp = highPriority(s.conn);
        QCOMPARE(byteAt(hp, kAdc0AttByte), 20);
        QCOMPARE(byteAt(hp, kAdc1AttByte), 7);
        QCOMPARE(s.ctrl.attenuatorDb(), 20);
        QCOMPARE(s.ctrl.rx2AttenuatorDb(), 7);
    }

    // The connection's own per-ADC setter writes the ADC1 byte.
    void p2SetterWritesTheAdc1Byte()
    {
        P2RadioConnection conn;
        conn.setAttenuatorForAdc(1, 9);
        conn.setAttenuatorForAdc(0, 4);
        const QByteArray hp = highPriority(conn);
        QCOMPARE(byteAt(hp, kAdc1AttByte), 9);
        QCOMPARE(byteAt(hp, kAdc0AttByte), 4);
    }

    // Protocol 1 carries ADC1's attenuator in bank 12, ADC0's in bank 11.
    void p1SetterWritesBank12()
    {
        P1RadioConnection conn;
        conn.setAttenuatorForAdc(1, 17);
        conn.setAttenuatorForAdc(0, 5);
        QCOMPARE(conn.currentAttenForAdcForTest(1), 17);
        QCOMPARE(conn.currentAttenForAdcForTest(0), 5);
        quint8 bank12[5] = {};
        conn.composeCcForBankForTest(12, bank12);
        QCOMPARE(int(bank12[1]), 17 | 0x20);
        quint8 bank11[5] = {};
        conn.composeCcForBankForTest(11, bank11);
        QCOMPARE(int(bank11[4]), 5 | 0x20);
    }

    // Two slices on ADC0 share one attenuator: one value, one offset, one
    // byte, and ADC1's byte is left alone.
    void twoSlicesOnAdc0ShareOneAttenuator()
    {
        G2Station s(QStringLiteral("02:00:00:00:ad:03"));
        QVERIFY(s.sliceA && s.sliceB);
        s.route(0, 0);
        QCOMPARE(s.ctrl.rx2Adc(), -1);
        QVERIFY(s.ctrl.adcUsesRx1Attenuator(0));

        s.ctrl.setAttenuationForAdc(s.model.sliceAdcIndex(s.sliceB->sliceIndex()), 15);
        QCOMPARE(s.ctrl.attenuatorDb(), 15);
        QCOMPARE(s.ctrl.attenuatorDbForAdc(0), 15);
        QCOMPARE(s.model.rxMeterOffsetDbForSlice(s.sliceA->sliceIndex()),
                 s.model.rxMeterOffsetDbForSlice(s.sliceB->sliceIndex()));
        const QByteArray hp = highPriority(s.conn);
        QCOMPARE(byteAt(hp, kAdc0AttByte), 15);
        QCOMPARE(byteAt(hp, kAdc1AttByte), 0);
    }

    // Diversity links the two ADCs: both take slice A's value, and an edit
    // of either sets both.
    void diversityForcesEqualValues()
    {
        G2Station s(QStringLiteral("02:00:00:00:ad:04"));
        QVERIFY(s.sliceA && s.sliceB);
        s.route(0, 1);
        s.ctrl.setAttenuation(20);
        s.ctrl.setRx2Attenuation(12);
        QVERIFY(!s.ctrl.adcAttenuatorsLinked());

        s.model.setDdcContextForTest(false, false, true);
        s.model.syncStepAttenuatorAdcRouting();
        QVERIFY(s.ctrl.adcAttenuatorsLinked());
        QCOMPARE(s.ctrl.rx2AttenuatorDb(), 20);
        QCOMPARE(s.ctrl.attenuatorDbForAdc(1), 20);
        QByteArray hp = highPriority(s.conn);
        QCOMPARE(byteAt(hp, kAdc0AttByte), 20);
        QCOMPARE(byteAt(hp, kAdc1AttByte), 20);
        const double cal = s.model.rxMeterCalOffsetDb();
        QCOMPARE(s.model.rxMeterOffsetDbForSlice(s.sliceB->sliceIndex()), cal + 20.0);

        // The other ADC's edit sets RX1's, and both go out.
        s.ctrl.setRx2Attenuation(5);
        QCOMPARE(s.ctrl.attenuatorDb(), 5);
        QCOMPARE(s.ctrl.rx2AttenuatorDb(), 5);
        hp = highPriority(s.conn);
        QCOMPARE(byteAt(hp, kAdc0AttByte), 5);
        QCOMPARE(byteAt(hp, kAdc1AttByte), 5);

        // And slice A's.
        s.ctrl.setAttenuation(9);
        QCOMPARE(s.ctrl.rx2AttenuatorDb(), 9);
        hp = highPriority(s.conn);
        QCOMPARE(byteAt(hp, kAdc0AttByte), 9);
        QCOMPARE(byteAt(hp, kAdc1AttByte), 9);
    }

    // Slice A moving to ADC1 (an RX-only input) takes RX1's value with it;
    // slice B, now on ADC0, gets the other ADC's value there.
    void aSwapMovesEachValueToItsNewAdc()
    {
        G2Station s(QStringLiteral("02:00:00:00:ad:05"));
        QVERIFY(s.sliceA && s.sliceB);
        s.route(0, 1);
        s.ctrl.setAttenuation(20);
        s.ctrl.setRx2Attenuation(12);

        s.route(1, 0);
        QCOMPARE(s.ctrl.rx1Adc(), 1);
        QCOMPARE(s.ctrl.rx2Adc(), 0);
        const QByteArray hp = highPriority(s.conn);
        QCOMPARE(byteAt(hp, kAdc1AttByte), 20);
        QCOMPARE(byteAt(hp, kAdc0AttByte), 12);
        const double cal = s.model.rxMeterCalOffsetDb();
        QCOMPARE(s.model.rxMeterOffsetDbForSlice(s.sliceA->sliceIndex()), cal + 20.0);
        QCOMPARE(s.model.rxMeterOffsetDbForSlice(s.sliceB->sliceIndex()), cal + 12.0);
    }

    // The other ADC's attenuator keeps its own band memory, following the
    // band of the slice on it (Thetis rx2_step_attenuator_by_band).
    void theOtherAdcRemembersItsValuePerBand()
    {
        G2Station s(QStringLiteral("02:00:00:00:ad:06"));
        QVERIFY(s.sliceA && s.sliceB);
        s.route(0, 1);
        QCOMPARE(s.ctrl.rx2Band(), Band::Band40m);
        s.ctrl.setRx2Attenuation(12);

        s.sliceB->setFrequency(k17mHz);
        s.pick(s.sliceB, 1);  // the antenna is kept per band
        QCOMPARE(s.model.sliceAdcIndex(s.sliceB->sliceIndex()), 1);
        QCOMPARE(s.ctrl.rx2Band(), Band::Band17m);
        QCOMPARE(s.ctrl.rx2AttenuatorDb(), 12);  // never visited: kept
        s.ctrl.setRx2Attenuation(3);
        QCOMPARE(byteAt(highPriority(s.conn), kAdc1AttByte), 3);

        s.sliceB->setFrequency(k40mHz);
        QCOMPARE(s.model.sliceAdcIndex(s.sliceB->sliceIndex()), 1);
        QCOMPARE(s.ctrl.rx2Band(), Band::Band40m);
        QCOMPARE(s.ctrl.rx2AttenuatorDb(), 12);
        QCOMPARE(byteAt(highPriority(s.conn), kAdc1AttByte), 12);
        // Slice A's attenuator never moved.
        QCOMPARE(s.ctrl.attenuatorDb(), 0);
        QCOMPARE(byteAt(highPriority(s.conn), kAdc0AttByte), 0);
    }

    // The other ADC's value and band memory survive a restart, and the
    // restored value reaches the radio.
    void theOtherAdcsValueSurvivesARestart()
    {
        const QString mac = QStringLiteral("02:00:00:00:ad:07");
        startFromNothing(mac);
        {
            StepAttenuatorController ctrl;
            ctrl.setTickTimerEnabled(false);
            ctrl.setMaxAttenuation(31);
            ctrl.loadSettings(mac);
            ctrl.setAdcRouting(0, 1, Band::Band40m, false);
            ctrl.setRx2Attenuation(14);
            ctrl.setAdcRouting(0, 1, Band::Band20m, false);
            ctrl.setRx2Attenuation(6);
            ctrl.saveSettings(mac);
        }
        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);
        ctrl.setMaxAttenuation(31);
        RecordingConnection radio;
        ctrl.setAdcRouting(0, 1, Band::Band20m, false);
        ctrl.setRadioConnection(&radio);
        radio.sends.clear();
        ctrl.loadSettings(mac);
        QCOMPARE(ctrl.rx2AttenuatorDb(), 6);
        QVERIFY(radio.sends.contains(QPair<int, int>(1, 6)));

        radio.sends.clear();
        ctrl.setBandRestoreToRadio(true);
        ctrl.setAdcRouting(0, 1, Band::Band40m, false);
        QCOMPARE(ctrl.rx2AttenuatorDb(), 14);
        QCOMPARE(radio.sends, (Sends{{1, 14}}));
        ctrl.setRadioConnection(nullptr);
    }

    // Slice A's attenuation and preamp restored at connect reach the radio
    // then, as Thetis sends RX1's (and RX2's) stored values when the radio
    // starts (console.cs InitConsole, 2175-2178, and SetComboPreampForHPSDR).
    void theValuesRestoredAtConnectReachTheRadio()
    {
        const QString mac = QStringLiteral("02:00:00:00:ad:09");
        startFromNothing(mac);
        {
            StepAttenuatorController ctrl;
            ctrl.setTickTimerEnabled(false);
            ctrl.setMaxAttenuation(31);
            ctrl.loadSettings(mac);
            ctrl.setAttenuation(17);
            ctrl.setPreampMode(PreampMode::On);
            ctrl.saveSettings(mac);
        }
        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);
        ctrl.setMaxAttenuation(31);
        RecordingConnection radio;
        ctrl.setRadioConnection(&radio);
        radio.sends.clear();
        radio.preamp.clear();
        ctrl.loadSettings(mac);
        QCOMPARE(ctrl.attenuatorDb(), 17);
        QVERIFY(radio.sends.contains(QPair<int, int>(0, 17)));
        QCOMPARE(radio.preamp, QList<bool>{true});
        ctrl.setRadioConnection(nullptr);
    }

    // Auto-attenuate acts per receiver on the ADC that overloaded, as Thetis
    // does (console.cs 21584-21700): RX1 reacts only to slice A's ADC, RX2
    // only to the other ADC in use, each with its own undo.
    void autoAttenuateActsOnTheAdcThatOverloaded()
    {
        StepAttenuatorController ctrl;
        loadController(ctrl, QStringLiteral("02:00:00:00:ad:0a"));
        RecordingConnection radio;
        ctrl.setRadioConnection(&radio);
        ctrl.setAdcRouting(0, 1, Band::Band20m, false, 1u << 1);
        ctrl.setAutoAttMode(AutoAttMode::Classic);
        ctrl.setAutoAttEnabled(true);
        // RX2 has its own auto-attenuate settings (Thetis _auto_att_rx2,
        // _auto_att_undo_rx2, _auto_att_hold_delay_rx2).
        ctrl.setRx2AutoAttEnabled(true);
        ctrl.setRx2AutoAttUndo(true);
        ctrl.setRx2AutoUndoDelaySec(0);
        radio.sends.clear();

        const auto overload = [&ctrl](int adc) {
            for (int i = 0; i < 4; ++i) {  // red once the level passes 3
                ctrl.onAdcOverflow(adc);
                ctrl.tick();
            }
        };

        // ADC1 (slice B's) overloads: only the other ADC's attenuator moves.
        overload(1);
        QCOMPARE(ctrl.attenuatorDb(), 0);
        QVERIFY(ctrl.rx2AttenuatorDb() > 0);
        for (const auto& send : radio.sends) {
            QCOMPARE(send.first, 1);
        }
        // It clears: undo puts it back (undo delay off).
        ctrl.tick();
        QCOMPARE(ctrl.rx2AttenuatorDb(), 0);
        QCOMPARE(ctrl.attenuatorDb(), 0);

        // ADC0 (slice A's) overloads: only slice A's attenuator moves.
        radio.sends.clear();
        overload(0);
        QVERIFY(ctrl.attenuatorDb() > 0);
        QCOMPARE(ctrl.rx2AttenuatorDb(), 0);
        for (const auto& send : radio.sends) {
            QCOMPARE(send.first, 0);
        }
        ctrl.tick();
        QCOMPARE(ctrl.attenuatorDb(), 0);

        // No slice on ADC1: its overload moves nothing (no receiver there).
        ctrl.setAdcRouting(0, -1, Band::Band20m, false);
        overload(1);
        QCOMPARE(ctrl.attenuatorDb(), 0);
        QCOMPARE(ctrl.rx2AttenuatorDb(), 0);
        ctrl.setRadioConnection(nullptr);
    }

    // RX2 has its own step attenuator enable (Thetis _rx2_step_att_enabled):
    // off, slice B's offset is the second preamp's (Thetis rx2_preamp_offset,
    // HPSDR_OFF 20 dB); slice A keeps RX1's.
    void rx2HasItsOwnStepAttEnable()
    {
        G2Station s(QStringLiteral("02:00:00:00:ad:0b"));
        QVERIFY(s.sliceA && s.sliceB);
        s.route(0, 1);
        QVERIFY(s.ctrl.rx2StepAttEnabled());
        s.ctrl.setAttenuation(6);
        s.ctrl.setRx2Attenuation(12);
        const double cal = s.model.rxMeterCalOffsetDb();
        QCOMPARE(s.model.rxMeterOffsetDbForSlice(s.sliceB->sliceIndex()), cal + 12.0);

        QSignalSpy changed(&s.ctrl, &StepAttenuatorController::rx2StepAttEnabledChanged);
        s.ctrl.setRx2StepAttEnabled(false);
        QCOMPARE(changed.count(), 1);
        QVERIFY(s.ctrl.stepAttEnabled());  // RX1's is its own on another ADC
        QCOMPARE(s.model.rxMeterOffsetDbForSlice(s.sliceB->sliceIndex()), cal + 20.0);
        QCOMPARE(s.model.rxMeterOffsetDbForSlice(s.sliceA->sliceIndex()), cal + 6.0);
    }

    // Receivers on one ADC keep one enable, as Thetis Setup mirrors
    // chkRX2StepAtt and chkHermesStepAttenuator when the ADCs are the same.
    void onOneAdcTheTwoEnablesMoveTogether()
    {
        StepAttenuatorController ctrl;
        loadController(ctrl, QStringLiteral("02:00:00:00:ad:0c"));
        ctrl.setAdcRouting(0, -1, Band::Band20m, false);
        ctrl.setRx2StepAttEnabled(false);
        QVERIFY(!ctrl.stepAttEnabled());
        ctrl.setStepAttEnabled(true);
        QVERIFY(ctrl.rx2StepAttEnabled());
        // A second ADC in use: each is its own.
        ctrl.setAdcRouting(0, 1, Band::Band20m, false);
        ctrl.setRx2StepAttEnabled(false);
        QVERIFY(ctrl.stepAttEnabled());
        // Back on one ADC: RX2's follows RX1's (Thetis updateAttenuationInfo).
        ctrl.setAdcRouting(0, -1, Band::Band20m, false);
        QVERIFY(ctrl.rx2StepAttEnabled());
    }

    // RX2's auto-attenuate runs on its own settings: RX1's off does not stop
    // it, and with its undo off the raised value stays when the overload
    // clears (Thetis restores only with _auto_att_undo_rx2).
    void rx2AutoAttenuateHasItsOwnSettings()
    {
        StepAttenuatorController ctrl;
        loadController(ctrl, QStringLiteral("02:00:00:00:ad:0d"));
        ctrl.setAdcRouting(0, 1, Band::Band20m, false, 1u << 1);
        QVERIFY(!ctrl.autoAttEnabled());
        QVERIFY(!ctrl.rx2AutoAttEnabled());
        QVERIFY(!ctrl.rx2AutoAttUndo());
        QCOMPARE(ctrl.rx2AutoUndoDelaySec(), 5);
        ctrl.setRx2AutoAttEnabled(true);
        const auto overload = [&ctrl](int adc) {
            for (int i = 0; i < 4; ++i) {
                ctrl.onAdcOverflow(adc);
                ctrl.tick();
            }
        };
        overload(1);
        const int raised = ctrl.rx2AttenuatorDb();
        QVERIFY(raised > 0);
        ctrl.tick();
        QCOMPARE(ctrl.rx2AttenuatorDb(), raised);  // undo off: kept
        // RX1's auto-attenuate is off: ADC0's overload moves nothing.
        overload(0);
        QCOMPARE(ctrl.attenuatorDb(), 0);

        ctrl.setRx2AutoAttUndo(true);
        ctrl.setRx2AutoUndoDelaySec(0);
        overload(1);
        QVERIFY(ctrl.rx2AttenuatorDb() > raised);
        ctrl.tick();
        QCOMPARE(ctrl.rx2AttenuatorDb(), raised);  // undo on: put back
    }

    // RX2's enable starts at Thetis's default (console.cs:11109,
    // _rx2_step_att_enabled = false) when the radio has none saved and RX2
    // is on its own ADC; a saved value is kept.
    void rx2EnableDefaultsToThetissAndKeepsASavedValue()
    {
        const QString mac = QStringLiteral("02:00:00:00:ad:0f");
        startFromNothing(mac);
        {
            StepAttenuatorController ctrl;
            ctrl.setTickTimerEnabled(false);
            ctrl.setAdcRouting(0, 1, Band::Band20m, false);
            ctrl.loadSettings(mac);
            QVERIFY(ctrl.stepAttEnabled());
            QVERIFY(!ctrl.rx2StepAttEnabled());
            ctrl.setRx2StepAttEnabled(true);
            ctrl.saveSettings(mac);
        }
        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);
        ctrl.setAdcRouting(0, 1, Band::Band20m, false);
        ctrl.loadSettings(mac);
        QVERIFY(ctrl.rx2StepAttEnabled());
    }

    // RX2's enable and auto-attenuate settings are saved for the radio.
    void rx2SettingsSurviveARestart()
    {
        const QString mac = QStringLiteral("02:00:00:00:ad:0e");
        startFromNothing(mac);
        {
            StepAttenuatorController ctrl;
            ctrl.setTickTimerEnabled(false);
            ctrl.loadSettings(mac);
            ctrl.setAdcRouting(0, 1, Band::Band20m, false);
            ctrl.setRx2StepAttEnabled(false);
            ctrl.setRx2AutoAttEnabled(true);
            ctrl.setRx2AutoAttUndo(true);
            ctrl.setRx2AutoUndoDelaySec(9);
            ctrl.saveSettings(mac);
        }
        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);
        ctrl.setAdcRouting(0, 1, Band::Band20m, false);
        ctrl.loadSettings(mac);
        QVERIFY(!ctrl.rx2StepAttEnabled());
        QVERIFY(ctrl.stepAttEnabled());
        QVERIFY(ctrl.rx2AutoAttEnabled());
        QVERIFY(ctrl.rx2AutoAttUndo());
        QCOMPARE(ctrl.rx2AutoUndoDelaySec(), 9);
    }

    // Controller sends: slice A's value only to slice A's ADC, the other
    // ADC's only to it, and a routing call that moves nothing sends nothing.
    void eachValueGoesOnlyToItsAdc()
    {
        StepAttenuatorController ctrl;
        loadController(ctrl, QStringLiteral("02:00:00:00:ad:08"));
        RecordingConnection radio;
        ctrl.setRadioConnection(&radio);
        ctrl.setAdcRouting(0, 1, Band::Band20m, false);
        radio.sends.clear();

        ctrl.setAttenuation(18);
        QCOMPARE(radio.sends, (Sends{{0, 18}}));
        radio.sends.clear();
        ctrl.setRx2Attenuation(11);
        QCOMPARE(radio.sends, (Sends{{1, 11}}));
        radio.sends.clear();
        ctrl.setAdcRouting(0, 1, Band::Band20m, false);
        QVERIFY(radio.sends.isEmpty());

        // A single-ADC radio (every slice on ADC0, the Hermes Lite 2 among
        // them): the other ADC's value is never sent.
        ctrl.setAdcRouting(0, -1, Band::Band20m, false);
        radio.sends.clear();
        ctrl.setAttenuationForAdc(0, 9);
        QCOMPARE(radio.sends, (Sends{{0, 9}}));
        QCOMPARE(ctrl.attenuatorDbForAdc(1), 9);  // an unused ADC reads RX1's
        ctrl.setRadioConnection(nullptr);
    }

    // Level Cal fix wave: RX2's preamp mode, as Thetis's RX2PreampMode
    // setter drives it (console.cs:19431-19505 [v2.10.3.15]). On a G2 with
    // RX2's step attenuator off, each mode puts its attenuation on RX2's
    // ADC; no board but the HPSDR takes a preamp bit.
    void rx2PreampModeDrivesTheOtherAdcOnAG2()
    {
        StepAttenuatorController ctrl;
        ctrl.setBoardIdentity(HPSDRHW::Saturn, HPSDRModel::ANAN_G2, true);
        loadController(ctrl, QStringLiteral("02:00:00:00:ad:20"));
        RecordingConnection radio;
        ctrl.setRadioConnection(&radio);
        ctrl.setAdcRouting(0, 1, Band::Band20m, false, 1u << 1);
        ctrl.setRx2StepAttEnabled(false);
        struct Row { PreampMode mode; int att; };
        const Row rows[] = {
            {PreampMode::Minus10,   10},
            {PreampMode::Off,       20},
            {PreampMode::Minus20,   20},
            {PreampMode::Minus30,   30},
            {PreampMode::SaMinus10, 10},
            {PreampMode::SaMinus20, 20},
            {PreampMode::SaMinus30, 30},
            {PreampMode::On,        0},
        };
        const PreampMode rx1 = ctrl.preampMode();
        for (const Row& row : rows) {
            radio.sends.clear();
            ctrl.setRx2PreampMode(row.mode);
            QCOMPARE(ctrl.rx2PreampMode(), row.mode);
            QCOMPARE(radio.sends, (Sends{{1, row.att}}));
            QCOMPARE(ctrl.preampMode(), rx1);  // on its own ADC: RX1 untouched
        }
        QVERIFY(radio.preamp.isEmpty());
        QVERIFY(radio.rx2Preamp.isEmpty());
        // RX2's step attenuator on: the mode sends nothing to the ADC.
        ctrl.setRx2StepAttEnabled(true);
        radio.sends.clear();
        ctrl.setRx2PreampMode(PreampMode::SaMinus20);
        QVERIFY(radio.sends.isEmpty());
        ctrl.setRadioConnection(nullptr);
    }

    // The HPSDR takes RX2's preamp bit (NetworkIO.SetRX2Preamp) and no
    // attenuation from it; its one ADC keeps the two modes one.
    void rx2PreampModeOnTheHpsdrSendsTheSecondPreampBit()
    {
        StepAttenuatorController ctrl;
        ctrl.setBoardIdentity(HPSDRHW::Atlas, HPSDRModel::HPSDR, true);
        loadController(ctrl, QStringLiteral("02:00:00:00:ad:21"));
        RecordingConnection radio;
        ctrl.setRadioConnection(&radio);
        ctrl.setStepAttEnabled(false);
        radio.sends.clear();
        radio.rx2Preamp.clear();
        ctrl.setRx2PreampMode(PreampMode::Minus10);
        QCOMPARE(radio.rx2Preamp, QList<bool>{true});
        ctrl.setRx2PreampMode(PreampMode::Off);
        QCOMPARE(radio.rx2Preamp, (QList<bool>{true, false}));
        QCOMPARE(ctrl.preampMode(), PreampMode::Off);
        ctrl.setPreampMode(PreampMode::On);
        QCOMPARE(ctrl.rx2PreampMode(), PreampMode::On);
        QCOMPARE(radio.rx2Preamp.last(), true);
        QVERIFY(radio.sends.isEmpty());
        ctrl.setRadioConnection(nullptr);
    }

    // RX2's mode is kept per band (rx2_preamp_by_band) and for the radio.
    void rx2PreampModeKeepsItsBandMemoryAndSurvivesARestart()
    {
        const QString mac = QStringLiteral("02:00:00:00:ad:22");
        {
            StepAttenuatorController ctrl;
            ctrl.setBoardIdentity(HPSDRHW::Saturn, HPSDRModel::ANAN_G2, true);
            loadController(ctrl, mac);
            ctrl.setAdcRouting(0, 1, Band::Band20m, false, 1u << 1);
            ctrl.setRx2PreampMode(PreampMode::SaMinus20);
            ctrl.setAdcRouting(0, 1, Band::Band40m, false, 1u << 1);
            ctrl.setRx2PreampMode(PreampMode::Off);
            ctrl.setAdcRouting(0, 1, Band::Band20m, false, 1u << 1);
            QCOMPARE(ctrl.rx2PreampMode(), PreampMode::SaMinus20);
            ctrl.setAdcRouting(0, 1, Band::Band40m, false, 1u << 1);
            ctrl.saveSettings(mac);
        }
        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);
        ctrl.setBoardIdentity(HPSDRHW::Saturn, HPSDRModel::ANAN_G2, true);
        ctrl.setAdcRouting(0, 1, Band::Band40m, false, 1u << 1);
        ctrl.loadSettings(mac);
        QCOMPARE(ctrl.rx2PreampMode(), PreampMode::Off);
        ctrl.setAdcRouting(0, 1, Band::Band20m, false, 1u << 1);
        QCOMPARE(ctrl.rx2PreampMode(), PreampMode::SaMinus20);
    }

    // On one ADC the two modes are one, whichever is set (Thetis links
    // them when nRX1ADCinUse == nRX2ADCinUse, console.cs:19384-19394 and
    // 19509-19519 [v2.10.3.15]); RX2 moving back to slice A's ADC takes
    // RX1's mode.
    void onOneAdcTheTwoPreampModesMoveTogether()
    {
        StepAttenuatorController ctrl;
        ctrl.setBoardIdentity(HPSDRHW::Saturn, HPSDRModel::ANAN_G2, true);
        loadController(ctrl, QStringLiteral("02:00:00:00:ad:23"));
        ctrl.setAdcRouting(0, -1, Band::Band20m, false);
        ctrl.setPreampMode(PreampMode::SaMinus10);
        QCOMPARE(ctrl.rx2PreampMode(), PreampMode::SaMinus10);
        ctrl.setRx2PreampMode(PreampMode::On);
        QCOMPARE(ctrl.preampMode(), PreampMode::On);

        ctrl.setAdcRouting(0, 1, Band::Band20m, false, 1u << 1);
        ctrl.setRx2PreampMode(PreampMode::SaMinus30);
        QCOMPARE(ctrl.preampMode(), PreampMode::On);
        ctrl.setAdcRouting(0, -1, Band::Band20m, false);
        QCOMPARE(ctrl.rx2PreampMode(), PreampMode::On);
    }

    // RX2's auto-attenuate with its step attenuator off steps RX2's mode to
    // the SA settings, and the undo puts it back (console.cs:21693-21716,
    // 21737-21740 [v2.10.3.15]).
    void rx2AutoAttenuateStepsThePreampWithTheStepAttenuatorOff()
    {
        StepAttenuatorController ctrl;
        ctrl.setBoardIdentity(HPSDRHW::Saturn, HPSDRModel::ANAN_G2, true);
        loadController(ctrl, QStringLiteral("02:00:00:00:ad:24"));
        ctrl.setAdcRouting(0, 1, Band::Band20m, false, 1u << 1);
        ctrl.setRx2StepAttEnabled(false);
        ctrl.setRx2AutoAttEnabled(true);
        ctrl.setRx2AutoAttUndo(true);
        ctrl.setRx2AutoUndoDelaySec(0);
        const auto overload = [&ctrl]() {
            for (int i = 0; i < 4; ++i) {
                ctrl.onAdcOverflow(1);
                ctrl.tick();
            }
        };
        ctrl.setRx2PreampMode(PreampMode::On);
        overload();
        QCOMPARE(ctrl.rx2PreampMode(), PreampMode::SaMinus10);
        QCOMPARE(ctrl.rx2AttenuatorDb(), 0);
        for (int i = 0; i < 40 && ctrl.rx2PreampMode() != PreampMode::On; ++i) {
            ctrl.tick();
        }
        QCOMPARE(ctrl.rx2PreampMode(), PreampMode::On);
    }

    // Level Cal 2 (JJ's ruling of 2026-09-30): RX2's slider is RX2's own
    // step attenuator, and every 1 dB step from 0 to 31 reaches the second
    // ADC's attenuator on the wire. Protocol 1 carries it in C0 0001_011x
    // C1[4:0] with the enable in C1[5] (bank 12; TAPR-OpenHPSDR-Firmware
    // @e7c6584 Angelia.v:2318-2321, Orion.v:2417-2421); Protocol 2 in
    // CmdHighPriority byte 1442 (Anvelina High_Priority_CC.v:69-70, 272).
    void rx2StepAttenuatorCarriesEveryStepOnProtocol1_data()
    {
        QTest::addColumn<int>("board");
        QTest::addColumn<int>("model");
        QTest::newRow("ANAN-100D") << int(HPSDRHW::Angelia) << int(HPSDRModel::ANAN100D);
        QTest::newRow("ANAN-200D") << int(HPSDRHW::Orion) << int(HPSDRModel::ANAN200D);
        QTest::newRow("OrionMKII") << int(HPSDRHW::OrionMKII) << int(HPSDRModel::ORIONMKII);
        QTest::newRow("ANAN-7000D") << int(HPSDRHW::OrionMKII) << int(HPSDRModel::ANAN7000D);
        QTest::newRow("ANAN-8000D") << int(HPSDRHW::OrionMKII) << int(HPSDRModel::ANAN8000D);
        QTest::newRow("AnvelinaPro3") << int(HPSDRHW::OrionMKII) << int(HPSDRModel::ANVELINAPRO3);
    }
    void rx2StepAttenuatorCarriesEveryStepOnProtocol1()
    {
        QFETCH(int, board);
        QFETCH(int, model);
        P1RadioConnection conn;
        conn.setBoardForTest(static_cast<HPSDRHW>(board));
        StepAttenuatorController ctrl;
        ctrl.setBoardIdentity(static_cast<HPSDRHW>(board), static_cast<HPSDRModel>(model), true);
        loadController(ctrl, QStringLiteral("02:00:00:00:ad:30"));
        ctrl.setRadioConnection(&conn);
        ctrl.setAdcRouting(0, 1, Band::Band20m, false, 1u << 1);
        ctrl.setRx2StepAttEnabled(true);
        QList<int> steps;
        for (int dB = 1; dB <= 31; ++dB) {
            steps.append(dB);
        }
        steps.append(0);
        for (int dB : steps) {
            ctrl.setRx2Attenuation(dB);
            QCOMPARE(ctrl.rx2AttenuatorDb(), dB);
            quint8 bank12[5] = {};
            conn.composeCcForBankForTest(12, bank12);
            QCOMPARE(int(bank12[0] & 0xFE), 0x16);
            QCOMPARE(int(bank12[1]), dB | 0x20);
        }
        ctrl.setRadioConnection(nullptr);
    }

    void rx2StepAttenuatorCarriesEveryStepOnProtocol2_data()
    {
        QTest::addColumn<int>("board");
        QTest::addColumn<int>("model");
        QTest::newRow("OrionMKII") << int(HPSDRHW::OrionMKII) << int(HPSDRModel::ORIONMKII);
        QTest::newRow("ANAN-7000D") << int(HPSDRHW::OrionMKII) << int(HPSDRModel::ANAN7000D);
        QTest::newRow("ANAN-8000D") << int(HPSDRHW::OrionMKII) << int(HPSDRModel::ANAN8000D);
        QTest::newRow("AnvelinaPro3") << int(HPSDRHW::OrionMKII) << int(HPSDRModel::ANVELINAPRO3);
        QTest::newRow("ANAN-G2") << int(HPSDRHW::Saturn) << int(HPSDRModel::ANAN_G2);
        QTest::newRow("ANAN-G2 1K") << int(HPSDRHW::Saturn) << int(HPSDRModel::ANAN_G2_1K);
    }
    void rx2StepAttenuatorCarriesEveryStepOnProtocol2()
    {
        QFETCH(int, board);
        QFETCH(int, model);
        P2RadioConnection conn;
        conn.setBoardForTest(static_cast<HPSDRHW>(board));
        StepAttenuatorController ctrl;
        ctrl.setBoardIdentity(static_cast<HPSDRHW>(board), static_cast<HPSDRModel>(model), true);
        loadController(ctrl, QStringLiteral("02:00:00:00:ad:31"));
        ctrl.setRadioConnection(&conn);
        ctrl.setAdcRouting(0, 1, Band::Band20m, false, 1u << 1);
        ctrl.setRx2StepAttEnabled(true);
        QList<int> steps;
        for (int dB = 1; dB <= 31; ++dB) {
            steps.append(dB);
        }
        steps.append(0);
        for (int dB : steps) {
            ctrl.setRx2Attenuation(dB);
            QCOMPARE(byteAt(highPriority(conn), kAdc1AttByte), dB);
        }
        ctrl.setRadioConnection(nullptr);
    }

    // Level Cal 2: on the HPSDR, RX2's two Mercury states reach C0
    // 0001_010x C1 bit 1, the second Mercury's preamp (TAPR-OpenHPSDR-
    // Firmware @e7c6584 Mercury_V3.4/Mercury.v:765, 803-807): 0 dB sets it
    // (no attenuator), -20 dB clears it (the 20 dB attenuator in).
    void rx2MercuryStatesReachCaseElevenBitOne()
    {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::Atlas);
        StepAttenuatorController ctrl;
        ctrl.setBoardIdentity(HPSDRHW::Atlas, HPSDRModel::HPSDR, true);
        loadController(ctrl, QStringLiteral("02:00:00:00:ad:32"));
        ctrl.setRadioConnection(&conn);
        ctrl.setRx2PreampMode(PreampMode::Off);
        quint8 bank11[5] = {};
        conn.composeCcForBankForTest(11, bank11);
        QCOMPARE(int(bank11[0] & 0xFE), 0x14);
        QCOMPARE(int(bank11[1] & 0x02), 0);
        ctrl.setRx2PreampMode(PreampMode::On);
        conn.composeCcForBankForTest(11, bank11);
        QCOMPARE(int(bank11[1] & 0x02), 0x02);
        ctrl.setRadioConnection(nullptr);
    }

    // Level Cal 2 review: on an Alex board RX1 reaches 61 dB with the Alex
    // attenuator in, but RX2's own value is the second ADC's 5-bit field
    // (Angelia.v:2319, Orion.v:2295 and :2419): Thetis's RX2 setter never
    // switches the Alex attenuator in (console.cs:11176-11222
    // [v2.10.3.15]), so above 31 its value + 2 wrapped in the gateware.
    // NereusSDR holds RX2 to 0-31: asking 32, 40 or 61 keeps 31 and the
    // wire carries 31, on both protocols; a linked value above 31, once
    // unlinked, comes back to 31 too.
    void rx2AboveItsFieldNeverWraps_data()
    {
        QTest::addColumn<int>("board");
        QTest::addColumn<int>("model");
        QTest::addColumn<int>("protocol");
        QTest::newRow("ANAN-100D P1") << int(HPSDRHW::Angelia) << int(HPSDRModel::ANAN100D) << 1;
        QTest::newRow("ANAN-200D P1") << int(HPSDRHW::Orion) << int(HPSDRModel::ANAN200D) << 1;
        QTest::newRow("ANAN-100D P2") << int(HPSDRHW::Angelia) << int(HPSDRModel::ANAN100D) << 2;
        QTest::newRow("ANAN-200D P2") << int(HPSDRHW::Orion) << int(HPSDRModel::ANAN200D) << 2;
    }
    void rx2AboveItsFieldNeverWraps()
    {
        QFETCH(int, board);
        QFETCH(int, model);
        QFETCH(int, protocol);
        std::unique_ptr<P1RadioConnection> p1;
        std::unique_ptr<P2RadioConnection> p2;
        RadioConnection* conn = nullptr;
        if (protocol == 1) {
            p1 = std::make_unique<P1RadioConnection>();
            p1->setBoardForTest(static_cast<HPSDRHW>(board));
            conn = p1.get();
        } else {
            p2 = std::make_unique<P2RadioConnection>();
            p2->setBoardForTest(static_cast<HPSDRHW>(board));
            conn = p2.get();
        }
        const auto rx2Wire = [&p1, &p2]() {
            if (p1) {
                quint8 bank12[5] = {};
                p1->composeCcForBankForTest(12, bank12);
                return int(bank12[1]);
            }
            return byteAt(highPriority(*p2), kAdc1AttByte);
        };
        const int enable = protocol == 1 ? 0x20 : 0;
        StepAttenuatorController ctrl;
        ctrl.setBoardIdentity(static_cast<HPSDRHW>(board), static_cast<HPSDRModel>(model), true);
        loadController(ctrl, QStringLiteral("02:00:00:00:ad:33"));
        ctrl.setMaxAttenuation(61);
        QCOMPARE(ctrl.maxAttenuation(), 61);
        QCOMPARE(ctrl.rx2MaxAttenuation(), 31);
        ctrl.setRadioConnection(conn);
        ctrl.setAdcRouting(0, 1, Band::Band20m, false, 1u << 1);
        ctrl.setRx2StepAttEnabled(true);

        ctrl.setRx2Attenuation(31);
        QCOMPARE(rx2Wire(), 31 | enable);
        for (int asked : {32, 40, 61}) {
            QSignalSpy changed(&ctrl, &StepAttenuatorController::rx2AttenuationChanged);
            ctrl.setRx2Attenuation(asked);
            QCOMPARE(ctrl.rx2AttenuatorDb(), 31);
            QCOMPARE(rx2Wire(), 31 | enable);
            // The kept value is said again, so a box showing the asked
            // value returns to it.
            QCOMPARE(changed.count(), 1);
            QCOMPARE(changed.first().first().toInt(), 31);
        }
        ctrl.setRx2Attenuation(8);
        QCOMPARE(rx2Wire(), 8 | enable);

        // Linked (diversity): RX2 takes RX1's 45; unlinked, RX2's own value
        // comes back within its field and the wire carries 31.
        ctrl.setAdcRouting(0, 1, Band::Band20m, true);
        ctrl.setAttenuation(45, 0);
        QCOMPARE(ctrl.rx2AttenuatorDb(), 45);
        ctrl.setAdcRouting(0, 1, Band::Band20m, false, 1u << 1);
        QCOMPARE(ctrl.rx2AttenuatorDb(), 31);
        QCOMPARE(rx2Wire(), 31 | enable);
        ctrl.setRadioConnection(nullptr);
    }

    // Level Cal 2 re-review: a stored RX2 value above 31 (saved before RX2
    // was held to its field, on an Alex board's 61 dB range) comes back as
    // 31, both when the settings load and when a band restores it, and the
    // radio is sent 31.
    void rx2StoredAboveItsFieldLoadsAndRestoresAs31()
    {
        const QString mac = QStringLiteral("02:00:00:00:ad:35");
        startFromNothing(mac);
        AppSettings& s = AppSettings::instance();
        s.setHardwareValue(mac, QStringLiteral("options/stepAtt/rx2Value"), 45);
        s.setHardwareValue(mac, QStringLiteral("options/stepAtt/rx2Band/")
                                    + bandKeyName(Band::Band40m), 50);
        s.save();

        StepAttenuatorController ctrl;
        ctrl.setBoardIdentity(HPSDRHW::Angelia, HPSDRModel::ANAN100D, true);
        ctrl.setTickTimerEnabled(false);
        ctrl.setMinAttenuation(0);
        ctrl.setMaxAttenuation(61);
        RecordingConnection radio;
        ctrl.setAdcRouting(0, 1, Band::Band20m, false, 1u << 1);
        ctrl.setRadioConnection(&radio);
        ctrl.loadSettings(mac);
        QCOMPARE(ctrl.rx2AttenuatorDb(), 31);
        ctrl.setRx2StepAttEnabled(true);

        ctrl.setRx2Attenuation(10);
        QCOMPARE(ctrl.rx2AttenuatorDb(), 10);
        radio.sends.clear();
        ctrl.setBandRestoreToRadio(true);
        ctrl.setAdcRouting(0, 1, Band::Band40m, false, 1u << 1);
        QCOMPARE(ctrl.rx2AttenuatorDb(), 31);
        QVERIFY2(radio.sends.contains(QPair<int, int>(1, 31)),
                 qPrintable(QStringLiteral("%1 sends").arg(radio.sends.size())));
        for (const auto& send : radio.sends) {
            QVERIFY(send.second <= 31 || send.first != 1);
        }
        ctrl.setRadioConnection(nullptr);
        startFromNothing(mac);
    }

    // Level Cal 2 review: a write of RX2's value above 31 through stepAtt
    // (a window or the phone) keeps 31 and says RX2's range.
    void rx2FacadeWriteAboveItsFieldSaysItsRange()
    {
        StepAttenuatorController ctrl;
        ctrl.setBoardIdentity(HPSDRHW::Angelia, HPSDRModel::ANAN100D, true);
        loadController(ctrl, QStringLiteral("02:00:00:00:ad:34"));
        ctrl.setMaxAttenuation(61);
        ctrl.setAdcRouting(0, 1, Band::Band20m, false, 1u << 1);
        StepAttenuatorFacade facade(nullptr);
        facade.bindController(&ctrl);
        facade.setRx2AttenuationDb(40);
        QCOMPARE(ctrl.rx2AttenuatorDb(), 31);
        QCOMPARE(facade.rx2AttenuationDb(), 31);
        QCOMPARE(facade.settleReason("rx2AttenuationDb"),
                 QStringLiteral("RX2's attenuator goes from 0 to 31 dB."));
        QVERIFY(OperatorWording::isPlain(facade.settleReason("rx2AttenuationDb")));
    }
};

QTEST_MAIN(TstPerAdcStepAttenuator)
#include "tst_per_adc_step_attenuator.moc"
