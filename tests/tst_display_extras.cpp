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
        desktop.setWfAgcEnabled(levelMode == QLatin1String("agc"));
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
        for (int i = 0; i < kFrames; ++i) {
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
            inputs.nowMs = static_cast<qint64>(i) * (1000 / kFps);
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
        QTest::newRow("average 9 ms") << QStringLiteral("averageTimeMs") << QJsonValue(9);
        QTest::newRow("average fraction") << QStringLiteral("averageTimeMs") << QJsonValue(30.5);
        QTest::newRow("waterfall average 9 ms") << QStringLiteral("waterfallAverageTimeMs")
                                                << QJsonValue(9);
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
        // Absent: the waterfall falls back to averageTimeMs.
        const auto fallback = harness.controller.spectrumAveraging(8);
        QVERIFY(fallback.has_value());
        QCOMPARE(fallback->traceAlpha, spectrum);
        QCOMPARE(fallback->waterfallAlpha, spectrum);
        // The waterfall's alone leaves the trace its plane's constant.
        const auto waterfallOnly = harness.controller.spectrumAveraging(9);
        QVERIFY(waterfallOnly.has_value());
        QCOMPARE(waterfallOnly->traceAlpha, 0.0);
        QCOMPARE(waterfallOnly->waterfallAlpha, waterfall);
        // Neither: both planes' own.
        const auto neither = harness.controller.spectrumAveraging(10);
        QVERIFY(neither.has_value());
        QCOMPARE(neither->traceAlpha, 0.0);
        QCOMPARE(neither->waterfallAlpha, 0.0);
        // No field asks for a section, so every endpoint is charged as today.
        const auto plainCost = spectrumDisplayCost(128, 60, false);
        QVERIFY(plainCost.has_value());
        QCOMPARE(harness.controller.acceptedDisplayCharge().applicationBytesPerSecond,
                 4 * plainCost->charge.applicationBytesPerSecond);
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
        // floor are not clipped.
        const QJsonObject wide{{QStringLiteral("minDbm"), -400.0},
                               {QStringLiteral("maxDbm"), 100.0}};
        QVERIFY(harness.client.sendMediaControl(
            withFields(withFields(subscription(7, harness.sliceId, centreHz), wide),
                       {{QStringLiteral("calibrationOffsetDb"), 20.0}}),
            harness.client.sessionEpoch()));
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
                       {{QStringLiteral("averageTimeMs"), 5}}),
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
