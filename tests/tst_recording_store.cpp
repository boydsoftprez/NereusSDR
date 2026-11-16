// =================================================================
// tests/tst_recording_store.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test. The Core's recording store
// (spec 6.4): names, collisions, rename, delete, the recordings.json
// index across a fresh open, the disk list's SD card rule (spec 6.7) and
// the recording settings' keys and defaults (spec 6.5, 6.7).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-15 - Created. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimeZone>

#include "core/AppSettings.h"
#include "core/recordings/RecordingDisks.h"
#include "core/recordings/RecordingNames.h"
#include "core/recordings/RecordingSettings.h"
#include "core/recordings/RecordingStore.h"

using namespace NereusSDR;

namespace {

const QDateTime kStart(QDate(2026, 11, 15), QTime(14, 32, 10), QTimeZone::UTC);

bool touch(const QString& path, const QByteArray& bytes = QByteArray(100, 'x'))
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        return false;
    }
    return f.write(bytes) == bytes.size();
}

// A recording file made the way a recorder makes one: a free path from the
// store, the file on disk, then the row.
QString addRecording(RecordingStore& store, const QDateTime& start, const QString& call = QString())
{
    const QString base = recordingBaseName(start, 7236400, QStringLiteral("LSB"), call);
    const QString path = store.pathForNew(base, QStringLiteral("mp3"));
    if (!touch(path)) {
        return QString();
    }
    RecordingEntry e;
    e.kind = RecordingKind::Contact;
    e.sliceId = 0;
    e.startUtc = start;
    e.lengthMs = 61000;
    e.frequencyHz = 7236400;
    e.mode = QStringLiteral("LSB");
    e.call = call;
    e.name = QFileInfo(path).completeBaseName();
    e.fileName = QFileInfo(path).fileName();
    return store.add(e);
}

}  // namespace

class TestRecordingStore : public QObject {
    Q_OBJECT

private slots:
    void init() { AppSettings::instance().clear(); }

    void baseNames()
    {
        QCOMPARE(recordingBaseName(kStart, 7236400, QStringLiteral("LSB"), QStringLiteral("K4ABC")),
                 QStringLiteral("2026-11-15 1432Z 7.236.400 LSB K4ABC"));
        QCOMPARE(recordingBaseName(kStart, 7236400, QStringLiteral("LSB"), QString()),
                 QStringLiteral("2026-11-15 1432Z 7.236.400 LSB"));
        QCOMPARE(recordingFrequencyText(14074000), QStringLiteral("14.074.000"));
        QCOMPARE(recordingFrequencyText(146520000), QStringLiteral("146.520.000"));
        // A local time names the same UTC minute.
        QCOMPARE(recordingBaseName(kStart.toOffsetFromUtc(-5 * 3600), 14074000, QStringLiteral("USB"),
                                   QString()),
                 QStringLiteral("2026-11-15 1432Z 14.074.000 USB"));
    }

    void sameMinuteGetsANumber()
    {
        QTemporaryDir dir;
        RecordingStore store;
        QVERIFY(store.openFolder(dir.path(), nullptr));
        const QString first = addRecording(store, kStart);
        const QString second = addRecording(store, kStart.addSecs(20));
        QVERIFY(!first.isEmpty());
        QVERIFY(!second.isEmpty());
        QCOMPARE(store.find(first)->fileName, QStringLiteral("2026-11-15 1432Z 7.236.400 LSB.mp3"));
        QCOMPARE(store.find(second)->fileName, QStringLiteral("2026-11-15 1432Z 7.236.400 LSB (2).mp3"));
        QCOMPARE(store.find(second)->name, QStringLiteral("2026-11-15 1432Z 7.236.400 LSB (2)"));
        QVERIFY(!store.find(first)->id.contains(QLatin1Char('{')));
        // Newest first.
        QCOMPARE(store.entries().first().id, second);
        QCOMPARE(store.usedBytes(), qint64(200));
        QVERIFY(store.freeBytes() > 0);
    }

    void renameChangesNameAndFile()
    {
        QTemporaryDir dir;
        RecordingStore store;
        QVERIFY(store.openFolder(dir.path(), nullptr));
        const QString id = addRecording(store, kStart);
        const QString other = addRecording(store, kStart.addSecs(60), QStringLiteral("K4ABC"));
        const QString oldFile = store.find(id)->fileName;
        QSignalSpy changed(&store, &RecordingStore::changed);

        QString error;
        QVERIFY(store.rename(id, QStringLiteral("Field day opener"), &error));
        QCOMPARE(store.find(id)->name, QStringLiteral("Field day opener"));
        QCOMPARE(store.find(id)->fileName, QStringLiteral("Field day opener.mp3"));
        QVERIFY(QFileInfo::exists(dir.filePath(QStringLiteral("Field day opener.mp3"))));
        QVERIFY(!QFileInfo::exists(dir.filePath(oldFile)));
        QCOMPARE(changed.count(), 1);

        for (const QString& bad : {QStringLiteral("a/b"), QStringLiteral("a\\b"), QStringLiteral("a:b"),
                                   QStringLiteral(".hidden"), QStringLiteral("  ")}) {
            error.clear();
            QVERIFY2(!store.rename(id, bad, &error), qPrintable(bad));
            QVERIFY2(!error.isEmpty(), qPrintable(bad));
        }
        error.clear();
        QVERIFY(!store.rename(id, store.find(other)->name, &error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(store.find(id)->name, QStringLiteral("Field day opener"));
        QCOMPARE(changed.count(), 1);
    }

    void removeDeletesFilesAndRows()
    {
        QTemporaryDir dir;
        RecordingStore store;
        QVERIFY(store.openFolder(dir.path(), nullptr));
        const QStringList ids{addRecording(store, kStart), addRecording(store, kStart.addSecs(60)),
                              addRecording(store, kStart.addSecs(120))};
        const QString keep = addRecording(store, kStart.addSecs(180));
        QStringList files;
        for (const QString& id : ids) {
            files << dir.filePath(store.find(id)->fileName);
        }
        // A file that went missing still loses its row.
        QVERIFY(QFile::remove(files.at(1)));

        QString error;
        QVERIFY(store.remove(ids, &error));
        for (const QString& f : files) {
            QVERIFY(!QFileInfo::exists(f));
        }
        QCOMPARE(store.entries().size(), 1);
        QCOMPARE(store.entries().first().id, keep);
        QVERIFY(QFileInfo::exists(dir.filePath(store.find(keep)->fileName)));
    }

    void indexSurvivesAFreshOpen()
    {
        QTemporaryDir dir;
        QStringList ids;
        {
            RecordingStore store;
            QVERIFY(store.openFolder(dir.path(), nullptr));
            ids << addRecording(store, kStart, QStringLiteral("K4ABC")) << addRecording(store, kStart.addSecs(60))
                << addRecording(store, kStart.addSecs(120));
        }
        // A file with no row is not added and not deleted.
        const QString stray = dir.filePath(QStringLiteral("stray.mp3"));
        QVERIFY(touch(stray));
        // A row whose file vanished is dropped, with a warning.
        RecordingStore before;
        QVERIFY(before.openFolder(dir.path(), nullptr));
        const QString gone = before.find(ids.at(1))->fileName;
        QVERIFY(QFile::remove(dir.filePath(gone)));

        RecordingStore store;
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("dropped, its file is gone")));
        QVERIFY(store.openFolder(dir.path(), nullptr));
        QCOMPARE(store.entries().size(), 2);
        const auto first = store.find(ids.at(0));
        QVERIFY(first.has_value());
        QCOMPARE(first->kind, RecordingKind::Contact);
        QCOMPARE(first->sliceId, 0);
        QCOMPARE(first->startUtc, kStart);
        QCOMPARE(first->lengthMs, qint64(61000));
        QCOMPARE(first->frequencyHz, qint64(7236400));
        QCOMPARE(first->mode, QStringLiteral("LSB"));
        QCOMPARE(first->call, QStringLiteral("K4ABC"));
        QCOMPARE(first->name, QStringLiteral("2026-11-15 1432Z 7.236.400 LSB K4ABC"));
        QCOMPARE(first->sizeBytes, qint64(100));
        QVERIFY(!store.find(ids.at(1)).has_value());
        QVERIFY(store.find(ids.at(2)).has_value());
        QVERIFY(QFileInfo::exists(stray));
        for (const RecordingEntry& e : store.entries()) {
            QVERIFY(e.fileName != QStringLiteral("stray.mp3"));
        }
        // The new name of a fresh recording skips the stray file's name too.
        QCOMPARE(QFileInfo(store.pathForNew(QStringLiteral("stray"), QStringLiteral(".mp3"))).fileName(),
                 QStringLiteral("stray (2).mp3"));
    }

    void storeNeverDeletesByItself()
    {
        QTemporaryDir dir;
        const QString stray = dir.filePath(QStringLiteral("unlisted.wav"));
        QVERIFY(touch(stray));
        {
            RecordingStore store;
            QVERIFY(store.openFolder(dir.path(), nullptr));
            addRecording(store, kStart);
            QString error;
            QVERIFY(store.remove({QStringLiteral("no-such-id")}, &error));
        }
        const QStringList files = QDir(dir.path()).entryList(QDir::Files);
        QVERIFY(files.contains(QStringLiteral("unlisted.wav")));
        QVERIFY(files.contains(QStringLiteral("2026-11-15 1432Z 7.236.400 LSB.mp3")));
        QVERIFY(files.contains(QString::fromLatin1(RecordingStore::kIndexFileName)));
    }

    void settingsDefaults()
    {
        const RecordingSettings s = RecordingSettings::load();
        QCOMPARE(s.keepMyVoice, true);
        QCOMPARE(s.sidesApart, false);
        QCOMPARE(s.autoStartOnTransmit, false);
        QCOMPARE(s.autoStopWhenQuiet, true);
        QCOMPARE(s.quietMinutes, 2);
        QCOMPARE(s.format, AudioFileFormat::Mp3);
        QVERIFY(s.disk.isEmpty());
        QCOMPARE(s.onAirUseTxEq, false);
        QCOMPARE(s.onAirUseCompressor, false);
        QCOMPARE(s.onAirUseLeveler, false);
        QCOMPARE(s.onAirUseCfc, false);
        QCOMPARE(s.onAirUsePhaseRotator, false);
        QCOMPARE(s.dsrEnabled, true);
        QVERIFY(s.dsrDisk.isEmpty());
        QCOMPARE(s.dsrSizeLimitGb, 10);
        QVERIFY(s.recordingsFolder().endsWith(QStringLiteral("/Recordings")));
        QVERIFY(s.dsrFolder().endsWith(QStringLiteral("/Spectrum Recorder")));
    }

    void settingsKeys()
    {
        RecordingSettings s;
        s.keepMyVoice = false;
        s.sidesApart = true;
        s.autoStartOnTransmit = true;
        s.autoStopWhenQuiet = false;
        s.quietMinutes = 7;
        s.format = AudioFileFormat::Wav;
        s.disk = QStringLiteral("/media/usb");
        s.onAirUseTxEq = true;
        s.onAirUseCompressor = true;
        s.onAirUseLeveler = false;
        s.onAirUseCfc = true;
        s.onAirUsePhaseRotator = true;
        s.dsrEnabled = false;
        s.dsrDisk = QStringLiteral("/media/ssd");
        s.dsrSizeLimitGb = 40;
        s.save();

        const AppSettings& a = AppSettings::instance();
        QCOMPARE(a.value(QStringLiteral("RecordingsKeepMyVoice")).toString(), QStringLiteral("False"));
        QCOMPARE(a.value(QStringLiteral("RecordingsSidesApart")).toString(), QStringLiteral("True"));
        QCOMPARE(a.value(QStringLiteral("RecordingsAutoStartOnTransmit")).toString(), QStringLiteral("True"));
        QCOMPARE(a.value(QStringLiteral("RecordingsAutoStopWhenQuiet")).toString(), QStringLiteral("False"));
        QCOMPARE(a.value(QStringLiteral("RecordingsQuietMinutes")).toInt(), 7);
        QCOMPARE(a.value(QStringLiteral("RecordingsFormat")).toString(), QStringLiteral("Wav"));
        QCOMPARE(a.value(QStringLiteral("RecordingsDisk")).toString(), QStringLiteral("/media/usb"));
        QCOMPARE(a.value(QStringLiteral("RecordingsOnAirUseTxEq")).toString(), QStringLiteral("True"));
        QCOMPARE(a.value(QStringLiteral("RecordingsOnAirUseCompressor")).toString(), QStringLiteral("True"));
        QCOMPARE(a.value(QStringLiteral("RecordingsOnAirUseLeveler")).toString(), QStringLiteral("False"));
        QCOMPARE(a.value(QStringLiteral("RecordingsOnAirUseCfc")).toString(), QStringLiteral("True"));
        QCOMPARE(a.value(QStringLiteral("RecordingsOnAirUsePhaseRotator")).toString(), QStringLiteral("True"));
        QCOMPARE(a.value(QStringLiteral("DsrEnabled")).toString(), QStringLiteral("False"));
        QCOMPARE(a.value(QStringLiteral("DsrDisk")).toString(), QStringLiteral("/media/ssd"));
        QCOMPARE(a.value(QStringLiteral("DsrSizeLimitGb")).toInt(), 40);

        const RecordingSettings back = RecordingSettings::load();
        QCOMPARE(back.keepMyVoice, false);
        QCOMPARE(back.sidesApart, true);
        QCOMPARE(back.format, AudioFileFormat::Wav);
        QCOMPARE(back.quietMinutes, 7);
        QCOMPARE(back.recordingsFolder(), QStringLiteral("/media/usb/NereusSDR/Recordings"));
        QCOMPARE(back.dsrFolder(), QStringLiteral("/media/ssd/NereusSDR/Spectrum Recorder"));

        RecordingSettings mp3;
        mp3.save();
        QCOMPARE(a.value(QStringLiteral("RecordingsFormat")).toString(), QStringLiteral("Mp3"));
    }

    void quietMinutesClamped()
    {
        AppSettings::instance().setValue(QStringLiteral("RecordingsQuietMinutes"), 0);
        QCOMPARE(RecordingSettings::load().quietMinutes, 1);
        AppSettings::instance().setValue(QStringLiteral("RecordingsQuietMinutes"), 99);
        QCOMPARE(RecordingSettings::load().quietMinutes, 60);
    }

    void sdCardDevices()
    {
        QVERIFY(RecordingDisks::isSdCardDevice("/dev/mmcblk0p2"));
        QVERIFY(!RecordingDisks::isSdCardDevice("/dev/sda1"));
        QVERIFY(!RecordingDisks::isSdCardDevice("/dev/nvme0n1p1"));
        QVERIFY(!RecordingDisks::isSdCardDevice("/dev/disk3s1"));
        QVERIFY(RecordingDisks::isPseudoFileSystem("tmpfs"));
        QVERIFY(!RecordingDisks::isPseudoFileSystem("ext4"));
    }

    void volumesListTheCoreOwnDisk()
    {
        const QList<RecordingVolume> volumes = RecordingDisks::volumes();
        QVERIFY(!volumes.isEmpty());
        QVERIFY(volumes.first().isCoreOwnDisk);
        QVERIFY(volumes.first().totalBytes > 0);
    }
};

QTEST_GUILESS_MAIN(TestRecordingStore)
#include "tst_recording_store.moc"
