// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 12.
//                AI-assisted implementation via Anthropic Claude Code.
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 16: HID product names, for a
//                device with no ids to look for. AI-assisted via Anthropic Claude Code.
// no-port-check: NereusSDR-original.
#include "gui/hotkeys/devices/UsbPresence.h"

#include <QMutex>
#include <QMutexLocker>
#include <QtGlobal>

#if defined(Q_OS_MAC)
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#elif defined(NEREUS_HAVE_HIDAPI)
#include "hidapi/hidapi.h"
#endif

namespace NereusSDR {

namespace {

#if defined(Q_OS_MAC)

// kUSBVendorID and kUSBProductID in <IOKit/usb/USBSpec.h>.
bool readRegistryNumber(io_object_t device, CFStringRef name, int* value)
{
    const CFTypeRef property = IORegistryEntryCreateCFProperty(device, name, kCFAllocatorDefault, 0);
    if (property == nullptr) {
        return false;
    }
    const bool ok = CFGetTypeID(property) == CFNumberGetTypeID()
        && CFNumberGetValue(static_cast<CFNumberRef>(property), kCFNumberIntType, value);
    CFRelease(property);
    return ok;
}

#endif

} // namespace

QSet<quint32> UsbPresence::connectedVidPids()
{
    QSet<quint32> present;
#if defined(Q_OS_MAC)
    // IOServiceGetMatchingServices consumes the dictionary. MACH_PORT_NULL is the
    // default main port on every SDK (kIOMainPortDefault, kIOMasterPortDefault).
    io_iterator_t devices = 0;
    if (IOServiceGetMatchingServices(MACH_PORT_NULL, IOServiceMatching("IOUSBHostDevice"), &devices) != KERN_SUCCESS) {
        return present;
    }
    while (const io_object_t device = IOIteratorNext(devices)) {
        int vendorId = 0;
        int productId = 0;
        if (readRegistryNumber(device, CFSTR("idVendor"), &vendorId)
            && readRegistryNumber(device, CFSTR("idProduct"), &productId)) {
            present.insert(key(static_cast<quint16>(vendorId), static_cast<quint16>(productId)));
        }
        IOObjectRelease(device);
    }
    IOObjectRelease(devices);
#elif defined(NEREUS_HAVE_HIDAPI)
    const QMutexLocker locker(&hidApiLock());
    hid_device_info* const all = hid_enumerate(0, 0);
    for (const hid_device_info* info = all; info != nullptr; info = info->next) {
        present.insert(key(info->vendor_id, info->product_id));
    }
    hid_free_enumeration(all);
#endif
    return present;
}

QStringList UsbPresence::hidProductNames()
{
    QStringList names;
#if defined(Q_OS_MAC)
    // Each HID interface is an IOHIDDevice service with a Product property. "Product"
    // is kIOHIDProductKey in the SDK's IOHIDDeviceKeys.h:74, spelled out so this file
    // includes no HID header.
    io_iterator_t devices = 0;
    if (IOServiceGetMatchingServices(MACH_PORT_NULL, IOServiceMatching("IOHIDDevice"), &devices) != KERN_SUCCESS) {
        return names;
    }
    while (const io_object_t device = IOIteratorNext(devices)) {
        const CFTypeRef property =
            IORegistryEntryCreateCFProperty(device, CFSTR("Product"), kCFAllocatorDefault, 0);
        if (property != nullptr) {
            if (CFGetTypeID(property) == CFStringGetTypeID()) {
                char buffer[256] = {};
                if (CFStringGetCString(static_cast<CFStringRef>(property), buffer, sizeof(buffer),
                                       kCFStringEncodingUTF8)) {
                    names.append(QString::fromUtf8(buffer));
                }
            }
            CFRelease(property);
        }
        IOObjectRelease(device);
    }
    IOObjectRelease(devices);
#endif
    return names;
}

QMutex& UsbPresence::hidApiLock()
{
    static QMutex kLock;
    return kLock;
}

} // namespace NereusSDR
