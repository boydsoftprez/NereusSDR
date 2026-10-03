// no-port-check: NereusSDR-original test.
//
// tests/tst_remote_step_attenuator.cpp
//
// R-R3-46 / R-R3-11 / R-R3-13: the Core's step attenuator and preamp as the
// mirrored `stepAtt` object, end to end. The Core is a real DaemonApp
// (primed board, no radio socket; its controller drives a fake radio
// connection), a StationServer over it, and a remote window's RadioModel,
// SettingsProxy and StationClient joined over the in-process loopback.
//
// Covered: a window's 20 dB reaches the radio and is saved on the Core for
// that radio and band; an ANAN-100D with Alex takes 45 dB and settles 70 dB
// at 61 dB with a plain reason; a Hermes Lite 2 reports -28..31 dB and
// settles Adaptive to Classic; a band change on the Core restores that
// band's attenuation and the window follows; overload on either ADC reaches
// the window; a slice on the other ADC shows and sets that ADC's own
// attenuator through the window (adcAttenuatorVersion 1), and a peer that
// did not declare adcAttenuators sees today's `stepAtt`. No hardware; no
// audio device is opened by these objects.
//
// Modification history (NereusSDR):
//   2026-09-23: created (R-R3-46, R-R3-11, R-R3-13), by J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-23: each radio starts with no saved attenuator values, so a
//               second run in the same test sandbox passes too (R-R3-46),
//               by J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-28: the other ADC's own attenuator on the window, and today's
//               stepAtt for a peer that did not declare it (R-R3-46,
//               R-R3-11), by J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.

#include <QtTest/QtTest>

#include <QCheckBox>
#include <QGroupBox>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <memory>

#include "core/AppSettings.h"
#include "core/RadioConnection.h"
#include "core/StepAttenuatorController.h"
#include "core/StepAttenuatorFacade.h"
#define private public
#include "core/daemon/DaemonApp.h"
#undef private
#include "core/daemon/DaemonConfig.h"
#include "core/session/SessionMessages.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/settings/SettingsProxy.h"
#include "gui/setup/GeneralOptionsPage.h"
#include "models/Band.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include "fakes/LoopbackTransport.h"
#include "OperatorWording.h"
#include "fakes/UpgradedCoreToken.h"

using namespace NereusSDR;

namespace {
// iPhone app Task 12 put the Core's listener on by default (TCP 47910 on
// every interface). A test Core opens no listener unless the test asks for
// one on a port of its own.
NereusSDR::DaemonConfig testCoreConfig()
{
    NereusSDR::DaemonConfig config = NereusSDR::DaemonConfig::defaults();
    config.remotePort = 0;
    return config;
}
} // namespace
using NereusSDR::Test::LoopbackTransport;

namespace {

// The radio end of the controller: records what the Core sent it.
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

struct WriteResult {
    bool seen = false;
    bool accepted = false;
    QString reason;
};

WriteResult lastResultFor(const QSignalSpy& spy, const QByteArray& property)
{
    WriteResult out;
    for (const QList<QVariant>& call : spy) {
        if (call.at(0).toByteArray() == "stepAtt" && call.at(1).toByteArray() == property) {
            out.seen = true;
            out.accepted = call.at(3).toBool();
            out.reason = call.at(4).toString();
        }
    }
    return out;
}

} // namespace

class TstRemoteStepAttenuator : public QObject {
    Q_OBJECT

private:
    // A Core (DaemonApp on `board`) and one remote window, joined.
    struct Session {
        QTemporaryDir dir;
        std::unique_ptr<AppSettings> stationSettings;
        DaemonApp app;
        FakeRadioConnection radio;
        std::unique_ptr<StationServer> server;
        std::unique_ptr<RadioModel> window;
        std::unique_ptr<SettingsProxy> proxy;
        std::unique_ptr<StationClient> client;
        QString mac;

        StepAttenuatorController* controller() const { return app.m_stepAttController.get(); }
        StepAttenuatorFacade* remote() const { return window->stepAttFacade(); }

        ~Session()
        {
            client.reset();
            proxy.reset();
            window.reset();
            server.reset();
            if (app.m_stepAttController) {
                app.m_stepAttController->setRadioConnection(nullptr);
            }
            app.stop();
        }
    };

    std::unique_ptr<Session> join(HPSDRHW board, const QString& mac)
    {
        // The Core saves this radio's attenuator into the process's own
        // settings (the test sandbox, kept between runs). Start each radio
        // from nothing, or a value an earlier run saved (40 m at 20 dB)
        // makes the window's 20 dB no change and nothing reaches the radio.
        AppSettings::instance().load();
        AppSettings::instance().clearHardwareValues(mac);
        AppSettings::instance().save();
        auto s = std::make_unique<Session>();
        s->mac = mac;
        s->stationSettings = std::make_unique<AppSettings>(
            s->dir.filePath(QStringLiteral("NereusSDR.settings")));
        DaemonConfig cfg = testCoreConfig();
        cfg.sliceCount = 1;
        s->app.primeBoardForTest(board, mac);
        if (!s->app.start(cfg)) {
            return nullptr;
        }
        s->controller()->setRadioConnection(&s->radio);
        s->controller()->setTickTimerEnabled(false);
        s->server = std::make_unique<StationServer>(s->app.m_radioModel.get(),
                                                    *s->stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
        s->window = std::make_unique<RadioModel>(RadioModel::Role::Remote);
        s->proxy = std::make_unique<SettingsProxy>();
        s->client = std::make_unique<StationClient>(s->window.get(), s->proxy.get());
        auto* stationEnd = new LoopbackTransport(QStringLiteral("station-end"), this);
        auto* clientEnd = new LoopbackTransport(QStringLiteral("client-end"), this);
        stationEnd->linkTo(clientEnd);
        QSignalSpy completed(s->client.get(), &StationClient::handshakeComplete);
        s->client->startSession(clientEnd, s->server->token());
        s->server->acceptTransport(stationEnd);
        if (!completed.wait(5000) && completed.isEmpty()) {
            return nullptr;
        }
        return s;
    }

    QTemporaryDir m_securityDir;

private slots:
    void initTestCase() { QVERIFY(m_securityDir.isValid()); }

    void windowAttenuationReachesTheRadioAndIsSavedForItsBand()
    {
        const QString mac = QStringLiteral("02:00:00:00:46:01");
        auto s = join(HPSDRHW::Angelia, mac);
        QVERIFY(s != nullptr);
        QVERIFY(s->client->remoteRadioHardwareAvailable());
        // R-R3-46: 3 since the Core's antennas and hardware apply step (2)
        // and its I/O board and per-band antenna verb (3) came with it; the
        // attenuator is offered from 1. 4 added the filter policy verb
        // (R-R3-46 / R-R3-21).
        QCOMPARE(s->client->capabilities().radioHardwareVersion, 13);
        StepAttenuatorFacade* remote = s->remote();
        QVERIFY(!remote->isBound());
        QTRY_COMPARE(remote->maxDb(), 61);

        SliceModel* txSlice = s->app.m_radioModel->txBoundSlice();
        QVERIFY(txSlice != nullptr);
        txSlice->setFrequency(7'100'000.0);
        const QString bandKey = QStringLiteral("options/stepAtt/rx1Band/")
                                + bandKeyName(txSlice->band());

        QSignalSpy results(s->client.get(), &StationClient::propertyWriteCompleted);
        remote->setAttenuationDb(20);
        QTRY_VERIFY(!s->radio.attenuator.isEmpty() && s->radio.attenuator.last() == 20);
        QCOMPARE(s->controller()->attenuatorDb(), 20);
        QTRY_VERIFY(lastResultFor(results, "attenuationDb").seen);
        QVERIFY(lastResultFor(results, "attenuationDb").accepted);
        QCOMPARE(remote->attenuationDb(), 20);

        // Saved on the Core for this radio and this band, while it runs.
        QTRY_COMPARE(AppSettings::instance().hardwareValue(mac, bandKey).toInt(), 20);

        // The preamp setting goes the same way.
        remote->setPreampMode(static_cast<int>(PreampMode::Minus10));
        QTRY_COMPARE(s->controller()->preampMode(), PreampMode::Minus10);
    }

    void anan100dTakesFortyFiveAndSettlesSeventyAtItsMaximum()
    {
        auto s = join(HPSDRHW::Angelia, QStringLiteral("02:00:00:00:46:02"));
        QVERIFY(s != nullptr);
        StepAttenuatorFacade* remote = s->remote();
        QTRY_COMPARE(remote->maxDb(), 61);
        QCOMPARE(remote->minDb(), 0);

        QSignalSpy results(s->client.get(), &StationClient::propertyWriteCompleted);
        remote->setAttenuationDb(45);
        QTRY_VERIFY(lastResultFor(results, "attenuationDb").seen);
        QVERIFY(lastResultFor(results, "attenuationDb").accepted);
        QCOMPARE(s->controller()->attenuatorDb(), 45);
        // Above 31 dB an Alex board's step attenuator carries the value + 2
        // (console.cs:11044-11056 [v2.10.3.15]).
        QCOMPARE(s->radio.attenuator.last(), 47);

        results.clear();
        remote->setAttenuationDb(70);
        QTRY_VERIFY(lastResultFor(results, "attenuationDb").seen);
        const WriteResult settled = lastResultFor(results, "attenuationDb");
        QVERIFY(!settled.accepted);
        QCOMPARE(settled.reason, QStringLiteral("This radio's attenuator goes from 0 to 61 dB."));
        QVERIFY(OperatorWording::isPlain(settled.reason));
        QCOMPARE(s->controller()->attenuatorDb(), 61);
        QCOMPARE(remote->attenuationDb(), 61);
        QCOMPARE(s->radio.attenuator.last(), 63);
    }

    void hermesLite2ReportsItsRangeAndSettlesAdaptiveToClassic()
    {
        auto s = join(HPSDRHW::HermesLite, QStringLiteral("02:00:00:00:46:03"));
        QVERIFY(s != nullptr);
        StepAttenuatorFacade* remote = s->remote();
        QTRY_COMPARE(remote->minDb(), -28);
        QCOMPARE(remote->maxDb(), 31);

        QSignalSpy results(s->client.get(), &StationClient::propertyWriteCompleted);
        remote->setAttenuationDb(-10);
        QTRY_VERIFY(lastResultFor(results, "attenuationDb").seen);
        QVERIFY(lastResultFor(results, "attenuationDb").accepted);
        QCOMPARE(s->radio.attenuator.last(), -10);

        remote->setAutoAttMode(static_cast<int>(AutoAttMode::Adaptive));
        QTRY_VERIFY(lastResultFor(results, "autoAttMode").seen);
        const WriteResult settled = lastResultFor(results, "autoAttMode");
        QVERIFY(!settled.accepted);
        QCOMPARE(settled.reason, QStringLiteral("This radio offers only Classic auto-attenuate."));
        QVERIFY(OperatorWording::isPlain(settled.reason));
        QCOMPARE(remote->autoAttMode(), static_cast<int>(AutoAttMode::Classic));
        QCOMPARE(s->controller()->autoAttMode(), AutoAttMode::Classic);

        // The Hermes Lite 2 has no second preamp.
        remote->setRx1Preamp(true);
        QTRY_VERIFY(lastResultFor(results, "rx1Preamp").seen);
        QVERIFY(!lastResultFor(results, "rx1Preamp").accepted);
        QVERIFY(OperatorWording::isPlain(lastResultFor(results, "rx1Preamp").reason));
        QVERIFY(!remote->rx1Preamp());
    }

    void coreBandChangeRestoresTheBandAndTheWindowFollows()
    {
        auto s = join(HPSDRHW::Angelia, QStringLiteral("02:00:00:00:46:04"));
        QVERIFY(s != nullptr);
        StepAttenuatorFacade* remote = s->remote();
        SliceModel* txSlice = s->app.m_radioModel->txBoundSlice();
        QVERIFY(txSlice != nullptr);

        txSlice->setFrequency(7'100'000.0);
        remote->setAttenuationDb(20);
        QTRY_COMPARE(s->controller()->attenuatorDb(), 20);
        txSlice->setFrequency(14'200'000.0);
        remote->setAttenuationDb(5);
        QTRY_COMPARE(s->controller()->attenuatorDb(), 5);

        txSlice->setFrequency(7'150'000.0);
        QCOMPARE(s->controller()->attenuatorDb(), 20);
        QTRY_COMPARE(remote->attenuationDb(), 20);
        // The radio runs the restored value too (Thetis RX1Band setter).
        QCOMPARE(s->radio.attenuator.last(), 20);
        txSlice->setFrequency(14'250'000.0);
        QTRY_COMPARE(remote->attenuationDb(), 5);
        QCOMPARE(s->radio.attenuator.last(), 5);
    }

    void overloadOnEitherAdcReachesTheWindow()
    {
        auto s = join(HPSDRHW::Angelia, QStringLiteral("02:00:00:00:46:05"));
        QVERIFY(s != nullptr);
        StepAttenuatorFacade* remote = s->remote();
        StepAttenuatorController* controller = s->controller();

        controller->onAdcOverflow(1);
        controller->tick();
        QTRY_COMPARE(remote->overloadAdc1(), 1);
        QCOMPARE(remote->overloadAdc0(), 0);
        for (int i = 0; i < 4; ++i) {
            controller->onAdcOverflow(1);
            controller->tick();
        }
        QTRY_COMPARE(remote->overloadAdc1(), 2);

        controller->onAdcOverflow(0);
        controller->tick();
        QTRY_COMPARE(remote->overloadAdc0(), 1);

        // It clears on the window when it clears on the Core.
        for (int i = 0; i < 6; ++i) {
            controller->tick();
        }
        QTRY_COMPARE(remote->overloadAdc0(), 0);
        QTRY_COMPARE(remote->overloadAdc1(), 0);
    }

    // R-R3-46 / R-R3-11: slice B on the other ADC reads and sets that ADC's
    // own attenuator through the window; slice A keeps attenuationDb.
    void aSliceOnTheOtherAdcShowsAndSetsItsOwnAttenuator()
    {
        auto s = join(HPSDRHW::Angelia, QStringLiteral("02:00:00:00:46:06"));
        QVERIFY(s != nullptr);
        QCOMPARE(s->client->capabilities().adcAttenuatorVersion, 1);
        StepAttenuatorFacade* remote = s->remote();
        StepAttenuatorController* controller = s->controller();

        // Slice A (0) on ADC0, slice B (1) on ADC1.
        controller->setAttenuation(20);
        controller->setAdcRouting(0, 1, Band::Band40m, false, 1u << 1);
        QTRY_COMPARE(remote->rx2SliceMask(), 2);
        QVERIFY(!remote->sliceUsesRx2(0));
        QVERIFY(remote->sliceUsesRx2(1));
        QTRY_COMPARE(remote->attenuationDbForSlice(0), 20);
        QCOMPARE(remote->attenuationDbForSlice(1), controller->rx2AttenuatorDb());

        QSignalSpy results(s->client.get(), &StationClient::propertyWriteCompleted);
        remote->setAttenuationDbForSlice(1, 12);
        QTRY_VERIFY(lastResultFor(results, "rx2AttenuationDb").seen);
        QVERIFY(lastResultFor(results, "rx2AttenuationDb").accepted);
        QCOMPARE(controller->rx2AttenuatorDb(), 12);
        QCOMPARE(controller->attenuatorDb(), 20);
        QCOMPARE(remote->attenuationDbForSlice(1), 12);
        QCOMPARE(remote->attenuationDbForSlice(0), 20);

        // RX2's own enable and auto-attenuate settings reach the Core.
        remote->setRx2StepAttEnabled(false);
        QTRY_VERIFY(!controller->rx2StepAttEnabled());
        QVERIFY(controller->stepAttEnabled());
        remote->setRx2AutoAttEnabled(true);
        remote->setRx2AutoAttUndo(true);
        remote->setRx2AutoAttUndoDelayMs(7000);
        QTRY_VERIFY(controller->rx2AutoAttEnabled());
        QTRY_VERIFY(controller->rx2AutoAttUndo());
        QTRY_COMPARE(controller->rx2AutoUndoDelaySec(), 7);
        controller->setRx2AutoAttUndo(false);
        QTRY_VERIFY(!remote->rx2AutoAttUndo());

        // Level Cal: RX2's own preamp mode, for the slice on the other ADC,
        // leaves slice A's mode alone; a mode RX2's list lacks is refused
        // with its reason.
        const int rx1Mode = remote->preampModeForSlice(0);
        remote->setPreampModeForSlice(1, static_cast<int>(PreampMode::SaMinus20));
        QTRY_VERIFY(lastResultFor(results, "rx2PreampMode").seen);
        QVERIFY(lastResultFor(results, "rx2PreampMode").accepted);
        QTRY_COMPARE(controller->rx2PreampMode(), PreampMode::SaMinus20);
        QCOMPARE(static_cast<int>(controller->preampMode()), rx1Mode);
        QCOMPARE(remote->preampModeForSlice(1), static_cast<int>(PreampMode::SaMinus20));
        controller->setRx2PreampMode(PreampMode::SaMinus10);
        QTRY_COMPARE(remote->rx2PreampMode(), static_cast<int>(PreampMode::SaMinus10));
        results.clear();
        remote->setRx2PreampMode(static_cast<int>(PreampMode::Minus40));
        QTRY_VERIFY(lastResultFor(results, "rx2PreampMode").seen);
        QCOMPARE(controller->rx2PreampMode(), PreampMode::SaMinus10);
        QTRY_COMPARE(remote->rx2PreampMode(), static_cast<int>(PreampMode::SaMinus10));

        // Back on one ADC: every slice reads attenuationDb again.
        controller->setAdcRouting(0, -1, Band::Band40m, false);
        QTRY_COMPARE(remote->rx2SliceMask(), 0);
        QCOMPARE(remote->attenuationDbForSlice(1), 20);
    }

    // R-R3-46 / R-R3-11: a remote window's Setup > General > Options sets
    // RX2's own enable and auto-attenuate on the Core (two-ADC radio).
    void remoteSetupSetsRx2OnTheCore()
    {
        auto s = join(HPSDRHW::Angelia, QStringLiteral("02:00:00:00:46:08"));
        QVERIFY(s != nullptr);
        StepAttenuatorController* controller = s->controller();
        controller->setAdcRouting(0, 1, Band::Band40m, false, 1u << 1);
        // MainWindow tells the object the Core takes this window's edits.
        s->remote()->setWindowAvailability(true, QString());
        GeneralOptionsPage page(s->window.get());
        auto* enable = page.findChild<QCheckBox*>(QStringLiteral("chkRx2StepAttEnable"));
        auto* rx2Auto = page.findChild<QGroupBox*>(QStringLiteral("grpAutoAttRx2"));
        QVERIFY(enable && rx2Auto);
        QTRY_VERIFY2(enable->isEnabled(), qPrintable(enable->toolTip()));
        QTRY_VERIFY(rx2Auto->isEnabled());
        QVERIFY(enable->isChecked());
        enable->click();
        QTRY_VERIFY(!controller->rx2StepAttEnabled());
        QVERIFY(controller->stepAttEnabled());
        QCheckBox* autoEnable = nullptr;
        for (QCheckBox* c : rx2Auto->findChildren<QCheckBox*>()) {
            if (c->text() == QStringLiteral("Enable")) { autoEnable = c; }
        }
        QVERIFY(autoEnable);
        autoEnable->click();
        QTRY_VERIFY(controller->rx2AutoAttEnabled());
        controller->setRx2AutoAttEnabled(false);
        QTRY_VERIFY(!autoEnable->isChecked());
    }

    // R-R3-46 / R-R3-11: a peer whose hello did not declare adcAttenuators
    // gets today's stepAtt (no rx2AttenuationDb, no rx2SliceMask) and no
    // adcAttenuatorVersion; one that did gets all three.
    void onlyAPeerThatDeclaredItGetsTheOtherAdcsAttenuator()
    {
        auto s = join(HPSDRHW::Angelia, QStringLiteral("02:00:00:00:46:07"));
        QVERIFY(s != nullptr);
        for (const bool declares : {false, true}) {
            auto* station = new LoopbackTransport(QStringLiteral("raw-station"), this);
            auto* peer = new LoopbackTransport(QStringLiteral("raw-peer"), this);
            station->linkTo(peer);
            s->server->acceptTransport(station);
            QHash<QByteArray, int> features;
            if (declares) {
                features.insert(QByteArrayLiteral("adcAttenuators"), 1);
            }
            peer->sendText(SessionMessages::encode(SessionMessages::hello(
                kSessionProtocolMajor, kSessionProtocolMinor, 0, QStringLiteral("raw-peer"),
                {kSessionProtocolMajor}, features)));
            peer->sendText(SessionMessages::encode(SessionMessages::authRequest(s->server->token())));
            QTRY_VERIFY(peer->receivedKinds().contains(QByteArrayLiteral("snapshot.complete")));

            bool sawVersion = false;
            bool sawSchemaField = false;
            bool sawStepAtt = false;
            bool sawValue = false;
            for (const QByteArray& raw : peer->received()) {
                SessionMessage m;
                if (!SessionMessages::decode(raw, &m)) {
                    continue;
                }
                if (m.kind == SessionMessageKind::Capabilities) {
                    for (const MirrorUpdate& u : m.updates) {
                        sawVersion = sawVersion || u.name == "adcAttenuatorVersion";
                    }
                } else if (m.kind == SessionMessageKind::Schema
                           && m.className == "StepAttenuatorFacade") {
                    for (const SessionSchemaField& f : m.fields) {
                        sawSchemaField = sawSchemaField || f.name == "rx2AttenuationDb"
                            || f.name == "rx2SliceMask" || f.name == "rx2StepAttEnabled"
                            || f.name == "rx2AutoAttEnabled" || f.name == "rx2PreampMode";
                    }
                } else if (m.kind == SessionMessageKind::ObjectCreate && m.objectKey == "stepAtt") {
                    sawStepAtt = true;
                    for (const MirrorUpdate& u : m.updates) {
                        sawValue = sawValue || u.name == "rx2AttenuationDb"
                            || u.name == "rx2SliceMask";
                    }
                }
            }
            QVERIFY(sawStepAtt);
            QCOMPARE(sawVersion, declares);
            QCOMPARE(sawSchemaField, declares);
            QCOMPARE(sawValue, declares);

            // A later change reaches only the peer that declared it.
            peer->clearReceived();
            s->controller()->setRx2Attenuation(declares ? 9 : 7);
            QCoreApplication::processEvents();
            QTest::qWait(50);
            bool sawDelta = false;
            for (const QByteArray& raw : peer->received()) {
                SessionMessage m;
                if (SessionMessages::decode(raw, &m) && m.kind == SessionMessageKind::Delta
                    && m.objectKey == "stepAtt") {
                    for (const MirrorUpdate& u : m.updates) {
                        sawDelta = sawDelta || u.name == "rx2AttenuationDb";
                    }
                }
            }
            QCOMPARE(sawDelta, declares);
            station->closeLink(QStringLiteral("done"));
        }
    }
};

QTEST_MAIN(TstRemoteStepAttenuator)
#include "tst_remote_step_attenuator.moc"
