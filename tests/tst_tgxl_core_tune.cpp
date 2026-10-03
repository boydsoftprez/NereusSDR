// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_tgxl_core_tune.cpp  (NereusSDR)
// =================================================================
//
// The Tuner Genius XL tune cycle on a Core (bench 2026-09-30): the tuner's
// own LAN PTT (`transmit tune on/off` on the SmartSDR API listener) and a
// device's tx.tunerTune, against a real Core over loopback links.
//
// The fake tuner does only what the bench log (Core 01d797e5,
// 2026-09-30 23:30) and captures/flex-tgxl-direct-CONTROL.pcapng show a
// real TGXL doing. On :9010 it sends its `V` banner, answers every command
// `R<seq>|0|`, and pushes `S0|state tuning=1/0` and `M|<text>` lines. On
// the Core's SmartSDR API listener it sends `C<seq>|transmit tune on/off`
// from its own address: on a front-panel press, as its answer to an
// `autotune`, and when it ends or gives up a tune. As the bench showed
// (2026-05-20, commit 01ca5b824 item 8) it also echoes the Core's
// `transmit ... tune=1` with its own tune on. How often is unmeasured, so
// the fake has both cadences (LanClient::Echo): PerFrame echoes every
// tune=1 frame (the default here), PerChange only each change; both echo
// a change to tune=0 with a tune off. A test that needs the echo late
// turns it Off and sends it itself. Every test drives those lines over
// real sockets: the fake tuner is at ::1, and any other client connects
// from 127.0.0.1.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-30: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), TGXL tune lane, with AI-assisted implementation
//               via Anthropic Claude Code.
//   2026-10-01: fix round: a socket fake tuner and LAN PTT lines through
//               the listener; the sender, our own autotune and tuning=1
//               cases; the untested ends. J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
//   2026-10-01: round 2: the fake tuner echoes the Core's tune state;
//               a late echo of a device's plain tune, the amplifier-wait
//               press, a v4-mapped tuner, and the answer window's
//               recoveries (rejection, a sweep, a reconnect, the window).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-01: round 3: echo per frame or per change; late echoes never
//               key on any holder; frames sent while :9010 is down or to a
//               reconnecting client are counted. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-01: round 4: a sub push and a new client during a tune are
//               counted, an idle push is not; the recall's answer keys as
//               a station key and never takes. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-01: tune-ended lane: each end of a device's cycle before its
//               carrier keyed tells that device once (notice tuneEnded,
//               plain words) and nobody else; its own stop and a keyed
//               cycle are not told. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-10-02: offline regressions for carrier readiness and a cancelled
//               cycle's settle callback. J.J. Boyd (KG4VCF), AI-assisted
//               via OpenAI Codex.
// =================================================================

#include "MultiDeviceHarness.h"

#include "core/PgxlConnection.h"
#include "core/DspControlThread.h"
#include "core/SmartSdrApiListener.h"
#include "core/TgxlAnswerTracker.h"
#include "core/TgxlConnection.h"
#include "core/TuneMemoryStore.h"
#include "core/TxChannel.h"
#include "core/WdspEngine.h"
#include "core/safety/TransmitHolder.h"
#include "core/safety/TxRefusal.h"
#include "models/SliceModel.h"

#include <QHostAddress>
#include <QElapsedTimer>
#include <QPointer>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>

#include <chrono>

using namespace NereusSDR;

namespace {

const MirrorUpdate kOn{0, "on", MirrorWireKind::Bool, QVariant(true)};
const MirrorUpdate kOff{0, "on", MirrorWireKind::Bool, QVariant(false)};

// The Power Genius has no socket in these tests (the Core's operate
// commands to it are counted, not delivered); its unopened-socket write
// warnings are not findings here. Same filter as tst_amp_changeover_gate.
QtMessageHandler g_previousHandler = nullptr;
void quietUnopenedSockets(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
    if (msg.startsWith(QLatin1String("QIODevice::write"))
        && msg.contains(QLatin1String("device not open"))) {
        return;
    }
    if (g_previousHandler != nullptr) {
        g_previousHandler(type, context, msg);
    }
}

// A SmartSDR-API client of the Core's listener, from a given address.
class LanClient {
public:
    LanClient()
    {
        QObject::connect(&m_sock, &QTcpSocket::readyRead, &m_sock, [this]() { readStatus(); });
    }
    bool connectTo(const QHostAddress& listener, quint16 port)
    {
        m_sock.connectToHost(listener, port);
        return m_sock.waitForConnected(2000);
    }
    /// The tune state the Core last broadcast to this client.
    bool tuneSeen() const { return m_tuneSeen; }
    /// Frames with tune=1 this client has been sent.
    int tuneFramesSeen() const { return m_tuneFrames; }
    /// How this client echoes the Core's tune state (see the file header).
    enum class Echo { Off, PerChange, PerFrame };
    Echo echo{Echo::Off};
    int send(const QString& command)
    {
        m_sock.write(QStringLiteral("C%1|%2\n").arg(++m_seq).arg(command).toUtf8());
        m_sock.flush();
        return m_seq;
    }
    QString response(int sequence) const { return m_responses.value(sequence); }
    bool greeted() const { return m_greeted; }
    void close() { m_sock.abort(); }

private:
    void readStatus()
    {
        while (m_sock.canReadLine()) {
            const QString line = QString::fromUtf8(m_sock.readLine());
            if (line.startsWith(QLatin1Char('H'))) {
                m_greeted = true;
                continue;
            }
            if (line.startsWith(QLatin1Char('R'))) {
                const int bar = line.indexOf(QLatin1Char('|'));
                if (bar > 1) {
                    m_responses.insert(line.mid(1, bar - 1).toInt(), line.trimmed());
                }
                continue;
            }
            const int at = line.indexOf(QLatin1String("|transmit "));
            if (at < 0) {
                continue;
            }
            const int key = line.indexOf(QLatin1String(" tune="), at);
            if (key < 0) {
                continue;
            }
            const bool tune = line.mid(key + 6, 1) == QLatin1String("1");
            const bool change = tune != m_tuneSeen;
            m_tuneSeen = tune;
            if (tune) {
                ++m_tuneFrames;
            }
            if (echo == Echo::Off) {
                continue;
            }
            if (tune && (change || echo == Echo::PerFrame)) {
                send(QStringLiteral("transmit tune on"));
            } else if (!tune && change) {
                send(QStringLiteral("transmit tune off"));
            }
        }
    }

    QTcpSocket m_sock;
    QHash<int, QString> m_responses;
    bool m_greeted{false};
    int m_seq{0};
    bool m_tuneSeen{false};
    int m_tuneFrames{0};
};

// The Tuner Genius: its :9010 control port, and its LAN PTT client.
class FakeTuner : public QObject {
    Q_OBJECT
public:
    FakeTuner()
    {
        lan.echo = LanClient::Echo::PerFrame;
        connect(&m_server, &QTcpServer::newConnection, this, [this]() {
            m_conn = m_server.nextPendingConnection();
            connect(m_conn, &QTcpSocket::readyRead, this, [this]() { readCommands(); });
            m_conn->write("V1.2.17\n");
            m_conn->flush();
        });
    }
    bool listen(const QHostAddress& at = QHostAddress::LocalHostIPv6)
    {
        return m_server.listen(at, 0);
    }
    /// Answer `autotune` R<seq>|1| (refused) instead of R<seq>|0|.
    bool rejectAutotune{false};
    quint16 port() const { return m_server.serverPort(); }
    void push(const QString& line)
    {
        if (!m_conn.isNull()) {
            m_conn->write((line + QLatin1Char('\n')).toUtf8());
            m_conn->flush();
        }
    }
    void dropLink()
    {
        if (!m_conn.isNull()) {
            m_conn->abort();
        }
        m_server.close();
    }
    int count(const QString& command) const { return m_commands.count(command); }
    LanClient lan;   // from ::1, the tuner's own address

signals:
    void commandReceived(const QString& command);

private:
    void readCommands()
    {
        while (!m_conn.isNull() && m_conn->canReadLine()) {
            const QString line = QString::fromUtf8(m_conn->readLine()).trimmed();
            const int bar = line.indexOf(QLatin1Char('|'));
            if (!line.startsWith(QLatin1Char('C')) || bar < 0) {
                continue;
            }
            const QString command = line.mid(bar + 1);
            m_commands << command;
            emit commandReceived(command);
            const bool refuse = rejectAutotune && command == QLatin1String("autotune");
            m_conn->write(QStringLiteral("R%1|%2|\n")
                              .arg(line.mid(1, bar - 1))
                              .arg(refuse ? 1 : 0)
                              .toUtf8());
            m_conn->flush();
        }
    }

    QTcpServer m_server;
    QPointer<QTcpSocket> m_conn;
    QStringList m_commands;
};

// Legacy socket cases also need a real carrier gate. No WDSP channel or
// radio is initialized: only the production RadioModel keying wiring.
struct TuningCore : Core {
    TxChannel tx{WdspEngine::kTxChannelId};

    TuningCore()
    {
        model->injectTxChannelForTest(&tx);
        model->wireTxChannelKeyingForTest();
    }
    ~TuningCore()
    {
        tx.closeRfGate();
        model->injectTxChannelForTest(nullptr);
    }
};

// The Core with its SmartSDR API listener up (any address, a free port)
// and the fake tuner connected on :9010 and to the listener from ::1.
// `tunerAt` is where the tuner listens and the Core dials it; `lanFrom` is
// the address its LAN PTT client dials the listener at.
bool startTuner(Core& core, FakeTuner& tuner,
                const QHostAddress& tunerAt = QHostAddress::LocalHostIPv6,
                const QString& dial = QStringLiteral("::1"),
                const QHostAddress& lanFrom = QHostAddress::LocalHostIPv6)
{
    SmartSdrApiListener* listener = core.model->smartSdrListener();
    if (!listener->start(QHostAddress::Any, 0) || !tuner.listen(tunerAt)) {
        return false;
    }
    core.model->tgxlConnection()->connectToTgxl(dial, tuner.port());
    if (!QTest::qWaitFor([&core]() { return core.model->tgxlConnection()->isConnected(); },
                         3000)) {
        return false;
    }
    return tuner.lan.connectTo(lanFrom, listener->serverPort());
}

// The band-change recall armed for 40 m and 20 m; each call moves the TX
// slice to the other band, so the Core sends one more `autotune`.
void armRecall(Core& core)
{
    AppSettings::instance().setValue(QStringLiteral("TGXL_AutoTuneMemoryRecall"),
                                     QStringLiteral("True"));
    core.model->tuneMemoryStore()->store({1, Band::Band40m, 10, 20, 30, 1});
    core.model->tuneMemoryStore()->store({1, Band::Band20m, 10, 20, 30, 1});
}
void recallOnce(Core& core, FakeTuner& tuner)
{
    SliceModel* tx = core.model->txBoundSlice();
    QVERIFY(tx != nullptr);
    const int before = tuner.count(QStringLiteral("autotune"));
    tx->setFrequency(tx->frequency() < 10'000'000.0 ? 14'100'000.0 : 7'100'000.0);
    QTRY_COMPARE(tuner.count(QStringLiteral("autotune")), before + 1);
    QTest::qWait(100);   // its reply is read
}

// How many times `spy` (on tuneStateSent) saw `tune`.
int countEmits(const QSignalSpy& spy, bool tune)
{
    int n = 0;
    for (const QList<QVariant>& args : spy) {
        if (args.at(0).toBool() == tune) {
            ++n;
        }
    }
    return n;
}

// The tuner's tune on (and its tune off) take nothing: the holder keeps
// transmit and nothing keys.
void tuneOnTakesNothing(Core& core, FakeTuner& tuner, const QByteArray& holderId,
                        int waitMs = 500)
{
    tuner.lan.send(QStringLiteral("transmit tune on"));
    QTest::qWait(waitMs);
    QVERIFY(!core.model->moxController()->isMox());
    QVERIFY(!core.model->isTune());
    QVERIFY(!core.model->isTgxlAutotuneInProgress());
    QVERIFY(core.server->transmitHolder()->isHeldBy(holderId));
}

// The tuner's tune on is its own TUNE: it takes and keys; its tune off
// ends it.
void tuneOnTakesAndKeys(Core& core, FakeTuner& tuner)
{
    tuner.lan.send(QStringLiteral("transmit tune on"));
    QTRY_VERIFY(core.model->moxController()->isMox());
    QVERIFY(core.server->transmitHolder()->isHeldBy(QByteArray(KeyerIdentity::kStationDeviceId)));
    tuner.lan.send(QStringLiteral("transmit tune off"));
    QTRY_COMPARE(core.model->moxController()->state(), MoxState::Rx);
    QTRY_VERIFY(!core.model->isTune());
}

// Device A holds transmit, unkeyed, after its own app TUNE, which the
// tuner answered as the pcap shows (its tune on, its sweep, its tune off).
void deviceHoldsAfterItsTune(Core& core, FakeTuner& tuner, LoopbackTransport* appA)
{
    MoxController* mox = core.model->moxController();
    QVERIFY(core.invoke(appA, "tx.tunerTune", {kOn}).value(QStringLiteral("accepted")).toBool());
    QTRY_VERIFY(mox->isMox());
    QTRY_COMPARE_WITH_TIMEOUT(tuner.count(QStringLiteral("autotune")), 1, 3000);
    tuner.lan.send(QStringLiteral("transmit tune on"));
    tuner.push(QStringLiteral("S0|state tuning=1"));
    QTest::qWait(100);
    QVERIFY(mox->isMox());   // the answer is an echo of this cycle
    tuner.lan.send(QStringLiteral("transmit tune off"));
    tuner.push(QStringLiteral("S0|state tuning=0"));
    QTRY_COMPARE(mox->state(), MoxState::Rx);
    QTRY_VERIFY_WITH_TIMEOUT(!core.model->isTune(), 5000);
    QVERIFY(!core.model->isTgxlAutotuneInProgress());
    QTRY_VERIFY(!tuner.lan.tuneSeen());   // and its echo of tune=0 is sent
    QTest::qWait(100);
}

// Device A's plain TUN (no autotune), on and off, short; it then holds
// transmit, unkeyed.
void plainTuneBy(Core& core, FakeTuner& tuner, LoopbackTransport* app)
{
    MoxController* mox = core.model->moxController();
    QVERIFY(core.invoke(app, "tx.tune", {kOn}).value(QStringLiteral("accepted")).toBool());
    QTRY_VERIFY(core.model->isTune());
    QTRY_VERIFY(tuner.lan.tuneSeen());
    QVERIFY(core.invoke(app, "tx.tune", {kOff}).value(QStringLiteral("accepted")).toBool());
    QTRY_COMPARE(mox->state(), MoxState::Rx);
    QTRY_VERIFY(!core.model->isTune());
    QTRY_VERIFY(!tuner.lan.tuneSeen());
    QTest::qWait(100);
}

// Tune-ended lane: the tuneEnded notices `app` was sent.
QList<QJsonObject> tuneEndedNotices(const LoopbackTransport* app)
{
    QList<QJsonObject> found;
    for (const QJsonObject& n : ofType(app->received(), QStringLiteral("notice"))) {
        if (n.value(QStringLiteral("kind")).toString() == QStringLiteral("tuneEnded")) {
            found.append(n);
        }
    }
    return found;
}

// Tune-ended lane: the Power Genius reports operating, so a device's cycle
// waits for its standby with nothing keyed.
void ampOperating(Core& core)
{
    PgxlConnection* amp = core.model->pgxlConnection();
    amp->injectLineForTesting(QStringLiteral("V3.8.9"));
    amp->injectLineForTesting(QStringLiteral("R1|0|state=OPERATE"));
}

// Tune-ended lane: station VOX armed by `device`, which holds transmit (as
// tst_station_multi_session's armVoxAsDevice), so its key is the holder's.
void armVoxAsHolder(Core& core, LoopbackTransport* app, const Device& device, quint32 writeId)
{
    const QByteArray id = device.key.fingerprint();
    QVERIFY(core.server->transmitHolder()->isHeldBy(id));
    core.model->openRemoteMicLine(id);
    app->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
        "transmit", {MirrorUpdate{0, "voxEnabled", MirrorWireKind::Bool, QVariant(true)}},
        writeId)));
    QTRY_VERIFY(!propertyResult(app, writeId).isEmpty());
    const QJsonObject result =
        propertyResult(app, writeId).value(QStringLiteral("results")).toArray().first().toObject();
    QVERIFY2(result.value(QStringLiteral("accepted")).toBool(),
             qPrintable(result.value(QStringLiteral("reason")).toString()));
    core.model->setRemoteMicVoxArmed(id, true);
}

// Tune-ended lane: the ends of a device's cycle before its carrier keyed.
enum class UnkeyedEnd {
    TunerDisconnected,
    TunerLetGo,
    ReceiveOnly,
    OnAir,
    CarrierRefused,
    TransmitTaken,
    LinkLost,
    NoReasonGiven,
};

} // namespace

Q_DECLARE_METATYPE(UnkeyedEnd)

class TgxlCoreTuneTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { g_previousHandler = qInstallMessageHandler(quietUnopenedSockets); }
    void cleanupTestCase() { qInstallMessageHandler(g_previousHandler); }

    void deviceAutotuneWaitsForTxReadyAndRfGate_data()
    {
        QTest::addColumn<bool>("grantFirst");
        QTest::addColumn<bool>("loseInterlockListener");
        QTest::newRow("grant before txReady") << true << false;
        QTest::newRow("txReady before grant") << false << false;
        QTest::newRow("interlock fallback before txReady") << true << true;
    }

    // A TRANSMITTING interlock frame alone cannot prove the carrier can
    // flow. Exercise the production RF-gate wiring, not a second copy of
    // its two-signal condition, and record it at the real :9010 boundary.
    // The injected channel has no radio/WDSP lane: this observes the Core's
    // gate, not RF power or the tuner's coupler threshold.
    void deviceAutotuneWaitsForTxReadyAndRfGate()
    {
        QFETCH(bool, grantFirst);
        QFETCH(bool, loseInterlockListener);
        Core core;
        allowTransmit(core);
        MoxController* mox = core.model->moxController();
        mox->setTimerIntervals(loseInterlockListener ? 2200 : (grantFirst ? 800 : 0),
                              0, 0, 0, 0, 0);
        core.model->setTuneOffSettleMsForTest(0);
        TxChannel tx{WdspEngine::kTxChannelId};
        core.model->injectTxChannelForTest(&tx);
        core.model->wireTxChannelKeyingForTest();
        const auto detachTx = qScopeGuard([&]() {
            tx.closeRfGate();
            core.model->injectTxChannelForTest(nullptr);
        });
        Device a;
        core.pair(a);
        LoopbackTransport* appA = core.signIn(a, kTransmitter);
        QVERIFY(admitted(appA));
        FakeTuner tuner;
        tuner.lan.echo = LanClient::Echo::Off;
        QVERIFY(startTuner(core, tuner));
        SmartSdrApiListener* listener = core.model->smartSdrListener();
        tuner.lan.send(QStringLiteral("interlock create type=AMP name=TG serial=TEST valid_antennas=1"));
        QTRY_VERIFY(listener->hasInterlockedAmp());
        QSignalSpy ready(mox, &MoxController::txReady);
        QSignalSpy grants(listener, &SmartSdrApiListener::interlockGranted);
        QList<QPair<bool, bool>> atAutotune;
        QObject observation;
        connect(&tuner, &FakeTuner::commandReceived, &observation, [&](const QString& command) {
            if (command == QLatin1String("autotune")) {
                atAutotune.append({!ready.isEmpty(), tx.isRfGateOpen()});
            }
        });

        QVERIFY(core.invoke(appA, "tx.tunerTune", {kOn}).value(QStringLiteral("accepted")).toBool());
        QVERIFY(core.model->isTgxlAutotuneInProgress());
        if (grantFirst) {
            QVERIFY(ready.isEmpty());
        } else {
            QTRY_COMPARE(ready.count(), 1);
        }
        QVERIFY(!tx.isRfGateOpen());
        QCOMPARE(tuner.count(QStringLiteral("autotune")), 0);
        if (loseInterlockListener) {
            // Stop only the LAN listener, cancelling its lenient 500 ms
            // interlock grant. The :9010 tuner connection stays up, so the
            // orchestration's 1.5 s fallback is reached without txReady.
            listener->stop();
            QVERIFY(core.model->tgxlConnection()->isConnected());
            QTimer deadline;
            deadline.setSingleShot(true);
            deadline.setTimerType(Qt::PreciseTimer);
            QSignalSpy elapsed(&deadline, &QTimer::timeout);
            deadline.start(1700);
            QTRY_VERIFY_WITH_TIMEOUT(!elapsed.isEmpty() || !atAutotune.isEmpty(), 2500);
            QCOMPARE(grants.count(), 0);
            QVERIFY(ready.isEmpty());
            QVERIFY(!tx.isRfGateOpen());
            QCOMPARE(tuner.count(QStringLiteral("autotune")), 0);
            QVERIFY(core.invoke(appA, "tx.tunerTune", {kOff}).value(QStringLiteral("accepted")).toBool());
            QTRY_COMPARE(mox->state(), MoxState::Rx);
            return;
        }
        tuner.lan.send(QStringLiteral("interlock ready 1"));
        QTRY_COMPARE(grants.count(), 1);
        if (grantFirst) {
            QVERIFY(ready.isEmpty());
            QVERIFY(!tx.isRfGateOpen());
        }
        QTRY_COMPARE_WITH_TIMEOUT(atAutotune.size(), 1, 3000);
        QVERIFY2(atAutotune.first().first, "autotune arrived before the real MoxController txReady");
        QVERIFY2(atAutotune.first().second, "autotune arrived while the production TX RF gate was closed");
        QCOMPARE(tuner.count(QStringLiteral("autotune")), 1);
        QVERIFY(core.invoke(appA, "tx.tunerTune", {kOff}).value(QStringLiteral("accepted")).toBool());
        QTRY_COMPARE(mox->state(), MoxState::Rx);
    }

    // Both prerequisites can arrive while the real control lane is still
    // stopped: setRunningAsync has only queued the gate-opening job. The
    // tune command must wait for that actual opening and its own settle.
    void deviceAutotuneSettlesAfterDelayedLaneRfGateOpening_data()
    {
        QTest::addColumn<bool>("ampHold");
        QTest::newRow("lane delayed") << false;
        QTest::newRow("amplifier hold then lane delayed") << true;
    }
    void deviceAutotuneSettlesAfterDelayedLaneRfGateOpening()
    {
        QFETCH(bool, ampHold);
        Core core;
        allowTransmit(core);
        core.model->setTuneOffSettleMsForTest(0);
        core.model->moxController()->setTimerIntervals(ampHold ? 300 : 0, 0, 0, 0, 0, 0);
        DspControlThread lane(DspLane::Transmit);
        TxChannel tx{WdspEngine::kTxChannelId};
        tx.setControlLane(&lane);
        core.model->injectTxChannelForTest(&tx);
        core.model->wireTxChannelKeyingForTest();
        const auto nowNs = []() -> qint64 {
            return std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
        };
        std::atomic<qint64> openedNs{-1};
        tx.setRfGateObserverForTest([&openedNs, nowNs](bool open) {
            if (open) {
                openedNs.store(nowNs(), std::memory_order_release);
            }
        });
        const auto detachTx = qScopeGuard([&]() {
            // Invalidate any queued on before draining, including on an
            // early QtTest assertion return. Keep the channel and observer
            // alive until the real lane has drained and joined.
            tx.closeRfGate();
            lane.start();
            lane.stop();
            tx.setRfGateObserverForTest({});
            tx.setControlLane(nullptr);
            core.model->injectTxChannelForTest(nullptr);
        });
        Device a;
        core.pair(a);
        LoopbackTransport* appA = core.signIn(a, kTransmitter);
        QVERIFY(admitted(appA));
        FakeTuner tuner;
        tuner.lan.echo = LanClient::Echo::Off;
        QVERIFY(startTuner(core, tuner));
        SmartSdrApiListener* listener = core.model->smartSdrListener();
        tuner.lan.send(QStringLiteral("interlock create type=AMP name=TG serial=TEST valid_antennas=1"));
        QTRY_VERIFY(listener->hasInterlockedAmp());
        QSignalSpy ready(core.model->moxController(), &MoxController::txReady);
        QSignalSpy grants(listener, &SmartSdrApiListener::interlockGranted);
        if (ampHold) {
            core.model->pgxlConnection()->injectLineForTesting(QStringLiteral("V3.8.9"));
            core.model->pgxlConnection()->injectLineForTesting(QStringLiteral("R1|0|state=STANDBY"));
        }
        qint64 commandNs = -1;
        qint64 openedAtCommandNs = -1;
        bool gateAtCommand = false;
        QObject observation;
        connect(&tuner, &FakeTuner::commandReceived, &observation, [&](const QString& command) {
            if (command == QLatin1String("autotune")) {
                commandNs = nowNs();
                openedAtCommandNs = openedNs.load(std::memory_order_acquire);
                gateAtCommand = tx.isRfGateOpen();
            }
        });
        QVERIFY(core.invoke(appA, "tx.tunerTune", {kOn}).value(QStringLiteral("accepted")).toBool());
        if (ampHold) {
            // A pre-existing changeover correctly refuses the tune. Start
            // the cycle first, then introduce the hold before txReady.
            QVERIFY(ready.isEmpty());
            core.model->pgxlConnection()->sendCommand(QStringLiteral("operate=0"));
            QVERIFY(core.model->ampChangingOver());
        }
        QTRY_COMPARE(ready.count(), 1);
        tuner.lan.send(QStringLiteral("interlock ready 1"));
        QTRY_COMPARE(grants.count(), 1);
        QVERIFY(!tx.isRfGateOpen());
        QCOMPARE(openedNs.load(std::memory_order_acquire), qint64(-1));

        QTimer deadline;
        deadline.setSingleShot(true);
        deadline.setTimerType(Qt::PreciseTimer);
        QSignalSpy elapsed(&deadline, &QTimer::timeout);
        deadline.start(200);
        QTRY_VERIFY_WITH_TIMEOUT(!elapsed.isEmpty() || tuner.count(QStringLiteral("autotune")) > 0, 1000);
        QCOMPARE(tuner.count(QStringLiteral("autotune")), 0);
        QVERIFY(!tx.isRfGateOpen());
        QCOMPARE(openedNs.load(std::memory_order_acquire), qint64(-1));

        lane.start();
        if (ampHold) {
            QVERIFY(lane.waitIdleForTest(1000));
            QVERIFY(!tx.isRfGateOpen());
            QCOMPARE(openedNs.load(std::memory_order_acquire), qint64(-1));
            QCOMPARE(tuner.count(QStringLiteral("autotune")), 0);
            core.model->pgxlConnection()->injectLineForTesting(QStringLiteral("S0|status state=STANDBY"));
        }
        QTRY_VERIFY(openedNs.load(std::memory_order_acquire) >= 0);
        QTRY_COMPARE_WITH_TIMEOUT(tuner.count(QStringLiteral("autotune")), 1, 2000);
        QVERIFY(gateAtCommand);
        QVERIFY(openedAtCommandNs >= 0);
        QVERIFY2(commandNs - openedAtCommandNs >= 150'000'000,
                 "autotune arrived less than 150 ms after the lane actually opened the RF gate");
        QVERIFY(core.invoke(appA, "tx.tunerTune", {kOff}).value(QStringLiteral("accepted")).toBool());
        QTRY_COMPARE(core.model->moxController()->state(), MoxState::Rx);
    }

    void cancelledCycleSettleCannotAutotuneNextCycle_data()
    {
        QTest::addColumn<bool>("waitForStandby");
        QTest::newRow("next cycle waits for PGXL standby") << true;
        QTest::newRow("next cycle waits for interlock") << false;
    }

    // Cycle A gets the interlock grant and arms the 150 ms settle. End A
    // and start B before that deadline; A's callback must not send B an
    // autotune while B has no carrier gate (standby or interlock pending).
    void cancelledCycleSettleCannotAutotuneNextCycle()
    {
        QFETCH(bool, waitForStandby);
        Core core;
        allowTransmit(core);
        core.model->setTuneOffSettleMsForTest(0);
        TxChannel tx{WdspEngine::kTxChannelId};
        core.model->injectTxChannelForTest(&tx);
        core.model->wireTxChannelKeyingForTest();
        const auto detachTx = qScopeGuard([&]() {
            tx.closeRfGate();
            core.model->injectTxChannelForTest(nullptr);
        });
        Device a;
        core.pair(a);
        LoopbackTransport* appA = core.signIn(a, kTransmitter);
        QVERIFY(admitted(appA));
        FakeTuner tuner;
        tuner.lan.echo = LanClient::Echo::Off;
        QVERIFY(startTuner(core, tuner));
        SmartSdrApiListener* listener = core.model->smartSdrListener();
        // Establish transport before A's short settle window. Register
        // only after A stops, so B has a genuinely unacknowledged amp.
        LanClient waitingAmp;
        if (!waitForStandby) {
            QVERIFY(waitingAmp.connectTo(QHostAddress::LocalHostIPv6, listener->serverPort()));
            QVERIFY(QTest::qWaitFor([&]() { return waitingAmp.greeted(); }, 1000));
        }
        tuner.lan.send(QStringLiteral("interlock create type=AMP name=TG serial=TEST valid_antennas=1"));
        QTRY_VERIFY(listener->hasInterlockedAmp());
        MoxController* mox = core.model->moxController();
        QSignalSpy ready(mox, &MoxController::txReady);
        QElapsedTimer settleAge;
        QObject observation;
        connect(listener, &SmartSdrApiListener::interlockGranted, &observation,
                [&settleAge](const QString&) { settleAge.start(); });
        QVERIFY(core.invoke(appA, "tx.tunerTune", {kOn}).value(QStringLiteral("accepted")).toBool());
        QVERIFY(QTest::qWaitFor([&ready]() { return ready.count() == 1; }, 1000));
        QVERIFY(!tx.isRfGateOpen());
        tuner.lan.send(QStringLiteral("interlock ready 1"));
        QVERIFY(QTest::qWaitFor([&settleAge]() { return settleAge.isValid(); }, 1000));
        QVERIFY(tx.isRfGateOpen());

        QVERIFY(core.invoke(appA, "tx.tunerTune", {kOff}).value(QStringLiteral("accepted")).toBool());
        QVERIFY(QTest::qWaitFor([mox]() { return mox->state() == MoxState::Rx; }, 1000));
        QVERIFY(!core.model->isTgxlAutotuneInProgress());
        QVERIFY(!tx.isRfGateOpen());
        if (waitForStandby) {
            ampOperating(core);
        } else {
            // The listener preserves A's recent pre-ACK for 500 ms. A
            // second real participant must withhold its ACK to hold B.
            const int registration = waitingAmp.send(QStringLiteral(
                "interlock create type=AMP name=WAIT serial=WAIT valid_antennas=1"));
            QVERIFY(QTest::qWaitFor([&]() { return !waitingAmp.response(registration).isEmpty(); }, 1000));
            QCOMPARE(waitingAmp.response(registration), QStringLiteral("R%1|0|2").arg(registration));
        }
        QVERIFY(core.invoke(appA, "tx.tunerTune", {kOn}).value(QStringLiteral("accepted")).toBool());
        QVERIFY(core.model->isTgxlAutotuneInProgress());
        QCOMPARE(core.model->tgxlAutotuneDeviceId(), a.key.fingerprint());
        QVERIFY(!tx.isRfGateOpen());
        if (waitForStandby) {
            QCOMPARE(mox->state(), MoxState::Rx);
            QCOMPARE(ready.count(), 1);
        } else {
            QVERIFY(QTest::qWaitFor([&ready]() { return ready.count() == 2; }, 1000));
        }
        QVERIFY2(settleAge.elapsed() < 150, "test setup missed cycle A's settle window");
        QCOMPARE(tuner.count(QStringLiteral("autotune")), 0);

        // This timer is an observation deadline for a negative assertion,
        // not a sleep while waiting for a state change. End the wait early
        // if the prohibited socket command arrives.
        QTimer deadline;
        deadline.setSingleShot(true);
        deadline.setTimerType(Qt::PreciseTimer);
        QSignalSpy elapsed(&deadline, &QTimer::timeout);
        deadline.start(static_cast<int>(200 - settleAge.elapsed()));
        QTRY_VERIFY_WITH_TIMEOUT(!elapsed.isEmpty() || tuner.count(QStringLiteral("autotune")) > 0, 1000);
        QCOMPARE(tuner.count(QStringLiteral("autotune")), 0);
        QVERIFY(!tx.isRfGateOpen());
        QVERIFY(core.invoke(appA, "tx.tunerTune", {kOff}).value(QStringLiteral("accepted")).toBool());
        QTRY_COMPARE(mox->state(), MoxState::Rx);
    }

    void noTxChannelRefusesDeviceAutotuneWithoutKeying()
    {
        Core core; // Explicit null-channel fixture; do not adapt this case.
        allowTransmit(core);
        Device a;
        core.pair(a);
        LoopbackTransport* appA = core.signIn(a, kTransmitter);
        QVERIFY(admitted(appA));
        FakeTuner tuner;
        QVERIFY(startTuner(core, tuner));
        const QJsonObject answer = core.invoke(appA, "tx.tunerTune", {kOn});
        QVERIFY(!answer.value(QStringLiteral("accepted")).toBool());
        QCOMPARE(answer.value(QStringLiteral("reason")).toString(),
                 QStringLiteral("The Core could not start the tune carrier."));
        QCOMPARE(core.model->moxController()->state(), MoxState::Rx);
        QVERIFY(!core.model->isTune());
        QVERIFY(!core.model->isTgxlAutotuneInProgress());
        QCOMPARE(tuner.count(QStringLiteral("autotune")), 0);
    }

    void pendingLaneCompletionCannotAutotuneAfterEnding_data()
    {
        QTest::addColumn<int>("ending");
        QTest::newRow("own stop") << 0;
        QTest::newRow("Stop All TX") << 1;
        QTest::newRow("receive only") << 2;
        QTest::newRow("transmit taken") << 3;
        QTest::newRow("tuner disconnected") << 4;
        QTest::newRow("device disconnected") << 5;
    }
    void pendingLaneCompletionCannotAutotuneAfterEnding()
    {
        QFETCH(int, ending);
        TuningCore core;
        allowTransmit(core);
        core.model->setTuneOffSettleMsForTest(0);
        DspControlThread lane(DspLane::Transmit);
        core.tx.setControlLane(&lane);
        const auto stopLane = qScopeGuard([&]() {
            core.tx.closeRfGate();
            lane.start();
            lane.stop();
            core.tx.setControlLane(nullptr);
        });
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        LoopbackTransport* appA = core.signIn(a, kTransmitter);
        LoopbackTransport* appB = core.signIn(b, kTransmitter);
        QVERIFY(admitted(appA));
        QVERIFY(admitted(appB));
        FakeTuner tuner;
        tuner.lan.echo = LanClient::Echo::Off;
        QVERIFY(startTuner(core, tuner));
        SmartSdrApiListener* listener = core.model->smartSdrListener();
        tuner.lan.send(QStringLiteral("interlock create type=AMP name=TG serial=TEST valid_antennas=1"));
        QTRY_VERIFY(listener->hasInterlockedAmp());
        QSignalSpy ready(core.model->moxController(), &MoxController::txReady);
        QSignalSpy grants(listener, &SmartSdrApiListener::interlockGranted);
        QVERIFY(core.invoke(appA, "tx.tunerTune", {kOn}).value(QStringLiteral("accepted")).toBool());
        QTRY_COMPARE(ready.count(), 1);
        tuner.lan.send(QStringLiteral("interlock ready 1"));
        QTRY_COMPARE(grants.count(), 1);
        QVERIFY(!core.tx.isRfGateOpen());
        QCOMPARE(tuner.count(QStringLiteral("autotune")), 0);

        switch (ending) {
        case 0:
            QVERIFY(core.invoke(appA, "tx.tunerTune", {kOff}).value(QStringLiteral("accepted")).toBool());
            break;
        case 1:
            core.model->stopAllTx(QStringLiteral("Test stop"));
            break;
        case 2:
            core.model->setReceiveOnlyStationPolicy(true);
            break;
        case 3: {
            const QJsonObject ask = core.invoke(appB, "tx.take", {});
            QCOMPARE(ask.value(QStringLiteral("reason")).toString(),
                     QStringLiteral("Waiting for you to confirm."));
            QTRY_VERIFY(!ofType(appB->received(), QStringLiteral("confirm.request")).isEmpty());
            const QJsonObject question = ofType(appB->received(), QStringLiteral("confirm.request")).last();
            const QJsonObject took = core.invoke(appB, "confirm.proceed",
                {int64("id", question.value(QStringLiteral("id")).toInteger()), int64("choice", -1)});
            QVERIFY(took.value(QStringLiteral("accepted")).toBool());
            break;
        }
        case 4:
            tuner.dropLink();
            QTRY_VERIFY(!core.model->tgxlConnection()->isConnected());
            break;
        case 5:
            appA->closeLink(QStringLiteral("Test disconnect"));
            QTRY_VERIFY(!appA->isOpen());
            break;
        }
        lane.start();
        QTRY_VERIFY_WITH_TIMEOUT(!core.model->isTgxlAutotuneInProgress(), 4000);
        QTRY_COMPARE(core.model->moxController()->state(), MoxState::Rx);
        QVERIFY(!core.tx.isRfGateOpen());
        if (ending == 3) {
            QVERIFY(core.server->transmitHolder()->isHeldBy(b.key.fingerprint()));
        }
        QTimer deadline;
        deadline.setSingleShot(true);
        deadline.setTimerType(Qt::PreciseTimer);
        QSignalSpy elapsed(&deadline, &QTimer::timeout);
        deadline.start(200);
        QTRY_VERIFY_WITH_TIMEOUT(!elapsed.isEmpty() || tuner.count(QStringLiteral("autotune")) > 0, 1000);
        QCOMPARE(tuner.count(QStringLiteral("autotune")), 0);
    }

    // A long but bounded ready wait must not consume the tuner's 3 s
    // sweep-start observation before the command was even submitted.
    void softwareSweepTimeoutStartsAtCommandSubmission()
    {
        TuningCore core;
        allowTransmit(core);
        core.model->setTuneOffSettleMsForTest(0);
        core.model->moxController()->setTimerIntervals(1700, 0, 0, 0, 0, 0);
        Device a;
        core.pair(a);
        LoopbackTransport* appA = core.signIn(a, kTransmitter);
        QVERIFY(admitted(appA));
        FakeTuner tuner;
        tuner.lan.echo = LanClient::Echo::Off;
        QVERIFY(startTuner(core, tuner));
        QVERIFY(core.invoke(appA, "tx.tunerTune", {kOn}).value(QStringLiteral("accepted")).toBool());
        QTRY_COMPARE_WITH_TIMEOUT(tuner.count(QStringLiteral("autotune")), 1, 3000);
        QTimer deadline;
        deadline.setSingleShot(true);
        deadline.setTimerType(Qt::PreciseTimer);
        QSignalSpy elapsed(&deadline, &QTimer::timeout);
        deadline.start(1500); // Past 3 s from key request, before 3 s from send.
        QTRY_VERIFY_WITH_TIMEOUT(!elapsed.isEmpty() || !core.model->isTgxlAutotuneInProgress(), 2000);
        QVERIFY(core.model->isTgxlAutotuneInProgress());
        QVERIFY(core.model->isTune());
        QVERIFY(core.tx.isRfGateOpen());
        QCOMPARE(tuner.count(QStringLiteral("autotune")), 1); // The 1.5 s retry cannot duplicate it.
        QVERIFY(core.invoke(appA, "tx.tunerTune", {kOff}).value(QStringLiteral("accepted")).toBool());
        QTRY_COMPARE(core.model->moxController()->state(), MoxState::Rx);
    }

    // An ending observer may immediately retry after unkeying the old
    // carrier. The old readiness deadline must not stop that new cycle.
    void readinessTimeoutCannotUnkeyAReentrantReplacementCycle()
    {
        TuningCore core;
        allowTransmit(core);
        core.model->setTuneOffSettleMsForTest(0);
        core.model->moxController()->setTimerIntervals(4000, 0, 0, 0, 0, 0);
        Device a;
        core.pair(a);
        LoopbackTransport* appA = core.signIn(a, kTransmitter);
        QVERIFY(admitted(appA));
        FakeTuner tuner;
        tuner.lan.echo = LanClient::Echo::Off;
        QVERIFY(startTuner(core, tuner));
        bool retried = false;
        bool replacementAccepted = false;
        bool returnedToRx = false;
        bool gateClosedAtEnding = false;
        QString endedReason;
        QObject observation;
        connect(core.model.get(), &RadioModel::tgxlAutotuneEnded, &observation,
                [&](const QByteArray&, const QString& reason) {
            if (retried) {
                return;
            }
            retried = true;
            endedReason = reason;
            gateClosedAtEnding = !core.tx.isRfGateOpen();
            core.model->setTune(false);
            // Complete the real zero-delay walk in this observer's nested
            // event loop, then retry before the old timeout returns.
            returnedToRx = QTest::qWaitFor([&]() {
                return core.model->moxController()->state() == MoxState::Rx;
            }, 1000);
            if (!returnedToRx) {
                return;
            }
            core.model->moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
            replacementAccepted = core.invoke(appA, "tx.tunerTune", {kOn})
                                      .value(QStringLiteral("accepted")).toBool();
        });
        QVERIFY(core.invoke(appA, "tx.tunerTune", {kOn}).value(QStringLiteral("accepted")).toBool());
        QTRY_VERIFY_WITH_TIMEOUT(retried, 4000);
        QCOMPARE(endedReason, RadioModel::tunerTuneEndedReason(RadioModel::TunerTuneEnd::CarrierNotStarted));
        QVERIFY(gateClosedAtEnding);
        QVERIFY2(returnedToRx, "The ending observer's bounded real unkey walk did not reach receive");
        QVERIFY(replacementAccepted);
        QTRY_COMPARE_WITH_TIMEOUT(tuner.count(QStringLiteral("autotune")), 1, 1000);
        QVERIFY(core.model->isTgxlAutotuneInProgress());
        QVERIFY(core.model->isTune());
        QVERIFY(core.tx.isRfGateOpen());
        QVERIFY(core.invoke(appA, "tx.tunerTune", {kOff}).value(QStringLiteral("accepted")).toBool());
        QTRY_COMPARE(core.model->moxController()->state(), MoxState::Rx);
    }

    // Bug 2 replay, with JJ's ruling (2026-09-30): a device holds transmit
    // after its own tune; the operator presses the tuner's front-panel
    // TUNE, which arrives as its `transmit tune on` (before its interlock
    // ready, 23:30:16.249). It takes transmit as the radio's own PTT does
    // (ruling 8.9) and keys the tune carrier once the take ends; the
    // device is told; the Core sends no autotune of its own.
    void hardwareTuneTakesFromTheHolderAndKeys()
    {
        TuningCore core;
        allowTransmit(core);
        Device a;
        core.pair(a);
        LoopbackTransport* appA = core.signIn(a, kTransmitter);
        QVERIFY(admitted(appA));
        FakeTuner tuner;
        QVERIFY(startTuner(core, tuner));
        deviceHoldsAfterItsTune(core, tuner, appA);
        MoxController* mox = core.model->moxController();
        TransmitHolder* holder = core.server->transmitHolder();
        QVERIFY(holder->isHeldBy(a.key.fingerprint()));
        appA->clearReceived();

        tuner.push(QStringLiteral("S0|state tuning=1"));
        tuner.lan.send(QStringLiteral("transmit tune on"));
        QTRY_VERIFY(core.model->isTgxlAutotuneInProgress());
        QTRY_VERIFY(core.model->isTune());
        QTRY_VERIFY(mox->isMox());
        QVERIFY(holder->isHeldBy(QByteArray(KeyerIdentity::kStationDeviceId)));
        QCOMPARE(holder->holder()->source, TransmitHolder::Source::RadioPtt);

        QTRY_VERIFY(!ofType(appA->received(), QStringLiteral("notice")).isEmpty());
        const QJsonObject notice = ofType(appA->received(), QStringLiteral("notice")).last();
        QCOMPARE(notice.value(QStringLiteral("kind")).toString(), QStringLiteral("transmitTaken"));
        QCOMPARE(notice.value(QStringLiteral("bySource")).toString(), QStringLiteral("radioPtt"));
        QCOMPARE(notice.value(QStringLiteral("byName")).toString(), QStringLiteral("Radio"));

        tuner.lan.send(QStringLiteral("transmit tune off"));
        tuner.push(QStringLiteral("S0|state tuning=0"));
        QTRY_COMPARE(mox->state(), MoxState::Rx);
        QTRY_VERIFY(!core.model->isTune());
        QVERIFY(!core.model->isTgxlAutotuneInProgress());
        QCOMPARE(tuner.count(QStringLiteral("autotune")), 1);   // only the device's
    }

    // C1: `transmit tune on` from any other SmartSDR-API client is not the
    // tuner's press: the station key is refused naming the holder (8.9a).
    void tuneOnFromAnotherClientDoesNotTake()
    {
        TuningCore core;
        allowTransmit(core);
        Device a;
        core.pair(a);
        LoopbackTransport* appA = core.signIn(a, kTransmitter);
        QVERIFY(admitted(appA));
        FakeTuner tuner;
        QVERIFY(startTuner(core, tuner));
        deviceHoldsAfterItsTune(core, tuner, appA);
        MoxController* mox = core.model->moxController();
        TransmitHolder* holder = core.server->transmitHolder();

        LanClient other;
        QVERIFY(other.connectTo(QHostAddress::LocalHost,
                                core.model->smartSdrListener()->serverPort()));
        other.send(QStringLiteral("transmit tune on"));
        QTest::qWait(500);
        QVERIFY(!mox->isMox());
        QVERIFY(!core.model->isTune());
        QVERIFY(!core.model->isTgxlAutotuneInProgress());
        QVERIFY(holder->isHeldBy(a.key.fingerprint()));
    }

    // I2a: the band-change recall sends `autotune`; the tuner's answering
    // `transmit tune on` is not a press and takes nothing.
    void autoRecallAnswerDoesNotTake()
    {
        TuningCore core;
        allowTransmit(core);
        Device a;
        core.pair(a);
        LoopbackTransport* appA = core.signIn(a, kTransmitter);
        QVERIFY(admitted(appA));
        FakeTuner tuner;
        QVERIFY(startTuner(core, tuner));
        deviceHoldsAfterItsTune(core, tuner, appA);
        MoxController* mox = core.model->moxController();
        TransmitHolder* holder = core.server->transmitHolder();

        AppSettings::instance().setValue(QStringLiteral("TGXL_AutoTuneMemoryRecall"),
                                         QStringLiteral("True"));
        core.model->tuneMemoryStore()->store({1, Band::Band40m, 10, 20, 30, 1});
        SliceModel* tx = core.model->txBoundSlice();
        QVERIFY(tx != nullptr);
        tx->setFrequency(7'100'000.0);
        QTRY_COMPARE(tuner.count(QStringLiteral("autotune")), 2);

        tuner.push(QStringLiteral("S0|state tuning=1"));
        tuner.lan.send(QStringLiteral("transmit tune on"));
        QTest::qWait(500);
        QVERIFY(!mox->isMox());
        QVERIFY(!core.model->isTune());
        QVERIFY(holder->isHeldBy(a.key.fingerprint()));
        tuner.lan.send(QStringLiteral("transmit tune off"));
        tuner.push(QStringLiteral("S0|state tuning=0"));

        // Answered: the next tune on is a press again.
        tuner.lan.send(QStringLiteral("transmit tune on"));
        QTRY_VERIFY(mox->isMox());
        QVERIFY(holder->isHeldBy(QByteArray(KeyerIdentity::kStationDeviceId)));
        tuner.lan.send(QStringLiteral("transmit tune off"));
        QTRY_COMPARE(mox->state(), MoxState::Rx);
    }

    // I2b: a device's tune turned off before the tuner answers its
    // autotune: the late `transmit tune on` takes nothing and keys nothing.
    void lateAnswerToADevicesTuneDoesNotTake()
    {
        TuningCore core;
        allowTransmit(core);
        Device a;
        core.pair(a);
        LoopbackTransport* appA = core.signIn(a, kTransmitter);
        QVERIFY(admitted(appA));
        FakeTuner tuner;
        QVERIFY(startTuner(core, tuner));
        MoxController* mox = core.model->moxController();
        TransmitHolder* holder = core.server->transmitHolder();
        QVERIFY(core.invoke(appA, "tx.tunerTune", {kOn}).value(QStringLiteral("accepted")).toBool());
        QTRY_COMPARE_WITH_TIMEOUT(tuner.count(QStringLiteral("autotune")), 1, 3000);
        QVERIFY(core.invoke(appA, "tx.tunerTune", {kOff}).value(QStringLiteral("accepted")).toBool());
        QTRY_COMPARE(mox->state(), MoxState::Rx);
        QTRY_VERIFY_WITH_TIMEOUT(!core.model->isTune(), 5000);
        QVERIFY(holder->isHeldBy(a.key.fingerprint()));

        tuner.push(QStringLiteral("S0|state tuning=1"));
        tuner.lan.send(QStringLiteral("transmit tune on"));
        QTest::qWait(500);
        QVERIFY(!mox->isMox());
        QVERIFY(!core.model->isTune());
        QVERIFY(!core.model->isTgxlAutotuneInProgress());
        QVERIFY(holder->isHeldBy(a.key.fingerprint()));
    }

    // I-A (round 2), round 3: a device's plain tune short enough that the
    // tuner's echoes of our tune=1 frames arrive after the carrier dropped.
    // Every frame is counted, so every late echo is dropped (it neither
    // keys nor takes); once they are all in, the tuner's next tune on is
    // its own TUNE again. Under PerChange the tuner echoes once; a press
    // inside the window uses up one of the other frames' entries and is
    // dropped (it keys nothing and takes nothing) until the window passes.
    void lateEchoesOfAPlainTuneDoNotTake_data()
    {
        QTest::addColumn<bool>("perFrame");
        QTest::newRow("tuner echoes every frame") << true;
        QTest::newRow("tuner echoes each change") << false;
    }
    void lateEchoesOfAPlainTuneDoNotTake()
    {
        QFETCH(bool, perFrame);
        TuningCore core;
        allowTransmit(core);
        Device a;
        core.pair(a);
        LoopbackTransport* appA = core.signIn(a, kTransmitter);
        QVERIFY(admitted(appA));
        FakeTuner tuner;
        QVERIFY(startTuner(core, tuner));
        tuner.lan.echo = LanClient::Echo::Off;
        plainTuneBy(core, tuner, appA);
        QCOMPARE(tuner.count(QStringLiteral("autotune")), 0);

        const int late = perFrame ? tuner.lan.tuneFramesSeen() : 1;
        for (int i = 0; i < late; ++i) {
            tuneOnTakesNothing(core, tuner, a.key.fingerprint(), 150);   // a late echo
        }
        tuner.lan.send(QStringLiteral("transmit tune off"));        // the echo of tune=0
        QTest::qWait(100);
        tuner.lan.echo = LanClient::Echo::PerFrame;
        if (!perFrame && tuner.lan.tuneFramesSeen() > 1) {
            // Frames the tuner did not echo: a press inside the window uses
            // up one of their entries and is dropped, failing closed.
            tuneOnTakesNothing(core, tuner, a.key.fingerprint());
            QTest::qWait(int(TgxlAnswerTracker::kAnswerWindowMs) + 200);
        }
        tuneOnTakesAndKeys(core, tuner);
    }

    // m-5: a counted echo never keys and never takes, whoever holds
    // transmit: nobody, the station, or a device.
    void aLateEchoNeverKeys_data()
    {
        QTest::addColumn<QString>("holder");
        QTest::newRow("unheld") << QStringLiteral("none");
        QTest::newRow("station holds") << QStringLiteral("station");
        QTest::newRow("device holds") << QStringLiteral("device");
    }
    void aLateEchoNeverKeys()
    {
        QFETCH(QString, holder);
        TuningCore core;
        allowTransmit(core);
        Device a;
        core.pair(a);
        LoopbackTransport* appA = core.signIn(a, kTransmitter);
        QVERIFY(admitted(appA));
        FakeTuner tuner;
        QVERIFY(startTuner(core, tuner));
        tuner.lan.echo = LanClient::Echo::Off;
        TransmitHolder* held = core.server->transmitHolder();
        MoxController* mox = core.model->moxController();
        if (holder == QLatin1String("device")) {
            plainTuneBy(core, tuner, appA);
        } else {
            core.model->setTune(true);
            QTRY_VERIFY(core.model->isTune());
            QTRY_VERIFY(tuner.lan.tuneSeen());
            core.model->setTune(false);
            QTRY_COMPARE(mox->state(), MoxState::Rx);
            QTRY_VERIFY(!core.model->isTune());
            QTRY_VERIFY(!tuner.lan.tuneSeen());
            if (holder == QLatin1String("none") && held->holder().has_value()) {
                held->release(held->holder()->deviceId, QStringLiteral("test"));
            }
        }
        QByteArray holderId;
        if (holder == QLatin1String("device")) {
            holderId = a.key.fingerprint();
        } else if (holder == QLatin1String("station")) {
            holderId = QByteArray(KeyerIdentity::kStationDeviceId);
        }
        if (holderId.isEmpty()) {
            QVERIFY(!held->holder().has_value());
        } else {
            QVERIFY(held->isHeldBy(holderId));
        }

        tuner.lan.send(QStringLiteral("transmit tune on"));   // a late echo of tune=1
        QTest::qWait(500);
        QVERIFY(!mox->isMox());
        QVERIFY(!core.model->isTune());
        QVERIFY(!core.model->isTgxlAutotuneInProgress());
        if (holderId.isEmpty()) {
            QVERIFY(!held->holder().has_value());
        } else {
            QVERIFY(held->isHeldBy(holderId));
        }
    }

    // m-2: frames sent while the tuner's :9010 link is down are counted
    // (the echo comes on :4992), and so is a reconnecting client's first
    // frame during a tune: none of their late echoes takes. A frame the
    // Core wrote to the dropped socket is counted too and never echoed:
    // a press inside the window uses up its entry and is dropped.
    void framesSentAroundReconnectsAreCounted()
    {
        TuningCore core;
        allowTransmit(core);
        Device a;
        core.pair(a);
        LoopbackTransport* appA = core.signIn(a, kTransmitter);
        QVERIFY(admitted(appA));
        FakeTuner tuner;
        QVERIFY(startTuner(core, tuner));
        tuner.lan.echo = LanClient::Echo::Off;
        MoxController* mox = core.model->moxController();
        TgxlConnection* link = core.model->tgxlConnection();
        const quint16 listenerPort = core.model->smartSdrListener()->serverPort();

        tuner.dropLink();
        QTRY_VERIFY(!link->isConnected());
        QVERIFY(core.invoke(appA, "tx.tune", {kOn}).value(QStringLiteral("accepted")).toBool());
        QTRY_VERIFY(core.model->isTune());
        QTRY_VERIFY(tuner.lan.tuneSeen());
        const int beforeDrop = tuner.lan.tuneFramesSeen();
        tuner.lan.close();   // its LAN PTT link drops and comes back mid-tune
        QVERIFY(tuner.lan.connectTo(QHostAddress::LocalHostIPv6, listenerPort));
        QTRY_VERIFY(tuner.lan.tuneFramesSeen() > beforeDrop);   // its first frame: tune=1
        QVERIFY(core.invoke(appA, "tx.tune", {kOff}).value(QStringLiteral("accepted")).toBool());
        QTRY_COMPARE(mox->state(), MoxState::Rx);
        QTRY_VERIFY(!core.model->isTune());
        QTRY_VERIFY(!tuner.lan.tuneSeen());
        QVERIFY(tuner.listen());
        link->connectToTgxl(QStringLiteral("::1"), tuner.port());
        QTRY_VERIFY_WITH_TIMEOUT(link->isConnected(), 3000);

        for (int i = 0; i < tuner.lan.tuneFramesSeen(); ++i) {
            tuneOnTakesNothing(core, tuner, a.key.fingerprint(), 150);
        }
        QTest::qWait(int(TgxlAnswerTracker::kAnswerWindowMs) + 200);
        tuneOnTakesAndKeys(core, tuner);
    }

    // N-2 (round 4): the tuner subscribes during a tune; the Core's
    // `transmit ... tune=1` push to it is counted like any other frame, so
    // its late echo takes nothing. Each tune=1 frame the tuner saw is one
    // count; one more tune on is a press.
    void aSubscriptionPushDuringATuneIsCounted()
    {
        TuningCore core;
        allowTransmit(core);
        Device a;
        core.pair(a);
        LoopbackTransport* appA = core.signIn(a, kTransmitter);
        QVERIFY(admitted(appA));
        FakeTuner tuner;
        QVERIFY(startTuner(core, tuner));
        tuner.lan.echo = LanClient::Echo::Off;
        QSignalSpy sent(core.model->smartSdrListener(), &SmartSdrApiListener::tuneStateSent);
        MoxController* mox = core.model->moxController();

        QVERIFY(core.invoke(appA, "tx.tune", {kOn}).value(QStringLiteral("accepted")).toBool());
        QTRY_VERIFY(core.model->isTune());
        QTRY_VERIFY(tuner.lan.tuneSeen());
        const int beforeSub = tuner.lan.tuneFramesSeen();
        tuner.lan.send(QStringLiteral("sub transmit all"));
        QTRY_VERIFY(tuner.lan.tuneFramesSeen() > beforeSub);
        QVERIFY(core.invoke(appA, "tx.tune", {kOff}).value(QStringLiteral("accepted")).toBool());
        QTRY_COMPARE(mox->state(), MoxState::Rx);
        QTRY_VERIFY(!tuner.lan.tuneSeen());
        QTest::qWait(100);
        QCOMPARE(countEmits(sent, true), tuner.lan.tuneFramesSeen());

        for (int i = 0; i < tuner.lan.tuneFramesSeen(); ++i) {
            tuneOnTakesNothing(core, tuner, a.key.fingerprint(), 100);
        }
        tuneOnTakesAndKeys(core, tuner);
    }

    // N-2 (round 4): a client that connects during a tune is sent tune=1,
    // and that frame is counted: one more than the frame rounds the
    // tuner's own connection saw. Deterministic: the tuner's connection
    // stays up, so no frame goes to a dropped socket.
    void aClientConnectingDuringATuneIsCounted()
    {
        TuningCore core;
        allowTransmit(core);
        Device a;
        core.pair(a);
        LoopbackTransport* appA = core.signIn(a, kTransmitter);
        QVERIFY(admitted(appA));
        FakeTuner tuner;
        QVERIFY(startTuner(core, tuner));
        tuner.lan.echo = LanClient::Echo::Off;
        SmartSdrApiListener* listener = core.model->smartSdrListener();
        QSignalSpy sent(listener, &SmartSdrApiListener::tuneStateSent);
        MoxController* mox = core.model->moxController();

        QVERIFY(core.invoke(appA, "tx.tune", {kOn}).value(QStringLiteral("accepted")).toBool());
        QTRY_VERIFY(core.model->isTune());
        QTRY_VERIFY(tuner.lan.tuneSeen());
        LanClient other;
        QVERIFY(other.connectTo(QHostAddress::LocalHost, listener->serverPort()));
        QTRY_VERIFY(other.tuneSeen());
        QVERIFY(core.invoke(appA, "tx.tune", {kOff}).value(QStringLiteral("accepted")).toBool());
        QTRY_COMPARE(mox->state(), MoxState::Rx);
        QTRY_VERIFY(!tuner.lan.tuneSeen());
        QTRY_VERIFY(!other.tuneSeen());
        QTest::qWait(100);
        const int counted = countEmits(sent, true);
        QCOMPARE(counted, tuner.lan.tuneFramesSeen() + 1);

        for (int i = 0; i < counted; ++i) {
            tuneOnTakesNothing(core, tuner, a.key.fingerprint(), 100);
        }
        tuneOnTakesAndKeys(core, tuner);
    }

    // N-4 (round 4): while idle, a new client's and a sub's tune=0 push
    // is not a change the tuner can echo, so nothing is counted; the
    // tuner's tune off after that ends its sweep as before.
    void anIdlePushIsNotCounted()
    {
        TuningCore core;
        FakeTuner tuner;
        QVERIFY(startTuner(core, tuner));
        SmartSdrApiListener* listener = core.model->smartSdrListener();
        QSignalSpy sent(listener, &SmartSdrApiListener::tuneStateSent);
        LanClient other;
        QVERIFY(other.connectTo(QHostAddress::LocalHost, listener->serverPort()));
        tuner.lan.send(QStringLiteral("sub transmit all"));
        other.send(QStringLiteral("sub transmit all"));
        QTest::qWait(1500);   // a 1 Hz idle tick too
        QCOMPARE(sent.count(), 0);
    }

    // N-2 (round 4), the recall exception: the tuner's tune on that
    // answers the band-change recall's `autotune` keys the carrier the
    // sweep needs as a station key, and never takes. Transmit unheld, and
    // held by the station. (A device holding it: autoRecallAnswerDoesNotTake.)
    void aRecallAnswerKeysAsAStationKey_data()
    {
        QTest::addColumn<bool>("stationHolds");
        QTest::newRow("unheld") << false;
        QTest::newRow("station holds") << true;
    }
    void aRecallAnswerKeysAsAStationKey()
    {
        QFETCH(bool, stationHolds);
        TuningCore core;
        allowTransmit(core);
        FakeTuner tuner;
        QVERIFY(startTuner(core, tuner));
        tuner.lan.echo = LanClient::Echo::Off;
        TransmitHolder* held = core.server->transmitHolder();
        MoxController* mox = core.model->moxController();
        const QByteArray station(KeyerIdentity::kStationDeviceId);
        if (stationHolds) {
            core.model->setTune(true);
            QTRY_VERIFY(core.model->isTune());
            core.model->setTune(false);
            QTRY_COMPARE(mox->state(), MoxState::Rx);
            QTRY_VERIFY(!core.model->isTune());
            QTest::qWait(int(TgxlAnswerTracker::kAnswerWindowMs) + 200);   // its echoes' entries pass
            QVERIFY(held->isHeldBy(station));
        } else {
            QVERIFY(!held->holder().has_value());
        }
        const quint64 epochBefore = held->epoch();

        armRecall(core);
        recallOnce(core, tuner);
        tuner.push(QStringLiteral("S0|state tuning=1"));
        tuner.lan.send(QStringLiteral("transmit tune on"));   // the recall's answer
        QTRY_VERIFY(mox->isMox());
        QVERIFY(held->isHeldBy(station));
        if (stationHolds) {
            QCOMPARE(held->epoch(), epochBefore);   // no change of holder: nothing taken
        }
        tuner.lan.send(QStringLiteral("transmit tune off"));
        tuner.push(QStringLiteral("S0|state tuning=0"));
        QTRY_COMPARE(mox->state(), MoxState::Rx);
        QTRY_VERIFY(!core.model->isTune());
        QVERIFY(held->isHeldBy(station));
    }

    // m-A (round 2): two autotunes in flight, the second refused: the
    // first's answer still takes nothing (counted, not one flag).
    void aRefusedAutotuneLeavesAnEarlierOneCounted()
    {
        TuningCore core;
        allowTransmit(core);
        Device a;
        core.pair(a);
        LoopbackTransport* appA = core.signIn(a, kTransmitter);
        QVERIFY(admitted(appA));
        FakeTuner tuner;
        QVERIFY(startTuner(core, tuner));
        deviceHoldsAfterItsTune(core, tuner, appA);
        armRecall(core);
        recallOnce(core, tuner);
        tuner.rejectAutotune = true;
        recallOnce(core, tuner);
        tuneOnTakesNothing(core, tuner, a.key.fingerprint());
    }

    // m-B: recovery by the tuner refusing the autotune: nothing is left
    // to answer, so its next tune on is its own TUNE.
    void aRefusedAutotuneIsNotWaitedFor()
    {
        TuningCore core;
        allowTransmit(core);
        Device a;
        core.pair(a);
        LoopbackTransport* appA = core.signIn(a, kTransmitter);
        QVERIFY(admitted(appA));
        FakeTuner tuner;
        QVERIFY(startTuner(core, tuner));
        deviceHoldsAfterItsTune(core, tuner, appA);
        armRecall(core);
        tuner.rejectAutotune = true;
        recallOnce(core, tuner);
        tuneOnTakesAndKeys(core, tuner);
    }

    // m-A / m-B: a tuning fall left from an earlier sweep does not clear
    // an autotune sent after it went up; a sweep that starts after the
    // send and ends without a tune on does.
    void onlyTheAutotunesOwnSweepClearsIt()
    {
        TuningCore core;
        allowTransmit(core);
        Device a;
        core.pair(a);
        LoopbackTransport* appA = core.signIn(a, kTransmitter);
        QVERIFY(admitted(appA));
        FakeTuner tuner;
        QVERIFY(startTuner(core, tuner));
        deviceHoldsAfterItsTune(core, tuner, appA);
        armRecall(core);

        tuner.push(QStringLiteral("S0|state tuning=1"));   // an earlier sweep
        QTest::qWait(100);
        recallOnce(core, tuner);
        tuner.push(QStringLiteral("S0|state tuning=0"));   // ... ends
        QTest::qWait(100);
        tuneOnTakesNothing(core, tuner, a.key.fingerprint());
        tuner.lan.send(QStringLiteral("transmit tune off"));
        QTest::qWait(100);

        recallOnce(core, tuner);
        tuner.push(QStringLiteral("S0|state tuning=1"));   // its own sweep
        tuner.push(QStringLiteral("S0|state tuning=0"));
        QTest::qWait(100);
        tuneOnTakesAndKeys(core, tuner);
    }

    // m-A / m-B: the tuner's link dropping and coming back inside the
    // answer window does not clear the autotune (its answer comes on the
    // SmartSDR API port); the window does.
    void aReconnectDoesNotClearAnAutotuneTheWindowDoes()
    {
        TuningCore core;
        allowTransmit(core);
        Device a;
        core.pair(a);
        LoopbackTransport* appA = core.signIn(a, kTransmitter);
        QVERIFY(admitted(appA));
        FakeTuner tuner;
        QVERIFY(startTuner(core, tuner));
        deviceHoldsAfterItsTune(core, tuner, appA);
        armRecall(core);
        recallOnce(core, tuner);

        TgxlConnection* link = core.model->tgxlConnection();
        tuner.dropLink();
        QTRY_VERIFY(!link->isConnected());
        QVERIFY(tuner.listen());
        link->connectToTgxl(QStringLiteral("::1"), tuner.port());
        QTRY_VERIFY_WITH_TIMEOUT(link->isConnected(), 3000);
        tuneOnTakesNothing(core, tuner, a.key.fingerprint());
        tuner.lan.send(QStringLiteral("transmit tune off"));

        recallOnce(core, tuner);
        QTest::qWait(int(TgxlAnswerTracker::kAnswerWindowMs) + 200);
        tuneOnTakesAndKeys(core, tuner);
    }

    // m-B: the tuner seen at its IPv4 address on :9010 and at the mapped
    // IPv6 form of it on the listener is the same tuner: its press takes.
    void aMappedAddressIsTheSameTuner()
    {
        TuningCore core;
        allowTransmit(core);
        Device a;
        core.pair(a);
        LoopbackTransport* appA = core.signIn(a, kTransmitter);
        QVERIFY(admitted(appA));
        FakeTuner tuner;
        QSignalSpy lines(core.model->smartSdrListener(), &SmartSdrApiListener::tuneRequested);
        QVERIFY(startTuner(core, tuner, QHostAddress::Any, QStringLiteral("127.0.0.1"),
                           QHostAddress(QStringLiteral("::ffff:127.0.0.1"))));
        QCOMPARE(QHostAddress(core.model->tgxlConnection()->peerAddress()).protocol(),
                 QAbstractSocket::IPv4Protocol);
        deviceHoldsAfterItsTune(core, tuner, appA);
        QVERIFY(!lines.isEmpty());
        const QHostAddress seen = lines.first().at(1).value<QHostAddress>();
        QCOMPARE(seen.protocol(), QAbstractSocket::IPv6Protocol);
        QVERIFY2(seen.toString().startsWith(QLatin1String("::ffff:")), qPrintable(seen.toString()));
        tuneOnTakesAndKeys(core, tuner);
    }

    // m-B: a cycle a desktop's Tuner page started from the tuner's
    // tuning=1 is waiting for the amplifier's standby when the tuner's own
    // press arrives: the press makes it the tuner's TUNE, which takes.
    void aPressDuringTheAmplifierWaitMakesTheCycleTheTuners()
    {
        TuningCore core;
        allowTransmit(core);
        Device a;
        core.pair(a);
        LoopbackTransport* appA = core.signIn(a, kTransmitter);
        QVERIFY(admitted(appA));
        FakeTuner tuner;
        QVERIFY(startTuner(core, tuner));
        deviceHoldsAfterItsTune(core, tuner, appA);
        MoxController* mox = core.model->moxController();
        PgxlConnection* amp = core.model->pgxlConnection();
        amp->injectLineForTesting(QStringLiteral("V3.8.9"));
        amp->injectLineForTesting(QStringLiteral("R1|0|state=OPERATE"));

        core.model->startTgxlAutotune(true);
        QTRY_VERIFY(core.model->isTgxlAutotuneInProgress());
        QVERIFY(!mox->isMox());   // waiting for the amplifier's standby
        tuner.lan.send(QStringLiteral("transmit tune on"));
        QTest::qWait(200);
        amp->injectLineForTesting(QStringLiteral("S0|status state=STANDBY"));
        QTRY_VERIFY(mox->isMox());
        QVERIFY(core.server->transmitHolder()->isHeldBy(
            QByteArray(KeyerIdentity::kStationDeviceId)));
        tuner.lan.send(QStringLiteral("transmit tune off"));
        QTRY_COMPARE(mox->state(), MoxState::Rx);
    }

    // I1: a cycle a desktop's Tuner page starts from the tuner's tuning=1
    // (startTgxlAutotune(true), TunerApplet.cpp) is not a press: it keys
    // as the station and is refused while a device holds transmit.
    void aTuningEdgeAloneDoesNotTake()
    {
        TuningCore core;
        allowTransmit(core);
        Device a;
        core.pair(a);
        LoopbackTransport* appA = core.signIn(a, kTransmitter);
        QVERIFY(admitted(appA));
        FakeTuner tuner;
        QVERIFY(startTuner(core, tuner));
        deviceHoldsAfterItsTune(core, tuner, appA);
        MoxController* mox = core.model->moxController();
        TransmitHolder* holder = core.server->transmitHolder();

        core.model->startTgxlAutotune(/*fromHardware=*/true);
        QTest::qWait(500);
        QVERIFY(!mox->isMox());
        QVERIFY(!core.model->isTune());
        QVERIFY(!core.model->isTgxlAutotuneInProgress());
        QVERIFY(holder->isHeldBy(a.key.fingerprint()));
    }

    // The tuner lets go while its take runs: the take ends, nothing keys.
    void hardwareTuneReleasedDuringTheTakeKeysNothing()
    {
        TuningCore core;
        allowTransmit(core);
        Device a;
        core.pair(a);
        LoopbackTransport* appA = core.signIn(a, kTransmitter);
        QVERIFY(admitted(appA));
        FakeTuner tuner;
        QVERIFY(startTuner(core, tuner));
        deviceHoldsAfterItsTune(core, tuner, appA);
        MoxController* mox = core.model->moxController();

        tuner.lan.send(QStringLiteral("transmit tune on"));
        tuner.lan.send(QStringLiteral("transmit tune off"));
        QTest::qWait(500);
        QVERIFY(!core.model->isTgxlAutotuneInProgress());
        QVERIFY(!mox->isMox());
        QVERIFY(!core.model->isTune());
        QCOMPARE(mox->state(), MoxState::Rx);
    }

    // M2: a take that does not finish ends the cycle; nothing keys after.
    void aTakeThatFailsEndsTheCycle()
    {
        TuningCore core;
        allowTransmit(core);
        Device a;
        core.pair(a);
        LoopbackTransport* appA = core.signIn(a, kTransmitter);
        QVERIFY(admitted(appA));
        FakeTuner tuner;
        QVERIFY(startTuner(core, tuner));
        deviceHoldsAfterItsTune(core, tuner, appA);
        MoxController* mox = core.model->moxController();

        // The take's end reported as failed, in the same call that asked
        // for it (after the Core's own handler, connected first), so before
        // the real transfer's end arrives a turn later.
        bool failedOnce = false;
        connect(core.model->smartSdrListener(), &SmartSdrApiListener::tuneRequested, this,
                [&core, mox, &failedOnce](bool on) {
                    if (on && !failedOnce && core.model->isTgxlAutotuneInProgress()
                        && !mox->isMox()) {
                        failedOnce = true;
                        emit mox->tunerTakeFinished(false);
                    }
                });
        tuner.lan.send(QStringLiteral("transmit tune on"));
        QTRY_VERIFY(failedOnce);
        QVERIFY(!core.model->isTgxlAutotuneInProgress());
        QTest::qWait(500);
        QVERIFY(!mox->isMox());
        QVERIFY(!core.model->isTune());
    }

    // M2: a holder on the air is never taken from: the cycle refuses to
    // start while RF flows.
    void aHolderOnTheAirIsNotTaken()
    {
        TuningCore core;
        allowTransmit(core);
        Device a;
        core.pair(a);
        LoopbackTransport* appA = core.signIn(a, kTransmitter);
        QVERIFY(admitted(appA));
        FakeTuner tuner;
        QVERIFY(startTuner(core, tuner));
        MoxController* mox = core.model->moxController();
        TransmitHolder* holder = core.server->transmitHolder();
        mox->setMox(true, keyerFor(a));
        QTRY_VERIFY(mox->isMox());
        QVERIFY(holder->isHeldBy(a.key.fingerprint()));

        tuner.lan.send(QStringLiteral("transmit tune on"));
        QTest::qWait(500);
        QVERIFY(mox->isMox());
        QCOMPARE(mox->currentKeyer().deviceId, a.key.fingerprint());
        QVERIFY(!core.model->isTune());
        QVERIFY(!core.model->isTgxlAutotuneInProgress());
        QVERIFY(holder->isHeldBy(a.key.fingerprint()));
        mox->setMox(false, keyerFor(a));
        QTRY_COMPARE(mox->state(), MoxState::Rx);
    }

    // M2: the start watchdog covers a carrier keyed by a take.
    void aTakenCarrierDropsOnTheSafetyTimeout()
    {
        TuningCore core;
        allowTransmit(core);
        Device a;
        core.pair(a);
        LoopbackTransport* appA = core.signIn(a, kTransmitter);
        QVERIFY(admitted(appA));
        FakeTuner tuner;
        QVERIFY(startTuner(core, tuner));
        deviceHoldsAfterItsTune(core, tuner, appA);
        MoxController* mox = core.model->moxController();

        tuner.lan.send(QStringLiteral("transmit tune on"));
        QTRY_VERIFY(mox->isMox());
        QTest::qWait(1000);
        QVERIFY(mox->isMox());
        QTRY_COMPARE_WITH_TIMEOUT(mox->state(), MoxState::Rx, 5000);
        QTRY_VERIFY(!core.model->isTune());
        QVERIFY(!core.model->isTgxlAutotuneInProgress());
    }

    // M2: the tuner lets go during the amplifier's standby wait: the cycle
    // ends, and the standby arriving afterwards keys nothing.
    void tuneOffDuringTheAmplifierStandbyKeysNothing()
    {
        TuningCore core;
        allowTransmit(core);
        FakeTuner tuner;
        QVERIFY(startTuner(core, tuner));
        MoxController* mox = core.model->moxController();
        PgxlConnection* amp = core.model->pgxlConnection();
        amp->injectLineForTesting(QStringLiteral("V3.8.9"));
        amp->injectLineForTesting(QStringLiteral("R1|0|state=OPERATE"));

        tuner.lan.send(QStringLiteral("transmit tune on"));
        QTRY_VERIFY(core.model->isTgxlAutotuneInProgress());
        QVERIFY(!mox->isMox());   // waiting for the amplifier's standby
        tuner.lan.send(QStringLiteral("transmit tune off"));
        QTRY_VERIFY(!core.model->isTgxlAutotuneInProgress());
        amp->injectLineForTesting(QStringLiteral("S0|status state=STANDBY"));
        QTest::qWait(500);
        QVERIFY(!mox->isMox());
        QVERIFY(!core.model->isTune());
    }

    // A device's tx.tunerTune never takes: with another device holding
    // transmit it is refused, and nothing keys.
    void aDeviceTunerTuneStillCannotTake()
    {
        TuningCore core;
        allowTransmit(core);
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        LoopbackTransport* appA = core.signIn(a, kTransmitter);
        LoopbackTransport* appB = core.signIn(b, kTransmitter);
        QVERIFY(admitted(appA));
        QVERIFY(admitted(appB));
        FakeTuner tuner;
        QVERIFY(startTuner(core, tuner));
        deviceHoldsAfterItsTune(core, tuner, appA);
        MoxController* mox = core.model->moxController();
        TransmitHolder* holder = core.server->transmitHolder();

        const QJsonObject answer = core.invoke(appB, "tx.tunerTune", {kOn});
        QVERIFY(!answer.value(QStringLiteral("accepted")).toBool());
        QVERIFY2(answer.value(QStringLiteral("reason")).toString().contains(QStringLiteral("iPhone")),
                 qPrintable(answer.value(QStringLiteral("reason")).toString()));
        QTest::qWait(500);
        QVERIFY(!mox->isMox());
        QVERIFY(!core.model->isTune());
        QVERIFY(holder->isHeldBy(a.key.fingerprint()));
        QCOMPARE(tuner.count(QStringLiteral("autotune")), 1);
    }

    // The tuner's front-panel TUNE with transmit unheld keys the station's
    // tune carrier without an autotune of ours, and its tune off drops it.
    void hardwareTuneKeysAndDropsOnTheTunersTuneOff()
    {
        TuningCore core;
        allowTransmit(core);
        FakeTuner tuner;
        QVERIFY(startTuner(core, tuner));
        MoxController* mox = core.model->moxController();
        tuner.lan.send(QStringLiteral("transmit tune on"));
        QTRY_VERIFY(core.model->isTune());
        QTRY_VERIFY(mox->isMox());
        QVERIFY(core.model->isTgxlAutotuneInProgress());

        tuner.lan.send(QStringLiteral("transmit tune off"));
        QTRY_COMPARE(mox->state(), MoxState::Rx);
        QTRY_VERIFY(!core.model->isTune());
        QVERIFY(!core.model->isTgxlAutotuneInProgress());
        QCOMPARE(tuner.count(QStringLiteral("autotune")), 0);
    }

    // Bug 1 replay (23:30:30.091): the tuner answers a device's autotune
    // with `transmit tune off` before its sweep starts. The pcap shows that
    // as the tuner giving the tune up (tune off with interlock not_ready,
    // then `M|LOW RF POWER`), so the carrier drops and the cycle ends.
    void appTuneDropsWhenTheTunerGivesUp()
    {
        TuningCore core;
        allowTransmit(core);
        Device a;
        core.pair(a);
        LoopbackTransport* appA = core.signIn(a, kTransmitter);
        QVERIFY(admitted(appA));
        FakeTuner tuner;
        QVERIFY(startTuner(core, tuner));
        MoxController* mox = core.model->moxController();
        QVERIFY(core.invoke(appA, "tx.tunerTune", {kOn}).value(QStringLiteral("accepted")).toBool());
        QTRY_VERIFY(mox->isMox());
        QTRY_COMPARE_WITH_TIMEOUT(tuner.count(QStringLiteral("autotune")), 1, 3000);

        tuner.lan.send(QStringLiteral("transmit tune off"));
        tuner.push(QStringLiteral("M|LOW RF POWER"));
        QTRY_COMPARE(mox->state(), MoxState::Rx);
        QTRY_VERIFY(!core.model->isTune());
        QVERIFY(!core.model->isTgxlAutotuneInProgress());
    }

    // A device's cycle ends with the tuner's sweep: tuning 1 then 0.
    void deviceCycleDropsWhenTheSweepEnds()
    {
        TuningCore core;
        allowTransmit(core);
        Device a;
        core.pair(a);
        LoopbackTransport* appA = core.signIn(a, kTransmitter);
        QVERIFY(admitted(appA));
        FakeTuner tuner;
        QVERIFY(startTuner(core, tuner));
        MoxController* mox = core.model->moxController();
        QVERIFY(core.invoke(appA, "tx.tunerTune", {kOn}).value(QStringLiteral("accepted")).toBool());
        QTRY_VERIFY(mox->isMox());
        tuner.push(QStringLiteral("S0|state tuning=1"));
        QTest::qWait(100);
        QVERIFY(mox->isMox());
        tuner.push(QStringLiteral("S0|state tuning=0"));
        QTRY_COMPARE(mox->state(), MoxState::Rx);
        QTRY_VERIFY(!core.model->isTune());
        QVERIFY(!core.model->isTgxlAutotuneInProgress());
    }

    // A device's cycle whose tuner never starts its sweep drops after the
    // bounded start watchdog (3 s).
    void deviceCycleDropsOnTheSafetyTimeout()
    {
        TuningCore core;
        allowTransmit(core);
        Device a;
        core.pair(a);
        LoopbackTransport* appA = core.signIn(a, kTransmitter);
        QVERIFY(admitted(appA));
        FakeTuner tuner;
        QVERIFY(startTuner(core, tuner));
        MoxController* mox = core.model->moxController();
        QVERIFY(core.invoke(appA, "tx.tunerTune", {kOn}).value(QStringLiteral("accepted")).toBool());
        QTRY_VERIFY(mox->isMox());
        QTest::qWait(1000);
        QVERIFY(mox->isMox());
        QTRY_COMPARE_WITH_TIMEOUT(mox->state(), MoxState::Rx, 5000);
        QTRY_VERIFY(!core.model->isTune());
        QVERIFY(!core.model->isTgxlAutotuneInProgress());
    }

    // The tuner's link dropping mid-sweep drops the carrier: no tune off or
    // tuning=0 will come from a lost tuner.
    void tunerDisconnectDropsTheCarrier()
    {
        TuningCore core;
        allowTransmit(core);
        Device a;
        core.pair(a);
        LoopbackTransport* appA = core.signIn(a, kTransmitter);
        QVERIFY(admitted(appA));
        FakeTuner tuner;
        QVERIFY(startTuner(core, tuner));
        MoxController* mox = core.model->moxController();
        QVERIFY(core.invoke(appA, "tx.tunerTune", {kOn}).value(QStringLiteral("accepted")).toBool());
        QTRY_VERIFY(mox->isMox());
        tuner.push(QStringLiteral("S0|state tuning=1"));
        QTest::qWait(100);

        tuner.dropLink();
        QTRY_COMPARE(mox->state(), MoxState::Rx);
        QTRY_VERIFY(!core.model->isTune());
        QVERIFY(!core.model->isTgxlAutotuneInProgress());
    }

    // The same for the tuner's own front-panel cycle.
    void tunerDisconnectDropsAHardwareCycle()
    {
        TuningCore core;
        allowTransmit(core);
        FakeTuner tuner;
        QVERIFY(startTuner(core, tuner));
        MoxController* mox = core.model->moxController();
        tuner.push(QStringLiteral("S0|state tuning=1"));
        tuner.lan.send(QStringLiteral("transmit tune on"));
        QTRY_VERIFY(mox->isMox());

        tuner.dropLink();
        QTRY_COMPARE(mox->state(), MoxState::Rx);
        QTRY_VERIFY(!core.model->isTune());
        QVERIFY(!core.model->isTgxlAutotuneInProgress());
    }

    // The tuner's front-panel cycle gets the 3 s start watchdog: a tuner
    // that never reports its sweep has its carrier dropped.
    void hardwareCycleDropsOnTheSafetyTimeout()
    {
        TuningCore core;
        allowTransmit(core);
        FakeTuner tuner;
        QVERIFY(startTuner(core, tuner));
        MoxController* mox = core.model->moxController();
        tuner.lan.send(QStringLiteral("transmit tune on"));
        QTRY_VERIFY(mox->isMox());
        QTest::qWait(1000);
        QVERIFY(mox->isMox());
        QTRY_COMPARE_WITH_TIMEOUT(mox->state(), MoxState::Rx, 5000);
        QTRY_VERIFY(!core.model->isTune());
        QVERIFY(!core.model->isTgxlAutotuneInProgress());
    }

    // ... and its sweep's end (tuning 1 then 0) drops the carrier too.
    void hardwareCycleDropsWhenTheSweepEnds()
    {
        TuningCore core;
        allowTransmit(core);
        FakeTuner tuner;
        QVERIFY(startTuner(core, tuner));
        MoxController* mox = core.model->moxController();
        tuner.push(QStringLiteral("S0|state tuning=1"));
        tuner.lan.send(QStringLiteral("transmit tune on"));
        QTRY_VERIFY(mox->isMox());
        QTest::qWait(3500);
        QVERIFY(mox->isMox());   // the sweep runs past the start watchdog
        tuner.push(QStringLiteral("S0|state tuning=0"));
        QTRY_COMPARE(mox->state(), MoxState::Rx);
        QTRY_VERIFY(!core.model->isTune());
        QVERIFY(!core.model->isTgxlAutotuneInProgress());
    }

    // Tune-ended lane: every end of a device's tx.tunerTune (answered
    // accepted) before its carrier keyed tells that device, once, why, in
    // plain words (notice tuneEnded, about its own request: no `by` keys,
    // no Take it back). The other device is told nothing. Each cycle waits
    // for the amplifier's standby, so nothing has keyed when it ends.
    void aDevicesCycleEndedUnkeyedTellsThatDeviceOnce_data()
    {
        QTest::addColumn<UnkeyedEnd>("end");
        QTest::newRow("tuner disconnected") << UnkeyedEnd::TunerDisconnected;
        QTest::newRow("tuner let go") << UnkeyedEnd::TunerLetGo;
        QTest::newRow("receive only") << UnkeyedEnd::ReceiveOnly;
        QTest::newRow("on the air") << UnkeyedEnd::OnAir;
        QTest::newRow("carrier refused") << UnkeyedEnd::CarrierRefused;
        QTest::newRow("transmit taken") << UnkeyedEnd::TransmitTaken;
        QTest::newRow("link lost") << UnkeyedEnd::LinkLost;
        QTest::newRow("no reason given") << UnkeyedEnd::NoReasonGiven;
    }
    void aDevicesCycleEndedUnkeyedTellsThatDeviceOnce()
    {
        QFETCH(UnkeyedEnd, end);
        TuningCore core;
        allowTransmit(core);
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        LoopbackTransport* appA = core.signIn(a, kTransmitter);
        LoopbackTransport* appB = core.signIn(b, kTransmitter);
        QVERIFY(admitted(appA));
        QVERIFY(admitted(appB));
        FakeTuner tuner;
        QVERIFY(startTuner(core, tuner));
        ampOperating(core);
        MoxController* mox = core.model->moxController();
        PgxlConnection* amp = core.model->pgxlConnection();

        QVERIFY(core.invoke(appA, "tx.tunerTune", {kOn}).value(QStringLiteral("accepted")).toBool());
        QVERIFY(core.model->isTgxlAutotuneInProgress());
        QVERIFY(!mox->isMox());   // waiting for the amplifier's standby
        QCOMPARE(core.model->tgxlAutotuneDeviceId(), a.key.fingerprint());

        QString expected;
        switch (end) {
        case UnkeyedEnd::TunerDisconnected:
            expected = RadioModel::tunerTuneEndedReason(RadioModel::TunerTuneEnd::TunerDisconnected);
            tuner.dropLink();
            break;
        case UnkeyedEnd::TunerLetGo:
            expected = RadioModel::tunerTuneEndedReason(RadioModel::TunerTuneEnd::TunerStopped);
            tuner.lan.send(QStringLiteral("transmit tune off"));
            break;
        case UnkeyedEnd::ReceiveOnly:
            expected = TxRefusals::stationReceiveOnly().text;
            core.model->setReceiveOnlyStationPolicy(true);
            amp->injectLineForTesting(QStringLiteral("S0|status state=STANDBY"));
            break;
        case UnkeyedEnd::OnAir:
            expected = TxRefusals::radioOnAir().text;
            armVoxAsHolder(core, appA, a, 940);
            if (QTest::currentTestFailed()) { return; }
            mox->onVoxActive(true);
            QTRY_COMPARE(mox->state(), MoxState::Tx);
            amp->injectLineForTesting(QStringLiteral("S0|status state=STANDBY"));
            break;
        case UnkeyedEnd::CarrierRefused:
            expected = TxRefusals::txInhibited().text;
            mox->setTxInhibited(true);
            amp->injectLineForTesting(QStringLiteral("S0|status state=STANDBY"));
            break;
        case UnkeyedEnd::TransmitTaken:
            expected = RadioModel::tunerTuneEndedReason(RadioModel::TunerTuneEnd::TransmitTaken);
            mox->onMicPttFromRadio(true);
            QTRY_VERIFY(core.server->transmitHolder()->isHeldBy(
                QByteArray(KeyerIdentity::kStationDeviceId)));
            break;
        case UnkeyedEnd::LinkLost: {
            // A's link drops while its cycle waits; the notice is kept for
            // it, away, and comes when it signs in again.
            expected = RadioModel::tunerTuneEndedReason(RadioModel::TunerTuneEnd::LinkLost);
            LoopbackTransport* dropped = appA;
            dropped->closeLink(QStringLiteral("lost"));
            QTRY_VERIFY(!core.model->isTgxlAutotuneInProgress());
            QVERIFY(tuneEndedNotices(dropped).isEmpty());
            core.now += 60000;
            appA = core.signIn(a, kTransmitter);
            QVERIFY(admitted(appA));
            break;
        }
        case UnkeyedEnd::NoReasonGiven:
            // The backstop for an end that names no reason. Every real end
            // path names its own now (the tune-ended lane's review found
            // none left), so this row is synthetic: MOX's manual flag
            // dropping with nothing keyed, emitted from outside, stands
            // for a future path that forgets its words.
            expected = RadioModel::tunerTuneEndedReason(RadioModel::TunerTuneEnd::NoReasonGiven);
            emit mox->manualMoxChanged(false);
            break;
        }
        QTRY_VERIFY(!core.model->isTgxlAutotuneInProgress());
        QVERIFY(!core.model->isTune());

        QTRY_COMPARE(tuneEndedNotices(appA).size(), 1);
        const QJsonObject told = tuneEndedNotices(appA).first();
        const QString reason = told.value(QStringLiteral("reason")).toString();
        QCOMPARE(reason, expected);
        QVERIFY2(OperatorWording::isPlain(reason), qPrintable(reason));
        QCOMPARE(told.value(QStringLiteral("takeBack")).toBool(true), false);
        QVERIFY(!told.contains(QStringLiteral("byDeviceId")));
        QTest::qWait(300);
        QCOMPARE(tuneEndedNotices(appA).size(), 1);
        QVERIFY(tuneEndedNotices(appB).isEmpty());

        if (end == UnkeyedEnd::OnAir) {
            mox->onVoxActive(false);
            QTRY_COMPARE(mox->state(), MoxState::Rx);
            core.model->transmitModel().setVoxEnabled(false);
        } else if (end == UnkeyedEnd::TransmitTaken) {
            mox->onMicPttFromRadio(false);
            QTRY_COMPARE(mox->state(), MoxState::Rx);
        } else {
            QVERIFY(!mox->isMox());
        }
        if (end == UnkeyedEnd::CarrierRefused) {
            mox->setTxInhibited(false);
        }
    }

    // Tune-ended lane: no tuneEnded where the device already knows. Its own
    // tx.tunerTune or tx.tune off answers it; a cycle whose carrier keyed
    // ends as any key does (keyedBy and txState), whatever ends it.
    void aStoppedOrKeyedCycleIsNotToldTuneEnded_data()
    {
        QTest::addColumn<int>("how");
        QTest::newRow("own tunerTune off") << 0;
        QTest::newRow("own tune off") << 1;
        QTest::newRow("keyed, sweep ends") << 2;
        QTest::newRow("keyed, tuner disconnected") << 3;
    }
    void aStoppedOrKeyedCycleIsNotToldTuneEnded()
    {
        QFETCH(int, how);
        TuningCore core;
        allowTransmit(core);
        Device a;
        core.pair(a);
        LoopbackTransport* appA = core.signIn(a, kTransmitter);
        QVERIFY(admitted(appA));
        FakeTuner tuner;
        QVERIFY(startTuner(core, tuner));
        MoxController* mox = core.model->moxController();
        if (how <= 1) {
            ampOperating(core);
        }
        QVERIFY(core.invoke(appA, "tx.tunerTune", {kOn}).value(QStringLiteral("accepted")).toBool());
        QVERIFY(core.model->isTgxlAutotuneInProgress());
        switch (how) {
        case 0:
            QVERIFY(core.invoke(appA, "tx.tunerTune", {kOff}).value(QStringLiteral("accepted")).toBool());
            break;
        case 1:
            QVERIFY(core.invoke(appA, "tx.tune", {kOff}).value(QStringLiteral("accepted")).toBool());
            break;
        case 2:
            QTRY_VERIFY(mox->isMox());
            tuner.push(QStringLiteral("S0|state tuning=1"));
            QTest::qWait(100);
            tuner.push(QStringLiteral("S0|state tuning=0"));
            break;
        default:
            QTRY_VERIFY(mox->isMox());
            tuner.push(QStringLiteral("S0|state tuning=1"));
            QTest::qWait(100);
            tuner.dropLink();
            break;
        }
        QTRY_VERIFY(!core.model->isTgxlAutotuneInProgress());
        QTRY_COMPARE(mox->state(), MoxState::Rx);
        QTRY_VERIFY(!core.model->isTune());
        QTest::qWait(300);
        QVERIFY(tuneEndedNotices(appA).isEmpty());
    }

    // The tuner's `M|` messages (why a tune ended) reach the log and a signal.
    void tunerMessageLineIsReported()
    {
        TuningCore core;
        FakeTuner tuner;
        QVERIFY(startTuner(core, tuner));
        QSignalSpy spy(core.model->tgxlConnection(), &TgxlConnection::messageReceived);
        tuner.push(QStringLiteral("M|Tuned SWR: 1.05:1"));
        tuner.push(QStringLiteral("M|LOW RF POWER"));
        QTRY_COMPARE(spy.count(), 2);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("Tuned SWR: 1.05:1"));
        QCOMPARE(spy.at(1).at(0).toString(), QStringLiteral("LOW RF POWER"));
    }
};

QTEST_MAIN(TgxlCoreTuneTest)
#include "tst_tgxl_core_tune.moc"
