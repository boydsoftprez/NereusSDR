// Modification history (NereusSDR): 2026-10-27 J.J. Boyd KG4VCF, AI-assisted OpenAI Codex.
// no-port-check: Diagnostic measurement fixture; never a product or candidate input.
// Measures first- and last-packet availability ages from appendPcm to
// the block arriving at a TCI app (a QWebSocket on its own thread).
// This is not audible latency. Steady state, then 200 ms
// stalls of the thread the TciServer drains on.
#include <QtTest>
#include <QThread>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QWebSocket>

#include "core/AppSettings.h"
#include "core/TciServer.h"
#include "core/TciBinaryFrame.h"
#include "core/session/media/IReceiverPcmSink.h"
#include "core/session/media/RemoteTciAudioStage.h"
#include "models/RadioModel.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cmath>
#include <optional>
#include <cstdio>
#include <cstring>
#include <memory>
#include <limits>
#include <mutex>
#include <thread>
#include <vector>

using namespace NereusSDR;

namespace {

qint64 nowNs()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch()).count();
}

quint32 le32(const QByteArray& f, int at)
{
    const auto* p = reinterpret_cast<const quint8*>(f.constData() + at);
    return quint32(p[0]) | (quint32(p[1]) << 8) | (quint32(p[2]) << 16) | (quint32(p[3]) << 24);
}

// Frame k: left = low 20 bits / 2^20, right = -(high bits + 1) / 4096.
// Both exact in float32.
void encode(quint64 k, float* lr)
{
    lr[0] = float(k & 0xFFFFF) / 1048576.0f;
    lr[1] = -float(((k >> 20) & 0x7FF) + 1) / 4096.0f;
}
bool decode(const char* data, qint64& index)
{
    float left = 0;
    float right = 0;
    std::memcpy(&left, data, sizeof(float));
    std::memcpy(&right, data + sizeof(float), sizeof(float));
    const double low = double(left) * 1048576.0;
    const double high = -double(right) * 4096.0;
    if (!std::isfinite(low) || !std::isfinite(high) || low < 0 || low > 1048575
        || high < 1 || high > 2048 || std::floor(low) != low || std::floor(high) != high) {
        return false;
    }
    index = (qint64(high - 1) << 20) | qint64(low);
    return true;
}

constexpr int kPacketFrames = 960;   // 20 ms Opus packet
constexpr qint64 kPacketNs = 20'000'000;

struct Generator final : IReceiverPcmSink {
    std::shared_ptr<RemoteTciAudioStage> stage;
    IReceiverPcmSink* downstream = nullptr;
    int slice = 0;
    std::atomic<bool> stop{false};
    std::thread worker;
    std::vector<qint64> appendAt;   // per packet
    std::mutex m;
    std::condition_variable cv;

    Generator(int sliceId, IReceiverPcmSink* sink)
        : stage(std::make_shared<RemoteTciAudioStage>(sliceId)), downstream(sink), slice(sliceId)
    {
        appendAt.reserve(200000);
        worker = std::thread([this] { loop(); });
    }
    ~Generator() override { finish(); }
    void finish()
    {
        stop = true;
        cv.notify_all();
        if (worker.joinable()) { worker.join(); }
    }
    void loop()
    {
        auto run = stage->createRun();
        run->reconcile();
        std::vector<float> pcm(std::size_t(kPacketFrames) * 2);
        quint64 written = 0;
        qint64 next = nowNs();
        while (!stop.load()) {
            if (!run->hasRunnableWork()) {
                // As RemoteAudioReceiver::WorkerState::pass: wait up to 2 ms
                // or until the next packet.
                std::unique_lock<std::mutex> lock(m);
                const qint64 until = std::min(next, nowNs() + 2'000'000);
                const qint64 wait = until - nowNs();
                if (wait > 0) {
                    cv.wait_for(lock, std::chrono::nanoseconds(wait), [this] { return stop.load(); });
                }
            }
            if (stop.load()) { break; }
            if (nowNs() >= next) {
                for (int i = 0; i < kPacketFrames; ++i) { encode(written + quint64(i), &pcm[std::size_t(2 * i)]); }
                downstream->receiverAudioBlock(slice, pcm.data(), kPacketFrames);
                appendAt.push_back(nowNs());
                run->appendPcm(pcm.data(), kPacketFrames);
                written += kPacketFrames;
                next += kPacketNs;
            }
            run->reconcile();
            run->serviceUntil(std::chrono::steady_clock::now() + std::chrono::microseconds(1800), 32);
        }
    }
    void receiverAudioBlock(int s, const float* pcm, int frames) override
    {
        downstream->receiverAudioBlock(s, pcm, frames);
    }
    void receiverAudioStopped(int s, const QString& r) override { downstream->receiverAudioStopped(s, r); }
};

struct Arrival {
    qint64 first = 0;
    qint64 last = 0;
    qint64 at = 0;
};

// TciBinaryFrame.cpp:101-136,182-188: LE header; native float32 memcpy.
// TciServer.cpp:3255: length is total interleaved sample count.
std::optional<Arrival> parseFrame(const QByteArray& f, qint64 at, int expectedFrames)
{
    if (expectedFrames <= 0 || quint64(expectedFrames) * 2 > std::numeric_limits<quint32>::max()
        || f.size() < 72 || f.size() > RemoteTciAudioStage::kMaxPayloadBytes
        || le32(f, 0) != 0u || le32(f, 4) != 48000u || le32(f, 8) != 3u
        || le32(f, 24) != 1u || le32(f, 28) != 2u) { return std::nullopt; }
    const quint32 samples = le32(f, 20);
    if (samples != quint64(expectedFrames) * 2 || samples < 2 || samples % 2 != 0
        || quint64(samples) * sizeof(float) != quint64(f.size() - 64)) { return std::nullopt; }
    Arrival a;
    qint64 previous = -1;
    for (quint32 sample = 0; sample < samples; sample += 2) {
        qint64 index = 0;
        if (!decode(f.constData() + 64 + qsizetype(sample) * sizeof(float), index)
            || (previous >= 0 && index != previous + 1)) { return std::nullopt; }
        if (sample == 0) { a.first = index; }
        previous = index;
    }
    a.last = previous;
    a.at = at;
    return a;
}

struct FrameAccounting {
    qint64 highest = -1;
    qint64 gaps = 0;
    qint64 overlaps = 0;
    void observe(const Arrival& a, bool count)
    {
        if (count && highest >= 0) {
            if (a.first > highest + 1) { gaps += a.first - highest - 1; }
            if (a.first <= highest) { overlaps += std::min(a.last, highest) - a.first + 1; }
        }
        highest = std::max(highest, a.last);
    }
};

class App : public QObject {
    Q_OBJECT
public:
    explicit App(QStringList setup, int blockFrames)
        : m_setup(std::move(setup)), m_blockFrames(blockFrames) {}
    std::mutex mutex;
    std::vector<Arrival> arrivals;
    std::atomic<bool> started{false};
    std::size_t invalidFrames = 0;
public slots:
    void open(quint16 port)
    {
        m_socket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
        connect(m_socket, &QWebSocket::connected, this, [this] {
            for (const QString& line : m_setup) { m_socket->sendTextMessage(line); }
            m_socket->sendTextMessage(QStringLiteral("audio_start:0;"));
        });
        connect(m_socket, &QWebSocket::textMessageReceived, this, [this](const QString& t) {
            if (t.contains(QStringLiteral("audio_start:0"))) { started = true; }
        });
        connect(m_socket, &QWebSocket::binaryMessageReceived, this, [this](const QByteArray& f) {
            const qint64 at = nowNs();
            const auto arrival = parseFrame(f, at, m_blockFrames);
            std::lock_guard<std::mutex> lock(mutex);
            if (!arrival) { ++invalidFrames; return; }
            arrivals.push_back(*arrival);
        });
        m_socket->open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(port)));
    }
    void close() { if (m_socket) { m_socket->close(); } }
private:
    QStringList m_setup;
    int m_blockFrames;
    QWebSocket* m_socket = nullptr;
};

double pct(std::vector<double> v, double p)
{
    if (v.empty()) { return std::numeric_limits<double>::quiet_NaN(); }
    std::sort(v.begin(), v.end());
    const std::size_t i = std::min(v.size() - 1, std::size_t(p * double(v.size() - 1) + 0.5));
    return v[i];
}

}  // namespace

class TmpTciLatency : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        AppSettings::setProfileOverride(QStringLiteral("tmp-tci-latency-%1")
                                            .arg(QCoreApplication::applicationPid()));
        AppSettings::instance().clear();
    }

    void malformedFrameAndNoData()
    {
        QVERIFY(!parseFrame(QByteArray(71, '\0'), 0, 1));
        QVERIFY(!parseFrame(QByteArray(RemoteTciAudioStage::kMaxPayloadBytes + 1, '\0'), 0, 1));
        QVERIFY(std::isnan(pct({}, 0.5)));
    }

    void expectedBlockSize()
    {
        float samples[4];
        encode(0, samples);
        encode(1, samples + 2);
        const QByteArray frame = TciBinaryFrame::buildStreamPayload(0, 48000, 3, 4, 1, 2, samples);
        QVERIFY(parseFrame(frame, 0, 2));
        QVERIFY(!parseFrame(frame, 0, 1));
    }

    void reorderedFramesKeepHighWatermark()
    {
        FrameAccounting frames;
        frames.observe({1900, 2000, 0}, false);
        frames.observe({0, 99, 0}, true);
        QCOMPARE(frames.overlaps, qint64(100));
        QCOMPARE(frames.highest, qint64(2000));
        frames.observe({2001, 2100, 0}, true);
        QCOMPARE(frames.gaps, qint64(0));
        frames.observe({2110, 2120, 0}, true);
        QCOMPARE(frames.gaps, qint64(9));
    }

    void measure()
    {
        const int steadyMs = qEnvironmentVariableIntValue("LAT_STEADY_MS") > 0
            ? qEnvironmentVariableIntValue("LAT_STEADY_MS") : 20000;
        const int stalls = qEnvironmentVariableIntValue("LAT_STALLS") > 0
            ? qEnvironmentVariableIntValue("LAT_STALLS") : 8;
        RadioModel remote(RadioModel::Role::Remote);
        std::unique_ptr<Generator> gen;
        TciServer tci(&remote);
        TciServer::RemoteReceiverAudio source;
        source.request = [&gen](int sliceId, IReceiverPcmSink* sink) {
            if (gen) {
                std::printf("NOTE second request for slice %d\n", sliceId);
                return gen->stage;
            }
            gen = std::make_unique<Generator>(sliceId, sink);
            return gen->stage;
        };
        source.release = [](int, IReceiverPcmSink*) {};
        source.unavailableReason = QStringLiteral("unavailable");
        tci.setRemoteReceiverAudio(source);
        std::vector<std::unique_ptr<QThread>> threads;
        std::vector<App*> apps;
        struct Cleanup {
            TciServer& server;
            std::unique_ptr<Generator>& generator;
            std::vector<std::unique_ptr<QThread>>& threads;
            std::vector<App*>& apps;
            ~Cleanup()
            {
                if (generator) { generator->finish(); }
                for (std::size_t i = 0; i < threads.size(); ++i) {
                    if (threads[i]->isRunning()) {
                        QMetaObject::invokeMethod(apps[i], "close", Qt::BlockingQueuedConnection);
                        threads[i]->quit();
                        threads[i]->wait();
                    }
                }
                server.stop();
                server.setRemoteReceiverAudio({});
                generator.reset();
            }
        } cleanup{tci, gen, threads, apps};
        QVERIFY(tci.start(0));

        struct AppSpec { const char* name; int blockFrames; QStringList setup; };
        const std::vector<AppSpec> specs = {
            {"A 48k 2048 (default)", 2048, {}},
            {"B 48k 480", 480, {QStringLiteral("audio_stream_samples:480;")}},
        };
        for (const auto& spec : specs) {
            auto thread = std::make_unique<QThread>();
            auto* app = new App(spec.setup, spec.blockFrames);
            app->moveToThread(thread.get());
            connect(thread.get(), &QThread::finished, app, &QObject::deleteLater);
            thread->start();
            QMetaObject::invokeMethod(app, "open", Qt::QueuedConnection, Q_ARG(quint16, tci.port()));
            apps.push_back(app);
            threads.push_back(std::move(thread));
        }
        for (App* app : apps) { QTRY_VERIFY_WITH_TIMEOUT(app->started.load(), 5000); }
        QTest::qWait(2000);   // warm up
        const qint64 steadyFrom = nowNs();
        QTest::qWait(steadyMs);
        const qint64 steadyTo = nowNs();
        std::vector<std::pair<qint64, qint64>> stallWindows;
        for (int s = 0; s < stalls; ++s) {
            QTest::qWait(1500);
            const qint64 a = nowNs();
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            const qint64 b = nowNs();
            stallWindows.push_back({a, b});
        }
        QTest::qWait(1500);
        // Stop the receiver first so its packet times are final.
        QVERIFY(gen);
        gen->finish();
        const std::vector<qint64> appendAt = gen->appendAt;
        std::vector<std::vector<Arrival>> got;
        std::vector<std::size_t> invalidFrames;
        for (App* app : apps) {
            std::lock_guard<std::mutex> lock(app->mutex);
            got.push_back(app->arrivals);
            invalidFrames.push_back(app->invalidFrames);
        }

        double l[3] = {0, 0, 0};
        getloadavg(l, 3);
        std::printf("RESULT load1=%.2f load5=%.2f\n", l[0], l[1]);
        const QString evidenceDir = qEnvironmentVariable("LAT_EVIDENCE_DIR");
        const bool exportRequested = qEnvironmentVariableIsSet("LAT_EVIDENCE_DIR");
        QJsonArray evidenceApps;
        std::size_t emptySteadyApps = 0;
        std::size_t emptyStallWindows = 0;
        std::size_t totalInvalidFrames = 0;
        std::size_t totalMissingAge = 0;
        std::size_t totalNegativeAge = 0;
        std::size_t totalNonmonotonicAge = 0;
        for (std::size_t i = 0; i < specs.size(); ++i) {
            const auto& arr = got[i];
            std::size_t missingAge = 0;
            std::size_t negativeAge = 0;
            std::size_t nonmonotonicAge = 0;
            const auto agesMs = [&](const Arrival& a) -> std::optional<std::pair<double, double>> {
                if (a.first < 0 || a.last < a.first) { ++missingAge; return std::nullopt; }
                const std::size_t first = std::size_t(a.first / kPacketFrames);
                const std::size_t last = std::size_t(a.last / kPacketFrames);
                if (first >= appendAt.size() || last >= appendAt.size()) { ++missingAge; return std::nullopt; }
                if (appendAt[first] > appendAt[last]) { ++nonmonotonicAge; return std::nullopt; }
                if (appendAt[last] > a.at) { ++negativeAge; return std::nullopt; }
                // Convert before subtraction to avoid signed integer overflow.
                return std::pair<double, double>{(double(a.at) - double(appendAt[first])) / 1e6,
                                                 (double(a.at) - double(appendAt[last])) / 1e6};
            };
            std::vector<double> firstAges;
            std::vector<double> lastAges;
            FrameAccounting steadyFrames;
            for (const Arrival& a : arr) {
                const bool inWindow = a.at >= steadyFrom && a.at < steadyTo;
                if (a.at >= steadyTo) { break; }
                steadyFrames.observe(a, inWindow);
                if (inWindow) {
                    const auto ages = agesMs(a);
                    if (ages) { firstAges.push_back(ages->first); lastAges.push_back(ages->second); }
                }
            }
            const double p99 = pct(lastAges, 0.99);
            std::printf("RESULT app=\"%s\" steady blocks=%zu no_data=%d first_age_ms_p50=%.2f first_age_ms_p99=%.2f first_age_ms_max=%.2f "
                        "last_age_ms_p50=%.2f last_age_ms_p99=%.2f last_age_ms_max=%.2f inferredArrivalGapFrames=%lld overlapOrReorderedFrames=%lld invalidFrames=%zu\n",
                        specs[i].name, lastAges.size(), lastAges.empty() ? 1 : 0, pct(firstAges, 0.5), pct(firstAges, 0.99),
                        pct(firstAges, 1.0), pct(lastAges, 0.5), p99, pct(lastAges, 1.0),
                        (long long)steadyFrames.gaps, (long long)steadyFrames.overlaps, invalidFrames[i]);
            std::vector<double> crossings;
            std::vector<double> peaks;
            std::size_t missingCrossings = 0;
            qint64 stallGaps = 0;
            qint64 stallOverlaps = 0;
            for (const auto& [a0, b0] : stallWindows) {
                std::optional<double> crossing;
                std::optional<double> peak;
                FrameAccounting frames;
                for (const Arrival& a : arr) {
                    if (a.at > b0 + 1'000'000'000) { break; }
                    frames.observe(a, a.at >= a0 - 100'000'000);
                    if (a.at < b0) { continue; }
                    const auto ages = agesMs(a);
                    if (!ages) { continue; }
                    const double age = ages->second;
                    peak = std::max(peak.value_or(age), age);
                    if (!crossing && std::isfinite(p99) && age <= std::max(p99, 1.0)) {
                        crossing = double(a.at - b0) / 1e6;
                    }
                }
                stallGaps += frames.gaps;
                stallOverlaps += frames.overlaps;
                if (crossing) { crossings.push_back(*crossing); } else { ++missingCrossings; }
                if (peak) { peaks.push_back(*peak); }
            }
            std::printf("RESULT app=\"%s\" stalls=%zu firstCrossing_notSustained_ms_p50=%.1f firstCrossing_ms_max=%.1f missing=%zu "
                        "last_age_peak_ms_p50=%.1f max=%.1f peakSamples=%zu inferredArrivalGapFrames=%lld overlapOrReorderedFrames=%lld missingAgeEvaluations=%zu negativeAgeEvaluations=%zu nonmonotonicAgeEvaluations=%zu\n",
                        specs[i].name, stallWindows.size(), pct(crossings, 0.5), pct(crossings, 1.0),
                        missingCrossings, pct(peaks, 0.5), pct(peaks, 1.0), peaks.size(),
                        (long long)stallGaps, (long long)stallOverlaps, missingAge, negativeAge, nonmonotonicAge);
            if (firstAges.empty() || lastAges.empty()) { ++emptySteadyApps; }
            emptyStallWindows += stallWindows.size() - peaks.size();
            totalInvalidFrames += invalidFrames[i];
            totalMissingAge += missingAge;
            totalNegativeAge += negativeAge;
            totalNonmonotonicAge += nonmonotonicAge;
            if (exportRequested) {
                QJsonArray rawArrivals;
                for (const Arrival& arrival : arr) {
                    rawArrivals.append(QJsonObject{
                        {QStringLiteral("firstFrame"), QString::number(arrival.first)},
                        {QStringLiteral("lastFrame"), QString::number(arrival.last)},
                        {QStringLiteral("callbackNs"), QString::number(arrival.at)}});
                }
                evidenceApps.append(QJsonObject{
                    {QStringLiteral("name"), QString::fromUtf8(specs[i].name)},
                    {QStringLiteral("blockFrames"), specs[i].blockFrames},
                    {QStringLiteral("arrivals"), rawArrivals},
                    {QStringLiteral("invalidFrames"), QString::number(qulonglong(invalidFrames[i]))},
                    {QStringLiteral("missingAgeEvaluations"), QString::number(qulonglong(missingAge))},
                    {QStringLiteral("negativeAgeEvaluations"), QString::number(qulonglong(negativeAge))},
                    {QStringLiteral("nonmonotonicAgeEvaluations"), QString::number(qulonglong(nonmonotonicAge))},
                    {QStringLiteral("emptySteady"), firstAges.empty() || lastAges.empty()},
                    {QStringLiteral("emptyStallWindows"), QString::number(qulonglong(stallWindows.size() - peaks.size()))},
                    {QStringLiteral("missingCrossings"), QString::number(qulonglong(missingCrossings))}});
            }
        }
        const bool baseValid = emptySteadyApps == 0 && emptyStallWindows == 0 && totalInvalidFrames == 0
            && totalMissingAge == 0 && totalNegativeAge == 0 && totalNonmonotonicAge == 0;
        bool exportSucceeded = false;
        QString exportError;
        if (exportRequested) {
            QJsonArray packets;
            for (std::size_t packet = 0; packet < appendAt.size(); ++packet) {
                packets.append(QJsonObject{
                    {QStringLiteral("packetIndex"), QString::number(qulonglong(packet))},
                    {QStringLiteral("appendNs"), QString::number(appendAt[packet])}});
            }
            QJsonArray windows;
            for (const auto& [start, end] : stallWindows) {
                windows.append(QJsonObject{{QStringLiteral("startNs"), QString::number(start)},
                                           {QStringLiteral("endNs"), QString::number(end)}});
            }
            const QJsonObject evidence{
                {QStringLiteral("clock"), QStringLiteral("same-process std::chrono::steady_clock")},
                {QStringLiteral("metric"), QStringLiteral("packet availability: appendPcm to WebSocket callback; not end-to-end")},
                {QStringLiteral("appendTimestampPoint"), QStringLiteral("immediately before appendPcm, after receiverAudioBlock")},
                {QStringLiteral("packetFrames"), kPacketFrames},
                {QStringLiteral("sampleRateHz"), 48000},
                {QStringLiteral("percentileMethod"), QStringLiteral("sorted index round(p * (count - 1))")},
                {QStringLiteral("firstCrossingThreshold"), QStringLiteral("max(steady last-packet age p99, 1 ms); not sustained recovery")},
                {QStringLiteral("postStallWindowNs"), QStringLiteral("1000000000")},
                {QStringLiteral("preStallGapWindowNs"), QStringLiteral("100000000")},
                {QStringLiteral("steadyMs"), steadyMs},
                {QStringLiteral("stalls"), stalls},
                {QStringLiteral("stallDurationMs"), 200},
                {QStringLiteral("steadyFromNs"), QString::number(steadyFrom)},
                {QStringLiteral("steadyToNs"), QString::number(steadyTo)},
                {QStringLiteral("stallWindows"), windows},
                {QStringLiteral("packets"), packets},
                {QStringLiteral("apps"), evidenceApps},
                {QStringLiteral("baseValid"), baseValid},
                {QStringLiteral("validityScope"), QStringLiteral("baseValid covers observations; console aggregateValid also requires requested export success")},
                {QStringLiteral("emptySteadyApps"), QString::number(qulonglong(emptySteadyApps))},
                {QStringLiteral("emptyStallWindows"), QString::number(qulonglong(emptyStallWindows))},
                {QStringLiteral("invalidFrames"), QString::number(qulonglong(totalInvalidFrames))},
                {QStringLiteral("missingAgeEvaluations"), QString::number(qulonglong(totalMissingAge))},
                {QStringLiteral("negativeAgeEvaluations"), QString::number(qulonglong(totalNegativeAge))},
                {QStringLiteral("nonmonotonicAgeEvaluations"), QString::number(qulonglong(totalNonmonotonicAge))}};
            if (evidenceDir.isEmpty() || !QDir(evidenceDir).exists()) {
                exportError = QStringLiteral("evidence directory is empty or does not exist");
            } else {
                QFile file(QDir(evidenceDir).filePath(QStringLiteral("tmp-tci-latency-evidence.json")));
                if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
                    exportError = file.errorString();
                } else {
                    const QByteArray bytes = QJsonDocument(evidence).toJson(QJsonDocument::Compact);
                    if (file.write(bytes) != bytes.size() || !file.flush()) {
                        exportError = file.errorString();
                    } else {
                        exportSucceeded = true;
                    }
                }
            }
        }
        const bool valid = baseValid && (!exportRequested || exportSucceeded);
        std::printf("RESULT aggregateValid=%d emptySteadyApps=%zu emptyStallWindows=%zu invalidFrames=%zu "
                    "missingAgeEvaluations=%zu negativeAgeEvaluations=%zu nonmonotonicAgeEvaluations=%zu "
                    "exportRequested=%d exportSucceeded=%d exportError=\"%s\"\n",
                    valid ? 1 : 0, emptySteadyApps, emptyStallWindows, totalInvalidFrames,
                    totalMissingAge, totalNegativeAge, totalNonmonotonicAge,
                    exportRequested ? 1 : 0, exportSucceeded ? 1 : 0, qPrintable(exportError));
        std::fflush(stdout);
        QVERIFY2(valid, "Measurement contains invalid or missing timed data; see RESULT diagnostics");
    }
};

QTEST_MAIN(TmpTciLatency)
#include "tmp_tci_latency.moc"
