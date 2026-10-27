// no-port-check: NereusSDR-original test.
// =================================================================
// tests/tst_cw_keyer_controller.cpp  (NereusSDR)
// =================================================================
//
// CW transmit (R-CW-05 to R-CW-07, R-CW-10, R-CW-13, R-CW-16, R-CW-20,
// R-CW-22, R-CW-38, D16): the CW keyer controller against a RadioModel with
// a fake connection. Nothing here opens a radio socket or keys a radio: the
// fake connection records the wire states it is handed.
//
// Modification history (NereusSDR):
//   2026-10-26: CW transmit, the CW keyer controller. J.J. Boyd (KG4VCF),
//               AI-assisted via Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QSignalSpy>
#include <QTcpSocket>

#include <functional>

#include "OperatorWording.h"
#include "core/AppSettings.h"
#include "core/CwTypes.h"
#include "core/MoxController.h"
#include "core/RadioConnection.h"
#include "core/RadioDiscovery.h"
#include "core/SmartSdrApiListener.h"
#include "core/TxInterlockPolicy.h"
#include "core/TxSliceArbiter.h"
#include "core/codec/CwWireState.h"
#include "core/safety/CwKeyerGate.h"
#include "core/safety/TxRefusal.h"
#include "core/safety/TxTimeOutTimer.h"
#include "models/CwKeyerController.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

namespace {

// Records every CW wire state handed to it. Nothing reaches a radio.
class CwMockConnection : public RadioConnection {
    Q_OBJECT
public:
    QList<CwWireState> states;

    explicit CwMockConnection(QObject* parent = nullptr)
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
    void setCwWireState(const NereusSDR::CwWireState& s) override
    {
        states.append(s);
        RadioConnection::setCwWireState(s);
    }
    void reportKey(bool dot, bool dash, bool keyOut) { emit radioKeyStatus(dot, dash, keyOut); }
    void setStateForTest(ConnectionState s) { setState(s); }
};

void pump(int passes = 6)
{
    for (int i = 0; i < passes; ++i) {
        QCoreApplication::processEvents();
    }
}

RadioInfo radioInfo(ProtocolVersion protocol, HPSDRHW board, int firmware)
{
    RadioInfo info;
    info.protocol = protocol;
    info.boardType = board;
    info.firmwareVersion = firmware;
    return info;
}

// A local model on a P2 OrionMKII (firmware 22) with a fake connection, one
// slice on 20 m CWU, transmit time-out on at 30 s with a test clock.
struct Rig {
    RadioModel model;
    CwMockConnection conn;
    qint64 clockMs{0};
    SliceModel* slice{nullptr};

    explicit Rig(bool withSlice = true)
    {
        AppSettings::instance().clear();
        model.setLastRadioInfoForTest(radioInfo(ProtocolVersion::Protocol2, HPSDRHW::OrionMKII, 22));
        model.injectConnectionForTest(&conn);
        model.wireCwKeyerConnectionForTest();
        model.moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
        if (TxTimeOutTimer* tot = model.txTimeOutTimer()) {
            tot->setClock([this]() { return clockMs; });
            tot->setSettingsSource([]() {
                TxTimeOutTimer::Settings s;
                s.moxEnabled = true;
                s.moxSeconds = TxTimeOutTimer::kMinimumSeconds;
                return s;
            });
        }
        if (withSlice) {
            slice = model.sliceById(model.addSlice());
            slice->setDspMode(DSPMode::CWU);
            slice->setFrequency(14'050'000.0);
        }
        pump();
    }
    ~Rig()
    {
        model.injectConnectionForTest(nullptr);
        AppSettings::instance().clear();
    }

    CwKeyerController* keyer() { return model.cwKeyerController(); }
    // The radio's key status, delivered as the connection's queued signal.
    void key(bool dot, bool dash, bool keyOut)
    {
        conn.reportKey(dot, dash, keyOut);
        pump();
    }
};

} // namespace

class TestCwKeyerController : public QObject {
    Q_OBJECT

private slots:
    void init() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    // ── Push: once on connect, then only on a change ─────────────────────
    void pushesOnceOnConnectThenOnlyOnChange()
    {
        Rig rig(/*withSlice=*/false);
        QCOMPARE(rig.conn.states.size(), 1);
        QVERIFY(!rig.conn.states.last().enable);
        QVERIFY(!rig.keyer()->keyerAllowed());
        QCOMPARE(rig.keyer()->refusal(), TxRefusals::noTransmitSlice());

        SliceModel* slice = rig.model.sliceById(rig.model.addSlice());
        QVERIFY(slice);
        slice->setFrequency(14'050'000.0);
        slice->setDspMode(DSPMode::CWU);
        pump();
        QVERIFY2(rig.keyer()->keyerAllowed(), qPrintable(rig.keyer()->refusal().text));
        QVERIFY(rig.conn.states.last().enable);
        const int pushes = rig.conn.states.size();

        // In band and still CW: re-evaluated, the same state, nothing sent.
        const int evaluations = rig.keyer()->evaluationCountForTest();
        QSignalSpy wire(rig.keyer(), &CwKeyerController::wireStateChanged);
        slice->setFrequency(14'060'000.0);
        pump();
        QCOMPARE(rig.keyer()->evaluationCountForTest(), evaluations + 1);
        QCOMPARE(wire.count(), 0);
        QCOMPARE(rig.conn.states.size(), pushes);

        // Wiring again (a reconnect) sends the state once more.
        rig.model.wireCwKeyerConnectionForTest();
        QCOMPARE(rig.conn.states.size(), pushes + 1);
        QCOMPARE(rig.conn.states.last(), rig.keyer()->wireState());
    }

    // ── R-CW-06: each input re-evaluates once and emits only on a change ──
    void eachInputReevaluatesAndEmitsOnlyWhenTheStateChanges()
    {
        Rig rig;
        QVERIFY2(rig.keyer()->keyerAllowed(), qPrintable(rig.keyer()->refusal().text));
        CwKeyerController* k = rig.keyer();
        QSignalSpy wire(k, &CwKeyerController::wireStateChanged);

        // Each step: one change, one evaluation, `refusedAfter` says whether
        // the gate now refuses; then the step's undo.
        struct Step {
            const char* name;
            std::function<void()> change;
            std::function<void()> undo;
            QByteArray code;
        };
        SliceModel* s = rig.slice;
        auto& settings = AppSettings::instance();
        const QList<Step> steps = {
            {"frequency", [s]() { s->setFrequency(14'400'000.0); },
             [s]() { s->setFrequency(14'050'000.0); }, TxRefusals::kBandPlan},
            {"mode", [s]() { s->setDspMode(DSPMode::USB); },
             [s]() { s->setDspMode(DSPMode::CWU); }, TxRefusals::kBandPlan},
            {"receiveOnly", [&rig]() { rig.model.setReceiveOnlyStationPolicy(true); },
             [&rig]() { rig.model.setReceiveOnlyStationPolicy(false); },
             TxRefusals::kStationReceiveOnly},
            {"inhibit",
             [&rig]() {
                 rig.model.txInhibit().setEnabled(true);
                 rig.model.txInhibit().notifyBlockTxAntenna(true);
             },
             [&rig]() { rig.model.txInhibit().notifyBlockTxAntenna(false); },
             TxRefusals::kInterlock},
            {"region",
             [&rig, &settings]() {
                 settings.setValue(QStringLiteral("BandPlanRegion"),
                                   QString::number(static_cast<int>(safety::Region::Japan)));
                 rig.model.reportTransmitGateSettingChanged(QStringLiteral("BandPlanRegion"));
             },
             [&rig, &settings]() {
                 settings.remove(QStringLiteral("BandPlanRegion"));
                 rig.model.reportTransmitGateSettingChanged(QStringLiteral("BandPlanRegion"));
             },
             QByteArray()},
        };
        for (const Step& step : steps) {
            const int before = k->evaluationCountForTest();
            const int emitted = wire.count();
            step.change();
            pump();
            QVERIFY2(k->evaluationCountForTest() >= before + 1, step.name);
            if (!step.code.isEmpty()) {
                QVERIFY2(!k->keyerAllowed(), step.name);
                QCOMPARE(k->refusal().code, step.code);
                QCOMPARE(wire.count(), emitted + 1);
                QVERIFY(!rig.conn.states.last().enable);
            }
            step.undo();
            pump();
            QVERIFY2(k->keyerAllowed(), qPrintable(QStringLiteral("%1: %2").arg(
                                            QLatin1String(step.name), k->refusal().text)));
            QVERIFY(rig.conn.states.last().enable);
        }

        // XIT: the carrier (dial plus XIT) moved past the band edge refuses;
        // the offset and its switch are each an input.
        s->setFrequency(14'345'000.0);
        pump();
        int before = k->evaluationCountForTest();
        s->setXitHz(9'000);
        pump();
        QCOMPARE(k->evaluationCountForTest(), before + 1);
        QVERIFY(k->keyerAllowed());
        before = k->evaluationCountForTest();
        s->setXitEnabled(true);
        pump();
        QCOMPARE(k->evaluationCountForTest(), before + 1);
        QVERIFY2(!k->keyerAllowed(), qPrintable(QString::number(rig.model.txFrequencyForSlice(s))));
        QCOMPARE(k->refusal().code, QByteArray(TxRefusals::kBandPlan));
        s->setXitEnabled(false);
        pump();
        QVERIFY(k->keyerAllowed());
        s->setFrequency(14'050'000.0);
        pump();

        // A frequency change that keeps the state emits nothing.
        const int emitted = wire.count();
        s->setFrequency(14'051'000.0);
        pump();
        QCOMPARE(wire.count(), emitted);
    }

    void theThreeBandPlanSettingsReevaluateOnce()
    {
        Rig rig;
        CwKeyerController* k = rig.keyer();
        auto& settings = AppSettings::instance();

        // Out of band, so Extended is what decides.
        rig.slice->setFrequency(14'400'000.0);
        pump();
        QVERIFY(!k->keyerAllowed());

        int before = k->evaluationCountForTest();
        settings.setValue(QString::fromLatin1(RadioModel::kExtendedTransmitKey), QStringLiteral("True"));
        rig.model.reportTransmitGateSettingChanged(QString::fromLatin1(RadioModel::kExtendedTransmitKey));
        QCOMPARE(k->evaluationCountForTest(), before + 1);
        QVERIFY(k->keyerAllowed());
        settings.setValue(QString::fromLatin1(RadioModel::kExtendedTransmitKey), QStringLiteral("False"));
        rig.model.reportTransmitGateSettingChanged(QString::fromLatin1(RadioModel::kExtendedTransmitKey));
        QVERIFY(!k->keyerAllowed());

        before = k->evaluationCountForTest();
        settings.setValue(QStringLiteral("BandPlanRegion"), QStringLiteral("99"));
        rig.model.reportTransmitGateSettingChanged(QStringLiteral("BandPlanRegion"));
        QCOMPARE(k->evaluationCountForTest(), before + 1);
        QCOMPARE(k->refusal().text, QStringLiteral("The transmit region setting is invalid."));
        settings.remove(QStringLiteral("BandPlanRegion"));
        rig.model.reportTransmitGateSettingChanged(QStringLiteral("BandPlanRegion"));

        before = k->evaluationCountForTest();
        settings.setValue(QString::fromLatin1(RadioModel::kPreventTxOnDifferentBandKey),
                          QStringLiteral("True"));
        rig.model.reportTransmitGateSettingChanged(
            QString::fromLatin1(RadioModel::kPreventTxOnDifferentBandKey));
        QCOMPARE(k->evaluationCountForTest(), before + 1);

        // Any other key does not re-evaluate.
        before = k->evaluationCountForTest();
        rig.model.reportTransmitGateSettingChanged(QStringLiteral("SomethingElse"));
        QCOMPARE(k->evaluationCountForTest(), before);
    }

    void transmitSliceLinkTripAndRadioChangeRefuse()
    {
        Rig rig;
        CwKeyerController* k = rig.keyer();
        QVERIFY(k->keyerAllowed());

        // The TX slice moves to a USB slice.
        SliceModel* other = rig.model.sliceById(rig.model.addSlice());
        QVERIFY(other);
        other->setDspMode(DSPMode::USB);
        other->setFrequency(14'200'000.0);
        pump();
        QVERIFY(rig.model.txSliceArbiter()->requestHandoff(other->sliceIndex()));
        pump();
        QVERIFY(!k->keyerAllowed());
        QCOMPARE(k->refusal().text, QString::fromLatin1(safety::kCwGateNotCwText));
        QVERIFY(rig.model.txSliceArbiter()->requestHandoff(rig.slice->sliceIndex()));
        pump();
        QVERIFY(k->keyerAllowed());

        // A radio change under way.
        int before = k->evaluationCountForTest();
        rig.model.setStationRadioChangeUnderway(true);
        QCOMPARE(k->evaluationCountForTest(), before + 1);
        QVERIFY(!k->keyerAllowed());
        QCOMPARE(k->refusal().code, QByteArray(TxRefusals::kNotReady));
        rig.model.setStationRadioChangeUnderway(false);
        QVERIFY(k->keyerAllowed());

        // The PA trip.
        rig.model.handleGanymedeTrip(1);
        pump();
        QVERIFY(!k->keyerAllowed());
        QCOMPARE(k->refusal(), TxRefusals::paProtection());
        rig.model.handleGanymedeTrip(0);
        pump();
        QVERIFY(k->keyerAllowed());

        // The link to the radio.
        rig.conn.setStateForTest(ConnectionState::LinkLost);
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("^Link lost to")));
        rig.model.onConnectionStateChangedForTest(ConnectionState::LinkLost);
        pump();
        QVERIFY(!k->keyerAllowed());
        QCOMPARE(k->refusal(), TxRefusals::radioLinkDown());
        QVERIFY(!rig.conn.states.last().enable);
    }

    void theInterlockRefuses()
    {
        Rig rig;
        CwKeyerController* k = rig.keyer();
        // The SWR over the interlock's limit, as the amplifier's meters
        // report it (RadioModel hands the SWR to MoxController first).
        TxInterlockPolicy* policy = rig.model.txInterlockPolicy();
        policy->setMode(TxInterlockPolicy::Block);
        policy->setSwrGateEnabled(true);
        policy->setSwrGateMax(3.0f);
        QVERIFY(k->keyerAllowed());
        emit rig.model.ampMetersChanged(10.0f, 5.0f);
        QVERIFY(!k->keyerAllowed());
        QCOMPARE(k->refusal(), TxRefusals::swr());
        QVERIFY(!rig.conn.states.last().enable);
        emit rig.model.ampMetersChanged(10.0f, 1.2f);
        QVERIFY(k->keyerAllowed());
        policy->setMode(TxInterlockPolicy::Disabled);
    }

    // ── Composition (R-CW-07, R-CW-13, R-CW-16, R-CW-38) ───────────────────
    void composesTheWireState()
    {
        Rig rig;
        CwKeyerController* k = rig.keyer();
        TransmitModel& tm = rig.model.transmitModel();
        tm.setCwKeyerSpeed(30);
        tm.setCwKeyDownDelayMs(20);
        tm.setCwBreakInDelayMs(400);
        tm.setCwKeyUpDelayMs(50);
        tm.setMonitorVolume(0.5f);
        pump();

        // Semi (the default).
        QCOMPARE(k->effectiveBreakInMode(), CwBreakInMode::Semi);
        CwWireState w = k->wireState();
        QVERIFY(w.enable);
        QVERIFY(w.breakIn);
        QCOMPARE(w.speedWpm, 30);
        QCOMPARE(w.hangMs, cwHangMs(CwBreakInMode::Semi, 400, 50));
        QCOMPARE(w.rfDelayMs, cwRfDelayMs(20, 30));
        QCOMPARE(w.sidetoneLevel,
                 cwRadioSidetoneLevel(true, true, false, CwBreakInMode::Semi, 0.5f));
        QCOMPARE(w.sidetoneFreqHz, rig.model.cwPitchHz());
        QVERIFY(!w.qskTxAttenuation);
        QCOMPARE(rig.conn.states.last(), w);

        // Off: no break-in, hang 0, the Off sidetone scale.
        tm.setCwBreakInMode(static_cast<int>(CwBreakInMode::Off));
        pump();
        w = k->wireState();
        QVERIFY(w.enable);
        QVERIFY(!w.breakIn);
        QCOMPARE(w.hangMs, 0);
        QCOMPARE(w.sidetoneLevel, cwRadioSidetoneLevel(true, true, false, CwBreakInMode::Off, 0.5f));

        // QSK on an OrionMKII at firmware 22 with one antenna: available.
        tm.setCwBreakInMode(static_cast<int>(CwBreakInMode::Qsk));
        pump();
        QVERIFY2(k->qskAvailable(), qPrintable(k->qskUnavailableReason()));
        QCOMPARE(k->effectiveBreakInMode(), CwBreakInMode::Qsk);
        w = k->wireState();
        QVERIFY(w.breakIn);
        QVERIFY(w.qskTxAttenuation);
        QCOMPARE(w.hangMs, cwHangMs(CwBreakInMode::Qsk, 400, 50));
        QCOMPARE(w.hangMs, 50);

        // Refused: enable, break-in and the QSK attenuation all off.
        rig.slice->setDspMode(DSPMode::USB);
        pump();
        w = k->wireState();
        QVERIFY(!w.enable);
        QVERIFY(!w.breakIn);
        QVERIFY(!w.qskTxAttenuation);
        rig.slice->setDspMode(DSPMode::CWU);
        pump();

        // A pitch, a setting and the RADIO mute each re-emit.
        QSignalSpy wire(k, &CwKeyerController::wireStateChanged);
        rig.model.setCwPitchHz(700);
        pump();
        QCOMPARE(wire.count(), 1);
        QCOMPARE(k->wireState().sidetoneFreqHz, 700);
        QCOMPARE(rig.conn.states.last().sidetoneFreqHz, 700);

        tm.setCwKeyerWeight(60);
        pump();
        QCOMPARE(wire.count(), 2);
        QCOMPARE(k->wireState().weight, 60);

        QVERIFY(k->wireState().sidetoneLevel > 0);
        rig.model.setRadioSpeakerMuted(true);
        pump();
        QCOMPARE(wire.count(), 3);
        QCOMPARE(k->wireState().sidetoneLevel, 0);
        QCOMPARE(rig.conn.states.last().sidetoneLevel, 0);
    }

    // ── Fallbacks (R-CW-20, R-CW-22, D16) ────────────────────────────────
    void qskAvailabilityFollowsTheRuleInOrder()
    {
        Rig rig;
        CwKeyerController* k = rig.keyer();
        rig.model.transmitModel().setCwBreakInMode(static_cast<int>(CwBreakInMode::Qsk));

        const struct {
            ProtocolVersion protocol;
            HPSDRHW board;
            int firmware;
            const char* reason;
        } cases[] = {
            {ProtocolVersion::Protocol1, HPSDRHW::Hermes, 30, kCwQskNeedsP2Reason},
            {ProtocolVersion::Protocol1, HPSDRHW::HermesLite, 72, kCwQskNeedsP2Reason},
            {ProtocolVersion::Protocol2, HPSDRHW::Angelia, 30, kCwQskBoardReason},
            {ProtocolVersion::Protocol2, HPSDRHW::HermesC10, 30, kCwQskBoardReason},
            {ProtocolVersion::Protocol2, HPSDRHW::Orion, 16, kCwQskFirmwareReason},
            {ProtocolVersion::Protocol2, HPSDRHW::OrionMKII, 16, kCwQskFirmwareReason},
            {ProtocolVersion::Protocol2, HPSDRHW::Orion, 17, nullptr},
            {ProtocolVersion::Protocol2, HPSDRHW::OrionMKII, 17, nullptr},
            {ProtocolVersion::Protocol2, HPSDRHW::Saturn, 10, nullptr},
            {ProtocolVersion::Protocol2, HPSDRHW::SaturnMKII, 10, nullptr},
        };
        for (const auto& c : cases) {
            rig.model.setLastRadioInfoForTest(radioInfo(c.protocol, c.board, c.firmware));
            k->reevaluate();
            if (c.reason == nullptr) {
                QVERIFY2(k->qskAvailable(), qPrintable(k->qskUnavailableReason()));
                QCOMPARE(k->effectiveBreakInMode(), CwBreakInMode::Qsk);
            } else {
                QVERIFY(!k->qskAvailable());
                QCOMPARE(k->qskUnavailableReason(), QString::fromLatin1(c.reason));
                // QSK selected where it is not allowed falls back to Semi.
                QCOMPARE(k->effectiveBreakInMode(), CwBreakInMode::Semi);
                QVERIFY(k->semiAvailable());
                QVERIFY(k->wireState().breakIn);
                QVERIFY(!k->wireState().qskTxAttenuation);
            }
            if (!k->qskAvailable()) {
                // The brief's P2 sentence names "protocol", a word the
                // internal-term list holds; it is used as written and the
                // term is the only one allowed here.
                const QString term = OperatorWording::internalTermIn(k->qskUnavailableReason());
                QVERIFY2(term.isEmpty()
                             || (c.reason == kCwQskNeedsP2Reason
                                 && term.compare(QStringLiteral("protocol"), Qt::CaseInsensitive) == 0),
                         qPrintable(k->qskUnavailableReason() + QStringLiteral(" / ") + term));
            }
        }

        // The antenna rule re-runs on an antenna change.
        rig.model.setLastRadioInfoForTest(radioInfo(ProtocolVersion::Protocol2, HPSDRHW::Saturn, 10));
        k->reevaluate();
        QVERIFY(k->qskAvailable());
        rig.slice->setTxAntenna(QStringLiteral("ANT2"));
        pump();
        QCOMPARE(k->qskUnavailableReason(), QString::fromLatin1(kCwQskAntennaReason));
        QCOMPARE(k->effectiveBreakInMode(), CwBreakInMode::Semi);
        rig.slice->setTxAntenna(rig.slice->rxAntenna());
        pump();
        QCOMPARE(k->effectiveBreakInMode(), CwBreakInMode::Qsk);
    }

    void anArmedInterlockLimitsCwToOffUntilItGrants()
    {
        Rig rig;
        CwKeyerController* k = rig.keyer();
        rig.model.setLastRadioInfoForTest(radioInfo(ProtocolVersion::Protocol2, HPSDRHW::Saturn, 10));
        rig.model.transmitModel().setCwBreakInMode(static_cast<int>(CwBreakInMode::Qsk));
        pump();
        QCOMPARE(k->effectiveBreakInMode(), CwBreakInMode::Qsk);

        // An amplifier creates its interlock on the 4O3A listener.
        SmartSdrApiListener* listener = rig.model.smartSdrListener();
        QVERIFY(listener->start(QHostAddress::LocalHost, 0));
        QTcpSocket amp;
        amp.connectToHost(QHostAddress::LocalHost, listener->serverPort());
        QVERIFY(amp.waitForConnected(2000));
        QSignalSpy armed(listener, &SmartSdrApiListener::interlockedAmpChanged);
        amp.write("C1|interlock create type=AMP name=PG serial=TEST\n");
        amp.flush();
        QTRY_COMPARE_WITH_TIMEOUT(armed.count(), 1, 3000);
        QVERIFY(listener->hasInterlockedAmp());

        const QString d16 = QString::fromLatin1(kCwAmpInterlockBreakInReason);
        QVERIFY(OperatorWording::isPlain(d16));
        QVERIFY(!k->semiAvailable());
        QCOMPARE(k->semiUnavailableReason(), d16);
        QVERIFY(!k->qskAvailable());
        QCOMPARE(k->qskUnavailableReason(), d16);
        QCOMPARE(k->effectiveBreakInMode(), CwBreakInMode::Off);
        // In Off the keyer waits for the interlock's grant.
        QVERIFY(!k->keyerAllowed());
        QCOMPARE(k->refusal().text, QString::fromLatin1(safety::kCwGateInterlockNotGrantedText));
        QVERIFY(!rig.conn.states.last().enable);
        QVERIFY(!rig.conn.states.last().breakIn);

        emit listener->interlockGranted(QStringLiteral("MOX"));
        QVERIFY(k->keyerAllowed());
        QVERIFY(rig.conn.states.last().enable);
        QVERIFY(!rig.conn.states.last().breakIn);
        QCOMPARE(rig.conn.states.last().hangMs, 0);

        // The key ends: the grant goes with it.
        emit rig.model.moxController()->moxStateChanged(false);
        QVERIFY(!k->keyerAllowed());

        // The amplifier goes: QSK is back.
        QSignalSpy disarmed(listener, &SmartSdrApiListener::interlockedAmpChanged);
        amp.abort();
        QTRY_COMPARE_WITH_TIMEOUT(disarmed.count(), 1, 3000);
        QCOMPARE(k->effectiveBreakInMode(), CwBreakInMode::Qsk);
        QVERIFY(k->keyerAllowed());
        listener->stop();
    }

    // ── R-CW-10: the TX time-out on QSK key-out ──────────────────────────
    void qskKeyOutRunsTheTimeOutAndItsExpiryLatches()
    {
        Rig rig;
        CwKeyerController* k = rig.keyer();
        rig.model.setLastRadioInfoForTest(radioInfo(ProtocolVersion::Protocol2, HPSDRHW::Saturn, 10));
        rig.model.transmitModel().setCwBreakInMode(static_cast<int>(CwBreakInMode::Qsk));
        pump();
        QCOMPARE(k->effectiveBreakInMode(), CwBreakInMode::Qsk);
        QVERIFY(k->keyerAllowed());
        TxTimeOutTimer* tot = rig.model.txTimeOutTimer();
        QVERIFY(tot);
        QVERIFY(!tot->isTicking());

        // Key-out counts; its drop stops the count.
        rig.key(true, false, true);
        QVERIFY(rig.model.cwQskKeyOutTimingForTest());
        QVERIFY(tot->isTicking());
        rig.key(false, false, false);
        QVERIFY(!rig.model.cwQskKeyOutTimingForTest());
        QVERIFY(!tot->isTicking());

        // Held past the limit: the time-out stops it and latches the keyer.
        QSignalSpy stopped(&rig.model, &RadioModel::transmitStopped);
        rig.key(false, true, true);
        rig.clockMs += TxTimeOutTimer::kMinimumSeconds * 1000;
        tot->tick();
        pump();
        QCOMPARE(stopped.count(), 1);
        QVERIFY(k->isStopLatched());
        QVERIFY(!k->keyerAllowed());
        QCOMPARE(k->refusal().text, QString::fromLatin1(safety::kCwGateStopLatchedText));
        QVERIFY(!rig.conn.states.last().enable);
        QVERIFY(!rig.conn.states.last().breakIn);

        // The keyer off, key-out drops; the paddle still held keeps the latch.
        rig.key(false, true, false);
        QVERIFY(k->isStopLatched());
        QVERIFY(!tot->isTicking());
        // Released: the keyer may run again.
        rig.key(false, false, false);
        QVERIFY(!k->isStopLatched());
        QVERIFY(k->keyerAllowed());
        QVERIFY(rig.conn.states.last().enable);
    }

    void keyOutOutsideQskIsNotTimed()
    {
        Rig rig;
        rig.model.transmitModel().setCwBreakInMode(static_cast<int>(CwBreakInMode::Semi));
        pump();
        rig.key(true, false, true);
        QVERIFY(!rig.model.cwQskKeyOutTimingForTest());
        QVERIFY(!rig.model.txTimeOutTimer()->isTicking());
    }

    void aStopWithTheKeyUpDoesNotLatch()
    {
        Rig rig;
        CwKeyerController* k = rig.keyer();
        k->latchStop();
        QVERIFY(!k->isStopLatched());
        QVERIFY(k->keyerAllowed());
    }
};

QTEST_MAIN(TestCwKeyerController)
#include "tst_cw_keyer_controller.moc"
