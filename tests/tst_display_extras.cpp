// no-port-check: NereusSDR-original. iPhone app Task 20 (R-IOS-27).
// =================================================================
// tests/tst_display_extras.cpp  (NereusSDR)
// =================================================================
//
// The display extras the Core computes for an app (display extras v1):
//   - the station's extras equal what the desktop's SpectrumWidget computes
//     for the same recorded rows and settings, within 0.01 dB;
//   - the NSDX datagram, its bounds and its refusals;
//   - the subscription fields and their ranges;
//   - through a real StationServer session: an endpoint that asks gets an
//     NSDX datagram beside each NSDC frame, calibration and normalise move
//     the bins, and one that does not ask gets exactly the bytes the Core
//     sent before display extras (a golden recorded from 535dd412).
//   - displayExtrasVersion 2 (R-IOS-27, R-IOS-06): the clarity-retune
//     operation re-tunes that endpoint's Clarity as the desktop's Re-tune
//     button does, leaves the others alone, refuses what it cannot do, and
//     is not there for an older peer.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-04: Budget-enforced waterfall-only/no-peak-hold daemon
//               regressions. J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-09-25  J.J. Boyd / KG4VCF  R-IOS-27, R-IOS-06: clarity-retune
//                                    and displayExtrasVersion 2.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  Parity Task 19 (R-IOS-25):
//                                    recordStreamVersion and the record
//                                    streams. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-10-11  J.J. Boyd / KG4VCF  A level mode changed while keyed: the
//                                    new Clarity's key state and the
//                                    per-band AGC levels. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-10-11  J.J. Boyd / KG4VCF  calibrationMovesTheFramesBins compares
//                                    planes that hold nothing, with one
//                                    endpoint started before the other.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-10-23  J.J. Boyd / KG4VCF  A started display reads the band from
//                                    its first frame with data, at Log
//                                    Recursive and with placeholder wings;
//                                    on a noisy band, and with wings whose
//                                    data arrives late. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-10-23  J.J. Boyd / KG4VCF  The seed starts fast attack; the
//                                    noisy band at 1, 2 and 4 bins to a
//                                    pixel and 30, 650 and 2000 ms.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-10-23  J.J. Boyd / KG4VCF  The NF-AGC low level from the first
//                                    frame again; 30 ms at 1 and 2 bins to
//                                    a pixel. AI-assisted via Anthropic
//                                    Claude Code.
// =================================================================

#include <QtTest>

#include "core/AppSettings.h"
#include "core/ClarityController.h"
#include "core/FFTEngine.h"
#include "core/HpsdrModel.h"
#include "core/session/StationCapabilities.h"
#include "core/session/StationClient.h"
#include "core/session/SessionMessages.h"
#include "core/session/StationServer.h"
#include "core/session/media/DaemonMediaController.h"
#include "core/session/media/DisplayBudget.h"
#include "core/session/media/DisplayCodec.h"
#include "core/session/media/DisplayExtras.h"
#include "core/session/media/IMediaTransport.h"
#include "core/session/media/RemoteSpectrumContext.h"
#include "core/session/media/SpectrumEndpoint.h"
#include "core/settings/SettingsProxy.h"
#include "core/WidebandFftEngine.h"
#include "core/spectrum/WidebandDisplayReference.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/UpgradedCoreToken.h"
#include "OlderPeerDisplayRun.h"
#include "OperatorWording.h"
#include "gui/SpectrumWidget.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <numbers>
#include <random>

using namespace NereusSDR;

namespace {

constexpr char kConnectionId[] = "11111111-2222-4333-8444-555555555555";
constexpr char kOtherConnectionId[] = "11111111-2222-4333-8444-666666666666";
const QString kNotThisAppsDisplay = QStringLiteral("That display is not one this app opened.");
const QString kDisplayClosed = QStringLiteral("That display is no longer open on the Core.");
const QString kNotClarity =
    QStringLiteral("Clarity is not setting this display's waterfall levels.");

QJsonObject clarityRetune(quint32 endpointId, const char* connectionId = kConnectionId)
{
    return {{QStringLiteral("op"), QStringLiteral("clarity-retune")},
            {QStringLiteral("connectionId"), QLatin1String(connectionId)},
            {QStringLiteral("endpointId"), static_cast<qint64>(endpointId)}};
}

QJsonObject levels(const QString& mode)
{
    return {{QStringLiteral("waterfallLevels"),
             QJsonObject{{QStringLiteral("mode"), mode},
                         {QStringLiteral("lowDbm"), -122.0},
                         {QStringLiteral("highDbm"), -62.0},
                         {QStringLiteral("offsetDb"), 0}}}};
}

int countOf(const QSignalSpy& messages, const QString& op, quint32 endpointId)
{
    int count = 0;
    for (const QList<QVariant>& args : messages) {
        const QJsonObject message = args.at(0).toJsonObject();
        if (message.value(QStringLiteral("op")) == op
            && static_cast<quint32>(message.value(QStringLiteral("endpointId")).toInteger())
                == endpointId) {
            ++count;
        }
    }
    return count;
}
constexpr float kToleranceDb = 0.01f;

// ── A recorded spectrum sequence ────────────────────────────────────────

// A deterministic "recording": noise around -120 dBm, a steady carrier, a
// carrier that walks up the band, and one that keys on and off, so the
// blobs, the hold, the floor and the levels all have work to do.
QVector<QVector<float>> recordedRows(int frames, int pixels)
{
    QVector<QVector<float>> rows;
    quint32 seed = 0x2468ACE1U;
    const auto noise = [&seed] {
        seed = seed * 1664525U + 1013904223U;
        return static_cast<float>((seed >> 8) & 0xFFFF) / 65535.0f;
    };
    for (int frame = 0; frame < frames; ++frame) {
        QVector<float> row(pixels);
        for (int x = 0; x < pixels; ++x) {
            row[x] = -123.0f + 6.0f * noise();
        }
        row[30] = -60.0f;
        row[29] = -75.0f;
        row[31] = -74.0f;
        // A row narrower than the walker's range or the keyed carrier's
        // pixel leaves them out: writing past the row's end corrupted the
        // heap and crashed later tests at random (fix wave 71-76).
        if (pixels > 80) {
            const int walker = 60 + (frame / 4) % (pixels - 80);
            row[walker] = -70.0f - 5.0f * noise();
        }
        if (pixels > 200 && (frame / 40) % 2 == 0) {
            row[200] = -50.0f;
        }
        rows.append(row);
    }
    return rows;
}

// ── The Core end of a real session ──────────────────────────────────────

class FakeTransport final : public IMediaTransport {
public:
    explicit FakeTransport(QObject* parent = nullptr) : IMediaTransport(parent) {}
    bool start(const StartOptions& options) override
    {
        startOptions = options;
        started = true;
        return true;
    }
    void stop() override { started = readyState = false; }
    bool acceptDescription(const QString&, const QString&) override { return true; }
    bool acceptCandidate(const QString&, const QString&) override { return true; }
    bool sendDisplay(const QByteArray& bytes) override
    {
        if (!readyState) { return false; }
        displays.append(bytes);
        return true;
    }
    bool sendRtp(const QByteArray&) override { return readyState; }
    bool isReady() const override { return readyState; }
    void becomeReady() { readyState = true; emit ready(); }

    bool started{false};
    bool readyState{false};
    StartOptions startOptions{Role::Answerer, 0};
    QList<QByteArray> displays;
};

QVector<float> syntheticIq(int complexSamples, double cyclesPerSample)
{
    QVector<float> samples;
    samples.reserve(complexSamples * 2);
    for (int sample = 0; sample < complexSamples; ++sample) {
        const double phase = 2.0 * std::numbers::pi * cyclesPerSample * sample;
        samples.append(static_cast<float>(0.01 * std::cos(phase)));
        samples.append(static_cast<float>(0.01 * std::sin(phase)));
    }
    return samples;
}

QJsonObject plane()
{
    return {{QStringLiteral("detector"), static_cast<int>(SpectrumDetectorMode::Peak)},
            {QStringLiteral("averageMode"), -1},
            {QStringLiteral("averageAlpha"), 0.0}};
}

// A plane that keeps nothing from one frame to the next. plane() holds each
// pixel's maximum, so what an endpoint shows for a frame depends on every
// frame it was sent before. Two endpoints compared on one source frame
// need this one: one of them may have started a frame earlier.
QJsonObject planeWithoutHold()
{
    return {{QStringLiteral("detector"), static_cast<int>(SpectrumDetectorMode::Peak)},
            {QStringLiteral("averageMode"), 0},
            {QStringLiteral("averageAlpha"), 0.0}};
}

QJsonObject subscription(quint32 endpointId, int sliceId, double centreHz)
{
    return {{QStringLiteral("op"), QStringLiteral("subscribe")},
            {QStringLiteral("connectionId"), QLatin1String(kConnectionId)},
            {QStringLiteral("endpointId"), static_cast<qint64>(endpointId)},
            {QStringLiteral("revision"), 1},
            {QStringLiteral("sliceId"), sliceId},
            {QStringLiteral("tier"), QStringLiteral("wide")},
            {QStringLiteral("fftSize"), 1024},
            {QStringLiteral("windowType"), static_cast<int>(WindowFunction::Hann)},
            {QStringLiteral("centreHz"), centreHz},
            {QStringLiteral("spanHz"), 48000.0},
            {QStringLiteral("pixels"), 128},
            {QStringLiteral("fps"), 60},
            {QStringLiteral("framesPerLine"), 1},
            {QStringLiteral("trace"), plane()},
            {QStringLiteral("waterfall"), plane()},
            {QStringLiteral("minDbm"), -180.0},
            {QStringLiteral("maxDbm"), 0.0},
            {QStringLiteral("wideSpanFactor"), 0.0}};
}

QJsonObject everyExtra()
{
    return {{QStringLiteral("peakBlobs"),
             QJsonObject{{QStringLiteral("count"), 3}, {QStringLiteral("holdMs"), 500},
                         {QStringLiteral("fallDbPerSec"), 6.0},
                         {QStringLiteral("insideOnly"), false}}},
            {QStringLiteral("activePeakHold"),
             QJsonObject{{QStringLiteral("enabled"), true}, {QStringLiteral("holdMs"), 2000},
                         {QStringLiteral("fallDbPerSec"), 6.0}}},
            {QStringLiteral("noiseFloor"),
             QJsonObject{{QStringLiteral("enabled"), true}, {QStringLiteral("shiftDb"), 0.0},
                         {QStringLiteral("fastAttack"), true}}},
            {QStringLiteral("waterfallLevels"),
             QJsonObject{{QStringLiteral("mode"), QStringLiteral("agc")},
                         {QStringLiteral("lowDbm"), -122.0},
                         {QStringLiteral("highDbm"), -62.0},
                         {QStringLiteral("offsetDb"), 0}}},
            {QStringLiteral("normalize"), false},
            {QStringLiteral("calibrationOffsetDb"), 0.0},
            {QStringLiteral("averageTimeMs"), 30}};
}

QJsonObject withFields(QJsonObject base, const QJsonObject& fields)
{
    for (auto it = fields.constBegin(); it != fields.constEnd(); ++it) {
        base.insert(it.key(), it.value());
    }
    return base;
}

QJsonObject messageFor(const QSignalSpy& messages, const QString& op, quint32 endpointId)
{
    for (auto it = messages.crbegin(); it != messages.crend(); ++it) {
        const QJsonObject message = it->at(0).toJsonObject();
        if (message.value(QStringLiteral("op")) == op
            && static_cast<quint32>(message.value(QStringLiteral("endpointId")).toInteger())
                == endpointId) {
            return message;
        }
    }
    return {};
}

struct Harness {
    QTemporaryDir directory;
    AppSettings settings;
    RadioModel radio;
    StationServer server;
    RadioModel remote{RadioModel::Role::Remote};
    SettingsProxy settingsProxy;
    StationClient client{&remote, &settingsProxy};
    QPointer<FakeTransport> mediaTransport;
    DaemonMediaController controller;
    int sliceId{-1};
    int streamIndex{-1};

    Harness()
        : settings(directory.filePath(QStringLiteral("station.settings")))
        , server(&radio, settings, NereusSDR::Test::seedUpgradedCoreToken(directory.path()))
        , controller(&server, &radio, nullptr, [this](QObject* parent) -> IMediaTransport* {
            mediaTransport = new FakeTransport(parent);
            return mediaTransport;
        })
    {
        radio.setBoardForTest(HPSDRHW::Saturn);
        radio.configureStreamPool(/*userDdcCount=*/5, /*maxSlices=*/5,
                                  /*defaultRateHz=*/192000);
        radio.setConnectionStateForTest(ConnectionState::Connected);
        sliceId = radio.addSlice();
        SliceModel* const slice = radio.sliceById(sliceId);
        streamIndex = slice ? slice->streamIndex() : -1;
        server.setMediaEnabled(true);
    }

    bool establishSession()
    {
        auto* stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        return QTest::qWaitFor([this] {
            return server.mediaAvailable() && client.mediaAvailable();
        }, 10'000);
    }

    bool startReadyPeer()
    {
        if (!client.sendMediaControl({{QStringLiteral("op"), QStringLiteral("start")},
                                      {QStringLiteral("connectionId"),
                                       QLatin1String(kConnectionId)}},
                                     client.sessionEpoch())) {
            return false;
        }
        if (!QTest::qWaitFor([this] { return !mediaTransport.isNull(); }, 10'000)) {
            return false;
        }
        mediaTransport->becomeReady();
        return true;
    }

    void feedRadio(double cyclesPerSample = 0.125)
    {
        QMetaObject::invokeMethod(&radio, "rawIqDataForStream", Qt::DirectConnection,
                                  Q_ARG(int, streamIndex),
                                  Q_ARG(QVector<float>, syntheticIq(1026, cyclesPerSample)));
    }

    void finish() { client.disconnectFromStation(QStringLiteral("test complete")); }
};

DisplayCodecContext contextFrom(const QJsonObject& message)
{
    DisplayCodecContext context;
    context.endpointId = static_cast<quint32>(message.value(QStringLiteral("endpointId")).toInteger());
    context.contextGeneration =
        static_cast<quint32>(message.value(QStringLiteral("contextGeneration")).toInteger());
    context.minDbm = static_cast<float>(message.value(QStringLiteral("minDbm")).toDouble());
    context.maxDbm = static_cast<float>(message.value(QStringLiteral("maxDbm")).toDouble());
    context.traceSamples =
        static_cast<quint16>(message.value(QStringLiteral("traceSamples")).toInt());
    context.waterfallSamples =
        static_cast<quint16>(message.value(QStringLiteral("waterfallSamples")).toInt());
    context.wideSamples = static_cast<quint16>(message.value(QStringLiteral("wideSamples")).toInt());
    return context;
}

QList<QByteArray> withMagic(const QList<QByteArray>& displays, const QByteArray& magic)
{
    QList<QByteArray> found;
    for (const QByteArray& bytes : displays) {
        if (bytes.startsWith(magic)) { found.append(bytes); }
    }
    return found;
}

quint32 sequenceOfNsdc(const QByteArray& bytes)
{
    return (static_cast<quint32>(static_cast<quint8>(bytes.at(16))) << 24)
        | (static_cast<quint32>(static_cast<quint8>(bytes.at(17))) << 16)
        | (static_cast<quint32>(static_cast<quint8>(bytes.at(18))) << 8)
        | static_cast<quint32>(static_cast<quint8>(bytes.at(19)));
}

DisplayCodecContext smallContext()
{
    DisplayCodecContext context;
    context.endpointId = 9;
    context.contextGeneration = 4;
    context.minDbm = -180.0f;
    context.maxDbm = 0.0f;
    context.traceSamples = 16;
    context.waterfallSamples = 16;
    return context;
}

DisplayExtrasFrame fullFrame(const DisplayCodecContext& context)
{
    DisplayExtrasFrame frame;
    frame.endpointId = context.endpointId;
    frame.contextGeneration = context.contextGeneration;
    frame.encoderSequence = 77;
    frame.peakBlobs = QVector<DisplayExtrasBlob>{{3, -61.5f}, {12, -80.25f}};
    QVector<float> hold(context.traceSamples);
    for (int i = 0; i < hold.size(); ++i) { hold[i] = -120.0f + static_cast<float>(i); }
    frame.peakHoldDbm = hold;
    frame.noiseFloorDbm = -118.75f;
    frame.waterfallLevelsDbm = std::make_pair(-131.0f, -58.5f);
    return frame;
}

} // namespace

class TstDisplayExtras : public QObject {
    Q_OBJECT
private:
    // Two endpoints on one source frame, the same but for `normalize`:
    // the mean difference of their traces (normalised minus plain).
    double normaliseShiftFor(const QJsonObject& traceFields)
    {
        Harness harness;
        if (!harness.establishSession()) { return 1000.0; }
        QSignalSpy controls(&harness.client, &StationClient::mediaControlReceived);
        if (!harness.startReadyPeer()) { return 1000.0; }
        const double centreHz = harness.radio.streamCentreHz(harness.streamIndex);
        const QJsonObject wide{{QStringLiteral("minDbm"), -400.0},
                               {QStringLiteral("maxDbm"), 100.0}};
        const QJsonObject base = withFields(wide, traceFields);
        harness.client.sendMediaControl(
            withFields(withFields(subscription(7, harness.sliceId, centreHz), base),
                       {{QStringLiteral("normalize"), true}}),
            harness.client.sessionEpoch());
        harness.client.sendMediaControl(
            withFields(subscription(8, harness.sliceId, centreHz), base),
            harness.client.sessionEpoch());
        if (!QTest::qWaitFor([&] { return harness.controller.activeEndpointCount() == 2; },
                             10'000)) {
            return 1000.0;
        }
        QMap<quint32, QMap<quint64, DisplayCodecFrame>> frames;
        QMap<quint32, DisplayCodecDecoder> decoders;
        int cursor = 0;
        quint64 shared = 0;
        const auto sameFrame = [&] {
            harness.feedRadio(0.15);
            QTest::qWait(20);
            const QList<QByteArray>& displays = harness.mediaTransport->displays;
            for (; cursor < displays.size(); ++cursor) {
                const QByteArray& bytes = displays.at(cursor);
                if (bytes.startsWith("NSDX")) { continue; }
                const quint32 endpoint = static_cast<quint8>(bytes.at(11));
                const auto decoded = decoders[endpoint].decode(bytes);
                if (decoded.disposition == DisplayCodecDisposition::Accepted) {
                    frames[endpoint].insert(decoded.frame.producerTimestamp, decoded.frame);
                }
            }
            for (auto it = frames[7].cbegin(); it != frames[7].cend(); ++it) {
                if (frames[8].contains(it.key())) {
                    shared = it.key();
                    return true;
                }
            }
            return false;
        };
        if (!QTest::qWaitFor(sameFrame, 30'000)) { return 1000.0; }
        const DisplayCodecFrame normalised = frames[7].value(shared);
        const DisplayCodecFrame plain = frames[8].value(shared);
        double sum = 0.0;
        int n = 0;
        for (int x = 0; x < plain.traceDbm.size() && x < normalised.traceDbm.size(); ++x) {
            if (plain.traceDbm.at(x) < -380.0f || plain.traceDbm.at(x) > 80.0f) { continue; }
            sum += normalised.traceDbm.at(x) - plain.traceDbm.at(x);
            ++n;
        }
        harness.finish();
        return n > 100 ? sum / n : 1000.0;
    }

private slots:
    void initTestCase()
    {
        qRegisterMetaType<QVector<float>>();
        qRegisterMetaType<MediaSourceKey>();
    }

    // ── Parity with the desktop ─────────────────────────────────────────

    void stationExtrasEqualTheDesktopsForARecordedSequence_data()
    {
        QTest::addColumn<QString>("levelMode");
        QTest::addColumn<bool>("insideOnly");
        QTest::addColumn<int>("holdMs");
        QTest::addColumn<double>("fall");
        QTest::addColumn<int>("minimumBlobs");
        // The passband row sees only the walking carrier's visits (a blob
        // needs a 10 dB rise and fall, which the noise never makes).
        QTest::newRow("agc, hold and decay") << QStringLiteral("agc") << false << 500 << 6.0
                                             << 300;
        QTest::newRow("manual, hard cut") << QStringLiteral("manual") << false << 400 << 0.0
                                          << 300;
        QTest::newRow("noise floor agc, no hold, passband")
            << QStringLiteral("noiseFloorAgc") << true << 0 << 0.0 << 10;
    }

    void stationExtrasEqualTheDesktopsForARecordedSequence()
    {
        QFETCH(QString, levelMode);
        QFETCH(bool, insideOnly);
        QFETCH(int, holdMs);
        QFETCH(double, fall);
        QFETCH(int, minimumBlobs);
        constexpr int kPixels = 256;
        constexpr int kFrames = 300;
        constexpr int kFps = 25;
        constexpr double kCentreHz = 14100000.0;
        constexpr double kSpanHz = 48000.0;
        constexpr double kSliceHz = 14095000.0;
        constexpr int kFilterLowHz = 100;
        constexpr int kFilterHighHz = 3000;
        const QVector<QVector<float>> rows = recordedRows(kFrames, kPixels);

        // The desktop, as a remote window runs it on received rows.
        SpectrumWidget desktop;
        desktop.setDisplayFps(kFps);
        // The recorded rows go to both sides as the Core's trace; nothing
        // averages them here, so the window asks for no averaging (at Log
        // Recursive it would take the Core's starting value out of them).
        desktop.setSpectrumAveraging(SpectrumAveraging::None);
        desktop.setPeakBlobsEnabled(true);
        desktop.setPeakBlobsCount(5);
        desktop.setPeakBlobsInsideFilterOnly(insideOnly);
        desktop.setPeakBlobsHoldEnabled(holdMs > 0);
        if (holdMs > 0) { desktop.setPeakBlobsHoldMs(holdMs); }
        desktop.setPeakBlobsHoldDrop(fall > 0.0);
        if (fall > 0.0) { desktop.setPeakBlobsFallDbPerSec(fall); }
        desktop.setActivePeakHoldEnabled(true);
        desktop.setActivePeakHoldDurationMs(2000);
        desktop.setActivePeakHoldDropDbPerSec(6.0);
        desktop.setNFShiftDbm(2.0f);
        desktop.setClarityActive(false);
        // NF-AGC is an option under AGC (S7): the desktop has both on where
        // the station link says `noiseFloorAgc`.
        desktop.setWfAgcEnabled(levelMode == QLatin1String("agc")
                                || levelMode == QLatin1String("noiseFloorAgc"));
        desktop.setWaterfallNFAGCEnabled(levelMode == QLatin1String("noiseFloorAgc"));
        desktop.setWaterfallAGCOffsetDb(-4);
        desktop.setWfLowThreshold(-125.0f);
        desktop.setWfHighThreshold(-65.0f);
        desktop.setVfoFrequency(kSliceHz);
        desktop.setFilterOffset(kFilterLowHz, kFilterHighHz);
        SpectrumEndpointContext context;
        context.codec = {41, 1, -180.0f, 0.0f, kPixels, kPixels, 0};
        context.exactCentreHz = kCentreHz;
        context.exactSpanHz = kSpanHz;
        desktop.setRemoteSpectrumContext(context, kCentreHz, 192000);

        // The Core, for the same settings.
        DisplayExtrasRequest request;
        request.peakBlobs = DisplayExtrasRequest::PeakBlobs{5, holdMs, fall, insideOnly};
        request.activePeakHold = DisplayExtrasRequest::ActivePeakHold{true, 2000, 6.0};
        request.noiseFloor = DisplayExtrasRequest::NoiseFloor{true, 2.0};
        DisplayExtrasRequest::WaterfallLevels levels;
        levels.mode = levelMode == QLatin1String("agc") ? WaterfallLevelMode::Agc
            : levelMode == QLatin1String("noiseFloorAgc") ? WaterfallLevelMode::NoiseFloorAgc
                                                           : WaterfallLevelMode::Manual;
        levels.lowDbm = -125.0;
        levels.highDbm = -65.0;
        levels.offsetDb = -4;
        request.waterfallLevels = levels;
        DisplayExtrasProcessor station(request);

        int blobsCompared = 0;
        int holdRowsCompared = 0;
        // The desktop's display clock is the station's: the noise floor's
        // fast attack, which the first frame starts, ends on time.
        qint64 nowMs = 0;
        desktop.setDisplayClockForTest([&nowMs] { return nowMs; });
        for (int i = 0; i < kFrames; ++i) {
            nowMs = static_cast<qint64>(i) * (1000 / kFps);
            DisplayCodecFrame frame;
            frame.context = context.codec;
            frame.encoderSequence = static_cast<quint32>(i);
            frame.traceDbm = rows.at(i);
            frame.waterfallDbm = rows.at(i);
            frame.waterfallAdvance = true;
            QVERIFY(desktop.updateRemoteSpectrum(frame));
            desktop.composeWaterfallActiveThresholds(frame.waterfallDbm);

            DisplayExtrasInputs inputs;
            inputs.fps = kFps;
            inputs.nowMs = nowMs;
            inputs.centreHz = kCentreHz;
            inputs.spanHz = kSpanHz;
            inputs.sliceHz = kSliceHz;
            inputs.filterLowHz = kFilterLowHz;
            inputs.filterHighHz = kFilterHighHz;
            const DisplayExtrasFrame out = station.process(frame, inputs);

            // Blobs: the same peaks at the same pixels.
            QVector<PeakBlob> desktopBlobs;
            for (const PeakBlob& blob : desktop.peakBlobsForTest()) {
                if (blob.enabled) { desktopBlobs.append(blob); }
            }
            QVERIFY(out.peakBlobs.has_value());
            QCOMPARE(out.peakBlobs->size(), desktopBlobs.size());
            for (int b = 0; b < desktopBlobs.size(); ++b) {
                QCOMPARE(int(out.peakBlobs->at(b).pixel), desktopBlobs.at(b).binIndex);
                QVERIFY(std::abs(out.peakBlobs->at(b).dbm - desktopBlobs.at(b).max_dBm)
                        <= kToleranceDb);
                ++blobsCompared;
            }
            // The active peak hold row: both wait out the same 500 ms display
            // delay after the first frame's reset, then agree.
            const QVector<float>& desktopHold = desktop.activePeakHoldPeaksForTest();
            QCOMPARE(out.peakHoldDbm.has_value(), desktop.activePeakHoldActive());
            if (out.peakHoldDbm.has_value()) {
                ++holdRowsCompared;
                QCOMPARE(out.peakHoldDbm->size(), desktopHold.size());
                for (int x = 0; x < desktopHold.size(); ++x) {
                    QVERIFY(std::abs(out.peakHoldDbm->at(x) - desktopHold.at(x)) <= kToleranceDb);
                }
            }
            // The noise-floor line, where the desktop draws it (lerp + shift).
            QVERIFY(out.noiseFloorDbm.has_value());
            QVERIFY(std::abs(*out.noiseFloorDbm
                             - (desktop.nfLerpAverageForTest() + desktop.nfShiftDbm()))
                    <= kToleranceDb);
            // The waterfall's levels in force.
            QVERIFY(out.waterfallLevelsDbm.has_value());
            QVERIFY(std::abs(out.waterfallLevelsDbm->first - desktop.wfActiveLowThreshold())
                    <= kToleranceDb);
            QVERIFY(std::abs(out.waterfallLevelsDbm->second - desktop.wfActiveHighThreshold())
                    <= kToleranceDb);
        }
        QVERIFY2(blobsCompared >= minimumBlobs, qPrintable(QString::number(blobsCompared)));
        QVERIFY2(holdRowsCompared >= kFrames / 2, qPrintable(QString::number(holdRowsCompared)));
    }

    void averagingConstantIsTheDesktopsForTheSameTimeAndRate()
    {
        SpectrumWidget desktop;
        desktop.setDisplayFps(25);
        desktop.setSpectrumAverageTimeMs(120);
        desktop.setWaterfallAverageTimeMs(700);
        QCOMPARE(averageAlphaForTimeMs(120, 25), desktop.spectrumAverageAlpha());
        QCOMPARE(averageAlphaForTimeMs(700, 25), desktop.waterfallAverageAlpha());
    }

    void calibrationAndNormaliseShiftEveryExtra()
    {
        DisplayExtrasRequest plain;
        plain.peakBlobs = DisplayExtrasRequest::PeakBlobs{};
        plain.activePeakHold = DisplayExtrasRequest::ActivePeakHold{true, 2000, 6.0};
        plain.noiseFloor = DisplayExtrasRequest::NoiseFloor{true, 0.0};
        plain.waterfallLevels = DisplayExtrasRequest::WaterfallLevels{};
        DisplayExtrasRequest shifted = plain;
        shifted.calibrationOffsetDb = 7.5;
        shifted.normalize = true;
        DisplayExtrasProcessor a(plain);
        DisplayExtrasProcessor b(shifted);
        // 192 kHz over 4096 bins.
        const double binWidthHz = 192000.0 / 4096.0;
        const float shift = 7.5f - 10.0f * std::log10(static_cast<float>(binWidthHz));
        // Average (2): normalise applies. Peak (0): only the calibration.
        QCOMPARE(a.displayShiftDb(binWidthHz, 2), 0.0f);
        QVERIFY(std::abs(b.displayShiftDb(binWidthHz, 2) - shift) < 1.0e-5f);
        QVERIFY(std::abs(b.displayShiftDb(binWidthHz, 0) - 7.5f) < 1.0e-5f);
        const QVector<QVector<float>> rows = recordedRows(40, 64);
        for (int i = 0; i < rows.size(); ++i) {
            DisplayCodecFrame frame;
            frame.context = {1, 1, -180.0f, 0.0f, 64, 64, 0};
            frame.traceDbm = rows.at(i);
            frame.waterfallDbm = rows.at(i);
            frame.waterfallAdvance = true;
            DisplayExtrasInputs inputs;
            inputs.fps = 30;
            inputs.binWidthHz = binWidthHz;
            inputs.traceDetector = 2;   // Average: normalise applies
            const DisplayExtrasFrame x = a.process(frame, inputs);
            const DisplayExtrasFrame y = b.process(frame, inputs);
            QCOMPARE(y.peakBlobs->size(), x.peakBlobs->size());
            for (int k = 0; k < x.peakBlobs->size(); ++k) {
                QCOMPARE(y.peakBlobs->at(k).pixel, x.peakBlobs->at(k).pixel);
                QVERIFY(std::abs(y.peakBlobs->at(k).dbm - x.peakBlobs->at(k).dbm - shift)
                        < 1.0e-3f);
            }
            // Inside the display delay after the first reset neither sends
            // the row; after it both do.
            QCOMPARE(y.peakHoldDbm.has_value(), x.peakHoldDbm.has_value());
            // 500 ms at 30 frames a second: absent through frame 14, present
            // from frame 17 (frames 15 and 16 sit on the floating-point edge).
            if (i <= 14 || i >= 17) { QCOMPARE(x.peakHoldDbm.has_value(), i >= 17); }
            for (int k = 0; x.peakHoldDbm && k < x.peakHoldDbm->size(); ++k) {
                QVERIFY(std::abs(y.peakHoldDbm->at(k) - x.peakHoldDbm->at(k) - shift) < 1.0e-3f);
            }
            QVERIFY(std::abs(*y.noiseFloorDbm - *x.noiseFloorDbm - shift) < 1.0e-3f);
            QVERIFY(std::abs(y.waterfallLevelsDbm->first - x.waterfallLevelsDbm->first - shift)
                    < 1.0e-3f);
        }
    }

    void clarityLevelsAreTheControllersForTheSameFloors()
    {
        DisplayExtrasRequest request;
        DisplayExtrasRequest::WaterfallLevels levels;
        levels.mode = WaterfallLevelMode::Clarity;
        request.waterfallLevels = levels;
        DisplayExtrasProcessor station(request);
        ClarityController desktop;
        desktop.setEnabled(true);
        DisplayCodecFrame frame;
        frame.context = {1, 1, -180.0f, 0.0f, 8, 8, 0};
        frame.traceDbm = QVector<float>(8, -120.0f);
        frame.waterfallDbm = frame.traceDbm;
        frame.waterfallAdvance = true;
        DisplayExtrasInputs inputs;
        // Before Clarity speaks, the operator's own levels stand.
        std::pair<float, float> out = *station.process(frame, inputs).waterfallLevelsDbm;
        QCOMPARE(out.first, -122.0f);
        QCOMPARE(out.second, -62.0f);
        for (int step = 0; step < 12; ++step) {
            const qint64 nowMs = 10'000 + static_cast<qint64>(step) * 600;
            const float floor = step < 6 ? -118.0f : -104.0f;
            station.feedNoiseFloor(floor, nowMs);
            desktop.feedNoiseFloor(floor, nowMs);
            inputs.nowMs = nowMs;
            out = *station.process(frame, inputs).waterfallLevelsDbm;
            QCOMPARE(out.first, desktop.lastLow());
            QCOMPARE(out.second, desktop.lastHigh());
        }
    }

    // ── Key edges: kept level, restore, hold (design 5.2, 5.3, 5.6) ─────

private:
    // One frame with every pixel at `dbm`, adding a waterfall line.
    static DisplayCodecFrame flatFrame(float dbm)
    {
        DisplayCodecFrame frame;
        frame.context = {1, 1, -180.0f, 0.0f, 64, 64, 0};
        frame.traceDbm = QVector<float>(64, dbm);
        frame.waterfallDbm = frame.traceDbm;
        frame.waterfallAdvance = true;
        return frame;
    }

    static DisplayExtrasRequest levelsRequest(WaterfallLevelMode mode)
    {
        DisplayExtrasRequest request;
        DisplayExtrasRequest::WaterfallLevels levels;
        levels.mode = mode;
        request.waterfallLevels = levels;
        request.noiseFloor = DisplayExtrasRequest::NoiseFloor{true, 0.0, true};
        return request;
    }

    // A processor on a clock of the test's own, 33 ms a frame.
    struct Run {
        DisplayExtrasProcessor station;
        DisplayExtrasInputs inputs;
        qint64 nowMs {0};

        explicit Run(const DisplayExtrasRequest& request, qint64 holdLengthMs = 3000)
            : station(request)
        {
            inputs.fps = 30;
            inputs.sliceHz = 14200000.0;
            inputs.band = 5;
            inputs.holdLengthMs = holdLengthMs;
        }
        // One frame at the clock as it stands; the clock does not move.
        DisplayExtrasFrame frameAt(qint64 atMs, float dbm)
        {
            nowMs = atMs;
            inputs.nowMs = atMs;
            return station.process(flatFrame(dbm), inputs);
        }
        // `frames` frames, each 33 ms after the one before.
        DisplayExtrasFrame frames(int frames, float dbm)
        {
            DisplayExtrasFrame out;
            for (int i = 0; i < frames; ++i) {
                out = frameAt(nowMs + 33, dbm);
            }
            return out;
        }
    };

private slots:
    // JJ 2026-10-23: each time the phone came back from a sound-only
    // stretch, and each time a window's Core restarted, the Core started a
    // fresh display. Its noise floor read about -194 dBm and climbed to the
    // band's -111 over about eight seconds, and NF-AGC drew the waterfall
    // against it, so ordinary noise came out bright. The fresh display reads
    // the band from its first frame with data, and frames with none (a
    // display that has nothing yet) change nothing. From that frame the
    // floor is in fast attack, as Thetis starts a display: within its 3 dB
    // step of the band, and within 1 dB once the fast attack ends. NF-AGC's
    // low level is the AGC's run level until then, which starts at the
    // seed's target: as near the band as the floor is from the first frame.
    void aStartedDisplayReadsTheBandFromItsFirstFrameWithData()
    {
        Run run(levelsRequest(WaterfallLevelMode::NoiseFloorAgc));
        const DisplayExtrasFrame empty = run.frames(5, -200.0f);
        QVERIFY(empty.noiseFloorDbm.has_value());
        QVERIFY(!*empty.noiseFloorFastAttack);
        const DisplayExtrasFrame first = run.frames(1, -111.0f);
        QVERIFY2(std::abs(*first.noiseFloorDbm + 111.0f) < 0.05f,
                 qPrintable(QString::number(*first.noiseFloorDbm)));
        QVERIFY(*first.noiseFloorFastAttack);
        QVERIFY2(std::abs(first.waterfallLevelsDbm->first + 111.0f) < 0.05f,
                 qPrintable(QString::number(first.waterfallLevelsDbm->first)));
        bool sawFastAttackEnd = false;
        for (int i = 1; i <= 300; ++i) {
            const DisplayExtrasFrame out = run.frames(1, -111.0f);
            const bool fastAttack = *out.noiseFloorFastAttack;
            sawFastAttackEnd = sawFastAttackEnd || !fastAttack;
            // Fast attack lasts at least a second.
            QVERIFY(fastAttack || i * 33 > 1000);
            QVERIFY2(std::abs(*out.noiseFloorDbm + 111.0f) < (fastAttack ? 3.05f : 1.0f),
                     qPrintable(QStringLiteral("frame %1 floor %2")
                                    .arg(i).arg(*out.noiseFloorDbm)));
            QVERIFY2(std::abs(out.waterfallLevelsDbm->first + 111.0f)
                         < (fastAttack ? 3.05f : 1.0f),
                     qPrintable(QStringLiteral("frame %1 low %2")
                                    .arg(i).arg(out.waterfallLevelsDbm->first)));
        }
        QVERIFY(sawFastAttackEnd);
    }

    // PR 628 review: the trace's Log Recursive average (both clients'
    // default) starts at -160 dBm on a new endpoint, so its first frames
    // read below the band. A real endpoint at Log Recursive, 30 ms and
    // 650 ms, into the display's extras: the floor and the NF-AGC low level
    // are within 3 dB of the band from the first frame (fast attack's 3 dB
    // step on this flat band reaches it; the run level starts at the
    // seed's target).
    void aStartedDisplayAtLogRecursiveReadsTheBandFromItsFirstFrame_data()
    {
        QTest::addColumn<int>("averageTimeMs");
        QTest::newRow("30 ms") << 30;
        QTest::newRow("650 ms") << 650;
    }

    void aStartedDisplayAtLogRecursiveReadsTheBandFromItsFirstFrame()
    {
        QFETCH(int, averageTimeMs);
        constexpr int kFps = 30;
        // 1e-12 W through a peak detector with no offset: -120 dBm.
        constexpr float kBandDbm = -120.0f;
        SpectrumEndpointSourceContext source;
        source.source = {2, FftTier::Wide};
        source.sourceGeneration = 11;
        source.fftBins = 1024;
        source.centreHz = 14'200'000.0;
        source.sampleRateHz = 192'000.0;
        source.contextGeneration = 4;
        SpectrumEndpointRequest request;
        request.endpointId = 91;
        request.source = source.source;
        request.centreHz = source.centreHz;
        request.spanHz = 96'000.0;
        request.pixels = 64;
        request.targetFps = kFps;
        request.framesPerLine = 1;
        request.trace.detector = SpectrumDetectorMode::Peak;
        request.trace.averageMode = 3;
        request.trace.averageAlpha = averageAlphaForTimeMs(averageTimeMs, kFps);
        request.waterfall.detector = SpectrumDetectorMode::Peak;
        request.waterfall.averageMode = 0;
        SpectrumEndpoint endpoint;
        QVERIFY(endpoint.configure(request, source));

        Run run(levelsRequest(WaterfallLevelMode::NoiseFloorAgc));
        for (int i = 0; i < 8 * kFps; ++i) {
            // Half way, the source restarts (a rate or FFT size change): the
            // endpoint's reducers start again from -160 dBm and the floor
            // the display carries must not follow them down.
            if (i == 4 * kFps) {
                source.sourceGeneration = 12;
                source.contextGeneration = 5;
                QVERIFY(endpoint.configure(request, source));
                QVERIFY(endpoint.tracePicture() == SpectrumPictureKind::New);
            }
            DaemonSpectrumFrame input;
            input.source = source.source;
            input.generation = source.sourceGeneration;
            input.centreHz = source.centreHz;
            input.sampleRateHz = source.sampleRateHz;
            input.producedAtNs = (static_cast<qint64>(i) + 1) * 34'000'000;
            input.windowEnb = 1.0;
            input.dbmOffset = 0.0;
            input.binsLinear.fill(1.0e-12f, source.fftBins);
            input.binsLinear[300] = 1.0e-5f;
            const std::optional<DisplayCodecFrame> reduced = endpoint.consume(input, 0.0);
            QVERIFY(reduced.has_value());
            run.nowMs = static_cast<qint64>(i) * 34;
            run.inputs.nowMs = run.nowMs;
            const DisplayExtrasFrame out = run.station.process(*reduced, run.inputs);
            QVERIFY2(std::abs(*out.noiseFloorDbm - kBandDbm) < 3.05f,
                     qPrintable(QStringLiteral("frame %1 floor %2")
                                    .arg(i).arg(*out.noiseFloorDbm)));
            QVERIFY2(std::abs(out.waterfallLevelsDbm->first - kBandDbm) < 3.0f,
                     qPrintable(QStringLiteral("frame %1 low %2")
                                    .arg(i).arg(out.waterfallLevelsDbm->first)));
        }
    }

    // PR 628 review: before the first wideband frame, an extended view
    // fills its wings with the request's minimum. Those pixels are no data,
    // so the floor is the island's, not the bottom of the scale.
    void anExtendedViewsPlaceholderWingsAreNotTheFloor()
    {
        SpectrumEndpointSourceContext source;
        source.source = {2, FftTier::Wide};
        source.sourceGeneration = 11;
        source.fftBins = 1024;
        source.centreHz = 500'000.0;
        source.sampleRateHz = 1'000'000.0;
        source.contextGeneration = 4;
        source.wideband.available = true;
        source.wideband.active = true;
        source.wideband.physicalAdcIndex = 0;
        source.wideband.filterChainIndex = 0;
        source.wideband.sourceGeneration = 31;
        source.wideband.adcRateHz = 4'000'000.0;
        SpectrumEndpointRequest request;
        request.endpointId = 91;
        request.source = source.source;
        request.centreHz = 500'000.0;
        request.spanHz = 2'000'000.0;
        request.pixels = 100;
        request.targetFps = 30;
        request.framesPerLine = 1;
        request.extendedView = true;
        request.trace.detector = SpectrumDetectorMode::Peak;
        request.trace.averageMode = 0;
        request.waterfall.detector = SpectrumDetectorMode::Peak;
        request.waterfall.averageMode = 0;
        SpectrumEndpoint endpoint;
        QVERIFY(endpoint.configure(request, source));

        Run run(levelsRequest(WaterfallLevelMode::NoiseFloorAgc));
        for (int i = 0; i < 60; ++i) {
            DaemonSpectrumFrame input;
            input.source = source.source;
            input.generation = source.sourceGeneration;
            input.centreHz = source.centreHz;
            input.sampleRateHz = source.sampleRateHz;
            input.producedAtNs = (static_cast<qint64>(i) + 1) * 34'000'000;
            input.windowEnb = 1.0;
            input.dbmOffset = 0.0;
            input.binsLinear.fill(1.0e-12f, source.fftBins);
            const std::optional<DisplayCodecFrame> reduced = endpoint.consume(input, 0.0);
            QVERIFY(reduced.has_value());
            // More than half the row is wing at the request's minimum.
            if (i == 0) {
                const auto atMinimum = std::count(reduced->traceDbm.cbegin(),
                                                  reduced->traceDbm.cend(), request.minDbm);
                QVERIFY2(atMinimum > reduced->traceDbm.size() / 2,
                         qPrintable(QString::number(atMinimum)));
            }
            run.nowMs = static_cast<qint64>(i) * 34;
            run.inputs.nowMs = run.nowMs;
            const DisplayExtrasFrame out = run.station.process(*reduced, run.inputs);
            // Within fast attack's 3 dB step on this flat band.
            QVERIFY2(std::abs(*out.noiseFloorDbm + 120.0f) < 3.05f,
                     qPrintable(QStringLiteral("frame %1 floor %2")
                                    .arg(i).arg(*out.noiseFloorDbm)));
        }
    }

    // PR 628 re-review: the warm-up on a noisy band. The FFT's bins are
    // exponentially distributed power, so the trace's own spread narrows as
    // its average fills, and the peak detector's spread narrows with more
    // bins to a pixel. The floor and the NF-AGC low level (whose run level
    // starts at the seed's target) are within 3 dB of where they settle
    // from the first frame at 4 FFT bins to a pixel, and from the fourth
    // (the seed's fast attack, as Thetis starts a display, carries them
    // there) at 1 and 2, and across a restart of the source. Rows: a Peak
    // detector at 1, 2 and 4 bins to a pixel, 30, 650 and 2000 ms. At 30 ms
    // with 1 or 2 bins to a pixel the trace is all but unaveraged and the
    // floor, which fast attack does not smooth, swings past 3 dB: under
    // 3.5 dB there while the fast attack runs, under 3 dB after it.
    void aNoisyBandStaysWithinThreeDbOfItsSettledFloor_data()
    {
        QTest::addColumn<int>("binsPerPixel");
        QTest::addColumn<int>("averageTimeMs");
        // The first frame checked, and the bound while the fast attack runs.
        QTest::addColumn<int>("firstFrame");
        QTest::addColumn<double>("fastAttackBoundDb");
        for (const int bins : {1, 2}) {
            QTest::addRow("%d bins a pixel, 30 ms", bins) << bins << 30 << 3 << 3.5;
            for (const int ms : {650, 2000}) {
                QTest::addRow("%d bins a pixel, %d ms", bins, ms) << bins << ms << 3 << 3.0;
            }
        }
        for (const int ms : {30, 650, 2000}) {
            QTest::addRow("4 bins a pixel, %d ms", ms) << 4 << ms << 0 << 3.0;
        }
    }

    void aNoisyBandStaysWithinThreeDbOfItsSettledFloor()
    {
        QFETCH(int, binsPerPixel);
        QFETCH(int, averageTimeMs);
        QFETCH(int, firstFrame);
        QFETCH(double, fastAttackBoundDb);
        constexpr int kFps = 30;
        // The floor, low level and fast attack of each frame, from a new
        // endpoint whose source restarts at frame `restartAt` (none if
        // negative).
        const auto runBand = [binsPerPixel, averageTimeMs](int frames, int restartAt,
                                                           QVector<float>& floors,
                                                           QVector<float>& lows,
                                                           QVector<bool>& fastAttacks) {
            SpectrumEndpointSourceContext source;
            source.source = {2, FftTier::Wide};
            source.sourceGeneration = 11;
            source.fftBins = 1024;
            source.centreHz = 14'200'000.0;
            source.sampleRateHz = 192'000.0;
            source.contextGeneration = 4;
            SpectrumEndpointRequest request;
            request.endpointId = 91;
            request.source = source.source;
            request.centreHz = source.centreHz;
            request.spanHz = 96'000.0;
            // 512 of the 1024 bins are in the span.
            request.pixels = 512 / binsPerPixel;
            request.targetFps = kFps;
            request.framesPerLine = 1;
            request.trace.detector = SpectrumDetectorMode::Peak;
            request.trace.averageMode = 3;
            request.trace.averageAlpha = averageAlphaForTimeMs(averageTimeMs, kFps);
            request.waterfall.detector = SpectrumDetectorMode::Peak;
            request.waterfall.averageMode = 0;
            SpectrumEndpoint endpoint;
            QVERIFY(endpoint.configure(request, source));
            Run run(levelsRequest(WaterfallLevelMode::NoiseFloorAgc));
            std::mt19937 rng(628);
            std::exponential_distribution<float> noise(1.0f);
            for (int i = 0; i < frames; ++i) {
                if (i == restartAt) {
                    source.sourceGeneration = 12;
                    source.contextGeneration = 5;
                    QVERIFY(endpoint.configure(request, source));
                    QVERIFY(endpoint.tracePicture() == SpectrumPictureKind::New);
                }
                DaemonSpectrumFrame input;
                input.source = source.source;
                input.generation = source.sourceGeneration;
                input.centreHz = source.centreHz;
                input.sampleRateHz = source.sampleRateHz;
                input.producedAtNs = (static_cast<qint64>(i) + 1) * 34'000'000;
                input.windowEnb = 1.0;
                input.dbmOffset = 0.0;
                input.binsLinear.resize(source.fftBins);
                for (float& bin : input.binsLinear) { bin = 1.0e-12f * noise(rng); }
                input.binsLinear[300] = 1.0e-5f;
                const std::optional<DisplayCodecFrame> reduced = endpoint.consume(input, 0.0);
                QVERIFY(reduced.has_value());
                run.nowMs = static_cast<qint64>(i) * 34;
                run.inputs.nowMs = run.nowMs;
                const DisplayExtrasFrame out = run.station.process(*reduced, run.inputs);
                QVERIFY(out.noiseFloorDbm.has_value() && out.waterfallLevelsDbm.has_value());
                floors.append(*out.noiseFloorDbm);
                lows.append(out.waterfallLevelsDbm->first);
                fastAttacks.append(out.noiseFloorFastAttack.value_or(false));
            }
        };
        QVector<float> referenceFloors;
        QVector<float> referenceLows;
        QVector<bool> referenceFastAttacks;
        runBand(20 * kFps, -1, referenceFloors, referenceLows, referenceFastAttacks);
        QVERIFY(!QTest::currentTestFailed());
        double settledFloor = 0.0;
        double settledLow = 0.0;
        for (int i = 17 * kFps; i < 20 * kFps; ++i) {
            settledFloor += referenceFloors.at(i);
            settledLow += referenceLows.at(i);
        }
        settledFloor /= 3 * kFps;
        settledLow /= 3 * kFps;

        QVector<float> floors;
        QVector<float> lows;
        QVector<bool> fastAttacks;
        runBand(10 * kFps, 5 * kFps, floors, lows, fastAttacks);
        QVERIFY(!QTest::currentTestFailed());
        double worstFloor = 0.0;
        double worstLow = 0.0;
        double worstInFastAttack = 0.0;
        int worstFrame = 0;
        int fastAttackFrames = 0;
        for (int i = firstFrame; i < floors.size(); ++i) {
            const double floorOff = std::abs(floors.at(i) - settledFloor);
            const double lowOff = std::abs(lows.at(i) - settledLow);
            if (floorOff > worstFloor) {
                worstFloor = floorOff;
                worstFrame = i;
            }
            worstLow = std::max(worstLow, lowOff);
            if (fastAttacks.at(i)) {
                ++fastAttackFrames;
                worstInFastAttack = std::max({worstInFastAttack, floorOff, lowOff});
            }
        }
        qInfo("%d bins a pixel, %d ms: settled floor %.2f dBm; frames 0 to 2 %+.2f %+.2f %+.2f dB,"
              " low %+.2f %+.2f %+.2f dB; from frame %d worst floor %.2f dB (frame %d), low %.2f"
              " dB, %.2f dB in %d fast attack frames",
              binsPerPixel, averageTimeMs, settledFloor,
              floors.at(0) - settledFloor, floors.at(1) - settledFloor,
              floors.at(2) - settledFloor, lows.at(0) - settledLow, lows.at(1) - settledLow,
              lows.at(2) - settledLow, firstFrame, worstFloor, worstFrame, worstLow,
              worstInFastAttack, fastAttackFrames);
        for (int i = firstFrame; i < floors.size(); ++i) {
            const double bound = fastAttacks.at(i) ? fastAttackBoundDb : 3.0;
            QVERIFY2(std::abs(floors.at(i) - settledFloor) < bound,
                     qPrintable(QStringLiteral("frame %1 floor %2, settled %3")
                                    .arg(i).arg(floors.at(i)).arg(settledFloor)));
            QVERIFY2(std::abs(lows.at(i) - settledLow) < bound,
                     qPrintable(QStringLiteral("frame %1 low %2, settled %3")
                                    .arg(i).arg(lows.at(i)).arg(settledLow)));
        }
    }

    // PR 628 re-review: an extended view whose wideband data first arrives
    // after the trace's average has warmed up. Each wing pixel's average
    // still holds its placeholder for as long as the average takes, so it
    // is no data until then; the floor stays within 3 dB of the band.
    void anExtendedViewsLateWingsAreNotTheFloorWhileTheyWarm()
    {
        constexpr int kFps = 30;
        constexpr float kBandDbm = -120.0f;
        SpectrumEndpointSourceContext source;
        source.source = {2, FftTier::Wide};
        source.sourceGeneration = 11;
        source.fftBins = 1024;
        source.centreHz = 500'000.0;
        source.sampleRateHz = 1'000'000.0;
        source.contextGeneration = 4;
        source.wideband.available = true;
        source.wideband.active = true;
        source.wideband.physicalAdcIndex = 0;
        source.wideband.filterChainIndex = 0;
        source.wideband.sourceGeneration = 31;
        source.wideband.adcRateHz = 4'000'000.0;
        SpectrumEndpointRequest request;
        request.endpointId = 91;
        request.source = source.source;
        request.centreHz = 500'000.0;
        request.spanHz = 2'000'000.0;
        request.pixels = 100;
        request.targetFps = kFps;
        request.framesPerLine = 1;
        request.extendedView = true;
        request.trace.detector = SpectrumDetectorMode::Peak;
        request.trace.averageMode = 3;
        request.trace.averageAlpha = averageAlphaForTimeMs(650, kFps);
        request.waterfall.detector = SpectrumDetectorMode::Peak;
        request.waterfall.averageMode = 0;
        SpectrumEndpoint endpoint;
        QVERIFY(endpoint.configure(request, source));
        // The wideband cache holds unnormalized FFT dB; this puts the wings
        // at the band's display level through the Core's own reference.
        WidebandSpectrumFrame wideband;
        wideband.source.physicalAdcIndex = source.wideband.physicalAdcIndex;
        wideband.source.sourceGeneration = source.wideband.sourceGeneration;
        wideband.source.adcRateHz = source.wideband.adcRateHz;
        wideband.producedAtNs = 1;
        wideband.rawDbBins.fill(kBandDbm - widebandRelativeReferenceDb(
                                    source.wideband.adcRateHz,
                                    source.sampleRateHz / source.fftBins, 1.0,
                                    SpectrumDetectorMode::Peak),
                                WidebandFftEngine::kOutputBins);

        Run run(levelsRequest(WaterfallLevelMode::NoiseFloorAgc));
        for (int i = 0; i < 10 * kFps; ++i) {
            DaemonSpectrumFrame input;
            input.source = source.source;
            input.generation = source.sourceGeneration;
            input.centreHz = source.centreHz;
            input.sampleRateHz = source.sampleRateHz;
            input.producedAtNs = (static_cast<qint64>(i) + 1) * 34'000'000;
            input.windowEnb = 1.0;
            input.dbmOffset = 0.0;
            input.binsLinear.fill(1.0e-12f, source.fftBins);
            // The wideband data arrives 4 s in, after the average is warm.
            const std::optional<DisplayCodecFrame> reduced = i < 4 * kFps
                ? endpoint.consume(input, 0.0)
                : endpoint.consume(input, 0.0, wideband);
            QVERIFY(reduced.has_value());
            if (i == 10 * kFps - 1) {
                QVERIFY2(std::abs(reduced->traceDbm.last() - kBandDbm) < 1.0f,
                         qPrintable(QString::number(reduced->traceDbm.last())));
            }
            run.nowMs = static_cast<qint64>(i) * 34;
            run.inputs.nowMs = run.nowMs;
            const DisplayExtrasFrame out = run.station.process(*reduced, run.inputs);
            QVERIFY2(std::abs(*out.noiseFloorDbm - kBandDbm) < 3.05f,
                     qPrintable(QStringLiteral("frame %1 floor %2")
                                    .arg(i).arg(*out.noiseFloorDbm)));
            QVERIFY2(std::abs(out.waterfallLevelsDbm->first - kBandDbm) < 3.0f,
                     qPrintable(QStringLiteral("frame %1 low %2")
                                    .arg(i).arg(out.waterfallLevelsDbm->first)));
        }
    }

    // D17, R-DSP-23, R-DSP-24: the rows of a key are 40 dB hotter than the
    // band. The AGC's run level and its low level stay where they were
    // through the key, and the first receive row after it starts from them:
    // no re-prime on a row that still holds the transmission.
    void aKeyKeepsTheAgcRunLevelAndTheUnKeyRestoresIt()
    {
        Run run(levelsRequest(WaterfallLevelMode::Agc));
        run.frames(200, -120.0f);
        const std::optional<float> before = run.station.agcRunLevel();
        QVERIFY(before.has_value());
        QVERIFY(std::abs(*before + 120.0f) < 0.01f);
        const std::pair<float, float> levelsBefore = run.station.waterfallLevels();
        QVERIFY(std::abs(levelsBefore.first + 132.0f) < 0.01f);

        run.inputs.mox = true;
        const DisplayExtrasFrame keyedOut = run.frames(50, -80.0f);
        QVERIFY(run.station.keyed());
        QVERIFY(run.station.levelFollower().keyed());
        QCOMPARE(*run.station.agcRunLevel(), *before);
        QCOMPARE(keyedOut.waterfallLevelsDbm->first, levelsBefore.first);
        QCOMPARE(keyedOut.waterfallLevelsDbm->second, levelsBefore.second);

        run.inputs.mox = false;
        const DisplayExtrasFrame first = run.frames(1, -80.0f);
        QVERIFY(!run.station.keyed());
        QVERIFY(run.station.agcRunLevel().has_value());
        QCOMPARE(*run.station.agcRunLevel(), *before);
        QCOMPARE(first.waterfallLevelsDbm->first, levelsBefore.first);
        QCOMPARE(first.waterfallLevelsDbm->second, levelsBefore.second);
    }

    // A band changed while keyed, with nothing saved for the new band:
    // the AGC starts at the restart floor, -150 dBm. Each band gets its
    // own level back.
    void aBandChangedWhileKeyedStartsAtTheRestartFloor()
    {
        Run run(levelsRequest(WaterfallLevelMode::Agc));
        run.frames(200, -120.0f);
        const float bandFive = *run.station.agcRunLevel();
        run.inputs.mox = true;
        run.frames(10, -80.0f);
        run.inputs.band = 6;
        run.frames(10, -80.0f);
        run.inputs.mox = false;
        const DisplayExtrasFrame first = run.frames(1, -80.0f);
        QCOMPARE(*run.station.agcRunLevel(), WaterfallLevelFollower::kAgcRestartFloorDbm);
        QCOMPARE(*run.station.agcRunLevel(), -150.0f);
        QCOMPARE(first.waterfallLevelsDbm->first,
                 -150.0f - WaterfallLevelFollower::kAgcMarginDb);

        // Keyed again on band 6 (inside the hold, so -150 is what is saved),
        // back to band 5 while keyed: band 5's level returns.
        run.inputs.mox = true;
        run.frames(10, -80.0f);
        run.inputs.band = 5;
        run.frames(10, -80.0f);
        run.inputs.mox = false;
        run.frames(1, -80.0f);
        QCOMPARE(*run.station.agcRunLevel(), bandFive);
        // And band 6 keeps the one saved for it.
        run.inputs.mox = true;
        run.frames(10, -80.0f);
        run.inputs.band = 6;
        run.frames(10, -80.0f);
        run.inputs.mox = false;
        run.frames(1, -80.0f);
        QCOMPARE(*run.station.agcRunLevel(), -150.0f);
    }

    // S13, R-DSP-28, R-DSP-53: with a hold of 3000 ms the AGC takes nothing
    // from the rows of the first 3000 ms after the un-key, then follows
    // again; the noise floor stays in fast attack for at least as long.
    void theUnKeyHoldKeepsTheAgcAndTheFastAttackForItsLength()
    {
        Run run(levelsRequest(WaterfallLevelMode::Agc), 3000);
        run.frames(300, -120.0f);
        QVERIFY(!run.station.noiseFloor().fastAttack());
        const float before = *run.station.agcRunLevel();
        run.inputs.mox = true;
        run.frames(30, -80.0f);
        QVERIFY(run.station.noiseFloor().fastAttack());   // the key edge
        run.inputs.mox = false;
        const qint64 unKeyMs = run.nowMs + 33;
        run.frameAt(unKeyMs, -80.0f);
        QVERIFY(run.station.levelFollower().inHold(unKeyMs));
        for (qint64 at = unKeyMs + 33; at < unKeyMs + 3000; at += 33) {
            const DisplayExtrasFrame out = run.frameAt(at, -80.0f);
            QCOMPARE(*run.station.agcRunLevel(), before);
            QVERIFY2(run.station.noiseFloor().fastAttack(), qPrintable(QString::number(at)));
            QVERIFY(*out.noiseFloorFastAttack);
        }
        run.frameAt(unKeyMs + 2999, -80.0f);
        QCOMPARE(*run.station.agcRunLevel(), before);
        QVERIFY(run.station.noiseFloor().fastAttack());
        // The hold has run: the next row moves the level.
        run.frameAt(unKeyMs + 3000, -80.0f);
        QVERIFY(!run.station.levelFollower().inHold(unKeyMs + 3000));
        QVERIFY(*run.station.agcRunLevel() > before);
        const float resumed = *run.station.agcRunLevel();
        run.frameAt(unKeyMs + 3033, -80.0f);
        QVERIFY(*run.station.agcRunLevel() > resumed);
    }

    // The same hold for Clarity: its floor and levels are the ones from
    // before the key through the key and the hold, whatever floors arrive;
    // after the hold it follows again, smoothed from the hold's end.
    void theUnKeyHoldKeepsClaritysLevelForItsLength()
    {
        Run run(levelsRequest(WaterfallLevelMode::Clarity), 3000);
        qint64 at = 0;
        for (int step = 0; step < 6; ++step) {
            at = 1000 + static_cast<qint64>(step) * 600;
            run.station.feedNoiseFloor(-118.0f, at);
            run.frameAt(at, -118.0f);
        }
        const float floorBefore = run.station.clarity()->smoothedFloor();
        const std::pair<float, float> levelsBefore = run.station.waterfallLevels();
        QCOMPARE(floorBefore, -118.0f);
        QCOMPARE(levelsBefore.first, run.station.clarity()->lastLow());

        // A 20 s key: longer than Clarity's time constant.
        run.inputs.mox = true;
        run.frameAt(at + 100, -80.0f);
        for (qint64 keyed = at + 600; keyed < at + 20'000; keyed += 600) {
            run.station.feedNoiseFloor(-80.0f, keyed);
            run.frameAt(keyed, -80.0f);
        }
        QCOMPARE(run.station.clarity()->smoothedFloor(), floorBefore);
        QVERIFY(run.station.waterfallLevels() == levelsBefore);

        run.inputs.mox = false;
        const qint64 unKeyMs = at + 20'000;
        run.frameAt(unKeyMs, -80.0f);
        QVERIFY(run.station.clarity()->inHold(unKeyMs));
        for (qint64 held = unKeyMs + 500; held < unKeyMs + 3000; held += 500) {
            run.station.feedNoiseFloor(-80.0f, held);
            run.frameAt(held, -80.0f);
            QCOMPARE(run.station.clarity()->smoothedFloor(), floorBefore);
            QVERIFY(run.station.waterfallLevels() == levelsBefore);
        }
        run.station.feedNoiseFloor(-80.0f, unKeyMs + 2999);
        QCOMPARE(run.station.clarity()->smoothedFloor(), floorBefore);
        QVERIFY(!run.station.clarity()->inHold(unKeyMs + 3000));
        // After the hold: the floor moves toward the new reading, smoothed
        // over the 600 ms since the hold's end, not over the 20 s key.
        run.station.feedNoiseFloor(-110.0f, unKeyMs + 3600);
        const float after = run.station.clarity()->smoothedFloor();
        const float alpha = 1.0f - std::exp(-0.6f / 3.0f);
        QVERIFY(std::abs(after - (floorBefore + alpha * (-110.0f - floorBefore))) < 0.001f);
        const DisplayExtrasFrame out = run.frameAt(unKeyMs + 3600, -110.0f);
        QCOMPARE(out.waterfallLevelsDbm->first, run.station.clarity()->lastLow());
        QVERIFY(out.waterfallLevelsDbm->first > levelsBefore.first);
    }

    // A key inside the hold ends that hold and keeps the level; the next
    // un-key starts a whole new hold.
    void aReKeyInsideTheHoldKeepsTheLevelAndStartsAFreshHold()
    {
        Run run(levelsRequest(WaterfallLevelMode::Agc), 3000);
        run.frames(200, -120.0f);
        const float before = *run.station.agcRunLevel();
        run.inputs.mox = true;
        run.frames(10, -80.0f);
        run.inputs.mox = false;
        const qint64 firstUnKeyMs = run.nowMs + 33;
        run.frameAt(firstUnKeyMs, -80.0f);
        run.inputs.mox = true;
        run.frameAt(firstUnKeyMs + 1000, -80.0f);
        QVERIFY(!run.station.levelFollower().inHold(firstUnKeyMs + 1000));
        run.frameAt(firstUnKeyMs + 1500, -80.0f);
        run.inputs.mox = false;
        const qint64 secondUnKeyMs = firstUnKeyMs + 2000;
        run.frameAt(secondUnKeyMs, -80.0f);
        // Past the first hold's end, inside the second's.
        run.frameAt(firstUnKeyMs + 3500, -80.0f);
        QCOMPARE(*run.station.agcRunLevel(), before);
        run.frameAt(secondUnKeyMs + 2999, -80.0f);
        QCOMPARE(*run.station.agcRunLevel(), before);
        run.frameAt(secondUnKeyMs + 3000, -80.0f);
        QVERIFY(*run.station.agcRunLevel() > before);
    }

    // D17a, R-DSP-55: a processor that replaces another for the same display
    // carries on from it. Its levels on the next frame are the ones the
    // replaced processor would have made.
    void aReplacementProcessorCarriesTheAgcLevelAndTheNoiseFloor_data()
    {
        QTest::addColumn<int>("mode");
        QTest::newRow("agc") << static_cast<int>(WaterfallLevelMode::Agc);
        QTest::newRow("noise floor agc") << static_cast<int>(WaterfallLevelMode::NoiseFloorAgc);
    }

    void aReplacementProcessorCarriesTheAgcLevelAndTheNoiseFloor()
    {
        QFETCH(int, mode);
        const DisplayExtrasRequest request = levelsRequest(static_cast<WaterfallLevelMode>(mode));
        const QVector<QVector<float>> rows = recordedRows(260, 64);
        DisplayExtrasProcessor replaced(request);
        DisplayExtrasProcessor twin(request);
        DisplayExtrasInputs inputs;
        inputs.fps = 30;
        inputs.band = 5;
        const auto frameOf = [&rows](int i) {
            DisplayCodecFrame frame = flatFrame(0.0f);
            frame.traceDbm = rows.at(i);
            frame.waterfallDbm = rows.at(i);
            return frame;
        };
        for (int i = 0; i < 250; ++i) {
            inputs.nowMs = static_cast<qint64>(i) * 33;
            replaced.process(frameOf(i), inputs);
            twin.process(frameOf(i), inputs);
        }
        QVERIFY(twin.agcRunLevel().has_value());
        // The operator moved High: the request differs, the mode does not.
        DisplayExtrasRequest moved = request;
        moved.waterfallLevels->highDbm = -70.0;
        DisplayExtrasProcessor replacement(moved);
        QVERIFY(!replacement.agcRunLevel().has_value());
        replacement.adoptCarry(replaced.takeCarry());
        QCOMPARE(*replacement.agcRunLevel(), *twin.agcRunLevel());
        QCOMPARE(replacement.waterfallLevels().first, twin.waterfallLevels().first);
        QCOMPARE(replacement.waterfallLevels().second, -70.0f);
        QCOMPARE(replacement.noiseFloor().lerpAverage(), twin.noiseFloor().lerpAverage());
        for (int i = 250; i < 260; ++i) {
            inputs.nowMs = static_cast<qint64>(i) * 33;
            const DisplayExtrasFrame carried = replacement.process(frameOf(i), inputs);
            const DisplayExtrasFrame expected = twin.process(frameOf(i), inputs);
            QCOMPARE(*replacement.agcRunLevel(), *twin.agcRunLevel());
            QCOMPARE(carried.waterfallLevelsDbm->first, expected.waterfallLevelsDbm->first);
            QCOMPARE(carried.waterfallLevelsDbm->second, -70.0f);
            QCOMPARE(*carried.noiseFloorDbm, *expected.noiseFloorDbm);
        }

        // Another level mode starts afresh.
        DisplayExtrasProcessor manual(levelsRequest(WaterfallLevelMode::Manual));
        manual.adoptCarry(twin.takeCarry());
        QVERIFY(!manual.agcRunLevel().has_value());
        QCOMPARE(manual.waterfallLevels().first, -122.0f);
    }

    // The same for Clarity, with its smoothing clock, and for a hold that
    // is still running when the display is asked for again.
    void aReplacementProcessorCarriesClaritysFloorClockAndHold()
    {
        const DisplayExtrasRequest request = levelsRequest(WaterfallLevelMode::Clarity);
        Run replaced(request, 3000);
        Run twin(request, 3000);
        for (Run* run : {&replaced, &twin}) {
            run->station.feedNoiseFloor(-125.0f, 1000);
            run->station.feedNoiseFloor(-112.0f, 1700);
            run->frameAt(1700, -112.0f);
        }
        const float floor = twin.station.clarity()->smoothedFloor();
        QVERIFY(floor > -125.0f && floor < -112.0f);

        Run replacement(request, 3000);
        replacement.station.adoptCarry(replaced.station.takeCarry());
        QCOMPARE(replacement.station.clarity()->smoothedFloor(), floor);
        QCOMPARE(replacement.station.clarity()->lastLow(), twin.station.clarity()->lastLow());
        QCOMPARE(replacement.station.clarity()->lastHigh(), twin.station.clarity()->lastHigh());
        QVERIFY(replacement.station.waterfallLevels() == twin.station.waterfallLevels());
        // Its first frame sends those levels.
        const DisplayExtrasFrame first = replacement.frameAt(1733, -112.0f);
        QCOMPARE(first.waterfallLevelsDbm->first, twin.station.clarity()->lastLow());
        QCOMPARE(first.waterfallLevelsDbm->second, twin.station.clarity()->lastHigh());
        // The clock came too: the next floor is smoothed over the same time.
        replacement.station.feedNoiseFloor(-100.0f, 2400);
        twin.station.feedNoiseFloor(-100.0f, 2400);
        QCOMPARE(replacement.station.clarity()->smoothedFloor(),
                 twin.station.clarity()->smoothedFloor());

        // Asked for again inside the un-key hold: the hold carries on.
        twin.inputs.mox = true;
        twin.frameAt(3000, -80.0f);
        twin.inputs.mox = false;
        twin.frameAt(4000, -80.0f);
        const float held = twin.station.clarity()->smoothedFloor();
        Run again(request, 3000);
        again.station.adoptCarry(twin.station.takeCarry());
        QVERIFY(again.station.clarity()->inHold(6999));
        again.station.feedNoiseFloor(-80.0f, 5000);
        again.frameAt(5000, -80.0f);
        QCOMPARE(again.station.clarity()->smoothedFloor(), held);
        QVERIFY(!again.station.clarity()->inHold(7000));

        // A display asked for again while keyed stays keyed: no second edge.
        Run keyedRun(levelsRequest(WaterfallLevelMode::Agc), 3000);
        keyedRun.frames(100, -120.0f);
        const float level = *keyedRun.station.agcRunLevel();
        keyedRun.inputs.mox = true;
        keyedRun.frames(5, -80.0f);
        Run keyedAgain(levelsRequest(WaterfallLevelMode::Agc), 3000);
        keyedAgain.station.adoptCarry(keyedRun.station.takeCarry());
        QVERIFY(keyedAgain.station.keyed());
        keyedAgain.inputs.mox = true;
        keyedAgain.nowMs = keyedRun.nowMs;
        keyedAgain.frames(5, -80.0f);
        QCOMPARE(*keyedAgain.station.agcRunLevel(), level);
        keyedAgain.inputs.mox = false;
        keyedAgain.frames(1, -80.0f);
        QCOMPARE(*keyedAgain.station.agcRunLevel(), level);
    }

    // Version 3: onTx is optional, a bool, and true when absent.
    void activePeakHoldOnTxParses()
    {
        const auto hold = [](const QJsonObject& members) {
            QJsonObject subscribe;
            subscribe.insert(QStringLiteral("activePeakHold"), members);
            return subscribe;
        };
        QJsonObject base{{QStringLiteral("enabled"), true},
                         {QStringLiteral("holdMs"), 500},
                         {QStringLiteral("fallDbPerSec"), 6.0}};
        DisplayExtrasRequest request;
        QVERIFY(parseDisplayExtrasRequest(hold(base), request));
        QVERIFY(request.activePeakHold->onTx);
        QJsonObject off = base;
        off.insert(QStringLiteral("onTx"), false);
        QVERIFY(parseDisplayExtrasRequest(hold(off), request));
        QVERIFY(!request.activePeakHold->onTx);
        QCOMPARE(request.activePeakHold->holdMs, 500);
        QJsonObject wrong = base;
        wrong.insert(QStringLiteral("onTx"), 1);
        DisplayExtrasRequest untouched;
        QVERIFY(!parseDisplayExtrasRequest(hold(wrong), untouched));
        QJsonObject extra = off;
        extra.insert(QStringLiteral("other"), true);
        QVERIFY(!parseDisplayExtrasRequest(hold(extra), untouched));
    }

    // Thetis display.cs:5011, 5359-5363 [v2.10.3.15], as the desktop: a
    // raised bin holds for holdMs, and while the endpoint's slice transmits
    // without onTx the trace is not updated, not decayed and not sent.
    void thePeakHoldHoldsAndStopsWhileTheSliceTransmits()
    {
        DisplayExtrasRequest request;
        request.activePeakHold = DisplayExtrasRequest::ActivePeakHold{true, 100, 25.0, false};
        DisplayExtrasProcessor station(request);
        DisplayCodecFrame frame;
        frame.context = {1, 1, -180.0f, 0.0f, 4, 4, 0};
        frame.traceDbm = {-40, -40, -40, -40};
        frame.waterfallDbm = frame.traceDbm;
        DisplayExtrasInputs inputs;
        inputs.fps = 25;
        // The first frame's reset holds the row back 500 ms (Thetis
        // display.cs:859-877 [v2.10.3.15]): 14 frames of 40 ms, then it runs.
        DisplayExtrasFrame out;
        int delayed = 0;
        while (!(out = station.process(frame, inputs)).peakHoldDbm.has_value()) {
            QVERIFY(++delayed <= 20);
        }
        QCOMPARE(delayed, 14);
        QCOMPARE(out.peakHoldDbm->first(), -40.0f);
        frame.traceDbm = {-90, -90, -90, -90};
        out = station.process(frame, inputs);   // 40 ms old
        out = station.process(frame, inputs);   // 80 ms old
        QCOMPARE(out.peakHoldDbm->first(), -40.0f);
        out = station.process(frame, inputs);   // 120 ms old: 1 dB lower
        QVERIFY(std::abs(out.peakHoldDbm->first() - -41.0f) < 1.0e-4f);

        inputs.transmitting = true;
        frame.traceDbm = {-10, -10, -10, -10};
        out = station.process(frame, inputs);
        QVERIFY(!out.peakHoldDbm.has_value());
        inputs.transmitting = false;
        frame.traceDbm = {-90, -90, -90, -90};
        out = station.process(frame, inputs);
        // Not raised by the transmit frame, and it did not fall meanwhile.
        QVERIFY(std::abs(out.peakHoldDbm->first() - -42.0f) < 1.0e-4f);

        DisplayExtrasRequest running = request;
        running.activePeakHold->onTx = true;
        DisplayExtrasProcessor keyed(running);
        for (int i = 0; i < 15; ++i) { keyed.process(frame, inputs); }
        inputs.transmitting = true;
        frame.traceDbm = {-10, -10, -10, -10};
        out = keyed.process(frame, inputs);
        QVERIFY(out.peakHoldDbm.has_value());
        QCOMPARE(out.peakHoldDbm->first(), -10.0f);
    }

    void aNewContextRestartsTheHoldAndTheBlobs()
    {
        DisplayExtrasRequest request;
        request.peakBlobs = DisplayExtrasRequest::PeakBlobs{};
        request.activePeakHold = DisplayExtrasRequest::ActivePeakHold{true, 2000, 6.0};
        DisplayExtrasProcessor station(request);
        DisplayCodecFrame frame;
        frame.context = {1, 1, -180.0f, 0.0f, 8, 8, 0};
        frame.traceDbm = {-120, -120, -40, -120, -120, -120, -120, -120};
        frame.waterfallDbm = frame.traceDbm;
        station.process(frame, {});
        QCOMPARE(station.activePeakHold().size(), 8);
        station.newContext();
        QCOMPARE(station.activePeakHold().size(), 0);
        QVERIFY(station.peakBlobs().blobs().isEmpty());
    }

    // R-DISP-HOLD: the same picture asked again keeps what its changed
    // settings do not invalidate.
    void aChangedRequestKeepsTheStateItsSettingsDoNotInvalidate()
    {
        DisplayExtrasRequest request;
        request.peakBlobs = DisplayExtrasRequest::PeakBlobs{};
        request.activePeakHold = DisplayExtrasRequest::ActivePeakHold{true, 2000, 6.0};
        request.noiseFloor = DisplayExtrasRequest::NoiseFloor{true, 0.0, false};
        request.waterfallLevels = DisplayExtrasRequest::WaterfallLevels{
            WaterfallLevelMode::Agc, -122.0, -62.0, 0};
        DisplayExtrasProcessor station(request);
        DisplayCodecFrame frame;
        frame.context = {1, 1, -180.0f, 0.0f, 8, 8, 0};
        frame.traceDbm = {-120, -120, -40, -120, -120, -120, -120, -120};
        frame.waterfallDbm = frame.traceDbm;
        frame.waterfallAdvance = true;
        DisplayExtrasInputs inputs;
        inputs.fps = 30;
        // Past the 500 ms display delay, so the blobs and the hold are shown.
        for (int index = 0; index < 60; ++index) {
            inputs.nowMs = index * 33;
            station.process(frame, inputs);
        }
        QCOMPARE(station.activePeakHold().size(), 8);
        QVERIFY(station.activePeakHold().active());
        QVERIFY(!station.peakBlobs().blobs().isEmpty());
        const float floorBefore = station.noiseFloor().lerpAverage();
        const std::pair<float, float> levelsBefore = station.waterfallLevels();
        QVERIFY(levelsBefore != std::make_pair(-122.0f, -62.0f));

        // Settings of the same state: all of it stays.
        DisplayExtrasRequest settings = request;
        settings.peakBlobs->holdMs = 900;
        settings.activePeakHold->fallDbPerSec = 12.0;
        settings.noiseFloor->shiftDb = 3.0;
        settings.waterfallLevels->lowDbm = -130.0;
        settings.calibrationOffsetDb = 1.0;
        station.updateRequest(settings);
        QCOMPARE(station.request(), settings);
        QCOMPARE(station.activePeakHold().size(), 8);
        QVERIFY(station.activePeakHold().active());
        QVERIFY(!station.peakBlobs().blobs().isEmpty());
        QCOMPARE(station.noiseFloor().lerpAverage(), floorBefore);
        QCOMPARE(station.waterfallLevels(), levelsBefore);

        // A different blob count starts the blobs again and nothing else.
        DisplayExtrasRequest fewer = settings;
        fewer.peakBlobs->count = 1;
        station.updateRequest(fewer);
        QVERIFY(station.peakBlobs().blobs().isEmpty());
        QCOMPARE(station.activePeakHold().size(), 8);
        QCOMPARE(station.waterfallLevels(), levelsBefore);

        // A different levels mode starts the levels again from those asked.
        DisplayExtrasRequest mode = fewer;
        mode.waterfallLevels->mode = WaterfallLevelMode::Clarity;
        station.updateRequest(mode);
        QCOMPARE(station.waterfallLevels(), std::make_pair(-130.0f, -62.0f));
        QVERIFY(station.clarity() != nullptr);
        QCOMPARE(station.activePeakHold().size(), 8);
        mode.waterfallLevels->mode = WaterfallLevelMode::Manual;
        station.updateRequest(mode);
        QVERIFY(station.clarity() == nullptr);

        // The hold switched off empties its row.
        DisplayExtrasRequest off = mode;
        off.activePeakHold->enabled = false;
        station.updateRequest(off);
        QCOMPARE(station.activePeakHold().size(), 0);
        QCOMPARE(station.noiseFloor().lerpAverage(), floorBefore);
    }

    // The merge of main: a level mode changed while keyed (updateRequest
    // reaches startWaterfallLevels). The new Clarity is told the key state
    // the processor holds at once, before any frame: a floor that arrives
    // first is the transmitter's and is not taken, and the un-key is
    // Clarity's first edge, so it runs the hold.
    void aLevelModeChangedWhileKeyedGivesTheNewClarityTheKeyState()
    {
        Run run(levelsRequest(WaterfallLevelMode::Agc), 3000);
        run.frames(200, -120.0f);
        run.inputs.mox = true;
        run.frames(10, -80.0f);
        QVERIFY(run.station.keyed());

        run.station.updateRequest(levelsRequest(WaterfallLevelMode::Clarity));
        const ClarityController* const clarity = run.station.clarity();
        QVERIFY(clarity != nullptr);
        QVERIFY(run.station.keyed());
        QVERIFY2(clarity->isTransmitting(), "the new Clarity was not told the pan is keyed");
        QVERIFY(run.station.levelFollower().keyed());
        // The levels start from those asked for.
        const std::pair<float, float> asked = run.station.waterfallLevels();
        QCOMPARE(asked, std::make_pair(-122.0f, -62.0f));
        // A floor read while keyed, before the next frame, moves nothing.
        const float floorBefore = clarity->smoothedFloor();
        run.station.feedNoiseFloor(-80.0f, run.nowMs + 10);
        QCOMPARE(clarity->smoothedFloor(), floorBefore);
        QVERIFY(run.station.waterfallLevels() == asked);
        run.frames(5, -80.0f);
        run.station.feedNoiseFloor(-80.0f, run.nowMs + 10);
        QVERIFY(run.station.waterfallLevels() == asked);

        // The un-key: one edge, and Clarity's hold from it.
        run.inputs.mox = false;
        const qint64 unKeyMs = run.nowMs + 33;
        run.frameAt(unKeyMs, -80.0f);
        QVERIFY(!clarity->isTransmitting());
        QVERIFY(clarity->inHold(unKeyMs + 1));
        run.station.feedNoiseFloor(-80.0f, unKeyMs + 500);
        QVERIFY(run.station.waterfallLevels() == asked);
        QVERIFY(!clarity->inHold(unKeyMs + 3000));

        // A mode changed while not keyed: the new Clarity is not keyed.
        Run receiving(levelsRequest(WaterfallLevelMode::Agc), 3000);
        receiving.frames(20, -120.0f);
        receiving.station.updateRequest(levelsRequest(WaterfallLevelMode::Clarity));
        QVERIFY(receiving.station.clarity() != nullptr);
        QVERIFY(!receiving.station.clarity()->isTransmitting());
    }

    // The same join: the per-band AGC levels belong to the mode that made
    // them. A mode changed while keyed clears them, so the un-key finds
    // nothing saved for the band and starts at the restart floor; without
    // a mode change the band's level comes back (the control).
    void aLevelModeChangedWhileKeyedClearsThePerBandAgcLevels()
    {
        // The control: no mode change, the band's level returns.
        Run kept(levelsRequest(WaterfallLevelMode::Agc), 3000);
        kept.frames(200, -120.0f);
        const float level = *kept.station.agcRunLevel();
        QVERIFY(std::abs(level + 120.0f) < 0.01f);
        kept.inputs.mox = true;
        kept.frames(10, -80.0f);
        kept.inputs.mox = false;
        kept.frames(1, -80.0f);
        QCOMPARE(*kept.station.agcRunLevel(), level);

        // Keyed on band 5 (its level is saved), then the mode goes to
        // NF-AGC while keyed.
        Run run(levelsRequest(WaterfallLevelMode::Agc), 3000);
        run.frames(200, -120.0f);
        QCOMPARE(*run.station.agcRunLevel(), level);
        run.inputs.mox = true;
        run.frames(10, -80.0f);
        run.station.updateRequest(levelsRequest(WaterfallLevelMode::NoiseFloorAgc));
        // The follower forgot its envelope and is still keyed, so the rows
        // of the key (-80 dBm) prime nothing: it stands at the restart
        // floor.
        QVERIFY(run.station.levelFollower().keyed());
        QVERIFY(!run.station.agcRunLevel().has_value());
        run.frames(10, -80.0f);
        QCOMPARE(*run.station.agcRunLevel(), WaterfallLevelFollower::kAgcRestartFloorDbm);
        // The un-key on band 5: nothing is saved for it any more.
        run.inputs.mox = false;
        run.frames(1, -80.0f);
        QVERIFY(run.station.agcRunLevel().has_value());
        QCOMPARE(*run.station.agcRunLevel(), WaterfallLevelFollower::kAgcRestartFloorDbm);
    }

    // ── The datagram ────────────────────────────────────────────────────

    void everySectionRoundTrips()
    {
        const DisplayCodecContext context = smallContext();
        const DisplayExtrasFrame frame = fullFrame(context);
        const QByteArray bytes = encodeDisplayExtras(frame, context);
        QVERIFY(bytes.startsWith("NSDX"));
        QCOMPARE(int(static_cast<quint8>(bytes.at(4))), 1);
        QCOMPARE(int(static_cast<quint8>(bytes.at(5))), 0x0F);
        const DisplayExtrasDecodeResult decoded = decodeDisplayExtras(bytes, context);
        QVERIFY(decoded.accepted);
        QCOMPARE(decoded.frame.endpointId, 9U);
        QCOMPARE(decoded.frame.contextGeneration, 4U);
        QCOMPARE(decoded.frame.encoderSequence, 77U);
        QCOMPARE(*decoded.frame.peakBlobs, *frame.peakBlobs);
        QCOMPARE(*decoded.frame.noiseFloorDbm, -118.75f);
        QCOMPARE(decoded.frame.waterfallLevelsDbm->first, -131.0f);
        QCOMPARE(decoded.frame.waterfallLevelsDbm->second, -58.5f);
        // The hold row travels as NSDC bins: half a quantum at most.
        const float halfQuantum = 180.0f / 255.0f / 2.0f;
        for (int i = 0; i < context.traceSamples; ++i) {
            QVERIFY(std::abs(decoded.frame.peakHoldDbm->at(i) - frame.peakHoldDbm->at(i))
                    <= halfQuantum + 1.0e-4f);
        }
        QCOMPARE(bytes.size(), 20 + 1 + 2 * 6 + (3 + 5 + 16) + 4 + 8);
    }

    void onlyTheSectionsAskedForTravel()
    {
        const DisplayCodecContext context = smallContext();
        DisplayExtrasFrame frame;
        frame.endpointId = context.endpointId;
        frame.contextGeneration = context.contextGeneration;
        frame.noiseFloorDbm = -110.0f;
        const QByteArray bytes = encodeDisplayExtras(frame, context);
        QCOMPARE(bytes.size(), 24);
        QCOMPARE(int(static_cast<quint8>(bytes.at(5))), 0x04);
        const auto decoded = decodeDisplayExtras(bytes, context);
        QVERIFY(decoded.accepted);
        QVERIFY(!decoded.frame.peakBlobs && !decoded.frame.peakHoldDbm
                && !decoded.frame.waterfallLevelsDbm);
        // Nothing asked for: nothing to send.
        DisplayExtrasFrame empty;
        empty.endpointId = context.endpointId;
        empty.contextGeneration = context.contextGeneration;
        QVERIFY(encodeDisplayExtras(empty, context).isEmpty());
    }

    void aHoldRowNotYetReachedIsSentAtTheFloor()
    {
        const DisplayCodecContext context = smallContext();
        DisplayExtrasFrame frame;
        frame.endpointId = context.endpointId;
        frame.contextGeneration = context.contextGeneration;
        frame.peakHoldDbm = QVector<float>(context.traceSamples,
                                           -std::numeric_limits<float>::infinity());
        const auto decoded = decodeDisplayExtras(encodeDisplayExtras(frame, context), context);
        QVERIFY(decoded.accepted);
        for (float value : *decoded.frame.peakHoldDbm) { QCOMPARE(value, -180.0f); }
    }

    void theLargestDatagramFitsTheDisplayChannel()
    {
        DisplayCodecContext context = smallContext();
        context.traceSamples = SpectrumEndpoint::kMaxPixels;
        context.waterfallSamples = SpectrumEndpoint::kMaxPixels;
        DisplayExtrasFrame frame;
        frame.endpointId = context.endpointId;
        frame.contextGeneration = context.contextGeneration;
        QVector<DisplayExtrasBlob> blobs;
        for (int i = 0; i < kDisplayExtrasMaxBlobs; ++i) {
            blobs.append({static_cast<quint16>(i * 100), -50.0f - static_cast<float>(i)});
        }
        frame.peakBlobs = blobs;
        QVector<float> hold(SpectrumEndpoint::kMaxPixels);
        for (int i = 0; i < hold.size(); ++i) {
            hold[i] = -180.0f + static_cast<float>((i * 37) % 180);
        }
        frame.peakHoldDbm = hold;
        frame.noiseFloorDbm = -120.0f;
        frame.waterfallLevelsDbm = std::make_pair(-130.0f, -60.0f);
        frame.noiseFloorFastAttack = true;
        const QByteArray bytes = encodeDisplayExtras(frame, context);
        QCOMPARE(quint32(bytes.size()),
                 displayExtrasWorstCaseBytes(kDisplayExtrasKnownSections,
                                             SpectrumEndpoint::kMaxPixels));
        QVERIFY(bytes.size() <= kDisplayExtrasMaxBytes);
        QVERIFY(bytes.size() <= DisplayCodecEncoder::kMaxEncodedBytes);
        QCOMPARE(bytes.size(), 20 + 121 + (3 + 5 * 32 + 4096) + 4 + 8 + 1);
    }

    void malformedDatagramsAreRefused_data()
    {
        QTest::addColumn<QByteArray>("bytes");
        QTest::addColumn<int>("reason");
        const DisplayCodecContext context = smallContext();
        const QByteArray good = encodeDisplayExtras(fullFrame(context), context);
        const auto with = [&good](int at, char value) {
            QByteArray bytes = good;
            bytes[at] = value;
            return bytes;
        };
        QTest::newRow("magic") << with(3, 'Y') << int(DisplayExtrasReason::BadMagic);
        QTest::newRow("version") << with(4, 2) << int(DisplayExtrasReason::UnsupportedVersion);
        QTest::newRow("unknown section") << with(5, 0x3F)
                                         << int(DisplayExtrasReason::UnknownSections);
        QTest::newRow("no section") << with(5, 0) << int(DisplayExtrasReason::Malformed);
        QTest::newRow("header size") << with(7, 21) << int(DisplayExtrasReason::Malformed);
        QTest::newRow("other endpoint") << with(11, 10)
                                        << int(DisplayExtrasReason::ContextMismatch);
        QTest::newRow("other generation") << with(15, 5)
                                          << int(DisplayExtrasReason::ContextMismatch);
        QTest::newRow("header cut") << good.left(19) << int(DisplayExtrasReason::Truncated);
        QTest::newRow("last byte cut") << good.chopped(1) << int(DisplayExtrasReason::Truncated);
        QTest::newRow("trailing byte") << good + QByteArray(1, '\0')
                                       << int(DisplayExtrasReason::Malformed);
        QTest::newRow("too many blobs") << with(20, 21) << int(DisplayExtrasReason::Malformed);
        QTest::newRow("blob off the row") << with(21, 1) << int(DisplayExtrasReason::Malformed);
        QByteArray notFinite = good;
        notFinite[good.size() - 12] = '\x7f';
        notFinite[good.size() - 11] = '\xc0';
        QTest::newRow("floor not finite") << notFinite << int(DisplayExtrasReason::Malformed);
        QTest::newRow("oversized") << QByteArray(kDisplayExtrasMaxBytes + 1, 'N')
                                   << int(DisplayExtrasReason::Oversized);
    }

    void malformedDatagramsAreRefused()
    {
        QFETCH(QByteArray, bytes);
        QFETCH(int, reason);
        const auto decoded = decodeDisplayExtras(bytes, smallContext());
        QVERIFY(!decoded.accepted);
        QCOMPARE(int(decoded.reason), reason);
    }

    // ── The subscription fields ─────────────────────────────────────────

    void everyFieldParses()
    {
        DisplayExtrasRequest request;
        QVERIFY(parseDisplayExtrasRequest(everyExtra(), request));
        QCOMPARE(request.peakBlobs->count, 3);
        QCOMPARE(request.activePeakHold->holdMs, 2000);
        QVERIFY(request.noiseFloor->enabled);
        QCOMPARE(request.waterfallLevels->mode, WaterfallLevelMode::Agc);
        QCOMPARE(*request.averageTimeMs, 30);
        // Absent, the waterfall takes averageTimeMs (the Core's fallback).
        QVERIFY(!request.waterfallAverageTimeMs.has_value());
        QVERIFY(request.noiseFloor->fastAttack);
        QCOMPARE(int(request.sections()), 0x1F);
        DisplayExtrasRequest none;
        QVERIFY(parseDisplayExtrasRequest(QJsonObject{}, none));
        QVERIFY(none.empty());
        QCOMPARE(int(none.sections()), 0);
        // Disabled hold and floor are fields, not sections.
        QJsonObject off = everyExtra();
        off.insert(QStringLiteral("activePeakHold"),
                   QJsonObject{{QStringLiteral("enabled"), false},
                               {QStringLiteral("holdMs"), 2000},
                               {QStringLiteral("fallDbPerSec"), 6.0}});
        off.insert(QStringLiteral("noiseFloor"),
                   QJsonObject{{QStringLiteral("enabled"), false},
                               {QStringLiteral("shiftDb"), 0.0}});
        QVERIFY(parseDisplayExtrasRequest(off, request));
        QCOMPARE(int(request.sections()), 0x09);
    }

    // waterfallAverageTimeMs is the waterfall's own time, in averageTimeMs's
    // range, and a field on its own: no section, charged as today.
    void theWaterfallAverageTimeParses()
    {
        DisplayExtrasRequest request;
        QVERIFY(parseDisplayExtrasRequest(
            withFields(everyExtra(), {{QStringLiteral("waterfallAverageTimeMs"), 700}}),
            request));
        QCOMPARE(*request.averageTimeMs, 30);
        QCOMPARE(*request.waterfallAverageTimeMs, 700);
        DisplayExtrasRequest alone;
        QVERIFY(parseDisplayExtrasRequest(
            QJsonObject{{QStringLiteral("waterfallAverageTimeMs"), 10}}, alone));
        QVERIFY(!alone.empty());
        QVERIFY(!alone.averageTimeMs.has_value());
        QCOMPARE(*alone.waterfallAverageTimeMs, 10);
        QCOMPARE(int(alone.sections()), 0);
        QVERIFY(displayExtrasSubscribeKeys().contains(QStringLiteral("waterfallAverageTimeMs")));
    }

    void outOfRangeFieldsAreRefused_data()
    {
        QTest::addColumn<QString>("key");
        QTest::addColumn<QJsonValue>("value");
        const auto blobs = [](int count, int holdMs, double fall) {
            return QJsonValue(QJsonObject{{QStringLiteral("count"), count},
                                          {QStringLiteral("holdMs"), holdMs},
                                          {QStringLiteral("fallDbPerSec"), fall},
                                          {QStringLiteral("insideOnly"), false}});
        };
        QTest::newRow("21 blobs") << QStringLiteral("peakBlobs") << blobs(21, 500, 6.0);
        QTest::newRow("no blobs") << QStringLiteral("peakBlobs") << blobs(0, 500, 6.0);
        QTest::newRow("50 ms hold") << QStringLiteral("peakBlobs") << blobs(3, 50, 6.0);
        QTest::newRow("half dB fall") << QStringLiteral("peakBlobs") << blobs(3, 500, 0.5);
        QTest::newRow("blob extra member")
            << QStringLiteral("peakBlobs")
            << QJsonValue(QJsonObject{{QStringLiteral("count"), 3}, {QStringLiteral("holdMs"), 500},
                                      {QStringLiteral("fallDbPerSec"), 6.0},
                                      {QStringLiteral("insideOnly"), false},
                                      {QStringLiteral("colour"), 1}});
        QTest::newRow("hold 99 ms")
            << QStringLiteral("activePeakHold")
            << QJsonValue(QJsonObject{{QStringLiteral("enabled"), true},
                                      {QStringLiteral("holdMs"), 99},
                                      {QStringLiteral("fallDbPerSec"), 6.0}});
        QTest::newRow("shift 13 dB")
            << QStringLiteral("noiseFloor")
            << QJsonValue(QJsonObject{{QStringLiteral("enabled"), true},
                                      {QStringLiteral("shiftDb"), 13.0}});
        QTest::newRow("unknown mode")
            << QStringLiteral("waterfallLevels")
            << QJsonValue(QJsonObject{{QStringLiteral("mode"), QStringLiteral("auto")},
                                      {QStringLiteral("lowDbm"), -120.0},
                                      {QStringLiteral("highDbm"), -60.0},
                                      {QStringLiteral("offsetDb"), 0}});
        QTest::newRow("offset 61 dB")
            << QStringLiteral("waterfallLevels")
            << QJsonValue(QJsonObject{{QStringLiteral("mode"), QStringLiteral("noiseFloorAgc")},
                                      {QStringLiteral("lowDbm"), -120.0},
                                      {QStringLiteral("highDbm"), -60.0},
                                      {QStringLiteral("offsetDb"), 61}});
        QTest::newRow("normalize as number") << QStringLiteral("normalize") << QJsonValue(1);
        QTest::newRow("calibration 31 dB") << QStringLiteral("calibrationOffsetDb")
                                           << QJsonValue(31.0);
        QTest::newRow("average 0 ms") << QStringLiteral("averageTimeMs") << QJsonValue(0);
        QTest::newRow("average 10000 ms") << QStringLiteral("averageTimeMs") << QJsonValue(10000);
        QTest::newRow("average fraction") << QStringLiteral("averageTimeMs") << QJsonValue(30.5);
        QTest::newRow("waterfall average 0 ms") << QStringLiteral("waterfallAverageTimeMs")
                                                << QJsonValue(0);
        QTest::newRow("waterfall average 10000 ms") << QStringLiteral("waterfallAverageTimeMs")
                                                    << QJsonValue(10000);
        QTest::newRow("waterfall average fraction") << QStringLiteral("waterfallAverageTimeMs")
                                                    << QJsonValue(700.5);
    }

    void outOfRangeFieldsAreRefused()
    {
        QFETCH(QString, key);
        QFETCH(QJsonValue, value);
        QJsonObject subscribe = everyExtra();
        subscribe.insert(key, value);
        DisplayExtrasRequest request;
        request.averageTimeMs = 1234;
        QVERIFY(!parseDisplayExtrasRequest(subscribe, request));
        QCOMPARE(*request.averageTimeMs, 1234);
    }

    // The two averaging times are taken from 1 ms, the floor Setup
    // description version 28 gives a phone and the desktop's own. (Before
    // it, a request below 10 ms was refused.)
    void averageTimesAreTakenFromOneMs()
    {
        for (const int timeMs : {1, 5, 9, 10, 9999}) {
            QJsonObject subscribe = everyExtra();
            subscribe.insert(QStringLiteral("averageTimeMs"), timeMs);
            subscribe.insert(QStringLiteral("waterfallAverageTimeMs"), timeMs);
            DisplayExtrasRequest request;
            QVERIFY2(parseDisplayExtrasRequest(subscribe, request),
                     qPrintable(QString::number(timeMs)));
            QCOMPARE(*request.averageTimeMs, timeMs);
            QCOMPARE(*request.waterfallAverageTimeMs, timeMs);
        }
    }

    // ── Through a real session ──────────────────────────────────────────

    void theCoreKeepsDisplayExtrasInTheOriginalMinorElevenBlock()
    {
        StationCapabilities caps;
        caps.radioIdentityEntries = true;
        caps.stationCatalogVersion = 1;
        caps.displayExtrasVersion = 2;
        const QList<MirrorUpdate> updates = caps.toUpdates();
        // Preserve the deployed contiguous block without assuming that
        // later capabilities cannot be appended after it.
        const QList<QByteArray> originalBlock{
            "displayExtrasVersion", "transmitSettingsVersion", "bandSelectVersion",
            "meterReadingsVersion", "dspInfoVersion", "recordStreamVersion",
            "stationRadiosVersion", "txDisplayVersion", "displayClockVersion",
            "controlChannelVersion", "txMonitorAudioVersion", "stationFreedvVersion",
            "mediaReplaceVersion", "controlSwitchVersion", "relayAllowed",
            "supportBundleVersion", "mediaTunnelVersion", "mediaRelayRoutingVersion"};
        qsizetype first = -1;
        for (qsizetype i = 0; i < updates.size(); ++i) {
            if (updates.at(i).name == originalBlock.first()) {
                QVERIFY(first < 0);
                first = i;
            }
        }
        QVERIFY(first >= 0);
        QVERIFY(first + originalBlock.size() <= updates.size());
        for (qsizetype i = 0; i < originalBlock.size(); ++i) {
            QCOMPARE(updates.at(first + i).name, originalBlock.at(i));
        }
        QCOMPARE(StationCapabilities::fromUpdates(updates).displayExtrasVersion, 2);
        // An older peer's block (no minor-11 entries) carries none.
        StationCapabilities older;
        older.displayExtrasVersion = 1;
        for (const MirrorUpdate& update : older.toUpdates()) {
            QVERIFY(update.name != QByteArray("displayExtrasVersion"));
        }
        Harness harness;
        // 2 (R-IOS-27, R-IOS-06): the extras and clarity-retune; 3: the peak
        // hold's hold time and activePeakHold.onTx; 4: noiseFloor.fastAttack
        // and the noise floor state section.
        QCOMPARE(harness.server.displayExtrasVersion(), 4);
        QVERIFY(!harness.server.displayExtrasAvailable()); // no session yet
        QVERIFY(harness.establishSession());
        QVERIFY(harness.server.displayExtrasAvailable());
        QCOMPARE(harness.client.capabilities().displayExtrasVersion, 4);
        harness.finish();
    }

    // Version 4: noiseFloor may ask for the fast-attack state with an
    // optional `fastAttack` member; the state travels in section 0x10, one
    // byte, bit 0 the fast attack (the desktop's grey NF line).
    void noiseFloorFastAttackStateParsesAndTravels()
    {
        DisplayExtrasRequest request;
        QVERIFY(parseDisplayExtrasRequest(
            QJsonObject{{QStringLiteral("noiseFloor"),
                         QJsonObject{{QStringLiteral("enabled"), true},
                                     {QStringLiteral("shiftDb"), 0.0},
                                     {QStringLiteral("fastAttack"), true}}}},
            request));
        QCOMPARE(int(request.sections()), 0x04 | 0x10);
        QVERIFY(!parseDisplayExtrasRequest(
            QJsonObject{{QStringLiteral("noiseFloor"),
                         QJsonObject{{QStringLiteral("enabled"), true},
                                     {QStringLiteral("shiftDb"), 0.0},
                                     {QStringLiteral("fastAttack"), 1}}}},
            request));
        // Without the member, or false, no state section: older apps keep
        // exactly today's datagrams.
        QVERIFY(parseDisplayExtrasRequest(
            QJsonObject{{QStringLiteral("noiseFloor"),
                         QJsonObject{{QStringLiteral("enabled"), true},
                                     {QStringLiteral("shiftDb"), 0.0}}}},
            request));
        QCOMPARE(int(request.sections()), 0x04);

        // A hand-built datagram: noise floor -118.75 and state 1.
        const DisplayCodecContext context = smallContext();
        QByteArray bytes("NSDX");
        bytes.append(char(1));
        bytes.append(char(0x04 | 0x10));
        bytes.append(char(0));
        bytes.append(char(20));
        for (const quint32 value : {quint32(context.endpointId),
                                    quint32(context.contextGeneration), quint32(5)}) {
            for (int shift = 24; shift >= 0; shift -= 8) {
                bytes.append(char((value >> shift) & 0xFF));
            }
        }
        quint32 floorBits = 0;
        const float floor = -118.75f;
        std::memcpy(&floorBits, &floor, sizeof floorBits);
        for (int shift = 24; shift >= 0; shift -= 8) {
            bytes.append(char((floorBits >> shift) & 0xFF));
        }
        bytes.append(char(1));
        const DisplayExtrasDecodeResult decoded = decodeDisplayExtras(bytes, context);
        QVERIFY(decoded.accepted);
        QCOMPARE(*decoded.frame.noiseFloorDbm, -118.75f);
        // The encoder writes the same bytes.
        QCOMPARE(encodeDisplayExtras(decoded.frame, context), bytes);
        // Any other bit in the state byte is refused.
        QByteArray other = bytes;
        other[other.size() - 1] = char(2);
        QVERIFY(!decodeDisplayExtras(other, context).accepted);
        // A missing state byte is truncated.
        QVERIFY(!decodeDisplayExtras(bytes.left(bytes.size() - 1), context).accepted);
    }

    void budgetPacingSendsExtrasWithoutPeakHold_data()
    {
        QTest::addColumn<QJsonObject>("extras");
        QTest::addColumn<int>("expectedSections");
        QTest::newRow("waterfall-only-agc") << levels(QStringLiteral("agc"))
                                          << int(kDisplayExtrasWaterfallLevels);
        QTest::newRow("waterfall-only-noise-floor-agc") << levels(QStringLiteral("noiseFloorAgc"))
                                                      << int(kDisplayExtrasWaterfallLevels);
        QJsonObject withoutHold = everyExtra();
        withoutHold.remove(QStringLiteral("activePeakHold"));
        QTest::newRow("all-extras-without-peak-hold") << withoutHold
            << int(kDisplayExtrasKnownSections & ~kDisplayExtrasPeakHold);
    }

    void budgetPacingSendsExtrasWithoutPeakHold()
    {
#ifndef HAVE_FFTW3
        QSKIP("FFTEngine has no FFTW3 backend in this build");
#endif
        QFETCH(QJsonObject, extras);
        QFETCH(int, expectedSections);
        Harness harness;
        QVERIFY(harness.server.setDisplayBudgetLimits({1'000'000, 1'000'000, 1}));
        harness.server.setDisplayBudgetEnforcementEnabled(true);
        QVERIFY(harness.establishSession());
        QVERIFY(harness.server.displayBudgetAvailable(harness.client.sessionEpoch()));
        QSignalSpy controls(&harness.client, &StationClient::mediaControlReceived);
        QVERIFY(harness.startReadyPeer());
        const double centreHz = harness.radio.streamCentreHz(harness.streamIndex);
        QVERIFY(harness.client.sendMediaControl(
            withFields(subscription(7, harness.sliceId, centreHz), extras),
            harness.client.sessionEpoch()));
        QTRY_COMPARE_WITH_TIMEOUT(harness.controller.activeEndpointCount(), 1, 2'000);
        QTRY_VERIFY_WITH_TIMEOUT(
            (harness.feedRadio(), !messageFor(controls, QStringLiteral("context"), 7).isEmpty()),
            2'000);
        const DisplayCodecContext context = contextFrom(messageFor(controls, QStringLiteral("context"), 7));
        const auto plainCost = spectrumDisplayCost(128, 60, false);
        QVERIFY(plainCost.has_value());
        // Scalar/blob extras consume bytes, but add no peak-hold sample plane.
        QCOMPARE(harness.controller.acceptedDisplayCharge().spectrumSampleUnitsPerSecond,
                 plainCost->charge.spectrumSampleUnitsPerSecond);
        QTRY_VERIFY_WITH_TIMEOUT(
            (harness.feedRadio(), !withMagic(harness.mediaTransport->displays, "NSDC").isEmpty()),
            2'000);
        const bool extrasArrived = QTest::qWaitFor([&harness] {
            harness.feedRadio();
            return withMagic(harness.mediaTransport->displays, "NSDX").size() >= 3;
        }, 2'000);
        QVERIFY2(extrasArrived, qPrintable(QStringLiteral("budget-enabled NSDC=%1 NSDX=%2")
            .arg(withMagic(harness.mediaTransport->displays, "NSDC").size())
            .arg(withMagic(harness.mediaTransport->displays, "NSDX").size())));

        int checked = 0;
        const QList<QByteArray>& displays = harness.mediaTransport->displays;
        for (int i = 0; i < displays.size(); ++i) {
            if (!displays[i].startsWith("NSDX")) { continue; }
            const auto decoded = decodeDisplayExtras(displays[i], context);
            QVERIFY(decoded.accepted);
            QCOMPARE(int(decoded.frame.sections()), expectedSections);
            QVERIFY(!decoded.frame.peakHoldDbm.has_value());
            QVERIFY(decoded.frame.waterfallLevelsDbm.has_value());
            QVERIFY(std::isfinite(decoded.frame.waterfallLevelsDbm->first));
            QVERIFY(decoded.frame.waterfallLevelsDbm->second > decoded.frame.waterfallLevelsDbm->first);
            QVERIFY(i > 0 && displays[i - 1].startsWith("NSDC"));
            QCOMPARE(sequenceOfNsdc(displays[i - 1]), decoded.frame.encoderSequence);
            ++checked;
        }
        QVERIFY(checked >= 3);
        harness.finish();
    }

    void anEndpointThatAsksGetsExtrasBesideEachFrame()
    {
#ifndef HAVE_FFTW3
        QSKIP("FFTEngine has no FFTW3 backend in this build");
#endif
        Harness harness;
        QVERIFY(harness.establishSession());
        QSignalSpy controls(&harness.client, &StationClient::mediaControlReceived);
        QVERIFY(harness.startReadyPeer());
        const double centreHz = harness.radio.streamCentreHz(harness.streamIndex);
        // Endpoint 7 asks for every extra; endpoint 8, on the same source,
        // asks for none.
        QVERIFY(harness.client.sendMediaControl(
            withFields(subscription(7, harness.sliceId, centreHz), everyExtra()),
            harness.client.sessionEpoch()));
        QVERIFY(harness.client.sendMediaControl(subscription(8, harness.sliceId, centreHz),
                                                harness.client.sessionEpoch()));
        QTRY_COMPARE(harness.controller.activeEndpointCount(), 2);
        // The Core charges endpoint 7 for its extras.
        const DisplayBudgetCharge charge = harness.controller.acceptedDisplayCharge();
        const auto plainCost = spectrumDisplayCost(128, 60, false);
        QVERIFY(plainCost.has_value());
        QCOMPARE(charge.messagesPerSecond, 3U * 60U);
        QCOMPARE(charge.applicationBytesPerSecond,
                 2 * plainCost->charge.applicationBytesPerSecond
                     + quint64(displayExtrasWorstCaseBytes(kDisplayExtrasKnownSections, 128)) * 60);

        QTRY_VERIFY_WITH_TIMEOUT(
            (harness.feedRadio(), !messageFor(controls, QStringLiteral("context"), 7).isEmpty()
             && !messageFor(controls, QStringLiteral("context"), 8).isEmpty()),
            10'000);
        const DisplayCodecContext context7 =
            contextFrom(messageFor(controls, QStringLiteral("context"), 7));
        const DisplayCodecContext context8 =
            contextFrom(messageFor(controls, QStringLiteral("context"), 8));
        for (int i = 0; i < 12; ++i) {
            harness.feedRadio(0.1 + 0.01 * i);
            QTest::qWait(20);
        }
        QTRY_VERIFY(withMagic(harness.mediaTransport->displays, "NSDX").size() >= 3);
        // The peak hold section follows once the first reset's 500 ms
        // display delay (counted in the Core's frames, Thetis display.cs:
        // 859-877 [v2.10.3.15]) is over.
        const auto fullExtras = [&harness, &context7]() {
            for (const QByteArray& bytes : withMagic(harness.mediaTransport->displays, "NSDX")) {
                const auto decoded = decodeDisplayExtras(bytes, context7);
                if (decoded.accepted && decoded.frame.sections() == kDisplayExtrasKnownSections) {
                    return true;
                }
            }
            return false;
        };
        QTRY_VERIFY_WITH_TIMEOUT((harness.feedRadio(0.2), QTest::qWait(20), fullExtras()), 20'000);
        const QList<QByteArray> displays = harness.mediaTransport->displays;

        // Every NSDX datagram decodes against endpoint 7's context, goes
        // right after endpoint 7's frame with the same sequence, and fits.
        int checked = 0;
        bool seenPeakHold = false;
        for (int i = 0; i < displays.size(); ++i) {
            if (!displays.at(i).startsWith("NSDX")) { continue; }
            const auto decoded = decodeDisplayExtras(displays.at(i), context7);
            QVERIFY(decoded.accepted);
            // Every section, or every one but the peak hold while it is
            // held back after the first reset; never the other way round.
            const int sections = int(decoded.frame.sections());
            QVERIFY2(sections == 0x1F || (sections == 0x1D && !seenPeakHold),
                     qPrintable(QString::number(sections)));
            seenPeakHold = seenPeakHold || sections == 0x1F;
            // The fast-attack state travels beside the floor (version 4).
            QVERIFY(decoded.frame.noiseFloorFastAttack.has_value());
            QVERIFY(displays.at(i).size() <= kDisplayExtrasMaxBytes);
            QVERIFY(i > 0 && displays.at(i - 1).startsWith("NSDC"));
            DisplayCodecDecoder check;
            const QByteArray& frame = displays.at(i - 1);
            // The frame before it is endpoint 7's (bytes 8..11) with the
            // extras' sequence.
            QCOMPARE(int(static_cast<quint8>(frame.at(11))), 7);
            QCOMPARE(sequenceOfNsdc(frame), decoded.frame.encoderSequence);
            QVERIFY(decoded.frame.peakBlobs->size() <= 3);
            if (decoded.frame.peakHoldDbm) {
                QCOMPARE(decoded.frame.peakHoldDbm->size(), int(context7.traceSamples));
            }
            ++checked;
        }
        QVERIFY(checked >= 3);
        QVERIFY(seenPeakHold);
        // Nothing but NSDC for endpoint 8.
        for (const QByteArray& bytes : displays) {
            if (bytes.startsWith("NSDX")) {
                QVERIFY(!decodeDisplayExtras(bytes, context8).accepted);
            }
        }
        harness.finish();
    }

    // A subscription without any extras field gets exactly what the Core
    // sent before display extras existed: the same charge, the same display
    // control messages and every datagram byte for byte, against the golden
    // recorded from 535dd412 with the same fixed run (OlderPeerDisplayRun.h).
    void anEndpointThatDoesNotAskGetsTodaysBytes()
    {
#ifndef HAVE_FFTW3
        QSKIP("FFTEngine has no FFTW3 backend in this build");
#endif
        using namespace NereusSDR::Test::OlderPeerDisplay;
        QFile file(QStringLiteral(NEREUS_TEST_DATA_DIR "/display_extras_older_peer_frames.json"));
        QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(file.fileName()));
        const QJsonObject golden = QJsonDocument::fromJson(file.readAll()).object();
        QCOMPARE(golden.value(QStringLiteral("recordedFrom")).toString(),
                 QStringLiteral("535dd412"));
        const Recording expected = Recording::fromJson(golden);
        QVERIFY(expected.displays.size() >= 10);

        Recording actual;
        run(actual);
        if (QTest::currentTestFailed()) {
            return;
        }
        QCOMPARE(actual.charge, expected.charge);
        QCOMPARE(actual.controls.size(), expected.controls.size());
        for (qsizetype i = 0; i < expected.controls.size(); ++i) {
            QVERIFY2(actual.controls.at(i) == expected.controls.at(i),
                     qPrintable(QStringLiteral("control message %1 differs").arg(i)));
        }
        QCOMPARE(actual.displays.size(), expected.displays.size());
        for (qsizetype i = 0; i < expected.displays.size(); ++i) {
            QVERIFY2(actual.displays.at(i) == expected.displays.at(i),
                     qPrintable(QStringLiteral("datagram %1 differs").arg(i)));
        }
    }

    // averageTimeMs sets the trace's constant, waterfallAverageTimeMs the
    // waterfall's; without waterfallAverageTimeMs the waterfall takes
    // averageTimeMs's, and without either both keep their plane's.
    void theWaterfallTakesItsOwnAverageTimeOrTheSpectrums()
    {
#ifndef HAVE_FFTW3
        QSKIP("FFTEngine has no FFTW3 backend in this build");
#endif
        Harness harness;
        QVERIFY(harness.establishSession());
        QVERIFY(harness.startReadyPeer());
        const double centreHz = harness.radio.streamCentreHz(harness.streamIndex);
        // The subscription asks for 60 fps; plane() asks for alpha 0.
        QVERIFY(harness.client.sendMediaControl(
            withFields(subscription(7, harness.sliceId, centreHz),
                       {{QStringLiteral("averageTimeMs"), 120},
                        {QStringLiteral("waterfallAverageTimeMs"), 700}}),
            harness.client.sessionEpoch()));
        QVERIFY(harness.client.sendMediaControl(
            withFields(subscription(8, harness.sliceId, centreHz),
                       {{QStringLiteral("averageTimeMs"), 120}}),
            harness.client.sessionEpoch()));
        QVERIFY(harness.client.sendMediaControl(
            withFields(subscription(9, harness.sliceId, centreHz),
                       {{QStringLiteral("waterfallAverageTimeMs"), 700}}),
            harness.client.sessionEpoch()));
        QVERIFY(harness.client.sendMediaControl(subscription(10, harness.sliceId, centreHz),
                                                harness.client.sessionEpoch()));
        QTRY_COMPARE(harness.controller.activeEndpointCount(), 4);
        const double spectrum = averageAlphaForTimeMs(120, 60);
        const double waterfall = averageAlphaForTimeMs(700, 60);
        QVERIFY(spectrum != waterfall);

        // Present: each plane its own.
        const auto both = harness.controller.spectrumAveraging(7);
        QVERIFY(both.has_value());
        QCOMPARE(both->traceAlpha, spectrum);
        QCOMPARE(both->waterfallAlpha, waterfall);
        QCOMPARE(both->traceWindowFrames, 7);
        QCOMPARE(both->waterfallWindowFrames, 42);
        // Absent: the waterfall falls back to averageTimeMs.
        const auto fallback = harness.controller.spectrumAveraging(8);
        QVERIFY(fallback.has_value());
        QCOMPARE(fallback->traceAlpha, spectrum);
        QCOMPARE(fallback->waterfallAlpha, spectrum);
        QCOMPARE(fallback->traceWindowFrames, 7);
        QCOMPARE(fallback->waterfallWindowFrames, 7);
        // The waterfall's alone leaves the trace its plane's constant.
        const auto waterfallOnly = harness.controller.spectrumAveraging(9);
        QVERIFY(waterfallOnly.has_value());
        QCOMPARE(waterfallOnly->traceAlpha, 0.0);
        QCOMPARE(waterfallOnly->waterfallAlpha, waterfall);
        QCOMPARE(waterfallOnly->traceWindowFrames, 2);
        QCOMPARE(waterfallOnly->waterfallWindowFrames, 42);
        // Neither: both planes' own.
        const auto neither = harness.controller.spectrumAveraging(10);
        QVERIFY(neither.has_value());
        QCOMPARE(neither->traceAlpha, 0.0);
        QCOMPARE(neither->waterfallAlpha, 0.0);
        QCOMPARE(neither->traceWindowFrames, 2);
        QCOMPARE(neither->waterfallWindowFrames, 2);
        // No field asks for a section, so every endpoint is charged as today.
        const auto plainCost = spectrumDisplayCost(128, 60, false);
        QVERIFY(plainCost.has_value());
        QCOMPARE(harness.controller.acceptedDisplayCharge().applicationBytesPerSecond,
                 4 * plainCost->charge.applicationBytesPerSecond);
        // Renew the same real endpoint at its newly accepted cadence.
        QVERIFY(harness.client.sendMediaControl(
            withFields(subscription(7, harness.sliceId, centreHz),
                       {{QStringLiteral("revision"), 2},
                        {QStringLiteral("fps"), 30},
                        {QStringLiteral("averageTimeMs"), 120},
                        {QStringLiteral("waterfallAverageTimeMs"), 700}}),
            harness.client.sessionEpoch()));
        QTRY_VERIFY(harness.controller.spectrumAveraging(7).has_value()
                    && harness.controller.spectrumAveraging(7)->traceAlpha
                        == averageAlphaForTimeMs(120, 30));
        const auto renewed = harness.controller.spectrumAveraging(7);
        QCOMPARE(renewed->traceWindowFrames, 4);
        QCOMPARE(renewed->waterfallWindowFrames, 21);
        QCOMPARE(renewed->waterfallAlpha, double(averageAlphaForTimeMs(700, 30)));
        QVERIFY(harness.client.sendMediaControl(
            withFields(subscription(7, harness.sliceId, centreHz),
                       {{QStringLiteral("revision"), 3},
                        {QStringLiteral("fps"), 30},
                        {QStringLiteral("averageTimeMs"), 9999},
                        {QStringLiteral("waterfallAverageTimeMs"), 1}}),
            harness.client.sessionEpoch()));
        QTRY_VERIFY(harness.controller.spectrumAveraging(7).has_value()
                    && harness.controller.spectrumAveraging(7)->traceAlpha
                        == averageAlphaForTimeMs(9999, 30));
        const auto retimed = harness.controller.spectrumAveraging(7);
        QCOMPARE(retimed->traceWindowFrames, 60);
        QCOMPARE(retimed->waterfallWindowFrames, 2);
        harness.finish();
    }

    void calibrationMovesTheFramesBins()
    {
#ifndef HAVE_FFTW3
        QSKIP("FFTEngine has no FFTW3 backend in this build");
#endif
        Harness harness;
        QVERIFY(harness.establishSession());
        QSignalSpy controls(&harness.client, &StationClient::mediaControlReceived);
        QVERIFY(harness.startReadyPeer());
        const double centreHz = harness.radio.streamCentreHz(harness.streamIndex);
        // The widest interval a request may name, so the tone's skirts and
        // floor are not clipped. Neither plane holds anything: the two
        // endpoints are compared on one frame, whatever each was sent before.
        const QJsonObject wide{{QStringLiteral("minDbm"), -400.0},
                               {QStringLiteral("maxDbm"), 100.0},
                               {QStringLiteral("trace"), planeWithoutHold()},
                               {QStringLiteral("waterfall"), planeWithoutHold()}};
        QVERIFY(harness.client.sendMediaControl(
            withFields(withFields(subscription(7, harness.sliceId, centreHz), wide),
                       {{QStringLiteral("calibrationOffsetDb"), 20.0}}),
            harness.client.sessionEpoch()));
        // Endpoint 7 is sent frames before endpoint 8 exists, so the
        // comparison below covers an endpoint that started earlier. On a
        // loaded machine that happened by itself now and then, and with
        // planes that held each pixel's maximum the two traces differed.
        const auto sentTo = [&harness](quint8 endpoint) {
            int count = 0;
            for (const QByteArray& bytes : harness.mediaTransport->displays) {
                if (!bytes.startsWith("NSDX") && static_cast<quint8>(bytes.at(11)) == endpoint) {
                    ++count;
                }
            }
            return count;
        };
        QTRY_VERIFY_WITH_TIMEOUT((harness.feedRadio(), sentTo(7) >= 3), 10'000);
        QVERIFY(harness.client.sendMediaControl(
            withFields(subscription(8, harness.sliceId, centreHz), wide),
            harness.client.sessionEpoch()));
        QTRY_COMPARE(harness.controller.activeEndpointCount(), 2);
        QTRY_VERIFY_WITH_TIMEOUT(
            (harness.feedRadio(), !messageFor(controls, QStringLiteral("context"), 7).isEmpty()
             && !messageFor(controls, QStringLiteral("context"), 8).isEmpty()),
            10'000);
        // One source frame reaches both endpoints: compare their traces for
        // that frame. Every datagram is decoded in order from the first, so
        // each endpoint's decoder starts at its keyframe; frames are kept by
        // the source frame's timestamp until both endpoints have one.
        QMap<quint32, QMap<quint64, DisplayCodecFrame>> frames;
        QMap<quint32, DisplayCodecDecoder> decoders;
        int cursor = 0;
        bool extrasSeen = false;
        quint64 shared = 0;
        const auto sameFrame = [&] {
            harness.feedRadio(0.15);
            QTest::qWait(20);
            const QList<QByteArray>& displays = harness.mediaTransport->displays;
            for (; cursor < displays.size(); ++cursor) {
                const QByteArray& bytes = displays.at(cursor);
                if (bytes.startsWith("NSDX")) {
                    extrasSeen = true; // no section asked for: never
                    continue;
                }
                const quint32 endpoint = static_cast<quint8>(bytes.at(11));
                const auto decoded = decoders[endpoint].decode(bytes);
                if (decoded.disposition == DisplayCodecDisposition::Accepted) {
                    frames[endpoint].insert(decoded.frame.producerTimestamp, decoded.frame);
                }
            }
            for (auto it = frames[7].cbegin(); it != frames[7].cend(); ++it) {
                if (frames[8].contains(it.key())) {
                    shared = it.key();
                    return true;
                }
            }
            return false;
        };
        // Feeding has side effects, so wait on it once rather than with a
        // QTRY macro, which evaluates the condition again at the end.
        QVERIFY(QTest::qWaitFor(sameFrame, 30'000));
        QVERIFY(!extrasSeen);
        const DisplayCodecFrame calibrated = frames[7].value(shared);
        const DisplayCodecFrame plainFrame = frames[8].value(shared);
        const float quantum = 500.0f / 255.0f;
        int moved = 0;
        QCOMPARE(calibrated.traceDbm.size(), plainFrame.traceDbm.size());
        for (int x = 0; x < plainFrame.traceDbm.size(); ++x) {
            const float plain = plainFrame.traceDbm.at(x);
            // Clipped at either end of -400..100 before or after the move.
            if (plain + 20.0f > 98.0f || plain < -398.0f) { continue; }
            QVERIFY(std::abs(calibrated.traceDbm.at(x) - (plain + 20.0f)) <= quantum + 1.0e-3f);
            ++moved;
        }
        QVERIFY(moved > 100);
        harness.finish();
    }

    // Thetis applies the 1 Hz normalise only with the Average, Sample and
    // RMS pan detectors (specHPSDR.cs:288-294 [v2.10.3.15]); the desktop
    // does, so the Core must too: with Peak a normalise request moves
    // nothing, with Average it moves the trace by -10 log10(bin width).
    // The state section reports the Core's own noise floor follower: a band
    // change puts it into fast attack (the desktop's grey line), and it
    // leaves once the floor has settled, as the desktop's does.
    void theNoiseFloorStateFollowsFastAttack()
    {
        DisplayExtrasRequest request;
        request.noiseFloor = DisplayExtrasRequest::NoiseFloor{true, 0.0, true};
        DisplayExtrasProcessor station(request);
        const QVector<QVector<float>> rows = recordedRows(300, 64);
        DisplayExtrasInputs inputs;
        inputs.fps = 30;
        inputs.sliceHz = 14200000.0;
        inputs.band = 5;
        bool sawSettled = false;
        for (int i = 0; i < 200; ++i) {
            DisplayCodecFrame frame;
            frame.context = {1, 1, -180.0f, 0.0f, 64, 64, 0};
            frame.traceDbm = rows.at(i);
            frame.waterfallDbm = rows.at(i);
            inputs.nowMs = static_cast<qint64>(i) * 33;
            const DisplayExtrasFrame out = station.process(frame, inputs);
            QVERIFY(out.noiseFloorFastAttack.has_value());
            QCOMPARE(*out.noiseFloorFastAttack, station.noiseFloor().fastAttack());
            sawSettled = sawSettled || !*out.noiseFloorFastAttack;
        }
        QVERIFY(sawSettled);
        inputs.band = 6;   // a band change
        DisplayCodecFrame frame;
        frame.context = {1, 1, -180.0f, 0.0f, 64, 64, 0};
        frame.traceDbm = rows.at(200);
        frame.waterfallDbm = rows.at(200);
        inputs.nowMs += 33;
        QVERIFY(*station.process(frame, inputs).noiseFloorFastAttack);
        // Without the member, no state.
        DisplayExtrasRequest plain;
        plain.noiseFloor = DisplayExtrasRequest::NoiseFloor{true, 0.0};
        DisplayExtrasProcessor older(plain);
        QVERIFY(!older.process(frame, inputs).noiseFloorFastAttack.has_value());
    }

    void normaliseFollowsTheTraceDetector()
    {
#ifndef HAVE_FFTW3
        QSKIP("FFTEngine has no FFTW3 backend in this build");
#endif
        QJsonObject average = plane();
        average.insert(QStringLiteral("detector"), static_cast<int>(SpectrumDetectorMode::Average));
        const QJsonObject peakTrace{{QStringLiteral("trace"), plane()}};
        const QJsonObject averageTrace{{QStringLiteral("trace"), average}};
        const double peak = normaliseShiftFor(peakTrace);
        const double avg = normaliseShiftFor(averageTrace);
        QVERIFY2(std::abs(peak) <= 500.0 / 255.0, qPrintable(QString::number(peak)));
        QVERIFY2(avg < -5.0, qPrintable(QString::number(avg)));
    }

    void aRequestTheCoreCannotReadIsRefused()
    {
        Harness harness;
        QVERIFY(harness.establishSession());
        QSignalSpy controls(&harness.client, &StationClient::mediaControlReceived);
        QVERIFY(harness.startReadyPeer());
        const double centreHz = harness.radio.streamCentreHz(harness.streamIndex);
        QVERIFY(harness.client.sendMediaControl(
            withFields(subscription(7, harness.sliceId, centreHz),
                       {{QStringLiteral("averageTimeMs"), 0}}),
            harness.client.sessionEpoch()));
        // A request the Core can read, sent after it, is taken; the first
        // never became an endpoint.
        QVERIFY(harness.client.sendMediaControl(
            withFields(subscription(9, harness.sliceId, centreHz),
                       {{QStringLiteral("averageTimeMs"), 50}}),
            harness.client.sessionEpoch()));
        QTRY_COMPARE(harness.controller.activeEndpointCount(), 1);
        QVERIFY(!harness.controller.spectrumGrant(7).has_value());
        QVERIFY(harness.controller.spectrumGrant(9).has_value());
        Q_UNUSED(controls);
        harness.finish();
    }

    // R-IOS-27, R-IOS-06: clarity-retune does for one endpoint what the
    // desktop's Re-tune button does for its pan (ClarityController::
    // retuneNow): the next floor re-anchors the smoothing and the levels at
    // once instead of easing toward it, inside the poll window. Observed the
    // way tst_clarity_controller observes retuneNow: a desktop controller fed
    // the same floors and re-tuned at the same point ends where the
    // endpoint's does. Another Clarity endpoint keeps easing.
    void clarityRetuneReTunesThatEndpointOnly()
    {
        Harness harness;
        QVERIFY(harness.establishSession());
        QSignalSpy controls(&harness.client, &StationClient::mediaControlReceived);
        QVERIFY(harness.startReadyPeer());
        const double centreHz = harness.radio.streamCentreHz(harness.streamIndex);
        const quint32 epoch = harness.client.sessionEpoch();
        QVERIFY(harness.client.sendMediaControl(
            withFields(subscription(7, harness.sliceId, centreHz), levels(QStringLiteral("clarity"))),
            epoch));
        QVERIFY(harness.client.sendMediaControl(
            withFields(subscription(8, harness.sliceId, centreHz), levels(QStringLiteral("clarity"))),
            epoch));
        QVERIFY(harness.client.sendMediaControl(
            withFields(subscription(9, harness.sliceId, centreHz), levels(QStringLiteral("agc"))),
            epoch));
        QTRY_COMPARE(harness.controller.activeEndpointCount(), 3);
        DisplayExtrasProcessor* const seven = harness.controller.displayExtrasForTest(7);
        DisplayExtrasProcessor* const eight = harness.controller.displayExtrasForTest(8);
        QVERIFY(seven && seven->clarity() && eight && eight->clarity());

        ClarityController desktop;
        desktop.setEnabled(true);
        const auto feed = [&](float floor, qint64 nowMs) {
            seven->feedNoiseFloor(floor, nowMs);
            eight->feedNoiseFloor(floor, nowMs);
            desktop.feedNoiseFloor(floor, nowMs);
        };
        feed(-130.0f, 0);
        feed(-100.0f, 1000);   // eases toward -100
        const float eased = eight->clarity()->smoothedFloor();
        const auto easedLevels = eight->waterfallLevels();
        QVERIFY(eased > -130.0f && eased < -100.0f);
        QCOMPARE(seven->clarity()->smoothedFloor(), eased);

        // The device's Re-tune for endpoint 7, then a refused one for
        // endpoint 9 (not Clarity) so its answer marks that 7's has run.
        QVERIFY(harness.client.sendMediaControl(clarityRetune(7), epoch));
        QVERIFY(harness.client.sendMediaControl(clarityRetune(9), epoch));
        QTRY_COMPARE(countOf(controls, QStringLiteral("rejected"), 9), 1);
        desktop.retuneNow();   // the desktop's Re-tune button

        feed(-110.0f, 1100);   // inside the 500 ms poll window
        QCOMPARE(seven->clarity()->smoothedFloor(), -110.0f);
        QCOMPARE(seven->clarity()->smoothedFloor(), desktop.smoothedFloor());
        QCOMPARE(seven->waterfallLevels().first, desktop.lastLow());
        QCOMPARE(seven->waterfallLevels().second, desktop.lastHigh());
        // Endpoint 8 was not re-tuned: the floor inside its poll window is
        // not taken, and its levels stand.
        QCOMPARE(eight->clarity()->smoothedFloor(), eased);
        QVERIFY(eight->waterfallLevels() == easedLevels);

        // A Re-tune that runs is not answered, and retires nothing.
        QCOMPARE(countOf(controls, QStringLiteral("rejected"), 7), 0);
        QCOMPARE(countOf(controls, QStringLiteral("allocation-result"), 7),
                 countOf(controls, QStringLiteral("allocation-result"), 8));
        QCOMPARE(harness.controller.activeEndpointCount(), 3);
        harness.finish();
    }

    // D17a, R-DSP-55: a display asked for again on the same receiver (here a
    // narrower span) gets a new processor, and that processor starts with
    // the replaced one's Clarity floor and thresholds. Asked for on another
    // source, it starts afresh.
    void aDisplayAskedForAgainOnTheSameReceiverKeepsItsLevels()
    {
        Harness harness;
        QVERIFY(harness.establishSession());
        QSignalSpy controls(&harness.client, &StationClient::mediaControlReceived);
        QVERIFY(harness.startReadyPeer());
        const double centreHz = harness.radio.streamCentreHz(harness.streamIndex);
        const quint32 epoch = harness.client.sessionEpoch();
        const QJsonObject clarity = withFields(
            levels(QStringLiteral("clarity")),
            {{QStringLiteral("tier"), QStringLiteral("wide")}});
        QVERIFY(harness.client.sendMediaControl(
            withFields(subscription(7, harness.sliceId, centreHz), clarity), epoch));
        // Endpoint 9 is not Clarity: its refused Re-tune answers in order,
        // which marks that the request sent before it has run.
        QVERIFY(harness.client.sendMediaControl(
            withFields(subscription(9, harness.sliceId, centreHz), levels(QStringLiteral("agc"))),
            epoch));
        QTRY_COMPARE(harness.controller.activeEndpointCount(), 2);
        DisplayExtrasProcessor* const first = harness.controller.displayExtrasForTest(7);
        QVERIFY(first && first->clarity());
        first->feedNoiseFloor(-125.0f, 1000);
        first->feedNoiseFloor(-112.0f, 1700);
        const float floor = first->clarity()->smoothedFloor();
        const std::pair<float, float> levelsBefore = first->waterfallLevels();
        QVERIFY(floor > -125.0f && floor < -112.0f);
        QCOMPARE(levelsBefore.first, first->clarity()->lastLow());
        const quint8 sectionsBefore = first->request().sections();

        // The same receiver, half the span, and more extras asked for (which
        // shows below that the processor is a new one).
        QVERIFY(harness.client.sendMediaControl(
            withFields(withFields(withFields(subscription(7, harness.sliceId, centreHz),
                                             everyExtra()), clarity),
                       {{QStringLiteral("revision"), 2}, {QStringLiteral("spanHz"), 24000.0}}),
            epoch));
        QVERIFY(harness.client.sendMediaControl(clarityRetune(9), epoch));
        QTRY_COMPARE(countOf(controls, QStringLiteral("rejected"), 9), 1);
        QCOMPARE(countOf(controls, QStringLiteral("rejected"), 7), 0);
        QVERIFY(harness.controller.spectrumGrant(7).has_value());
        DisplayExtrasProcessor* const second = harness.controller.displayExtrasForTest(7);
        QVERIFY(second && second->clarity());
        QVERIFY(second->request().sections() != sectionsBefore);
        QVERIFY(second->clarity()->carriedState().hasSmoothed);
        QCOMPARE(second->clarity()->smoothedFloor(), floor);
        QCOMPARE(second->clarity()->lastLow(), levelsBefore.first);
        QCOMPARE(second->clarity()->lastHigh(), levelsBefore.second);
        QVERIFY(second->waterfallLevels() == levelsBefore);

        // Another source (the receiver's other spectrum tier, with its own
        // engine): nothing is carried.
        const int sourcesBefore = harness.controller.activeSourceCount();
        QVERIFY(harness.client.sendMediaControl(
            withFields(withFields(subscription(7, harness.sliceId, centreHz), clarity),
                       {{QStringLiteral("revision"), 3},
                        {QStringLiteral("tier"), QStringLiteral("fine")}}),
            epoch));
        QVERIFY(harness.client.sendMediaControl(clarityRetune(9), epoch));
        QTRY_COMPARE(countOf(controls, QStringLiteral("rejected"), 9), 2);
        QCOMPARE(countOf(controls, QStringLiteral("rejected"), 7), 0);
        QCOMPARE(harness.controller.activeSourceCount(), sourcesBefore + 1);
        QVERIFY(harness.controller.spectrumGrant(7).has_value());
        DisplayExtrasProcessor* const third = harness.controller.displayExtrasForTest(7);
        QVERIFY(third && third->clarity());
        QVERIFY(!third->clarity()->carriedState().hasSmoothed);
        QCOMPARE(third->waterfallLevels().first, -122.0f);
        QCOMPARE(third->waterfallLevels().second, -62.0f);
        harness.finish();
    }

    // S21, R-DSP-23: the extras' key state is the end of MoxController's
    // walk (moxStateChanged), not TransmitModel's flag. A key shorter than
    // the RF delay toggles the flag and never reaches the walk's end: the
    // extras see nothing. The walk's end does key them, and its other end
    // un-keys them into the hold.
    void aKeyShorterThanTheRfDelayChangesNothingInTheExtras()
    {
#ifndef HAVE_FFTW3
        QSKIP("FFTEngine has no FFTW3 backend in this build");
#endif
        Harness harness;
        QVERIFY(harness.establishSession());
        QSignalSpy controls(&harness.client, &StationClient::mediaControlReceived);
        QVERIFY(harness.startReadyPeer());
        const double centreHz = harness.radio.streamCentreHz(harness.streamIndex);
        QVERIFY(harness.client.sendMediaControl(
            withFields(subscription(7, harness.sliceId, centreHz), everyExtra()),
            harness.client.sessionEpoch()));
        QTRY_COMPARE(harness.controller.activeEndpointCount(), 1);
        DisplayExtrasProcessor* const extras = harness.controller.displayExtrasForTest(7);
        QVERIFY(extras);
        // Frames whose extras the Core has computed and sent.
        const auto extrasSent = [&harness] {
            return withMagic(harness.mediaTransport->displays, "NSDX").size();
        };
        const auto moreExtras = [&](int count) {
            const auto before = extrasSent();
            return QTest::qWaitFor([&] {
                harness.feedRadio(0.15);
                return extrasSent() >= before + count;
            }, 30'000);
        };
        QVERIFY(moreExtras(3));
        QVERIFY(!extras->keyed());

        // The flag alone: a key that never reached the end of its walk.
        MoxController* const mox = harness.radio.moxController();
        QVERIFY(mox);
        QSignalSpy walkEnds(mox, &MoxController::moxStateChanged);
        harness.radio.transmitModel().setMox(true);
        QVERIFY(harness.radio.transmitModel().isMox());
        QVERIFY(moreExtras(3));
        QVERIFY(!extras->keyed());
        QVERIFY(!extras->levelFollower().keyed());
        harness.radio.transmitModel().setMox(false);
        QVERIFY(moreExtras(3));
        QVERIFY(!extras->keyed());
        QVERIFY(!extras->noiseFloor().fastAttack() || extras->noiseFloor().lerpAverage() < 0.0f);
        QCOMPARE(walkEnds.size(), 0);

        // The walk's end keys the extras, though the flag is off.
        emit mox->moxStateChanged(true);
        QVERIFY(!harness.radio.transmitModel().isMox());
        QVERIFY(moreExtras(2));
        QVERIFY(extras->keyed());
        QVERIFY(extras->levelFollower().keyed());
        emit mox->moxStateChanged(false);
        QVERIFY(moreExtras(2));
        QVERIFY(!extras->keyed());
        QVERIFY(!extras->levelFollower().keyed());
        harness.finish();
    }

    // Each refusal is a `rejected` naming the endpoint with revision 0, in
    // plain words, and closes nothing. A request of another shape is
    // ignored.
    void clarityRetuneRefusesWhatItCannotDo()
    {
        Harness harness;
        QVERIFY(harness.establishSession());
        QSignalSpy controls(&harness.client, &StationClient::mediaControlReceived);
        QVERIFY(harness.startReadyPeer());
        const double centreHz = harness.radio.streamCentreHz(harness.streamIndex);
        const quint32 epoch = harness.client.sessionEpoch();
        QVERIFY(harness.client.sendMediaControl(
            withFields(subscription(7, harness.sliceId, centreHz), levels(QStringLiteral("clarity"))),
            epoch));
        QVERIFY(harness.client.sendMediaControl(
            withFields(subscription(9, harness.sliceId, centreHz), levels(QStringLiteral("manual"))),
            epoch));
        QVERIFY(harness.client.sendMediaControl(subscription(10, harness.sliceId, centreHz), epoch));
        QTRY_COMPARE(harness.controller.activeEndpointCount(), 3);

        const auto refusal = [&](const QJsonObject& request, quint32 endpointId) {
            const int before = countOf(controls, QStringLiteral("rejected"), endpointId);
            if (!harness.client.sendMediaControl(request, epoch)) { return QJsonObject(); }
            if (!QTest::qWaitFor([&] {
                    return countOf(controls, QStringLiteral("rejected"), endpointId) > before;
                }, 5000)) {
                return QJsonObject();
            }
            return messageFor(controls, QStringLiteral("rejected"), endpointId);
        };
        const auto expectRefused = [&](const QJsonObject& message, const char* connectionId,
                                       quint32 endpointId, const QString& reason) {
            QCOMPARE(message.size(), 5);
            QCOMPARE(message.value(QStringLiteral("connectionId")).toString(),
                     QLatin1String(connectionId));
            QCOMPARE(message.value(QStringLiteral("endpointId")).toInteger(), qint64(endpointId));
            QCOMPARE(message.value(QStringLiteral("revision")).toInteger(), qint64(0));
            QCOMPARE(message.value(QStringLiteral("reason")).toString(), reason);
        };
        expectRefused(refusal(clarityRetune(42), 42), kConnectionId, 42, kDisplayClosed);
        expectRefused(refusal(clarityRetune(9), 9), kConnectionId, 9, kNotClarity);
        expectRefused(refusal(clarityRetune(10), 10), kConnectionId, 10, kNotClarity);
        expectRefused(refusal(clarityRetune(7, kOtherConnectionId), 7), kOtherConnectionId, 7,
                      kNotThisAppsDisplay);

        // Not one this Core reads: nothing comes back.
        QJsonObject extraKey = clarityRetune(7);
        extraKey.insert(QStringLiteral("revision"), 1);
        QJsonObject zero = clarityRetune(0);
        QJsonObject text = clarityRetune(7);
        text.insert(QStringLiteral("endpointId"), QStringLiteral("7"));
        for (const QJsonObject& request : {extraKey, zero, text}) {
            QVERIFY(harness.client.sendMediaControl(request, epoch));
        }
        // A request after them is answered, so they have been read.
        expectRefused(refusal(clarityRetune(43), 43), kConnectionId, 43, kDisplayClosed);
        QCOMPARE(countOf(controls, QStringLiteral("rejected"), 7), 1);   // the other connection's
        QCOMPARE(countOf(controls, QStringLiteral("rejected"), 0), 0);

        // No refusal closed a display.
        QCOMPARE(harness.controller.activeEndpointCount(), 3);
        for (const QString& reason : {kNotThisAppsDisplay, kDisplayClosed, kNotClarity}) {
            QVERIFY2(OperatorWording::isPlain(reason), qPrintable(reason));
            QVERIFY2(OperatorWording::coreCalledStationIn(reason).isEmpty(), qPrintable(reason));
        }
        harness.finish();
    }

    // A peer the Core did not tell displayExtrasVersion 2 (one below minor
    // 11 is never told the capability) gets today's behaviour: the operation
    // goes where an unknown one always has, and nothing comes back.
    void clarityRetuneIsNotThereForAnOlderPeer()
    {
        Harness harness;
        auto* app = new Test::LoopbackTransport(QStringLiteral("app"), this);
        auto* station = new Test::LoopbackTransport(QStringLiteral("station"), &harness.server);
        station->linkTo(app);
        harness.server.acceptTransport(station);
        QVERIFY(QTest::qWaitFor([app] { return !app->received().isEmpty(); }, 5000));
        app->sendText(SessionMessages::encode(SessionMessages::hello(
            kSessionProtocolMajor, quint16(kRadioIdentitySessionProtocolMinor - 1), 0,
            QStringLiteral("NereusSDR"))));
        app->sendText(SessionMessages::encode(SessionMessages::authRequest(harness.server.token())));
        QVERIFY(QTest::qWaitFor(
            [app] { return app->receivedKinds().contains(QByteArrayLiteral("snapshot.complete")); },
            5000));
        QVERIFY(harness.server.mediaAvailable());
        QVERIFY(!harness.server.displayExtrasAvailable());
        const auto sendMedia = [app](const QJsonObject& payload) {
            SessionMessage message;
            message.kind = SessionMessageKind::MediaControl;
            message.mediaPayload = payload;
            app->sendText(SessionMessages::encode(message));
        };
        sendMedia({{QStringLiteral("op"), QStringLiteral("start")},
                   {QStringLiteral("connectionId"), QLatin1String(kConnectionId)}});
        QVERIFY(QTest::qWaitFor([&harness] { return !harness.mediaTransport.isNull(); }, 10'000));
        harness.mediaTransport->becomeReady();
        const qsizetype before = app->received().size();
        sendMedia(clarityRetune(42));
        QTest::qWait(300);
        for (qsizetype i = before; i < app->received().size(); ++i) {
            SessionMessage message;
            if (SessionMessages::decode(app->received().at(i), &message)
                && message.kind == SessionMessageKind::MediaControl) {
                QVERIFY2(message.mediaPayload.value(QStringLiteral("op"))
                             != QJsonValue(QStringLiteral("rejected")),
                         "an older peer was answered");
            }
        }
    }
};

QTEST_MAIN(TstDisplayExtras)
#include "tst_display_extras.moc"
