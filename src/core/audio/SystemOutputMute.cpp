// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/core/audio/SystemOutputMute.cpp  (Longpath)
// =================================================================
// Siehe SystemOutputMute.h fuer die Begruendung.
//
// Modification history (Longpath):
//   2026-11-14 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================
#include "SystemOutputMute.h"

#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(lcAudio)

namespace Longpath {

namespace {
SystemOutputMute::Backend g_testBackend{nullptr, nullptr};
}  // namespace

void SystemOutputMute::setBackendForTest(const Backend& backend)
{
    g_testBackend = backend;
}

}  // namespace Longpath

// ---------------------------------------------------------------- macOS

#ifdef Q_OS_MAC

#include <CoreAudio/CoreAudio.h>

namespace Longpath {

namespace {

// Der Vorgabe-Ausgang; kAudioObjectUnknown, wenn es keinen gibt.
AudioDeviceID vorgabeAusgang()
{
    AudioObjectPropertyAddress adresse = {
        kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain,
    };
    AudioDeviceID geraet = kAudioObjectUnknown;
    UInt32 groesse = sizeof(geraet);
    const OSStatus st = AudioObjectGetPropertyData(
        kAudioObjectSystemObject, &adresse, 0, nullptr, &groesse, &geraet);
    if (st != noErr) {
        return kAudioObjectUnknown;
    }
    return geraet;
}

AudioObjectPropertyAddress stummAdresse()
{
    return AudioObjectPropertyAddress{
        kAudioDevicePropertyMute,
        kAudioObjectPropertyScopeOutput,
        kAudioObjectPropertyElementMain,
    };
}

AudioObjectPropertyAddress lautstaerkeAdresse()
{
    return AudioObjectPropertyAddress{
        kAudioDevicePropertyVolumeScalar,
        kAudioObjectPropertyScopeOutput,
        kAudioObjectPropertyElementMain,
    };
}

}  // namespace

SystemOutputMute::State SystemOutputMute::isMuted()
{
    if (g_testBackend.read != nullptr) {
        return g_testBackend.read();
    }

    const AudioDeviceID geraet = vorgabeAusgang();
    if (geraet == kAudioObjectUnknown) {
        return Unknown;
    }

    // Der Schalter selbst -- dasselbe, was `osascript -e 'get volume
    // settings'` als `output muted` zeigt.
    AudioObjectPropertyAddress adresse = stummAdresse();
    if (AudioObjectHasProperty(geraet, &adresse)) {
        UInt32 stumm = 0;
        UInt32 groesse = sizeof(stumm);
        const OSStatus st = AudioObjectGetPropertyData(
            geraet, &adresse, 0, nullptr, &groesse, &stumm);
        if (st == noErr) {
            return stumm != 0 ? Muted : NotMuted;
        }
    }

    // Kein Stummschalter am Geraet (kommt bei manchen USB- und
    // Netzgeraeten vor): dann ist eine Lautstaerke von 0 die einzige
    // Stille, die man von hier aus sehen kann. Abwesenheit des
    // Schalters ist KEINE Aussage ueber den Zustand -- deshalb wird
    // hier nicht NotMuted geraten.
    adresse = lautstaerkeAdresse();
    if (AudioObjectHasProperty(geraet, &adresse)) {
        Float32 pegel = -1.0f;
        UInt32 groesse = sizeof(pegel);
        const OSStatus st = AudioObjectGetPropertyData(
            geraet, &adresse, 0, nullptr, &groesse, &pegel);
        if (st == noErr && pegel >= 0.0f) {
            return pegel <= 0.0f ? Muted : NotMuted;
        }
    }

    return Unknown;
}

bool SystemOutputMute::setMuted(bool on)
{
    if (g_testBackend.write != nullptr) {
        return g_testBackend.write(on);
    }

    const AudioDeviceID geraet = vorgabeAusgang();
    if (geraet == kAudioObjectUnknown) {
        return false;
    }

    AudioObjectPropertyAddress adresse = stummAdresse();
    if (!AudioObjectHasProperty(geraet, &adresse)) {
        return false;
    }
    Boolean schreibbar = false;
    if (AudioObjectIsPropertySettable(geraet, &adresse, &schreibbar) != noErr
        || !schreibbar) {
        return false;
    }

    const UInt32 wert = on ? 1 : 0;
    return AudioObjectSetPropertyData(geraet, &adresse, 0, nullptr,
                                      sizeof(wert), &wert) == noErr;
}

}  // namespace Longpath

#else  // ------------------------------------------------- Linux / Windows

namespace Longpath {

SystemOutputMute::State SystemOutputMute::isMuted()
{
    if (g_testBackend.read != nullptr) {
        return g_testBackend.read();
    }
    return Unknown;
}

bool SystemOutputMute::setMuted(bool on)
{
    if (g_testBackend.write != nullptr) {
        return g_testBackend.write(on);
    }
    (void)on;
    return false;
}

}  // namespace Longpath

#endif
