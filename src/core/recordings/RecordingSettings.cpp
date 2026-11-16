// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original recording settings.
// Modification history (NereusSDR):
// 2026-11-15  J.J. Boyd / KG4VCF; the Core's recording and Digital
//             Spectrum Recorder settings (spec 6.5, 6.7). AI tooling:
//             Anthropic Claude Code.

#include "RecordingSettings.h"

#include "core/AppSettings.h"

#include <QDir>
#include <QStandardPaths>

#include <algorithm>

namespace NereusSDR {

namespace {

const QString kKeepMyVoice = QStringLiteral("RecordingsKeepMyVoice");
const QString kSidesApart = QStringLiteral("RecordingsSidesApart");
const QString kAutoStartOnTransmit = QStringLiteral("RecordingsAutoStartOnTransmit");
const QString kAutoStopWhenQuiet = QStringLiteral("RecordingsAutoStopWhenQuiet");
const QString kQuietMinutes = QStringLiteral("RecordingsQuietMinutes");
const QString kFormat = QStringLiteral("RecordingsFormat");
const QString kDisk = QStringLiteral("RecordingsDisk");
const QString kOnAirUseTxEq = QStringLiteral("RecordingsOnAirUseTxEq");
const QString kOnAirUseCompressor = QStringLiteral("RecordingsOnAirUseCompressor");
const QString kOnAirUseLeveler = QStringLiteral("RecordingsOnAirUseLeveler");
const QString kOnAirUseCfc = QStringLiteral("RecordingsOnAirUseCfc");
const QString kOnAirUsePhaseRotator = QStringLiteral("RecordingsOnAirUsePhaseRotator");
const QString kDsrEnabled = QStringLiteral("DsrEnabled");
const QString kDsrDisk = QStringLiteral("DsrDisk");
const QString kDsrSizeLimitGb = QStringLiteral("DsrSizeLimitGb");

const QString kTrue = QStringLiteral("True");
const QString kFalse = QStringLiteral("False");
const QString kMp3 = QStringLiteral("Mp3");
const QString kWav = QStringLiteral("Wav");

bool readBool(const AppSettings& s, const QString& key, bool fallback)
{
    return s.value(key, fallback ? kTrue : kFalse).toString() == kTrue;
}

int readInt(const AppSettings& s, const QString& key, int fallback)
{
    bool ok = false;
    const int v = s.value(key, fallback).toInt(&ok);
    return ok ? v : fallback;
}

void writeBool(AppSettings& s, const QString& key, bool value)
{
    s.setValue(key, value ? kTrue : kFalse);
}

QString coreDataFolder(const QString& leaf)
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath(leaf);
}

QString diskFolder(const QString& disk, const QString& leaf)
{
    return QDir(QDir(disk).filePath(QStringLiteral("NereusSDR"))).filePath(leaf);
}

}  // namespace

RecordingSettings RecordingSettings::load()
{
    const AppSettings& s = AppSettings::instance();
    RecordingSettings r;
    r.keepMyVoice = readBool(s, kKeepMyVoice, r.keepMyVoice);
    r.sidesApart = readBool(s, kSidesApart, r.sidesApart);
    r.autoStartOnTransmit = readBool(s, kAutoStartOnTransmit, r.autoStartOnTransmit);
    r.autoStopWhenQuiet = readBool(s, kAutoStopWhenQuiet, r.autoStopWhenQuiet);
    r.quietMinutes = std::clamp(readInt(s, kQuietMinutes, r.quietMinutes), kQuietMinutesMin,
                                kQuietMinutesMax);
    r.format = s.value(kFormat, kMp3).toString() == kWav ? AudioFileFormat::Wav : AudioFileFormat::Mp3;
    r.disk = s.value(kDisk, QString()).toString();
    r.onAirUseTxEq = readBool(s, kOnAirUseTxEq, r.onAirUseTxEq);
    r.onAirUseCompressor = readBool(s, kOnAirUseCompressor, r.onAirUseCompressor);
    r.onAirUseLeveler = readBool(s, kOnAirUseLeveler, r.onAirUseLeveler);
    r.onAirUseCfc = readBool(s, kOnAirUseCfc, r.onAirUseCfc);
    r.onAirUsePhaseRotator = readBool(s, kOnAirUsePhaseRotator, r.onAirUsePhaseRotator);
    r.dsrEnabled = readBool(s, kDsrEnabled, r.dsrEnabled);
    r.dsrDisk = s.value(kDsrDisk, QString()).toString();
    r.dsrSizeLimitGb = std::max(readInt(s, kDsrSizeLimitGb, r.dsrSizeLimitGb), kDsrSizeLimitGbMin);
    return r;
}

void RecordingSettings::save() const
{
    AppSettings& s = AppSettings::instance();
    writeBool(s, kKeepMyVoice, keepMyVoice);
    writeBool(s, kSidesApart, sidesApart);
    writeBool(s, kAutoStartOnTransmit, autoStartOnTransmit);
    writeBool(s, kAutoStopWhenQuiet, autoStopWhenQuiet);
    s.setValue(kQuietMinutes, std::clamp(quietMinutes, kQuietMinutesMin, kQuietMinutesMax));
    s.setValue(kFormat, format == AudioFileFormat::Wav ? kWav : kMp3);
    s.setValue(kDisk, disk);
    writeBool(s, kOnAirUseTxEq, onAirUseTxEq);
    writeBool(s, kOnAirUseCompressor, onAirUseCompressor);
    writeBool(s, kOnAirUseLeveler, onAirUseLeveler);
    writeBool(s, kOnAirUseCfc, onAirUseCfc);
    writeBool(s, kOnAirUsePhaseRotator, onAirUsePhaseRotator);
    writeBool(s, kDsrEnabled, dsrEnabled);
    s.setValue(kDsrDisk, dsrDisk);
    s.setValue(kDsrSizeLimitGb, std::max(dsrSizeLimitGb, kDsrSizeLimitGbMin));
}

QString RecordingSettings::recordingsFolder() const
{
    return disk.isEmpty() ? coreDataFolder(QStringLiteral("Recordings"))
                          : diskFolder(disk, QStringLiteral("Recordings"));
}

QString RecordingSettings::dsrFolder() const
{
    return dsrDisk.isEmpty() ? coreDataFolder(QStringLiteral("Spectrum Recorder"))
                             : diskFolder(dsrDisk, QStringLiteral("Spectrum Recorder"));
}

}  // namespace NereusSDR
