// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_remote_cw.cpp  (NereusSDR)
// =================================================================
//
// CW transmit (R-CW-48, R-CW-49): the remote CW verbs on a test Core.
// cw.send, cw.memory, cw.stop and cw.key go to the Core's CWX sender for
// the device that holds transmit; remoteCwVersion 1 goes only to a peer
// that declared remoteCw 1, and the CW settings on `transmit` only to a
// peer that declared cwSettings 1. The Core's radio is a fake connection:
// nothing opens a radio socket and nothing keys a radio.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-27: F3: controlled watchdog expiry and direct queued-close
//               release evidence replace wall-clock proxies. J.J. Boyd
//               (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-26: CW transmit, remote CW verbs, capability and the CW
//               settings' peer gate (R-CW-48, R-CW-49). J.J. Boyd
//               (KG4VCF), AI-assisted via Claude Code.
// =================================================================

#include "MultiDeviceHarness.h"
#include "fakes/QuietRadioConnection.h"

#include "core/CwKeyLine.h"
#include "core/CwTypes.h"
#include "core/CwxSender.h"
#include "core/safety/TxRefusal.h"

#include <QEvent>
#include <QPointer>
#include <QThread>

#include <chrono>
#include <condition_variable>
#include <mutex>

namespace {

// Test-owned monotonic time. Returning from sleep on every generation change
// lets the sender re-read a shortened deadline as well as an advanced clock.
// The fixture shuts this down before RadioModel destroys/joins the sender.
class ControlledCwxTime {
public:
    using TimePoint = std::chrono::steady_clock::time_point;

    TimePoint now()
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        return timePoint(m_nowMs);
    }
    qint64 nowMs()
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        return m_nowMs;
    }
    void sleep(TimePoint until)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_shutdown || timePoint(m_nowMs) >= until) {
            return;
        }
        // Keep a wake that arrives between run()'s deadline snapshot and
        // entry here; otherwise a just-shortened deadline could be missed.
        if (m_wakePending) {
            m_wakePending = false;
            return;
        }
        m_waiting = true;
        m_waitingAtMs = m_nowMs;
        m_deadlineMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            until - timePoint(0)).count();
        const quint64 generation = m_generation;
        m_changed.wait(lock, [this, generation]() {
            return m_shutdown || m_generation != generation;
        });
        m_waiting = false;
        m_wakePending = false;
    }
    void advanceTo(qint64 ms)
    {
        {
            const std::lock_guard<std::mutex> lock(m_mutex);
            m_nowMs = ms;
            m_waiting = false;
            m_wakePending = true;
            ++m_generation;
        }
        m_changed.notify_all();
    }
    void wake()
    {
        {
            const std::lock_guard<std::mutex> lock(m_mutex);
            m_waiting = false;
            m_wakePending = true;
            ++m_generation;
        }
        m_changed.notify_all();
    }
    bool waitingAt(qint64 nowMs, qint64 deadlineMs)
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        return m_waiting && m_waitingAtMs == nowMs && m_deadlineMs == deadlineMs;
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
    qint64 m_waitingAtMs{-1};
    qint64 m_deadlineMs{-1};
    quint64 m_generation{0};
    bool m_waiting{false};
    bool m_wakePending{false};
    bool m_shutdown{false};
};

const QHash<QByteArray, int> kCwPeer{{"deviceAuth", 1}, {"sessionHolder", 1},
                                     {"remoteTx", 1}, {"remoteCw", 1}, {"cwSettings", 1}};

const QString kOlderApp = QStringLiteral("Update this app to send CW through this Core.");
const QString kCwSettingsWrite = QStringLiteral("Update this app to change the CW settings on this Core.");

// A test Core whose radio is the fake connection, its transmit slice on
// 20 m CWU with Semi break-in and CWX at 5 WPM, so a short text is still
// queued when the answer arrives.
struct CwCore {
    ControlledCwxTime time; // Outlives Core's model and sender.
    std::atomic<bool> lastKey{false};
    std::atomic<int> downs{0};
    std::atomic<int> ups{0};
    std::atomic<qint64> upAt{-1};
    std::atomic<bool> deliveringPeerClose{false};
    std::atomic<int> upsDuringPeerCloseOnMain{0};
    Core core;
    NereusSDR::Test::QuietRadioConnection conn;

    explicit CwCore(bool controlledTime = false)
        : core(true, controlledTime ? Core::ModelFactory([this]() {
                   return std::make_unique<RadioModel>(RadioModel::Role::Local,
                                                       time.makeSender(), nullptr);
               }) : Core::ModelFactory{})
    {
        core.model->injectConnectionForTest(&conn);
        core.model->wireCwKeyerConnectionForTest();
        allowTransmit(core);
        core.model->transmitModel().setCwBreakInMode(static_cast<int>(CwBreakInMode::Semi));
        core.model->transmitModel().setCwxSpeed(CwxSender::kMinWpm);
        if (SliceModel* slice = core.model->sliceById(0)) {
            slice->setDspMode(DSPMode::CWU);
            slice->setFrequency(14050000.0);
        }
        // QuietRadioConnection and its base install no key callback. Own
        // this observer before any writer starts; unlike keyHeldChanged,
        // CwKeyLine calls it directly on the thread that changed the bit.
        QThread* const mainThread = QCoreApplication::instance()->thread();
        conn.cwKeyLine()->setOnChange([this, mainThread]() {
            const bool key = conn.cwKeyLine()->cwx();
            if (lastKey.exchange(key) == key) {
                return; // HL2 PTT changes are not additional CWX key edges.
            }
            if (key) {
                downs.fetch_add(1);
            } else {
                upAt.store(time.nowMs());
                ups.fetch_add(1);
                if (deliveringPeerClose.load() && QThread::currentThread() == mainThread) {
                    upsDuringPeerCloseOnMain.fetch_add(1);
                }
            }
        });
        QCoreApplication::processEvents();
    }
    ~CwCore()
    {
        if (CwxSender* cwx = core.model->cwxSender()) {
            cwx->stop();
        }
        time.shutdown(); // Unblocks a fake sleeper even after QVERIFY fails.
        conn.cwKeyLine()->setOnChange({});
        core.server.reset();
        core.model->injectConnectionForTest(nullptr);
    }
    CwxSender* cwx() const { return core.model->cwxSender(); }
};

MirrorUpdate boolArg(const char* name, bool value)
{
    return MirrorUpdate{0, QByteArray(name), MirrorWireKind::Bool, QVariant(value)};
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

QStringList transmitSchemaFields(const LoopbackTransport* app)
{
    QStringList names;
    for (const QJsonObject& o : ofType(app->received(), QStringLiteral("schema"))) {
        if (o.value(QStringLiteral("class")).toString() != QStringLiteral("TransmitModel")) {
            continue;
        }
        for (const QJsonValue& f : o.value(QStringLiteral("fields")).toArray()) {
            names << f.toObject().value(QStringLiteral("name")).toString();
        }
    }
    return names;
}

QStringList capabilityNames(const LoopbackTransport* app)
{
    QStringList names;
    const QJsonObject caps = firstOfType(app->received(), QStringLiteral("capabilities"));
    for (const QJsonValue& p : caps.value(QStringLiteral("properties")).toArray()) {
        names << p.toObject().value(QStringLiteral("name")).toString();
    }
    return names;
}

} // namespace

class TstRemoteCw : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    // remoteCwVersion 1 right after txReadingsVersion, only for remoteCw 1.
    void capabilityOnlyForAPeerThatDeclaredRemoteCw()
    {
        CwCore rig;
        Device a(QStringLiteral("iPhone"), QStringLiteral("phone"));
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        rig.core.pair(a);
        rig.core.pair(b);
        LoopbackTransport* cw = rig.core.signIn(a, kCwPeer);
        LoopbackTransport* older = rig.core.signIn(b, kTransmitter);
        QVERIFY(admitted(cw));
        QVERIFY(admitted(older));
        QCOMPARE(capability(cw->received(), QStringLiteral("remoteCwVersion")), std::optional<qint64>(1));
        const QStringList names = capabilityNames(cw);
        QCOMPARE(names.indexOf(QStringLiteral("remoteCwVersion")),
                 names.indexOf(QStringLiteral("txReadingsVersion")) + 1);
        QVERIFY(!capability(older->received(), QStringLiteral("remoteCwVersion")).has_value());
    }

    // The holder's "CQ" is queued in the Core's sender, for that device.
    void holdersTextIsQueuedInTheCoresSender()
    {
        CwCore rig;
        Device a(QStringLiteral("iPhone"), QStringLiteral("phone"));
        rig.core.pair(a);
        LoopbackTransport* app = rig.core.signIn(a, kCwPeer);
        QVERIFY(admitted(app));
        QVERIFY(rig.core.invoke(app, "tx.take").value(QStringLiteral("accepted")).toBool());
        const QJsonObject result =
            rig.core.invoke(app, "cw.send", {utf8("text", QStringLiteral("CQ"))});
        QVERIFY2(result.value(QStringLiteral("accepted")).toBool(),
                 qPrintable(result.value(QStringLiteral("reason")).toString()));
        QVERIFY(rig.cwx()->isSending() || rig.cwx()->queuedCharacters() > 0);
        QCOMPARE(rig.core.model->cwxSendingFor(), a.key.fingerprint());
        // cw.stop ends it.
        QVERIFY(rig.core.invoke(app, "cw.stop").value(QStringLiteral("accepted")).toBool());
        QTRY_VERIFY(!rig.cwx()->isSending());
        QCOMPARE(rig.cwx()->queuedCharacters(), 0);
    }

    // A memory goes the same way; an empty one is refused as not ready.
    void memoriesSendAndAnEmptyOneIsRefused()
    {
        CwCore rig;
        Device a(QStringLiteral("iPhone"), QStringLiteral("phone"));
        rig.core.pair(a);
        rig.core.model->transmitModel().setCwxMemory1(QStringLiteral("CQ CQ"));
        rig.core.model->transmitModel().setCwxMemory2(QString());
        LoopbackTransport* app = rig.core.signIn(a, kCwPeer);
        QVERIFY(admitted(app));
        QVERIFY(rig.core.invoke(app, "tx.take").value(QStringLiteral("accepted")).toBool());
        const QJsonObject empty = rig.core.invoke(app, "cw.memory", {int64("number", 2)});
        QVERIFY(!empty.value(QStringLiteral("accepted")).toBool());
        QCOMPARE(empty.value(QStringLiteral("reason")).toString(), QStringLiteral("Memory 2 is empty."));
        QCOMPARE(valueOf(empty, QStringLiteral("refusalCode")), QStringLiteral("notReady"));
        const QJsonObject sent = rig.core.invoke(app, "cw.memory", {int64("number", 1)});
        QVERIFY2(sent.value(QStringLiteral("accepted")).toBool(),
                 qPrintable(sent.value(QStringLiteral("reason")).toString()));
        QVERIFY(rig.cwx()->isSending() || rig.cwx()->queuedCharacters() > 0);
    }

    // Another device holds transmit: refused with the holder's words.
    void anotherHolderIsNamed()
    {
        CwCore rig;
        Device a(QStringLiteral("iPhone"), QStringLiteral("phone"));
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        rig.core.pair(a);
        rig.core.pair(b);
        LoopbackTransport* appA = rig.core.signIn(a, kCwPeer);
        LoopbackTransport* appB = rig.core.signIn(b, kCwPeer);
        QVERIFY(admitted(appA));
        QVERIFY(admitted(appB));
        QVERIFY(rig.core.invoke(appA, "tx.take").value(QStringLiteral("accepted")).toBool());
        const TxRefusal expected = TxRefusals::otherDeviceHolds(QStringLiteral("iPhone"));
        for (const auto& [verb, args] : QList<QPair<QByteArray, QList<MirrorUpdate>>>{
                 {"cw.send", {utf8("text", QStringLiteral("CQ"))}},
                 {"cw.memory", {int64("number", 1)}},
                 {"cw.key", {boolArg("down", true)}}}) {
            const QJsonObject result = rig.core.invoke(appB, verb, args);
            QVERIFY2(!result.value(QStringLiteral("accepted")).toBool(), verb.constData());
            QCOMPARE(result.value(QStringLiteral("reason")).toString(), expected.text);
            QCOMPARE(valueOf(result, QStringLiteral("refusalCode")), QString::fromUtf8(expected.code));
            QCOMPARE(valueOf(result, QStringLiteral("refusalFix")),
                     QString::fromLatin1(TxRefusals::kFixTakeTransmit));
        }
        QVERIFY(!rig.cwx()->isSending());
        QVERIFY(!rig.cwx()->isKeyHeld());
        // A release never touches another device's CW: taken, nothing ends.
        QVERIFY(rig.core.invoke(appA, "cw.send", {utf8("text", QStringLiteral("CQ"))})
                    .value(QStringLiteral("accepted")).toBool());
        QVERIFY(rig.core.invoke(appB, "cw.stop").value(QStringLiteral("accepted")).toBool());
        QVERIFY(rig.cwx()->isSending() || rig.cwx()->queuedCharacters() > 0);
    }

    // Nobody holds transmit: the device is asked to take it first.
    void unheldTransmitAsksForATake()
    {
        CwCore rig;
        Device a(QStringLiteral("iPhone"), QStringLiteral("phone"));
        rig.core.pair(a);
        LoopbackTransport* app = rig.core.signIn(a, kCwPeer);
        QVERIFY(admitted(app));
        const QJsonObject result =
            rig.core.invoke(app, "cw.send", {utf8("text", QStringLiteral("CQ"))});
        QVERIFY(!result.value(QStringLiteral("accepted")).toBool());
        QCOMPARE(result.value(QStringLiteral("reason")).toString(), TxRefusals::notHolder().text);
        QCOMPARE(valueOf(result, QStringLiteral("refusalFix")),
                 QString::fromLatin1(TxRefusals::kFixTakeTransmit));
        QVERIFY(!rig.cwx()->isSending());
    }

    // The window sends each request three times under one id: every copy
    // gets the first answer and the sender acts once.
    void copiesAreAnsweredAlikeAndActOnce()
    {
        CwCore rig;
        Device a(QStringLiteral("iPhone"), QStringLiteral("phone"));
        rig.core.pair(a);
        LoopbackTransport* app = rig.core.signIn(a, kCwPeer);
        QVERIFY(admitted(app));
        QVERIFY(rig.core.invoke(app, "tx.take").value(QStringLiteral("accepted")).toBool());
        const quint32 id = 900;
        const QByteArray wire = SessionMessages::encode(SessionMessages::commandInvoke(
            "cw.send", id, {utf8("text", QStringLiteral("CQ"))}));
        for (int copy = 0; copy < 3; ++copy) {
            app->sendText(wire);
        }
        const auto answers = [app, id]() {
            QList<QJsonObject> found;
            for (const QJsonObject& o : ofType(app->received(), QStringLiteral("command.result"))) {
                if (o.value(QStringLiteral("id")).toInteger() == id) {
                    found << o;
                }
            }
            return found;
        };
        QTRY_COMPARE(answers().size(), 3);
        for (const QJsonObject& o : answers()) {
            QCOMPARE(o, answers().first());
            QVERIFY(o.value(QStringLiteral("accepted")).toBool());
        }
        // One "CQ" queued, not three.
        QVERIFY(rig.cwx()->queuedCharacters() <= 2);
        // The same id with other bytes is not read.
        app->sendText(SessionMessages::encode(SessionMessages::commandInvoke(
            "cw.send", id, {utf8("text", QStringLiteral("TEST"))})));
        QTRY_COMPARE(answers().size(), 4);
        QCOMPARE(answers().last().value(QStringLiteral("reason")).toString(),
                 QStringLiteral("The Core could not read this request."));
        QVERIFY(rig.cwx()->queuedCharacters() <= 2);
    }

    // cw.key down with no up: the Core's sender lets go after 3 s.
    void aHeldKeyIsReleasedAfterThreeSeconds()
    {
        CwCore rig(true);
        Device a(QStringLiteral("iPhone"), QStringLiteral("phone"));
        rig.core.pair(a);
        LoopbackTransport* app = rig.core.signIn(a, kCwPeer);
        QVERIFY(admitted(app));
        QVERIFY(rig.core.invoke(app, "tx.take").value(QStringLiteral("accepted")).toBool());
        const QJsonObject down = rig.core.invoke(app, "cw.key", {boolArg("down", true)});
        QVERIFY2(down.value(QStringLiteral("accepted")).toBool(),
                 qPrintable(down.value(QStringLiteral("reason")).toString()));
        QVERIFY(rig.cwx()->isKeyHeld());
        QTRY_VERIFY_WITH_TIMEOUT(rig.time.waitingAt(0, CwxSender::kKeyWatchdogMs), 2000);
        QCOMPARE(rig.downs.load(), 1);
        rig.time.advanceTo(CwxSender::kKeyWatchdogMs - 1);
        QTRY_VERIFY_WITH_TIMEOUT(
            rig.time.waitingAt(CwxSender::kKeyWatchdogMs - 1, CwxSender::kKeyWatchdogMs), 2000);
        QVERIFY(rig.cwx()->isKeyHeld());
        QVERIFY(rig.conn.cwKeyLine()->cwx());
        QCOMPARE(rig.ups.load(), 0);
        rig.time.advanceTo(CwxSender::kKeyWatchdogMs);
        QTRY_VERIFY_WITH_TIMEOUT(!rig.cwx()->isKeyHeld(), 2000);
        QTRY_VERIFY_WITH_TIMEOUT(!rig.cwx()->isSending(), 2000);
        QTRY_COMPARE_WITH_TIMEOUT(rig.ups.load(), 1, 2000);
        QCOMPARE(rig.upAt.load(), qint64(CwxSender::kKeyWatchdogMs));
        QVERIFY(!rig.conn.cwKeyLine()->cwx());
        QVERIFY(!rig.conn.cwKeyLine()->cwxPtt());
        QVERIFY(!rig.cwx()->isSending());
    }

    // cw.key up lets go at once.
    void keyUpReleases()
    {
        CwCore rig(true);
        Device a(QStringLiteral("iPhone"), QStringLiteral("phone"));
        rig.core.pair(a);
        LoopbackTransport* app = rig.core.signIn(a, kCwPeer);
        QVERIFY(admitted(app));
        QVERIFY(rig.core.invoke(app, "tx.take").value(QStringLiteral("accepted")).toBool());
        QVERIFY(rig.core.invoke(app, "cw.key", {boolArg("down", true)})
                    .value(QStringLiteral("accepted")).toBool());
        QVERIFY(rig.cwx()->isKeyHeld());
        QTRY_VERIFY_WITH_TIMEOUT(rig.time.waitingAt(0, CwxSender::kKeyWatchdogMs), 2000);
        QCOMPARE(rig.downs.load(), 1);
        QCOMPARE(rig.ups.load(), 0);
        QVERIFY(rig.core.invoke(app, "cw.key", {boolArg("down", false)})
                    .value(QStringLiteral("accepted")).toBool());
        QVERIFY(!rig.cwx()->isKeyHeld());
        QVERIFY(!rig.conn.cwKeyLine()->cwx());
        QVERIFY(!rig.conn.cwKeyLine()->cwxPtt());
        QVERIFY(!rig.cwx()->isSending());
        QCOMPARE(rig.ups.load(), 1);
        QCOMPARE(rig.upAt.load(), qint64(0));
    }

    // A key held when the link drops is released by the Core at once, not
    // at the watchdog.
    void aHeldKeyIsReleasedWhenTheLinkDrops()
    {
        CwCore rig;
        Device a(QStringLiteral("iPhone"), QStringLiteral("phone"));
        rig.core.pair(a);
        LoopbackTransport* app = rig.core.signIn(a, kCwPeer);
        QVERIFY(admitted(app));
        QVERIFY(rig.core.invoke(app, "tx.take").value(QStringLiteral("accepted")).toBool());
        QVERIFY(rig.core.invoke(app, "cw.key", {boolArg("down", true)})
                    .value(QStringLiteral("accepted")).toBool());
        QVERIFY(rig.cwx()->isKeyHeld());
        const QPointer<LoopbackTransport> peer(app->peerForTest());
        QVERIFY(peer);
        QVERIFY(app->isOpen());
        QVERIFY(peer->isOpen());
        QVERIFY(rig.conn.cwKeyLine()->cwx());
        QCOMPARE(rig.downs.load(), 1);
        QCOMPARE(rig.ups.load(), 0);
        app->closeLink(QStringLiteral("network lost"));
        QVERIFY(!app->isOpen());
        QVERIFY(peer->isOpen()); // The Core-side close is still queued.
        QVERIFY(rig.cwx()->isKeyHeld());
        QVERIFY(rig.conn.cwKeyLine()->cwx());
        rig.deliveringPeerClose.store(true);
        QCoreApplication::sendPostedEvents(peer.data(), QEvent::MetaCall);
        rig.deliveringPeerClose.store(false);
        QVERIFY(!rig.cwx()->isKeyHeld());
        QVERIFY(!rig.conn.cwKeyLine()->cwx());
        QVERIFY(!rig.conn.cwKeyLine()->cwxPtt());
        QVERIFY(!rig.cwx()->isSending());
        QCOMPARE(rig.ups.load(), 1);
        // A watchdog edge is written on CwxSender's worker and cannot
        // satisfy this direct, main-thread close-dispatch observation.
        QCOMPARE(rig.upsDuringPeerCloseOnMain.load(), 1);
    }

    // A transmit cut (Stop All TX) releases a held key and stops the text.
    void aTransmitCutReleasesTheKey()
    {
        CwCore rig(true);
        Device a(QStringLiteral("iPhone"), QStringLiteral("phone"));
        rig.core.pair(a);
        LoopbackTransport* app = rig.core.signIn(a, kCwPeer);
        QVERIFY(admitted(app));
        QVERIFY(rig.core.invoke(app, "tx.take").value(QStringLiteral("accepted")).toBool());
        QVERIFY(rig.core.invoke(app, "cw.key", {boolArg("down", true)})
                    .value(QStringLiteral("accepted")).toBool());
        QVERIFY(rig.cwx()->isKeyHeld());
        QTRY_VERIFY_WITH_TIMEOUT(rig.time.waitingAt(0, CwxSender::kKeyWatchdogMs), 2000);
        QCOMPARE(rig.downs.load(), 1);
        QCOMPARE(rig.ups.load(), 0);
        rig.core.model->stopAllTx(QStringLiteral("Transmit stopped."));
        QTRY_VERIFY_WITH_TIMEOUT(!rig.cwx()->isKeyHeld(), 1000);
        QVERIFY(!rig.conn.cwKeyLine()->cwx());
        QVERIFY(!rig.conn.cwKeyLine()->cwxPtt());
        QVERIFY(!rig.cwx()->isSending());
        QCOMPARE(rig.ups.load(), 1);
        QCOMPARE(rig.upAt.load(), qint64(0));
    }

    // Another device taking transmit ends the holder's CW at once.
    void anotherDevicesTakeEndsTheCw()
    {
        CwCore rig;
        Device a(QStringLiteral("iPhone"), QStringLiteral("phone"));
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        rig.core.pair(a);
        rig.core.pair(b);
        LoopbackTransport* appA = rig.core.signIn(a, kCwPeer);
        LoopbackTransport* appB = rig.core.signIn(b, kCwPeer);
        QVERIFY(admitted(appA));
        QVERIFY(admitted(appB));
        QVERIFY(rig.core.invoke(appA, "tx.take").value(QStringLiteral("accepted")).toBool());
        QVERIFY(rig.core.invoke(appA, "cw.send", {utf8("text", QStringLiteral("CQ CQ"))})
                    .value(QStringLiteral("accepted")).toBool());
        QVERIFY(rig.cwx()->isSending() || rig.cwx()->queuedCharacters() > 0);
        // Another device holds transmit, so the take asks B to confirm.
        QCOMPARE(rig.core.invoke(appB, "tx.take").value(QStringLiteral("reason")).toString(),
                 QStringLiteral("Waiting for you to confirm."));
        const QJsonObject question = firstOfType(appB->received(), QStringLiteral("confirm.request"));
        QVERIFY(!question.isEmpty());
        const QJsonObject took = rig.core.invoke(
            appB, "confirm.proceed",
            {int64("id", question.value(QStringLiteral("id")).toInteger()), int64("choice", -1)});
        QVERIFY2(took.value(QStringLiteral("accepted")).toBool(),
                 qPrintable(took.value(QStringLiteral("reason")).toString()));
        QTRY_VERIFY(rig.core.server->transmitHolder()->isHeldBy(b.key.fingerprint()));
        QTRY_VERIFY(!rig.cwx()->isSending());
        QCOMPARE(rig.cwx()->queuedCharacters(), 0);
    }

    // A peer that did not declare remoteCw is refused, and nothing is sent.
    void anOlderPeerIsRefused()
    {
        CwCore rig;
        Device a(QStringLiteral("iPhone"), QStringLiteral("phone"));
        rig.core.pair(a);
        LoopbackTransport* app = rig.core.signIn(a, kTransmitter);
        QVERIFY(admitted(app));
        QVERIFY(rig.core.invoke(app, "tx.take").value(QStringLiteral("accepted")).toBool());
        for (const auto& [verb, args] : QList<QPair<QByteArray, QList<MirrorUpdate>>>{
                 {"cw.send", {utf8("text", QStringLiteral("CQ"))}},
                 {"cw.memory", {int64("number", 1)}},
                 {"cw.stop", {}},
                 {"cw.key", {boolArg("down", true)}}}) {
            const QJsonObject result = rig.core.invoke(app, verb, args);
            QVERIFY2(!result.value(QStringLiteral("accepted")).toBool(), verb.constData());
            QCOMPARE(result.value(QStringLiteral("reason")).toString(), kOlderApp);
        }
        QVERIFY(!rig.cwx()->isSending());
        QVERIFY(!rig.cwx()->isKeyHeld());
    }

    // The CW settings go only to a peer that declared cwSettings 1; any
    // other peer's transmit schema keeps its fields, and its write of one
    // is refused.
    void cwSettingsOnlyForAPeerThatDeclaredThem()
    {
        CwCore rig;
        Device a(QStringLiteral("iPhone"), QStringLiteral("phone"));
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        rig.core.pair(a);
        rig.core.pair(b);
        LoopbackTransport* cw = rig.core.signIn(a, kCwPeer);
        LoopbackTransport* older = rig.core.signIn(b, kTransmitter);
        QVERIFY(admitted(cw));
        QVERIFY(admitted(older));
        const QStringList cwFields = transmitSchemaFields(cw);
        const QStringList olderFields = transmitSchemaFields(older);
        QVERIFY(cwFields.contains(QStringLiteral("cwKeyerSpeed")));
        QVERIFY(cwFields.contains(QStringLiteral("cwxMemory8")));
        // F2: why QSK is not offered, after the 30 settings.
        QCOMPARE(cwFields.indexOf(QStringLiteral("cwQskUnavailableReason")),
                 cwFields.indexOf(QStringLiteral("cwxMemory8")) + 1);
        for (const QString& name : olderFields) {
            QVERIFY2(!name.startsWith(QStringLiteral("cw")), qPrintable(name));
        }
        QCOMPARE(cwFields.size(), olderFields.size() + 31);

        older->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
            "transmit", {MirrorUpdate{0, "cwKeyerSpeed", MirrorWireKind::Int64, QVariant(qint64(30))}},
            71)));
        QTRY_VERIFY(!propertyResult(older, 71).isEmpty());
        const QJsonObject refused =
            propertyResult(older, 71).value(QStringLiteral("results")).toArray().first().toObject();
        QVERIFY(!refused.value(QStringLiteral("accepted")).toBool());
        QCOMPARE(refused.value(QStringLiteral("reason")).toString(), kCwSettingsWrite);

        cw->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
            "transmit", {MirrorUpdate{0, "cwKeyerSpeed", MirrorWireKind::Int64, QVariant(qint64(30))}},
            72)));
        QTRY_VERIFY(!propertyResult(cw, 72).isEmpty());
        const QJsonObject taken =
            propertyResult(cw, 72).value(QStringLiteral("results")).toArray().first().toObject();
        QVERIFY2(taken.value(QStringLiteral("accepted")).toBool(),
                 qPrintable(taken.value(QStringLiteral("reason")).toString()));
        QCOMPARE(rig.core.model->transmitModel().cwKeyerSpeed(), 30);
    }

    // Every new sentence is plain.
    void theWordsArePlain()
    {
        for (const QString& words : {kOlderApp, kCwSettingsWrite}) {
            QVERIFY2(OperatorWording::isPlain(words), qPrintable(words));
        }
    }
};

QTEST_MAIN(TstRemoteCw)
#include "tst_remote_cw.moc"
