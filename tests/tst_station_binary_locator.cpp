#include "core/StationBinaryLocator.h"

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>

using namespace NereusSDR;

class StationBinaryLocatorTest : public QObject {
    Q_OBJECT
private slots:
    void layouts_data();
    void layouts();
    void missingOrNonExecutable();
    void appImageLaunchUsesStableFile();
    void appRunDispatchesAndPreservesArguments();
};

void StationBinaryLocatorTest::layouts_data()
{
    QTest::addColumn<int>("platform");
    QTest::addColumn<QString>("relativeBinary");
    QTest::newRow("mac nested bundle") << int(StationPlatform::MacOS)
        << QStringLiteral("NereusSDR.app/Contents/Helpers/NereusStation.app/Contents/MacOS/nereusd");
    QTest::newRow("linux appimage") << int(StationPlatform::Linux)
        << QStringLiteral("NereusSDR.app/Contents/MacOS/nereusd");
    QTest::newRow("windows portable") << int(StationPlatform::Windows)
        << QStringLiteral("NereusSDR.app/Contents/MacOS/nereusd.exe");
}

void StationBinaryLocatorTest::layouts()
{
    QFETCH(int, platform);
    QFETCH(QString, relativeBinary);
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString appDir = temp.filePath(QStringLiteral("NereusSDR.app/Contents/MacOS"));
    const QString binary = temp.filePath(relativeBinary);
    QVERIFY(QDir().mkpath(appDir));
    QVERIFY(QDir().mkpath(QFileInfo(binary).absolutePath()));
    QFile file(binary);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("binary");
    file.close();
    QVERIFY(file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
    QCOMPARE(locateStationBinary(appDir, StationPlatform(platform)), binary);
}

void StationBinaryLocatorTest::missingOrNonExecutable()
{
    QTemporaryDir temp;
    QVERIFY(locateStationBinary(temp.path(), StationPlatform::Linux).isEmpty());
    QFile file(temp.filePath(QStringLiteral("nereusd")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();
    QVERIFY(file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner));
    QVERIFY(locateStationBinary(temp.path(), StationPlatform::Linux).isEmpty());
}

void StationBinaryLocatorTest::appImageLaunchUsesStableFile()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString mounted = QDir::tempPath() + QStringLiteral("/.mount_NereusSDR/usr/bin");
    const StationLaunchSpec missing = locateStationLaunch(mounted, StationPlatform::Linux, {},
                                                           QDir::tempPath() + QStringLiteral("/.mount_NereusSDR"));
    QVERIFY(missing.program.isEmpty());
    QVERIFY(missing.error.contains(QStringLiteral("AppImage path")));
    const QString image = temp.filePath(QStringLiteral("Nereus Station.AppImage"));
    QFile file(image);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("image");
    file.close();
    QVERIFY(file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
    const StationLaunchSpec launch = locateStationLaunch(mounted, StationPlatform::Linux, image,
                                                         QDir::tempPath() + QStringLiteral("/.mount_NereusSDR"));
    QCOMPARE(launch.program, image);
    QCOMPARE(launch.prefixArguments, QStringList({QStringLiteral("--nereus-station")}));
    QVERIFY(launch.error.isEmpty());
}

void StationBinaryLocatorTest::appRunDispatchesAndPreservesArguments()
{
#if defined(Q_OS_WIN)
    QSKIP("AppRun is a POSIX shell script");
#else
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString root = temp.path();
    QVERIFY(QDir().mkpath(root + QStringLiteral("/usr/bin")));
    QVERIFY(QFile::copy(QStringLiteral(NEREUS_SOURCE_DIR "/packaging/linux/AppRun"), root + QStringLiteral("/AppRun.wrapped")));
    for (const QString& name : {QStringLiteral("nereusd"), QStringLiteral("NereusSDR")}) {
        QFile program(root + QStringLiteral("/usr/bin/") + name);
        QVERIFY(program.open(QIODevice::WriteOnly));
        program.write("#!/bin/sh\nprintf '%s\\n' \"$0\" \"$@\"\n");
        program.close();
        QVERIFY(program.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
    }
    QProcess station;
    station.start(root + QStringLiteral("/AppRun.wrapped"), {QStringLiteral("--nereus-station"),
                   QStringLiteral("--profile"), QStringLiteral("Desk One"), QStringLiteral("--config"),
                   QStringLiteral("a & b")});
    QVERIFY(station.waitForFinished(5000));
    QCOMPARE(station.exitCode(), 0);
    const QByteArray stationOutput = station.readAllStandardOutput();
    QVERIFY(stationOutput.contains("/usr/bin/nereusd\n"));
    QVERIFY(stationOutput.contains("Desk One\n"));
    QVERIFY(stationOutput.contains("a & b\n"));
    QVERIFY(!stationOutput.contains("--nereus-station"));
    QProcess gui;
    gui.start(root + QStringLiteral("/AppRun.wrapped"), {QStringLiteral("--normal")});
    QVERIFY(gui.waitForFinished(5000));
    QCOMPARE(gui.exitCode(), 0);
    QVERIFY(gui.readAllStandardOutput().contains("/usr/bin/NereusSDR\n--normal\n"));
#endif
}

QTEST_GUILESS_MAIN(StationBinaryLocatorTest)
#include "tst_station_binary_locator.moc"
