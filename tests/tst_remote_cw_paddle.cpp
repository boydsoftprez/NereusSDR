// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_remote_cw_paddle.cpp  (NereusSDR)
// =================================================================
//
// Hot keys (R-HK-13): cw.paddle, a remote window's dot and dash, over the
// loopback link to a test Core. The Core's radio is a fake connection and its
// CWX sender runs on an injected clock: nothing opens a radio socket and nothing
// keys a radio.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-05: Hot keys plan Task 6. J.J. Boyd (KG4VCF), AI-assisted via
//               Claude Code.
// =================================================================

#include "MultiDeviceHarness.h"
#include "fakes/QuietRadioConnection.h"

#include "core/CwKeyLine.h"
#include "core/CwTypes.h"
#include "core/CwxSender.h"
#include "core/MoxController.h"
#include "core/safety/RemoteCwSafetyState.h"
#include "core/safety/RemoteTxCutGuard.h"
#include "core/safety/RemoteTxWatchdog.h"
#include "core/safety/TxRefusal.h"
#include "core/session/RemoteTransmitClient.h"

#include <QEvent>
#include <QPointer>

#include <chrono>
#include <condition_variable>
#include <mutex>

namespace {

const QString kUpdateForPaddle = QStringLiteral("Update this app to send dot and dash through this Core.");

// As tst_remote_cw: test-owned monotonic time for the sender's worker.
class ControlledCwxTime {
public:
    using TimePoint = std::chrono::steady_clock::time_point;

    TimePoint now()
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        return timePoint(m_nowMs);
    }
    void sleep(TimePoint until)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_shutdown || timePoint(m_nowMs) >= until) {
            return;
        }
        if (m_wakePending) {
            m_wakePending = false;
            return;
        }
        const quint64 generation = m_generation;
        m_changed.wait(lock, [this, generation]() { return m_shutdown || m_generation != generation; });
        m_wakePending = false;
    }
    void advanceTo(qint64 ms)
    {
        {
            const std::lock_guard<std::mutex> lock(m_mutex);
            m_nowMs = ms;
            m_wakePending = true;
            ++m_generation;
        }
        m_changed.notify_all();
    }
    void wake()
    {
        {
            const std::lock_guard<std::mutex> lock(m_mutex);
            m_wakePending = true;
            ++m_generation;
        }
        m_changed.notify_all();
    }
    void shutdown()
    {
        {
            const std::lock_guard<std::mutex> lock(m_mutex);
            m_shutdown = true;
        }
        m_changed.notify_all();
    }
    std::unique_ptr<CwxSender> makeSender()
    {
        return std::make_unique<CwxSender>([this]() { return now(); },
                                           [this](TimePoint until) { sleep(until); });
    }

private:
    static TimePoint timePoint(qint64 ms)
    {
        return TimePoint{} + std::chrono::hours(1000) + std::chrono::milliseconds(ms);
    }
    std::mutex m_mutex;
    std::condition_variable m_changed;
    qint64 m_nowMs{0};
    quint64 m_generation{0};
    bool m_wakePending{false};
    bool m_shutdown{false};
};

QHash<QByteArray, int> peerDeclaring(int remoteCw)
{
    return {{"deviceAuth", 1}, {"sessionHolder", 1}, {"remoteTx", 1}, {"remoteCw", remoteCw}, {"cwSettings", 1}};
}

// A test Core on a fake connection: 20 m CWU, Semi break-in, CWX at 5 WPM.
struct PaddleCore {
    ControlledCwxTime time; // Outlives Core's model and sender.
    Core core;
    NereusSDR::Test::QuietRadioConnection conn;

    PaddleCore()
        : core(true, Core::ModelFactory([this]() {
              return std::make_unique<RadioModel>(RadioModel::Role::Local, time.makeSender(), nullptr);
          }))
    {
        core.model->injectConnectionForTest(&conn);
        core.model->wireCwKeyerConnectionForTest();
        allowTransmit(core);
        // No keepalives here: the link's 400 ms hold runs only when a test says.
        core.server->txCutGuard()->useControlledClockForTest(RemoteTxCutGuard::Clock::time_point{});
        core.model->transmitModel().setCwBreakInMode(static_cast<int>(CwBreakInMode::Semi));
        core.model->transmitModel().setCwxSpeed(CwxSender::kMinWpm);
        if (SliceModel* slice = core.model->sliceById(0)) {
            slice->setDspMode(DSPMode::CWU);
            slice->setFrequency(14050000.0);
        }
        QCoreApplication::processEvents();
    }
    ~PaddleCore()
    {
        if (CwxSender* sender = core.model->cwxSender()) {
            sender->stop();
        }
        time.shutdown();
        conn.cwKeyLine()->setOnChange({});
        core.server.reset();
        core.model->injectConnectionForTest(nullptr);
    }
    CwxSender* cwx() const { return core.model->cwxSender(); }
    CwKeyLine* line() { return conn.cwKeyLine().get(); }
    quint32 epoch() const { return core.model->remoteCwActivity().epoch; }
    QJsonObject paddle(LoopbackTransport* app, bool dot, bool dash)
    {
        return core.invoke(app, "cw.paddle",
                           {MirrorUpdate{0, "dot", MirrorWireKind::Bool, QVariant(dot)},
                            MirrorUpdate{1, "dash", MirrorWireKind::Bool, QVariant(dash)}});
    }
    bool bitsUp() { return !line()->dot() && !line()->dash() && !line()->cwx() && !line()->cwxPtt(); }
};

bool accepted(const QJsonObject& result)
{
    return result.value(QStringLiteral("accepted")).toBool();
}

QString reasonOf(const QJsonObject& result)
{
    return result.value(QStringLiteral("reason")).toString();
}

QString valueOf(const QJsonObject& result, const QString& name)
{
    for (const QJsonValue& v : result.value(QStringLiteral("values")).toArray()) {
        if (v.toObject().value(QStringLiteral("name")).toString() == name) {
            return v.toObject().value(QStringLiteral("value")).toString();
        }
    }
    return {};
}

} // namespace

class TstRemoteCwPaddle : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    // ---- Versions ----

    void aPeerThatDeclared3Or2GetsTheCoresVersionAnd1Gets1()
    {
        PaddleCore rig;
        Device a(QStringLiteral("iPhone"), QStringLiteral("phone"));
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        Device c(QStringLiteral("iPad 2"), QStringLiteral("tablet"));
        rig.core.pair(a);
        rig.core.pair(b);
        rig.core.pair(c);
        LoopbackTransport* current = rig.core.signIn(a, peerDeclaring(3));
        LoopbackTransport* two = rig.core.signIn(b, peerDeclaring(2));
        LoopbackTransport* one = rig.core.signIn(c, peerDeclaring(1));
        QVERIFY(admitted(current));
        QVERIFY(admitted(two));
        QVERIFY(admitted(one));
        QCOMPARE(capability(current->received(), QStringLiteral("remoteCwVersion")), std::optional<qint64>(3));
        QCOMPARE(capability(two->received(), QStringLiteral("remoteCwVersion")), std::optional<qint64>(3));
        QCOMPARE(capability(one->received(), QStringLiteral("remoteCwVersion")), std::optional<qint64>(1));
    }

    void aStartFromAPeerThatDidNotDeclare3IsRefusedAndAReleaseIsTaken()
    {
        PaddleCore rig;
        Device phone(QStringLiteral("iPhone"), QStringLiteral("phone"));
        rig.core.pair(phone);
        LoopbackTransport* app = rig.core.signIn(phone, peerDeclaring(2));
        QVERIFY(admitted(app));
        QVERIFY(accepted(rig.core.invoke(app, "tx.take")));
        for (const auto& [dot, dash] : QList<QPair<bool, bool>>{{true, false}, {false, true}, {true, true}}) {
            const QJsonObject result = rig.paddle(app, dot, dash);
            QVERIFY(!accepted(result));
            QCOMPARE(reasonOf(result), kUpdateForPaddle);
        }
        QVERIFY(rig.bitsUp());
        QCOMPARE(rig.epoch(), 0u);
        QVERIFY(accepted(rig.paddle(app, false, false)));
        // Everything version 2 uses is unchanged for that peer.
        QVERIFY(accepted(rig.core.invoke(app, "cw.key",
                                         {MirrorUpdate{0, "down", MirrorWireKind::Bool, QVariant(true)}})));
        QVERIFY(rig.line()->cwx());
        QVERIFY(accepted(rig.core.invoke(app, "cw.key",
                                         {MirrorUpdate{0, "down", MirrorWireKind::Bool, QVariant(false)}})));
        QVERIFY(!rig.line()->cwx());
    }

    void malformedArgumentsAreRefused()
    {
        PaddleCore rig;
        Device mac(QStringLiteral("iPhone"), QStringLiteral("phone"));
        rig.core.pair(mac);
        LoopbackTransport* app = rig.core.signIn(mac, peerDeclaring(3));
        QVERIFY(admitted(app));
        QVERIFY(accepted(rig.core.invoke(app, "tx.take")));
        const QString unreadable = QStringLiteral("The Core could not read this request.");
        QCOMPARE(reasonOf(rig.core.invoke(app, "cw.paddle",
                 {MirrorUpdate{0, "dot", MirrorWireKind::Bool, QVariant(true)}})), unreadable);
        QCOMPARE(reasonOf(rig.core.invoke(app, "cw.paddle",
                 {MirrorUpdate{0, "dot", MirrorWireKind::Int64, QVariant(qint64(1))},
                  MirrorUpdate{1, "dash", MirrorWireKind::Bool, QVariant(false)}})), unreadable);
        QVERIFY(rig.bitsUp());
    }

    // ---- The holder rule ----

    void aNonHolderIsRefusedWithTheHoldersWords()
    {
        PaddleCore rig;
        Device a(QStringLiteral("iPhone"), QStringLiteral("phone"));
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        rig.core.pair(a);
        rig.core.pair(b);
        LoopbackTransport* appA = rig.core.signIn(a, peerDeclaring(3));
        LoopbackTransport* appB = rig.core.signIn(b, peerDeclaring(3));
        QVERIFY(admitted(appA));
        QVERIFY(admitted(appB));

        const QJsonObject unheld = rig.paddle(appA, true, false);
        QVERIFY(!accepted(unheld));
        QCOMPARE(reasonOf(unheld), TxRefusals::notHolder().text);
        QCOMPARE(valueOf(unheld, QStringLiteral("refusalCode")), QStringLiteral("notHolder"));
        QVERIFY(rig.bitsUp());

        // Taking transmit does not bring a refused press back: the key held
        // through it says the same again, and that is no press.
        QVERIFY(accepted(rig.core.invoke(appA, "tx.take")));
        QVERIFY(accepted(rig.paddle(appA, true, false)));
        QVERIFY(rig.bitsUp());
        QVERIFY(accepted(rig.paddle(appA, false, false)));
        QVERIFY(accepted(rig.paddle(appA, true, false)));
        QVERIFY(rig.line()->dot());
        const QJsonObject other = rig.paddle(appB, false, true);
        QVERIFY(!accepted(other));
        QVERIFY(reasonOf(other).contains(QStringLiteral("has the transmitter")));
        QCOMPARE(valueOf(other, QStringLiteral("refusalCode")), QString::fromUtf8(TxRefusals::kOtherDeviceHolds));
        QVERIFY(rig.line()->dot());
        QVERIFY(!rig.line()->dash());
        // Another device's release ends nothing of the holder's.
        QVERIFY(accepted(rig.paddle(appB, false, false)));
        QVERIFY(rig.line()->dot());
        QVERIFY(accepted(rig.paddle(appA, false, false)));
        QVERIFY(rig.bitsUp());
    }

    // ---- The bits ----

    void dotAndDashSetOnlyTheSoftBitsAndWatchBeforeTheFirstBit()
    {
        PaddleCore rig;
        Device mac(QStringLiteral("iPhone"), QStringLiteral("phone"));
        rig.core.pair(mac);
        LoopbackTransport* app = rig.core.signIn(mac, peerDeclaring(3));
        QVERIFY(admitted(app));
        QVERIFY(accepted(rig.core.invoke(app, "tx.take")));
        std::atomic<bool> armedAtFirstBit{false};
        std::atomic<int> bitsBeforeArmed{0};
        rig.line()->setOnChange([&]() {
            if (rig.line()->dot() || rig.line()->dash()) {
                const bool armed = rig.core.server->txCutGuard()->isJudging()
                    && rig.core.server->txWatchdog()->isWatching(mac.key.fingerprint());
                armedAtFirstBit.store(armed);
                bitsBeforeArmed.fetch_add(armed ? 0 : 1);
            }
        });

        const QJsonObject dot = rig.paddle(app, true, false);
        QVERIFY2(accepted(dot), qPrintable(reasonOf(dot)));
        QCOMPARE(int(rig.line()->p1Bits(true)), 0x04); // no CWX bit, no CWX PTT on the HL2 either
        QVERIFY(armedAtFirstBit.load());
        QCOMPARE(bitsBeforeArmed.load(), 0);
        QVERIFY(!rig.cwx()->isKeyHeld());
        QVERIFY(rig.cwx()->isRemotePaddleHeld());
        QVERIFY(rig.core.model->cwxActiveFor(mac.key.fingerprint()));
        QVERIFY(!rig.core.model->isTransmitting()); // no MOX and no PTT write
        const quint32 epoch = rig.epoch();
        QVERIFY(epoch != 0);

        // A change keeps the activity, and never lifts the key that stays down.
        std::atomic<int> dotLifts{0};
        rig.line()->setOnChange([&]() { dotLifts.fetch_add(rig.line()->dot() ? 0 : 1); });
        QVERIFY(accepted(rig.paddle(app, true, true)));
        QCOMPARE(int(rig.line()->p1Bits(true)), 0x06);
        QCOMPARE(rig.epoch(), epoch);
        QCOMPARE(dotLifts.load(), 0);
        QVERIFY(!rig.conn.cwOutputSuppressed());
        rig.line()->setOnChange({});
        QVERIFY(accepted(rig.paddle(app, false, true)));
        QCOMPARE(int(rig.line()->p1Bits(true)), 0x02);
        QCOMPARE(rig.epoch(), epoch);

        // Both up clears both and ends the activity.
        QVERIFY(accepted(rig.paddle(app, false, false)));
        QVERIFY(rig.bitsUp());
        QVERIFY(!rig.cwx()->isRemotePaddleHeld());
        QVERIFY(!rig.cwx()->isSending());
        QTRY_COMPARE(rig.epoch(), 0u);
        QVERIFY(!rig.core.server->txWatchdog()->isWatching(mac.key.fingerprint()));
        QVERIFY(!rig.conn.cwOutputSuppressed());

        // The next press is a new activity.
        QVERIFY(accepted(rig.paddle(app, true, false)));
        QVERIFY(rig.epoch() > epoch);
        QVERIFY(accepted(rig.core.invoke(app, "cw.stop")));
        QVERIFY(rig.bitsUp());
    }

    void eachRequestOfOneIdActsOnce()
    {
        PaddleCore rig;
        Device mac(QStringLiteral("iPhone"), QStringLiteral("phone"));
        rig.core.pair(mac);
        LoopbackTransport* app = rig.core.signIn(mac, peerDeclaring(3));
        QVERIFY(admitted(app));
        QVERIFY(accepted(rig.core.invoke(app, "tx.take")));
        const QList<MirrorUpdate> press{MirrorUpdate{0, "dot", MirrorWireKind::Bool, QVariant(true)},
                                        MirrorUpdate{1, "dash", MirrorWireKind::Bool, QVariant(false)}};
        const auto wire = SessionMessages::encode(SessionMessages::commandInvoke("cw.paddle", 4242, press));
        app->sendText(wire);
        QTRY_VERIFY(rig.line()->dot());
        const quint32 epoch = rig.epoch();
        rig.time.advanceTo(2000);
        // Later copies of that id get its answer: no renewal, no new epoch.
        app->sendText(wire);
        app->sendText(wire);
        QCoreApplication::processEvents();
        QCOMPARE(rig.epoch(), epoch);
        rig.time.advanceTo(CwxSender::kKeyWatchdogMs);
        QTRY_VERIFY(!rig.line()->dot());
    }

    // ---- Safety: the deadline ----

    void theDeadlineFallsAfterTheLastPressAndEachPressRenewsIt()
    {
        PaddleCore rig;
        Device mac(QStringLiteral("iPhone"), QStringLiteral("phone"));
        rig.core.pair(mac);
        LoopbackTransport* app = rig.core.signIn(mac, peerDeclaring(3));
        QVERIFY(admitted(app));
        QVERIFY(accepted(rig.core.invoke(app, "tx.take")));
        QVERIFY(accepted(rig.paddle(app, true, false)));
        rig.time.advanceTo(2000);
        QVERIFY(accepted(rig.paddle(app, true, true))); // a press at 2000
        rig.time.advanceTo(4000);
        QVERIFY(accepted(rig.paddle(app, false, true))); // only lets go: no press
        rig.time.advanceTo(4999);
        QTest::qWait(20); // Time for a wrong release to show; the worker runs free.
        QVERIFY(rig.line()->dash());
        rig.time.advanceTo(2000 + CwxSender::kKeyWatchdogMs);
        QTRY_VERIFY(!rig.line()->dash());
        QVERIFY(rig.bitsUp());
        QVERIFY(!rig.cwx()->isRemotePaddleHeld());
        QTRY_COMPARE(rig.epoch(), 0u);
    }

    // ---- Safety: the remote CW hold ----

    void aLinkHoldLiftsTheBitsAndRecoveryRestoresThemInsideTheDeadline()
    {
        PaddleCore rig;
        Device mac(QStringLiteral("iPhone"), QStringLiteral("phone"));
        rig.core.pair(mac);
        LoopbackTransport* app = rig.core.signIn(mac, peerDeclaring(3));
        QVERIFY(admitted(app));
        QVERIFY(accepted(rig.core.invoke(app, "tx.take")));
        auto* guard = rig.core.server->txCutGuard();
        QVERIFY(accepted(rig.paddle(app, true, true)));
        const quint32 epoch = rig.epoch();
        QVERIFY(!rig.conn.cwOutputSuppressed());

        // 400 ms with no keepalive: the wire is masked at once, the sender follows.
        guard->advanceClockForTest(std::chrono::milliseconds(401));
        QTRY_VERIFY(rig.conn.cwOutputSuppressed());
        rig.time.wake();
        QTRY_VERIFY(!rig.line()->dot() && !rig.line()->dash());
        QVERIFY(rig.cwx()->isRemotePaddleHeld()); // The intent stays for recovery.
        QCOMPARE(rig.epoch(), epoch);

        // A press during the hold stays off the wire.
        QVERIFY(accepted(rig.paddle(app, true, false)));
        QVERIFY(!rig.line()->dot());
        QVERIFY(accepted(rig.paddle(app, true, true)));

        QVERIFY(rig.core.model->resumeRemoteCw(epoch));
        QVERIFY(!rig.conn.cwOutputSuppressed());
        QVERIFY(rig.line()->dot());
        QVERIFY(rig.line()->dash());
        QVERIFY(accepted(rig.paddle(app, false, false)));
        QVERIFY(rig.bitsUp());
    }

    void theDeadlineKeepsRunningThroughAHold()
    {
        PaddleCore rig;
        Device mac(QStringLiteral("iPhone"), QStringLiteral("phone"));
        rig.core.pair(mac);
        LoopbackTransport* app = rig.core.signIn(mac, peerDeclaring(3));
        QVERIFY(admitted(app));
        QVERIFY(accepted(rig.core.invoke(app, "tx.take")));
        QVERIFY(accepted(rig.paddle(app, false, true)));
        const quint32 epoch = rig.epoch();
        QVERIFY(rig.core.model->pauseRemoteCw(epoch));
        rig.time.wake();
        QTRY_VERIFY(!rig.line()->dash());

        rig.time.advanceTo(CwxSender::kKeyWatchdogMs);
        QTRY_VERIFY(!rig.cwx()->isRemotePaddleHeld());
        QTRY_COMPARE(rig.epoch(), 0u);
        QVERIFY(!rig.core.model->resumeRemoteCw(epoch));
        QVERIFY(rig.bitsUp());
    }

    void aReleaseIsTakenDuringAHoldAndRecoveryPutsNothingDown()
    {
        PaddleCore rig;
        Device mac(QStringLiteral("iPhone"), QStringLiteral("phone"));
        rig.core.pair(mac);
        LoopbackTransport* app = rig.core.signIn(mac, peerDeclaring(3));
        QVERIFY(admitted(app));
        QVERIFY(accepted(rig.core.invoke(app, "tx.take")));
        QVERIFY(accepted(rig.paddle(app, true, false)));
        const quint32 epoch = rig.epoch();
        QVERIFY(rig.core.model->pauseRemoteCw(epoch));
        QVERIFY(accepted(rig.paddle(app, false, false)));
        QTRY_COMPARE(rig.epoch(), 0u);
        QVERIFY(!rig.core.model->resumeRemoteCw(epoch));
        QVERIFY(rig.bitsUp());
        QVERIFY(!rig.conn.cwOutputSuppressed());
    }

    // ---- Safety: a cut, a closed session, whatever replaces it ----

    void aCutClearsTheBitsAtOnce()
    {
        PaddleCore rig;
        Device mac(QStringLiteral("iPhone"), QStringLiteral("phone"));
        rig.core.pair(mac);
        LoopbackTransport* app = rig.core.signIn(mac, peerDeclaring(3));
        QVERIFY(admitted(app));
        QVERIFY(accepted(rig.core.invoke(app, "tx.take")));
        QVERIFY(accepted(rig.paddle(app, true, true)));
        const quint32 epoch = rig.epoch();
        rig.core.model->transmitCutLine()->cut(epoch);
        QVERIFY(rig.conn.cwOutputSuppressed());
        rig.core.model->cancelRemoteCw(epoch);
        QVERIFY(rig.bitsUp());
        QVERIFY(!rig.cwx()->isRemotePaddleHeld());
        rig.core.model->catchUpTransmitCut(QStringLiteral("test"));
    }

    void aClosingSessionClearsTheBitsAtOnce()
    {
        PaddleCore rig;
        Device mac(QStringLiteral("iPhone"), QStringLiteral("phone"));
        rig.core.pair(mac);
        LoopbackTransport* app = rig.core.signIn(mac, peerDeclaring(3));
        QVERIFY(admitted(app));
        QVERIFY(accepted(rig.core.invoke(app, "tx.take")));
        QVERIFY(accepted(rig.paddle(app, true, true)));
        const QPointer<LoopbackTransport> peer(app->peerForTest());
        QVERIFY(peer);
        app->closeLink(QStringLiteral("network lost"));
        QVERIFY(rig.line()->dot()); // The Core-side close is still queued.
        QCoreApplication::sendPostedEvents(peer.data(), QEvent::MetaCall);
        QVERIFY(rig.bitsUp()); // Directly, with the fake clock still at 0.
        QVERIFY(!rig.cwx()->isRemotePaddleHeld());
    }

    void anotherDevicesTakeEndsThePaddle()
    {
        PaddleCore rig;
        Device a(QStringLiteral("iPhone"), QStringLiteral("phone"));
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        rig.core.pair(a);
        rig.core.pair(b);
        LoopbackTransport* appA = rig.core.signIn(a, peerDeclaring(3));
        LoopbackTransport* appB = rig.core.signIn(b, peerDeclaring(3));
        QVERIFY(admitted(appA));
        QVERIFY(admitted(appB));
        QVERIFY(accepted(rig.core.invoke(appA, "tx.take")));
        QVERIFY(accepted(rig.paddle(appA, true, false)));
        QCOMPARE(reasonOf(rig.core.invoke(appB, "tx.take")), QStringLiteral("Waiting for you to confirm."));
        const QJsonObject question = firstOfType(appB->received(), QStringLiteral("confirm.request"));
        QVERIFY(!question.isEmpty());
        QVERIFY(accepted(rig.core.invoke(appB, "confirm.proceed",
            {int64("id", question.value(QStringLiteral("id")).toInteger()), int64("choice", -1)})));
        QTRY_VERIFY(rig.core.server->transmitHolder()->isHeldBy(b.key.fingerprint()));
        QTRY_VERIFY(rig.bitsUp());
        QVERIFY(!rig.cwx()->isRemotePaddleHeld());
    }

    void stopClearsTheBitsAndOtherCwReplacesThePaddle()
    {
        PaddleCore rig;
        Device mac(QStringLiteral("iPhone"), QStringLiteral("phone"));
        rig.core.pair(mac);
        LoopbackTransport* app = rig.core.signIn(mac, peerDeclaring(3));
        QVERIFY(admitted(app));
        QVERIFY(accepted(rig.core.invoke(app, "tx.take")));

        QVERIFY(accepted(rig.paddle(app, true, true)));
        QVERIFY(accepted(rig.core.invoke(app, "cw.stop")));
        QVERIFY(rig.bitsUp());
        QCOMPARE(rig.epoch(), 0u);

        // The Key: the paddle lets go, and the Key's release leaves it so.
        QVERIFY(accepted(rig.paddle(app, true, false)));
        QVERIFY(accepted(rig.core.invoke(app, "cw.key",
                                         {MirrorUpdate{0, "down", MirrorWireKind::Bool, QVariant(true)}})));
        QVERIFY(!rig.line()->dot());
        QVERIFY(rig.line()->cwx());
        QVERIFY(!rig.cwx()->isRemotePaddleHeld());
        // The paddle replaces the Key, whose CWX bit lets go.
        QVERIFY(accepted(rig.paddle(app, false, true)));
        QVERIFY(!rig.line()->cwx());
        QVERIFY(!rig.cwx()->isKeyHeld());
        QVERIFY(rig.line()->dash());
        // A release of the Key that no longer runs leaves the paddle down.
        QVERIFY(accepted(rig.core.invoke(app, "cw.key",
                                         {MirrorUpdate{0, "down", MirrorWireKind::Bool, QVariant(false)}})));
        QVERIFY(rig.line()->dash());

        // Text goes on in the same activity with the bits up.
        const quint32 epoch = rig.epoch();
        QVERIFY(accepted(rig.core.invoke(app, "cw.send", {utf8("text", QStringLiteral("CQ"))})));
        QVERIFY(!rig.line()->dash());
        QCOMPARE(rig.epoch(), epoch);
        QVERIFY(!rig.cwx()->isRemotePaddleHeld());
        // And the paddle replaces the text.
        QVERIFY(accepted(rig.paddle(app, true, false)));
        QVERIFY(rig.epoch() > epoch);
        QCOMPARE(rig.cwx()->queuedCharacters(), 0);
        QVERIFY(!rig.line()->cwx());
        QVERIFY(rig.line()->dot());
        QVERIFY(accepted(rig.paddle(app, false, false)));
        QVERIFY(rig.bitsUp());
    }

    void aRefusedChangeLetsGoOfTheKeys()
    {
        PaddleCore rig;
        Device mac(QStringLiteral("iPhone"), QStringLiteral("phone"));
        rig.core.pair(mac);
        LoopbackTransport* app = rig.core.signIn(mac, peerDeclaring(3));
        QVERIFY(admitted(app));
        QVERIFY(accepted(rig.core.invoke(app, "tx.take")));
        QVERIFY(accepted(rig.paddle(app, true, false)));
        rig.core.model->sliceById(0)->setDspMode(DSPMode::USB);
        QCoreApplication::processEvents();
        const QJsonObject refused = rig.paddle(app, true, true);
        QVERIFY(!accepted(refused));
        QVERIFY(rig.bitsUp());
        QVERIFY(!rig.cwx()->isRemotePaddleHeld());
    }

    // ---- First-look fixes: only a key that rises (items 3 and 8) ----

    void resendingTheSameStateDoesNotRenewTheDeadline()
    {
        PaddleCore rig;
        Device mac(QStringLiteral("iPhone"), QStringLiteral("phone"));
        rig.core.pair(mac);
        LoopbackTransport* app = rig.core.signIn(mac, peerDeclaring(3));
        QVERIFY(admitted(app));
        QVERIFY(accepted(rig.core.invoke(app, "tx.take")));
        QVERIFY(accepted(rig.paddle(app, true, false)));
        rig.time.advanceTo(2000);
        QVERIFY(accepted(rig.paddle(app, true, false))); // no press: a new id, the same state
        QVERIFY(rig.line()->dot());
        rig.time.advanceTo(CwxSender::kKeyWatchdogMs);
        QTRY_VERIFY(!rig.line()->dot());
        QVERIFY(rig.bitsUp());
    }

    void liftingOneKeyAfterTheDeadlineKeysNothing()
    {
        PaddleCore rig;
        Device mac(QStringLiteral("iPhone"), QStringLiteral("phone"));
        rig.core.pair(mac);
        LoopbackTransport* app = rig.core.signIn(mac, peerDeclaring(3));
        QVERIFY(admitted(app));
        QVERIFY(accepted(rig.core.invoke(app, "tx.take")));
        QVERIFY(accepted(rig.paddle(app, true, true)));
        rig.time.advanceTo(CwxSender::kKeyWatchdogMs);
        QTRY_VERIFY(rig.bitsUp());
        QTRY_COMPARE(rig.epoch(), 0u);

        const QJsonObject lifted = rig.paddle(app, true, false);
        QVERIFY2(accepted(lifted), qPrintable(reasonOf(lifted)));
        QVERIFY(rig.bitsUp());
        QVERIFY(!rig.cwx()->isRemotePaddleHeld());
        QCOMPARE(rig.epoch(), 0u);

        // The first press after a real both-up keys.
        QVERIFY(accepted(rig.paddle(app, false, false)));
        QVERIFY(rig.bitsUp());
        QVERIFY(accepted(rig.paddle(app, true, false)));
        QVERIFY(rig.line()->dot());
        QVERIFY(accepted(rig.paddle(app, false, false)));
    }

    void liftingOneKeyAfterAStopKeysNothing()
    {
        PaddleCore rig;
        Device mac(QStringLiteral("iPhone"), QStringLiteral("phone"));
        rig.core.pair(mac);
        LoopbackTransport* app = rig.core.signIn(mac, peerDeclaring(3));
        QVERIFY(admitted(app));
        QVERIFY(accepted(rig.core.invoke(app, "tx.take")));
        QVERIFY(accepted(rig.paddle(app, true, true)));
        QVERIFY(accepted(rig.core.invoke(app, "cw.stop")));
        QVERIFY(rig.bitsUp());
        QVERIFY(accepted(rig.paddle(app, false, true)));
        QVERIFY(rig.bitsUp());
        QVERIFY(!rig.cwx()->isRemotePaddleHeld());
    }

    void tappingTheOtherKeyDoesNotBringBackAKeyPastItsDeadline()
    {
        PaddleCore rig;
        Device mac(QStringLiteral("iPhone"), QStringLiteral("phone"));
        rig.core.pair(mac);
        LoopbackTransport* app = rig.core.signIn(mac, peerDeclaring(3));
        QVERIFY(admitted(app));
        QVERIFY(accepted(rig.core.invoke(app, "tx.take")));
        QVERIFY(accepted(rig.paddle(app, true, false)));
        rig.time.advanceTo(CwxSender::kKeyWatchdogMs);
        QTRY_VERIFY(rig.bitsUp());
        QTRY_COMPARE(rig.epoch(), 0u);

        // Dash rises; only dash goes down.
        QVERIFY(accepted(rig.paddle(app, true, true)));
        QVERIFY(rig.line()->dash());
        QVERIFY(!rig.line()->dot());
        // Dash lifts with dot still reported down: a release.
        QVERIFY(accepted(rig.paddle(app, true, false)));
        QVERIFY(rig.bitsUp());
        QVERIFY(!rig.cwx()->isRemotePaddleHeld());
    }

    void addingTheOtherKeyKeepsTheFirstDown()
    {
        PaddleCore rig;
        Device mac(QStringLiteral("iPhone"), QStringLiteral("phone"));
        rig.core.pair(mac);
        LoopbackTransport* app = rig.core.signIn(mac, peerDeclaring(3));
        QVERIFY(admitted(app));
        QVERIFY(accepted(rig.core.invoke(app, "tx.take")));
        QVERIFY(accepted(rig.paddle(app, true, false)));
        QVERIFY(accepted(rig.paddle(app, true, true)));
        QVERIFY(rig.line()->dot() && rig.line()->dash());
        // One key rises while the other lifts, in one message.
        QVERIFY(accepted(rig.paddle(app, false, true)));
        QVERIFY(accepted(rig.paddle(app, true, false)));
        QVERIFY(rig.line()->dot());
        QVERIFY(!rig.line()->dash());
        QVERIFY(accepted(rig.paddle(app, false, false)));
        QVERIFY(rig.bitsUp());
    }

    // The last pair is the session's: the next session's first press keys.
    void aNewSessionsFirstPressKeysWhateverTheLastSessionReported()
    {
        PaddleCore rig;
        Device mac(QStringLiteral("iPhone"), QStringLiteral("phone"));
        rig.core.pair(mac);
        LoopbackTransport* app = rig.core.signIn(mac, peerDeclaring(3));
        QVERIFY(admitted(app));
        QVERIFY(accepted(rig.core.invoke(app, "tx.take")));
        QVERIFY(accepted(rig.paddle(app, true, true)));
        QVERIFY(rig.line()->dot() && rig.line()->dash());
        app->closeLink(QStringLiteral("network lost"));
        QTRY_VERIFY(rig.bitsUp());

        LoopbackTransport* again = rig.core.signIn(mac, peerDeclaring(3));
        QVERIFY(admitted(again));
        QVERIFY(accepted(rig.core.invoke(again, "tx.take")));
        const QJsonObject press = rig.paddle(again, true, false);
        QVERIFY2(accepted(press), qPrintable(reasonOf(press)));
        QVERIFY(rig.line()->dot());
        QVERIFY(accepted(rig.paddle(again, false, false)));
    }

    // ---- First-look fixes, item 10: a key down, then text, from one device ----

    void textAfterTheKeyPlaysInTheKeysActivity()
    {
        PaddleCore rig;
        Device mac(QStringLiteral("iPhone"), QStringLiteral("phone"));
        rig.core.pair(mac);
        LoopbackTransport* app = rig.core.signIn(mac, peerDeclaring(3));
        QVERIFY(admitted(app));
        QVERIFY(accepted(rig.core.invoke(app, "tx.take")));
        QVERIFY(accepted(rig.core.invoke(app, "cw.key",
                                         {MirrorUpdate{0, "down", MirrorWireKind::Bool, QVariant(true)}})));
        QVERIFY(rig.line()->cwx());
        const quint32 epoch = rig.epoch();
        QVERIFY(epoch != 0);

        const QJsonObject sent = rig.core.invoke(app, "cw.send", {utf8("text", QStringLiteral("CQ"))});
        QVERIFY2(accepted(sent), qPrintable(reasonOf(sent)));
        // Queued work, whatever ends the key, runs before this is read.
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();
        QVERIFY(!rig.cwx()->isKeyHeld());
        QCOMPARE(rig.epoch(), epoch);
        QVERIFY(rig.cwx()->isSending());
        QVERIFY(rig.core.model->cwxActiveFor(mac.key.fingerprint()));
        QCOMPARE(rig.cwx()->acceptedRemoteEpoch(), epoch);
        QVERIFY(rig.cwx()->queuedCharacters() > 0);

        // Letting go of the key does not end it.
        QVERIFY(accepted(rig.core.invoke(app, "cw.key",
                                         {MirrorUpdate{0, "down", MirrorWireKind::Bool, QVariant(false)}})));
        QCoreApplication::processEvents();
        QCOMPARE(rig.epoch(), epoch);
        QVERIFY(rig.cwx()->isSending());
        QVERIFY(accepted(rig.core.invoke(app, "cw.stop")));
        QVERIFY(rig.bitsUp());
    }

    // It is still the holder's rule: a key another device's take ended lets no
    // text through.
    void textAfterTheKeyStillNeedsTheHolder()
    {
        PaddleCore rig;
        Device mac(QStringLiteral("iPhone"), QStringLiteral("phone"));
        rig.core.pair(mac);
        LoopbackTransport* app = rig.core.signIn(mac, peerDeclaring(3));
        QVERIFY(admitted(app));
        QVERIFY(accepted(rig.core.invoke(app, "tx.take")));
        QVERIFY(accepted(rig.core.invoke(app, "cw.key",
                                         {MirrorUpdate{0, "down", MirrorWireKind::Bool, QVariant(true)}})));
        QVERIFY(rig.line()->cwx());
        rig.core.model->sliceById(0)->setDspMode(DSPMode::USB);
        QCoreApplication::processEvents();
        const QJsonObject sent = rig.core.invoke(app, "cw.send", {utf8("text", QStringLiteral("CQ"))});
        QVERIFY(!accepted(sent));
        QCOMPARE(rig.cwx()->queuedCharacters(), 0);
        QVERIFY(accepted(rig.core.invoke(app, "cw.stop")));
        QVERIFY(rig.bitsUp());
    }

    // ---- First-look fixes, item 7: a release ends only what its key started ----

    void aReleaseLeavesAMessageThatReplacedTheKey()
    {
        QList<QByteArray> sent;
        quint32 nextId = 1;
        RemoteTransmitClient client([&](const QByteArray& verb, const QList<MirrorUpdate>&) {
            sent.append(verb);
            return nextId++;
        });
        client.setAvailable(true);
        client.setCwAvailable(true);
        client.setCwReleaseAvailable(true);
        client.setCwPaddleAvailable(true);
        const auto answer = [&client](quint32 id, const QByteArray& verb, qint64 epoch) {
            client.commandFinished(id, verb, true, {},
                                   {{0, "cwEpoch", MirrorWireKind::Int64, epoch},
                                    {1, "cwActive", MirrorWireKind::Bool, true}});
        };

        // The straight key, then a memory answered before the release.
        const quint32 key = client.setCwKey(true);
        answer(key, "cw.key", 11);
        QCOMPARE(client.remoteCwEpoch(), 11u);
        const quint32 memory = client.sendCwMemory(1);
        answer(memory, "cw.memory", 12);
        const quint32 up = client.setCwKey(false);
        QVERIFY(up != 0);
        QCOMPARE(sent.last(), QByteArray("cw.key"));
        QCOMPARE(client.remoteCwEpoch(), 12u);
        QVERIFY(client.keepaliveRunning());
        client.commandFinished(up, "cw.key", true, {}, {});
        QCOMPARE(client.remoteCwEpoch(), 12u);
        QVERIFY(client.keepaliveRunning());
        client.setRemoteCwActivity(12, false);
        QVERIFY(!client.keepaliveRunning());

        // The paddle, then a memory still unanswered at the release.
        const quint32 paddle = client.setCwPaddle(true, false);
        answer(paddle, "cw.paddle", 13);
        const quint32 pending = client.sendCwMemory(2);
        const quint32 lifted = client.setCwPaddle(false, false);
        QVERIFY(lifted != 0);
        QVERIFY(client.keepaliveRunning());
        // The release's answer, first or last, leaves the memory's coming.
        client.commandFinished(lifted, "cw.paddle", true, {}, {});
        QVERIFY(client.keepaliveRunning());
        answer(pending, "cw.memory", 14);
        QCOMPARE(client.remoteCwEpoch(), 14u);
        QVERIFY(client.keepaliveRunning());
        client.setRemoteCwActivity(14, false);
        QVERIFY(!client.keepaliveRunning());

        // Both keys, a memory, then one key lifts and then the other.
        answer(client.setCwPaddle(true, true), "cw.paddle", 15);
        answer(client.sendCwMemory(3), "cw.memory", 16);
        const quint32 one = client.setCwPaddle(false, true);
        QVERIFY(one != 0);
        client.commandFinished(one, "cw.paddle", true, {}, {});
        QCOMPARE(client.remoteCwEpoch(), 16u);
        QVERIFY(client.keepaliveRunning());
        QVERIFY(client.setCwPaddle(false, false) != 0);
        QCOMPARE(client.remoteCwEpoch(), 16u);
        QVERIFY(client.keepaliveRunning());
        client.setRemoteCwActivity(16, false);

        // The paddle takes over from the key: the key's release leaves it.
        answer(client.setCwKey(true), "cw.key", 17);
        answer(client.setCwPaddle(true, false), "cw.paddle", 18);
        QVERIFY(client.setCwKey(false) != 0);
        QCOMPARE(client.remoteCwEpoch(), 18u);
        QVERIFY(client.keepaliveRunning());

        // A release of what the key did start still ends it.
        QVERIFY(client.setCwPaddle(false, false) != 0);
        QCOMPARE(client.remoteCwEpoch(), 0u);
        QVERIFY(!client.keepaliveRunning());
        answer(client.setCwKey(true), "cw.key", 19);
        QVERIFY(client.setCwKey(false) != 0);
        QCOMPARE(client.remoteCwEpoch(), 0u);
        QVERIFY(!client.keepaliveRunning());
    }

    // ---- The client ----

    void theClientSendsBothKeysWithKeepalivesLikeTheKey()
    {
        struct Sent { QByteArray verb; QList<MirrorUpdate> arguments; };
        QList<Sent> sent;
        quint32 nextId = 1;
        RemoteTransmitClient client([&](const QByteArray& verb, const QList<MirrorUpdate>& arguments) {
            sent.append({verb, arguments});
            return nextId++;
        });
        client.setAvailable(true);
        client.setCwAvailable(true);

        // Version 2: nothing goes, down or up.
        QVERIFY(!client.cwPaddleAvailable());
        QCOMPARE(client.setCwPaddle(true, false), 0u);
        QCOMPARE(client.setCwPaddle(false, false), 0u);
        QVERIFY(sent.isEmpty());
        QVERIFY(!client.keepaliveRunning());

        client.setCwPaddleAvailable(true);
        QVERIFY(client.cwPaddleAvailable());
        std::vector<TxKeepalive> session;
        client.setSessionCwKeepalive([&](const TxKeepalive& value) { session.push_back(value); return true; });

        const quint32 id = client.setCwPaddle(true, false);
        QVERIFY(id != 0);
        QVERIFY(client.keepaliveRunning());
        QVERIFY(!session.empty());
        QCOMPARE(sent.last().verb, QByteArray("cw.paddle"));
        QCOMPARE(sent.last().arguments.size(), 2);
        QCOMPARE(sent.last().arguments.at(0).name, QByteArray("dot"));
        QCOMPARE(sent.last().arguments.at(0).value.toBool(), true);
        QCOMPARE(sent.last().arguments.at(1).name, QByteArray("dash"));
        QCOMPARE(sent.last().arguments.at(1).value.toBool(), false);

        client.commandFinished(id, "cw.paddle", true, {},
            {{0, "cwEpoch", MirrorWireKind::Int64, qint64{11}},
             {1, "cwActive", MirrorWireKind::Bool, true}});
        QCOMPARE(client.remoteCwEpoch(), 11u);
        client.keepaliveTick();
        QCOMPARE(session.back().cwEpoch, 11u);

        // A change keeps the activity's keepalives going.
        const quint32 change = client.setCwPaddle(true, true);
        QVERIFY(change != 0);
        client.commandFinished(change, "cw.paddle", true, {},
            {{0, "cwEpoch", MirrorWireKind::Int64, qint64{11}},
             {1, "cwActive", MirrorWireKind::Bool, true}});
        QCOMPARE(client.remoteCwEpoch(), 11u);
        QVERIFY(client.keepaliveRunning());

        // Both up goes out and stops them, and a late answer cannot reopen them.
        const quint32 late = client.setCwPaddle(true, false);
        QVERIFY(client.setCwPaddle(false, false) != 0);
        QVERIFY(!client.keepaliveRunning());
        QCOMPARE(sent.last().arguments.at(0).value.toBool(), false);
        QCOMPARE(sent.last().arguments.at(1).value.toBool(), false);
        client.commandFinished(late, "cw.paddle", true, {},
            {{0, "cwEpoch", MirrorWireKind::Int64, qint64{11}},
             {1, "cwActive", MirrorWireKind::Bool, true}});
        QCOMPARE(client.remoteCwEpoch(), 0u);
        QVERIFY(!client.keepaliveRunning());

        // A refusal is cwRefused, and stops them.
        QSignalSpy refused(&client, &RemoteTransmitClient::cwRefused);
        const quint32 no = client.setCwPaddle(false, true);
        client.commandFinished(no, "cw.paddle", false, QStringLiteral("No."), {});
        QCOMPARE(refused.size(), 1);
        QVERIFY(!client.keepaliveRunning());

        // A session ending takes it all down.
        client.setCwPaddle(true, false);
        QVERIFY(client.keepaliveRunning());
        client.setAvailable(false);
        QVERIFY(!client.keepaliveRunning());
        QVERIFY(!client.cwPaddleAvailable());
    }
};

QTEST_MAIN(TstRemoteCwPaddle)
#include "tst_remote_cw_paddle.moc"
