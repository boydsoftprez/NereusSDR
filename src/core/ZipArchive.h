#pragma once
// no-port-check: NereusSDR-original. A small in-memory ZIP writer and
// reader over zlib, for the support bundle.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/core/ZipArchive.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. R-R3-49, R-IOS-18 (remote-window
// parity Task 22, the iPhone app plan's Task 25): the Core's support bundle
// travels as one ZIP file (`support.collect`'s `bundle`, base64). Built in
// memory, with no helper program, so the Core can make it on a worker
// thread on every platform, and read back by a window (and the tests).
//
// The format written is the plain PKWARE one every unzip tool reads:
// local headers, entries compressed with raw deflate (method 8) or stored
// (method 0) when deflate does not help, a central directory and its end
// record. No ZIP64, no encryption, no data descriptors; an archive or an
// entry past 4 GiB is refused. The reader takes the same subset.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-27  J.J. Boyd / KG4VCF  Created (remote-window parity Task 22,
//                                    R-R3-49). AI-assisted via Anthropic
//                                    Claude Code.
// =================================================================

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QString>

#include <optional>

namespace NereusSDR {

struct ZipEntry {
    QString name;
    QByteArray data;
};

class ZipWriter {
public:
    explicit ZipWriter(const QDateTime& stamp = QDateTime::currentDateTime());

    /// Adds one file (deflated, or stored when that is smaller).
    void add(const QString& name, const QByteArray& data);
    /// The archive's size if it ended now (the central directory included).
    qint64 finishedSize() const;
    /// The whole archive.
    QByteArray finish() const;
    int count() const { return static_cast<int>(m_entries.size()); }

private:
    struct Written {
        QByteArray name;
        quint32 crc = 0;
        quint32 compressedSize = 0;
        quint32 size = 0;
        quint16 method = 0;
        quint32 offset = 0;
    };
    QByteArray m_body;
    QList<Written> m_entries;
    quint16 m_dosTime = 0;
    quint16 m_dosDate = 0;
};

namespace ZipArchive {
/// Raw deflate (no zlib header), at the default level. Empty on failure.
QByteArray deflateRaw(const QByteArray& data);
/// Inflates raw deflate that should come to `expectedSize` bytes; nullopt
/// when it does not.
std::optional<QByteArray> inflateRaw(const QByteArray& data, qint64 expectedSize);
/// Every entry of an archive this reader takes, in the central directory's
/// order; nullopt for anything else (or a CRC that does not match).
std::optional<QList<ZipEntry>> read(const QByteArray& archive);
} // namespace ZipArchive

} // namespace NereusSDR
