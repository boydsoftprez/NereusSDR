// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 17.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original test.
//
// Every device's lights and display, from the state feed through the host and the real
// USB worker to a simulated device. The clock is injected, so nothing here waits on real
// time. A pass shows the code matches its sources, not what a device does.
#include <QtTest>

#include <QImage>
#include <QMutex>
#include <QMutexLocker>
#include <QPointer>

#include <memory>
#include <utility>

#include "core/AppSettings.h"
#include "core/hotkeys/HotKeyBindings.h"
#include "gui/hotkeys/ControllerHost.h"
#include "gui/hotkeys/HotKeyDispatcher.h"
#include "gui/hotkeys/HotKeyStateFeed.h"
#include "gui/hotkeys/HotKeyTargets.h"
#include "gui/hotkeys/devices/DeviceInput.h"
#include "gui/hotkeys/devices/DeviceOutput.h"
#include "gui/hotkeys/devices/FlexControlInput.h"
#include "gui/hotkeys/devices/FlexControlParser.h"
#include "gui/hotkeys/devices/Rc28Parser.h"
#include "gui/hotkeys/devices/UsbPresence.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

constexpr int kGateMs = ControllerHost::kDisplayMinMs;          // local copies, for MinGW
constexpr int kSettleMs = ControllerHost::kRc28LightSettleMs;
constexpr int kWaitMs = 60000;      // a limit on a wait that ends as soon as it can, not a timing
constexpr int kRc28Size = 33;
constexpr int kTMateSize = 65;
constexpr int kDeckPacketSize = 1024;
constexpr quint8 kKeyCommand = 0x07;
constexpr quint8 kStripCommand = 0x0C;
constexpr int kBurst = 30;

constexpr quint16 kRc28Vid = 0x0C26;
constexpr quint16 kRc28Pid = 0x001E;
constexpr quint16 kTMateVid = 0x1721;
constexpr quint16 kTMatePid = 0x0614;
constexpr quint16 kDeckVid = 0x0FD9;
constexpr quint16 kDeckPid = 0x0084;

quint8 byteAt(const QByteArray& bytes, int at)
{
    return static_cast<quint8>(bytes[at]);
}

struct FakeBus {
    QMutex lock;
    QSet<quint32> present;
    QList<QByteArray> writes;
};

class FakeHandle : public HidDeviceHandle {
public:
    explicit FakeHandle(std::shared_ptr<FakeBus> bus)
        : m_bus(std::move(bus))
    {
    }
    int read(char* /*buffer*/, int /*size*/) override { return 0; }
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
        return m_bus->present;
    }
    std::unique_ptr<HidDeviceHandle> open(quint16 /*vid*/, quint16 /*pid*/, QString* /*error*/) override
    {
        return std::make_unique<FakeHandle>(m_bus);
    }

private:
    std::shared_ptr<FakeBus> m_bus;
};

struct FakePorts {
    QMutex lock;
    bool plugged{false};
    QList<QByteArray> writes;
    QPointer<FlexSerialHandle> live;
};

class FakePort : public FlexSerialHandle {
public:
    explicit FakePort(std::shared_ptr<FakePorts> ports)
        : m_ports(std::move(ports))
    {
    }
    bool write(const QByteArray& bytes) override
    {
        const QMutexLocker locker(&m_ports->lock);
        m_ports->writes.append(bytes);
        return true;
    }
    void finishWrites() override {}
    void closePort() override {}

private:
    std::shared_ptr<FakePorts> m_ports;
};

class FakePortLayer : public FlexSerialLayer {
public:
    explicit FakePortLayer(std::shared_ptr<FakePorts> ports)
        : m_ports(std::move(ports))
    {
    }
    QString findPort(quint16 /*vendorId*/, quint16 /*productId*/) override
    {
        const QMutexLocker locker(&m_ports->lock);
        return m_ports->plugged ? QStringLiteral("fake-flex-port") : QString();
    }
    std::unique_ptr<FlexSerialHandle> open(const QString& /*portName*/, FlexOpenFailure* /*failure*/,
                                           QString* /*detail*/) override
    {
        auto port = std::make_unique<FakePort>(m_ports);
        const QMutexLocker locker(&m_ports->lock);
        m_ports->live = port.get();
        return port;
    }

private:
    std::shared_ptr<FakePorts> m_ports;
};

class NoTargets : public HotKeyTargets {
public:
    SliceModel* windowSlice() const override { return nullptr; }
    SliceModel* sliceByLetter(QChar) const override { return nullptr; }
    QChar letterOf(const SliceModel*) const override { return QLatin1Char('A'); }
    QString sliceRefusal(const SliceModel*) const override { return {}; }
    bool listened(const SliceModel*) const override { return false; }
    bool tuneTo(SliceModel*, double) override { return false; }
    void runSliceAction(const QString&, SliceModel*, int) override {}
    void runWindowAction(const QString&) override {}
    QString unavailableReason(const QString&) const override { return {}; }
    bool transmitting() const override { return false; }
    bool tuneOrTwoToneOn() const override { return false; }
    bool keyHold(bool) override { return false; }
    void runTransmitAction(const QString&) override {}
    void runCwMessage(const QString&) override {}
    void setCwKeyDown(const QString&, bool) override {}
    void toast(const QString&) override {}
    qint64 nowMs() const override { return 0; }
};

class FakeSource : public HotKeyStateSource {
public:
    SliceModel* slice{nullptr};
    QHash<QString, bool> transmit;
    bool radio{true};
    bool onAir{false};

    SliceModel* windowSlice() const override { return slice; }
    bool transmitOn(const QString& id) const override { return transmit.value(id, false); }
    bool muted(const SliceModel* s) const override { return s->muted(); }
    bool radioConnected() const override { return radio; }
    bool radioTransmitting() const override { return onAir; }
};

DeviceEvent button(DeviceEvent::Kind kind, const QString& control)
{
    DeviceEvent event;
    event.kind = kind;
    event.control = control;
    return event;
}

QByteArray flexLights(int lit)
{
    return FlexControlParser::auxLights(lit);
}

} // namespace

class TestControllerHostLights : public QObject {
    Q_OBJECT

private:
    std::shared_ptr<FakeBus> m_bus;
    std::shared_ptr<FakePorts> m_ports;
    std::unique_ptr<HotKeyBindings> m_bindings;
    std::unique_ptr<NoTargets> m_targets;
    std::unique_ptr<HotKeyDispatcher> m_dispatcher;
    std::unique_ptr<SliceModel> m_slice;
    std::unique_ptr<SliceModel> m_other;
    std::unique_ptr<FakeSource> m_source;
    std::unique_ptr<HotKeyStateFeed> m_feed;
    std::unique_ptr<ControllerHost> m_host;
    qint64 m_now{1000};

    // Builds the host and waits for these to open on the simulated bus and port, then
    // moves the clock past the gate and clears what they were sent on opening.
    bool startWith(const QList<DeviceId>& devices, bool clear = true)
    {
        for (const DeviceId id : devices) {
            if (id == DeviceId::Rc28) {
                m_bus->present.insert(UsbPresence::key(kRc28Vid, kRc28Pid));
            } else if (id == DeviceId::TMate2) {
                m_bus->present.insert(UsbPresence::key(kTMateVid, kTMatePid));
            } else if (id == DeviceId::StreamDeckPlus) {
                m_bus->present.insert(UsbPresence::key(kDeckVid, kDeckPid));
            } else if (id == DeviceId::FlexControl) {
                m_ports->plugged = true;
            }
        }
        m_host = std::make_unique<ControllerHost>(m_dispatcher.get(), m_bindings.get());
        m_host->setDeviceClockForTest([this] { return m_now; });
        m_host->setStateFeed(m_feed.get());
        const bool opened = QTest::qWaitFor([&] {
            return std::all_of(devices.cbegin(), devices.cend(), [this](DeviceId id) { return m_host->deviceConnected(id); });
        }, kWaitMs);
        if (opened && clear) {
            tick();
            hidWrites();
            flexWrites();
        }
        return opened;
    }

    void tick()
    {
        m_now += kGateMs;
        m_host->onDisplayTick();
    }

    // All queued output has reached the simulated device when this returns.
    void drainWorker()
    {
        QMetaObject::invokeMethod(m_host->usb(), [] {}, Qt::BlockingQueuedConnection);
    }

    QList<QByteArray> hidWrites()
    {
        drainWorker();
        const QMutexLocker locker(&m_bus->lock);
        return std::exchange(m_bus->writes, {});
    }

    QList<QByteArray> flexWrites()
    {
        drainWorker();
        const QMutexLocker locker(&m_ports->lock);
        return std::exchange(m_ports->writes, {});
    }

    QList<QByteArray> rc28Reports()
    {
        QList<QByteArray> reports;
        const QList<QByteArray> writes = hidWrites();
        for (const QByteArray& write : writes) {
            if (write.size() == kRc28Size) {
                reports.append(write);
            }
        }
        return reports;
    }

    QList<QByteArray> tmateFrames()
    {
        QList<QByteArray> frames;
        const QList<QByteArray> writes = hidWrites();
        for (const QByteArray& write : writes) {
            if (write.size() == kTMateSize) {
                frames.append(write);
            }
        }
        return frames;
    }

    static QList<int> keysIn(const QList<QByteArray>& writes)
    {
        QList<int> keys;
        for (const QByteArray& write : writes) {
            if (write.size() == kDeckPacketSize && byteAt(write, 1) == kKeyCommand && !keys.contains(byteAt(write, 2))) {
                keys.append(byteAt(write, 2));
            }
        }
        return keys;
    }

    static int stripPackets(const QList<QByteArray>& writes)
    {
        int count = 0;
        for (const QByteArray& write : writes) {
            count += write.size() == kDeckPacketSize && byteAt(write, 1) == kStripCommand ? 1 : 0;
        }
        return count;
    }

    static QImage keyImage(const QList<QByteArray>& writes, int key)
    {
        QByteArray jpeg;
        for (const QByteArray& write : writes) {
            if (write.size() == kDeckPacketSize && byteAt(write, 1) == kKeyCommand && byteAt(write, 2) == key) {
                jpeg += write.mid(8, byteAt(write, 4) | (byteAt(write, 5) << 8));
            }
        }
        return QImage::fromData(jpeg, "JPEG");
    }

    static QImage stripImage(const QList<QByteArray>& writes)
    {
        QByteArray jpeg;
        for (const QByteArray& write : writes) {
            if (write.size() == kDeckPacketSize && byteAt(write, 1) == kStripCommand) {
                jpeg += write.mid(16, byteAt(write, 13) | (byteAt(write, 14) << 8));
            }
        }
        return QImage::fromData(jpeg, "JPEG");
    }

    void bindDeckKey(int key, const QString& action)
    {
        m_host->deviceBindings()->setBinding(DeviceId::StreamDeckPlus, QStringLiteral("button%1").arg(key + 1),
                                             ControlGesture::Press, HotKeyDeviceBinding{action, {}, {}});
    }

    void bindFlex(int button, const QString& action, ControlGesture gesture = ControlGesture::Tap)
    {
        m_host->deviceBindings()->setBinding(DeviceId::FlexControl, QStringLiteral("button%1").arg(button), gesture,
                                             HotKeyDeviceBinding{action, {}, {}});
    }

    void sendFlexBytes(const QByteArray& bytes)
    {
        QPointer<FlexSerialHandle> port;
        {
            const QMutexLocker locker(&m_ports->lock);
            port = m_ports->live;
        }
        QVERIFY(port);
        QMetaObject::invokeMethod(port.data(), [port, bytes] { emit port->bytesArrived(bytes); },
                                  Qt::BlockingQueuedConnection);
    }

    // Each changes the window's slice to one that differs from the last in the things
    // the lights show, and moves the clock 10 ms.
    void burstOfSliceChanges()
    {
        m_other->setLocked(true);
        m_other->setMuted(true);
        for (int i = 0; i < kBurst; ++i) {
            m_source->slice = i % 2 == 0 ? m_other.get() : m_slice.get();
            m_now += 10;
            m_feed->refresh();
            m_host->onDisplayTick();
        }
    }

    void endBurstOnOther()
    {
        m_source->slice = m_other.get();
        m_now += 10;
        m_feed->refresh();
        m_host->onDisplayTick();
    }

private slots:
    void init()
    {
        AppSettings::instance().clear();
        m_bus = std::make_shared<FakeBus>();
        m_ports = std::make_shared<FakePorts>();
        const std::shared_ptr<FakeBus> bus = m_bus;
        const std::shared_ptr<FakePorts> ports = m_ports;
        DeviceInput::setLayerFactoryForTest([bus] { return std::make_unique<FakeLayer>(bus); });
        FlexControlInput::setLayerFactoryForTest([ports] { return std::make_unique<FakePortLayer>(ports); });
        FlexControlInput::setSchedulerForTest([](int) {});
        m_bindings = std::make_unique<HotKeyBindings>();
        m_targets = std::make_unique<NoTargets>();
        m_dispatcher = std::make_unique<HotKeyDispatcher>(m_bindings.get(), m_targets.get());
        m_slice = std::make_unique<SliceModel>(0);
        m_slice->setFrequency(14'250'000.0);
        m_other = std::make_unique<SliceModel>(1);
        m_other->setFrequency(7'074'000.0);
        m_source = std::make_unique<FakeSource>();
        m_source->slice = m_slice.get();
        m_feed = std::make_unique<HotKeyStateFeed>(m_source.get());
        m_now = 1000;
    }

    void cleanup()
    {
        m_host.reset();
        m_feed.reset();
        m_source.reset();
        m_slice.reset();
        m_other.reset();
        m_dispatcher.reset();
        m_targets.reset();
        m_bindings.reset();
        DeviceInput::setLayerFactoryForTest(nullptr);
        FlexControlInput::setLayerFactoryForTest(nullptr);
        FlexControlInput::setSchedulerForTest(nullptr);
        AppSettings::instance().clear();
    }

    // ---- RC-28 ----

    void anRc28IsWrittenWhenItOpens()
    {
        QVERIFY(startWith({DeviceId::Rc28}, false));
        const QList<QByteArray> reports = rc28Reports();
        QCOMPARE(reports.size(), 1);
        QCOMPARE(reports.at(0), rc28LightReport(false, false, false, true));
    }

    void theTxLightFollowsTransmittingAtOnce()
    {
        QVERIFY(startWith({DeviceId::Rc28}));
        m_source->onAir = true;
        m_feed->refresh();
        QList<QByteArray> reports = rc28Reports();
        QCOMPARE(reports.size(), 1);
        QCOMPARE(reports.at(0), rc28LightReport(true, false, false, true));

        // No move of the clock: this one doesn't wait for the gate either.
        m_source->onAir = false;
        m_feed->refresh();
        reports = rc28Reports();
        QCOMPARE(reports.size(), 1);
        QCOMPARE(reports.at(0), rc28LightReport(false, false, false, true));
    }

    void theTxLightIsNotMox()
    {
        QVERIFY(startWith({DeviceId::Rc28}));
        m_source->transmit.insert(QStringLiteral("tx.mox"), true);
        m_feed->refresh();
        tick();
        QCOMPARE(rc28Reports().size(), 0);

        // Keyed some other way, with nothing asked for here.
        m_source->transmit.clear();
        m_source->onAir = true;
        m_feed->refresh();
        QCOMPARE(rc28Reports().value(0), rc28LightReport(true, false, false, true));
    }

    void linkFollowsConnected()
    {
        QVERIFY(startWith({DeviceId::Rc28}));
        m_source->radio = false;
        m_feed->refresh();
        tick();
        QList<QByteArray> reports = rc28Reports();
        QCOMPARE(reports.size(), 1);
        QCOMPARE(reports.at(0), rc28LightReport(false, false, false, false));

        m_source->radio = true;
        m_feed->refresh();
        tick();
        reports = rc28Reports();
        QCOMPARE(reports.size(), 1);
        QCOMPARE(reports.at(0), rc28LightReport(false, false, false, true));
    }

    void theFLightsFollowTheirHoldRows()
    {
        // The defaults: F1 holds slice lock, F2 holds mute.
        QVERIFY(startWith({DeviceId::Rc28}));
        m_slice->setLocked(true);
        tick();
        QCOMPARE(rc28Reports().value(0), rc28LightReport(false, true, false, true));

        m_slice->setMuted(true);
        tick();
        QCOMPARE(rc28Reports().value(0), rc28LightReport(false, true, true, true));

        m_host->deviceBindings()->setBinding(DeviceId::Rc28, QStringLiteral("f1"), ControlGesture::Hold,
                                             HotKeyDeviceBinding{QStringLiteral("rit.toggle"), {}, {}});
        tick();
        QCOMPARE(rc28Reports().value(0), rc28LightReport(false, false, true, true));
    }

    void nothingIsWrittenWhileAnFKeyIsDown()
    {
        QVERIFY(startWith({DeviceId::Rc28}));
        m_host->onDeviceEvent(DeviceId::Rc28, button(DeviceEvent::Kind::ButtonDown, QStringLiteral("f1")));
        m_slice->setLocked(true);
        m_source->onAir = true;
        m_feed->refresh();
        tick();
        tick();
        QCOMPARE(rc28Reports().size(), 0);

        // Not at the release, and not until it has settled.
        m_host->onDeviceEvent(DeviceId::Rc28, button(DeviceEvent::Kind::ButtonUp, QStringLiteral("f1")));
        QCOMPARE(rc28Reports().size(), 0);
        m_now += kSettleMs - 1;
        m_host->onDisplayTick();
        QCOMPARE(rc28Reports().size(), 0);
        m_now += 1;
        m_host->onDisplayTick();
        const QList<QByteArray> reports = rc28Reports();
        QCOMPARE(reports.size(), 1);
        QCOMPARE(reports.at(0), rc28LightReport(true, true, false, true));
    }

    void bothFKeysHaveToComeUp()
    {
        QVERIFY(startWith({DeviceId::Rc28}));
        m_host->onDeviceEvent(DeviceId::Rc28, button(DeviceEvent::Kind::ButtonDown, QStringLiteral("f1")));
        m_host->onDeviceEvent(DeviceId::Rc28, button(DeviceEvent::Kind::ButtonDown, QStringLiteral("f2")));
        m_slice->setLocked(true);
        m_host->onDeviceEvent(DeviceId::Rc28, button(DeviceEvent::Kind::ButtonUp, QStringLiteral("f1")));
        tick();
        QCOMPARE(rc28Reports().size(), 0);
        m_host->onDeviceEvent(DeviceId::Rc28, button(DeviceEvent::Kind::ButtonUp, QStringLiteral("f2")));
        tick();
        QCOMPARE(rc28Reports().size(), 1);
    }

    void aReleaseWithNoChangeWritesNothing()
    {
        QVERIFY(startWith({DeviceId::Rc28}));
        m_host->onDeviceEvent(DeviceId::Rc28, button(DeviceEvent::Kind::ButtonDown, QStringLiteral("f1")));
        m_host->onDeviceEvent(DeviceId::Rc28, button(DeviceEvent::Kind::ButtonUp, QStringLiteral("f1")));
        tick();
        QCOMPARE(rc28Reports().size(), 0);
    }

    void theTxBarDoesNotHoldBackTheTxLight()
    {
        QVERIFY(startWith({DeviceId::Rc28}));
        m_host->onDeviceEvent(DeviceId::Rc28, button(DeviceEvent::Kind::ButtonDown, QStringLiteral("tx")));
        m_source->onAir = true;
        m_feed->refresh();
        QCOMPARE(rc28Reports().value(0), rc28LightReport(true, false, false, true));
        m_host->onDeviceEvent(DeviceId::Rc28, button(DeviceEvent::Kind::ButtonUp, QStringLiteral("tx")));
    }

    void anFKeyIsSeenWithHotKeysOff()
    {
        QVERIFY(startWith({DeviceId::Rc28}));
        m_bindings->setEnabled(false);
        m_host->onDeviceEvent(DeviceId::Rc28, button(DeviceEvent::Kind::ButtonDown, QStringLiteral("f1")));
        m_slice->setLocked(true);
        tick();
        QCOMPARE(rc28Reports().size(), 0);
        m_host->onDeviceEvent(DeviceId::Rc28, button(DeviceEvent::Kind::ButtonUp, QStringLiteral("f1")));
        tick();
        QCOMPARE(rc28Reports().size(), 1);
    }

    void aLostReleaseCannotStopTheLights()
    {
        QVERIFY(startWith({DeviceId::Rc28}));
        m_host->onDeviceEvent(DeviceId::Rc28, button(DeviceEvent::Kind::ButtonDown, QStringLiteral("f1")));
        m_slice->setLocked(true);
        tick();
        QCOMPARE(rc28Reports().size(), 0);

        // The hot key switch forgets what's down, as sleep does.
        m_bindings->setEnabled(false);
        m_bindings->setEnabled(true);
        tick();
        QCOMPARE(rc28Reports().size(), 1);

        m_host->onDeviceEvent(DeviceId::Rc28, button(DeviceEvent::Kind::ButtonDown, QStringLiteral("f2")));
        m_slice->setLocked(false);
        tick();
        QCOMPARE(rc28Reports().size(), 0);
        m_host->onDeviceStateChanged(DeviceId::Rc28, false, QStringLiteral("Not plugged in"));
        m_host->onDeviceStateChanged(DeviceId::Rc28, true, QString());
        tick();
        QCOMPARE(rc28Reports().size(), 1);
    }

    void anRc28IsWrittenAgainWhenItComesBack()
    {
        QVERIFY(startWith({DeviceId::Rc28}));
        m_host->onDeviceStateChanged(DeviceId::Rc28, false, QStringLiteral("Not plugged in"));
        tick();
        QCOMPARE(rc28Reports().size(), 0);
        m_host->onDeviceStateChanged(DeviceId::Rc28, true, QString());
        QCOMPARE(rc28Reports().value(0), rc28LightReport(false, false, false, true));
    }

    // ---- FlexControl ----

    void aFlexIsWrittenWhenItOpens()
    {
        m_slice->setMuted(true);    // button 3 taps to mute by default
        m_feed->refresh();
        QVERIFY(startWith({DeviceId::FlexControl}, false));
        const QList<QByteArray> writes = flexWrites();
        QVERIFY(!writes.isEmpty());
        QCOMPARE(writes.constLast(), flexLights(3));
    }

    void theAuxLightsFollowTheTapRows()
    {
        QVERIFY(startWith({DeviceId::FlexControl}));
        QVERIFY(flexWrites().isEmpty());

        m_slice->setMuted(true);
        tick();
        QCOMPARE(flexWrites(), QList<QByteArray>{flexLights(3)});

        m_slice->setMuted(false);
        tick();
        QCOMPARE(flexWrites(), QList<QByteArray>{flexLights(0)});

        bindFlex(1, QStringLiteral("rit.toggle"));
        m_slice->setRitEnabled(true);
        tick();
        QCOMPARE(flexWrites(), QList<QByteArray>{flexLights(1)});

        // Only the tap row counts.
        m_slice->setRitEnabled(false);
        tick();
        flexWrites();
        bindFlex(1, QStringLiteral("rit.toggle"), ControlGesture::DoubleTap);
        bindFlex(1, QString());
        m_slice->setRitEnabled(true);
        tick();
        QVERIFY(flexWrites().isEmpty());
    }

    void oneAuxLightAtATime()
    {
        QVERIFY(startWith({DeviceId::FlexControl}));
        m_slice->setMuted(true);
        tick();
        QCOMPARE(flexWrites(), QList<QByteArray>{flexLights(3)});

        // Button 2 taps to MOX by default. The lower button shows, and it goes at once.
        m_source->transmit.insert(QStringLiteral("tx.mox"), true);
        m_feed->refresh();
        QCOMPARE(flexWrites(), QList<QByteArray>{flexLights(2)});

        m_source->transmit.insert(QStringLiteral("tx.mox"), false);
        m_feed->refresh();
        QCOMPARE(flexWrites(), QList<QByteArray>{flexLights(3)});
    }

    void aChangedBindingMovesTheAuxLight()
    {
        QVERIFY(startWith({DeviceId::FlexControl}));
        m_slice->setMuted(true);
        tick();
        flexWrites();
        bindFlex(3, QStringLiteral("rit.toggle"));
        bindFlex(1, QStringLiteral("mute"));
        tick();
        const QList<QByteArray> writes = flexWrites();
        QVERIFY(!writes.isEmpty());
        QCOMPARE(writes.constLast(), flexLights(1));
    }

    void aDeviceResetWritesTheAuxLightsAgain()
    {
        QVERIFY(startWith({DeviceId::FlexControl}));
        m_slice->setMuted(true);
        tick();
        flexWrites();
        sendFlexBytes("F0304;");
        QCOMPARE(flexWrites(), QList<QByteArray>{flexLights(3)});
    }

    void closingTheHostTurnsTheAuxLightsOff()
    {
        QVERIFY(startWith({DeviceId::FlexControl}));
        m_slice->setMuted(true);
        tick();
        flexWrites();
        m_host.reset();
        const QMutexLocker locker(&m_ports->lock);
        QCOMPARE(m_ports->writes, QList<QByteArray>{QByteArray("I000;")});
    }

    // ---- TMate 2 and Stream Deck+ ----

    void aChangeOnAnotherSliceChangesNothing()
    {
        QVERIFY(startWith({DeviceId::Rc28, DeviceId::FlexControl, DeviceId::TMate2, DeviceId::StreamDeckPlus}));
        bindDeckKey(4, QStringLiteral("rit.toggle"));
        hidWrites();
        const int queued = m_host->outputsQueuedForTest();
        const int drawn = m_host->imagesDrawnForTest();

        m_other->setFrequency(7'074'100.0);
        m_other->setLocked(true);
        m_other->setMuted(true);
        m_other->setRitEnabled(true);
        tick();
        QVERIFY(hidWrites().isEmpty());
        QVERIFY(flexWrites().isEmpty());
        QCOMPARE(m_host->outputsQueuedForTest(), queued);
        QCOMPARE(m_host->imagesDrawnForTest(), drawn);
    }

    void aDeckKeyLightsFromTheFeed()
    {
        QVERIFY(startWith({DeviceId::StreamDeckPlus}));
        bindDeckKey(4, QStringLiteral("rit.toggle"));
        const QImage idle = keyImage(hidWrites(), 4);
        QVERIFY(!idle.isNull());
        m_slice->setRitEnabled(true);
        tick();
        const QList<QByteArray> writes = hidWrites();
        QCOMPARE(keysIn(writes), QList<int>{4});
        QVERIFY(keyImage(writes, 4).pixelColor(2, 2).green() > idle.pixelColor(2, 2).green() + 40);
    }

    void aTMateFrameFollowsTheWindowsSliceWhenItChanges()
    {
        QVERIFY(startWith({DeviceId::TMate2}));
        m_source->slice = m_other.get();
        m_now += kGateMs;
        m_feed->refresh();
        const QList<QByteArray> frames = tmateFrames();
        QCOMPARE(frames.size(), 1);
        QCOMPARE(frames.at(0), DeviceOutput::tmate2Frame(QStringLiteral("7.074.000"), true, false));
    }

    // ---- at most one output per device per 100 ms ----

    void aBurstReachesAnRc28AtTheGateRate()
    {
        QVERIFY(startWith({DeviceId::Rc28}));
        burstOfSliceChanges();
        // 300 ms of changes, and the first may go at once. Two different reports, too.
        QVERIFY2(rc28Reports().size() <= 1 + (kBurst * 10) / kGateMs, "too many reports");
    }

    void noTwoOutputsInsideTheGateOnAnyDevice_data()
    {
        QTest::addColumn<int>("device");
        QTest::newRow("RC-28") << static_cast<int>(DeviceId::Rc28);
        QTest::newRow("FlexControl") << static_cast<int>(DeviceId::FlexControl);
        QTest::newRow("TMate 2") << static_cast<int>(DeviceId::TMate2);
        QTest::newRow("Stream Deck+") << static_cast<int>(DeviceId::StreamDeckPlus);
    }

    void noTwoOutputsInsideTheGateOnAnyDevice()
    {
        QFETCH(int, device);
        const DeviceId id = static_cast<DeviceId>(device);
        QVERIFY(startWith({id}));
        if (id == DeviceId::StreamDeckPlus) {
            bindDeckKey(4, QStringLiteral("slice.lock"));
            hidWrites();
        }
        m_other->setLocked(true);
        m_other->setMuted(true);

        qint64 lastOutputAt = -1;
        int passes = 0;
        for (int i = 0; i < kBurst; ++i) {
            m_source->slice = i % 2 == 0 ? m_other.get() : m_slice.get();
            m_now += 10;
            const int before = m_host->outputsQueuedForTest();
            m_feed->refresh();
            m_host->onDisplayTick();
            if (m_host->outputsQueuedForTest() != before) {
                if (lastOutputAt >= 0) {
                    QVERIFY2(m_now - lastOutputAt >= kGateMs, "two outputs inside the gate");
                }
                lastOutputAt = m_now;
                ++passes;
            }
        }
        QVERIFY(passes > 0);
    }

    void theLastStateOfABurstAlwaysArrives_data()
    {
        noTwoOutputsInsideTheGateOnAnyDevice_data();
    }

    void theLastStateOfABurstAlwaysArrives()
    {
        QFETCH(int, device);
        const DeviceId id = static_cast<DeviceId>(device);
        QVERIFY(startWith({id}));
        if (id == DeviceId::StreamDeckPlus) {
            bindDeckKey(4, QStringLiteral("slice.lock"));
        }
        burstOfSliceChanges();

        // A change that goes out at once, so that the one after it lands inside the gate.
        m_source->slice = m_slice.get();
        m_now += kGateMs;
        m_feed->refresh();
        m_host->onDisplayTick();
        hidWrites();
        flexWrites();
        endBurstOnOther();
        QVERIFY2(hidWrites().isEmpty() && flexWrites().isEmpty(), "this should wait at the gate");

        tick();
        const QList<QByteArray> hid = hidWrites();
        const QList<QByteArray> flex = flexWrites();
        switch (id) {
        case DeviceId::Rc28:
            QCOMPARE(hid, QList<QByteArray>{rc28LightReport(false, true, true, true)});
            break;
        case DeviceId::FlexControl:
            QCOMPARE(flex, QList<QByteArray>{flexLights(3)});
            break;
        case DeviceId::TMate2:
            QCOMPARE(hid, QList<QByteArray>{DeviceOutput::tmate2Frame(QStringLiteral("7.074.000"), true, true)});
            break;
        default: {
            QVERIFY(keysIn(hid).contains(4));
            QVERIFY(keyImage(hid, 4).pixelColor(2, 2).green() > 70);    // lit, as slice B is locked
            const HotKeyStateFeed::Display shown = m_feed->display();
            const QList<QByteArray> want = DeviceOutput::streamDeckStrip(DeviceOutput::renderStreamDeckStrip(
                QString(shown.slice), shown.frequency, shown.mode, shown.filter));
            QList<QByteArray> strip;
            for (const QByteArray& write : hid) {
                if (write.size() == kDeckPacketSize && byteAt(write, 1) == kStripCommand) {
                    strip.append(write);
                }
            }
            QCOMPARE(strip, want);
            break;
        }
        }

        // Nothing is left over.
        tick();
        QVERIFY(hidWrites().isEmpty());
        QVERIFY(flexWrites().isEmpty());
    }

    void aBurstDrawsADeckAtTheGateRate()
    {
        QVERIFY(startWith({DeviceId::StreamDeckPlus}));
        bindDeckKey(4, QStringLiteral("slice.lock"));
        hidWrites();
        const int drawn = m_host->imagesDrawnForTest();
        burstOfSliceChanges();
        // Each pass redraws what changed: the strip, the lock key and the mute key.
        constexpr int kMostPerPass = 3;
        const int passes = 1 + (kBurst * 10) / kGateMs;
        const int images = m_host->imagesDrawnForTest() - drawn;
        QVERIFY2(images <= passes * kMostPerPass, qPrintable(QString::number(images)));
        QVERIFY(images > 0);
    }

    void aTransmitChangeDoesNotWaitForTheGate()
    {
        QVERIFY(startWith({DeviceId::StreamDeckPlus, DeviceId::FlexControl}));
        m_slice->setFrequency(14'250'100.0);
        hidWrites();
        flexWrites();
        m_now += 1;     // well inside the gate
        m_source->transmit.insert(QStringLiteral("tx.mox"), true);
        m_feed->refresh();
        QCOMPARE(keysIn(hidWrites()), QList<int>{1});   // MOX is key 2 of the default table
        QCOMPARE(flexWrites(), QList<QByteArray>{flexLights(2)});
    }

    // ---- turning them off ----

    void disablingWritesTheOffStateOnceAndStopsUpdates_data()
    {
        noTwoOutputsInsideTheGateOnAnyDevice_data();
    }

    void disablingWritesTheOffStateOnceAndStopsUpdates()
    {
        QFETCH(int, device);
        const DeviceId id = static_cast<DeviceId>(device);
        QVERIFY(startWith({id}));
        m_slice->setMuted(true);
        tick();
        hidWrites();
        flexWrites();

        m_host->setLightsEnabled(id, false);
        QVERIFY(!m_host->lightsEnabled(id));
        const QList<QByteArray> hid = hidWrites();
        const QList<QByteArray> flex = flexWrites();
        switch (id) {
        case DeviceId::Rc28:
            QCOMPARE(hid, QList<QByteArray>{rc28LightReport(false, false, false, false)});
            break;
        case DeviceId::FlexControl:
            QCOMPARE(flex, QList<QByteArray>{QByteArray("I000;")});
            break;
        case DeviceId::TMate2:
            QCOMPARE(hid, QList<QByteArray>{DeviceOutput::tmate2OffFrame()});
            break;
        default:
            QCOMPARE(keysIn(hid), (QList<int>{0, 1, 2, 3, 4, 5, 6, 7}));
            QVERIFY(keyImage(hid, 3).pixelColor(60, 60).lightness() < 10);
            QVERIFY(stripImage(hid).pixelColor(400, 50).lightness() < 10);
            break;
        }

        const int queued = m_host->outputsQueuedForTest();
        const int drawn = m_host->imagesDrawnForTest();
        m_slice->setFrequency(14'250'100.0);
        m_slice->setLocked(true);
        m_slice->setMuted(false);
        m_source->radio = false;
        m_source->onAir = true;
        m_source->transmit.insert(QStringLiteral("tx.mox"), true);
        m_feed->refresh();
        bindDeckKey(4, QStringLiteral("slice.lock"));
        bindFlex(1, QStringLiteral("slice.lock"));
        tick();
        tick();
        QVERIFY(hidWrites().isEmpty());
        QVERIFY(flexWrites().isEmpty());
        QCOMPARE(m_host->outputsQueuedForTest(), queued);
        QCOMPARE(m_host->imagesDrawnForTest(), drawn);

        // Asking twice is nothing new.
        m_host->setLightsEnabled(id, false);
        tick();
        QCOMPARE(m_host->outputsQueuedForTest(), queued);
    }

    void enablingShowsTheCurrentStateAtOnce_data()
    {
        noTwoOutputsInsideTheGateOnAnyDevice_data();
    }

    void enablingShowsTheCurrentStateAtOnce()
    {
        QFETCH(int, device);
        const DeviceId id = static_cast<DeviceId>(device);
        QVERIFY(startWith({id}));
        m_host->setLightsEnabled(id, false);
        m_slice->setMuted(true);
        hidWrites();
        flexWrites();

        m_host->setLightsEnabled(id, true);     // no move of the clock
        const QList<QByteArray> hid = hidWrites();
        const QList<QByteArray> flex = flexWrites();
        switch (id) {
        case DeviceId::Rc28:
            QCOMPARE(hid, QList<QByteArray>{rc28LightReport(false, false, true, true)});
            break;
        case DeviceId::FlexControl:
            QCOMPARE(flex, QList<QByteArray>{flexLights(3)});
            break;
        case DeviceId::TMate2:
            QCOMPARE(hid, QList<QByteArray>{DeviceOutput::tmate2Frame(QStringLiteral("14.250.000"), true, false)});
            break;
        default:
            QCOMPARE(keysIn(hid).size(), 8);
            QVERIFY(stripPackets(hid) > 0);
            break;
        }
    }

    void disablingAnRc28WaitsForAHeldKeyToComeUp()
    {
        QVERIFY(startWith({DeviceId::Rc28}));
        m_host->onDeviceEvent(DeviceId::Rc28, button(DeviceEvent::Kind::ButtonDown, QStringLiteral("f1")));
        m_host->setLightsEnabled(DeviceId::Rc28, false);
        tick();
        QCOMPARE(rc28Reports().size(), 0);
        m_host->onDeviceEvent(DeviceId::Rc28, button(DeviceEvent::Kind::ButtonUp, QStringLiteral("f1")));
        tick();
        QCOMPARE(rc28Reports(), QList<QByteArray>{rc28LightReport(false, false, false, false)});
    }

    void theChoiceIsSavedAndUsedAtTheNextOpen()
    {
        QVERIFY(startWith({DeviceId::Rc28, DeviceId::FlexControl, DeviceId::TMate2, DeviceId::StreamDeckPlus}));
        m_host->setLightsEnabled(DeviceId::Rc28, false);
        m_host->setLightsEnabled(DeviceId::TMate2, false);
        QCOMPARE(AppSettings::instance().value(QStringLiteral("HotKeys/Device/RC28/Lights")).toString(),
                 QStringLiteral("False"));
        m_host->setLightsEnabled(DeviceId::TMate2, true);
        const QString tmateKey = QStringLiteral("HotKeys/Device/%1/Lights")
                                     .arg(HotKeyDeviceBindings::deviceKey(DeviceId::TMate2));
        QCOMPARE(AppSettings::instance().value(tmateKey).toString(), QStringLiteral("True"));

        m_host.reset();
        hidWritesWithoutHost();
        m_slice->setMuted(true);
        QVERIFY(startWith({DeviceId::Rc28, DeviceId::TMate2}, false));
        QVERIFY(!m_host->lightsEnabled(DeviceId::Rc28));
        QVERIFY(m_host->lightsEnabled(DeviceId::TMate2));
        QVERIFY(m_host->lightsEnabled(DeviceId::FlexControl));
        // A device it opens with them off is told once, whatever it showed before.
        QCOMPARE(rc28Reports(), QList<QByteArray>{rc28LightReport(false, false, false, false)});
    }

    void aDeviceWithNoLightsHasNothingToTurnOff()
    {
        QVERIFY(startWith({}));
        for (const DeviceId id : {DeviceId::PowerMate, DeviceId::ShuttleXpress, DeviceId::ShuttlePro2, DeviceId::UlanziDial}) {
            QVERIFY(!ControllerHost::hasLights(id));
            QVERIFY(!m_host->lightsEnabled(id));
            m_host->setLightsEnabled(id, false);
            m_host->setLightsEnabled(id, true);
            QVERIFY(!AppSettings::instance().contains(
                QStringLiteral("HotKeys/Device/%1/Lights").arg(HotKeyDeviceBindings::deviceKey(id))));
        }
        for (const DeviceId id : {DeviceId::Rc28, DeviceId::FlexControl, DeviceId::TMate2, DeviceId::StreamDeckPlus}) {
            QVERIFY(ControllerHost::hasLights(id));
            QVERIFY(m_host->lightsEnabled(id));     // on unless turned off
        }
    }

    // ---- a device that isn't there ----

    void aDeviceThatIsNotConnectedCostsNothing()
    {
        QVERIFY(startWith({}));
        m_slice->setFrequency(14'250'100.0);
        m_slice->setMuted(true);
        m_slice->setLocked(true);
        m_source->onAir = true;
        m_source->transmit.insert(QStringLiteral("tx.mox"), true);
        m_feed->refresh();
        burstOfSliceChanges();
        bindDeckKey(4, QStringLiteral("rit.toggle"));
        bindFlex(1, QStringLiteral("rit.toggle"));
        for (const DeviceId id : {DeviceId::Rc28, DeviceId::FlexControl, DeviceId::TMate2, DeviceId::StreamDeckPlus}) {
            m_host->setLightsEnabled(id, false);
            m_host->setLightsEnabled(id, true);
        }
        tick();
        QCOMPARE(m_host->imagesDrawnForTest(), 0);
        QCOMPARE(m_host->outputsQueuedForTest(), 0);
        QVERIFY(hidWrites().isEmpty());
        QVERIFY(flexWrites().isEmpty());
    }

    void aDeviceThatLeftCostsNothingEither()
    {
        QVERIFY(startWith({DeviceId::StreamDeckPlus, DeviceId::Rc28, DeviceId::FlexControl, DeviceId::TMate2}));
        for (const DeviceId id : {DeviceId::Rc28, DeviceId::FlexControl, DeviceId::TMate2, DeviceId::StreamDeckPlus}) {
            m_host->onDeviceStateChanged(id, false, QStringLiteral("Not plugged in"));
        }
        const int queued = m_host->outputsQueuedForTest();
        const int drawn = m_host->imagesDrawnForTest();
        burstOfSliceChanges();
        tick();
        QCOMPARE(m_host->outputsQueuedForTest(), queued);
        QCOMPARE(m_host->imagesDrawnForTest(), drawn);
    }

    void theOffStateIsBlank()
    {
        const QByteArray frame = DeviceOutput::tmate2OffFrame();
        QCOMPARE(frame.size(), kTMateSize);
        QCOMPARE(frame.mid(0, 1 + 33 + 3), QByteArray(1 + 33 + 3, '\0'));   // segments, LEDs, backlight
        // Timing and contrast as the running frame has them.
        const QByteArray running = DeviceOutput::tmate2Frame(QString(), false, false);
        QCOMPARE(frame.mid(1 + 36), running.mid(1 + 36));
    }

private:
    void hidWritesWithoutHost()
    {
        const QMutexLocker locker(&m_bus->lock);
        m_bus->writes.clear();
    }
};

QTEST_MAIN(TestControllerHostLights)
#include "tst_controller_host_lights.moc"
