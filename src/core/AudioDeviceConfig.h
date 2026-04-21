#pragma once

// =================================================================
// src/core/AudioDeviceConfig.h  (NereusSDR)
// =================================================================
//
// Phase 3O VAX per-endpoint audio device configuration. NereusSDR-
// original. Carried into AudioEngine setSpeakersConfig / setTxInputConfig
// / setVaxConfig, internally translated into a backend-specific
// PortAudioConfig (or CoreAudioHalBus / LinuxPipeBus config in
// Sub-Phases 5/6). Design spec: docs/architecture/2026-04-19-vax-design.md
// §5.3.
//
// Sub-Phase 12 Task 12.2 (2026-04-20): extended with 5 additional fields
// (driverApi, bitDepth, eventDriven, bypassMixer, manualLatencyMs) plus
// loadFromSettings / saveToSettings helpers for the 10-field AppSettings
// round-trip required by the live-edit Devices page.
// =================================================================

#include <QString>
#include <QMetaType>

namespace NereusSDR {

class AppSettings;

struct AudioDeviceConfig {
    // ── Original 6 fields ───────────────────────────────────────────────────
    QString deviceName;            // empty = platform default
    int     sampleRate    = 48000;
    int     channels      = 2;
    int     bufferSamples = 256;
    bool    exclusiveMode = false; // WASAPI only
    int     hostApiIndex  = -1;    // -1 = PortAudio default host API

    // ── Sub-Phase 12 additional 5 fields ───────────────────────────────────
    // Human-readable host API name (e.g. "Windows WASAPI", "CoreAudio",
    // "ALSA"). Empty string means "use PortAudio default".
    // driverApi is the Settings round-trip form; hostApiIndex is the
    // PortAudio runtime form. loadFromSettings leaves hostApiIndex at -1;
    // the value is resolved from driverApi at the callsite that opens the
    // bus (currently DeviceCard::currentConfig via the combo's itemData).
    QString driverApi;             // empty = PortAudio default
    int     bitDepth      = 32;    // bit depth hint for WASAPI/ASIO: 16/24/32
    bool    eventDriven   = false; // WASAPI event-driven mode
    bool    bypassMixer   = false; // WASAPI stream-flags bypass-mixer
    int     manualLatencyMs = 0;   // 0 = let PortAudio pick; >0 = explicit

    // ── Settings round-trip helpers (10 fields) ─────────────────────────────
    // loadFromSettings reads audio/<prefix>/{DriverApi,DeviceName,...} keys.
    // On a fresh install where none of the keys exist, returns a
    // default-constructed AudioDeviceConfig (empty deviceName → platform
    // default), which preserves the pre-Sub-Phase-12 behavior exactly.
    static AudioDeviceConfig loadFromSettings(const QString& prefix);

    // saveToSettings writes all 10 fields under audio/<prefix>/<Key>.
    void saveToSettings(const QString& prefix) const;
};

} // namespace NereusSDR

// Required for cross-thread signal delivery of AudioDeviceConfig via Qt
// auto-queued connections. Must be outside the namespace.
Q_DECLARE_METATYPE(NereusSDR::AudioDeviceConfig)
