// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original recording names.
// Modification history (NereusSDR):
// 2026-11-15  J.J. Boyd / KG4VCF; recording kinds, entries and names
//             (spec 6.4). AI tooling: Anthropic Claude Code.

#include "RecordingNames.h"

#include <QLatin1Char>
#include <QStringList>

namespace NereusSDR {

QString recordingFrequencyText(qint64 frequencyHz)
{
    const qint64 hz = frequencyHz < 0 ? -frequencyHz : frequencyHz;
    const qint64 mhz = hz / 1000000;
    const qint64 khz = (hz / 1000) % 1000;
    const qint64 rest = hz % 1000;
    return QStringLiteral("%1%2.%3.%4")
        .arg(frequencyHz < 0 ? QStringLiteral("-") : QString())
        .arg(mhz)
        .arg(khz, 3, 10, QLatin1Char('0'))
        .arg(rest, 3, 10, QLatin1Char('0'));
}

QString recordingBaseName(const QDateTime& startUtc, qint64 frequencyHz, const QString& mode,
                          const QString& call)
{
    QStringList parts;
    parts << startUtc.toUTC().toString(QStringLiteral("yyyy-MM-dd HHmm")) + QLatin1Char('Z')
          << recordingFrequencyText(frequencyHz);
    const QString m = mode.trimmed();
    if (!m.isEmpty()) {
        parts << m;
    }
    const QString c = call.trimmed();
    if (!c.isEmpty()) {
        parts << c;
    }
    return parts.join(QLatin1Char(' '));
}

bool isValidRecordingName(const QString& name, QString* reason)
{
    auto refuse = [reason](const QString& why) {
        if (reason) {
            *reason = why;
        }
        return false;
    };
    if (name.trimmed().isEmpty()) {
        return refuse(QStringLiteral("A recording needs a name."));
    }
    if (name.startsWith(QLatin1Char('.'))) {
        return refuse(QStringLiteral("A name cannot start with a dot."));
    }
    for (const QChar ch : name) {
        if (ch == QLatin1Char('/') || ch == QLatin1Char('\\') || ch == QLatin1Char(':')) {
            return refuse(QStringLiteral("A name cannot contain \"%1\".").arg(ch));
        }
        if (ch.category() == QChar::Other_Control) {
            return refuse(QStringLiteral("A name cannot contain a control character."));
        }
    }
    return true;
}

QString recordingKindKey(RecordingKind kind)
{
    switch (kind) {
    case RecordingKind::Contact:
        return QStringLiteral("Contact");
    case RecordingKind::WholePan:
        return QStringLiteral("WholePan");
    case RecordingKind::Sound:
        return QStringLiteral("Sound");
    }
    return QStringLiteral("Contact");
}

bool recordingKindFromKey(const QString& key, RecordingKind* kind)
{
    for (const RecordingKind k : {RecordingKind::Contact, RecordingKind::WholePan, RecordingKind::Sound}) {
        if (key == recordingKindKey(k)) {
            if (kind) {
                *kind = k;
            }
            return true;
        }
    }
    return false;
}

}  // namespace NereusSDR
