// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_recordings_link.cpp  (NereusSDR)
// =================================================================
//
// Recordings over the link (recording design section 6.9): the `recordings`
// object and recordingsVersion for a peer that declared the feature and
// nothing for one that did not, Record from any device (A1), a recording
// that keeps going while a device's link drops, rename and delete seen on
// every device, each device's own playback (V6) and a copy fetched in
// chunks.
//
// Loopback link, no RF and no hardware. The recordings go to a temporary
// folder as WAV, on an injected clock; nothing keys.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-15 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code. Recording plan
//                 Task 5.
//   2026-11-15 - J.J. Boyd (KG4VCF), recording plan Task 7: recordings.list
//                 by slice and kind, and the reader in its answer.
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-11-15 - J.J. Boyd (KG4VCF), recording plan Task 10: version 2 and
//                 the five on-air switches. AI-assisted implementation via
//                 Anthropic Claude Code.
//   2026-11-16 - J.J. Boyd (KG4VCF), recording plan Task 11: the voice
//                 keyer's verbs (recordingsVersion 3); a device's send keys
//                 as it and its stop unkeys; another device's send is
//                 refused while the first holds transmit. AI-assisted via
//                 Anthropic Claude Code.
//   2026-11-16 - J.J. Boyd (KG4VCF), recording plan Task 11 review fixes:
//                 the sender's keepalives keep its message on the air past
//                 the watchdog's 3 s and without them it is cut; another
//                 device's press and Stop leave it going, the radio's own
//                 PTT takes it; the sender's own key hands over; a session's
//                 import file goes with it. AI-assisted via Anthropic Claude
//                 Code.
// =================================================================

#include "MultiDeviceHarness.h"

#include <QLoggingCategory>
#include <QRandomGenerator>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimeZone>

#include <memory>
#include <optional>
#include <vector>

#include "core/recordings/AudioFileWriter.h"
#include "core/recordings/OnAirSource.h"
#include "core/recordings/RecordingService.h"
#include "core/recordings/RecordingStore.h"
#include "core/recordings/VoiceKeyer.h"
#include "core/safety/RemoteTxWatchdog.h"
#include "core/session/RecordingsFacade.h"
#include "core/session/SessionCommandDispatcher.h"
#include "core/session/StationCapabilities.h"

namespace {

const QString kUpdateApp = QStringLiteral("Update this app to use recordings on this Core.");
const QString kNotUnderstood = QStringLiteral("The request for recordings was not understood.");

const QHash<QByteArray, int> kRecordings{
    {"deviceAuth", 1}, {"sessionHolder", 1}, {"recordings", 1}};

QJsonObject parse(const QString& json)
{
    return QJsonDocument::fromJson(json.toUtf8()).object();
}

QJsonObject mirrored(const LoopbackTransport* app)
{
    return parse(latest(app->received(), QStringLiteral("recordings"), QStringLiteral("json")).toString());
}

QString reasonOf(const QJsonObject& result)
{
    return result.value(QStringLiteral("reason")).toString(QStringLiteral("(no answer)"));
}

// Empty when accepted, otherwise why not.
QString refusalOf(const QJsonObject& result)
{
    return result.value(QStringLiteral("accepted")).toBool(false) ? QString() : reasonOf(result);
}

QJsonValue valueIn(const QJsonObject& result, const QString& name)
{
    for (const QJsonValue& value : result.value(QStringLiteral("values")).toArray()) {
        if (value.toObject().value(QStringLiteral("name")).toString() == name) {
            return value.toObject().value(QStringLiteral("value"));
        }
    }
    return {};
}

QJsonObject sliceIn(const QJsonObject& document, int slice)
{
    for (const QJsonValue& value : document.value(QStringLiteral("slices")).toArray()) {
        if (value.toObject().value(QStringLiteral("slice")).toInt(-1) == slice) {
            return value.toObject();
        }
    }
    return {};
}

QString stateIn(const QJsonObject& document, int slice)
{
    return sliceIn(document, slice).value(QStringLiteral("state")).toString();
}

// The recordings.playback pushes a session received, oldest first.
QList<QJsonObject> playbackPushes(const LoopbackTransport* app)
{
    QList<QJsonObject> out;
    for (const QJsonObject& o : ofType(app->received(), QStringLiteral("command.result"))) {
        if (o.value(QStringLiteral("verb")).toString() == QLatin1String("recordings.playback")) {
            out.append(o);
        }
    }
    return out;
}

// A session keepalive as a client sends it with no key of its own
// (RemoteTransmitClient::kReleaseAnyEpoch).
QByteArray keepaliveWire(quint64 sequence)
{
    const QList<MirrorUpdate> arguments{
        MirrorUpdate{0, QByteArrayLiteral("sequence"), MirrorWireKind::Int64,
                     QVariant(static_cast<qint64>(sequence))},
        MirrorUpdate{0, QByteArrayLiteral("epoch"), MirrorWireKind::Int64, QVariant(qint64(0xFFFFFFFFLL))}};
    return SessionMessages::encode(SessionMessages::commandInvoke(
        QByteArrayLiteral("tx.keepalive"), static_cast<quint32>(sequence), arguments));
}

QJsonObject voiceSending(const LoopbackTransport* app)
{
    return mirrored(app).value(QStringLiteral("voiceKeyer")).toObject().value(QStringLiteral("sending")).toObject();
}

QStringList listedNames(const QJsonObject& result)
{
    QStringList names;
    const QJsonArray page =
        QJsonDocument::fromJson(valueIn(result, QStringLiteral("entries")).toString().toUtf8()).array();
    for (const QJsonValue& value : page) {
        names.append(value.toObject().value(QStringLiteral("name")).toString());
    }
    return names;
}

}  // namespace

class TstRecordingsLink : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    int m_disks = 0;
    qint64 m_clockMs = 0;

    // The Core's recordings in a folder of their own, as WAV, on the test's clock.
    void useTestDisk(Core& core)
    {
        RecordingService* service = core.model->recordingService();
        QVERIFY(service != nullptr);
        m_clockMs = QDateTime(QDate(2026, 11, 15), QTime(14, 32, 0), QTimeZone::UTC).toMSecsSinceEpoch();
        service->setClockForTest([this] { return m_clockMs; });
        const QString disk = m_dir.filePath(QStringLiteral("disk%1").arg(++m_disks));
        QVERIFY(QDir().mkpath(disk));
        RecordingSettings s = service->settings();
        s.disk = disk;
        s.format = AudioFileFormat::Wav;
        s.autoStartOnTransmit = false;
        service->setSettings(s);
    }

    // A file of `bytes` in the Core's recordings folder and its row; the id.
    static QString addFile(Core& core, const QString& name, const QByteArray& bytes)
    {
        RecordingStore* store = core.model->recordingService()->store();
        if (store == nullptr || store->folder().isEmpty()) {
            return {};
        }
        const QString fileName = name + QStringLiteral(".wav");
        QFile file(QDir(store->folder()).filePath(fileName));
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size()) {
            return {};
        }
        file.close();
        RecordingEntry e;
        e.kind = RecordingKind::Contact;
        e.sliceId = 0;
        e.startUtc = QDateTime(QDate(2026, 11, 15), QTime(14, 0, 0), QTimeZone::UTC);
        e.lengthMs = 1000;
        e.frequencyHz = 7236400;
        e.mode = QStringLiteral("LSB");
        e.name = name;
        e.fileName = fileName;
        return store->add(e);
    }

    // A real one-second WAV the player can open.
    static QString addPlayable(Core& core, const QString& name)
    {
        RecordingStore* store = core.model->recordingService()->store();
        if (store == nullptr || store->folder().isEmpty()) {
            return {};
        }
        const QString fileName = name + QStringLiteral(".wav");
        QString error;
        std::unique_ptr<AudioFileWriter> writer = AudioFileWriter::open(
            QDir(store->folder()).filePath(fileName), AudioFileSpec{AudioFileFormat::Wav, 24000, 1, 0},
            &error);
        if (!writer) {
            return {};
        }
        const std::vector<float> quiet(24000, 0.0f);
        if (!writer->write(quiet.data(), static_cast<int>(quiet.size())) || !writer->finish()) {
            return {};
        }
        RecordingEntry e;
        e.kind = RecordingKind::Contact;
        e.sliceId = 0;
        e.startUtc = QDateTime(QDate(2026, 11, 15), QTime(13, 0, 0), QTimeZone::UTC);
        e.lengthMs = 1000;
        e.frequencyHz = 14250000;
        e.mode = QStringLiteral("USB");
        e.name = name;
        e.fileName = fileName;
        return store->add(e);
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        AppSettings::setProfileOverride(
            QStringLiteral("recordings-link-%1").arg(QCoreApplication::applicationPid()));
        // The harness turns its heartbeat off on purpose and a model with no
        // radio logs each tune it cannot push.
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.*.debug=false\n"
                                                        "nereus.*.info=false\n"
                                                        "nereussdr.*.info=false\n"
                                                        "nereus.connection.warning=false\n"
                                                        "nereus.dsp.warning=false\n"
                                                        "nereus.station.warning=false\n"
                                                        "nereus.stationclient.warning=false"));
    }

    void init()
    {
        AppSettings::instance().clear();
        AppSettings::instance().setValue(QStringLiteral("SettingsSchemaVersion"), QStringLiteral("7"));
    }

    void cleanupTestCase()
    {
        const QString path = AppSettings::instance().filePath();
        QFile::remove(path);
        QFile::remove(path + QStringLiteral(".bak"));
    }

    void theVerbsAreDeclaredWithTheirCapability()
    {
        const QSet<QByteArray> expected{
            "recordings.start", "recordings.stop", "recordings.list", "recordings.play",
            "recordings.stopPlay", "recordings.seek", "recordings.rename", "recordings.delete",
            "recordings.export", "recordings.setSettings"};
        QSet<QByteArray> found;
        for (const CommandVerbSpec& spec : SessionCommandDispatcher::verbSpecs()) {
            if (spec.verb.startsWith("recordings.")) {
                found.insert(spec.verb);
                QCOMPARE(spec.capability, QByteArrayLiteral("recordingsVersion"));
                QCOMPARE(spec.capabilityVersion, 1);
                QCOMPARE(int(spec.minMinor), int(kRadioIdentitySessionProtocolMinor));
            }
        }
        QCOMPARE(found, expected);
    }

    void theVoiceKeyerVerbsAreDeclaredWithVersionThree()
    {
        const QSet<QByteArray> expected{
            "voiceKeyer.send", "voiceKeyer.stop", "voiceKeyer.beginImport", "voiceKeyer.chunk",
            "voiceKeyer.finishImport", "voiceKeyer.export", "voiceKeyer.rename", "voiceKeyer.clear",
            "voiceKeyer.setRepeat", "voiceKeyer.listen"};
        QSet<QByteArray> found;
        for (const CommandVerbSpec& spec : SessionCommandDispatcher::verbSpecs()) {
            if (spec.verb.startsWith("voiceKeyer.")) {
                found.insert(spec.verb);
                QCOMPARE(spec.capability, QByteArrayLiteral("recordingsVersion"));
                QCOMPARE(spec.capabilityVersion, 3);
                QCOMPARE(int(spec.minMinor), int(kRadioIdentitySessionProtocolMinor));
            }
        }
        QCOMPARE(found, expected);
        QCOMPARE(RecordingsFacade::kVersion, 3);
    }

    // A device's send keys the radio as that device and its stop unkeys;
    // while it holds transmit another device's send is refused with the
    // holder's words and nothing keys.
    // M1 holds a second of sound on the Core's voice keyer, in a folder of
    // its own. Nothing pulls the transmit blocks here, so a message sent
    // stays on the air until something stops it.
    VoiceKeyer* voiceKeyerWithMessage(Core& core)
    {
        VoiceKeyer* vk = core.model->recordingService()->voiceKeyer();
        if (vk == nullptr) {
            return nullptr;
        }
        vk->setFolder(m_dir.filePath(QStringLiteral("voice%1").arg(++m_disks)));
        const QString wav = m_dir.filePath(QStringLiteral("voice-in-%1.wav").arg(m_disks));
        QString error;
        std::unique_ptr<AudioFileWriter> w =
            AudioFileWriter::open(wav, AudioFileSpec{AudioFileFormat::Wav, 48000, 1, 0}, &error);
        const std::vector<float> samples(48000, 0.25f);
        if (!w || !w->write(samples.data(), 48000) || !w->finish()) {
            return nullptr;
        }
        auto done = std::make_shared<std::optional<bool>>();
        vk->importFile(1, wav, QStringLiteral("CQ"), [done](bool ok, const QString&) { *done = ok; });
        if (!QTest::qWaitFor([done] { return done->has_value(); }, 30000) || !**done) {
            return nullptr;
        }
        return vk;
    }

    static QHash<QByteArray, int> voiceFeatures()
    {
        QHash<QByteArray, int> features = kRecordings;
        for (auto it = kTransmitter.cbegin(); it != kTransmitter.cend(); ++it) {
            features.insert(it.key(), it.value());
        }
        return features;
    }

    void aDevicesSendKeysAsItAndItsStopUnkeys()
    {
        Core core;
        useTestDisk(core);
        allowTransmit(core);
        VoiceKeyer* vk = voiceKeyerWithMessage(core);
        QVERIFY(vk != nullptr);
        const QHash<QByteArray, int> features = voiceFeatures();
        Device phone;
        Device tablet(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(phone);
        core.pair(tablet);
        LoopbackTransport* first = core.signIn(phone, features);
        LoopbackTransport* second = core.signIn(tablet, features);
        QVERIFY(admitted(first));
        QVERIFY(admitted(second));
        MoxController* mox = core.model->moxController();

        QCOMPARE(reasonOf(core.invoke(first, "voiceKeyer.send", {int64("n", 3)})),
                 QStringLiteral("M3 is empty."));
        QVERIFY(!mox->isMox());

        QCOMPARE(refusalOf(core.invoke(first, "voiceKeyer.send", {int64("n", 1)})), QString());
        QVERIFY(mox->isMox());
        QCOMPARE(mox->currentKeyer().deviceId, phone.key.fingerprint());
        QTRY_VERIFY(!mirrored(second).value(QStringLiteral("voiceKeyer")).toObject()
                         .value(QStringLiteral("sending")).isNull());

        const QString held = reasonOf(core.invoke(second, "voiceKeyer.send", {int64("n", 1)}));
        QVERIFY(!held.isEmpty());
        QCOMPARE(held, core.server->transmitHolder()->keyRefusalFor(tablet.key.fingerprint()).text);
        QCOMPARE(mox->currentKeyer().deviceId, phone.key.fingerprint());
        QCOMPARE(voiceSending(second).value(QStringLiteral("deviceId")).toString(),
                 StationIdentity::toBase64Url(phone.key.fingerprint()));

        // Ruling 8.5: the tablet's Stop and its PTT are refused, and the
        // message goes on.
        const QString holderName = core.server->transmitHolder()->holder()->name;
        QCOMPARE(reasonOf(core.invoke(second, "voiceKeyer.stop", {})),
                 TxRefusals::otherDeviceHoldsStop(holderName).text);
        const QJsonObject press = core.invoke(
            second, "tx.key", {MirrorUpdate{0, "trigger", MirrorWireKind::Utf8, QStringLiteral("screen")}});
        QVERIFY(!press.value(QStringLiteral("accepted")).toBool(true));
        QCoreApplication::processEvents();
        QVERIFY(mox->isMox());
        QCOMPARE(mox->currentKeyer().deviceId, phone.key.fingerprint());
        QVERIFY(vk->sending().has_value());
        QCOMPARE(core.model->recordingService()->onAirSource()->active(), OnAirSource::Kind::VoiceMessage);

        QCOMPARE(refusalOf(core.invoke(first, "voiceKeyer.stop", {})), QString());
        QVERIFY(!mox->isMox());
        QTRY_VERIFY(mirrored(second).value(QStringLiteral("voiceKeyer")).toObject()
                        .value(QStringLiteral("sending")).isNull());
        QCOMPARE(mirrored(second).value(QStringLiteral("voiceKeyer")).toObject()
                     .value(QStringLiteral("slots")).toArray().size(), 8);
    }

    // The sender's own tx.key, once admitted, hands the air from its
    // message to its microphone; the radio stays keyed for it.
    void theSendersOwnKeyHandsOver()
    {
        Core core;
        useTestDisk(core);
        allowTransmit(core);
        VoiceKeyer* vk = voiceKeyerWithMessage(core);
        QVERIFY(vk != nullptr);
        Device phone;
        core.pair(phone);
        LoopbackTransport* app = core.signIn(phone, voiceFeatures());
        QVERIFY(admitted(app));
        MoxController* mox = core.model->moxController();
        QCOMPARE(refusalOf(core.invoke(app, "voiceKeyer.send", {int64("n", 1)})), QString());
        QVERIFY(vk->sending().has_value());
        const QJsonObject key = core.invoke(
            app, "tx.key", {MirrorUpdate{0, "trigger", MirrorWireKind::Utf8, QStringLiteral("screen")}});
        QVERIFY2(key.value(QStringLiteral("accepted")).toBool(), qPrintable(reasonOf(key)));
        QVERIFY(!vk->sending().has_value());
        QVERIFY(mox->isMox());
        QCOMPARE(mox->currentKeyer().deviceId, phone.key.fingerprint());
        QCOMPARE(core.model->recordingService()->onAirSource()->active(), OnAirSource::Kind::None);
    }

    // Ruling 8.9: the radio's own PTT takes transmit, which ends another
    // device's message.
    void theRadiosOwnPttTakesIt()
    {
        Core core;
        useTestDisk(core);
        allowTransmit(core);
        VoiceKeyer* vk = voiceKeyerWithMessage(core);
        QVERIFY(vk != nullptr);
        Device phone;
        core.pair(phone);
        LoopbackTransport* app = core.signIn(phone, voiceFeatures());
        QVERIFY(admitted(app));
        MoxController* mox = core.model->moxController();
        QCOMPARE(refusalOf(core.invoke(app, "voiceKeyer.send", {int64("n", 1)})), QString());
        QCOMPARE(mox->currentKeyer().deviceId, phone.key.fingerprint());
        mox->setMox(true, KeyerIdentity::station(PttMode::Mic));
        QTRY_VERIFY(!vk->sending().has_value());
        QTRY_VERIFY(!mox->isMox() || mox->currentKeyer().isStation());
        QCOMPARE(core.model->recordingService()->onAirSource()->active() == OnAirSource::Kind::VoiceMessage,
                 false);
    }

    // C1: the message keys as the phone, so the watchdog watches it. The
    // phone's keepalives keep it on the air past the 3 s cut; without them
    // it is cut at 3 s and the radio unkeys.
    void theSendersKeepalivesKeepItsMessageOnTheAir()
    {
        Core core;
        useTestDisk(core);
        allowTransmit(core);
        VoiceKeyer* vk = voiceKeyerWithMessage(core);
        QVERIFY(vk != nullptr);
        Device phone;
        core.pair(phone);
        LoopbackTransport* app = core.signIn(phone, voiceFeatures());
        QVERIFY(admitted(app));
        MoxController* mox = core.model->moxController();
        RemoteTxWatchdog* watchdog = core.server->txWatchdog();
        QVERIFY(watchdog != nullptr);
        const QByteArray id = phone.key.fingerprint();

        QCOMPARE(refusalOf(core.invoke(app, "voiceKeyer.send", {int64("n", 1)})), QString());
        QVERIFY(mox->isMox());
        QVERIFY(watchdog->isWatching(id));
        // The phone is told the message is its own.
        QTRY_COMPARE(voiceSending(app).value(QStringLiteral("deviceId")).toString(),
                     StationIdentity::toBase64Url(id));

        QSignalSpy heard(watchdog, &RemoteTxWatchdog::keepaliveHeard);
        QSignalSpy held(watchdog, &RemoteTxWatchdog::holdStarted);
        quint64 sequence = 0;
        for (int i = 0; i < 35; ++i) {
            core.now += 100;
            app->sendText(keepaliveWire(++sequence));
            QTRY_COMPARE(heard.count(), i + 1);
            watchdog->onTimer();
        }
        QVERIFY(mox->isMox());
        QCOMPARE(mox->currentKeyer().deviceId, id);
        QVERIFY(!watchdog->isHeld(id));
        QCOMPARE(held.count(), 0);
        QVERIFY(vk->sending().has_value());
        QCOMPARE(core.model->recordingService()->onAirSource()->active(), OnAirSource::Kind::VoiceMessage);

        // A dead sender is still cut: silence past 3 s.
        core.now += RemoteTxWatchdog::kLinkCutMs + 1;
        watchdog->onTimer();
        watchdog->onTimer();
        QTRY_VERIFY(!mox->isMox());
        QVERIFY(!vk->sending().has_value());
        QCOMPARE(core.model->recordingService()->onAirSource()->active(), OnAirSource::Kind::None);
    }

    // M3: a session that drops in the middle of loading a message leaves
    // no file behind.
    void aDroppedSessionTakesItsImportFile()
    {
        Core core;
        useTestDisk(core);
        VoiceKeyer* vk = core.model->recordingService()->voiceKeyer();
        QVERIFY(vk != nullptr);
        vk->setFolder(m_dir.filePath(QStringLiteral("voice%1").arg(++m_disks)));
        Device phone;
        core.pair(phone);
        LoopbackTransport* app = core.signIn(phone, kRecordings);
        QVERIFY(admitted(app));
        QCOMPARE(refusalOf(core.invoke(app, "voiceKeyer.beginImport",
                                       {int64("n", 2), utf8("name", QString()), int64("size", 1000)})),
                 QString());
        QCOMPARE(refusalOf(core.invoke(app, "voiceKeyer.chunk",
                                       {utf8("data", QString::fromLatin1(QByteArray(100, 'a').toBase64()))})),
                 QString());
        const auto parts = [vk] {
            return QDir(vk->folder()).entryList({QStringLiteral("*.part")}, QDir::Files);
        };
        QCOMPARE(parts().size(), 1);
        app->closeLink(QStringLiteral("lost"));
        QTRY_VERIFY(!app->isOpen());
        QTRY_COMPARE(parts(), QStringList());
    }

    void theCapabilityRoundTripsAfterTheBandStack()
    {
        StationCapabilities caps;
        caps.bandStackVersion = 1;
        caps.recordingsVersion = 1;
        caps.coreBuildInfo = CoreBuildInfo{QStringLiteral("2026.11.0"), QStringLiteral("v2026.11.0")};
        QList<QByteArray> names;
        for (const MirrorUpdate& update : caps.toUpdates()) {
            names.append(update.name);
        }
        const qsizetype at = names.indexOf(QByteArrayLiteral("recordingsVersion"));
        QVERIFY(at > 0);
        QCOMPARE(names.at(at - 1), QByteArrayLiteral("bandStackVersion"));
        QCOMPARE(names.last(), QByteArrayLiteral("coreBuildInfo"));
        const StationCapabilities decoded = StationCapabilities::fromUpdates(caps.toUpdates());
        QCOMPARE(decoded.recordingsVersion, 1);
        QCOMPARE(decoded.bandStackVersion, 1);

        caps.recordingsVersion = 0;
        for (const MirrorUpdate& update : caps.toUpdates()) {
            QVERIFY(update.name != "recordingsVersion");
        }
        const MirrorUpdate wrongKind{0, "recordingsVersion", MirrorWireKind::Utf8, QVariant(QStringLiteral("1"))};
        QCOMPARE(StationCapabilities::fromUpdates({wrongKind}).recordingsVersion, 0);
    }

    void thePeerThatDidNotDeclareItGetsNothing()
    {
        Core core;
        useTestDisk(core);
        Device older(QStringLiteral("Older iPhone"), QStringLiteral("phone"));
        core.pair(older);
        LoopbackTransport* app = core.signIn(older, kHolder);
        QVERIFY(admitted(app));
        QCOMPARE(capability(app->received(), QStringLiteral("recordingsVersion")), std::optional<qint64>());
        QVERIFY(latest(app->received(), QStringLiteral("recordings"), QStringLiteral("json")).isNull());
        for (const QJsonObject& schema : ofType(app->received(), QStringLiteral("schema"))) {
            QVERIFY(schema.value(QStringLiteral("class")).toString() != QLatin1String("RecordingsFacade"));
        }
        QCOMPARE(reasonOf(core.invoke(app, "recordings.start", {int64("slice", 0)})), kUpdateApp);
        QCOMPARE(reasonOf(core.invoke(app, "recordings.stopPlay")), kUpdateApp);
        QCOMPARE(core.model->recordingService()->sliceStates().first().state, RecordState::Idle);
    }

    void thePeerThatDeclaredItGetsTheDocument()
    {
        Core core;
        useTestDisk(core);
        Device phone;
        core.pair(phone);
        LoopbackTransport* app = core.signIn(phone, kRecordings);
        QVERIFY(admitted(app));
        QCOMPARE(capability(app->received(), QStringLiteral("recordingsVersion")), std::optional<qint64>(3));
        const QJsonObject document = mirrored(app);
        QCOMPARE(document.value(QStringLiteral("version")).toInt(), 3);
        QCOMPARE(stateIn(document, 0), QStringLiteral("idle"));
        for (const char* key : {"slices", "kept", "index", "settings", "voiceKeyer"}) {
            QVERIFY2(document.contains(QLatin1String(key)), key);
        }
        const QJsonObject index = document.value(QStringLiteral("index")).toObject();
        QCOMPARE(index.value(QStringLiteral("count")).toInt(-1), 0);
        const QJsonObject settings = document.value(QStringLiteral("settings")).toObject();
        QCOMPARE(settings.value(QStringLiteral("format")).toString(), QStringLiteral("wav"));
        for (const char* key : {"keepMyVoice", "sidesApart", "autoStart", "autoStop", "quietMinutes",
                                "disk", "disks"}) {
            QVERIFY2(settings.contains(QLatin1String(key)), key);
        }
        // Version 2: the on-air switches, each off by default.
        for (const char* key : {"onAirUseTxEq", "onAirUseCompressor", "onAirUseLeveler", "onAirUseCfc",
                                "onAirUsePhaseRotator"}) {
            QVERIFY2(settings.contains(QLatin1String(key)), key);
            QVERIFY2(!settings.value(QLatin1String(key)).toBool(true), key);
        }

        // A malformed request is refused in plain words.
        QCOMPARE(reasonOf(core.invoke(app, "recordings.start", {utf8("slice", QStringLiteral("A"))})),
                 kNotUnderstood);
        QCOMPARE(reasonOf(core.invoke(app, "recordings.start", {int64("slice", 3)})),
                 QStringLiteral("That slice is not open."));

        // A settings change reaches the device.
        QCOMPARE(refusalOf(core.invoke(app, "recordings.setSettings", {int64("quietMinutes", 5)})),
                 QString());
        QTRY_COMPARE(mirrored(app).value(QStringLiteral("settings")).toObject()
                         .value(QStringLiteral("quietMinutes")).toInt(), 5);
        QCOMPARE(refusalOf(core.invoke(app, "recordings.setSettings",
                                       {MirrorUpdate{0, "onAirUseCompressor", MirrorWireKind::Bool, QVariant(true)},
                                        MirrorUpdate{0, "onAirUseCfc", MirrorWireKind::Bool, QVariant(true)}})),
                 QString());
        QTRY_VERIFY(mirrored(app).value(QStringLiteral("settings")).toObject()
                        .value(QStringLiteral("onAirUseCompressor")).toBool());
        QVERIFY(core.model->recordingService()->settings().onAirUseCfc);
        QVERIFY(!core.model->recordingService()->settings().onAirUseTxEq);
        QCOMPARE(reasonOf(core.invoke(app, "recordings.setSettings", {int64("quietMinutes", 0)})),
                 QStringLiteral("Quiet can be 1 to 60 minutes."));
    }

    // A1: one device starts Slice A; the other sees it recording and stops it.
    void anyDeviceStartsAndAnotherStops()
    {
        Core core;
        useTestDisk(core);
        Device phone;
        Device tablet(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(phone);
        core.pair(tablet);
        LoopbackTransport* first = core.signIn(phone, kRecordings);
        LoopbackTransport* second = core.signIn(tablet, kRecordings);
        QVERIFY(admitted(first));
        QVERIFY(admitted(second));
        QCOMPARE(stateIn(mirrored(second), 0), QStringLiteral("idle"));

        QCOMPARE(refusalOf(core.invoke(first, "recordings.start", {int64("slice", 0)})), QString());
        QTRY_COMPARE(stateIn(mirrored(second), 0), QStringLiteral("recording"));
        QVERIFY(!sliceIn(mirrored(second), 0).value(QStringLiteral("startedBy")).toString().isEmpty());

        RecordingService* service = core.model->recordingService();
        m_clockMs += 4000;
        service->pumpForTest();
        QCOMPARE(refusalOf(core.invoke(second, "recordings.stop", {int64("slice", 0)})), QString());
        service->pumpForTest();
        QTRY_COMPARE(mirrored(second).value(QStringLiteral("kept")).toObject()
                         .value(QStringLiteral("seq")).toInt(), 1);
        QTRY_COMPARE(mirrored(first).value(QStringLiteral("kept")).toObject()
                         .value(QStringLiteral("seq")).toInt(), 1);
        for (LoopbackTransport* app : {first, second}) {
            const QJsonObject kept = mirrored(app).value(QStringLiteral("kept")).toObject();
            QCOMPARE(kept.value(QStringLiteral("reason")).toString(), QStringLiteral("device"));
            QVERIFY(!kept.value(QStringLiteral("device")).toString().isEmpty());
            QVERIFY(!kept.value(QStringLiteral("name")).toString().isEmpty());
            QCOMPARE(kept.value(QStringLiteral("slice")).toInt(-1), 0);
            QCOMPARE(stateIn(mirrored(app), 0), QStringLiteral("idle"));
        }
        QCOMPARE(mirrored(first).value(QStringLiteral("kept")).toObject(),
                 mirrored(second).value(QStringLiteral("kept")).toObject());
        QCOMPARE(mirrored(first).value(QStringLiteral("index")).toObject()
                     .value(QStringLiteral("count")).toInt(), 1);
    }

    // A device's link drops while it records: the recording goes on, and the
    // device sees it still running when it is back.
    void aDroppedLinkLeavesTheRecordingRunning()
    {
        Core core;
        useTestDisk(core);
        Device phone;
        core.pair(phone);
        LoopbackTransport* app = core.signIn(phone, kRecordings);
        QVERIFY(admitted(app));
        QCOMPARE(refusalOf(core.invoke(app, "recordings.start", {int64("slice", 0)})), QString());
        QTRY_COMPARE(stateIn(mirrored(app), 0), QStringLiteral("recording"));
        const qint64 before = sliceIn(mirrored(app), 0).value(QStringLiteral("elapsedMs")).toInteger();

        app->closeLink(QStringLiteral("lost"));
        QTRY_VERIFY(!app->isOpen());
        m_clockMs += 3000;
        core.model->recordingService()->pumpForTest();
        QCOMPARE(core.model->recordingService()->sliceStates().first().state, RecordState::Recording);
        // What the facade's once-a-second rebuild does on the real clock.
        core.model->recordingsFacade()->refresh();

        LoopbackTransport* back = core.signIn(phone, kRecordings);
        QVERIFY(admitted(back));
        const QJsonObject slice = sliceIn(mirrored(back), 0);
        QCOMPARE(slice.value(QStringLiteral("state")).toString(), QStringLiteral("recording"));
        QVERIFY(slice.value(QStringLiteral("elapsedMs")).toInteger() >= before + 3000);

        m_clockMs += 2000;
        core.model->recordingsFacade()->refresh();
        QTRY_VERIFY(sliceIn(mirrored(back), 0).value(QStringLiteral("elapsedMs")).toInteger()
                    >= before + 5000);
        core.model->recordingService()->stopAll();
        core.model->recordingService()->pumpForTest();
    }

    void renameAndDeleteReachEveryDevice()
    {
        Core core;
        useTestDisk(core);
        Device phone;
        Device tablet(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(phone);
        core.pair(tablet);
        LoopbackTransport* first = core.signIn(phone, kRecordings);
        LoopbackTransport* second = core.signIn(tablet, kRecordings);
        QVERIFY(admitted(first));
        QVERIFY(admitted(second));
        const QString older = addFile(core, QStringLiteral("older"), QByteArray(1000, 'a'));
        const QString newer = addFile(core, QStringLiteral("newer"), QByteArray(2000, 'b'));
        QVERIFY(!older.isEmpty());
        QVERIFY(!newer.isEmpty());
        QTRY_COMPARE(mirrored(second).value(QStringLiteral("index")).toObject()
                         .value(QStringLiteral("count")).toInt(), 2);
        const qint64 revision = mirrored(second).value(QStringLiteral("index")).toObject()
                                    .value(QStringLiteral("revision")).toInteger();

        QCOMPARE(refusalOf(core.invoke(first, "recordings.rename",
                                       {utf8("id", older), utf8("name", QStringLiteral("Field Day"))})),
                 QString());
        QTRY_COMPARE(mirrored(second).value(QStringLiteral("index")).toObject()
                         .value(QStringLiteral("revision")).toInteger(), revision + 1);
        const QJsonObject listed = core.invoke(second, "recordings.list",
                                               {int64("offset", 0), int64("count", 10)});
        QCOMPARE(refusalOf(listed), QString());
        QCOMPARE(valueIn(listed, QStringLiteral("total")).toInteger(), 2);
        QVERIFY(listedNames(listed).contains(QStringLiteral("Field Day")));
        QVERIFY(!listedNames(listed).contains(QStringLiteral("older")));
        QVERIFY(valueIn(listed, QStringLiteral("reader")).isNull());

        // Only slice 0's contact recordings, for reader 7; slice 2 has none,
        // and the answer names the reader that asked.
        const QJsonObject mine = core.invoke(second, "recordings.list",
                                             {int64("offset", 0), int64("count", 10), int64("slice", 0),
                                              utf8("kind", QStringLiteral("Contact")), int64("reader", 7)});
        QCOMPARE(refusalOf(mine), QString());
        QCOMPARE(valueIn(mine, QStringLiteral("reader")).toInteger(), 7);
        QCOMPARE(valueIn(mine, QStringLiteral("total")).toInteger(), 2);
        const QJsonObject none = core.invoke(second, "recordings.list",
                                             {int64("offset", 0), int64("count", 10), int64("slice", 2),
                                              int64("reader", 8)});
        QCOMPARE(valueIn(none, QStringLiteral("reader")).toInteger(), 8);
        QCOMPARE(valueIn(none, QStringLiteral("total")).toInteger(), 0);
        QCOMPARE(listedNames(none), QStringList());
        const QJsonObject pans = core.invoke(second, "recordings.list",
                                             {int64("offset", 0), int64("count", 10),
                                              utf8("kind", QStringLiteral("WholePan"))});
        QCOMPARE(valueIn(pans, QStringLiteral("total")).toInteger(), 0);
        QCOMPARE(refusalOf(core.invoke(second, "recordings.list",
                                       {int64("offset", 0), int64("count", 10),
                                        utf8("kind", QStringLiteral("Pictures"))})),
                 kNotUnderstood);

        const QString ids = QString::fromUtf8(QJsonDocument(QJsonArray{older}).toJson(QJsonDocument::Compact));
        QCOMPARE(refusalOf(core.invoke(first, "recordings.delete", {utf8("ids", ids)})), QString());
        QTRY_COMPARE(mirrored(second).value(QStringLiteral("index")).toObject()
                         .value(QStringLiteral("revision")).toInteger(), revision + 2);
        QCOMPARE(mirrored(second).value(QStringLiteral("index")).toObject()
                     .value(QStringLiteral("count")).toInt(), 1);
        const QJsonObject after = core.invoke(second, "recordings.list",
                                              {int64("offset", 0), int64("count", 10)});
        QCOMPARE(listedNames(after), QStringList{QStringLiteral("newer")});
        // The other device deleted it first.
        QCOMPARE(reasonOf(core.invoke(second, "recordings.delete", {utf8("ids", ids)})),
                 QString::fromLatin1(RecordingService::kPlayGoneReason));
    }

    // V6: each device hears, and is told about, its own playback only.
    void eachDeviceGetsItsOwnPlayback()
    {
        Core core;
        useTestDisk(core);
        Device phone;
        Device tablet(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(phone);
        core.pair(tablet);
        LoopbackTransport* first = core.signIn(phone, kRecordings);
        LoopbackTransport* second = core.signIn(tablet, kRecordings);
        QVERIFY(admitted(first));
        QVERIFY(admitted(second));
        const QString one = addPlayable(core, QStringLiteral("one"));
        const QString two = addPlayable(core, QStringLiteral("two"));
        QVERIFY(!one.isEmpty());
        QVERIFY(!two.isEmpty());

        QCOMPARE(refusalOf(core.invoke(first, "recordings.play", {utf8("id", one)})), QString());
        QCOMPARE(refusalOf(core.invoke(second, "recordings.play", {utf8("id", two)})), QString());
        QTRY_VERIFY(!playbackPushes(first).isEmpty());
        QTRY_VERIFY(!playbackPushes(second).isEmpty());
        for (const QJsonObject& push : playbackPushes(first)) {
            QCOMPARE(push.value(QStringLiteral("id")).toInt(-1), 0);
            const QString id = valueIn(push, QStringLiteral("id")).toString();
            QVERIFY2(id.isEmpty() || id == one, qPrintable(id));
        }
        for (const QJsonObject& push : playbackPushes(second)) {
            const QString id = valueIn(push, QStringLiteral("id")).toString();
            QVERIFY2(id.isEmpty() || id == two, qPrintable(id));
        }
        const QJsonObject latestFirst = playbackPushes(first).last();
        QCOMPARE(valueIn(latestFirst, QStringLiteral("id")).toString(), one);
        QCOMPARE(valueIn(latestFirst, QStringLiteral("lengthMs")).toInteger(), 1000);
        QCOMPARE(valueIn(latestFirst, QStringLiteral("onAir")).toBool(true), false);
        // Playback alone never keys.
        QVERIFY(!core.model->mox());

        QCOMPARE(refusalOf(core.invoke(first, "recordings.stopPlay")), QString());
        QTRY_VERIFY(valueIn(playbackPushes(first).last(), QStringLiteral("id")).toString().isEmpty());
        QCOMPARE(valueIn(playbackPushes(second).last(), QStringLiteral("id")).toString(), two);
        QCOMPARE(reasonOf(core.invoke(first, "recordings.play", {utf8("id", QStringLiteral("nope"))})),
                 QString::fromLatin1(RecordingService::kPlayGoneReason));
        QCOMPARE(refusalOf(core.invoke(second, "recordings.stopPlay")), QString());
    }

    // A6: the device that keys while its recording plays is told it is on
    // the air; the other device is told nothing.
    void theKeyingDeviceIsToldItsPlaybackIsOnTheAir()
    {
        Core core;
        useTestDisk(core);
        Device phone;
        Device tablet(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(phone);
        core.pair(tablet);
        LoopbackTransport* first = core.signIn(phone, kRecordings);
        LoopbackTransport* second = core.signIn(tablet, kRecordings);
        QVERIFY(admitted(first));
        QVERIFY(admitted(second));
        const QString one = addPlayable(core, QStringLiteral("one"));
        QVERIFY(!one.isEmpty());
        QCOMPARE(refusalOf(core.invoke(first, "recordings.play", {utf8("id", one)})), QString());
        QTRY_VERIFY(!playbackPushes(first).isEmpty());
        QCOMPARE(valueIn(playbackPushes(first).last(), QStringLiteral("onAir")).toBool(true), false);
        const qsizetype toldSecond = playbackPushes(second).size();

        // The phone's key, as its tx.key reaches the service.
        RecordingService* service = core.model->recordingService();
        service->keyChanged(true, RecordingService::listenerForKey(keyerFor(phone), false));
        QCOMPARE(service->onAirSource()->active(), OnAirSource::Kind::Playback);
        QTRY_VERIFY(valueIn(playbackPushes(first).last(), QStringLiteral("onAir")).toBool(false));
        QCOMPARE(valueIn(playbackPushes(first).last(), QStringLiteral("id")).toString(), one);

        service->keyChanged(false, std::nullopt);
        QTRY_VERIFY(!valueIn(playbackPushes(first).last(), QStringLiteral("onAir")).toBool(true));
        QCOMPARE(playbackPushes(second).size(), toldSecond);
        QCOMPARE(refusalOf(core.invoke(first, "recordings.stopPlay")), QString());
    }

    // A copy of 3 MB in chunks comes back byte for byte.
    void anExportComesBackWhole()
    {
        Core core;
        useTestDisk(core);
        Device phone;
        core.pair(phone);
        LoopbackTransport* app = core.signIn(phone, kRecordings);
        QVERIFY(admitted(app));
        QByteArray bytes(3 * 1024 * 1024 + 123, Qt::Uninitialized);
        QRandomGenerator generator(42);
        for (char& b : bytes) {
            b = static_cast<char>(generator.bounded(256));
        }
        const QString id = addFile(core, QStringLiteral("big"), bytes);
        QVERIFY(!id.isEmpty());

        QByteArray copy;
        int chunks = 0;
        while (copy.size() < bytes.size()) {
            const QJsonObject result = core.invoke(app, "recordings.export",
                                                   {utf8("id", id), int64("offset", copy.size())});
            QCOMPARE(refusalOf(result), QString());
            QCOMPARE(valueIn(result, QStringLiteral("offset")).toInteger(), copy.size());
            QCOMPARE(valueIn(result, QStringLiteral("size")).toInteger(), bytes.size());
            const QByteArray chunk =
                QByteArray::fromBase64(valueIn(result, QStringLiteral("data")).toString().toLatin1());
            QVERIFY(!chunk.isEmpty());
            QVERIFY(chunk.size() <= RecordingsFacade::kExportChunkBytes);
            copy.append(chunk);
            ++chunks;
            QVERIFY(chunks < 100);
        }
        QVERIFY(chunks > 1);
        QCOMPARE(copy, bytes);
        QCOMPARE(reasonOf(core.invoke(app, "recordings.export",
                                      {utf8("id", id), int64("offset", bytes.size() + 1)})),
                 kNotUnderstood);
        QCOMPARE(reasonOf(core.invoke(app, "recordings.export",
                                      {utf8("id", QStringLiteral("nope")), int64("offset", 0)})),
                 QString::fromLatin1(RecordingService::kPlayGoneReason));
    }
};

QTEST_MAIN(TstRecordingsLink)
#include "tst_recordings_link.moc"
