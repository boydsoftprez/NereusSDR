// no-port-check: NereusSDR-original. Real temporary files; no RF/audio.
#include <QtTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include "core/LogSink.h"

using namespace NereusSDR;
namespace {
QStringList logs(const QString& dir) {
    return QDir(dir).entryList({"nereussdr-*.log"}, QDir::Files, QDir::Name);
}
QString currentLog(const QTemporaryDir& dir) {
#ifdef Q_OS_WIN
    return QFileInfo(dir.filePath("nereussdr.log.lnk")).symLinkTarget();
#else
    return dir.filePath("nereussdr.log");
#endif
}
QByteArray read(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) { return {}; }
    return f.readAll();
}
}
class TstLogRetention : public QObject {
    Q_OBJECT
private slots:
    void multipleRotationsKeepNewestFiveAndContinueWriting() {
        QTemporaryDir dir;
        LogSink sink;
        QVERIFY(sink.setRotatingOutput(dir.path(), false, 512));
        QStringList previous;
        for (int i = 0; i < 12; ++i) {
            const QString line = QString::number(i).rightJustified(2, '0') + QString(380, 'x') + '\n';
            QVERIFY(sink.offer(line));
            sink.drainNow();
            const QStringList current = logs(dir.path());
            QCOMPARE(current.size(), qMin(i + 1, 5));
            for (const QString& name : current) {
                QVERIFY(QFileInfo(dir.filePath(name)).size() <= 512);
            }
            if (i >= 5) { QVERIFY(!current.contains(previous.first())); }
            QCOMPARE(read(currentLog(dir)), line.toUtf8());
            previous = current;
        }
        QCOMPARE(sink.lastSequence(), quint64(12));
        QVERIFY(sink.linesSince(0).last().text.startsWith("11"));
    }
    void aSingleDrainRotatesRepeatedlyWithoutGaps() {
        QTemporaryDir dir;
        LogSink sink;
        QVERIFY(sink.setRotatingOutput(dir.path(), false, 512));
        QByteArray expected;
        for (int i = 0; i < 12; ++i) {
            const QString line = QString::number(i).rightJustified(2, '0') + QString(380, 'x') + '\n';
            QVERIFY(sink.offer(line));
            if (i >= 7) { expected += line.toUtf8(); }
        }
        sink.drainNow();
        const QStringList names = logs(dir.path());
        QCOMPARE(names.size(), 5);
        QByteArray retained;
        for (const QString& name : names) {
            QVERIFY(QFileInfo(dir.filePath(name)).size() <= 512);
            retained += read(dir.filePath(name));
        }
        QCOMPARE(retained, expected);
        QCOMPARE(sink.lastSequence(), quint64(12));
    }
    void oversizedEntryIsBoundedValidUtf8AndFollowingLinesSurvive() {
        QTemporaryDir dir;
        LogSink sink;
        QVERIFY(sink.setRotatingOutput(dir.path(), false, 512, 5));
        const QString giant = QString(600, QChar(0x20ac)) + "\n";
        QVERIFY(sink.offer(giant));
        sink.drainNow();
        const QStringList names = logs(dir.path());
        QCOMPARE(names.size(), 1);
        const QByteArray disk = read(dir.filePath(names.first()));
        QVERIFY(disk.size() <= 512);
        QVERIFY(!QString::fromUtf8(disk).contains(QChar::ReplacementCharacter));
        QVERIFY(disk.contains("truncated"));
        QVERIFY(sink.offer("after-large\n"));
        QVERIFY(sink.tryDrainNow());
        QVERIFY(read(currentLog(dir)).contains("after-large\n"));
        QCOMPARE(sink.linesSince(0).first().text, giant.chopped(1));
    }
    void defaultsBoundAThirtyThreeMiBRecord() {
        QTemporaryDir dir;
        LogSink sink;
        QVERIFY(sink.setRotatingOutput(dir.path(), false));
        QVERIFY(sink.offer(QString(33 * 1024 * 1024, 'x') + "\n"));
        sink.drainNow();
        for (const QString& name : logs(dir.path())) {
            QVERIFY(QFileInfo(dir.filePath(name)).size() <= 32 * 1024 * 1024);
        }
    }
    void startupRetainsNewestDiagnosticsAndPrunesOldest() {
        QTemporaryDir dir;
        for (int i = 0; i < 6; ++i) {
            QFile f(dir.filePath(QString("nereussdr-20000101-00000%1.log").arg(i)));
            QVERIFY(f.open(QIODevice::WriteOnly));
            QVERIFY(f.write(QByteArray(700, 'x') + QString(300, QChar(0x20ac)).toUtf8() + "\nrecent-tail\n") > 512);
        }
        QFile unrelated(dir.filePath("other.log"));
        QVERIFY(unrelated.open(QIODevice::WriteOnly));
        unrelated.write(QByteArray(700, 'y'));
        unrelated.close();
        LogSink sink;
        QVERIFY(sink.setRotatingOutput(dir.path(), false, 512, 5));
        const QStringList names = logs(dir.path());
        QCOMPARE(names.size(), 5);
        QVERIFY(!names.contains("nereussdr-20000101-000000.log"));
        QVERIFY(!names.contains("nereussdr-20000101-000001.log"));
        for (const QString& name : names) {
            const QByteArray disk = read(dir.filePath(name));
            QVERIFY(disk.size() <= 512);
            QVERIFY(!QString::fromUtf8(disk).contains(QChar::ReplacementCharacter));
            if (name.startsWith("nereussdr-2000")) { QVERIFY(disk.endsWith("recent-tail\n")); }
        }
        QCOMPARE(QFileInfo(unrelated.fileName()).size(), qint64(700));
    }
    void failedStartupKeepsThePreviousAliasTarget() {
        QTemporaryDir dir;
        for (int i = 0; i < 6; ++i) {
            QFile f(dir.filePath(QString("nereussdr-20000101-00000%1.log").arg(i)));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("previous-diagnostic\n");
        }
#ifdef Q_OS_WIN
        const QString alias = dir.filePath("nereussdr.log.lnk");
        const QString pending = alias + ".new.lnk";
#else
        const QString alias = dir.filePath("nereussdr.log");
        const QString pending = alias + ".new";
#endif
        const QString previous = dir.filePath("nereussdr-20000101-000000.log");
        QVERIFY(QFile::link(previous, alias));
        // A clock change can make the last run's active log sort oldest.
        // An obstructed pending alias forces failure after pruning.
        QVERIFY(QDir().mkpath(pending));
        QFile obstruction(QDir(pending).filePath("keep"));
        QVERIFY(obstruction.open(QIODevice::WriteOnly));
        obstruction.close();
        LogSink sink;
        QVERIFY(!sink.setRotatingOutput(dir.path(), false, 512));
        QVERIFY(QFileInfo::exists(previous));
        QCOMPARE(read(currentLog(dir)), QByteArray("previous-diagnostic\n"));
        QVERIFY(logs(dir.path()).size() <= 5);
        QVERIFY(sink.offer("startup-failed-still-recent\n")); sink.drainNow();
        QCOMPARE(sink.linesSince(0).last().text, QString("startup-failed-still-recent"));
    }
    void secondWriterCannotTouchAnActiveProfile() {
        QTemporaryDir dir;
        LogSink first;
        QVERIFY(first.setRotatingOutput(dir.path(), false, 512, 5));
        QVERIFY(first.offer("original\n")); first.drainNow();
        const auto before = logs(dir.path());
        LogSink second;
        QVERIFY(!second.setRotatingOutput(dir.path(), false, 512, 5));
        QVERIFY(second.offer("second-still-recent\n")); second.drainNow();
        QCOMPARE(logs(dir.path()), before);
        QCOMPARE(read(currentLog(dir)), QByteArray("original\n"));
        QCOMPARE(second.linesSince(0).last().text, QString("second-still-recent"));
        QVERIFY(first.offer("first-continues\n")); first.drainNow();
        QVERIFY(read(currentLog(dir)).endsWith("first-continues\n"));
        first.setOutputs(nullptr, false);
        QVERIFY(second.setRotatingOutput(dir.path(), false, 512, 5));
    }
    void failedRotationPreservesLastFileAndRecentStream() {
#ifdef Q_OS_WIN
        QSKIP("This filesystem failure fixture renames a directory with an open file (POSIX).");
#endif
        QTemporaryDir dir;
        const QString path = dir.filePath("profile");
        QVERIFY(QDir().mkpath(path));
        LogSink sink;
        QVERIFY(sink.setRotatingOutput(path, qEnvironmentVariableIsSet("NEREUS_RETENTION_STDERR_CHILD"), 128, 5));
        QVERIFY(sink.offer(QString(110, 'a') + "\n")); sink.drainNow();
        const QString oldName = logs(path).first();
        const QString moved = dir.filePath("moved-profile");
        QVERIFY(QDir().rename(path, moved));
        QFile obstruction(path);
        QVERIFY(obstruction.open(QIODevice::WriteOnly));
        obstruction.close();
        for (int i = 0; i < 5; ++i) {
            QVERIFY(sink.offer(QString(110, 'b') + "\n")); sink.drainNow();
        }
        // Restore the directory before releasing the profile lock, so
        // this fault fixture also leaves no stale lock behind.
        QVERIFY(obstruction.remove());
        QVERIFY(QDir().rename(moved, path));
        QCOMPARE(QFileInfo(QDir(path).filePath(oldName)).size(), qint64(111));
        QCOMPARE(logs(path).size(), 1);
        QCOMPARE(sink.lastSequence(), quint64(6));
    }
    void failedFileOutputContinuesOnStderr() {
#ifdef Q_OS_WIN
        QSKIP("The child uses the POSIX directory rename failure fixture.");
#endif
        QProcess child;
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert("NEREUS_RETENTION_STDERR_CHILD", "1");
        child.setProcessEnvironment(environment);
        child.setProcessChannelMode(QProcess::MergedChannels);
        child.start(QCoreApplication::applicationFilePath(),
                    {"failedRotationPreservesLastFileAndRecentStream"});
        QVERIFY(child.waitForFinished(60000));
        QCOMPARE(child.exitStatus(), QProcess::NormalExit);
        QCOMPARE(child.exitCode(), 0);
        const QByteArray output = child.readAll();
        QCOMPARE(output.count(QByteArray(110, 'b') + "\n"), 5);
    }
    void shutdownReleasesProfileAndFinalLineIsFlushed() {
        QTemporaryDir dir;
        {
            LogSink sink;
            QVERIFY(sink.setRotatingOutput(dir.path(), false, 128, 5));
            sink.start();
            QVERIFY(sink.offer("final-line\n"));
            sink.stop();
            QCOMPARE(read(currentLog(dir)), QByteArray("final-line\n"));
        }
        LogSink next;
        QVERIFY(next.setRotatingOutput(dir.path(), false, 128, 5));
        QCOMPARE(logs(dir.path()).size(), 2);
    }
};
QTEST_GUILESS_MAIN(TstLogRetention)
#include "tst_log_retention.moc"
