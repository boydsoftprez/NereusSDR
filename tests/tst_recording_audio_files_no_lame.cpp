// =================================================================
// tests/tst_recording_audio_files_no_lame.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test. AudioFileWriter compiled
// without NEREUS_HAVE_LAME, as in a build that found no LAME: MP3 is
// refused with a reason, before any file is made, so the caller can fall
// back to WAV, and WAV still opens.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-15 - Created. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QFileInfo>
#include <QTemporaryDir>

#include "core/LogCategories.h"
#include "core/recordings/AudioFileWriter.h"

namespace NereusSDR {
// The writer's one dependency outside itself; LogCategories.cpp would
// bring the settings store with it.
Q_LOGGING_CATEGORY(lcRecordings, "nereus.recordings")
}  // namespace NereusSDR

using namespace NereusSDR;

class TestRecordingAudioFilesNoLame : public QObject {
    Q_OBJECT

private slots:
    void mp3IsRefusedWithAReason()
    {
#ifdef NEREUS_HAVE_LAME
        QFAIL("This test must build the writer without LAME.");
#endif
        QVERIFY(!AudioFileWriter::mp3Available());
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString mp3 = dir.filePath(QStringLiteral("contact.mp3"));
        QString error;
        QVERIFY(!AudioFileWriter::open(mp3, {AudioFileFormat::Mp3, 24000, 1, 64}, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!QFileInfo::exists(mp3));

        const QString wav = dir.filePath(QStringLiteral("contact.wav"));
        error.clear();
        auto writer = AudioFileWriter::open(wav, {AudioFileFormat::Wav, 24000, 1, 0}, &error);
        QVERIFY2(writer, qPrintable(error));
        const float silence[4] = {};
        QVERIFY(writer->write(silence, 4));
        QVERIFY(writer->finish());
        QCOMPARE(QFileInfo(wav).size(), qint64(44 + 8));
    }
};

QTEST_GUILESS_MAIN(TestRecordingAudioFilesNoLame)
#include "tst_recording_audio_files_no_lame.moc"
