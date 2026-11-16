// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original recording names.
// Modification history (NereusSDR):
// 2026-11-15  J.J. Boyd / KG4VCF; recording kinds, entries and names
//             (spec 6.4). AI tooling: Anthropic Claude Code.
#pragma once

#include "core/NereusCoreExport.h"

#include <QDateTime>
#include <QString>
#include <QtGlobal>

namespace NereusSDR {

enum class RecordingKind { Contact, WholePan, Sound };

struct RecordingEntry {
    QString id;                 // stable, a UUID without braces
    RecordingKind kind = RecordingKind::Contact;
    int sliceId = -1;           // -1 for WholePan
    QDateTime startUtc;
    qint64 lengthMs = 0;
    qint64 frequencyHz = 0;
    QString mode;
    QString call;               // call empty when unknown
    QString name;               // shown name, also the file's base name
    QString fileName;           // in the store folder
    qint64 sizeBytes = 0;
};

// "2026-11-15 1432Z 7.236.400 LSB K4ABC"; no trailing space when call is empty.
NEREUS_CORE_EXPORT QString recordingBaseName(const QDateTime& startUtc, qint64 frequencyHz,
                                             const QString& mode, const QString& call);

// The frequency as the name shows it: MHz, kHz and Hz in groups of three,
// "7.236.400" for 7236400 Hz.
NEREUS_CORE_EXPORT QString recordingFrequencyText(qint64 frequencyHz);

// True when a name can be a recording's shown name and file base name.
// Refuses an empty name, a slash, a backslash, a colon, a control
// character and a leading dot, with the reason in plain words.
NEREUS_CORE_EXPORT bool isValidRecordingName(const QString& name, QString* reason);

// "Contact", "WholePan", "Sound": the recordings index's words for a kind.
NEREUS_CORE_EXPORT QString recordingKindKey(RecordingKind kind);
NEREUS_CORE_EXPORT bool recordingKindFromKey(const QString& key, RecordingKind* kind);

}  // namespace NereusSDR
