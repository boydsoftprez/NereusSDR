// =================================================================
// tests/tst_tx_mic_latency.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test. It cites WDSP rmatch only for the
// filter delay it reports; no upstream logic is ported here.
//
// R-IOS-13, R-R3-42 (2026-09-27): the transmit path adds no latency it does
// not need. The remote microphone's path from a packet arriving at the Core
// to the I/Q leaving for the radio, on simulated time: the phone's 20 ms
// packets into RemoteMicFeed, the transmit pump taking a 64-frame block for
// each of the radio's microphone packets (TX DSP stood in for by a 4x hold
// to 192 kHz, measured separately; see the report), and the Protocol 2 send
// path's ring and pacer (P2RadioConnection's test seams: one pass of its
// send thread a millisecond, frames captured, nothing sent).
//
// It measures the added latency stage by stage on a steady link, and after
// 200 ms stalls of the link, of the connection thread (the pump) and of the
// send thread, each starting in a pause of speech-like audio. After every
// stall the added latency comes back to the steady figure within two
// seconds, every word leaves for the radio whole, and no sample on the wire
// steps as a splice would: whatever is shed is silence, before TX DSP.
//
// Nothing here opens an audio device, a socket or a radio.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-27: original test for NereusSDR by J.J. Boyd (KG4VCF),
//               R-IOS-13 and R-R3-42, with AI-assisted implementation via
//               Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include "core/P2RadioConnection.h"
#include "core/session/media/RemoteAudioRateMatcher.h"
#include "core/session/media/RemoteMicReceiver.h"

#include <opus.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

using namespace NereusSDR;

namespace {

constexpr qint64 kMs = 1'000'000;
constexpr int kBlock = RemoteMicConfig::kPumpBlockFrames;
constexpr int kPacket = RemoteMicConfig::kOpusFrameSamples;
constexpr int kUp = 4;   // 48 kHz microphone to 192 kHz I/Q

// Speech-like microphone audio: a 300 ms word (a 1 kHz tone at 0.3) and a
// 200 ms pause (a -70 dBFS noise floor), repeating.
constexpr qint64 kWordFrames = 14'400;
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

qint64 micPacketNs(qint64 k) { return k * 4'000'000 / 3; }   // the radio's, 1.333 ms each

struct Window {
    qint64 fromNs{0};
    qint64 toNs{0};
    bool inside(qint64 t) const { return t >= fromNs && t < toNs; }
};

// One over, 12 s: the phone's packets, the pump and the send thread on
// their own clocks.
struct Chain {
    P2RadioConnection conn;
    P2RadioConnection::TxIqCapture cap;
    RemoteMicFeed feed;
    Window linkStall;        // packets due inside arrive at its end
    Window pumpStall;        // the radio's mic packets wait, then burst
    Window senderStall;      // no send passes
    int nextPacket{0};
    qint64 nextMic{0};
    std::vector<float> block = std::vector<float>(kBlock);
    std::vector<float> iq = std::vector<float>(kBlock * kUp * 2);
    // Per pump block: the time, the added latency (the feed's fill plus
    // the ring) and the ring alone, in ms.
    std::vector<qint64> tickNs;
    std::vector<double> addedMs;
    std::vector<double> ringMs;
    std::vector<double> leadMs;   // the radio's estimated lead, per send pass
    int shedTicks{0};

    double packetArrivalMs(int k) const
    {
        const double onTime = 0.4 + 20.0 * k;
        const qint64 ns = static_cast<qint64>(onTime * kMs);
        return linkStall.inside(ns) ? static_cast<double>(linkStall.toNs) / kMs : onTime;
    }

    void pumpBlock(qint64 t)
    {
        const double ring = conn.txIqQueuedMs();
        const RemoteMicFeed::Pull pull = feed.pullBlock(block.data(), kBlock, ring);
        tickNs.push_back(t);
        addedMs.push_back(static_cast<double>(feed.stats().fillFrames) / 48.0 + ring);
        ringMs.push_back(ring);
        if (pull == RemoteMicFeed::Pull::Shed) {
            ++shedTicks;
            return;   // the pump skips the block: no TX DSP, no I/Q
        }
        // TX DSP's stand-in: each microphone sample held for 4 I/Q samples.
        for (int i = 0; i < kBlock; ++i) {
            for (int u = 0; u < kUp; ++u) {
                iq[static_cast<size_t>(2 * (i * kUp + u))] = block[static_cast<size_t>(i)];
                iq[static_cast<size_t>(2 * (i * kUp + u) + 1)] = 0.0f;
            }
        }
        conn.sendTxIq(iq.data(), kBlock * kUp);
    }

    void run(qint64 toNs)
    {
        conn.setMox(true);
        feed.setInUse(true);
        std::vector<float> packet(kPacket);
        for (qint64 t = 0; t < toNs; t += kMs / 4) {
            while (packetArrivalMs(nextPacket) * kMs <= t) {
                for (int i = 0; i < kPacket; ++i) {
                    packet[static_cast<size_t>(i)] =
                        speechSample(static_cast<qint64>(nextPacket) * kPacket + i);
                }
                feed.write(packet.data(), kPacket);
                ++nextPacket;
            }
            while (!pumpStall.inside(t) && micPacketNs(nextMic) <= t) {
                pumpBlock(t);
                ++nextMic;
            }
            if (!senderStall.inside(t) && t % kMs == 0) {
                conn.serviceTxIqSendForTest(t, &cap);
                leadMs.push_back(conn.txIqRadioLeadForTest() / 192.0);
            }
        }
    }

    // The I channel on the wire, in order.
    std::vector<float> wireI() const
    {
        std::vector<float> out;
        for (const QByteArray& f : cap.frames) {
            for (int s = 0; s < 240; ++s) {
                const int o = 4 + s * 6;
                const qint32 v = (qint32(quint8(f[o])) << 24 | qint32(quint8(f[o + 1])) << 16
                                  | qint32(quint8(f[o + 2])) << 8) >> 8;
                out.push_back(static_cast<float>(v) / 8388607.0f);
            }
        }
        return out;
    }
};

double meanOf(const std::vector<double>& v, size_t from, size_t to)
{
    double sum = 0.0;
    for (size_t i = from; i < to; ++i) {
        sum += v[i];
    }
    return to > from ? sum / static_cast<double>(to - from) : 0.0;
}

double maxOf(const std::vector<double>& v, size_t from, size_t to)
{
    double m = 0.0;
    for (size_t i = from; i < to; ++i) {
        m = std::max(m, v[i]);
    }
    return m;
}

// The words on the wire: [first loud sample, last loud sample] of each run
// of |I| > 0.05 whose gaps are shorter than 2 ms at 192 kHz.
std::vector<std::pair<qint64, qint64>> wireWords(const std::vector<float>& wire)
{
    std::vector<std::pair<qint64, qint64>> words;
    qint64 last = -1'000'000;
    for (qint64 i = 0; i < static_cast<qint64>(wire.size()); ++i) {
        if (std::abs(wire[static_cast<size_t>(i)]) <= 0.05f) {
            continue;
        }
        if (i - last > 384) {
            words.push_back({i, i});
        }
        words.back().second = i;
        last = i;
    }
    return words;
}

double largestStep(const std::vector<float>& wire)
{
    double largest = 0.0;
    for (size_t i = 1; i < wire.size(); ++i) {
        largest = std::max(largest, static_cast<double>(std::abs(wire[i] - wire[i - 1])));
    }
    return largest;
}

// Steady figures of a run without a stall, over 2..12 s.
struct Steady {
    double feedMeanMs{0.0};
    double feedMaxMs{0.0};
    double ringMeanMs{0.0};
    double ringMaxMs{0.0};
    double addedMaxMs{0.0};
    double leadMeanMs{0.0};
    double ringWindowFloorMaxMs{0.0};
};

Steady steadyFigures()
{
    Chain c;
    c.run(12'000 * kMs);
    const size_t from = 1500;
    const size_t to = c.addedMs.size();
    std::vector<double> feed(c.addedMs.size());
    for (size_t i = 0; i < feed.size(); ++i) {
        feed[i] = c.addedMs[i] - c.ringMs[i];
    }
    Steady s;
    s.feedMeanMs = meanOf(feed, from, to);
    s.feedMaxMs = maxOf(feed, from, to);
    s.ringMeanMs = meanOf(c.ringMs, from, to);
    s.ringMaxMs = maxOf(c.ringMs, from, to);
    s.addedMaxMs = maxOf(c.addedMs, from, to);
    s.leadMeanMs = meanOf(c.leadMs, 2000, c.leadMs.size());
    // The ring's lowest fill over each 250 ms window (188 pump blocks),
    // and the highest of those: what shedding compares with its slack.
    for (size_t w = from; w + 188 <= to; w += 188) {
        double low = 1e9;
        for (size_t i = w; i < w + 188; ++i) {
            low = std::min(low, c.ringMs[i]);
        }
        s.ringWindowFloorMaxMs = std::max(s.ringWindowFloorMaxMs, low);
    }
    return s;
}

} // namespace

class TestTxMicLatency : public QObject {
    Q_OBJECT

private slots:
    void steadyLinkAddsOnlyWhatItNeeds();
    void opusRoundTripDelay();
    void aStallReturnsToTheSteadyLatencyWithoutASplice_data();
    void aStallReturnsToTheSteadyLatencyWithoutASplice();
};

// Steady 20 ms packets: the feed holds its smallest target (30 ms after a
// packet lands, 10 ms before the next), the send ring holds under a frame
// and a block, and the radio's buffer sits at its 15 ms target lead.
void TestTxMicLatency::steadyLinkAddsOnlyWhatItNeeds()
{
    const Steady s = steadyFigures();
    qInfo().noquote()
        << QStringLiteral("steady link, per stage: feed %1 ms mean (%2 max), rmatch filter %3 ms, "
                          "pump block %4 ms, send ring %5 ms mean (%6 max), radio lead %7 ms; "
                          "added (feed + ring) max %8 ms; ring's window floors at most %9 ms")
               .arg(s.feedMeanMs, 0, 'f', 1).arg(s.feedMaxMs, 0, 'f', 1)
               .arg(RemoteAudioRateMatcher::kFilterDelayFrames / 48.0, 0, 'f', 2)
               .arg(kBlock / 48.0, 0, 'f', 2)
               .arg(s.ringMeanMs, 0, 'f', 2).arg(s.ringMaxMs, 0, 'f', 2)
               .arg(s.leadMeanMs, 0, 'f', 1).arg(s.addedMaxMs, 0, 'f', 1)
               .arg(s.ringWindowFloorMaxMs, 0, 'f', 2);
    QVERIFY2(s.feedMeanMs < 25.0, qPrintable(QString::number(s.feedMeanMs)));
    QVERIFY2(s.feedMaxMs <= RemoteMicConfig::kTargetDepthMs + RemoteMicConfig::kReserveMs + 1.5,
             qPrintable(QString::number(s.feedMaxMs)));
    // The ring is headroom, not delay: under a 240-sample frame plus a
    // 256-sample block at 192 kHz (and a block more when two pump blocks
    // fall between send passes); its lowest over a window stays under the
    // slack shedding leaves alone.
    QVERIFY2(s.ringMeanMs < 3.0, qPrintable(QString::number(s.ringMeanMs)));
    QVERIFY2(s.ringMaxMs < 4.0, qPrintable(QString::number(s.ringMaxMs)));
    QVERIFY2(s.ringWindowFloorMaxMs < RemoteMicConfig::kRingSlackMs,
             qPrintable(QString::number(s.ringWindowFloorMaxMs)));
    QVERIFY2(std::abs(s.leadMeanMs - 15.0) < 1.5, qPrintable(QString::number(s.leadMeanMs)));
}

// The codec's own delay (the phone's Opus encoder and the Core's decoder):
// a 1 kHz tone switched on, measured where its envelope reaches half.
void TestTxMicLatency::opusRoundTripDelay()
{
    RemoteMicEncoder encoder;
    QVERIFY(encoder.isReady());
    int error = OPUS_OK;
    OpusDecoder* decoder = opus_decoder_create(48000, 1, &error);
    QVERIFY(decoder != nullptr && error == OPUS_OK);
    constexpr qint64 kOnAt = 48000;   // after 1 s of silence
    std::vector<float> in(kPacket);
    std::vector<float> out(kPacket * 6);
    std::vector<float> decoded;
    for (int k = 0; k < 150; ++k) {
        for (int i = 0; i < kPacket; ++i) {
            const qint64 n = static_cast<qint64>(k) * kPacket + i;
            in[static_cast<size_t>(i)] = n < kOnAt ? 0.0f
                : 0.3f * static_cast<float>(std::sin(2.0 * std::numbers::pi * 1000.0 * n / 48000.0));
        }
        const QByteArray rtp = encoder.encode(in.data(), static_cast<quint16>(k),
                                              static_cast<quint32>(k * kPacket), 1U);
        QVERIFY(rtp.size() > 12);
        const auto* payload = reinterpret_cast<const unsigned char*>(rtp.constData()) + 12;
        const int frames = opus_decode_float(decoder, payload, rtp.size() - 12, out.data(),
                                             static_cast<int>(out.size()), 0);
        QVERIFY(frames == kPacket);
        decoded.insert(decoded.end(), out.begin(), out.begin() + frames);
    }
    opus_decoder_destroy(decoder);
    // Envelope: the largest |x| over each 1 ms; the onset where it first
    // reaches half the tone's peak.
    qint64 onset = -1;
    for (qint64 n = 0; n + 48 <= static_cast<qint64>(decoded.size()); ++n) {
        if (std::abs(decoded[static_cast<size_t>(n)]) >= 0.15f) {
            onset = n;
            break;
        }
    }
    QVERIFY(onset > kOnAt);
    // The input tone first reaches 0.15 within its first quarter period.
    qint64 inOnset = kOnAt;
    while (0.3 * std::abs(std::sin(2.0 * std::numbers::pi * 1000.0 * inOnset / 48000.0)) < 0.15) {
        ++inOnset;
    }
    const double delayMs = static_cast<double>(onset - inOnset) / 48.0;
    qInfo().noquote() << QStringLiteral("Opus 24 kbit/s round trip (encoder and decoder): %1 ms")
                             .arg(delayMs, 0, 'f', 2);
    QVERIFY(delayMs > 0.0 && delayMs < 10.0);
}

void TestTxMicLatency::aStallReturnsToTheSteadyLatencyWithoutASplice_data()
{
    QTest::addColumn<int>("which");
    QTest::newRow("link stalls 200 ms") << 0;
    QTest::newRow("connection thread (pump) stalls 200 ms") << 1;
    QTest::newRow("send thread stalls 200 ms") << 2;
}

// A 200 ms stall starting 100 ms before a pause ends (the pause at
// 3300..3500 ms of the seventh cycle). The added latency comes back to the
// steady figure within two seconds and stays; every word reaches the wire
// whole and nothing on the wire steps as a splice would.
void TestTxMicLatency::aStallReturnsToTheSteadyLatencyWithoutASplice()
{
    QFETCH(int, which);
    const Steady steady = steadyFigures();
    Chain c;
    const Window stall{3400 * kMs, 3600 * kMs};
    if (which == 0) {
        c.linkStall = stall;
    } else if (which == 1) {
        c.pumpStall = stall;
    } else {
        c.senderStall = stall;
    }
    c.run(12'000 * kMs);

    // Back: the first pump block after the stall from which the added
    // latency never again passes the steady maximum by more than the
    // shedding's granularity (one 1.33 ms block past the ring's slack).
    const double limit = steady.addedMaxMs + 2.0;
    size_t back = c.addedMs.size();
    while (back > 0 && c.tickNs[back - 1] >= stall.toNs && c.addedMs[back - 1] <= limit) {
        --back;
    }
    const double backMs = back < c.addedMs.size()
        ? static_cast<double>(c.tickNs[back] - stall.toNs) / kMs
        : -1.0;
    size_t peakFrom = 0;
    while (peakFrom < c.tickNs.size() && c.tickNs[peakFrom] < stall.fromNs) {
        ++peakFrom;
    }
    const RemoteMicFeed::Stats st = c.feed.stats();
    qInfo().noquote()
        << QStringLiteral("%1: added latency peaked at %2 ms (ring %3 ms), back to the steady %4 ms "
                          "%5 ms after the stall; shed %6 ms (%7 ms for the ring), inserted %8 ms; "
                          "underruns %9; over mean %10 ms, max %11 ms")
               .arg(QString::fromLatin1(QTest::currentDataTag()))
               .arg(maxOf(c.addedMs, peakFrom, c.addedMs.size()), 0, 'f', 1)
               .arg(maxOf(c.ringMs, peakFrom, c.ringMs.size()), 0, 'f', 1)
               .arg(steady.addedMaxMs, 0, 'f', 1)
               .arg(backMs, 0, 'f', 0)
               .arg((st.shedFrames + st.shedForRingFrames) / 48.0, 0, 'f', 1)
               .arg(st.shedForRingFrames / 48.0, 0, 'f', 1)
               .arg(st.insertedFrames / 48.0, 0, 'f', 1)
               .arg(st.underflows)
               .arg(st.addedMeanMs, 0, 'f', 1).arg(st.addedMaxMs, 0, 'f', 1);
    QVERIFY2(backMs >= 0.0 && backMs <= 2000.0, qPrintable(QString::number(backMs)));
    QVERIFY2(c.tickNs.back() - c.tickNs[back] >= 5000 * kMs, "did not stay back");
    if (which != 0) {
        QVERIFY2(st.shedForRingFrames >= 150 * 48,
                 qPrintable(QString::number(st.shedForRingFrames)));
        QCOMPARE(c.shedTicks * kBlock, static_cast<int>(st.shedForRingFrames));
    }
    QCOMPARE(c.conn.txSendStats().overflowSamples, quint64(0));

    // The wire: every word whole (300 ms at 192 kHz, within the ratio's
    // and the filter's few samples), and no step a splice would make.
    const std::vector<float> wire = c.wireI();
    const std::vector<std::pair<qint64, qint64>> words = wireWords(wire);
    QVERIFY2(words.size() >= 20, qPrintable(QString::number(words.size())));
    for (size_t w = 1; w + 1 < words.size(); ++w) {
        const qint64 length = words[w].second - words[w].first + 1;
        QVERIFY2(std::abs(length - kWordFrames * kUp) <= 24 * kUp,
                 qPrintable(QStringLiteral("word %1: %2").arg(w).arg(length)));
    }
    QVERIFY2(largestStep(wire) < 0.045, qPrintable(QString::number(largestStep(wire))));
}

QTEST_GUILESS_MAIN(TestTxMicLatency)
#include "tst_tx_mic_latency.moc"
