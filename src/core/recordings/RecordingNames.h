// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original recording names.
// Modification history (NereusSDR):
// 2026-11-15  J.J. Boyd / KG4VCF; recording kinds, entries and names
//             (spec 6.4). AI tooling: Anthropic Claude Code.
// 2026-11-15  J.J. Boyd / KG4VCF; filterRecordings, for the list's slice and
//             kind (recording plan Task 7). AI tooling: Anthropic Claude Code.
// 2026-11-16  J.J. Boyd / KG4VCF; a whole-pan recording keeps the pan's
//             centre and span and slice A's filter (recording plan Task 25).
//             AI tooling: Anthropic Claude Code.
// 2026-11-16  J.J. Boyd / KG4VCF; the name refuses what Windows forbids.
//             AI tooling: Anthropic Claude Code.
#pragma once

#include "core/NereusCoreExport.h"

#include <QDateTime>
#include <QList>
#include <QString>
#include <QtGlobal>

#include <optional>

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
    // WholePan only (Task 25): the receiver's centre and span (its rate)
    // when it was saved, so the pan's range can be shown ("7.089 to
    // 7.281"); and slice A's filter edges then, for its sound only.
    qint64 centreHz = 0;
    qint64 spanHz = 0;
    int filterLowHz = 0;
    int filterHighHz = 0;
};

// "2026-11-15 1432Z 7.236.400 LSB K4ABC"; no trailing space when call is empty.
NEREUS_CORE_EXPORT QString recordingBaseName(const QDateTime& startUtc, qint64 frequencyHz,
                                             const QString& mode, const QString& call);

// The frequency as the name shows it: MHz, kHz and Hz in groups of three,
// "7.236.400" for 7236400 Hz.
NEREUS_CORE_EXPORT QString recordingFrequencyText(qint64 frequencyHz);

// True when a name can be a recording's shown name and file base name.
// Refuses an empty name, any character Windows forbids (/ \ : ? * " < > |),
// a control character, a leading dot, a trailing space or dot, and the
// names Windows keeps for devices (CON, PRN, AUX, NUL, COM1 to COM9, LPT1
// to LPT9, any case, with or without an extension), on every platform, so
// a recording copied from one Core to another stays usable. The reason is
// in plain words.
NEREUS_CORE_EXPORT bool isValidRecordingName(const QString& name, QString* reason);

// "Contact", "WholePan", "Sound": the recordings index's words for a kind.
NEREUS_CORE_EXPORT QString recordingKindKey(RecordingKind kind);
NEREUS_CORE_EXPORT bool recordingKindFromKey(const QString& key, RecordingKind* kind);

// The entries of `all`, in its order, on slice `sliceId` and of `kind`; an
// empty filter takes every slice or every kind.
NEREUS_CORE_EXPORT QList<RecordingEntry> filterRecordings(const QList<RecordingEntry>& all,
                                                          std::optional<int> sliceId,
                                                          std::optional<RecordingKind> kind);

}  // namespace NereusSDR
