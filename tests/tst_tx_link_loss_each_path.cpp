// =================================================================
// tests/tst_tx_link_loss_each_path.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test file.
//
// iPhone app plan Task 37 (R-IOS-13; remote design section 12.1, "the test
// must include severing the path with MOX asserted on each relay rung
// separately"; pairing design section 9.7; spec section 4.6 items 1 and
// 2). A real Core and a real remote window (the desktop's, which keys and
// sends its keepalives as a phone does) key the Core's radio over each
// path that exists, and the path is severed with the key on. Remote
// transmit hold then cut (2026-10-26): with every path quiet, the Core
// holds the TX channel's RF silent 400 to 500 ms after the last keepalive
// it heard, with MOX kept, and stops transmitting 3000 to 3100 ms after
// it; keepalives coming back first end the hold with nothing stopped; a
// closed session, or the device signing in again, stops at once.
//
//   - the session's WebSocket (the in-process loopback, severed without a
//     close, and closed);
//   - the media connection's "tx" data channel on a direct pair (real
//     DTLS/SCTP between two libdatachannel peers on this machine), severed
//     at the Core's end with the session still up. The session carries the
//     same keepalive, so that alone neither holds nor stops transmit;
//   - through TURN over UDP, and through the relay floor: the transports
//     (the plan's Tasks 27 to 29) are built, but this file cannot yet sever
//     either path with the key held (the fake TURN server and the stand-in
//     relay have no way to stop forwarding a live session, and this file's
//     session runs over the loopback link, not the service). Those rows are
//     skipped with that reason; the watchdog takes their keepalives through
//     the same call (RemoteTxWatchdog::keepalive) and its rules do not
//     change.
//
// Also here, with the same real Core and window: VOX a device armed goes
// off with its session and its link, and the Core never keys from its own
// microphone because of it; and a keyed device's microphone starving on a
// live link holds the RF in AM and not in USB, and with no audio for 10 s
// stops transmitting in both (FM transmit is not built yet, 3M-3b, so the
// Core refuses an FM key before any starvation).
//
// Nothing keys a real radio (the Core's radio is a static test model) and
// no real audio device opens (the window's microphone is a paced test
// bus). Real time throughout: labelled realtime.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-28: Run microphone-starvation cases in a separate test process
//               so their real-time waits do not exhaust the aggregate
//               120 s limit. Every case and timing assertion is retained.
//               J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-09-25: original test for NereusSDR by J.J. Boyd (KG4VCF), iPhone
//               app plan Task 37 (R-IOS-13), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-26: merge of Tasks 37 to 39: the window is told the watchdog's
//               and the starvation's stops on txState. J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26: Transmit group fix wave C1: the window tests without
//               media open a fake microphone line. J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude
//               Code.
//   2026-09-27: iPhone app plan Task 29 fix wave (R-IOS-16, review
//               Important 2): a key taken after the Core accepted a move's
//               join and before its barrier rides across the move with its
//               keepalives in order and the watchdog quiet; a move and its
//               station deadline never key and never drop a held key.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28: the TURN and relay-floor rows' skip reasons name what they
//               still need (a session through the service that can be
//               severed with the key held), not transports that now exist.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-01: the station's move deadline now ends the session as a
//               lost link (LINK-I2, link section 21.2), so a key held
//               through the move is kept, watchdog quiet, until that end
//               and stopped at once by it (section 18.2). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-15: remote transmit cut off the main thread: a Core whose
//               main thread is held still cuts a keyed device's transmit
//               on the connection's own thread within 500 ms of the last
//               "tx" message; keepalives fed from another thread hold the
//               key through the same hold; the cut's catch-up and the
//               watchdog stop once in either order. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-15: review fix wave: a release read after the cut's post is a
//               normal end, not a lost link; the cut's handling waits one
//               turn, so the order tests wait for it; why the feeder may
//               call the window's sendTx off its thread. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-15: re-review fixes: dropped VOX assertions that could not fail;
//               noted the order in the stop-once test is not forced.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26: Remote transmit hold then cut: the stop bound is named
//               RemoteTxWatchdog::kLinkCutMs (renamed from
//               kLinkLossDeadlineMs). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26: Remote transmit hold then cut, end to end: the data
//               channel severed alone keeps transmitting; every path
//               severed holds the TX channel's RF from 400 ms and cuts at
//               3000 ms; a short outage holds and ends with no stop; the
//               device signing in again while held stops at once; the
//               stalled Core's guard holds, then cuts; VOX goes off at the
//               cut; a starved microphone holds AM and cuts every mode at
//               10 s. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//               Code.
//   2026-10-27: Remote transmit hold then cut, fix wave B: signing in
//               again is a second window object on the same key while the
//               first link stays severed and unclosed; the window's media
//               dropped mid-hold keeps the hold and cuts at last packet +
//               10 s, in AM and USB; the short outage's hold is timed from
//               the restore edge with room for a 250 ms main-thread stall.
//               Round 2: signing in again with a VOX key, and a dropped
//               link with a VOX key, are Stop All TX; the first link is
//               open when the replacement's stop comes; a replacement line
//               that delivers at 5 s releases the hold and cancels the cut,
//               and one that never delivers moves nothing; the cut's lower
//               edge allows a 250 ms late read. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/safety/TransmitHolder.h"
#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/MoxController.h"
#include "core/IAudioBus.h"
#include "core/RadioConnection.h"
#include "core/TxChannel.h"
#include "core/WdspEngine.h"
#include "core/safety/RemoteTxCutGuard.h"
#include "core/safety/RemoteTxWatchdog.h"
#include "core/safety/StarvationPolicy.h"
#include "core/safety/TransmitCutLine.h"
#include "core/session/PathRacer.h"
#include "core/session/RemoteTransmitClient.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/session/TransmitStateFacade.h"
#include "core/session/media/DaemonMediaController.h"
#include "core/session/media/LibDataChannelMediaTransport.h"
#include "gui/RemoteMediaController.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

#include "fakes/RemoteAudioSessionHarness.h"
#include "RealtimeTestLoad.h"
#include "TestFunctionGroups.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QSignalSpy>
#include <QtTest>

#include <QThread>

#include <atomic>
#include <chrono>
#include <cmath>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

using namespace NereusSDR;

namespace {

// The window's microphone: a tone paced at 48 kHz that the test can
// silence (never a real device).
class PacedMicrophone final : public IAudioBus {
public:
    explicit PacedMicrophone(std::shared_ptr<std::atomic<bool>> giving)
        : m_giving(std::move(giving))
    {
    }
    bool open(const AudioFormat& format) override
    {
        m_format = format;
        m_open = true;
        return true;
    }
    void close() override { m_open = false; }
    bool isOpen() const override { return m_open; }
    qint64 push(const char*, qint64) override { return 0; }
    void flush() override {}
    qint64 pull(char* data, qint64 maxBytes) override
    {
        if (!m_open || data == nullptr || maxBytes < 4 || !m_giving->load()) {
            m_clock.invalidate();
            return 0;
        }
        if (!m_clock.isValid() || m_clock.elapsed() - m_lastPullMs > 100) {
            m_clock.start();
            m_delivered = 0;
        }
        m_lastPullMs = m_clock.elapsed();
        const qint64 due = m_clock.elapsed() * 48;
        const qint64 frames = std::min(due - m_delivered, maxBytes / 4);
        if (frames <= 0) {
            return 0;
        }
        auto* out = reinterpret_cast<float*>(data);
        for (qint64 i = 0; i < frames; ++i) {
            out[i] = 0.3f * static_cast<float>(std::sin(
                2.0 * 3.14159265358979323846 * 1000.0 * static_cast<double>(m_delivered + i)
                / 48000.0));
        }
        m_delivered += frames;
        return frames * 4;
    }
    float rxLevel() const override { return 0.0f; }
    float txLevel() const override { return 0.0f; }
    QString backendName() const override { return QStringLiteral("PacedMicrophone"); }
    AudioFormat negotiatedFormat() const override { return m_format; }

private:
    std::shared_ptr<std::atomic<bool>> m_giving;
    AudioFormat m_format{};
    bool m_open{false};
    QElapsedTimer m_clock;
    qint64 m_lastPullMs{0};
    qint64 m_delivered{0};
};

std::shared_ptr<std::atomic<bool>> attachMicrophone(Test::RemoteAudioSessionHarness& h)
{
    auto giving = std::make_shared<std::atomic<bool>>(true);
    AudioFormat fmt{};
    fmt.sample = AudioFormat::Sample::Float32;
    fmt.channels = 1;
    fmt.sampleRate = 48000;
    auto bus = std::make_unique<PacedMicrophone>(giving);
    bus->open(fmt);
    h.remote.audioEngine()->setTxInputBusForTest(std::move(bus));
    return giving;
}

// Fix wave B, round 2: the window takes transmit, arms the Core's VOX
// over its microphone line, and the Core's VOX detector keys it. Returns
// false (with a QVERIFY failure) when any step did not happen.
bool armVoxAndKey(Test::RemoteAudioSessionHarness& h, DaemonMediaController& daemonMedia)
{
    TransmitHolder::KeyRequest take;
    take.deviceId = h.windowKey->fingerprint();
    if (h.server.transmitHolder()->askKey(take).verdict != KeyingVerdict::Admit) {
        qWarning("the window could not take transmit");
        return false;
    }
    h.remote.transmitModel().setVoxEnabled(true);
    if (!QTest::qWaitFor([&]() {
            return h.station.transmitModel().voxEnabled()
                && h.station.remoteVoxDevice() == h.windowKey->fingerprint()
                && daemonMedia.micReceiver() != nullptr
                && daemonMedia.micReceiver()->stats().accepted >= 20;
        }, 5000)) {
        qWarning("VOX was not armed over the window's microphone line");
        return false;
    }
    h.station.moxController()->onVoxActive(true);
    if (!QTest::qWaitFor([&]() { return h.station.moxController()->isMox(); }, 5000)) {
        qWarning("VOX did not key");
        return false;
    }
    return h.station.keyedBy().deviceId == h.windowKey->fingerprint()
        && h.station.keyedBy().trigger == QByteArrayLiteral("vox");
}

int commandsOf(const Test::LoopbackTransport* core, const QString& verb)
{
    int count = 0;
    for (const QByteArray& wire : core->received()) {
        const QJsonObject o = QJsonDocument::fromJson(wire).object();
        if (o.value(QStringLiteral("type")).toString() == QLatin1String("command.invoke")
            && o.value(QStringLiteral("verb")).toString() == verb) {
            ++count;
        }
    }
    return count;
}

// What the watchdog reported when it stopped.
struct Trip {
    bool linkClosed{false};
    qint64 silentMs{-1};
};

Trip tripOf(const QSignalSpy& spy)
{
    if (spy.isEmpty()) {
        return {};
    }
    return {spy.first().at(1).toBool(), spy.first().at(2).toLongLong()};
}

// Remote transmit hold then cut: the hold lands within 100 ms of
// RemoteTxWatchdog::kLinkHoldMs and the cut within 100 ms of kLinkCutMs.
constexpr qint64 kHoldBoundMs = RemoteTxWatchdog::kLinkHoldMs + 100;
constexpr qint64 kCutBoundMs = RemoteTxWatchdog::kLinkCutMs + 100;
constexpr quint32 kLinkHoldBit = static_cast<quint32>(TxChannel::RfHold::Link);
constexpr quint32 kGuardHoldBit = static_cast<quint32>(TxChannel::RfHold::Guard);

// Task 29 fix wave: the tx.keepalive sequences a link carried to the Core,
// in the order it read them.
QList<qint64> keepaliveSequences(const QList<QByteArray>& wires)
{
    QList<qint64> out;
    for (const QByteArray& wire : wires) {
        const QJsonObject o = QJsonDocument::fromJson(wire).object();
        if (o.value(QStringLiteral("type")).toString() != QLatin1String("command.invoke")
            || o.value(QStringLiteral("verb")).toString() != QLatin1String("tx.keepalive")) {
            continue;
        }
        for (const QJsonValue& arg : o.value(QStringLiteral("args")).toArray()) {
            if (arg.toObject().value(QStringLiteral("name")).toString()
                == QLatin1String("sequence")) {
                out.append(arg.toObject().value(QStringLiteral("value")).toInteger());
            }
        }
    }
    return out;
}

bool strictlyRising(const QList<qint64>& values)
{
    for (qsizetype i = 1; i < values.size(); ++i) {
        if (values.at(i) <= values.at(i - 1)) {
            return false;
        }
    }
    return true;
}

// Task 29 fix wave: a second connection to the harness's Core, its hello
// read (what a race hands over), and a key taken from the window the
// moment the Core reads the move's path.join, before the Core answers
// with its barrier. `severOld` also drops what the Core sends on the old
// connection from then on (what the window sends there still arrives), so
// its barrier never reaches the window.
struct MoveWhileKeying {
    Test::LoopbackTransport* stationB = nullptr;
    Test::LoopbackTransport* clientB = nullptr;
    bool keyed = false;
    // What the Core read on each connection (the old one is gone after the
    // move, so it is recorded as it arrives).
    QList<QByteArray> readOnOld;
    QList<QByteArray> readOnNew;

    bool open(Test::RemoteAudioSessionHarness& h, bool severOld)
    {
        stationB = new Test::LoopbackTransport(QStringLiteral("station B"));
        clientB = new Test::LoopbackTransport(QStringLiteral("client B"));
        stationB->linkTo(clientB);
        QObject::connect(h.stationLink, &SessionTransport::textReceived, stationB,
                         [this](const QByteArray& wire) { readOnOld.append(wire); });
        QObject::connect(stationB, &SessionTransport::textReceived, stationB,
                         [this](const QByteArray& wire) { readOnNew.append(wire); });
        // Connected before the Core's own handler (acceptTransport), so it
        // runs first on the join.
        QObject::connect(stationB, &SessionTransport::textReceived, stationB,
                         [this, &h, severOld](const QByteArray& wire) {
            if (keyed || !wire.contains("path.join")) {
                return;
            }
            keyed = true;
            if (severOld) {
                h.stationLink->setDropsOutgoing(true);
            }
            QMetaObject::invokeMethod(
                &h.remote, [&h] { h.remote.setMoxFromButton(true); }, Qt::QueuedConnection);
        });
        QList<QByteArray> hello;
        const QMetaObject::Connection watch = QObject::connect(
            clientB, &SessionTransport::textReceived, clientB,
            [&hello](const QByteArray& wire) { hello.append(wire); });
        h.server.acceptTransport(stationB);
        const bool arrived = QTest::qWaitFor([&hello] { return !hello.isEmpty(); }, 5000);
        QObject::disconnect(watch);
        return arrived;
    }
};
const QString kStopSentence =
    QStringLiteral("The link to Shack MacBook went quiet, so the Core stopped transmitting.");

// ---- Remote transmit cut off the main thread (2026-10-15) ----------------

using Clock = std::chrono::steady_clock;

double msBetween(Clock::time_point a, Clock::time_point b)
{
    return std::chrono::duration<double, std::milli>(b - a).count();
}

// The station model's radio connection: records MOX and relay writes with
// their thread and time, from any thread (as tst_transmit_cut_line's).
class RecordingConnection : public RadioConnection {
    Q_OBJECT
public:
    struct Entry {
        QString what;
        QThread* thread{nullptr};
        Clock::time_point at;
    };

    RecordingConnection() : RadioConnection(nullptr)
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
    void setMox(bool on) override { add(on ? QStringLiteral("MOX on") : QStringLiteral("MOX off")); }
    void setTrxRelay(bool on) override
    {
        add(on ? QStringLiteral("relay on") : QStringLiteral("relay off"));
    }
    void setMicBoost(bool) override {}
    void setLineIn(bool) override {}
    void setMicTipRing(bool) override {}
    void setMicBias(bool) override {}
    void setLineInGain(int) override {}
    void setUserDigOut(quint8) override {}
    void setPuresignalRun(bool) override {}
    void setMicPTTDisabled(bool) override {}
    void setMicXlr(bool) override {}

    std::vector<Entry> since(std::size_t from) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (from >= m_entries.size()) {
            return {};
        }
        return std::vector<Entry>(m_entries.begin() + static_cast<std::ptrdiff_t>(from),
                                  m_entries.end());
    }
    std::size_t count() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_entries.size();
    }

private:
    void add(const QString& what)
    {
        const Clock::time_point now = Clock::now();
        std::lock_guard<std::mutex> lock(m_mutex);
        m_entries.push_back({what, QThread::currentThread(), now});
    }

    mutable std::mutex m_mutex;
    std::vector<Entry> m_entries;
};

const RecordingConnection::Entry* findEntry(const std::vector<RecordingConnection::Entry>& list,
                                            const QString& what)
{
    for (const auto& e : list) {
        if (e.what == what) {
            return &e;
        }
    }
    return nullptr;
}

bool hasEntry(const std::vector<RecordingConnection::Entry>& list, const QString& what)
{
    return findEntry(list, what) != nullptr;
}

// Remote transmit hold then cut: watches a TX channel's hold mask from a
// thread of its own (the cut guard holds off the main thread, so a held
// main thread cannot see the moment): when any of `bits` was first set.
class HoldMaskWatch {
public:
    HoldMaskWatch(const TxChannel& tx, quint32 bits)
        : m_thread([this, &tx, bits] {
            while (m_running.load()) {
                if (!m_seen.load() && (tx.rfHoldMask() & bits) != 0u) {
                    m_firstAt = Clock::now();
                    m_seen.store(true);
                }
                std::this_thread::sleep_for(std::chrono::microseconds(200));
            }
        })
    {
    }
    ~HoldMaskWatch() { stop(); }
    HoldMaskWatch(const HoldMaskWatch&) = delete;
    HoldMaskWatch& operator=(const HoldMaskWatch&) = delete;

    void stop()
    {
        if (m_running.exchange(false)) {
            m_thread.join();
        }
    }
    bool seen() const { return m_seen.load(); }
    Clock::time_point firstAt() const { return m_firstAt; }

private:
    std::atomic<bool> m_running{true};
    std::atomic<bool> m_seen{false};
    Clock::time_point m_firstAt{};
    std::thread m_thread;   // last: it starts once the rest are set
};

// The real Core and window on a direct media pair, as
// dataChannelSeveredAloneKeepsTransmitting sets them up, with the station
// model given a recording connection on a QThread of its own (the
// harness's has none) and a TX channel wrapper with no WDSP channel behind
// it (its hold mask is what the guard's hold sets; declared first, so it
// outlives the harness), and both ends' transports at hand.
struct CutRig {
    TxChannel tx{WdspEngine::kTxChannelId};
    Test::RemoteAudioSessionHarness h;
    RecordingConnection conn;
    QThread connThread;
    QPointer<LibDataChannelMediaTransport> coreTransport;
    QPointer<LibDataChannelMediaTransport> windowTransport;
    std::unique_ptr<RemoteMediaController> remoteMedia;
    std::unique_ptr<DaemonMediaController> daemonMedia;

    CutRig()
    {
        h.pairWindow = true;
        h.makeTransmitReady();
        attachMicrophone(h);
        conn.moveToThread(&connThread);
        connThread.start();
        h.station.injectConnectionForTest(&conn);
        h.station.injectTxChannelForTest(&tx);
        remoteMedia = std::make_unique<RemoteMediaController>(
            &h.client, &h.remote, nullptr, nullptr, [this](QObject* parent) {
                auto* transport = new LibDataChannelMediaTransport(parent);
                windowTransport = transport;
                return static_cast<IMediaTransport*>(transport);
            });
        daemonMedia = std::make_unique<DaemonMediaController>(
            &h.server, &h.station, nullptr, [this](QObject* parent) {
                auto* transport = new LibDataChannelMediaTransport(parent);
                coreTransport = transport;
                return static_cast<IMediaTransport*>(transport);
            });
    }

    ~CutRig()
    {
        h.client.disconnectFromStation(QStringLiteral("test complete"));
        remoteMedia.reset();
        daemonMedia.reset();
        h.station.injectTxChannelForTest(nullptr);
        h.station.injectConnectionForTest(nullptr);
        connThread.quit();
        connThread.wait();
    }

    // Connected, the window keyed with its keepalives on the "tx" channel,
    // and the Core's cut guard judging them.
    bool keyOnTheTxChannel()
    {
        h.connectSession();
        if (!QTest::qWaitFor([this] { return daemonMedia->micReceiver() != nullptr; }, 5000)
            || !QTest::qWaitFor([this] { return h.client.capabilities().txPermitted; }, 5000)
            || !remoteMedia->micLineNegotiated() || coreTransport.isNull()
            || windowTransport.isNull() || h.server.txCutGuard() == nullptr) {
            qWarning("cut rig: the media session did not come up");
            return false;
        }
        // Give the "tx" channel time to open after media is ready.
        QTest::qWait(300);
        h.remote.setMoxFromButton(true);
        RemoteTransmitClient* const transmit = h.client.remoteTransmit();
        if (!QTest::qWaitFor(
                [this, transmit] {
                    return h.station.moxController()->isMox() && transmit != nullptr
                        && transmit->channelKeepalivesSent() > 0
                        && h.server.txCutGuard()->isJudging()
                        && hasEntry(conn.since(0), QStringLiteral("MOX on"));
                },
                5000)) {
            qWarning("cut rig: no key with the guard judging the tx channel");
            return false;
        }
        // Settled: the channel keepalive and the same sequence on the session.
        QTest::qWait(500);
        return h.station.moxController()->isMox() && h.server.txCutGuard()->isJudging()
            && transmit->channelKeepalivesSent() > 0 && transmit->sessionKeepalivesSent() > 0
            && h.station.keyedBy().deviceId == h.windowKey->fingerprint();
    }
};

} // namespace

class TestTxLinkLossEachPath : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        const QString profile = QStringLiteral("tx-link-loss-each-path-%1")
                                    .arg(QCoreApplication::applicationPid());
        AppSettings::setProfileOverride(profile);
        QCOMPARE(AppSettings::instance().filePath(), AppSettings::resolveSettingsPath(profile));
        AppSettings::instance().clear();
    }

    void cleanupTestCase()
    {
        const QString path = AppSettings::instance().filePath();
        QFile::remove(path);
        QFile::remove(path + QStringLiteral(".bak"));
    }

    void cleanup() { RealtimeTestLoad::printLoadAverageIfFailed(); }

    // ---- The session's WebSocket ------------------------------------------

    // Keyed over the session (no media here), keepalives on the session
    // hold the key; the path goes dead without closing. Hold then cut:
    // 400 to 500 ms after the last keepalive the Core heard, the TX
    // channel's RF is held silent with MOX kept; 3000 to 3100 ms after it,
    // transmit stops with the lost link's reason.
    void everyPathSeveredHoldsThenCuts()
    {
        TxChannel tx{WdspEngine::kTxChannelId};   // outlives the harness
        Test::RemoteAudioSessionHarness h;
        h.station.injectTxChannelForTest(&tx);
        h.pairWindow = true;
        h.makeTransmitReady();
        h.openFakeMicrophoneLine();   // fix wave C1: no media here
        h.connectSession();
        QTRY_VERIFY(h.client.capabilities().txPermitted);
        RemoteTransmitClient* transmit = h.client.remoteTransmit();
        RemoteTxWatchdog* const watchdog = h.server.txWatchdog();
        QSignalSpy tripped(watchdog, &RemoteTxWatchdog::tripped);
        QSignalSpy held(watchdog, &RemoteTxWatchdog::holdStarted);
        QSignalSpy ended(watchdog, &RemoteTxWatchdog::holdEnded);
        QSignalSpy stopped(&h.station, &RadioModel::transmitStopped);
        // What the TX channel and MOX were the moment the hold started.
        bool channelHeldAtHold = false;
        bool moxAtHold = false;
        QObject scope;   // last: disconnects before the flags go
        QObject::connect(watchdog, &RemoteTxWatchdog::holdStarted, &scope,
                         [&](const QByteArray&, qint64) {
            channelHeldAtHold = (tx.rfHoldMask() & kLinkHoldBit) != 0u;
            moxAtHold = h.station.moxController()->isMox();
        });

        h.remote.setMoxFromButton(true);
        QTRY_VERIFY_WITH_TIMEOUT(h.station.moxController()->isMox(), 5000);
        QCOMPARE(h.station.keyedBy().deviceId, h.windowKey->fingerprint());
        QVERIFY(watchdog->isWatching(h.windowKey->fingerprint()));

        // Two seconds keyed: the keepalives on the session hold it, and
        // nothing is held.
        QTest::qWait(2000);
        QVERIFY(h.station.moxController()->isMox());
        QCOMPARE(tripped.count(), 0);
        QCOMPARE(held.count(), 0);
        QCOMPARE(tx.rfHoldMask(), 0u);
        QVERIFY(transmit->sessionKeepalivesSent() >= 15);
        QCOMPARE(transmit->channelKeepalivesSent(), quint64(0));
        QVERIFY(commandsOf(h.stationLink, QStringLiteral("tx.keepalive")) >= 15);

        // Severed: nothing crosses either way, and nothing closes.
        h.stationLink->setSevered(true);
        QTRY_COMPARE_WITH_TIMEOUT(held.count(), 1, 3000);
        const qint64 silentAtHold = held.first().at(1).toLongLong();
        qInfo("every path severed: held %lld ms after the last keepalive",
              static_cast<long long>(silentAtHold));
        QVERIFY2(silentAtHold > RemoteTxWatchdog::kLinkHoldMs && silentAtHold <= kHoldBoundMs,
                 qPrintable(QString::number(silentAtHold)));
        QVERIFY(channelHeldAtHold);
        QVERIFY(moxAtHold);
        QVERIFY(tx.isRfHeld());
        QVERIFY(h.station.isRemoteRfHeld());
        QVERIFY(h.station.moxController()->isMox());
        QCOMPARE(tripped.count(), 0);
        QCOMPARE(stopped.count(), 0);

        QTRY_COMPARE_WITH_TIMEOUT(tripped.count(), 1, 5000);
        const Trip trip = tripOf(tripped);
        qInfo("every path severed: stopped %lld ms after the last keepalive",
              static_cast<long long>(trip.silentMs));
        QVERIFY(!trip.linkClosed);
        QVERIFY2(trip.silentMs > RemoteTxWatchdog::kLinkCutMs && trip.silentMs <= kCutBoundMs,
                 qPrintable(QString::number(trip.silentMs)));
        QCOMPARE(held.count(), 1);
        QCOMPARE(ended.count(), 1);
        QCOMPARE(ended.first().at(2).value<RemoteTxWatchdog::HoldEnd>(),
                 RemoteTxWatchdog::HoldEnd::Cut);
        QVERIFY(!h.station.moxController()->isMox());
        QCOMPARE(stopped.count(), 1);
        QCOMPARE(stopped.first().at(0).toString(), kStopSentence);
        QTRY_COMPARE(tx.rfHoldMask(), 0u);
        QVERIFY(!h.station.isRemoteRfHeld());
        h.client.disconnectFromStation(QStringLiteral("test complete"));
        h.station.injectTxChannelForTest(nullptr);
    }

    // Every path severed for 1500 ms, then restored: the RF is held from
    // about 400 ms, the first keepalive the Core counts again ends the hold
    // (the TX channel unheld within 200 ms of it), nothing is stopped, and
    // txState counts the one hold, about 1100 ms long.
    void everyPathSeveredBrieflyHoldsAndKeepalivesEndIt()
    {
        TxChannel tx{WdspEngine::kTxChannelId};   // outlives the harness
        Test::RemoteAudioSessionHarness h;
        h.station.injectTxChannelForTest(&tx);
        h.pairWindow = true;
        h.makeTransmitReady();
        h.openFakeMicrophoneLine();
        h.connectSession();
        QTRY_VERIFY(h.client.capabilities().txPermitted);
        RemoteTxWatchdog* const watchdog = h.server.txWatchdog();
        const QByteArray device = h.windowKey->fingerprint();
        QSignalSpy tripped(watchdog, &RemoteTxWatchdog::tripped);
        QSignalSpy held(watchdog, &RemoteTxWatchdog::holdStarted);
        QSignalSpy ended(watchdog, &RemoteTxWatchdog::holdEnded);
        QSignalSpy stopped(&h.station, &RadioModel::transmitStopped);
        QSignalSpy channelHolds(&h.station, &RadioModel::remoteRfHoldChanged);

        h.remote.setMoxFromButton(true);
        QTRY_VERIFY_WITH_TIMEOUT(h.station.moxController()->isMox(), 5000);
        QCOMPARE(h.station.keyedBy().deviceId, device);
        QTest::qWait(1000);
        QCOMPARE(held.count(), 0);
        QCOMPARE(h.server.transmitState()->rfHoldSerial(), quint64(0));

        // After the restore: when the first keepalive was counted and when
        // the TX channel was let go, on one clock.
        QElapsedTimer restored;
        qint64 firstHeardMs = -1;
        qint64 unheldMs = -1;
        bool channelHeldAtFirstHeard = true;
        QElapsedTimer severed;
        qint64 heldAtSeveredMs = -1;
        QObject scope;   // last: disconnects before the values go
        QObject::connect(watchdog, &RemoteTxWatchdog::keepaliveHeard, &scope,
                         [&](const QByteArray& id, qint64) {
            if (restored.isValid() && firstHeardMs < 0 && id == device) {
                firstHeardMs = restored.elapsed();
                channelHeldAtFirstHeard = tx.isRfHeld();
            }
        });
        QObject::connect(&h.station, &RadioModel::remoteRfHoldChanged, &scope, [&](bool on) {
            if (restored.isValid() && unheldMs < 0 && !on) {
                unheldMs = restored.elapsed();
            }
        });

        QObject::connect(watchdog, &RemoteTxWatchdog::holdStarted, &scope,
                         [&](const QByteArray& id, qint64) {
            if (id == device && heldAtSeveredMs < 0) {
                heldAtSeveredMs = severed.elapsed();
            }
        });
        h.stationLink->setSevered(true);
        severed.start();
        QTRY_COMPARE_WITH_TIMEOUT(held.count(), 1, 3000);
        const qint64 silentAtHold = held.first().at(1).toLongLong();
        QVERIFY2(silentAtHold > RemoteTxWatchdog::kLinkHoldMs && silentAtHold <= kHoldBoundMs,
                 qPrintable(QString::number(silentAtHold)));
        QVERIFY(tx.isRfHeld());
        QVERIFY(h.station.moxController()->isMox());
        QTest::qWait(static_cast<int>(std::max<qint64>(0, 1500 - severed.elapsed())));
        QVERIFY(tx.isRfHeld());
        QVERIFY(h.station.moxController()->isMox());
        QCOMPARE(tripped.count(), 0);

        // Restored: the keepalives cross again.
        const qint64 restoredAtSeveredMs = severed.elapsed();
        restored.start();
        h.stationLink->setSevered(false);
        QTRY_COMPARE_WITH_TIMEOUT(ended.count(), 1, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(firstHeardMs >= 0 && unheldMs >= 0, 3000);
        qInfo("restored after %lld ms severed: first keepalive counted %lld ms later, "
              "TX channel unheld at %lld ms",
              static_cast<long long>(severed.elapsed() - restored.elapsed()),
              static_cast<long long>(firstHeardMs), static_cast<long long>(unheldMs));
        QCOMPARE(ended.first().at(0).toByteArray(), device);
        QCOMPARE(ended.first().at(2).value<RemoteTxWatchdog::HoldEnd>(),
                 RemoteTxWatchdog::HoldEnd::Heard);
        QVERIFY(!channelHeldAtFirstHeard);
        // Fix wave B: room for a 250 ms main-thread stall in the bound.
        QVERIFY2(unheldMs <= firstHeardMs + 200 + 250,
                 qPrintable(QStringLiteral("%1 %2").arg(unheldMs).arg(firstHeardMs)));
        QVERIFY(!tx.isRfHeld());
        QVERIFY(!h.station.isRemoteRfHeld());
        // Fix wave B: the hold's length, timed from its own start and the
        // restore edge (the first counted keepalive after it), on the test's
        // clock: both ends are read in the handlers that report them, so a
        // stalled main thread moves both alike.
        const qint64 holdMs = ended.first().at(1).toLongLong();
        QVERIFY(heldAtSeveredMs >= 0);
        const qint64 expectedHoldMs = restoredAtSeveredMs - heldAtSeveredMs + firstHeardMs;
        QVERIFY2(std::abs(holdMs - expectedHoldMs) <= 50,
                 qPrintable(QStringLiteral("%1 %2").arg(holdMs).arg(expectedHoldMs)));
        QVERIFY2(holdMs >= 1500 - kHoldBoundMs - 250 && holdMs <= 1400 + 250,
                 qPrintable(QString::number(holdMs)));

        // The key goes on with nothing stopped; txState counts one hold.
        QTest::qWait(1500);
        QVERIFY(h.station.moxController()->isMox());
        QCOMPARE(tripped.count(), 0);
        QCOMPARE(stopped.count(), 0);
        QCOMPARE(held.count(), 1);
        QCOMPARE(channelHolds.count(), 2);   // on, then off
        TransmitState* const core = h.server.transmitState();
        QVERIFY(!core->rfHeld());
        QCOMPARE(core->rfHoldSerial(), quint64(1));
        QVERIFY2(std::abs(core->rfLastHoldMs() - holdMs) <= 50,
                 qPrintable(QStringLiteral("%1 %2").arg(core->rfLastHoldMs()).arg(holdMs)));
        QCOMPARE(core->stopSerial(), quint32(0));
        QTRY_COMPARE(h.client.transmitState()->rfHoldSerial(), quint64(1));
        QCOMPARE(h.client.transmitState()->rfLastHoldMs(), core->rfLastHoldMs());
        QVERIFY(!h.client.transmitState()->rfHeld());
        QVERIFY(h.client.transmitState()->keyed());

        h.remote.setMoxFromButton(false);
        QTRY_VERIFY_WITH_TIMEOUT(!h.station.moxController()->isMox(), 5000);
        QCOMPARE(stopped.count(), 0);
        h.client.disconnectFromStation(QStringLiteral("test complete"));
        h.station.injectTxChannelForTest(nullptr);
    }

    // The window signs in again while its key is held: a second window
    // object on the same device key signs in over a new link while the
    // first link stays severed and unclosed (fix wave B, review Important
    // 1). The Core ends the old session for the replacement (ruling 4.8),
    // and with it transmit at once as a closed link, well inside the
    // 3000 ms cut; the first link saw no close before the Core's own; the
    // new session finds transmit stopped.
    // Fix wave B, round 2: a VOX key (the Core's VOX keyed by the window's
    // microphone line) is stopped the same way, through Stop All TX, never
    // through VOX's own release.
    void signingInAgainWhileHeldStopsAtOnce_data()
    {
        QTest::addColumn<bool>("vox");
        QTest::newRow("a screen key") << false;
        QTest::newRow("a VOX key") << true;
    }
    void signingInAgainWhileHeldStopsAtOnce()
    {
        QFETCH(bool, vox);
        TxChannel tx{WdspEngine::kTxChannelId};   // outlives the harness
        Test::RemoteAudioSessionHarness h;
        h.station.injectTxChannelForTest(&tx);
        h.pairWindow = true;
        h.makeTransmitReady();
        QPointer<LibDataChannelMediaTransport> coreTransport;
        std::unique_ptr<RemoteMediaController> remoteMedia;
        std::unique_ptr<DaemonMediaController> daemonMedia;
        if (vox) {
            attachMicrophone(h);
            remoteMedia = std::make_unique<RemoteMediaController>(&h.client, &h.remote, nullptr);
            daemonMedia = std::make_unique<DaemonMediaController>(
                &h.server, &h.station, nullptr, [&coreTransport](QObject* parent) {
                    auto* transport = new LibDataChannelMediaTransport(parent);
                    coreTransport = transport;
                    return static_cast<IMediaTransport*>(transport);
                });
        } else {
            h.openFakeMicrophoneLine();
        }
        h.connectSession();
        QTRY_VERIFY(h.client.capabilities().txPermitted);
        RemoteTxWatchdog* const watchdog = h.server.txWatchdog();
        QSignalSpy tripped(watchdog, &RemoteTxWatchdog::tripped);
        QSignalSpy held(watchdog, &RemoteTxWatchdog::holdStarted);
        QSignalSpy ended(watchdog, &RemoteTxWatchdog::holdEnded);
        QSignalSpy stopped(&h.station, &RadioModel::transmitStopped);

        if (vox) {
            QTRY_VERIFY_WITH_TIMEOUT(daemonMedia->micReceiver() != nullptr, 5000);
            QVERIFY(armVoxAndKey(h, *daemonMedia));
        } else {
            h.remote.setMoxFromButton(true);
            QTRY_VERIFY_WITH_TIMEOUT(h.station.moxController()->isMox(), 5000);
        }
        QTest::qWait(500);
        const QPointer<Test::RewritingTransport> firstLink = h.stationLink;
        QVERIFY(!firstLink.isNull());
        firstLink->setSevered(true);
        if (vox) {
            // The media connection's keepalives go quiet with the session's.
            QVERIFY(!coreTransport.isNull());
            coreTransport->setReceiveSeveredForTest(true);
        }
        QTRY_COMPARE_WITH_TIMEOUT(held.count(), 1, 3000);
        QVERIFY(tx.isRfHeld());
        QVERIFY(h.station.moxController()->isMox());
        QCOMPARE(tripped.count(), 0);
        // Nothing has closed the first link.
        QVERIFY(firstLink->isOpen());
        QVERIFY(firstLink->closeReason().isEmpty());

        // What the first link had seen when the stop came.
        bool firstLinkOpenAtStop = false;
        QString firstLinkCloseAtStop;
        QObject scope;   // last: disconnects before the values go
        QObject::connect(watchdog, &RemoteTxWatchdog::tripped, &scope,
                         [&](const QByteArray&, bool, qint64) {
            if (!firstLink.isNull()) {
                firstLinkOpenAtStop = firstLink->isOpen();
                firstLinkCloseAtStop = firstLink->closeReason();
            }
        });

        // A second window object, the same device key, a new link.
        RadioModel remoteAgain{RadioModel::Role::Remote};
        remoteAgain.setConnectionStateForTest(ConnectionState::Connected);
        SettingsProxy proxyAgain;
        StationClient again{&remoteAgain, &proxyAgain};
        again.setDeviceIdentity(h.windowKey, QStringLiteral("Shack MacBook"),
                                QStringLiteral("MacBook"));
        auto* stationEnd = new Test::LoopbackTransport(QStringLiteral("station again"));
        auto* clientEnd = new Test::LoopbackTransport(QStringLiteral("client again"));
        stationEnd->linkTo(clientEnd);
        QString pin = h.server.certificateFingerprint();
        pin.remove(QLatin1Char(':'));
        clientEnd->setPeerCertificateSha256(QByteArray::fromHex(pin.toLatin1()));
        QElapsedTimer signIn;
        signIn.start();
        again.startSession(clientEnd, QString(), QString(),
                           h.server.stationIdentity().fingerprint());
        h.server.acceptTransport(stationEnd);

        QTRY_COMPARE_WITH_TIMEOUT(tripped.count(), 1, 3000);
        const qint64 stoppedAfterMs = signIn.elapsed();
        const Trip trip = tripOf(tripped);
        qInfo("signed in again from a second window while held: stopped %lld ms after the "
              "sign-in, %lld ms after the last keepalive; the first link %s, close reason "
              "\"%s\"",
              static_cast<long long>(stoppedAfterMs), static_cast<long long>(trip.silentMs),
              firstLinkOpenAtStop ? "open" : "closed", qPrintable(firstLinkCloseAtStop));
        QVERIFY(trip.linkClosed);
        QVERIFY2(trip.silentMs < RemoteTxWatchdog::kLinkCutMs,
                 qPrintable(QString::number(trip.silentMs)));
        QVERIFY2(stoppedAfterMs < RemoteTxWatchdog::kLinkCutMs / 2,
                 qPrintable(QString::number(stoppedAfterMs)));
        // The Core stopped the key before it closed the first link.
        QVERIFY(firstLinkOpenAtStop);
        // The first link was never closed by its window: open, or closed
        // only by the Core for the replacement.
        QVERIFY2(firstLinkCloseAtStop.isEmpty()
                     || firstLinkCloseAtStop == QStringLiteral("This device connected again."),
                 qPrintable(firstLinkCloseAtStop));
        QCOMPARE(ended.count(), 1);
        QCOMPARE(ended.first().at(2).value<RemoteTxWatchdog::HoldEnd>(),
                 RemoteTxWatchdog::HoldEnd::LinkClosed);
        QVERIFY(!h.station.moxController()->isMox());
        QCOMPARE(stopped.count(), 1);
        QCOMPARE(stopped.first().at(0).toString(), kStopSentence);
        QTRY_COMPARE(tx.rfHoldMask(), 0u);
        // The new session finds transmit stopped, with the reason (its
        // snapshot can come while MOX is still walking back to receive, so
        // keyed is waited for), and nothing keys again.
        QTRY_VERIFY(again.isHandshakeComplete());
        QTRY_COMPARE(again.transmitState()->stopSerial(), quint32(1));
        QCOMPARE(again.transmitState()->stopReason(), QStringLiteral("linkLost"));
        QTRY_VERIFY(!again.transmitState()->keyed());
        QVERIFY(!again.transmitState()->rfHeld());
        QTest::qWait(500);
        QVERIFY(!h.station.moxController()->isMox());
        QCOMPARE(tripped.count(), 1);
        QCOMPARE(stopped.count(), 1);
        if (vox) {
            QVERIFY(!h.station.transmitModel().voxEnabled());
            QVERIFY(h.server.voxArmedBy().isEmpty());
        }
        again.disconnectFromStation(QStringLiteral("test complete"));
        h.client.disconnectFromStation(QStringLiteral("test complete"));
        h.station.injectTxChannelForTest(nullptr);
    }

    // The session closing while keyed stops transmitting at once, before
    // the holder's normal unkey.
    void webSocketClosingStopsAtOnce()
    {
        Test::RemoteAudioSessionHarness h;
        h.pairWindow = true;
        h.makeTransmitReady();
        h.openFakeMicrophoneLine();   // fix wave C1: no media here
        h.connectSession();
        QTRY_VERIFY(h.client.capabilities().txPermitted);
        QSignalSpy tripped(h.server.txWatchdog(), &RemoteTxWatchdog::tripped);
        h.remote.setMoxFromButton(true);
        QTRY_VERIFY_WITH_TIMEOUT(h.station.moxController()->isMox(), 5000);
        QTest::qWait(300);

        // The Core's end closes (its socket saw the close): the stop runs
        // inside that close, not on a later turn.
        h.stationLink->closeLink(QStringLiteral("the app went away"));
        QCOMPARE(tripped.count(), 1);
        QVERIFY(tripOf(tripped).linkClosed);
        QVERIFY(!h.station.moxController()->isMox());
        QVERIFY(!h.server.txWatchdog()->isWatchingAny());
    }

    // Fix wave B, round 2: the session of a device whose VOX key is on the
    // air closes. The key is Stop All TX at once (transmitStopped, the lost
    // link's words), then its VOX goes off; VOX's own release (the normal
    // unkey, with the send ring's drain) never ends it.
    void aDroppedLinkWithAVoxKeyIsStopAllTx()
    {
        Test::RemoteAudioSessionHarness h;
        h.pairWindow = true;
        h.makeTransmitReady();
        attachMicrophone(h);
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
        DaemonMediaController daemonMedia(&h.server, &h.station);
        h.connectSession();
        QTRY_VERIFY_WITH_TIMEOUT(daemonMedia.micReceiver() != nullptr, 5000);
        QTRY_VERIFY(h.client.capabilities().txPermitted);
        QSignalSpy tripped(h.server.txWatchdog(), &RemoteTxWatchdog::tripped);
        QSignalSpy stopped(&h.station, &RadioModel::transmitStopped);
        QVERIFY(armVoxAndKey(h, daemonMedia));
        QTest::qWait(300);
        QCOMPARE(stopped.count(), 0);

        h.stationLink->closeLink(QStringLiteral("the app went away"));
        QCOMPARE(tripped.count(), 1);
        QVERIFY(tripOf(tripped).linkClosed);
        QCOMPARE(stopped.count(), 1);
        QVERIFY(!h.station.transmitModel().voxEnabled());
        QVERIFY(h.server.voxArmedBy().isEmpty());
        QTRY_VERIFY(!h.station.moxController()->isMox());
        QTest::qWait(300);
        QVERIFY(!h.station.moxController()->isMox());
        QCOMPARE(stopped.count(), 1);
    }

    // ---- The media connection's "tx" data channel (a direct pair) --------

    // Keyed with the media connection up, every keepalive goes on the "tx"
    // channel and on the session. The channel's path dies at the Core's
    // end and the session stays up: the session copy keeps the key, with
    // nothing held and nothing stopped over 4 s. (Every path going quiet
    // is everyPathSeveredHoldsThenCuts.)
    void dataChannelSeveredAloneKeepsTransmitting()
    {
        TxChannel tx{WdspEngine::kTxChannelId};   // outlives the harness
        Test::RemoteAudioSessionHarness h;
        h.station.injectTxChannelForTest(&tx);
        h.pairWindow = true;
        h.makeTransmitReady();
        attachMicrophone(h);
        QPointer<LibDataChannelMediaTransport> coreTransport;
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
        DaemonMediaController daemonMedia(
            &h.server, &h.station, nullptr, [&coreTransport](QObject* parent) {
                auto* transport = new LibDataChannelMediaTransport(parent);
                coreTransport = transport;
                return static_cast<IMediaTransport*>(transport);
            });
        h.connectSession();
        QTRY_VERIFY_WITH_TIMEOUT(daemonMedia.micReceiver() != nullptr, 5000);
        QTRY_VERIFY(h.client.capabilities().txPermitted);
        QVERIFY(remoteMedia.micLineNegotiated());
        QVERIFY(!coreTransport.isNull());
        RemoteTransmitClient* transmit = h.client.remoteTransmit();
        QSignalSpy tripped(h.server.txWatchdog(), &RemoteTxWatchdog::tripped);
        QSignalSpy held(h.server.txWatchdog(), &RemoteTxWatchdog::holdStarted);
        QSignalSpy channelHolds(&h.station, &RadioModel::remoteRfHoldChanged);
        QSignalSpy stopped(&h.station, &RadioModel::transmitStopped);
        // Give the "tx" channel time to open after media is ready.
        QTest::qWait(300);

        h.remote.setMoxFromButton(true);
        QTRY_VERIFY_WITH_TIMEOUT(h.station.moxController()->isMox(), 5000);
        QCOMPARE(h.station.keyedBy().deviceId, h.windowKey->fingerprint());
        QTest::qWait(2000);
        QVERIFY(h.station.moxController()->isMox());
        QCOMPARE(tripped.count(), 0);
        QVERIFY(transmit->channelKeepalivesSent() >= 15);
        QVERIFY(transmit->sessionKeepalivesSent() >= 15);
        QVERIFY(commandsOf(h.stationLink, QStringLiteral("tx.keepalive")) >= 15);

        // The media path dies at the Core's end: nothing arrives on it (the
        // channel keepalives and the microphone alike). The session copy
        // still arrives, so the key stays up, its RF never held, for 4 s.
        coreTransport->setReceiveSeveredForTest(true);
        QElapsedTimer quiet;
        quiet.start();
        while (quiet.elapsed() < 4000) {
            QTest::qWait(100);
            QVERIFY(h.station.moxController()->isMox());
            QCOMPARE(tx.rfHoldMask(), 0u);
        }
        QCOMPARE(tripped.count(), 0);
        QCOMPARE(held.count(), 0);
        QCOMPARE(channelHolds.count(), 0);
        QVERIFY(h.client.isHandshakeComplete());

        // USB: the microphone starved on the way (at 250 ms), which holds
        // and stops nothing in USB before the 10 s no-audio cut. The
        // session keepalive keeps the watchdog quiet, so a dead "tx"
        // channel alone neither holds nor stops.
        QCOMPARE(stopped.count(), 0);
        QCOMPARE(h.client.transmitState()->stopSerial(), quint32(0));
        QCOMPARE(h.client.transmitState()->rfHoldSerial(), quint64(0));
        QVERIFY(!h.client.transmitState()->rfHeld());
        QVERIFY(h.client.transmitState()->keyed());
        h.client.disconnectFromStation(QStringLiteral("test complete"));
        h.station.injectTxChannelForTest(nullptr);
    }

    // ---- Remote transmit cut off the main thread (2026-10-15) ------------

    // Keyed with the keepalives on the "tx" channel and the Core's cut
    // guard judging them, the Core's main thread is held for 3.5 s (the
    // window's keepalives stop with it: it runs on the same thread). Hold
    // then cut, off the main thread: the guard holds the TX channel's RF
    // 400 to 500 ms after the last "tx" message the Core received, MOX
    // still on, and cuts transmit 3000 to 3100 ms after it: MOX off
    // reaches the radio connection on its own thread while main is still
    // held. Once main runs, the model catches up: one stop with the
    // watchdog's sentence, one MOX-off edge, the window told linkLost, the
    // connection's cut and the hold cleared, and no key reached the
    // connection after the cut.
    void aStalledCoreStillCutsTransmitOffMain()
    {
        CutRig rig;
        QVERIFY(rig.keyOnTheTxChannel());
        Test::RemoteAudioSessionHarness& h = rig.h;
        QSignalSpy tripped(h.server.txWatchdog(), &RemoteTxWatchdog::tripped);
        QSignalSpy stopped(&h.station, &RadioModel::transmitStopped);
        QSignalSpy moxSpy(h.station.moxController(), &MoxController::moxStateChanged);
        const std::size_t mark = rig.conn.count();
        const quint64 armSerial = h.server.txCutGuard()->armSerial();
        QCOMPARE(rig.tx.rfHoldMask(), 0u);
        HoldMaskWatch guardHold(rig.tx, kGuardHoldBit);

        const Clock::time_point holdStart = Clock::now();
        std::this_thread::sleep_for(std::chrono::milliseconds(3500));
        // Before the main thread runs anything again.
        const Clock::time_point holdEnd = Clock::now();
        guardHold.stop();
        const Clock::time_point lastReceipt = h.server.lastTxChannelReceiptForTest();
        const std::vector<RecordingConnection::Entry> during = rig.conn.since(mark);
        const bool cutDuringHold = rig.conn.transmitCut();
        const bool judgingAfterHold = h.server.txCutGuard()->isJudging();

        QVERIFY2(guardHold.seen(), "the guard did not hold the RF while main was held");
        const double heldAfterReceiptMs = msBetween(lastReceipt, guardHold.firstAt());
        const RecordingConnection::Entry* const moxOff = findEntry(during, QStringLiteral("MOX off"));
        QVERIFY2(moxOff != nullptr, "no MOX off reached the connection while main was held");
        const double afterReceiptMs = msBetween(lastReceipt, moxOff->at);
        qInfo("main held 3500 ms: RF held %.1f ms and MOX off on the connection %.1f ms after "
              "the last tx message (%.1f ms into the hold), limits %lld and %lld",
              heldAfterReceiptMs, afterReceiptMs, msBetween(holdStart, moxOff->at),
              static_cast<long long>(kHoldBoundMs), static_cast<long long>(kCutBoundMs));
        QVERIFY2(heldAfterReceiptMs > double(RemoteTxWatchdog::kLinkHoldMs) - 1.0
                     && heldAfterReceiptMs <= double(kHoldBoundMs),
                 qPrintable(QString::number(heldAfterReceiptMs)));
        QVERIFY2(guardHold.firstAt() < moxOff->at, "the RF was not held before the cut");
        QCOMPARE(moxOff->thread, &rig.connThread);
        QVERIFY(moxOff->thread != QThread::currentThread());
        QVERIFY2(moxOff->at < holdEnd, "the cut waited for the main thread");
        QVERIFY2(afterReceiptMs > double(RemoteTxWatchdog::kLinkCutMs) - 1.0
                     && afterReceiptMs <= double(kCutBoundMs),
                 qPrintable(QString::number(afterReceiptMs)));
        QVERIFY(hasEntry(during, QStringLiteral("relay off")));
        QVERIFY(!hasEntry(during, QStringLiteral("MOX on")));
        QVERIFY(cutDuringHold);
        QVERIFY(!judgingAfterHold);   // the guard disarmed itself with its cut
        QCOMPARE(h.server.txCutGuard()->armSerial(), armSerial);

        // The main thread runs again: the model catches up.
        QTRY_VERIFY_WITH_TIMEOUT(!h.station.moxController()->isMox()
                                     && h.station.moxController()->state() == MoxState::Rx,
                                 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!rig.conn.transmitCut(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(stopped.count(), 1, 3000);
        QCOMPARE(stopped.first().at(0).toString(), kStopSentence);
        QTRY_COMPARE(h.client.transmitState()->stopSerial(), quint32(1));
        QCOMPARE(h.client.transmitState()->stopReason(), QStringLiteral("linkLost"));
        QCOMPARE(h.client.transmitState()->stopText(), kStopSentence);
        QCOMPARE(tripped.count(), 1);
        QVERIFY(!tripOf(tripped).linkClosed);

        // A second later: still off, nothing stopped or recorded twice, and
        // no key reached the connection after the cut.
        QTest::qWait(1000);
        QVERIFY(!h.station.moxController()->isMox());
        QCOMPARE(stopped.count(), 1);
        QCOMPARE(tripped.count(), 1);
        QCOMPARE(h.client.transmitState()->stopSerial(), quint32(1));
        int moxOffEdges = 0;
        for (const QList<QVariant>& args : moxSpy) {
            QCOMPARE(args.at(0).toBool(), false);
            ++moxOffEdges;
        }
        QCOMPARE(moxOffEdges, 1);
        QVERIFY2(!hasEntry(rig.conn.since(mark), QStringLiteral("MOX on")),
                 "a key reached the connection after the cut");
        QVERIFY(!rig.conn.transmitCut());
        QCOMPARE(rig.tx.rfHoldMask(), 0u);
    }

    // A hold of the Core's main thread, with the window's "tx" channel fed
    // from a thread of its own (the window's transport, while its main
    // thread is held too): the guard hears them off the main thread and
    // the key is kept. Nothing is held, nothing is cut, nothing stops, and
    // the window's own release unkeys.
    void aLiveChannelKeepsTheKeyWhenMainIsHeld()
    {
        CutRig rig;
        QVERIFY(rig.keyOnTheTxChannel());
        Test::RemoteAudioSessionHarness& h = rig.h;
        QSignalSpy tripped(h.server.txWatchdog(), &RemoteTxWatchdog::tripped);
        QSignalSpy stopped(&h.station, &RadioModel::transmitStopped);
        const std::size_t mark = rig.conn.count();
        const quint32 keyEpoch = h.station.keyedBy().epoch;
        LibDataChannelMediaTransport* const window = rig.windowTransport.data();

        // Sequences above anything the window's own keepalives reach in this
        // test, so the watchdog counts the fed ones when main runs again.
        // The feeder calls the window's sendTx off the transport's own
        // thread. That is safe here: sendTx only reads the transport's
        // members, and nothing writes them while the feeder runs (the
        // transport neither starts nor stops before the feeder is joined),
        // and the libdatachannel calls it makes are safe from any thread
        // (libdatachannel v0.24.5: DataChannel::isOpen reads atomics,
        // impl/datachannel.cpp:149; bufferedAmount is atomic,
        // impl/channel.hpp:43; DataChannel::outgoing sends under the
        // channel's shared lock, impl/datachannel.cpp:181-184).
        std::atomic<bool> feeding{true};
        std::atomic<int> fed{0};
        HoldMaskWatch anyHold(rig.tx, ~0u);
        std::thread feeder([window, keyEpoch, &feeding, &fed]() {
            quint64 sequence = 1'000'000;
            while (feeding.load()) {
                if (window->sendTx(RemoteTxWatchdog::channelKeepalive(++sequence, keyEpoch))) {
                    fed.fetch_add(1);
                }
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(RemoteTxWatchdog::kKeepaliveIntervalMs / 2));
            }
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        const std::vector<RecordingConnection::Entry> during = rig.conn.since(mark);
        const bool cutDuringHold = rig.conn.transmitCut();
        const bool judgingAfterHold = h.server.txCutGuard()->isJudging();
        const int fedDuringHold = fed.load();

        // Main runs again with the feeder still going; then the window
        // releases.
        QTest::qWait(300);
        const bool keyedAfter = h.station.moxController()->isMox();
        anyHold.stop();
        h.remote.setMoxFromButton(false);
        QTRY_VERIFY_WITH_TIMEOUT(!h.station.moxController()->isMox(), 5000);
        feeding.store(false);
        feeder.join();
        QVERIFY2(!anyHold.seen(), "the RF was held with the tx channel alive");

        qInfo("main held 1000 ms with the tx channel fed off main: %d fed during the hold",
              fedDuringHold);
        QVERIFY2(fedDuringHold >= 10, qPrintable(QString::number(fedDuringHold)));
        QVERIFY2(during.empty(), "the connection was written while main was held");
        QVERIFY(!cutDuringHold);
        QVERIFY(judgingAfterHold);
        QVERIFY(keyedAfter);
        QCOMPARE(tripped.count(), 0);
        for (const QList<QVariant>& args : stopped) {
            QVERIFY(args.at(0).toString() != kStopSentence);
        }
        QVERIFY(!rig.conn.transmitCut());
    }

    // The two orders after a stall, made deterministic: the guard's cut
    // (the model's cut line) and its post to the main thread
    // (onTransmitCutOffMain) run before the watchdog's own check. The post
    // trips the watchdog once (its stop and its record) and the watchdog no
    // longer watches the device, so the check that was due stops nothing
    // more; the connection's cut is cleared.
    // The cut's handling now runs one pass later, so which of the two comes
    // first is no longer forced; the one-stop assertions hold in either order.
    void aCutPostedBeforeTheWatchdogsCheckStopsOnce()
    {
        CutRig rig;
        QVERIFY(rig.keyOnTheTxChannel());
        Test::RemoteAudioSessionHarness& h = rig.h;
        QSignalSpy tripped(h.server.txWatchdog(), &RemoteTxWatchdog::tripped);
        QSignalSpy stopped(&h.station, &RadioModel::transmitStopped);
        const QByteArray device = h.windowKey->fingerprint();
        const quint64 mediaEpoch = h.server.mediaEpochOfDevice(device);
        QVERIFY(mediaEpoch != 0);

        h.station.transmitCutLine()->cut();
        QVERIFY(rig.conn.transmitCut());
        h.server.onTransmitCutOffMain(mediaEpoch, h.server.txCutGuard()->armSerial(), 450);
        // Its handling runs one event-loop turn later.
        QTRY_COMPARE_WITH_TIMEOUT(tripped.count(), 1, 3000);
        QVERIFY(!tripOf(tripped).linkClosed);
        QVERIFY(!h.server.txWatchdog()->isWatching(device));
        QTRY_VERIFY_WITH_TIMEOUT(!h.station.moxController()->isMox(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!rig.conn.transmitCut(), 3000);
        QTRY_COMPARE(h.client.transmitState()->stopSerial(), quint32(1));
        QCOMPARE(h.client.transmitState()->stopReason(), QStringLiteral("linkLost"));

        QTest::qWait(1000);
        QCOMPARE(tripped.count(), 1);
        QCOMPARE(stopped.count(), 1);
        QCOMPARE(stopped.first().at(0).toString(), kStopSentence);
        QCOMPARE(h.client.transmitState()->stopSerial(), quint32(1));
    }

    // The other order: the watchdog trips first (its stop and its record);
    // the guard's post that follows stops and records nothing more, and
    // only clears the connection's cut.
    void aCutPostedAfterTheWatchdogsTripStopsNothingMore()
    {
        CutRig rig;
        QVERIFY(rig.keyOnTheTxChannel());
        Test::RemoteAudioSessionHarness& h = rig.h;
        QSignalSpy tripped(h.server.txWatchdog(), &RemoteTxWatchdog::tripped);
        QSignalSpy stopped(&h.station, &RadioModel::transmitStopped);
        const QByteArray device = h.windowKey->fingerprint();
        const quint64 mediaEpoch = h.server.mediaEpochOfDevice(device);
        QVERIFY(mediaEpoch != 0);
        const quint64 armSerial = h.server.txCutGuard()->armSerial();

        h.station.transmitCutLine()->cut();
        QVERIFY(rig.conn.transmitCut());
        // The watchdog's check, overdue after the stall, runs first (the
        // same trip as its timer's).
        QVERIFY(h.server.txWatchdog()->tripQuiet(device));
        QCOMPARE(stopped.count(), 1);
        h.server.onTransmitCutOffMain(mediaEpoch, armSerial, 450);
        QCOMPARE(tripped.count(), 1);
        QTRY_VERIFY_WITH_TIMEOUT(!h.station.moxController()->isMox(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!rig.conn.transmitCut(), 3000);
        QTRY_COMPARE(h.client.transmitState()->stopSerial(), quint32(1));

        QTest::qWait(1000);
        QCOMPARE(tripped.count(), 1);
        QCOMPARE(stopped.count(), 1);
        QCOMPARE(stopped.first().at(0).toString(), kStopSentence);
        QCOMPARE(h.client.transmitState()->stopSerial(), quint32(1));
        QCOMPARE(h.client.transmitState()->stopReason(), QStringLiteral("linkLost"));
    }

    // The window releases while the Core's main thread is held (3.5 s): its
    // keepalives stop with the release, which waits on the session link
    // (only the Core's main thread reads it), so the guard holds the RF and
    // then cuts while main is held, as it must for RF. The cut's post runs before the Core
    // reads the release; its handling waits one turn, reads the release
    // first, and records nothing: one MOX-off edge, no link-lost stop, the
    // window never told linkLost, VOX untouched, the connection's cut
    // cleared, and the next key reaches the connection.
    void aReleaseReadAfterTheCutIsNotALostLink()
    {
        CutRig rig;
        QVERIFY(rig.keyOnTheTxChannel());
        Test::RemoteAudioSessionHarness& h = rig.h;
        Test::LoopbackTransport* const windowEnd = h.stationLink->peerForTest();
        QVERIFY(windowEnd != nullptr);
        QSignalSpy tripped(h.server.txWatchdog(), &RemoteTxWatchdog::tripped);
        QSignalSpy stopped(&h.station, &RadioModel::transmitStopped);
        QSignalSpy moxSpy(h.station.moxController(), &MoxController::moxStateChanged);
        const quint32 stopSerialBefore = h.client.transmitState()->stopSerial();
        const std::size_t mark = rig.conn.count();

        // The release leaves the window and waits on the link, as it does
        // behind a stalled Core's socket.
        windowEnd->setHoldsOutgoing(true);
        h.remote.setMoxFromButton(false);
        std::this_thread::sleep_for(std::chrono::milliseconds(3500));
        // Before the main thread runs anything again.
        const bool cutDuringHold = rig.conn.transmitCut();
        const std::vector<RecordingConnection::Entry> during = rig.conn.since(mark);
        // The release reaches the Core behind the cut's post.
        windowEnd->setHoldsOutgoing(false);

        QVERIFY2(cutDuringHold, "the guard did not cut while main was held");
        QVERIFY(hasEntry(during, QStringLiteral("MOX off")));
        QTRY_VERIFY_WITH_TIMEOUT(!h.station.moxController()->isMox()
                                     && h.station.moxController()->state() == MoxState::Rx,
                                 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!rig.conn.transmitCut(), 3000);
        QTRY_VERIFY(!h.client.transmitState()->keyed());

        // A second later: nothing recorded as a lost link.
        QTest::qWait(1000);
        QCOMPARE(tripped.count(), 0);
        QCOMPARE(stopped.count(), 0);
        int moxOffEdges = 0;
        for (const QList<QVariant>& args : moxSpy) {
            QCOMPARE(args.at(0).toBool(), false);
            ++moxOffEdges;
        }
        QCOMPARE(moxOffEdges, 1);
        QCOMPARE(h.client.transmitState()->stopSerial(), stopSerialBefore);
        QVERIFY(h.client.transmitState()->stopReason() != QLatin1String("linkLost"));
        // tripped == 0 carries the VOX property: VOX is disarmed only by the
        // watchdog's stop.
        QVERIFY(!rig.conn.transmitCut());

        // The next key reaches the connection.
        const std::size_t beforeKey = rig.conn.count();
        h.remote.setMoxFromButton(true);
        QTRY_VERIFY_WITH_TIMEOUT(hasEntry(rig.conn.since(beforeKey), QStringLiteral("MOX on")),
                                 5000);
        QVERIFY(h.station.moxController()->isMox());
        h.remote.setMoxFromButton(false);
        QTRY_VERIFY_WITH_TIMEOUT(!h.station.moxController()->isMox(), 5000);
    }

    // ---- The Part E paths --------------------------------------------------

    void turnUdpSeveredHoldsThenCuts()
    {
        // The transports are built (Tasks 27 to 29) and the service runs on
        // this computer (RendezvousTestHarness with fake_turn_server.py), but
        // nothing here can yet cut a live TURN path: the fake TURN server
        // relays every allocation until it exits, and this file's session
        // (RemoteAudioSessionHarness) runs over the loopback link, not the
        // service. The Docker traversal harness keys over its paths only to
        // measure false stops (web-relay-deadline, direct-wss-deadline), not
        // to sever one with the key held.
        QSKIP("Through TURN over UDP: needs a keyable Core and window session through the "
              "service on a relayed pair, and a way to stop the fake TURN server relaying "
              "that pair while the key is held.");
    }

    void relayFloorSeveredHoldsThenCuts()
    {
        // The relay leg is built (RelayLeg, Task 29 step 2b) and
        // tst_relay_session runs a whole session over it through a stand-in
        // relay, but that stand-in is private to that test and forwards
        // until it is destroyed, so nothing can cut the floor while a key
        // is held; this file's session does not use the service.
        QSKIP("Through the relay floor: needs a keyable Core and window session over the web "
              "relay leg, and a shared stand-in relay that can stop forwarding while the key "
              "is held.");
    }

    // ---- VOX a device armed -------------------------------------------------

    // The window tries to arm the Core's VOX with no microphone line. Fix
    // wave M8: VOX could never key from it, so the Core refuses the arming
    // with the reason instead of holding VOX armed and silent; nothing is
    // watched.
    void voxArmedByADeviceNeverKeysTheCoresOwnMicrophone()
    {
        Test::RemoteAudioSessionHarness h;
        h.pairWindow = true;
        h.makeTransmitReady();
        h.openFakeMicrophoneLine();   // fix wave C1: no media here
        h.connectSession();
        QTRY_VERIFY(h.client.capabilities().txPermitted);
        QSignalSpy completed(&h.client, &StationClient::propertyWriteCompleted);

        // iPhone app plan Task 77 (ruling 8.4): arming VOX needs holding
        // transmit; the window takes it first (on unheld transmit).
        {
            TransmitHolder::KeyRequest take;
            take.deviceId = h.windowKey->fingerprint();
            QCOMPARE(h.server.transmitHolder()->askKey(take).verdict, KeyingVerdict::Admit);
        }
        h.remote.transmitModel().setVoxEnabled(true);
        QTRY_VERIFY(!completed.isEmpty());
        bool refusedWithReason = false;
        for (const QList<QVariant>& call : completed) {
            if (call.at(1).toByteArray() == "voxEnabled" && !call.at(3).toBool()
                && call.at(4).toString() == TxRefusals::micNotConnected().text) {
                refusedWithReason = true;
            }
        }
        QVERIFY(refusedWithReason);
        QVERIFY(!h.station.transmitModel().voxEnabled());
        QTRY_VERIFY(!h.remote.transmitModel().voxEnabled());
        QVERIFY(h.server.voxArmedBy().isEmpty());
        QVERIFY(!h.server.txWatchdog()->isWatchingAny());

        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // VOX armed with the line, the link goes quiet: an unkeyed watch is
    // never held, and VOX goes off at the cut, 3000 to 3100 ms after the
    // last keepalive.
    void voxArmedGoesOffWhenTheLinkGoesQuiet()
    {
        Test::RemoteAudioSessionHarness h;
        h.pairWindow = true;
        h.makeTransmitReady();
        attachMicrophone(h);
        QPointer<LibDataChannelMediaTransport> coreTransport;
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
        DaemonMediaController daemonMedia(
            &h.server, &h.station, nullptr, [&coreTransport](QObject* parent) {
                auto* transport = new LibDataChannelMediaTransport(parent);
                coreTransport = transport;
                return static_cast<IMediaTransport*>(transport);
            });
        h.connectSession();
        QTRY_VERIFY_WITH_TIMEOUT(daemonMedia.micReceiver() != nullptr, 5000);
        QTRY_VERIFY(h.client.capabilities().txPermitted);
        QSignalSpy tripped(h.server.txWatchdog(), &RemoteTxWatchdog::tripped);
        QSignalSpy held(h.server.txWatchdog(), &RemoteTxWatchdog::holdStarted);

        // iPhone app plan Task 77 (ruling 8.4): arming VOX needs holding
        // transmit; the window takes it first (on unheld transmit).
        {
            TransmitHolder::KeyRequest take;
            take.deviceId = h.windowKey->fingerprint();
            QCOMPARE(h.server.transmitHolder()->askKey(take).verdict, KeyingVerdict::Admit);
        }
        h.remote.transmitModel().setVoxEnabled(true);
        QTRY_VERIFY(h.station.transmitModel().voxEnabled());
        QTRY_COMPARE(h.station.remoteVoxDevice(), h.windowKey->fingerprint());
        QTest::qWait(1000);
        QCOMPARE(tripped.count(), 0);

        // The whole path dies: the session's and the media connection's.
        h.stationLink->setSevered(true);
        coreTransport->setReceiveSeveredForTest(true);
        QTRY_COMPARE_WITH_TIMEOUT(tripped.count(), 1, 5000);
        const Trip trip = tripOf(tripped);
        qInfo("VOX armed, link quiet: off %lld ms after the last keepalive",
              static_cast<long long>(trip.silentMs));
        QVERIFY2(trip.silentMs > RemoteTxWatchdog::kLinkCutMs && trip.silentMs <= kCutBoundMs,
                 qPrintable(QString::number(trip.silentMs)));
        QVERIFY(!h.station.transmitModel().voxEnabled());
        QVERIFY(h.server.voxArmedBy().isEmpty());
        QVERIFY(!h.station.moxController()->isMox());
        QCOMPARE(held.count(), 0);
    }

    // ---- The microphone starving on a live link ---------------------------

    void starvationOnALiveLinkHoldsAmThenCutsEveryMode_data()
    {
        QTest::addColumn<int>("mode");
        QTest::addColumn<bool>("holds");
        QTest::addColumn<bool>("dropMedia");
        QTest::addColumn<bool>("newLineDelivers");
        QTest::newRow("AM holds, then cuts")
            << static_cast<int>(DSPMode::AM) << true << false << false;
        QTest::newRow("USB stays keyed, then cuts")
            << static_cast<int>(DSPMode::USB) << false << false << false;
        QTest::newRow("AM, media dropped while held, the new line never delivers")
            << static_cast<int>(DSPMode::AM) << true << true << false;
        QTest::newRow("USB, media dropped, the new line never delivers")
            << static_cast<int>(DSPMode::USB) << false << true << false;
        QTest::newRow("AM, media dropped while held, the new line delivers at 5 s")
            << static_cast<int>(DSPMode::AM) << true << true << true;
    }

    // The keyed window's microphone stops on a live link (the keepalives
    // go on). AM holds the RF within 300 ms of the last microphone packet
    // the Core received and USB does not; with no audio for 10 s, both stop
    // transmitting with the microphone's reason. Fix wave B (review
    // Important 2): the window's media dropped between the hold and the cut
    // (the Core's microphone line torn down, and a new one opened when the
    // window's media recovers) is no audio: the hold stays on, and the cut
    // still lands 10 s after the last packet of the first line. Round 2:
    // the new line opening is checked; when the window's microphone gives
    // audio again at 5 s, the new line's first packet releases the hold and
    // the cut never comes.
    void starvationOnALiveLinkHoldsAmThenCutsEveryMode()
    {
        QFETCH(int, mode);
        QFETCH(bool, holds);
        QFETCH(bool, dropMedia);
        QFETCH(bool, newLineDelivers);
        Test::RemoteAudioSessionHarness h;
        h.pairWindow = true;
        h.makeTransmitReady();
        h.station.sliceById(h.sliceA)->setDspMode(static_cast<DSPMode>(mode));
        const auto giving = attachMicrophone(h);
        auto remoteMedia =
            std::make_unique<RemoteMediaController>(&h.client, &h.remote, nullptr);
        DaemonMediaController daemonMedia(&h.server, &h.station);
        h.connectSession();
        QTRY_VERIFY_WITH_TIMEOUT(daemonMedia.micReceiver() != nullptr, 5000);
        QTRY_VERIFY(h.client.capabilities().txPermitted);
        RemoteMicReceiver* const receiver = daemonMedia.micReceiver();
        QSignalSpy stopped(&h.station, &RadioModel::transmitStopped);
        QSignalSpy starved(receiver, &RemoteMicReceiver::starved);
        QSignalSpy linkHolds(h.server.txWatchdog(), &RemoteTxWatchdog::holdStarted);
        QTest::qWait(300);

        h.remote.setMoxFromButton(true);
        QTRY_VERIFY_WITH_TIMEOUT(h.station.moxController()->isMox(), 5000);
        QTest::qWait(500);
        QVERIFY(h.station.moxController()->isMox());
        QVERIFY(!h.station.isRemoteRfHeld());

        // The microphone stops giving audio; the link (and the keepalives)
        // stay up. The Core's last packet is found by watching its count,
        // and the hold and the stop are timed on the same clock.
        QElapsedTimer quiet;
        qint64 heldMs = -1;
        qint64 stoppedMs = -1;
        int holdEdges = 0;
        QObject scope;   // last: disconnects before the values go
        QObject::connect(&h.station, &RadioModel::remoteRfHoldChanged, &scope, [&](bool on) {
            ++holdEdges;
            if (on && heldMs < 0) {
                heldMs = quiet.elapsed();
            }
        });
        QObject::connect(&h.station, &RadioModel::transmitStopped, &scope, [&](const QString&) {
            if (stoppedMs < 0) {
                stoppedMs = quiet.elapsed();
            }
        });
        // The line in use now (a dropped media connection replaces it), and
        // its accepted count.
        QPointer<RemoteMicReceiver> line(receiver);
        const QPointer<RemoteMicReceiver> firstLine(receiver);
        quint64 accepted = receiver->stats().accepted;
        qint64 lastPacketMs = 0;
        qint64 droppedMs = -1;
        bool heldAfterDrop = false;
        bool heldWhenLineGone = !holds;
        qint64 newLineMs = -1;
        bool restarted = false;
        qint64 givingAgainMs = -1;
        qint64 releasedMs = -1;
        QObject::connect(&h.station, &RadioModel::remoteRfHoldChanged, &scope, [&](bool on) {
            if (!on && releasedMs < 0 && givingAgainMs >= 0) {
                releasedMs = quiet.elapsed();
            }
        });
        constexpr qint64 kAudioAgainMs = 5000;
        const qint64 deadline =
            RemoteMicConfig::kNoAudioCutMs + (newLineDelivers ? 1500 : 5000);
        giving->store(false);
        quiet.start();
        while (h.station.moxController()->isMox() && quiet.elapsed() < deadline) {
            RemoteMicReceiver* const current = daemonMedia.micReceiver();
            if (current != line.data()) {
                line = current;
                accepted = current != nullptr ? current->stats().accepted : 0;
            }
            if (newLineMs < 0 && current != nullptr && firstLine.isNull()) {
                newLineMs = quiet.elapsed();
            }
            if (newLineDelivers && givingAgainMs < 0 && quiet.elapsed() >= kAudioAgainMs) {
                givingAgainMs = quiet.elapsed();
                giving->store(true);
            }
            if (!line.isNull()) {
                const quint64 now = line->stats().accepted;
                if (now != accepted) {
                    accepted = now;
                    lastPacketMs = quiet.elapsed();
                }
            }
            // Between the hold and the cut: the window's media goes.
            if (dropMedia && droppedMs < 0 && quiet.elapsed() >= 1500
                && (!holds || heldMs >= 0)) {
                droppedMs = quiet.elapsed();
                remoteMedia->dropMediaPeerForTest();
            }
            // The window starts its media again half a second later (what
            // its media recovery does; the harness has no connection
            // controller to run it): a new media connection, and the Core
            // opens a new microphone line on it.
            if (droppedMs >= 0 && !restarted && quiet.elapsed() >= droppedMs + 500) {
                restarted = true;
                remoteMedia = std::make_unique<RemoteMediaController>(&h.client, &h.remote,
                                                                      nullptr);
                remoteMedia->planNowForTest();
            }
            if (droppedMs >= 0 && firstLine.isNull() && !heldAfterDrop) {
                heldAfterDrop = true;
                heldWhenLineGone = h.station.isRemoteRfHeld() == holds;
            }
            QTest::qWait(2);
        }
        qInfo("microphone silent: last packet at %lld ms, held at %lld ms, media dropped at "
              "%lld ms, new line at %lld ms, audio again at %lld ms, released at %lld ms, "
              "stopped at %lld ms",
              static_cast<long long>(lastPacketMs), static_cast<long long>(heldMs),
              static_cast<long long>(droppedMs), static_cast<long long>(newLineMs),
              static_cast<long long>(givingAgainMs), static_cast<long long>(releasedMs),
              static_cast<long long>(stoppedMs));
        QVERIFY(!starved.isEmpty());
        if (dropMedia) {
            QVERIFY2(droppedMs >= 0, "the media was never dropped");
            QVERIFY2(firstLine.isNull(), "the Core's first microphone line was never torn down");
            QVERIFY2(heldWhenLineGone, "the hold changed when the microphone line went");
            QVERIFY2(newLineMs >= 0, "no new microphone line opened");
        }
        if (newLineDelivers) {
            // The new line's audio released the hold; the cut never came.
            QVERIFY2(releasedMs >= givingAgainMs && releasedMs <= givingAgainMs + 2000,
                     qPrintable(QString::number(releasedMs)));
            QVERIFY(h.station.moxController()->isMox());
            QCOMPARE(stoppedMs, qint64(-1));
            QCOMPARE(stopped.count(), 0);
            QCOMPARE(holdEdges, 2);   // on, then off as audio returned
            QVERIFY(!h.station.isRemoteRfHeld());
            QCOMPARE(h.client.transmitState()->stopSerial(), quint32(0));
            QTRY_COMPARE(h.client.transmitState()->rfHoldSerial(), quint64(1));
            h.remote.setMoxFromButton(false);
            QTRY_VERIFY_WITH_TIMEOUT(!h.station.moxController()->isMox(), 5000);
            QCOMPARE(stopped.count(), 0);
            h.client.disconnectFromStation(QStringLiteral("test complete"));
            return;
        }
        QVERIFY(!h.station.moxController()->isMox());
        if (dropMedia) {
            QVERIFY(droppedMs < stoppedMs);
        }
        if (holds) {
            QVERIFY2(heldMs >= 0, "AM was not held");
            QVERIFY2(heldMs - lastPacketMs <= 300,
                     qPrintable(QString::number(heldMs - lastPacketMs)));
            QVERIFY(heldMs < stoppedMs);
            QCOMPARE(holdEdges, 2);   // on, then off at the cut
        } else {
            QCOMPARE(heldMs, qint64(-1));
            QCOMPARE(holdEdges, 0);
        }
        QCOMPARE(linkHolds.count(), 0);
        // The count is read every few ms, so the last packet is seen up to
        // that much after the Core received it, more under load (round 2:
        // one read 23 ms late at load 18): 250 ms of room.
        QVERIFY2(stoppedMs - lastPacketMs >= RemoteMicConfig::kNoAudioCutMs - 250
                     && stoppedMs - lastPacketMs <= RemoteMicConfig::kNoAudioCutMs + 500,
                 qPrintable(QString::number(stoppedMs - lastPacketMs)));
        QCOMPARE(stopped.count(), 1);
        const QString sentence =
            QStringLiteral("No microphone audio arrived from Shack MacBook, so the Core "
                           "stopped transmitting.");
        QCOMPARE(stopped.first().at(0).toString(), sentence);
        // Merge of Tasks 37 to 39: the window is told why on txState.
        QTRY_COMPARE(h.client.transmitState()->stopSerial(), quint32(1));
        QCOMPARE(h.client.transmitState()->stopReason(), QStringLiteral("micStarved"));
        QCOMPARE(h.client.transmitState()->stopText(), sentence);
        QCOMPARE(h.client.transmitState()->rfHoldSerial(), quint64(holds ? 1 : 0));
        QVERIFY(!h.client.transmitState()->rfHeld());
        QVERIFY(!h.station.isRemoteRfHeld());
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // ---- A move of the session (Task 29 fix wave, review Important 2) ----

    // A key the window takes after the Core accepted the join and before
    // its barrier: the Core takes it on the old connection, the move
    // finishes, the keepalives go on the new one in order, and the watchdog
    // never trips while the key is held; nothing keys but the window's
    // press, and the release unkeys.
    void aKeyDuringAMoveRidesAcrossIt()
    {
        Test::RemoteAudioSessionHarness h;
        h.pairWindow = true;
        h.makeTransmitReady();
        h.openFakeMicrophoneLine();
        h.connectSession();
        QTRY_VERIFY(h.client.capabilities().txPermitted);
        QSignalSpy tripped(h.server.txWatchdog(), &RemoteTxWatchdog::tripped);
        QSignalSpy moved(&h.client, &StationClient::pathChanged);
        QVERIFY(!h.station.moxController()->isMox());

        MoveWhileKeying move;
        QVERIFY(move.open(h, /*severOld=*/false));
        QVERIFY(h.client.moveSessionForTest(move.clientB, PathRacer::ThisNetwork));
        QTRY_COMPARE_WITH_TIMEOUT(moved.size(), 1, 5000);
        QVERIFY(move.keyed);
        QTRY_VERIFY_WITH_TIMEOUT(h.station.moxController()->isMox(), 5000);
        QCOMPARE(h.server.sessionsMoved(), 1);

        // Two seconds keyed on the new connection: its keepalives hold it.
        QTest::qWait(2000);
        QVERIFY(h.station.moxController()->isMox());
        QCOMPARE(tripped.count(), 0);
        const QList<qint64> onOld = keepaliveSequences(move.readOnOld);
        const QList<qint64> onNew = keepaliveSequences(move.readOnNew);
        QVERIFY2(onNew.size() >= 15, qPrintable(QString::number(onNew.size())));
        QVERIFY(strictlyRising(onOld));
        QVERIFY(strictlyRising(onNew));
        // Re-review: the before-barrier case is always covered: the key's
        // first keepalive went on the old connection.
        QVERIFY(!onOld.isEmpty());
        QVERIFY(onOld.last() < onNew.first());

        h.remote.setMoxFromButton(false);
        QTRY_VERIFY_WITH_TIMEOUT(!h.station.moxController()->isMox(), 5000);
        QCOMPARE(tripped.count(), 0);
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // The Core's barrier is lost on the old connection, so the window
    // never answers it. Link 21.2 (LINK-I2, 5e4412593): a Core that hears
    // no path.switch from the device within its move deadline ends the
    // session as a lost link does, and a holder whose link drops is unkeyed
    // at once (section 18.2). A key taken during the move is held, its
    // keepalives on the old connection and the watchdog quiet, right up to
    // that end: the move's wait never drops it, and the session's end
    // stops it at once as a closed link.
    void theStationsMoveDeadlineHoldsAKeyUntilItEndsTheSession()
    {
        Test::RemoteAudioSessionHarness h;
        h.pairWindow = true;
        h.makeTransmitReady();
        h.openFakeMicrophoneLine();
        h.server.setPathSwitchDeadlineMsForTest(1500);
        h.connectSession();
        QTRY_VERIFY(h.client.capabilities().txPermitted);
        QSignalSpy tripped(h.server.txWatchdog(), &RemoteTxWatchdog::tripped);
        QSignalSpy stopped(&h.station, &RadioModel::transmitStopped);

        // The moment the move's deadline ends the session, the Core closes
        // the old connection first, before the session's own close stops
        // transmitting: what transmit looked like then.
        bool endSeen = false;
        bool moxAtEnd = false;
        qsizetype tripsAtEnd = -1;
        QString endReason;
        QObject::connect(h.stationLink, &SessionTransport::closed, h.stationLink,
                         [&h, &tripped, &endSeen, &moxAtEnd, &tripsAtEnd, &endReason] {
            endSeen = true;
            moxAtEnd = h.station.moxController()->isMox();
            tripsAtEnd = tripped.count();
            endReason = h.stationLink->closeReason();
        });

        MoveWhileKeying move;
        QVERIFY(move.open(h, /*severOld=*/true));
        QVERIFY(h.client.moveSessionForTest(move.clientB, PathRacer::ThisNetwork));
        QTRY_VERIFY_WITH_TIMEOUT(move.keyed, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(h.station.moxController()->isMox(), 5000);

        // The station's deadline ends the session, and transmit with it.
        QTRY_VERIFY_WITH_TIMEOUT(!h.station.moxController()->isMox(), 5000);
        QVERIFY(endSeen);
        QCOMPARE(endReason, QStringLiteral("no path.switch from the device in time"));
        QVERIFY(moxAtEnd);
        QCOMPARE(tripsAtEnd, qsizetype(0));
        // The Core took the join and began the move (sessionsMoved counts
        // joins taken); the device's barrier is what never came.
        QCOMPARE(h.server.sessionsMoved(), 1);
        QCOMPARE(tripped.count(), 1);
        QVERIFY(tripOf(tripped).linkClosed);
        QCOMPARE(stopped.count(), 1);
        QCOMPARE(stopped.first().at(0).toString(), kStopSentence);
        QVERIFY(!h.server.txWatchdog()->isWatchingAny());
        // The keepalives that held the key reached the Core on the old
        // connection, in order, through the whole wait.
        const QList<qint64> onOld = keepaliveSequences(move.readOnOld);
        QVERIFY2(onOld.size() >= 10, qPrintable(QString::number(onOld.size())));
        QVERIFY(strictlyRising(onOld));

        h.remote.setMoxFromButton(false);
        QVERIFY(!h.station.moxController()->isMox());
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // A move with nothing keyed never keys: the Core's MOX stays off
    // throughout, and the watchdog watches nobody.
    void aMoveNeverKeys()
    {
        Test::RemoteAudioSessionHarness h;
        h.pairWindow = true;
        h.makeTransmitReady();
        h.openFakeMicrophoneLine();
        h.connectSession();
        QTRY_VERIFY(h.client.capabilities().txPermitted);
        QSignalSpy moxChanges(h.station.moxController(), &MoxController::stateChanged);
        QSignalSpy moved(&h.client, &StationClient::pathChanged);
        auto* stationB = new Test::LoopbackTransport(QStringLiteral("station B"));
        auto* clientB = new Test::LoopbackTransport(QStringLiteral("client B"));
        stationB->linkTo(clientB);
        QList<QByteArray> hello;
        QObject::connect(clientB, &SessionTransport::textReceived, clientB,
                         [&hello](const QByteArray& wire) { hello.append(wire); });
        h.server.acceptTransport(stationB);
        QTRY_VERIFY(!hello.isEmpty());
        QObject::disconnect(clientB, &SessionTransport::textReceived, nullptr, nullptr);
        QVERIFY(h.client.moveSessionForTest(clientB, PathRacer::ThisNetwork));
        QTRY_COMPARE_WITH_TIMEOUT(moved.size(), 1, 5000);
        QTest::qWait(500);
        QCOMPARE(moxChanges.count(), 0);
        QVERIFY(!h.station.moxController()->isMox());
        QVERIFY(!h.server.txWatchdog()->isWatchingAny());
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    TestTxLinkLossEachPath test;
    QTEST_SET_MAIN_SOURCE_PATH
    // Whole functions keep every data row together; future functions join
    // rest. Both entries retain the real-time checks and 120 s limit.
    const std::optional<QStringList> arguments = NereusSDR::TestFunctionGroups::arguments(
        test.metaObject(), app.arguments(), "NEREUS_TX_LINK_LOSS_GROUP",
        {{QStringLiteral("starvation"),
          {QStringLiteral("starvationOnALiveLinkHoldsAmThenCutsEveryMode")}}});
    return arguments ? QTest::qExec(&test, *arguments) : 1;
}
#include "tst_tx_link_loss_each_path.moc"
