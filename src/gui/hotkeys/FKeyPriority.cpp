// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/gui/hotkeys/FKeyPriority.cpp  (Longpath)
// =================================================================
//
// Longpath-original; siehe FKeyPriority.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-11 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include "gui/hotkeys/FKeyPriority.h"

#include "core/AppSettings.h"
#include "core/hotkeys/HotKeyAction.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <cstdlib>
#include <utility>

#ifdef Q_OS_MACOS
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/hidsystem/IOHIDLib.h>
#include <IOKit/hidsystem/IOHIDParameter.h>
#endif

namespace Longpath {

namespace {

// Cmd+Q endet in exit() mitten in closeEvent -- kein Destruktor laeuft,
// nur atexit. Darum haengt die Rueckstellung auch dort.
FKeyPriority* s_live = nullptr;

void releaseAtExit()
{
    if (s_live != nullptr) {
        s_live->release();
    }
}

#ifdef Q_OS_MACOS

io_connect_t openHidSystem()
{
    const io_service_t service =
        IOServiceGetMatchingService(MACH_PORT_NULL, IOServiceMatching(kIOHIDSystemClass));
    if (service == IO_OBJECT_NULL) {
        return IO_OBJECT_NULL;
    }
    io_connect_t connect = IO_OBJECT_NULL;
    const kern_return_t kr = IOServiceOpen(service, mach_task_self(), kIOHIDParamConnectType, &connect);
    IOObjectRelease(service);
    return kr == KERN_SUCCESS ? connect : IO_OBJECT_NULL;
}

int readMacMode()
{
    const io_connect_t connect = openHidSystem();
    if (connect == IO_OBJECT_NULL) {
        return -1;
    }
    unsigned int mode = 0;
    IOByteCount size = 0;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    const kern_return_t kr =
        IOHIDGetParameter(connect, CFSTR(kIOHIDFKeyModeKey), sizeof(mode), &mode, &size);
#pragma clang diagnostic pop
    IOServiceClose(connect);
    if (kr != KERN_SUCCESS || size != sizeof(mode)) {
        return -1;
    }
    return static_cast<int>(mode);
}

bool writeMacMode(int mode)
{
    const io_connect_t connect = openHidSystem();
    if (connect == IO_OBJECT_NULL) {
        return false;
    }
    const int value = mode;
    const CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &value);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    const kern_return_t kr = IOHIDSetCFTypeParameter(connect, CFSTR(kIOHIDFKeyModeKey), number);
#pragma clang diagnostic pop
    CFRelease(number);
    IOServiceClose(connect);
    return kr == KERN_SUCCESS;
}

#endif

} // namespace

FKeyPriority::FKeyPriority(Backend backend, QString markerPath, QObject* parent)
    : QObject(parent)
    , m_backend(std::move(backend))
    , m_markerPath(std::move(markerPath))
{
    // Die Marke ueberlebt nur einen Absturz waehrend der Umstellung:
    // dann steht das System noch auf F-Tasten.
    if (!m_markerPath.isEmpty() && QFile::exists(m_markerPath)) {
        if (read() == kModeFKeys) {
            qCInfo(lcHotKeys) << "F-Tasten: Umstellung vom letzten Lauf zurueckgenommen";
            write(kModeSpecial);
        }
        setMarker(false);
    }

    s_live = this;
    static const bool registered = std::atexit(&releaseAtExit) == 0;
    Q_UNUSED(registered);
}

FKeyPriority::~FKeyPriority()
{
    release();
    if (s_live == this) {
        s_live = nullptr;
    }
}

void FKeyPriority::setWanted(bool on)
{
    m_wanted = on;
    apply();
}

void FKeyPriority::setActive(bool on)
{
    m_active = on;
    apply();
}

void FKeyPriority::setHoldWhileRunning(bool on)
{
    if (m_hold == on) {
        return;
    }
    m_hold = on;
    apply();
}

void FKeyPriority::release()
{
    if (!m_engaged) {
        return;
    }
    m_engaged = false;
    if (write(kModeSpecial)) {
        qCInfo(lcHotKeys) << "F-Tasten: zurueck auf die Mac-Belegung";
    } else {
        qCWarning(lcHotKeys) << "F-Tasten: Rueckstellen hat macOS nicht angenommen";
    }
    setMarker(false);
}

void FKeyPriority::setBackendForTest(Backend backend)
{
    release();
    m_backend = std::move(backend);
    apply();
}

void FKeyPriority::apply()
{
    // Durchhalten (m_hold) macht das vordere Programm gleichgueltig:
    // der Modus bleibt gestellt, solange der Wunsch steht. Ohne das
    // landet ein F10 fuer den Sprachspeicher am Mac, sobald Longpath
    // nicht genau in diesem Augenblick vorne ist -- und F10 ist am Mac
    // die Stummtaste. Begruendung im Kopf von FKeyPriority.h.
    if (!(m_wanted && (m_active || m_hold))) {
        release();
        return;
    }
    if (m_engaged) {
        return;
    }
    // -1: kein Mac oder nicht lesbar. 1: der Betreiber hat es im System
    // schon so -- dann gehoert der Modus ihm, nicht uns.
    if (read() != kModeSpecial) {
        return;
    }

    // Erst die Marke, dann umstellen: ein Absturz dazwischen ist so
    // gedeckt.
    setMarker(true);
    if (!write(kModeFKeys) || read() != kModeFKeys) {
        setMarker(false);
        qCWarning(lcHotKeys) << "F-Tasten: macOS hat die Umstellung nicht angenommen";
        if (!m_failedOnce) {
            m_failedOnce = true;
            emit failed(QStringLiteral("macOS did not let Longpath make F1…F12 plain F keys — "
                                       "hold fn for the voice keyer keys."));
        }
        return;
    }
    m_engaged = true;
    // Ob es geklappt hat, soll man im Protokoll sehen -- ohne diese Zeile
    // sah ein erfolgreicher Lauf genauso aus wie einer, der nie vorne war.
    qCInfo(lcHotKeys) << (m_hold
        ? "F-Tasten: F1…F12 sind F-Tasten, solange Longpath laeuft"
        : "F-Tasten: F1…F12 sind F-Tasten, solange Longpath vorne ist");
}

int FKeyPriority::read() const
{
    return m_backend.read ? m_backend.read() : -1;
}

bool FKeyPriority::write(int mode) const
{
    return m_backend.write ? m_backend.write(mode) : false;
}

void FKeyPriority::setMarker(bool on) const
{
    if (m_markerPath.isEmpty()) {
        return;
    }
    if (!on) {
        QFile::remove(m_markerPath);
        return;
    }
    QDir().mkpath(QFileInfo(m_markerPath).absolutePath());
    QFile file(m_markerPath);
    if (!file.open(QIODevice::WriteOnly)) {
        qCWarning(lcHotKeys) << "F-Tasten: Marke nicht schreibbar" << m_markerPath;
    }
}

FKeyPriority::Backend FKeyPriority::systemBackend()
{
#ifdef Q_OS_MACOS
    return Backend{&readMacMode, &writeMacMode};
#else
    return Backend{};
#endif
}

QString FKeyPriority::defaultMarkerPath()
{
    const QString settingsFile = AppSettings::instance().filePath();
    if (settingsFile.isEmpty()) {
        return QString();
    }
    return QFileInfo(settingsFile).absoluteDir().filePath(QStringLiteral("fkeys-switched.mark"));
}

} // namespace Longpath
