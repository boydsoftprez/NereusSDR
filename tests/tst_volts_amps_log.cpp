// no-port-check: NereusSDR-original test of the Volts/Amps log ported from
// Thetis console.cs (readMKIIPAVoltsAmps / LogVA) (R-R3-49).
#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "core/CalibrationController.h"
#include "core/VoltsAmpsLog.h"
#include "models/RadioModel.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTimeZone>

using namespace NereusSDR;

class TestVoltsAmpsLog : public QObject {
    Q_OBJECT
private slots:
    // From Thetis console.cs:24892-24916 [v2.10.3.15] LogVA: turning it on
    // writes the version, the title and the calibration; the reading loop
    // (console.cs:24838-24870, //[2.10.1.0]MW0LGE) appends one line a second
    // and turns the log off after an hour.
    void writesHeaderThenOneLineASecondAndStopsAfterAnHour()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("VALog.txt"));
        QDateTime now = QDateTime(QDate(2026, 9, 29), QTime(10, 0, 0), QTimeZone::UTC);
        VoltsAmpsLog log;
        log.setFilePath(path);
        log.setClock([&now]() { return now; });
        QSignalSpy expired(&log, &VoltsAmpsLog::expired);

        log.sample(100, 200, 13.8, 1.5);   // off: nothing written
        QVERIFY(!QFile::exists(path));

        log.setEnabled(true, QStringLiteral("0.5.2"), QStringLiteral("NereusSDR 0.5.2"), 360.0, 120.0);
        log.sample(2000, 300, 13.81, 1.234);     // first reading: written at once
        now = now.addMSecs(500);
        log.sample(2001, 301, 13.82, 1.3);       // within the second: not written
        now = now.addMSecs(600);
        log.sample(2002, 302, 13.8, 1.0);        // a second on: written

        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QStringList lines = QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'),
                                                                          Qt::SkipEmptyParts);
        QCOMPARE(lines.size(), 5);
        QCOMPARE(lines.at(0), QStringLiteral("0.5.2"));
        QCOMPARE(lines.at(1), QStringLiteral("NereusSDR 0.5.2"));
        QCOMPARE(lines.at(2), QStringLiteral("Volts/Amps Log \t_amp_voff=360\t_amp_sens=120"));
        QCOMPARE(lines.at(3), QStringLiteral(
            "2026-09-29 10:00:00\tadc0(v)=2000\tadc1(a)=300\tvolts=13.81\tamps=1.23"));
        QCOMPARE(lines.at(4), QStringLiteral(
            "2026-09-29 10:00:01\tadc0(v)=2002\tadc1(a)=302\tvolts=13.80\tamps=1.00"));
        file.close();

        // An hour after it was turned on, the next line turns it off.
        now = now.addSecs(3600);
        log.sample(2003, 303, 13.8, 1.0);
        QCOMPARE(expired.count(), 1);
        QVERIFY(!log.enabled());
        now = now.addSecs(2);
        log.sample(2004, 304, 13.8, 1.0);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts)
                     .size(), 6);
    }

    // The station's own radio model logs its PA readings while the box is
    // on, on a board that reads both volts and amps.
    void radioModelLogsItsPaReadingsWhileTheBoxIsOn()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("VALog.txt"));
        RadioModel model;
        model.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
        QVERIFY(model.boardCapabilities().hasPaVoltsTelemetry);
        QVERIFY(model.boardCapabilities().hasPaAmpsTelemetry);
        model.voltsAmpsLogForTest()->setFilePath(path);
        model.handlePaTelemetryForTest(100, 10, 0, 2000, 300, 0);
        QVERIFY(!QFile::exists(path));
        model.calibrationControllerMutable().setLogVoltsAmps(true);
        model.handlePaTelemetryForTest(100, 10, 0, 2000, 300, 0);
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QStringList lines = QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'),
                                                                          Qt::SkipEmptyParts);
        // The header (the test program has no version, so its first line is
        // empty), then the reading.
        QVERIFY(lines.size() >= 3);
        QVERIFY(lines.at(lines.size() - 2).startsWith(QStringLiteral("Volts/Amps Log \t_amp_voff=")));
        QVERIFY(lines.last().contains(QStringLiteral("\tadc0(v)=2000\tadc1(a)=300\tvolts=")));
    }

    // The Calibration tab's box is stored as the tab stores it; the Core's
    // controller reads it with the rest of the calibration.
    void controllerReadsTheStoredBox()
    {
        AppSettings::instance().setValue(
            QStringLiteral("hardware/AA:BB:CC:00:11:22/paCalibration/cal/logVoltsAmps"),
            QStringLiteral("true"));
        CalibrationController cal;
        cal.setMacAddress(QStringLiteral("AA:BB:CC:00:11:22"));
        QVERIFY(!cal.logVoltsAmps());
        cal.load();
        QVERIFY(cal.logVoltsAmps());
        AppSettings::instance().setValue(
            QStringLiteral("hardware/AA:BB:CC:00:11:22/paCalibration/cal/logVoltsAmps"),
            QStringLiteral("False"));
        QSignalSpy changed(&cal, &CalibrationController::logVoltsAmpsChanged);
        cal.load();
        QVERIFY(!cal.logVoltsAmps());
        QCOMPARE(changed.count(), 1);
    }
};

QTEST_MAIN(TestVoltsAmpsLog)
#include "tst_volts_amps_log.moc"
