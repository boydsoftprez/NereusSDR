// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 15.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original test.
//
// Simulated FlexControl tokens and a simulated serial port (R-HK-15, spec 5 and 10). No
// case lists or opens a real port. A pass shows the code matches its sources, not what
// a device does.
#include <QtTest>

#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QPointer>
#include <QSignalSpy>
#include <QThread>
#include <QTimer>

#include <cmath>
#include <memory>

#include "core/AppSettings.h"
#include "core/hotkeys/HotKeyBindings.h"
#include "core/hotkeys/HotKeyDeviceBindings.h"
#include "device_parser_test_util.h"
#include "gui/hotkeys/ControllerHost.h"
#include "gui/hotkeys/HotKeyDispatcher.h"
#include "gui/hotkeys/HotKeyTargets.h"
#include "gui/hotkeys/devices/DeviceInput.h"
#include "gui/hotkeys/devices/FlexControlInput.h"
#include "gui/hotkeys/devices/FlexControlParser.h"
#include "models/SliceModel.h"

using namespace NereusSDR;
using parsertest::text;

namespace {

const QString kNotPlugged = QStringLiteral("Not plugged in");
const QString kNoSupport = QStringLiteral("This build has no serial port support.");
const QString kPortName = QStringLiteral("fake-flex-port");

// What the simulated ports are doing. A mutex, since some cases run the input on the
// host's worker.
struct FakePorts {
    mutable QMutex lock;
    bool plugged{false};
    bool refuse{false};
    FlexOpenFailure refusal{FlexOpenFailure::Denied};
    bool writesFail{false};
    QList<QPair<quint16, quint16>> finds;
    QStringList opens;
    QList<QByteArray> writes;
    int finishes{0};
    int closes{0};
    QList<int> delays;
    QPointer<FlexSerialHandle> live;

    QList<QByteArray> writesNow() const
    {
        const QMutexLocker locker(&lock);
        return writes;
    }
    QList<int> delaysNow() const
    {
        const QMutexLocker locker(&lock);
        return delays;
    }
    void setPlugged(bool on)
    {
        const QMutexLocker locker(&lock);
        plugged = on;
    }
    // On the handle's thread: at once if that's this one, otherwise queued.
    void send(const QByteArray& bytes)
    {
        const QPointer<FlexSerialHandle> handle = liveHandle();
        QVERIFY(handle);
        QMetaObject::invokeMethod(handle.data(), [handle, bytes] { emit handle->bytesArrived(bytes); });
    }
    void fail()
    {
        const QPointer<FlexSerialHandle> handle = liveHandle();
        QVERIFY(handle);
        QMetaObject::invokeMethod(handle.data(), [handle] { emit handle->failed(QStringLiteral("gone")); });
    }

private:
    QPointer<FlexSerialHandle> liveHandle() const
    {
        const QMutexLocker locker(&lock);
        return live;
    }
};

class FakeHandle : public FlexSerialHandle {
public:
    explicit FakeHandle(std::shared_ptr<FakePorts> ports)
        : m_ports(std::move(ports))
    {
    }
    bool write(const QByteArray& bytes) override
    {
        const QMutexLocker locker(&m_ports->lock);
        if (m_ports->writesFail) {
            return false;
        }
        m_ports->writes.append(bytes);
        return true;
    }
    void finishWrites() override
    {
        const QMutexLocker locker(&m_ports->lock);
        ++m_ports->finishes;
    }
    void closePort() override
    {
        const QMutexLocker locker(&m_ports->lock);
        ++m_ports->closes;
    }

private:
    std::shared_ptr<FakePorts> m_ports;
};

class FakeLayer : public FlexSerialLayer {
public:
    explicit FakeLayer(std::shared_ptr<FakePorts> ports)
        : m_ports(std::move(ports))
    {
    }
    QString findPort(quint16 vendorId, quint16 productId) override
    {
        const QMutexLocker locker(&m_ports->lock);
        m_ports->finds.append({vendorId, productId});
        return m_ports->plugged ? kPortName : QString();
    }
    std::unique_ptr<FlexSerialHandle> open(const QString& portName, FlexOpenFailure* failure, QString* detail) override
    {
        const QMutexLocker locker(&m_ports->lock);
        m_ports->opens.append(portName);
        if (m_ports->refuse) {
            *failure = m_ports->refusal;
            *detail = QStringLiteral("refused");
            return nullptr;
        }
        auto handle = std::make_unique<FakeHandle>(m_ports);
        m_ports->live = handle.get();
        return handle;
    }

private:
    std::shared_ptr<FakePorts> m_ports;
};

class FakeTargets : public HotKeyTargets {
public:
    SliceModel* window{nullptr};
    bool transmittingNow{false};
    qint64 clock{1000};
    QStringList sliceActions;
    QStringList transmitActions;
    QList<bool> keyHolds;

    SliceModel* windowSlice() const override { return window; }
    SliceModel* sliceByLetter(QChar) const override { return window; }
    QChar letterOf(const SliceModel*) const override { return QLatin1Char('A'); }
    QString sliceRefusal(const SliceModel*) const override { return {}; }
    bool listened(const SliceModel*) const override { return false; }
    bool tuneTo(SliceModel* slice, double hz) override
    {
        slice->setFrequency(hz);
        return true;
    }
    void runSliceAction(const QString& id, SliceModel*, int) override { sliceActions.append(id); }
    void runWindowAction(const QString&) override {}
    QString unavailableReason(const QString&) const override { return {}; }
    bool transmitting() const override { return transmittingNow; }
    bool tuneOrTwoToneOn() const override { return false; }
    bool keyHold(bool down) override
    {
        keyHolds.append(down);
        transmittingNow = down;
        return true;
    }
    void runTransmitAction(const QString& id) override { transmitActions.append(id); }
    void runCwMessage(const QString&) override {}
    void setCwKeyDown(const QString&, bool) override {}
    void toast(const QString&) override {}
    qint64 nowMs() const override { return clock; }
};

// The worker's state changes, for cases that drive the input on this thread.
int connectedOf(const QSignalSpy& spy, int at)
{
    return spy.at(at).at(1).toBool() ? 1 : 0;
}

void runDeferredDeletes()
{
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

} // namespace

class TestDeviceParserFlexControl : public QObject {
    Q_OBJECT

private:
    std::shared_ptr<FakePorts> m_ports;

    void useFakePorts()
    {
        const std::shared_ptr<FakePorts> ports = m_ports;
        FlexControlInput::setLayerFactoryForTest([ports] { return std::make_unique<FakeLayer>(ports); });
        FlexControlInput::setSchedulerForTest([ports](int delayMs) {
            const QMutexLocker locker(&ports->lock);
            ports->delays.append(delayMs);
        });
    }

private slots:
    void init()
    {
        m_ports = std::make_shared<FakePorts>();
        useFakePorts();
    }

    void cleanup()
    {
        FlexControlInput::setLayerFactoryForTest(nullptr);
        FlexControlInput::setSchedulerForTest(nullptr);
        runDeferredDeletes();
    }

    // ---- the parser ----

    void theWheelAndItsSpeeds_data()
    {
        QTest::addColumn<QByteArray>("token");
        QTest::addColumn<int>("steps");
        QTest::newRow("U") << QByteArrayLiteral("U;") << 1;
        QTest::newRow("U02") << QByteArrayLiteral("U02;") << 2;
        QTest::newRow("U06") << QByteArrayLiteral("U06;") << 6;
        // Upstream's code, which its header contradicts. See FlexControlParser.cpp.
        QTest::newRow("D") << QByteArrayLiteral("D;") << -1;
        QTest::newRow("D03") << QByteArrayLiteral("D03;") << -3;
        QTest::newRow("D06") << QByteArrayLiteral("D06;") << -6;
        QTest::newRow("no number reads as one") << QByteArrayLiteral("Uxx;") << 1;
    }

    void theWheelAndItsSpeeds()
    {
        QFETCH(QByteArray, token);
        QFETCH(int, steps);
        FlexControlParser parser;
        QCOMPARE(text(parser.feed(token)), QStringList{QStringLiteral("turn wheel %1").arg(steps)});
    }

    void aTokenInTwoReadsWaitsForItsEnd()
    {
        FlexControlParser parser;
        QVERIFY(parser.feed("U0").isEmpty());
        QCOMPARE(text(parser.feed("2;")), QStringList{"turn wheel 2"});
    }

    void severalTokensInOneReadAndARemainder()
    {
        FlexControlParser parser;
        QCOMPARE(text(parser.feed("U;D02;U")), (QStringList{"turn wheel 1", "turn wheel -2"}));
        QCOMPARE(text(parser.feed(";")), QStringList{"turn wheel 1"});
    }

    void spacesAndEmptyTokensAreIgnored()
    {
        FlexControlParser parser;
        QCOMPARE(text(parser.feed("\r\n U \r\n;;;")), QStringList{"turn wheel 1"});
    }

    void aLongRunWithNoEndClearsTheBuffer()
    {
        FlexControlParser parser;
        QVERIFY(parser.feed(QByteArray(300, 'x')).isEmpty());
        // Cleared: this is a clean token, not the end of a very long one.
        QCOMPARE(text(parser.feed("U;")), QStringList{"turn wheel 1"});
    }

    void theBufferLimitIs256Bytes()
    {
        FlexControlParser kept;
        QVERIFY(kept.feed(QByteArray("U") + QByteArray(255, ' ')).isEmpty());
        QCOMPARE(text(kept.feed(";")), QStringList{"turn wheel 1"});

        FlexControlParser cleared;
        QVERIFY(cleared.feed(QByteArray("U") + QByteArray(256, ' ')).isEmpty());
        QVERIFY(cleared.feed(";").isEmpty());
    }

    void buttonsAndTheirGestures_data()
    {
        QTest::addColumn<QByteArray>("token");
        QTest::addColumn<QString>("control");
        QTest::addColumn<ControlGesture>("gesture");
        QTest::newRow("X1S") << QByteArrayLiteral("X1S;") << "button1" << ControlGesture::Tap;
        QTest::newRow("X2C") << QByteArrayLiteral("X2C;") << "button2" << ControlGesture::DoubleTap;
        QTest::newRow("X3L") << QByteArrayLiteral("X3L;") << "button3" << ControlGesture::Hold;
        QTest::newRow("X4S is the knob") << QByteArrayLiteral("X4S;") << "knob" << ControlGesture::Tap;
        QTest::newRow("S") << QByteArrayLiteral("S;") << "knob" << ControlGesture::Tap;
        QTest::newRow("C") << QByteArrayLiteral("C;") << "knob" << ControlGesture::DoubleTap;
        QTest::newRow("L") << QByteArrayLiteral("L;") << "knob" << ControlGesture::Hold;
        QTest::newRow("anything else holds") << QByteArrayLiteral("X1Q;") << "button1" << ControlGesture::Hold;
    }

    void buttonsAndTheirGestures()
    {
        QFETCH(QByteArray, token);
        QFETCH(QString, control);
        QFETCH(ControlGesture, gesture);
        FlexControlParser parser;
        const QList<DeviceEvent> events = parser.feed(token);
        QCOMPARE(events.size(), 1);
        QCOMPARE(events.at(0).kind, DeviceEvent::Kind::TokenGesture);
        QCOMPARE(events.at(0).control, control);
        QCOMPARE(events.at(0).gesture, gesture);
    }

    void whatIsNotATokenGivesNothing()
    {
        FlexControlParser parser;
        QVERIFY(parser.feed("X5S;X0S;X1;X;Z;SS;").isEmpty());
        QVERIFY(!parser.takeDeviceReset());
    }

    void aDeviceResetIsNoControlEvent()
    {
        FlexControlParser parser;
        QVERIFY(!parser.takeDeviceReset());
        QVERIFY(parser.feed("F0304;").isEmpty());
        QVERIFY(parser.takeDeviceReset());
        QVERIFY(!parser.takeDeviceReset());
    }

    void resetDropsAPartToken()
    {
        FlexControlParser parser;
        parser.feed("U0");
        parser.reset();
        QVERIFY(parser.feed("2;").isEmpty());
    }

    void auxLights_data()
    {
        QTest::addColumn<int>("lit");
        QTest::addColumn<QByteArray>("report");
        QTest::newRow("none") << 0 << QByteArrayLiteral("I000;");
        QTest::newRow("aux 1") << 1 << QByteArrayLiteral("I100;");
        QTest::newRow("aux 2") << 2 << QByteArrayLiteral("I010;");
        QTest::newRow("aux 3") << 3 << QByteArrayLiteral("I001;");
        QTest::newRow("too high") << 4 << QByteArrayLiteral("I000;");
        QTest::newRow("too low") << -1 << QByteArrayLiteral("I000;");
    }

    void auxLights()
    {
        QFETCH(int, lit);
        QFETCH(QByteArray, report);
        QCOMPARE(FlexControlParser::auxLights(lit), report);
    }

    void theDefaultBindings()
    {
        AppSettings::instance().clear();
        const HotKeyDeviceBindings bindings;
        const auto action = [&bindings](const char* control, ControlGesture gesture) {
            return bindings.binding(DeviceId::FlexControl, QString::fromLatin1(control), gesture).actionId;
        };
        QCOMPARE(action("button2", ControlGesture::Tap), QStringLiteral("tx.mox"));
        QCOMPARE(action("button2", ControlGesture::DoubleTap), QStringLiteral("tx.tune"));
        QCOMPARE(action("button1", ControlGesture::Tap), QStringLiteral("step.larger"));
        QCOMPARE(action("knob", ControlGesture::DoubleTap), QStringLiteral("step.smaller"));
        QCOMPARE(action("button3", ControlGesture::Tap), QStringLiteral("mute"));
        QCOMPARE(action("button3", ControlGesture::DoubleTap), QStringLiteral("slice.lock"));
        QCOMPARE(action("wheel", ControlGesture::Turn), QStringLiteral("tune.up"));
    }

    void staysMarkedUntested()
    {
        QVERIFY(!HotKeyDeviceBindings::testedOnRealDevice(DeviceId::FlexControl));
    }

    // ---- the input, driven by hand on this thread ----

    void itLooksForItsOwnIdAndOpensThatPort()
    {
        m_ports->setPlugged(true);
        FlexControlInput input;
        QSignalSpy states(&input, &FlexControlInput::deviceStateChanged);
        input.start();
        QCOMPARE(m_ports->finds, (QList<QPair<quint16, quint16>>{{0x2192, 0x0010}}));
        QCOMPARE(m_ports->opens, QStringList{kPortName});
        QCOMPARE(states.size(), 2);     // not plugged in, then connected
        QCOMPARE(states.last().at(0).value<DeviceId>(), DeviceId::FlexControl);
        QCOMPARE(connectedOf(states, 1), 1);
        QVERIFY(states.last().at(2).toString().isEmpty());
        QVERIFY(m_ports->delays.isEmpty());
    }

    void itsTokensBecomeItsEvents()
    {
        m_ports->setPlugged(true);
        FlexControlInput input;
        QSignalSpy events(&input, &FlexControlInput::deviceEvent);
        input.start();
        m_ports->send("U0");
        QVERIFY(events.isEmpty());
        m_ports->send("2;X2C;");
        QCOMPARE(events.size(), 2);
        QCOMPARE(events.at(0).at(0).value<DeviceId>(), DeviceId::FlexControl);
        QCOMPARE(events.at(0).at(1).value<DeviceEvent>().value, 2);
        QCOMPARE(events.at(1).at(1).value<DeviceEvent>().control, QStringLiteral("button2"));
    }

    void aDeviceAbsentAtLaunchIsLookedForWithBackOff()
    {
        FlexControlInput input;
        QSignalSpy states(&input, &FlexControlInput::deviceStateChanged);
        input.start();
        QCOMPARE(m_ports->delays, QList<int>{2000});
        for (int i = 0; i < 6; ++i) {
            input.retryNow();
        }
        QCOMPARE(m_ports->delays, (QList<int>{2000, 4000, 8000, 16000, 30000, 30000, 30000}));
        QCOMPARE(m_ports->opens.size(), 0);
        QCOMPARE(states.size(), 1);     // said once, however many attempts
        QCOMPARE(states.at(0).at(2).toString(), kNotPlugged);

        m_ports->setPlugged(true);
        input.retryNow();
        QCOMPARE(connectedOf(states, 1), 1);
        QCOMPARE(m_ports->delays.size(), 7);    // connected, so no more attempts
    }

    void anUnplugClosesSaysSoAndRecovers()
    {
        m_ports->setPlugged(true);
        FlexControlInput input;
        QSignalSpy states(&input, &FlexControlInput::deviceStateChanged);
        input.start();
        const QPointer<FlexSerialHandle> first = m_ports->live;

        m_ports->setPlugged(false);
        m_ports->fail();
        QCOMPARE(m_ports->closes, 1);
        QCOMPARE(states.last().at(2).toString(), kNotPlugged);
        QCOMPARE(connectedOf(states, states.size() - 1), 0);
        runDeferredDeletes();
        QVERIFY(first.isNull());

        QCOMPARE(m_ports->delays, QList<int>{2000});
        input.retryNow();
        input.retryNow();
        QCOMPARE(m_ports->delays, (QList<int>{2000, 4000, 8000}));

        // Back on what may be another port: the id is looked for again each time.
        m_ports->setPlugged(true);
        const int writesBefore = m_ports->writes.size();
        input.retryNow();
        QCOMPARE(connectedOf(states, states.size() - 1), 1);
        QCOMPARE(m_ports->writes.size(), writesBefore + 1);     // lights written on open

        // Connected, so the next outage starts from the beginning.
        m_ports->delays.clear();
        m_ports->setPlugged(false);
        m_ports->fail();
        QCOMPARE(m_ports->delays, QList<int>{2000});
    }

    void aFailureFromAnOldPortChangesNothing()
    {
        m_ports->setPlugged(true);
        FlexControlInput input;
        input.start();
        const QPointer<FlexSerialHandle> first = m_ports->live;
        m_ports->fail();
        QVERIFY(first);     // waiting for its deferred delete
        input.retryNow();
        QVERIFY(m_ports->live != first);
        QSignalSpy states(&input, &FlexControlInput::deviceStateChanged);
        emit first->failed(QStringLiteral("late"));
        QVERIFY(states.isEmpty());
    }

    void onlyOneAttemptIsEverPending()
    {
        m_ports->setPlugged(true);
        FlexControlInput input;
        input.start();
        m_ports->fail();
        QCOMPARE(m_ports->delays, QList<int>{2000});
        input.setLit(2);    // no port: nothing to write, nothing more to arm
        QCOMPARE(m_ports->delays, QList<int>{2000});
    }

    void aRefusedOpenSaysWhyAndKeepsTrying()
    {
        m_ports->setPlugged(true);
        m_ports->refuse = true;
        FlexControlInput input;
        QSignalSpy states(&input, &FlexControlInput::deviceStateChanged);
        input.start();
        QCOMPARE(states.size(), 2);
        QCOMPARE(states.last().at(2).toString(), flexOpenFailureReason(FlexOpenFailure::Denied));
        QVERIFY(flexOpenFailureReason(FlexOpenFailure::Denied).contains(QStringLiteral("Another app is using it")));
        input.retryNow();
        QCOMPARE(states.size(), 2);     // the same reason isn't said twice
        QCOMPARE(m_ports->delays, (QList<int>{2000, 4000}));

        m_ports->refuse = false;
        input.retryNow();
        QCOMPARE(connectedOf(states, states.size() - 1), 1);
    }

    void openFailureWording()
    {
        QCOMPARE(flexOpenFailureReason(FlexOpenFailure::Gone), kNotPlugged);
        QCOMPARE(flexOpenFailureReason(FlexOpenFailure::Other),
                 QStringLiteral("It couldn't be opened. Unplug it and plug it back in."));
    }

    void aWedgedDeviceIsNeverAnnouncedConnected()
    {
        m_ports->setPlugged(true);
        m_ports->writesFail = true;
        FlexControlInput input;
        QSignalSpy states(&input, &FlexControlInput::deviceStateChanged);
        input.start();
        for (int i = 0; i < states.size(); ++i) {
            QCOMPARE(connectedOf(states, i), 0);
        }
        QCOMPARE(states.last().at(2).toString(), flexOpenFailureReason(FlexOpenFailure::Other));
        QCOMPARE(m_ports->closes, 1);
        input.retryNow();
        QCOMPARE(m_ports->delays, (QList<int>{2000, 4000}));    // still backing off
    }

    void aShortWriteInUseIsAnUnplug()
    {
        m_ports->setPlugged(true);
        FlexControlInput input;
        QSignalSpy states(&input, &FlexControlInput::deviceStateChanged);
        input.start();
        m_ports->writesFail = true;
        input.setLit(1);
        QCOMPARE(states.last().at(2).toString(), kNotPlugged);
        QCOMPARE(connectedOf(states, states.size() - 1), 0);
        QCOMPARE(m_ports->delays, QList<int>{2000});
    }

    void aWriteFailingInsideAReadIsSafe()
    {
        m_ports->setPlugged(true);
        FlexControlInput input;
        QSignalSpy events(&input, &FlexControlInput::deviceEvent);
        input.start();
        m_ports->writesFail = true;
        m_ports->send("U;F0304;D;");
        // The port went while it was being read. What had arrived still counts.
        QCOMPARE(events.size(), 2);
        QCOMPARE(m_ports->closes, 1);
        QCOMPARE(m_ports->delays, QList<int>{2000});
    }

    void lightsAreWrittenOnOpenResetAndChange()
    {
        m_ports->setPlugged(true);
        FlexControlInput input;
        input.setLit(3);        // before any port: nothing to write yet
        QVERIFY(m_ports->writes.isEmpty());
        input.start();
        QCOMPARE(m_ports->writes, QList<QByteArray>{"I001;"});
        input.setLit(2);
        QCOMPARE(m_ports->writes.last(), QByteArrayLiteral("I010;"));
        const int count = m_ports->writes.size();
        input.setLit(2);        // no change
        QCOMPARE(m_ports->writes.size(), count);
        m_ports->send("F0304;");
        QCOMPARE(m_ports->writes.size(), count + 1);
        QCOMPARE(m_ports->writes.last(), QByteArrayLiteral("I010;"));
        input.setLit(7);        // out of range lights none
        QCOMPARE(m_ports->writes.last(), QByteArrayLiteral("I000;"));
    }

    void stopTurnsTheLightsOffAndStopsLooking()
    {
        m_ports->setPlugged(true);
        FlexControlInput input;
        QSignalSpy states(&input, &FlexControlInput::deviceStateChanged);
        input.start();
        input.setLit(1);
        input.stop();
        QCOMPARE(m_ports->writes.last(), QByteArrayLiteral("I000;"));
        QCOMPARE(m_ports->finishes, 1);
        QCOMPARE(m_ports->closes, 1);
        QCOMPARE(connectedOf(states, states.size() - 1), 0);
        QVERIFY(states.last().at(2).toString().isEmpty());
        const int finds = m_ports->finds.size();
        input.retryNow();       // a retry already on its way finds nothing wanted
        QCOMPARE(m_ports->finds.size(), finds);
        QVERIFY(m_ports->delays.isEmpty());
    }

    void stopCancelsAnAttemptAndStartBeginsAgain()
    {
        FlexControlInput input;
        input.start();
        input.retryNow();
        input.retryNow();
        input.stop();
        m_ports->delays.clear();
        input.start();
        QCOMPARE(m_ports->delays, QList<int>{2000});
    }

    void closingTheProgramTurnsTheLightsOff()
    {
        m_ports->setPlugged(true);
        {
            FlexControlInput input;
            input.start();
            input.setLit(1);
        }
        QCOMPARE(m_ports->writes.last(), QByteArrayLiteral("I000;"));
        QCOMPARE(m_ports->finishes, 1);
        QCOMPARE(m_ports->closes, 1);
    }

    // ---- a build without Qt SerialPort, a test process, the real ports ----

    void noSerialSupportSaysSoAndLooksForNothing()
    {
        // This is the layer a build without HAVE_SERIALPORT makes. Nothing else differs.
        FlexControlInput::setLayerFactoryForTest([] { return std::unique_ptr<FlexSerialLayer>(); });
        FlexControlInput input;
        QSignalSpy states(&input, &FlexControlInput::deviceStateChanged);
        input.start();
        QCOMPARE(states.size(), 1);
        QCOMPARE(connectedOf(states, 0), 0);
        QCOMPARE(states.at(0).at(2).toString(), kNoSupport);
        QVERIFY(m_ports->delays.isEmpty());
        input.retryNow();
        input.setLit(2);
        input.stop();
        QCOMPARE(states.size(), 1);
    }

    void aTestProcessNeverReachesARealPort()
    {
        QVERIFY(!DeviceInput::realDevicesAllowed());
        FlexControlInput::setLayerFactoryForTest(nullptr);
        FlexControlInput input;
        QSignalSpy states(&input, &FlexControlInput::deviceStateChanged);
        input.start();
        QCOMPARE(states.size(), 1);
#ifdef HAVE_SERIALPORT
        QVERIFY(FlexControlInput::supported());
        QCOMPARE(states.at(0).at(2).toString(), kNotPlugged);
#else
        QVERIFY(!FlexControlInput::supported());
        QCOMPARE(states.at(0).at(2).toString(), kNoSupport);
#endif
        input.stop();
    }

    void theRealPortsAreBehindTheApplicationsSwitch()
    {
        // The other half of the case above, which can't see a port not being listed.
        // Qt's serial classes are named only inside the guarded real layer.
        QFile source(QStringLiteral(NEREUS_SOURCE_DIR "/src/gui/hotkeys/devices/FlexControlInput.cpp"));
        QVERIFY(source.open(QIODevice::ReadOnly));
        const QByteArray text = source.readAll();
        const qsizetype guard = text.indexOf("if (!DeviceInput::realDevicesAllowed())");
        const qsizetype real = text.indexOf("std::make_unique<RealSerialLayer>()");
        QVERIFY(guard >= 0);
        QVERIFY(guard < real);
        QCOMPARE(text.count("std::make_unique<RealSerialLayer>()"), 1);
        const qsizetype layerEnd = text.indexOf("#endif // HAVE_SERIALPORT");
        QVERIFY(layerEnd >= 0);
        QVERIFY(text.lastIndexOf("QSerialPort") < layerEnd);
        QVERIFY(text.indexOf("QSerialPort") > text.indexOf("#ifdef HAVE_SERIALPORT"));
    }

    // ---- through the host and its worker ----

    void theHostRunsItOnTheUsbWorkerWithItsTimer()
    {
        AppSettings::instance().clear();
        HotKeyBindings bindings;
        FakeTargets targets;
        HotKeyDispatcher dispatcher(&bindings, &targets);
        ControllerHost host(&dispatcher, &bindings);
        FlexControlInput* const flex = host.flex();
        QVERIFY(flex != nullptr);
        QCOMPARE(flex->thread(), host.usb()->thread());
        QVERIFY(flex->thread() != QThread::currentThread());
        QTRY_COMPARE(host.deviceReason(DeviceId::FlexControl), kNotPlugged);

        // Upstream's trap: a timer left on the main thread can't be started from the worker.
        int timers = 0;
        bool allOnWorker = true;
        QMetaObject::invokeMethod(
            flex,
            [flex, &timers, &allOnWorker] {
                const QList<QTimer*> children = flex->findChildren<QTimer*>();
                timers = static_cast<int>(children.size());
                for (const QTimer* timer : children) {
                    allOnWorker = allOnWorker && timer->thread() == QThread::currentThread();
                }
            },
            Qt::BlockingQueuedConnection);
        QCOMPARE(timers, 1);
        QVERIFY(allOnWorker);
    }

    void tokensReachTheBindingsAndAnUnplugDropsTheHold()
    {
        AppSettings::instance().clear();
        m_ports->setPlugged(true);
        HotKeyBindings bindings;
        FakeTargets targets;
        HotKeyDispatcher dispatcher(&bindings, &targets);
        SliceModel slice(0);
        constexpr double kStartHz = 14'000'000.0;
        slice.setStepHz(100);
        slice.setFrequency(kStartHz);
        targets.window = &slice;
        ControllerHost host(&dispatcher, &bindings);
        QTRY_VERIFY(host.deviceConnected(DeviceId::FlexControl));
        QCOMPARE(host.deviceReason(DeviceId::FlexControl), QString());

        // Button 2's tap and double tap, as the default table has them.
        m_ports->send("X2S;");
        QTRY_COMPARE(targets.transmitActions, QStringList{QStringLiteral("tx.mox")});
        m_ports->send("X2C;");
        QTRY_COMPARE(targets.transmitActions, (QStringList{QStringLiteral("tx.mox"), QStringLiteral("tx.tune")}));
        m_ports->send("X3S;");
        QTRY_COMPARE(targets.sliceActions, QStringList{QStringLiteral("mute")});
        m_ports->send("S;");
        QTRY_COMPARE(targets.sliceActions, (QStringList{QStringLiteral("mute"), QStringLiteral("step.larger")}));

        m_ports->send("U03;");
        QTRY_VERIFY([&] {
            targets.clock += 100;
            dispatcher.flushTune();
            return std::lround((slice.frequency() - kStartHz) / slice.stepHz()) == 3;
        }());

        // No FlexControl token can start a hold, since no release follows one. Any hold
        // under its source still has to drop.
        const HotKeySource source = ControllerHost::deviceSource(DeviceId::FlexControl);
        dispatcher.press(QStringLiteral("tx.hold"), source, {});
        QCOMPARE(targets.keyHolds, QList<bool>{true});

        m_ports->setPlugged(false);
        m_ports->fail();
        QTRY_COMPARE(targets.keyHolds, (QList<bool>{true, false}));
        QVERIFY(!host.deviceConnected(DeviceId::FlexControl));
        QCOMPARE(host.deviceReason(DeviceId::FlexControl), kNotPlugged);
        QCOMPARE(m_ports->delaysNow(), QList<int>{2000});

        // Recovery on the worker, by the scheduler's word.
        m_ports->setPlugged(true);
        QMetaObject::invokeMethod(host.flex(), &FlexControlInput::retryNow, Qt::QueuedConnection);
        QTRY_VERIFY(host.deviceConnected(DeviceId::FlexControl));
    }

    void shuttingDownTurnsTheLightsOffOnTheWorker()
    {
        AppSettings::instance().clear();
        m_ports->setPlugged(true);
        HotKeyBindings bindings;
        FakeTargets targets;
        HotKeyDispatcher dispatcher(&bindings, &targets);
        {
            ControllerHost host(&dispatcher, &bindings);
            QTRY_VERIFY(host.deviceConnected(DeviceId::FlexControl));
            QMetaObject::invokeMethod(host.flex(), [flex = host.flex()] { flex->setLit(2); }, Qt::QueuedConnection);
            QTRY_COMPARE(m_ports->writesNow().last(), QByteArrayLiteral("I010;"));
        }
        QCOMPARE(m_ports->writesNow().last(), QByteArrayLiteral("I000;"));
        const QMutexLocker locker(&m_ports->lock);
        QCOMPARE(m_ports->closes, 1);
    }
};

QTEST_MAIN(TestDeviceParserFlexControl)
#include "tst_device_parser_flexcontrol.moc"
