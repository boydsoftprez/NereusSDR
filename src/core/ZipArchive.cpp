// no-port-check: NereusSDR-original. See ZipArchive.h.
// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/core/ZipArchive.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. R-R3-49, R-IOS-18 (remote-window
// parity Task 22). See the header.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-27  J.J. Boyd / KG4VCF  Created (remote-window parity Task 22,
//                                    R-R3-49). AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-27  J.J. Boyd / KG4VCF  Reader allocation and path bounds,
//                                    refined with OpenAI Codex assistance.
// =================================================================

#include "ZipArchive.h"

#include <zlib.h>

#include <limits>

namespace NereusSDR {

namespace {

constexpr quint32 kLocalHeaderSignature = 0x04034b50;
constexpr quint32 kCentralHeaderSignature = 0x02014b50;
constexpr quint32 kEndSignature = 0x06054b50;
constexpr quint16 kVersion = 20;        // 2.0: deflate
constexpr quint16 kUtf8Names = 0x0800;  // general purpose bit 11
constexpr quint16 kStored = 0;
constexpr quint16 kDeflated = 8;
constexpr int kLocalHeaderBytes = 30;
constexpr int kCentralHeaderBytes = 46;
constexpr int kEndBytes = 22;

void put16(QByteArray& out, quint16 value)
{
    out.append(static_cast<char>(value & 0xff));
    out.append(static_cast<char>((value >> 8) & 0xff));
}

void put32(QByteArray& out, quint32 value)
{
    put16(out, static_cast<quint16>(value & 0xffff));
    put16(out, static_cast<quint16>((value >> 16) & 0xffff));
}

quint16 get16(const QByteArray& in, qint64 at)
{
    const auto* bytes = reinterpret_cast<const unsigned char*>(in.constData());
    return static_cast<quint16>(bytes[at] | (bytes[at + 1] << 8));
}

quint32 get32(const QByteArray& in, qint64 at)
{
    return static_cast<quint32>(get16(in, at)) | (static_cast<quint32>(get16(in, at + 2)) << 16);
}

quint32 crcOf(const QByteArray& data)
{
    uLong crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, reinterpret_cast<const Bytef*>(data.constData()),
                static_cast<uInt>(data.size()));
    return static_cast<quint32>(crc);
}

} // namespace

QByteArray ZipArchive::deflateRaw(const QByteArray& data)
{
    z_stream stream{};
    // -15: raw deflate, the form a ZIP entry carries.
    if (deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY)
        != Z_OK) {
        return {};
    }
    QByteArray out;
    out.resize(static_cast<qsizetype>(deflateBound(&stream, static_cast<uLong>(data.size()))));
    stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.constData()));
    stream.avail_in = static_cast<uInt>(data.size());
    stream.next_out = reinterpret_cast<Bytef*>(out.data());
    stream.avail_out = static_cast<uInt>(out.size());
    const int result = deflate(&stream, Z_FINISH);
    const auto written = static_cast<qsizetype>(stream.total_out);
    deflateEnd(&stream);
    if (result != Z_STREAM_END) {
        return {};
    }
    out.resize(written);
    return out;
}

std::optional<QByteArray> ZipArchive::inflateRaw(const QByteArray& data, qint64 expectedSize)
{
    if (expectedSize < 0 || expectedSize > std::numeric_limits<uInt>::max()) {
        return std::nullopt;
    }
    z_stream stream{};
    if (inflateInit2(&stream, -15) != Z_OK) {
        return std::nullopt;
    }
    QByteArray out;
    out.resize(static_cast<qsizetype>(expectedSize));
    stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.constData()));
    stream.avail_in = static_cast<uInt>(data.size());
    stream.next_out = reinterpret_cast<Bytef*>(out.data());
    stream.avail_out = static_cast<uInt>(out.size());
    const int result = inflate(&stream, Z_FINISH);
    const qint64 produced = static_cast<qint64>(stream.total_out);
    inflateEnd(&stream);
    if (result != Z_STREAM_END || produced != expectedSize) {
        return std::nullopt;
    }
    return out;
}

ZipWriter::ZipWriter(const QDateTime& stamp)
{
    const QDate date = stamp.date();
    const QTime time = stamp.time();
    const int year = std::max(1980, date.year());
    m_dosDate = static_cast<quint16>(((year - 1980) << 9) | (date.month() << 5) | date.day());
    m_dosTime = static_cast<quint16>((time.hour() << 11) | (time.minute() << 5)
                                     | (time.second() / 2));
}

void ZipWriter::add(const QString& name, const QByteArray& data)
{
    Written entry;
    entry.name = name.toUtf8();
    entry.crc = crcOf(data);
    entry.size = static_cast<quint32>(data.size());
    QByteArray payload = ZipArchive::deflateRaw(data);
    entry.method = kDeflated;
    if (payload.isEmpty() && !data.isEmpty()) {
        payload = data;
        entry.method = kStored;
    } else if (payload.size() >= data.size()) {
        payload = data;
        entry.method = kStored;
    }
    entry.compressedSize = static_cast<quint32>(payload.size());
    entry.offset = static_cast<quint32>(m_body.size());

    put32(m_body, kLocalHeaderSignature);
    put16(m_body, kVersion);
    put16(m_body, kUtf8Names);
    put16(m_body, entry.method);
    put16(m_body, m_dosTime);
    put16(m_body, m_dosDate);
    put32(m_body, entry.crc);
    put32(m_body, entry.compressedSize);
    put32(m_body, entry.size);
    put16(m_body, static_cast<quint16>(entry.name.size()));
    put16(m_body, 0);
    m_body.append(entry.name);
    m_body.append(payload);
    m_entries.append(entry);
}

qint64 ZipWriter::finishedSize() const
{
    qint64 directory = 0;
    for (const Written& entry : m_entries) {
        directory += kCentralHeaderBytes + entry.name.size();
    }
    return m_body.size() + directory + kEndBytes;
}

QByteArray ZipWriter::finish() const
{
    QByteArray out = m_body;
    const auto directoryStart = static_cast<quint32>(out.size());
    for (const Written& entry : m_entries) {
        put32(out, kCentralHeaderSignature);
        put16(out, kVersion);   // made by
        put16(out, kVersion);   // needed
        put16(out, kUtf8Names);
        put16(out, entry.method);
        put16(out, m_dosTime);
        put16(out, m_dosDate);
        put32(out, entry.crc);
        put32(out, entry.compressedSize);
        put32(out, entry.size);
        put16(out, static_cast<quint16>(entry.name.size()));
        put16(out, 0);  // extra
        put16(out, 0);  // comment
        put16(out, 0);  // disk
        put16(out, 0);  // internal attributes
        put32(out, 0);  // external attributes
        put32(out, entry.offset);
        out.append(entry.name);
    }
    const auto directorySize = static_cast<quint32>(out.size()) - directoryStart;
    put32(out, kEndSignature);
    put16(out, 0);
    put16(out, 0);
    put16(out, static_cast<quint16>(m_entries.size()));
    put16(out, static_cast<quint16>(m_entries.size()));
    put32(out, directorySize);
    put32(out, directoryStart);
    put16(out, 0);
    return out;
}

std::optional<QList<ZipEntry>> ZipArchive::read(const QByteArray& archive)
{
    const qint64 size = archive.size();
    constexpr qint64 kMaxArchiveBytes = 2 * 1024 * 1024;
    constexpr qint64 kMaxEntryBytes = 8 * 1024 * 1024 + 1024;
    constexpr qint64 kMaxExpandedBytes = 32 * 1024 * 1024;
    constexpr int kMaxEntries = 16;
    if (size < kEndBytes || size > kMaxArchiveBytes) {
        return std::nullopt;
    }
    // The end record, found from the back (it may carry a comment).
    qint64 end = -1;
    for (qint64 at = size - kEndBytes; at >= 0 && at >= size - kEndBytes - 0xffff; --at) {
        if (get32(archive, at) == kEndSignature) {
            end = at;
            break;
        }
    }
    if (end < 0) {
        return std::nullopt;
    }
    const int count = get16(archive, end + 10);
    if (count > kMaxEntries) { return std::nullopt; }
    const qint64 directorySize = get32(archive, end + 12);
    qint64 at = get32(archive, end + 16);
    if (at + directorySize > end) {
        return std::nullopt;
    }
    QList<ZipEntry> entries;
    qint64 expanded = 0;
    for (int i = 0; i < count; ++i) {
        if (at + kCentralHeaderBytes > end || get32(archive, at) != kCentralHeaderSignature) {
            return std::nullopt;
        }
        const quint16 method = get16(archive, at + 10);
        const quint32 crc = get32(archive, at + 16);
        const qint64 compressedSize = get32(archive, at + 20);
        const qint64 uncompressedSize = get32(archive, at + 24);
        if (uncompressedSize > kMaxEntryBytes
            || expanded > kMaxExpandedBytes - uncompressedSize) {
            return std::nullopt;
        }
        expanded += uncompressedSize;
        const int nameLength = get16(archive, at + 28);
        const int extraLength = get16(archive, at + 30);
        const int commentLength = get16(archive, at + 32);
        const qint64 local = get32(archive, at + 42);
        if (at + kCentralHeaderBytes + nameLength + extraLength + commentLength > end) {
            return std::nullopt;
        }
        const QString name = QString::fromUtf8(archive.mid(at + kCentralHeaderBytes, nameLength));
        const QStringList parts = name.split(QLatin1Char('/'));
        if (name.isEmpty() || name.startsWith(QLatin1Char('/'))
            || name.contains(QLatin1Char('\\')) || name.contains(QLatin1Char(':'))
            || parts.contains(QStringLiteral("..")) || parts.contains(QStringLiteral("."))
            || parts.contains(QString())) {
            return std::nullopt;
        }
        at += kCentralHeaderBytes + nameLength + extraLength + commentLength;

        if (local + kLocalHeaderBytes > size || get32(archive, local) != kLocalHeaderSignature) {
            return std::nullopt;
        }
        const qint64 dataStart = local + kLocalHeaderBytes + get16(archive, local + 26)
            + get16(archive, local + 28);
        if (dataStart + compressedSize > size) {
            return std::nullopt;
        }
        const QByteArray payload = archive.mid(dataStart, compressedSize);
        QByteArray data;
        if (method == kStored) {
            if (compressedSize != uncompressedSize) {
                return std::nullopt;
            }
            data = payload;
        } else if (method == kDeflated) {
            const std::optional<QByteArray> inflated = inflateRaw(payload, uncompressedSize);
            if (!inflated) {
                return std::nullopt;
            }
            data = *inflated;
        } else {
            return std::nullopt;
        }
        if (crcOf(data) != crc) {
            return std::nullopt;
        }
        entries.append(ZipEntry{name, data});
    }
    return entries;
}

} // namespace NereusSDR
