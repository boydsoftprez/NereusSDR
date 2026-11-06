// =================================================================
// src/gui/hotkeys/devices/DeviceInput.cpp  (NereusSDR)
// =================================================================
//
// Source attribution (AetherSDR — GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 §5 requirements.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-06 - USB knob and controller reader built on AetherSDR
//                 src/core/HidEncoderManager.cpp [@1e0718ad] (the hidapi open
//                 sequence, the read loop, the hotplug check) for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation via
//                 Anthropic Claude Code (Claude Opus 5.5). Hot keys plan Task
//                 12. One device per kind, all at once, where upstream has one
//                 in all; its own thread; nothing opened until the device is
//                 seen on the USB bus; open failures say how to fix them.
// =================================================================

#include "gui/hotkeys/devices/DeviceInput.h"

#include "core/LogCategories.h"
#include "gui/hotkeys/devices/PowerMateLinuxInput.h"
#include "gui/hotkeys/devices/PowerMateParser.h"
#include "gui/hotkeys/devices/Rc28Parser.h"
#include "gui/hotkeys/devices/ShuttleParser.h"
#include "gui/hotkeys/devices/StreamDeckPlusParser.h"
#include "gui/hotkeys/devices/TMate2Parser.h"
#include "gui/hotkeys/devices/UsbPresence.h"

#include <QMutexLocker>
#include <QTimer>

#include <optional>
#include <utility>

#ifdef NEREUS_HAVE_HIDAPI
#include "hidapi/hidapi.h"
#endif

namespace NereusSDR {

namespace {

// A device that never stops sending must not hold the worker in one tick.
// NereusSDR's choice, not a value from a reference.
constexpr int kMaxReportsPerPoll = 64;

// On Linux the kernel's own driver takes a PowerMate, so hidapi can't see it.
#if defined(Q_OS_LINUX) && defined(NEREUS_HAVE_HIDAPI)
constexpr bool kKernelOwnsPowerMate = true;
#else
constexpr bool kKernelOwnsPowerMate = false;
#endif

const QString& notPluggedInReason()
{
    static const QString kReason = QStringLiteral("Not plugged in");
    return kReason;
}

const QString& claimedReason()
{
    static const QString kReason = QStringLiteral("A deck plugin is using it.");
    return kReason;
}

const QString& noSupportReason()
{
    static const QString kReason = QStringLiteral("This build has no USB device support.");
    return kReason;
}

DeviceInput::LayerFactory& testLayerFactory()
{
    static DeviceInput::LayerFactory kFactory;
    return kFactory;
}

// Only the application turns this on. A test process links the same library and never
// does, so no test can reach a real device or the prompt that opening one raises.
bool& realDevicesAllowedFlag()
{
    static bool kAllowed = false;
    return kAllowed;
}

std::optional<QList<DeviceSpec>>& testTable()
{
    static std::optional<QList<DeviceSpec>> kTable;
    return kTable;
}

// FlexControl and the Ulanzi Dial have inputs of their own.
QList<DeviceSpec> realTable()
{
    const auto rc28 = [] { return std::make_unique<Rc28Parser>(); };
    return {
        // Ids from AetherSDR src/core/HidDeviceParser.cpp:7-24, report sizes from
        // src/core/HidDeviceParser.h:46-126 [@1e0718ad]
        DeviceSpec{DeviceId::Rc28, 0x0C26, 0x001E, 32, rc28},
        DeviceSpec{DeviceId::Rc28, 0x2341, 0x0266, 32, rc28},
        DeviceSpec{DeviceId::PowerMate, 0x077D, 0x0410, 6, [] { return std::make_unique<PowerMateParser>(); }},
        DeviceSpec{DeviceId::ShuttleXpress, 0x0B33, 0x0020, 5,
                   [] { return std::make_unique<ShuttleParser>(ShuttleParser::Model::Xpress); }},
        DeviceSpec{DeviceId::ShuttlePro2, 0x0B33, 0x0030, 5,
                   [] { return std::make_unique<ShuttleParser>(ShuttleParser::Model::Pro2); }},
        DeviceSpec{DeviceId::TMate2, 0x1721, 0x0614, 64, [] { return std::make_unique<TMate2Parser>(); }},
        DeviceSpec{DeviceId::StreamDeckPlus, 0x0FD9, 0x0084, 14, [] { return std::make_unique<StreamDeckPlusParser>(); }},
    };
}

// A bus with nothing on it, for a process that hasn't allowed real devices. It never
// calls the device library.
class NoDeviceLayer : public HidLayer {
public:
    QSet<quint32> present() override
    {
        return {};
    }

    std::unique_ptr<HidDeviceHandle> open(quint16 /*vendorId*/, quint16 /*productId*/, QString* /*error*/) override
    {
        return nullptr;
    }
};

#ifdef NEREUS_HAVE_HIDAPI

class RealHidHandle : public HidDeviceHandle {
public:
    explicit RealHidHandle(hid_device* device)
        : m_device(device)
    {
    }

    ~RealHidHandle() override
    {
        const QMutexLocker locker(&UsbPresence::hidApiLock());
        hid_close(m_device);
    }

    int read(char* buffer, int size) override
    {
        return hid_read(m_device, reinterpret_cast<unsigned char*>(buffer), static_cast<size_t>(size));
    }

    int write(const QByteArray& report) override
    {
        return hid_write(m_device, reinterpret_cast<const unsigned char*>(report.constData()),
                         static_cast<size_t>(report.size()));
    }

private:
    hid_device* m_device;
};

class RealHidLayer : public HidLayer {
public:
    ~RealHidLayer() override
    {
        const QMutexLocker locker(&UsbPresence::hidApiLock());
        hid_exit();
    }

    QSet<quint32> present() override
    {
        return UsbPresence::connectedVidPids();
    }

    std::unique_ptr<HidDeviceHandle> open(quint16 vendorId, quint16 productId, QString* error) override
    {
        const QMutexLocker locker(&UsbPresence::hidApiLock());
        // From AetherSDR src/core/HidEncoderManager.cpp:481-488 [@1e0718ad]
        hid_device* const device = hid_open(vendorId, productId, nullptr);
        if (device == nullptr) {
            *error = QString::fromWCharArray(hid_error(nullptr));
            return nullptr;
        }
        hid_set_nonblocking(device, 1);
        return std::make_unique<RealHidHandle>(device);
    }
};

#endif // NEREUS_HAVE_HIDAPI

std::unique_ptr<HidLayer> makeLayer()
{
    if (testLayerFactory()) {
        return testLayerFactory()();
    }
#ifdef NEREUS_HAVE_HIDAPI
    if (!realDevicesAllowedFlag()) {
        return std::make_unique<NoDeviceLayer>();
    }
    return std::make_unique<RealHidLayer>();
#else
    return nullptr;
#endif
}

} // namespace

QString deviceOpenFailureReason(const QString& detail)
{
    // The words each platform's hid.c passes on: mach_error_string on macOS,
    // strerror on Linux, FormatMessage on Windows.
    const auto says = [&detail](const char* text) {
        return detail.contains(QLatin1String(text), Qt::CaseInsensitive);
    };
    if (says("exclusive access") || says("resource busy") || says("being used by another")
        || says("access is denied")) {
        // Windows denies access to a HID device another app holds alone.
        return QStringLiteral("Another app is using it. Close that app and it connects on its own.");
    }
    if (says("not permitted") || says("permission denied")) {
#if defined(Q_OS_MAC)
        return QStringLiteral("macOS is blocking it. Turn on NereusSDR in System Settings, "
                              "under Privacy & Security, Input Monitoring.");
#else
        return QStringLiteral("This computer's access rules block it. Copy 60-nereussdr-controllers.rules "
                              "to /etc/udev/rules.d, then plug it in again.");
#endif
    }
    return QStringLiteral("It couldn't be opened. Unplug it and plug it back in.");
}

DeviceInput::DeviceInput(QObject* parent)
    : QObject(parent)
    , m_layer(makeLayer())
    , m_table(testTable() ? *testTable() : realTable())
    , m_pollTimer(new QTimer(this))
    , m_presenceTimer(new QTimer(this))
{
    qRegisterMetaType<DeviceId>();
    qRegisterMetaType<DeviceEvent>();
    m_pollTimer->setInterval(kPollMs);
    m_presenceTimer->setInterval(kPresenceMs);
    connect(m_pollTimer, &QTimer::timeout, this, &DeviceInput::poll);
    connect(m_presenceTimer, &QTimer::timeout, this, &DeviceInput::checkPresence);
    if (kKernelOwnsPowerMate && realDevicesAllowedFlag() && !testLayerFactory()) {
        m_powerMateLinux = new PowerMateLinuxInput(this);
        connect(m_powerMateLinux, &PowerMateLinuxInput::deviceEvent, this,
                [this](const DeviceEvent& event) { emit deviceEvent(DeviceId::PowerMate, event); });
        connect(m_powerMateLinux, &PowerMateLinuxInput::wentAway, this, [this] {
            qCInfo(lcHotKeys) << HotKeyDeviceBindings::deviceName(DeviceId::PowerMate) << "went away";
            notify(DeviceId::PowerMate, false, notPluggedInReason());
        });
    }
}

DeviceInput::~DeviceInput()
{
    m_open.clear();
}

bool DeviceInput::supported()
{
#ifdef NEREUS_HAVE_HIDAPI
    return true;
#else
    return false;
#endif
}

void DeviceInput::allowRealDevices()
{
    realDevicesAllowedFlag() = true;
}

bool DeviceInput::realDevicesAllowed()
{
    return realDevicesAllowedFlag();
}

void DeviceInput::setLayerFactoryForTest(LayerFactory factory)
{
    testLayerFactory() = std::move(factory);
}

void DeviceInput::setDeviceTableForTest(const QList<DeviceSpec>& table)
{
    testTable() = table;
}

void DeviceInput::clearDeviceTableForTest()
{
    testTable().reset();
}

QList<DeviceId> DeviceInput::tableDevices() const
{
    QList<DeviceId> ids;
    for (const DeviceSpec& spec : m_table) {
        if (!ids.contains(spec.id)) {
            ids.append(spec.id);
        }
    }
    return ids;
}

void DeviceInput::start()
{
    if (m_started) {
        return;
    }
    m_started = true;
    const QList<DeviceId> ids = tableDevices();
    if (!m_layer) {
        for (const DeviceId id : ids) {
            notify(id, false, noSupportReason());
        }
        return;
    }
    for (const DeviceId id : ids) {
        notify(id, false, m_claimedElsewhere.contains(id) ? claimedReason() : notPluggedInReason());
    }
    checkPresence();
    m_presenceTimer->start();
}

void DeviceInput::stop()
{
    m_presenceTimer->stop();
    m_pollTimer->stop();
    m_started = false;
    while (!m_open.empty()) {
        const DeviceId id = m_open.begin()->first;
        closeDevice(id);
        notify(id, false, QString());
    }
    if (m_powerMateLinux != nullptr && m_powerMateLinux->isOpen()) {
        m_powerMateLinux->close();
        notify(DeviceId::PowerMate, false, QString());
    }
    m_states.clear();
}

void DeviceInput::sendOutput(DeviceId id, QByteArray report)
{
    const auto open = m_open.find(id);
    if (open == m_open.end()) {
        return;
    }
    // hid_write may return EIO on surprise-disconnect; safe to ignore.
    // [original inline comment from src/core/HidEncoderManager.cpp:568]
    if (open->second.handle->write(report) < 0) {
        qCDebug(lcHotKeyDevice) << "A write failed on" << HotKeyDeviceBindings::deviceName(id);
    }
}

void DeviceInput::setDeviceClaimedElsewhere(DeviceId id, bool claimed)
{
    if (claimed == m_claimedElsewhere.contains(id)) {
        return;
    }
    if (!claimed) {
        m_claimedElsewhere.remove(id);
        checkPresence();
        return;
    }
    m_claimedElsewhere.insert(id);
    closeDevice(id);
    if (id == DeviceId::PowerMate && m_powerMateLinux != nullptr && m_powerMateLinux->isOpen()) {
        m_powerMateLinux->close();
    }
    if (m_started && m_layer) {
        notify(id, false, claimedReason());
    }
}

void DeviceInput::checkPresence()
{
    if (!m_started || !m_layer) {
        return;
    }
    // First the bus, so nothing below runs for a device that isn't on it: opening
    // is what brings up the Input Monitoring prompt on macOS.
    const QSet<quint32> present = m_layer->present();
    for (const DeviceId id : tableDevices()) {
        if (m_claimedElsewhere.contains(id)) {
            notify(id, false, claimedReason());
            continue;
        }
        if (m_open.find(id) != m_open.end()) {
            continue;
        }
        if (id == DeviceId::PowerMate && m_powerMateLinux != nullptr && m_powerMateLinux->isOpen()) {
            continue;
        }
        bool seen = false;
        // First match wins: a device with two ids opens once.
        for (const DeviceSpec& spec : std::as_const(m_table)) {
            if (spec.id == id && spec.makeParser && present.contains(UsbPresence::key(spec.vendorId, spec.productId))) {
                seen = true;
                tryOpen(spec);
                break;
            }
        }
        if (seen) {
            continue;
        }
        if (id == DeviceId::PowerMate && m_powerMateLinux != nullptr) {
            m_powerMateLinux->check();
            if (m_powerMateLinux->isOpen()) {
                notify(id, true, QString());
                continue;
            }
            if (!m_powerMateLinux->failureReason().isEmpty()) {
                notify(id, false, m_powerMateLinux->failureReason());
                continue;
            }
        }
        notify(id, false, notPluggedInReason());
    }
}

void DeviceInput::tryOpen(const DeviceSpec& spec)
{
    QString error;
    std::unique_ptr<HidDeviceHandle> handle = m_layer->open(spec.vendorId, spec.productId, &error);
    if (!handle) {
        // It stays on the bus, so this runs again at the next check. Only a change
        // is worth a line in the log.
        const QString reason = deviceOpenFailureReason(error);
        if (m_states.value(spec.id).reason != reason) {
            qCWarning(lcHotKeys) << "Couldn't open" << HotKeyDeviceBindings::deviceName(spec.id) << error;
        }
        notify(spec.id, false, reason);
        return;
    }
    std::unique_ptr<DeviceParser>& parser = m_parsers[spec.id];
    if (!parser) {
        parser = spec.makeParser();
    }
    m_open[spec.id] = OpenDevice{std::move(handle), parser.get(), QByteArray(spec.reportSize, '\0')};
    m_pollTimer->start();
    qCInfo(lcHotKeys) << "Opened" << HotKeyDeviceBindings::deviceName(spec.id);
    notify(spec.id, true, QString());
}

void DeviceInput::closeDevice(DeviceId id)
{
    const auto open = m_open.find(id);
    if (open == m_open.end()) {
        return;
    }
    open->second.parser->reset();
    m_open.erase(open);
    if (m_open.empty()) {
        m_pollTimer->stop();
    }
}

void DeviceInput::poll()
{
    QList<DeviceId> gone;
    for (auto& [id, device] : m_open) {
        // Read all pending reports
        // [original inline comment from src/core/HidEncoderManager.cpp:589]
        for (int n = 0; n < kMaxReportsPerPoll; ++n) {
            const int res = device.handle->read(device.buffer.data(), static_cast<int>(device.buffer.size()));
            if (res < 0) {
                // Device disconnected
                // [original inline comment from src/core/HidEncoderManager.cpp:593]
                gone.append(id);
                break;
            }
            // no more data
            // [original inline comment from src/core/HidEncoderManager.cpp:599]
            if (res == 0) {
                break;
            }
            const QByteArrayView report(device.buffer.constData(), res);
            qCDebug(lcHotKeyDevice) << HotKeyDeviceBindings::deviceName(id) << report.toByteArray().toHex(' ');
            const QList<DeviceEvent> events = device.parser->parse(report);
            for (const DeviceEvent& event : events) {
                emit deviceEvent(id, event);
            }
        }
    }
    for (const DeviceId id : std::as_const(gone)) {
        qCInfo(lcHotKeys) << HotKeyDeviceBindings::deviceName(id) << "went away";
        closeDevice(id);
        notify(id, false, notPluggedInReason());
    }
}

void DeviceInput::notify(DeviceId id, bool connected, const QString& reason)
{
    const auto known = m_states.constFind(id);
    if (known != m_states.constEnd() && known->connected == connected && known->reason == reason) {
        return;
    }
    m_states.insert(id, State{connected, reason});
    emit deviceStateChanged(id, connected, reason);
}

} // namespace NereusSDR
