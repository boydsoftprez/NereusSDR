// =================================================================
// tests/tst_display_codec_v2_link.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test. Display codec v2 (NSDC v2),
// Task 3: negotiation and the fallback setting.
//
// A peer whose hello declares displayCodec 2 is told displayCodecVersion 2,
// last before coreBuildInfo, and its media start may then ask for version 2
// display frames with displayCodecVersion 2. Any other start value is
// refused. A peer that did not declare the feature is sent exactly today's
// capabilities and may not ask. The Core's RemoteDisplayCodecV2 setting off
// withdraws the capability and the start field from every peer.
//
// The Core is the real StationServer and DaemonMediaController behind a
// loopback station session. No radio and no audio; displays only where a
// test feeds synthetic I/Q (Task 4: a v2 start's frames are version 2, a
// v1 start's version 1).
//
// Task 6, the interop matrix: every pairing of a Core and a peer gets the
// version it agreed, in every frame (a v2 start; a start without the field;
// a peer that did not declare; RemoteDisplayCodecV2 off), and a v2 frame
// carries the wide plane. A real window (RemoteMediaController, its pan on
// a PanadapterStack) on a real Core, over a fake media link that can drop a
// display message: on v2 it asks no keyframe for a context, a transmit
// context, the receive context the fall brings back or a lost frame; on a
// Core that sends v1 (one with the setting off stands in for an older Core,
// and sends no capability entry) it asks for each, and a lost frame's
// NeedKeyframe asks once and recovers, as before.
//
// Modification history (NereusSDR):
//   2026-10-22  J.J. Boyd / KG4VCF  Task 6: the interop matrix (older
//                                    peer, fallback off, wide plane, a real
//                                    window on a v2 Core and on a v1 Core).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-10-22  J.J. Boyd / KG4VCF  Task 4: the agreed version is the
//                                    version byte of every display frame.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-10-22  J.J. Boyd / KG4VCF  Created. AI-assisted via Anthropic
//                                    Claude Code.
// =================================================================

#include <QtTest>

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <cmath>
#include <numbers>

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/FFTEngine.h"
#include "core/HpsdrModel.h"
#include "core/MoxController.h"
#include "core/TxAnalyzer.h"
#include "core/session/MirrorSchema.h"
#include "core/session/StationCapabilities.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/session/media/DaemonMediaController.h"
#include "core/session/media/DisplayCodec.h"
#include "core/session/media/IMediaTransport.h"
#include "core/spectrum/SpectrumDetectorMode.h"
#include "core/settings/SettingsProxy.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/UpgradedCoreToken.h"
#include "gui/PanadapterApplet.h"
#include "gui/PanadapterStack.h"
#include "gui/RemoteMediaController.h"
#include "gui/SpectrumWidget.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

constexpr char kConnectionId[] = "11111111-2222-4333-8444-555555555555";
const QString kField = QStringLiteral("displayCodecVersion");

class FakeTransport final : public IMediaTransport {
public:
    explicit FakeTransport(QObject* parent = nullptr) : IMediaTransport(parent) {}
    bool start(const StartOptions&) override { return true; }
    void stop() override {}
    bool acceptDescription(const QString&, const QString&) override { return true; }
    bool acceptCandidate(const QString&, const QString&) override { return true; }
    bool sendDisplay(const QByteArray& bytes) override
    {
        if (!readyState) { return false; }
        displays.append(bytes);
        return true;
    }
    bool sendRtp(const QByteArray&) override { return false; }
    bool isReady() const override { return readyState; }
    void becomeReady() { readyState = true; emit ready(); }

    bool readyState = false;
    QList<QByteArray> displays;
};

QVector<float> syntheticIq(int complexSamples, double cyclesPerSample)
{
    QVector<float> samples;
    samples.reserve(complexSamples * 2);
    for (int sample = 0; sample < complexSamples; ++sample) {
        const double phase = 2.0 * std::numbers::pi * cyclesPerSample * sample;
        samples.append(static_cast<float>(std::cos(phase)));
        samples.append(static_cast<float>(std::sin(phase)));
    }
    return samples;
}

QJsonObject spectrumPlane()
{
    return {{QStringLiteral("detector"), static_cast<int>(SpectrumDetectorMode::Peak)},
            {QStringLiteral("averageMode"), -1},
            {QStringLiteral("averageAlpha"), 0.0}};
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
    int transportsMade = 0;
    DaemonMediaController controller;
    Test::LoopbackTransport* clientLink = nullptr;

    Harness()
        : settings(directory.filePath(QStringLiteral("station.settings")))
        , server(&radio, settings, NereusSDR::Test::seedUpgradedCoreToken(directory.path()))
        , controller(&server, &radio, nullptr,
                     [this](QObject* parent) -> IMediaTransport* {
                         ++transportsMade;
                         mediaTransport = new FakeTransport(parent);
                         return mediaTransport;
                     },
                     [] { return qint64{10'000'000'000}; })
    {
        radio.setBoardForTest(HPSDRHW::Saturn);
        radio.configureStreamPool(/*userDdcCount=*/5, /*maxSlices=*/5,
                                  /*defaultRateHz=*/192000);
        radio.setConnectionStateForTest(ConnectionState::Connected);
        server.setMediaEnabled(true);
    }

    // The station session, up to media being available; no media start.
    bool connectSession()
    {
        auto* stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
        clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        return QTest::qWaitFor([this] {
            return server.mediaAvailable() && client.mediaAvailable();
        }, 10'000);
    }

    // `version` undefined: a start without the field.
    bool startWith(const QJsonValue& version)
    {
        QJsonObject control{{QStringLiteral("op"), QStringLiteral("start")},
                            {QStringLiteral("connectionId"), QLatin1String(kConnectionId)}};
        if (!version.isUndefined()) {
            control.insert(kField, version);
        }
        return client.sendMediaControl(control, client.sessionEpoch());
    }

    // The properties of the first capabilities message the Core sent.
    QJsonArray capabilityProperties() const
    {
        for (const QByteArray& wire : clientLink->received()) {
            const QJsonObject message = QJsonDocument::fromJson(wire).object();
            if (message.value(QStringLiteral("type")).toString()
                == QStringLiteral("capabilities")) {
                return message.value(QStringLiteral("properties")).toArray();
            }
        }
        return {};
    }

    QStringList capabilityNames() const
    {
        QStringList names;
        for (const QJsonValue& property : capabilityProperties()) {
            names.append(property.toObject().value(QStringLiteral("name")).toString());
        }
        return names;
    }

    // A slice and its stream, so a subscribe has a source to show.
    int addFedSlice()
    {
        const int sliceId = radio.addSlice();
        if (sliceId >= 0) {
            streamIndex = radio.sliceById(sliceId)->streamIndex();
        }
        return sliceId;
    }

    void feedRadio()
    {
        QVERIFY(QMetaObject::invokeMethod(&radio, "rawIqDataForStream", Qt::DirectConnection,
                                          Q_ARG(int, streamIndex),
                                          Q_ARG(QVector<float>, syntheticIq(1026, 0.125))));
    }

    // `wideSpanFactor` above 1 asks for the wide plane around `spanHz`.
    bool subscribe(quint32 endpointId, int sliceId, double spanHz = 192000.0,
                   double wideSpanFactor = 0.0)
    {
        const QJsonObject control{
            {QStringLiteral("op"), QStringLiteral("subscribe")},
            {QStringLiteral("connectionId"), QLatin1String(kConnectionId)},
            {QStringLiteral("endpointId"), static_cast<qint64>(endpointId)},
            {QStringLiteral("revision"), 1},
            {QStringLiteral("sliceId"), sliceId},
            {QStringLiteral("tier"), QStringLiteral("wide")},
            {QStringLiteral("fftSize"), 4096},
            {QStringLiteral("windowType"), static_cast<int>(WindowFunction::Hann)},
            {QStringLiteral("centreHz"), radio.streamCentreHz(streamIndex)},
            {QStringLiteral("spanHz"), spanHz},
            {QStringLiteral("pixels"), 1024},
            {QStringLiteral("fps"), 60},
            {QStringLiteral("framesPerLine"), 1},
            {QStringLiteral("trace"), spectrumPlane()},
            {QStringLiteral("waterfall"), spectrumPlane()},
            {QStringLiteral("minDbm"), -180.0},
            {QStringLiteral("maxDbm"), 0.0},
            {QStringLiteral("wideSpanFactor"), wideSpanFactor}};
        return client.sendMediaControl(control, client.sessionEpoch());
    }

    int streamIndex = -1;

    void finish() { client.disconnectFromStation(QStringLiteral("test complete")); }
};

// Sends the start (`version` undefined: without the field), a subscribe,
// and synthetic I/Q until the Core has sent `frames` display frames; then
// the version byte of every display frame it sent.
QList<int> versionBytesOfFrames(Harness& h, const QJsonValue& version, int frames)
{
    QList<int> versions;
    const int sliceId = h.addFedSlice();
    if (sliceId < 0) {
        return versions;
    }
    if (!h.connectSession() || !h.startWith(version)
        || !QTest::qWaitFor([&h] { return !h.mediaTransport.isNull(); }, 10'000)) {
        return versions;
    }
    h.mediaTransport->becomeReady();
    if (!h.subscribe(91, sliceId)) {
        return versions;
    }
    const auto codecFrames = [&h] {
        int count = 0;
        for (const QByteArray& message : h.mediaTransport->displays) {
            if (message.startsWith("NSDC")) {
                ++count;
            }
        }
        return count;
    };
    // A short count fails the caller's size check.
    const bool enough = QTest::qWaitFor([&] {
        h.feedRadio();
        return codecFrames() >= frames;
    }, 10'000);
    Q_UNUSED(enough);
    for (const QByteArray& message : h.mediaTransport->displays) {
        if (message.startsWith("NSDC") && message.size() > 4) {
            versions.append(static_cast<quint8>(message.at(4)));
        }
    }
    return versions;
}

// Sends each refused start, then a start without the field. The loopback
// delivers in order, so once the last start's transport exists every
// earlier start was handled: had any been taken, it would have made the
// transport (the later start on the same connection is then a no-op) and
// stored version 2.
void refusedThenPlainStarts(Harness& h, const QList<QJsonValue>& refused)
{
    for (const QJsonValue& value : refused) {
        QVERIFY(h.startWith(value));
    }
    QVERIFY(h.startWith(QJsonValue(QJsonValue::Undefined)));
    QTRY_VERIFY_WITH_TIMEOUT(!h.mediaTransport.isNull(), 10'000);
    QCOMPARE(h.transportsMade, 1);
    QCOMPARE(h.controller.displayCodecVersion(), quint32{1});
}

// Task 6: the media link between a real Core and a real window. Each
// display message the Core sends is kept and handed to the window's end,
// except the display codec messages a test asks to lose.
class BridgeTransport final : public IMediaTransport {
public:
    explicit BridgeTransport(QObject* parent) : IMediaTransport(parent) {}
    bool start(const StartOptions&) override { return true; }
    void stop() override { active = false; }
    bool acceptDescription(const QString&, const QString&) override { return true; }
    bool acceptCandidate(const QString&, const QString&) override { return true; }
    bool sendDisplay(const QByteArray& message) override
    {
        if (!active) { return false; }
        displays.append(message);
        if (message.startsWith("NSDC") && codecMessagesToLose > 0) {
            --codecMessagesToLose;
            return true;
        }
        if (other) { other->deliver(message); }
        return true;
    }
    bool sendRtp(const QByteArray&) override { return active; }
    bool isReady() const override { return active; }
    void activate()
    {
        active = true;
        emit ready();
    }
    void deliver(const QByteArray& message) { emit displayReceived(message); }

    bool active = false;
    int codecMessagesToLose = 0;
    QPointer<BridgeTransport> other;
    QList<QByteArray> displays;
};

int countOp(const QSignalSpy& spy, const QString& op)
{
    int count = 0;
    for (const QList<QVariant>& call : spy) {
        if (call.at(0).toJsonObject().value(QStringLiteral("op")).toString() == op) {
            ++count;
        }
    }
    return count;
}

QJsonObject lastOp(const QSignalSpy& spy, const QString& op)
{
    for (auto it = spy.crbegin(); it != spy.crend(); ++it) {
        const QJsonObject control = it->at(0).toJsonObject();
        if (control.value(QStringLiteral("op")).toString() == op) {
            return control;
        }
    }
    return {};
}

// The names of the first capabilities message a link received.
QStringList capabilityNamesOn(const Test::LoopbackTransport* link)
{
    QStringList names;
    for (const QByteArray& wire : link->received()) {
        const QJsonObject message = QJsonDocument::fromJson(wire).object();
        if (message.value(QStringLiteral("type")).toString() == QStringLiteral("capabilities")) {
            for (const QJsonValue& property : message.value(QStringLiteral("properties")).toArray()) {
                names.append(property.toObject().value(QStringLiteral("name")).toString());
            }
            break;
        }
    }
    return names;
}

} // namespace

class TstDisplayCodecV2Link : public QObject {
    Q_OBJECT

    QString m_savedVersion;

private slots:
    void initTestCase()
    {
        qRegisterMetaType<QVector<float>>();
        qRegisterMetaType<MediaSourceKey>();
        // coreBuildInfo is sent only when the Core knows its version.
        m_savedVersion = QCoreApplication::applicationVersion();
        QCoreApplication::setApplicationVersion(QStringLiteral("2026.10.0"));
    }

    void cleanupTestCase() { QCoreApplication::setApplicationVersion(m_savedVersion); }

    void theSettingKeyAndDefault()
    {
        QCOMPARE(QString::fromLatin1(kRemoteDisplayCodecV2SettingsKey),
                 QStringLiteral("RemoteDisplayCodecV2"));
        Harness h;
        QVERIFY(h.server.displayCodecV2Enabled());
        h.server.setDisplayCodecV2Enabled(false);
        QVERIFY(!h.server.displayCodecV2Enabled());
        // No media session yet: nothing is available.
        h.server.setDisplayCodecV2Enabled(true);
        QVERIFY(!h.server.displayCodecV2Available(h.server.mediaSessionEpoch()));
    }

    // The entry reads back, and a bad entry reads as absent.
    void theCapabilityEntryRoundTrips()
    {
        StationCapabilities sent;
        sent.displayCodecVersion = 2;
        sent.rxMeterStampsVersion = 1;
        sent.coreBuildInfo = CoreBuildInfo{QStringLiteral("2026.10.0"), QStringLiteral("test@1")};
        const QList<MirrorUpdate> updates = sent.toUpdates();
        QList<QByteArray> names;
        for (const MirrorUpdate& update : updates) {
            names.append(update.name);
        }
        const qsizetype at = names.indexOf(QByteArrayLiteral("displayCodecVersion"));
        QVERIFY(at > 0);
        QCOMPARE(names.at(at - 1), QByteArrayLiteral("rxMeterStampsVersion"));
        QCOMPARE(names.at(at + 1), QByteArrayLiteral("coreBuildInfo"));
        QCOMPARE(names.last(), QByteArrayLiteral("coreBuildInfo"));
        QCOMPARE(StationCapabilities::fromUpdates(updates).displayCodecVersion, 2);

        // 0 sends no entry.
        StationCapabilities none;
        for (const MirrorUpdate& update : none.toUpdates()) {
            QVERIFY(update.name != QByteArrayLiteral("displayCodecVersion"));
        }
        QCOMPARE(StationCapabilities::fromUpdates(none.toUpdates()).displayCodecVersion, 0);

        // A string, or a value out of range, reads as absent.
        QList<MirrorUpdate> bad = updates;
        for (MirrorUpdate& update : bad) {
            if (update.name == QByteArrayLiteral("displayCodecVersion")) {
                update.kind = MirrorWireKind::Utf8;
                update.value = QStringLiteral("2");
            }
        }
        QCOMPARE(StationCapabilities::fromUpdates(bad).displayCodecVersion, 0);
        for (MirrorUpdate& update : bad) {
            if (update.name == QByteArrayLiteral("displayCodecVersion")) {
                update.kind = MirrorWireKind::Int64;
                update.value = qlonglong{0};
            }
        }
        QCOMPARE(StationCapabilities::fromUpdates(bad).displayCodecVersion, 0);
    }

    // A default Core tells a declaring peer version 2, last before
    // coreBuildInfo, and takes its start's displayCodecVersion 2.
    void aDeclaringPeerIsToldAndMayAsk()
    {
        Harness h;
        QVERIFY(h.connectSession());
        QCOMPARE(h.client.capabilities().displayCodecVersion, 2);
        const QStringList names = h.capabilityNames();
        QVERIFY(names.size() >= 3);
        QCOMPARE(names.last(), QStringLiteral("coreBuildInfo"));
        QCOMPARE(names.at(names.size() - 2), kField);
        QCOMPARE(names.at(names.size() - 3), QStringLiteral("rxMeterStampsVersion"));
        QVERIFY(h.server.displayCodecV2Available(h.server.mediaSessionEpoch()));
        QVERIFY(h.startWith(2));
        QTRY_VERIFY_WITH_TIMEOUT(!h.mediaTransport.isNull(), 10'000);
        QCOMPARE(h.transportsMade, 1);
        QCOMPARE(h.controller.displayCodecVersion(), quint32{2});
        h.finish();
    }

    // Only the integer 2 is taken; a start without the field agrees 1.
    void aStartTakesOnlyVersionTwo()
    {
        Harness h;
        QVERIFY(h.connectSession());
        QCOMPARE(h.client.capabilities().displayCodecVersion, 2);
        refusedThenPlainStarts(h, {QJsonValue(1), QJsonValue(3), QJsonValue(0),
                                   QJsonValue(true), QJsonValue(QStringLiteral("2")),
                                   QJsonValue(2.5)});
        h.finish();
    }

    // A peer that did not declare the feature is sent exactly today's
    // capabilities, and its start may not carry the field.
    void aPeerThatDidNotDeclareGetsTodaysCapabilities()
    {
        QJsonArray declared;
        {
            Harness h;
            QVERIFY(h.connectSession());
            declared = h.capabilityProperties();
            h.finish();
        }
        Harness h;
        h.client.withholdFeatureForTest(QByteArrayLiteral("displayCodec"));
        QVERIFY(h.connectSession());
        QCOMPARE(h.client.capabilities().displayCodecVersion, 0);
        QVERIFY(!h.capabilityNames().contains(kField));
        QCOMPARE(h.capabilityNames().last(), QStringLiteral("coreBuildInfo"));
        QVERIFY(!h.server.displayCodecV2Available(h.server.mediaSessionEpoch()));
        // Byte for byte the declaring peer's list without its one entry.
        QJsonArray stripped;
        for (const QJsonValue& property : declared) {
            if (property.toObject().value(QStringLiteral("name")).toString() != kField) {
                stripped.append(property);
            }
        }
        QCOMPARE(stripped.size(), declared.size() - 1);
        QCOMPARE(QJsonDocument(h.capabilityProperties()).toJson(QJsonDocument::Compact),
                 QJsonDocument(stripped).toJson(QJsonDocument::Compact));
        refusedThenPlainStarts(h, {QJsonValue(2)});
        h.finish();
    }

    // The fallback switch: off, a declaring peer is told nothing and its
    // start may not carry the field.
    void theFallbackSwitchWithdrawsIt()
    {
        Harness h;
        h.server.setDisplayCodecV2Enabled(false);
        QVERIFY(h.connectSession());
        QCOMPARE(h.client.capabilities().displayCodecVersion, 0);
        QVERIFY(!h.capabilityNames().contains(kField));
        QVERIFY(!h.server.displayCodecV2Available(h.server.mediaSessionEpoch()));
        refusedThenPlainStarts(h, {QJsonValue(2)});
        h.finish();
    }

    // Task 4: a v2 start's display frames all carry version byte 2; a
    // start without the field, on the same Core build in another harness,
    // gets version 1 frames. Task 6: so does a peer that did not declare
    // the feature, and a declaring peer of a Core whose RemoteDisplayCodecV2
    // is off (it is told nothing, so its start has no field).
    void theAgreedVersionIsEveryFramesVersionByte_data()
    {
        QTest::addColumn<bool>("declares");
        QTest::addColumn<bool>("settingOn");
        QTest::addColumn<bool>("asksV2");
        QTest::addColumn<int>("expected");
        QTest::newRow("start agreed v2") << true << true << true << 2;
        QTest::newRow("start without the field") << true << true << false << 1;
        QTest::newRow("peer that did not declare") << false << true << false << 1;
        QTest::newRow("RemoteDisplayCodecV2 off") << true << false << false << 1;
    }

    void theAgreedVersionIsEveryFramesVersionByte()
    {
#ifndef HAVE_FFTW3
        QSKIP("FFTEngine has no FFTW3 backend in this build");
#endif
        QFETCH(bool, declares);
        QFETCH(bool, settingOn);
        QFETCH(bool, asksV2);
        QFETCH(int, expected);
        Harness h;
        if (!declares) {
            h.client.withholdFeatureForTest(QByteArrayLiteral("displayCodec"));
        }
        h.server.setDisplayCodecV2Enabled(settingOn);
        const QList<int> versions = versionBytesOfFrames(
            h, asksV2 ? QJsonValue(2) : QJsonValue(QJsonValue::Undefined), 4);
        QVERIFY2(versions.size() >= 4, qPrintable(QString::number(versions.size())));
        QCOMPARE(h.client.capabilities().displayCodecVersion, declares && settingOn ? 2 : 0);
        QCOMPARE(h.capabilityNames().contains(kField), declares && settingOn);
        QCOMPARE(h.controller.displayCodecVersion(), quint32(expected));
        for (int version : versions) {
            QCOMPARE(version, expected);
        }
        h.finish();
    }

    // Task 6: a v2 frame with the wide plane (flag 0x04) through the Core.
    // A subscribe whose wideSpanFactor widens it past its span asks for the
    // plane; every frame the Core sends is version 2 and decodes, Held or
    // Accepted, to a wide plane of the context's length.
    void aV2FrameCarriesTheWidePlane()
    {
#ifndef HAVE_FFTW3
        QSKIP("FFTEngine has no FFTW3 backend in this build");
#endif
        constexpr quint8 kWideFlag = 0x04;
        Harness h;
        const int sliceId = h.addFedSlice();
        QVERIFY(sliceId >= 0);
        QVERIFY(h.connectSession());
        QVERIFY(h.startWith(2));
        QTRY_VERIFY_WITH_TIMEOUT(!h.mediaTransport.isNull(), 10'000);
        h.mediaTransport->becomeReady();
        QCOMPARE(h.controller.displayCodecVersion(), quint32{2});
        QVERIFY(h.subscribe(92, sliceId, /*spanHz=*/48000.0, /*wideSpanFactor=*/2.0));
        const auto codecMessages = [&h] {
            int count = 0;
            for (const QByteArray& message : h.mediaTransport->displays) {
                if (message.startsWith("NSDC")) {
                    ++count;
                }
            }
            return count;
        };
        QTRY_VERIFY_WITH_TIMEOUT((h.feedRadio(), codecMessages() >= 8), 10'000);

        DisplayCodecDecoder decoder;
        int flagged = 0;
        int wideFrames = 0;
        for (const QByteArray& message : h.mediaTransport->displays) {
            if (!message.startsWith("NSDC")) {
                continue;
            }
            QVERIFY(message.size() > 5);
            QCOMPARE(static_cast<quint8>(message.at(4)), quint8{2});
            if ((static_cast<quint8>(message.at(5)) & kWideFlag) != 0) {
                ++flagged;
            }
            const DisplayCodecDecodeResult result = decoder.decode(message);
            QVERIFY2(result.disposition == DisplayCodecDisposition::Accepted
                         || result.disposition == DisplayCodecDisposition::Held,
                     qPrintable(QString::number(static_cast<int>(result.disposition))));
            for (int i = 0; i < result.frameCount; ++i) {
                const DisplayCodecFrame& frame = i == 0 ? result.frame : result.secondFrame;
                QVERIFY(frame.context.wideSamples > 0);
                QCOMPARE(frame.wideDbm.size(), int(frame.context.wideSamples));
                ++wideFrames;
            }
        }
        QVERIFY(flagged > 0);
        QVERIFY(wideFrames > 0);
        h.finish();
    }

    // Task 6: a real window on a real Core. On a Core that offers v2 the
    // window asks for it and gets version 2 frames, and asks no keyframe for
    // its pan's receive context, a lost frame, the transmit context or the
    // receive context the fall brings back (a newer context for the same
    // request). On a Core that sends v1 (RemoteDisplayCodecV2 off, standing
    // in for an older Core: no capability entry, so no start field) the
    // window gets version 1 frames and asks one keyframe for each, as
    // before: a lost frame's NeedKeyframe asks once and the pan recovers.
    void aWindowAsksForKeyframesOnlyOnVersionOne_data()
    {
        QTest::addColumn<bool>("v2");
        QTest::newRow("Core offers v2") << true;
        QTest::newRow("Core sends v1") << false;
    }

    void aWindowAsksForKeyframesOnlyOnVersionOne()
    {
#ifndef HAVE_FFTW3
        QSKIP("FFTEngine has no FFTW3 backend in this build");
#endif
        QFETCH(bool, v2);
        // Past the window's 200 ms spacing between keyframe requests.
        constexpr int kPastKeyframeSpacingMs = 250;
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        station.setBoardForTest(HPSDRHW::Saturn);
        station.configureStreamPool(/*userDdcCount=*/5, /*maxSlices=*/5,
                                    /*defaultRateHz=*/192000);
        station.setConnectionStateForTest(ConnectionState::Connected);
        const int sliceId = station.addSlice();
        SliceModel* slice = station.sliceById(sliceId);
        QVERIFY(slice);
        const int stream = slice->streamIndex();
        QVERIFY(stream >= 0);
        const double centre = station.streamCentreHz(stream);
        // A TX analyzer: this Core sends the transmit display.
        TxAnalyzer txAnalyzer(TxAnalyzer::kTxDispId);
        station.setTxAnalyzer(&txAnalyzer);
        const auto clearAnalyzer = qScopeGuard([&station] { station.setTxAnalyzer(nullptr); });
        StationServer server(&station, settings,
                             NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        server.setDisplayCodecV2Enabled(v2);
        QPointer<BridgeTransport> coreMedia;
        DaemonMediaController daemon(&server, &station, nullptr,
            [&coreMedia](QObject* owner) -> IMediaTransport* {
                coreMedia = new BridgeTransport(owner);
                return coreMedia;
            });
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true); // Display fixture opens no speaker.
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        client.withholdFeatureForTest(QByteArrayLiteral("remoteTx")); // Display-only fixture.
        PanadapterStack stack;
        auto* applet = stack.addPanadapter(QStringLiteral("only"));
        applet->setActiveSliceIndex(sliceId);
        applet->spectrumWidget()->setDisplayWindowPreservingHistory(centre, 48000);
        stack.resize(600, 400);
        stack.show();
        QVERIFY(QTest::qWaitForWindowExposed(&stack));
        QPointer<BridgeTransport> windowMedia;
        RemoteMediaController gui(&client, &remote, &stack, nullptr,
            [&windowMedia](QObject* owner) -> IMediaTransport* {
                windowMedia = new BridgeTransport(owner);
                return windowMedia;
            });
        QSignalSpy outbound(&server, &StationServer::mediaControlReceived);
        QSignalSpy inbound(&client, &StationClient::mediaControlReceived);
        QSignalSpy frames(&gui, &RemoteMediaController::displayFrameReceived);
        QSignalSpy transmitFrames(&gui, &RemoteMediaController::transmitFrameReceived);
        auto* stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(coreMedia && windowMedia);

        // What the Core offered and what the window's start asked.
        QCOMPARE(client.capabilities().displayCodecVersion, v2 ? 2 : 0);
        const QStringList names = capabilityNamesOn(clientLink);
        QVERIFY(names.size() >= 2);
        QCOMPARE(names.last(), QStringLiteral("coreBuildInfo"));
        QCOMPARE(names.contains(kField), v2);
        if (v2) {
            QCOMPARE(names.at(names.size() - 2), kField);
        }
        const QJsonObject start = lastOp(outbound, QStringLiteral("start"));
        QVERIFY(!start.isEmpty());
        QCOMPARE(start.contains(kField), v2);
        if (v2) {
            QCOMPARE(start.value(kField).toInt(), 2);
        }
        QVERIFY(gui.txDisplayNegotiated());

        coreMedia->other = windowMedia;
        coreMedia->activate();
        windowMedia->activate();
        QTRY_COMPARE(daemon.activeEndpointCount(), 1);
        QCOMPARE(daemon.displayCodecVersion(), v2 ? quint32{2} : quint32{1});
        QVector<float> iq(2048);
        for (int i = 0; i < iq.size(); i += 2) {
            iq[i] = 0.01f * static_cast<float>(std::cos(double(i) * 0.17));
            iq[i + 1] = 0.01f * static_cast<float>(std::sin(double(i) * 0.17));
        }
        const auto feed = [&] {
            QMetaObject::invokeMethod(&station, "rawIqDataForStream", Qt::DirectConnection,
                                      Q_ARG(int, stream), Q_ARG(QVector<float>, iq));
        };
        const auto keyframes = [&outbound] { return countOp(outbound, QStringLiteral("keyframe")); };
        const auto lastContext = [&inbound] { return lastOp(inbound, QStringLiteral("context")); };

        // 1. The receive context is accepted and its frames drawn.
        QTRY_VERIFY_WITH_TIMEOUT((feed(), frames.count() > 2), 5000);
        if (v2) {
            QCOMPARE(keyframes(), 0);
        } else {
            QTRY_VERIFY(keyframes() >= 1);
        }
        const qint64 receiveGeneration =
            lastContext().value(QStringLiteral("contextGeneration")).toInteger();

        // 2. A frame is lost on the way. v1: the next one needs a keyframe,
        //    the window asks once, and the pan draws again. v2: the next
        //    frame decodes alone.
        QTest::qWait(kPastKeyframeSpacingMs);
        int before = keyframes();
        const int framesAtLoss = frames.count();
        coreMedia->codecMessagesToLose = 1;
        QTRY_VERIFY_WITH_TIMEOUT((feed(), coreMedia->codecMessagesToLose == 0
                                              && frames.count() > framesAtLoss + 2), 5000);
        QCOMPARE(keyframes(), v2 ? before : before + 1);

        // 3. Keyed at the Core: the transmit context and its frames.
        QTest::qWait(kPastKeyframeSpacingMs);
        before = keyframes();
        MoxController* mox = station.moxController();
        QVERIFY(mox);
        // Today's Core is receive-only; lifting the pre-check stands in for
        // one that can transmit.
        mox->setMoxCheck({});
        mox->setMox(true);
        const auto unkey = qScopeGuard([mox] { mox->setMox(false); });
        QTRY_VERIFY(lastContext().value(QStringLiteral("transmit")).toBool());
        const int traceSamples = lastContext().value(QStringLiteral("traceSamples")).toInt();
        QVERIFY(traceSamples > 0);
        const QVector<float> trace(traceSamples, -30.0f);
        QTRY_VERIFY_WITH_TIMEOUT(([&] {
            emit txAnalyzer.txFftReady(-1, trace);
            emit txAnalyzer.txWaterfallReady(-1, trace);
            return transmitFrames.count() > 0;
        }()), 5000);
        if (v2) {
            QCOMPARE(keyframes(), before);
        } else {
            QTRY_VERIFY(keyframes() > before);
        }

        // 4. The fall: a newer receive context for the same request.
        QTest::qWait(kPastKeyframeSpacingMs);
        before = keyframes();
        mox->setMox(false);
        QTRY_VERIFY_WITH_TIMEOUT(([&] {
            feed();
            const QJsonObject context = lastContext();
            return context.value(QStringLiteral("transmit")) == QJsonValue(false)
                && context.value(QStringLiteral("contextGeneration")).toInteger()
                    > receiveGeneration;
        }()), 5000);
        const int framesAtFall = frames.count();
        QTRY_VERIFY_WITH_TIMEOUT((feed(), frames.count() > framesAtFall + 2), 5000);
        if (v2) {
            QCOMPARE(keyframes(), before);
            QCOMPARE(keyframes(), 0);
        } else {
            QTRY_VERIFY(keyframes() > before);
        }

        // Every display frame the Core sent carried the agreed version.
        int codecMessages = 0;
        for (const QByteArray& message : coreMedia->displays) {
            if (!message.startsWith("NSDC")) {
                continue;
            }
            ++codecMessages;
            QCOMPARE(static_cast<int>(static_cast<quint8>(message.at(4))), v2 ? 2 : 1);
        }
        QVERIFY(codecMessages > 0);
        client.disconnectFromStation(QStringLiteral("test complete"));
    }
};

QTEST_MAIN(TstDisplayCodecV2Link)
#include "tst_display_codec_v2_link.moc"
