// =================================================================
// tests/tst_starvation_policy.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test file.
//
// iPhone app plan Task 37 (R-IOS-13; remote design section 12.3, spec
// section 4.6 item 2): what the Core does when a keyed device's microphone
// line starves on a live link. The real microphone receiver (Task 36)
// signals starvation 250 ms after the last audio; the policy stops
// transmitting at that moment in the six modes where silence is a bare or
// garbage carrier (AM, SAM, FM, DRM, RADE_U, RADE_L), and keeps the key in
// the eight where silence sends nothing (LSB, USB, DSB, CWL, CWU, DIGL,
// DIGU, SPEC) until the time-out or the operator ends it. TUNE and
// two-tone use no microphone and are never stopped by it.
//
// Timing uses an injected clock and scheduler, never sleeps.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original test for NereusSDR by J.J. Boyd (KG4VCF), iPhone
//               app plan Task 37 (R-IOS-13), with AI-assisted
//               implementation via Anthropic Claude Code.
// =================================================================

#include "core/safety/StarvationPolicy.h"
#include "core/session/media/PcmAudioCodec.h"
#include "core/session/media/RemoteMicReceiver.h"

#include "OperatorWording.h"

#include <QtTest>

#include <functional>
#include <map>
#include <optional>
#include <vector>

using namespace NereusSDR;

Q_DECLARE_METATYPE(NereusSDR::DSPMode)

namespace {

constexpr quint32 kMicSsrc = 0x6d696301U;
const QByteArray kPhone = QByteArrayLiteral("phone-1");

struct FakeTime {
    qint64 nowMs = 0;
    std::multimap<qint64, std::function<void()>> due;

    RemoteMicReceiver::Clock clock()
    {
        return [this] { return nowMs; };
    }
    RemoteMicReceiver::Scheduler scheduler()
    {
        return [this](int ms, std::function<void()> fire) {
            due.emplace(nowMs + ms, std::move(fire));
        };
    }
    void advanceTo(qint64 ms)
    {
        while (!due.empty() && due.begin()->first <= ms) {
            auto next = due.begin();
            nowMs = next->first;
            std::function<void()> fire = std::move(next->second);
            due.erase(next);
            fire();
        }
        nowMs = ms;
    }
};

QByteArray l16Packet(float value, quint16 sequence, quint32 timestamp)
{
    QVector<float> stereo(PcmAudioCodecConfig::kPacketFrames * 2, value);
    return PcmAudioPacketiser{}.encode(stereo, sequence, timestamp, kMicSsrc).packet;
}

// A keyed device's line, the real receiver, and the policy behind it, as
// the Core wires them (DaemonMediaController -> StationServer).
struct Rig {
    FakeTime time;
    RemoteMicFeed feed;
    RemoteMicReceiver receiver{&feed, nullptr, time.clock(), time.scheduler()};
    StarvationPolicy policy;
    std::optional<DSPMode> mode;
    bool microphoneUnused = false;
    struct Stop {
        QString message;
        qint64 atMs;
    };
    std::vector<Stop> stops;
    quint16 sequence = 1;

    Rig()
    {
        StarvationPolicy::Hooks hooks;
        hooks.transmitMode = [this] { return mode; };
        hooks.microphoneUnused = [this] { return microphoneUnused; };
        hooks.stopAllTx = [this](const QString& message) {
            stops.push_back({message, time.nowMs});
        };
        hooks.deviceName = [](const QByteArray&) { return QStringLiteral("JJ's iPhone"); };
        policy.setHooks(std::move(hooks));
        QObject::connect(&receiver, &RemoteMicReceiver::starved, &receiver,
                         [this](bool starved) { policy.onStarved(kPhone, starved); });
        receiver.start(kMicSsrc, true);
        feed.setInUse(true);
        receiver.setWatching(true);
    }

    // Audio every 4 ms (one L16 packet) up to `untilMs`; returns the time
    // of the last packet.
    qint64 streamUntil(qint64 untilMs)
    {
        qint64 last = time.nowMs;
        for (qint64 t = time.nowMs + 4; t <= untilMs; t += 4) {
            time.advanceTo(t);
            receiver.submit(l16Packet(0.1f, sequence, static_cast<quint32>(sequence) * 192U));
            ++sequence;
            last = t;
        }
        return last;
    }
};

const QList<DSPMode>& unkeyModes()
{
    static const QList<DSPMode> modes{DSPMode::AM,  DSPMode::SAM,    DSPMode::FM,
                                      DSPMode::DRM, DSPMode::RADE_U, DSPMode::RADE_L};
    return modes;
}

const QList<DSPMode>& keepModes()
{
    static const QList<DSPMode> modes{DSPMode::LSB,  DSPMode::USB,  DSPMode::DSB,
                                      DSPMode::CWL,  DSPMode::CWU,  DSPMode::DIGL,
                                      DSPMode::DIGU, DSPMode::SPEC};
    return modes;
}

} // namespace

class TestStarvationPolicy : public QObject {
    Q_OBJECT

private slots:
    void theTableCoversEveryModeOnce();
    void starvationUnkeysAt250ms_data();
    void starvationUnkeysAt250ms();
    void starvationKeepsTheKey_data();
    void starvationKeepsTheKey();
    void tuneAndTwoToneAreExempt_data();
    void tuneAndTwoToneAreExempt();
    void noTransmitModeKnownIsTheSafeAnswer();
    void audioAgainIsNotAStop();
    void theStopSentenceIsPlain();
};

void TestStarvationPolicy::theTableCoversEveryModeOnce()
{
    QCOMPARE(unkeyModes().size(), 6);
    QCOMPARE(keepModes().size(), 8);
    for (int value = static_cast<int>(DSPMode::LSB); value <= static_cast<int>(DSPMode::RADE_L);
         ++value) {
        const DSPMode mode = static_cast<DSPMode>(value);
        const bool unkey = unkeyModes().contains(mode);
        const bool keep = keepModes().contains(mode);
        QVERIFY2(unkey != keep, qPrintable(QStringLiteral("mode %1 is in one list").arg(value)));
        QCOMPARE(StarvationPolicy::actionFor(mode),
                 unkey ? StarvationAction::Unkey : StarvationAction::KeepKeyed);
    }
    // DSB stays keyed: WDSP's DSB modulator adds no carrier (ammod.c,
    // xammod mode 1), so silence there sends nothing.
    QCOMPARE(StarvationPolicy::actionFor(DSPMode::DSB), StarvationAction::KeepKeyed);
}

void TestStarvationPolicy::starvationUnkeysAt250ms_data()
{
    QTest::addColumn<DSPMode>("mode");
    for (DSPMode mode : unkeyModes()) {
        QTest::newRow(QByteArray::number(static_cast<int>(mode)).constData()) << mode;
    }
}

void TestStarvationPolicy::starvationUnkeysAt250ms()
{
    QFETCH(DSPMode, mode);
    Rig rig;
    rig.mode = mode;
    const qint64 last = rig.streamUntil(2000);
    QVERIFY(rig.stops.empty());
    rig.time.advanceTo(last + RemoteMicConfig::kStarvationMs - 1);
    QVERIFY(rig.stops.empty());
    rig.time.advanceTo(last + RemoteMicConfig::kStarvationMs);
    QCOMPARE(rig.stops.size(), size_t(1));
    QCOMPARE(rig.stops.front().atMs - last, qint64(250));
    QCOMPARE(rig.stops.front().message,
             QStringLiteral("No microphone audio arrived from JJ's iPhone, so the Core stopped transmitting."));
    // Once: it does not stop again while the line stays quiet.
    rig.time.advanceTo(last + 60000);
    QCOMPARE(rig.stops.size(), size_t(1));
}

void TestStarvationPolicy::starvationKeepsTheKey_data()
{
    QTest::addColumn<DSPMode>("mode");
    for (DSPMode mode : keepModes()) {
        QTest::newRow(QByteArray::number(static_cast<int>(mode)).constData()) << mode;
    }
}

void TestStarvationPolicy::starvationKeepsTheKey()
{
    QFETCH(DSPMode, mode);
    Rig rig;
    rig.mode = mode;
    const qint64 last = rig.streamUntil(2000);
    QSignalSpy starved(&rig.receiver, &RemoteMicReceiver::starved);
    rig.time.advanceTo(last + RemoteMicConfig::kStarvationMs);
    QCOMPARE(starved.count(), 1);  // starvation happened ...
    // ... and the key goes on, silent, until the phone's 3-minute time-out
    // or the operator ends it (neither is this policy's).
    rig.time.advanceTo(last + 180 * 1000);
    QVERIFY(rig.stops.empty());
}

void TestStarvationPolicy::tuneAndTwoToneAreExempt_data()
{
    QTest::addColumn<DSPMode>("mode");
    for (DSPMode mode : unkeyModes()) {
        QTest::newRow(QByteArray::number(static_cast<int>(mode)).constData()) << mode;
    }
}

void TestStarvationPolicy::tuneAndTwoToneAreExempt()
{
    QFETCH(DSPMode, mode);
    Rig rig;
    rig.mode = mode;
    rig.microphoneUnused = true;  // TUNE or two-tone on
    const qint64 last = rig.streamUntil(500);
    rig.time.advanceTo(last + 10000);
    QVERIFY(rig.stops.empty());
}

void TestStarvationPolicy::noTransmitModeKnownIsTheSafeAnswer()
{
    Rig rig;
    rig.mode.reset();
    const qint64 last = rig.streamUntil(300);
    rig.time.advanceTo(last + RemoteMicConfig::kStarvationMs);
    QCOMPARE(rig.stops.size(), size_t(1));
}

void TestStarvationPolicy::audioAgainIsNotAStop()
{
    StarvationPolicy policy;
    int stops = 0;
    StarvationPolicy::Hooks hooks;
    hooks.transmitMode = [] { return std::optional<DSPMode>(DSPMode::FM); };
    hooks.stopAllTx = [&stops](const QString&) { ++stops; };
    policy.setHooks(std::move(hooks));
    QVERIFY(!policy.onStarved(kPhone, false));
    QCOMPARE(stops, 0);
    QVERIFY(policy.onStarved(kPhone, true));
    QCOMPARE(stops, 1);
}

void TestStarvationPolicy::theStopSentenceIsPlain()
{
    const QString sentence = StarvationPolicy::stopMessage(QStringLiteral("JJ's iPhone"));
    QCOMPARE(sentence,
             QStringLiteral("No microphone audio arrived from JJ's iPhone, so the Core stopped transmitting."));
    QVERIFY2(OperatorWording::isPlain(sentence), qPrintable(sentence));
    QVERIFY(OperatorWording::coreCalledStationIn(sentence).isEmpty());
}

QTEST_GUILESS_MAIN(TestStarvationPolicy)
#include "tst_starvation_policy.moc"
