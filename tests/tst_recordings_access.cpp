// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_recordings_access.cpp  (NereusSDR)
// =================================================================
//
// RecordingsAccess (recording design sections 6.9 and V9): a window that runs
// its own radio calls its RecordingService straight; a remote window sends
// recordings.* and follows the mirrored document; against a Core that does
// not offer recordingsVersion every call is refused here with the V9 words
// and nothing goes out.
//
// Loopback link, no RF and no hardware. The recordings go to a temporary
// folder as WAV, on an injected clock; nothing keys.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-15 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code. Recording plan
//                 Task 5.
//   2026-11-15 - J.J. Boyd (KG4VCF), recording plan Task 7: two list
//                 readers, local and remote, each get their own page, with
//                 the slice and kind filters. AI-assisted implementation via
//                 Anthropic Claude Code.
//   2026-11-15 - J.J. Boyd (KG4VCF), recording plan Task 10: the Core offers
//                 version 2. AI-assisted implementation via Anthropic Claude
//                 Code.
// =================================================================

#include "MultiDeviceHarness.h"

#include <algorithm>

#include <QLoggingCategory>
#include <QRandomGenerator>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimeZone>

#include "core/recordings/AudioFileWriter.h"
#include "core/recordings/RecordingService.h"
#include "core/recordings/RecordingStore.h"
#include "core/session/RecordingsFacade.h"
#include "core/session/StationClient.h"
#include "core/settings/SettingsProxy.h"
#include "models/recordings/LocalRecordingsAccess.h"
#include "models/recordings/RecordingsAccess.h"
#include "models/recordings/RemoteRecordingsAccess.h"

namespace {

const QString kNewerCore = QStringLiteral("This Core needs an update for recordings.");
const QString kNotConnected = QStringLiteral("Connect to a radio to use recordings.");

QString stateOfSlice(const QJsonObject& document, int slice)
{
    for (const QJsonValue& value : document.value(QStringLiteral("slices")).toArray()) {
        if (value.toObject().value(QStringLiteral("slice")).toInt(-1) == slice) {
            return value.toObject().value(QStringLiteral("state")).toString();
        }
    }
    return {};
}

int countIn(const QJsonObject& document)
{
    return document.value(QStringLiteral("index")).toObject().value(QStringLiteral("count")).toInt(-1);
}

QByteArray randomBytes(qsizetype size)
{
    QByteArray bytes(size, Qt::Uninitialized);
    QRandomGenerator generator(7);
    for (char& b : bytes) {
        b = static_cast<char>(generator.bounded(256));
    }
    return bytes;
}

QByteArray readFile(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

// A remote window on the loopback, up to the end of its handshake.
struct RemoteWindow {
    RadioModel window{RadioModel::Role::Remote};
    SettingsProxy proxy;
    StationClient client{&window, &proxy};
    LoopbackTransport* coreEnd = nullptr;

    RecordingsAccess* access() const { return window.recordings(); }

    bool connectTo(Core& core, QObject* parent)
    {
        coreEnd = new LoopbackTransport(QStringLiteral("station-end"), parent);
        auto* windowEnd = new LoopbackTransport(QStringLiteral("client-end"), parent);
        coreEnd->linkTo(windowEnd);
        QSignalSpy completed(&client, &StationClient::handshakeComplete);
        client.startSession(windowEnd, core.server->token());
        core.server->acceptTransport(coreEnd);
        return completed.wait(5000) || completed.count() == 1;
    }

    // What the window sent the Core: the recordings.* invokes.
    int recordingsInvokesSent() const
    {
        int n = 0;
        for (const QJsonObject& o : ofType(coreEnd->received(), QStringLiteral("command.invoke"))) {
            if (o.value(QStringLiteral("verb")).toString().startsWith(QLatin1String("recordings."))) {
                ++n;
            }
        }
        return n;
    }
};

}  // namespace

class TstRecordingsAccess : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    int m_disks = 0;
    qint64 m_clockMs = 0;

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

    // A recording of `kind` on `slice`, `minutesAgo` before 14:00 UTC.
    static QString addKind(Core& core, const QString& name, RecordingKind kind, int slice, int minutesAgo)
    {
        RecordingStore* store = core.model->recordingService()->store();
        if (store == nullptr || store->folder().isEmpty()) {
            return {};
        }
        const QString fileName = name + QStringLiteral(".wav");
        QFile file(QDir(store->folder()).filePath(fileName));
        if (!file.open(QIODevice::WriteOnly) || file.write(QByteArray(100, 'r')) != 100) {
            return {};
        }
        file.close();
        RecordingEntry e;
        e.kind = kind;
        e.sliceId = slice;
        e.startUtc = QDateTime(QDate(2026, 11, 15), QTime(14, 0, 0), QTimeZone::UTC).addSecs(-60 * minutesAgo);
        e.lengthMs = 1000;
        e.frequencyHz = 7236400;
        e.mode = QStringLiteral("LSB");
        e.name = name;
        e.fileName = fileName;
        return store->add(e);
    }

    static QStringList namesOf(const RecordingsListPage& page)
    {
        QStringList out;
        for (const RecordingEntry& e : page.entries) {
            out.append(e.name);
        }
        return out;
    }

    // Two readers ask at once, one for slice 1's contacts and one for the
    // newest two of everything: each gets its own page, whatever order the
    // answers come in.
    void twoReadersGetTheirOwnPages(Core& core, RecordingsAccess* access)
    {
        QVERIFY(!addKind(core, QStringLiteral("a-old"), RecordingKind::Contact, 0, 50).isEmpty());
        QVERIFY(!addKind(core, QStringLiteral("b-old"), RecordingKind::Contact, 1, 40).isEmpty());
        QVERIFY(!addKind(core, QStringLiteral("pan"), RecordingKind::WholePan, -1, 30).isEmpty());
        QVERIFY(!addKind(core, QStringLiteral("b-sound"), RecordingKind::Sound, 1, 20).isEmpty());
        QVERIFY(!addKind(core, QStringLiteral("b-new"), RecordingKind::Contact, 1, 10).isEmpty());
        QTRY_COMPARE(countIn(access->document()), 5);

        const int flag = access->newListReader();
        const int panel = access->newListReader();
        QVERIFY(flag != 0);
        QVERIFY(panel != flag);
        QSignalSpy listed(access, &RecordingsAccess::listChanged);
        access->requestList(flag, RecordingsListQuery{0, 8, 1, RecordingKind::Contact});
        access->requestList(panel, RecordingsListQuery{0, 2, {}, {}});
        QTRY_COMPARE(listed.count(), 2);
        QList<int> answered{listed.at(0).at(0).toInt(), listed.at(1).at(0).toInt()};
        std::sort(answered.begin(), answered.end());
        QCOMPARE(answered, (QList<int>{flag, panel}));

        QCOMPARE(namesOf(access->listed(flag)), (QStringList{QStringLiteral("b-new"), QStringLiteral("b-old")}));
        QCOMPARE(access->listed(flag).total, 2);
        QCOMPARE(namesOf(access->listed(panel)),
                 (QStringList{QStringLiteral("b-new"), QStringLiteral("b-sound")}));
        QCOMPARE(access->listed(panel).total, 5);

        // A later page for one reader leaves the other's alone.
        access->requestList(panel, RecordingsListQuery{2, 2, {}, {}});
        QTRY_COMPARE(listed.count(), 3);
        QCOMPARE(listed.last().at(0).toInt(), panel);
        QCOMPARE(access->listed(panel).offset, 2);
        QCOMPARE(namesOf(access->listed(panel)), (QStringList{QStringLiteral("pan"), QStringLiteral("b-old")}));
        QCOMPARE(namesOf(access->listed(flag)), (QStringList{QStringLiteral("b-new"), QStringLiteral("b-old")}));

        const int sounds = access->newListReader();
        access->requestList(sounds, RecordingsListQuery{0, 10, {}, RecordingKind::WholePan});
        QTRY_COMPARE(listed.count(), 4);
        QCOMPARE(namesOf(access->listed(sounds)), QStringList{QStringLiteral("pan")});
        QVERIFY(access->listed(access->newListReader()).entries.isEmpty());
    }

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
            QStringLiteral("recordings-access-%1").arg(QCoreApplication::applicationPid()));
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

    void theWordsAreTheDesigns()
    {
        QCOMPARE(RecordingsAccess::needsNewerCoreReason(), kNewerCore);
        QCOMPARE(RecordingsAccess::notConnectedReason(), kNotConnected);
    }

    // ── A window that runs its own radio ─────────────────────────────────

    void aLocalWindowSaysConnectWithoutARadio()
    {
        Core core;
        useTestDisk(core);
        RecordingsAccess* access = core.model->recordings();
        QVERIFY(qobject_cast<LocalRecordingsAccess*>(access) != nullptr);
        QCOMPARE(access->state(), RecordingsAccess::State::Ready);
        QCOMPARE(access->coreVersion(), 2);
        QVERIFY(access->coreIsOnThisComputer());

        QSignalSpy state(access, &RecordingsAccess::stateChanged);
        core.model->setConnectionStateForTest(ConnectionState::Disconnected);
        QCOMPARE(access->state(), RecordingsAccess::State::NotConnected);
        QCOMPARE(access->unavailableReason(), kNotConnected);
        QSignalSpy refused(access, &RecordingsAccess::refused);
        access->start(0);
        QCOMPARE(refused.count(), 1);
        QCOMPARE(refused.first().at(0).toString(), kNotConnected);
        QCOMPARE(core.model->recordingService()->sliceStates().first().state, RecordState::Idle);
        core.model->setConnectionStateForTest(ConnectionState::Connected);
        QCOMPARE(access->state(), RecordingsAccess::State::Ready);
        QVERIFY(state.count() >= 1);
    }

    void aLocalCallGoesStraightToTheService()
    {
        Core core;
        useTestDisk(core);
        RecordingService* service = core.model->recordingService();
        RecordingsAccess* access = core.model->recordings();
        QCOMPARE(access->unavailableReason(), QString());
        // Version 2 (Task 10): the on-air switches work on this Core.
        QCOMPARE(access->unavailableReason(2), QString());
        QCOMPARE(access->unavailableReason(3), kNewerCore);
        QSignalSpy refused(access, &RecordingsAccess::refused);
        QSignalSpy kept(access, &RecordingsAccess::kept);
        QCOMPARE(stateOfSlice(access->document(), 0), QStringLiteral("idle"));

        access->start(0);
        QCOMPARE(service->sliceStates().first().state, RecordState::Recording);
        QCOMPARE(stateOfSlice(access->document(), 0), QStringLiteral("recording"));
        m_clockMs += 3000;
        service->pumpForTest();
        access->stop(0);
        service->pumpForTest();
        QTRY_COMPARE(kept.count(), 1);
        QCOMPARE(kept.first().at(0).toJsonObject().value(QStringLiteral("reason")).toString(),
                 QStringLiteral("here"));
        QCOMPARE(countIn(access->document()), 1);
        QCOMPARE(refused.count(), 0);

        access->start(3);
        QCOMPARE(refused.count(), 1);
        QCOMPARE(refused.last().at(0).toString(), QStringLiteral("That slice is not open."));

        QSignalSpy listed(access, &RecordingsAccess::listChanged);
        const int reader = access->newListReader();
        access->requestList(reader, RecordingsListQuery{0, 10, {}, {}});
        QCOMPARE(listed.count(), 1);
        QCOMPARE(listed.first().at(0).toInt(), reader);
        QCOMPARE(access->listed(reader).entries.size(), 1);
        const QString id = access->listed(reader).entries.first().id;

        access->rename(id, QStringLiteral("Field Day"));
        QCOMPARE(service->store()->find(id)->name, QStringLiteral("Field Day"));

        access->setSettings(QJsonObject{{QStringLiteral("quietMinutes"), 7}});
        QCOMPARE(service->settings().quietMinutes, 7);
        QCOMPARE(access->document().value(QStringLiteral("settings")).toObject()
                     .value(QStringLiteral("quietMinutes")).toInt(), 7);
        access->setSettings(QJsonObject{{QStringLiteral("format"), QStringLiteral("flac")}});
        QCOMPARE(refused.count(), 2);

        const QString playable = addPlayable(core, QStringLiteral("tone"));
        QVERIFY(!playable.isEmpty());
        QSignalSpy playback(access, &RecordingsAccess::playbackChanged);
        access->play(playable);
        QVERIFY(playback.count() >= 1);
        QVERIFY(access->playback().has_value());
        QCOMPARE(access->playback()->recordingId, playable);
        QVERIFY(!core.model->mox());
        access->stopPlay();
        QVERIFY(!access->playback().has_value());

        const QByteArray bytes = randomBytes(5 * 1024 * 1024 + 17);
        const QString big = addFile(core, QStringLiteral("big"), bytes);
        QVERIFY(!big.isEmpty());
        const QString to = m_dir.filePath(QStringLiteral("local-copy.wav"));
        QSignalSpy finished(access, &RecordingsAccess::downloadFinished);
        QSignalSpy progress(access, &RecordingsAccess::downloadProgress);
        access->download(big, to);
        QTRY_COMPARE(finished.count(), 1);
        QCOMPARE(finished.first().at(1).toBool(), true);
        QVERIFY(progress.count() >= 2);
        QCOMPARE(readFile(to), bytes);

        access->remove({id, big});
        QVERIFY(!service->store()->find(id).has_value());
        QVERIFY(!service->store()->find(big).has_value());
        access->remove({id});
        QCOMPARE(refused.last().at(0).toString(), QString::fromLatin1(RecordingService::kPlayGoneReason));
    }

    void aLocalWindowsReadersEachGetTheirOwnPage()
    {
        Core core;
        useTestDisk(core);
        twoReadersGetTheirOwnPages(core, core.model->recordings());
    }

    // ── A remote window ──────────────────────────────────────────────────

    void aRemoteWindowsReadersEachGetTheirOwnPage()
    {
        Core core(/*upgradedWithToken=*/true);
        useTestDisk(core);
        RemoteWindow remote;
        RecordingsAccess* access = remote.access();
        QVERIFY(remote.connectTo(core, this));
        QTRY_COMPARE(access->state(), RecordingsAccess::State::Ready);
        twoReadersGetTheirOwnPages(core, access);
        remote.client.disconnectFromStation(QStringLiteral("test over"));
    }

    void aRemoteWindowOnAnOlderCoreIsToldAndSendsNothing()
    {
        Core core(/*upgradedWithToken=*/true);
        useTestDisk(core);
        RemoteWindow remote;
        RecordingsAccess* access = remote.access();
        QVERIFY(qobject_cast<RemoteRecordingsAccess*>(access) != nullptr);
        QCOMPARE(access->state(), RecordingsAccess::State::NotConnected);
        QCOMPARE(access->unavailableReason(), kNotConnected);
        // What an older Core amounts to here: no recordingsVersion comes back.
        remote.client.withholdFeatureForTest(QByteArrayLiteral("recordings"));
        QVERIFY(remote.connectTo(core, this));
        QTRY_COMPARE(access->state(), RecordingsAccess::State::NeedsNewerCore);
        QCOMPARE(access->coreVersion(), 0);
        QCOMPARE(access->unavailableReason(), kNewerCore);
        QVERIFY(access->document().isEmpty());

        QSignalSpy refused(access, &RecordingsAccess::refused);
        QSignalSpy finished(access, &RecordingsAccess::downloadFinished);
        access->start(0);
        access->stop(0);
        access->requestList(access->newListReader(), RecordingsListQuery{0, 10, {}, {}});
        access->play(QStringLiteral("x"));
        access->stopPlay();
        access->seek(100);
        access->rename(QStringLiteral("x"), QStringLiteral("y"));
        access->remove({QStringLiteral("x")});
        access->setSettings(QJsonObject{{QStringLiteral("quietMinutes"), 3}});
        access->download(QStringLiteral("x"), m_dir.filePath(QStringLiteral("never.wav")));
        QCOMPARE(refused.count(), 9);
        for (const QList<QVariant>& call : refused) {
            QCOMPARE(call.at(0).toString(), kNewerCore);
        }
        QCOMPARE(finished.count(), 1);
        QCOMPARE(finished.first().at(2).toString(), kNewerCore);
        QTest::qWait(50);
        QCOMPARE(remote.recordingsInvokesSent(), 0);
        QCOMPARE(core.model->recordingService()->sliceStates().first().state, RecordState::Idle);
        QVERIFY(!QFile::exists(m_dir.filePath(QStringLiteral("never.wav"))));
        remote.client.disconnectFromStation(QStringLiteral("test over"));
    }

    void aRemoteWindowSendsTheVerbsAndFollowsTheDocument()
    {
        Core core(/*upgradedWithToken=*/true);
        useTestDisk(core);
        RemoteWindow remote;
        RecordingsAccess* access = remote.access();
        QVERIFY(remote.connectTo(core, this));
        QTRY_COMPARE(access->state(), RecordingsAccess::State::Ready);
        QCOMPARE(access->coreVersion(), 2);
        QCOMPARE(access->unavailableReason(), QString());
        // Version 2 (Task 10): the on-air switches work on this Core.
        QCOMPARE(access->unavailableReason(2), QString());
        QCOMPARE(access->unavailableReason(3), kNewerCore);
        QCOMPARE(stateOfSlice(access->document(), 0), QStringLiteral("idle"));
        QSignalSpy refused(access, &RecordingsAccess::refused);
        QSignalSpy kept(access, &RecordingsAccess::kept);
        RecordingService* service = core.model->recordingService();

        access->start(0);
        QTRY_COMPARE(stateOfSlice(access->document(), 0), QStringLiteral("recording"));
        QCOMPARE(service->sliceStates().first().state, RecordState::Recording);
        m_clockMs += 3000;
        service->pumpForTest();
        access->stop(0);
        QTRY_COMPARE(service->sliceStates().first().state, RecordState::Idle);
        service->pumpForTest();
        QTRY_COMPARE(kept.count(), 1);
        QCOMPARE(kept.first().at(0).toJsonObject().value(QStringLiteral("reason")).toString(),
                 QStringLiteral("device"));
        QTRY_COMPARE(countIn(access->document()), 1);

        QSignalSpy listed(access, &RecordingsAccess::listChanged);
        const int reader = access->newListReader();
        access->requestList(reader, RecordingsListQuery{0, 10, {}, {}});
        QTRY_COMPARE(listed.count(), 1);
        QCOMPARE(listed.first().at(0).toInt(), reader);
        QCOMPARE(access->listed(reader).entries.size(), 1);
        const QString id = access->listed(reader).entries.first().id;
        QCOMPARE(id, service->store()->entries().first().id);

        access->rename(id, QStringLiteral("Field Day"));
        QTRY_COMPARE(service->store()->find(id)->name, QStringLiteral("Field Day"));

        access->setSettings(QJsonObject{{QStringLiteral("quietMinutes"), 9}});
        QTRY_COMPARE(service->settings().quietMinutes, 9);

        // The Core's refusal comes back once.
        access->start(3);
        QTRY_COMPARE(refused.count(), 1);
        QCOMPARE(refused.first().at(0).toString(), QStringLiteral("That slice is not open."));

        const QString playable = addPlayable(core, QStringLiteral("tone"));
        QVERIFY(!playable.isEmpty());
        access->play(playable);
        QTRY_VERIFY(access->playback().has_value());
        QCOMPARE(access->playback()->recordingId, playable);
        QCOMPARE(access->playback()->lengthMs, qint64(1000));
        access->stopPlay();
        QTRY_VERIFY(!access->playback().has_value());

        // A 3 MB copy in chunks, byte for byte.
        const QByteArray bytes = randomBytes(3 * 1024 * 1024 + 5);
        const QString big = addFile(core, QStringLiteral("big"), bytes);
        QVERIFY(!big.isEmpty());
        const QString to = m_dir.filePath(QStringLiteral("remote-copy.wav"));
        QSignalSpy finished(access, &RecordingsAccess::downloadFinished);
        QSignalSpy progress(access, &RecordingsAccess::downloadProgress);
        access->download(big, to);
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 30000);
        QCOMPARE(finished.first().at(2).toString(), QString());
        QCOMPARE(finished.first().at(1).toBool(), true);
        QVERIFY(progress.count() > 1);
        QCOMPARE(readFile(to), bytes);

        access->remove({id, big});
        QTRY_COMPARE(countIn(access->document()), 1);
        QVERIFY(!service->store()->find(id).has_value());

        remote.client.disconnectFromStation(QStringLiteral("test over"));
        QTRY_COMPARE(access->state(), RecordingsAccess::State::NotConnected);
        QCOMPARE(access->unavailableReason(), kNotConnected);
    }
};

QTEST_MAIN(TstRecordingsAccess)
#include "tst_recordings_access.moc"
