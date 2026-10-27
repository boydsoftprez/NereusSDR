// =================================================================
// tests/tst_starvation_policy.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test file.
//
// iPhone app plan Task 37 (R-IOS-13; remote design section 12.3, spec
// section 4.6 item 2): what the Core does when a keyed device's microphone
// line starves on a live link. The real microphone receiver (Task 36)
// signals starvation 250 ms after the last audio; the policy holds RF
// silent at that moment (the key stays) in the modes where silence is a
// bare or garbage carrier (AM, SAM, FM, DRM, RADE), releases the hold when
// audio comes again, and does nothing in the eight where silence sends
// nothing (LSB, USB, DSB, CWL, CWU, DIGL, DIGU, SPEC). In every mode the
// receiver's no-audio signal, 10 s after the last packet, stops
// transmitting. TUNE and two-tone use no microphone and are never held or
// stopped by it.
//
// Timing uses an injected clock and scheduler, never sleeps.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original test for NereusSDR by J.J. Boyd (KG4VCF), iPhone
//               app plan Task 37 (R-IOS-13), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-10-16: the device id is logged in hex on every starvation path.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26: Remote transmit hold then cut: starvation holds RF in
//               the carrier modes and audio again releases it; the 10 s
//               no-audio signal stops transmitting in every mode, releasing
//               a hold; TUNE and two-tone are neither held nor stopped.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26: Remote transmit hold then cut, Task 5: releaseHold lets
//               go of a hold with no stop. J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
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
    struct Hold {
        bool held;
        qint64 atMs;
    };
    std::vector<Hold> holds;
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
        hooks.holdRf = [this](bool held) { holds.push_back({held, time.nowMs}); };
        policy.setHooks(std::move(hooks));
        QObject::connect(&receiver, &RemoteMicReceiver::starved, &receiver,
                         [this](bool starved) { policy.onStarved(kPhone, starved); });
        QObject::connect(&receiver, &RemoteMicReceiver::noAudio, &receiver,
                         [this]() { policy.onNoAudio(kPhone); });
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

const QList<DSPMode>& holdModes()
{
    static const QList<DSPMode> modes{DSPMode::AM,  DSPMode::SAM,    DSPMode::FM,
                                      DSPMode::DRM, DSPMode::RADE_U, DSPMode::RADE_L,
                                      static_cast<DSPMode>(14), static_cast<DSPMode>(15)};
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
    void starvationHoldsAt250ms_data();
    void starvationHoldsAt250ms();
    void starvationKeepsTheKeyThenNoAudioStopsAt10s_data();
    void starvationKeepsTheKeyThenNoAudioStopsAt10s();
    void audioAgainReleasesTheHold();
    void tuneAndTwoToneAreExempt_data();
    void tuneAndTwoToneAreExempt();
    void noTransmitModeKnownIsTheSafeAnswer();
    void onStarvedNeverStops();
    void onNoAudioStopsAndReleasesTheHold_data();
    void onNoAudioStopsAndReleasesTheHold();
    void onNoAudioIgnoresTuneAndTwoTone();
    void theStopSentenceIsPlain();
    void theDeviceIdIsLoggedInHex_data();
    void theDeviceIdIsLoggedInHex();
    void releaseHoldReleasesWithNoStop();
};

void TestStarvationPolicy::theTableCoversEveryModeOnce()
{
    QCOMPARE(holdModes().size(), 8);
    QCOMPARE(keepModes().size(), 8);
    for (int value = static_cast<int>(DSPMode::LSB); value <= 15;
         ++value) {
        const DSPMode mode = static_cast<DSPMode>(value);
        const bool hold = holdModes().contains(mode);
        const bool keep = keepModes().contains(mode);
        QVERIFY2(hold != keep, qPrintable(QStringLiteral("mode %1 is in one list").arg(value)));
        QCOMPARE(StarvationPolicy::actionFor(mode),
                 hold ? StarvationAction::Hold : StarvationAction::KeepKeyed);
    }
    // DSB stays keyed: WDSP's DSB modulator adds no carrier (ammod.c,
    // xammod mode 1), so silence there sends nothing.
    QCOMPARE(StarvationPolicy::actionFor(DSPMode::DSB), StarvationAction::KeepKeyed);
    // A mode the table does not know: the safe answer.
    QCOMPARE(StarvationPolicy::actionFor(static_cast<DSPMode>(99)), StarvationAction::Hold);
}

void TestStarvationPolicy::starvationHoldsAt250ms_data()
{
    QTest::addColumn<DSPMode>("mode");
    for (DSPMode mode : holdModes()) {
        QTest::newRow(QByteArray::number(static_cast<int>(mode)).constData()) << mode;
    }
}

void TestStarvationPolicy::starvationHoldsAt250ms()
{
    QFETCH(DSPMode, mode);
    Rig rig;
    rig.mode = mode;
    const qint64 last = rig.streamUntil(2000);
    QVERIFY(rig.holds.empty());
    rig.time.advanceTo(last + RemoteMicConfig::kStarvationMs - 1);
    QVERIFY(rig.holds.empty());
    rig.time.advanceTo(last + RemoteMicConfig::kStarvationMs);
    QCOMPARE(rig.holds.size(), size_t(1));
    QVERIFY(rig.holds.front().held);
    QCOMPARE(rig.holds.front().atMs - last, qint64(250));
    QVERIFY(rig.policy.isHolding());
    // The key stays: a hold is not a stop.
    QVERIFY(rig.stops.empty());
    rig.time.advanceTo(last + RemoteMicConfig::kNoAudioCutMs - 1);
    QVERIFY(rig.stops.empty());
    QCOMPARE(rig.holds.size(), size_t(1));
    // 10 s without audio: the Core stops, once, and the hold goes with it.
    rig.time.advanceTo(last + RemoteMicConfig::kNoAudioCutMs);
    QCOMPARE(rig.stops.size(), size_t(1));
    QCOMPARE(rig.stops.front().atMs - last, qint64(10000));
    QCOMPARE(rig.stops.front().message,
             QStringLiteral("No microphone audio arrived from JJ's iPhone, so the Core stopped transmitting."));
    QCOMPARE(rig.holds.size(), size_t(2));
    QVERIFY(!rig.holds.back().held);
    QVERIFY(!rig.policy.isHolding());
    rig.time.advanceTo(last + 60000);
    QCOMPARE(rig.stops.size(), size_t(1));
    QCOMPARE(rig.holds.size(), size_t(2));
}

void TestStarvationPolicy::starvationKeepsTheKeyThenNoAudioStopsAt10s_data()
{
    QTest::addColumn<DSPMode>("mode");
    for (DSPMode mode : keepModes()) {
        QTest::newRow(QByteArray::number(static_cast<int>(mode)).constData()) << mode;
    }
}

void TestStarvationPolicy::starvationKeepsTheKeyThenNoAudioStopsAt10s()
{
    QFETCH(DSPMode, mode);
    Rig rig;
    rig.mode = mode;
    const qint64 last = rig.streamUntil(2000);
    QSignalSpy starved(&rig.receiver, &RemoteMicReceiver::starved);
    rig.time.advanceTo(last + RemoteMicConfig::kStarvationMs);
    QCOMPARE(starved.count(), 1);  // starvation happened ...
    // ... and silence sends nothing here: no hold, the key goes on.
    QVERIFY(rig.holds.empty());
    rig.time.advanceTo(last + RemoteMicConfig::kNoAudioCutMs - 1);
    QVERIFY(rig.stops.empty());
    // A mic line with no audio packets for 10 s ends the key in every mode.
    rig.time.advanceTo(last + RemoteMicConfig::kNoAudioCutMs);
    QCOMPARE(rig.stops.size(), size_t(1));
    QCOMPARE(rig.stops.front().atMs - last, qint64(10000));
    QCOMPARE(rig.stops.front().message,
             QStringLiteral("No microphone audio arrived from JJ's iPhone, so the Core stopped transmitting."));
    QVERIFY(rig.holds.empty());
    rig.time.advanceTo(last + 180 * 1000);
    QCOMPARE(rig.stops.size(), size_t(1));
}

void TestStarvationPolicy::audioAgainReleasesTheHold()
{
    Rig rig;
    rig.mode = DSPMode::AM;
    const qint64 last = rig.streamUntil(1000);
    rig.time.advanceTo(last + 5000);
    QCOMPARE(rig.holds.size(), size_t(1));
    QVERIFY(rig.holds.front().held);
    // Audio again: the hold is released at once, and the 10 s runs again
    // from the new last packet.
    const qint64 again = rig.streamUntil(last + 5000 + 4);
    QCOMPARE(rig.holds.size(), size_t(2));
    QVERIFY(!rig.holds.back().held);
    QCOMPARE(rig.holds.back().atMs, again);
    QVERIFY(!rig.policy.isHolding());
    rig.time.advanceTo(last + RemoteMicConfig::kNoAudioCutMs);
    QVERIFY(rig.stops.empty());
    rig.time.advanceTo(again + RemoteMicConfig::kNoAudioCutMs - 1);
    QVERIFY(rig.stops.empty());
    rig.time.advanceTo(again + RemoteMicConfig::kNoAudioCutMs);
    QCOMPARE(rig.stops.size(), size_t(1));
}

void TestStarvationPolicy::tuneAndTwoToneAreExempt_data()
{
    QTest::addColumn<DSPMode>("mode");
    for (DSPMode mode : holdModes()) {
        QTest::newRow(QByteArray::number(static_cast<int>(mode)).constData()) << mode;
    }
    for (DSPMode mode : keepModes()) {
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
    rig.time.advanceTo(last + 60000);
    QVERIFY(rig.holds.empty());
    QVERIFY(rig.stops.empty());
}

void TestStarvationPolicy::noTransmitModeKnownIsTheSafeAnswer()
{
    Rig rig;
    rig.mode.reset();
    const qint64 last = rig.streamUntil(300);
    rig.time.advanceTo(last + RemoteMicConfig::kStarvationMs);
    QCOMPARE(rig.holds.size(), size_t(1));
    QVERIFY(rig.holds.front().held);
    QVERIFY(rig.stops.empty());
}

void TestStarvationPolicy::onStarvedNeverStops()
{
    StarvationPolicy policy;
    int stops = 0;
    QList<bool> holds;
    StarvationPolicy::Hooks hooks;
    hooks.transmitMode = [] { return std::optional<DSPMode>(DSPMode::FM); };
    hooks.stopAllTx = [&stops](const QString&) { ++stops; };
    hooks.holdRf = [&holds](bool held) { holds.push_back(held); };
    policy.setHooks(std::move(hooks));
    // Audio while not holding: nothing to release.
    QVERIFY(!policy.onStarved(kPhone, false));
    QVERIFY(holds.isEmpty());
    QVERIFY(!policy.onStarved(kPhone, true));
    QCOMPARE(holds, QList<bool>{true});
    QVERIFY(policy.isHolding());
    QVERIFY(!policy.onStarved(kPhone, false));
    QCOMPARE(holds, (QList<bool>{true, false}));
    QVERIFY(!policy.isHolding());
    QCOMPARE(stops, 0);

    // Without a holdRf hook it is a no-op, and still never a stop.
    StarvationPolicy bare;
    StarvationPolicy::Hooks bareHooks;
    bareHooks.transmitMode = [] { return std::optional<DSPMode>(DSPMode::AM); };
    bareHooks.stopAllTx = [&stops](const QString&) { ++stops; };
    bare.setHooks(std::move(bareHooks));
    QVERIFY(!bare.onStarved(kPhone, true));
    QVERIFY(!bare.onStarved(kPhone, false));
    QCOMPARE(stops, 0);
}

void TestStarvationPolicy::onNoAudioStopsAndReleasesTheHold_data()
{
    QTest::addColumn<int>("mode");
    QTest::addColumn<bool>("held");
    QTest::newRow("USB") << static_cast<int>(DSPMode::USB) << false;
    QTest::newRow("AM") << static_cast<int>(DSPMode::AM) << true;
}

void TestStarvationPolicy::onNoAudioStopsAndReleasesTheHold()
{
    QFETCH(int, mode);
    QFETCH(bool, held);
    StarvationPolicy policy;
    QStringList stops;
    QList<bool> holds;
    StarvationPolicy::Hooks hooks;
    hooks.transmitMode = [mode] { return std::optional<DSPMode>(static_cast<DSPMode>(mode)); };
    hooks.microphoneUnused = [] { return false; };
    hooks.stopAllTx = [&stops](const QString& message) { stops.push_back(message); };
    hooks.deviceName = [](const QByteArray&) { return QStringLiteral("JJ's iPhone"); };
    hooks.holdRf = [&holds](bool on) { holds.push_back(on); };
    policy.setHooks(std::move(hooks));
    policy.onStarved(kPhone, true);
    QCOMPARE(policy.isHolding(), held);
    QVERIFY(policy.onNoAudio(kPhone));
    QCOMPARE(stops, QStringList{StarvationPolicy::stopMessage(QStringLiteral("JJ's iPhone"))});
    QCOMPARE(holds, held ? (QList<bool>{true, false}) : QList<bool>{});
    QVERIFY(!policy.isHolding());
}

void TestStarvationPolicy::onNoAudioIgnoresTuneAndTwoTone()
{
    StarvationPolicy policy;
    int stops = 0;
    int holds = 0;
    StarvationPolicy::Hooks hooks;
    hooks.transmitMode = [] { return std::optional<DSPMode>(DSPMode::AM); };
    hooks.microphoneUnused = [] { return true; };
    hooks.stopAllTx = [&stops](const QString&) { ++stops; };
    hooks.holdRf = [&holds](bool) { ++holds; };
    policy.setHooks(std::move(hooks));
    QVERIFY(!policy.onStarved(kPhone, true));
    QVERIFY(!policy.onNoAudio(kPhone));
    QVERIFY(!policy.onStarved(kPhone, false));
    QCOMPARE(stops, 0);
    QCOMPARE(holds, 0);
}

void TestStarvationPolicy::theStopSentenceIsPlain()
{
    const QString sentence = StarvationPolicy::stopMessage(QStringLiteral("JJ's iPhone"));
    QCOMPARE(sentence,
             QStringLiteral("No microphone audio arrived from JJ's iPhone, so the Core stopped transmitting."));
    QVERIFY2(OperatorWording::isPlain(sentence), qPrintable(sentence));
    QVERIFY(OperatorWording::coreCalledStationIn(sentence).isEmpty());
}


namespace {
QStringList* g_logged = nullptr;

void keepMessage(QtMsgType, const QMessageLogContext&, const QString& message)
{
    if (g_logged) {
        g_logged->push_back(message);
    }
}
} // namespace

void TestStarvationPolicy::theDeviceIdIsLoggedInHex_data()
{
    QTest::addColumn<int>("mode");
    QTest::addColumn<bool>("microphoneUnused");
    QTest::addColumn<bool>("noAudio");
    QTest::newRow("stays keyed") << static_cast<int>(DSPMode::DIGU) << false << false;
    QTest::newRow("holds") << static_cast<int>(DSPMode::AM) << false << false;
    QTest::newRow("TUNE") << static_cast<int>(DSPMode::AM) << true << false;
    QTest::newRow("no audio") << static_cast<int>(DSPMode::USB) << false << true;
    QTest::newRow("no audio in TUNE") << static_cast<int>(DSPMode::USB) << true << true;
}

void TestStarvationPolicy::theDeviceIdIsLoggedInHex()
{
    QFETCH(int, mode);
    QFETCH(bool, microphoneUnused);
    QFETCH(bool, noAudio);
    // A device id is binary; the log line names it in hex, never as raw bytes.
    const QByteArray id = QByteArray::fromHex("85c8ce8584") + QByteArrayLiteral("ABg");
    StarvationPolicy policy;
    StarvationPolicy::Hooks hooks;
    hooks.transmitMode = [mode] { return std::optional<DSPMode>(static_cast<DSPMode>(mode)); };
    hooks.microphoneUnused = [microphoneUnused] { return microphoneUnused; };
    hooks.stopAllTx = [](const QString&) {};
    policy.setHooks(std::move(hooks));

    QStringList logged;
    g_logged = &logged;
    const QtMessageHandler previous = qInstallMessageHandler(keepMessage);
    if (noAudio) {
        policy.onNoAudio(id);
    } else {
        policy.onStarved(id, true);
    }
    qInstallMessageHandler(previous);
    g_logged = nullptr;

    QStringList lines;
    for (const QString& line : logged) {
        if (line.contains(QStringLiteral("Microphone line from"))) {
            lines.push_back(line);
        }
    }
    QCOMPARE(lines.size(), 1);
    QVERIFY2(lines.front().contains(QString::fromLatin1(id.toHex())), qPrintable(lines.front()));
    for (const QChar c : lines.front()) {
        QVERIFY2(c.unicode() >= 0x20 && c.unicode() < 0x7f, qPrintable(lines.front()));
    }
}

// Task 5: the key's end, or TUNE or two-tone starting, releases a hold
// with no stop; with no hold on it does nothing.
void TestStarvationPolicy::releaseHoldReleasesWithNoStop()
{
    StarvationPolicy policy;
    std::vector<bool> holds;
    int stops = 0;
    StarvationPolicy::Hooks hooks;
    hooks.transmitMode = [] { return std::optional<DSPMode>(DSPMode::AM); };
    hooks.microphoneUnused = [] { return false; };
    hooks.stopAllTx = [&stops](const QString&) { ++stops; };
    hooks.holdRf = [&holds](bool held) { holds.push_back(held); };
    policy.setHooks(std::move(hooks));
    policy.releaseHold();
    QVERIFY(holds.empty());
    QVERIFY(!policy.onStarved(kPhone, true));
    QVERIFY(policy.isHolding());
    policy.releaseHold();
    QVERIFY(!policy.isHolding());
    QCOMPARE(holds, (std::vector<bool>{true, false}));
    policy.releaseHold();
    QCOMPARE(holds.size(), size_t(2));
    QCOMPARE(stops, 0);
}

QTEST_GUILESS_MAIN(TestStarvationPolicy)
#include "tst_starvation_policy.moc"
