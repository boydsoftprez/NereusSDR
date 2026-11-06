// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 14.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original test.
//
// What goes out to a TMate 2 and a Stream Deck+: the bytes, then the state feed through
// the host and the real USB worker to a simulated device. A pass shows the code matches
// its sources, not what a device does.
#include <QtTest>

#include <QBuffer>
#include <QFile>
#include <QImage>
#include <QMutex>
#include <QMutexLocker>
#include <QThread>

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
#include "gui/hotkeys/devices/UsbPresence.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

constexpr int kDisplayMinMs = ControllerHost::kDisplayMinMs;    // a local copy, for MinGW
constexpr int kWaitMs = 60000;      // a limit on a wait that ends as soon as it can, not a timing
constexpr int kKeyPayload = 1016;
constexpr int kStripPayload = 1008;
constexpr quint16 kTMateVid = 0x1721;
constexpr quint16 kTMatePid = 0x0614;
constexpr quint16 kDeckVid = 0x0FD9;
constexpr quint16 kDeckPid = 0x0084;

quint8 byteAt(const QByteArray& bytes, int at)
{
    return static_cast<quint8>(bytes[at]);
}

int le16(const QByteArray& bytes, int at)
{
    return byteAt(bytes, at) | (byteAt(bytes, at + 1) << 8);
}

QByteArray fakeJpeg(int size)
{
    QByteArray bytes(size, '\0');
    for (int i = 0; i < size; ++i) {
        bytes[i] = static_cast<char>((i * 7) % 251);
    }
    return bytes;
}

// The frame's vector index for a digit, 1 being the units (upstream's layout).
int highByte(int digit)
{
    return 1 + 22 - 2 * digit;
}

int lowByte(int digit)
{
    return 1 + 21 - 2 * digit;
}

QByteArray keyPayload(const QList<QByteArray>& packets)
{
    QByteArray jpeg;
    for (const QByteArray& packet : packets) {
        jpeg += packet.mid(8, le16(packet, 4));
    }
    return jpeg;
}

QByteArray stripPayload(const QList<QByteArray>& packets)
{
    QByteArray jpeg;
    for (const QByteArray& packet : packets) {
        jpeg += packet.mid(16, le16(packet, 13));
    }
    return jpeg;
}

struct FakeBus {
    QMutex lock;
    QSet<quint32> present;
    QList<QByteArray> writes;
    QSet<QThread*> writeThreads;
    int closes{0};
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
    int read(char* /*buffer*/, int /*size*/) override { return 0; }
    int write(const QByteArray& report) override
    {
        const QMutexLocker locker(&m_bus->lock);
        m_bus->writes.append(report);
        m_bus->writeThreads.insert(QThread::currentThread());
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

    SliceModel* windowSlice() const override { return slice; }
    bool transmitOn(const QString& id) const override { return transmit.value(id, false); }
    bool muted(const SliceModel* s) const override { return s->muted(); }
    bool radioConnected() const override { return radio; }
    bool radioTransmitting() const override { return false; }
};

} // namespace

class TestDeviceOutput : public QObject {
    Q_OBJECT

private:
    std::shared_ptr<FakeBus> m_bus;
    std::unique_ptr<HotKeyBindings> m_bindings;
    std::unique_ptr<NoTargets> m_targets;
    std::unique_ptr<HotKeyDispatcher> m_dispatcher;
    std::unique_ptr<SliceModel> m_slice;
    std::unique_ptr<SliceModel> m_other;
    std::unique_ptr<FakeSource> m_source;
    std::unique_ptr<HotKeyStateFeed> m_feed;
    std::unique_ptr<ControllerHost> m_host;
    qint64 m_now{1000};

    // Builds the host with these on the simulated bus and waits for them to open, then
    // lets the first images go and clears them.
    bool startWith(bool tmate, bool deck)
    {
        if (tmate) {
            m_bus->present.insert(UsbPresence::key(kTMateVid, kTMatePid));
        }
        if (deck) {
            m_bus->present.insert(UsbPresence::key(kDeckVid, kDeckPid));
        }
        m_host = std::make_unique<ControllerHost>(m_dispatcher.get(), m_bindings.get());
        m_host->setDeviceClockForTest([this] { return m_now; });
        m_host->setStateFeed(m_feed.get());
        return QTest::qWaitFor([&] {
            return (!tmate || m_host->deviceConnected(DeviceId::TMate2))
                && (!deck || m_host->deviceConnected(DeviceId::StreamDeckPlus));
        }, kWaitMs);
    }

    void passTheGate()
    {
        m_now += kDisplayMinMs;
        m_host->onDisplayTick();
    }

    // All queued output has reached the simulated device when this returns.
    void drainWorker()
    {
        QMetaObject::invokeMethod(m_host->usb(), [] {}, Qt::BlockingQueuedConnection);
    }

    QList<QByteArray> takeWrites()
    {
        drainWorker();
        const QMutexLocker locker(&m_bus->lock);
        return std::exchange(m_bus->writes, {});
    }

    QList<QByteArray> takeFrames()
    {
        QList<QByteArray> frames;
        const QList<QByteArray> writes = takeWrites();
        for (const QByteArray& write : writes) {
            if (write.size() == 65) {
                frames.append(write);
            }
        }
        return frames;
    }

    static QList<QByteArray> packetsOfKey(const QList<QByteArray>& writes, int key)
    {
        QList<QByteArray> packets;
        for (const QByteArray& write : writes) {
            if (write.size() == 1024 && byteAt(write, 1) == 0x07 && byteAt(write, 2) == key) {
                packets.append(write);
            }
        }
        return packets;
    }

    static QList<int> keysIn(const QList<QByteArray>& writes)
    {
        QList<int> keys;
        for (const QByteArray& write : writes) {
            if (write.size() == 1024 && byteAt(write, 1) == 0x07 && !keys.contains(byteAt(write, 2))) {
                keys.append(byteAt(write, 2));
            }
        }
        return keys;
    }

    static int stripPackets(const QList<QByteArray>& writes)
    {
        int count = 0;
        for (const QByteArray& write : writes) {
            count += write.size() == 1024 && byteAt(write, 1) == 0x0C ? 1 : 0;
        }
        return count;
    }

    static QColor cornerOfKey(const QList<QByteArray>& writes, int key)
    {
        const QImage image = QImage::fromData(keyPayload(packetsOfKey(writes, key)), "JPEG");
        return image.isNull() ? QColor() : image.pixelColor(2, 2);
    }

private slots:
    void init()
    {
        AppSettings::instance().clear();
        m_bus = std::make_shared<FakeBus>();
        const std::shared_ptr<FakeBus> bus = m_bus;
        DeviceInput::setLayerFactoryForTest([bus] { return std::make_unique<FakeLayer>(bus); });
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
        AppSettings::instance().clear();
    }

    // ---- the TMate 2 frame ----

    void aFrameHasItsFixedFields()
    {
        const QByteArray frame = DeviceOutput::tmate2Frame(QStringLiteral("14.250.000"), false, false);
        QCOMPARE(frame.size(), 65);
        QCOMPARE(byteAt(frame, 0), quint8(0x00));
        const auto vector = [&frame](int at) { return byteAt(frame, at + 1); };
        QCOMPARE(vector(32), quint8(0x00));
        QCOMPARE(vector(33), quint8(0));
        QCOMPARE(vector(34), quint8(50));
        QCOMPARE(vector(35), quint8(255));
        QCOMPARE(vector(36), quint8(0x00));
        QCOMPARE(vector(37), quint8(0x28));
        QCOMPARE(vector(38), quint8(0x01));
        QCOMPARE(vector(39), quint8(0x05));
        QCOMPARE(vector(40), quint8(0x0A));
        QCOMPARE(vector(41), quint8(0x0F));
        QCOMPARE(vector(42), quint8(0x19));
        QCOMPARE(vector(43), quint8(0x0A));
        for (int at = 44; at < 64; ++at) {
            QCOMPARE(vector(at), quint8(0x00));
        }
    }

    void theLedByteHasConnectedAndLocked()
    {
        QCOMPARE(byteAt(DeviceOutput::tmate2Frame(QString(), true, false), 33), quint8(0x01));
        QCOMPARE(byteAt(DeviceOutput::tmate2Frame(QString(), false, true), 33), quint8(0x02));
        QCOMPARE(byteAt(DeviceOutput::tmate2Frame(QString(), true, true), 33), quint8(0x03));
    }

    void digitsUseUpstreamsTable()
    {
        // 14.250.000: units first, 0 0 0 0 5 2 4 1.
        // kMainHigh and kMainLow, AetherSDR src/core/HidEncoderManager.cpp:33-38 [@1e0718ad]
        const QByteArray frame = DeviceOutput::tmate2Frame(QStringLiteral("14.250.000"), false, false);
        constexpr quint8 kDigitBits = 0x07;
        constexpr quint8 kLowBits = 0x0F;
        const auto high = [&frame](int digit) { return static_cast<quint8>(byteAt(frame, highByte(digit)) & kDigitBits); };
        const auto low = [&frame](int digit) { return static_cast<quint8>(byteAt(frame, lowByte(digit)) & kLowBits); };
        QCOMPARE(high(1), quint8(0x07));    // 0
        QCOMPARE(low(1), quint8(0x0D));
        QCOMPARE(high(5), quint8(0x05));    // 5
        QCOMPARE(low(5), quint8(0x0B));
        QCOMPARE(high(6), quint8(0x03));    // 2
        QCOMPARE(low(6), quint8(0x0E));
        QCOMPARE(high(7), quint8(0x06));    // 4
        QCOMPARE(low(7), quint8(0x03));
        QCOMPARE(high(8), quint8(0x06));    // 1
        QCOMPARE(low(8), quint8(0x00));
        QCOMPARE(high(9), quint8(0x00));    // blank, not a zero
        QCOMPARE(low(9), quint8(0x00));
    }

    void theDotsAndHzAreOn()
    {
        const QByteArray frame = DeviceOutput::tmate2Frame(QStringLiteral("7.074.000"), false, false);
        QVERIFY((byteAt(frame, 1 + 9) & 0x10) != 0);
        QVERIFY((byteAt(frame, 1 + 15) & 0x10) != 0);
        QVERIFY((byteAt(frame, 1 + 23) & 0x01) != 0);
    }

    void noFrequencyLeavesTheDigitsBlank()
    {
        const QByteArray frame = DeviceOutput::tmate2Frame(QString(), false, false);
        for (int digit = 1; digit <= 9; ++digit) {
            QCOMPARE(static_cast<quint8>(byteAt(frame, highByte(digit)) & 0x07), quint8(0));
            QCOMPARE(static_cast<quint8>(byteAt(frame, lowByte(digit)) & 0x0F), quint8(0));
        }
    }

    void aTooLargeFrequencyShowsAllNines()
    {
        const QByteArray frame = DeviceOutput::tmate2Frame(QStringLiteral("1.234.567.890"), false, false);
        for (int digit = 1; digit <= 9; ++digit) {
            QCOMPARE(static_cast<quint8>(byteAt(frame, highByte(digit)) & 0x07), quint8(0x07));
            QCOMPARE(static_cast<quint8>(byteAt(frame, lowByte(digit)) & 0x0F), quint8(0x0B));
        }
    }

    // ---- Stream Deck+ packets ----

    void aFakeJpegSplitsIntoKeyPackets()
    {
        const QByteArray jpeg = fakeJpeg(5000);
        const QList<QByteArray> packets = DeviceOutput::streamDeckKeyPackets(3, jpeg);
        QCOMPARE(packets.size(), 5);
        for (int i = 0; i < packets.size(); ++i) {
            const QByteArray& packet = packets.at(i);
            const bool last = i == packets.size() - 1;
            QCOMPARE(packet.size(), 1024);
            QCOMPARE(byteAt(packet, 0), quint8(0x02));
            QCOMPARE(byteAt(packet, 1), quint8(0x07));
            QCOMPARE(byteAt(packet, 2), quint8(3));
            QCOMPARE(byteAt(packet, 3), quint8(last ? 1 : 0));
            QCOMPARE(le16(packet, 4), last ? 5000 - 4 * kKeyPayload : kKeyPayload);
            QCOMPARE(le16(packet, 6), i);
        }
        QCOMPARE(keyPayload(packets), jpeg);
    }

    void aFakeJpegSplitsIntoStripPackets()
    {
        const QByteArray jpeg = fakeJpeg(5000);
        const QList<QByteArray> packets = DeviceOutput::streamDeckStripPackets(jpeg);
        QCOMPARE(packets.size(), 5);
        for (int i = 0; i < packets.size(); ++i) {
            const QByteArray& packet = packets.at(i);
            const bool last = i == packets.size() - 1;
            QCOMPARE(packet.size(), 1024);
            QCOMPARE(byteAt(packet, 0), quint8(0x02));
            QCOMPARE(byteAt(packet, 1), quint8(0x0C));
            QCOMPARE(le16(packet, 2), 0);
            QCOMPARE(le16(packet, 4), 0);
            QCOMPARE(le16(packet, 6), 800);
            QCOMPARE(le16(packet, 8), 100);
            QCOMPARE(byteAt(packet, 10), quint8(last ? 1 : 0));
            QCOMPARE(le16(packet, 11), i);
            QCOMPARE(le16(packet, 13), last ? 5000 - 4 * kStripPayload : kStripPayload);
            QCOMPARE(byteAt(packet, 15), quint8(0x00));
        }
        QCOMPARE(stripPayload(packets), jpeg);
    }

    void aJpegThatFitsExactlyHasNoEmptyLastPacket()
    {
        const QList<QByteArray> packets = DeviceOutput::streamDeckKeyPackets(0, fakeJpeg(2 * kKeyPayload));
        QCOMPARE(packets.size(), 2);
        QCOMPARE(byteAt(packets.at(0), 3), quint8(0));
        QCOMPARE(byteAt(packets.at(1), 3), quint8(1));
        QCOMPARE(le16(packets.at(1), 4), kKeyPayload);
        QVERIFY(DeviceOutput::streamDeckKeyPackets(0, QByteArray()).isEmpty());
        QVERIFY(DeviceOutput::streamDeckStripPackets(QByteArray()).isEmpty());
    }

    void imagesBecomeJpegsOfTheRightSize()
    {
        const QImage key = DeviceOutput::renderStreamDeckKey(QStringLiteral("MOX"), QColor(20, 28, 45));
        QCOMPARE(key.size(), QSize(120, 120));
        const QImage keyBack = QImage::fromData(keyPayload(DeviceOutput::streamDeckKeyImage(2, key)), "JPEG");
        QCOMPARE(keyBack.size(), QSize(120, 120));

        const QImage strip = DeviceOutput::renderStreamDeckStrip(QStringLiteral("A"), QStringLiteral("14.250.000"),
                                                                 QStringLiteral("USB"), QStringLiteral("2.7K"));
        QCOMPARE(strip.size(), QSize(800, 100));
        const QImage stripBack = QImage::fromData(stripPayload(DeviceOutput::streamDeckStrip(strip)), "JPEG");
        QCOMPARE(stripBack.size(), QSize(800, 100));
    }

    void anImageOfAnotherSizeIsScaled()
    {
        QImage big(240, 240, QImage::Format_ARGB32);
        big.fill(Qt::red);
        const QImage back = QImage::fromData(keyPayload(DeviceOutput::streamDeckKeyImage(0, big)), "JPEG");
        QCOMPARE(back.size(), QSize(120, 120));
    }

    void aLabelDrawsOnTheKey()
    {
        const QColor background(20, 28, 45);
        const QImage blank = DeviceOutput::renderStreamDeckKey(QString(), background);
        const QImage labelled = DeviceOutput::renderStreamDeckKey(QStringLiteral("MOX"), background);
        QCOMPARE(blank.pixelColor(60, 60), background);
        QVERIFY(blank != labelled);
    }

    // ---- from the state feed to the device ----

    void aFrequencyChangeMakesOneFrame()
    {
        QVERIFY(startWith(true, false));
        passTheGate();
        QCOMPARE(takeFrames().size(), 1);     // what a newly opened device is sent
        QVERIFY(m_bus->writeThreads.size() == 1);
        QVERIFY(!m_bus->writeThreads.contains(QThread::currentThread()));

        m_now += kDisplayMinMs;
        m_slice->setFrequency(14'250'100.0);
        const QList<QByteArray> frames = takeFrames();
        QCOMPARE(frames.size(), 1);
        QCOMPARE(frames.at(0), DeviceOutput::tmate2Frame(QStringLiteral("14.250.100"), true, false));
    }

    void framesComeAtMostOncePer100Ms()
    {
        QVERIFY(startWith(true, false));
        passTheGate();
        takeFrames();

        m_now += kDisplayMinMs;
        m_slice->setFrequency(14'250'100.0);
        QCOMPARE(takeFrames().size(), 1);

        // A burst inside the window sends nothing yet.
        m_now += 10;
        m_slice->setFrequency(14'250'200.0);
        m_now += 10;
        m_slice->setFrequency(14'250'300.0);
        m_now += kDisplayMinMs - 21;
        m_host->onDisplayTick();
        QCOMPARE(takeFrames().size(), 0);

        // Then the newest of it, and only that.
        m_now += 1;
        m_host->onDisplayTick();
        const QList<QByteArray> frames = takeFrames();
        QCOMPARE(frames.size(), 1);
        QCOMPARE(frames.at(0), DeviceOutput::tmate2Frame(QStringLiteral("14.250.300"), true, false));

        // Nothing is left over.
        m_now += kDisplayMinMs;
        m_host->onDisplayTick();
        QCOMPARE(takeFrames().size(), 0);
    }

    void aChangeThisDisplayDoesNotShowSendsNoFrame()
    {
        QVERIFY(startWith(true, false));
        passTheGate();
        takeFrames();
        m_now += kDisplayMinMs;
        m_slice->setFilter(100, 500);
        QCOMPARE(takeFrames().size(), 0);
    }

    void aSliceTheWindowIsNotOnSendsNoFrame()
    {
        QVERIFY(startWith(true, false));
        passTheGate();
        takeFrames();
        m_now += kDisplayMinMs;
        m_other->setFrequency(7'074'100.0);
        QCOMPARE(takeFrames().size(), 0);
    }

    void lockAndTheRadioConnectionReachTheLeds()
    {
        QVERIFY(startWith(true, false));
        passTheGate();
        takeFrames();

        m_now += kDisplayMinMs;
        m_slice->setLocked(true);
        QVERIFY(QTest::qWaitFor([this] {
            const QMutexLocker locker(&m_bus->lock);
            return !m_bus->writes.isEmpty();
        }, kWaitMs));
        QList<QByteArray> frames = takeFrames();
        QCOMPARE(frames.size(), 1);
        QCOMPARE(byteAt(frames.at(0), 33), quint8(0x03));

        m_now += kDisplayMinMs;
        m_source->radio = false;
        m_feed->refresh();
        frames = takeFrames();
        QCOMPARE(frames.size(), 1);
        QCOMPARE(byteAt(frames.at(0), 33), quint8(0x02));
    }

    void noDeviceMeansNoOutput()
    {
        QVERIFY(startWith(false, false));
        m_slice->setFrequency(14'250'100.0);
        m_source->transmit.insert(QStringLiteral("tx.mox"), true);
        m_feed->refresh();
        QCOMPARE(takeWrites().size(), 0);
    }

    void aNewlyOpenedDeckGetsEveryKeyAndTheStrip()
    {
        QVERIFY(startWith(false, true));
        passTheGate();
        const QList<QByteArray> writes = takeWrites();
        QCOMPARE(keysIn(writes), (QList<int>{0, 1, 2, 3, 4, 5, 6, 7}));
        QVERIFY(stripPackets(writes) > 0);
        QVERIFY(m_bus->writeThreads.size() == 1);
        QVERIFY(!m_bus->writeThreads.contains(QThread::currentThread()));
    }

    void moxOnLightsItsKeyAndOnlyThatKey()
    {
        QVERIFY(startWith(false, true));
        passTheGate();
        const QColor idle = cornerOfKey(takeWrites(), 1);
        QVERIFY(idle.isValid());

        // MOX is key 2 of the default table, sent as index 1.
        m_source->transmit.insert(QStringLiteral("tx.mox"), true);
        m_feed->refresh();
        QList<QByteArray> writes = takeWrites();
        QCOMPARE(keysIn(writes), QList<int>{1});
        const QColor lit = cornerOfKey(writes, 1);
        QVERIFY(lit.isValid());
        QVERIFY2(lit.red() > idle.red() + 80, "the key should turn red");
        QCOMPARE(stripPackets(writes), 0);

        m_source->transmit.insert(QStringLiteral("tx.mox"), false);
        m_feed->refresh();
        writes = takeWrites();
        QCOMPARE(keysIn(writes), QList<int>{1});
        QVERIFY(cornerOfKey(writes, 1).red() < lit.red() - 80);
    }

    void aKeyThatStaysTheSameIsNotDrawnAgain()
    {
        QVERIFY(startWith(false, true));
        passTheGate();
        takeWrites();
        m_slice->setRitEnabled(true);      // on no key of the default table
        QCOMPARE(keysIn(takeWrites()), QList<int>{});
    }

    void aChangedBindingRedrawsItsKey()
    {
        QVERIFY(startWith(false, true));
        passTheGate();
        takeWrites();
        m_host->deviceBindings()->setBinding(DeviceId::StreamDeckPlus, QStringLiteral("button5"), ControlGesture::Press,
                                             HotKeyDeviceBinding{QStringLiteral("tx.mox"), {}, {}});
        QCOMPARE(keysIn(takeWrites()), QList<int>{4});
    }

    void theStripFollowsTheDisplayAtTheSameRate()
    {
        QVERIFY(startWith(false, true));
        passTheGate();
        takeWrites();

        m_now += kDisplayMinMs;
        m_slice->setFrequency(14'250'100.0);
        QVERIFY(stripPackets(takeWrites()) > 0);

        m_now += 10;
        m_slice->setFrequency(14'250'200.0);
        QCOMPARE(stripPackets(takeWrites()), 0);
        m_now += kDisplayMinMs;
        m_host->onDisplayTick();
        QVERIFY(stripPackets(takeWrites()) > 0);
    }

    void aClaimedDeckIsClosedAndNotDrawnFor()
    {
        QVERIFY(startWith(false, true));
        passTheGate();
        takeWrites();
        DeviceInput* const usb = m_host->usb();
        QMetaObject::invokeMethod(
            usb, [usb] { usb->setDeviceClaimedElsewhere(DeviceId::StreamDeckPlus, true); },
            Qt::BlockingQueuedConnection);
        QVERIFY(QTest::qWaitFor([this] { return !m_host->deviceConnected(DeviceId::StreamDeckPlus); }, kWaitMs));
        QCOMPARE(m_host->deviceReason(DeviceId::StreamDeckPlus), QStringLiteral("A deck plugin is using it."));
        {
            const QMutexLocker locker(&m_bus->lock);
            QCOMPARE(m_bus->closes, 1);
        }
        m_source->transmit.insert(QStringLiteral("tx.mox"), true);
        m_feed->refresh();
        QCOMPARE(takeWrites().size(), 0);

        // Letting go opens it again, and it gets everything, MOX lit.
        QMetaObject::invokeMethod(
            usb, [usb] { usb->setDeviceClaimedElsewhere(DeviceId::StreamDeckPlus, false); },
            Qt::BlockingQueuedConnection);
        QVERIFY(QTest::qWaitFor([this] { return m_host->deviceConnected(DeviceId::StreamDeckPlus); }, kWaitMs));
        const QList<QByteArray> writes = takeWrites();
        QCOMPARE(keysIn(writes).size(), 8);
        QVERIFY(cornerOfKey(writes, 1).red() > cornerOfKey(writes, 0).red() + 80);
    }

    void aFeedThatGoesFirstLeavesTheHostSafe()
    {
        QVERIFY(startWith(true, true));
        passTheGate();
        takeWrites();
        m_feed.reset();
        QCOMPARE(m_host->stateFeed(), nullptr);
        m_now += kDisplayMinMs;
        m_host->onDisplayTick();
        m_host->deviceBindings()->setBinding(DeviceId::StreamDeckPlus, QStringLiteral("button5"), ControlGesture::Press,
                                             HotKeyDeviceBinding{QStringLiteral("tx.mox"), {}, {}});
        QCOMPARE(keysIn(takeWrites()), QList<int>{4});
    }

    void theWorkerNeverSeesAnImage()
    {
        // The worker's own code names no image type and no drawing code, so what it's
        // handed can only be bytes.
        for (const char* path : {"/src/gui/hotkeys/devices/DeviceInput.h", "/src/gui/hotkeys/devices/DeviceInput.cpp"}) {
            QFile file(QStringLiteral(NEREUS_SOURCE_DIR) + QLatin1String(path));
            QVERIFY(file.open(QIODevice::ReadOnly));
            const QByteArray source = file.readAll();
            for (const char* word : {"QImage", "QPainter", "DeviceOutput", "render"}) {
                QVERIFY2(!source.contains(word), word);
            }
        }
    }
};

QTEST_MAIN(TestDeviceOutput)
#include "tst_device_output.moc"
