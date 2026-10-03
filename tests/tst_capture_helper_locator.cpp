// =================================================================
// tests/tst_capture_helper_locator.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  Unit tests for locateCaptureHelper()
// (R-R3-36): per-platform lookup order against temporary directory
// layouts, including "none present" and non-executable candidates.
// =================================================================

#include <QtTest/QtTest>

#include "core/audio/CaptureHelperLocator.h"
#include "core/audio/CaptureSupervisor.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

using namespace NereusSDR;

namespace {

#if defined(Q_OS_WIN)
const QString kHelperName = QStringLiteral("nereus-audio-capture.exe");
#else
const QString kHelperName = QStringLiteral("nereus-audio-capture");
#endif

// Creates <root>/<relativePath> (and its parent directories) as a small file,
// executable unless told otherwise.  Returns the cleaned absolute path.
QString makeFile(const QDir& root, const QString& relativePath, bool executable = true)
{
    const QString path = QDir::cleanPath(root.absoluteFilePath(relativePath));
    const QFileInfo info(path);
    if (!QDir().mkpath(info.absolutePath())) {
        return {};
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return {};
    }
    file.write("#!/bin/sh\nexit 0\n");
    file.close();
    QFile::Permissions perms = QFile::ReadOwner | QFile::WriteOwner;
    if (executable) {
        perms |= QFile::ExeOwner;
    }
    file.setPermissions(perms);
    return path;
}

} // namespace

class TestCaptureHelperLocator : public QObject {
    Q_OBJECT

private slots:
    void nonePresentReturnsEmpty()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QDir root(tmp.path());
        QVERIFY(root.mkpath(QStringLiteral("app/bin")));
        QCOMPARE(locateCaptureHelper(root.absoluteFilePath(QStringLiteral("app/bin"))), QString());
    }

    void emptyApplicationDirReturnsEmpty()
    {
        QCOMPARE(locateCaptureHelper(QString()), QString());
    }

    void directoryWithHelperNameIsNotAHelper()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QDir root(tmp.path());
        QVERIFY(root.mkpath(QStringLiteral("app/bin/") + kHelperName));
        QCOMPARE(locateCaptureHelper(root.absoluteFilePath(QStringLiteral("app/bin"))), QString());
    }

#if defined(Q_OS_MAC)
    void macBundleHelpersDirectoryWins()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QDir root(tmp.path());
        const QString bundled =
            makeFile(root, QStringLiteral("NereusSDR.app/Contents/Helpers/nereus-audio-capture"));
        const QString beside =
            makeFile(root, QStringLiteral("NereusSDR.app/Contents/MacOS/nereus-audio-capture"));
        QVERIFY(!bundled.isEmpty());
        QVERIFY(!beside.isEmpty());
        const QString appDir = root.absoluteFilePath(QStringLiteral("NereusSDR.app/Contents/MacOS"));
        QCOMPARE(locateCaptureHelper(appDir), bundled);
    }

    void macBesideExecutableIsSecond()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QDir root(tmp.path());
        const QString beside = makeFile(root, QStringLiteral("bin/nereus-audio-capture"));
        QVERIFY(!beside.isEmpty());
        QCOMPARE(locateCaptureHelper(root.absoluteFilePath(QStringLiteral("bin"))), beside);
    }

    void macNonExecutableBundledHelperIsSkipped()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QDir root(tmp.path());
        QVERIFY(!makeFile(root, QStringLiteral("App.app/Contents/Helpers/nereus-audio-capture"),
                          false).isEmpty());
        const QString beside =
            makeFile(root, QStringLiteral("App.app/Contents/MacOS/nereus-audio-capture"));
        QCOMPARE(locateCaptureHelper(root.absoluteFilePath(QStringLiteral("App.app/Contents/MacOS"))),
                 beside);
    }

    void macLinuxLibLayoutIsNotSearched()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QDir root(tmp.path());
        QVERIFY(!makeFile(root, QStringLiteral("lib/nereus/nereus-audio-capture")).isEmpty());
        QVERIFY(root.mkpath(QStringLiteral("bin")));
        QCOMPARE(locateCaptureHelper(root.absoluteFilePath(QStringLiteral("bin"))), QString());
    }
#elif defined(Q_OS_WIN)
    void windowsBesideExecutable()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QDir root(tmp.path());
        const QString beside = makeFile(root, QStringLiteral("deploy/nereus-audio-capture.exe"));
        QVERIFY(!beside.isEmpty());
        QCOMPARE(locateCaptureHelper(root.absoluteFilePath(QStringLiteral("deploy"))), beside);
    }

    void windowsNameWithoutExeIsNotAHelper()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QDir root(tmp.path());
        QVERIFY(!makeFile(root, QStringLiteral("deploy/nereus-audio-capture")).isEmpty());
        QCOMPARE(locateCaptureHelper(root.absoluteFilePath(QStringLiteral("deploy"))), QString());
    }
#else
    void linuxBesideExecutableWins()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QDir root(tmp.path());
        const QString beside = makeFile(root, QStringLiteral("usr/bin/nereus-audio-capture"));
        const QString libexec = makeFile(root, QStringLiteral("usr/lib/nereus/nereus-audio-capture"));
        QVERIFY(!beside.isEmpty());
        QVERIFY(!libexec.isEmpty());
        QCOMPARE(locateCaptureHelper(root.absoluteFilePath(QStringLiteral("usr/bin"))), beside);
    }

    void linuxLibNereusIsSecond()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QDir root(tmp.path());
        const QString libexec = makeFile(root, QStringLiteral("usr/lib/nereus/nereus-audio-capture"));
        QVERIFY(!libexec.isEmpty());
        QVERIFY(root.mkpath(QStringLiteral("usr/bin")));
        QCOMPARE(locateCaptureHelper(root.absoluteFilePath(QStringLiteral("usr/bin"))), libexec);
    }

    void linuxNonExecutableIsSkipped()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QDir root(tmp.path());
        QVERIFY(!makeFile(root, QStringLiteral("usr/bin/nereus-audio-capture"), false).isEmpty());
        const QString libexec = makeFile(root, QStringLiteral("usr/lib/nereus/nereus-audio-capture"));
        QCOMPARE(locateCaptureHelper(root.absoluteFilePath(QStringLiteral("usr/bin"))), libexec);
    }
#endif

    // The supervisor's default program comes from the locator.  This test
    // binary has no helper beside it, so demand fails with HelperMissing
    // and nothing is spawned.
    void supervisorDefaultProgramUsesLocator()
    {
        QCOMPARE(locateCaptureHelper(), QString());
        CaptureSupervisor supervisor;
        auto lease = supervisor.acquire(CaptureSupervisor::Demand::TestMic);
        QVERIFY(lease.isActive());
        QTRY_COMPARE_WITH_TIMEOUT(supervisor.status().state,
                                  CaptureSupervisor::Status::State::Failed, 2000);
        QCOMPARE(supervisor.status().reason, CaptureSupervisor::Status::Reason::HelperMissing);
        QCOMPARE(supervisor.helperProcessId(), qint64(0));
    }
};

QTEST_MAIN(TestCaptureHelperLocator)
#include "tst_capture_helper_locator.moc"
