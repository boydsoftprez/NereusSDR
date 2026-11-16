// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original Digital Spectrum Recorder ring file.
// Modification history (NereusSDR):
// 2026-11-16  J.J. Boyd / KG4VCF; the Digital Spectrum Recorder's ring
//             file: one-second blocks of 16-bit I/Q per receiver, your
//             transmit marked with its audio beside it, overwriting the
//             oldest at the size limit (spec 3, 6.7, A10). AI tooling:
//             Anthropic Claude Code.
// 2026-11-16  J.J. Boyd / KG4VCF; blockAt, nextBlock and frameUtcNs, so
//             rewind (recording plan Task 22) can walk the blocks without
//             reading the disk to find them. AI tooling: Anthropic Claude
//             Code.
// 2026-11-16  J.J. Boyd / KG4VCF; stopBeforeHeaderForTest: a write cut
//             short between a block's samples and its header. AI tooling:
//             Anthropic Claude Code.
#pragma once

#include "core/NereusCoreExport.h"

#include <QFile>
#include <QList>
#include <QMap>
#include <QMutex>
#include <QPair>
#include <QString>
#include <QtGlobal>

namespace NereusSDR {

// "NDSR" as the first four bytes of a block on disk, read little-endian.
inline constexpr quint32 kDsrMagic = 0x5253444Eu;
inline constexpr quint16 kDsrFormat = 1;

// One block's header: the plan's format, magic "NDSR", format 1. A block is
// up to one second of one receiver's I/Q.
struct DsrBlockHeader {
    quint32 magic = kDsrMagic;
    quint16 format = kDsrFormat;
    quint16 receiver = 0;
    qint64 utcNs = 0;          // the first frame's time
    qint32 sampleRateHz = 0;
    qint64 centreHz = 0;
    quint8 transmit = 0;       // 1 while you transmitted in this block
    quint32 frames = 0;        // I/Q frames (one I and one Q each)
};

// The ring file. On disk, each block is a 64-byte header (the fields above,
// a sequence number, the transmit audio's rate and length, and a check
// word), then `frames` interleaved 16-bit I/Q pairs, then the transmit
// audio as 16-bit mono, if any. Blocks start on kAlignBytes boundaries, so
// a restart can find every block by its header. The file never grows past
// the size limit: a block that does not fit at the end starts again at the
// front, and every block it lands on (the oldest) is erased.
//
// Little-endian on disk; every platform NereusSDR builds for is.
//
// One thread writes (the recordings worker). Reads may come from another
// thread; a mutex inside keeps them apart. Never used from an audio or
// connection thread.
class NEREUS_CORE_EXPORT DsrRingFile {
public:
    static constexpr qint64 kHeaderBytes = 64;
    static constexpr qint64 kAlignBytes = 4096;
    static constexpr qint64 kNsPerSecond = 1000000000;

    DsrRingFile();
    ~DsrRingFile();
    DsrRingFile(const DsrRingFile&) = delete;
    DsrRingFile& operator=(const DsrRingFile&) = delete;

    // Creates the file, or scans an existing one for its blocks and takes
    // up after the newest. Blocks that do not fit under `sizeLimitBytes`
    // (a smaller limit than before) are erased and the file is cut there.
    bool open(const QString& path, qint64 sizeLimitBytes, QString* error);
    void close();
    bool isOpen() const;
    QString path() const;

    // Appends one block after the newest; at the limit it starts again at
    // the front and erases the oldest blocks it lands on. `txAudio` is the
    // transmit audio kept beside this block (mono, 16-bit), if any.
    bool append(const DsrBlockHeader& h, const qint16* iq, int frames);
    bool append(const DsrBlockHeader& h, const qint16* iq, int frames, const qint16* txAudio,
                int txAudioFrames, int txAudioRateHz);

    // The first frame of the oldest block, and the end of the newest, for
    // one receiver or (no argument) any. 0 when there is none.
    qint64 oldestUtcNs(int receiver) const;
    qint64 newestUtcNs(int receiver) const;
    qint64 oldestUtcNs() const;
    qint64 newestUtcNs() const;

    // Up to `maxFrames` frames of `receiver` from `fromUtcNs`, from the one
    // block that holds that moment, into `iq` (interleaved I and Q). `hdr`
    // gets that block's header with `utcNs` the first returned frame's time
    // and `frames` the count returned. 0 when no block holds the moment.
    int read(int receiver, qint64 fromUtcNs, qint16* iq, int maxFrames, DsrBlockHeader* hdr) const;
    // The header of `receiver`'s block that holds `utcNs`, as kept (its
    // first frame's time and all its frames). From the index; no disk read.
    // False when no block holds that moment.
    bool blockAt(int receiver, qint64 utcNs, DsrBlockHeader* hdr) const;
    // The header of `receiver`'s first block that starts at or after
    // `utcNs`. From the index; no disk read. False when none is kept.
    bool nextBlock(int receiver, qint64 utcNs, DsrBlockHeader* hdr) const;
    // The time to ask read() for so that it starts exactly at frame `frame`
    // of a block that starts at `blockUtcNs` (rounded up, so read()'s
    // rounding down lands on that frame and not the one before).
    static qint64 frameUtcNs(qint64 blockUtcNs, qint64 frame, qint64 sampleRateHz);
    // Up to `maxFrames` of your transmit audio from `fromUtcNs`, from the
    // block that holds it; `rateHz` gets its rate. 0 when none is kept then.
    int readTransmitAudio(qint64 fromUtcNs, qint16* out, int maxFrames, int* rateHz) const;
    // Where your transmit starts (true) and stops (false) between the two
    // times, in time order.
    QList<QPair<qint64, bool>> transmitMarks(qint64 fromNs, qint64 toNs) const;

    qint64 sizeLimitBytes() const;
    // Where the next block goes.
    qint64 writePosition() const;
    // The bytes the kept blocks take.
    qint64 usedBytes() const;
    qint64 fileBytes() const;
    int blockCount() const;
    QList<int> receivers() const;

    // A block's bytes on disk, rounded up to kAlignBytes.
    static qint64 blockBytes(int frames, int txAudioFrames = 0);
    // How long `frames` last at `sampleRateHz`.
    static qint64 durationNs(qint64 frames, qint64 sampleRateHz);

    // A crash in the middle of a write: append() stops after the samples,
    // before the header, and returns false.
    void stopBeforeHeaderForTest(bool stop) { m_stopBeforeHeaderForTest = stop; }

private:
    struct Block {
        qint64 offset = 0;
        qint64 bytes = 0;          // rounded up to kAlignBytes
        quint64 seq = 0;
        DsrBlockHeader header;
        qint32 audioRateHz = 0;
        quint32 audioFrames = 0;
        qint64 endUtcNs() const { return header.utcNs + durationNs(header.frames, header.sampleRateHz); }
    };

    bool appendLocked(const DsrBlockHeader& h, const qint16* iq, int frames, const qint16* txAudio,
                      int txAudioFrames, int txAudioRateHz);
    bool eraseHeader(qint64 offset);
    bool readBlockHeader(qint64 offset, qint64 fileSize, Block* block);
    void setError(QString* error, const QString& why) const;

    mutable QMutex m_mutex;
    mutable QFile m_file;
    qint64 m_limit = 0;
    qint64 m_writePos = 0;
    quint64 m_nextSeq = 1;
    QMap<quint64, Block> m_blocks;   // by sequence: oldest first
    bool m_stopBeforeHeaderForTest = false;
};

}  // namespace NereusSDR
