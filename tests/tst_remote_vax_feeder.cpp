// =================================================================
// tests/tst_remote_vax_feeder.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test. VAX in a remote window; no
// upstream logic is ported here.
//
// R3 receiver audio plan, Task 5 (R-R3-44, R-R3-21, R-R3-23).
//
// The VAX feed contract:
//   - slices sharing a VAX channel are mixed, as the local VAX tee mixes
//     them; a slice whose stream stops, or goes quiet, counts as silence
//     and the others play on (fix wave);
//   - a feeder keeps a VAX output fed through a 10-minute run of the
//     harness clock with the output's clock 200 ppm off the Core's, either
//     way, without the delay growing and without a gap or a restart;
//   - an output nothing reads is not a fault: the feeder stops writing and
//     plays again when the output moves; a quiet Core is waited for;
//   - blocks arrive on the receive worker and are only copied there;
//   - the VAX channel's gain and mute apply as the local VAX tee applies
//     them, and a Core (nereusd) opens no VAX device of either direction,
//     while a remote window opens this computer's VAX outputs only;
//   - end to end (RemoteAudioSessionHarness, Opus and lossless): assigning
//     the Core's slice B to VAX 1 in a remote window puts slice B's audio
//     on VAX 1 at the session's quality, at the level the Core's own local
//     VAX gives the same signal, and keeps the choice on this computer,
//     never in the Core's Slice<N>/VaxChannel.
//
// The feeders here are pumped by the test with a harness clock; nothing
// opens this computer's speakers, microphone or VAX devices.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-23  J.J. Boyd / KG4VCF  R3 receiver audio plan, Task 5.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-09-23  J.J. Boyd / KG4VCF  R3 receiver audio fix wave: slices
//                                    sharing a channel are mixed. AI-assisted
//                                    transformation via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  iPhone app Task 73 (ruling 5.14): the
//                                    Core's own VAX tee, the level
//                                    reference here, is told to carry the
//                                    window's slices again. AI-assisted via
//                                    Anthropic Claude Code.
// =================================================================

#include <QtTest>

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/audio/RemoteVaxFeeder.h"
#include "core/session/RemoteStationOptions.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/session/media/DaemonMediaController.h"
#include "core/session/media/IReceiverPcmSink.h"
#include "core/settings/SettingsScope.h"
#include "fakes/CapacityLimitedTransport.h"
#include "fakes/PacedAudioBus.h"
#include "fakes/RemoteAudioSessionHarness.h"
#include "gui/OperatorReasonText.h"
#include "gui/RemoteAudioStatus.h"
#include "gui/RemoteMediaController.h"
#include "gui/RemoteVaxRouter.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "OperatorWording.h"

#include <QFile>
#include <QPointer>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTimer>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <deque>
#include <limits>
#include <memory>
#include <mutex>
#include <vector>

using namespace NereusSDR;

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr int kOpusFrames = 1920;

// A VAX output an app reads by its own clock: `appBlock` frames at a time
// at 48 kHz * (1 + ppm / 1e6) of the harness clock. Like the macOS HAL
// ring, a read takes what is queued and plays silence for the rest.
struct SimulatedVaxOutput {
    const qint64* now = nullptr;
    int ppm = 0;
    int appBlock = 512;
    int capacity = 96000;
    bool reading = true;
    quint64 written = 0;
    quint64 taken = 0;
    quint64 dryFrames = 0;
    quint64 readsDue = 0;       // app reads done so far
    qint64 readingSinceNs = 0;  // reads are counted from here
    quint64 readsBefore = 0;
    std::deque<float> samples;  // what is queued, for the tone check
    std::vector<float> heard;   // what the app read, when recording
    bool record = false;

    void advance()
    {
        if (!reading) { return; }
        // Reads due: floor(elapsed * rate / appBlock), in integers. The
        // harness clock steps in whole microseconds.
        const qint64 elapsedUs = (*now - readingSinceNs) / 1000;
        const quint64 frames = quint64(elapsedUs * 48 * (1'000'000 + ppm) / 1'000'000'000);
        const quint64 due = readsBefore + frames / quint64(appBlock);
        while (readsDue < due) {
            ++readsDue;
            const quint64 queued = written - taken;
            const quint64 got = std::min<quint64>(queued, quint64(appBlock));
            for (quint64 i = 0; i < got * 2; ++i) {
                if (record) { heard.push_back(samples.front()); }
                samples.pop_front();
            }
            if (record) {
                heard.insert(heard.end(), std::size_t(appBlock - int(got)) * 2, 0.0f);
            }
            taken += got;
            dryFrames += quint64(appBlock) - got;
        }
    }
    void setReading(bool on)
    {
        advance();
        reading = on;
        readsBefore = readsDue;
        readingSinceNs = *now;
    }
    VaxOutputPort port()
    {
        VaxOutputPort p;
        p.pacing = [this]() -> std::optional<IAudioBus::OutputPacing> {
            advance();
            IAudioBus::OutputPacing pacing;
            pacing.consumedFrames = taken;
            pacing.queuedFrames = int(written - taken);
            pacing.capacityFrames = capacity;
            pacing.callbackFrames = 0;
            return pacing;
        };
        p.write = [this](const float* stereo, int frames) {
            advance();
            written += quint64(frames);
            samples.insert(samples.end(), stereo, stereo + frames * 2);
            return true;
        };
        return p;
    }
};

// One Opus packet's worth of a tone, as a receiver stream delivers it.
std::vector<float> toneBlock(quint64 firstFrame, int frames, double hz, double amplitude)
{
    std::vector<float> block(std::size_t(frames) * 2);
    for (int f = 0; f < frames; ++f) {
        const double t = double(firstFrame + quint64(f)) / 48000.0;
        const float v = float(amplitude * std::sin(2.0 * kPi * hz * t));
        block[std::size_t(f) * 2] = v;
        block[std::size_t(f) * 2 + 1] = v;
    }
    return block;
}

// The tone's amplitude over one second from firstFrame: short enough that
// the rate matcher's small corrections do not smear its phase.
double amplitudeOf(const std::vector<float>& samples, double hz, std::size_t firstFrame)
{
    const auto begin = samples.begin() + qsizetype(firstFrame * 2);
    const auto end = begin + std::min<qsizetype>(48000 * 2, samples.end() - begin);
    return Test::toneAmplitude(QVector<float>(begin, end), 0, hz);
}

// A tone's amplitude averaged over ten 100 ms windows from firstFrame: each
// window is short enough that the rate matcher's corrections do not move
// the tone's phase within it.
double shortWindowAmplitude(const std::vector<float>& samples, double hz, std::size_t firstFrame)
{
    constexpr std::size_t kWindow = 4800;
    double sum = 0.0;
    for (std::size_t w = 0; w < 10; ++w) {
        const std::size_t start = firstFrame + w * kWindow;
        double cosine = 0.0;
        double sine = 0.0;
        for (std::size_t f = 0; f < kWindow; ++f) {
            const double phase = 2.0 * kPi * hz * double(f) / 48000.0;
            const double v = samples[(start + f) * 2];
            cosine += v * std::cos(phase);
            sine += v * std::sin(phase);
        }
        sum += 2.0 * std::hypot(cosine, sine) / double(kWindow);
    }
    return sum / 10.0;
}

// A bus that records what is pushed (the Core's own local VAX tee target).
class CollectingBus final : public IAudioBus {
public:
    bool open(const AudioFormat& f) override { m_format = f; m_open = true; return true; }
    void close() override { m_open = false; }
    bool isOpen() const override { return m_open; }
    qint64 push(const char* data, qint64 bytes) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const auto* f = reinterpret_cast<const float*>(data);
        m_samples.append(QVector<float>(f, f + bytes / qint64(sizeof(float))));
        return bytes;
    }
    qint64 pull(char*, qint64) override { return 0; }
    float rxLevel() const override { return 0.0f; }
    float txLevel() const override { return 0.0f; }
    QString backendName() const override { return QStringLiteral("Collecting"); }
    AudioFormat negotiatedFormat() const override { return m_format; }
    QVector<float> samples() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_samples;
    }

private:
    mutable std::mutex m_mutex;
    QVector<float> m_samples;
    AudioFormat m_format;
    bool m_open = true;
};

// Receiver-audio requests the Core received for one slice, in order.
QList<QJsonObject> receiverRequests(const QSignalSpy& coreControls, int sliceId)
{
    QList<QJsonObject> found;
    for (const auto& call : coreControls) {
        const QJsonObject control = call.at(0).toJsonObject();
        if (control.value(QStringLiteral("op")) == QLatin1String("receiver-audio")
            && control.value(QStringLiteral("sliceId")).toInt() == sliceId) {
            found << control;
        }
    }
    return found;
}

// How many times this computer's lossless link trial failed and moved the
// session to Opus. RemoteMediaController::fallBackToOpus logs "lossless link
// trial failed ...; asking Core for Opus" and reports it with its one notice,
// NetworkTooSlow; nothing else reports that notice. -1 when the controller
// reported any other error.
int losslessFallbacks(const QSignalSpy& errors)
{
    const QString tooSlow =
        remoteAudioQualityReasonText(RemoteAudioQualityReason::NetworkTooSlow);
    int fallbacks = 0;
    for (const auto& call : errors) {
        if (call.at(0).toString() != tooSlow) {
            return -1;
        }
        ++fallbacks;
    }
    return fallbacks;
}

// The streams the Core was asked for, for one slice, before it is released:
// exactly one at the session's quality; or, only after the lossless link
// trial failed, exactly two, the lossless one and then the Opus one the
// fallback asks for.
void verifyStreamRequests(const QSignalSpy& coreControls, int sliceId, bool lossless,
                          int fallbacks)
{
    QVERIFY2(fallbacks == 0 || fallbacks == 1,
             qPrintable(QStringLiteral("lossless fallbacks: %1").arg(fallbacks)));
    const QList<QJsonObject> requests = receiverRequests(coreControls, sliceId);
    for (const QJsonObject& request : requests) {
        QVERIFY(request.value(QStringLiteral("enabled")).toBool());
    }
    const auto profile = [&requests](int i) {
        return requests.at(i).value(QStringLiteral("profile")).toString();
    };
    if (fallbacks == 0) {
        QCOMPARE(requests.size(), 1);
        QCOMPARE(profile(0), lossless ? QStringLiteral("lossless") : QStringLiteral("opus"));
        return;
    }
    QVERIFY2(lossless, "an Opus session has no lossless link trial to fail");
    QCOMPARE(requests.size(), 2);
    QCOMPARE(profile(0), QStringLiteral("lossless"));
    QCOMPARE(profile(1), QStringLiteral("opus"));
}

// Waits for the Opus request a lossless fallback sends for `sliceId` to
// reach the Core, when there was one.
void waitForFallbackRequest(const QSignalSpy& coreControls, int sliceId, int fallbacks)
{
    if (fallbacks != 1) {
        return;
    }
    QTRY_VERIFY_WITH_TIMEOUT(receiverRequests(coreControls, sliceId).size() >= 2, 5000);
}

// A receiver the status shows as receiving in `profile` (the pattern in
// tst_remote_media_controller).
bool receivesIn(const RemoteMediaController& media, int sliceId, RemoteAudioProfile profile)
{
    for (const RemoteReceiverAudioStatus& receiver : media.audioStatus().receivers) {
        if (receiver.sliceId == sliceId) {
            return receiver.state == RemoteReceiverAudioStatus::State::Receiving
                && receiver.runningProfile == profile;
        }
    }
    return false;
}

// The window's media transport: the real one, or, for a row that forces the
// lossless link trial to fail, CapacityLimitedTransport (Opus fits, lossless
// does not) through the controller's MediaPeer::TransportFactory seam.
MediaPeer::TransportFactory transportFor(bool forceFallback)
{
    if (!forceFallback) {
        return {};
    }
    return [](QObject* parent) -> IMediaTransport* {
        return new CapacityLimitedTransport(parent);
    };
}

// A row that forces the fallback: the lossless trial fails once, with its one
// notice and the NetworkTooSlow reason, and each slice then receives Opus
// before anything is measured.
void waitForForcedFallback(const RemoteMediaController& media, const QSignalSpy& errors,
                           const QSignalSpy& coreControls, const QList<int>& slices)
{
    QTRY_VERIFY_WITH_TIMEOUT(errors.count() >= 1, 20000);
    QCOMPARE(errors.count(), 1);
    QCOMPARE(errors.constFirst().at(0).toString(),
             QStringLiteral("The network could not carry lossless audio; staying on Opus."));
    QCOMPARE(losslessFallbacks(errors), 1);
    QCOMPARE(media.audioStatus().qualityReason, RemoteAudioQualityReason::NetworkTooSlow);
    for (int slice : slices) {
        waitForFallbackRequest(coreControls, slice, 1);
        QTRY_VERIFY_WITH_TIMEOUT(receivesIn(media, slice, RemoteAudioProfile::Opus), 10000);
    }
}

RemoteVaxRouter::ReceiverAudio sourceFor(RemoteMediaController& media)
{
    const QPointer<RemoteMediaController> guarded(&media);
    RemoteVaxRouter::ReceiverAudio source;
    source.request = [guarded](int sliceId, IReceiverPcmSink* sink) {
        if (guarded) { guarded->requestReceiverAudio(sliceId, sink); }
    };
    source.release = [guarded](int sliceId, IReceiverPcmSink* sink) {
        if (guarded) { guarded->releaseReceiverAudio(sliceId, sink); }
    };
    return source;
}

} // namespace

class TstRemoteVaxFeeder : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        const QString profile = QStringLiteral("remote-vax-feeder-%1")
                                    .arg(QCoreApplication::applicationPid());
        AppSettings::setProfileOverride(profile);
        QCOMPARE(AppSettings::instance().filePath(), AppSettings::resolveSettingsPath(profile));
        AppSettings::instance().clear();
    }

    void cleanupTestCase()
    {
        const QString path = AppSettings::instance().filePath();
        QFile::remove(path);
        QFile::remove(path + QStringLiteral(".bak"));
    }

    // ---- The acceptance case: 10 minutes at 200 ppm, either way --------
    void holdsTheDelayForTenMinutesAt200Ppm_data()
    {
        QTest::addColumn<int>("ppm");
        QTest::newRow("output 200 ppm fast") << 200;
        QTest::newRow("output 200 ppm slow") << -200;
    }

    void holdsTheDelayForTenMinutesAt200Ppm()
    {
#ifndef HAVE_WDSP
        QSKIP("WDSP is disabled");
#else
        QFETCH(int, ppm);
        qint64 now = 0;
        SimulatedVaxOutput output;
        output.now = &now;
        output.ppm = ppm;
        RemoteVaxFeeder feeder(1, output.port(), [&now] { return now; });
        feeder.setSourceSlice(1);

        constexpr qint64 kMs = 1'000'000;
        constexpr qint64 kRunNs = 600'000 * kMs;      // 10 minutes
        constexpr qint64 kWarmupNs = 30'000 * kMs;    // the matcher settles
        quint64 sourceFrames = 0;
        quint64 dryAtWarmup = 0;
        double minuteOneDelay = 0.0;
        double minuteTenDelay = 0.0;
        int minuteOneSamples = 0;
        int minuteTenSamples = 0;
        int minDelay = std::numeric_limits<int>::max();
        int maxDelay = 0;
        double ratioSum = 0.0;
        int ratioSamples = 0;
        for (now = 0; now <= kRunNs; now += kMs) {
            // The Core's clock: one Opus packet every 40 ms.
            if (now % (40 * kMs) == 0) {
                const std::vector<float> block =
                    toneBlock(sourceFrames, kOpusFrames, 1579.0, 0.19);
                feeder.receiverAudioBlock(1, block.data(), kOpusFrames);
                sourceFrames += kOpusFrames;
            }
            if (now % (5 * kMs) == 0) {
                feeder.pump();
            }
            if (now == kWarmupNs) {
                output.advance();
                dryAtWarmup = output.dryFrames;
            }
            if (now > kWarmupNs && now % (1000 * kMs) == 0) {
                const RemoteVaxFeederStats stats = feeder.stats();
                QCOMPARE(stats.state, RemoteVaxFeederStats::State::Playing);
                const int delay = stats.delayFrames();
                minDelay = std::min(minDelay, delay);
                maxDelay = std::max(maxDelay, delay);
                if (stats.ratio) {
                    ratioSum += *stats.ratio;
                    ++ratioSamples;
                }
                if (now > 60'000 * kMs && now <= 120'000 * kMs) {
                    minuteOneDelay += delay;
                    ++minuteOneSamples;
                } else if (now > 540'000 * kMs) {
                    minuteTenDelay += delay;
                    ++minuteTenSamples;
                }
            }
        }
        output.advance();
        const RemoteVaxFeederStats stats = feeder.stats();
        minuteOneDelay /= std::max(1, minuteOneSamples);
        minuteTenDelay /= std::max(1, minuteTenSamples);
        const double meanRatioPpm = ratioSamples > 0
            ? (ratioSum / ratioSamples - 1.0) * 1e6 : 0.0;
        qInfo().noquote() << QStringLiteral(
            "ppm %1: delay minute 2 %2 ms, minute 10 %3 ms, range %4..%5 ms, mean ratio %6 ppm, "
            "dry frames after warm-up %7, restarts %8, written %9 of %10 frames")
            .arg(ppm)
            .arg(minuteOneDelay / 48.0, 0, 'f', 2).arg(minuteTenDelay / 48.0, 0, 'f', 2)
            .arg(minDelay / 48.0, 0, 'f', 2).arg(maxDelay / 48.0, 0, 'f', 2)
            .arg(meanRatioPpm, 0, 'f', 1)
            .arg(output.dryFrames - dryAtWarmup).arg(stats.restarts)
            .arg(stats.writtenFrames).arg(sourceFrames);

        // Fed throughout: no gap at the output after warm-up, no restart,
        // nothing dropped on the way in.
        QCOMPARE(output.dryFrames - dryAtWarmup, quint64(0));
        QCOMPARE(stats.restarts, 0);
        QCOMPARE(stats.droppedFrames, quint64(0));
        QVERIFY(stats.paced);
        // Uncorrected, 200 ppm over the nine minutes between the samples is
        // 104 ms of growth or starvation (5184 frames). Held: under 5 ms.
        QVERIFY2(std::abs(minuteTenDelay - minuteOneDelay) < 240.0,
                 qPrintable(QStringLiteral("delay moved from %1 to %2 frames")
                                .arg(minuteOneDelay).arg(minuteTenDelay)));
        QVERIFY2(maxDelay - minDelay < 960,
                 qPrintable(QStringLiteral("delay ranged %1..%2 frames").arg(minDelay).arg(maxDelay)));
        // The matcher corrected in the clock difference's direction. Its
        // ratio swings with each correction, so this is only the sign and
        // size of its mean over once-a-second samples.
        QVERIFY(ratioSamples > 500);
        QVERIFY2(meanRatioPpm * ppm > 0 && std::abs(meanRatioPpm) > 100.0,
                 qPrintable(QStringLiteral("mean ratio %1 ppm").arg(meanRatioPpm)));
#endif
    }

    // The feeder plays the tone it is given, continuously.
    void playsTheStreamItIsGiven()
    {
#ifndef HAVE_WDSP
        QSKIP("WDSP is disabled");
#else
        qint64 now = 0;
        SimulatedVaxOutput output;
        output.now = &now;
        // Nominal clocks: the tone check projects onto 48 kHz sample times.
        output.ppm = 0;
        output.record = true;
        RemoteVaxFeeder feeder(2, output.port(), [&now] { return now; });
        feeder.setSourceSlice(3);
        constexpr qint64 kMs = 1'000'000;
        quint64 sourceFrames = 0;
        for (now = 0; now <= 20'000 * kMs; now += kMs) {
            if (now % (40 * kMs) == 0) {
                const std::vector<float> block = toneBlock(sourceFrames, kOpusFrames, 1579.0, 0.19);
                feeder.receiverAudioBlock(3, block.data(), kOpusFrames);
                // Another slice's block reaches this sink too; it is ignored.
                const std::vector<float> other = toneBlock(sourceFrames, kOpusFrames, 617.0, 0.22);
                feeder.receiverAudioBlock(0, other.data(), kOpusFrames);
                sourceFrames += kOpusFrames;
            }
            if (now % (5 * kMs) == 0) { feeder.pump(); }
        }
        output.advance();
        // Continuous and whole: the 0.19 tone's RMS in every second, and no
        // sample-to-sample step beyond the tone's own largest slope (2A
        // sin(pi f / 48000) = 0.039; 0.08 rejects a splice or a gap). The
        // rate matcher's corrections move the tone by a few hundred ppm, so
        // its level is read as RMS, not by projecting onto 1579 Hz.
        QVERIFY(output.heard.size() > 20 * 48000 * 2 - 4096);
        for (std::size_t second = 5; second < 19; ++second) {
            double squares = 0.0;
            double largestStep = 0.0;
            for (std::size_t f = second * 48000; f < (second + 1) * 48000; ++f) {
                const double v = output.heard[f * 2];
                squares += v * v;
                largestStep = std::max(largestStep, std::abs(v - output.heard[(f - 1) * 2]));
            }
            const double rms = std::sqrt(squares / 48000.0);
            QVERIFY2(std::abs(rms - 0.19 / std::sqrt(2.0)) < 0.005,
                     qPrintable(QStringLiteral("second %1: rms %2").arg(second).arg(rms)));
            QVERIFY2(largestStep < 0.08,
                     qPrintable(QStringLiteral("second %1: step %2").arg(second).arg(largestStep)));
        }
        // Slice 0's 617 Hz blocks reached this sink too and were ignored.
        QVERIFY(amplitudeOf(output.heard, 617.0, 10 * 48000) < 0.002);
        QCOMPARE(feeder.stats().receivedFrames, sourceFrames);
#endif
    }

    // Two slices on one channel are summed, as the local VAX tee sums them.
    // One that the Core stops counts as silence at once and the other plays
    // on without a gap; one that just goes quiet is waited for kQuietNs.
    void mixesTheSlicesOnTheChannel_data()
    {
        QTest::addColumn<bool>("stopNotice");
        QTest::newRow("the Core stops slice D") << true;
        QTest::newRow("slice D goes quiet") << false;
    }

    void mixesTheSlicesOnTheChannel()
    {
#ifndef HAVE_WDSP
        QSKIP("WDSP is disabled");
#else
        QFETCH(bool, stopNotice);
        qint64 now = 0;
        SimulatedVaxOutput output;
        output.now = &now;
        output.ppm = 0;
        output.record = true;
        RemoteVaxFeeder feeder(1, output.port(), [&now] { return now; });
        feeder.setSourceSlices({3, 1});
        QCOMPARE(feeder.sourceSlices(), (QList<int>{1, 3}));
        constexpr qint64 kMs = 1'000'000;
        constexpr qint64 kStopAt = 12'000 * kMs;
        quint64 framesB = 0;
        quint64 framesD = 0;
        for (now = 0; now <= 20'000 * kMs; now += kMs) {
            // The two streams' packets arrive 20 ms apart.
            if (now % (40 * kMs) == 0) {
                const std::vector<float> block = toneBlock(framesB, kOpusFrames, 1579.0, 0.10);
                feeder.receiverAudioBlock(1, block.data(), kOpusFrames);
                framesB += kOpusFrames;
            }
            if (now % (40 * kMs) == 20 * kMs && now < kStopAt) {
                const std::vector<float> block = toneBlock(framesD, kOpusFrames, 617.0, 0.12);
                feeder.receiverAudioBlock(3, block.data(), kOpusFrames);
                framesD += kOpusFrames;
            }
            if (stopNotice && now == kStopAt) {
                feeder.receiverAudioStopped(3, QStringLiteral("slice-removed"));
            }
            if (now % (5 * kMs) == 0) { feeder.pump(); }
        }
        output.advance();
        QVERIFY(output.heard.size() > 20 * 48000 * 2 - 8192);
        const auto rmsOf = [&output](std::size_t second, double* largestStep) {
            double squares = 0.0;
            *largestStep = 0.0;
            for (std::size_t f = second * 48000; f < (second + 1) * 48000; ++f) {
                const double v = output.heard[f * 2];
                squares += v * v;
                *largestStep = std::max(*largestStep, std::abs(v - output.heard[(f - 1) * 2]));
            }
            return std::sqrt(squares / 48000.0);
        };
        // Both tones, summed: the RMS of the two, and each tone present.
        const double bothRms = std::sqrt((0.10 * 0.10 + 0.12 * 0.12) / 2.0);
        for (std::size_t second = 3; second < 11; ++second) {
            double step = 0.0;
            const double rms = rmsOf(second, &step);
            QVERIFY2(std::abs(rms - bothRms) < 0.005,
                     qPrintable(QStringLiteral("second %1: rms %2, want %3")
                                    .arg(second).arg(rms).arg(bothRms)));
            QVERIFY2(step < 0.08, qPrintable(QStringLiteral("second %1: step %2").arg(second).arg(step)));
        }
        const double toneB = shortWindowAmplitude(output.heard, 1579.0, 5 * 48000);
        const double toneD = shortWindowAmplitude(output.heard, 617.0, 5 * 48000);
        QVERIFY2(std::abs(toneB - 0.10) < 0.005 && std::abs(toneD - 0.12) < 0.005,
                 qPrintable(QStringLiteral("B %1 D %2").arg(toneB).arg(toneD)));
        // After slice D stopped: slice B alone, whole.
        for (std::size_t second = 14; second < 19; ++second) {
            double step = 0.0;
            const double rms = rmsOf(second, &step);
            QVERIFY2(std::abs(rms - 0.10 / std::sqrt(2.0)) < 0.005,
                     qPrintable(QStringLiteral("second %1: rms %2").arg(second).arg(rms)));
            QVERIFY2(step < 0.08, qPrintable(QStringLiteral("second %1: step %2").arg(second).arg(step)));
        }
        QVERIFY(shortWindowAmplitude(output.heard, 617.0, 15 * 48000) < 0.005);
        QVERIFY(std::abs(shortWindowAmplitude(output.heard, 1579.0, 15 * 48000) - 0.10) < 0.005);
        QCOMPARE(feeder.stats().state, RemoteVaxFeederStats::State::Playing);
        QCOMPARE(feeder.stats().receivedFrames, framesB + framesD);
        // A stop the Core reports costs nothing; a slice that just goes
        // quiet holds the mix for kQuietNs, at most one gap.
        if (stopNotice) {
            QCOMPARE(feeder.stats().restarts, 0);
        } else {
            QVERIFY(feeder.stats().restarts <= 1);
        }
#endif
    }

    // Follow-up: a slice that falls behind (a stall, then a burst) is kept
    // within kMaxLagFrames of the others instead of playing late for good.
    void aLateSliceIsKeptInStep()
    {
#ifndef HAVE_WDSP
        QSKIP("WDSP is disabled");
#else
        qint64 now = 0;
        SimulatedVaxOutput output;
        output.now = &now;
        RemoteVaxFeeder feeder(1, output.port(), [&now] { return now; });
        feeder.setSourceSlices({1, 3});
        constexpr qint64 kMs = 1'000'000;
        quint64 framesB = 0;
        quint64 framesD = 0;
        for (now = 0; now <= 12'000 * kMs; now += kMs) {
            if (now % (40 * kMs) == 0) {
                const std::vector<float> block = toneBlock(framesB, kOpusFrames, 1579.0, 0.10);
                feeder.receiverAudioBlock(1, block.data(), kOpusFrames);
                framesB += kOpusFrames;
            }
            // Slice D stalls from 5.0 s to 5.4 s, then the Core delivers
            // everything it held at once.
            const bool stalled = now >= 5'000 * kMs && now < 5'400 * kMs;
            if (now % (40 * kMs) == 20 * kMs && !stalled) {
                const int packets = now == 5'420 * kMs ? 11 : 1;
                for (int p = 0; p < packets; ++p) {
                    const std::vector<float> block = toneBlock(framesD, kOpusFrames, 617.0, 0.12);
                    feeder.receiverAudioBlock(3, block.data(), kOpusFrames);
                    framesD += kOpusFrames;
                }
            }
            if (now % (5 * kMs) == 0) { feeder.pump(); }
        }
        const RemoteVaxFeederStats stats = feeder.stats();
        QCOMPARE(stats.state, RemoteVaxFeederStats::State::Playing);
        // No slice holds more than kMaxLagFrames (plus a packet in flight)
        // beyond the others: slice D's burst was cut, not kept.
        QVERIFY2(stats.handoffFrames <= RemoteVaxFeeder::kMaxLagFrames + 2 * kOpusFrames,
                 qPrintable(QStringLiteral("handoff %1").arg(stats.handoffFrames)));
        QVERIFY(stats.trimmedFrames > 0);
        QCOMPARE(stats.receivedFrames, framesB + framesD);
#endif
    }

    // Follow-up: a slice leaving the channel does not cut the backlog of
    // the slices that stay (only a slice that went quiet does).
    void aSliceLeavingDoesNotCutTheOthers()
    {
#ifndef HAVE_WDSP
        QSKIP("WDSP is disabled");
#else
        qint64 now = 0;
        SimulatedVaxOutput output;
        output.now = &now;
        RemoteVaxFeeder feeder(1, output.port(), [&now] { return now; });
        feeder.setSourceSlices({1, 3});
        constexpr qint64 kMs = 1'000'000;
        quint64 framesB = 0;
        quint64 framesD = 0;
        for (now = 0; now <= 8'000 * kMs; now += kMs) {
            if (now % (40 * kMs) == 0) {
                // At 5 s slice B's Core sends three packets at once, so B
                // has a backlog above 45 ms when slice D leaves right after.
                const int packets = now == 5'000 * kMs ? 3 : 1;
                for (int p = 0; p < packets; ++p) {
                    const std::vector<float> block = toneBlock(framesB, kOpusFrames, 1579.0, 0.10);
                    feeder.receiverAudioBlock(1, block.data(), kOpusFrames);
                    framesB += kOpusFrames;
                }
            }
            if (now % (40 * kMs) == 20 * kMs && now < 5'000 * kMs) {
                const std::vector<float> block = toneBlock(framesD, kOpusFrames, 617.0, 0.12);
                feeder.receiverAudioBlock(3, block.data(), kOpusFrames);
                framesD += kOpusFrames;
            }
            if (now == 5'001 * kMs) {
                // Slice D is taken off the channel (its stream was released).
                feeder.setSourceSlices({1});
            }
            if (now % (5 * kMs) == 0) { feeder.pump(); }
        }
        QCOMPARE(feeder.sourceSlices(), QList<int>{1});
        QCOMPARE(feeder.stats().state, RemoteVaxFeederStats::State::Playing);
        QCOMPARE(feeder.stats().trimmedFrames, quint64(0));
        QCOMPARE(feeder.stats().receivedFrames, framesB + framesD);
#endif
    }

    // An output nothing reads (a VAX device no app has open) is not a
    // fault: the feeder stops writing, raises nothing, and plays again as
    // soon as an app reads.
    void anOutputNobodyReadsIsNotAFault()
    {
#ifndef HAVE_WDSP
        QSKIP("WDSP is disabled");
#else
        qint64 now = 0;
        SimulatedVaxOutput output;
        output.now = &now;
        RemoteVaxFeeder feeder(1, output.port(), [&now] { return now; });
        feeder.setSourceSlice(1);
        constexpr qint64 kMs = 1'000'000;
        quint64 sourceFrames = 0;
        const auto run = [&](qint64 until) {
            for (; now <= until; now += kMs) {
                if (now % (40 * kMs) == 0) {
                    const std::vector<float> block = toneBlock(sourceFrames, kOpusFrames, 1000.0, 0.1);
                    feeder.receiverAudioBlock(1, block.data(), kOpusFrames);
                    sourceFrames += kOpusFrames;
                }
                if (now % (5 * kMs) == 0) { feeder.pump(); }
            }
        };
        run(5'000 * kMs);
        QCOMPARE(feeder.stats().state, RemoteVaxFeederStats::State::Playing);

        output.setReading(false);
        run(6'000 * kMs);
        QCOMPARE(feeder.stats().state, RemoteVaxFeederStats::State::NoReader);
        const quint64 writtenIdle = feeder.stats().writtenFrames;
        const quint64 queuedIdle = output.written - output.taken;
        run(60'000 * kMs);
        // A minute with nothing reading: nothing more written, the queue
        // did not grow, nothing counted as a restart or dropped.
        QCOMPARE(feeder.stats().state, RemoteVaxFeederStats::State::NoReader);
        QCOMPARE(feeder.stats().writtenFrames, writtenIdle);
        QCOMPARE(output.written - output.taken, queuedIdle);
        QVERIFY(queuedIdle <= 2 * 960);
        QCOMPARE(feeder.stats().restarts, 0);
        QCOMPARE(feeder.stats().droppedFrames, quint64(0));

        output.setReading(true);
        run(65'000 * kMs);
        QCOMPARE(feeder.stats().state, RemoteVaxFeederStats::State::Playing);
        QVERIFY(feeder.stats().writtenFrames > writtenIdle + 4 * 48000);
        QCOMPARE(feeder.stats().restarts, 0);
#endif
    }

    // The Core going quiet is waited for; audio returning plays again.
    void aQuietCoreIsWaitedFor()
    {
#ifndef HAVE_WDSP
        QSKIP("WDSP is disabled");
#else
        qint64 now = 0;
        SimulatedVaxOutput output;
        output.now = &now;
        RemoteVaxFeeder feeder(1, output.port(), [&now] { return now; });
        feeder.setSourceSlice(0);
        constexpr qint64 kMs = 1'000'000;
        quint64 sourceFrames = 0;
        const auto run = [&](qint64 until, bool feed) {
            for (; now <= until; now += kMs) {
                if (feed && now % (40 * kMs) == 0) {
                    const std::vector<float> block = toneBlock(sourceFrames, kOpusFrames, 1000.0, 0.1);
                    feeder.receiverAudioBlock(0, block.data(), kOpusFrames);
                    sourceFrames += kOpusFrames;
                }
                if (now % (5 * kMs) == 0) { feeder.pump(); }
            }
        };
        run(5'000 * kMs, true);
        QCOMPARE(feeder.stats().state, RemoteVaxFeederStats::State::Playing);
        feeder.receiverAudioStopped(0, QStringLiteral("slice-removed"));
        QCOMPARE(feeder.lastStopReason(), QStringLiteral("slice-removed"));
        run(10'000 * kMs, false);
        QCOMPARE(feeder.stats().state, RemoteVaxFeederStats::State::WaitingForAudio);
        run(15'000 * kMs, true);
        QCOMPARE(feeder.stats().state, RemoteVaxFeederStats::State::Playing);
        // Audio flowed again: the stop is no longer the news.
        QVERIFY(feeder.lastStopReason().isEmpty());
        // The app heard the Core stop: at most the one gap as the rate
        // matcher ran out, and none after audio returned.
        QVERIFY(feeder.stats().restarts <= 1);
#endif
    }

    // An output with no playback timing (the Linux pactl pipe) is written
    // as the audio arrives; a full hand-off ring drops whole blocks and
    // counts them.
    void anUnpacedOutputIsWrittenAsItArrives()
    {
        qint64 now = 0;
        quint64 written = 0;
        VaxOutputPort port;
        port.write = [&written](const float*, int frames) { written += quint64(frames); return true; };
        RemoteVaxFeeder feeder(4, port, [&now] { return now; });
        feeder.setSourceSlice(2);
        const std::vector<float> block = toneBlock(0, kOpusFrames, 1000.0, 0.1);
        feeder.receiverAudioBlock(2, block.data(), kOpusFrames);
        feeder.pump();
        QCOMPARE(written, quint64(kOpusFrames));
        QVERIFY(!feeder.stats().paced);

        // Nobody pumping: 17 blocks fit in the hand-off ring, the rest drop.
        for (int i = 0; i < 20; ++i) {
            feeder.receiverAudioBlock(2, block.data(), kOpusFrames);
        }
        feeder.pump();
        const RemoteVaxFeederStats stats = feeder.stats();
        QCOMPARE(stats.droppedFrames % quint64(kOpusFrames), quint64(0));
        QVERIFY(stats.droppedFrames > 0);
        QCOMPARE(written + stats.droppedFrames, quint64(21 * kOpusFrames));

        // Unassigned: nothing is taken.
        feeder.setSourceSlice(-1);
        feeder.receiverAudioBlock(2, block.data(), kOpusFrames);
        feeder.pump();
        QCOMPARE(feeder.stats().state, RemoteVaxFeederStats::State::Idle);
        QCOMPARE(written + stats.droppedFrames, quint64(21 * kOpusFrames));
    }

    // The worker thread pumps on its own and stops cleanly.
    void theWorkerPumpsAndStops()
    {
        std::atomic<quint64> written{0};
        VaxOutputPort port;
        port.write = [&written](const float*, int frames) { written += quint64(frames); return true; };
        RemoteVaxFeeder feeder(1, port);
        feeder.setSourceSlice(0);
        feeder.startWorker();
        QVERIFY(feeder.workerRunning());
        const std::vector<float> block = toneBlock(0, kOpusFrames, 1000.0, 0.1);
        feeder.receiverAudioBlock(0, block.data(), kOpusFrames);
        QTRY_COMPARE(written.load(), quint64(kOpusFrames));
        feeder.stopWorker();
        QVERIFY(!feeder.workerRunning());
    }

    // The VAX channel's gain and mute apply as the local tee applies them;
    // muted writes silence so the output's clock keeps running.
    void channelGainAndMuteApplyAsTheLocalTeeDoes()
    {
        AudioEngine engine;
        auto bus = std::make_unique<CollectingBus>();
        CollectingBus* collected = bus.get();
        engine.setVaxBusForTest(3, std::move(bus));
        engine.setVaxRxGain(3, 0.5f);
        const std::vector<float> block = toneBlock(0, 480, 1000.0, 0.4);
        QVERIFY(engine.writeVaxOutput(3, block.data(), 480));
        engine.setVaxMuted(3, true);
        QVERIFY(engine.writeVaxOutput(3, block.data(), 480));
        const QVector<float> samples = collected->samples();
        QCOMPARE(samples.size(), 480 * 2 * 2);
        for (int i = 0; i < 480 * 2; ++i) {
            QCOMPARE(samples.at(i), block[std::size_t(i)] * 0.5f);
            QCOMPARE(samples.at(480 * 2 + i), 0.0f);
        }
        // A closed or empty channel takes nothing; a bad block is refused.
        QVERIFY(!engine.writeVaxOutput(2, block.data(), 480));
        std::vector<float> bad = block;
        bad[7] = std::nanf("");
        engine.setVaxMuted(3, false);
        QVERIFY(!engine.writeVaxOutput(3, bad.data(), 480));
        QCOMPARE(collected->samples().size(), 480 * 2 * 2);
        QVERIFY(!engine.vaxOutputHasReader(2).has_value());
    }

    // A Core opens no VAX device, receive or transmit; a remote window
    // opens this computer's receive outputs and no transmit device; a
    // local engine opens all five, as before.
    void aCoreOpensNoVaxDeviceOfEitherDirection()
    {
        const auto countingFactory = [](QList<int>* asked) {
            return [asked](int channel) -> std::unique_ptr<IAudioBus> {
                asked->append(channel);
                auto bus = std::make_unique<PacedAudioBus>();
                return bus;
            };
        };
        const auto withFakeSpeakers = [](AudioEngine& e) {
            e.setSpeakersBusForTest(std::make_unique<PacedAudioBus>());
        };

        // The Core (DaemonApp sets this before connecting).
        {
            AudioEngine core;
            QList<int> asked;
            core.setVaxOutputsAllowed(false);
            core.setVaxBusFactoryForTest(countingFactory(&asked));
            core.setStartInitializerForTest(withFakeSpeakers);
            core.start();
            core.openVaxOutputs();
            core.setVaxEnabled(2, true);
            core.setVaxConfig(1, AudioDeviceConfig{});
            core.resetAudioSettings();
            QVERIFY2(asked.isEmpty(), qPrintable(QStringLiteral("asked for %1 VAX devices")
                                                     .arg(asked.size())));
            for (int ch = 1; ch <= 4; ++ch) {
                QVERIFY(!core.isVaxBusOpen(ch));
            }
            QCOMPARE(core.pullVaxTxMic(nullptr, 0), 0);
            core.stop();
        }
        // A local window, as before: VAX 1..4 and the TX device (0).
        {
            AudioEngine local;
            QList<int> asked;
            local.setVaxBusFactoryForTest(countingFactory(&asked));
            local.setStartInitializerForTest(withFakeSpeakers);
            local.start();
            std::sort(asked.begin(), asked.end());
            QCOMPARE(asked, (QList<int>{0, 1, 2, 3, 4}));
            local.stop();
        }
        // A remote window: the receive outputs only, without a start.
        {
            RadioModel remote(RadioModel::Role::Remote);
            AudioEngine* engine = remote.localAudioDevices();
            QList<int> asked;
            engine->setVaxBusFactoryForTest(countingFactory(&asked));
            engine->openVaxOutputs();
            engine->openVaxOutputs();  // idempotent
            QCOMPARE(asked, (QList<int>{1, 2, 3, 4}));
            for (int ch = 1; ch <= 4; ++ch) {
                QVERIFY(engine->isVaxBusOpen(ch));
            }
            engine->setVaxBusFactoryForTest({});
            engine->stop();
        }
    }

    // In a remote window a slice's VAX channel is this computer's: kept
    // per Core and slice, never in the Core's Slice<N>/VaxChannel, and
    // restored for the Core's slice when it appears.
    void theChoiceIsKeptOnThisComputerPerCoreAndSlice()
    {
        RadioModel remote(RadioModel::Role::Remote);
        remote.setConnectionStateForTest(ConnectionState::Connected);
        const QString key = RemoteVaxRouter::settingsKey(QStringLiteral("corekey1"), 1);
        QCOMPARE(key, QStringLiteral("RemoteVax/corekey1/Slice1/Channel"));
        QCOMPARE(classifySettingsKey(key), SettingsScope::OperatorLocal);
        AppSettings::instance().setValue(key, QStringLiteral("3"));

        std::vector<std::pair<int, IReceiverPcmSink*>> requests;
        RemoteVaxRouter router(&remote, remote.localAudioDevices(), QStringLiteral("corekey1"),
                               [](int channel) {
                                   VaxOutputPort port;
                                   port.write = [](const float*, int) { return true; };
                                   return std::make_unique<RemoteVaxFeeder>(channel, port);
                               },
                               /*startWorkers=*/false);
        RemoteVaxRouter::ReceiverAudio source;
        source.request = [&requests](int slice, IReceiverPcmSink* sink) {
            requests.emplace_back(slice, sink);
        };
        source.release = [&requests](int slice, IReceiverPcmSink* sink) {
            requests.emplace_back(-1 - slice, sink);
        };
        router.setReceiverAudio(source);

        // Station slices arrive with their ids; slice 1 gets its channel back.
        QVERIFY(remote.addSliceWithStationId(0) >= 0);
        QVERIFY(remote.addSliceWithStationId(1) >= 0);
        QCOMPARE(remote.sliceById(0)->vaxChannel(), 0);
        QCOMPARE(remote.sliceById(1)->vaxChannel(), 3);
        // No output is open for VAX 3 in this test, so nothing is asked for.
        QVERIFY(requests.empty());

        // A pick is written here, not to the Core's key.
        remote.sliceById(0)->setVaxChannel(2);
        QCOMPARE(AppSettings::instance().value(
                     RemoteVaxRouter::settingsKey(QStringLiteral("corekey1"), 0)).toString(),
                 QStringLiteral("2"));
        QVERIFY(!AppSettings::instance().contains(QStringLiteral("Slice0/VaxChannel")));
        QVERIFY(!AppSettings::instance().contains(QStringLiteral("Slice1/VaxChannel")));

        // Another Core keeps its own.
        QCOMPARE(RemoteVaxRouter::settingsKey(QStringLiteral("corekey2"), 0),
                 QStringLiteral("RemoteVax/corekey2/Slice0/Channel"));
        RemoteStationOptions a;
        a.url = QStringLiteral("wss://core-a.example:50055");
        RemoteStationOptions b = a;
        b.url = QStringLiteral("wss://core-b.example:50055");
        RemoteStationOptions pinned = a;
        pinned.fingerprint = QStringLiteral("AA:BB:CC");
        QVERIFY(RemoteVaxRouter::coreKeyFor(a) != RemoteVaxRouter::coreKeyFor(b));
        QVERIFY(RemoteVaxRouter::coreKeyFor(a) != RemoteVaxRouter::coreKeyFor(pinned));
        QCOMPARE(RemoteVaxRouter::coreKeyFor(a), RemoteVaxRouter::coreKeyFor(a));
        QVERIFY(!RemoteVaxRouter::coreKeyFor(a).contains(QStringLiteral("example")));
        AppSettings::instance().remove(key);
        AppSettings::instance().remove(RemoteVaxRouter::settingsKey(QStringLiteral("corekey1"), 0));
    }

    // The stream runs only while an app reads the channel where the
    // platform reports it, and while the channel is assigned elsewhere;
    // every slice on a channel is asked for; request and release come in
    // order on this thread.
    void theStreamFollowsTheAssignmentAndTheReader()
    {
        RadioModel remote(RadioModel::Role::Remote);
        remote.setConnectionStateForTest(ConnectionState::Connected);
        AudioEngine* engine = remote.localAudioDevices();

        // VAX 1 reports its reader; VAX 2 cannot say.
        class ReaderBus final : public IAudioBus {
        public:
            explicit ReaderBus(std::optional<bool>* reader) : m_reader(reader) {}
            bool open(const AudioFormat&) override { return true; }
            void close() override {}
            bool isOpen() const override { return true; }
            qint64 push(const char*, qint64 bytes) override { return bytes; }
            qint64 pull(char*, qint64) override { return 0; }
            float rxLevel() const override { return 0.0f; }
            float txLevel() const override { return 0.0f; }
            QString backendName() const override { return QStringLiteral("Reader"); }
            AudioFormat negotiatedFormat() const override { return {}; }
            std::optional<bool> outputHasReader() const override { return *m_reader; }
        private:
            std::optional<bool>* m_reader;
        };
        std::optional<bool> vax1Reader = false;
        std::optional<bool> vax2Reader;
        engine->setVaxBusForTest(1, std::make_unique<ReaderBus>(&vax1Reader));
        engine->setVaxBusForTest(2, std::make_unique<ReaderBus>(&vax2Reader));

        QStringList log;
        RemoteVaxRouter router(&remote, engine, QStringLiteral("corekey3"),
                               [](int channel) {
                                   VaxOutputPort port;
                                   port.write = [](const float*, int) { return true; };
                                   return std::make_unique<RemoteVaxFeeder>(channel, port);
                               },
                               /*startWorkers=*/false);
        RemoteVaxRouter::ReceiverAudio source;
        source.request = [&log, &router](int slice, IReceiverPcmSink* sink) {
            int channel = 0;
            for (int ch = 1; ch <= 4; ++ch) {
                if (router.feeder(ch) == sink) { channel = ch; }
            }
            log << QStringLiteral("request %1 on VAX %2").arg(slice).arg(channel);
        };
        source.release = [&log, &router](int slice, IReceiverPcmSink* sink) {
            int channel = 0;
            for (int ch = 1; ch <= 4; ++ch) {
                if (router.feeder(ch) == sink) { channel = ch; }
            }
            log << QStringLiteral("release %1 on VAX %2").arg(slice).arg(channel);
        };
        router.setReceiverAudio(source);
        QVERIFY(remote.addSliceWithStationId(0) >= 0);
        QVERIFY(remote.addSliceWithStationId(1) >= 0);
        QVERIFY(remote.addSliceWithStationId(2) >= 0);

        // Slice B on VAX 1 with no app reading VAX 1: nothing asked for.
        remote.sliceById(1)->setVaxChannel(1);
        QCOMPARE(router.requestedSlice(1), -1);
        QVERIFY(log.isEmpty());
        // An app opens VAX 1: slice B is asked for, into VAX 1's feeder.
        vax1Reader = true;
        router.refresh();
        QCOMPARE(log, QStringList{QStringLiteral("request 1 on VAX 1")});
        QCOMPARE(router.feeder(1)->sourceSlice(), 1);
        // Slice A joins VAX 1: both are asked for, and mixed (the fix
        // wave; this used to hand VAX 1 to the lowest slice alone).
        remote.sliceById(0)->setVaxChannel(1);
        QCOMPARE(router.requestedSlices(1), (QList<int>{0, 1}));
        QCOMPARE(router.requestedSlice(1), 0);
        QCOMPARE(log.mid(1), QStringList{QStringLiteral("request 0 on VAX 1")});
        QCOMPARE(router.feeder(1)->sourceSlices(), (QList<int>{0, 1}));
        // Slice B leaves VAX 1: only it is released.
        remote.sliceById(1)->setVaxChannel(0);
        QCOMPARE(log.mid(2), QStringList{QStringLiteral("release 1 on VAX 1")});
        QCOMPARE(router.requestedSlices(1), QList<int>{0});
        QCOMPARE(router.feeder(1)->sourceSlices(), QList<int>{0});
        // The app closes VAX 1: released.
        vax1Reader = false;
        router.refresh();
        QCOMPARE(log.constLast(), QStringLiteral("release 0 on VAX 1"));
        QCOMPARE(router.feeder(1)->sourceSlice(), -1);
        // VAX 2 cannot report a reader: it runs while assigned.
        log.clear();
        remote.sliceById(2)->setVaxChannel(2);
        QCOMPARE(log, QStringList{QStringLiteral("request 2 on VAX 2")});
        remote.sliceById(2)->setVaxChannel(0);
        QCOMPARE(log.constLast(), QStringLiteral("release 2 on VAX 2"));
        // A channel with no open output asks for nothing.
        log.clear();
        remote.sliceById(2)->setVaxChannel(4);
        QVERIFY(log.isEmpty());
        // Removing a carried slice releases it.
        remote.sliceById(2)->setVaxChannel(2);
        QCOMPARE(log.constLast(), QStringLiteral("request 2 on VAX 2"));
        remote.removeSliceWithStationId(2);
        QCOMPARE(log.constLast(), QStringLiteral("release 2 on VAX 2"));
        for (int slice = 0; slice < 2; ++slice) {
            AppSettings::instance().remove(RemoteVaxRouter::settingsKey(QStringLiteral("corekey3"), slice));
        }
        AppSettings::instance().remove(RemoteVaxRouter::settingsKey(QStringLiteral("corekey3"), 2));
    }

    // Follow-up: slice B joins VAX 1 with slice A already playing on it,
    // and the Core refuses B. A's audio keeps flowing, and the refusal is
    // still raised, naming B and B's reason.
    void aRefusedSliceOnASharedChannelIsExplained()
    {
        RadioModel remote(RadioModel::Role::Remote);
        remote.setConnectionStateForTest(ConnectionState::Connected);
        AudioEngine* engine = remote.localAudioDevices();
        engine->setVaxBusForTest(1, std::make_unique<CollectingBus>());
        RemoteVaxRouter router(&remote, engine, QStringLiteral("corekey5"),
                               [](int channel) {
                                   VaxOutputPort port;
                                   port.write = [](const float*, int) { return true; };
                                   return std::make_unique<RemoteVaxFeeder>(channel, port);
                               },
                               /*startWorkers=*/false);
        RemoteVaxRouter::ReceiverAudio source;
        source.request = [](int, IReceiverPcmSink*) {};
        source.release = [](int, IReceiverPcmSink*) {};
        router.setReceiverAudio(source);
        QSignalSpy notices(&router, &RemoteVaxRouter::notice);
        QVERIFY(remote.addSliceWithStationId(0) >= 0);
        QVERIFY(remote.addSliceWithStationId(1) >= 0);
        remote.sliceById(0)->setVaxChannel(1);
        RemoteVaxFeeder* feeder = router.feeder(1);
        const std::vector<float> block = toneBlock(0, kOpusFrames, 1000.0, 0.1);
        feeder->receiverAudioBlock(0, block.data(), kOpusFrames);
        remote.sliceById(1)->setVaxChannel(1);
        QCOMPARE(router.requestedSlices(1), (QList<int>{0, 1}));
        // The Core refuses slice B; slice A's audio keeps arriving before
        // the router next looks.
        feeder->receiverAudioStopped(1, QStringLiteral("receiver-limit"));
        for (int i = 0; i < 3; ++i) {
            feeder->receiverAudioBlock(0, block.data(), kOpusFrames);
        }
        router.refresh();
        QCOMPARE(notices.count(), 1);
        QCOMPARE(notices.constFirst().at(0).toInt(), 1);
        QCOMPARE(notices.constFirst().at(1).toInt(), 1);
        QCOMPARE(notices.constFirst().at(2).toString(), QStringLiteral("receiver-limit"));
        // Raised once.
        router.refresh();
        QCOMPARE(notices.count(), 1);
        for (int slice = 0; slice < 2; ++slice) {
            AppSettings::instance().remove(RemoteVaxRouter::settingsKey(QStringLiteral("corekey5"), slice));
        }
    }

    // End to end: slice B on VAX 1 in a remote window carries slice B at
    // the session's quality and at the level the Core's own VAX gives.
    void sliceBOnVax1PlaysAtTheLocalLevel_data()
    {
        QTest::addColumn<bool>("lossless");
        QTest::addColumn<bool>("delayedRoute");
        QTest::addColumn<bool>("forceFallback");
        QTest::newRow("opus") << false << false << false;
        QTest::newRow("lossless") << true << false << false;
        QTest::newRow("lossless-delayed-route") << true << true << false;
        // The link cannot carry lossless: the trial fails every time.
        QTest::newRow("lossless-forced-fallback") << true << false << true;
    }

    void sliceBOnVax1PlaysAtTheLocalLevel()
    {
        QFETCH(bool, lossless);
        QFETCH(bool, delayedRoute);
        QFETCH(bool, forceFallback);
        AppSettings::instance().setValue(
            QLatin1String(RemoteMediaController::kAudioProfileSettingKey),
            lossless ? QStringLiteral("Lossless") : QStringLiteral("Opus"));
        const auto restoreChoice = qScopeGuard([] {
            AppSettings::instance().remove(
                QLatin1String(RemoteMediaController::kAudioProfileSettingKey));
        });
        Test::RemoteAudioSessionHarness h;
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr, nullptr,
                                          transportFor(forceFallback));
        DaemonMediaController daemonMedia(&h.server, &h.station);
        QSignalSpy coreControls(&h.server, &StationServer::mediaControlReceived);
        QSignalSpy remoteErrors(&remoteMedia, &RemoteMediaController::errorOccurred);

        // The Core's own local VAX 1, fed by its tee from slice B.
        auto stationBus = std::make_unique<CollectingBus>();
        CollectingBus* stationVax = stationBus.get();
        h.stationAudio->setVaxBusForTest(1, std::move(stationBus));
        h.stationAudio->setVaxRxGain(1, 0.5f);
        h.station.sliceById(h.sliceB)->setVaxChannel(1);
        // The Core's own choice, stored where the Core keeps it. This test
        // process shares one settings store between the two ends, so clear
        // it: the remote window must not write it back.
        AppSettings::instance().remove(QStringLiteral("Slice%1/VaxChannel").arg(h.sliceB));

        // This computer's VAX 1, read by an app at 48 kHz.
        AudioEngine* remoteEngine = h.remote.audioEngine();
        auto vaxBus = std::make_unique<PacedAudioBus>();
        PacedAudioBus* remoteVax = vaxBus.get();
        remoteEngine->setVaxBusForTest(1, std::move(vaxBus));
        remoteEngine->setVaxRxGain(1, 0.5f);

        QTimer source;
        source.setInterval(10);
        source.setTimerType(Qt::PreciseTimer);
        connect(&source, &QTimer::timeout, &source, [&h] { h.feedMixedTone(); });
        QTimer devices;
        devices.setInterval(10);
        devices.setTimerType(Qt::PreciseTimer);
        connect(&devices, &QTimer::timeout, &devices, [&h, remoteVax] {
            h.remoteBus->render(Test::RemoteAudioSessionHarness::kFrames);
            remoteVax->render(Test::RemoteAudioSessionHarness::kFrames);
        });
        source.start();
        devices.start();

        h.connectSession();
        // iPhone app Task 73 (ruling 5.14): once the window owns the Core's
        // slices, the Core computer's VAX carries none of them. Here the
        // Core's tee is only the local level the window's VAX is held to
        // (a real Core publishes no VAX device, R-R3-44), so it is told to
        // carry every slice again.
        h.stationAudio->setVaxSliceMask(0xFFFFFFFFu);
        QVERIFY(remoteMedia.receiverAudioNegotiated());
        h.remote.audioEngine()->setMasterMuted(true);
        QTRY_VERIFY(h.remote.sliceById(h.sliceB) != nullptr);

        const QString coreKey = QStringLiteral("harnesscore");
        RemoteVaxRouter router(&h.remote, remoteEngine, coreKey);
        router.setReceiverAudio(sourceFor(remoteMedia));
        QTest::qWait(200);
        QCOMPARE(receiverRequests(coreControls, h.sliceB).size(), 0);

        if (delayedRoute) {
            QTRY_VERIFY_WITH_TIMEOUT(remoteVax->heard.size() >= 2 * 48000 * 2, 20000);
        }

        // The operator puts slice B on VAX 1 in the remote window.
        h.remote.sliceById(h.sliceB)->setVaxChannel(1);
        QCOMPARE(router.requestedSlice(1), h.sliceB);
        if (forceFallback) {
            waitForForcedFallback(remoteMedia, remoteErrors, coreControls, {h.sliceB});
            if (QTest::currentTestFailed()) {
                return;
            }
        }
        // Measure a fixed window after the stream starts. Connection setup
        // and time before the operator routes audio are not playback samples.
        // Keep the original one-second settling interval within this window;
        // any subsequent silence or discontinuity still affects the level.
        QTRY_VERIFY_WITH_TIMEOUT(router.feeder(1)->stats().state == RemoteVaxFeederStats::State::Playing
                                 && router.feeder(1)->stats().writtenFrames > 0, 20000);
        qsizetype remoteStart = remoteVax->heard.size();
        qsizetype localStart = stationVax->samples().size();
        constexpr qsizetype measurementSamples = 3 * 48000 * 2;
        QTRY_VERIFY_WITH_TIMEOUT(remoteVax->heard.size() >= remoteStart + measurementSamples, 20000);
        QTRY_VERIFY_WITH_TIMEOUT(stationVax->samples().size() >= localStart + measurementSamples, 20000);
        // A lossless link trial that failed on this machine moved the stream
        // to Opus, as designed: its window may hold the switch, so the level
        // is measured again on the Opus stream, held to Opus's tolerance.
        const int fallbacks = losslessFallbacks(remoteErrors);
        waitForFallbackRequest(coreControls, h.sliceB, fallbacks);
        verifyStreamRequests(coreControls, h.sliceB, lossless, fallbacks);
        if (QTest::currentTestFailed()) {
            return;
        }
        if (fallbacks == 1 && !forceFallback) {
            qInfo() << "the lossless link trial failed; measuring again on Opus";
            // The window may hold the switchover gap or the lossless tail:
            // wait until the slice actually receives Opus, then measure.
            QTRY_VERIFY_WITH_TIMEOUT(receivesIn(remoteMedia, h.sliceB, RemoteAudioProfile::Opus),
                                     10000);
            QTRY_VERIFY_WITH_TIMEOUT(router.feeder(1)->stats().state
                                         == RemoteVaxFeederStats::State::Playing, 20000);
            remoteStart = remoteVax->heard.size();
            localStart = stationVax->samples().size();
            QTRY_VERIFY_WITH_TIMEOUT(remoteVax->heard.size() >= remoteStart + measurementSamples, 20000);
            QTRY_VERIFY_WITH_TIMEOUT(stationVax->samples().size() >= localStart + measurementSamples, 20000);
            // No second fallback: the trial ended with the first.
            QCOMPARE(losslessFallbacks(remoteErrors), 1);
            verifyStreamRequests(coreControls, h.sliceB, lossless, fallbacks);
        }
        if (forceFallback) {
            // Exactly two requests, lossless then Opus, and one fallback.
            QCOMPARE(fallbacks, 1);
        }
        const bool playedLossless = lossless && fallbacks == 0;
        QCOMPARE(router.feeder(1)->stats().state, RemoteVaxFeederStats::State::Playing);

        // The same level as the Core's own VAX 1 for the same signal.
        const QVector<float> heard = remoteVax->heard.mid(remoteStart, measurementSamples);
        const QVector<float> local = stationVax->samples().mid(localStart, measurementSamples);
        const int skip = 48000;  // past the start
        const double remoteB = Test::toneAmplitude(heard, 0, Test::RemoteAudioSessionHarness::kSliceBToneHz, skip);
        const double remoteA = Test::toneAmplitude(heard, 0, Test::RemoteAudioSessionHarness::kSliceAToneHz, skip);
        const double localB = Test::toneAmplitude(local, 0, Test::RemoteAudioSessionHarness::kSliceBToneHz, skip);
        qInfo() << (playedLossless ? "lossless" : "opus") << "remote VAX 1 slice B" << remoteB
                << "slice A" << remoteA << "Core's own VAX 1 slice B" << localB;
        QVERIFY(localB > 0.09);
        // Lossless is the same samples; Opus measured within 2 % of it.
        QVERIFY2(std::abs(remoteB - localB) < (playedLossless ? 0.0005 : 0.005),
                 qPrintable(QStringLiteral("remote %1 local %2").arg(remoteB).arg(localB)));
        QVERIFY(remoteB > 20.0 * remoteA);

        // Kept on this computer, not in the Core's key.
        QCOMPARE(AppSettings::instance().value(RemoteVaxRouter::settingsKey(coreKey, h.sliceB))
                     .toString(), QStringLiteral("1"));
        QVERIFY(!AppSettings::instance().contains(
            QStringLiteral("Slice%1/VaxChannel").arg(h.sliceB)));

        // A trial that failed after the measurement (its window was all
        // lossless) still asked for exactly the one Opus stream.
        const int fallbacksBeforeRelease = losslessFallbacks(remoteErrors);
        waitForFallbackRequest(coreControls, h.sliceB, fallbacksBeforeRelease);
        verifyStreamRequests(coreControls, h.sliceB, lossless, fallbacksBeforeRelease);

        // Off again: released at the Core.
        h.remote.sliceById(h.sliceB)->setVaxChannel(0);
        QTRY_VERIFY_WITH_TIMEOUT(!receiverRequests(coreControls, h.sliceB).constLast()
                                      .value(QStringLiteral("enabled")).toBool(), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(daemonMedia.activeReceiverAudioStreamCount(), 0, 5000);
        // No error but a lossless fallback's one notice.
        const int fallbacksAtEnd = losslessFallbacks(remoteErrors);
        QVERIFY2(fallbacksAtEnd == 0 || (lossless && fallbacksAtEnd == 1),
                 qPrintable(QStringLiteral("errors: %1").arg(remoteErrors.count())));
        if (forceFallback) {
            QCOMPARE(fallbacksAtEnd, 1);
        }
        AppSettings::instance().remove(RemoteVaxRouter::settingsKey(coreKey, h.sliceB));
    }

    // End to end: slices A and B both on VAX 1, in a remote window and at
    // the Core: the remote window's VAX 1 carries both, each at the level
    // the Core's own VAX 1 gives it.
    void slicesAAndBOnVax1MixAsTheLocalVaxDoes_data()
    {
        QTest::addColumn<bool>("lossless");
        QTest::addColumn<bool>("delayedRoute");
        QTest::addColumn<bool>("forceFallback");
        QTest::newRow("opus") << false << false << false;
        QTest::newRow("lossless") << true << false << false;
        QTest::newRow("lossless-delayed-route") << true << true << false;
        // The link cannot carry lossless: the trial fails every time.
        QTest::newRow("lossless-forced-fallback") << true << false << true;
    }

    void slicesAAndBOnVax1MixAsTheLocalVaxDoes()
    {
        QFETCH(bool, lossless);
        QFETCH(bool, delayedRoute);
        QFETCH(bool, forceFallback);
        AppSettings::instance().setValue(
            QLatin1String(RemoteMediaController::kAudioProfileSettingKey),
            lossless ? QStringLiteral("Lossless") : QStringLiteral("Opus"));
        const auto restoreChoice = qScopeGuard([] {
            AppSettings::instance().remove(
                QLatin1String(RemoteMediaController::kAudioProfileSettingKey));
        });
        Test::RemoteAudioSessionHarness h;
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr, nullptr,
                                          transportFor(forceFallback));
        DaemonMediaController daemonMedia(&h.server, &h.station);
        QSignalSpy coreControls(&h.server, &StationServer::mediaControlReceived);
        QSignalSpy remoteErrors(&remoteMedia, &RemoteMediaController::errorOccurred);

        // The Core's own local VAX 1, fed by its tee from slices A and B.
        auto stationBus = std::make_unique<CollectingBus>();
        CollectingBus* stationVax = stationBus.get();
        h.stationAudio->setVaxBusForTest(1, std::move(stationBus));
        h.stationAudio->setVaxRxGain(1, 0.5f);
        h.station.sliceById(h.sliceA)->setVaxChannel(1);
        h.station.sliceById(h.sliceB)->setVaxChannel(1);
        for (int slice : {h.sliceA, h.sliceB}) {
            AppSettings::instance().remove(QStringLiteral("Slice%1/VaxChannel").arg(slice));
        }

        AudioEngine* remoteEngine = h.remote.audioEngine();
        auto vaxBus = std::make_unique<PacedAudioBus>();
        PacedAudioBus* remoteVax = vaxBus.get();
        remoteEngine->setVaxBusForTest(1, std::move(vaxBus));
        remoteEngine->setVaxRxGain(1, 0.5f);

        QTimer source;
        source.setInterval(10);
        source.setTimerType(Qt::PreciseTimer);
        connect(&source, &QTimer::timeout, &source, [&h] { h.feedMixedTone(); });
        QTimer devices;
        devices.setInterval(10);
        devices.setTimerType(Qt::PreciseTimer);
        connect(&devices, &QTimer::timeout, &devices, [&h, remoteVax] {
            h.remoteBus->render(Test::RemoteAudioSessionHarness::kFrames);
            remoteVax->render(Test::RemoteAudioSessionHarness::kFrames);
        });
        source.start();
        devices.start();

        h.connectSession();
        // iPhone app Task 73 (ruling 5.14): once the window owns the Core's
        // slices, the Core computer's VAX carries none of them. Here the
        // Core's tee is only the local level the window's VAX is held to
        // (a real Core publishes no VAX device, R-R3-44), so it is told to
        // carry every slice again.
        h.stationAudio->setVaxSliceMask(0xFFFFFFFFu);
        QVERIFY(remoteMedia.receiverAudioNegotiated());
        h.remote.audioEngine()->setMasterMuted(true);
        QTRY_VERIFY(h.remote.sliceById(h.sliceA) != nullptr);
        QTRY_VERIFY(h.remote.sliceById(h.sliceB) != nullptr);

        const QString coreKey = QStringLiteral("harnesscore2");
        RemoteVaxRouter router(&h.remote, remoteEngine, coreKey);
        router.setReceiverAudio(sourceFor(remoteMedia));
        if (delayedRoute) {
            QTRY_VERIFY_WITH_TIMEOUT(remoteVax->heard.size() >= 2 * 48000 * 2, 20000);
        }
        h.remote.sliceById(h.sliceA)->setVaxChannel(1);
        h.remote.sliceById(h.sliceB)->setVaxChannel(1);
        QList<int> both{h.sliceA, h.sliceB};
        std::sort(both.begin(), both.end());
        QCOMPARE(router.requestedSlices(1), both);
        if (forceFallback) {
            waitForForcedFallback(remoteMedia, remoteErrors, coreControls, both);
            if (QTest::currentTestFailed()) {
                return;
            }
        }
        // Measure a fixed window after the stream starts. Connection setup
        // and time before the operator routes audio are not playback samples.
        // Keep the original one-second settling interval within this window;
        // any subsequent silence or discontinuity still affects the level.
        QTRY_VERIFY_WITH_TIMEOUT(router.feeder(1)->stats().state == RemoteVaxFeederStats::State::Playing
                                 && router.feeder(1)->stats().writtenFrames > 0, 20000);
        qsizetype remoteStart = remoteVax->heard.size();
        qsizetype localStart = stationVax->samples().size();
        constexpr qsizetype measurementSamples = 3 * 48000 * 2;
        QTRY_VERIFY_WITH_TIMEOUT(remoteVax->heard.size() >= remoteStart + measurementSamples, 20000);
        QTRY_VERIFY_WITH_TIMEOUT(stationVax->samples().size() >= localStart + measurementSamples, 20000);
        // As in sliceBOnVax1PlaysAtTheLocalLevel: a failed lossless link
        // trial moves both streams to Opus, and the mix is measured again.
        const int fallbacks = losslessFallbacks(remoteErrors);
        for (int slice : {h.sliceA, h.sliceB}) {
            waitForFallbackRequest(coreControls, slice, fallbacks);
            verifyStreamRequests(coreControls, slice, lossless, fallbacks);
        }
        if (QTest::currentTestFailed()) {
            return;
        }
        if (fallbacks == 1 && !forceFallback) {
            qInfo() << "the lossless link trial failed; measuring again on Opus";
            // The window may hold the switchover gap or the lossless tail:
            // wait until both slices actually receive Opus, then measure.
            for (int slice : {h.sliceA, h.sliceB}) {
                QTRY_VERIFY_WITH_TIMEOUT(receivesIn(remoteMedia, slice, RemoteAudioProfile::Opus),
                                         10000);
            }
            QTRY_VERIFY_WITH_TIMEOUT(router.feeder(1)->stats().state
                                         == RemoteVaxFeederStats::State::Playing, 20000);
            remoteStart = remoteVax->heard.size();
            localStart = stationVax->samples().size();
            QTRY_VERIFY_WITH_TIMEOUT(remoteVax->heard.size() >= remoteStart + measurementSamples, 20000);
            QTRY_VERIFY_WITH_TIMEOUT(stationVax->samples().size() >= localStart + measurementSamples, 20000);
            QCOMPARE(losslessFallbacks(remoteErrors), 1);
            for (int slice : {h.sliceA, h.sliceB}) {
                verifyStreamRequests(coreControls, slice, lossless, fallbacks);
            }
        }
        if (forceFallback) {
            // Exactly two requests, lossless then Opus, and one fallback.
            QCOMPARE(fallbacks, 1);
        }
        const bool playedLossless = lossless && fallbacks == 0;
        QCOMPARE(router.feeder(1)->stats().state, RemoteVaxFeederStats::State::Playing);

        const QVector<float> heard = remoteVax->heard.mid(remoteStart, measurementSamples);
        const QVector<float> local = stationVax->samples().mid(localStart, measurementSamples);
        const int skip = 48000;
        using H = Test::RemoteAudioSessionHarness;
        const double remoteA = Test::toneAmplitude(heard, 0, H::kSliceAToneHz, skip);
        const double remoteB = Test::toneAmplitude(heard, 0, H::kSliceBToneHz, skip);
        const double localA = Test::toneAmplitude(local, 0, H::kSliceAToneHz, skip);
        const double localB = Test::toneAmplitude(local, 0, H::kSliceBToneHz, skip);
        qInfo() << (playedLossless ? "lossless" : "opus") << "remote VAX 1 slice A" << remoteA
                << "slice B" << remoteB << "Core's own VAX 1 slice A" << localA
                << "slice B" << localB;
        QVERIFY(localA > 0.09);
        QVERIFY(localB > 0.09);
        const double tolerance = playedLossless ? 0.0005 : 0.005;
        QVERIFY2(std::abs(remoteA - localA) < tolerance,
                 qPrintable(QStringLiteral("A: remote %1 local %2").arg(remoteA).arg(localA)));
        QVERIFY2(std::abs(remoteB - localB) < tolerance,
                 qPrintable(QStringLiteral("B: remote %1 local %2").arg(remoteB).arg(localB)));

        const int fallbacksBeforeRelease = losslessFallbacks(remoteErrors);
        for (int slice : {h.sliceA, h.sliceB}) {
            waitForFallbackRequest(coreControls, slice, fallbacksBeforeRelease);
            verifyStreamRequests(coreControls, slice, lossless, fallbacksBeforeRelease);
        }

        h.remote.sliceById(h.sliceA)->setVaxChannel(0);
        h.remote.sliceById(h.sliceB)->setVaxChannel(0);
        QTRY_COMPARE_WITH_TIMEOUT(daemonMedia.activeReceiverAudioStreamCount(), 0, 5000);
        const int fallbacksAtEnd = losslessFallbacks(remoteErrors);
        QVERIFY2(fallbacksAtEnd == 0 || (lossless && fallbacksAtEnd == 1),
                 qPrintable(QStringLiteral("errors: %1").arg(remoteErrors.count())));
        if (forceFallback) {
            QCOMPARE(fallbacksAtEnd, 1);
        }
        for (int slice : {h.sliceA, h.sliceB}) {
            AppSettings::instance().remove(RemoteVaxRouter::settingsKey(coreKey, slice));
        }
    }

    // An older Core: nothing is asked for on the wire, and the operator is
    // told once, in plain words.
    void anOlderCoreIsExplainedOnce()
    {
        Test::RemoteAudioSessionHarness h;
        h.hideReceiverAudio = true;
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
        DaemonMediaController daemonMedia(&h.server, &h.station);
        QSignalSpy coreControls(&h.server, &StationServer::mediaControlReceived);
        AudioEngine* remoteEngine = h.remote.audioEngine();
        remoteEngine->setVaxBusForTest(1, std::make_unique<PacedAudioBus>());
        QTimer source;
        source.setInterval(10);
        source.setTimerType(Qt::PreciseTimer);
        connect(&source, &QTimer::timeout, &source, [&h] { h.feedMixedTone(); });
        QTimer speaker;
        speaker.setInterval(10);
        speaker.setTimerType(Qt::PreciseTimer);
        connect(&speaker, &QTimer::timeout, &speaker,
                [&h] { h.remoteBus->render(Test::RemoteAudioSessionHarness::kFrames); });
        source.start();
        speaker.start();
        h.connectSession();
        QTRY_VERIFY(h.remote.sliceById(h.sliceB) != nullptr);

        RemoteVaxRouter router(&h.remote, remoteEngine, QStringLiteral("oldcore"));
        router.setReceiverAudio(sourceFor(remoteMedia));
        QSignalSpy notices(&router, &RemoteVaxRouter::notice);
        h.remote.sliceById(h.sliceB)->setVaxChannel(1);
        QTRY_COMPARE_WITH_TIMEOUT(notices.count(), 1, 3000);
        QCOMPARE(notices.constFirst().at(1).toInt(), h.sliceB);
        const QString reason = notices.constFirst().at(2).toString();
        QCOMPARE(reason, QString::fromLatin1(RemoteMediaController::kReceiverAudioUnavailableReason));
        QVERIFY(OperatorWording::isPlain(OperatorReasonText::forDisplay(reason)));
        QTest::qWait(1500);
        QCOMPARE(notices.count(), 1);
        QCOMPARE(receiverRequests(coreControls, h.sliceB).size(), 0);
        AppSettings::instance().remove(RemoteVaxRouter::settingsKey(QStringLiteral("oldcore"), h.sliceB));
    }
};

QTEST_MAIN(TstRemoteVaxFeeder)
#include "tst_remote_vax_feeder.moc"
