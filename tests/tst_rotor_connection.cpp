// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - the Core's rotor link (rotor control plan, Task 3b).
//
// Each driver against a fake byte stream: the exact bytes written for
// poll, set, stop and move, and what each reply does to the heading.
// Formats are the cited ones (Hamlib gs232a.c, gs232b.c, rotctld(1)), and
// both bench captures from JJ's (KG4VCF) Easy Rotor Control are replayed
// through the connection: tests/data/rotor/erc-gs232b-capture-2026-10-07.log
// and tests/data/rotor/erc-gs232b-range-2026-10-07.log. The span positions
// expected after each step are the ones the range capture records (south
// stop reads 183, span 3; the clockwise stop reads 269, span 449). No test
// opens a real serial port, starts the real rotctld or turns a rotor:
// driver 4 runs a stand-in shell script in rotctld's place.
//
// Modification history (NereusSDR):
//   2026-10-08: Initial version. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.

#include <QtTest>

#include "core/RotctldProcess.h"
#include "core/RotorConnection.h"
#include "core/RotorRoute.h"

#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QLoggingCategory>
#include <QPointer>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <cmath>
#include <functional>
#include <limits>

using namespace NereusSDR;
using RotorRoute::EndStop;

namespace {

class FakeTransport : public RotorTransport {
public:
    bool failOpen{false};
    QByteArray written;
    QByteArray pending;
    bool closed{false};

    void open() override
    {
        if (failOpen) {
            emit failed(QStringLiteral("Could not open the fake port."));
        } else {
            emit opened();
        }
    }
    void close() override { closed = true; }
    qint64 write(const QByteArray& bytes) override
    {
        written += bytes;
        return bytes.size();
    }
    QByteArray readAll() override
    {
        QByteArray out = pending;
        pending.clear();
        return out;
    }

    void feed(const QByteArray& bytes)
    {
        pending += bytes;
        emit readyRead();
    }
    QByteArray take()
    {
        QByteArray out = written;
        written.clear();
        return out;
    }
};

// Timers that never fire on their own: the test steps every poll.
RotorConnection::Timing steppedTiming()
{
    RotorConnection::Timing t;
    t.pollStillMs = 3600000;
    t.pollTurningMs = 3599000;
    t.replyTimeoutMs = 3600000;
    t.settleMs = 3600000;
    t.staleMs = 3600000;
    t.answerDeadlineMs = 3600000;
    t.reconnectUnitMs = 3600000;
    t.rotctldStartDelayMs = 0;
    return t;
}

// One line of a bench capture.
struct CaptureLine {
    QString    run;
    int        step{0};
    QString    sent;
    QByteArray reply;
};

QByteArray unescape(const QString& repr)
{
    QByteArray out;
    for (int i = 0; i < repr.size(); ++i) {
        if (repr.at(i) == QLatin1Char('\\') && i + 1 < repr.size()) {
            const QChar c = repr.at(++i);
            if (c == QLatin1Char('r')) { out += '\r'; }
            else if (c == QLatin1Char('n')) { out += '\n'; }
            else { out += c.toLatin1(); }
        } else {
            out += repr.at(i).toLatin1();
        }
    }
    return out;
}

// A warning the code under test is expected to log, matched on its text.
void expectWarning(const char* pattern)
{
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QString::fromLatin1(pattern)));
}

QVector<CaptureLine> readCapture(const QString& name)
{
    QVector<CaptureLine> lines;
    QFile f(QStringLiteral(NEREUS_TEST_DATA_DIR "/rotor/") + name);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) { return lines; }
    static const QRegularExpression kRun(QStringLiteral("^# ---- run: (\\S+)"));
    static const QRegularExpression kStep(QStringLiteral("^# step (\\d+)"));
    static const QRegularExpression kLine(QStringLiteral(
        "^\\S+ baud=\\d+ sent='([^']*)'\\+CR reply=b'([^']*)'"));
    QString run = QStringLiteral("capture");
    int step = 0;
    while (!f.atEnd()) {
        const QString text = QString::fromUtf8(f.readLine()).trimmed();
        QRegularExpressionMatch m = kRun.match(text);
        if (m.hasMatch()) { run = m.captured(1); step = 0; continue; }
        m = kStep.match(text);
        if (m.hasMatch()) { step = m.captured(1).toInt(); continue; }
        m = kLine.match(text);
        if (m.hasMatch()) {
            lines.append(CaptureLine{run, step, m.captured(1), unescape(m.captured(2))});
        }
    }
    return lines;
}

} // namespace

class TestRotorConnection : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void constantsAreTheDesignOnes();
    void noDriverRefuses();

    // GS-232B
    void gs232bWritesTheCitedBytes();
    void gs232bReadsTheErcReplies();
    void gs232bIgnoresWhatIsNotAPosition();
    void gs232bJoinsARepliesSplitAcrossReads();
    void offsetIsAddedToRepliesAndRemovedFromTargets();
    void offsetLeavesTheSpanOnTheControllersReading();
    void aReadingAbove360PlacesTheSpan();
    void strictHeadingsRefuseBeforeAnythingIsSent();
    void azimuthRotorRefusesElevation();

    // GS-232A
    void gs232aWritesTheCitedBytesAndReadsItsReply();

    // rotctld
    void rotctldDialsTheHostAndPort();
    void rotctldWritesTheCitedBytes();
    void rotctldReadsPositionsAndReports();
    void rotctldInvalidRepliesAreNotPositions();
    void rotctldNumbersIgnoreTheLocale();

    // Polling, stop and freshness
    void neverAPollWhileOneIsOutstanding();
    void pollsFasterWhileTurning();
    void stopIsWrittenAheadOfAnythingQueued();
    void rotctldStopIsWrittenAheadOfAnythingQueued();
    void staleAfter1500msThenFreshOnTheNextReply();
    void failedOpenIsRetried();
    void aSilentPortIsNotConnectedAndFaultsAtTheDeadline();
    void aPortAnsweringGarbageIsNotConnected();

    // A stop before the link closes mid-turn (final review I2)
    void disconnectDuringAMoveSendsAStop();
    void disconnectDuringATargetTurnSendsAStop();
    void reconnectDuringAMoveStopsTheOldLink();
    void rotctldDisconnectDuringATurnSendsItsStop();
    void teardownDuringAMoveSendsAStop();
    void quittingDuringAMoveSendsAStop();
    void disconnectWhileStillSendsNothing();

    // Arrival on the span (final review M1)
    void arrivalInTheOverlapIsJudgedOnTheSpan();
    void arrivalAtTheClockwiseStopIsJudgedOnTheSpan();

    // Bench captures
    void replayErcCapture();
    void replayErcRangeCapture();

    // Driver 4
    void driver4StartsRotctldAndStopsItOnDisconnect();
    void driver4StopsRotctldOnExit();
    void driver4WithoutRotctldRefusesWithTheDocumentReason();
    void driver4ReportsWhyRotctldExited();

private:
    void connectWith(const RotorConfig& config,
                     const RotorConnection::Timing& timing = steppedTiming());
    // Driver 4's stand-in: records its arguments, writes "stopped" on
    // SIGTERM. Returns the script path.
    QString writeStandIn(const QString& body);

    std::unique_ptr<RotorConnection> m_conn;
    QPointer<FakeTransport> m_fake;
    RotorTransportTarget m_target;
    int m_transportsMade{0};
    std::unique_ptr<QTemporaryDir> m_dir;
};

void TestRotorConnection::initTestCase()
{
    // The routine connect and reply-parsing lines are not test output;
    // expected warnings are named with expectWarning() where they occur.
    QLoggingCategory::setFilterRules(QStringLiteral(
        "nereus.rotor.debug=false\nnereus.rotor.info=false\n"
        "nereus.rotctld.debug=false\nnereus.rotctld.info=false"));
}

void TestRotorConnection::init()
{
    m_conn = std::make_unique<RotorConnection>();
    m_transportsMade = 0;
    m_target = RotorTransportTarget{};
    m_conn->setTransportFactoryForTesting([this](const RotorTransportTarget& t) {
        m_target = t;
        ++m_transportsMade;
        auto fake = std::make_unique<FakeTransport>();
        m_fake = fake.get();
        return std::unique_ptr<RotorTransport>(std::move(fake));
    });
    m_dir = std::make_unique<QTemporaryDir>();
}

void TestRotorConnection::cleanup()
{
    m_conn.reset();
    RotctldProcess::setBinaryOverrideForTesting(std::nullopt);
    m_dir.reset();
}

void TestRotorConnection::connectWith(const RotorConfig& config,
                                      const RotorConnection::Timing& timing)
{
    m_conn->setTimingForTesting(timing);
    m_conn->configure(config);
    QVERIFY(m_conn->connectToRotor());
}

QString TestRotorConnection::writeStandIn(const QString& body)
{
    const QString path = m_dir->filePath(QStringLiteral("rotctld"));
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) { return {}; }
    f.write(body.toUtf8());
    f.close();
    f.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                     | QFileDevice::ExeOwner);
    return path;
}

// ── Basics ───────────────────────────────────────────────────────────

void TestRotorConnection::constantsAreTheDesignOnes()
{
    QCOMPARE(RotorConnection::kStaleMs, 1500);
    QCOMPARE(RotorConnection::kPollStillMs, 1000);
    QCOMPARE(RotorConnection::kPollTurningMs, 250);
    QCOMPARE(RotorConnection::kArrivedDeg, 1.5);
    QCOMPARE(RotorConnection::kRotctldDefaultPort, quint16(4533));
    const RotorConfig c;
    QCOMPARE(c.driver, RotorDriver::None);
    QCOMPARE(c.baud, 9600);
    QCOMPARE(c.port, quint16(4533));
    QCOMPARE(c.axes, RotorAxes::Azimuth);
    QCOMPARE(c.endStop, EndStop::North);
    QCOMPARE(c.rangeDeg, 360.0);
    QCOMPARE(c.offsetDeg, 0.0);
    // The wire enums (remote rotor control v1).
    QCOMPARE(int(RotorDriver::Gs232a), 1);
    QCOMPARE(int(RotorDriver::Gs232b), 2);
    QCOMPARE(int(RotorDriver::Rotctld), 3);
    QCOMPARE(int(RotorDriver::RotctldStarted), 4);
    QCOMPARE(int(RotorDirection::Ccw), 0);
    QCOMPARE(int(RotorDirection::Cw), 1);
    QCOMPARE(int(RotorDirection::Down), 2);
    QCOMPARE(int(RotorDirection::Up), 3);
}

void TestRotorConnection::noDriverRefuses()
{
    expectWarning("No rotor is set up on this Core");
    QSignalSpy failed(m_conn.get(), &RotorConnection::connectionFailed);
    QVERIFY(!m_conn->connectToRotor());
    QCOMPARE(failed.count(), 1);
    QCOMPARE(m_conn->lastError(), QStringLiteral("No rotor is set up on this Core."));
    QCOMPARE(m_transportsMade, 0);
    QVERIFY(!m_conn->setTarget(100.0));
}

// ── GS-232B ──────────────────────────────────────────────────────────

void TestRotorConnection::gs232bWritesTheCitedBytes()
{
    RotorConfig c;
    c.driver = RotorDriver::Gs232b;
    c.serialPort = QStringLiteral("/dev/ttyUSB0");
    connectWith(c);
    QVERIFY(m_fake);
    QVERIFY(m_target.serial);
    QCOMPARE(m_target.serialPort, QStringLiteral("/dev/ttyUSB0"));
    QCOMPARE(m_target.baud, 9600);
    // Bench fix: an open port is still connecting until a reply parses.
    QVERIFY(!m_conn->isConnected());

    // The first poll goes out on connect.
    QCOMPARE(m_fake->take(), QByteArray("C2\r"));
    m_fake->feed("AZ=302  EL=000\r\n");
    QVERIFY(m_conn->isConnected());

    QVERIFY(m_conn->setTarget(292.0));
    QCOMPARE(m_fake->take(), QByteArray("W292 000\r"));
    QVERIFY(m_conn->setTarget(5.0));
    QCOMPARE(m_fake->take(), QByteArray("W005 000\r"));
    // 360 is north and is sent as 0 (the strict-heading rule).
    QVERIFY(m_conn->setTarget(360.0));
    QCOMPARE(m_fake->take(), QByteArray("W000 000\r"));
    m_conn->stop();
    QCOMPARE(m_fake->take(), QByteArray("S\r"));
    QVERIFY(m_conn->startMove(RotorDirection::Ccw));
    QCOMPARE(m_fake->take(), QByteArray("L\r"));
    QVERIFY(m_conn->startMove(RotorDirection::Cw));
    QCOMPARE(m_fake->take(), QByteArray("R\r"));
    QVERIFY(m_conn->moveActive());
    m_conn->stop();
    QCOMPARE(m_fake->take(), QByteArray("S\r"));
    QVERIFY(!m_conn->moveActive());

    QCOMPARE(RotorConnection::moveCommand(RotorDriver::Gs232b, RotorDirection::Up),
             QByteArray("U\r"));
    QCOMPARE(RotorConnection::moveCommand(RotorDriver::Gs232b, RotorDirection::Down),
             QByteArray("D\r"));
    QCOMPARE(RotorConnection::setCommand(RotorDriver::Gs232b, 45.4, 30.0),
             QByteArray("W045 030\r"));
}

void TestRotorConnection::gs232bReadsTheErcReplies()
{
    RotorConfig c;
    c.driver = RotorDriver::Gs232b;
    connectWith(c);
    QSignalSpy updated(m_conn.get(), &RotorConnection::positionUpdated);
    QCOMPARE(m_conn->azimuthDeg(), -1.0);
    QVERIFY(!m_conn->positionFresh());

    // `C2` on the ERC: two spaces before EL.
    QCOMPARE(m_fake->take(), QByteArray("C2\r"));
    m_fake->feed("AZ=302  EL=000\r\n");
    QCOMPARE(updated.count(), 1);
    QCOMPARE(m_conn->azimuthDeg(), 302.0);
    QVERIFY(m_conn->positionFresh());
    // An azimuth rotor reports no elevation, whatever EL says.
    QCOMPARE(m_conn->elevationDeg(), -1.0);
    // North stop, 360: one span position.
    QCOMPARE(m_conn->spanPositionDeg(), 302.0);

    // `C` on the ERC: azimuth alone.
    m_conn->pollNowForTesting();
    QCOMPARE(m_fake->take(), QByteArray("C2\r"));
    m_fake->feed("AZ=303\r\n");
    QCOMPARE(updated.count(), 2);
    QCOMPARE(m_conn->azimuthDeg(), 303.0);

    // North reads 360 and is 0.
    m_conn->pollNowForTesting();
    QCOMPARE(m_fake->take(), QByteArray("C2\r"));
    m_fake->feed("AZ=360  EL=000\r\n");
    QCOMPARE(m_conn->azimuthDeg(), 0.0);

    // One space (Hamlib's documented form) parses too.
    m_conn->pollNowForTesting();
    m_fake->take();
    m_fake->feed("AZ=010 EL=000\r\n");
    QCOMPARE(m_conn->azimuthDeg(), 10.0);

    // On an az/el rotor the elevation is read.
    RotorConfig ae = c;
    ae.axes = RotorAxes::AzimuthElevation;
    m_conn->disconnectFromRotor();
    connectWith(ae);
    QCOMPARE(m_fake->take(), QByteArray("C2\r"));
    m_fake->feed("AZ=120  EL=045\r\n");
    QCOMPARE(m_conn->azimuthDeg(), 120.0);
    QCOMPARE(m_conn->elevationDeg(), 45.0);
}

void TestRotorConnection::gs232bIgnoresWhatIsNotAPosition()
{
    RotorConfig c;
    c.driver = RotorDriver::Gs232b;
    connectWith(c);
    QSignalSpy updated(m_conn.get(), &RotorConnection::positionUpdated);
    QCOMPARE(m_fake->take(), QByteArray("C2\r"));

    // The bare CR acknowledging a command, a bare CR LF and `>`, and
    // Hamlib's other invalid replies: no position, and the poll stays
    // outstanding.
    m_fake->feed("\r");
    m_fake->feed("\r\n");
    m_fake->feed(">\r\n");
    m_fake->feed(">");
    m_fake->feed("\r");
    m_fake->feed("AZ=\r\n");
    m_fake->feed("AZ=abc  EL=000\r\n");
    m_fake->feed("?>\r\n");
    m_fake->feed("+0302+0000\r\n");   // GS-232A form on a GS-232B driver
    QCOMPARE(updated.count(), 0);
    QCOMPARE(m_conn->azimuthDeg(), -1.0);
    QVERIFY(!m_conn->positionFresh());
    m_conn->pollNowForTesting();
    QCOMPARE(m_fake->take(), QByteArray());   // still outstanding

    m_fake->feed("AZ=302  EL=000\r\n");
    QCOMPARE(updated.count(), 1);
    QCOMPARE(m_conn->azimuthDeg(), 302.0);
}

void TestRotorConnection::gs232bJoinsARepliesSplitAcrossReads()
{
    RotorConfig c;
    c.driver = RotorDriver::Gs232b;
    connectWith(c);
    m_fake->take();
    m_fake->feed("AZ=3");
    QCOMPARE(m_conn->azimuthDeg(), -1.0);
    m_fake->feed("02  EL=0");
    QCOMPARE(m_conn->azimuthDeg(), -1.0);
    m_fake->feed("00\r\n");
    QCOMPARE(m_conn->azimuthDeg(), 302.0);
}

void TestRotorConnection::offsetIsAddedToRepliesAndRemovedFromTargets()
{
    RotorConfig c;
    c.driver = RotorDriver::Gs232b;
    c.offsetDeg = 10.0;
    connectWith(c);
    m_fake->take();
    m_fake->feed("AZ=355  EL=000\r\n");
    QCOMPARE(m_conn->azimuthDeg(), 5.0);
    QVERIFY(m_conn->setTarget(10.0));
    QCOMPARE(m_fake->take(), QByteArray("W000 000\r"));
    QCOMPARE(m_conn->targetAzimuthDeg(), 10.0);
}

void TestRotorConnection::offsetLeavesTheSpanOnTheControllersReading()
{
    // Final review I1: north stop, range 360, offset +5. The controller
    // reads 358, 2 degrees from its own clockwise stop; shown as 003. A
    // target of 010 is sent as 005, and the controller turns 353 degrees
    // counter-clockwise to reach it.
    RotorConfig c;
    c.driver = RotorDriver::Gs232b;
    c.endStop = EndStop::North;
    c.offsetDeg = 5.0;
    connectWith(c);
    m_fake->take();
    m_fake->feed("AZ=358  EL=000\r\n");
    QCOMPARE(m_conn->azimuthDeg(), 3.0);
    QCOMPARE(m_conn->spanPositionDeg(), 358.0);
    QVERIFY(m_conn->setTarget(10.0));
    QCOMPARE(m_fake->take(), QByteArray("W005 000\r"));
    const RotorRoute::Move route = m_conn->routeToTarget();
    QVERIFY(route.routeKnown);
    QCOMPARE(route.travelDeg, -353.0);
}

void TestRotorConnection::aReadingAbove360PlacesTheSpan()
{
    // Final review M12: a 450 degree north-stop rotor whose controller
    // reads 400 is at span 400, compass 040, from its first reply.
    RotorConfig c;
    c.driver = RotorDriver::Gs232b;
    c.endStop = EndStop::North;
    c.rangeDeg = 450.0;
    connectWith(c);
    m_fake->take();
    m_fake->feed("AZ=400  EL=000\r\n");
    QCOMPARE(m_conn->azimuthDeg(), 40.0);
    QCOMPARE(m_conn->spanPositionDeg(), 400.0);
}

void TestRotorConnection::strictHeadingsRefuseBeforeAnythingIsSent()
{
    RotorConfig c;
    c.driver = RotorDriver::Gs232b;
    connectWith(c);
    m_fake->take();
    m_fake->feed("AZ=100  EL=000\r\n");

    QVERIFY(!m_conn->setTarget(std::numeric_limits<double>::quiet_NaN()));
    QVERIFY(!m_conn->setTarget(std::numeric_limits<double>::infinity()));
    QVERIFY(!m_conn->setTarget(-90.0));
    QVERIFY(!m_conn->setTarget(-0.5));
    QVERIFY(!m_conn->setTarget(360.5));
    QVERIFY(!m_conn->setTarget(450.0));
    QCOMPARE(m_fake->take(), QByteArray());
    QCOMPARE(m_conn->targetAzimuthDeg(), -1.0);
    QVERIFY(!m_conn->turning());

    // Off connected nothing is sent either.
    m_conn->disconnectFromRotor();
    QVERIFY(!m_conn->setTarget(100.0));
    QVERIFY(!m_conn->startMove(RotorDirection::Cw));
    m_conn->stop();
}

void TestRotorConnection::azimuthRotorRefusesElevation()
{
    RotorConfig c;
    c.driver = RotorDriver::Gs232b;
    connectWith(c);
    m_fake->take();
    m_fake->feed("AZ=100  EL=000\r\n");
    QVERIFY(!m_conn->setTarget(120.0, 30.0));
    QVERIFY(!m_conn->startMove(RotorDirection::Up));
    QVERIFY(!m_conn->startMove(RotorDirection::Down));
    QCOMPARE(m_fake->take(), QByteArray());

    // An az/el rotor takes them, and refuses elevation outside 0 to 90.
    RotorConfig ae = c;
    ae.axes = RotorAxes::AzimuthElevation;
    m_conn->disconnectFromRotor();
    connectWith(ae);
    m_fake->take();
    m_fake->feed("AZ=100  EL=020\r\n");
    QVERIFY(!m_conn->setTarget(120.0, 91.0));
    QVERIFY(!m_conn->setTarget(120.0, -2.0));
    QVERIFY(!m_conn->setTarget(120.0, std::numeric_limits<double>::quiet_NaN()));
    QCOMPARE(m_fake->take(), QByteArray());
    QVERIFY(m_conn->setTarget(120.0, 30.0));
    QCOMPARE(m_fake->take(), QByteArray("W120 030\r"));
    QCOMPARE(m_conn->targetElevationDeg(), 30.0);
    // Leaving elevation sends the one last read, never 0.
    QVERIFY(m_conn->setTarget(130.0));
    QCOMPARE(m_fake->take(), QByteArray("W130 020\r"));
    QVERIFY(m_conn->startMove(RotorDirection::Up));
    QCOMPARE(m_fake->take(), QByteArray("U\r"));
    QVERIFY(m_conn->startMove(RotorDirection::Down));
    QCOMPARE(m_fake->take(), QByteArray("D\r"));
}

// ── GS-232A ──────────────────────────────────────────────────────────

void TestRotorConnection::gs232aWritesTheCitedBytesAndReadsItsReply()
{
    RotorConfig c;
    c.driver = RotorDriver::Gs232a;
    c.serialPort = QStringLiteral("COM4");
    c.baud = 4800;
    c.axes = RotorAxes::AzimuthElevation;
    connectWith(c);
    QVERIFY(m_target.serial);
    QCOMPARE(m_target.serialPort, QStringLiteral("COM4"));
    QCOMPARE(m_target.baud, 4800);
    QCOMPARE(m_fake->take(), QByteArray("C2\r"));

    // `+0aaa+0eee` ending LF (gs232a.c).
    m_fake->feed("+0302+0045\n");
    QCOMPARE(m_conn->azimuthDeg(), 302.0);
    QCOMPARE(m_conn->elevationDeg(), 45.0);
    m_conn->pollNowForTesting();
    QCOMPARE(m_fake->take(), QByteArray("C2\r"));
    m_fake->feed("+0310\n");
    QCOMPARE(m_conn->azimuthDeg(), 310.0);

    // Not GS-232A replies.
    QSignalSpy updated(m_conn.get(), &RotorConnection::positionUpdated);
    m_conn->pollNowForTesting();
    m_fake->take();
    m_fake->feed("AZ=302  EL=000\n");
    m_fake->feed("\n");
    m_fake->feed("+abc\n");
    QCOMPARE(updated.count(), 0);

    QVERIFY(m_conn->setTarget(90.0, 10.0));
    // Queued behind the outstanding poll.
    QCOMPARE(m_fake->take(), QByteArray());
    m_fake->feed("+0311+0045\n");
    QCOMPARE(m_fake->take(), QByteArray("W090 010\r"));
    m_conn->stop();
    QCOMPARE(m_fake->take(), QByteArray("S\r"));
    QVERIFY(m_conn->startMove(RotorDirection::Ccw));
    QCOMPARE(m_fake->take(), QByteArray("L\r"));
    QVERIFY(m_conn->startMove(RotorDirection::Cw));
    QCOMPARE(m_fake->take(), QByteArray("R\r"));
    QVERIFY(m_conn->startMove(RotorDirection::Up));
    QCOMPARE(m_fake->take(), QByteArray("U\r"));
    QVERIFY(m_conn->startMove(RotorDirection::Down));
    QCOMPARE(m_fake->take(), QByteArray("D\r"));
}

// ── rotctld ──────────────────────────────────────────────────────────

void TestRotorConnection::rotctldDialsTheHostAndPort()
{
    RotorConfig c;
    c.driver = RotorDriver::Rotctld;
    c.host = QStringLiteral("192.168.1.16");
    c.port = 4533;
    connectWith(c);
    QVERIFY(!m_target.serial);
    QCOMPARE(m_target.host, QStringLiteral("192.168.1.16"));
    QCOMPARE(m_target.port, quint16(4533));
    QCOMPARE(m_fake->take(), QByteArray("p\n"));
}

void TestRotorConnection::rotctldWritesTheCitedBytes()
{
    RotorConfig c;
    c.driver = RotorDriver::Rotctld;
    c.host = QStringLiteral("127.0.0.1");
    c.axes = RotorAxes::AzimuthElevation;
    connectWith(c);
    QCOMPARE(m_fake->take(), QByteArray("p\n"));
    m_fake->feed("302.500000\n12.000000\n");

    QVERIFY(m_conn->setTarget(292.0));
    QCOMPARE(m_fake->take(), QByteArray("P 292.00 12.00\n"));
    m_fake->feed("RPRT 0\n");
    QVERIFY(m_conn->setTarget(10.25, 30.0));
    QCOMPARE(m_fake->take(), QByteArray("P 10.25 30.00\n"));
    m_fake->feed("RPRT 0\n");
    m_conn->stop();
    QCOMPARE(m_fake->take(), QByteArray("S\n"));
    m_fake->feed("RPRT 0\n");

    // `M dir speed`: 8 left, 16 right, 2 up, 4 down.
    QVERIFY(m_conn->startMove(RotorDirection::Ccw));
    QCOMPARE(m_fake->take(), QByteArray("M 8 100\n"));
    m_fake->feed("RPRT 0\n");
    QVERIFY(m_conn->startMove(RotorDirection::Cw));
    QCOMPARE(m_fake->take(), QByteArray("M 16 100\n"));
    m_fake->feed("RPRT 0\n");
    QVERIFY(m_conn->startMove(RotorDirection::Up));
    QCOMPARE(m_fake->take(), QByteArray("M 2 100\n"));
    m_fake->feed("RPRT 0\n");
    QVERIFY(m_conn->startMove(RotorDirection::Down));
    QCOMPARE(m_fake->take(), QByteArray("M 4 100\n"));
    m_fake->feed("RPRT 0\n");

    // An azimuth rotor sends elevation 0.
    QCOMPARE(RotorConnection::setCommand(RotorDriver::Rotctld, 360.0, 0.0),
             QByteArray("P 0.00 0.00\n"));
}

void TestRotorConnection::rotctldReadsPositionsAndReports()
{
    RotorConfig c;
    c.driver = RotorDriver::Rotctld;
    c.host = QStringLiteral("127.0.0.1");
    c.axes = RotorAxes::AzimuthElevation;
    connectWith(c);
    QSignalSpy errors(m_conn.get(), &RotorConnection::rotorError);
    m_fake->take();

    // Two lines, possibly in pieces.
    m_fake->feed("302.5");
    m_fake->feed("00000\n");
    QCOMPARE(m_conn->azimuthDeg(), -1.0);
    m_fake->feed("12.000000\n");
    QCOMPARE(m_conn->azimuthDeg(), 302.5);
    QCOMPARE(m_conn->elevationDeg(), 12.0);

    // A refused set: `RPRT -11`.
    QVERIFY(m_conn->setTarget(100.0));
    m_fake->take();
    expectWarning("rotctld answered \"RPRT -11\"");
    m_fake->feed("RPRT -11\n");
    QCOMPARE(errors.count(), 1);
    QCOMPARE(errors.at(0).at(0).toInt(), -11);
    // Plain words for the operator; the code is in the log (final review M8).
    QCOMPARE(errors.at(0).at(1).toString(),
             QStringLiteral("The rotor controller refused that command."));

    // Hamlib backends that report -180 to 180 are wrapped.
    m_conn->pollNowForTesting();
    QCOMPARE(m_fake->take(), QByteArray("p\n"));
    m_fake->feed("-90.000000\n0.000000\n");
    QCOMPARE(m_conn->azimuthDeg(), 270.0);
}

void TestRotorConnection::rotctldInvalidRepliesAreNotPositions()
{
    RotorConfig c;
    c.driver = RotorDriver::Rotctld;
    c.host = QStringLiteral("127.0.0.1");
    connectWith(c);
    QSignalSpy updated(m_conn.get(), &RotorConnection::positionUpdated);
    QSignalSpy errors(m_conn.get(), &RotorConnection::rotorError);

    // `p` answered with an error: one line, never the number -1.
    QCOMPARE(m_fake->take(), QByteArray("p\n"));
    expectWarning("rotctld answered \"RPRT -1\"");
    m_fake->feed("RPRT -1\n");
    QCOMPARE(updated.count(), 0);
    QCOMPARE(errors.count(), 1);
    QCOMPARE(m_conn->azimuthDeg(), -1.0);

    // Not numbers.
    m_conn->pollNowForTesting();
    QCOMPARE(m_fake->take(), QByteArray("p\n"));
    m_fake->feed("abc\ndef\n");
    QCOMPARE(updated.count(), 0);

    // Not a number in range.
    m_conn->pollNowForTesting();
    QCOMPARE(m_fake->take(), QByteArray("p\n"));
    m_fake->feed("nan\n0.0\n");
    m_conn->pollNowForTesting();
    QCOMPARE(m_fake->take(), QByteArray("p\n"));
    m_fake->feed("999.0\n0.0\n");
    QCOMPARE(updated.count(), 0);
    QVERIFY(!m_conn->positionFresh());

    // A line with nothing awaited is dropped.
    m_fake->feed("RPRT 0\n");
    QCOMPARE(errors.count(), 1);

    // And the link still works.
    m_conn->pollNowForTesting();
    QCOMPARE(m_fake->take(), QByteArray("p\n"));
    m_fake->feed("45.000000\n0.000000\n");
    QCOMPARE(updated.count(), 1);
    QCOMPARE(m_conn->azimuthDeg(), 45.0);
}

void TestRotorConnection::rotctldNumbersIgnoreTheLocale()
{
    const QLocale before;
    QLocale::setDefault(QLocale(QLocale::German, QLocale::Germany));
    const QByteArray p = RotorConnection::setCommand(RotorDriver::Rotctld, 145.0, 0.0);
    QLocale::setDefault(before);
    QCOMPARE(p, QByteArray("P 145.00 0.00\n"));
}

// ── Polling, stop and freshness ──────────────────────────────────────

void TestRotorConnection::neverAPollWhileOneIsOutstanding()
{
    RotorConfig c;
    c.driver = RotorDriver::Gs232b;
    connectWith(c);
    QCOMPARE(m_fake->take(), QByteArray("C2\r"));
    m_conn->pollNowForTesting();
    m_conn->pollNowForTesting();
    QCOMPARE(m_fake->take(), QByteArray());
    // A bare CR is not the answer.
    m_fake->feed("\r");
    m_conn->pollNowForTesting();
    QCOMPARE(m_fake->take(), QByteArray());
    m_fake->feed("AZ=302  EL=000\r\n");
    m_conn->pollNowForTesting();
    QCOMPARE(m_fake->take(), QByteArray("C2\r"));
}

void TestRotorConnection::pollsFasterWhileTurning()
{
    RotorConfig c;
    c.driver = RotorDriver::Gs232b;
    RotorConnection::Timing t = steppedTiming();
    t.pollStillMs = RotorConnection::kPollStillMs * 1000;
    t.pollTurningMs = RotorConnection::kPollTurningMs * 1000;
    t.settleMs = 0;
    connectWith(c, t);
    m_fake->take();
    m_fake->feed("AZ=302  EL=000\r\n");
    QCOMPARE(m_conn->pollIntervalMs(), t.pollStillMs);
    QVERIFY(!m_conn->turning());

    QSignalSpy turning(m_conn.get(), &RotorConnection::turningChanged);
    QVERIFY(m_conn->setTarget(310.0));
    m_fake->take();
    QVERIFY(m_conn->turning());
    QCOMPARE(m_conn->pollIntervalMs(), t.pollTurningMs);
    QCOMPARE(turning.count(), 1);

    // Within 1.5 degrees of the target: arrived, still again.
    m_conn->pollNowForTesting();
    QCOMPARE(m_fake->take(), QByteArray("C2\r"));
    m_fake->feed("AZ=309  EL=000\r\n");
    QVERIFY(!m_conn->turning());
    QCOMPARE(m_conn->targetAzimuthDeg(), -1.0);
    QCOMPARE(m_conn->pollIntervalMs(), t.pollStillMs);

    // A move polls fast until stop, then until the heading settles.
    QVERIFY(m_conn->startMove(RotorDirection::Cw));
    m_fake->take();
    QCOMPARE(m_conn->pollIntervalMs(), t.pollTurningMs);
    m_conn->pollNowForTesting();
    m_fake->take();
    m_fake->feed("AZ=312  EL=000\r\n");
    QVERIFY(m_conn->turning());   // a move never settles by itself
    m_conn->stop();
    m_fake->take();
    QVERIFY(m_conn->turning());
    // settleMs 0: the next unchanged reply settles it.
    m_conn->pollNowForTesting();
    m_fake->take();
    m_fake->feed("AZ=312  EL=000\r\n");
    QVERIFY(!m_conn->turning());
}

void TestRotorConnection::stopIsWrittenAheadOfAnythingQueued()
{
    RotorConfig c;
    c.driver = RotorDriver::Gs232b;
    connectWith(c);
    QCOMPARE(m_fake->take(), QByteArray("C2\r"));
    m_fake->feed("AZ=302  EL=000\r\n");

    m_conn->pollNowForTesting();
    QCOMPARE(m_fake->take(), QByteArray("C2\r"));   // outstanding
    QVERIFY(m_conn->setTarget(100.0));
    QVERIFY(m_conn->startMove(RotorDirection::Cw));
    QCOMPARE(m_fake->take(), QByteArray());         // queued behind it
    m_conn->stop();
    QCOMPARE(m_fake->take(), QByteArray("S\r"));    // at once
    QCOMPARE(m_conn->targetAzimuthDeg(), -1.0);
    QVERIFY(!m_conn->moveActive());

    // The queued turns were dropped: the poll's answer releases nothing.
    m_fake->feed("AZ=302  EL=000\r\n");
    QCOMPARE(m_fake->take(), QByteArray());
}

void TestRotorConnection::rotctldStopIsWrittenAheadOfAnythingQueued()
{
    RotorConfig c;
    c.driver = RotorDriver::Rotctld;
    c.host = QStringLiteral("127.0.0.1");
    connectWith(c);
    QCOMPARE(m_fake->take(), QByteArray("p\n"));    // outstanding
    QVERIFY(m_conn->setTarget(100.0));
    QCOMPARE(m_fake->take(), QByteArray());
    m_conn->stop();
    QCOMPARE(m_fake->take(), QByteArray("S\n"));

    // rotctld answers in order: the position, then the stop's report.
    QSignalSpy errors(m_conn.get(), &RotorConnection::rotorError);
    m_fake->feed("292.000000\n0.000000\nRPRT 0\n");
    QCOMPARE(m_conn->azimuthDeg(), 292.0);
    QCOMPARE(errors.count(), 0);
    QCOMPARE(m_fake->take(), QByteArray());         // the set was dropped
    m_conn->pollNowForTesting();
    QCOMPARE(m_fake->take(), QByteArray("p\n"));
}

void TestRotorConnection::staleAfter1500msThenFreshOnTheNextReply()
{
    RotorConfig c;
    c.driver = RotorDriver::Gs232b;
    c.endStop = EndStop::South;
    c.rangeDeg = RotorRoute::kOverlapRangeDeg;
    RotorConnection::Timing t = steppedTiming();
    t.staleMs = RotorConnection::kStaleMs;   // the real 1500 ms
    connectWith(c, t);
    QSignalSpy fresh(m_conn.get(), &RotorConnection::positionFreshChanged);

    QCOMPARE(m_fake->take(), QByteArray("C2\r"));
    QElapsedTimer clock;
    clock.start();
    m_fake->feed("AZ=302  EL=000\r\n");
    QVERIFY(m_conn->positionFresh());
    QCOMPARE(m_conn->spanPositionDeg(), 122.0);
    QCOMPARE(fresh.count(), 1);

    QTRY_VERIFY_WITH_TIMEOUT(!m_conn->positionFresh(), 3000);
    QVERIFY2(clock.elapsed() >= RotorConnection::kStaleMs - 50,
             qPrintable(QString::number(clock.elapsed())));
    QCOMPARE(fresh.count(), 2);
    // The last heard heading stays; the span must be placed again.
    QCOMPARE(m_conn->azimuthDeg(), 302.0);
    QVERIFY(!m_conn->spanKnown());

    // The next reply: fresh again, and placed again.
    m_conn->pollNowForTesting();
    m_fake->take();
    m_fake->feed("AZ=303  EL=000\r\n");
    QVERIFY(m_conn->positionFresh());
    QCOMPARE(fresh.count(), 3);
    QCOMPARE(m_conn->spanPositionDeg(), 123.0);
}

// Bench fix: JJ chose /dev/ttyFIQ0 (the board's debug console). The port
// opened, nothing answered, and the Core said "connected".
void TestRotorConnection::aSilentPortIsNotConnectedAndFaultsAtTheDeadline()
{
    RotorConfig c;
    c.driver = RotorDriver::Gs232b;
    c.serialPort = QStringLiteral("/dev/ttyFIQ0");
    RotorConnection::Timing t = steppedTiming();
    t.answerDeadlineMs = 50;
    QSignalSpy up(m_conn.get(), &RotorConnection::connected);
    QSignalSpy failed(m_conn.get(), &RotorConnection::connectionFailed);
    QSignalSpy scheduled(m_conn.get(), &RotorConnection::reconnectScheduled);
    connectWith(c, t);
    QCOMPARE(m_fake->take(), QByteArray("C2\r"));   // it polls
    QVERIFY(!m_conn->isConnected());
    QVERIFY(!m_conn->setTarget(100.0));
    QCOMPARE(m_fake->take(), QByteArray());           // and sends no turn

    const QString reason = QStringLiteral(
        "The rotor controller on /dev/ttyFIQ0 is not answering. "
        "Check the serial port and the baud rate.");
    expectWarning("is not answering");
    QTRY_COMPARE_WITH_TIMEOUT(failed.count(), 1, 2000);
    QCOMPARE(failed.at(0).at(0).toString(), reason);
    QCOMPARE(m_conn->lastError(), reason);
    QCOMPARE(up.count(), 0);
    QVERIFY(!m_conn->isConnected());
    QCOMPARE(scheduled.count(), 1);   // dialled again on the schedule
    QVERIFY(m_fake == nullptr || m_transportsMade == 1);
}

void TestRotorConnection::aPortAnsweringGarbageIsNotConnected()
{
    RotorConfig c;
    c.driver = RotorDriver::Gs232b;
    c.serialPort = QStringLiteral("/dev/ttyS2");
    RotorConnection::Timing t = steppedTiming();
    t.answerDeadlineMs = 50;
    QSignalSpy up(m_conn.get(), &RotorConnection::connected);
    QSignalSpy failed(m_conn.get(), &RotorConnection::connectionFailed);
    connectWith(c, t);
    // A console's chatter, and a heading out of range: neither is a rotor.
    m_fake->feed("U-Boot 2017.09\r\nlogin: \r\n");
    m_fake->feed("AZ=999\r\n");
    QVERIFY(!m_conn->isConnected());
    QCOMPARE(up.count(), 0);
    expectWarning("is not answering");
    QTRY_COMPARE_WITH_TIMEOUT(failed.count(), 1, 2000);
    QVERIFY(!m_conn->isConnected());
    QCOMPARE(up.count(), 0);
}

void TestRotorConnection::failedOpenIsRetried()
{
    RotorConfig c;
    c.driver = RotorDriver::Gs232b;
    c.serialPort = QStringLiteral("/dev/ttyUSB9");
    RotorConnection::Timing t = steppedTiming();
    t.reconnectUnitMs = 10;

    int made = 0;
    m_conn->setTransportFactoryForTesting([&made, this](const RotorTransportTarget&) {
        auto fake = std::make_unique<FakeTransport>();
        fake->failOpen = (++made == 1);
        m_fake = fake.get();
        return std::unique_ptr<RotorTransport>(std::move(fake));
    });
    QSignalSpy failed(m_conn.get(), &RotorConnection::connectionFailed);
    QSignalSpy scheduled(m_conn.get(), &RotorConnection::reconnectScheduled);
    QSignalSpy up(m_conn.get(), &RotorConnection::connected);
    expectWarning("Could not open the fake port");
    connectWith(c, t);
    QCOMPARE(failed.count(), 1);
    QCOMPARE(scheduled.count(), 1);
    QCOMPARE(scheduled.at(0).at(1).toInt(), 10);   // 1 s on the schedule
    QVERIFY(!m_conn->isConnected());
    QTRY_COMPARE_WITH_TIMEOUT(made, 2, 2000);
    // Connected on the first answer.
    m_fake->take();
    m_fake->feed("AZ=302  EL=000\r\n");
    QVERIFY(m_conn->isConnected());
    QCOMPARE(up.count(), 1);

    // A dropped link: disconnected, headings forgotten, dialled again.
    QSignalSpy down(m_conn.get(), &RotorConnection::disconnected);
    expectWarning("The serial port went away");
    emit m_fake->dropped(QStringLiteral("The serial port went away."));
    QCOMPARE(down.count(), 1);
    QCOMPARE(m_conn->azimuthDeg(), -1.0);
    QVERIFY(!m_conn->positionFresh());
    QTRY_COMPARE_WITH_TIMEOUT(made, 3, 2000);
    m_fake->feed("AZ=302  EL=000\r\n");
    QVERIFY(m_conn->isConnected());

    // An explicit disconnect stops retrying.
    m_conn->disconnectFromRotor();
    QVERIFY(!m_conn->reconnectPending());
}

// ── A stop before the link closes mid-turn ──────────────────────────

namespace {
RotorConfig gs232bConfig()
{
    RotorConfig c;
    c.driver = RotorDriver::Gs232b;
    c.serialPort = QStringLiteral("/dev/ttyUSB0");
    return c;
}
} // namespace

void TestRotorConnection::disconnectDuringAMoveSendsAStop()
{
    connectWith(gs232bConfig());
    m_fake->feed("AZ=100  EL=000\r\n");
    QVERIFY(m_conn->isConnected());
    m_fake->take();
    QVERIFY(m_conn->startMove(RotorDirection::Cw));
    QCOMPARE(m_fake->take(), QByteArray("R\r"));
    const QPointer<FakeTransport> link = m_fake;
    m_conn->disconnectFromRotor();
    QVERIFY(link);
    QCOMPARE(link->written, QByteArray("S\r"));
    QVERIFY(link->closed);
}

void TestRotorConnection::disconnectDuringATargetTurnSendsAStop()
{
    connectWith(gs232bConfig());
    m_fake->feed("AZ=100  EL=000\r\n");
    m_fake->take();
    QVERIFY(m_conn->setTarget(200.0));
    QCOMPARE(m_fake->take(), QByteArray("W200 000\r"));
    const QPointer<FakeTransport> link = m_fake;
    m_conn->disconnectFromRotor();
    QCOMPARE(link->written, QByteArray("S\r"));
}

void TestRotorConnection::reconnectDuringAMoveStopsTheOldLink()
{
    // A new setup reconnects (StationRotorController::connectNow and
    // connectToRotor): the old link's turn is stopped before it closes.
    connectWith(gs232bConfig());
    m_fake->feed("AZ=100  EL=000\r\n");
    m_fake->take();
    QVERIFY(m_conn->startMove(RotorDirection::Ccw));
    QCOMPARE(m_fake->take(), QByteArray("L\r"));
    const QPointer<FakeTransport> oldLink = m_fake;
    QVERIFY(m_conn->connectToRotor());
    QCOMPARE(oldLink->written, QByteArray("S\r"));
    QVERIFY(oldLink->closed);
    QVERIFY(m_fake != oldLink);
    QCOMPARE(m_fake->take(), QByteArray("C2\r"));   // the new link polls
}

void TestRotorConnection::rotctldDisconnectDuringATurnSendsItsStop()
{
    RotorConfig c;
    c.driver = RotorDriver::Rotctld;
    c.host = QStringLiteral("127.0.0.1");
    connectWith(c);
    QCOMPARE(m_fake->take(), QByteArray("p\n"));
    m_fake->feed("100.000000\n0.000000\n");
    QVERIFY(m_conn->isConnected());
    QVERIFY(m_conn->startMove(RotorDirection::Cw));
    QVERIFY(m_fake->take().startsWith("M "));
    m_fake->feed("RPRT 0\n");
    const QPointer<FakeTransport> link = m_fake;
    m_conn->disconnectFromRotor();
    QCOMPARE(link->written, QByteArray("S\n"));
}

void TestRotorConnection::teardownDuringAMoveSendsAStop()
{
    // The desktop's role switch and nereusd's exit destroy the connection.
    connectWith(gs232bConfig());
    m_fake->feed("AZ=100  EL=000\r\n");
    m_fake->take();
    QVERIFY(m_conn->startMove(RotorDirection::Cw));
    m_fake->take();
    const QPointer<FakeTransport> link = m_fake;
    m_conn.reset();
    QVERIFY(link);   // deleted later, by the event loop
    QCOMPARE(link->written, QByteArray("S\r"));
}

void TestRotorConnection::quittingDuringAMoveSendsAStop()
{
    connectWith(gs232bConfig());
    m_fake->feed("AZ=100  EL=000\r\n");
    m_fake->take();
    QVERIFY(m_conn->setTarget(300.0));
    m_fake->take();
    // aboutToQuit is a private signal: reach it the way QCoreApplication
    // does, by name.
    QVERIFY(QMetaObject::invokeMethod(QCoreApplication::instance(), "aboutToQuit",
                                      Qt::DirectConnection));
    QCOMPARE(m_fake->take(), QByteArray("S\r"));
}

void TestRotorConnection::arrivalInTheOverlapIsJudgedOnTheSpan()
{
    // South stop, 450: span 100 is compass 280. A target of 200 has span
    // positions 20 and 380; the route takes 20, counter-clockwise 80.
    RotorConfig c = gs232bConfig();
    c.endStop = EndStop::South;
    c.rangeDeg = 450.0;
    connectWith(c);
    m_fake->feed("AZ=280  EL=000\r\n");
    QCOMPARE(m_conn->spanPositionDeg(), 100.0);
    m_fake->take();
    QVERIFY(m_conn->setTarget(200.0));
    QCOMPARE(m_conn->routeToTarget().travelDeg, -80.0);
    QVERIFY(m_conn->turning());
    // Two degrees short on the span is not there.
    m_conn->pollNowForTesting();
    m_fake->feed("AZ=202  EL=000\r\n");
    QVERIFY(m_conn->turning());
    QCOMPARE(m_conn->targetAzimuthDeg(), 200.0);
    // Within 1.5 on the span is.
    m_conn->pollNowForTesting();
    m_fake->feed("AZ=201  EL=000\r\n");
    QVERIFY(!m_conn->turning());
    QCOMPARE(m_conn->targetAzimuthDeg(), -1.0);
}

void TestRotorConnection::arrivalAtTheClockwiseStopIsJudgedOnTheSpan()
{
    // Re-review M1: north stop, range 360, the rotor at its clockwise stop
    // reading 360 (span 360). Target 001 sits only at span 1 (its other
    // place, 361, is past the stop), so the way there is -359. On the
    // compass 360 and 001 are 1 apart, which the old rule called arrived.
    RotorConfig c = gs232bConfig();
    c.endStop = EndStop::North;
    c.rangeDeg = 360.0;
    connectWith(c);
    m_fake->feed("AZ=350  EL=000\r\n");
    m_conn->pollNowForTesting();
    m_fake->feed("AZ=355  EL=000\r\n");
    m_conn->pollNowForTesting();
    m_fake->feed("AZ=360  EL=000\r\n");
    QCOMPARE(m_conn->spanPositionDeg(), 360.0);
    m_fake->take();
    QVERIFY(m_conn->setTarget(1.0));
    QCOMPARE(m_conn->routeToTarget().travelDeg, -359.0);
    m_conn->pollNowForTesting();
    m_fake->feed("AZ=360  EL=000\r\n");
    // Still turning, with all of -359 to go.
    QVERIFY(m_conn->turning());
    QCOMPARE(m_conn->targetAzimuthDeg(), 1.0);
    QCOMPARE(m_conn->routeToTarget().travelDeg, -359.0);
}

void TestRotorConnection::disconnectWhileStillSendsNothing()
{
    connectWith(gs232bConfig());
    m_fake->feed("AZ=100  EL=000\r\n");
    m_fake->take();
    const QPointer<FakeTransport> link = m_fake;
    m_conn->disconnectFromRotor();
    QCOMPARE(link->written, QByteArray());
}

// ── Bench captures ───────────────────────────────────────────────────

namespace {

// Replays a capture through a connected GS-232B link: every `C2` and `C`
// line is answered to one of our polls; every `W`, `S`, `L` and `R` is
// sent through the connection and the bytes compared; `M` (the ERC's span
// command, which the Core does not send) and `B` only have their reply
// fed. Returns false with a message on the first mismatch. After each
// step, the span position and heading are recorded under "run/step".
struct ReplayResult {
    QString error;
    int polls{0};
    int commands{0};
    QMap<QString, double> span;
    QMap<QString, double> heading;
};

} // namespace

static ReplayResult replay(const QVector<CaptureLine>& lines,
                           const std::function<RotorConnection*()>& reconnect,
                           const std::function<FakeTransport*()>& fake)
{
    static const QRegularExpression kAz(QStringLiteral("AZ=(\\d+)"));
    ReplayResult r;
    QString run;
    RotorConnection* conn = nullptr;
    for (const CaptureLine& l : lines) {
        if (l.run != run) {
            run = l.run;
            conn = reconnect();
            if (!conn || !fake()) { r.error = QStringLiteral("no connection"); return r; }
            fake()->take();   // the poll sent on connect
            conn->pollNowForTesting();   // nothing: it is outstanding
            if (!fake()->take().isEmpty()) {
                r.error = QStringLiteral("poll while one was outstanding");
                return r;
            }
            // Answer the connect poll with the first reply of the run.
        }
        FakeTransport* f = fake();
        const QString where = QStringLiteral("%1 step %2 '%3'").arg(run).arg(l.step).arg(l.sent);
        const QString key = QStringLiteral("%1/%2").arg(run).arg(l.step);

        if (l.sent == QLatin1String("C2") || l.sent == QLatin1String("C")) {
            conn->pollNowForTesting();
            const QByteArray w = f->take();
            if (!w.isEmpty() && w != QByteArray("C2\r")) {
                r.error = where + QStringLiteral(": wrote ") + QString::fromLatin1(w);
                return r;
            }
            f->feed(l.reply);
            const QRegularExpressionMatch m = kAz.match(QString::fromLatin1(l.reply));
            const double expect = RotorRoute::wrap360(m.captured(1).toDouble());
            if (conn->azimuthDeg() != expect || !conn->positionFresh()) {
                r.error = where + QStringLiteral(": heading %1, expected %2")
                                      .arg(conn->azimuthDeg()).arg(expect);
                return r;
            }
            ++r.polls;
        } else if (l.sent == QLatin1String("B") || l.sent.startsWith(QLatin1Char('M'))) {
            const double was = conn->azimuthDeg();
            f->feed(l.reply);
            if (conn->azimuthDeg() != was) {
                r.error = where + QStringLiteral(": an acknowledgement moved the heading");
                return r;
            }
        } else {
            QByteArray expect;
            if (l.sent.startsWith(QLatin1Char('W'))) {
                const double az = l.sent.mid(1, 3).toDouble();
                if (!conn->setTarget(az)) { r.error = where + QStringLiteral(": refused"); return r; }
                expect = (az == 360.0) ? QByteArray("W000 000\r")   // 360 is sent as 0
                                       : (l.sent + QStringLiteral("\r")).toLatin1();
            } else if (l.sent == QLatin1String("S")) {
                conn->stop();
                expect = "S\r";
            } else if (l.sent == QLatin1String("L")) {
                conn->startMove(RotorDirection::Ccw);
                expect = "L\r";
            } else if (l.sent == QLatin1String("R")) {
                conn->startMove(RotorDirection::Cw);
                expect = "R\r";
            } else {
                r.error = where + QStringLiteral(": unknown command");
                return r;
            }
            const QByteArray w = f->take();
            if (w != expect) {
                r.error = where + QStringLiteral(": wrote '%1'").arg(QString::fromLatin1(w));
                return r;
            }
            const double was = conn->azimuthDeg();
            f->feed(l.reply);   // the bare CR
            if (conn->azimuthDeg() != was) {
                r.error = where + QStringLiteral(": the CR moved the heading");
                return r;
            }
            ++r.commands;
        }
        r.span[key] = conn->spanPositionDeg();
        r.heading[key] = conn->azimuthDeg();
    }
    return r;
}

void TestRotorConnection::replayErcCapture()
{
    const QVector<CaptureLine> lines =
        readCapture(QStringLiteral("erc-gs232b-capture-2026-10-07.log"));
    QVERIFY2(lines.size() > 20, "capture not found");

    RotorConfig c;
    c.driver = RotorDriver::Gs232b;
    c.serialPort = QStringLiteral("/dev/ttyUSB0");
    c.endStop = EndStop::South;
    c.rangeDeg = RotorRoute::kOverlapRangeDeg;
    m_conn->setTimingForTesting(steppedTiming());
    m_conn->configure(c);

    const ReplayResult r = replay(lines,
        [this]() -> RotorConnection* {
            m_conn->disconnectFromRotor();
            return m_conn->connectToRotor() ? m_conn.get() : nullptr;
        },
        [this]() { return m_fake.data(); });
    QVERIFY2(r.error.isEmpty(), qPrintable(r.error));
    QCOMPARE(r.polls, 25);   // every `C2` and `C` line
    // W292 000, R, L and five S; M312 is the ERC's own command, not sent.
    QCOMPARE(r.commands, 8);

    // Step 1, M312 from 302: stopped at 313. Step 2, W292 000: 292.
    QCOMPARE(r.heading.value(QStringLiteral("capture/1")), 313.0);
    QCOMPARE(r.heading.value(QStringLiteral("capture/2")), 292.0);
    // Step 3, R: 299. Step 4, L: 292. On the south-stop rotor, 292 is
    // span 112 and 313 span 133.
    QCOMPARE(r.heading.value(QStringLiteral("capture/3")), 299.0);
    QCOMPARE(r.heading.value(QStringLiteral("capture/4")), 292.0);
    QCOMPARE(r.span.value(QStringLiteral("capture/1")), 133.0);
    QCOMPARE(r.span.value(QStringLiteral("capture/4")), 112.0);
}

void TestRotorConnection::replayErcRangeCapture()
{
    const QVector<CaptureLine> lines =
        readCapture(QStringLiteral("erc-gs232b-range-2026-10-07.log"));
    QVERIFY2(lines.size() > 200, "range capture not found");

    RotorConfig c;
    c.driver = RotorDriver::Gs232b;
    c.serialPort = QStringLiteral("/dev/ttyUSB0");
    c.endStop = EndStop::South;
    c.rangeDeg = RotorRoute::kOverlapRangeDeg;
    m_conn->setTimingForTesting(steppedTiming());
    m_conn->configure(c);

    // Each run was its own session on the bench: connect afresh.
    const ReplayResult r = replay(lines,
        [this]() -> RotorConnection* {
            m_conn->disconnectFromRotor();
            return m_conn->connectToRotor() ? m_conn.get() : nullptr;
        },
        [this]() { return m_fake.data(); });
    QVERIFY2(r.error.isEmpty(), qPrintable(r.error));
    QCOMPARE(r.polls, 237);     // every `C2` and `C` line
    QCOMPARE(r.commands, 26);   // every `W`, `S`, `L` and `R` line

    // erc-range starts at 292 (span 112); W000 ends at north, span 180.
    QCOMPARE(r.span.value(QStringLiteral("erc-range/0")), 112.0);
    QCOMPARE(r.span.value(QStringLiteral("erc-range/1")), 180.0);
    QCOMPARE(r.heading.value(QStringLiteral("erc-range/1")), 0.0);
    // M450 went into the overlap and read 090: span 270.
    QCOMPARE(r.span.value(QStringLiteral("erc-range/2")), 270.0);
    // erc-ccw: W180 from 301 went to the counter-clockwise end (reads
    // 183, span 3); W010 from there went clockwise through north, span 190.
    QCOMPARE(r.span.value(QStringLiteral("erc-ccw/5")), 3.0);
    QCOMPARE(r.span.value(QStringLiteral("erc-ccw/6")), 190.0);
    // erc-ends: L held to the counter-clockwise stop (183, span 3), then R
    // held to the clockwise stop: AZ=269, span 449.
    QCOMPARE(r.span.value(QStringLiteral("erc-ends/10")), 3.0);
    QCOMPARE(r.heading.value(QStringLiteral("erc-ends/10")), 183.0);
    QCOMPARE(r.span.value(QStringLiteral("erc-ends/11")), 449.0);
    QCOMPARE(r.heading.value(QStringLiteral("erc-ends/11")), 269.0);
    QCOMPARE(r.span.value(QStringLiteral("erc-ends/12")), 121.0);
}

// ── Driver 4 ─────────────────────────────────────────────────────────

void TestRotorConnection::driver4StartsRotctldAndStopsItOnDisconnect()
{
#ifdef Q_OS_WIN
    QSKIP("The rotctld stand-in is a POSIX shell script.");
#endif
    const QString out = m_dir->filePath(QStringLiteral("args"));
    const QString script = writeStandIn(QStringLiteral(
        "#!/bin/sh\n"
        "trap 'echo stopped >> \"%1\"; exit 0' TERM\n"
        "echo \"$@\" > \"%1\"\n"
        "while :; do sleep 0.2 & wait $!; done\n").arg(out));
    QVERIFY(!script.isEmpty());
    RotctldProcess::setBinaryOverrideForTesting(script);

    RotorConfig c;
    c.driver = RotorDriver::RotctldStarted;
    c.hamlibModel = 404;
    c.serialPort = QStringLiteral("/dev/ttyUSB0");
    c.baud = 9600;
    c.port = 4533;
    connectWith(c);

    QVERIFY(m_conn->rotctldProcess()->isRunning());
    const quint16 port = m_conn->rotctldPort();
    QVERIFY(port != 0);
    // It talks to the rotctld it started, on loopback, as driver 3.
    QTRY_VERIFY_WITH_TIMEOUT(m_conn->isConnected(), 2000);
    QVERIFY(!m_target.serial);
    QCOMPARE(m_target.host, QStringLiteral("127.0.0.1"));
    QCOMPARE(m_target.port, port);
    QCOMPARE(m_fake->take(), QByteArray("p\n"));

    QTRY_VERIFY_WITH_TIMEOUT(QFileInfo(out).size() > 0, 3000);
    QFile f(out);
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QString args = QString::fromUtf8(f.readAll()).trimmed();
    f.close();
    QCOMPARE(args, RotctldProcess::arguments(404, QStringLiteral("/dev/ttyUSB0"),
                                             9600, port).join(QLatin1Char(' ')));
    QCOMPARE(args, QStringLiteral("-m 404 -r /dev/ttyUSB0 -s 9600 -T 127.0.0.1 -t %1")
                       .arg(port));

    m_conn->disconnectFromRotor();
    QVERIFY(!m_conn->rotctldProcess()->isRunning());
    QFile g(out);
    QVERIFY(g.open(QIODevice::ReadOnly));
    QVERIFY(QString::fromUtf8(g.readAll()).contains(QStringLiteral("stopped")));
}

void TestRotorConnection::driver4StopsRotctldOnExit()
{
#ifdef Q_OS_WIN
    QSKIP("The rotctld stand-in is a POSIX shell script.");
#endif
    const QString out = m_dir->filePath(QStringLiteral("args"));
    const QString script = writeStandIn(QStringLiteral(
        "#!/bin/sh\n"
        "trap 'echo stopped >> \"%1\"; exit 0' TERM\n"
        "echo \"$@\" > \"%1\"\n"
        "while :; do sleep 0.2 & wait $!; done\n").arg(out));
    RotctldProcess::setBinaryOverrideForTesting(script);

    RotorConfig c;
    c.driver = RotorDriver::RotctldStarted;
    c.hamlibModel = 603;
    c.serialPort = QStringLiteral("/dev/ttyUSB0");
    c.baud = 9600;
    connectWith(c);
    QVERIFY(m_conn->rotctldProcess()->isRunning());
    QTRY_VERIFY_WITH_TIMEOUT(QFileInfo(out).size() > 0, 3000);

    // The connection goes away (the Core quitting): rotctld goes too.
    m_conn.reset();
    QFile g(out);
    QVERIFY(g.open(QIODevice::ReadOnly));
    QVERIFY(QString::fromUtf8(g.readAll()).contains(QStringLiteral("stopped")));
}

void TestRotorConnection::driver4WithoutRotctldRefusesWithTheDocumentReason()
{
    RotctldProcess::setBinaryOverrideForTesting(QString());
    RotorConfig c;
    c.driver = RotorDriver::RotctldStarted;
    c.hamlibModel = 404;
    c.serialPort = QStringLiteral("/dev/ttyUSB0");
    m_conn->configure(c);
    QSignalSpy failed(m_conn.get(), &RotorConnection::connectionFailed);
    expectWarning("rotctld is not installed");
    QVERIFY(!m_conn->connectToRotor());
    QCOMPARE(failed.count(), 1);
    QCOMPARE(failed.at(0).at(0).toString(),
             QStringLiteral("Hamlib's rotctld is not installed on the Core's computer."));
    QCOMPARE(m_conn->lastError(), RotctldProcess::notInstalledReason());
    QCOMPARE(m_transportsMade, 0);
    QVERIFY(!m_conn->reconnectPending());
}

void TestRotorConnection::driver4ReportsWhyRotctldExited()
{
#ifdef Q_OS_WIN
    QSKIP("The rotctld stand-in is a POSIX shell script.");
#endif
    // Hamlib's stderr from Longpath's bench (tst_rotctld_process.cpp):
    // the cause is the timed-out line, not the closing "IO error".
    const QString script = writeStandIn(QStringLiteral(
        "#!/bin/sh\n"
        "sleep 0.3\n"
        "echo 'connect to 192.168.1.16:4001 failed, (trying next interface): "
        "Network error 60: Operation timed out' >&2\n"
        "echo 'IO error' >&2\n"
        "exit 1\n"));
    RotctldProcess::setBinaryOverrideForTesting(script);

    RotorConfig c;
    c.driver = RotorDriver::RotctldStarted;
    c.hamlibModel = 601;
    c.serialPort = QStringLiteral("192.168.1.16:4001");
    RotorConnection::Timing t = steppedTiming();
    t.reconnectUnitMs = 3600000;
    connectWith(c, t);
    QTRY_VERIFY_WITH_TIMEOUT(m_conn->isConnected(), 2000);

    QSignalSpy failed(m_conn.get(), &RotorConnection::connectionFailed);
    QSignalSpy down(m_conn.get(), &RotorConnection::disconnected);
    expectWarning("rotctld exited on its own, code 1");
    // Hamlib's own line goes to the log; the operator reads plain words.
    expectWarning("rotctld stopped: connect to 192.168.1.16:4001");
    expectWarning("rotctld stopped: the rotor controller did not answer in time");
    QTRY_COMPARE_WITH_TIMEOUT(failed.count(), 1, 5000);
    const QString why = failed.at(0).at(0).toString();
    QCOMPARE(why, QStringLiteral("Hamlib's rotctld stopped: the rotor controller did not "
                                 "answer in time. Check the rotor's port and that it is on."));
    QVERIFY2(!why.contains(QStringLiteral("192.168")), qPrintable(why));
    QCOMPARE(down.count(), 1);
    QVERIFY(!m_conn->isConnected());
    QVERIFY(m_conn->reconnectPending());
}

QTEST_GUILESS_MAIN(TestRotorConnection)
#include "tst_rotor_connection.moc"
