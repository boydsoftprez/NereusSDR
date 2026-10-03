// no-port-check: NereusSDR-original network ordering regression.
#include <QtTest>
#include <QRandomGenerator>
#include <algorithm>
#include <limits>
#include <vector>
#include "core/session/media/AudioJitterBuffer.h"
using namespace NereusSDR;
using Admission = AudioJitterBuffer::Admission;
namespace {
// R-R3-21: a device and rate matcher for the queue, 1 ms a step. The
// matcher starts at its 90 ms working level in a 180 ms ring and plays
// 1 ms a step; the queue releases into it what is due, while the ring has
// room for two more packets (as the receiver's release loop does); when it
// is about to run dry the device takes the expected packet on demand, or
// conceals the interval. Its fill against the working level is the
// downstream excess. Like WDSP rmatch it steers its fill toward the working
// level by resampling: its proportional gain, 4e-6 a frame at 48 kHz
// (third_party/wdsp/src/rmatch.c), reads about 1.9e-4 of rate per ms of
// fill away from 90 ms.
struct SimulatedDevice {
    double levelMs = 90.0;
    bool useMatcherExcess = true;
    void step(AudioJitterBuffer& queue, qint64 tMs)
    {
        constexpr qint64 ms = 1'000'000;
        levelMs = std::max(0.0, levelMs - (1.0 + 1.92e-4 * (levelMs - 90.0)));
        if (useMatcherExcess) { queue.setDownstreamExcessNs(qint64((levelMs - 90.0) * double(ms))); }
        const qint64 packetMs = queue.packetDurationNs() / ms;
        queue.tick(tMs * ms);
        for (int i = 0; i < queue.maxPackets() && 180.0 - levelMs >= 2.0 * double(packetMs); ++i) {
            if (!queue.takeReady(tMs * ms)) { break; }
            levelMs += double(packetMs);
        }
        if (levelMs < 10.0) {
            if (queue.takeExpectedPresentEarly(tMs * ms) || queue.concealExpectedNow(tMs * ms)) {
                levelMs += double(queue.packetDurationNs() / ms);
            }
        }
    }
};
} // namespace
class TstAudioJitterBuffer : public QObject {
    Q_OBJECT
private slots:
    void reorderLossAndLateArrival()
    {
        AudioJitterBuffer queue;
        queue.reset(100);
        QCOMPARE(queue.insert("second", 2020, 40'000'000), Admission::Accepted);
        QCOMPARE(queue.insert("first", 100, 45'000'000), Admission::Accepted);
        QCOMPARE(queue.insert("duplicate", 100, 46'000'000), Admission::Duplicate);
        QVERIFY(!queue.takeReady(124'999'999));
        const auto first = queue.takeReady(125'000'000);
        QVERIFY(first);
        QCOMPARE(first->packet, QByteArray("first"));
        const auto second = queue.takeReady(125'000'000);
        QVERIFY(second);
        QCOMPARE(second->packet, QByteArray("second"));
        QCOMPARE(queue.insert("fourth", 5860, 120'000'000), Admission::Accepted);
        const auto missing = queue.takeReady(160'000'000);
        QVERIFY(missing && missing->concealed());
        QCOMPARE(missing->timestamp, quint32(3940));
        // R-R3-21: the third came 1 ms after its interval was concealed.
        // The queue rewinds to play it next (the concealment heard becomes
        // one interval of added delay) and the hold grows by that interval
        // plus the margin: 80 + 40 + 20 ms.
        QCOMPARE(queue.insert("late third", 3940, 161'000'000), Admission::Rewound);
        QCOMPARE(queue.holdNs(), qint64(140'000'000));
        QCOMPARE(queue.nextTimestamp(), quint32(3940));
        QVERIFY(!queue.takeReady(300'999'999));
        QCOMPARE(queue.takeReady(301'000'000)->packet, QByteArray("late third"));
        QCOMPARE(queue.takeReady(301'000'000)->packet, QByteArray("fourth"));
        // A late copy of a packet that played is only late.
        QCOMPARE(queue.insert("late copy", 3940, 302'000'000), Admission::Late);
        QCOMPARE(queue.holdNs(), qint64(140'000'000));
    }
    void wrapBoundAndReset()
    {
        AudioJitterBuffer queue;
        const quint32 base = 0xfffffe00u;
        queue.reset(base);
        QCOMPARE(queue.maxPackets(), 8);
        for (int i = 0; i < queue.maxPackets(); ++i) {
            QCOMPARE(queue.insert("frame", base + i * 1920u, i * 40'000'000LL),
                     Admission::Accepted);
        }
        QCOMPARE(queue.queuedPackets(), queue.maxPackets());
        QCOMPARE(queue.insert("too far", base + 8 * 1920u, 0), Admission::OutsideWindow);
        for (int i = 0; i < queue.maxPackets(); ++i) {
            const auto frame = queue.takeReady(80'000'000 + i * 40'000'000LL);
            QVERIFY(frame && !frame->concealed());
            QCOMPARE(frame->timestamp, base + i * 1920u);
        }
        queue.reset(500);
        QVERIFY(!queue.takeReady(9'000'000'000));
        QCOMPARE(queue.queuedPackets(), 0);
        QCOMPARE(queue.insert("unaligned", 501, 0), Admission::Invalid);
    }
    void arrivalClockIsPreserved_data()
    {
        QTest::addColumn<int>("ppm");
        QTest::addColumn<int>("packetFrames");
        QTest::addColumn<int>("packets");
        QTest::addColumn<int>("peakLow");
        QTest::addColumn<int>("peakHigh");
        // The 80 ms hold is two or three 40 ms Opus packets, and twenty or
        // twenty-one 4 ms lossless packets.
        QTest::newRow("fast producer") << 500 << 1920 << 90000 << 2 << 3;
        QTest::newRow("slow producer") << -500 << 1920 << 90000 << 2 << 3;
        QTest::newRow("lossless fast producer") << 500 << 192 << 150000 << 20 << 21;
        QTest::newRow("lossless slow producer") << -500 << 192 << 150000 << 20 << 21;
    }
    void arrivalClockIsPreserved()
    {
        QFETCH(int, ppm);
        QFETCH(int, packetFrames);
        QFETCH(int, packets);
        QFETCH(int, peakLow);
        QFETCH(int, peakHigh);
        AudioJitterBuffer queue(packetFrames, qint64(packetFrames) * 1'000'000'000 / 48'000);
        queue.reset(0);
        // Independent producer timing (one simulated hour of Opus, ten
        // minutes of lossless). Every packet remains held for 80 ms; timing
        // does not converge to a local packet-period timer and concealment
        // does not substitute for valid early/late data.
        const double packetNs = (packetFrames * 1'000'000'000.0 / 48'000.0)
            / (1.0 + ppm / 1'000'000.0);
        int produced = 0;
        int consumed = 0;
        int peakQueued = 0;
        while (consumed < packets) {
            const qint64 due = qRound64(consumed * packetNs) + AudioJitterBuffer::kHoldNs;
            const qint64 arrival = qRound64(produced * packetNs);
            if (produced < packets && arrival <= due) {
                QCOMPARE(queue.insert("audio", quint32(produced) * quint32(packetFrames), arrival),
                         Admission::Accepted);
                ++produced;
                peakQueued = qMax(peakQueued, queue.queuedPackets());
            } else {
                QVERIFY(!queue.takeReady(due - 1));
                const auto frame = queue.takeReady(due);
                QVERIFY(frame && !frame->concealed());
                QCOMPARE(frame->timestamp, quint32(consumed) * quint32(packetFrames));
                ++consumed;
            }
        }
        QCOMPARE(produced, packets);
        QVERIFY2(peakQueued >= peakLow && peakQueued <= peakHigh,
                 qPrintable(QString::number(peakQueued)));
        QCOMPARE(queue.queuedPackets(), 0);
    }
    void demandReleaseRequiresExactPresentPacket()
    {
        AudioJitterBuffer queue;
        queue.reset(100);

        // A future packet cannot bypass or conceal the missing head.
        QCOMPARE(queue.insert("future", 2020, 5'000'000), Admission::Accepted);
        QVERIFY(!queue.takeExpectedPresentEarly());
        QCOMPARE(queue.nextTimestamp(), quint32(100));
        QCOMPARE(queue.queuedPackets(), 1);

        QCOMPARE(queue.insert("expected", 100, 10'000'000), Admission::Accepted);
        const auto expected = queue.takeExpectedPresentEarly();
        QVERIFY(expected && !expected->concealed());
        QCOMPARE(expected->packet, QByteArray("expected"));
        QCOMPARE(expected->timestamp, quint32(100));
        QCOMPARE(queue.nextTimestamp(), quint32(2020));

        // The future packet retains its own arrival-plus-hold release time.
        QVERIFY(!queue.takeReady(84'999'999));
        const auto future = queue.takeReady(85'000'000);
        QVERIFY(future && future->packet == QByteArray("future"));
    }
    void demandReleasePreservesMissingDeadlineResetAndWrap()
    {
        AudioJitterBuffer queue;
        const quint32 base = 0xfffffe00u;
        queue.reset(base);
        QCOMPARE(queue.insert("expected", base, 10'000'000), Admission::Accepted);
        const auto expected = queue.takeExpectedPresentEarly();
        QVERIFY(expected && expected->timestamp == base);
        QCOMPARE(queue.nextTimestamp(), base + 1920u);

        // Early release does not move the producer-derived 90+40 ms loss
        // deadline forward to the demand time.
        QVERIFY(!queue.takeReady(129'999'999));
        const auto missing = queue.takeReady(130'000'000);
        QVERIFY(missing && missing->concealed());
        QCOMPARE(missing->timestamp, base + 1920u);

        queue.reset(500);
        QVERIFY(!queue.takeExpectedPresentEarly());
        QVERIFY(!queue.takeReady(9'000'000'000));
        QCOMPARE(queue.nextTimestamp(), quint32(500));
    }
    void demandReleasePreservesFutureAnchorRule()
    {
        AudioJitterBuffer normal;
        AudioJitterBuffer demand;
        normal.reset(100);
        demand.reset(100);
        for (AudioJitterBuffer* queue : {&normal, &demand}) {
            QCOMPARE(queue->insert("expected", 100, 0), Admission::Accepted);
            QCOMPARE(queue->insert("after gap", 3940, 50'000'000),
                     Admission::Accepted);
        }

        const auto normalExpected = normal.takeReady(80'000'000);
        const auto demandExpected = demand.takeExpectedPresentEarly();
        QVERIFY(normalExpected && demandExpected);
        QCOMPARE(normalExpected->packet, demandExpected->packet);

        // The queued N+2 packet anchors missing N+1 at 130-40 = 90 ms in
        // both paths. The stored empty-queue deadlines (120 ms) do not replace
        // this pre-existing reorder rule.
        QVERIFY(!normal.takeReady(89'999'999));
        QVERIFY(!demand.takeReady(89'999'999));
        const auto normalMissing = normal.takeReady(90'000'000);
        const auto demandMissing = demand.takeReady(90'000'000);
        QVERIFY(normalMissing && normalMissing->concealed());
        QVERIFY(demandMissing && demandMissing->concealed());
        QCOMPARE(normalMissing->timestamp, demandMissing->timestamp);
        const auto normalFuture = normal.takeReady(130'000'000);
        const auto demandFuture = demand.takeReady(130'000'000);
        QVERIFY(normalFuture && demandFuture);
        QCOMPARE(normalFuture->packet, QByteArray("after gap"));
        QCOMPARE(demandFuture->packet, QByteArray("after gap"));
    }
    // R-R3-23: the lossless shape. 192-frame, 4 ms packets on their own
    // grid; the window is the same 320 ms, so 80 packets rather than the
    // eight that used to stall a lossless stream after 32 ms.
    void losslessShapeKeepsTheWindowInTime()
    {
        AudioJitterBuffer queue(192, 4'000'000);
        QCOMPARE(queue.packetFrames(), 192);
        QCOMPARE(queue.packetDurationNs(), qint64(4'000'000));
        QCOMPARE(queue.maxPackets(), 80);
        QCOMPARE(AudioJitterBuffer::windowPackets(4'000'000), 80);
        QCOMPARE(AudioJitterBuffer::windowPackets(40'000'000), 8);
        queue.reset(1000);
        QCOMPARE(queue.insert("off grid", 1000 + 96, 0), Admission::Invalid);
        QCOMPARE(queue.insert("next opus block", 1000 + 1920, 0), Admission::Accepted);
        QCOMPARE(queue.insert("last in window", 1000 + 79 * 192, 0), Admission::Accepted);
        QCOMPARE(queue.insert("beyond window", 1000 + 80 * 192, 0), Admission::OutsideWindow);
        QCOMPARE(queue.insert("late", 1000 - 192, 0), Admission::Late);

        // A lost packet is one 4 ms interval, at the anchor of the next
        // queued packet (arrival + hold - distance x 4 ms).
        queue.reset(0);
        QCOMPARE(queue.insert("first", 0, 0), Admission::Accepted);
        QCOMPARE(queue.insert("third", 384, 8'000'000), Admission::Accepted);
        QVERIFY(!queue.takeReady(79'999'999));
        QCOMPARE(queue.takeReady(80'000'000)->packet, QByteArray("first"));
        QVERIFY(!queue.takeReady(83'999'999));
        const auto missing = queue.takeReady(84'000'000);
        QVERIFY(missing && missing->concealed());
        QCOMPARE(missing->timestamp, quint32(192));
        QVERIFY(!queue.takeReady(87'999'999));
        QCOMPARE(queue.takeReady(88'000'000)->packet, QByteArray("third"));
        // An empty queue conceals one packet every 4 ms after the last one.
        QVERIFY(!queue.takeReady(91'999'999));
        const auto next = queue.takeReady(92'000'000);
        QVERIFY(next && next->concealed());
        QCOMPARE(next->timestamp, quint32(576));
        QCOMPARE(queue.nextTimestamp(), quint32(768));
    }
    // R-R3-21: a packet that arrives after its interval was concealed
    // deepens the hold by as much as it was late, plus the margin, up to
    // 500 ms; the window grows with it. A late copy of a packet that did
    // play changes nothing. After 2 s without a late packet the hold eases
    // back 20 ms a second, down to the 80 ms it started at.
    void lateArrivalDeepensTheHoldAndASteadyLinkEasesIt()
    {
        constexpr qint64 ms = 1'000'000;
        AudioJitterBuffer queue;
        queue.reset(0);
        QCOMPARE(queue.holdNs(), AudioJitterBuffer::kHoldNs);
        QCOMPARE(queue.insert("zero", 0, 0), Admission::Accepted);
        QCOMPARE(queue.takeReady(80 * ms)->packet, QByteArray("zero"));
        const auto missing = queue.takeReady(120 * ms);
        QVERIFY(missing && missing->concealed());
        QCOMPARE(missing->timestamp, quint32(1920));

        // 230 ms after its interval was concealed, and nothing has played
        // since: the queue rewinds to it and the hold grows by the
        // lateness plus the margin.
        QCOMPARE(queue.insert("one", 1920, 350 * ms), Admission::Rewound);
        QCOMPARE(queue.nextTimestamp(), quint32(1920));
        QCOMPARE(queue.holdNs(), (80 + 230 + 20) * ms);
        QCOMPARE(queue.maxPackets(), AudioJitterBuffer::windowPackets((320 + 250) * ms, 40 * ms));
        QCOMPARE(queue.maxPackets(), 14);
        // A late copy of a packet that played leaves it.
        QCOMPARE(queue.insert("zero again", 0, 355 * ms), Admission::Late);
        QCOMPARE(queue.holdNs(), 330 * ms);

        // Held for the deeper hold: the rewound packet, then the next.
        QCOMPARE(queue.insert("two", 3840, 360 * ms), Admission::Accepted);
        QVERIFY(!queue.takeReady(679 * ms));
        QCOMPARE(queue.takeReady(680 * ms)->packet, QByteArray("one"));
        QVERIFY(!queue.takeReady(689 * ms));
        QCOMPARE(queue.takeReady(690 * ms)->packet, QByteArray("two"));

        // Downstream about to run dry with the expected packet missing and
        // a later one queued: conceal it now rather than wait.
        QCOMPARE(queue.insert("five", 5760 + 2 * 1920, 700 * ms), Admission::Accepted);
        const auto now = queue.concealExpectedNow(701 * ms);
        QVERIFY(now && now->concealed());
        QCOMPARE(now->timestamp, quint32(5760));

        // Never deeper than the ceiling, and far too late to rewind for.
        QCOMPARE(queue.insert("three", 5760, 2730 * ms), Admission::LateConcealed);
        QCOMPARE(queue.holdNs(), AudioJitterBuffer::kMaxHoldNs);
        QCOMPARE(queue.maxPackets(), 18);
        QCOMPARE(queue.nextTimestamp(), quint32(7680));
        // A new anchor keeps the link's hold.
        queue.reset(9600);
        QCOMPARE(queue.holdNs(), AudioJitterBuffer::kMaxHoldNs);
        QCOMPARE(queue.maxPackets(), 18);

        // A steady link: nothing for 2 s, then 20 ms a second.
        queue.takeReady(4729 * ms);
        QCOMPARE(queue.holdNs(), 500 * ms);
        queue.takeReady(4730 * ms);
        QCOMPARE(queue.holdNs(), 480 * ms);
        queue.takeReady(5729 * ms);
        QCOMPARE(queue.holdNs(), 480 * ms);
        queue.takeReady(5730 * ms);
        QCOMPARE(queue.holdNs(), 460 * ms);
        for (qint64 at = 6730; at < 60'000; at += 1000) { queue.takeReady(at * ms); }
        QCOMPARE(queue.holdNs(), AudioJitterBuffer::kHoldNs);
        QCOMPARE(queue.maxPackets(), 8);
    }
    // R-R3-21 (review I1): a rewind replays every interval released since
    // the late packet's own. When one of them played real audio, that
    // would be out of order, so the packet stays late; the hold still
    // grows by its lateness.
    void lateArrivalNeverRewindsOverPlayedAudio()
    {
        constexpr qint64 ms = 1'000'000;
        AudioJitterBuffer queue;
        queue.reset(0);
        QCOMPARE(queue.insert("zero", 0, 0), Admission::Accepted);
        QCOMPARE(queue.takeReady(80 * ms)->packet, QByteArray("zero"));
        // One is missing and concealed; two arrives and plays after it.
        QCOMPARE(queue.insert("two", 3840, 110 * ms), Admission::Accepted);
        const auto hole = queue.takeReady(150 * ms); // two's anchor: 190 - 40
        QVERIFY(hole && hole->concealed());
        QCOMPARE(hole->timestamp, quint32(1920));
        QCOMPARE(queue.takeReady(190 * ms)->packet, QByteArray("two"));
        // One arrives 50 ms after its interval was concealed.
        QCOMPARE(queue.insert("one", 1920, 200 * ms), Admission::LateConcealed);
        QCOMPARE(queue.nextTimestamp(), quint32(5760)); // no rewind
        QCOMPARE(queue.queuedPackets(), 0);
        QCOMPARE(queue.holdNs(), (80 + 50 + 20) * ms);
    }
    // R-R3-21 (review I2, re-review): the delay a rewind added comes back
    // down with the hold. Only a standing excess is shed: queued audio (and
    // the downstream excess) beyond the hold plus the reserve for a whole
    // kShrinkIntervalNs, and then one interval at a time. Arrivals here are
    // jittered by up to 30 ms, as a real link's are. The shed packets are
    // handed back so an Opus decoder can stay continuous.
    void easingShedsTheDelayARewindAdded_data()
    {
        QTest::addColumn<int>("packetFrames");
        QTest::newRow("opus, 40 ms packets") << 1920;
        QTest::newRow("lossless, 4 ms packets") << 192;
    }
    void easingShedsTheDelayARewindAdded()
    {
        QFETCH(int, packetFrames);
        constexpr qint64 ms = 1'000'000;
        const qint64 packetMs = qint64(packetFrames) / 48;
        AudioJitterBuffer queue(packetFrames, packetMs * ms);
        queue.reset(0);
        QRandomGenerator random(20260926);
        // A simulated link and device, 1 ms a step: packet p is sent at
        // p * packetMs, delayed 0-30 ms; the packets sent from 10000 to
        // 10350 ms are held up by a 350 ms stall and arrive together (the
        // matcher has steered to its working level by then).
        struct Arrival { int packet; qint64 atMs; };
        std::vector<Arrival> arrivals;
        constexpr qint64 kRunMs = 50'000;
        const int packets = int(kRunMs / packetMs);
        for (int p = 0; p < packets; ++p) {
            const qint64 sent = qint64(p) * packetMs;
            const qint64 at = sent >= 10'000 && sent < 10'350 ? 10'350 : sent + random.bounded(31);
            arrivals.push_back({p, at});
        }
        std::stable_sort(arrivals.begin(), arrivals.end(),
                         [](const Arrival& a, const Arrival& b) { return a.atMs < b.atMs; });
        std::size_t next = 0;
        SimulatedDevice device;
        quint64 shedReturned = 0;
        qint64 largestShedMs = 0;
        qint64 lastShedMs = -1;
        qint64 minShedGapMs = std::numeric_limits<qint64>::max();
        std::optional<qint64> peakHold;
        std::optional<qint64> floorAtMs;
        qint64 spanAtEnd = 0;
        bool rewound = false;
        // The delay the model plays at (queued span plus the matcher's
        // fill): its baseline before the stall, and the worst lag behind
        // the readout (the hold) once the hold has been easing for 3 s.
        double baselineSum = 0.0;
        int baselineCount = 0;
        qint64 worstLagMs = std::numeric_limits<qint64>::min();
        qint64 worstLagAtMs = -1;
        std::optional<qint64> firstEaseMs;
        QStringList series;
        for (qint64 t = 0; t <= kRunMs + 200; ++t) {
            while (next < arrivals.size() && arrivals[next].atMs <= t) {
                const auto admission = queue.insert(
                    "p", quint32(arrivals[next].packet) * quint32(packetFrames), t * ms);
                rewound = rewound || admission == Admission::Rewound;
                ++next;
            }
            const qint64 holdBefore = queue.holdNs();
            peakHold = std::max(peakHold.value_or(0), queue.holdNs());
            device.step(queue, t);
            if (!firstEaseMs && queue.holdNs() < holdBefore) { firstEaseMs = t; }
            if (!floorAtMs && *peakHold > AudioJitterBuffer::kHoldNs
                && queue.holdNs() == AudioJitterBuffer::kHoldNs) {
                floorAtMs = t;
            }
            const std::vector<QByteArray> shed = queue.takeShedPackets();
            if (!shed.empty()) {
                shedReturned += shed.size();
                largestShedMs = std::max(largestShedMs, qint64(shed.size()) * packetMs);
                if (lastShedMs >= 0) { minShedGapMs = std::min(minShedGapMs, t - lastShedMs); }
                lastShedMs = t;
            }
            const qint64 delayMs = queue.queuedSpanNs() / ms + qint64(device.levelMs);
            if (t >= 7000 && t < 9990) { baselineSum += double(delayMs); ++baselineCount; }
            if (firstEaseMs && t >= *firstEaseMs + 3000 && t < kRunMs - 100) {
                const qint64 lag = delayMs - qint64(baselineSum / baselineCount)
                    - (queue.holdNs() - AudioJitterBuffer::kHoldNs) / ms;
                if (lag > worstLagMs) { worstLagMs = lag; worstLagAtMs = t; }
            }
            if (t == kRunMs - 20) { spanAtEnd = queue.queuedSpanNs(); }
            if (t % 1000 == 0) {
                series << QStringLiteral("%1/%2/%3").arg(queue.queuedSpanNs() / ms)
                              .arg(qint64(device.levelMs)).arg(queue.holdNs() / ms);
            }
        }
        qInfo().noquote() << "span/matcher/hold by second:" << series.join(QLatin1Char(' '));
        const QString evidence = QStringLiteral(
            "rewound=%1 peakHold=%2 trimmed=%3 skipped=%4 returned=%5 lastShed=%6 minGap=%7 "
            "spanAtEnd=%8 hold=%9 baseline=%10 firstEase=%11 floorAt=%12 worstLag=%13 at %14 "
            "largestShedMs=%15")
            .arg(rewound).arg(peakHold.value_or(0) / ms).arg(queue.trimmedPackets())
            .arg(queue.skippedIntervals()).arg(shedReturned).arg(lastShedMs).arg(minShedGapMs)
            .arg(spanAtEnd / ms).arg(queue.holdNs() / ms)
            .arg(baselineSum / baselineCount, 0, 'f', 1).arg(firstEaseMs.value_or(-1))
            .arg(floorAtMs.value_or(-1)).arg(worstLagMs).arg(worstLagAtMs).arg(largestShedMs);
        qInfo().noquote() << evidence;
        QVERIFY2(rewound && *peakHold >= 300 * ms, qPrintable(evidence));
        // Shed, every skipped interval handed back (a missing one as an
        // empty packet), at most one shed a second, and done well before
        // the end: the delay is back and stays back.
        QVERIFY2(queue.skippedIntervals() > 0 && shedReturned == queue.skippedIntervals(),
                 qPrintable(evidence));
        QVERIFY2(minShedGapMs >= AudioJitterBuffer::kShrinkIntervalNs / ms, qPrintable(evidence));
        QVERIFY2(lastShedMs < kRunMs - 5'000, qPrintable(evidence));
        QCOMPARE(queue.holdNs(), AudioJitterBuffer::kHoldNs);
        QVERIFY2(spanAtEnd <= AudioJitterBuffer::kHoldNs + AudioJitterBuffer::kShedReserveNs
                                  + 40 * ms,
                 qPrintable(evidence));
        // The heard delay keeps up with the readout while the hold eases,
        // at either packet size: never more behind it than the reserve,
        // one Opus interval, the link's 30 ms of jitter and the matcher's
        // room above its working level (what rmatch then steers away).
        QVERIFY2(worstLagMs <= 150, qPrintable(evidence));
        // Each shed of a standing excess takes 40 ms at once at either
        // packet size (one Opus interval, ten lossless ones), not one
        // interval: a lossless context sheds as fast as an Opus one, so
        // skips, not a long resampling correction, bring the delay back.
        QVERIFY2(largestShedMs == AudioJitterBuffer::kShedStepNs / ms, qPrintable(evidence));
    }
    // R-R3-21 (re-review): once shedding is armed (a late packet, then 2 s
    // quiet), ordinary jitter is not a standing excess. A head delayed
    // 50 ms while armed, inside the hold, with the rate matcher 30 ms above its
    // working level, sheds nothing: the excess does not last a whole
    // kShrinkIntervalNs.
    void jitteredArrivalsNeverShedOnTimeAudio()
    {
        constexpr qint64 ms = 1'000'000;
        AudioJitterBuffer queue;
        queue.reset(0);
        QRandomGenerator random(7);
        // Arm it: interval 1 is concealed, 2 plays, then 1 comes late.
        QCOMPARE(queue.insert("0", 0, 0), Admission::Accepted);
        QCOMPARE(queue.insert("2", 3840, 110 * ms), Admission::Accepted);
        SimulatedDevice device;
        device.useMatcherExcess = false;
        queue.setDownstreamExcessNs(30 * ms);
        constexpr int kPackets = 750; // 30 s
        // Packet p is sent at p * 40 ms, delayed 0-30 ms, and packet 70
        // by 50 ms: at 2.8 s, after shedding arms (2.2 s) and while the
        // hold is still easing (back at 80 ms about 5.2 s), so armed.
        const auto arrival = [&](qint64 p) {
            return p * 40 + (p == 70 ? 50 : random.bounded(31));
        };
        qint64 p = 3;
        qint64 nextAt = arrival(p);
        bool armed = false;
        for (qint64 t = 0; t <= kPackets * 40; ++t) {
            if (t == 200) {
                QCOMPARE(queue.insert("1", 1920, t * ms), Admission::LateConcealed);
                armed = true;
            }
            while (t >= nextAt) {
                queue.insert("p", quint32(p) * 1920u, t * ms);
                ++p;
                nextAt = arrival(p);
            }
            device.step(queue, t);
        }
        QVERIFY(armed);
        QCOMPARE(queue.holdNs(), AudioJitterBuffer::kHoldNs);
        QCOMPARE(queue.trimmedPackets(), quint64(0));
        QCOMPARE(queue.skippedIntervals(), quint64(0));
    }
    // Load findings 2 (R-R3-21): a stall that leaves a standing excess with
    // no late packet. Playout stops for 150 ms while packets keep arriving
    // on time (a receive worker that did not wake), so the stream from then
    // on plays that much later, and nothing ever arrives late. Before the
    // fix the excess stood to the end (content 157 ms, heard delay 107 ms
    // over its baseline, nothing shed). The
    // excess above the hold plus the reserve is shed in the same 40 ms
    // steps as after a late packet, and the delay comes back to the target;
    // the ordinary jitter before the stall sheds nothing.
    void aStallsStandingExcessIsShedWithoutALatePacket()
    {
        constexpr qint64 ms = 1'000'000;
        AudioJitterBuffer queue;
        queue.reset(0);
        QRandomGenerator random(20260928);
        SimulatedDevice device;
        constexpr qint64 kStallAtMs = 10'000;
        constexpr qint64 kStallMs = 150;
        constexpr qint64 kRunMs = 20'000;
        const auto arrival = [&](qint64 p) { return p * 40 + random.bounded(31); };
        qint64 p = 0;
        qint64 nextAt = arrival(p);
        double baselineSum = 0.0;
        int baselineCount = 0;
        double endSum = 0.0;
        int endCount = 0;
        qint64 contentAtEnd = 0;
        quint64 skippedBeforeStall = 0;
        std::optional<qint64> firstShedMs;
        for (qint64 t = 0; t <= kRunMs; ++t) {
            while (t >= nextAt) {
                QCOMPARE(queue.insert("p", quint32(p) * 1920u, t * ms), Admission::Accepted);
                ++p;
                nextAt = arrival(p);
            }
            if (t >= kStallAtMs && t < kStallAtMs + kStallMs) { continue; }
            device.step(queue, t);
            if (!queue.takeShedPackets().empty() && !firstShedMs) { firstShedMs = t; }
            const double delayMs = double(queue.queuedSpanNs() / ms) + device.levelMs;
            if (t >= 5'000 && t < kStallAtMs) { baselineSum += delayMs; ++baselineCount; }
            if (t == kStallAtMs - 1) { skippedBeforeStall = queue.skippedIntervals(); }
            if (t >= kRunMs - 1'000) { endSum += delayMs; ++endCount; }
            if (t == kRunMs) {
                contentAtEnd = queue.queuedSpanNs() + qint64((device.levelMs - 90.0) * double(ms));
            }
        }
        const double baselineMs = baselineSum / baselineCount;
        const double endMs = endSum / endCount;
        const QString evidence =
            QStringLiteral("baseline=%1 end=%2 contentAtEnd=%3 skipped=%4 beforeStall=%5 "
                           "firstShed=%6 hold=%7")
                .arg(baselineMs, 0, 'f', 1).arg(endMs, 0, 'f', 1).arg(contentAtEnd / ms)
                .arg(queue.skippedIntervals()).arg(skippedBeforeStall)
                .arg(firstShedMs.value_or(-1)).arg(queue.holdNs() / ms);
        qInfo().noquote() << evidence;
        QCOMPARE(queue.holdNs(), AudioJitterBuffer::kHoldNs);
        QCOMPARE(skippedBeforeStall, quint64(0));
        // Shed after the stall, in whole 40 ms steps, at most one a second.
        QVERIFY2(firstShedMs && *firstShedMs >= kStallAtMs + AudioJitterBuffer::kShrinkIntervalNs / ms,
                 qPrintable(evidence));
        QVERIFY2(queue.skippedIntervals() >= 1, qPrintable(evidence));
        // Back to the target, and the delay heard back near its baseline:
        // within less than one shed step of it.
        QVERIFY2(contentAtEnd <= AudioJitterBuffer::kHoldNs + AudioJitterBuffer::kShedReserveNs,
                 qPrintable(evidence));
        QVERIFY2(endMs < baselineMs + double(AudioJitterBuffer::kShedStepNs / ms),
                 qPrintable(evidence));
    }
    // Load findings 2: only a stall arms it. The same standing excess with
    // the consumer waking every millisecond (and wakes 20 ms apart, the
    // stall bound itself) is left alone; one wake 21 ms late arms shedding,
    // and the excess goes 40 ms at a time until the target holds.
    void onlyAConsumerStallArmsSheddingWithoutALatePacket()
    {
        constexpr qint64 ms = 1'000'000;
        AudioJitterBuffer queue;
        queue.reset(0);
        // A backlog standing downstream: 100 ms over the matcher's level,
        // so the content stays over the target (hold plus reserve).
        queue.setDownstreamExcessNs(100 * ms);
        qint64 p = 0;
        // Packets on time every 40 ms; the consumer wakes every stepMs and
        // takes what is due (each take ticks the queue first).
        const auto run = [&](qint64 fromMs, qint64 toMs, qint64 stepMs) {
            for (qint64 t = fromMs; t < toMs; t += stepMs) {
                while (p * 40 <= t) {
                    QCOMPARE(queue.insert("p", quint32(p) * 1920u, t * ms), Admission::Accepted);
                    ++p;
                }
                while (queue.takeReady(t * ms)) {}
            }
        };
        run(0, 5'000, 1);
        run(5'000, 8'000, AudioJitterBuffer::kStallNs / ms);
        QCOMPARE(queue.skippedIntervals(), quint64(0));
        QVERIFY(queue.takeShedPackets().empty());
        // One wake 21 ms after the last.
        run(8'001, 12'000, 1);
        QVERIFY2(queue.skippedIntervals() >= 1,
                 qPrintable(QString::number(queue.skippedIntervals())));
        QCOMPARE(queue.takeShedPackets().size(), std::size_t(queue.skippedIntervals()));
    }
    // R-R3-21 (re-review, M1's second half): running on demand under a
    // deepened hold, with the queue empty and the rate matcher about to run
    // dry, the interval is concealed now rather than at its later loss
    // deadline, so a gap in arrivals cannot underflow the matcher. Before
    // anything has played, and at the base hold, an empty queue is not.
    void demandConcealsAnEmptyQueueOnceItHasPlayed()
    {
        constexpr qint64 ms = 1'000'000;
        AudioJitterBuffer queue;
        queue.reset(0);
        QVERIFY(!queue.concealExpectedNow(0));
        QCOMPARE(queue.insert("0", 0, 0), Admission::Accepted);
        QCOMPARE(queue.takeExpectedPresentEarly(10 * ms)->packet, QByteArray("0"));
        QCOMPARE(queue.queuedPackets(), 0);
        QVERIFY(!queue.concealExpectedNow(20 * ms)); // base hold
        // Interval 1 concealed at its deadline, 2 plays, 1 comes late: the
        // hold deepens.
        QVERIFY(queue.takeReady(130 * ms)->concealed());
        QCOMPARE(queue.insert("2", 3840, 140 * ms), Admission::Accepted);
        QVERIFY(queue.takeExpectedPresentEarly(150 * ms).has_value());
        QCOMPARE(queue.insert("1", 1920, 200 * ms), Admission::LateConcealed);
        QVERIFY(queue.holdNs() > AudioJitterBuffer::kHoldNs);
        QCOMPARE(queue.queuedPackets(), 0);
        const auto concealed = queue.concealExpectedNow(210 * ms);
        QVERIFY(concealed && concealed->concealed());
        QCOMPARE(concealed->timestamp, quint32(5760));
        // A present expected packet is never concealed.
        QCOMPARE(queue.insert("4", 7680, 215 * ms), Admission::Accepted);
        QVERIFY(!queue.concealExpectedNow(220 * ms));
    }
    // R-R3-21 (review I4): a fixed-hold queue (the PCM sink's) keeps a
    // late packet late: no rewind and no deeper hold.
    void fixedHoldKeepsLatePacketsLate()
    {
        constexpr qint64 ms = 1'000'000;
        AudioJitterBuffer queue;
        queue.setAdaptive(false);
        queue.reset(0);
        QCOMPARE(queue.insert("zero", 0, 0), Admission::Accepted);
        QCOMPARE(queue.takeReady(80 * ms)->packet, QByteArray("zero"));
        QVERIFY(queue.takeReady(120 * ms)->concealed());
        QCOMPARE(queue.insert("one", 1920, 350 * ms), Admission::LateConcealed);
        QCOMPARE(queue.holdNs(), AudioJitterBuffer::kHoldNs);
        QCOMPARE(queue.nextTimestamp(), quint32(3840));
        QCOMPARE(queue.maxPackets(), 8);
    }
    // R-R3-21 (review M1): after a demand release under a deepened hold,
    // the empty-queue loss deadline is anchored no later than now plus the
    // base hold, so a stall right after it is concealed in time. At the
    // base hold nothing changes (demandReleasePreservesMissingDeadline...).
    void demandReleaseUnderADeepHoldKeepsTheLossDeadlineNear()
    {
        constexpr qint64 ms = 1'000'000;
        AudioJitterBuffer queue;
        queue.reset(0);
        QCOMPARE(queue.insert("zero", 0, 0), Admission::Accepted);
        QCOMPARE(queue.takeReady(80 * ms)->packet, QByteArray("zero"));
        QVERIFY(queue.takeReady(120 * ms)->concealed());
        // 440 ms late: too late to rewind for, the hold goes to the ceiling.
        QCOMPARE(queue.insert("one", 1920, 560 * ms), Admission::LateConcealed);
        QCOMPARE(queue.holdNs(), AudioJitterBuffer::kMaxHoldNs);
        // Due at 570 + 500 ms; taken on demand at 580 ms.
        QCOMPARE(queue.insert("two", 3840, 570 * ms), Admission::Accepted);
        QCOMPARE(queue.takeExpectedPresentEarly(580 * ms)->packet, QByteArray("two"));
        // The next interval is concealed at 580 + 80 + 40 ms, not 1110.
        QVERIFY(!queue.takeReady(699 * ms));
        const auto next = queue.takeReady(700 * ms);
        QVERIFY(next && next->concealed());
    }
    // R-R3-21: a packet beyond the window moves the head just far enough
    // for it to fit, dropping only the oldest queued packets. With nothing
    // queued (review M2) the head moves to the packet itself, so no run of
    // concealment is made for intervals that were never coming.
    void outsideWindowAdvancesOnlyAsFarAsNeeded()
    {
        AudioJitterBuffer queue;
        queue.reset(0);
        for (int i = 1; i < 8; ++i) {
            QCOMPARE(queue.insert("queued", quint32(i) * 1920u, 0), Admission::Accepted);
        }
        QCOMPARE(queue.insert("far", 10u * 1920u, 0), Admission::OutsideWindow);
        QCOMPARE(queue.advanceToFit(5u * 1920u), 0); // already fits
        QCOMPARE(queue.advanceToFit(10u * 1920u), 2);  // drops 1 and 2
        QCOMPARE(queue.trimmedPackets(), quint64(2));
        QCOMPARE(queue.nextTimestamp(), 3u * 1920u);
        QCOMPARE(queue.insert("far", 10u * 1920u, 0), Admission::Accepted);
        QCOMPARE(queue.queuedPackets(), 6);
        QCOMPARE(queue.takeExpectedPresentEarly()->timestamp, 3u * 1920u);

        AudioJitterBuffer empty;
        empty.reset(0);
        QCOMPARE(empty.insert("far", 100u * 1920u, 0), Admission::OutsideWindow);
        QCOMPARE(empty.advanceToFit(100u * 1920u), 0);
        QCOMPARE(empty.nextTimestamp(), 100u * 1920u);
        QCOMPARE(empty.insert("far", 100u * 1920u, 0), Admission::Accepted);
        QCOMPARE(empty.takeReady(80'000'000)->packet, QByteArray("far"));
    }
    void invalidShapeKeepsTheOpusDefault()
    {
        const AudioJitterBuffer defaulted(0, 0);
        QCOMPARE(defaulted.packetFrames(), AudioJitterBuffer::kDefaultPacketFrames);
        QCOMPARE(defaulted.packetDurationNs(), AudioJitterBuffer::kDefaultPacketDurationNs);
        QCOMPARE(defaulted.maxPackets(), 8);
    }
};
QTEST_APPLESS_MAIN(TstAudioJitterBuffer)
#include "tst_audio_jitter_buffer.moc"
