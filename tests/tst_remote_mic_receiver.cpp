// =================================================================
// tests/tst_remote_mic_receiver.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test file.
//
// iPhone app plan Task 36 (R-IOS-13; remote design sections 8.2 and 8.3):
// the microphone uplink at the Core. The feed gives the transmit pump
// silence until its buffer holds the 60 ms target, then the audio; a tone
// crosses at its level with no gap over 10 ms through 1 % loss (in-band FEC
// rebuilds each lost packet); a sender 200 ppm fast and one 200 ppm slow
// each run 10 minutes keyed with no starvation and no overflow; a key waits
// for the buffer and is answered not ready after 250 ms without audio; and
// starvation is signalled after 250 ms without audio while keyed.
//
// Timing uses an injected clock and scheduler, never sleeps. The 10-minute
// runs are 10 minutes of simulated time: the pump's blocks and the sender's
// packets are interleaved by their own clocks, as fast as the computer can.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original test for NereusSDR by J.J. Boyd (KG4VCF), iPhone
//               app plan Task 36 (R-IOS-13), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-27: R-IOS-13, R-R3-42: the small adaptive buffer. Its numbers,
//               the first loss teaching it, steady packets holding the
//               smallest target, jitter growing it and a steady link easing
//               it back, and a stall's excess shed only in silence. J.J.
//               Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-30: TX stall lane: a replay of stalled drains; the buffer
//               times each packet at its receipt in the transport. J.J.
//               Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-01: TX mic thread: the replay of a held delivery, timed at
//               receipt (each stall runs the buffer dry once); the line
//               decoded on a thread of its own while the owner changes the
//               feed's use and reads the figures, and a pump pulls. J.J.
//               Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-01: TX mic thread fix round 2: a packet received before a
//               later one but handed over after it cannot move the last
//               audio back. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-10-01: TX diagnostics lane: a replay with a half-second pause in
//               the phone's packets mid-key places the one underrun in the
//               feed's figures (when, how long silent, the gap before it and
//               how late the packets came). J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
// =================================================================

#include "core/session/media/MediaPeer.h"
#include "core/session/media/OpusAudioCodec.h"
#include "core/session/media/PcmAudioCodec.h"
#include "core/session/media/RemoteMicReceiver.h"

#include <QSignalSpy>
#include <QThread>
#include <QUuid>
#include <QtTest>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <numbers>
#include <thread>
#include <vector>

using namespace NereusSDR;

namespace {

constexpr quint32 kMicSsrc = 0x6d696301U;
constexpr int kBlock = RemoteMicConfig::kPumpBlockFrames;

// A scheduler and clock driven by the test.
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

// A 1 kHz tone at `amplitude`, one 20 ms frame starting at sample `start`.
std::vector<float> toneFrame(qint64 start, float amplitude)
{
    std::vector<float> frame(RemoteMicConfig::kOpusFrameSamples);
    for (int i = 0; i < RemoteMicConfig::kOpusFrameSamples; ++i) {
        const double t = static_cast<double>(start + i) / RemoteMicConfig::kSampleRate;
        frame[static_cast<size_t>(i)] =
            amplitude * static_cast<float>(std::sin(2.0 * std::numbers::pi * 1000.0 * t));
    }
    return frame;
}

double rms(const float* samples, int count)
{
    double sum = 0.0;
    for (int i = 0; i < count; ++i) {
        sum += static_cast<double>(samples[i]) * samples[i];
    }
    return count > 0 ? std::sqrt(sum / count) : 0.0;
}

// L16 microphone packet: mono duplicated to both channels, as the window
// sends it.
QByteArray l16Packet(float value, quint16 sequence, quint32 timestamp)
{
    QVector<float> stereo(PcmAudioCodecConfig::kPacketFrames * 2, value);
    return PcmAudioPacketiser{}.encode(stereo, sequence, timestamp, kMicSsrc).packet;
}

// ---- R-IOS-13 (2026-09-27): the buffer on simulated time ----

// Speech-like microphone audio: 300 ms of a 1 kHz tone at 0.3 (a word),
// then 200 ms of a -70 dBFS noise floor (a pause), repeating. Sample n is
// the same whenever it is asked for, so what the pump plays can be checked
// against it.
constexpr qint64 kWordFrames = 14'400;
// The most a settled smallest buffer holds after a block: one packet plus
// the 10 ms margin plus the 3 ms reserve it leaves alone.
constexpr int kSmallestSettled =
    RemoteMicConfig::kTargetDepthFrames + RemoteMicConfig::kReserveMs * 48;
constexpr qint64 kCycleFrames = 24'000;
float speechSample(qint64 n)
{
    const qint64 inCycle = n % kCycleFrames;
    if (inCycle < kWordFrames) {
        return 0.3f * static_cast<float>(
            std::sin(2.0 * std::numbers::pi * 1000.0 * static_cast<double>(n) / 48000.0));
    }
    quint32 h = static_cast<quint32>(n) * 2654435761U;
    h ^= h >> 15;
    h *= 2246822519U;
    h ^= h >> 13;
    return 3.0e-4f * (static_cast<float>(h & 0xffffU) / 32768.0f - 1.0f);
}

// The pump takes a block every 4/3 ms of radio time; packet k of 20 ms
// arrives at arrivalMs(k) (the sender's clock, plus whatever the link did).
struct FeedRun {
    std::vector<float> played;
    std::vector<int> fillAfterBlock;       // stats().fillFrames after each block
    RemoteMicFeed::Stats stats;
};

FeedRun runFeed(RemoteMicFeed& feed, int packets, const std::function<double(int k)>& arrivalMs,
                const std::function<void(qint64 block, const RemoteMicFeed&)>& onBlock = {},
                const std::function<bool(qint64 block)>& holdSplices = {})
{
    FeedRun run;
    constexpr int kFrames = RemoteMicConfig::kOpusFrameSamples;
    std::vector<float> packet(kFrames);
    std::vector<float> out(kBlock);
    int next = 0;
    // Until the last packet has had its 20 ms: the tests look at the buffer
    // while packets come, not at it running dry after the last one.
    const qint64 blocks = static_cast<qint64>(packets) * (kFrames / kBlock);
    for (qint64 b = 0; b < blocks; ++b) {
        const double nowMs = static_cast<double>(b) * kBlock * 1000.0 / 48000.0;
        while (next < packets && arrivalMs(next) <= nowMs) {
            for (int i = 0; i < kFrames; ++i) {
                packet[static_cast<size_t>(i)] =
                    speechSample(static_cast<qint64>(next) * kFrames + i);
            }
            feed.write(packet.data(), kFrames);
            ++next;
        }
        feed.pullBlock(out.data(), kBlock, -1.0, holdSplices ? holdSplices(b) : false);
        run.played.insert(run.played.end(), out.begin(), out.end());
        run.fillAfterBlock.push_back(feed.stats().fillFrames);
        if (onBlock) {
            onBlock(b, feed);
        }
    }
    run.stats = feed.stats();
    return run;
}

// The words the pump played: [first loud sample, last loud sample] of each
// run of |x| > 0.05 whose gaps are shorter than 2 ms.
std::vector<std::pair<qint64, qint64>> playedWords(const std::vector<float>& audio)
{
    std::vector<std::pair<qint64, qint64>> words;
    qint64 last = -1'000'000;
    for (qint64 i = 0; i < static_cast<qint64>(audio.size()); ++i) {
        if (std::abs(audio[static_cast<size_t>(i)]) <= 0.05f) {
            continue;
        }
        if (i - last > 96) {
            words.push_back({i, i});
        }
        words.back().second = i;
        last = i;
    }
    return words;
}

// The largest step between neighbouring samples from `from` on: a 1 kHz
// tone at 0.3 steps by at most 0.04; a splice under it by up to 0.6.
double largestStep(const std::vector<float>& audio, size_t from)
{
    double largest = 0.0;
    for (size_t i = std::max<size_t>(from, 1); i < audio.size(); ++i) {
        largest = std::max(largest, static_cast<double>(std::abs(audio[i] - audio[i - 1])));
    }
    return largest;
}

} // namespace

class TestRemoteMicReceiver : public QObject {
    Q_OBJECT

private slots:
    void theLineKeepsTheTransmitNumbers();
    void micSsrcIsDistinctFromTheCoresStreams();
    void feedGivesSilenceUntilTheTargetThenAudio();
    void feedOutOfUseDropsAudioAndEmptiesTheBuffer();
    void toneCrossesAtItsLevelWithinTheBuffersLatency();
    void onePercentLossLeavesNoGapOver10ms();
    void lossWithoutFecIsConcealedWithinOneFrame();
    void twoLostInARowAreConcealedThenRecovered();
    void latePacketsAreDroppedAndCounted();
    void losslessOnlyWhenTheLineAgreedIt();
    void senderOffTheRadiosClockRunsTenMinutesKeyed_data();
    void senderOffTheRadiosClockRunsTenMinutesKeyed();
    void keyWaitIsAnsweredOnceTheBufferFills();
    void keyWaitIsRefusedWhenTheLineNeverSends();
    void keyWaitFillsFromALateFirstPacket();
    void keyWaitRefusesALineThatStartsThenStops();
    void keyWaitEndedAfterItsFirstPacketIsAnsweredOnlyByTheEnd();
    void starvationIsSignalledOnlyWhileWatched();
    void anOlderReceiptCannotMoveTheLastAudioBack();
    // R-IOS-13 (2026-09-27): the small adaptive buffer.
    void steadyPacketsHoldTheSmallestTargetWithoutUnderrun();
    void jitterGrowsTheTargetAndASteadyLinkEasesItBack();
    void aStallsExcessIsShedInSilenceNeverUnderTheVoice();
    void aLongStallsStaleAudioIsNotSentLate();
    void aHeldDeliveryIsTimedAtReceiptAndCountedAsOwnerWaits();
    void theLineIsDecodedOnAThreadOfItsOwn();
    void aMidKeyArrivalPauseIsPlacedInTheUnderrunFigures();
    void nothingIsSplicedWhileDexpTimingRuns();
};

void TestRemoteMicReceiver::theLineKeepsTheTransmitNumbers()
{
    // R-IOS-13 (2026-09-27): the smallest target is one 20 ms packet plus
    // a 10 ms margin; the ceiling stays 120 ms.
    QCOMPARE(RemoteMicConfig::kTargetDepthMs, 30);
    QCOMPARE(RemoteMicConfig::kMinMarginMs, 10);
    QCOMPARE(RemoteMicConfig::kMaxDepthMs, 120);
    QCOMPARE(RemoteMicConfig::kStarvationMs, 250);
    QCOMPARE(RemoteMicConfig::kReadyDeadlineMs, 250);
    QCOMPARE(RemoteMicConfig::kLineStartDeadlineMs, 1000);
    QCOMPARE(RemoteMicConfig::kTargetDepthFrames, 1440);
    QCOMPARE(RemoteMicConfig::kMaxDepthFrames, 5760);
    QCOMPARE(RemoteMicFeed().targetFrames(), 1440);
    QCOMPARE(RemoteMicConfig::kOpusFrameSamples, 960);
    QCOMPARE(RemoteMicConfig::kOpusPayloadType, 111);
    QCOMPARE(RemoteMicConfig::kOpusBitrate, 24000);
}

// The line's SSRC is none of the ids the Core sends on, for any connection.
void TestRemoteMicReceiver::micSsrcIsDistinctFromTheCoresStreams()
{
    for (int i = 0; i < 64; ++i) {
        const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        const quint32 mic = MediaPeer::micAudioSsrcForConnection(id);
        QVERIFY(mic != 0);
        QVERIFY(!MediaPeer::receiverAudioSsrcsForConnection(id).contains(mic));
        QVERIFY(mic != MediaPeer::headphonesAudioSsrcForConnection(id));
        // The same id always gives the same SSRC.
        QCOMPARE(MediaPeer::micAudioSsrcForConnection(id), mic);
    }
    MediaPeer peer;
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QVERIFY(peer.start(IMediaTransport::Role::Answerer, id));
    QVERIFY(peer.micAudioSsrc() != peer.audioSsrc());
    QCOMPARE(peer.micAudioSsrc(), quint32(0));
    peer.stop();
    MediaPeer micPeer;
    QVERIFY(micPeer.start(IMediaTransport::Role::Answerer, id,
                          IMediaTransport::kDefaultAudioTargetBitrate, false, false, false,
                          /*micLine=*/true));
    QCOMPARE(micPeer.micAudioSsrc(), MediaPeer::micAudioSsrcForConnection(id));
    QVERIFY(micPeer.micAudioSsrc() != micPeer.audioSsrc());
    micPeer.stop();
    QCOMPARE(micPeer.micAudioSsrc(), quint32(0));
}

void TestRemoteMicReceiver::feedGivesSilenceUntilTheTargetThenAudio()
{
    RemoteMicFeed feed;
    std::vector<float> out(kBlock, 9.0f);
    // Out of use: the pump keeps its own source.
    QVERIFY(!feed.pull(out.data(), kBlock));
    QCOMPARE(out.front(), 9.0f);

    feed.setInUse(true);
    std::vector<float> audio(RemoteMicConfig::kOpusFrameSamples, 0.5f);
    // 20 ms: below the 30 ms target, the pump hears silence.
    QVERIFY(feed.write(audio.data(), 960));
    QVERIFY(feed.pull(out.data(), kBlock));
    QCOMPARE(rms(out.data(), kBlock), 0.0);
    QVERIFY(!feed.stats().started);
    // 40 ms written: the target is reached and the audio starts.
    QVERIFY(feed.write(audio.data(), 960));
    QCOMPARE(feed.framesSinceInUse(), qint64(1920));
    bool heard = false;
    for (int i = 0; i < 30 && !heard; ++i) {
        QVERIFY(feed.pull(out.data(), kBlock));
        heard = rms(out.data(), kBlock) > 0.4;
    }
    QVERIFY(heard);
    QVERIFY(feed.stats().started);
    QVERIFY(feed.stats().fillFrames <= RemoteMicConfig::kMaxDepthFrames);
    QCOMPARE(feed.stats().overflows, 0);
}

// Out of use, the feed refuses audio and the pump's source is its own; a
// change of use empties the buffer, so nothing from before it is heard.
void TestRemoteMicReceiver::feedOutOfUseDropsAudioAndEmptiesTheBuffer()
{
    RemoteMicFeed feed;
    std::vector<float> audio(960, 0.5f);
    QVERIFY(!feed.write(audio.data(), 960));
    feed.setInUse(true);
    for (int i = 0; i < 5; ++i) {
        QVERIFY(feed.write(audio.data(), 960));
    }
    std::vector<float> out(kBlock);
    QVERIFY(feed.pull(out.data(), kBlock));
    QVERIFY(feed.stats().fillFrames > RemoteMicConfig::kTargetDepthFrames);

    // The key ends: the buffer is emptied, the pump's own source returns.
    feed.setInUse(false);
    QCOMPARE(feed.framesSinceInUse(), qint64(0));
    QVERIFY(!feed.write(audio.data(), 960));
    QVERIFY(!feed.pull(out.data(), kBlock));
    QCOMPARE(feed.stats().fillFrames, 0);
    QVERIFY(!feed.stats().started);

    // In use again: nothing from the last key is heard.
    feed.setInUse(true);
    QVERIFY(feed.pull(out.data(), kBlock));
    QCOMPARE(rms(out.data(), kBlock), 0.0);
    QCOMPARE(feed.stats().fillFrames, 0);
    QVERIFY(feed.stats().changes >= 3);
}

namespace {

// One run of the uplink: `packets` 20 ms frames of a 1 kHz tone from the
// app's encoder, the ones `lose` picks left out, the pump taking a block at
// a time between packets. Returns what the pump heard from its first block.
struct UplinkRun {
    std::vector<float> heard;
    int firstBlockHeard = -1;
    int lost = 0;
    RemoteMicReceiver::Stats receiver;
    RemoteMicFeed::Stats feed;
};

// The signal of frame k: a steady 1 kHz tone, or the tone keyed in 350 ms
// bursts with 150 ms between them at -26 dB, as speech comes in syllables
// (Opus codes in-band FEC only for frames its voice detector calls active).
enum class ToneShape { Steady, Syllables };

UplinkRun runTone(int packets, float amplitude,
                  const std::function<bool(int k, const QByteArray& next)>& lose,
                  ToneShape shape = ToneShape::Steady)
{
    UplinkRun run;
    FakeTime time;
    RemoteMicFeed feed;
    RemoteMicReceiver receiver(&feed, nullptr, time.clock(), time.scheduler());
    RemoteMicEncoder encoder;
    if (!encoder.isReady() || !receiver.start(kMicSsrc, false)) {
        return run;
    }
    feed.setInUse(true);
    // Encoded one ahead, so a loss can be chosen by what the next packet
    // carries.
    std::vector<QByteArray> encoded;
    for (int k = 0; k < packets + 1; ++k) {
        std::vector<float> frame =
            toneFrame(static_cast<qint64>(k) * RemoteMicConfig::kOpusFrameSamples, amplitude);
        if (shape == ToneShape::Syllables && (k % 25) >= 17) {
            for (float& sample : frame) {
                sample *= 0.05f;
            }
        }
        encoded.push_back(encoder.encode(frame.data(), static_cast<quint16>(k),
                                         static_cast<quint32>(k * RemoteMicConfig::kOpusFrameSamples),
                                         kMicSsrc));
    }
    const int blocksPerPacket = RemoteMicConfig::kOpusFrameSamples / kBlock;   // 15
    std::vector<float> pumped(kBlock);
    int block = 0;
    for (int k = 0; k < packets; ++k) {
        if (lose(k, encoded[static_cast<size_t>(k) + 1])) {
            ++run.lost;
        } else {
            receiver.submit(encoded[static_cast<size_t>(k)]);
        }
        for (int b = 0; b < blocksPerPacket; ++b, ++block) {
            feed.pull(pumped.data(), kBlock);
            if (run.firstBlockHeard < 0 && rms(pumped.data(), kBlock) > 0.01) {
                run.firstBlockHeard = block;
            }
            run.heard.insert(run.heard.end(), pumped.begin(), pumped.end());
        }
        time.advanceTo(time.nowMs + 20);
    }
    run.receiver = receiver.stats();
    run.feed = feed.stats();
    return run;
}

// The longest run, in ms, of 1 ms windows from `from` on whose RMS is below
// `floor`.
int longestRunBelow(const std::vector<float>& audio, size_t from, double floor)
{
    constexpr int kWindow = RemoteMicConfig::kSampleRate / 1000;
    int run = 0;
    int longest = 0;
    for (size_t start = from; start + kWindow <= audio.size(); start += kWindow) {
        if (rms(audio.data() + start, kWindow) < floor) {
            longest = std::max(longest, ++run);
        } else {
            run = 0;
        }
    }
    return longest;
}

bool carriesFec(const QByteArray& packet)
{
    AudioRtpPacket parsed;
    if (parseAudioRtp(packet, RemoteMicConfig::kOpusPayloadType, parsed)
        != OpusAudioCodecStatus::Accepted) {
        return false;
    }
    return opusPacketCarriesFec(parsed.payload);
}

} // namespace

// A 1 kHz tone from the app's encoder reaches the pump at its level, within
// the buffer's latency, with no gap at all.
void TestRemoteMicReceiver::toneCrossesAtItsLevelWithinTheBuffersLatency()
{
    constexpr float kAmplitude = 0.3f;
    const UplinkRun run = runTone(1000, kAmplitude, [](int, const QByteArray&) { return false; });
    QVERIFY2(run.firstBlockHeard >= 0, "the tone never reached the pump");
    QCOMPARE(run.feed.underflows, 0);
    QCOMPARE(run.feed.overflows, 0);
    // Within the buffer's latency: the pump starts once it holds its 30 ms
    // target (two 20 ms packets), plus the codec's own delay.
    const double latencyMs =
        run.firstBlockHeard * 1000.0 * kBlock / RemoteMicConfig::kSampleRate;
    QVERIFY2(latencyMs <= RemoteMicConfig::kTargetDepthMs + 30.0,
             qPrintable(QString::number(latencyMs)));
    // At its level: the settled output's RMS is the tone's within 1 dB.
    const size_t settled = static_cast<size_t>(run.firstBlockHeard + 200) * kBlock;
    const double expected = kAmplitude / std::sqrt(2.0);
    const double measured = rms(run.heard.data() + settled,
                                static_cast<int>(run.heard.size() - settled));
    const double errorDb = 20.0 * std::log10(measured / expected);
    QVERIFY2(std::abs(errorDb) < 1.0, qPrintable(QString::number(errorDb)));
    QCOMPARE(longestRunBelow(run.heard, settled, expected / 2.0), 0);
}

// 1 % of the packets lost: each is rebuilt from the next packet's in-band
// FEC when it arrives, so against the same stream without loss the audio
// never falls 6 dB, let alone for 10 ms, and keeps its level. The tone comes
// in syllables because Opus codes FEC only for frames its voice detector
// calls active; a steady tone stops being active within a second (measured
// with this tree's libopus: 39 of 1000 steady-tone packets carry FEC, 720
// of 1000 in syllables). The lost packets are ones whose next packet
// carries FEC, as speech's do; lossWithoutFecIsConcealedWithinOneFrame
// covers the others.
void TestRemoteMicReceiver::onePercentLossLeavesNoGapOver10ms()
{
    constexpr float kAmplitude = 0.3f;
    constexpr int kPackets = 1000;
    const UplinkRun reference = runTone(kPackets, kAmplitude,
        [](int, const QByteArray&) { return false; }, ToneShape::Syllables);
    int nextLoss = 11;
    const UplinkRun run = runTone(kPackets, kAmplitude,
        [&nextLoss](int k, const QByteArray& next) {
            if (k >= nextLoss && carriesFec(next)) {
                nextLoss = k + 95;   // about 1 %, never two in a row
                return true;
            }
            return false;
        },
        ToneShape::Syllables);
    QVERIFY2(run.lost >= 8, qPrintable(QString::number(run.lost)));
    QCOMPARE(run.receiver.recoveredPackets, quint64(run.lost));
    QCOMPARE(run.receiver.concealedPackets, quint64(0));
    QCOMPARE(run.receiver.framesWritten, quint64(kPackets) * RemoteMicConfig::kOpusFrameSamples);
    // R-IOS-13 (2026-09-27): the buffer starts at its smallest target, 10
    // ms past one packet, and a packet rebuilt from the next one's FEC
    // comes 20 ms late. The first loss is the measurement: the buffer may
    // run dry once and grows by what it measured; no later loss reaches it.
    QVERIFY2(run.feed.underflows <= 1, qPrintable(QString::number(run.feed.underflows)));
    QVERIFY(run.feed.marginFrames >= 20 * RemoteMicConfig::kFramesPerMs);
    QCOMPARE(run.firstBlockHeard, reference.firstBlockHeard);
    QCOMPARE(run.heard.size(), reference.heard.size());

    // Where the stream without loss has the tone, the lossy one keeps it
    // within 6 dB; a stretch that does not is a gap. Measured once the
    // buffer has learned the link, 200 packets (4 s) after the first loss,
    // and with the lossy stream moved back by the delay the buffer grew by
    // (found by matching the two streams' 1 ms envelopes).
    constexpr int kWindow = RemoteMicConfig::kSampleRate / 1000;
    const double toneRms = kAmplitude / std::sqrt(2.0);
    int gap = 0;
    int longest = 0;
    const size_t settled = static_cast<size_t>(run.firstBlockHeard + 211 * 15) * kBlock;
    size_t shift = 0;
    double bestMismatch = std::numeric_limits<double>::max();
    for (size_t lag = 0; lag <= 60; ++lag) {
        double mismatch = 0.0;
        for (size_t start = settled; start + (lag + 1) * kWindow <= run.heard.size();
             start += kWindow) {
            mismatch += std::abs(rms(run.heard.data() + start + lag * kWindow, kWindow)
                                 - rms(reference.heard.data() + start, kWindow));
        }
        if (mismatch < bestMismatch) {
            bestMismatch = mismatch;
            shift = lag * kWindow;
        }
    }
    for (size_t start = settled; start + shift + kWindow <= run.heard.size(); start += kWindow) {
        const double want = rms(reference.heard.data() + start, kWindow);
        const double got = rms(run.heard.data() + start + shift, kWindow);
        if (want >= toneRms / 2.0 && got < want / 2.0) {
            longest = std::max(longest, ++gap);
        } else {
            gap = 0;
        }
    }
    QVERIFY2(longest <= 10, qPrintable(QString::number(longest)));
    // At its level: the delay the buffer grew by only shifts the stream,
    // so the level is compared over what both runs played.
    const double levelDb = 20.0 * std::log10(
        rms(run.heard.data() + settled, static_cast<int>(run.heard.size() - settled))
        / rms(reference.heard.data() + settled, static_cast<int>(reference.heard.size() - settled)));
    QVERIFY2(std::abs(levelDb) < 0.5, qPrintable(QString::number(levelDb)));
}

// A lost packet whose next carries no FEC (a steady tone past Opus's voice
// detector) is concealed: the timeline is kept and the tone goes on, faded,
// for no longer than the one lost 20 ms frame.
void TestRemoteMicReceiver::lossWithoutFecIsConcealedWithinOneFrame()
{
    constexpr float kAmplitude = 0.3f;
    int nextLoss = 200;
    const UplinkRun run = runTone(1000, kAmplitude,
        [&nextLoss](int k, const QByteArray& next) {
            if (k >= nextLoss && !carriesFec(next)) {
                nextLoss = k + 100;
                return true;
            }
            return false;
        });
    QVERIFY(run.lost >= 5);
    QCOMPARE(run.receiver.concealedPackets, quint64(run.lost));
    QCOMPARE(run.receiver.framesWritten, quint64(1000) * RemoteMicConfig::kOpusFrameSamples);
    // R-IOS-13: as for FEC, the first loss may run the smallest buffer dry
    // once and teaches it; the later ones are concealed in place.
    QVERIFY2(run.feed.underflows <= 1, qPrintable(QString::number(run.feed.underflows)));
    // Measured from 200 packets (4 s) after the first loss, which comes at
    // packet 200 or later.
    const size_t settled = static_cast<size_t>(run.firstBlockHeard + 411 * 15) * kBlock;
    const double expected = kAmplitude / std::sqrt(2.0);
    const int gapMs = longestRunBelow(run.heard, settled, expected / 4.0);
    QVERIFY2(gapMs <= 20, qPrintable(QString::number(gapMs)));
}

void TestRemoteMicReceiver::twoLostInARowAreConcealedThenRecovered()
{
    FakeTime time;
    RemoteMicFeed feed;
    RemoteMicReceiver receiver(&feed, nullptr, time.clock(), time.scheduler());
    RemoteMicEncoder encoder;
    QVERIFY(receiver.start(kMicSsrc, false));
    feed.setInUse(true);
    for (int k = 0; k < 10; ++k) {
        const std::vector<float> frame = toneFrame(k * 960, 0.3f);
        const QByteArray packet = encoder.encode(frame.data(), static_cast<quint16>(k),
                                                 static_cast<quint32>(k * 960), kMicSsrc);
        if (k != 4 && k != 5) {
            receiver.submit(packet);
        }
    }
    // Packet 4 by loss concealment, packet 5 from packet 6 (its FEC while
    // the voice detector calls the tone active, concealment otherwise):
    // every frame is written, so the buffer keeps its timeline.
    QVERIFY(receiver.stats().concealedPackets >= 1);
    QCOMPARE(receiver.stats().concealedPackets + receiver.stats().recoveredPackets, quint64(2));
    QCOMPARE(receiver.stats().framesWritten, quint64(10 * 960));
}

void TestRemoteMicReceiver::latePacketsAreDroppedAndCounted()
{
    RemoteMicFeed feed;
    RemoteMicReceiver receiver(&feed);
    RemoteMicEncoder encoder;
    QVERIFY(receiver.start(kMicSsrc, false));
    feed.setInUse(true);
    std::vector<QByteArray> packets;
    for (int k = 0; k < 6; ++k) {
        const std::vector<float> frame = toneFrame(k * 960, 0.3f);
        packets.push_back(encoder.encode(frame.data(), static_cast<quint16>(k),
                                         static_cast<quint32>(k * 960), kMicSsrc));
    }
    receiver.submit(packets[0]);
    receiver.submit(packets[1]);
    receiver.submit(packets[3]);   // 2 is missing: recovered from 3's FEC
    receiver.submit(packets[2]);   // too late now
    receiver.submit(packets[3]);   // a repeat
    receiver.submit(packets[4]);
    QCOMPARE(receiver.stats().latePackets, quint64(2));
    QCOMPARE(receiver.stats().recoveredPackets, quint64(1));
    QCOMPARE(receiver.stats().framesWritten, quint64(5 * 960));
    // Another SSRC is refused.
    const std::vector<float> frame = toneFrame(0, 0.3f);
    receiver.submit(encoder.encode(frame.data(), 5, 5 * 960, kMicSsrc + 1));
    QCOMPARE(receiver.stats().rejectedPackets, quint64(1));
}

// L16 on the line only from a connection that agreed it (the desktop
// window's lossless uplink); its mono is the mean of the two channels.
void TestRemoteMicReceiver::losslessOnlyWhenTheLineAgreedIt()
{
    RemoteMicFeed feed;
    RemoteMicReceiver receiver(&feed);
    QVERIFY(receiver.start(kMicSsrc, false));
    feed.setInUse(true);
    receiver.submit(l16Packet(0.25f, 1, 0));
    QCOMPARE(receiver.stats().rejectedPackets, quint64(1));
    QCOMPARE(receiver.stats().framesWritten, quint64(0));

    receiver.setLosslessNegotiated(true);
    for (quint16 s = 2; s < 2 + 20; ++s) {
        receiver.submit(l16Packet(0.25f, s, s * 192U));
    }
    // One lost 4 ms packet is 4 ms of silence; the timeline is kept.
    receiver.submit(l16Packet(0.25f, 23, 23 * 192U));
    QCOMPARE(receiver.stats().concealedPackets, quint64(1));
    QCOMPARE(receiver.stats().framesWritten, quint64(22 * 192));
    std::vector<float> out(kBlock);
    bool heard = false;
    for (int i = 0; i < 40 && !heard; ++i) {
        QVERIFY(feed.pull(out.data(), kBlock));
        heard = std::abs(out[10] - 0.25f) < 0.01f;
    }
    QVERIFY(heard);
}

void TestRemoteMicReceiver::senderOffTheRadiosClockRunsTenMinutesKeyed_data()
{
    QTest::addColumn<double>("ppm");
    QTest::newRow("sender-200ppm-fast") << 200.0;
    QTest::newRow("sender-200ppm-slow") << -200.0;
}

// A sender 200 ppm off the radio's clock, either way, keyed for 10 minutes
// of simulated time: the rate matcher follows it, so the buffer never runs
// empty (no starvation, no underflow) and never overflows.
void TestRemoteMicReceiver::senderOffTheRadiosClockRunsTenMinutesKeyed()
{
    QFETCH(double, ppm);
    FakeTime time;
    RemoteMicFeed feed;
    RemoteMicReceiver receiver(&feed, nullptr, time.clock(), time.scheduler());
    QSignalSpy starved(&receiver, &RemoteMicReceiver::starved);
    QVERIFY(receiver.start(kMicSsrc, true));
    feed.setInUse(true);
    receiver.setWatching(true);

    // L16 keeps 10 minutes cheap; the clocks, not the codec, are under test.
    constexpr double kSeconds = 600.0;
    const double packetSeconds = PcmAudioCodecConfig::kPacketFrames
        / static_cast<double>(RemoteMicConfig::kSampleRate) / (1.0 + ppm * 1e-6);
    const double blockSeconds = kBlock / static_cast<double>(RemoteMicConfig::kSampleRate);
    double nextPacket = 0.0;
    quint16 sequence = 0;
    quint32 timestamp = 0;
    std::vector<float> out(kBlock);
    int minFillAfterStart = RemoteMicConfig::kMaxDepthFrames;
    int maxFill = 0;
    const qint64 blocks = static_cast<qint64>(kSeconds / blockSeconds);
    for (qint64 b = 0; b < blocks; ++b) {
        const double now = static_cast<double>(b) * blockSeconds;
        while (nextPacket <= now) {
            receiver.submit(l16Packet(0.2f, sequence++, timestamp));
            timestamp += PcmAudioCodecConfig::kPacketFrames;
            nextPacket += packetSeconds;
        }
        QVERIFY(feed.pull(out.data(), kBlock));
        const RemoteMicFeed::Stats stats = feed.stats();
        if (stats.started && b > blocks / 100) {
            minFillAfterStart = std::min(minFillAfterStart, stats.fillFrames);
        }
        maxFill = std::max(maxFill, stats.fillFrames);
        const qint64 ms = static_cast<qint64>(now * 1000.0);
        if (ms != time.nowMs) {
            time.advanceTo(ms);
        }
    }
    const RemoteMicFeed::Stats stats = feed.stats();
    QVERIFY(stats.started);
    QCOMPARE(stats.underflows, 0);
    QCOMPARE(stats.overflows, 0);
    QCOMPARE(starved.count(), 0);
    QVERIFY2(minFillAfterStart > 0, qPrintable(QString::number(minFillAfterStart)));
    QVERIFY2(maxFill <= RemoteMicConfig::kMaxDepthFrames, qPrintable(QString::number(maxFill)));
    // The matcher corrected the right way: a fast sender is slowed
    // (fewer frames out than in), a slow one sped up.
    QVERIFY2(ppm > 0 ? stats.ratio < 1.0 : stats.ratio > 1.0,
             qPrintable(QString::number(stats.ratio, 'g', 10)));
    qInfo().noquote() << QStringLiteral("ppm %1: ratio %2, fill %3..%4 frames at the end %5")
                             .arg(ppm).arg(stats.ratio, 0, 'g', 10)
                             .arg(minFillAfterStart).arg(maxFill).arg(stats.fillFrames);
}

// A key waits for the buffer: answered ready as soon as the feed holds its
// 30 ms target, and never twice.
void TestRemoteMicReceiver::keyWaitIsAnsweredOnceTheBufferFills()
{
    FakeTime time;
    RemoteMicFeed feed;
    RemoteMicReceiver receiver(&feed, nullptr, time.clock(), time.scheduler());
    RemoteMicEncoder encoder;
    QVERIFY(receiver.start(kMicSsrc, false));
    feed.setInUse(true);
    QList<bool> answers;
    receiver.awaitReady([&answers](bool ready) { answers.append(ready); });
    QVERIFY(receiver.isWaiting());
    for (int k = 0; k < 2; ++k) {
        QVERIFY(answers.isEmpty());
        const std::vector<float> frame = toneFrame(k * 960, 0.3f);
        receiver.submit(encoder.encode(frame.data(), static_cast<quint16>(k),
                                       static_cast<quint32>(k * 960), kMicSsrc));
        time.advanceTo(time.nowMs + 20);
    }
    QCOMPARE(answers, QList<bool>{true});
    QVERIFY(!receiver.isWaiting());
    time.advanceTo(time.nowMs + 1000);
    QCOMPARE(answers, QList<bool>{true});

    // Already full: answered at once.
    receiver.awaitReady([&answers](bool ready) { answers.append(ready); });
    QCOMPARE(answers, (QList<bool>{true, true}));
}

// Load findings 2 (R-IOS-13): a line that never sends a packet is refused
// once the line's own start bound passes (the 250 ms fill wait runs from
// the first packet).
void TestRemoteMicReceiver::keyWaitIsRefusedWhenTheLineNeverSends()
{
    FakeTime time;
    RemoteMicFeed feed;
    RemoteMicReceiver receiver(&feed, nullptr, time.clock(), time.scheduler());
    QVERIFY(receiver.start(kMicSsrc, false));
    feed.setInUse(true);
    QList<bool> answers;
    receiver.awaitReady([&answers](bool ready) { answers.append(ready); });
    time.advanceTo(RemoteMicConfig::kLineStartDeadlineMs - 1);
    QVERIFY(answers.isEmpty());
    QVERIFY(receiver.isWaiting());
    time.advanceTo(RemoteMicConfig::kLineStartDeadlineMs);
    QCOMPARE(answers, QList<bool>{false});
    QVERIFY(!receiver.isWaiting());
    QCOMPARE(feed.framesSinceInUse(), qint64(0));

    // A cancelled wait is never answered.
    receiver.awaitReady([&answers](bool ready) { answers.append(ready); });
    receiver.cancelWait();
    time.advanceTo(1000);
    QCOMPARE(answers, QList<bool>{false});
}

// Load findings 2 (R-IOS-13): a device opens its microphone with the key,
// so its line can start late. A first packet 900 ms into the wait (past the
// old 250 ms bound, inside the line's start bound) starts the fill wait,
// and the key is answered ready once the feed holds its target, never
// before: at the first packet the feed holds 20 ms, under the 30 ms target.
void TestRemoteMicReceiver::keyWaitFillsFromALateFirstPacket()
{
    FakeTime time;
    RemoteMicFeed feed;
    RemoteMicReceiver receiver(&feed, nullptr, time.clock(), time.scheduler());
    RemoteMicEncoder encoder;
    QVERIFY(receiver.start(kMicSsrc, false));
    feed.setInUse(true);
    QList<bool> answers;
    receiver.awaitReady([&answers](bool ready) { answers.append(ready); });
    time.advanceTo(900);
    QVERIFY(answers.isEmpty());
    const auto send = [&](int k) {
        const std::vector<float> frame = toneFrame(k * 960, 0.3f);
        receiver.submit(encoder.encode(frame.data(), static_cast<quint16>(k),
                                       static_cast<quint32>(k * 960), kMicSsrc));
    };
    send(0);
    QVERIFY(answers.isEmpty());
    QVERIFY(feed.framesSinceInUse() < feed.targetFrames());
    // The line's start bound passes with the line running: no refusal.
    time.advanceTo(RemoteMicConfig::kLineStartDeadlineMs + 50);
    QVERIFY(answers.isEmpty());
    QVERIFY(receiver.isWaiting());
    send(1);
    QCOMPARE(answers, QList<bool>{true});
    QVERIFY(feed.framesSinceInUse() >= feed.targetFrames());
    // Answered once: neither deadline answers again.
    time.advanceTo(5000);
    QCOMPARE(answers, QList<bool>{true});
}

// Load findings 2 (R-IOS-13): a line that starts and then stops before the
// buffer fills is refused 250 ms after its first packet, as before; the
// line's start bound gives it no more.
void TestRemoteMicReceiver::keyWaitRefusesALineThatStartsThenStops()
{
    FakeTime time;
    RemoteMicFeed feed;
    RemoteMicReceiver receiver(&feed, nullptr, time.clock(), time.scheduler());
    RemoteMicEncoder encoder;
    QVERIFY(receiver.start(kMicSsrc, false));
    feed.setInUse(true);
    QList<bool> answers;
    receiver.awaitReady([&answers](bool ready) { answers.append(ready); });
    time.advanceTo(100);
    const std::vector<float> frame = toneFrame(0, 0.3f);
    receiver.submit(encoder.encode(frame.data(), 0, 0, kMicSsrc));
    time.advanceTo(100 + RemoteMicConfig::kReadyDeadlineMs - 1);
    QVERIFY(answers.isEmpty());
    time.advanceTo(100 + RemoteMicConfig::kReadyDeadlineMs);
    QCOMPARE(answers, QList<bool>{false});
    QVERIFY(!receiver.isWaiting());
    time.advanceTo(5000);
    QCOMPARE(answers, QList<bool>{false});
}

// Load findings 3 (review of the line-start wait): once the line's first
// packet has armed the 250 ms fill timer, a cancelled wait is never
// answered, and a line that stops answers its waiting key false at once;
// neither timer answers afterwards.
void TestRemoteMicReceiver::keyWaitEndedAfterItsFirstPacketIsAnsweredOnlyByTheEnd()
{
    FakeTime time;
    RemoteMicFeed feed;
    RemoteMicReceiver receiver(&feed, nullptr, time.clock(), time.scheduler());
    RemoteMicEncoder encoder;
    QVERIFY(receiver.start(kMicSsrc, false));
    feed.setInUse(true);
    const std::vector<float> frame = toneFrame(0, 0.3f);
    QList<bool> answers;
    const qint64 pastBothBounds =
        RemoteMicConfig::kLineStartDeadlineMs + RemoteMicConfig::kReadyDeadlineMs + 100;

    // Cancelled after the first packet.
    receiver.awaitReady([&answers](bool ready) { answers.append(ready); });
    time.advanceTo(100);
    receiver.submit(encoder.encode(frame.data(), 0, 0, kMicSsrc));
    QVERIFY(receiver.isWaiting());
    receiver.cancelWait();
    QVERIFY(!receiver.isWaiting());
    time.advanceTo(100 + pastBothBounds);
    QVERIFY(answers.isEmpty());

    // The line stops after the first packet: false at once, and only once.
    // A new over: the feed's change of use empties it, so one packet is
    // again under the target.
    feed.setInUse(false);
    feed.setInUse(true);
    const qint64 waitAt = 100 + pastBothBounds;
    receiver.awaitReady([&answers](bool ready) { answers.append(ready); });
    time.advanceTo(waitAt + 50);
    receiver.submit(encoder.encode(frame.data(), 1, 960, kMicSsrc));
    QVERIFY(answers.isEmpty());
    receiver.stop();
    QCOMPARE(answers, QList<bool>{false});
    QVERIFY(!receiver.isWaiting());
    time.advanceTo(waitAt + 50 + pastBothBounds);
    QCOMPARE(answers, QList<bool>{false});
}

void TestRemoteMicReceiver::starvationIsSignalledOnlyWhileWatched()
{
    FakeTime time;
    RemoteMicFeed feed;
    RemoteMicReceiver receiver(&feed, nullptr, time.clock(), time.scheduler());
    QSignalSpy starved(&receiver, &RemoteMicReceiver::starved);
    QVERIFY(receiver.start(kMicSsrc, true));
    feed.setInUse(true);

    // Not watched: silence is not starvation.
    time.advanceTo(1000);
    QCOMPARE(starved.count(), 0);

    receiver.setWatching(true);
    receiver.submit(l16Packet(0.1f, 1, 0));
    time.advanceTo(1249);
    QCOMPARE(starved.count(), 0);
    time.advanceTo(1250);
    QCOMPARE(starved.count(), 1);
    QCOMPARE(starved.at(0).at(0).toBool(), true);
    QVERIFY(receiver.isStarved());

    // Audio again ends it.
    receiver.submit(l16Packet(0.1f, 2, 192));
    QCOMPARE(starved.count(), 2);
    QCOMPARE(starved.at(1).at(0).toBool(), false);

    // Steady audio: no starvation.
    for (quint16 s = 3; s < 200; ++s) {
        time.advanceTo(time.nowMs + 4);
        receiver.submit(l16Packet(0.1f, s, s * 192U));
    }
    QCOMPARE(starved.count(), 2);

    // Starved again, then the key ends: the starvation ends with it.
    time.advanceTo(time.nowMs + 300);
    QCOMPARE(starved.count(), 3);
    receiver.setWatching(false);
    QCOMPARE(starved.count(), 4);
    QCOMPARE(starved.at(3).at(0).toBool(), false);
    time.advanceTo(time.nowMs + 1000);
    QCOMPARE(starved.count(), 4);
}

// TX mic thread fix round 2: the last audio only moves forward. A packet
// received at 1100 ms is handed over at once; one received at 900 ms (held
// 200 ms) is handed over after it. Starvation is still measured from 1100.
void TestRemoteMicReceiver::anOlderReceiptCannotMoveTheLastAudioBack()
{
    FakeTime time;
    RemoteMicFeed feed;
    RemoteMicReceiver receiver(&feed, nullptr, time.clock(), time.scheduler());
    QSignalSpy starved(&receiver, &RemoteMicReceiver::starved);
    QVERIFY(receiver.start(kMicSsrc, true));
    feed.setInUse(true);
    time.advanceTo(1000);
    receiver.setWatching(true);
    time.advanceTo(1100);
    receiver.submit(l16Packet(0.1f, 1, 0));
    receiver.submit(l16Packet(0.1f, 2, 192), 200'000);
    time.advanceTo(1349);
    QCOMPARE(starved.count(), 0);
    time.advanceTo(1350);
    QCOMPARE(starved.count(), 1);
    QCOMPARE(starved.at(0).at(0).toBool(), true);
}

// R-IOS-13: steady 20 ms packets play from the smallest target, 30 ms (one
// packet plus a 10 ms margin), for a minute with no underrun: the buffer
// never holds more than that, and the delay it adds averages about 20 ms.
void TestRemoteMicReceiver::steadyPacketsHoldTheSmallestTargetWithoutUnderrun()
{
    RemoteMicFeed feed;
    feed.setInUse(true);
    // Packets land 0.4 ms into a pump block, 20 ms apart.
    const FeedRun run = runFeed(feed, 3000, [](int k) { return 0.4 + 20.0 * k; });
    QCOMPARE(run.stats.underflows, 0);
    int maxFill = 0;
    int minFill = std::numeric_limits<int>::max();
    // From 2 s on: the buffer starts on the packet that reaches its target,
    // up to one packet over it, and sheds that in the first pause.
    for (size_t b = 1500; b < run.fillAfterBlock.size() - 250; ++b) {
        maxFill = std::max(maxFill, run.fillAfterBlock[b]);
        minFill = std::min(minFill, run.fillAfterBlock[b]);
    }
    qInfo().noquote() << QStringLiteral("steady: fill %1..%2 frames (%3..%4 ms)")
                             .arg(minFill).arg(maxFill)
                             .arg(minFill / 48.0, 0, 'f', 1).arg(maxFill / 48.0, 0, 'f', 1);
    // After a block the buffer holds at most one packet plus its margin
    // (and the 3 ms it leaves alone), less the block just played.
    QVERIFY2(maxFill <= kSmallestSettled, qPrintable(QString::number(maxFill)));
    QVERIFY2(minFill >= kBlock, qPrintable(QString::number(minFill)));
    QCOMPARE(run.stats.targetFrames, RemoteMicConfig::kTargetDepthFrames);
    QCOMPARE(run.stats.grows, 0);
    QVERIFY2(run.stats.shedFrames <= quint64(20 * 48),
             qPrintable(QString::number(run.stats.shedFrames)));
    QVERIFY2(run.stats.addedMeanMs > 15.0 && run.stats.addedMeanMs < 25.0,
             qPrintable(QString::number(run.stats.addedMeanMs)));
    QVERIFY2(run.stats.addedMaxMs <= 41.0,
             qPrintable(QString::number(run.stats.addedMaxMs)));
    // What was sent is what played, sample for sample, from the start.
    const std::vector<std::pair<qint64, qint64>> words = playedWords(run.played);
    QVERIFY(words.size() >= 100);
    for (size_t w = 1; w + 1 < words.size(); ++w) {
        const qint64 length = words[w].second - words[w].first + 1;
        QVERIFY2(std::abs(length - kWordFrames) <= 24, qPrintable(QString::number(length)));
    }
    QVERIFY2(largestStep(run.played, 0) < 0.045,
             qPrintable(QString::number(largestStep(run.played, 0))));
}

// R-IOS-13: packets up to 15 ms late grow the target by what they measure
// (13 to 25 ms), with at most the first late packet running the buffer dry;
// once the link is steady again the target eases back to its smallest.
void TestRemoteMicReceiver::jitterGrowsTheTargetAndASteadyLinkEasesItBack()
{
    RemoteMicFeed feed;
    feed.setInUse(true);
    constexpr int kJittered = 1000;    // 20 s
    constexpr int kPackets = 2500;     // then 30 s steady
    std::vector<double> arrival(kPackets);
    quint32 seed = 12345U;
    double previous = 0.0;
    for (int k = 0; k < kPackets; ++k) {
        seed = seed * 1664525U + 1013904223U;
        const double late = k < kJittered ? 15.0 * static_cast<double>(seed >> 8) / 16777216.0
                                          : 0.0;
        arrival[static_cast<size_t>(k)] = std::max(previous, 0.4 + 20.0 * k + late);
        previous = arrival[static_cast<size_t>(k)];
    }
    int targetAfterJitter = 0;
    int underrunsAfterJitter = -1;
    const qint64 jitterEndBlock = static_cast<qint64>(kJittered) * 15;
    const FeedRun run = runFeed(feed, kPackets,
        [&arrival](int k) { return arrival[static_cast<size_t>(k)]; },
        [&](qint64 block, const RemoteMicFeed& f) {
            if (block == jitterEndBlock) {
                targetAfterJitter = f.stats().targetFrames;
                underrunsAfterJitter = f.stats().underflows;
            }
        });
    qInfo().noquote() << QStringLiteral("jitter: target %1 ms after 20 s of up to 15 ms late, "
                                        "%2 ms after 30 s steady; underruns %3, grew %4 times")
                             .arg(targetAfterJitter / 48.0, 0, 'f', 1)
                             .arg(run.stats.targetFrames / 48.0, 0, 'f', 1)
                             .arg(run.stats.underflows).arg(run.stats.grows);
    QVERIFY2(targetAfterJitter >= RemoteMicConfig::kTargetDepthFrames + 13 * 48,
             qPrintable(QString::number(targetAfterJitter)));
    QVERIFY2(targetAfterJitter <= RemoteMicConfig::kTargetDepthFrames + 25 * 48,
             qPrintable(QString::number(targetAfterJitter)));
    QVERIFY2(underrunsAfterJitter <= 1, qPrintable(QString::number(underrunsAfterJitter)));
    QCOMPARE(run.stats.underflows, underrunsAfterJitter);
    QCOMPARE(run.stats.targetFrames, RemoteMicConfig::kTargetDepthFrames);
    // The words played whole once the buffer had learned the link.
    const std::vector<std::pair<qint64, qint64>> words = playedWords(run.played);
    for (size_t w = 4; w + 1 < words.size(); ++w) {
        const qint64 length = words[w].second - words[w].first + 1;
        QVERIFY2(std::abs(length - kWordFrames) <= 24,
                 qPrintable(QStringLiteral("word %1: %2").arg(w).arg(length)));
    }
}

// R-IOS-13: the link stalls for 200 ms in the middle of an over (starting
// 100 ms before a pause ends), then delivers what it held at once. The
// buffer runs dry in the pause, plays the late audio whole, and sheds the
// excess only in the following pauses: within two seconds it is back at
// its smallest, no word is shortened, and no sample steps as a splice
// would. The stall was longer than the ceiling could cover, so the target
// does not grow for it.
void TestRemoteMicReceiver::aStallsExcessIsShedInSilenceNeverUnderTheVoice()
{
    RemoteMicFeed feed;
    feed.setInUse(true);
    // A cycle is a 300 ms word and a 200 ms pause; packet k is 20k..20k+20
    // ms. The stall holds packets 70..79 (the last 100 ms of the pause at
    // 1300..1500 ms and the first 100 ms of the next word) until 1600 ms.
    constexpr int kStallFirst = 70;
    constexpr int kStallLast = 79;
    constexpr double kStallEndMs = 20.0 * (kStallLast + 1) + 0.4;
    const auto arrival = [](int k) {
        const double onTime = 0.4 + 20.0 * k;
        return k >= kStallFirst && k <= kStallLast ? kStallEndMs : onTime;
    };
    const qint64 stallEndBlock = static_cast<qint64>(kStallEndMs * 48.0 / kBlock) + 1;
    const FeedRun run = runFeed(feed, 750, arrival);
    // Back at its smallest: the first block from which the buffer never
    // again holds more than a settled smallest buffer does.
    const size_t end = run.fillAfterBlock.size() - 250;
    size_t backAt = end;
    while (backAt > static_cast<size_t>(stallEndBlock)
           && run.fillAfterBlock[backAt - 1] <= kSmallestSettled) {
        --backAt;
    }
    const double backMs = static_cast<double>(static_cast<qint64>(backAt) - stallEndBlock)
        * kBlock / 48.0;
    qInfo().noquote()
        << QStringLiteral("stall: back to the smallest buffer %1 ms after the stall ended; "
                          "shed %2 ms, inserted %3 ms, underruns %4")
               .arg(backMs, 0, 'f', 0)
               .arg(run.stats.shedFrames / 48.0, 0, 'f', 1)
               .arg(run.stats.insertedFrames / 48.0, 0, 'f', 1)
               .arg(run.stats.underflows);
    QVERIFY2(backAt < end - 1500, "the buffer did not come back to its smallest and stay");
    QVERIFY2(backMs <= 2000.0, qPrintable(QString::number(backMs)));
    QCOMPARE(run.stats.underflows, 1);
    QCOMPARE(run.stats.targetFrames, RemoteMicConfig::kTargetDepthFrames);
    QVERIFY(run.stats.shedFrames >= 150 * 48);
    // Every word played whole: nothing shed or cut under the voice.
    const std::vector<std::pair<qint64, qint64>> words = playedWords(run.played);
    QVERIFY(words.size() >= 14);
    for (size_t w = 1; w + 1 < words.size(); ++w) {
        const qint64 length = words[w].second - words[w].first + 1;
        QVERIFY2(std::abs(length - kWordFrames) <= 24,
                 qPrintable(QStringLiteral("word %1: %2").arg(w).arg(length)));
    }
    QVERIFY2(largestStep(run.played, 0) < 0.045,
             qPrintable(QString::number(largestStep(run.played, 0))));
}


// LINK minor 12 (TX audio): the link stalls for a whole second mid-over and
// then delivers everything it held at once. Past kStaleAfterStallMs the
// held audio is stale: the buffer starts again at its target with the
// newest audio instead of sending speech a second late and waiting for
// pauses to shed it.
void TestRemoteMicReceiver::aLongStallsStaleAudioIsNotSentLate()
{
    RemoteMicFeed feed;
    feed.setInUse(true);
    constexpr int kStallFirst = 70;
    constexpr int kStallLast = 119;   // 1 s of packets held
    constexpr double kStallEndMs = 20.0 * (kStallLast + 1) + 0.4;
    const auto arrival = [](int k) {
        const double onTime = 0.4 + 20.0 * k;
        return k >= kStallFirst && k <= kStallLast ? kStallEndMs : onTime;
    };
    const qint64 stallEndBlock = static_cast<qint64>(kStallEndMs * 48.0 / kBlock) + 1;
    const FeedRun run = runFeed(feed, 300, arrival);
    QCOMPARE(run.stats.underflows, 1);
    // From the block the held audio landed on, the buffer never holds more
    // than the ceiling: nothing a second old is waiting to go out.
    int largest = 0;
    for (size_t b = static_cast<size_t>(stallEndBlock); b < run.fillAfterBlock.size(); ++b) {
        largest = std::max(largest, run.fillAfterBlock[b]);
    }
    QVERIFY2(largest <= RemoteMicConfig::kMaxDepthFrames,
             qPrintable(QStringLiteral("%1 frames buffered after the stall").arg(largest)));
}

// R-IOS-13: while DEXP's own timing runs (its hold, decay or VOX turn-off,
// which it counts in the samples it processes), the buffer splices
// nothing, so the pause DEXP sees is the one spoken; the excess waits and
// goes in the first pause after DEXP's timing ends.
void TestRemoteMicReceiver::nothingIsSplicedWhileDexpTimingRuns()
{
    const auto arrival = [](int k) {
        return k >= 70 && k <= 79 ? 20.0 * 80 + 0.4 : 0.4 + 20.0 * k;
    };
    // Held for the whole run: the stall's excess stays, nothing is shed.
    {
        RemoteMicFeed feed;
        feed.setInUse(true);
        const FeedRun run = runFeed(feed, 750, arrival, {}, [](qint64) { return true; });
        QCOMPARE(run.stats.shedFrames, quint64(0));
        QCOMPARE(run.stats.insertedFrames, quint64(0));
        QVERIFY(run.stats.heldBlocks > 10'000);
        QVERIFY(run.fillAfterBlock[run.fillAfterBlock.size() - 20] > 150 * 48);
    }
    // Held until 4 s: nothing shed before, the excess shed after.
    {
        RemoteMicFeed feed;
        feed.setInUse(true);
        constexpr qint64 kReleaseBlock = 3000;   // 4 s
        quint64 shedAtRelease = 0;
        const FeedRun run = runFeed(feed, 750, arrival,
            [&shedAtRelease](qint64 block, const RemoteMicFeed& f) {
                if (block == kReleaseBlock) {
                    shedAtRelease = f.stats().shedFrames;
                }
            },
            [](qint64 block) { return block < kReleaseBlock; });
        QCOMPARE(shedAtRelease, quint64(0));
        QVERIFY(run.stats.shedFrames >= 150 * 48);
        const std::vector<std::pair<qint64, qint64>> words = playedWords(run.played);
        for (size_t w = 1; w + 1 < words.size(); ++w) {
            const qint64 length = words[w].second - words[w].first + 1;
            QVERIFY2(std::abs(length - kWordFrames) <= 24,
                     qPrintable(QStringLiteral("word %1: %2").arg(w).arg(length)));
        }
    }
}

// TX stall lane: a replay of the bench's stalled drains (Rock, 2026-10-01:
// the phone's packets reached the transport 20 ms apart, and the Core's
// event loop handed them over 80 to 410 ms late, in bursts). The network
// is steady and the audio is speech-like (words and pauses, so the buffer
// sheds in the pauses); five times the delivering thread stalls for 90 ms
// and then delivers what it held at once, each packet carrying how long it
// waited since its receipt. TX mic thread: the buffer times each packet at
// its receipt, so the steady link never grows the margin; a stall of the
// delivering thread runs it dry each time. On a real connection the line
// is delivered by the transport's own thread, so a stall of the event loop
// is not such a stall (tst_media_transport,
// aStalledOwnerLeavesTheMicrophoneLineWhole). Timed by the drain instead
// (fix round 1), the first stall grew the target to 119.3 ms and only it
// ran dry.
void TestRemoteMicReceiver::aHeldDeliveryIsTimedAtReceiptAndCountedAsOwnerWaits()
{
    FakeTime time;
    RemoteMicFeed feed;
    RemoteMicReceiver receiver(&feed, nullptr, time.clock(), time.scheduler());
    RemoteMicEncoder encoder;
    QVERIFY(encoder.isReady());
    QVERIFY(receiver.start(kMicSsrc, false));
    feed.setInUse(true);

    constexpr int kPackets = 400;   // 8 s
    constexpr int kFrames = RemoteMicConfig::kOpusFrameSamples;
    constexpr double kStallMs = 90.0;
    const std::vector<double> stallStarts{2000.0, 3000.0, 4000.0, 5000.0, 6000.0};
    const auto receivedMs = [](int k) { return 0.4 + 20.0 * k; };
    // When the owner hands packet k over: at once, or at the end of the
    // stall it arrived in.
    const auto deliveredMs = [&](int k) {
        const double at = receivedMs(k);
        for (const double start : stallStarts) {
            if (at >= start && at < start + kStallMs) {
                return start + kStallMs;
            }
        }
        return at;
    };
    std::vector<QByteArray> encoded;
    std::vector<float> frame(kFrames);
    for (int k = 0; k < kPackets; ++k) {
        for (int i = 0; i < kFrames; ++i) {
            frame[static_cast<size_t>(i)] = speechSample(static_cast<qint64>(k) * kFrames + i);
        }
        encoded.push_back(encoder.encode(frame.data(), static_cast<quint16>(k),
                                         static_cast<quint32>(k * kFrames), kMicSsrc));
    }

    std::vector<float> out(kBlock);
    int next = 0;
    int largestTarget = 0;
    int underrunsAtFirstStallEnd = -1;
    double longestHeldMs = 0.0;
    int heldOver50 = 0;
    const qint64 blocks = static_cast<qint64>(kPackets) * (kFrames / kBlock);
    for (qint64 b = 0; b < blocks; ++b) {
        const double nowMs = static_cast<double>(b) * kBlock * 1000.0 / 48000.0;
        time.advanceTo(static_cast<qint64>(nowMs));
        while (next < kPackets && deliveredMs(next) <= nowMs) {
            const double heldMs = nowMs - receivedMs(next);
            longestHeldMs = std::max(longestHeldMs, heldMs);
            heldOver50 += heldMs > RemoteMicConfig::kLongOwnerWaitMs ? 1 : 0;
            receiver.submit(encoded[static_cast<size_t>(next)],
                            static_cast<qint64>(std::llround(heldMs * 1000.0)));
            ++next;
        }
        feed.pullBlock(out.data(), kBlock, -1.0);
        if (nowMs >= stallStarts.front()) {
            largestTarget = std::max(largestTarget, feed.stats().targetFrames);
        }
        if (underrunsAtFirstStallEnd < 0 && nowMs >= stallStarts.front() + 500.0) {
            underrunsAtFirstStallEnd = feed.stats().underflows;
        }
    }
    const RemoteMicFeed::Stats stats = feed.stats();
    qInfo().noquote() << QStringLiteral("stalled drains: target at most %1 ms over five 90 ms "
                                        "stalls; grew %2 times, underruns %3 (%4 by the end of "
                                        "the first); owner waits mean %5 ms, max %6 ms, %7 over "
                                        "50 ms")
                             .arg(largestTarget / 48.0, 0, 'f', 1)
                             .arg(stats.grows)
                             .arg(stats.underflows)
                             .arg(underrunsAtFirstStallEnd)
                             .arg(stats.ownerWaitMeanMs, 0, 'f', 2)
                             .arg(stats.ownerWaitMaxMs, 0, 'f', 2)
                             .arg(stats.ownerWaitsLong);
    QCOMPARE(next, kPackets);
    QCOMPARE(receiver.stats().decodedPackets, quint64(kPackets));
    // Timed at receipt, the steady link never grows the margin, and each
    // of the five stalls runs the buffer dry once.
    QCOMPARE(stats.grows, 0);
    QCOMPARE(largestTarget, RemoteMicConfig::kTargetDepthFrames);
    QCOMPARE(stats.targetFrames, RemoteMicConfig::kTargetDepthFrames);
    QCOMPARE(stats.underflows, 5);
    QCOMPARE(underrunsAtFirstStallEnd, 1);
    // The waits: every packet's, the longest about the stall, and the
    // three in each stall that waited past 50 ms.
    QCOMPARE(heldOver50, 15);
    QCOMPARE(stats.ownerWaitsLong, heldOver50);
    QVERIFY2(std::abs(stats.ownerWaitMaxMs - longestHeldMs) < 0.05,
             qPrintable(QStringLiteral("%1 vs %2").arg(stats.ownerWaitMaxMs).arg(longestHeldMs)));
    QVERIFY(longestHeldMs > kStallMs - 1.0 && longestHeldMs < kStallMs + 2.0);
    QVERIFY(stats.ownerWaitMeanMs > 0.0 && stats.ownerWaitMeanMs < 5.0);
}

// TX mic thread: the line's packets are decoded and written on a thread of
// their own (the transport's "NereusMicRx"), while the owner thread starts
// and ends overs on the feed, watches the line and reads its figures, and
// the pump pulls blocks. Two seconds of that, with no lock on the pump's
// side; then a steady over plays the line's audio, and the starvation
// check the line's thread hands to the owner has run there.
void TestRemoteMicReceiver::theLineIsDecodedOnAThreadOfItsOwn()
{
    using Clock = std::chrono::steady_clock;
    constexpr int kFrames = RemoteMicConfig::kOpusFrameSamples;
    constexpr int kPackets = 300;   // 6 s
    std::vector<QByteArray> encoded;
    {
        RemoteMicEncoder encoder;
        QVERIFY(encoder.isReady());
        std::vector<float> frame(kFrames);
        for (int k = 0; k < kPackets; ++k) {
            for (int i = 0; i < kFrames; ++i) {
                frame[static_cast<size_t>(i)] = 0.3f * static_cast<float>(std::sin(
                    2.0 * std::numbers::pi * 1000.0 * static_cast<double>(k * kFrames + i)
                    / 48000.0));
            }
            encoded.push_back(encoder.encode(frame.data(), static_cast<quint16>(k),
                                             static_cast<quint32>(k * kFrames), kMicSsrc));
        }
    }
    RemoteMicFeed feed;
    RemoteMicReceiver receiver(&feed);
    QVERIFY(receiver.start(kMicSsrc, false));
    QSignalSpy starved(&receiver, &RemoteMicReceiver::starved);

    std::atomic<bool> stop{false};
    std::atomic<int> submitted{0};
    // The line's thread: a packet every 20 ms, each reporting a wait.
    std::thread line([&]() {
        const Clock::time_point start = Clock::now();
        for (qint64 k = 0; k < kPackets && !stop.load(); ++k) {
            std::this_thread::sleep_until(start + std::chrono::milliseconds(20 * k));
            receiver.submit(encoded[static_cast<size_t>(k)], (k % 7) * 300);
            submitted.fetch_add(1);
        }
    });
    // The pump: a block every 4/3 ms.
    std::atomic<bool> loudAfterSteady{false};
    std::atomic<bool> steady{false};
    std::thread pump([&]() {
        std::vector<float> out(kBlock);
        const Clock::time_point start = Clock::now();
        for (qint64 b = 0; !stop.load(); ++b) {
            std::this_thread::sleep_until(
                start + std::chrono::microseconds(b * kBlock * 1000000 / 48000));
            const bool steadyNow = steady.load();
            if (feed.pullBlock(out.data(), kBlock, -1.0) == RemoteMicFeed::Pull::Audio
                && steadyNow) {
                for (const float sample : out) {
                    if (std::abs(sample) > 0.1f) {
                        loudAfterSteady.store(true);
                    }
                }
            }
        }
    });

    // Both threads end with the test, however it ends.
    struct Join {
        std::atomic<bool>& stop;
        std::thread& line;
        std::thread& pump;
        ~Join()
        {
            stop.store(true);
            for (std::thread* thread : {&line, &pump}) {
                if (thread->joinable()) {
                    thread->join();
                }
            }
        }
    } join{stop, line, pump};

    // The owner: overs of 20 to 110 ms, the line watched in each.
    QElapsedTimer churn;
    churn.start();
    int overs = 0;
    while (churn.elapsed() < 2000) {
        feed.setInUse(true);
        receiver.setWatching(true);
        QTest::qWait(20 + (overs % 10) * 10);
        const RemoteMicReceiver::Stats stats = receiver.stats();
        // At most the one being decoded is counted ahead of its submit's end.
        QVERIFY(stats.decodedPackets <= static_cast<quint64>(submitted.load()) + 1);
        QVERIFY(feed.stats().fillFrames >= 0);
        receiver.setWatching(false);
        feed.setInUse(false);
        QTest::qWait(1);
        ++overs;
    }
    // A steady over: the line's audio plays.
    feed.setInUse(true);
    receiver.setWatching(true);
    QTest::qWait(150);
    steady.store(true);
    QTRY_VERIFY_WITH_TIMEOUT(loudAfterSteady.load(), 2000);
    const int startsBefore = static_cast<int>(starved.count());
    stop.store(true);
    line.join();
    pump.join();
    QVERIFY(submitted.load() < kPackets);
    // The line went quiet: the starvation the line's thread scheduled runs
    // on the owner thread and reports it.
    QTRY_VERIFY_WITH_TIMEOUT(starved.count() > startsBefore, 2000);
    QCOMPARE(starved.last().at(0).toBool(), true);
    receiver.setWatching(false);
    feed.setInUse(false);
    QVERIFY(overs > 10);
    QVERIFY(receiver.stats().decodedPackets > 0);
    receiver.stop();
}

// TX diagnostics lane: the phone's audio stops arriving for half a second
// mid-key with no sequence gap (a capture or send pause): every packet
// after it comes 500 ms late against its RTP timestamp. The buffer runs dry
// once, and the feed places that underrun: about one target after the
// pause began, silent until the buffer refills after the pause, after a
// 520 ms gap between receipts, with packets 500 ms behind their timestamps.
void TestRemoteMicReceiver::aMidKeyArrivalPauseIsPlacedInTheUnderrunFigures()
{
    FakeTime time;
    RemoteMicFeed feed;
    RemoteMicReceiver receiver(&feed, nullptr, time.clock(), time.scheduler());
    RemoteMicEncoder encoder;
    QVERIFY(encoder.isReady());
    QVERIFY(receiver.start(kMicSsrc, false));
    feed.setInUse(true);

    constexpr int kPackets = 250;   // 5 s
    constexpr int kFrames = RemoteMicConfig::kOpusFrameSamples;
    constexpr int kPauseAt = 100;   // packets from here on come late
    constexpr double kPauseMs = 500.0;
    const auto receivedMs = [](int k) {
        return 0.4 + 20.0 * k + (k >= kPauseAt ? kPauseMs : 0.0);
    };
    std::vector<QByteArray> encoded;
    std::vector<float> frame(kFrames);
    for (int k = 0; k < kPackets; ++k) {
        for (int i = 0; i < kFrames; ++i) {
            frame[static_cast<size_t>(i)] = speechSample(static_cast<qint64>(k) * kFrames + i);
        }
        encoded.push_back(encoder.encode(frame.data(), static_cast<quint16>(k),
                                         static_cast<quint32>(k * kFrames), kMicSsrc));
    }

    std::vector<float> out(kBlock);
    int next = 0;
    int placedBeforePause = -1;
    // Up to 10 ms past the last packet's receipt, so the over ends with the
    // buffer still playing.
    const qint64 blocks =
        static_cast<qint64>((receivedMs(kPackets - 1) + 10.0) * 48.0 / kBlock);
    for (qint64 b = 0; b < blocks; ++b) {
        const double nowMs = static_cast<double>(b) * kBlock * 1000.0 / 48000.0;
        time.advanceTo(static_cast<qint64>(nowMs));
        while (next < kPackets && receivedMs(next) <= nowMs) {
            // Handed over at once on the line's thread; the receiver times
            // the packet at its receipt.
            receiver.submit(encoded[static_cast<size_t>(next)],
                            static_cast<qint64>(std::llround((nowMs - receivedMs(next)) * 1000.0)));
            ++next;
        }
        feed.pullBlock(out.data(), kBlock, -1.0);
        if (placedBeforePause < 0 && nowMs >= 20.0 * kPauseAt) {
            placedBeforePause = feed.stats().underrunsPlacedCount;
        }
    }
    QCOMPARE(next, kPackets);
    QCOMPARE(placedBeforePause, 0);

    const RemoteMicFeed::Stats stats = feed.stats();
    QCOMPARE(stats.underflows, 1);
    QCOMPARE(stats.underrunsPlacedCount, 1);
    const RemoteMicFeed::Stats::Underrun& event = stats.underrunsPlaced[0];
    const double pauseStartMs = 20.0 * kPauseAt;
    qInfo().noquote() << QStringLiteral("placed underrun: at +%1 ms of the line, silent %2 ms, "
                                        "largest gap %3 ms, latest %4 ms")
                             .arg(event.atLineMs, 0, 'f', 1)
                             .arg(event.silentMs, 0, 'f', 1)
                             .arg(event.arrivalGapMs, 0, 'f', 1)
                             .arg(event.lateMs, 0, 'f', 1);
    // It ran dry about one target after the last on-time packet.
    QVERIFY2(event.atLineMs >= pauseStartMs
                 && event.atLineMs <= pauseStartMs + RemoteMicConfig::kTargetDepthMs + 5.0,
             qPrintable(QString::number(event.atLineMs)));
    QVERIFY(event.atSteadyUs > 0);
    // Silent until the late packets refilled it past the target: the pause
    // less the target the buffer held, give or take a packet.
    QVERIFY2(event.silentMs >= kPauseMs - RemoteMicConfig::kTargetDepthMs - 20.0
                 && event.silentMs <= kPauseMs + 20.0,
             qPrintable(QString::number(event.silentMs)));
    // The gap: one packet's 20 ms plus the pause. The lateness: the pause.
    // (The injected clock reads whole ms.)
    QVERIFY2(std::abs(event.arrivalGapMs - (20.0 + kPauseMs)) <= 1.5,
             qPrintable(QString::number(event.arrivalGapMs)));
    QVERIFY2(std::abs(event.lateMs - kPauseMs) <= 1.5,
             qPrintable(QString::number(event.lateMs)));

    // The figures stay readable after the over and start again with the
    // next one.
    feed.setInUse(false);
    feed.pullBlock(out.data(), kBlock, -1.0);
    QCOMPARE(feed.stats().underrunsPlacedCount, 1);
    feed.setInUse(true);
    feed.pullBlock(out.data(), kBlock, -1.0);
    QCOMPARE(feed.stats().underrunsPlacedCount, 0);
}

QTEST_GUILESS_MAIN(TestRemoteMicReceiver)
#include "tst_remote_mic_receiver.moc"
