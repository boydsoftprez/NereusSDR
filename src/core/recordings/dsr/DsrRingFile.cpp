// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original Digital Spectrum Recorder ring file.
// Modification history (NereusSDR):
// 2026-11-16  J.J. Boyd / KG4VCF; the Digital Spectrum Recorder's ring
//             file (spec 3, 6.7, A10). AI tooling: Anthropic Claude Code.
// 2026-11-16  J.J. Boyd / KG4VCF; blockAt, nextBlock and frameUtcNs for
//             rewind (recording plan Task 22). AI tooling: Anthropic Claude
//             Code.
// 2026-11-16  J.J. Boyd / KG4VCF; a block's samples are written before its
//             checked header, and every header a block lands on is erased
//             first, so a cut-short write is never taken on a restart. AI
//             tooling: Anthropic Claude Code.

#include "DsrRingFile.h"

#include "core/LogCategories.h"

#include <QDir>
#include <QFileInfo>
#include <QMutexLocker>
#include <QSysInfo>
#include <QtEndian>

#include <algorithm>
#include <array>
#include <cstring>
#include <map>

namespace NereusSDR {

static_assert(QSysInfo::ByteOrder == QSysInfo::LittleEndian,
              "the ring file keeps 16-bit samples as they lie in memory, little-endian");

namespace {

// Where each field lies in a block's 64-byte header.
constexpr int kOffMagic = 0;
constexpr int kOffFormat = 4;
constexpr int kOffReceiver = 6;
constexpr int kOffSeq = 8;
constexpr int kOffUtcNs = 16;
constexpr int kOffRate = 24;
constexpr int kOffTransmit = 28;
constexpr int kOffCentre = 32;
constexpr int kOffFrames = 40;
constexpr int kOffAudioRate = 44;
constexpr int kOffAudioFrames = 48;
constexpr int kOffCheck = 56;
constexpr int kCheckedBytes = kOffCheck;

constexpr qint64 kIqFrameBytes = 4;      // 16-bit I and Q
constexpr qint64 kAudioFrameBytes = 2;   // 16-bit mono
// Far above any receiver rate (1536 kHz) or one second of it; a header
// that claims more is not one of ours.
constexpr qint64 kMaxRateHz = 100000000;

using HeaderBytes = std::array<uchar, DsrRingFile::kHeaderBytes>;

// FNV-1a, 32-bit, over the header's fields: a restart takes only blocks
// whose header is whole, never sample data that happens to start "NDSR".
quint32 checkWord(const uchar* bytes, int count)
{
    quint32 h = 2166136261u;
    for (int i = 0; i < count; ++i) {
        h ^= bytes[i];
        h *= 16777619u;
    }
    return h;
}

HeaderBytes encode(const DsrBlockHeader& h, quint64 seq, qint32 audioRateHz, quint32 audioFrames)
{
    HeaderBytes b{};
    qToLittleEndian<quint32>(kDsrMagic, b.data() + kOffMagic);
    qToLittleEndian<quint16>(kDsrFormat, b.data() + kOffFormat);
    qToLittleEndian<quint16>(h.receiver, b.data() + kOffReceiver);
    qToLittleEndian<quint64>(seq, b.data() + kOffSeq);
    qToLittleEndian<qint64>(h.utcNs, b.data() + kOffUtcNs);
    qToLittleEndian<qint32>(h.sampleRateHz, b.data() + kOffRate);
    b[kOffTransmit] = h.transmit != 0 ? 1 : 0;
    qToLittleEndian<qint64>(h.centreHz, b.data() + kOffCentre);
    qToLittleEndian<quint32>(h.frames, b.data() + kOffFrames);
    qToLittleEndian<qint32>(audioRateHz, b.data() + kOffAudioRate);
    qToLittleEndian<quint32>(audioFrames, b.data() + kOffAudioFrames);
    qToLittleEndian<quint32>(checkWord(b.data(), kCheckedBytes), b.data() + kOffCheck);
    return b;
}

qint64 roundUp(qint64 bytes)
{
    return (bytes + DsrRingFile::kAlignBytes - 1) / DsrRingFile::kAlignBytes * DsrRingFile::kAlignBytes;
}

qint64 actualBytes(qint64 frames, qint64 audioFrames)
{
    return DsrRingFile::kHeaderBytes + frames * kIqFrameBytes + audioFrames * kAudioFrameBytes;
}

}  // namespace

DsrRingFile::DsrRingFile() = default;

DsrRingFile::~DsrRingFile()
{
    close();
}

qint64 DsrRingFile::blockBytes(int frames, int txAudioFrames)
{
    return roundUp(actualBytes(std::max(0, frames), std::max(0, txAudioFrames)));
}

qint64 DsrRingFile::durationNs(qint64 frames, qint64 sampleRateHz)
{
    return sampleRateHz > 0 ? frames * kNsPerSecond / sampleRateHz : 0;
}

void DsrRingFile::setError(QString* error, const QString& why) const
{
    if (error != nullptr) {
        *error = why;
    }
}

bool DsrRingFile::readBlockHeader(qint64 offset, qint64 fileSize, Block* block)
{
    if (offset + kHeaderBytes > fileSize || !m_file.seek(offset)) {
        return false;
    }
    HeaderBytes b{};
    if (m_file.read(reinterpret_cast<char*>(b.data()), kHeaderBytes) != kHeaderBytes) {
        return false;
    }
    if (qFromLittleEndian<quint32>(b.data() + kOffMagic) != kDsrMagic
        || qFromLittleEndian<quint16>(b.data() + kOffFormat) != kDsrFormat
        || qFromLittleEndian<quint32>(b.data() + kOffCheck) != checkWord(b.data(), kCheckedBytes)) {
        return false;
    }
    Block blk;
    blk.offset = offset;
    blk.seq = qFromLittleEndian<quint64>(b.data() + kOffSeq);
    blk.header.receiver = qFromLittleEndian<quint16>(b.data() + kOffReceiver);
    blk.header.utcNs = qFromLittleEndian<qint64>(b.data() + kOffUtcNs);
    blk.header.sampleRateHz = qFromLittleEndian<qint32>(b.data() + kOffRate);
    blk.header.transmit = b[kOffTransmit] != 0 ? 1 : 0;
    blk.header.centreHz = qFromLittleEndian<qint64>(b.data() + kOffCentre);
    blk.header.frames = qFromLittleEndian<quint32>(b.data() + kOffFrames);
    blk.audioRateHz = qFromLittleEndian<qint32>(b.data() + kOffAudioRate);
    blk.audioFrames = qFromLittleEndian<quint32>(b.data() + kOffAudioFrames);
    if (blk.header.sampleRateHz <= 0 || blk.header.sampleRateHz > kMaxRateHz || blk.header.frames == 0
        || blk.header.frames > static_cast<quint32>(blk.header.sampleRateHz) || blk.audioRateHz < 0
        || blk.audioRateHz > kMaxRateHz || blk.audioFrames > static_cast<quint32>(kMaxRateHz)) {
        return false;
    }
    const qint64 actual = actualBytes(blk.header.frames, blk.audioFrames);
    if (offset + actual > fileSize) {
        return false;
    }
    blk.bytes = roundUp(actual);
    *block = blk;
    return true;
}

bool DsrRingFile::open(const QString& path, qint64 sizeLimitBytes, QString* error)
{
    QMutexLocker lock(&m_mutex);
    if (m_file.isOpen()) {
        m_file.close();
    }
    m_blocks.clear();
    m_writePos = 0;
    m_nextSeq = 1;
    m_limit = sizeLimitBytes / kAlignBytes * kAlignBytes;
    if (m_limit < kAlignBytes) {
        setError(error, QStringLiteral("The recorder's size limit is too small."));
        return false;
    }
    QDir().mkpath(QFileInfo(path).absolutePath());
    m_file.setFileName(path);
    if (!m_file.open(QIODevice::ReadWrite)) {
        setError(error, m_file.errorString());
        qCWarning(lcRecordings) << "the spectrum recorder cannot open" << path << m_file.errorString();
        return false;
    }

    // Find every block by its header. A whole block is skipped at once;
    // anywhere else (an erased header, the rest of a block written over)
    // the scan steps one alignment at a time.
    const qint64 size = m_file.size();
    QList<Block> beyondLimit;
    qint64 pos = 0;
    while (pos + kHeaderBytes <= size) {
        Block blk;
        if (readBlockHeader(pos, size, &blk)) {
            if (blk.offset + actualBytes(blk.header.frames, blk.audioFrames) > m_limit) {
                beyondLimit.append(blk);
            } else {
                m_blocks.insert(blk.seq, blk);
            }
            pos += blk.bytes;
        } else {
            pos += kAlignBytes;
        }
    }
    for (const Block& blk : std::as_const(beyondLimit)) {
        eraseHeader(blk.offset);
    }
    if (size > m_limit) {
        m_file.resize(m_limit);
    }
    if (!m_blocks.isEmpty()) {
        const Block& newest = m_blocks.last();
        m_nextSeq = newest.seq + 1;
        m_writePos = newest.offset + newest.bytes;
        if (m_writePos >= m_limit) {
            m_writePos = 0;
        }
    }
    return true;
}

void DsrRingFile::close()
{
    QMutexLocker lock(&m_mutex);
    if (m_file.isOpen()) {
        m_file.close();
    }
    m_blocks.clear();
}

bool DsrRingFile::isOpen() const
{
    QMutexLocker lock(&m_mutex);
    return m_file.isOpen();
}

QString DsrRingFile::path() const
{
    QMutexLocker lock(&m_mutex);
    return m_file.fileName();
}

bool DsrRingFile::eraseHeader(qint64 offset)
{
    static constexpr std::array<char, 4> kZeros{};
    return m_file.seek(offset) && m_file.write(kZeros.data(), kZeros.size()) == qint64(kZeros.size());
}

bool DsrRingFile::append(const DsrBlockHeader& h, const qint16* iq, int frames)
{
    return append(h, iq, frames, nullptr, 0, 0);
}

bool DsrRingFile::append(const DsrBlockHeader& h, const qint16* iq, int frames, const qint16* txAudio,
                         int txAudioFrames, int txAudioRateHz)
{
    QMutexLocker lock(&m_mutex);
    return appendLocked(h, iq, frames, txAudio, txAudioFrames, txAudioRateHz);
}

bool DsrRingFile::appendLocked(const DsrBlockHeader& h, const qint16* iq, int frames, const qint16* txAudio,
                               int txAudioFrames, int txAudioRateHz)
{
    if (!m_file.isOpen() || iq == nullptr || frames <= 0 || h.sampleRateHz <= 0) {
        return false;
    }
    if (txAudio == nullptr || txAudioRateHz <= 0) {
        txAudioFrames = 0;
        txAudioRateHz = 0;
    }
    const qint64 actual = actualBytes(frames, std::max(0, txAudioFrames));
    const qint64 bytes = roundUp(actual);
    if (bytes > m_limit) {
        qCWarning(lcRecordings) << "a spectrum recorder block of" << bytes << "bytes is larger than the limit"
                                << m_limit;
        return false;
    }

    // At the limit: start again at the front. The blocks left past this
    // point are the oldest; they go now, so the front is never written
    // over while older blocks stay behind it.
    if (m_writePos + bytes > m_limit) {
        for (auto it = m_blocks.begin(); it != m_blocks.end();) {
            if (it->offset >= m_writePos) {
                if (!eraseHeader(it->offset)) {
                    return false;
                }
                it = m_blocks.erase(it);
            } else {
                ++it;
            }
        }
        m_writePos = 0;
    }

    // Every block this one lands on goes, its header erased first, so a
    // restart never finds an old header over new samples. The new samples
    // go down before the new header: a write cut short leaves no header
    // that checks over a torn block.
    const qint64 start = m_writePos;
    const qint64 end = start + bytes;
    for (auto it = m_blocks.begin(); it != m_blocks.end();) {
        if (it->offset < end && it->offset + it->bytes > start) {
            if (!eraseHeader(it->offset)) {
                return false;
            }
            it = m_blocks.erase(it);
        } else {
            ++it;
        }
    }

    Block blk;
    blk.offset = start;
    blk.bytes = bytes;
    blk.seq = m_nextSeq;
    blk.header = h;
    blk.header.magic = kDsrMagic;
    blk.header.format = kDsrFormat;
    blk.header.transmit = h.transmit != 0 ? 1 : 0;
    blk.header.frames = static_cast<quint32>(frames);
    blk.audioRateHz = txAudioRateHz;
    blk.audioFrames = static_cast<quint32>(std::max(0, txAudioFrames));

    const qint64 iqBytes = qint64(frames) * kIqFrameBytes;
    if (!m_file.seek(start + kHeaderBytes)
        || m_file.write(reinterpret_cast<const char*>(iq), iqBytes) != iqBytes) {
        qCWarning(lcRecordings) << "the spectrum recorder cannot write:" << m_file.errorString();
        return false;
    }
    if (blk.audioFrames > 0) {
        const qint64 audioBytes = qint64(blk.audioFrames) * kAudioFrameBytes;
        if (m_file.write(reinterpret_cast<const char*>(txAudio), audioBytes) != audioBytes) {
            qCWarning(lcRecordings) << "the spectrum recorder cannot write:" << m_file.errorString();
            return false;
        }
    }
    if (m_stopBeforeHeaderForTest) {
        return false;
    }
    const HeaderBytes head = encode(blk.header, blk.seq, blk.audioRateHz, blk.audioFrames);
    if (!m_file.seek(start)
        || m_file.write(reinterpret_cast<const char*>(head.data()), kHeaderBytes) != kHeaderBytes) {
        qCWarning(lcRecordings) << "the spectrum recorder cannot write:" << m_file.errorString();
        return false;
    }
    m_blocks.insert(blk.seq, blk);
    ++m_nextSeq;
    m_writePos = end;
    return true;
}

qint64 DsrRingFile::oldestUtcNs(int receiver) const
{
    QMutexLocker lock(&m_mutex);
    qint64 oldest = 0;
    for (const Block& blk : m_blocks) {
        if (blk.header.receiver == receiver && (oldest == 0 || blk.header.utcNs < oldest)) {
            oldest = blk.header.utcNs;
        }
    }
    return oldest;
}

qint64 DsrRingFile::newestUtcNs(int receiver) const
{
    QMutexLocker lock(&m_mutex);
    qint64 newest = 0;
    for (const Block& blk : m_blocks) {
        if (blk.header.receiver == receiver) {
            newest = std::max(newest, blk.endUtcNs());
        }
    }
    return newest;
}

qint64 DsrRingFile::oldestUtcNs() const
{
    QMutexLocker lock(&m_mutex);
    qint64 oldest = 0;
    for (const Block& blk : m_blocks) {
        if (oldest == 0 || blk.header.utcNs < oldest) {
            oldest = blk.header.utcNs;
        }
    }
    return oldest;
}

qint64 DsrRingFile::newestUtcNs() const
{
    QMutexLocker lock(&m_mutex);
    qint64 newest = 0;
    for (const Block& blk : m_blocks) {
        newest = std::max(newest, blk.endUtcNs());
    }
    return newest;
}

int DsrRingFile::read(int receiver, qint64 fromUtcNs, qint16* iq, int maxFrames, DsrBlockHeader* hdr) const
{
    QMutexLocker lock(&m_mutex);
    if (!m_file.isOpen() || iq == nullptr || maxFrames <= 0) {
        return 0;
    }
    for (const Block& blk : m_blocks) {
        if (blk.header.receiver != receiver || fromUtcNs < blk.header.utcNs || fromUtcNs >= blk.endUtcNs()) {
            continue;
        }
        const qint64 first = (fromUtcNs - blk.header.utcNs) * blk.header.sampleRateHz / kNsPerSecond;
        const qint64 count = std::min<qint64>(maxFrames, qint64(blk.header.frames) - first);
        if (count <= 0) {
            return 0;
        }
        if (!m_file.seek(blk.offset + kHeaderBytes + first * kIqFrameBytes)) {
            return 0;
        }
        const qint64 want = count * kIqFrameBytes;
        if (m_file.read(reinterpret_cast<char*>(iq), want) != want) {
            qCWarning(lcRecordings) << "the spectrum recorder cannot read:" << m_file.errorString();
            return 0;
        }
        if (hdr != nullptr) {
            *hdr = blk.header;
            hdr->utcNs = blk.header.utcNs + durationNs(first, blk.header.sampleRateHz);
            hdr->frames = static_cast<quint32>(count);
        }
        return static_cast<int>(count);
    }
    return 0;
}

bool DsrRingFile::blockAt(int receiver, qint64 utcNs, DsrBlockHeader* hdr) const
{
    QMutexLocker lock(&m_mutex);
    if (!m_file.isOpen()) {
        return false;
    }
    for (const Block& blk : m_blocks) {
        if (blk.header.receiver == receiver && utcNs >= blk.header.utcNs && utcNs < blk.endUtcNs()) {
            if (hdr != nullptr) {
                *hdr = blk.header;
            }
            return true;
        }
    }
    return false;
}

bool DsrRingFile::nextBlock(int receiver, qint64 utcNs, DsrBlockHeader* hdr) const
{
    QMutexLocker lock(&m_mutex);
    if (!m_file.isOpen()) {
        return false;
    }
    const Block* best = nullptr;
    for (const Block& blk : m_blocks) {
        if (blk.header.receiver == receiver && blk.header.utcNs >= utcNs
            && (best == nullptr || blk.header.utcNs < best->header.utcNs)) {
            best = &blk;
        }
    }
    if (best == nullptr) {
        return false;
    }
    if (hdr != nullptr) {
        *hdr = best->header;
    }
    return true;
}

qint64 DsrRingFile::frameUtcNs(qint64 blockUtcNs, qint64 frame, qint64 sampleRateHz)
{
    if (sampleRateHz <= 0) {
        return blockUtcNs;
    }
    return blockUtcNs + (frame * kNsPerSecond + sampleRateHz - 1) / sampleRateHz;
}

int DsrRingFile::readTransmitAudio(qint64 fromUtcNs, qint16* out, int maxFrames, int* rateHz) const
{
    QMutexLocker lock(&m_mutex);
    if (!m_file.isOpen() || out == nullptr || maxFrames <= 0) {
        return 0;
    }
    for (const Block& blk : m_blocks) {
        if (blk.audioFrames == 0 || blk.audioRateHz <= 0) {
            continue;
        }
        const qint64 end = blk.header.utcNs + durationNs(blk.audioFrames, blk.audioRateHz);
        if (fromUtcNs < blk.header.utcNs || fromUtcNs >= end) {
            continue;
        }
        const qint64 first = (fromUtcNs - blk.header.utcNs) * blk.audioRateHz / kNsPerSecond;
        const qint64 count = std::min<qint64>(maxFrames, qint64(blk.audioFrames) - first);
        if (count <= 0) {
            return 0;
        }
        const qint64 at = blk.offset + kHeaderBytes + qint64(blk.header.frames) * kIqFrameBytes
                          + first * kAudioFrameBytes;
        const qint64 want = count * kAudioFrameBytes;
        if (!m_file.seek(at) || m_file.read(reinterpret_cast<char*>(out), want) != want) {
            return 0;
        }
        if (rateHz != nullptr) {
            *rateHz = blk.audioRateHz;
        }
        return static_cast<int>(count);
    }
    return 0;
}

QList<QPair<qint64, bool>> DsrRingFile::transmitMarks(qint64 fromNs, qint64 toNs) const
{
    QMutexLocker lock(&m_mutex);
    // Each second once: transmitting when any receiver's block was.
    std::map<qint64, bool> seconds;
    for (const Block& blk : m_blocks) {
        if (blk.endUtcNs() <= fromNs || blk.header.utcNs >= toNs) {
            continue;
        }
        bool& on = seconds[blk.header.utcNs];
        on = on || blk.header.transmit != 0;
    }
    QList<QPair<qint64, bool>> marks;
    bool on = false;
    for (const auto& [utcNs, transmit] : seconds) {
        if (transmit != on) {
            marks.append(qMakePair(utcNs, transmit));
            on = transmit;
        }
    }
    return marks;
}

qint64 DsrRingFile::sizeLimitBytes() const
{
    QMutexLocker lock(&m_mutex);
    return m_limit;
}

qint64 DsrRingFile::writePosition() const
{
    QMutexLocker lock(&m_mutex);
    return m_writePos;
}

qint64 DsrRingFile::usedBytes() const
{
    QMutexLocker lock(&m_mutex);
    qint64 used = 0;
    for (const Block& blk : m_blocks) {
        used += blk.bytes;
    }
    return used;
}

qint64 DsrRingFile::fileBytes() const
{
    QMutexLocker lock(&m_mutex);
    return m_file.isOpen() ? m_file.size() : 0;
}

int DsrRingFile::blockCount() const
{
    QMutexLocker lock(&m_mutex);
    return static_cast<int>(m_blocks.size());
}

QList<int> DsrRingFile::receivers() const
{
    QMutexLocker lock(&m_mutex);
    QList<int> list;
    for (const Block& blk : m_blocks) {
        if (!list.contains(blk.header.receiver)) {
            list.append(blk.header.receiver);
        }
    }
    std::sort(list.begin(), list.end());
    return list;
}

}  // namespace NereusSDR
