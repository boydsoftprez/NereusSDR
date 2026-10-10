// no-port-check: NereusSDR-original tests for RadioModel's timed settings
// save, which must not write the file on the thread that sends the Core's
// receive audio.
#include <QtTest>
#include <QDir>
#include <QFile>
#include <QScopeGuard>
#include <QSemaphore>
#include <QSignalSpy>
#include <QThread>

#include <atomic>
#include <memory>

#include "core/AppSettings.h"
#include "core/SettingsFileWriter.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

constexpr int kTimedSaveWaitMs = 10000;

double savedDouble(const QString& key)
{
    AppSettings onDisk(AppSettings::instance().filePath());
    onDisk.load();
    return onDisk.value(key, QStringLiteral("-1")).toDouble();
}

} // namespace

class TestRadioSettingsBackgroundSave : public QObject {
    Q_OBJECT
private slots:
    void init()
    {
        AppSettings::instance().clear();
        QFile::remove(AppSettings::instance().filePath());
    }

    void cleanup() { AppSettings::instance().setBackgroundSaveStartHookForTesting({}); }

    void theTimedSaveWritesTheFileAwayFromTheMainThread()
    {
        auto& settings = AppSettings::instance();
        RadioModel radio;
        auto* slice = radio.sliceById(radio.addSlice());
        QVERIFY(slice);
        slice->setSettingsRadioIdentity("AA:BB:CC:DD:EE:21");
        const QString prefix = slice->nnrSettingsPrefix();
        std::atomic<QThread*> writerThread{nullptr};
        settings.setBackgroundSaveStartHookForTesting(
            [&writerThread] { writerThread = QThread::currentThread(); });
        QSignalSpy finished(settings.backgroundSaveNotifier(), &SettingsFileWriter::finished);

        slice->setNnrAlpha(2.75);

        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, kTimedSaveWaitMs);
        QCOMPARE(finished.at(0).at(1).toBool(), true);
        QVERIFY(writerThread.load() != nullptr);
        QVERIFY(writerThread.load() != QThread::currentThread());
        QCOMPARE(savedDouble(prefix + "NnrAlpha"), 2.75);
        QVERIFY(radio.settingsSaveError().isEmpty());
    }

    void aFailedTimedSaveIsShownAndALaterSaveClearsIt()
    {
        auto& settings = AppSettings::instance();
        RadioModel radio;
        auto* slice = radio.sliceById(radio.addSlice());
        QVERIFY(slice);
        slice->setSettingsRadioIdentity("AA:BB:CC:DD:EE:22");
        const QString prefix = slice->nnrSettingsPrefix();
        // A directory at the target file path deterministically refuses an
        // atomic file replacement, including when tests run with elevated rights.
        QVERIFY(QDir().mkpath(settings.filePath()));
        QSignalSpy errorChanged(&radio, &RadioModel::settingsSaveErrorChanged);

        slice->setNnrAlpha(2.75);

        QTRY_COMPARE_WITH_TIMEOUT(errorChanged.size(), 1, kTimedSaveWaitMs);
        QVERIFY(!radio.settingsSaveError().isEmpty());
        QCOMPARE(slice->nnrAlpha(), 2.75);
        QVERIFY(QDir().rmdir(settings.filePath()));
        radio.flushPendingSettingsSave();
        QVERIFY(radio.settingsSaveError().isEmpty());
        QCOMPARE(savedDouble(prefix + "NnrAlpha"), 2.75);
    }

    void aFlushWhileTheTimedSaveIsWithTheWriterWritesTheFileItself()
    {
        auto& settings = AppSettings::instance();
        RadioModel radio;
        auto* slice = radio.sliceById(radio.addSlice());
        QVERIFY(slice);
        slice->setSettingsRadioIdentity("AA:BB:CC:DD:EE:23");
        const QString prefix = slice->nnrSettingsPrefix();
        // Shared with the hook, and let go on every way out of this test,
        // so a failed check cannot leave the writer thread held.
        const auto entered = std::make_shared<QSemaphore>();
        const auto release = std::make_shared<QSemaphore>();
        settings.setBackgroundSaveStartHookForTesting([entered, release] {
            entered->release();
            release->acquire();
        });
        const auto letGo = qScopeGuard([release] { release->release(8); });
        QSignalSpy finished(settings.backgroundSaveNotifier(), &SettingsFileWriter::finished);

        slice->setNnrAlpha(2.75);
        QTRY_VERIFY_WITH_TIMEOUT(entered->available() > 0, kTimedSaveWaitMs);
        // The quit and disconnect paths: the file is on disk when the
        // flush returns, with the timed save still held on its thread.
        radio.flushPendingSettingsSave();
        QCOMPARE(savedDouble(prefix + "NnrAlpha"), 2.75);

        // Let the held save go (it was replaced, so it reports nothing),
        // then a marker save to know the writer thread is idle again.
        release->release(2);
        const quint64 marker = settings.saveInBackground();
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, kTimedSaveWaitMs);
        QCOMPARE(finished.at(0).at(0).toULongLong(), marker);
        QCOMPARE(savedDouble(prefix + "NnrAlpha"), 2.75);
    }
};

QTEST_MAIN(TestRadioSettingsBackgroundSave)
#include "tst_radio_settings_background_save.moc"
