// =================================================================
// src/gui/hotkeys/devices/UlanziMacInput.cpp  (NereusSDR)
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
//   2026-11-06 - Ulanzi Dial on macOS built on AetherSDR
//                 src/core/UlanziDialMacOSManager.cpp [@1e0718ad] for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation via
//                 Anthropic Claude Code (Claude Opus 5.5). Hot keys plan Task
//                 16. Nothing is created until the IOKit registry shows the dial
//                 and the application allows real devices, where upstream opens
//                 at start. Checks the open result and closes when it fails. Counts
//                 interfaces, so one going away does not read as the device
//                 gone. Closes the manager when the last goes away. Schedules in
//                 the common modes, so a release arrives while a menu is open.
// =================================================================

#include "gui/hotkeys/devices/UlanziMacInput.h"

#include "core/LogCategories.h"
#include "gui/hotkeys/devices/DeviceInput.h"
#include "gui/hotkeys/devices/UsbPresence.h"

#include <QMetaObject>
#include <QStringList>
#include <QTimer>

#include <optional>
#include <utility>

#if defined(Q_OS_MAC)
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDElement.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <IOKit/hid/IOHIDManager.h>
#include <IOKit/hid/IOHIDValue.h>
#include <mach/mach_error.h>
#endif

namespace NereusSDR {

namespace {

UlanziMacInput::LayerFactory& testLayerFactory()
{
    static UlanziMacInput::LayerFactory kFactory;
    return kFactory;
}

// Nothing on it, for a process that hasn't allowed real devices. It reads no registry
// and creates no manager.
class NoDialMacLayer : public UlanziMacLayer {
public:
    bool dialPresent() override
    {
        return false;
    }

    std::unique_ptr<UlanziMacSession> open(UlanziMacSink* /*sink*/, QString* /*detail*/) override
    {
        return nullptr;
    }
};

#if defined(Q_OS_MAC)

// From AetherSDR src/core/UlanziDialMacOSManager.cpp:54 [@1e0718ad]
const char* const kDialProductName = "Ulanzi Dial";

void onMatched(void* context, IOReturn /*result*/, void* /*sender*/, IOHIDDeviceRef device)
{
    static_cast<UlanziMacSink*>(context)->dialMatched(device);
}

void onRemoved(void* context, IOReturn /*result*/, void* /*sender*/, IOHIDDeviceRef device)
{
    static_cast<UlanziMacSink*>(context)->dialRemoved(device);
}

void onValue(void* context, IOReturn /*result*/, void* /*sender*/, IOHIDValueRef value)
{
    // From AetherSDR src/core/UlanziDialMacOSManager.cpp:107-116 [@1e0718ad]
    IOHIDElementRef element = IOHIDValueGetElement(value);
    const quint32 usagePage = IOHIDElementGetUsagePage(element);
    const quint32 usage = IOHIDElementGetUsage(element);
    // Only the dial's own one-bit keys are read as a number.
    const bool known = usagePage <= 0xFF && usage <= 0xFFFF
        && UlanziChordDecoder::keyCodeFromHidUsage(static_cast<quint8>(usagePage), static_cast<quint16>(usage)).has_value();
    if (known) {
        static_cast<UlanziMacSink*>(context)->dialValue(usagePage, usage, IOHIDValueGetIntegerValue(value));
    }
}

class RealMacSession : public UlanziMacSession {
public:
    explicit RealMacSession(IOHIDManagerRef manager)
        : m_manager(manager)
    {
    }

    ~RealMacSession() override
    {
        // From AetherSDR src/core/UlanziDialMacOSManager.cpp:95-97 [@1e0718ad]
        IOHIDManagerClose(m_manager, kIOHIDOptionsTypeNone);
        IOHIDManagerUnscheduleFromRunLoop(m_manager, CFRunLoopGetMain(), kCFRunLoopCommonModes);
        CFRelease(m_manager);
    }

private:
    IOHIDManagerRef m_manager;
};

std::unique_ptr<UlanziMacSession> openManager(UlanziMacSink* sink, QString* detail)
{
    // Build a matching dictionary so IOHIDManager only surfaces our dial,
    // not every keyboard on the machine.  Match by product-name substring
    // "Ulanzi Dial".
    // [original inline comment from src/core/UlanziDialMacOSManager.cpp:46-48]
    // Matching with no dictionary means every device, and seizing that is every
    // keyboard. So no dictionary, no manager. And the property match is whole-string,
    // the same comparison dialPresent makes.
    CFStringRef product = CFStringCreateWithCString(kCFAllocatorDefault, kDialProductName, kCFStringEncodingUTF8);
    CFMutableDictionaryRef match = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
                                                             &kCFTypeDictionaryValueCallBacks);
    IOHIDManagerRef manager = nullptr;
    if (product != nullptr && match != nullptr) {
        CFDictionarySetValue(match, CFSTR(kIOHIDProductKey), product);
        manager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
    }
    if (manager != nullptr) {
        IOHIDManagerSetDeviceMatching(manager, match);
    }
    if (match != nullptr) {
        CFRelease(match);
    }
    if (product != nullptr) {
        CFRelease(product);
    }
    if (manager == nullptr) {
        return nullptr;
    }

    IOHIDManagerRegisterDeviceMatchingCallback(manager, &onMatched, sink);
    IOHIDManagerRegisterDeviceRemovalCallback(manager, &onRemoved, sink);
    IOHIDManagerRegisterInputValueCallback(manager, &onValue, sink);

    // Common modes, where upstream has the default mode: a hold's release has to
    // arrive while a menu is being tracked.
    IOHIDManagerScheduleWithRunLoop(manager, CFRunLoopGetMain(), kCFRunLoopCommonModes);

    // The session closes and frees whatever happens next, so a refused open leaves
    // nothing seized.
    auto session = std::make_unique<RealMacSession>(manager);

    // Open with seize-device so the dial's events stop reaching the
    // OS keyboard stack — the macOS equivalent of Linux EVIOCGRAB.
    // [original inline comment from src/core/UlanziDialMacOSManager.cpp:86-87]
    const IOReturn result = IOHIDManagerOpen(manager, kIOHIDOptionsTypeSeizeDevice);
    if (result != kIOReturnSuccess) {
        *detail = QString::fromLatin1(mach_error_string(result));
        return nullptr;
    }
    return session;
}

class RealMacLayer : public UlanziMacLayer {
public:
    bool dialPresent() override
    {
        return UsbPresence::hidProductNames().contains(QLatin1String(kDialProductName));
    }

    std::unique_ptr<UlanziMacSession> open(UlanziMacSink* sink, QString* detail) override
    {
        return UlanziMacInput::openRealSession(sink, detail);
    }
};

#endif // Q_OS_MAC

std::unique_ptr<UlanziMacLayer> makeLayer()
{
    if (testLayerFactory()) {
        return testLayerFactory()();
    }
#if defined(Q_OS_MAC)
    // Only the application turns this on, so no test can raise the prompt.
    if (!DeviceInput::realDevicesAllowed()) {
        return std::make_unique<NoDialMacLayer>();
    }
    return std::make_unique<RealMacLayer>();
#else
    return std::make_unique<NoDialMacLayer>();
#endif
}

} // namespace

std::unique_ptr<UlanziMacSession> UlanziMacInput::openRealSession(UlanziMacSink* sink, QString* detail)
{
    // The second lock on the one call that can raise the prompt for every Mac user.
    if (!DeviceInput::realDevicesAllowed()) {
        return nullptr;
    }
#if defined(Q_OS_MAC)
    return openManager(sink, detail);
#else
    Q_UNUSED(sink);
    Q_UNUSED(detail);
    return nullptr;
#endif
}

UlanziMacInput::UlanziMacInput(QObject* parent)
    : UlanziDialInput(parent)
    , m_layer(makeLayer())
    , m_presenceTimer(new QTimer(this))
{
    qRegisterMetaType<DeviceId>();
    qRegisterMetaType<DeviceEvent>();
    m_presenceTimer->setInterval(kPresenceMs);
    connect(m_presenceTimer, &QTimer::timeout, this, &UlanziMacInput::checkPresence);
}

UlanziMacInput::~UlanziMacInput()
{
    m_session.reset();
}

void UlanziMacInput::setLayerFactoryForTest(LayerFactory factory)
{
    testLayerFactory() = std::move(factory);
}

void UlanziMacInput::start()
{
    if (m_wanted) {
        return;
    }
    m_wanted = true;
    notify(false, notPluggedInReason());
    m_presenceTimer->start();
    checkPresence();
}

void UlanziMacInput::stop()
{
    m_wanted = false;
    m_presenceTimer->stop();
    m_checksToWait = 0;
    m_devices.clear();
    m_session.reset();
    if (m_opened) {
        m_opened = false;
        keysGone(QString());
    }
    forgetState();
}

void UlanziMacInput::checkPresence()
{
    if (!m_wanted || m_session) {
        return;
    }
    if (!m_layer->dialPresent()) {
        m_checksToWait = 0;
        notify(false, notPluggedInReason());
        return;
    }
    if (m_checksToWait > 0) {
        --m_checksToWait;
        return;
    }
    QString detail;
    m_session = m_layer->open(this, &detail);
    if (!m_session) {
        const QString reason = deviceOpenFailureReason(detail);
        if (reason != lastReason()) {
            qCWarning(lcHotKeys) << "Couldn't open the Ulanzi Dial:" << detail;
        }
        notify(false, reason);
        m_checksToWait = kChecksBetweenTries;
        return;
    }
    // Its callbacks now say what's there, and the registry needn't be read.
    m_presenceTimer->stop();
}

void UlanziMacInput::dialMatched(const void* device)
{
    m_devices.insert(device);
    if (!m_opened) {
        m_opened = true;
        qCInfo(lcHotKeys) << "Opened the Ulanzi Dial";
        notify(true, QString());
    }
}

void UlanziMacInput::dialRemoved(const void* device)
{
    m_devices.remove(device);
    if (!m_devices.isEmpty() || !m_opened) {
        return;
    }
    qCInfo(lcHotKeys) << "The Ulanzi Dial went away";
    m_opened = false;
    keysGone(notPluggedInReason());
    // Not from inside the manager's own callback.
    QMetaObject::invokeMethod(this, &UlanziMacInput::closeIfNoDevices, Qt::QueuedConnection);
}

void UlanziMacInput::closeIfNoDevices()
{
    if (!m_wanted || !m_session || !m_devices.isEmpty()) {
        return;
    }
    m_session.reset();
    m_presenceTimer->start();
}

void UlanziMacInput::dialValue(quint32 usagePage, quint32 usage, qint64 value)
{
    if (usagePage > 0xFF || usage > 0xFFFF) {
        return;
    }
    // Consumer page = 0x0C, Keyboard/Keypad page = 0x07.
    // [original inline comment from src/core/UlanziDialMacOSManager.cpp:159]
    const std::optional<int> key =
        UlanziChordDecoder::keyCodeFromHidUsage(static_cast<quint8>(usagePage), static_cast<quint16>(usage));
    if (key.has_value()) {
        feedKey(*key, value != 0 ? 1 : 0);
    }
}

} // namespace NereusSDR
