// =================================================================
// tests/tst_remote_tx_watchdog.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test file.
//
// iPhone app plan Task 37 (R-IOS-13; remote design section 12.1, pairing
// design section 9.7, spec section 4.6 item 1): the transmit watchdog's
// rules on an injected clock and timer. Keepalives every 100 ms hold a key
// for 10 minutes; one missed at the end holds the key's RF silent 401 ms
// after the last and cuts it 3001 ms after the last; a keepalive that
// counts during the hold ends it; the link closing stops it at once; VOX
// armed is watched too (cut, never held); copies, overtaken keepalives and
// older epochs do not count; the device's own release ends the watch, and
// any hold, at once. And why the keepalives travel on their own unordered
// channel: at 5 % loss that channel never holds the key in 10 minutes,
// while the same loss on a reliable, in-order channel (retransmission after
// a timeout, later keepalives held behind the lost one) does.
//
// Timing uses an injected clock and timer, never sleeps.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original test for NereusSDR by J.J. Boyd (KG4VCF), iPhone
//               app plan Task 37 (R-IOS-13), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-29: a trip's warning names the device id in hex, by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-10-01: TX mic thread: a stall of the Core's event loop with
//               keepalives arriving keeps the key, whichever runs first
//               after it; a silent link still stops 401 ms after its last
//               keepalive, or at the end of a stall that outlasts that. By
//               J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-10-01: TX mic thread fix round 2: another device's keepalives
//               arriving on every turn after a stall cannot hold a dead
//               link's key past one turn, and a check only a few ms late
//               is judged at once. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-10-01: TX watch follow-up: the dead link stops in time whether
//               the check or the other device's drain runs first in each
//               turn. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//               Code.
//   2026-10-15: a quiet trip from the off-main cut guard (tripQuiet)
//               stops once in either order. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-16: a quiet trip reports the guard's measured silence when
//               this clock reads less, never less than its own. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26: Remote transmit hold then cut: a quiet keyed link holds RF
//               silent after 400 ms and cuts after 3000 ms; the hold's
//               start and end in order with the stop, its late turn, and
//               the new chain 120 < 250 < 400 < 1000 < 3000. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26: Remote transmit hold then cut, fix wave A: keepalives that
//               count but arrive 450 ms old (a lagging main thread) cannot
//               hold a key past heldSinceMs + 2600 ms, on time or with
//               every check late; a fresh one still ends such a hold. J.J.
//               Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/safety/RemoteTxWatchdog.h"

#include "OperatorWording.h"

#include <QRandomGenerator>
#include <QStringList>
#include <QtTest>

#include <algorithm>
#include <functional>
#include <optional>
#include <vector>

using namespace NereusSDR;

namespace {

const QByteArray kPhone = QByteArrayLiteral("phone-1");
const QByteArray kTablet = QByteArrayLiteral("tablet-2");

QString holdEndName(RemoteTxWatchdog::HoldEnd how)
{
    switch (how) {
    case RemoteTxWatchdog::HoldEnd::Heard:
        return QStringLiteral("Heard");
    case RemoteTxWatchdog::HoldEnd::Cut:
        return QStringLiteral("Cut");
    case RemoteTxWatchdog::HoldEnd::KeyEnded:
        return QStringLiteral("KeyEnded");
    case RemoteTxWatchdog::HoldEnd::LinkClosed:
        return QStringLiteral("LinkClosed");
    }
    return QString();
}

// The watchdog with a clock and one timer the test drives.
struct Rig {
    qint64 nowMs = 0;
    std::optional<qint64> timerDue;
    // TX watch follow-up: how many times the timer was started, so a test
    // can tell a check restarted during a turn of the event loop (which
    // runs in a later turn) from one left due (which runs in this one).
    int starts = 0;
    struct Stop {
        QByteArray device;
        QString message;
        qint64 atMs;
    };
    std::vector<Stop> stops;
    // Hold then cut: each hold(id, held) the watchdog asked for.
    struct Hold {
        QByteArray device;
        bool held;
        qint64 atMs;
    };
    std::vector<Hold> holds;
    struct HoldStart {
        QByteArray device;
        qint64 silentMs;
    };
    std::vector<HoldStart> holdStarts;
    struct HoldEnd {
        QByteArray device;
        qint64 lengthMs;
        RemoteTxWatchdog::HoldEnd how;
    };
    std::vector<HoldEnd> holdEnds;
    // The hooks and signals in the order they came.
    QStringList events;
    RemoteTxWatchdog watchdog;

    Rig()
    {
        RemoteTxWatchdog::Hooks hooks;
        hooks.clock = [this] { return nowMs; };
        hooks.startTimer = [this](int ms) {
            timerDue = nowMs + ms;
            ++starts;
        };
        hooks.stopTimer = [this] { timerDue.reset(); };
        hooks.stop = [this](const QByteArray& device, const QString& message) {
            stops.push_back({device, message, nowMs});
            events << QStringLiteral("stop");
        };
        hooks.deviceName = [](const QByteArray& device) {
            return device == kPhone ? QStringLiteral("JJ's iPhone") : QStringLiteral("Shack iPad");
        };
        hooks.hold = [this](const QByteArray& device, bool held) {
            holds.push_back({device, held, nowMs});
            events << (held ? QStringLiteral("hold on") : QStringLiteral("hold off"));
        };
        watchdog.setHooks(std::move(hooks));
        QObject::connect(&watchdog, &RemoteTxWatchdog::tripped,
                         [this](const QByteArray&, bool, qint64) {
                             events << QStringLiteral("tripped");
                         });
        QObject::connect(&watchdog, &RemoteTxWatchdog::holdStarted,
                         [this](const QByteArray& device, qint64 silentMs) {
                             holdStarts.push_back({device, silentMs});
                             events << QStringLiteral("holdStarted");
                         });
        QObject::connect(&watchdog, &RemoteTxWatchdog::holdEnded,
                         [this](const QByteArray& device, qint64 lengthMs,
                                RemoteTxWatchdog::HoldEnd how) {
                             holdEnds.push_back({device, lengthMs, how});
                             events << QStringLiteral("holdEnded ") + holdEndName(how);
                         });
    }

    // Moves the clock to `ms`, firing the timer whenever it comes due.
    void advanceTo(qint64 ms)
    {
        while (timerDue && *timerDue <= ms) {
            nowMs = std::max(nowMs, *timerDue);
            timerDue.reset();
            watchdog.onTimer();
        }
        nowMs = std::max(nowMs, ms);
    }

    int holdsStarted() const
    {
        return static_cast<int>(std::count_if(holds.begin(), holds.end(),
                                              [](const Hold& h) { return h.held; }));
    }
};

} // namespace

class TestRemoteTxWatchdog : public QObject {
    Q_OBJECT

private slots:
    void cwCodecKeepsLegacyAndCarriesExplicitEpoch()
    {
        const TxKeepalive value{0x0102030405060708ULL, 0x11223344u, 0x55667788u};
        const QByteArray legacy = RemoteTxWatchdog::channelKeepalive(value.sequence, value.epoch);
        QCOMPARE(RemoteTxWatchdog::channelKeepalive({value.sequence, value.epoch, 0}), legacy);
        const QByteArray cw = RemoteTxWatchdog::channelKeepalive(value);
        QCOMPARE(cw.toHex(), QByteArray("0201020304050607081122334455667788"));
        TxKeepalive decoded;
        QVERIFY(RemoteTxWatchdog::readChannelKeepalive(cw, &decoded));
        QCOMPARE(decoded.cwEpoch, value.cwEpoch);
        QVERIFY(!RemoteTxWatchdog::readChannelKeepalive(cw.left(16), &decoded));
        QVERIFY(!RemoteTxWatchdog::readChannelKeepalive(cw + 'x', &decoded));
    }

    void cwOnlyWatchSurvivesMicReleaseAndRejectsOldEpoch()
    {
        Rig rig;
        rig.watchdog.setKeyed(kPhone, true, 7);
        rig.watchdog.setCwActive(kPhone, true, 11);
        rig.watchdog.setKeyed(kPhone, false);
        QVERIFY(rig.watchdog.isWatching(kPhone));
        QVERIFY(!rig.watchdog.keepalive(kPhone, {1, 7, 10}, RemoteTxWatchdog::Path::Session));
        rig.advanceTo(401);
        QVERIFY(rig.watchdog.isHeld(kPhone));
        QVERIFY(rig.watchdog.keepalive(kPhone, {1, 7, 11}, RemoteTxWatchdog::Path::Session));
        QVERIFY(!rig.watchdog.keepalive(kPhone, {1, 7, 11}, RemoteTxWatchdog::Path::Auxiliary));
        QVERIFY(!rig.watchdog.isHeld(kPhone));
        rig.advanceTo(3402);
        QCOMPARE(rig.stops.size(), size_t{1});
    }

    void theNumbersAreStatedTogether();
    void aQuietKeyHoldsAfter400AndCutsAfter3000();
    void keepalivesHoldTenMinutesAndOneMissedHoldsThenCuts();
    void aKeepaliveDuringTheHoldEndsIt();
    void aKeepaliveThatDoesNotCountEndsNoHold();
    void staleKeepalivesCannotHoldAKeyForever();
    void staleKeepalivesWithEveryCheckLateStillCut();
    void aFreshKeepaliveEndsAHoldThatStaleOnesKept();
    void theReleaseDuringAHoldEndsItWithoutAStop();
    void theLinkClosingStopsAtOnce();
    void theLinkClosingDuringAHoldStopsAtOnce();
    void aQuietTripFromTheCutGuardStopsOnce();
    void aQuietTripFromTheCutGuardDuringAHoldIsTheCut();
    void aQuietTripReportsTheGuardsSilence();
    void aTripLogsTheDeviceIdAsHex();
    void voxArmedIsWatchedAndStops();
    void aVoxArmedUnkeyedWatchIsCutNeverHeld();
    void watchingStartsWithAFreshHoldAndCut();
    void copiesAndOvertakenKeepalivesDoNotCount();
    void lateKeepaliveCannotRescueExpiredWatch_data();
    void lateKeepaliveCannotRescueExpiredWatch();
    void anOlderEpochDoesNotCount();
    void aKeyTheDeviceWasNeverAnsweredForTakesAnyEpoch();
    void theDevicesReleaseEndsTheWatchAtOnce();
    void eachDeviceIsWatchedOnItsOwn();
    void aKeepaliveWhileNothingIsWatchedChangesNothing();
    void theStopSentenceIsPlain();
    void theChannelMessageRoundTrips();
    void fivePercentLossOnTheTxChannelNeverHolds();
    void theSameLossOnAReliableChannelHolds();
    void aStallWithKeepalivesArrivingKeepsTheKey_data();
    void aStallWithKeepalivesArrivingKeepsTheKey();
    void aSilentLinkStillHoldsAndCutsInTime();
    void anotherDevicesKeepalivesCannotPutOffADeadLink_data();
    void anotherDevicesKeepalivesCannotPutOffADeadLink();
    void aCheckOnlySlightlyLateJudgesAtOnce();
};

void TestRemoteTxWatchdog::theNumbersAreStatedTogether()
{
    // Hold then cut design 5.1: buffer < starvation < link hold < first
    // reconnect < link cut, all together.
    QCOMPARE(RemoteTxWatchdog::kKeepaliveIntervalMs, 100);
    QCOMPARE(RemoteTxWatchdog::kLinkHoldMs, 400);
    QCOMPARE(RemoteTxWatchdog::kLinkCutMs, 3000);
    QCOMPARE(RemoteTxWatchdog::kLinkHoldMaxMs, 2600);
    QCOMPARE(RemoteTxWatchdog::kFirstReconnectMs, 1000);
    QCOMPARE(RemoteMicConfig::kStarvationMs, 250);
    QCOMPARE(RemoteMicConfig::kMaxDepthMs, 120);
    QVERIFY(RemoteMicConfig::kMaxDepthMs < RemoteMicConfig::kStarvationMs);
    QVERIFY(RemoteMicConfig::kStarvationMs < RemoteTxWatchdog::kLinkHoldMs);
    QVERIFY(RemoteTxWatchdog::kLinkHoldMs < RemoteTxWatchdog::kFirstReconnectMs);
    QVERIFY(RemoteTxWatchdog::kFirstReconnectMs < RemoteTxWatchdog::kLinkCutMs);
    QVERIFY(RemoteTxWatchdog::kKeepaliveIntervalMs * 3 < RemoteTxWatchdog::kLinkHoldMs);
}

// Hold then cut: a keyed watch that never hears a keepalive holds its RF
// silent after more than 400 ms and is cut after more than 3000 ms. The cut
// stops transmitting before it releases the hold, so no real sample leaves
// between them.
void TestRemoteTxWatchdog::aQuietKeyHoldsAfter400AndCutsAfter3000()
{
    Rig rig;
    QSignalSpy tripped(&rig.watchdog, &RemoteTxWatchdog::tripped);
    QVERIFY(tripped.isValid());
    rig.watchdog.setKeyed(kPhone, true, 7);
    rig.advanceTo(399);
    QVERIFY(rig.holds.empty());
    rig.advanceTo(400);
    QVERIFY(rig.holds.empty());
    QVERIFY(!rig.watchdog.isHeld(kPhone));
    rig.advanceTo(401);
    QCOMPARE(rig.holds.size(), size_t(1));
    QCOMPARE(rig.holds.front().device, kPhone);
    QVERIFY(rig.holds.front().held);
    QCOMPARE(rig.holds.front().atMs, qint64(401));
    QCOMPARE(rig.holdStarts.size(), size_t(1));
    QCOMPARE(rig.holdStarts.front().device, kPhone);
    QVERIFY(rig.holdStarts.front().silentMs >= 400);
    QVERIFY(rig.watchdog.isHeld(kPhone));
    QVERIFY(rig.watchdog.isWatching(kPhone));
    QVERIFY(rig.stops.empty());

    rig.advanceTo(2999);
    QVERIFY(rig.stops.empty());
    rig.advanceTo(3000);
    QVERIFY(rig.stops.empty());
    QVERIFY(rig.holdEnds.empty());
    rig.advanceTo(3001);
    QCOMPARE(rig.stops.size(), size_t(1));
    QCOMPARE(rig.stops.front().atMs, qint64(3001));
    QCOMPARE(rig.stops.front().message, RemoteTxWatchdog::stopMessage(QStringLiteral("JJ's iPhone")));
    QCOMPARE(tripped.count(), 1);
    QCOMPARE(tripped.at(0).at(1).toBool(), false);
    QVERIFY(tripped.at(0).at(2).toLongLong() >= 3000);
    QCOMPARE(rig.holds.size(), size_t(2));
    QVERIFY(!rig.holds.back().held);
    QCOMPARE(rig.holds.back().atMs, qint64(3001));
    QCOMPARE(rig.holdEnds.size(), size_t(1));
    QCOMPARE(rig.holdEnds.front().device, kPhone);
    QCOMPARE(rig.holdEnds.front().lengthMs, qint64(2600));
    QVERIFY(rig.holdEnds.front().how == RemoteTxWatchdog::HoldEnd::Cut);
    QCOMPARE(rig.events,
             QStringList({QStringLiteral("hold on"), QStringLiteral("holdStarted"),
                          QStringLiteral("stop"), QStringLiteral("tripped"),
                          QStringLiteral("hold off"), QStringLiteral("holdEnded Cut")}));
    QVERIFY(!rig.watchdog.isWatching(kPhone));
    QVERIFY(!rig.watchdog.isHeld(kPhone));
    QVERIFY(!rig.timerDue.has_value());
}

void TestRemoteTxWatchdog::keepalivesHoldTenMinutesAndOneMissedHoldsThenCuts()
{
    Rig rig;
    rig.watchdog.setKeyed(kPhone, true, 7);
    QVERIFY(rig.watchdog.isWatching(kPhone));

    constexpr qint64 kTenMinutesMs = 10 * 60 * 1000;
    quint64 sequence = 0;
    qint64 lastMs = 0;
    for (qint64 t = 100; t <= kTenMinutesMs; t += RemoteTxWatchdog::kKeepaliveIntervalMs) {
        rig.advanceTo(t);
        QVERIFY(rig.watchdog.keepalive(kPhone, ++sequence, 7, RemoteTxWatchdog::Path::Session));
        lastMs = t;
    }
    QVERIFY2(rig.stops.empty(), "keepalives every 100 ms stopped the key");
    QVERIFY2(rig.holds.empty(), "keepalives every 100 ms held the key");
    QCOMPARE(sequence, quint64(6000));

    // The next one never comes.
    rig.advanceTo(lastMs + 399);
    QVERIFY(rig.holds.empty());
    rig.advanceTo(lastMs + 2000);
    QCOMPARE(rig.holds.size(), size_t(1));
    const qint64 heldAfter = rig.holds.front().atMs - lastMs;
    QVERIFY2(heldAfter >= 400 && heldAfter <= 410,
             qPrintable(QStringLiteral("held %1 ms after the last").arg(heldAfter)));
    QVERIFY(rig.stops.empty());
    rig.advanceTo(lastMs + 5000);
    QCOMPARE(rig.stops.size(), size_t(1));
    const qint64 after = rig.stops.front().atMs - lastMs;
    QVERIFY2(after >= 3000 && after <= 3010,
             qPrintable(QStringLiteral("stopped %1 ms after the last").arg(after)));
    QCOMPARE(rig.stops.front().device, kPhone);
    QCOMPARE(rig.stops.front().message,
             QStringLiteral("The link to JJ's iPhone went quiet, so the Core stopped transmitting."));
    QVERIFY(!rig.watchdog.isWatching(kPhone));
    QVERIFY(!rig.timerDue.has_value());
}

// A keepalive that counts during the hold ends it; the device stays
// watched, and the next hold needs a fresh 400 ms of silence.
void TestRemoteTxWatchdog::aKeepaliveDuringTheHoldEndsIt()
{
    Rig rig;
    rig.watchdog.setKeyed(kPhone, true, 7);
    rig.advanceTo(401);
    QVERIFY(rig.watchdog.isHeld(kPhone));
    rig.advanceTo(1500);
    QVERIFY(rig.watchdog.keepalive(kPhone, 1, 7, RemoteTxWatchdog::Path::TxChannel));
    QCOMPARE(rig.holds.size(), size_t(2));
    QVERIFY(!rig.holds.back().held);
    QCOMPARE(rig.holds.back().atMs, qint64(1500));
    QCOMPARE(rig.holdEnds.size(), size_t(1));
    QCOMPARE(rig.holdEnds.front().device, kPhone);
    QVERIFY(qAbs(rig.holdEnds.front().lengthMs - 1100) <= 1);
    QVERIFY(rig.holdEnds.front().how == RemoteTxWatchdog::HoldEnd::Heard);
    QVERIFY(rig.watchdog.isWatching(kPhone));
    QVERIFY(!rig.watchdog.isHeld(kPhone));
    QVERIFY(rig.stops.empty());

    rig.advanceTo(1900);
    QCOMPARE(rig.holds.size(), size_t(2));
    rig.advanceTo(1901);
    QCOMPARE(rig.holds.size(), size_t(3));
    QVERIFY(rig.holds.back().held);
    QCOMPARE(rig.holdStarts.size(), size_t(2));
    QCOMPARE(rig.holdStarts.back().silentMs, qint64(401));
    QVERIFY(rig.stops.empty());
}

// During a hold, a keepalive with an old sequence, a repeated sequence or an
// epoch older than the key's ends nothing and is not counted: the cut comes
// 3000 ms after the last one that counted.
void TestRemoteTxWatchdog::aKeepaliveThatDoesNotCountEndsNoHold()
{
    Rig rig;
    rig.watchdog.setKeyed(kPhone, true, 9);
    rig.advanceTo(100);
    QVERIFY(rig.watchdog.keepalive(kPhone, 5, 9, RemoteTxWatchdog::Path::TxChannel));
    rig.advanceTo(501);
    QVERIFY(rig.watchdog.isHeld(kPhone));
    QSignalSpy heard(&rig.watchdog, &RemoteTxWatchdog::keepaliveHeard);
    QVERIFY(heard.isValid());
    rig.advanceTo(800);
    QVERIFY(!rig.watchdog.keepalive(kPhone, 5, 9, RemoteTxWatchdog::Path::Session));   // repeated
    QVERIFY(!rig.watchdog.keepalive(kPhone, 4, 9, RemoteTxWatchdog::Path::TxChannel)); // old
    QVERIFY(!rig.watchdog.keepalive(kPhone, 6, 8, RemoteTxWatchdog::Path::TxChannel)); // older epoch
    QCOMPARE(heard.count(), 0);
    QVERIFY(rig.watchdog.isHeld(kPhone));
    QCOMPARE(rig.holds.size(), size_t(1));
    QVERIFY(rig.holdEnds.empty());
    rig.advanceTo(3100);
    QVERIFY(rig.stops.empty());
    rig.advanceTo(3101);
    QCOMPARE(rig.stops.size(), size_t(1));
    QCOMPARE(rig.holdEnds.size(), size_t(1));
    QVERIFY(rig.holdEnds.front().how == RemoteTxWatchdog::HoldEnd::Cut);
}

// Fix wave A: the Core's main thread lags by more than 400 ms, so every
// keepalive counts (a newer sequence) but each is heard 450 ms before it is
// handed over. Each one moves the 3000 ms cut back and none is fresh enough
// to end the hold. The hold starts at 401 ms and is cut at its bound,
// heldSinceMs + 2600 (+1 ms, the timer's usual one), never later, with
// today's stop and HoldEnd::Cut.
void TestRemoteTxWatchdog::staleKeepalivesCannotHoldAKeyForever()
{
    Rig rig;
    QSignalSpy tripped(&rig.watchdog, &RemoteTxWatchdog::tripped);
    QVERIFY(tripped.isValid());
    rig.watchdog.setKeyed(kPhone, true, 7);
    constexpr qint64 kAgeMs = 450;
    quint64 sequence = 0;
    for (qint64 t = 100; t <= 8000; t += RemoteTxWatchdog::kKeepaliveIntervalMs) {
        rig.advanceTo(t);
        if (!rig.watchdog.isWatching(kPhone)) {
            break;
        }
        QVERIFY(rig.watchdog.keepalive(kPhone, ++sequence, 7, RemoteTxWatchdog::Path::TxChannel,
                                       kAgeMs));
    }
    QCOMPARE(rig.holds.size(), size_t(2));
    QVERIFY(rig.holds.front().held);
    QCOMPARE(rig.holds.front().atMs, qint64(401));
    const qint64 heldSinceMs = rig.holds.front().atMs;
    QCOMPARE(rig.stops.size(), size_t(1));
    QCOMPARE(rig.stops.front().atMs, heldSinceMs + RemoteTxWatchdog::kLinkHoldMaxMs + 1);
    QCOMPARE(rig.stops.front().device, kPhone);
    QCOMPARE(rig.stops.front().message,
             RemoteTxWatchdog::stopMessage(QStringLiteral("JJ's iPhone")));
    QCOMPARE(tripped.count(), 1);
    QCOMPARE(tripped.at(0).at(1).toBool(), false);
    QVERIFY(!rig.holds.back().held);
    QCOMPARE(rig.holds.back().atMs, rig.stops.front().atMs);
    QCOMPARE(rig.holdEnds.size(), size_t(1));
    QVERIFY(rig.holdEnds.front().how == RemoteTxWatchdog::HoldEnd::Cut);
    QCOMPARE(rig.holdEnds.front().lengthMs, qint64(RemoteTxWatchdog::kLinkHoldMaxMs + 1));
    QCOMPARE(rig.events,
             QStringList({QStringLiteral("hold on"), QStringLiteral("holdStarted"),
                          QStringLiteral("stop"), QStringLiteral("tripped"),
                          QStringLiteral("hold off"), QStringLiteral("holdEnded Cut")}));
    QVERIFY(!rig.watchdog.isWatching(kPhone));
    QVERIFY(!rig.timerDue.has_value());
    // A stale keepalive after the cut does not bring the watch back.
    QVERIFY(!rig.watchdog.keepalive(kPhone, ++sequence, 7, RemoteTxWatchdog::Path::TxChannel,
                                    kAgeMs));
    QCOMPARE(rig.stops.size(), size_t(1));
}

// Fix wave A, the lag as the Core sees it: the check fires late too (up to
// 100 ms behind its time, past kLateCheckSlackMs), so every check offers a
// late turn. The hold bound's turn is given once per hold, so stale
// keepalives counting on every turn cannot renew it: the cut comes within
// two late turns of the bound.
void TestRemoteTxWatchdog::staleKeepalivesWithEveryCheckLateStillCut()
{
    Rig rig;
    rig.watchdog.setKeyed(kPhone, true, 7);
    constexpr qint64 kAgeMs = 450;
    constexpr qint64 kStepMs = RemoteTxWatchdog::kKeepaliveIntervalMs;
    quint64 sequence = 0;
    for (qint64 t = kStepMs; t <= 10000; t += kStepMs) {
        rig.nowMs = t;
        if (rig.timerDue && *rig.timerDue <= t) {
            rig.timerDue.reset();
            rig.watchdog.onTimer();
        }
        if (!rig.watchdog.isWatching(kPhone)) {
            break;
        }
        QVERIFY(rig.watchdog.keepalive(kPhone, ++sequence, 7, RemoteTxWatchdog::Path::TxChannel,
                                       kAgeMs));
    }
    QVERIFY(rig.holdsStarted() == 1);
    const qint64 heldSinceMs = rig.holds.front().atMs;
    QCOMPARE(rig.stops.size(), size_t(1));
    const qint64 cutAt = rig.stops.front().atMs;
    QVERIFY2(cutAt > heldSinceMs + RemoteTxWatchdog::kLinkHoldMaxMs
                 && cutAt <= heldSinceMs + RemoteTxWatchdog::kLinkHoldMaxMs + 1 + 2 * kStepMs,
             qPrintable(QStringLiteral("held at %1 ms, cut at %2 ms").arg(heldSinceMs).arg(cutAt)));
    QCOMPARE(rig.holdEnds.size(), size_t(1));
    QVERIFY(rig.holdEnds.front().how == RemoteTxWatchdog::HoldEnd::Cut);
    QVERIFY(!rig.watchdog.isWatching(kPhone));
}

// Fix wave A: the bound does not change how a hold ends. Held behind stale
// keepalives, a fresh one ends the hold (Heard) as before, and fresh ones
// after it keep the key with no further hold and no stop.
void TestRemoteTxWatchdog::aFreshKeepaliveEndsAHoldThatStaleOnesKept()
{
    Rig rig;
    rig.watchdog.setKeyed(kPhone, true, 7);
    quint64 sequence = 0;
    for (qint64 t = 100; t <= 2000; t += RemoteTxWatchdog::kKeepaliveIntervalMs) {
        rig.advanceTo(t);
        QVERIFY(rig.watchdog.keepalive(kPhone, ++sequence, 7, RemoteTxWatchdog::Path::TxChannel,
                                       450));
    }
    QVERIFY(rig.watchdog.isHeld(kPhone));
    QVERIFY(rig.holdEnds.empty());
    rig.advanceTo(2100);
    QVERIFY(rig.watchdog.keepalive(kPhone, ++sequence, 7, RemoteTxWatchdog::Path::TxChannel));
    QVERIFY(!rig.watchdog.isHeld(kPhone));
    QCOMPARE(rig.holdEnds.size(), size_t(1));
    QVERIFY(rig.holdEnds.front().how == RemoteTxWatchdog::HoldEnd::Heard);
    QCOMPARE(rig.holdEnds.front().lengthMs, qint64(2100 - 401));
    for (qint64 t = 2200; t <= 10000; t += RemoteTxWatchdog::kKeepaliveIntervalMs) {
        rig.advanceTo(t);
        QVERIFY(rig.watchdog.keepalive(kPhone, ++sequence, 7, RemoteTxWatchdog::Path::TxChannel));
    }
    QVERIFY(rig.stops.empty());
    QCOMPARE(rig.holdsStarted(), 1);
    QVERIFY(rig.watchdog.isWatching(kPhone));
}

// The device's own release (or its key read off) during a hold ends the
// hold and the watch, with no stop.
void TestRemoteTxWatchdog::theReleaseDuringAHoldEndsItWithoutAStop()
{
    for (const bool viaRelease : {true, false}) {
        Rig rig;
        rig.watchdog.setKeyed(kPhone, true, 3);
        rig.advanceTo(401);
        QVERIFY(rig.watchdog.isHeld(kPhone));
        rig.advanceTo(600);
        if (viaRelease) {
            rig.watchdog.released(kPhone);
        } else {
            rig.watchdog.setKeyed(kPhone, false);
        }
        QCOMPARE(rig.holds.size(), size_t(2));
        QVERIFY(!rig.holds.back().held);
        QCOMPARE(rig.holds.back().atMs, qint64(600));
        QCOMPARE(rig.holdEnds.size(), size_t(1));
        QCOMPARE(rig.holdEnds.front().lengthMs, qint64(199));
        QVERIFY(rig.holdEnds.front().how == RemoteTxWatchdog::HoldEnd::KeyEnded);
        QVERIFY(!rig.watchdog.isWatching(kPhone));
        QVERIFY(!rig.watchdog.isHeld(kPhone));
        rig.advanceTo(60000);
        QVERIFY(rig.stops.empty());
        QVERIFY(!rig.timerDue.has_value());
    }

    // VOX still armed: the key's hold ends, the watch stays, and its
    // silence is still cut.
    Rig vox;
    vox.watchdog.setVoxArmed(kPhone, true);
    vox.watchdog.setKeyed(kPhone, true, 4);
    vox.advanceTo(401);
    QVERIFY(vox.watchdog.isHeld(kPhone));
    vox.watchdog.released(kPhone);
    QVERIFY(!vox.watchdog.isHeld(kPhone));
    QVERIFY(vox.watchdog.isWatching(kPhone));
    QCOMPARE(vox.holdEnds.size(), size_t(1));
    QVERIFY(vox.holdEnds.front().how == RemoteTxWatchdog::HoldEnd::KeyEnded);
    vox.advanceTo(3000);
    QVERIFY(vox.stops.empty());
    vox.advanceTo(3001);
    QCOMPARE(vox.stops.size(), size_t(1));
    QCOMPARE(vox.holdEnds.size(), size_t(1));
}

void TestRemoteTxWatchdog::theLinkClosingStopsAtOnce()
{
    Rig rig;
    rig.watchdog.setKeyed(kPhone, true, 3);
    rig.advanceTo(100);
    QVERIFY(rig.watchdog.keepalive(kPhone, 1, 3, RemoteTxWatchdog::Path::TxChannel));
    rig.advanceTo(150);
    QSignalSpy tripped(&rig.watchdog, &RemoteTxWatchdog::tripped);
    QVERIFY(tripped.isValid());
    rig.watchdog.linkClosed(kPhone);
    QCOMPARE(rig.stops.size(), size_t(1));
    QCOMPARE(rig.stops.front().atMs, qint64(150));
    QCOMPARE(tripped.count(), 1);
    QCOMPARE(tripped.at(0).at(1).toBool(), true);
    QVERIFY(!rig.watchdog.isWatching(kPhone));
    // Not held, so no hold to end.
    QVERIFY(rig.holds.empty());
    QVERIFY(rig.holdEnds.empty());

    // A device that was not watched: nothing.
    rig.watchdog.linkClosed(kTablet);
    QCOMPARE(rig.stops.size(), size_t(1));
}

void TestRemoteTxWatchdog::theLinkClosingDuringAHoldStopsAtOnce()
{
    Rig rig;
    rig.watchdog.setKeyed(kPhone, true, 3);
    rig.advanceTo(401);
    QVERIFY(rig.watchdog.isHeld(kPhone));
    rig.advanceTo(700);
    QSignalSpy tripped(&rig.watchdog, &RemoteTxWatchdog::tripped);
    QVERIFY(tripped.isValid());
    rig.events.clear();
    rig.watchdog.linkClosed(kPhone);
    QCOMPARE(rig.stops.size(), size_t(1));
    QCOMPARE(rig.stops.front().atMs, qint64(700));
    QCOMPARE(tripped.count(), 1);
    QCOMPARE(tripped.at(0).at(1).toBool(), true);
    QCOMPARE(rig.holdEnds.size(), size_t(1));
    QCOMPARE(rig.holdEnds.front().lengthMs, qint64(299));
    QVERIFY(rig.holdEnds.front().how == RemoteTxWatchdog::HoldEnd::LinkClosed);
    QCOMPARE(rig.events,
             QStringList({QStringLiteral("stop"), QStringLiteral("tripped"),
                          QStringLiteral("hold off"), QStringLiteral("holdEnded LinkClosed")}));
    QVERIFY(!rig.watchdog.isWatching(kPhone));
    QVERIFY(!rig.watchdog.isHeld(kPhone));
}

// Remote transmit cut off the main thread (2026-10-15): the cut guard's
// judgement reaches the watchdog as tripQuiet. A watched device trips once,
// as a quiet link with the watchdog's sentence, and is no longer watched,
// so the check that was due during the stall stops nothing more; a device
// the watchdog already tripped is not stopped again.
void TestRemoteTxWatchdog::aQuietTripFromTheCutGuardStopsOnce()
{
    Rig rig;
    rig.watchdog.setKeyed(kPhone, true, 3);
    rig.advanceTo(100);
    QVERIFY(rig.watchdog.keepalive(kPhone, 1, 3, RemoteTxWatchdog::Path::TxChannel));
    QSignalSpy tripped(&rig.watchdog, &RemoteTxWatchdog::tripped);
    QVERIFY(tripped.isValid());

    // The main thread was held; the guard's post runs before the
    // watchdog's overdue check.
    rig.nowMs = 1100;
    QVERIFY(rig.watchdog.tripQuiet(kPhone));
    QCOMPARE(rig.stops.size(), size_t(1));
    QCOMPARE(rig.stops.front().device, kPhone);
    QCOMPARE(rig.stops.front().message, RemoteTxWatchdog::stopMessage(QStringLiteral("JJ's iPhone")));
    QCOMPARE(tripped.count(), 1);
    QCOMPARE(tripped.at(0).at(1).toBool(), false);
    QCOMPARE(tripped.at(0).at(2).toLongLong(), qint64(1000));
    QVERIFY(!rig.watchdog.isWatching(kPhone));
    rig.advanceTo(5000);
    rig.watchdog.onTimer();   // a check that was already due runs anyway
    QCOMPARE(rig.stops.size(), size_t(1));
    QCOMPARE(tripped.count(), 1);

    // The other order: the watchdog tripped first; the guard's post stops
    // nothing more.
    Rig other;
    other.watchdog.setKeyed(kTablet, true, 1);
    other.advanceTo(3001);
    QCOMPARE(other.stops.size(), size_t(1));
    QVERIFY(!other.watchdog.tripQuiet(kTablet));
    QCOMPARE(other.stops.size(), size_t(1));
}

void TestRemoteTxWatchdog::aQuietTripFromTheCutGuardDuringAHoldIsTheCut()
{
    Rig rig;
    rig.watchdog.setKeyed(kPhone, true, 3);
    rig.advanceTo(100);
    QVERIFY(rig.watchdog.keepalive(kPhone, 1, 3, RemoteTxWatchdog::Path::TxChannel));
    rig.advanceTo(501);
    QVERIFY(rig.watchdog.isHeld(kPhone));
    QSignalSpy tripped(&rig.watchdog, &RemoteTxWatchdog::tripped);
    QVERIFY(tripped.isValid());
    rig.events.clear();
    rig.nowMs = 1100;
    QVERIFY(rig.watchdog.tripQuiet(kPhone));
    QCOMPARE(rig.stops.size(), size_t(1));
    QCOMPARE(tripped.count(), 1);
    QCOMPARE(tripped.at(0).at(1).toBool(), false);
    QCOMPARE(tripped.at(0).at(2).toLongLong(), qint64(1000));
    QCOMPARE(rig.holdEnds.size(), size_t(1));
    QCOMPARE(rig.holdEnds.front().lengthMs, qint64(599));
    QVERIFY(rig.holdEnds.front().how == RemoteTxWatchdog::HoldEnd::Cut);
    QCOMPARE(rig.events,
             QStringList({QStringLiteral("stop"), QStringLiteral("tripped"),
                          QStringLiteral("hold off"), QStringLiteral("holdEnded Cut")}));
    QVERIFY(!rig.watchdog.isWatching(kPhone));
    QVERIFY(!rig.watchdog.isHeld(kPhone));
}

// CI 2026-10-16: the guard judged more than its deadline on its own clock
// and the watchdog's clock, rounding at other instants, read the deadline;
// the stop said so. The guard's measurement is what the stop reports when
// it is the longer; it never lowers the watchdog's own reading.
void TestRemoteTxWatchdog::aQuietTripReportsTheGuardsSilence()
{
    Rig rig;
    rig.watchdog.setKeyed(kPhone, true, 3);
    rig.advanceTo(100);
    QVERIFY(rig.watchdog.keepalive(kPhone, 1, 3, RemoteTxWatchdog::Path::TxChannel));
    QSignalSpy tripped(&rig.watchdog, &RemoteTxWatchdog::tripped);
    QVERIFY(tripped.isValid());
    rig.nowMs = 100 + RemoteTxWatchdog::kLinkCutMs;   // reads 3000
    QVERIFY(rig.watchdog.tripQuiet(kPhone, RemoteTxWatchdog::kLinkCutMs + 1));
    QCOMPARE(tripped.count(), 1);
    QCOMPARE(tripped.at(0).at(2).toLongLong(), qint64(RemoteTxWatchdog::kLinkCutMs + 1));

    Rig longer;
    longer.watchdog.setKeyed(kPhone, true, 3);
    longer.advanceTo(100);
    QVERIFY(longer.watchdog.keepalive(kPhone, 1, 3, RemoteTxWatchdog::Path::TxChannel));
    QSignalSpy longerTripped(&longer.watchdog, &RemoteTxWatchdog::tripped);
    // Reads 3500, a main thread held past the guard's cut.
    longer.nowMs = 100 + RemoteTxWatchdog::kLinkCutMs + 500;
    QVERIFY(longer.watchdog.tripQuiet(kPhone, RemoteTxWatchdog::kLinkCutMs + 1));
    QCOMPARE(longerTripped.count(), 1);
    QCOMPARE(longerTripped.at(0).at(2).toLongLong(), qint64(RemoteTxWatchdog::kLinkCutMs + 500));
}

// A device id is raw bytes (the link's binary id), so the trip's warning
// names it in hex; the raw bytes read as garbage in a log.
void TestRemoteTxWatchdog::aTripLogsTheDeviceIdAsHex()
{
    Rig rig;
    const QByteArray binaryId = QByteArray::fromHex("00ff10a1c3");
    rig.watchdog.setKeyed(binaryId, true, 1);
    QTest::ignoreMessage(QtWarningMsg,
                         QRegularExpression(QStringLiteral("^Transmit watchdog: the link closed +"
                                                           "from 00ff10a1c3 - ")));
    rig.watchdog.linkClosed(binaryId);
    QCOMPARE(rig.stops.size(), size_t(1));
    QCOMPARE(rig.stops.front().device, binaryId);
}

void TestRemoteTxWatchdog::voxArmedIsWatchedAndStops()
{
    Rig rig;
    rig.advanceTo(1000);
    rig.watchdog.setVoxArmed(kTablet, true);
    QVERIFY(rig.watchdog.isWatching(kTablet));
    for (quint64 s = 1; s <= 20; ++s) {
        rig.advanceTo(1000 + static_cast<qint64>(s) * 100);
        // VOX has no key of its own: any epoch counts.
        QVERIFY(rig.watchdog.keepalive(kTablet, s, 0, RemoteTxWatchdog::Path::Session));
    }
    QVERIFY(rig.stops.empty());
    rig.advanceTo(3000 + 3000);
    QVERIFY(rig.stops.empty());
    rig.advanceTo(3000 + 3001);
    QCOMPARE(rig.stops.size(), size_t(1));
    QCOMPARE(rig.stops.front().message,
             QStringLiteral("The link to Shack iPad went quiet, so the Core stopped transmitting."));
    QVERIFY(rig.holds.empty());

    // Disarmed by the device: nothing is watched.
    Rig other;
    other.watchdog.setVoxArmed(kTablet, true);
    other.watchdog.setVoxArmed(kTablet, false);
    QVERIFY(!other.watchdog.isWatching(kTablet));
    other.advanceTo(5000);
    QVERIFY(other.stops.empty());
}

// Decision: VOX armed with no key holds nothing (there is no RF to hold);
// its silence is cut as today.
void TestRemoteTxWatchdog::aVoxArmedUnkeyedWatchIsCutNeverHeld()
{
    Rig rig;
    rig.watchdog.setVoxArmed(kTablet, true);
    rig.advanceTo(401);
    QVERIFY(rig.holds.empty());
    QVERIFY(!rig.watchdog.isHeld(kTablet));
    rig.advanceTo(3000);
    QVERIFY(rig.stops.empty());
    rig.advanceTo(3001);
    QCOMPARE(rig.stops.size(), size_t(1));
    QCOMPARE(rig.stops.front().atMs, qint64(3001));
    QCOMPARE(rig.stops.front().message, RemoteTxWatchdog::stopMessage(QStringLiteral("Shack iPad")));
    QVERIFY(rig.holds.empty());
    QVERIFY(rig.holdEnds.empty());
    QVERIFY(!rig.watchdog.isWatching(kTablet));
}

void TestRemoteTxWatchdog::watchingStartsWithAFreshHoldAndCut()
{
    Rig rig;
    rig.advanceTo(5000);
    rig.watchdog.setKeyed(kPhone, true, 1);
    rig.advanceTo(5400);
    QVERIFY(rig.holds.empty());
    rig.advanceTo(5401);
    QCOMPARE(rig.holdsStarted(), 1);
    rig.advanceTo(8000);
    QVERIFY(rig.stops.empty());
    rig.advanceTo(8001);
    QCOMPARE(rig.stops.size(), size_t(1));

    // Watching again later starts afresh, sequences included, and not held.
    rig.advanceTo(9000);
    rig.watchdog.setKeyed(kPhone, true, 2);
    QVERIFY(!rig.watchdog.isHeld(kPhone));
    rig.advanceTo(9100);
    QVERIFY(rig.watchdog.keepalive(kPhone, 1, 2, RemoteTxWatchdog::Path::Session));
    rig.advanceTo(9500);
    QCOMPARE(rig.holdsStarted(), 1);
    rig.advanceTo(9501);
    QCOMPARE(rig.holdsStarted(), 2);
    QCOMPARE(rig.stops.size(), size_t(1));
}

void TestRemoteTxWatchdog::copiesAndOvertakenKeepalivesDoNotCount()
{
    Rig rig;
    rig.watchdog.setKeyed(kPhone, true, 4);
    rig.advanceTo(100);
    QVERIFY(rig.watchdog.keepalive(kPhone, 5, 4, RemoteTxWatchdog::Path::TxChannel));
    rig.advanceTo(300);
    // The same keepalive by the other path, and one overtaken by it.
    QVERIFY(!rig.watchdog.keepalive(kPhone, 5, 4, RemoteTxWatchdog::Path::Session));
    QVERIFY(!rig.watchdog.keepalive(kPhone, 4, 4, RemoteTxWatchdog::Path::TxChannel));
    // Neither moved the hold: 400 ms after the one at 100 ms.
    rig.advanceTo(500);
    QVERIFY(rig.holds.empty());
    rig.advanceTo(501);
    QCOMPARE(rig.holdsStarted(), 1);
    QVERIFY(rig.stops.empty());
}

void TestRemoteTxWatchdog::lateKeepaliveCannotRescueExpiredWatch_data()
{
    QTest::addColumn<int>("path");
    QTest::addColumn<qint64>("gapMs");
    QTest::addColumn<quint64>("sequence");
    QTest::addColumn<quint32>("epoch");
    for (const auto path : {RemoteTxWatchdog::Path::Session,
                            RemoteTxWatchdog::Path::TxChannel,
                            RemoteTxWatchdog::Path::Auxiliary}) {
        const QByteArray name = QByteArray::number(static_cast<int>(path));
        QTest::newRow((name + "-at-deadline").constData())
            << static_cast<int>(path) << qint64(3000) << quint64(6) << quint32(4);
        QTest::newRow((name + "-expired-new").constData())
            << static_cast<int>(path) << qint64(3001) << quint64(6) << quint32(4);
        QTest::newRow((name + "-expired-copy").constData())
            << static_cast<int>(path) << qint64(3001) << quint64(5) << quint32(4);
        QTest::newRow((name + "-expired-old-key").constData())
            << static_cast<int>(path) << qint64(3001) << quint64(6) << quint32(3);
    }
}

void TestRemoteTxWatchdog::lateKeepaliveCannotRescueExpiredWatch()
{
    QFETCH(int, path);
    QFETCH(qint64, gapMs);
    QFETCH(quint64, sequence);
    QFETCH(quint32, epoch);
    Rig rig;
    rig.watchdog.setKeyed(kPhone, true, 4);
    rig.advanceTo(100);
    QVERIFY(rig.watchdog.keepalive(kPhone, 5, 4, RemoteTxWatchdog::Path::Session));
    QSignalSpy heard(&rig.watchdog, &RemoteTxWatchdog::keepaliveHeard);
    QSignalSpy tripped(&rig.watchdog, &RemoteTxWatchdog::tripped);
    QVERIFY(heard.isValid());
    QVERIFY(tripped.isValid());

    // A busy event loop can dispatch socket input before its overdue timer.
    // Move the clock without delivering that timer: arrival must check age.
    rig.nowMs = 100 + gapMs;
    const bool accepted = rig.watchdog.keepalive(
        kPhone, sequence, epoch, static_cast<RemoteTxWatchdog::Path>(path));
    const bool withinDeadline = gapMs <= RemoteTxWatchdog::kLinkCutMs;
    QCOMPARE(accepted, withinDeadline);
    QCOMPARE(heard.count(), withinDeadline ? 1 : 0);
    QCOMPARE(rig.stops.size(), withinDeadline ? size_t(0) : size_t(1));
    QCOMPARE(rig.watchdog.isWatching(kPhone), withinDeadline);
    if (!withinDeadline) {
        QCOMPARE(tripped.count(), 1);
        QCOMPARE(tripped.at(0).at(1).toBool(), false);
        QCOMPARE(tripped.at(0).at(2).toLongLong(), gapMs);
        QVERIFY(!rig.timerDue.has_value());
        rig.watchdog.onTimer();
        QVERIFY(!rig.watchdog.keepalive(kPhone, 7, 4, RemoteTxWatchdog::Path::Session));
        QCOMPARE(rig.stops.size(), size_t(1));
    }
}

// TX mic thread: the Core's event loop stalls 420 ms while the phone's
// keepalives keep arriving (every 100 ms, received off the network on
// time and held at the Core). When the stall ends, either the transports'
// drain runs first (the usual order: the overdue timers fire in the order
// they were due) or the watchdog's check does. Either way the key holds:
// each keepalive is heard at its receipt, and a check that fires late lets
// the waiting keepalives in before it judges. Nothing is held either.
void TestRemoteTxWatchdog::aStallWithKeepalivesArrivingKeepsTheKey_data()
{
    QTest::addColumn<bool>("checkFirst");
    QTest::newRow("drain first") << false;
    QTest::newRow("check first") << true;
}

void TestRemoteTxWatchdog::aStallWithKeepalivesArrivingKeepsTheKey()
{
    QFETCH(bool, checkFirst);
    Rig rig;
    rig.watchdog.setKeyed(kPhone, true, 3);
    quint64 sequence = 0;
    for (qint64 t = 100; t <= 1000; t += 100) {
        rig.advanceTo(t);
        QVERIFY(rig.watchdog.keepalive(kPhone, ++sequence, 3, RemoteTxWatchdog::Path::TxChannel));
    }
    QCOMPARE(rig.timerDue, std::optional<qint64>(1401));

    // The stall: 1000 to 1420. Nothing runs; the check due at 1401 waits.
    constexpr qint64 kStallEndMs = 1420;
    rig.nowMs = kStallEndMs;
    const auto drain = [&rig, &sequence]() {
        for (qint64 received = 1100; received <= 1400; received += 100) {
            QVERIFY(rig.watchdog.keepalive(kPhone, ++sequence, 3, RemoteTxWatchdog::Path::TxChannel,
                                           kStallEndMs - received));
        }
    };
    if (checkFirst) {
        // The late check gives the waiting keepalives one turn.
        rig.watchdog.onTimer();
        QVERIFY(rig.stops.empty());
        QVERIFY(rig.holds.empty());
        QCOMPARE(rig.timerDue, std::optional<qint64>(kStallEndMs));
    }
    drain();
    // TX watch follow-up: the check already due is left to run (at 1401
    // when the drain went first, at the stall's end after the late turn)
    // and judges by the keepalives heard; heard at 1400, the next check is
    // due at 1801.
    QCOMPARE(rig.timerDue, std::optional<qint64>(checkFirst ? kStallEndMs : 1401));
    rig.advanceTo(kStallEndMs);
    QVERIFY(rig.stops.empty());
    QVERIFY(rig.holds.empty());
    QCOMPARE(rig.timerDue, std::optional<qint64>(1801));
    for (qint64 t = 1500; t <= 3000; t += 100) {
        rig.advanceTo(t);
        QVERIFY(rig.watchdog.keepalive(kPhone, ++sequence, 3, RemoteTxWatchdog::Path::TxChannel));
    }
    QVERIFY(rig.stops.empty());
    QVERIFY(rig.holds.empty());
    QVERIFY(rig.watchdog.isWatching(kPhone));
}

// TX mic thread: nothing about a silent link is widened. Its last
// keepalive at 100 ms, it holds at 501 ms and is cut at 3101 ms. A stall of
// the event loop that holds a check past its time gives one turn before
// the hold and, separately, one before the cut, each with nothing having
// come; a stall past both stages gives one turn and then cuts.
void TestRemoteTxWatchdog::aSilentLinkStillHoldsAndCutsInTime()
{
    {
        Rig rig;
        rig.watchdog.setKeyed(kPhone, true, 3);
        rig.advanceTo(100);
        QVERIFY(rig.watchdog.keepalive(kPhone, 1, 3, RemoteTxWatchdog::Path::TxChannel));
        rig.advanceTo(500);
        QVERIFY(rig.holds.empty());
        rig.advanceTo(1000);
        QCOMPARE(rig.holds.size(), size_t(1));
        QCOMPARE(rig.holds.front().atMs, qint64(501));
        rig.advanceTo(3100);
        QVERIFY(rig.stops.empty());
        rig.advanceTo(4000);
        QCOMPARE(rig.stops.size(), size_t(1));
        QCOMPARE(rig.stops.front().atMs, qint64(3101));
    }
    {
        Rig rig;
        rig.watchdog.setKeyed(kPhone, true, 3);
        rig.advanceTo(100);
        QVERIFY(rig.watchdog.keepalive(kPhone, 1, 3, RemoteTxWatchdog::Path::TxChannel));
        QCOMPARE(rig.timerDue, std::optional<qint64>(501));
        // A stall before the hold: 300 to 700. One turn, then the hold.
        rig.nowMs = 700;
        rig.watchdog.onTimer();
        QVERIFY(rig.holds.empty());
        QCOMPARE(rig.timerDue, std::optional<qint64>(700));
        rig.advanceTo(700);
        QCOMPARE(rig.holds.size(), size_t(1));
        QCOMPARE(rig.holds.front().atMs, qint64(700));
        QVERIFY(rig.stops.empty());
        QCOMPARE(rig.timerDue, std::optional<qint64>(3101));
        // A stall before the cut: to 3300. Its own turn, then the cut.
        rig.nowMs = 3300;
        rig.watchdog.onTimer();
        QVERIFY(rig.stops.empty());
        QCOMPARE(rig.timerDue, std::optional<qint64>(3300));
        rig.advanceTo(3300);
        QCOMPARE(rig.stops.size(), size_t(1));
        QCOMPARE(rig.stops.front().atMs, qint64(3300));
        QVERIFY(!rig.watchdog.isWatching(kPhone));
        // A keepalive the link sent before it went quiet, received at
        // 3250 ms and handed over after the stop, does not bring it back.
        QVERIFY(!rig.watchdog.keepalive(kPhone, 2, 3, RemoteTxWatchdog::Path::TxChannel, 50));
        QCOMPARE(rig.stops.size(), size_t(1));
    }
    {
        Rig rig;
        rig.watchdog.setKeyed(kPhone, true, 3);
        rig.advanceTo(100);
        QVERIFY(rig.watchdog.keepalive(kPhone, 1, 3, RemoteTxWatchdog::Path::TxChannel));
        rig.advanceTo(501);
        QVERIFY(rig.watchdog.isHeld(kPhone));
        // On time to the hold, then a stall past the cut: its turn.
        rig.nowMs = 3300;
        rig.watchdog.onTimer();
        QVERIFY(rig.stops.empty());
        rig.advanceTo(3300);
        QCOMPARE(rig.stops.size(), size_t(1));
        QCOMPARE(rig.stops.front().atMs, qint64(3300));
    }
    {
        Rig rig;
        rig.watchdog.setKeyed(kPhone, true, 3);
        rig.advanceTo(100);
        QVERIFY(rig.watchdog.keepalive(kPhone, 1, 3, RemoteTxWatchdog::Path::TxChannel));
        // One stall past both stages: one turn, then the cut.
        rig.nowMs = 3300;
        rig.watchdog.onTimer();
        QVERIFY(rig.stops.empty());
        QVERIFY(rig.holds.empty());
        rig.advanceTo(3300);
        QCOMPARE(rig.stops.size(), size_t(1));
        QCOMPARE(rig.stops.front().atMs, qint64(3300));
        QVERIFY(rig.holds.empty());
    }
}

// TX mic thread fix round 2 (the reviewer's harness): both links last
// heard at 100 ms, then an owner stall. The phone's link died in it; the
// tablet's is alive, its keepalives received during the stall are handed
// over at its end and a new one arrives on every turn of the event loop
// after it, each rescheduling the check. The late check gives the waiting
// keepalives their one turn, and the phone is held (a stall to 700 ms) or
// cut (a stall to 3300 ms) by the stall's end plus one turn, not never.
// With 10 ms turns every later check fires more than kLateCheckSlackMs
// late too.
// TX watch follow-up: each turn runs the timers due at its start, the
// check and the tablet's drain, in either order: the check first, or the
// drain first (where a check restarted by the drain's keepalive would run
// only in a later turn). A check already due is left due, so it runs in
// the turn either way, and the phone is judged by the stall's end plus two
// turns whatever order the event loop picks.
void TestRemoteTxWatchdog::anotherDevicesKeepalivesCannotPutOffADeadLink_data()
{
    QTest::addColumn<bool>("checkFirst");
    QTest::addColumn<qint64>("turnMs");
    QTest::addColumn<qint64>("stallEndMs");
    for (const qint64 stallEnd : {qint64(700), qint64(3300)}) {
        const QByteArray stage = stallEnd < 3000 ? "hold" : "cut";
        QTest::newRow((stage + ", check first, 2 ms turns").constData()) << true << qint64(2) << stallEnd;
        QTest::newRow((stage + ", check first, 10 ms turns").constData()) << true << qint64(10) << stallEnd;
        QTest::newRow((stage + ", drain first, 2 ms turns").constData()) << false << qint64(2) << stallEnd;
        QTest::newRow((stage + ", drain first, 10 ms turns").constData()) << false << qint64(10) << stallEnd;
    }
}

void TestRemoteTxWatchdog::anotherDevicesKeepalivesCannotPutOffADeadLink()
{
    QFETCH(bool, checkFirst);
    QFETCH(qint64, turnMs);
    QFETCH(qint64, stallEndMs);
    const bool expectCut = stallEndMs > 100 + RemoteTxWatchdog::kLinkCutMs;
    Rig rig;
    rig.watchdog.setKeyed(kPhone, true, 3);
    rig.watchdog.setKeyed(kTablet, true, 5);
    rig.advanceTo(100);
    QVERIFY(rig.watchdog.keepalive(kPhone, 1, 3, RemoteTxWatchdog::Path::TxChannel));
    QVERIFY(rig.watchdog.keepalive(kTablet, 1, 5, RemoteTxWatchdog::Path::TxChannel));
    QCOMPARE(rig.timerDue, std::optional<qint64>(501));

    // The stall: 100 to its end. The check due at 501 fires at its end.
    quint64 tabletSequence = 1;
    // One turn of the event loop: the check, if due when the turn starts,
    // and the tablet's drain, in the row's order.
    const auto turnOfTheLoop = [&rig, checkFirst](const std::function<void()>& drain) {
        const bool checkDue = rig.timerDue && *rig.timerDue <= rig.nowMs;
        const int startsBefore = rig.starts;
        const auto check = [&rig] {
            rig.timerDue.reset();
            rig.watchdog.onTimer();
        };
        if (checkFirst) {
            if (checkDue) {
                check();
            }
            drain();
        } else {
            drain();
            // Restarted (or stopped) during the turn: not in this one.
            if (checkDue && rig.starts == startsBefore && rig.timerDue) {
                check();
            }
        }
    };
    const auto judged = [&rig, expectCut] {
        return expectCut ? !rig.stops.empty() : rig.holdsStarted() > 0;
    };
    rig.nowMs = stallEndMs;
    // The tablet's keepalives received during the stall, heard at receipt.
    turnOfTheLoop([&rig, &tabletSequence, stallEndMs] {
        for (qint64 received = 200; received < stallEndMs; received += 100) {
            QVERIFY(rig.watchdog.keepalive(kTablet, ++tabletSequence, 5,
                                           RemoteTxWatchdog::Path::TxChannel,
                                           stallEndMs - received));
        }
    });
    QVERIFY(rig.stops.empty());
    QVERIFY(rig.holds.empty());
    for (int turn = 0; turn < 1000 && !judged(); ++turn) {
        rig.nowMs += turnMs;
        // The tablet's keepalive comes in on every turn.
        turnOfTheLoop([&rig, &tabletSequence] {
            QVERIFY(rig.watchdog.keepalive(kTablet, ++tabletSequence, 5,
                                           RemoteTxWatchdog::Path::TxChannel));
        });
    }
    QVERIFY(judged());
    const qint64 atMs = expectCut ? rig.stops.front().atMs : rig.holds.front().atMs;
    const QByteArray device = expectCut ? rig.stops.front().device : rig.holds.front().device;
    QCOMPARE(device, kPhone);
    const qint64 boundMs = stallEndMs + (checkFirst ? 1 : 2) * turnMs;
    QVERIFY2(atMs <= boundMs,
             qPrintable(QStringLiteral("judged at %1, bound %2").arg(atMs).arg(boundMs)));
    if (expectCut) {
        QCOMPARE(rig.stops.size(), size_t(1));
        QVERIFY(!rig.watchdog.isWatching(kPhone));
    } else {
        QVERIFY(rig.stops.empty());
        QCOMPARE(rig.holdsStarted(), 1);
        QVERIFY(rig.watchdog.isHeld(kPhone));
    }
    QVERIFY(rig.watchdog.isWatching(kTablet));
    QVERIFY(!rig.watchdog.isHeld(kTablet));
}

// TX mic thread fix round 2: a check within kLateCheckSlackMs of its time
// is the timer's ordinary slack, not a stall, and judges at once; one more
// ms and it is late and gives the waiting keepalives their turn first.
// The same holds for the hold and for the cut.
void TestRemoteTxWatchdog::aCheckOnlySlightlyLateJudgesAtOnce()
{
    static_assert(RemoteTxWatchdog::kLateCheckSlackMs == 5);
    {
        Rig rig;
        rig.watchdog.setKeyed(kPhone, true, 3);
        rig.advanceTo(100);
        QVERIFY(rig.watchdog.keepalive(kPhone, 1, 3, RemoteTxWatchdog::Path::TxChannel));
        QCOMPARE(rig.timerDue, std::optional<qint64>(501));
        rig.nowMs = 501 + RemoteTxWatchdog::kLateCheckSlackMs;
        rig.watchdog.onTimer();
        QCOMPARE(rig.holds.size(), size_t(1));
        QCOMPARE(rig.holds.front().atMs, qint64(506));
    }
    {
        Rig rig;
        rig.watchdog.setKeyed(kPhone, true, 3);
        rig.advanceTo(100);
        QVERIFY(rig.watchdog.keepalive(kPhone, 1, 3, RemoteTxWatchdog::Path::TxChannel));
        rig.nowMs = 501 + RemoteTxWatchdog::kLateCheckSlackMs + 1;
        rig.watchdog.onTimer();
        QVERIFY(rig.holds.empty());
        QCOMPARE(rig.timerDue, std::optional<qint64>(507));
        rig.advanceTo(507);
        QCOMPARE(rig.holds.size(), size_t(1));
        QCOMPARE(rig.holds.front().atMs, qint64(507));
    }
    {
        Rig rig;
        rig.watchdog.setKeyed(kPhone, true, 3);
        rig.advanceTo(100);
        QVERIFY(rig.watchdog.keepalive(kPhone, 1, 3, RemoteTxWatchdog::Path::TxChannel));
        rig.advanceTo(501);
        QCOMPARE(rig.timerDue, std::optional<qint64>(3101));
        rig.timerDue.reset();
        rig.nowMs = 3101 + RemoteTxWatchdog::kLateCheckSlackMs;
        rig.watchdog.onTimer();
        QCOMPARE(rig.stops.size(), size_t(1));
        QCOMPARE(rig.stops.front().atMs, qint64(3106));
    }
    {
        Rig rig;
        rig.watchdog.setKeyed(kPhone, true, 3);
        rig.advanceTo(100);
        QVERIFY(rig.watchdog.keepalive(kPhone, 1, 3, RemoteTxWatchdog::Path::TxChannel));
        rig.advanceTo(501);
        rig.timerDue.reset();
        rig.nowMs = 3101 + RemoteTxWatchdog::kLateCheckSlackMs + 1;
        rig.watchdog.onTimer();
        QVERIFY(rig.stops.empty());
        QCOMPARE(rig.timerDue, std::optional<qint64>(3107));
        rig.advanceTo(3107);
        QCOMPARE(rig.stops.size(), size_t(1));
        QCOMPARE(rig.stops.front().atMs, qint64(3107));
    }
}

void TestRemoteTxWatchdog::anOlderEpochDoesNotCount()
{
    Rig rig;
    rig.watchdog.setKeyed(kPhone, true, 9);
    rig.advanceTo(100);
    QVERIFY(!rig.watchdog.keepalive(kPhone, 1, 8, RemoteTxWatchdog::Path::Session));
    QVERIFY(!rig.watchdog.keepalive(kPhone, 2, 0, RemoteTxWatchdog::Path::Session));
    rig.advanceTo(200);
    QVERIFY(rig.watchdog.keepalive(kPhone, 3, 9, RemoteTxWatchdog::Path::Session));
    rig.advanceTo(300);
    // Before its key's answer the device knows no epoch and sends the
    // highest: never older.
    QVERIFY(rig.watchdog.keepalive(kPhone, 4, 0xFFFFFFFFu, RemoteTxWatchdog::Path::Session));
    rig.advanceTo(700);
    QVERIFY(rig.holds.empty());
    rig.advanceTo(701);
    QCOMPARE(rig.holdsStarted(), 1);
    QVERIFY(rig.stops.empty());
}

void TestRemoteTxWatchdog::aKeyTheDeviceWasNeverAnsweredForTakesAnyEpoch()
{
    Rig rig;
    // A VOX key attributed to the device: epoch 0.
    rig.watchdog.setKeyed(kPhone, true, 0);
    rig.advanceTo(100);
    QVERIFY(rig.watchdog.keepalive(kPhone, 1, 0, RemoteTxWatchdog::Path::Session));
    rig.advanceTo(200);
    QVERIFY(rig.watchdog.keepalive(kPhone, 2, 12, RemoteTxWatchdog::Path::Session));
}

void TestRemoteTxWatchdog::theDevicesReleaseEndsTheWatchAtOnce()
{
    Rig rig;
    rig.watchdog.setKeyed(kPhone, true, 5);
    rig.advanceTo(100);
    QVERIFY(rig.watchdog.keepalive(kPhone, 1, 5, RemoteTxWatchdog::Path::Session));
    // The release: the key's keepalives stop with it, while MOX may still
    // read on (a tail); that is never a lost link.
    rig.watchdog.released(kPhone);
    QVERIFY(!rig.watchdog.isWatching(kPhone));
    rig.advanceTo(60000);
    QVERIFY(rig.stops.empty());
    QVERIFY(rig.holds.empty());

    // VOX armed is still watched after the key's release, and not held.
    Rig vox;
    vox.watchdog.setVoxArmed(kPhone, true);
    vox.watchdog.setKeyed(kPhone, true, 6);
    vox.watchdog.released(kPhone);
    QVERIFY(vox.watchdog.isWatching(kPhone));
    vox.advanceTo(3000);
    QVERIFY(vox.stops.empty());
    QVERIFY(vox.holds.empty());
    vox.advanceTo(3001);
    QCOMPARE(vox.stops.size(), size_t(1));
}

void TestRemoteTxWatchdog::eachDeviceIsWatchedOnItsOwn()
{
    Rig rig;
    rig.watchdog.setKeyed(kPhone, true, 1);
    rig.watchdog.setVoxArmed(kTablet, true);
    for (quint64 s = 1; s <= 35; ++s) {
        rig.advanceTo(static_cast<qint64>(s) * 100);
        QVERIFY(rig.watchdog.keepalive(kPhone, s, 1, RemoteTxWatchdog::Path::Session));
    }
    // The tablet went quiet at 0; the phone kept talking.
    QCOMPARE(rig.stops.size(), size_t(1));
    QCOMPARE(rig.stops.front().device, kTablet);
    QCOMPARE(rig.stops.front().atMs, qint64(3001));
    QVERIFY(rig.holds.empty());
    QVERIFY(rig.watchdog.isWatching(kPhone));
    // Another device's keepalives never keep the phone's key.
    QVERIFY(!rig.watchdog.keepalive(kTablet, 99, 1, RemoteTxWatchdog::Path::Session));
    rig.advanceTo(3901);
    QCOMPARE(rig.holdsStarted(), 1);
    QCOMPARE(rig.holds.front().device, kPhone);
    rig.advanceTo(6501);
    QCOMPARE(rig.stops.size(), size_t(2));
    QCOMPARE(rig.stops.back().device, kPhone);
}

void TestRemoteTxWatchdog::aKeepaliveWhileNothingIsWatchedChangesNothing()
{
    Rig rig;
    QVERIFY(!rig.watchdog.keepalive(kPhone, 1, 1, RemoteTxWatchdog::Path::Session));
    QVERIFY(!rig.watchdog.isWatchingAny());
    QVERIFY(!rig.watchdog.isHeld(kPhone));
    QVERIFY(!rig.timerDue.has_value());
    rig.advanceTo(5000);
    QVERIFY(rig.stops.empty());
}

void TestRemoteTxWatchdog::theStopSentenceIsPlain()
{
    const QString sentence = RemoteTxWatchdog::stopMessage(QStringLiteral("JJ's iPhone"));
    QCOMPARE(sentence,
             QStringLiteral("The link to JJ's iPhone went quiet, so the Core stopped transmitting."));
    QVERIFY2(OperatorWording::isPlain(sentence), qPrintable(sentence));
    QVERIFY(OperatorWording::coreCalledStationIn(sentence).isEmpty());
}

void TestRemoteTxWatchdog::theChannelMessageRoundTrips()
{
    const QByteArray message = RemoteTxWatchdog::channelKeepalive(0x0102030405060708ULL, 0xA1B2C3D4u);
    QCOMPARE(message.size(), 13);
    QCOMPARE(message.toHex(), QByteArray("010102030405060708a1b2c3d4"));
    quint64 sequence = 0;
    quint32 epoch = 0;
    QVERIFY(RemoteTxWatchdog::readChannelKeepalive(message, &sequence, &epoch));
    QCOMPARE(sequence, 0x0102030405060708ULL);
    QCOMPARE(epoch, 0xA1B2C3D4u);

    QVERIFY(!RemoteTxWatchdog::readChannelKeepalive(message.left(12), &sequence, &epoch));
    QVERIFY(!RemoteTxWatchdog::readChannelKeepalive(message + QByteArray(1, '\0'), &sequence, &epoch));
    QByteArray otherKind = message;
    otherKind[0] = 2;
    QVERIFY(!RemoteTxWatchdog::readChannelKeepalive(otherKind, &sequence, &epoch));
    // Sequences start at 1.
    QVERIFY(!RemoteTxWatchdog::readChannelKeepalive(RemoteTxWatchdog::channelKeepalive(0, 1),
                                                    &sequence, &epoch));
}

namespace {

// Ten minutes of keepalives, one every 100 ms, each lost with probability
// `loss`, carried either unordered and never retransmitted (the "tx"
// channel) or reliably in order (a TCP-like channel: a lost keepalive is
// sent again after a retransmission timeout of 200 ms, doubling for each
// time it is lost again, Linux's TCP_RTO_MIN and RFC 6298's backoff; the
// ones after it are held until it arrives). One-way delay 20 ms. Returns
// how many times the watchdog held and stopped the key (the key is taken
// again at once after each stop) and the longest silence it saw.
struct LossRun {
    int holds = 0;
    int stops = 0;
    qint64 longestGapMs = 0;
    int lost = 0;
};

LossRun runWithLoss(double loss, bool reliable, quint32 seed)
{
    constexpr qint64 kTenMinutesMs = 10 * 60 * 1000;
    constexpr qint64 kDelayMs = 20;
    constexpr qint64 kRtoMs = 200;
    QRandomGenerator random(seed);
    LossRun run;

    // Each keepalive that arrives: when, and its sequence.
    std::vector<std::pair<qint64, quint64>> ordered;
    qint64 headOfLine = 0;
    quint64 sequence = 0;
    for (qint64 sent = 100; sent <= kTenMinutesMs; sent += RemoteTxWatchdog::kKeepaliveIntervalMs) {
        ++sequence;
        if (!reliable) {
            if (random.generateDouble() < loss) {
                ++run.lost;
                continue;
            }
            ordered.emplace_back(sent + kDelayMs, sequence);
            continue;
        }
        qint64 delivered = sent + kDelayMs;
        qint64 rto = kRtoMs;
        while (random.generateDouble() < loss) {
            ++run.lost;
            delivered += rto;
            rto *= 2;
        }
        // In order: nothing is handed over before the one ahead of it.
        delivered = std::max(delivered, headOfLine);
        headOfLine = delivered;
        ordered.emplace_back(delivered, sequence);
    }
    std::stable_sort(ordered.begin(), ordered.end(),
                     [](const auto& a, const auto& b) { return a.first < b.first; });

    Rig rig;
    rig.watchdog.setKeyed(kPhone, true, 1);
    size_t stopsSeen = 0;
    qint64 last = 0;
    for (const auto& [at, seq] : ordered) {
        rig.advanceTo(at);
        if (rig.stops.size() != stopsSeen) {
            stopsSeen = rig.stops.size();
            // Keyed again at once, so the run counts every stall.
            rig.watchdog.setKeyed(kPhone, true, 1);
        }
        run.longestGapMs = std::max(run.longestGapMs, at - last);
        last = at;
        rig.watchdog.keepalive(kPhone, seq, 1, RemoteTxWatchdog::Path::TxChannel);
    }
    rig.advanceTo(last + 1);
    run.holds = rig.holdsStarted();
    run.stops = static_cast<int>(rig.stops.size());
    return run;
}

constexpr quint32 kLossSeed = 20260925;

} // namespace

void TestRemoteTxWatchdog::fivePercentLossOnTheTxChannelNeverHolds()
{
    const LossRun run = runWithLoss(0.05, /*reliable=*/false, kLossSeed);
    qInfo("tx channel, 5%% loss, 10 min: %d of 6000 lost, longest silence %lld ms, %d holds, "
          "%d stops",
          run.lost, static_cast<long long>(run.longestGapMs), run.holds, run.stops);
    QVERIFY(run.lost > 200);  // about 300 of 6000
    QCOMPARE(run.holds, 0);
    QCOMPARE(run.stops, 0);
    // Three lost in a row is a 400 ms silence, which is not more than 400.
    QVERIFY(run.longestGapMs <= RemoteTxWatchdog::kLinkHoldMs);
}

void TestRemoteTxWatchdog::theSameLossOnAReliableChannelHolds()
{
    // Why the keepalives have their own channel: on a reliable in-order
    // channel a lost keepalive holds every later one behind its
    // retransmission, and a keepalive lost twice holds them for 600 ms,
    // long enough to hold the key's RF silent.
    const LossRun run = runWithLoss(0.05, /*reliable=*/true, kLossSeed);
    qInfo("reliable channel, 5%% loss, 10 min: %d losses, longest silence %lld ms, %d holds, "
          "%d stops",
          run.lost, static_cast<long long>(run.longestGapMs), run.holds, run.stops);
    QVERIFY(run.longestGapMs > RemoteTxWatchdog::kLinkHoldMs);
    QVERIFY2(run.holds > 0, "the reliable channel never stalled past the hold");
}

QTEST_GUILESS_MAIN(TestRemoteTxWatchdog)
#include "tst_remote_tx_watchdog.moc"
