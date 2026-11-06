// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 12.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original test.
//
// The USB worker through a simulated reader, and device routing into the dispatcher
// (R-HK-02, R-HK-08, spec 7.6 and 10). Nobody here has any of these devices, so a
// pass shows the code does what its sources say, not what a device does.
#include <QtTest>

#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QThread>

#include <algorithm>
#include <cmath>
#include <deque>
#include <memory>

#include "core/AppSettings.h"
#include "core/hotkeys/HotKeyActionList.h"
#include "core/hotkeys/HotKeyBindings.h"
#include "gui/hotkeys/ControllerHost.h"
#include "gui/hotkeys/HotKeyDispatcher.h"
#include "gui/hotkeys/HotKeyTargets.h"
#include "gui/hotkeys/devices/DeviceInput.h"
#include "gui/hotkeys/devices/GestureEngine.h"
#include "gui/hotkeys/devices/UsbPresence.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

using Kind = DeviceEvent::Kind;

// Local copies: QCOMPARE binds a reference, and MinGW can't import an exported
// class's static constexpr by address (CONTRIBUTING, Windows DLL Boundaries).
constexpr int kHoldMs = GestureEngine::kHoldMs;
constexpr int kPollMs = DeviceInput::kPollMs;
constexpr int kPresenceMs = DeviceInput::kPresenceMs;

constexpr quint16 kRc28Vid = 0x0C26;
constexpr quint16 kRc28Pid = 0x001E;
constexpr quint16 kEmuVid = 0x2341;
constexpr quint16 kEmuPid = 0x0266;
constexpr quint16 kKnobVid = 0x077D;
constexpr quint16 kKnobPid = 0x0410;

const QString kNotPlugged = QStringLiteral("Not plugged in");

// What the simulated USB bus and its devices are doing, shared with the layer the
// worker owns. A mutex, since one case runs the worker on its own thread.
struct FakeBus {
    mutable QMutex lock;
    QSet<quint32> present;
    QSet<quint32> refused;
    QString refusal;
    QList<quint32> opens;
    int presenceChecks{0};
    std::deque<QByteArray> reports;
    bool readFails{false};
    int closes{0};
    QList<QByteArray> writes;
    int parserResets{0};

    void plug(quint16 vid, quint16 pid)
    {
        const QMutexLocker locker(&lock);
        present.insert(UsbPresence::key(vid, pid));
    }
    void unplug(quint16 vid, quint16 pid)
    {
        const QMutexLocker locker(&lock);
        present.remove(UsbPresence::key(vid, pid));
        readFails = true;
    }
    void send(const QByteArray& report)
    {
        const QMutexLocker locker(&lock);
        reports.push_back(report);
    }
};

class FakeHandle : public HidDeviceHandle {
public:
    explicit FakeHandle(std::shared_ptr<FakeBus> bus)
        : m_bus(std::move(bus))
    {
    }
    ~FakeHandle() override
    {
        const QMutexLocker locker(&m_bus->lock);
        ++m_bus->closes;
    }
    int read(char* buffer, int size) override
    {
        const QMutexLocker locker(&m_bus->lock);
        if (m_bus->readFails) {
            return -1;
        }
        if (m_bus->reports.empty()) {
            return 0;
        }
        const QByteArray report = m_bus->reports.front();
        m_bus->reports.pop_front();
        const int n = std::min(size, static_cast<int>(report.size()));
        std::copy_n(report.constData(), n, buffer);
        return n;
    }
    int write(const QByteArray& report) override
    {
        const QMutexLocker locker(&m_bus->lock);
        m_bus->writes.append(report);
        return static_cast<int>(report.size());
    }

private:
    std::shared_ptr<FakeBus> m_bus;
};

class FakeLayer : public HidLayer {
public:
    explicit FakeLayer(std::shared_ptr<FakeBus> bus)
        : m_bus(std::move(bus))
    {
    }
    QSet<quint32> present() override
    {
        const QMutexLocker locker(&m_bus->lock);
        ++m_bus->presenceChecks;
        return m_bus->present;
    }
    std::unique_ptr<HidDeviceHandle> open(quint16 vid, quint16 pid, QString* error) override
    {
        const QMutexLocker locker(&m_bus->lock);
        const quint32 key = UsbPresence::key(vid, pid);
        m_bus->opens.append(key);
        if (m_bus->refused.contains(key)) {
            *error = m_bus->refusal;
            return nullptr;
        }
        m_bus->readFails = false;
        return std::make_unique<FakeHandle>(m_bus);
    }

private:
    std::shared_ptr<FakeBus> m_bus;
};

// One byte per report: 'd' a down on "f1", 'u' its up, anything else a step.
class FakeParser : public DeviceParser {
public:
    explicit FakeParser(std::shared_ptr<FakeBus> bus)
        : m_bus(std::move(bus))
    {
    }
    QList<DeviceEvent> parse(QByteArrayView report) override
    {
        QList<DeviceEvent> events;
        for (const char c : report) {
            if (c == 'd') {
                events.append(DeviceEvent{Kind::ButtonDown, QStringLiteral("f1")});
            } else if (c == 'u') {
                events.append(DeviceEvent{Kind::ButtonUp, QStringLiteral("f1")});
            } else {
                events.append(DeviceEvent{Kind::Rotate, QStringLiteral("wheel"), 1});
            }
        }
        return events;
    }
    void reset() override
    {
        const QMutexLocker locker(&m_bus->lock);
        ++m_bus->parserResets;
    }

private:
    std::shared_ptr<FakeBus> m_bus;
};

class FakeTargets : public HotKeyTargets {
public:
    SliceModel* window{nullptr};
    bool transmittingNow{false};
    bool refuseKey{false};
    qint64 clock{1000};

    struct SliceCall {
        QString id;
        int steps;
    };
    QList<SliceCall> sliceActions;
    QList<double> tunes;
    QStringList transmitActions;
    QList<bool> keyHolds;
    QList<QPair<QString, bool>> cwKeys;
    QStringList windowActions;
    mutable QList<QChar> lettersAsked;

    SliceModel* windowSlice() const override { return window; }
    SliceModel* sliceByLetter(QChar letter) const override
    {
        lettersAsked.append(letter);
        return window;
    }
    QChar letterOf(const SliceModel*) const override { return QLatin1Char('A'); }
    QString sliceRefusal(const SliceModel*) const override { return {}; }
    bool listened(const SliceModel*) const override { return false; }
    bool tuneTo(SliceModel* slice, double hz) override
    {
        tunes.append(hz);
        slice->setFrequency(hz);
        return true;
    }
    void runSliceAction(const QString& id, SliceModel*, int steps) override { sliceActions.append({id, steps}); }
    void runWindowAction(const QString& id) override { windowActions.append(id); }
    QString unavailableReason(const QString&) const override { return {}; }
    bool transmitting() const override { return transmittingNow; }
    bool tuneOrTwoToneOn() const override { return false; }
    bool keyHold(bool down) override
    {
        if (down && refuseKey) {
            return false;
        }
        keyHolds.append(down);
        transmittingNow = down;
        return true;
    }
    void runTransmitAction(const QString& id) override { transmitActions.append(id); }
    void runCwMessage(const QString&) override {}
    void setCwKeyDown(const QString& id, bool down) override { cwKeys.append({id, down}); }
    void toast(const QString&) override {}
    qint64 nowMs() const override { return clock; }

    QStringList sliceIds() const
    {
        QStringList ids;
        for (const SliceCall& call : sliceActions) {
            ids.append(call.id);
        }
        return ids;
    }
    int volumeSteps() const
    {
        int sum = 0;
        for (const SliceCall& call : sliceActions) {
            if (call.id == QLatin1String("volume.up")) {
                sum += call.steps;
            } else if (call.id == QLatin1String("volume.down")) {
                sum -= call.steps;
            }
        }
        return sum;
    }
};

DeviceEvent down(const char* control)
{
    return DeviceEvent{Kind::ButtonDown, QString::fromLatin1(control)};
}

DeviceEvent up(const char* control)
{
    return DeviceEvent{Kind::ButtonUp, QString::fromLatin1(control)};
}

DeviceEvent rotate(const char* control, int steps)
{
    return DeviceEvent{Kind::Rotate, QString::fromLatin1(control), steps};
}

DeviceEvent ring(int position)
{
    return DeviceEvent{Kind::RingPosition, QStringLiteral("ring"), position};
}

DeviceEvent token(const char* control, ControlGesture gesture)
{
    return DeviceEvent{Kind::TokenGesture, QString::fromLatin1(control), 0, gesture};
}

const HotKeyDeviceBinding kVolumeKnob{QStringLiteral("volume.up"), QStringLiteral("volume.down"), {}};

} // namespace

class TestDeviceInput : public QObject {
    Q_OBJECT

private:
    // The bus for a DeviceInput a case drives by hand, and the one the host's own
    // worker sees. Kept apart, or the worker's 3 s check could open what a case plugs in.
    std::shared_ptr<FakeBus> m_bus;
    std::shared_ptr<FakeBus> m_hostBus;
    std::unique_ptr<HotKeyBindings> m_bindings;
    std::unique_ptr<FakeTargets> m_targets;
    std::unique_ptr<HotKeyDispatcher> m_dispatcher;
    std::unique_ptr<SliceModel> m_slice;
    std::unique_ptr<ControllerHost> m_host;
    qint64 m_deviceNow{0};

    FakeTargets& t() { return *m_targets; }
    ControllerHost& host() { return *m_host; }
    HotKeyDeviceBindings& deviceBindings() { return *m_host->deviceBindings(); }

    int openCount() const
    {
        const QMutexLocker locker(&m_bus->lock);
        return m_bus->opens.size();
    }

    // Turn on the dispatcher's flush timer's work without waiting for it.
    void flushTurns()
    {
        t().clock += 100;
        m_dispatcher->flushTune();
    }

    int tuneSteps()
    {
        flushTurns();
        const double step = m_slice->stepHz();
        return static_cast<int>(std::lround((m_slice->frequency() - kStartHz) / step));
    }

    static constexpr double kStartHz = 14'000'000.0;

    // An RC-28 and a PowerMate, with parsers, as the table will have them.
    static QList<DeviceSpec> tableOn(const std::shared_ptr<FakeBus>& bus)
    {
        const auto parser = [bus] { return std::make_unique<FakeParser>(bus); };
        return {
            DeviceSpec{DeviceId::Rc28, kRc28Vid, kRc28Pid, 32, parser},
            DeviceSpec{DeviceId::Rc28, kEmuVid, kEmuPid, 32, parser},
            DeviceSpec{DeviceId::PowerMate, kKnobVid, kKnobPid, 6, parser},
        };
    }

    static void useFakeUsb(const std::shared_ptr<FakeBus>& bus)
    {
        DeviceInput::setLayerFactoryForTest([bus] { return std::make_unique<FakeLayer>(bus); });
        DeviceInput::setDeviceTableForTest(tableOn(bus));
    }

private slots:
    void init()
    {
        AppSettings::instance().clear();
        m_bus = std::make_shared<FakeBus>();
        m_hostBus = std::make_shared<FakeBus>();
        useFakeUsb(m_hostBus);
        m_bindings = std::make_unique<HotKeyBindings>();
        m_targets = std::make_unique<FakeTargets>();
        m_dispatcher = std::make_unique<HotKeyDispatcher>(m_bindings.get(), m_targets.get());
        m_slice = std::make_unique<SliceModel>(0);
        m_slice->setStepHz(100);
        m_slice->setFrequency(kStartHz);
        t().window = m_slice.get();
        m_host = std::make_unique<ControllerHost>(m_dispatcher.get(), m_bindings.get());
        useFakeUsb(m_bus);
        m_deviceNow = 0;
        m_host->setDeviceClockForTest([this] { return m_deviceNow; });
    }

    void cleanup()
    {
        m_host.reset();
        m_dispatcher.reset();
        m_targets.reset();
        m_slice.reset();
        m_bindings.reset();
        DeviceInput::setLayerFactoryForTest(nullptr);
        DeviceInput::clearDeviceTableForTest();
        AppSettings::instance().clear();
    }

    // ---- the worker, driven by hand on this thread ----

    void intervalsAreUpstreams()
    {
        QCOMPARE(kPollMs, 5);
        QCOMPARE(kPresenceMs, 3000);
    }

    void startOpensNothingNotOnTheBus()
    {
        DeviceInput input;
        QSignalSpy states(&input, &DeviceInput::deviceStateChanged);
        input.start();
        input.checkPresence();
        input.poll();
        QCOMPARE(openCount(), 0);
        QVERIFY(m_bus->presenceChecks >= 2);
        // Every device says so, once, however many checks run.
        QCOMPARE(states.size(), 2);
        QCOMPARE(states.at(0).at(2).toString(), kNotPlugged);
    }

    void aDeviceWithNoParserIsNeverOpened()
    {
        DeviceInput::setDeviceTableForTest({DeviceSpec{DeviceId::PowerMate, kKnobVid, kKnobPid, 6, nullptr}});
        DeviceInput input;
        m_bus->plug(kKnobVid, kKnobPid);
        input.start();
        QCOMPARE(openCount(), 0);
    }

    void opensOnlyWhatIsOnTheBus()
    {
        DeviceInput input;
        QSignalSpy states(&input, &DeviceInput::deviceStateChanged);
        m_bus->plug(kKnobVid, kKnobPid);
        input.start();
        QCOMPARE(m_bus->opens, QList<quint32>{UsbPresence::key(kKnobVid, kKnobPid)});
        const auto lastFor = [&states](DeviceId id) {
            for (qsizetype i = states.size() - 1; i >= 0; --i) {
                if (states.at(i).at(0).value<DeviceId>() == id) {
                    return states.at(i);
                }
            }
            return QVariantList{};
        };
        QVERIFY(lastFor(DeviceId::PowerMate).at(1).toBool());
        QVERIFY(lastFor(DeviceId::PowerMate).at(2).toString().isEmpty());
        QVERIFY(!lastFor(DeviceId::Rc28).at(1).toBool());
    }

    void oneOpenPerDeviceFirstMatchWins()
    {
        DeviceInput input;
        m_bus->plug(kEmuVid, kEmuPid);
        m_bus->plug(kRc28Vid, kRc28Pid);
        m_bus->plug(kKnobVid, kKnobPid);
        input.start();
        input.checkPresence();
        // One RC-28 at its first id, and the PowerMate at the same time.
        QCOMPARE(m_bus->opens, (QList<quint32>{UsbPresence::key(kRc28Vid, kRc28Pid),
                                               UsbPresence::key(kKnobVid, kKnobPid)}));
    }

    void aReportBecomesEvents()
    {
        DeviceInput input;
        QSignalSpy events(&input, &DeviceInput::deviceEvent);
        m_bus->plug(kKnobVid, kKnobPid);
        input.start();
        m_bus->send(QByteArrayLiteral("du"));   // both in one report
        m_bus->send(QByteArrayLiteral("x"));
        input.poll();
        QCOMPARE(events.size(), 3);
        QCOMPARE(events.at(0).at(0).value<DeviceId>(), DeviceId::PowerMate);
        QCOMPARE(events.at(0).at(1).value<DeviceEvent>().kind, Kind::ButtonDown);
        QCOMPARE(events.at(1).at(1).value<DeviceEvent>().kind, Kind::ButtonUp);
        QCOMPARE(events.at(2).at(1).value<DeviceEvent>().kind, Kind::Rotate);
    }

    void aDeviceThatNeverStopsCantHoldTheWorker()
    {
        DeviceInput input;
        m_bus->plug(kKnobVid, kKnobPid);
        input.start();
        for (int i = 0; i < 200; ++i) {
            m_bus->send(QByteArrayLiteral("x"));
        }
        input.poll();
        QVERIFY(!m_bus->reports.empty());
    }

    void readFailureIsAnUnplugAndItComesBack()
    {
        DeviceInput input;
        QSignalSpy states(&input, &DeviceInput::deviceStateChanged);
        m_bus->plug(kKnobVid, kKnobPid);
        input.start();
        states.clear();

        m_bus->unplug(kKnobVid, kKnobPid);
        input.poll();
        QCOMPARE(states.size(), 1);
        QCOMPARE(states.at(0).at(0).value<DeviceId>(), DeviceId::PowerMate);
        QVERIFY(!states.at(0).at(1).toBool());
        QCOMPARE(states.at(0).at(2).toString(), kNotPlugged);
        QCOMPARE(m_bus->parserResets, 1);
        QCOMPARE(m_bus->closes, 1);

        // Still gone: quiet, and no open tried.
        input.checkPresence();
        QCOMPARE(states.size(), 1);
        QCOMPARE(openCount(), 1);

        m_bus->plug(kKnobVid, kKnobPid);
        input.checkPresence();
        QCOMPARE(openCount(), 2);
        QCOMPARE(states.size(), 2);
        QVERIFY(states.at(1).at(1).toBool());
    }

    void aPermissionFailureGivesTheFixAndStopsNothingElse()
    {
        m_bus->refused.insert(UsbPresence::key(kRc28Vid, kRc28Pid));
        // The text hidapi passes on from macOS for a device access was refused to.
        m_bus->refusal = QStringLiteral("hid_open_path: failed to open IOHIDDevice from mach entry: (0xE00002E2) "
                                        "(iokit/common) not permitted");
        DeviceInput input;
        QSignalSpy states(&input, &DeviceInput::deviceStateChanged);
        m_bus->plug(kRc28Vid, kRc28Pid);
        m_bus->plug(kKnobVid, kKnobPid);
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("Couldn't open")));
        input.start();

        QString rc28Reason;
        bool knobConnected = false;
        for (const QVariantList& state : std::as_const(states)) {
            if (state.at(0).value<DeviceId>() == DeviceId::Rc28) {
                rc28Reason = state.at(2).toString();
            } else {
                knobConnected = state.at(1).toBool();
            }
        }
        QCOMPARE(rc28Reason, deviceOpenFailureReason(m_bus->refusal));
        QVERIFY(rc28Reason != kNotPlugged);
        QVERIFY(knobConnected);

        // It is tried again, and says nothing new. No second warning either.
        const int before = states.size();
        input.checkPresence();
        QCOMPARE(states.size(), before);
        QCOMPARE(m_bus->opens.count(UsbPresence::key(kRc28Vid, kRc28Pid)), 2);

        // The fix made: it connects by itself.
        m_bus->refused.clear();
        input.checkPresence();
        QVERIFY(states.last().at(1).toBool());
    }

    void failureWordsGiveTheFix()
    {
        const QString busy = deviceOpenFailureReason(QStringLiteral("(iokit/common) exclusive access and device already open"));
        QVERIFY(busy.contains(QStringLiteral("Another app")));
        QCOMPARE(deviceOpenFailureReason(QStringLiteral("Failed to open a device with path 'x': Device or resource busy")), busy);
        QCOMPARE(deviceOpenFailureReason(QStringLiteral("open_device: (0x00000005) Access is denied.")), busy);
        const QString blocked = deviceOpenFailureReason(QStringLiteral("Failed to open a device with path 'x': Permission denied"));
#if defined(Q_OS_MAC)
        QVERIFY(blocked.contains(QStringLiteral("Input Monitoring")));
#else
        QVERIFY(blocked.contains(QStringLiteral("60-nereussdr-controllers.rules")));
#endif
        QVERIFY(deviceOpenFailureReason(QStringLiteral("something else")).contains(QStringLiteral("plug it back in")));
        // Never empty: an empty reason reads as connected.
        QVERIFY(!deviceOpenFailureReason(QString()).isEmpty());
    }

    void outputGoesToTheOpenDeviceOnly()
    {
        DeviceInput input;
        m_bus->plug(kKnobVid, kKnobPid);
        input.start();
        input.sendOutput(DeviceId::PowerMate, QByteArrayLiteral("lights"));
        input.sendOutput(DeviceId::Rc28, QByteArrayLiteral("nobody"));
        QCOMPARE(m_bus->writes, QList<QByteArray>{QByteArrayLiteral("lights")});
    }

    void stopClosesAndStartOpensAgain()
    {
        DeviceInput input;
        m_bus->plug(kKnobVid, kKnobPid);
        input.start();
        input.stop();
        QCOMPARE(m_bus->closes, 1);
        input.checkPresence();
        QCOMPARE(openCount(), 1);
        input.start();
        QCOMPARE(openCount(), 2);
    }

    void aBuildWithNoHidapiListsDevicesWithTheReason()
    {
        DeviceInput::setLayerFactoryForTest([] { return std::unique_ptr<HidLayer>(); });
        DeviceInput input;
        QSignalSpy states(&input, &DeviceInput::deviceStateChanged);
        input.start();
        input.checkPresence();
        QCOMPARE(states.size(), 2);
        QCOMPARE(states.at(0).at(2).toString(), QStringLiteral("This build has no USB device support."));
    }

    void theRealTableListsTheUsbDevices()
    {
        // The real layer. No row has a parser, so it opens nothing even with one plugged in.
        DeviceInput::setLayerFactoryForTest(nullptr);
        DeviceInput::clearDeviceTableForTest();
        DeviceInput input;
        QSignalSpy states(&input, &DeviceInput::deviceStateChanged);
        input.start();
        QCOMPARE(states.size(), 6);     // FlexControl and the Ulanzi Dial have inputs of their own
        input.stop();
    }

    void realPresenceReadsTheBusWithoutHid()
    {
        // Runs for real. It opens nothing, so nothing here can raise a prompt.
        const QSet<quint32> present = UsbPresence::connectedVidPids();
        Q_UNUSED(present);
#if defined(Q_OS_MAC)
        QFile source(QStringLiteral(NEREUS_SOURCE_DIR "/src/gui/hotkeys/devices/UsbPresence.cpp"));
        QVERIFY(source.open(QIODevice::ReadOnly));
        const QByteArray text = source.readAll();
        QVERIFY(text.contains("IOUSBHostDevice"));
        QVERIFY(!text.contains("IOKit/hid"));
        QVERIFY(!text.contains("IOHIDManager"));
#endif
    }

    // ---- gestures and routing ----

    void pressAndHoldOnRc28F1()
    {
        // Let go at 200 ms: the press row.
        host().onDeviceEvent(DeviceId::Rc28, down("f1"));
        m_deviceNow = 200;
        host().onGestureTick();
        QVERIFY(t().sliceActions.isEmpty());
        host().onDeviceEvent(DeviceId::Rc28, up("f1"));
        QCOMPARE(t().sliceIds(), QStringList{QStringLiteral("step.larger")});

        // Held past the time: the hold row, once, and its release does nothing.
        t().sliceActions.clear();
        host().onDeviceEvent(DeviceId::Rc28, down("f1"));
        m_deviceNow += kHoldMs;
        host().onGestureTick();
        QCOMPARE(t().sliceIds(), QStringList{QStringLiteral("slice.lock")});
        m_deviceNow += 1000;
        host().onGestureTick();
        host().onDeviceEvent(DeviceId::Rc28, up("f1"));
        QCOMPARE(t().sliceIds(), QStringList{QStringLiteral("slice.lock")});
    }

    void aControlWithNoHoldBindingActsAtOnce()
    {
        host().onDeviceEvent(DeviceId::PowerMate, down("press"));
        QCOMPARE(t().sliceIds(), QStringList{QStringLiteral("mute")});
        host().onDeviceEvent(DeviceId::PowerMate, up("press"));
        QCOMPARE(t().sliceIds(), QStringList{QStringLiteral("mute")});
    }

    void aHoldActionPressesOnDownAndReleasesOnUp()
    {
        host().onDeviceEvent(DeviceId::Rc28, down("tx"));
        QCOMPARE(t().keyHolds, QList<bool>{true});
        host().onDeviceEvent(DeviceId::Rc28, up("tx"));
        QCOMPARE(t().keyHolds, (QList<bool>{true, false}));
    }

    void aHoldActionOnTheHoldRowKeysUntilLetGo()
    {
        deviceBindings().setBinding(DeviceId::Rc28, QStringLiteral("f1"), ControlGesture::Hold,
                                    HotKeyDeviceBinding{QStringLiteral("tx.hold"), {}, {}});
        host().onDeviceEvent(DeviceId::Rc28, down("f1"));
        QVERIFY(t().keyHolds.isEmpty());
        m_deviceNow = kHoldMs;
        host().onGestureTick();
        QCOMPARE(t().keyHolds, QList<bool>{true});
        host().onDeviceEvent(DeviceId::Rc28, up("f1"));
        QCOMPARE(t().keyHolds, (QList<bool>{true, false}));
    }

    void aHoldActionOnThePressRowNeverWaitsForTheEngine()
    {
        // The press row keys for as long as it is down, so no blip at the release,
        // and the hold row can't run.
        deviceBindings().setBinding(DeviceId::Rc28, QStringLiteral("f1"), ControlGesture::Press,
                                    HotKeyDeviceBinding{QStringLiteral("tx.hold"), {}, {}});
        host().onDeviceEvent(DeviceId::Rc28, down("f1"));
        QCOMPARE(t().keyHolds, QList<bool>{true});
        m_deviceNow = kHoldMs * 2;
        host().onGestureTick();
        QVERIFY(t().sliceActions.isEmpty());
        host().onDeviceEvent(DeviceId::Rc28, up("f1"));
        QCOMPARE(t().keyHolds, (QList<bool>{true, false}));
    }

    void aDeviceDecidedGestureRunsItsRow()
    {
        host().onDeviceEvent(DeviceId::FlexControl, token("button3", ControlGesture::Tap));
        host().onDeviceEvent(DeviceId::FlexControl, token("button3", ControlGesture::DoubleTap));
        QCOMPARE(t().sliceIds(), (QStringList{QStringLiteral("mute"), QStringLiteral("slice.lock")}));
        host().onDeviceEvent(DeviceId::FlexControl, token("button2", ControlGesture::Tap));
        QCOMPARE(t().transmitActions, QStringList{QStringLiteral("tx.mox")});
    }

    void aDeviceDecidedGestureCantStartAHold()
    {
        // No release follows one of these, so a hold would never end.
        deviceBindings().setBinding(DeviceId::FlexControl, QStringLiteral("button3"), ControlGesture::Hold,
                                    HotKeyDeviceBinding{QStringLiteral("tx.hold"), {}, {}});
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("No release can end this hold")));
        host().onDeviceEvent(DeviceId::FlexControl, token("button3", ControlGesture::Hold));
        QVERIFY(t().keyHolds.isEmpty());
    }

    void rotateTurnsWithSpeedAndAKeptRemainder()
    {
        deviceBindings().setBinding(DeviceId::PowerMate, QStringLiteral("wheel"), ControlGesture::Turn, kVolumeKnob);
        deviceBindings().setSpeed(DeviceId::PowerMate, 3);
        host().onDeviceEvent(DeviceId::PowerMate, rotate("wheel", 2));
        flushTurns();
        QCOMPARE(t().volumeSteps(), 0);
        host().onDeviceEvent(DeviceId::PowerMate, rotate("wheel", 2));
        flushTurns();
        QCOMPARE(t().volumeSteps(), 1);     // 4 detents: one step, one kept
        host().onDeviceEvent(DeviceId::PowerMate, rotate("wheel", 2));
        flushTurns();
        QCOMPARE(t().volumeSteps(), 2);
    }

    void aDirectionChangeClearsTheRemainder()
    {
        deviceBindings().setBinding(DeviceId::PowerMate, QStringLiteral("wheel"), ControlGesture::Turn, kVolumeKnob);
        deviceBindings().setSpeed(DeviceId::PowerMate, 3);
        host().onDeviceEvent(DeviceId::PowerMate, rotate("wheel", 2));
        host().onDeviceEvent(DeviceId::PowerMate, rotate("wheel", -1));
        flushTurns();
        QCOMPARE(t().volumeSteps(), 0);
        host().onDeviceEvent(DeviceId::PowerMate, rotate("wheel", -2));
        flushTurns();
        QCOMPARE(t().volumeSteps(), -1);    // -3 from the change on, not -1 net of the +2
    }

    void reversedFlipsTheSign()
    {
        deviceBindings().setBinding(DeviceId::PowerMate, QStringLiteral("wheel"), ControlGesture::Turn, kVolumeKnob);
        deviceBindings().setReversed(DeviceId::PowerMate, true);
        host().onDeviceEvent(DeviceId::PowerMate, rotate("wheel", 2));
        flushTurns();
        QCOMPARE(t().volumeSteps(), -2);
    }

    void aWheelTunes()
    {
        host().onDeviceEvent(DeviceId::Rc28, rotate("wheel", 3));
        QCOMPARE(tuneSteps(), 3);
    }

    void aKnobCantKeyEvenWithASavedBinding()
    {
        // Refused at setBinding, and the dispatcher ignores it in any case.
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("Refused a binding")));
        deviceBindings().setBinding(DeviceId::Rc28, QStringLiteral("wheel"), ControlGesture::Turn,
                                    HotKeyDeviceBinding{QStringLiteral("tx.mox"), QStringLiteral("tx.tune"), {}});
        host().onDeviceEvent(DeviceId::Rc28, rotate("wheel", 1));
        QVERIFY(t().transmitActions.isEmpty());
        QVERIFY(t().keyHolds.isEmpty());
    }

    void aPinGoesWithTheCall()
    {
        deviceBindings().setBinding(DeviceId::PowerMate, QStringLiteral("press"), ControlGesture::Press,
                                    HotKeyDeviceBinding{QStringLiteral("mute"), {}, QLatin1Char('B')});
        host().onDeviceEvent(DeviceId::PowerMate, down("press"));
        QCOMPARE(t().sliceIds(), QStringList{QStringLiteral("mute")});
        QCOMPARE(t().lettersAsked, QList<QChar>{QLatin1Char('B')});
    }

    void aRingSendsItsPositionEvery100Ms()
    {
        host().onDeviceEvent(DeviceId::ShuttleXpress, ring(3));
        QCOMPARE(tuneSteps(), 30);          // through tune.up10
        host().onRingTick();
        QCOMPARE(tuneSteps(), 60);
        host().onDeviceEvent(DeviceId::ShuttleXpress, ring(-2));    // a move alone sends nothing extra
        QCOMPARE(tuneSteps(), 60);
        host().onRingTick();
        QCOMPARE(tuneSteps(), 40);          // through tune.down10
        host().onDeviceEvent(DeviceId::ShuttleXpress, ring(0));
        host().onRingTick();
        QCOMPARE(tuneSteps(), 40);
        constexpr int kRingMs = ControllerHost::kRingRepeatMs;
        QCOMPARE(kRingMs, 100);
    }

    void aRingStopsWhenTheDeviceGoes()
    {
        host().onDeviceEvent(DeviceId::ShuttleXpress, ring(1));
        QCOMPARE(tuneSteps(), 10);
        host().onDeviceStateChanged(DeviceId::ShuttleXpress, false, kNotPlugged);
        host().onRingTick();
        QCOMPARE(tuneSteps(), 10);
    }

    // ---- RC-28 stay-on ----

    void stayOnLatchesAndTheNextPressLetsGo()
    {
        deviceBindings().setRc28TxStaysOn(true);
        host().onDeviceEvent(DeviceId::Rc28, down("tx"));
        host().onDeviceEvent(DeviceId::Rc28, up("tx"));
        QCOMPARE(t().keyHolds, QList<bool>{true});
        host().onDeviceEvent(DeviceId::Rc28, down("tx"));
        QCOMPARE(t().keyHolds, (QList<bool>{true, false}));
        host().onDeviceEvent(DeviceId::Rc28, up("tx"));
        QCOMPARE(t().keyHolds, (QList<bool>{true, false}));
    }

    void stayOnDropsOnUnplug()
    {
        deviceBindings().setRc28TxStaysOn(true);
        host().onDeviceEvent(DeviceId::Rc28, down("tx"));
        host().onDeviceEvent(DeviceId::Rc28, up("tx"));
        host().onDeviceStateChanged(DeviceId::Rc28, false, kNotPlugged);
        QCOMPARE(t().keyHolds, (QList<bool>{true, false}));
        // Back, and the latch is gone: the next press keys.
        host().onDeviceEvent(DeviceId::Rc28, down("tx"));
        QCOMPARE(t().keyHolds, (QList<bool>{true, false, true}));
    }

    void stayOnDropsWithTheOffSwitch()
    {
        deviceBindings().setRc28TxStaysOn(true);
        host().onDeviceEvent(DeviceId::Rc28, down("tx"));
        m_bindings->setEnabled(false);
        QCOMPARE(t().keyHolds, (QList<bool>{true, false}));
    }

    void stayOnDropsAcrossSleep()
    {
        qint64 wall = 1000;
        qint64 awake = 1000;
        m_dispatcher->setSleepClocksForTest([&wall] { return wall; }, [&awake] { return awake; });
        deviceBindings().setRc28TxStaysOn(true);
        host().onDeviceEvent(DeviceId::Rc28, down("tx"));
        wall += HotKeyDispatcher::kSleepGapMs + 1000;
        m_dispatcher->checkForSleep();
        QCOMPARE(t().keyHolds, (QList<bool>{true, false}));
        host().onDeviceEvent(DeviceId::Rc28, up("tx"));
        host().onDeviceEvent(DeviceId::Rc28, down("tx"));
        QCOMPARE(t().keyHolds, (QList<bool>{true, false, true}));
    }

    void stayOnAfterTheKeyWentOffKeysAgainOnTheNextPress()
    {
        deviceBindings().setRc28TxStaysOn(true);
        host().onDeviceEvent(DeviceId::Rc28, down("tx"));
        host().onDeviceEvent(DeviceId::Rc28, up("tx"));
        t().transmittingNow = false;
        m_dispatcher->onTransmitWentOff();      // a time-out, say
        host().onDeviceEvent(DeviceId::Rc28, down("tx"));
        QCOMPARE(t().keyHolds, (QList<bool>{true, true}));
    }

    void stayOnARefusedPressDoesNotLatch()
    {
        deviceBindings().setRc28TxStaysOn(true);
        t().refuseKey = true;
        host().onDeviceEvent(DeviceId::Rc28, down("tx"));
        host().onDeviceEvent(DeviceId::Rc28, up("tx"));
        t().refuseKey = false;
        host().onDeviceEvent(DeviceId::Rc28, down("tx"));
        QCOMPARE(t().keyHolds, QList<bool>{true});
    }

    void stayOnTurnedOffWhileLatchedStillLetsGo()
    {
        deviceBindings().setRc28TxStaysOn(true);
        host().onDeviceEvent(DeviceId::Rc28, down("tx"));
        host().onDeviceEvent(DeviceId::Rc28, up("tx"));
        deviceBindings().setRc28TxStaysOn(false);
        host().onDeviceEvent(DeviceId::Rc28, down("tx"));
        QCOMPARE(t().keyHolds, (QList<bool>{true, false}));
    }

    void stayOnIsOnlyTheRc28s()
    {
        deviceBindings().setRc28TxStaysOn(true);
        host().onDeviceEvent(DeviceId::StreamDeckPlus, down("button1"));
        host().onDeviceEvent(DeviceId::StreamDeckPlus, up("button1"));
        QCOMPARE(t().keyHolds, (QList<bool>{true, false}));
    }

    // ---- the off switch and unplug ----

    void theOffSwitchStopsEverything()
    {
        m_bindings->setEnabled(false);
        host().onDeviceEvent(DeviceId::Rc28, down("tx"));
        host().onDeviceEvent(DeviceId::Rc28, up("tx"));
        host().onDeviceEvent(DeviceId::Rc28, down("f1"));
        m_deviceNow = kHoldMs;
        host().onGestureTick();
        host().onDeviceEvent(DeviceId::Rc28, up("f1"));
        host().onDeviceEvent(DeviceId::Rc28, rotate("wheel", 5));
        host().onDeviceEvent(DeviceId::ShuttleXpress, ring(3));
        host().onRingTick();
        host().onDeviceEvent(DeviceId::FlexControl, token("button2", ControlGesture::Tap));
        QVERIFY(t().keyHolds.isEmpty());
        QVERIFY(t().sliceActions.isEmpty());
        QVERIFY(t().transmitActions.isEmpty());
        QCOMPARE(tuneSteps(), 0);
    }

    void aPressWaitingWhenTheSwitchGoesOffNeverFires()
    {
        host().onDeviceEvent(DeviceId::Rc28, down("f1"));
        m_bindings->setEnabled(false);
        m_bindings->setEnabled(true);
        m_deviceNow = kHoldMs;
        host().onGestureTick();
        host().onDeviceEvent(DeviceId::Rc28, up("f1"));
        QVERIFY(t().sliceActions.isEmpty());
    }

    void anUnplugReleasesOnlyThatDevice()
    {
        host().onDeviceEvent(DeviceId::StreamDeckPlus, down("button1"));
        host().onDeviceEvent(DeviceId::Rc28, down("tx"));
        QCOMPARE(t().keyHolds, QList<bool>{true});
        host().onDeviceStateChanged(DeviceId::Rc28, false, kNotPlugged);
        QCOMPARE(t().keyHolds, QList<bool>{true});      // the deck still holds it
        host().onDeviceStateChanged(DeviceId::StreamDeckPlus, false, kNotPlugged);
        QCOMPARE(t().keyHolds, (QList<bool>{true, false}));
    }

    void anUnplugForgetsAPressWaitingToBecomeAHold()
    {
        host().onDeviceEvent(DeviceId::Rc28, down("f1"));
        host().onDeviceStateChanged(DeviceId::Rc28, false, kNotPlugged);
        m_deviceNow = kHoldMs;
        host().onGestureTick();
        host().onDeviceEvent(DeviceId::Rc28, up("f1"));
        QVERIFY(t().sliceActions.isEmpty());
    }

    void anUnplugClearsTheRemainder()
    {
        deviceBindings().setBinding(DeviceId::PowerMate, QStringLiteral("wheel"), ControlGesture::Turn, kVolumeKnob);
        deviceBindings().setSpeed(DeviceId::PowerMate, 3);
        host().onDeviceEvent(DeviceId::PowerMate, rotate("wheel", 2));
        host().onDeviceStateChanged(DeviceId::PowerMate, false, kNotPlugged);
        host().onDeviceEvent(DeviceId::PowerMate, rotate("wheel", 2));
        flushTurns();
        QCOMPARE(t().volumeSteps(), 0);
    }

    void sleepForgetsDownButtons()
    {
        qint64 wall = 1000;
        qint64 awake = 1000;
        m_dispatcher->setSleepClocksForTest([&wall] { return wall; }, [&awake] { return awake; });
        host().onDeviceEvent(DeviceId::Rc28, down("tx"));
        wall += HotKeyDispatcher::kSleepGapMs + 1000;
        m_dispatcher->checkForSleep();
        QCOMPARE(t().keyHolds, (QList<bool>{true, false}));
        // Its release, lost or late, does nothing.
        host().onDeviceEvent(DeviceId::Rc28, up("tx"));
        QCOMPARE(t().keyHolds, (QList<bool>{true, false}));
    }

    void twoButtonsOnOneHoldEndWithTheLast()
    {
        deviceBindings().setBinding(DeviceId::StreamDeckPlus, QStringLiteral("button2"), ControlGesture::Press,
                                    HotKeyDeviceBinding{QStringLiteral("tx.hold"), {}, {}});
        host().onDeviceEvent(DeviceId::StreamDeckPlus, down("button1"));
        host().onDeviceEvent(DeviceId::StreamDeckPlus, down("button2"));
        host().onDeviceEvent(DeviceId::StreamDeckPlus, up("button1"));
        QCOMPARE(t().keyHolds, QList<bool>{true});
        host().onDeviceEvent(DeviceId::StreamDeckPlus, up("button2"));
        QCOMPARE(t().keyHolds, (QList<bool>{true, false}));
    }

    void connectingNeverKeys()
    {
        host().onDeviceStateChanged(DeviceId::Rc28, true, QString());
        QVERIFY(t().keyHolds.isEmpty());
        QVERIFY(host().deviceConnected(DeviceId::Rc28));
    }

    void theHostKeepsTheStateForThePage()
    {
        QSignalSpy states(&host(), &ControllerHost::deviceStateChanged);
        host().onDeviceStateChanged(DeviceId::Rc28, false, kNotPlugged);
        QCOMPARE(states.size(), 1);
        QVERIFY(!host().deviceConnected(DeviceId::Rc28));
        QCOMPARE(host().deviceReason(DeviceId::Rc28), kNotPlugged);
    }

    // ---- the real worker thread ----

    void theWorkerDeliversToTheDispatcherAndAnUnplugDropsTheHold()
    {
        // The host's own worker, with no calls on this thread but the tests' waits.
        const std::shared_ptr<FakeBus> bus = m_hostBus;
        // start() ran before anything was on the bus. The 3 s check finds it; don't
        // wait for that, ask for it.
        bus->plug(kKnobVid, kKnobPid);
        QMetaObject::invokeMethod(host().usb(), &DeviceInput::checkPresence, Qt::QueuedConnection);
        QTRY_VERIFY(host().deviceConnected(DeviceId::PowerMate));
        QVERIFY(host().usb()->thread() != QThread::currentThread());

        // A report whose FakeParser event is on control "f1": give that control the hold.
        deviceBindings().setBinding(DeviceId::PowerMate, QStringLiteral("f1"), ControlGesture::Press,
                                    HotKeyDeviceBinding{QStringLiteral("tx.hold"), {}, {}});
        bus->send(QByteArrayLiteral("d"));
        QTRY_COMPARE(t().keyHolds, QList<bool>{true});

        bus->unplug(kKnobVid, kKnobPid);
        QTRY_COMPARE(t().keyHolds, (QList<bool>{true, false}));
        QTRY_VERIFY(!host().deviceConnected(DeviceId::PowerMate));
        QCOMPARE(host().deviceReason(DeviceId::PowerMate), kNotPlugged);
    }

    void shutdownEndsTheWorkerWithADeviceOpen()
    {
        const std::shared_ptr<FakeBus> bus = m_hostBus;
        bus->plug(kKnobVid, kKnobPid);
        QMetaObject::invokeMethod(host().usb(), &DeviceInput::checkPresence, Qt::QueuedConnection);
        QTRY_VERIFY(host().deviceConnected(DeviceId::PowerMate));
        m_host.reset();
        const QMutexLocker locker(&bus->lock);
        QCOMPARE(bus->closes, 1);
    }
};

QTEST_MAIN(TestDeviceInput)
#include "tst_device_input.moc"
