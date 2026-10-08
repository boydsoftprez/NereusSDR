// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - the Core's rotor controller (rotor control plan, Task 3c).
//
// The controller over a RotorConnection whose byte stream is a fake: the
// settings it keeps under Rotor/* and their defaults, the refusals word for
// word, arrival within 1.5 degrees or when the heading stops changing, Stop
// ahead of anything queued, the hold dead man (a lapsed hold, a released
// hold and a window that goes away all send stop), turning to a callsign,
// the presets, and the span and travel it reports. No test opens a real
// serial port, starts the real rotctld or turns a rotor.
//
// Modification history (NereusSDR):
//   2026-10-08: Initial version. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.

#include <QtTest>

#include "OperatorWording.h"
#include "core/AppSettings.h"
#include "core/GreatCircle.h"
#include "core/RotctldProcess.h"
#include "core/RotorConnection.h"
#include "core/StationRotorController.h"

#include <QElapsedTimer>
#include <QLoggingCategory>
#include <QPointer>
#include <QRegularExpression>
#include <QSignalSpy>

#include <cmath>
#include <limits>
#include <memory>

using namespace NereusSDR;
using RotorRoute::EndStop;
using Phase = RotorConnectionPhase;

namespace {

class FakeTransport : public RotorTransport {
public:
    bool failOpen{false};
    QByteArray written;
    QByteArray pending;

    void open() override
    {
        if (failOpen) {
            emit failed(QStringLiteral("Could not open the fake port."));
        } else {
            emit opened();
        }
    }
    void close() override {}
    qint64 write(const QByteArray& bytes) override
    {
        written += bytes;
        return bytes.size();
    }
    QByteArray readAll() override
    {
        QByteArray out = pending;
        pending.clear();
        return out;
    }
    void feed(const QByteArray& bytes)
    {
        pending += bytes;
        emit readyRead();
    }
    QByteArray take()
    {
        QByteArray out = written;
        written.clear();
        return out;
    }
};

// Timers that never fire on their own: the test steps every poll.
RotorConnection::Timing steppedTiming()
{
    RotorConnection::Timing t;
    t.pollStillMs = 3600000;
    t.pollTurningMs = 3599000;
    t.replyTimeoutMs = 3600000;
    t.settleMs = 3600000;
    t.staleMs = 3600000;
    t.answerDeadlineMs = 3600000;
    t.reconnectUnitMs = 3600000;
    t.rotctldStartDelayMs = 0;
    return t;
}

const QString kPort = QStringLiteral("/dev/ttyUSB0");

void expectWarning(const char* pattern)
{
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QString::fromLatin1(pattern)));
}

} // namespace

class TestStationRotorController : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<StationRotorController> m_ctl;
    QPointer<FakeTransport> m_fake;
    bool m_failOpen{false};

    void make(const RotorConnection::Timing& timing = steppedTiming())
    {
        m_ctl = std::make_unique<StationRotorController>();
        m_ctl->setSerialPortListerForTesting(
            [] { return QStringList{kPort, QStringLiteral("COM4")}; });
        m_ctl->connection()->setTransportFactoryForTesting(
            [this](const RotorTransportTarget&) {
                auto t = std::make_unique<FakeTransport>();
                t->failOpen = m_failOpen;
                m_fake = t.get();
                return std::unique_ptr<RotorTransport>(std::move(t));
            });
        m_ctl->connection()->setTimingForTesting(timing);
    }

    static RotorConfig gs232b()
    {
        RotorConfig c;
        c.driver = RotorDriver::Gs232b;
        c.serialPort = kPort;
        return c;
    }

    // A GS-232B rotor, connected, its first poll answered at `heading`.
    void connectAt(const QByteArray& heading, RotorConfig c = gs232b(),
                   const RotorConnection::Timing& timing = steppedTiming())
    {
        make(timing);
        QString why;
        QVERIFY2(m_ctl->configureRotor(c, &why), qPrintable(why));
        QVERIFY(m_fake);
        // Bench fix: connected only once the first poll is answered.
        QCOMPARE(m_ctl->connectionPhase(), Phase::Connecting);
        QCOMPARE(m_fake->take(), QByteArray("C2\r"));
        m_fake->feed("AZ=" + heading + "  EL=000\r\n");
        QCOMPARE(m_ctl->connectionPhase(), Phase::Connected);
    }

private slots:
    void initTestCase()
    {
        QLoggingCategory::setFilterRules(QStringLiteral(
            "nereus.rotor.debug=false\nnereus.rotor.info=false\n"
            "nereus.rotor.controller.info=false"));
    }

    void init()
    {
        AppSettings::instance().clear();
        m_failOpen = false;
        RotctldProcess::setBinaryOverrideForTesting(std::nullopt);
    }

    void cleanup()
    {
        m_ctl.reset();
        RotctldProcess::setBinaryOverrideForTesting(std::nullopt);
        AppSettings::instance().clear();
    }

    // ── Settings ────────────────────────────────────────────────────

    void defaultsWithNothingSaved()
    {
        make();
        m_ctl->start();
        const RotorConfig& c = m_ctl->config();
        QCOMPARE(c.driver, RotorDriver::None);
        QCOMPARE(c.baud, 9600);
        QCOMPARE(c.port, quint16(4533));
        QCOMPARE(c.axes, RotorAxes::Azimuth);
        QCOMPARE(c.endStop, EndStop::North);
        QCOMPARE(c.rangeDeg, 360.0);
        QCOMPARE(c.offsetDeg, 0.0);
        QCOMPARE(m_ctl->connectionPhase(), Phase::Disabled);
        QCOMPARE(m_ctl->motion(), RotorMotion::Stopped);
        QCOMPARE(m_ctl->azimuthDeg(), -1.0);
        QCOMPARE(m_ctl->targetAzimuthDeg(), -1.0);
        QCOMPARE(m_ctl->spanPositionDeg(), -1.0);
        QCOMPARE(m_ctl->travelDeg(), 0.0);
        QVERIFY(!m_ctl->positionFresh());
        QVERIFY(m_ctl->label().isEmpty());
        QVERIFY(!m_fake);   // nothing was opened
    }

    void configureSavesUnderTheContractKeysAndConnects()
    {
        make();
        RotorConfig c = gs232b();
        c.baud = 4800;
        c.endStop = EndStop::South;
        c.rangeDeg = 450.0;
        c.offsetDeg = -2.5;
        QSignalSpy changed(m_ctl.get(), &StationRotorController::stateChanged);
        QString why;
        QVERIFY(m_ctl->configureRotor(c, &why));
        QVERIFY(changed.count() > 0);
        QCOMPARE(m_ctl->connectionPhase(), Phase::Connecting);
        m_fake->feed("AZ=302  EL=000\r\n");
        QCOMPARE(m_ctl->connectionPhase(), Phase::Connected);
        QVERIFY(m_ctl->connectionError().isEmpty());
        QCOMPARE(m_ctl->label(), QStringLiteral("Yaesu GS-232B on /dev/ttyUSB0"));
        QCOMPARE(m_ctl->serialPort(), kPort);
        QVERIFY(m_ctl->host().isEmpty());

        auto& s = AppSettings::instance();
        QCOMPARE(s.value(QStringLiteral("Rotor/Driver")).toString(), QStringLiteral("2"));
        QCOMPARE(s.value(QStringLiteral("Rotor/SerialPort")).toString(), kPort);
        QCOMPARE(s.value(QStringLiteral("Rotor/Baud")).toString(), QStringLiteral("4800"));
        QCOMPARE(s.value(QStringLiteral("Rotor/Port")).toString(), QStringLiteral("4533"));
        QCOMPARE(s.value(QStringLiteral("Rotor/HamlibModel")).toString(), QStringLiteral("0"));
        QCOMPARE(s.value(QStringLiteral("Rotor/Axes")).toString(), QStringLiteral("0"));
        QCOMPARE(s.value(QStringLiteral("Rotor/EndStop")).toString(), QStringLiteral("2"));
        QCOMPARE(s.value(QStringLiteral("Rotor/RangeDeg")).toString(), QStringLiteral("450"));
        QCOMPARE(s.value(QStringLiteral("Rotor/OffsetDeg")).toString(), QStringLiteral("-2.5"));

        // A Core started later reads the same setup back and connects.
        m_ctl.reset();
        make();
        m_ctl->start();
        QCOMPARE(m_ctl->config().driver, RotorDriver::Gs232b);
        QCOMPARE(m_ctl->config().serialPort, kPort);
        QCOMPARE(m_ctl->config().baud, 4800);
        QCOMPARE(m_ctl->config().endStop, EndStop::South);
        QCOMPARE(m_ctl->config().rangeDeg, 450.0);
        QCOMPARE(m_ctl->config().offsetDeg, -2.5);
        QCOMPARE(m_ctl->connectionPhase(), Phase::Connecting);
        m_fake->feed("AZ=302  EL=000\r\n");
        QCOMPARE(m_ctl->connectionPhase(), Phase::Connected);
    }

    void configureRefusesAPortTheCoreDoesNotHave()
    {
        make();
        RotorConfig c = gs232b();
        c.serialPort = QStringLiteral("/dev/ttyACM9");
        QString why;
        QVERIFY(!m_ctl->configureRotor(c, &why));
        QCOMPARE(why, QStringLiteral("That serial port is not on the Core's computer."));
        QVERIFY(!AppSettings::instance().contains(QStringLiteral("Rotor/Driver")));
        QCOMPARE(m_ctl->serialPortsText(), QStringLiteral("/dev/ttyUSB0\nCOM4"));
    }

    void configureRefusesDriver4WithoutRotctld()
    {
        make();
        RotctldProcess::setBinaryOverrideForTesting(QString());
        QVERIFY(!m_ctl->rotctldAvailable());
        RotorConfig c;
        c.driver = RotorDriver::RotctldStarted;
        c.serialPort = kPort;
        c.hamlibModel = 404;
        QString why;
        QVERIFY(!m_ctl->configureRotor(c, &why));
        QCOMPARE(why, QStringLiteral("Hamlib's rotctld is not installed on the Core's computer."));
        QVERIFY(!AppSettings::instance().contains(QStringLiteral("Rotor/Driver")));

        RotctldProcess::setBinaryOverrideForTesting(QStringLiteral("/usr/local/bin/rotctld"));
        QVERIFY(m_ctl->rotctldAvailable());
    }

    void driverNoneDisconnectsAndForgets()
    {
        connectAt("090");
        QVERIFY(m_ctl->setRotorPresets(QStringLiteral("Europe\t45"), nullptr));
        QString why;
        QVERIFY(m_ctl->configureRotor(RotorConfig{}, &why));
        QCOMPARE(m_ctl->connectionPhase(), Phase::Disabled);
        QCOMPARE(m_ctl->config().driver, RotorDriver::None);
        QCOMPARE(m_ctl->azimuthDeg(), -1.0);
        auto& s = AppSettings::instance();
        QVERIFY(!s.contains(QStringLiteral("Rotor/Driver")));
        QVERIFY(!s.contains(QStringLiteral("Rotor/SerialPort")));
        // The presets are kept.
        QCOMPARE(s.value(QStringLiteral("Rotor/Presets")).toString(), QStringLiteral("Europe\t45"));
        QVERIFY(!m_ctl->setRotorTarget(10.0, -1.0, &why));
        QCOMPARE(why, QStringLiteral("No rotor is set up on this Core."));
    }

    void disconnectKeepsTheSetup()
    {
        connectAt("090");
        QVERIFY(m_ctl->disconnectRotor(nullptr));
        QCOMPARE(m_ctl->connectionPhase(), Phase::Disconnected);
        QCOMPARE(m_ctl->azimuthDeg(), -1.0);
        QCOMPARE(m_ctl->motion(), RotorMotion::Stopped);
        QCOMPARE(AppSettings::instance().value(QStringLiteral("Rotor/Driver")).toString(),
                 QStringLiteral("2"));
        QString why;
        QVERIFY(!m_ctl->stopRotor(&why));
        QCOMPARE(why, QStringLiteral("The rotor is not connected."));
    }

    void aFailedOpenIsRetryingWithItsReason()
    {
        make();
        m_failOpen = true;
        expectWarning("Could not open the fake port");
        QString why;
        QVERIFY(m_ctl->configureRotor(gs232b(), &why));
        QCOMPARE(m_ctl->connectionPhase(), Phase::Retrying);
        QCOMPARE(m_ctl->connectionError(), QStringLiteral("Could not open the fake port."));
        QCOMPARE(m_ctl->fault(), QStringLiteral("Could not open the fake port."));
        QVERIFY(!m_ctl->setRotorTarget(10.0, -1.0, &why));
        QCOMPARE(why, QStringLiteral("The rotor is not connected."));
    }

    // Bench fix: an open port with no rotor answering is connecting, then
    // a fault in plain words, never "connected".
    void aPortThatNeverAnswersIsConnectingThenAFault()
    {
        RotorConnection::Timing t = steppedTiming();
        t.answerDeadlineMs = 50;
        make(t);
        QString why;
        QVERIFY(m_ctl->configureRotor(gs232b(), &why));
        QCOMPARE(m_ctl->connectionPhase(), Phase::Connecting);
        expectWarning("is not answering");
        QTRY_COMPARE_WITH_TIMEOUT(m_ctl->connectionPhase(), Phase::Retrying, 2000);
        const QString reason = QStringLiteral(
            "The rotor controller on /dev/ttyUSB0 is not answering. "
            "Check the serial port and the baud rate.");
        QCOMPARE(m_ctl->connectionError(), reason);
        QCOMPARE(m_ctl->fault(), reason);
        QVERIFY(OperatorWording::isPlain(reason));
    }

    // Bench fix: on JJ's Rock the Core's list put /dev/ttyFIQ0 (the debug
    // console) above the ERC's /dev/ttyUSB0, and it was chosen by mistake.
    void serialPortsOfferUsbAdaptersFirstAndNoConsoles()
    {
        using Port = StationRotorController::SerialPortCandidate;
        const QStringList consoles = StationRotorController::consoleDevicesFromCmdline(
            QStringLiteral("storagemedia=emmc androidboot.mode=normal "
                           "console=ttyFIQ0,1500000n8 console=ttyS2,1500000n8 "
                           "console=tty1 root=/dev/mmcblk0p2\n"));
        QCOMPARE(consoles, QStringList({QStringLiteral("ttyFIQ0"), QStringLiteral("ttyS2"),
                                        QStringLiteral("tty1")}));
        QVERIFY(StationRotorController::consoleDevicesFromCmdline(QString()).isEmpty());

        // The Rock: the FIQ console and the kernel's console are left out,
        // the USB adapters lead in name order, the onboard UART follows.
        const QList<Port> rock = {
            {QStringLiteral("/dev/ttyFIQ0"), false}, {QStringLiteral("/dev/ttyS2"), false},
            {QStringLiteral("/dev/ttyS4"), false},   {QStringLiteral("/dev/ttyUSB10"), true},
            {QStringLiteral("/dev/ttyACM0"), true},  {QStringLiteral("/dev/ttyUSB0"), true},
            {QStringLiteral("/dev/ttyUSB2"), false}};
        QCOMPARE(StationRotorController::orderSerialPorts(rock, consoles),
                 QStringList({QStringLiteral("/dev/ttyACM0"), QStringLiteral("/dev/ttyUSB0"),
                              QStringLiteral("/dev/ttyUSB2"), QStringLiteral("/dev/ttyUSB10"),
                              QStringLiteral("/dev/ttyS4")}));
        // With no command line to read, ttyFIQ is still left out.
        QCOMPARE(StationRotorController::orderSerialPorts(
                     {{QStringLiteral("/dev/ttyFIQ0"), false},
                      {QStringLiteral("/dev/ttyS0"), false},
                      {QStringLiteral("/dev/ttyUSB0"), false}}, {}),
                 QStringList({QStringLiteral("/dev/ttyUSB0"), QStringLiteral("/dev/ttyS0")}));

        // macOS: the USB adapters' cu. and tty. nodes lead; Bluetooth follows.
        const QList<Port> mac = {
            {QStringLiteral("/dev/cu.Bluetooth-Incoming-Port"), false},
            {QStringLiteral("/dev/cu.usbserial-A10K"), true},
            {QStringLiteral("/dev/cu.usbmodem1101"), false}};
        QCOMPARE(StationRotorController::orderSerialPorts(mac, {}),
                 QStringList({QStringLiteral("/dev/cu.usbmodem1101"),
                              QStringLiteral("/dev/cu.usbserial-A10K"),
                              QStringLiteral("/dev/cu.Bluetooth-Incoming-Port")}));

        // Windows: COM ports with a USB vendor id lead, COM10 after COM3.
        const QList<Port> windows = {
            {QStringLiteral("COM1"), false}, {QStringLiteral("COM10"), true},
            {QStringLiteral("COM3"), true}, {QStringLiteral("COM3"), true}};
        QCOMPARE(StationRotorController::orderSerialPorts(windows, {}),
                 QStringList({QStringLiteral("COM3"), QStringLiteral("COM10"),
                              QStringLiteral("COM1")}));
    }

    // ── Refusals ────────────────────────────────────────────────────

    void turnCommandsRefusedWithNoRotor()
    {
        make();
        m_ctl->start();
        const QString none = QStringLiteral("No rotor is set up on this Core.");
        QString why;
        QVERIFY(!m_ctl->setRotorTarget(10.0, -1.0, &why));
        QCOMPARE(why, none);
        why.clear();
        QVERIFY(!m_ctl->stopRotor(&why));
        QCOMPARE(why, none);
        why.clear();
        QVERIFY(!m_ctl->nudgeRotor(RotorDirection::Cw, true, 1, &why));
        QCOMPARE(why, none);
        why.clear();
        QVERIFY(!m_ctl->turnRotorToCall(QStringLiteral("JA1XYZ"), false, &why));
        QCOMPARE(why, none);
    }

    void strictHeadingsAreRefusedWithTheContractReasons()
    {
        connectAt("090");
        const QString nan = QStringLiteral("That heading is not a number.");
        const QString range = QStringLiteral("That heading is outside the rotor's range.");
        QString why;
        QVERIFY(!m_ctl->setRotorTarget(std::numeric_limits<double>::quiet_NaN(), -1.0, &why));
        QCOMPARE(why, nan);
        QVERIFY(!m_ctl->setRotorTarget(std::numeric_limits<double>::infinity(), -1.0, &why));
        QCOMPARE(why, nan);
        QVERIFY(!m_ctl->setRotorTarget(-90.0, -1.0, &why));
        QCOMPARE(why, range);
        QVERIFY(!m_ctl->setRotorTarget(360.5, -1.0, &why));
        QCOMPARE(why, range);
        QVERIFY(!m_ctl->setRotorTarget(10.0, 30.0, &why));
        QCOMPARE(why, QStringLiteral("This rotor turns in azimuth only."));
        QVERIFY(!m_ctl->nudgeRotor(RotorDirection::Up, true, 1, &why));
        QCOMPARE(why, QStringLiteral("This rotor turns in azimuth only."));
        QCOMPARE(m_fake->take(), QByteArray());   // nothing reached the rotor

        // 360 is north, sent as 0.
        QVERIFY(m_ctl->setRotorTarget(360.0, -1.0, &why));
        QCOMPARE(m_ctl->targetAzimuthDeg(), 0.0);
        QCOMPARE(m_fake->take(), QByteArray("W000 000\r"));
    }

    void azElRotorRefusesElevationOutsideItsRange()
    {
        RotorConfig c = gs232b();
        c.axes = RotorAxes::AzimuthElevation;
        connectAt("090", c);
        QString why;
        QVERIFY(!m_ctl->setRotorTarget(10.0, 91.0, &why));
        QCOMPARE(why, QStringLiteral("That heading is outside the rotor's range."));
        QVERIFY(m_ctl->setRotorTarget(10.0, 45.0, &why));
        QCOMPARE(m_ctl->targetElevationDeg(), 45.0);
        QCOMPARE(m_fake->take(), QByteArray("W010 045\r"));
    }

    void refusalsAndLabelsAreOperatorWords()
    {
        for (const QString& text : {
                 StationRotorController::noRotorReason(),
                 StationRotorController::notConnectedReason(),
                 StationRotorController::azimuthOnlyReason(),
                 StationRotorController::notANumberReason(),
                 StationRotorController::outOfRangeReason(),
                 StationRotorController::noGridReason(),
                 StationRotorController::callNotPlacedReason(),
                 StationRotorController::rotctldMissingReason(),
                 StationRotorController::unknownSerialPortReason()}) {
            QVERIFY2(OperatorWording::isPlain(text), qPrintable(text));
            QVERIFY2(!text.contains(QChar(0x2014)), qPrintable(text));
        }
        connectAt("090");
        QVERIFY2(OperatorWording::isPlain(m_ctl->label()), qPrintable(m_ctl->label()));
    }

    // ── Target and arrival ──────────────────────────────────────────

    void arrivalWithinOneAndAHalfDegreesStops()
    {
        connectAt("090");
        QString why;
        QVERIFY(m_ctl->setRotorTarget(100.0, -1.0, &why));
        QCOMPARE(m_fake->take(), QByteArray("W100 000\r"));
        QCOMPARE(m_ctl->motion(), RotorMotion::Turning);
        QCOMPARE(m_ctl->targetAzimuthDeg(), 100.0);
        QCOMPARE(m_ctl->travelDeg(), 10.0);
        QVERIFY(m_ctl->routeKnown());

        m_ctl->connection()->pollNowForTesting();
        QCOMPARE(m_fake->take(), QByteArray("C2\r"));
        m_fake->feed("AZ=097  EL=000\r\n");    // 3 degrees short: still turning
        QCOMPARE(m_ctl->motion(), RotorMotion::Turning);
        QCOMPARE(m_ctl->travelDeg(), 3.0);

        m_ctl->connection()->pollNowForTesting();
        QCOMPARE(m_fake->take(), QByteArray("C2\r"));
        m_fake->feed("AZ=099  EL=000\r\n");    // within 1.5: arrived
        QCOMPARE(m_ctl->motion(), RotorMotion::Stopped);
        QCOMPARE(m_ctl->targetAzimuthDeg(), -1.0);
        QCOMPARE(m_ctl->travelDeg(), 0.0);
        QCOMPARE(m_ctl->azimuthDeg(), 99.0);
    }

    void aHeadingThatStopsChangingStops()
    {
        RotorConnection::Timing t = steppedTiming();
        t.settleMs = 60;
        connectAt("090", gs232b(), t);
        QVERIFY(m_ctl->setRotorTarget(200.0, -1.0, nullptr));
        QCOMPARE(m_ctl->motion(), RotorMotion::Turning);
        m_fake->take();
        QTest::qWait(120);
        // The rotor never moved (a stop, or a controller that ignored it).
        m_ctl->connection()->pollNowForTesting();
        QCOMPARE(m_ctl->motion(), RotorMotion::Stopped);
        QCOMPARE(m_ctl->targetAzimuthDeg(), -1.0);
    }

    void turningIsAllowedWhileTheRadioIsOnTheAir()
    {
        // The controller has no transmit input at all (JJ, 2026-10-07: a
        // rotor switches no RF path). A target is accepted and sent with
        // nothing but a connected rotor.
        connectAt("090");
        QString why;
        QVERIFY(m_ctl->setRotorTarget(180.0, -1.0, &why));
        QCOMPARE(m_fake->take(), QByteArray("W180 000\r"));
    }

    void spanAndTravelFollowTheEndStop()
    {
        // JJ's ERC: south stop, 450 degrees. 292 is span 112 (outside the
        // overlap, so placed at once); 292 to 000 is +68 clockwise through
        // north (the range capture).
        RotorConfig c = gs232b();
        c.endStop = EndStop::South;
        c.rangeDeg = 450.0;
        connectAt("292", c);
        QCOMPARE(m_ctl->spanPositionDeg(), 112.0);
        QVERIFY(m_ctl->setRotorTarget(0.0, -1.0, nullptr));
        QVERIFY(m_ctl->routeKnown());
        QCOMPARE(m_ctl->travelDeg(), 68.0);
    }

    void aHeadingInTheOverlapLeavesTheRouteUnknown()
    {
        RotorConfig c = gs232b();
        c.endStop = EndStop::South;
        c.rangeDeg = 450.0;
        connectAt("200", c);   // span 20 or 380: not yet placed
        QCOMPARE(m_ctl->spanPositionDeg(), -1.0);
        QVERIFY(m_ctl->setRotorTarget(10.0, -1.0, nullptr));
        QVERIFY(!m_ctl->routeKnown());
        QCOMPARE(m_ctl->travelDeg(), 0.0);
    }

    // ── Stop ────────────────────────────────────────────────────────

    void stopJumpsTheQueue()
    {
        connectAt("090");
        m_ctl->connection()->pollNowForTesting();
        QCOMPARE(m_fake->take(), QByteArray("C2\r"));   // outstanding
        QVERIFY(m_ctl->setRotorTarget(100.0, -1.0, nullptr));
        QCOMPARE(m_fake->take(), QByteArray());           // queued behind it
        QString why;
        QVERIFY(m_ctl->stopRotor(&why));
        QCOMPARE(m_fake->take(), QByteArray("S\r"));      // at once
        QCOMPARE(m_ctl->targetAzimuthDeg(), -1.0);
        QCOMPARE(m_ctl->motion(), RotorMotion::Stopped);
        m_fake->feed("AZ=090  EL=000\r\n");
        QCOMPARE(m_fake->take(), QByteArray());           // the set was dropped
        // Never refused while connected, even with nothing turning.
        QVERIFY(m_ctl->stopRotor(&why));
        QCOMPARE(m_fake->take(), QByteArray("S\r"));
    }

    // ── The hold dead man ───────────────────────────────────────────

    void aLapsedHoldSendsStop()
    {
        connectAt("090");
        QString why;
        QVERIFY(m_ctl->nudgeRotor(RotorDirection::Cw, true, 7, &why));
        QCOMPARE(m_fake->take(), QByteArray("R\r"));
        QCOMPARE(m_ctl->motion(), RotorMotion::Nudging);
        QCOMPARE(m_ctl->holdSessionId(), quint64(7));

        QElapsedTimer clock;
        clock.start();
        QTRY_COMPARE_WITH_TIMEOUT(m_ctl->motion(), RotorMotion::Stopped, 3000);
        QVERIFY2(clock.elapsed() >= StationRotorController::kHoldLapseMs - 1,
                 qPrintable(QString::number(clock.elapsed())));
        QCOMPARE(m_fake->take(), QByteArray("S\r"));
        QCOMPARE(m_ctl->holdSessionId(), quint64(0));
    }

    void repeatsKeepAHoldGoingAndReleaseStops()
    {
        connectAt("090");
        QVERIFY(m_ctl->nudgeRotor(RotorDirection::Ccw, true, 3, nullptr));
        QCOMPARE(m_fake->take(), QByteArray("L\r"));
        // Repeats every 250 ms for 1.25 s, past the 750 ms lapse.
        for (int i = 0; i < 5; ++i) {
            QTest::qWait(StationRotorController::kHoldRepeatMs);
            QVERIFY(m_ctl->nudgeRotor(RotorDirection::Ccw, true, 3, nullptr));
        }
        QCOMPARE(m_fake->take(), QByteArray());   // no stop, no second move
        QCOMPARE(m_ctl->motion(), RotorMotion::Nudging);

        // Another window letting go does not stop this one's hold.
        QVERIFY(m_ctl->nudgeRotor(RotorDirection::Ccw, false, 4, nullptr));
        QCOMPARE(m_fake->take(), QByteArray());
        QVERIFY(m_ctl->nudgeRotor(RotorDirection::Ccw, false, 3, nullptr));
        QCOMPARE(m_fake->take(), QByteArray("S\r"));
        QCOMPARE(m_ctl->motion(), RotorMotion::Stopped);
    }

    void theHoldingWindowGoingAwaySendsStop()
    {
        connectAt("090");
        QVERIFY(m_ctl->nudgeRotor(RotorDirection::Cw, true, 11, nullptr));
        QCOMPARE(m_fake->take(), QByteArray("R\r"));
        m_ctl->sessionEnded(12);
        QCOMPARE(m_fake->take(), QByteArray());
        QCOMPARE(m_ctl->motion(), RotorMotion::Nudging);
        m_ctl->sessionEnded(11);
        QCOMPARE(m_fake->take(), QByteArray("S\r"));
        QCOMPARE(m_ctl->motion(), RotorMotion::Stopped);
        // The lapse timer is gone with the hold: no second stop.
        QTest::qWait(StationRotorController::kHoldLapseMs + 100);
        QCOMPARE(m_fake->take(), QByteArray());
    }

    // ── A stop before the link closes (final review I2) ─────────────

    void aNewSetupDuringAHoldStopsTheRotorFirst()
    {
        connectAt("090");
        QVERIFY(m_ctl->nudgeRotor(RotorDirection::Cw, true, 5, nullptr));
        QCOMPARE(m_fake->take(), QByteArray("R\r"));
        const QPointer<FakeTransport> oldLink = m_fake;
        RotorConfig c = gs232b();
        c.serialPort = QStringLiteral("COM4");
        QString why;
        QVERIFY2(m_ctl->configureRotor(c, &why), qPrintable(why));
        QCOMPARE(oldLink->written, QByteArray("S\r"));
        QCOMPARE(m_ctl->holdSessionId(), quint64(0));
    }

    void disconnectingDuringATurnStopsTheRotorFirst()
    {
        connectAt("090");
        QVERIFY(m_ctl->setRotorTarget(200.0, -1.0, nullptr));
        QCOMPARE(m_fake->take(), QByteArray("W200 000\r"));
        const QPointer<FakeTransport> link = m_fake;
        QVERIFY(m_ctl->disconnectRotor(nullptr));
        QCOMPARE(link->written, QByteArray("S\r"));
    }

    void noRotorDuringAHoldStopsTheRotorFirst()
    {
        // Driver 0 forgets the setup and disconnects.
        connectAt("090");
        QVERIFY(m_ctl->nudgeRotor(RotorDirection::Ccw, true, 5, nullptr));
        QCOMPARE(m_fake->take(), QByteArray("L\r"));
        const QPointer<FakeTransport> link = m_fake;
        QVERIFY(m_ctl->configureRotor(RotorConfig{}, nullptr));
        QCOMPARE(link->written, QByteArray("S\r"));
    }

    // ── Turning to a callsign ───────────────────────────────────────

    void turnToCallNeedsAGridSquare()
    {
        connectAt("090");
        m_ctl->setCallsignLocator([](const QString&) {
            return std::optional<GeoPosition>(GeoPosition{35.0, 139.0});
        });
        QString why;
        QVERIFY(!m_ctl->turnRotorToCall(QStringLiteral("JA1XYZ"), false, &why));
        QCOMPARE(why, QStringLiteral("Set your grid square in Setup to turn the beam to spots."));
    }

    void turnToCallRefusesACallItCannotPlace()
    {
        connectAt("090");
        AppSettings::instance().setValue(QStringLiteral("User/GridSquare"), QStringLiteral("FN31"));
        QString why;
        // No cty.dat lookup at all, and one that does not know the call.
        QVERIFY(!m_ctl->turnRotorToCall(QStringLiteral("JA1XYZ"), false, &why));
        QCOMPARE(why, QStringLiteral("That callsign could not be placed."));
        m_ctl->setCallsignLocator([](const QString&) { return std::optional<GeoPosition>{}; });
        QVERIFY(!m_ctl->turnRotorToCall(QStringLiteral("XX0XX"), false, &why));
        QCOMPARE(why, QStringLiteral("That callsign could not be placed."));
    }

    void turnToCallTurnsToTheShortOrLongPathBearing()
    {
        connectAt("090");
        AppSettings::instance().setValue(QStringLiteral("User/GridSquare"), QStringLiteral("FN31"));
        const GeoPosition tokyo{35.0, 139.0};
        QString asked;
        m_ctl->setCallsignLocator([&asked, tokyo](const QString& call) {
            asked = call;
            return std::optional<GeoPosition>(tokyo);
        });
        const double shortPath = *GreatCircle::bearingFromGrid(QStringLiteral("FN31"), tokyo);

        QString why;
        QVERIFY2(m_ctl->turnRotorToCall(QStringLiteral(" ja1xyz "), false, &why), qPrintable(why));
        QCOMPARE(asked, QStringLiteral("JA1XYZ"));
        QVERIFY(std::abs(m_ctl->targetAzimuthDeg() - shortPath) < 1e-9);

        QVERIFY(m_ctl->turnRotorToCall(QStringLiteral("JA1XYZ"), true, &why));
        QVERIFY(std::abs(m_ctl->targetAzimuthDeg()
                         - GreatCircle::longPathBearingDeg(shortPath)) < 1e-9);
    }

    // ── Presets ─────────────────────────────────────────────────────

    void presetsAreSavedInOrder()
    {
        make();
        m_ctl->start();
        QString why;
        QVERIFY(m_ctl->setRotorPresets(QStringLiteral("Europe\t45\nJapan\t330\r\n\nNorth\t360"),
                                       &why));
        const QString expected = QStringLiteral("Europe\t45\nJapan\t330\nNorth\t0");
        QCOMPARE(m_ctl->presets(), expected);
        QCOMPARE(AppSettings::instance().value(QStringLiteral("Rotor/Presets")).toString(),
                 expected);

        m_ctl.reset();
        make();
        m_ctl->start();
        QCOMPARE(m_ctl->presets(), expected);
    }

    void presetsRefuseABadHeading()
    {
        make();
        m_ctl->start();
        QVERIFY(m_ctl->setRotorPresets(QStringLiteral("Europe\t45"), nullptr));
        QString why;
        QVERIFY(!m_ctl->setRotorPresets(QStringLiteral("Europe\t45\nOops\t400"), &why));
        QCOMPARE(why, QStringLiteral("That heading is outside the rotor's range."));
        QVERIFY(!m_ctl->setRotorPresets(QStringLiteral("Europe\t45\nOops\tnorth"), &why));
        QCOMPARE(why, QStringLiteral("That heading is not a number."));
        QVERIFY(!m_ctl->setRotorPresets(QStringLiteral("No heading here"), &why));
        QCOMPARE(why, QStringLiteral("That heading is not a number."));
        QCOMPARE(m_ctl->presets(), QStringLiteral("Europe\t45"));
    }
};

QTEST_GUILESS_MAIN(TestStationRotorController)
#include "tst_station_rotor_controller.moc"
