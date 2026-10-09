// no-port-check: NereusSDR-original. No upstream logic is ported here.
// =================================================================
// tests/tst_core_speaker_link.cpp  (NereusSDR)
// =================================================================
//
// The Core speaker over the link (native audio plan Task 21; R-AUD-25,
// R-AUD-27, R-AUD-28; V-SW-9 link half): radio's coreSpeakerVolume,
// coreSpeakerMuted, coreSpeakerDevice and coreSpeakerDetails are
// Bidirectional and coreSpeakerDevices and coreSpeakerState Outbound, all
// six only to a peer that declared coreSpeaker 1 on a Core that has its
// own speaker, which alone is sent coreSpeakerVersion 1 (after
// radioSpeakerVersion, before coreBuildInfo). A peer that did not declare
// it sees the capabilities and radio object it saw before, byte for byte.
// A remote window's change lands through the Core's setter and is saved on
// the Core; another device follows. A window of an older Core shows the
// Core speaker disabled with the update reason and sends nothing.
//
// Fake engines only; nothing keys a radio or touches a device.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-09 - Written for NereusSDR by J.J. Boyd (KG4VCF), with
//                AI-assisted implementation via Anthropic Claude Code
//                (native audio plan Task 21).
//   2026-10-09 - Native audio plan Task 22: the card struct is
//                CoreSpeakerCardInfo. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
// =================================================================

#include "MultiDeviceHarness.h"

#include <QScopeGuard>
#include <QSignalSpy>

#include "core/AudioEngine.h"
#include "core/audio/CoreSpeakerJson.h"
#include "core/session/IStationLink.h"
#include "core/session/MirrorPolicy.h"
#include "core/session/StationClient.h"
#include "core/settings/SettingsProxy.h"

#include "fakes/FakeAudioEngineBackend.h"

namespace {

const char* const kCoreSpeakerProperties[] = {"coreSpeakerVolume",  "coreSpeakerMuted",
                                              "coreSpeakerDevice",  "coreSpeakerDevices",
                                              "coreSpeakerState",   "coreSpeakerDetails"};
const QString kOlderCore = QStringLiteral("This Core can't set its speaker from here. Update the Core.");
const QString kUsbId = QStringLiteral("Device,0");
const QString kUsbName = QStringLiteral("USB Audio Device");

// A link with nothing negotiated: IStationLink's defaults, as a window of
// a Core that predates the Core speaker sees them.
class OlderCoreLink : public IStationLink {
public:
    CommandOutcome requestAddSlice(const QString&) override { return {}; }
    CommandOutcome requestAddSliceOnPan(const QString&) override { return {}; }
    CommandOutcome requestRemoveSlice(int) override { return {}; }
    CommandOutcome requestActiveSlice(int) override { return {}; }
    CommandOutcome requestSliceSampleRate(int, int) override { return {}; }
};

bool sawProperty(const LoopbackTransport* app, const QString& property)
{
    for (const QByteArray& wire : app->received()) {
        const QJsonObject o = QJsonDocument::fromJson(wire).object();
        const QJsonArray lists[] = {o.value(QStringLiteral("properties")).toArray(),
                                    o.value(QStringLiteral("fields")).toArray()};
        for (const QJsonArray& list : lists) {
            for (const QJsonValue& p : list) {
                if (p.toObject().value(QStringLiteral("name")).toString() == property) {
                    return true;
                }
            }
        }
    }
    return false;
}

int radioDeltasFrom(const LoopbackTransport* app, qsizetype mark)
{
    int count = 0;
    const QList<QByteArray> received = app->received();
    for (qsizetype i = mark; i < received.size(); ++i) {
        const QJsonObject o = QJsonDocument::fromJson(received.at(i)).object();
        if (o.value(QStringLiteral("type")).toString() == QStringLiteral("delta")
            && o.value(QStringLiteral("key")).toString() == QStringLiteral("radio")) {
            ++count;
        }
    }
    return count;
}

// Every message of `type` (and, for a snapshot, of object `key`) the peer
// was sent, in order, as the bytes on the wire.
QList<QByteArray> wireOf(const LoopbackTransport* app, const QString& type, const QString& key = {})
{
    QList<QByteArray> out;
    for (const QByteArray& wire : app->received()) {
        const QJsonObject o = QJsonDocument::fromJson(wire).object();
        if (o.value(QStringLiteral("type")).toString() != type) {
            continue;
        }
        if (!key.isEmpty() && o.value(QStringLiteral("key")).toString() != key) {
            continue;
        }
        out.append(wire);
    }
    return out;
}

QString savedOnCore(const char* key)
{
    return AppSettings::instance()
        .value(QStringLiteral("audio/") + QLatin1String(key), QStringLiteral("<unset>"))
        .toString();
}

MirrorUpdate boolean(const char* name, bool value)
{
    return MirrorUpdate{0, QByteArray(name), MirrorWireKind::Bool, QVariant(value)};
}

QHash<QByteArray, int> coreSpeakerAsks()
{
    QHash<QByteArray, int> asks = kHolder;
    asks.insert(QByteArrayLiteral("coreSpeaker"), 1);
    return asks;
}

// The harness Core given its own speaker, as DaemonApp does, on a fake
// ALSA direct engine with a USB card and a built-in default.
struct SpeakerCore {
    Core core{/*upgradedWithToken=*/true};
    std::shared_ptr<FakeAudioEngineBackend> alsa =
        std::make_shared<FakeAudioEngineBackend>(AudioBackendId::AlsaDirect);

    explicit SpeakerCore(bool host = true)
    {
        AudioDeviceInfo usb;
        usb.backend = AudioBackendId::AlsaDirect;
        usb.direction = AudioDeviceDirection::Output;
        usb.id = kUsbId;
        usb.name = kUsbName;
        AudioDeviceInfo builtIn = usb;
        builtIn.id = QStringLiteral("PCH,0");
        builtIn.name = QStringLiteral("Built-in Audio");
        alsa->setDevices({usb, builtIn});
        alsa->setDefault(AudioDeviceDirection::Output, builtIn.id);
        AudioEngine* engine = core.model->audioEngine();
        engine->setVaxOutputsAllowed(false);
        engine->setAudioBackendsForTest({alsa});
        if (host) {
            core.model->setCoreSpeakerHost(true);
        }
    }
};

// A remote window on the loopback, signed in with the Core's token.
struct Window {
    RadioModel model{RadioModel::Role::Remote};
    SettingsProxy proxy;
    StationClient client{&model, &proxy};
    LoopbackTransport* stationEnd = nullptr;
    LoopbackTransport* clientEnd = nullptr;

    bool open(Core& core, QObject* owner)
    {
        QSignalSpy completed(&client, &StationClient::handshakeComplete);
        stationEnd = new LoopbackTransport(QStringLiteral("core-speaker-station"), owner);
        clientEnd = new LoopbackTransport(QStringLiteral("core-speaker-client"), owner);
        stationEnd->linkTo(clientEnd);
        client.startSession(clientEnd, core.server->token());
        core.server->acceptTransport(stationEnd);
        return QTest::qWaitFor([&completed]() { return completed.count() == 1; }, 5000);
    }
};

} // namespace

class TstCoreSpeakerLink : public QObject {
    Q_OBJECT

private slots:
    void init()
    {
        AppSettings& settings = AppSettings::instance();
        for (const char* key : {"Master/Volume", "Master/Muted", "Speakers/Engine",
                                "Speakers/DeviceId", "Speakers/DeviceName",
                                "Speakers/BufferSamples", "Speakers/SampleRate",
                                "Speakers/DelayMs", "Speakers/FirstChannel",
                                "Speakers/DriverApi", "Speakers/MicChannel"}) {
            settings.remove(QStringLiteral("audio/") + QLatin1String(key));
        }
    }

    // Four Bidirectional, two Outbound, all six gated on coreSpeaker 1.
    void policy_directionsAndGates()
    {
        for (const char* property : {"coreSpeakerVolume", "coreSpeakerMuted",
                                     "coreSpeakerDevice", "coreSpeakerDetails"}) {
            QVERIFY2(MirrorPolicy::hasExplicitEntry("RadioModel", property), property);
            QCOMPARE(MirrorPolicy::directionFor("RadioModel", property),
                     MirrorDirection::Bidirectional);
            QVERIFY2(MirrorPolicy::inboundAllowed("RadioModel", property), property);
        }
        for (const char* property : {"coreSpeakerDevices", "coreSpeakerState"}) {
            QVERIFY2(MirrorPolicy::hasExplicitEntry("RadioModel", property), property);
            QCOMPARE(MirrorPolicy::directionFor("RadioModel", property),
                     MirrorDirection::Outbound);
            QVERIFY2(!MirrorPolicy::inboundAllowed("RadioModel", property), property);
        }
        for (const char* property : kCoreSpeakerProperties) {
            const MirrorPolicy::FeatureGate* gate =
                MirrorPolicy::featureGateFor("RadioModel", property);
            QVERIFY2(gate != nullptr, property);
            QCOMPARE(QByteArray(gate->feature), QByteArrayLiteral("coreSpeaker"));
            QCOMPARE(gate->minVersion, 1);
        }
    }

    // coreSpeakerVersion reaches a declaring peer, after radioSpeakerVersion
    // and before coreBuildInfo; a peer that did not declare it is sent the
    // descriptor and radio object it was sent before, byte for byte; the
    // entry reads back.
    void capability_onlyToADeclaringPeer_otherPeerByteForByte()
    {
        const QString savedVersion = QCoreApplication::applicationVersion();
        const auto restore = qScopeGuard([&savedVersion]() {
            QCoreApplication::setApplicationVersion(savedVersion);
        });
        QCoreApplication::setApplicationVersion(QStringLiteral("0.5.2"));

        QHash<QByteArray, int> plain = kHolder;
        plain.insert(QByteArrayLiteral("radioSpeaker"), 1);
        plain.insert(QByteArrayLiteral("coreBuildInfo"), 1);
        QHash<QByteArray, int> asks = plain;
        asks.insert(QByteArrayLiteral("coreSpeaker"), 1);

        // Today's Core: no Core speaker at all.
        QList<QByteArray> beforeCaps;
        QList<QByteArray> beforeRadio;
        {
            SpeakerCore today(/*host=*/false);
            Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
            today.core.pair(b);
            LoopbackTransport* appB = today.core.signIn(b, plain);
            QVERIFY(admitted(appB));
            QTest::qWait(200);
            beforeCaps = wireOf(appB, QStringLiteral("capabilities"));
            beforeRadio = wireOf(appB, QStringLiteral("object.create"), QStringLiteral("radio"));
            QVERIFY(!capability(appB->received(), QStringLiteral("coreSpeakerVersion")).has_value());
        }
        init();

        SpeakerCore speaker;
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        speaker.core.pair(a);
        speaker.core.pair(b);
        LoopbackTransport* appA = speaker.core.signIn(a, asks);
        LoopbackTransport* appB = speaker.core.signIn(b, plain);
        QVERIFY(admitted(appA) && admitted(appB));
        QTest::qWait(200);
        QCOMPARE(capability(appA->received(), QStringLiteral("coreSpeakerVersion")), 1);
        QVERIFY(!capability(appB->received(), QStringLiteral("coreSpeakerVersion")).has_value());

        const QJsonArray caps = firstOfType(appA->received(), QStringLiteral("capabilities"))
                                    .value(QStringLiteral("properties")).toArray();
        QVERIFY(caps.size() >= 3);
        QCOMPARE(caps.at(caps.size() - 3).toObject().value(QStringLiteral("name")).toString(),
                 QStringLiteral("radioSpeakerVersion"));
        QCOMPARE(caps.at(caps.size() - 2).toObject().value(QStringLiteral("name")).toString(),
                 QStringLiteral("coreSpeakerVersion"));
        QCOMPARE(caps.last().toObject().value(QStringLiteral("name")).toString(),
                 QStringLiteral("coreBuildInfo"));

        // The peer that did not declare it: what it was sent before.
        QVERIFY(!beforeCaps.isEmpty());
        QVERIFY(!beforeRadio.isEmpty());
        QCOMPARE(wireOf(appB, QStringLiteral("capabilities")), beforeCaps);
        QCOMPARE(wireOf(appB, QStringLiteral("object.create"), QStringLiteral("radio")), beforeRadio);

        StationCapabilities sent;
        sent.radioSpeakerVersion = 1;
        sent.coreSpeakerVersion = 1;
        const QList<MirrorUpdate> updates = sent.toUpdates();
        QCOMPARE(StationCapabilities::fromUpdates(updates).coreSpeakerVersion, 1);
        QCOMPARE(StationCapabilities::fromUpdates(StationCapabilities{}.toUpdates())
                     .coreSpeakerVersion, 0);
        qsizetype radioAt = -1;
        qsizetype coreAt = -1;
        for (qsizetype i = 0; i < updates.size(); ++i) {
            if (updates.at(i).name == "radioSpeakerVersion") {
                radioAt = i;
            } else if (updates.at(i).name == "coreSpeakerVersion") {
                coreAt = i;
            }
        }
        QVERIFY(radioAt >= 0);
        QCOMPARE(coreAt, radioAt + 1);
    }

    // A desktop that hosts a station has no Core speaker: a declaring peer
    // is sent no coreSpeakerVersion and none of the six.
    void capability_notFromAStationWithoutItsOwnSpeaker()
    {
        SpeakerCore desk(/*host=*/false);
        Device a;
        desk.core.pair(a);
        LoopbackTransport* appA = desk.core.signIn(a, coreSpeakerAsks());
        QVERIFY(admitted(appA));
        QTest::qWait(200);
        QVERIFY(!capability(appA->received(), QStringLiteral("coreSpeakerVersion")).has_value());
        for (const char* property : kCoreSpeakerProperties) {
            QVERIFY2(!sawProperty(appA, QString::fromLatin1(property)), property);
        }
    }

    // A declaring peer is sent the six properties and their changes; one
    // that did not declare it never sees a name, and a change to them alone
    // sends it no radio delta.
    void properties_onlyToADeclaringPeer()
    {
        SpeakerCore speaker;
        Core& core = speaker.core;
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        LoopbackTransport* appA = core.signIn(a, coreSpeakerAsks());
        LoopbackTransport* appB = core.signIn(b);
        QVERIFY(admitted(appA) && admitted(appB));
        for (const char* property : kCoreSpeakerProperties) {
            QVERIFY2(sawProperty(appA, QString::fromLatin1(property)), property);
        }
        QTest::qWait(200);
        const qsizetype mark = appB->received().size();

        core.model->setCoreSpeakerVolume(33);
        core.model->setCoreSpeakerMuted(true);
        QTRY_COMPARE(latest(appA->received(), QStringLiteral("radio"),
                            QStringLiteral("coreSpeakerVolume")).toInteger(), 33);
        QTRY_COMPARE(latest(appA->received(), QStringLiteral("radio"),
                            QStringLiteral("coreSpeakerMuted")).toBool(), true);
        QTest::qWait(200);

        for (const char* property : kCoreSpeakerProperties) {
            QVERIFY2(!sawProperty(appB, QString::fromLatin1(property)), property);
        }
        QCOMPARE(radioDeltasFrom(appB, mark), 0);
    }

    // A peer that did not declare coreSpeaker is refused a write of them,
    // and the Core's value stays; a declaring peer's write lands; the
    // devices and state are the Core's own.
    void write_refusedFromAPeerThatDidNotDeclare_readOnlyDenied()
    {
        SpeakerCore speaker;
        Core& core = speaker.core;
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        LoopbackTransport* appA = core.signIn(a, coreSpeakerAsks());
        LoopbackTransport* appB = core.signIn(b);
        QVERIFY(admitted(appA) && admitted(appB));
        QCOMPARE(core.model->coreSpeakerVolume(), 50);

        appB->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
            "radio", {int64("coreSpeakerVolume", 12)}, 71)));
        QTRY_VERIFY(!propertyResult(appB, 71).isEmpty());
        const QJsonArray refused =
            propertyResult(appB, 71).value(QStringLiteral("results")).toArray();
        QCOMPARE(refused.size(), 1);
        QCOMPARE(refused.at(0).toObject().value(QStringLiteral("accepted")).toBool(true), false);
        QCOMPARE(refused.at(0).toObject().value(QStringLiteral("reason")).toString(),
                 QStringLiteral("Update this app to change the Core speaker on this Core."));
        QCOMPARE(core.model->coreSpeakerVolume(), 50);
        QCOMPARE(savedOnCore("Master/Volume"), QStringLiteral("<unset>"));

        appA->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
            "radio", {int64("coreSpeakerVolume", 12)}, 72)));
        QTRY_COMPARE(core.model->coreSpeakerVolume(), 12);

        const QString stateBefore = core.model->coreSpeakerState();
        const QString devicesBefore = core.model->coreSpeakerDevices();
        const QString forgedState = coreSpeakerStateToJson(
            CoreSpeakerState{CoreSpeakerStateKind::Playing, kUsbName, kUsbName, true});
        const QString forgedDevices = coreSpeakerDevicesToJson(
            {CoreSpeakerCardInfo{QStringLiteral("x"), QStringLiteral("Forged"), AudioDeviceState::Present}});
        appA->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
            "radio", {utf8("coreSpeakerState", forgedState)}, 73)));
        appA->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
            "radio", {utf8("coreSpeakerDevices", forgedDevices)}, 74)));
        QTRY_VERIFY(!propertyResult(appA, 73).isEmpty());
        QTRY_VERIFY(!propertyResult(appA, 74).isEmpty());
        for (const qint64 id : {qint64(73), qint64(74)}) {
            QCOMPARE(propertyResult(appA, id).value(QStringLiteral("results")).toArray().at(0)
                         .toObject().value(QStringLiteral("accepted")).toBool(true),
                     false);
        }
        QCOMPARE(core.model->coreSpeakerState(), stateBefore);
        QCOMPARE(core.model->coreSpeakerDevices(), devicesBefore);
    }

    // V-SW-9: two remotes.  One window's change lands through the Core's
    // setter, is saved on the Core and reaches the other; the other's
    // reaches the first.  The devices and state are the Core's.
    void remoteWindows_roundTrip()
    {
        SpeakerCore speaker;
        Core& core = speaker.core;
        core.model->audioEngine()->start();
        QTRY_VERIFY_WITH_TIMEOUT(!speaker.alsa->outputRequests().empty(), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(
            coreSpeakerDevicesFromJson(core.model->coreSpeakerDevices()).value().size(), 2, 5000);

        // Values that differ from every default, so the snapshot the
        // windows open with is the Core's.
        core.model->setCoreSpeakerVolume(72);
        core.model->setCoreSpeakerMuted(true);

        Window first;
        Window second;
        QVERIFY(first.open(core, this));
        QVERIFY(second.open(core, this));
        RadioModel& one = first.model;
        RadioModel& two = second.model;
        QVERIFY(first.client.coreSpeakerAvailable());
        QVERIFY(!first.client.coreSpeakerNeedsNewerCore());
        QVERIFY(one.coreSpeakerAvailable());
        QVERIFY(one.coreSpeakerUnavailableReason().isEmpty());
        QTRY_COMPARE(one.coreSpeakerVolume(), 72);
        QTRY_VERIFY(one.coreSpeakerMuted());
        QTRY_COMPARE(two.coreSpeakerVolume(), 72);
        QTRY_COMPARE(one.coreSpeakerDevices(), core.model->coreSpeakerDevices());
        QTRY_COMPARE(one.coreSpeakerState(), core.model->coreSpeakerState());
        QCOMPARE(one.coreSpeakerDevice(), core.model->coreSpeakerDevice());
        QCOMPARE(one.coreSpeakerDetails(), core.model->coreSpeakerDetails());

        QSignalSpy coreVolume(core.model.get(), &RadioModel::coreSpeakerVolumeChanged);
        one.setCoreSpeakerVolume(40);
        QTRY_COMPARE(core.model->coreSpeakerVolume(), 40);
        QCOMPARE(coreVolume.count(), 1);
        QCOMPARE(savedOnCore("Master/Volume"), QStringLiteral("0.400"));
        QCOMPARE(core.model->audioEngine()->volume(), 0.4f);
        QTRY_COMPARE(two.coreSpeakerVolume(), 40);

        two.setCoreSpeakerMuted(false);
        QTRY_VERIFY(!core.model->coreSpeakerMuted());
        QCOMPARE(savedOnCore("Master/Muted"), QStringLiteral("False"));
        QTRY_VERIFY(!one.coreSpeakerMuted());

        // A card pick from the first window: saved on the Core, the Core's
        // speakers reopen on it, and both windows follow.
        const std::size_t opensBefore = speaker.alsa->outputRequests().size();
        one.setCoreSpeakerDevice(coreSpeakerDeviceToJson(kUsbId, kUsbName));
        QTRY_COMPARE(savedOnCore("Speakers/DeviceId"), kUsbId);
        QCOMPARE(savedOnCore("Speakers/DeviceName"), kUsbName);
        QCOMPARE(savedOnCore("Speakers/Engine"), QStringLiteral("AlsaDirect"));
        QTRY_VERIFY(speaker.alsa->outputRequests().size() > opensBefore);
        QCOMPARE(speaker.alsa->outputRequests().back().deviceId, kUsbId);
        QTRY_COMPARE(two.coreSpeakerDevice(), coreSpeakerDeviceToJson(kUsbId, kUsbName));
        QTRY_COMPARE(two.coreSpeakerState(), core.model->coreSpeakerState());
        QCOMPARE(coreSpeakerStateFromJson(two.coreSpeakerState()).value().chosenName, kUsbName);

        // Details from the second window.
        CoreSpeakerDetails asked = coreSpeakerDetailsFromJson(two.coreSpeakerDetails()).value();
        asked.bufferFrames = 512;
        two.setCoreSpeakerDetails(coreSpeakerDetailsToJson(asked));
        QTRY_COMPARE(savedOnCore("Speakers/BufferSamples"), QStringLiteral("512"));
        QTRY_COMPARE(coreSpeakerDetailsFromJson(one.coreSpeakerDetails()).value().bufferFrames, 512);

        // Another device's write over the wire.
        Device phone;
        core.pair(phone);
        LoopbackTransport* app = core.signIn(phone, coreSpeakerAsks());
        QVERIFY(admitted(app));
        app->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
            "radio", {boolean("coreSpeakerMuted", true)}, 81)));
        QTRY_VERIFY(core.model->coreSpeakerMuted());
        QCOMPARE(savedOnCore("Master/Muted"), QStringLiteral("True"));
        QTRY_VERIFY(one.coreSpeakerMuted());
        QTRY_VERIFY(two.coreSpeakerMuted());

        // A window cannot make up the Core's reports.
        QVERIFY(!one.applyStationCoreSpeakerValue("coreSpeakerState", QStringLiteral("loud")));
        QCOMPARE(one.coreSpeakerState(), core.model->coreSpeakerState());

        // The link drops: the reports go back to nothing known.
        QSignalSpy ended(&first.client, &StationClient::sessionEnded);
        first.stationEnd->closeLink(QStringLiteral("Core restarting"));
        QTRY_VERIFY(ended.count() >= 1);
        QVERIFY(!first.client.coreSpeakerAvailable());
        QVERIFY(!first.client.coreSpeakerNeedsNewerCore());
        QCOMPARE(one.coreSpeakerDevices(), QStringLiteral("[]"));
        QCOMPARE(one.coreSpeakerUnavailableReason(), QStringLiteral("Connect to the Core to change these."));
    }

    // A window of a Core without the capability: disabled with the update
    // reason, and the setters change nothing and send nothing.
    void olderCore_disabledAndSilent()
    {
        RadioModel remote(RadioModel::Role::Remote);
        OlderCoreLink link;
        remote.attachStation(&link);
        remote.setStationConnectionState(ConnectionState::Connected);
        QVERIFY(!remote.coreSpeakerAvailable());
        QVERIFY(remote.coreSpeakerNeedsNewerCore());
        QCOMPARE(remote.coreSpeakerUnavailableReason(), kOlderCore);
        QCOMPARE(IStationLink::coreSpeakerUnavailableReason(), kOlderCore);

        QSignalSpy volume(&remote, &RadioModel::coreSpeakerVolumeChanged);
        QSignalSpy muted(&remote, &RadioModel::coreSpeakerMutedChanged);
        QSignalSpy device(&remote, &RadioModel::coreSpeakerDeviceChanged);
        QSignalSpy details(&remote, &RadioModel::coreSpeakerDetailsChanged);
        const int before = remote.coreSpeakerVolume();
        const QString deviceBefore = remote.coreSpeakerDevice();
        remote.setCoreSpeakerVolume(before == 10 ? 20 : 10);
        remote.setCoreSpeakerMuted(!remote.coreSpeakerMuted());
        remote.setCoreSpeakerDevice(coreSpeakerDeviceToJson(kUsbId, kUsbName));
        CoreSpeakerDetails asked;
        asked.bufferFrames = 512;
        remote.setCoreSpeakerDetails(coreSpeakerDetailsToJson(asked));
        QCOMPARE(volume.count(), 0);
        QCOMPARE(muted.count(), 0);
        QCOMPARE(device.count(), 0);
        QCOMPARE(details.count(), 0);
        QCOMPARE(remote.coreSpeakerVolume(), before);
        QCOMPARE(remote.coreSpeakerDevice(), deviceBefore);
        remote.attachStation(nullptr);
    }

    // The same over the wire: a Core whose capabilities do not carry
    // coreSpeakerVersion turns the Core speaker off in the window, and the
    // window's change reaches neither the wire nor the Core.
    void olderCore_overTheWire_sendsNothing()
    {
        SpeakerCore speaker;
        Core& core = speaker.core;
        Window window;
        QVERIFY(window.open(core, this));
        RadioModel& remote = window.model;
        QVERIFY(window.client.coreSpeakerAvailable());

        // The Core's own descriptor, sent again without the entry.
        QJsonObject caps = firstOfType(window.clientEnd->received(), QStringLiteral("capabilities"));
        QJsonArray entries = caps.value(QStringLiteral("properties")).toArray();
        for (qsizetype i = entries.size() - 1; i >= 0; --i) {
            if (entries.at(i).toObject().value(QStringLiteral("name")).toString()
                == QStringLiteral("coreSpeakerVersion")) {
                entries.removeAt(i);
            }
        }
        caps.insert(QStringLiteral("properties"), entries);
        window.stationEnd->sendText(QJsonDocument(caps).toJson(QJsonDocument::Compact));
        QTRY_VERIFY(!window.client.coreSpeakerAvailable());
        QVERIFY(window.client.coreSpeakerNeedsNewerCore());
        QVERIFY(!remote.coreSpeakerAvailable());
        QCOMPARE(remote.coreSpeakerUnavailableReason(), kOlderCore);

        const qsizetype mark = window.stationEnd->received().size();
        const int coreBefore = core.model->coreSpeakerVolume();
        remote.setCoreSpeakerVolume(coreBefore == 10 ? 20 : 10);
        remote.setCoreSpeakerMuted(true);
        remote.setCoreSpeakerDevice(coreSpeakerDeviceToJson(kUsbId, kUsbName));
        QTest::qWait(300);
        const QList<QByteArray> sent = window.stationEnd->received();
        for (qsizetype i = mark; i < sent.size(); ++i) {
            QVERIFY2(!sent.at(i).contains("coreSpeaker"), sent.at(i).constData());
        }
        QCOMPARE(core.model->coreSpeakerVolume(), coreBefore);
        QVERIFY(!core.model->coreSpeakerMuted());
        QCOMPARE(savedOnCore("Speakers/DeviceId"), QStringLiteral("<unset>"));
    }
};

QTEST_MAIN(TstCoreSpeakerLink)
#include "tst_core_speaker_link.moc"
