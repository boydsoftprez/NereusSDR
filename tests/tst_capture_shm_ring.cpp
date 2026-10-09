// =================================================================
// tests/tst_capture_shm_ring.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  The PC mic hand-off's shared memory
// and wake signal (R-AUD-17, V-SW-6): the names, and a numbered ramp of
// 1,000,000 stereo frames passed from a child process (the writer, the
// helper's side) to this one (the reader, the window's side) through the
// clock matcher's ring in a region the child attaches by name.
//
// The two processes run in lockstep: the child writes one 64-frame block,
// posts the wake and waits for this side's ack, so the schedule is the
// same on every run.  The reader runs 10 % slow (9 reads per 10 blocks),
// then 10 % fast (11 per 10).  The matcher's ratio is forced to 1.0 at
// equal rates, so the frames pass unchanged and the order check is exact:
// every frame that is not slewed or blended carries its own number.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 13 (R-AUD-17, V-SW-6). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QProcess>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QThread>

#include "core/audio/AudioDelayProbe.h"
#include "core/audio/CaptureShm.h"
#include "core/audio/DeviceRateMatcher.h"
#include "core/audio/MatcherRing.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <thread>
#include <vector>

using namespace NereusSDR;

namespace {

constexpr int kBlockFrames = 64;
constexpr std::uint32_t kRampFrames = 1'000'000;
constexpr int kBlocks = static_cast<int>(kRampFrames / kBlockFrames);   // 15625
constexpr int kSlowBlocks = kBlocks / 2;
constexpr std::size_t kAckBytes = 64;

DeviceRateMatcher::Config ringConfig()
{
    DeviceRateMatcher::Config c;
    c.inRate = 48000;
    c.outRate = 48000;
    c.writeBlockFrames = kBlockFrames;
    c.callbackFrames = kBlockFrames;
    c.delayMs = 10;
    return c;
}

// The right channel's check value for frame number v: an integer in
// [2^22, 2^23), exact in a float, that a scaled or blended frame does not
// reproduce.
float checkValue(std::uint32_t v)
{
    const std::uint32_t mixed = (v * 2654435761u) >> 10;
    return static_cast<float>(0x400000u + (mixed & 0x3FFFFFu));
}

int runChild(const QStringList& args)
{
    if (args.size() != 5) {
        return 2;
    }
    const std::size_t ringBytes = args[4].toULongLong();
    auto ring = CaptureShmRegion::attach({args[0], args[1]}, ringBytes);
    auto ack = CaptureShmRegion::attach({args[2], args[3]}, kAckBytes);
    if (!ring || !ack) {
        return 3;
    }
    DeviceRateMatcher matcher(ringConfig(), ring->data(), ring->size());
    if (!matcher.valid()) {
        return 4;
    }
    matcher.forceRatioForTest(1.0);
    std::vector<float> block(static_cast<std::size_t>(kBlockFrames) * 2);
    for (int b = 0; b < kBlocks; ++b) {
        for (int f = 0; f < kBlockFrames; ++f) {
            const auto v = static_cast<std::uint32_t>(b * kBlockFrames + f + 1);
            block[2 * f + 0] = static_cast<float>(v);
            block[2 * f + 1] = checkValue(v);
        }
        matcher.write(block.data(), kBlockFrames, audioProbeNowNs());
        ring->postWake();
        if (!ack->waitWake()) {
            return 5;
        }
    }
    return 0;
}

struct Counters {
    std::uint64_t dryRuns = 0;
    std::uint64_t overruns = 0;
};

Counters countersOf(const MatcherRingHeader* ring)
{
    Counters c;
    if (ring != nullptr) {
        c.dryRuns = ring->dryRuns.load(std::memory_order_acquire);
        c.overruns = ring->overruns.load(std::memory_order_acquire);
    }
    return c;
}

} // namespace

class TstCaptureShmRing : public QObject {
    Q_OBJECT

private slots:
    void namesFollowTheContract()
    {
        const CaptureShmNames names = makeCaptureShmNames(4194303, 0xdeadbeefu);
#if defined(Q_OS_WIN)
        QCOMPARE(names.memory, QStringLiteral("Local\\nrsc-4194303-deadbeefm"));
        QCOMPARE(names.wake, QStringLiteral("Local\\nrsc-4194303-deadbeefw"));
#else
        QCOMPARE(names.memory, QStringLiteral("/nrsc-4194303-deadbeefm"));
        QCOMPARE(names.wake, QStringLiteral("/nrsc-4194303-deadbeefw"));
        QCOMPARE(names.memory.size(), 23);
        QVERIFY(names.memory.size() <= kCaptureShmNameMaxChars);
#endif
        // Eight lower-case hex digits, zero padded.
        const CaptureShmNames small = makeCaptureShmNames(1, 0x1Au);
        QVERIFY(small.memory.endsWith(QStringLiteral("nrsc-1-0000001am")));
        QVERIFY(small.wake.endsWith(QStringLiteral("nrsc-1-0000001aw")));
#if !defined(Q_OS_WIN)
        // The longest pid a 64-bit system gives still fits.
        QVERIFY(makeCaptureShmNames(4194304, 0xffffffffu).memory.size() <= kCaptureShmNameMaxChars);
#endif
    }

    void regionSharesMemoryAndWakes()
    {
        const CaptureShmNames names = makeCaptureShmNames(QCoreApplication::applicationPid(),
                                                          QRandomGenerator::global()->generate());
        auto owner = CaptureShmRegion::create(names, 4096);
        QVERIFY(owner);
        QCOMPARE(owner->size(), std::size_t(4096));
        // Zeroed, and a second create under the same names fails.
        QCOMPARE(static_cast<const unsigned char*>(owner->data())[100], 0u);
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("CaptureShm: ")));
        QVERIFY(!CaptureShmRegion::create(names, 4096));

        auto peer = CaptureShmRegion::attach(names, 4096);
        QVERIFY(peer);
        std::memcpy(owner->data(), "ring", 4);
        QCOMPARE(std::memcmp(peer->data(), "ring", 4), 0);
        // More than the region holds is refused.
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("CaptureShm: ")));
        QVERIFY(!CaptureShmRegion::attach(names, 1 << 20));

        peer->postWake();
        QVERIFY(owner->waitWake());

        // Unlinking keeps both mappings and the wake working, and a new
        // attach then fails.
        owner->unlinkNames();
#if !defined(Q_OS_WIN)
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("CaptureShm: ")));
        QVERIFY(!CaptureShmRegion::attach(names, 4096));
#endif
        peer->postWake();
        QVERIFY(owner->waitWake());

        // shutdownWake releases a blocked waiter with false.
        bool result = true;
        std::thread waiter([&] { result = owner->waitWake(); });
        QThread::msleep(20);
        owner->shutdownWake();
        waiter.join();
        QVERIFY(!result);
        QVERIFY(!owner->waitWake());
    }

    void rampPassesInOrderAcrossProcesses()
    {
        const DeviceRateMatcher::Config config = ringConfig();
        const std::size_t ringBytes = DeviceRateMatcher::ringBytes(config);
        QVERIFY(ringBytes > 0);
        const qint64 pid = QCoreApplication::applicationPid();
        const CaptureShmNames ringNames =
            makeCaptureShmNames(pid, QRandomGenerator::global()->generate());
        const CaptureShmNames ackNames =
            makeCaptureShmNames(pid, QRandomGenerator::global()->generate());
        auto ring = CaptureShmRegion::create(ringNames, ringBytes);
        auto ack = CaptureShmRegion::create(ackNames, kAckBytes);
        QVERIFY(ring && ack);

        std::vector<float> out;
        out.reserve(static_cast<std::size_t>(kRampFrames + 4096) * 2);
        MatcherRingHeader* header = nullptr;
        MatcherReader reader;
        Counters atSwitch;
        std::vector<char> readDryRan;   // per 64-frame read: it ran dry
        int blocksSeen = 0;
        bool attachFailed = false;

        std::thread readerThread([&] {
            std::vector<float> buf(static_cast<std::size_t>(kBlockFrames) * 2);
            while (ring->waitWake()) {
                if (header == nullptr) {
                    header = attachMatcherRing(ring->data(), ring->size());
                    if (header == nullptr) {
                        attachFailed = true;
                        return;
                    }
                    reader = MatcherReader(header);
                }
                const bool slow = blocksSeen < kSlowBlocks;
                if (blocksSeen == kSlowBlocks) {
                    atSwitch = countersOf(header);
                }
                const bool tenth = (blocksSeen % 10) == 9;
                const int reads = slow ? (tenth ? 0 : 1) : (tenth ? 2 : 1);
                for (int i = 0; i < reads; ++i) {
                    const std::uint64_t dryBefore = header->dryRuns.load(std::memory_order_relaxed);
                    reader.read(buf.data(), kBlockFrames);
                    readDryRan.push_back(
                        header->dryRuns.load(std::memory_order_relaxed) != dryBefore ? 1 : 0);
                    out.insert(out.end(), buf.begin(), buf.end());
                }
                ++blocksSeen;
                ack->postWake();
            }
        });

        QProcess child;
        child.setProcessChannelMode(QProcess::ForwardedErrorChannel);
        child.start(QCoreApplication::applicationFilePath(),
                    {QStringLiteral("--shm-ring-child"), ringNames.memory, ringNames.wake,
                     ackNames.memory, ackNames.wake, QString::number(ringBytes)});
        const bool started = child.waitForStarted(10000);
        const bool finished = started && child.waitForFinished(120000);
        if (!finished) {
            child.kill();
            child.waitForFinished(5000);
        }
        ring->shutdownWake();
        readerThread.join();
        QVERIFY2(finished, "the writer child did not finish");
        QCOMPARE(child.exitStatus(), QProcess::NormalExit);
        QCOMPARE(child.exitCode(), 0);
        QVERIFY(!attachFailed);
        QVERIFY(header != nullptr);
        QCOMPARE(blocksSeen, kBlocks);

        // Drain exactly what is left, so the drain adds no dry run.
        const std::uint64_t written = header->written.load(std::memory_order_acquire);
        const std::uint64_t read = header->read.load(std::memory_order_acquire);
        QCOMPARE(header->skipTo.load(std::memory_order_acquire), kMatcherNoSkip);
        if (written > read) {
            std::vector<float> rest(static_cast<std::size_t>(written - read) * 2);
            reader.read(rest.data(), static_cast<int>(written - read));
            out.insert(out.end(), rest.begin(), rest.end());
        }
        const Counters total = countersOf(header);

        // Every exact frame, in the order played.  A dry run's slew tail
        // starts from the last frame played at full gain (rmatch's dslew),
        // so that frame may play once more, in the read that ran dry.
        std::uint64_t exactCount = 0;
        std::uint64_t heldRepeats = 0;
        std::uint32_t first = 0;
        std::uint32_t previous = 0;
        std::uint64_t gaps = 0;
        const std::size_t frames = out.size() / 2;
        for (std::size_t i = 0; i < frames; ++i) {
            const float left = out[2 * i + 0];
            const float right = out[2 * i + 1];
            if (!(left >= 1.0f) || left > static_cast<float>(kRampFrames)
                || std::floor(left) != left) {
                continue;
            }
            const auto v = static_cast<std::uint32_t>(left);
            if (right != checkValue(v)) {
                continue;
            }
            if (exactCount == 0) {
                first = v;
            } else if (v == previous && i / kBlockFrames < readDryRan.size()
                       && readDryRan[i / kBlockFrames] != 0) {
                ++heldRepeats;
                continue;
            } else {
                QVERIFY2(v > previous, qPrintable(QStringLiteral("frame %1 after %2 at %3")
                                                      .arg(v).arg(previous).arg(i)));
                if (v != previous + 1) {
                    ++gaps;
                }
            }
            previous = v;
            ++exactCount;
        }
        QCOMPARE(first, 1u);
        QCOMPARE(previous, kRampFrames);
        QVERIFY2(atSwitch.overruns > 0, "no overrun while the reader ran slow");
        QVERIFY2(total.dryRuns > atSwitch.dryRuns, "no dry run while the reader ran fast");
        QCOMPARE(total.overruns, atSwitch.overruns);   // the fast reader never overruns
        QCOMPARE(gaps, total.dryRuns + total.overruns);
        QVERIFY(heldRepeats <= total.dryRuns);
        qInfo("exact frames %llu, gaps %llu, held repeats %llu, overruns %llu, dry runs %llu",
              static_cast<unsigned long long>(exactCount), static_cast<unsigned long long>(gaps),
              static_cast<unsigned long long>(heldRepeats),
              static_cast<unsigned long long>(total.overruns),
              static_cast<unsigned long long>(total.dryRuns));
    }
};

int main(int argc, char* argv[])
{
    if (argc > 1 && std::strcmp(argv[1], "--shm-ring-child") == 0) {
        QStringList args;
        for (int i = 2; i < argc; ++i) {
            args << QString::fromLocal8Bit(argv[i]);
        }
        return runChild(args);
    }
    QCoreApplication app(argc, argv);
    TstCaptureShmRing test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_capture_shm_ring.moc"
