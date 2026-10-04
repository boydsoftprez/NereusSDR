// =================================================================
// App-local compatibility wrapper for Qt qtbase v6.11.0 Cocoa.
// Reference: src/plugins/platforms/cocoa/qcocoaaccessibilityelement.mm
// 219-226,257-268,312-340. Existing method bodies are forwarded, not copied.
// Modification history (NereusSDR):
// 2026-10-03 — JJ Boyd (KG4VCF), with OpenAI Codex assistance: correct
// synthetic elements deleting their borrowed parent accessibility ID.
// =================================================================
// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
// Qt-Security score:significant reason:default

#include "QtCocoaAccessibilityOwnershipGuard.h"
#include "QtCocoaAccessibilityOwnershipGuard_p.h"
#include <QApplication>
#include <QGuiApplication>
#include <QThread>
#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>
#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <pthread.h>
#include <array>
#include <cstdlib>
#include <cstring>

namespace {
using DeallocFunction = void (*)(id, SEL);
using RemoveFunction = void (*)(id, SEL, NSArray*);
struct GuardState {
    Class elementClass = Nil;
    Ivar role = nullptr;
    Ivar identifier = nullptr;
    Method dealloc = nullptr;
    Method remove = nullptr;
    DeallocFunction originalDealloc = nullptr;
    RemoveFunction originalRemove = nullptr;
    bool installed = false;
};
GuardState state;

bool synthetic(id element)
{
    return element && [element isKindOfClass:state.elementClass]
        && object_getIvar(element, state.role) != nil;
}

void guardedDealloc(id element, SEL selector)
{
    if (synthetic(element)) {
        // Qt v6.11.0 populateTableRow:312-340 assigns the parent's ID.
        // This element owns its arrays, but never the borrowed interface.
        const unsigned int absent = 0;
        std::memcpy(reinterpret_cast<char*>(element) + ivar_getOffset(state.identifier),
                    &absent, sizeof(absent));
    }
    state.originalDealloc(element, selector);
}

void guardedRemove(id cls, SEL selector, NSArray* elements)
{
    NSMutableArray* owned = [NSMutableArray arrayWithCapacity:elements.count];
    for (id element in elements) {
        if (!synthetic(element)) {
            [owned addObject:element];
        }
    }
    state.originalRemove(cls, selector, owned);
}

Method declaredMethod(Class cls, SEL selector)
{
    unsigned int count = 0;
    Method* methods = class_copyMethodList(cls, &count);
    Method found = nullptr;
    for (unsigned int n = 0; n < count; ++n) {
        if (method_getName(methods[n]) == selector) {
            found = methods[n];
            break;
        }
    }
    std::free(methods);
    return found;
}

bool exactMethod(Method method, const char* encoding)
{
    return method && std::strcmp(method_getTypeEncoding(method), encoding) == 0;
}

bool exactIvar(Class cls, const char* name, const char* encoding, ptrdiff_t offset)
{
    Ivar ivar = class_getInstanceVariable(cls, name);
    return ivar && std::strcmp(ivar_getTypeEncoding(ivar), encoding) == 0
        && ivar_getOffset(ivar) == offset;
}

const mach_header* supportedPlugin(Class cls)
{
    const char* classImage = class_getImageName(cls);
    if (!classImage) {
        return nullptr;
    }
    const char* leaf = std::strrchr(classImage, '/');
    if (!leaf || std::strcmp(leaf + 1, "libqcocoa.dylib") != 0) {
        return nullptr;
    }
    // Observed in the actual Qt 6.11.0 arm64 crash image and private native
    // red/green fixture. Same version text alone does not establish its ABI.
    constexpr std::array<unsigned char, 16> kPluginUuid {
        0x04,0x94,0x06,0x9c,0x87,0x39,0x31,0xd2,0xa9,0xf9,0x35,0x32,0x08,0x68,0x98,0xc8
    };
    for (uint32_t n = 0; n < _dyld_image_count(); ++n) {
        if (std::strcmp(classImage, _dyld_get_image_name(n)) != 0) {
            continue;
        }
        const mach_header* header = _dyld_get_image_header(n);
        if (header->magic != MH_MAGIC_64 || header->cputype != CPU_TYPE_ARM64) {
            return nullptr;
        }
        const auto* image = reinterpret_cast<const mach_header_64*>(header);
        const char* cursor = reinterpret_cast<const char*>(image + 1);
        const char* end = cursor + image->sizeofcmds;
        for (uint32_t command = 0; command < image->ncmds; ++command) {
            if (end - cursor < ptrdiff_t(sizeof(load_command))) {
                return nullptr;
            }
            const auto* load = reinterpret_cast<const load_command*>(cursor);
            if (load->cmdsize < sizeof(load_command) || load->cmdsize > size_t(end - cursor)) {
                return nullptr;
            }
            if (load->cmd == LC_UUID) {
                if (load->cmdsize != sizeof(uuid_command)) {
                    return nullptr;
                }
                const auto* uuid = reinterpret_cast<const uuid_command*>(load);
                return std::memcmp(uuid->uuid, kPluginUuid.data(), kPluginUuid.size()) == 0 ? header : nullptr;
            }
            cursor += load->cmdsize;
        }
    }
    return nullptr;
}

bool belongsToPlugin(IMP implementation, const mach_header* plugin)
{
    Dl_info image {};
    return implementation && dladdr(reinterpret_cast<const void*>(implementation), &image)
        && image.dli_fbase == plugin;
}
}

bool QtCocoaOwnershipDetail::classAbiMatches(void* objcClass)
{
    Class cls = static_cast<Class>(objcClass);
    return cls && class_getSuperclass(cls) == [NSObject class]
        && class_conformsToProtocol(cls, @protocol(NSAccessibilityElement))
        && class_getInstanceSize(cls) == 48
        && exactIvar(cls, "axid", "I", 8)
        && exactIvar(cls, "m_rowIndex", "i", 12)
        && exactIvar(cls, "m_columnIndex", "i", 16)
        && exactIvar(cls, "rows", "@\"NSMutableArray\"", 24)
        && exactIvar(cls, "columns", "@\"NSMutableArray\"", 32)
        && exactIvar(cls, "synthesizedRole", "@\"NSString\"", 40);
}

bool installQtCocoaAccessibilityOwnershipGuard(QString* rejectionReason)
{
    const auto reject = [rejectionReason](const char* message) {
        if (rejectionReason) {
            *rejectionReason = QString::fromLatin1(message);
        }
        return false;
    };
    if (rejectionReason) {
        rejectionReason->clear();
    }
    QGuiApplication* app = qobject_cast<QGuiApplication*>(QCoreApplication::instance());
    if (!app || !pthread_main_np() || QThread::currentThread() != app->thread()) {
        return reject("Qt Cocoa accessibility ownership correction requires the main GUI thread.");
    }
    // Only the identified Qt 6.11.0 Cocoa regression is corrected. Other Qt
    // versions/platforms retain their original behavior and receive no hooks.
    if (std::strcmp(qVersion(), "6.11.0") != 0
        || QGuiApplication::platformName() != QStringLiteral("cocoa")) {
        return true;
    }
    if (QT_VERSION != QT_VERSION_CHECK(6, 11, 0)) {
        return reject("Qt 6.11.0 Cocoa runtime does not match the compiled accessibility correction ABI.");
    }
    if (state.installed) {
        if (method_getImplementation(state.dealloc) == reinterpret_cast<IMP>(guardedDealloc)
            && method_getImplementation(state.remove) == reinterpret_cast<IMP>(guardedRemove)) {
            return true;
        }
        return reject("Qt Cocoa accessibility methods changed after ownership correction installation.");
    }
    if (!QApplication::allWidgets().isEmpty()) {
        return reject("Qt Cocoa accessibility ownership correction must be installed before widgets are created.");
    }
    Class cls = NSClassFromString(@"QMacAccessibilityElement");
    if (!QtCocoaOwnershipDetail::classAbiMatches(cls)) {
        return reject("Unsupported Qt Cocoa accessibility class or ivar ABI.");
    }
    Method dealloc = declaredMethod(cls, sel_registerName("dealloc"));
    Method remove = declaredMethod(object_getClass(cls), sel_registerName("removeElementsFromCache:"));
    if (!exactMethod(dealloc, "v16@0:8") || !exactMethod(remove, "v24@0:8@16")) {
        return reject("Unsupported Qt Cocoa accessibility method ABI.");
    }
    const mach_header* plugin = supportedPlugin(cls);
    IMP deallocImplementation = method_getImplementation(dealloc);
    IMP removeImplementation = method_getImplementation(remove);
    if (!plugin || !belongsToPlugin(deallocImplementation, plugin)
        || !belongsToPlugin(removeImplementation, plugin)) {
        return reject("Qt Cocoa plugin identity or accessibility method provenance is incompatible (possible interceptor).");
    }
    state.elementClass = cls;
    state.role = class_getInstanceVariable(cls, "synthesizedRole");
    state.identifier = class_getInstanceVariable(cls, "axid");
    state.dealloc = dealloc;
    state.remove = remove;
    state.originalDealloc = reinterpret_cast<DeallocFunction>(deallocImplementation);
    state.originalRemove = reinterpret_cast<RemoveFunction>(removeImplementation);
    IMP displacedDealloc = method_setImplementation(dealloc, reinterpret_cast<IMP>(guardedDealloc));
    if (displacedDealloc != deallocImplementation) {
        method_setImplementation(dealloc, displacedDealloc);
        return reject("Qt Cocoa dealloc changed during ownership correction installation.");
    }
    IMP displacedRemove = method_setImplementation(remove, reinterpret_cast<IMP>(guardedRemove));
    if (displacedRemove != removeImplementation) {
        method_setImplementation(remove, displacedRemove);
        method_setImplementation(dealloc, deallocImplementation);
        return reject("Qt Cocoa cache removal changed during ownership correction installation.");
    }
    state.installed = true;
    return true;
}
