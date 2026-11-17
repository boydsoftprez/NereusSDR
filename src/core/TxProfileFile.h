#pragma once

// One transmit profile, portable between radios. The settings backup and the
// connected-radio export carry every setting; this file carries the selected
// profile's live keys only.
//
//   2026-11-17  Brian Sutton (W4KZO), AI-assisted via xAI Grok.

#include "core/NereusCoreExport.h"

#include <QByteArray>
#include <QHash>
#include <QString>

namespace NereusSDR::TxProfileFile {

inline constexpr int kVersion = 1;
inline constexpr qsizetype kMaxBytes = 256 * 1024;

struct Document {
    QString name;
    QHash<QString, QString> keys;
};

NEREUS_CORE_EXPORT bool encode(const Document& document, QByteArray* output,
                                   QString* error = nullptr);
NEREUS_CORE_EXPORT bool decode(const QByteArray& input, Document* output,
                               QString* error = nullptr);

// The `keys` object from a file, or the keysJson argument of txProfile.import.
// Every value must be text. An empty object is a profile of factory defaults.
NEREUS_CORE_EXPORT bool parseKeys(const QString& keysJson, QHash<QString, QString>* keys,
                                  QString* error = nullptr);

} // namespace NereusSDR::TxProfileFile
