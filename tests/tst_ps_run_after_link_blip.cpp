// no-port-check: NereusSDR-original test. The Thetis behaviour it guards is
// cited beside the code in RadioModel.cpp and P1RadioConnection.cpp; this
// file translates no C#.
//
// tests/tst_ps_run_after_link_blip.cpp
//
// TX safety fix round 1, I2 (2026-09-30), and its follow-up. Every new link
// starts with the PureSignal run flag off: P1 clears it for each reconnect
// (P1RadioConnection::dropTransmitForNewLink) and P2's connectToRadio clears
// it after the model's pre-start push. With PS-A running, a lost link turns
// PureSignal off on its readiness check (isConnected) within a tick, and the
// link back re-arms PS-A (RadioModel's Connected hook calls
// resumeAutomaticCalibrationPreference), so the run flag must be on the wire
// again once PureSignal reports itself enabled. These tests follow that path
// as it runs in use, with PureSignal's timers running throughout.
//
// Against in-process fake radios only (never a real radio); nothing keys.
//   1. P1: PS-A running, the link goes silent, the connection is held at its
//      LinkLost so no reconnect attempt is spent, PureSignal turns itself
//      off, the hold lifts, the connection reconnects by itself, PS-A
//      re-arms, and bank 11's C2 bit 6 (puresignal_run) is back on ep2.
//   2. P2: PS-A running, the link goes silent (LinkLost), PureSignal turns
//      itself off, the link is rebuilt as the hosted retry does
//      (disconnectFromRadio, then connectToRadio), PS-A re-arms, and the new
//      connection's run flag is on again. P2 carries the flag on the wire
//      only while keyed (the DDC frequency override and the PS DDC enable),
//      so this test reads the flag the next key would send.
//
// Modification history (NereusSDR):
//   2026-10-01  J.J. Boyd / KG4VCF: run the P2 peer on its own thread so
//               GUI pauses cannot silence it. AI-assisted via OpenAI Codex.

#include <QtTest/QtTest>
#include <QFile>
#include <QScopeGuard>
#include <QSemaphore>
#include <QTimer>
#include <QThread>

#include <atomic>
#include <memory>

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/DspControlThread.h"
#include "core/P1RadioConnection.h"
#include "core/P2RadioConnection.h"
#include "core/PureSignal.h"
#include "core/WdspEngine.h"
#include "core/session/PureSignalSessionFacade.h"
#include "models/RadioModel.h"
#include "fakes/ConnectableRadioModel.h"
#include "fakes/FakeAudioBus.h"
#include "fakes/P1FakeRadio.h"
#include "fakes/P2FakeRadio.h"

using namespace NereusSDR;
using NereusSDR::Test::ConnectableRadioModel;
using NereusSDR::Test::P1FakeRadio;
using NereusSDR::Test::P2FakeRadio;

namespace {

constexpr int kTestWatchdogSilenceMs   = 300;
constexpr int kTestReconnectIntervalMs = 300;
constexpr quint8 kBank11Address = 0x14;

// Any ep2 subframe in `log` that is bank 11 with C2 bit 6 (puresignal_run).
bool bank11PsRun(const QList<QByteArray>& log)
{
    for (const QByteArray& cc : log) {
        for (int sub = 0; sub < 2; ++sub) {
            const quint8 c0 = static_cast<quint8>(cc.at(sub * 5));
            if ((c0 & 0xFE) == kBank11Address
                && (static_cast<quint8>(cc.at(sub * 5 + 2)) & 0x40) != 0) {
                return true;
            }
        }
    }
    return false;
}

// Starts PS-A from the model's facade, as the [PS-A] button does.
bool startPsA(RadioModel& model)
{
    PureSignalSessionFacade* facade = model.pureSignalFacade();
    if (facade == nullptr
        || !QTest::qWaitFor([facade]() { return facade->canActuate(); }, 10000)) {
        return false;
    }
    facade->executeAction(Ps3Action::StartAutomatic, {}, 1);
    return true;
}

// Waits for PureSignal to report itself enabled (want = true) or off, its
// own timers running as in use.
bool waitPsEnabled(RadioModel& model, bool want)
{
    return QTest::qWaitFor(
        [&model, want]() {
            PureSignal* ps = model.pureSignal();
            return ps != nullptr && ps->isPsEnabled() == want;
        },
        10000);
}

// The run flag as the connection holds it, read on its own thread.
bool connectionPsRun(RadioConnection* conn)
{
    bool run = false;
    QMetaObject::invokeMethod(conn, [conn, &run]() { run = conn->puresignalRunForTest(); },
                              Qt::BlockingQueuedConnection);
    return run;
}

bool waitLanesIdle(RadioModel& model)
{
    constexpr int kLaneTimeoutMs = 600000;
    for (DspControlThread* lane : {model.transmitLane(), model.receiveLane()}) {
        if (lane != nullptr
            && !QTest::qWaitFor([lane]() { return lane->waitIdleForTest(0); },
                                kLaneTimeoutMs)) {
            return false;
        }
    }
    return true;
}

void installOpenAudioBuses(AudioEngine& engine)
{
    AudioFormat format;
    format.sampleRate = 48000;
    format.channels = 2;
    format.sample = AudioFormat::Sample::Float32;
    auto speakers = std::make_unique<NereusSDR::FakeAudioBus>(QStringLiteral("Fake speakers"));
    auto txInput = std::make_unique<NereusSDR::FakeAudioBus>(QStringLiteral("Fake TX input"));
    speakers->open(format);
    txInput->open(format);
    engine.setSpeakersBusForTest(std::move(speakers));
    engine.setTxInputBusForTest(std::move(txInput));
}

} // namespace

class TestPsRunAfterLinkBlip : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // PS-A saves its preference; keep it in this run's own file.
        AppSettings::setProfileOverride(QStringLiteral("ps-run-after-link-blip-%1")
                                            .arg(QCoreApplication::applicationPid()));
    }

    void cleanupTestCase()
    {
        QFile::remove(AppSettings::instance().filePath());
    }

    void p1SelfReconnectPutsPsRunBack()
    {
        std::unique_ptr<ConnectableRadioModel> harness = ConnectableRadioModel::create();
        QVERIFY2(harness != nullptr, "the model never reached Connected");
        RadioModel& model = harness->model();
        auto* p1 = qobject_cast<P1RadioConnection*>(model.connection());
        QVERIFY(p1 != nullptr);
        QMetaObject::invokeMethod(p1, [p1]() {
            p1->setReconnectTimingForTest(kTestWatchdogSilenceMs, kTestReconnectIntervalMs);
        }, Qt::BlockingQueuedConnection);

        QVERIFY2(startPsA(model), "PS-A could not start");
        QVERIFY2(waitPsEnabled(model, true), "PS-A never reported PureSignal enabled");
        harness->fake().clearEp2CcLog();
        QTRY_VERIFY_WITH_TIMEOUT(bank11PsRun(harness->fake().ep2CcReceived()), 5000);

        // The link goes silent and the connection declares LinkLost. The
        // connection is held at that point, on its own thread, before it
        // arms its first reconnect, so none of its bounded reconnect
        // attempts is spent while PureSignal turns itself off. The radio
        // answers again at once; the reconnect starts when the hold lifts.
        // (Fix round 2: the test used to keep the radio silent until
        // PureSignal was off, so a slow turn-off spent all three attempts
        // and failed the test for an unrelated reason.)
        bool sawLinkLost = false;
        P1FakeRadio* fake = &harness->fake();
        const QMetaObject::Connection watch = connect(
            &model, &RadioModel::connectionStateChanged, this,
            [&sawLinkLost](ConnectionState s) {
                if (s == ConnectionState::LinkLost) {
                    sawLinkLost = true;
                }
            });
        auto lift = std::make_shared<QSemaphore>();
        auto held = std::make_shared<std::atomic<bool>>(false);
        const QMetaObject::Connection hold = connect(
            p1, &RadioConnection::connectionStateChanged, p1,
            [lift, held](ConnectionState s) {
                if (s == ConnectionState::LinkLost && !held->exchange(true)) {
                    lift->tryAcquire(1, 10000);
                }
            },
            Qt::DirectConnection);
        auto release = qScopeGuard([lift]() { lift->release(); });
        fake->goSilent();
        QTRY_VERIFY_WITH_TIMEOUT(sawLinkLost, 5000);
        QVERIFY(held->load());
        disconnect(watch);
        disconnect(hold);
        fake->resume();
        const bool psWentOff = waitPsEnabled(model, false);
        lift->release();
        release.dismiss();
        QVERIFY2(psWentOff, "PureSignal stayed on with the link down");
        QTRY_COMPARE_WITH_TIMEOUT(model.connectionState(), ConnectionState::Connected, 10000);
        QCOMPARE(model.connection(), static_cast<RadioConnection*>(p1));
        QVERIFY(fake->metisStopCount() >= 1);

        // PS-A re-arms by itself, and the wire says so again.
        QVERIFY2(waitPsEnabled(model, true), "PS-A did not re-arm after the link came back");
        fake->clearEp2CcLog();
        QTRY_VERIFY_WITH_TIMEOUT(bank11PsRun(fake->ep2CcReceived()), 5000);
        QVERIFY(connectionPsRun(p1));
    }

    void p2LinkBackPutsPsRunBack()
    {
        QThread peerThread;
        peerThread.setObjectName(QStringLiteral("P2FakeRadio"));
        P2FakeRadio fake;
        QTimer ingress(&fake);
        ingress.setInterval(5);
        connect(&ingress, &QTimer::timeout, &fake, [&fake]() {
            if (fake.hasClient()) {
                fake.sendDdc(2);
                fake.sendDdc(3);
                fake.sendStatus();
            }
        });
        // A real radio's ingress continues independently of the GUI. All
        // peer sockets, its timer, and its mutable state stay on this thread.
        QThread* const ownerThread = QThread::currentThread();
        fake.moveToThread(&peerThread);
        peerThread.start();
        const auto stopPeer = qScopeGuard([&fake, &ingress, &peerThread, ownerThread]() {
            QMetaObject::invokeMethod(&fake, [&fake, &ingress, ownerThread]() {
                ingress.stop();
                fake.stop();
                fake.moveToThread(ownerThread);
            }, Qt::BlockingQueuedConnection);
            peerThread.quit();
            peerThread.wait();
        });
        const auto onPeer = [&fake](auto work) {
            QMetaObject::invokeMethod(&fake, work, Qt::BlockingQueuedConnection);
        };
        bool started = false;
        RadioInfo info;
        quint16 outboundPortBase = 0;
        quint16 inputRolePortBase = 0;
        onPeer([&]() {
            started = fake.start();
            info = fake.radioInfo();
            outboundPortBase = fake.outboundPortBase();
            inputRolePortBase = fake.inputRolePortBase();
            ingress.start();
        });
        QVERIFY(started);

        RadioModel model;
        model.audioEngine()->setStartInitializerForTest(installOpenAudioBuses);
        model.wdspEngine()->setSynchronousInitForTest(true);
        model.configureP2TransportForTest(outboundPortBase, inputRolePortBase, 1000, 200);

        model.connectToRadio(info);
        QTRY_VERIFY_WITH_TIMEOUT(model.isConnected(), 15000);
        QVERIFY(waitLanesIdle(model));
        // A radio keeps sending while the GUI event loop is occupied. This
        // bounded pause reproduces a main-thread fake starving its own
        // ingress timer past the unchanged 200 ms connection watchdog.
        QThread::msleep(250);
        QCoreApplication::processEvents();
        QVERIFY2(model.isConnected(), "the fake radio went silent during a GUI pause");
        QVERIFY2(startPsA(model), "PS-A could not start");
        QVERIFY2(waitPsEnabled(model, true), "PS-A never reported PureSignal enabled");
        QTRY_VERIFY_WITH_TIMEOUT(connectionPsRun(model.connection()), 5000);

        // The link goes silent: P2 sends one unkeyed stop and stays down,
        // and PureSignal turns itself off.
        onPeer([&fake]() { fake.stopIngress(); });
        QTRY_COMPARE_WITH_TIMEOUT(model.connectionState(), ConnectionState::LinkLost, 5000);
        int moxAssertedCount = 0;
        onPeer([&]() { moxAssertedCount = fake.moxAssertedCount(); });
        QCOMPARE(moxAssertedCount, 0);
        QVERIFY2(waitPsEnabled(model, false), "PureSignal stayed on with the link down");

        // The link is rebuilt as the hosted retry does it.
        onPeer([&fake]() { fake.resumeIngress(); });
        model.retireConnectionForRecovery();
        model.connectToRadio(info);
        QTRY_VERIFY_WITH_TIMEOUT(model.isConnected(), 15000);
        QVERIFY(waitLanesIdle(model));
        QVERIFY(qobject_cast<P2RadioConnection*>(model.connection()) != nullptr);

        // PS-A re-arms by itself, and the rebuilt link's run flag follows.
        QVERIFY2(waitPsEnabled(model, true), "PS-A did not re-arm after the link came back");
        QTRY_VERIFY_WITH_TIMEOUT(connectionPsRun(model.connection()), 5000);
        onPeer([&]() { moxAssertedCount = fake.moxAssertedCount(); });
        QCOMPARE(moxAssertedCount, 0);

        onPeer([&ingress]() { ingress.stop(); });
        model.disconnectFromRadio();
    }
};

QTEST_MAIN(TestPsRunAfterLinkBlip)
#include "tst_ps_run_after_link_blip.moc"
