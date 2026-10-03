// no-port-check: NereusSDR-original. Control logging lane: the Core logs
// each control write and command a device sends, each answer with how long
// the Core took, the gaps between a device's control messages, the link it
// runs on, transmit keepalive gaps per channel and watchdog stops, with
// names a device sends checked and the volume bounded. Logging only.
// Loopback link, no radio, no audio device, never keyed.
// =================================================================
// Modification history (NereusSDR):
//   2026-10-01  J.J. Boyd / KG4VCF  Created (control logging lane).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-10-01  J.J. Boyd / KG4VCF  Fix round: ControlLog on its own (names
//                                    checked, receipt times, token buckets,
//                                    the Core-wide cap, folded answers,
//                                    reused ids, the link, keepalive gaps,
//                                    watchdog stops) and through the
//                                    station (sign-in, close, heartbeat).
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest>

#include <QCoreApplication>
#include <QFile>
#include <QRegularExpression>
#include <QTemporaryDir>

#include <memory>
#include <optional>

#include "core/AppSettings.h"
#include "core/ConnectionState.h"
#include "core/HardwareProfile.h"
#include "core/session/ControlLog.h"
#include "core/session/SessionMessages.h"
#include "core/session/StationServer.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/UpgradedCoreToken.h"
#include "models/RadioModel.h"

using namespace NereusSDR;
using NereusSDR::Test::LoopbackTransport;

namespace {

std::unique_ptr<RadioModel> makeStationRadioModel()
{
    auto model = std::make_unique<RadioModel>();
    model->setBoardForTest(HPSDRHW::HermesLite);
    RadioInfo info;
    info.macAddress = QStringLiteral("AA:BB:CC:DD:EE:01");
    info.name = QStringLiteral("Bench HL2");
    info.boardType = HPSDRHW::HermesLite;
    model->setLastRadioInfoForTest(info);
    model->setConnectionStateForTest(ConnectionState::Connected);
    model->addSlice(QStringLiteral("pan-0"));
    return model;
}

// A transport whose link figures a test sets.
class ProbeTransport : public LoopbackTransport {
public:
    using LoopbackTransport::LoopbackTransport;
    qint64 backlogBytes() const override { return 1234; }
    SessionLinkDiagnostics linkDiagnostics() const override
    {
        if (diagnostics) {
            return *diagnostics;
        }
        return LoopbackTransport::linkDiagnostics();
    }
    std::optional<qint64> deliveringMessageWaitUs() const override { return waitUs; }
    std::optional<NetworkPathSnapshot> networkPathSnapshot() const override { return path; }

    std::optional<SessionLinkDiagnostics> diagnostics;
    std::optional<qint64> waitUs;
    std::optional<NetworkPathSnapshot> path;
};

// Collects the control log's lines.
class LogCapture {
public:
    LogCapture()
    {
        s_lines.clear();
        s_previous = qInstallMessageHandler(&LogCapture::handle);
    }
    ~LogCapture() { qInstallMessageHandler(s_previous); }
    QStringList lines(const QString& prefix) const
    {
        QStringList out;
        for (const QString& line : std::as_const(s_lines)) {
            if (line.startsWith(prefix)) {
                out.append(line);
            }
        }
        return out;
    }
    QStringList all() const { return s_lines; }
    void clear() { s_lines.clear(); }

private:
    static void handle(QtMsgType type, const QMessageLogContext& context, const QString& msg)
    {
        if (msg.startsWith(QStringLiteral("Control ")) || msg.startsWith(QStringLiteral("Keepalive "))
            || msg.startsWith(QStringLiteral("Transmit watchdog stop "))) {
            s_lines.append(msg);
            return;
        }
        if (s_previous) {
            s_previous(type, context, msg);
        }
    }
    static inline QStringList s_lines;
    static inline QtMessageHandler s_previous = nullptr;
};

struct Device {
    Device(StationServer* server, QObject* parent, bool signIn = true)
    {
        app = new LoopbackTransport(QStringLiteral("app"), parent);
        station = new ProbeTransport(QStringLiteral("station"), server);
        station->linkTo(app);
        server->acceptTransport(station);
        static_cast<void>(QTest::qWaitFor([this]() { return !app->received().isEmpty(); }, 5000));
        app->sendText(SessionMessages::encode(SessionMessages::hello(
            kSessionProtocolMajor, kSessionProtocolMinor, 0, QStringLiteral("Log phone"),
            {kSessionProtocolMajor}, {})));
        if (signIn) {
            app->sendText(SessionMessages::encode(SessionMessages::authRequest(server->token())));
        }
    }
    bool ready() const
    {
        return QTest::qWaitFor([this]() {
            return app->receivedKinds().contains(QByteArrayLiteral("snapshot.complete"));
        }, 5000);
    }
    QList<SessionMessage> messages() const
    {
        QList<SessionMessage> out;
        for (const QByteArray& wire : app->received()) {
            SessionMessage message;
            if (SessionMessages::decode(wire, &message)) {
                out.append(message);
            }
        }
        return out;
    }
    bool hasResult(SessionMessageKind kind, quint32 id) const
    {
        for (const SessionMessage& message : messages()) {
            if (message.kind == kind
                && (kind == SessionMessageKind::PropertyResult ? message.writeId
                                                               : message.commandId) == id) {
                return true;
            }
        }
        return false;
    }
    SessionMessage writeResult(quint32 writeId) const
    {
        for (const SessionMessage& message : messages()) {
            if (message.kind == SessionMessageKind::PropertyResult && message.writeId == writeId) {
                return message;
            }
        }
        return {};
    }
    // Sends a command and waits for its answer.
    bool invoke(const QByteArray& verb, quint32 id)
    {
        app->sendText(SessionMessages::encode(SessionMessages::commandInvoke(verb, id, {})));
        return QTest::qWaitFor(
            [&]() { return hasResult(SessionMessageKind::CommandResult, id); }, 3000);
    }
    // Sends a write and waits for its answer.
    bool write(const QByteArray& object, const QList<MirrorUpdate>& updates, quint32 writeId)
    {
        app->sendText(
            SessionMessages::encode(SessionMessages::propertyWrite(object, updates, writeId)));
        return QTest::qWaitFor(
            [&]() { return hasResult(SessionMessageKind::PropertyResult, writeId); }, 3000);
    }

    LoopbackTransport* app = nullptr;
    ProbeTransport* station = nullptr;
};

// The device id as the log prints it: hex, at least two characters.
const QRegularExpression kHexDevice(QStringLiteral("^[0-9a-f]{2,}$"));
const QString kClock = QStringLiteral("\\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d");
// A forged log line inside a name a device sends.
const QByteArray kForgedVerb = QByteArrayLiteral("x\n[12:00:00.000] INF: forged line");

ControlLog::PeerInfo signedInPeer(const QByteArray& id = QByteArrayLiteral("\xab\xcd"))
{
    ControlLog::PeerInfo peer;
    peer.deviceId = id;
    peer.signedIn = true;
    return peer;
}

SessionMessage command(const QByteArray& verb, quint32 id)
{
    return SessionMessages::commandInvoke(verb, id, {});
}

SessionMessage commandAnswer(const QByteArray& verb, quint32 id, bool accepted = true,
                             const QString& reason = {})
{
    return SessionMessages::commandResult(verb, id, accepted, reason, {});
}

SessionMessage frequencyWrite(quint32 writeId)
{
    return SessionMessages::propertyWrite(
        QByteArrayLiteral("slice:0"),
        {MirrorUpdate{0, "frequency", MirrorWireKind::Float64, QVariant(7074000.0)}}, writeId);
}

SessionMessage frequencyAnswer(quint32 writeId, bool accepted = true, const QString& reason = {})
{
    SessionPropertyResult result;
    result.property = QByteArrayLiteral("frequency");
    result.accepted = accepted;
    result.reason = reason;
    return SessionMessages::propertyResult(QByteArrayLiteral("slice:0"), writeId, {result});
}

bool noLineBreaks(const QStringList& lines)
{
    for (const QString& line : lines) {
        if (line.contains(QLatin1Char('\n')) || line.contains(QLatin1Char('\r'))) {
            return false;
        }
    }
    return true;
}

} // namespace

class TstStationControlLog : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY(m_securityDir.isValid());
        AppSettings::setProfileOverride(
            QStringLiteral("station-control-log-%1").arg(QCoreApplication::applicationPid()));
        AppSettings::instance().clear();
    }
    void cleanupTestCase()
    {
        const QString path = AppSettings::instance().filePath();
        QFile::remove(path);
        QFile::remove(path + QStringLiteral(".bak"));
    }
    void init()
    {
        AppSettings::instance().clear();
        m_nowMs = 1000;
        m_core = makeStationRadioModel();
        m_server = std::make_unique<StationServer>(
            m_core.get(), AppSettings::instance(),
            NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
        m_server->setControlLogClockForTest([this]() { return m_nowMs; });
    }
    void cleanup()
    {
        m_server.reset();
        m_core.reset();
    }

    // ---- ControlLog on its own ----

    // Names: the strict pattern, and every control character out of a
    // reason.
    void namesAndReasonsCannotBreakALine()
    {
        QCOMPARE(ControlLog::safeName(QByteArrayLiteral("slice:0")), QStringLiteral("slice:0"));
        QCOMPARE(ControlLog::safeName(QByteArrayLiteral("tx.key/a_b-c")),
                 QStringLiteral("tx.key/a_b-c"));
        QCOMPARE(ControlLog::safeName(kForgedVerb), QStringLiteral("an unrecognized name"));
        QCOMPARE(ControlLog::safeName(QByteArray()), QStringLiteral("an unrecognized name"));
        QCOMPARE(ControlLog::safeName(QByteArray(65, 'a')), QStringLiteral("an unrecognized name"));
        QCOMPARE(ControlLog::safeName(QByteArray(64, 'a')), QString(64, QLatin1Char('a')));
        QCOMPARE(ControlLog::safeText(QStringLiteral("a\nb\rc\u2028d\u0007e")),
                 QStringLiteral("a b c d e"));
        QCOMPARE(ControlLog::maskedAddress(QStringLiteral("192.168.1.42")),
                 QStringLiteral("*.*.*. 42"));
        QCOMPARE(ControlLog::maskedAddress(QStringLiteral("fe80::1:2")), QStringLiteral("*:2"));
    }

    // A message's wait between the transport's receipt and the main
    // thread is shown where the transport measures it; elsewhere the line
    // says it has none.
    void theReceiptAndQueueTimesAreTold()
    {
        ProbeTransport transport(QStringLiteral("probe"));
        ControlLog log;
        log.setClock([this]() { return m_nowMs; });
        LogCapture capture;
        log.inbound(&transport, signedInPeer(), command("control.a", 1));
        QStringList in = capture.lines(QStringLiteral("Control in from "));
        QCOMPARE(in.size(), 1);
        QVERIFY2(QRegularExpression(QStringLiteral(
                     "^Control in from abcd: command.invoke control.a, command 1, taken by the "
                     "main thread at %1 \\(no receipt time for this message\\), the "
                     "device's first control message$").arg(kClock))
                     .match(in.first()).hasMatch(),
                 qPrintable(in.first()));
        transport.waitUs = 3000;
        m_nowMs += 40;
        log.inbound(&transport, signedInPeer(), command("control.b", 2));
        in = capture.lines(QStringLiteral("Control in from "));
        QCOMPARE(in.size(), 2);
        QVERIFY2(QRegularExpression(QStringLiteral(
                     "^Control in from abcd: command.invoke control.b, command 2, received by "
                     "the transport at %1, taken from the queue at %1 \\(3.0 ms later\\), 40 ms "
                     "after the previous control message$").arg(kClock))
                     .match(in.last()).hasMatch(),
                 qPrintable(in.last()));
    }

    // A gap names both messages (object and property for a write), the
    // pickup gap, the receipt gap where both receipts are known, and the
    // link's buffer labelled for its kind.
    void aGapNamesItsMessagesAndBothTimes()
    {
        ProbeTransport transport(QStringLiteral("probe"));
        ControlLog log;
        log.setClock([this]() { return m_nowMs; });
        LogCapture capture;
        transport.waitUs = 3000;
        log.inbound(&transport, signedInPeer(), command("control.first", 1));
        m_nowMs += ControlLog::kGapLogMs - 1;
        log.inbound(&transport, signedInPeer(), command("control.second", 2));
        QCOMPARE(capture.lines(QStringLiteral("Control gap from ")).size(), 0);
        m_nowMs += 402;
        transport.waitUs = 1000;
        log.inbound(&transport, signedInPeer(), frequencyWrite(3));
        QStringList gaps = capture.lines(QStringLiteral("Control gap from "));
        QCOMPARE(gaps, QStringList{QStringLiteral(
                           "Control gap from abcd: 402 ms between taking control.second and "
                           "property.write slice:0 frequency from the queue, 404.0 ms between "
                           "their receipts; send backlog 1234 bytes")});

        // A WebSocket's buffer is named as Qt's, with the kernel's figures
        // where the transport gives them.
        SessionLinkDiagnostics ws;
        ws.buffer = SessionLinkDiagnostics::Buffer::WebSocket;
        ws.bufferedBytes = 10;
        ws.tcpRttUs = 2500;
        ws.tcpUnacked = 3;
        ws.tcpRetransmits = 7;
        ws.tcpNotSentBytes = 99;
        transport.diagnostics = ws;
        transport.waitUs.reset();
        m_nowMs += 300;
        log.inbound(&transport, signedInPeer(), command("control.third", 4));
        gaps = capture.lines(QStringLiteral("Control gap from "));
        QCOMPARE(gaps.size(), 2);
        QCOMPARE(gaps.last(),
                 QStringLiteral("Control gap from abcd: 300 ms between taking property.write "
                                "slice:0 frequency and control.third from the queue, receipt "
                                "times not measured on this link; WebSocket send buffer in Qt "
                                "10 bytes, TCP rtt 2.5 ms, 3 segments unacknowledged, 7 "
                                "retransmits in all, 99 bytes not sent by the kernel"));
        SessionLinkDiagnostics dc;
        dc.buffer = SessionLinkDiagnostics::Buffer::DataChannel;
        dc.bufferedBytes = 55;
        dc.sctpRttMs = 31;
        transport.diagnostics = dc;
        m_nowMs += 300;
        log.inbound(&transport, signedInPeer(), command("control.fourth", 5));
        QVERIFY2(capture.lines(QStringLiteral("Control gap from ")).last().endsWith(
                     QStringLiteral("; control data channel buffer 55 bytes, SCTP rtt 31 ms")),
                 qPrintable(capture.lines(QStringLiteral("Control gap from ")).last()));
    }

    // Before sign-in only message kinds are logged; after it, a name that
    // fails the pattern is a placeholder. A forged newline never reaches
    // the log.
    void aForgedNameNeverBreaksALine()
    {
        ProbeTransport transport(QStringLiteral("probe"));
        ControlLog log;
        log.setClock([this]() { return m_nowMs; });
        LogCapture capture;
        ControlLog::PeerInfo unsigned_;
        log.inbound(&transport, unsigned_, SessionMessages::hello(1, 0, 0, QStringLiteral("p")));
        m_nowMs += 300;
        log.inbound(&transport, unsigned_, command(kForgedVerb, 1));
        QCOMPARE(capture.lines(QStringLiteral("Control in from ")).size(), 0);
        QCOMPARE(capture.lines(QStringLiteral("Control gap from ")),
                 QStringList{QStringLiteral(
                     "Control gap from a device not signed in: 300 ms between taking hello and "
                     "command.invoke from the queue, receipt times not measured on this link; "
                     "send backlog 1234 bytes")});

        m_nowMs += 300;
        log.inbound(&transport, signedInPeer(), command(kForgedVerb, 2));
        SessionPropertyResult forged;
        forged.property = kForgedVerb;
        forged.accepted = false;
        forged.reason = QStringLiteral("no\n[12:00:00.000] INF: forged reason");
        log.inbound(&transport, signedInPeer(),
                    SessionMessages::propertyWrite(kForgedVerb, {MirrorUpdate{0, kForgedVerb,
                                                   MirrorWireKind::Int64, QVariant(1)}}, 9));
        log.answer(&transport, signedInPeer(), commandAnswer(kForgedVerb, 2, false,
                                                             QStringLiteral("bad\nverb")));
        log.answer(&transport, signedInPeer(),
                   SessionMessages::propertyResult(kForgedVerb, 9, {forged}));
        const QStringList all = capture.all();
        QVERIFY(noLineBreaks(all));
        const QStringList in = capture.lines(QStringLiteral("Control in from "));
        QCOMPARE(in.size(), 2);
        QVERIFY2(in.first().contains(QStringLiteral(": command.invoke an unrecognized name, ")),
                 qPrintable(in.first()));
        QVERIFY2(in.last().contains(QStringLiteral(
                     ": property.write an unrecognized name an unrecognized name, write 9")),
                 qPrintable(in.last()));
        const QStringList out = capture.lines(QStringLiteral("Control out to "));
        QCOMPARE(out.size(), 2);
        QVERIFY2(out.first().contains(QStringLiteral("refused (bad verb)")), qPrintable(out.first()));
        QVERIFY2(out.last().contains(QStringLiteral(
                     "refused (an unrecognized name: no [12:00:00.000] INF: forged reason)")),
                 qPrintable(out.last()));
    }

    // Each device's bucket: a burst of kBurstMessages, then
    // kRefillPerSecond. Under a sustained storm the lines stay bounded,
    // and every skipped message is counted and logged when it ends.
    void aSustainedStormIsBounded()
    {
        ProbeTransport transport(QStringLiteral("probe"));
        ControlLog log;
        log.setClock([this]() { return m_nowMs; });
        LogCapture capture;
        const ControlLog::PeerInfo peer = signedInPeer();
        quint32 id = 0;
        // A burst at one instant: the bucket's burst, the rest skipped.
        for (int i = 0; i < ControlLog::kBurstMessages + 10; ++i) {
            log.inbound(&transport, peer, frequencyWrite(++id));
            log.answer(&transport, peer, frequencyAnswer(id));
        }
        QCOMPARE(capture.lines(QStringLiteral("Control in from ")).size(),
                 ControlLog::kBurstMessages);
        // Then 100 a second for two seconds: about kRefillPerSecond a second
        // are logged.
        const int storm = 200;
        for (int i = 0; i < storm; ++i) {
            m_nowMs += 10;
            log.inbound(&transport, peer, frequencyWrite(++id));
            log.answer(&transport, peer, frequencyAnswer(id));
        }
        const int logged = static_cast<int>(capture.lines(QStringLiteral("Control in from ")).size());
        const int refilled = logged - ControlLog::kBurstMessages;
        QVERIFY2(refilled >= 2 * ControlLog::kRefillPerSecond - 1
                     && refilled <= 2 * ControlLog::kRefillPerSecond + 1,
                 qPrintable(QString::number(refilled)));
        // Fast accepted answers fold: at most a line and a summary a second.
        QVERIFY2(capture.lines(QStringLiteral("Control out to ")).size() <= 2 * 3 + 2,
                 qPrintable(capture.lines(QStringLiteral("Control out to ")).join(QLatin1Char('|'))));
        // Every line the storm made, at most the bucket's.
        QVERIFY(capture.all().size() <= ControlLog::kBurstMessages + 2 * ControlLog::kRefillPerSecond
                                            + 20);
        // The skipped count is logged when the connection ends; nothing is
        // lost from the account.
        log.closed(&transport, peer);
        const QStringList skipped = capture.lines(QStringLiteral("Control log for "));
        QVERIFY(!skipped.isEmpty());
        int counted = 0;
        const QRegularExpression shape(QStringLiteral(
            "^Control log for abcd: (\\d+) control messages with their answers and 0 gaps not "
            "logged \\(over this device's limit\\)$"));
        for (const QString& line : skipped) {
            const QRegularExpressionMatch match = shape.match(line);
            QVERIFY2(match.hasMatch(), qPrintable(line));
            counted += match.captured(1).toInt();
        }
        QCOMPARE(counted + logged, ControlLog::kBurstMessages + 10 + storm);
    }

    // At most kCoreLinesPerSecond control lines a second from all devices
    // together; the count of the rest is logged at the next second and at
    // the heartbeat.
    void theCoreWideCapHolds()
    {
        ProbeTransport a(QStringLiteral("a"));
        ProbeTransport b(QStringLiteral("b"));
        ProbeTransport c(QStringLiteral("c"));
        ControlLog log;
        log.setClock([this]() { return m_nowMs; });
        LogCapture capture;
        quint32 id = 0;
        for (ProbeTransport* transport : {&a, &b, &c}) {
            const ControlLog::PeerInfo peer =
                signedInPeer(QByteArray(1, static_cast<char>('a' + (transport - &a))));
            for (int i = 0; i < ControlLog::kBurstMessages; ++i) {
                log.inbound(transport, peer, command("control.cap", ++id));
            }
        }
        QCOMPARE(capture.all().size(), ControlLog::kCoreLinesPerSecond);
        log.tickCore();
        const QStringList core = capture.lines(QStringLiteral("Control log: "));
        QCOMPARE(core.size(), 1);
        // 150 in lines and three link lines, 100 of them logged.
        QCOMPARE(core.first(),
                 QStringLiteral("Control log: 53 control lines from all devices not logged "
                                "(over 100 a second)"));
    }

    // Accepted answers under kSlowAnswerMs for one object and property fold
    // into one line a window; a refusal and a slow answer keep their own.
    void fastAnswersFoldButRefusalsAndSlowOnesDoNot()
    {
        ProbeTransport transport(QStringLiteral("probe"));
        ControlLog log;
        log.setClock([this]() { return m_nowMs; });
        LogCapture capture;
        const ControlLog::PeerInfo peer = signedInPeer();
        for (quint32 id = 1; id <= 10; ++id) {
            log.inbound(&transport, peer, frequencyWrite(id));
            m_nowMs += 3;
            log.answer(&transport, peer, frequencyAnswer(id));
            m_nowMs += 47;
        }
        QCOMPARE(capture.lines(QStringLiteral("Control out to ")).size(), 1);
        // A refusal: its own line.
        log.inbound(&transport, peer, frequencyWrite(11));
        log.answer(&transport, peer, frequencyAnswer(11, false, QStringLiteral("Not now.")));
        // A slow answer: its own line, with the link.
        log.inbound(&transport, peer, frequencyWrite(12));
        m_nowMs += ControlLog::kSlowAnswerMs;
        log.answer(&transport, peer, frequencyAnswer(12));
        QStringList out = capture.lines(QStringLiteral("Control out to "));
        QCOMPARE(out.size(), 3);
        QCOMPARE(out.at(1), QStringLiteral("Control out to abcd: property.result slice:0 "
                                           "frequency, write 11, refused (frequency: Not now.), "
                                           "handled in 0 ms"));
        QCOMPARE(out.at(2), QStringLiteral("Control out to abcd: property.result slice:0 "
                                           "frequency, write 12, accepted, handled in 50 ms; "
                                           "send backlog 1234 bytes"));
        // The window's end: the folded ones in one line.
        m_nowMs += ControlLog::kFoldWindowMs;
        log.inbound(&transport, peer, command("control.next", 1));
        out = capture.lines(QStringLiteral("Control out to "));
        QCOMPARE(out.size(), 4);
        QCOMPARE(out.last(), QStringLiteral("Control out to abcd: property.result slice:0 "
                                            "frequency, 9 more accepted answers under 50 ms each "
                                            "(writes 2 to 10), slowest 3 ms"));
    }

    // A reason holding "%1" or "%2" stays text: the line's own numbers and
    // link are not substituted into it.
    void aPercentInAReasonStaysText()
    {
        ProbeTransport transport(QStringLiteral("probe"));
        ControlLog log;
        log.setClock([this]() { return m_nowMs; });
        LogCapture capture;
        const ControlLog::PeerInfo peer = signedInPeer();
        log.inbound(&transport, peer, frequencyWrite(21));
        log.answer(&transport, peer, frequencyAnswer(21, false, QStringLiteral("At %1 of %2.")));
        log.inbound(&transport, peer, command("control.next", 22));
        m_nowMs += ControlLog::kSlowAnswerMs;
        log.answer(&transport, peer, commandAnswer("control.next", 22, false,
                                                   QStringLiteral("Now %1%2%3.")));
        const QStringList out = capture.lines(QStringLiteral("Control out to "));
        QCOMPARE(out.size(), 2);
        QCOMPARE(out.at(0), QStringLiteral("Control out to abcd: property.result slice:0 "
                                           "frequency, write 21, refused (frequency: At %1 of "
                                           "%2.), handled in 0 ms"));
        QCOMPARE(out.at(1), QStringLiteral("Control out to abcd: command.result control.next, "
                                           "command 22, refused (Now %1%2%3.), handled in 50 ms; "
                                           "send backlog 1234 bytes"));
    }

    // A reused (or zero) command id: answers are matched in order, so each
    // answer gets its own invoke's handling time.
    void reusedCommandIdsMatchInOrder()
    {
        ProbeTransport transport(QStringLiteral("probe"));
        ControlLog log;
        log.setClock([this]() { return m_nowMs; });
        LogCapture capture;
        const ControlLog::PeerInfo peer = signedInPeer();
        log.inbound(&transport, peer, command("control.zero", 0));
        m_nowMs += 10;
        log.inbound(&transport, peer, command("control.zero", 0));
        m_nowMs += 20;
        log.answer(&transport, peer, commandAnswer("control.zero", 0));
        log.answer(&transport, peer, commandAnswer("control.zero", 0));
        // A third answer has no invoke left: no line.
        log.answer(&transport, peer, commandAnswer("control.zero", 0));
        const QStringList out = capture.lines(QStringLiteral("Control out to "));
        QCOMPARE(out.size(), 2);
        QVERIFY2(out.first().endsWith(QStringLiteral("accepted, handled in 30 ms")),
                 qPrintable(out.first()));
        QVERIFY2(out.last().endsWith(QStringLiteral("accepted, handled in 20 ms")),
                 qPrintable(out.last()));
    }

    // The link: carrier, pair, masked addresses, once and again on a
    // change; TCP_NODELAY once per session.
    void theLinkIsLoggedOnceAndOnChange()
    {
        ProbeTransport transport(QStringLiteral("probe"));
        ControlLog log;
        log.setClock([this]() { return m_nowMs; });
        LogCapture capture;
        NetworkPathSnapshot path;
        path.kind = NetworkPathSnapshot::Kind::Direct;
        path.carrier = NetworkPathSnapshot::Carrier::Ice;
        path.endpoints = NetworkPathSnapshot::Endpoints::IceCandidates;
        path.localAddress = QStringLiteral("192.168.1.10");
        path.localPort = 5000;
        path.remoteAddress = QStringLiteral("10.0.0.77");
        path.remotePort = 6000;
        path.localCandidateType = QStringLiteral("host");
        path.remoteCandidateType = QStringLiteral("srflx");
        transport.path = path;
        SessionLinkDiagnostics ws;
        ws.buffer = SessionLinkDiagnostics::Buffer::WebSocket;
        ws.noDelay = false;
        transport.diagnostics = ws;
        ControlLog::PeerInfo peer = signedInPeer();
        peer.introduced = true;
        log.inbound(&transport, peer, command("control.a", 1));
        QStringList link = capture.lines(QStringLiteral("Control link for "));
        QCOMPARE(link, QStringList{QStringLiteral(
                           "Control link for abcd: through the remote access service, control "
                           "data channel, direct pair, candidates local host, remote srflx, "
                           "local *.*.*. 10 port 5000, remote *.*.*. 77 port 6000; WebSocket "
                           "send buffer in Qt 0 bytes, TCP_NODELAY off")});
        // The same path: no line.
        m_nowMs += ControlLog::kLinkCheckMs;
        log.inbound(&transport, peer, command("control.b", 2));
        QCOMPARE(capture.lines(QStringLiteral("Control link for ")).size(), 1);
        // A new pair: logged as a change, without TCP_NODELAY again.
        path.kind = NetworkPathSnapshot::Kind::Relayed;
        path.localCandidateType = QStringLiteral("relay");
        transport.path = path;
        m_nowMs += ControlLog::kLinkCheckMs;
        log.inbound(&transport, peer, command("control.c", 3));
        link = capture.lines(QStringLiteral("Control link for "));
        QCOMPARE(link.size(), 2);
        QCOMPARE(link.last(), QStringLiteral(
                     "Control link for abcd (changed): through the remote access service, "
                     "control data channel, relayed pair, candidates local relay, remote srflx, "
                     "local *.*.*. 10 port 5000, remote *.*.*. 77 port 6000; WebSocket send "
                     "buffer in Qt 0 bytes"));
    }

    // Keepalive gaps are per channel and labelled as keepalives, apart from
    // the control gaps; unwatched keepalives are not measured.
    void keepaliveGapsNameTheirChannel()
    {
        ProbeTransport transport(QStringLiteral("probe"));
        ControlLog log;
        log.setClock([this]() { return m_nowMs; });
        LogCapture capture;
        const QByteArray id = QByteArrayLiteral("\xab\xcd");
        const ControlLog::PeerInfo peer = signedInPeer(id);
        using Channel = ControlLog::KeepaliveChannel;
        // Not watched: nothing measured.
        log.keepalive(id, Channel::MediaTx, 0, false, nullptr, peer);
        m_nowMs += 500;
        log.keepalive(id, Channel::MediaTx, 0, false, nullptr, peer);
        log.keepaliveWatchStarted(id);
        for (int i = 0; i < 3; ++i) {
            m_nowMs += 100;
            log.keepalive(id, Channel::MediaTx, 0, true, &transport, peer);
        }
        QCOMPARE(capture.all().size(), 0);
        // The media channel goes quiet; the control link carries one.
        m_nowMs += 250;
        log.keepalive(id, Channel::Control, 0, true, &transport, peer);
        // The media channel's next, 2 ms after its receipt, 302 ms after
        // the one before it was received.
        m_nowMs += 52;
        log.keepalive(id, Channel::MediaTx, 2, true, &transport, peer);
        const QStringList gaps = capture.lines(QStringLiteral("Keepalive gap from "));
        QCOMPARE(gaps, QStringList{QStringLiteral(
                           "Keepalive gap from abcd on the media connection's tx data channel: "
                           "300 ms between two keepalives as the transport received them (a "
                           "transmit keepalive, not a control message); the last keepalive "
                           "before it came on the control link (tx.keepalive), 50 ms earlier; "
                           "control link: send backlog 1234 bytes")});
        QCOMPARE(capture.lines(QStringLiteral("Control gap from ")).size(), 0);

        // The watchdog's stop names the last keepalive's channel and age.
        m_nowMs += 402;
        log.watchdogStopped(id, &transport, peer, false, 402);
        const QStringList stops = capture.lines(QStringLiteral("Transmit watchdog stop "));
        QCOMPARE(stops, QStringList{QStringLiteral(
                            "Transmit watchdog stop for abcd: no keepalive for 402 ms; the last "
                            "keepalive came on the media connection's tx data channel, received "
                            "404 ms ago; no control message taken on this connection; control "
                            "link: send backlog 1234 bytes")});
    }

    // ---- Through the station ----

    // An accepted write: one line in, with the device in hex, the object,
    // the property, the writeId and when it was taken; one line out, with
    // accepted and the handling time on the log's clock.
    void anAcceptedWriteIsLoggedInAndOut()
    {
        Device phone(m_server.get(), this);
        QVERIFY(phone.ready());
        LogCapture log;
        const quint32 writeId = 5101;
        QVERIFY(phone.write(QByteArrayLiteral("slice:0"),
                            {MirrorUpdate{0, "frequency", MirrorWireKind::Float64,
                                          QVariant(7074000.0)}},
                            writeId));
        const SessionMessage result = phone.writeResult(writeId);
        QVERIFY(!result.propertyResults.isEmpty());
        QVERIFY2(result.propertyResults.first().accepted,
                 qPrintable(result.propertyResults.first().reason));

        const QStringList in = log.lines(QStringLiteral("Control in from "));
        QCOMPARE(in.size(), 1);
        const QRegularExpression inShape(QStringLiteral(
            "^Control in from ([0-9a-f]+): property.write slice:0 frequency, write 5101, "
            "taken by the main thread at %1 \\(no receipt time for this message\\), "
            "0 ms after the previous control message$").arg(kClock));
        const QRegularExpressionMatch inMatch = inShape.match(in.first());
        QVERIFY2(inMatch.hasMatch(), qPrintable(in.first()));
        QVERIFY(kHexDevice.match(inMatch.captured(1)).hasMatch());

        const QStringList out = log.lines(QStringLiteral("Control out to "));
        QCOMPARE(out, QStringList{QStringLiteral(
                          "Control out to %1: property.result slice:0 frequency, write 5101, "
                          "accepted, handled in 0 ms")
                                      .arg(inMatch.captured(1))});
        // The value written is never in the log.
        QVERIFY(!in.first().contains(QStringLiteral("7074")));
    }

    // A refused write: its answer line names the property and the Core's
    // reason.
    void aRefusedWriteIsLoggedWithItsReason()
    {
        Device phone(m_server.get(), this);
        QVERIFY(phone.ready());
        LogCapture log;
        const quint32 writeId = 5102;
        QVERIFY(phone.write(QByteArrayLiteral("setup"),
                            {MirrorUpdate{0, "label", MirrorWireKind::Utf8,
                                          QVariant(QStringLiteral("secret words"))}},
                            writeId));
        const QStringList in = log.lines(QStringLiteral("Control in from "));
        QCOMPARE(in.size(), 1);
        QVERIFY2(in.first().contains(QStringLiteral(": property.write setup label, write 5102, ")),
                 qPrintable(in.first()));
        QVERIFY(!in.first().contains(QStringLiteral("secret")));
        const QStringList out = log.lines(QStringLiteral("Control out to "));
        QCOMPARE(out.size(), 1);
        QVERIFY2(out.first().endsWith(QStringLiteral(
                     ": property.result setup label, write 5102, refused (label: The Core owns "
                     "Setup descriptions.), handled in 0 ms")),
                 qPrintable(out.first()));
    }

    // A command: in with its verb and commandId, out with the Core's
    // answer and the time it took on the log's clock.
    void aCommandIsLoggedInAndOut()
    {
        Device phone(m_server.get(), this);
        QVERIFY(phone.ready());
        LogCapture log;
        QVERIFY(phone.invoke(QByteArrayLiteral("control.notAVerb"), 6101));
        const QStringList in = log.lines(QStringLiteral("Control in from "));
        QCOMPARE(in.size(), 1);
        QVERIFY2(QRegularExpression(QStringLiteral(
                     "^Control in from [0-9a-f]+: command.invoke control.notAVerb, command "
                     "6101, taken by the main thread at %1 ").arg(kClock))
                     .match(in.first())
                     .hasMatch(),
                 qPrintable(in.first()));
        const QStringList out = log.lines(QStringLiteral("Control out to "));
        QCOMPARE(out.size(), 1);
        QVERIFY2(QRegularExpression(QStringLiteral(
                     "^Control out to [0-9a-f]+: command.result control.notAVerb, command "
                     "6101, refused \\(.+\\), handled in 0 ms$"))
                     .match(out.first())
                     .hasMatch(),
                 qPrintable(out.first()));
    }

    // The handling time is from when the message was taken to the answer,
    // on the log's clock.
    void theHandlingTimeIsPickupToAnswer()
    {
        Device phone(m_server.get(), this);
        QVERIFY(phone.ready());
        LogCapture log;
        // The clock moves 7 ms while the Core handles the command.
        int reads = 0;
        m_server->setControlLogClockForTest([this, &reads]() {
            return m_nowMs + (reads++ > 0 ? 7 : 0);
        });
        QVERIFY(phone.invoke(QByteArrayLiteral("control.notAVerb"), 6102));
        const QStringList out = log.lines(QStringLiteral("Control out to "));
        QCOMPARE(out.size(), 1);
        m_server->setControlLogClockForTest([this]() { return m_nowMs; });
        QVERIFY2(out.first().endsWith(QStringLiteral(", handled in 7 ms")), qPrintable(out.first()));
    }

    // tx.keepalive (ten a second while keyed) gets no in or out line.
    void aKeepaliveGetsNoControlLine()
    {
        Device phone(m_server.get(), this);
        QVERIFY(phone.ready());
        LogCapture log;
        QVERIFY(phone.invoke(QByteArrayLiteral("tx.keepalive"), 6150));
        QCOMPARE(log.lines(QStringLiteral("Control in from ")).size(), 0);
        QCOMPARE(log.lines(QStringLiteral("Control out to ")).size(), 0);
    }

    // A forged newline in a verb, before and after sign-in, never reaches
    // the log as a line of its own.
    void aForgedVerbThroughTheStationStaysOnOneLine()
    {
        LogCapture log;
        {
            Device stranger(m_server.get(), this, /*signIn=*/false);
            // The hello is taken on the frozen clock first.
            QTest::qWait(100);
            m_nowMs += 300;
            stranger.app->sendText(
                SessionMessages::encode(SessionMessages::commandInvoke(kForgedVerb, 1, {})));
            QVERIFY(QTest::qWaitFor(
                [&]() { return !log.lines(QStringLiteral("Control gap from ")).isEmpty(); },
                3000));
            const QStringList gaps = log.lines(QStringLiteral("Control gap from "));
            QVERIFY2(gaps.first().startsWith(QStringLiteral(
                         "Control gap from a device not signed in: 300 ms between taking hello "
                         "and command.invoke from the queue")),
                     qPrintable(gaps.first()));
        }
        Device phone(m_server.get(), this);
        QVERIFY(phone.ready());
        QVERIFY(phone.invoke(kForgedVerb, 2));
        const QStringList in = log.lines(QStringLiteral("Control in from "));
        QCOMPARE(in.size(), 1);
        QVERIFY2(in.first().contains(QStringLiteral("command.invoke an unrecognized name, command 2")),
                 qPrintable(in.first()));
        QVERIFY(noLineBreaks(log.all()));
        for (const QString& line : log.all()) {
            QVERIFY2(!line.contains(QStringLiteral("forged")), qPrintable(line));
        }
    }

    // Skipped counts are logged when the connection ends.
    void skippedCountsAreLoggedAtDisconnect()
    {
        Device phone(m_server.get(), this);
        QVERIFY(phone.ready());
        LogCapture log;
        quint32 id = 6400;
        for (int i = 0; i < ControlLog::kBurstMessages + 5; ++i) {
            QVERIFY(phone.write(QByteArrayLiteral("slice:0"),
                                {MirrorUpdate{0, "frequency", MirrorWireKind::Float64,
                                              QVariant(7074000.0 + i)}},
                                ++id));
        }
        QCOMPARE(log.lines(QStringLiteral("Control in from ")).size(), ControlLog::kBurstMessages);
        QCOMPARE(log.lines(QStringLiteral("Control log for ")).size(), 0);
        phone.app->closeLink(QStringLiteral("bye"));
        QVERIFY(QTest::qWaitFor(
            [&]() { return !log.lines(QStringLiteral("Control log for ")).isEmpty(); }, 3000));
        QVERIFY2(QRegularExpression(QStringLiteral(
                     "^Control log for [0-9a-f]+: 5 control messages with their answers and 0 "
                     "gaps not logged \\(over this device's limit\\)$"))
                     .match(log.lines(QStringLiteral("Control log for ")).first())
                     .hasMatch(),
                 qPrintable(log.lines(QStringLiteral("Control log for ")).first()));
    }

    // Skipped counts are logged at the heartbeat, with no message after.
    void skippedCountsAreLoggedAtTheHeartbeat()
    {
        Device phone(m_server.get(), this);
        QVERIFY(phone.ready());
        LogCapture log;
        quint32 id = 6500;
        for (int i = 0; i < ControlLog::kBurstMessages + 3; ++i) {
            QVERIFY(phone.invoke(QByteArrayLiteral("control.storm"), ++id));
        }
        QCOMPARE(log.lines(QStringLiteral("Control log for ")).size(), 0);
        m_server->setHeartbeatIntervalMs(20);
        QVERIFY(QTest::qWaitFor(
            [&]() { return !log.lines(QStringLiteral("Control log for ")).isEmpty(); }, 3000));
        const QString line = log.lines(QStringLiteral("Control log for ")).first();
        QVERIFY2(line.contains(QStringLiteral(": 3 control messages with their answers and 0 gaps")),
                 qPrintable(line));
    }

private:
    QTemporaryDir m_securityDir;
    qint64 m_nowMs = 1000;
    std::unique_ptr<RadioModel> m_core;
    std::unique_ptr<StationServer> m_server;
};

QTEST_GUILESS_MAIN(TstStationControlLog)
#include "tst_station_control_log.moc"
