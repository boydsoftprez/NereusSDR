// =================================================================
// src/core/AudioDeviceConfig.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. See AudioDeviceConfig.h for the full header.
//
// Sub-Phase 12 Task 12.2 (2026-04-20): implements loadFromSettings /
// saveToSettings helpers for the 10-field AppSettings round-trip
// required by the live-edit Setup → Audio → Devices page.
// =================================================================

#include "AudioDeviceConfig.h"
#include "AppSettings.h"

namespace NereusSDR {

// ---------------------------------------------------------------------------
// AudioDeviceConfig::loadFromSettings
//
// Reads the 10 fields under audio/<prefix>/{Key}.  On a fresh install (no
// keys present) every value falls back to the struct's in-class default so
// makeBus() treats the result as "platform default" — identical to the
// pre-Sub-Phase-12 behavior where ensureSpeakersOpen() used a bare
// AudioDeviceConfig{}.
// ---------------------------------------------------------------------------
AudioDeviceConfig AudioDeviceConfig::loadFromSettings(const QString& prefix)
{
    auto& s = AppSettings::instance();
    const QString base = prefix + QLatin1Char('/');

    AudioDeviceConfig cfg;

    cfg.driverApi = s.value(base + QStringLiteral("DriverApi"),
                            QString()).toString();

    cfg.deviceName = s.value(base + QStringLiteral("DeviceName"),
                             QString()).toString();

    cfg.sampleRate = s.value(base + QStringLiteral("SampleRate"),
                             QString::number(cfg.sampleRate)).toString().toInt();

    cfg.bitDepth = s.value(base + QStringLiteral("BitDepth"),
                           QString::number(cfg.bitDepth)).toString().toInt();

    cfg.channels = s.value(base + QStringLiteral("Channels"),
                           QString::number(cfg.channels)).toString().toInt();

    cfg.bufferSamples = s.value(base + QStringLiteral("BufferSamples"),
                                QString::number(cfg.bufferSamples)).toString().toInt();

    cfg.exclusiveMode =
        s.value(base + QStringLiteral("ExclusiveMode"),
                QStringLiteral("False")).toString() == QStringLiteral("True");

    cfg.eventDriven =
        s.value(base + QStringLiteral("EventDriven"),
                QStringLiteral("False")).toString() == QStringLiteral("True");

    cfg.bypassMixer =
        s.value(base + QStringLiteral("BypassMixer"),
                QStringLiteral("False")).toString() == QStringLiteral("True");

    cfg.manualLatencyMs =
        s.value(base + QStringLiteral("ManualLatencyMs"),
                QString::number(cfg.manualLatencyMs)).toString().toInt();

    // hostApiIndex is a PortAudio runtime value — not persisted directly.
    // It remains -1 (PortAudio default) on load; the DeviceCard / AudioEngine
    // may resolve it from driverApi at open time in a future sub-phase.
    cfg.hostApiIndex = -1;

    return cfg;
}

// ---------------------------------------------------------------------------
// AudioDeviceConfig::saveToSettings
// ---------------------------------------------------------------------------
void AudioDeviceConfig::saveToSettings(const QString& prefix) const
{
    auto& s = AppSettings::instance();
    const QString base = prefix + QLatin1Char('/');

    s.setValue(base + QStringLiteral("DriverApi"),     driverApi);
    s.setValue(base + QStringLiteral("DeviceName"),    deviceName);
    s.setValue(base + QStringLiteral("SampleRate"),    QString::number(sampleRate));
    s.setValue(base + QStringLiteral("BitDepth"),      QString::number(bitDepth));
    s.setValue(base + QStringLiteral("Channels"),      QString::number(channels));
    s.setValue(base + QStringLiteral("BufferSamples"), QString::number(bufferSamples));
    s.setValue(base + QStringLiteral("ExclusiveMode"),
               exclusiveMode ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(base + QStringLiteral("EventDriven"),
               eventDriven ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(base + QStringLiteral("BypassMixer"),
               bypassMixer ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(base + QStringLiteral("ManualLatencyMs"),
               QString::number(manualLatencyMs));
}

} // namespace NereusSDR
