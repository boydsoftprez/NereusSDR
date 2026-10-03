#pragma once
// no-port-check: NereusSDR-original. iPhone app follow-up (R-IOS-27, R-IOS-11).
// =================================================================
// tests/OlderPeerDisplayRun.h  (NereusSDR)
// =================================================================
//
// One fixed run of the display a subscription without any display extras
// field gets, through a real StationServer session and the Core's
// DaemonMediaController: a shaped spectrum published on the test's own
// clock, the sender ticked by hand, every frame and display control message
// recorded. Nothing in the run reads the wall clock, so the bytes are the
// same on every run.
//
// tst_display_extras compares a run with the golden recorded from the Core
// before display extras existed (535dd412), tests/data/
// display_extras_older_peer_frames.json: a peer that never asks for an
// extra sees exactly the frames it saw then. The run uses only what that
// Core had, plus DaemonSpectrumSource's shaped-bins publishFrameForTest,
// a test seam added beside the flat one for this recording.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include <QtTest>

#include "core/AppSettings.h"
#include "core/HpsdrModel.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/session/media/DaemonMediaController.h"
#include "core/session/media/DaemonSpectrumSource.h"
#include "core/session/media/DisplayBudget.h"
#include "core/session/media/IMediaTransport.h"
#include "core/settings/SettingsProxy.h"
#include "core/spectrum/SpectrumDetectorMode.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/UpgradedCoreToken.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>

#include <algorithm>
#include <cmath>

namespace NereusSDR::Test::OlderPeerDisplay {

inline constexpr char kConnectionId[] = "11111111-2222-4333-8444-555555555555";
inline constexpr quint32 kEndpointId = 8;
inline constexpr int kFftSize = 1024;
inline constexpr int kSourceFps = 60;
inline constexpr int kFrames = 48;
inline constexpr qint64 kStartNs = 1'000'000'000;

/// What an older app's display received: the display charge the Core
/// accepted for it, its media control messages and every display datagram,
/// in order.
struct Recording {
    QJsonObject charge;
    QJsonArray controls;
    QList<QByteArray> displays;

    QJsonObject toJson() const
    {
        QJsonArray hex;
        for (const QByteArray& bytes : displays) {
            hex.append(QString::fromLatin1(bytes.toHex()));
        }
        return QJsonObject{{QStringLiteral("charge"), charge},
                           {QStringLiteral("controls"), controls},
                           {QStringLiteral("displays"), hex}};
    }

    static Recording fromJson(const QJsonObject& object)
    {
        Recording recording;
        recording.charge = object.value(QStringLiteral("charge")).toObject();
        recording.controls = object.value(QStringLiteral("controls")).toArray();
        for (const QJsonValue& value : object.value(QStringLiteral("displays")).toArray()) {
            recording.displays.append(QByteArray::fromHex(value.toString().toLatin1()));
        }
        return recording;
    }
};

class RecordingTransport final : public IMediaTransport {
public:
    explicit RecordingTransport(QObject* parent = nullptr) : IMediaTransport(parent) {}
    bool start(const StartOptions&) override { return true; }
    void stop() override { readyState = false; }
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

    bool readyState{false};
    QList<QByteArray> displays;
};

/// The shaped source row for frame `frame`: a floor near -120 dBFS with a
/// fixed ripple, a steady carrier, one that walks up the band and one that
/// keys on and off. Integer arithmetic only, so every float is exact.
inline QVector<float> sourceBins(int frame)
{
    QVector<float> bins(kFftSize);
    for (int bin = 0; bin < kFftSize; ++bin) {
        const int ripple = (bin * 7 + frame * 3) % 11;
        bins[bin] = 1.0e-12f * static_cast<float>(1 + ripple);
    }
    bins[300] = 1.0e-5f;
    bins[301] = 3.0e-6f;
    bins[299] = 3.0e-6f;
    bins[100 + (frame * 9) % 800] = 2.0e-7f;
    if ((frame / 8) % 2 == 0) {
        bins[700] = 5.0e-4f;
    }
    return bins;
}

inline QJsonObject planeRequest(SpectrumDetectorMode detector, int averageMode, double alpha)
{
    return {{QStringLiteral("detector"), static_cast<int>(detector)},
            {QStringLiteral("averageMode"), averageMode},
            {QStringLiteral("averageAlpha"), alpha}};
}

/// An older app's subscription: today's keys only, averaging on both planes.
inline QJsonObject olderSubscription(int sliceId, double centreHz)
{
    return {{QStringLiteral("op"), QStringLiteral("subscribe")},
            {QStringLiteral("connectionId"), QLatin1String(kConnectionId)},
            {QStringLiteral("endpointId"), static_cast<qint64>(kEndpointId)},
            {QStringLiteral("revision"), 1},
            {QStringLiteral("sliceId"), sliceId},
            {QStringLiteral("tier"), QStringLiteral("wide")},
            {QStringLiteral("fftSize"), kFftSize},
            {QStringLiteral("windowType"), 0},
            {QStringLiteral("centreHz"), centreHz},
            {QStringLiteral("spanHz"), 96000.0},
            {QStringLiteral("pixels"), 128},
            {QStringLiteral("fps"), 30},
            {QStringLiteral("framesPerLine"), 2},
            {QStringLiteral("trace"), planeRequest(SpectrumDetectorMode::Peak, 0, 0.5)},
            {QStringLiteral("waterfall"), planeRequest(SpectrumDetectorMode::Average, 0, 0.25)},
            {QStringLiteral("minDbm"), -180.0},
            {QStringLiteral("maxDbm"), 0.0},
            {QStringLiteral("wideSpanFactor"), 0.0}};
}

struct Station {
    QTemporaryDir directory;
    AppSettings settings;
    RadioModel radio;
    StationServer server;
    RadioModel remote{RadioModel::Role::Remote};
    SettingsProxy settingsProxy;
    StationClient client{&remote, &settingsProxy};
    QPointer<RecordingTransport> mediaTransport;
    qint64 nowNs{0};
    DaemonMediaController controller;
    int sliceId{-1};
    int streamIndex{-1};

    Station()
        : settings(directory.filePath(QStringLiteral("station.settings")))
        , server(&radio, settings, seedUpgradedCoreToken(directory.path()))
        , controller(&server, &radio, nullptr,
                     [this](QObject* parent) -> IMediaTransport* {
                         mediaTransport = new RecordingTransport(parent);
                         return mediaTransport;
                     },
                     [this] { return nowNs; })
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

    QTimer* displaySender()
    {
        for (QTimer* timer : controller.findChildren<QTimer*>(
                 QString(), Qt::FindDirectChildrenOnly)) {
            if (timer->interval() == static_cast<int>(kDisplaySenderIntervalMs)
                || timer->interval() == 60'000) {
                return timer;
            }
        }
        return nullptr;
    }
};

/// Runs the fixed scenario into `out`. Fails the current test on any setup
/// step it cannot complete.
inline void run(Recording& out)
{
    Station station;
    QVERIFY(station.sliceId >= 0 && station.streamIndex >= 0);
    QTimer* sender = station.displaySender();
    QVERIFY(sender);
    // Production keeps starting this timer; a long interval leaves its
    // timeout under the run's control.
    sender->setInterval(60'000);

    auto* stationLink = new LoopbackTransport(QStringLiteral("station"));
    auto* clientLink = new LoopbackTransport(QStringLiteral("client"));
    stationLink->linkTo(clientLink);
    station.client.startSession(clientLink, station.server.token());
    station.server.acceptTransport(stationLink);
    QTRY_VERIFY_WITH_TIMEOUT(
        station.server.mediaAvailable() && station.client.mediaAvailable(), 10'000);

    QSignalSpy controls(&station.client, &StationClient::mediaControlReceived);
    QVERIFY(station.client.sendMediaControl(
        {{QStringLiteral("op"), QStringLiteral("start")},
         {QStringLiteral("connectionId"), QLatin1String(kConnectionId)}},
        station.client.sessionEpoch()));
    QTRY_VERIFY_WITH_TIMEOUT(!station.mediaTransport.isNull(), 10'000);
    station.mediaTransport->becomeReady();

    const double centreHz = station.radio.streamCentreHz(station.streamIndex);
    QVERIFY(station.client.sendMediaControl(olderSubscription(station.sliceId, centreHz),
                                            station.client.sessionEpoch()));
    QTRY_COMPARE_WITH_TIMEOUT(station.controller.activeEndpointCount(), 1, 10'000);
    const DisplayBudgetCharge charge = station.controller.acceptedDisplayCharge();
    // Whole numbers well inside a double's exact range.
    out.charge = QJsonObject{
        {QStringLiteral("applicationBytesPerSecond"),
         static_cast<double>(charge.applicationBytesPerSecond)},
        {QStringLiteral("spectrumSampleUnitsPerSecond"),
         static_cast<double>(charge.spectrumSampleUnitsPerSecond)},
        {QStringLiteral("messagesPerSecond"), static_cast<double>(charge.messagesPerSecond)}};
    auto* source = station.controller.findChild<DaemonSpectrumSource*>();
    QVERIFY(source);
    const QList<MediaSourceKey> keys = source->activeSources();
    QCOMPARE(keys.size(), 1);
    const MediaSourceKey key = keys.constFirst();

    // The engine takes its configuration on its own thread; after that
    // every frame comes from here, never from I/Q (none is fed).
    station.nowNs = kStartNs;
    QTRY_VERIFY_WITH_TIMEOUT(source->publishFrameForTest(key, kStartNs, sourceBins(0)),
                             10'000);

    // Source frame k at kStartNs + k / 60 s, sender tick j at kStartNs +
    // j x 5 ms; a frame and a tick at the same instant: the frame first.
    constexpr qint64 kFramePeriodNs = 1'000'000'000LL / kSourceFps;
    const qint64 endNs = kStartNs + kFrames * kFramePeriodNs;
    qint64 frame = 1;
    qint64 tick = 1;
    for (;;) {
        const qint64 frameNs = kStartNs + frame * kFramePeriodNs;
        const qint64 tickNs = kStartNs + tick * kDisplaySenderIntervalNs;
        const qint64 nowNs = std::min(frameNs, tickNs);
        if (nowNs >= endNs) { break; }
        station.nowNs = nowNs;
        if (frameNs == nowNs) {
            QVERIFY(source->publishFrameForTest(key, frameNs, sourceBins(int(frame))));
            ++frame;
        } else {
            sender->stop();
            QVERIFY(QMetaObject::invokeMethod(sender, "timeout", Qt::DirectConnection));
            ++tick;
        }
    }
    // Let the last queued control messages reach the client.
    QCoreApplication::processEvents();

    for (const QList<QVariant>& arguments : controls) {
        out.controls.append(arguments.at(0).toJsonObject());
    }
    out.displays = station.mediaTransport->displays;
    station.client.disconnectFromStation(QStringLiteral("run complete"));
}

} // namespace NereusSDR::Test::OlderPeerDisplay
