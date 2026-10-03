// no-port-check: NereusSDR-original test.
// =================================================================
// tests/tst_tci_rx_audio_lane.cpp  (NereusSDR)
// =================================================================
// R-R3-39: TCI receive audio's WDSP resampler (create_resampleFV,
// xresampleFV, destroy_resampleFV) runs on the receive lane, never on the
// event loop. With TCI clients subscribed to receive audio at 12 kHz stereo,
// 24 kHz mono and 48 kHz, through a rate change, an audio_stop, a
// disconnect and the server's stop, the event loop makes no WDSP call.
//
// Each client's stream is unchanged: a server with no lane (the old,
// at-once path) is fed the same audio, and every block it sends matches
// the lane server's block for block (rate, sample type, channels, length
// and every sample), in the same order.
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original test for NereusSDR by J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code
//               (R-R3-39).
// =================================================================

#ifdef HAVE_WEBSOCKETS

#include <QtTest>
#include <QSignalSpy>
#include <QUrl>
#include <QWebSocket>

#include <array>
#include <cmath>
#include <cstring>
#include <memory>
#include <vector>

#include "core/TciServer.h"
#include "core/WdspThreadCheck.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

namespace {

constexpr int kLaneIdleTimeoutMs = 5000;

struct Block {
    quint32 receiver = 0;
    quint32 rate = 0;
    quint32 sampleType = 0;
    quint32 length = 0;
    quint32 channels = 0;
    QByteArray payload;
};

quint32 readLe32(const QByteArray& frame, int offset)
{
    const auto* p = reinterpret_cast<const quint8*>(frame.constData() + offset);
    return static_cast<quint32>(p[0]) | (static_cast<quint32>(p[1]) << 8)
         | (static_cast<quint32>(p[2]) << 16) | (static_cast<quint32>(p[3]) << 24);
}

// RX audio frames (stream type 1) only.
QList<Block> rxAudio(const QSignalSpy& spy)
{
    QList<Block> blocks;
    for (const QList<QVariant>& call : spy) {
        const QByteArray frame = call.at(0).toByteArray();
        if (frame.size() < 64 || readLe32(frame, 24) != 1u) {
            continue;
        }
        Block b;
        b.receiver = readLe32(frame, 0);
        b.rate = readLe32(frame, 4);
        b.sampleType = readLe32(frame, 8);
        b.length = readLe32(frame, 20);
        b.channels = readLe32(frame, 28);
        b.payload = frame.mid(64);
        blocks.append(b);
    }
    return blocks;
}

QVector<float> samplesOf(const QList<Block>& blocks)
{
    QVector<float> out;
    for (const Block& b : blocks) {
        const int n = b.payload.size() / 4;
        const int at = out.size();
        out.resize(at + n);
        std::memcpy(out.data() + at, b.payload.constData(), static_cast<size_t>(n) * 4);
    }
    return out;
}

double toneAmplitude(const QVector<float>& samples, int channels, int channel,
                     double hz, int rateHz, int firstFrame)
{
    double cosine = 0.0;
    double sine = 0.0;
    int frames = 0;
    for (int f = firstFrame; f * channels + channel < samples.size(); ++f) {
        const double phase = 2.0 * M_PI * hz * double(f) / double(rateHz);
        const double v = samples.at(f * channels + channel);
        cosine += v * std::cos(phase);
        sine += v * std::sin(phase);
        ++frames;
    }
    return frames > 0 ? 2.0 * std::hypot(cosine, sine) / frames : 0.0;
}

bool connectClient(QWebSocket& client, quint16 port)
{
    QSignalSpy connected(&client, &QWebSocket::connected);
    client.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(port)));
    return connected.wait(2000) && client.state() == QAbstractSocket::ConnectedState;
}

// One app on a server: its socket and every binary frame it received.
struct App {
    std::unique_ptr<QWebSocket> socket = std::make_unique<QWebSocket>();
    std::unique_ptr<QSignalSpy> binary;
};

bool openApp(App& app, TciServer& server, const QStringList& setup)
{
    app.binary = std::make_unique<QSignalSpy>(app.socket.get(),
                                              &QWebSocket::binaryMessageReceived);
    if (!connectClient(*app.socket, server.port())) {
        return false;
    }
    for (const QString& line : setup) {
        app.socket->sendTextMessage(line);
    }
    return true;
}

// Feeds `frames` frames of a 440 Hz left and 1000 Hz right tone, starting
// at frame `from`.
void feed(TciServer& server, int from, int frames)
{
    constexpr int kChunk = 1024;
    std::vector<float> left(kChunk), right(kChunk);
    for (int sent = 0; sent < frames; sent += kChunk) {
        for (int i = 0; i < kChunk; ++i) {
            const double t = double(from + sent + i) / 48000.0;
            left[size_t(i)] = float(0.3 * std::sin(2.0 * M_PI * 440.0 * t));
            right[size_t(i)] = float(0.2 * std::sin(2.0 * M_PI * 1000.0 * t));
        }
        server.injectAudioFrameForTest(0, left.data(), right.data(), kChunk, 48000);
        QTest::qWait(10);
    }
}

const QStringList kStereo12k{QStringLiteral("audio_samplerate:12000;"),
                             QStringLiteral("audio_stream_channels:2;"),
                             QStringLiteral("audio_start:0;")};
const QStringList kMono24k{QStringLiteral("audio_samplerate:24000;"),
                           QStringLiteral("audio_stream_channels:1;"),
                           QStringLiteral("audio_start:0;")};
const QStringList kStereo48k{QStringLiteral("audio_start:0;")};

// Three apps (12 kHz stereo, 24 kHz mono, 48 kHz stereo) on `server`, fed
// one second of audio, then app B changes rate to 8 kHz and one more second
// follows. 48 x 1024 frames is exactly 24 blocks of 2048, so every block
// boundary is the same on every run.
bool runStreams(TciServer& server, RadioModel* model, App& a, App& b, App& c)
{
    if (!openApp(a, server, kStereo12k) || !openApp(b, server, kMono24k)
        || !openApp(c, server, kStereo48k)) {
        return false;
    }
    QTest::qWait(150);
    feed(server, 0, 48 * 1024);
    QTest::qWait(100);
    b.socket->sendTextMessage(QStringLiteral("audio_samplerate:8000;"));
    QTest::qWait(50);
    feed(server, 48 * 1024, 48 * 1024);
    QTest::qWait(300);
    if (model != nullptr && !model->waitForReceiveLaneForTest(kLaneIdleTimeoutMs)) {
        return false;
    }
    QTest::qWait(100);
    return true;
}

} // namespace

class TestTciRxAudioLane : public QObject {
    Q_OBJECT

private slots:
    void receiveAudioResamplesOnTheReceiveLane()
    {
        RadioModel model;
        QVERIFY2(model.receiveLane() != nullptr, "a local model has a receive lane");

        // The server under test, on the model's receive lane. The check
        // counts every WDSP call the event loop makes from here on.
        TciServer onLane(&model);
        QVERIFY(onLane.start(0));
        WdspThreadCheck::install(QThread::currentThread());

        App laneA, laneB, laneC;
        QVERIFY(runStreams(onLane, &model, laneA, laneB, laneC));
        QCOMPARE(onLane.totalResamplerInstances(), 3);

        // Teardown paths: audio_stop, a disconnect, then the server's stop.
        laneA.socket->sendTextMessage(QStringLiteral("audio_stop:0;"));
        QTest::qWait(50);
        QCOMPARE(onLane.totalResamplerInstances(), 2);
        laneB.socket->close();
        QTest::qWait(100);
        QCOMPARE(onLane.totalResamplerInstances(), 1);
        onLane.stop();
        QCOMPARE(onLane.totalResamplerInstances(), 0);
        QVERIFY(model.waitForReceiveLaneForTest(kLaneIdleTimeoutMs));

        const quint64 eventLoopCalls = WdspThreadCheck::eventLoopEntries();
        WdspThreadCheck::uninstall();
        QCOMPARE(TciServer::liveRxAudioResamplersForTest(), 0);

        // The same audio through a server with no lane (the at-once path
        // every stream took before): every block matches, in order.
        TciServer reference(nullptr);
        QVERIFY(reference.start(0));
        App refA, refB, refC;
        QVERIFY(runStreams(reference, nullptr, refA, refB, refC));

        const std::array<std::pair<App*, App*>, 3> pairs{
            std::pair{&laneA, &refA}, std::pair{&laneB, &refB}, std::pair{&laneC, &refC}};
        for (const auto& [lane, ref] : pairs) {
            const QList<Block> got = rxAudio(*lane->binary);
            const QList<Block> want = rxAudio(*ref->binary);
            QVERIFY2(got.size() >= 40, qPrintable(QString::number(got.size())));
            QCOMPARE(got.size(), want.size());
            for (int i = 0; i < got.size(); ++i) {
                QCOMPARE(got[i].receiver, want[i].receiver);
                QCOMPARE(got[i].rate, want[i].rate);
                QCOMPARE(got[i].sampleType, want[i].sampleType);
                QCOMPARE(got[i].channels, want[i].channels);
                QCOMPARE(got[i].length, want[i].length);
                QVERIFY2(got[i].payload == want[i].payload,
                         qPrintable(QStringLiteral("block %1 differs").arg(i)));
            }
        }

        // The 12 kHz stereo stream is the tone, left and right apart, and
        // continuous (a block out of order would break the phase).
        const QVector<float> twelve = samplesOf(rxAudio(*laneA.binary));
        QVERIFY(twelve.size() > 2 * 20000);
        const double leftLow = toneAmplitude(twelve, 2, 0, 440.0, 12000, 600);
        const double rightHigh = toneAmplitude(twelve, 2, 1, 1000.0, 12000, 600);
        QVERIFY2(std::abs(leftLow - 0.3) < 0.01, qPrintable(QString::number(leftLow)));
        QVERIFY2(std::abs(rightHigh - 0.2) < 0.01, qPrintable(QString::number(rightHigh)));
        // App B changed rate mid-stream: 24 kHz mono blocks, then 8 kHz.
        const QList<Block> bBlocks = rxAudio(*laneB.binary);
        QCOMPARE(bBlocks.first().rate, 24000u);
        QCOMPARE(bBlocks.last().rate, 8000u);
        QCOMPARE(bBlocks.last().channels, 1u);

        reference.stop();
        QCOMPARE(TciServer::liveRxAudioResamplersForTest(), 0);
        QCOMPARE(eventLoopCalls, quint64{0});
    }
};

QTEST_MAIN(TestTciRxAudioLane)
#include "tst_tci_rx_audio_lane.moc"

#else

#include <QtTest>
class TestTciRxAudioLane : public QObject {
    Q_OBJECT
private slots:
    void skipped() { QSKIP("built without Qt WebSockets"); }
};
QTEST_MAIN(TestTciRxAudioLane)
#include "tst_tci_rx_audio_lane.moc"

#endif
