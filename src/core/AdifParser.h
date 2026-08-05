// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - AdifParser: ADIF (.adi / .adif) amateur-radio log parser.
//
// Ported from AetherSDR src/core/AdifParser.h [@0cd4559].
// AetherSDR is (C) its contributors and is licensed GPL-3.0-or-later
// (see https://github.com/ten9876/AetherSDR/blob/main/LICENSE).
//
// Modification history (NereusSDR):
//   2026-05-10  J.J. Boyd / KG4VCF  Phase 3J-2 Task C2. Initial port.
//                                    AetherSDR's "AetherSDR" namespace
//                                    becomes "NereusSDR". QsoRecord
//                                    field layout (callsign, band,
//                                    modeGroup, dxccPrefix) and the
//                                    public surface (static
//                                    parseFile, Q_INVOKABLE
//                                    parseFileAsync, finished and
//                                    openFailed signals) follow
//                                    upstream byte-for-byte. Added
//                                    one public test seam
//                                    parseBytesForTest() so unit
//                                    tests can drive the parser
//                                    against an in-memory byte
//                                    buffer without going through a
//                                    QFile round-trip (precedent:
//                                    B1-B5 added test seams).
//                                    AI tooling: Anthropic Claude
//                                    Code.

#pragma once

#include <QString>
#include <QVector>
#include <QObject>

namespace NereusSDR {

// From AetherSDR src/core/AdifParser.h:9-14 [@0cd4559]
struct QsoRecord {
    QString callsign;
    QString band;       // "80m", "40m", etc. (normalised)
    QString modeGroup;  // "CW", "PHONE", "DATA"
    QString dxccPrefix; // filled by DxccColorProvider after callsign resolution
};

// From AetherSDR src/core/AdifParser.h:16-46 [@0cd4559]
//
// ---------------------------------------------------------------------------
// AdifParser
//
// Parses .adi / .adif log files into QsoRecord vectors.
// Can be used synchronously (parseFile) or from a worker thread.
// ---------------------------------------------------------------------------
class AdifParser : public QObject {
    Q_OBJECT

public:
    explicit AdifParser(QObject* parent = nullptr) : QObject(parent) {}

    // Synchronous - returns records directly.
    static QVector<QsoRecord> parseFile(const QString& path);

    // Async - call this slot (e.g. via QMetaObject::invokeMethod on a worker
    // thread).  Emits finished() when done.
    Q_INVOKABLE void parseFileAsync(const QString& path);

    // NereusSDR test seam (not in upstream). Parses an in-memory ADIF
    // byte buffer directly so unit tests can exercise the parser
    // without a QFile round-trip. Precedent: B1-B5 added similar
    // *ForTest seams.
    static QVector<QsoRecord> parseBytesForTest(const QByteArray& data);

signals:
    void finished(QVector<QsoRecord> records);
    // Emitted when the file cannot be opened after all retry attempts
    // (e.g. locked by an external logger).  The existing worked status
    // should be preserved and the caller may schedule a later retry.
    void openFailed(QString path);

private:
    static QVector<QsoRecord> parse(const QByteArray& data);
    static QString normaliseMode(const QString& mode, const QString& submode);
    static QString freqToBand(double mhz);
};

} // namespace NereusSDR

Q_DECLARE_METATYPE(NereusSDR::QsoRecord)
