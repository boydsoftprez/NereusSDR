// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original Digital Spectrum Recorder rewind.
// Modification history (NereusSDR):
// 2026-11-16  J.J. Boyd / KG4VCF; rewind sound: one slice fed from the
//             recorder's ring file instead of the radio (spec 3, 6.7,
//             recording plan Task 22). AI tooling: Anthropic Claude Code.
// 2026-11-16  J.J. Boyd / KG4VCF; DsrPastRenderer: past waterfall rows, a
//             past spectrum frame and the hours map, drawn from the ring
//             file with the pan's own FFT settings on the recordings worker
//             (spec 3, 6.7, A12, recording plan Task 23). AI tooling:
//             Anthropic Claude Code.
// 2026-11-16  J.J. Boyd / KG4VCF; desktop rewind: the pan's pixel width
//             (DsrPanView::pixels; each row reduced to it as the live
//             display reduces its bins) and your transmit marked per row
//             (spec 3, recording plan Task 24). AI tooling: Anthropic
//             Claude Code.
#pragma once

#include "core/NereusCoreExport.h"
#include "core/recordings/SpscFloatRing.h"
#include "core/recordings/dsr/DsrRingFile.h"

#include <QByteArray>
#include <QList>
#include <QVector>
#include <QtGlobal>

#include <atomic>
#include <memory>
#include <vector>

namespace NereusSDR {

class FFTEngine;

// One rewound slice's input: the past I/Q of one receiver, read from the
// ring file on the recordings worker into a lock-free ring, and taken by the
// DSP thread one demodulator block at a time in place of the radio's I/Q
// (RxDspWorker, the one switch on the receive path).
//
// Three sides, none of which waits for another:
// - the recordings worker (fill) reads the disk, turns the 16-bit I/Q back
//   into floats, moves it from the centre it was kept at to the centre the
//   slice's receiver has now, and pushes it;
// - the DSP thread (pull) only copies out of the ring: no disk, no lock, no
//   allocation;
// - the main thread sets the receiver's live centre and reads the position.
//
// The past plays in real time because the DSP thread takes it at the pace
// of the live receiver it replaces: one block for each block of live I/Q.
// When it has played up to the newest second kept, the slice goes back to
// the radio's I/Q (pull returns false from then on); the radio has gone on
// meanwhile by less than one block of the recorder (the second still being
// gathered, plus one write interval).
class NEREUS_CORE_EXPORT DsrReplay {
public:
    // Interleaved I and Q: 1,048,576 frames, 5.4 s at 192 kHz, 0.68 s at
    // 1536 kHz (the recorder's own ring size, DsrRecorder::kIqRingFloats).
    static constexpr int kRingFloats = 1 << 21;
    // The worker keeps this much read ahead of what the DSP thread takes;
    // ten of the worker's 20 ms rounds.
    static constexpr int kReadAheadMs = 200;
    // The worker reads at most this many frames at once.
    static constexpr int kReadFrames = 16384;
    // The largest block the DSP thread takes at once. A demodulator block is
    // 64 * rate / 48000 frames (SampleRateCatalog bufferSizeForRate): 2048
    // at 1536 kHz.
    static constexpr int kMaxPullFrames = 8192;
    // The ring file keeps I and Q scaled by this (DsrRecorder::kIqScale).
    static constexpr float kIqScale = 32767.0f;

    // `slice` plays `receiver`'s I/Q kept at `sampleRateHz` from `fromUtcNs`,
    // moved to `liveCentreHz`.
    DsrReplay(int slice, int receiver, int sampleRateHz, qint64 fromUtcNs, qint64 liveCentreHz);
    DsrReplay(const DsrReplay&) = delete;
    DsrReplay& operator=(const DsrReplay&) = delete;

    int slice() const { return m_slice; }
    int receiver() const { return m_receiver; }
    int sampleRateHz() const { return m_rate; }
    qint64 fromUtcNs() const { return m_fromUtcNs; }

    // ---- Main thread.

    // The centre the slice's receiver tunes now. The kept I/Q is moved to
    // it, so the slice's own shift (its frequency less this centre) finds
    // the past signal where it was.
    void setLiveCentreHz(qint64 hz) { m_liveCentreHz.store(hz, std::memory_order_release); }
    qint64 liveCentreHz() const { return m_liveCentreHz.load(std::memory_order_acquire); }

    // ---- Any thread.

    // The moment the DSP thread is playing: the end of what the worker has
    // read, less what still waits in the ring.
    qint64 positionUtcNs() const;
    // The DSP thread has played up to the newest second kept and gone back
    // to the radio's I/Q.
    bool finished() const { return m_finished.load(std::memory_order_acquire); }
    // The worker has read at least once (the switch may go in).
    bool primed() const { return m_primed.load(std::memory_order_acquire); }
    // Blocks the DSP thread took before the worker had read them (played
    // partly as silence).
    quint64 underruns() const { return m_underruns.load(std::memory_order_relaxed); }
    // Frames the DSP thread has taken.
    qint64 framesPlayed() const { return m_framesPlayed.load(std::memory_order_relaxed); }

    // ---- The recordings worker.

    // Reads ahead from the ring file until kReadAheadMs is waiting, or the
    // newest kept second is reached.
    void fill(const DsrRingFile& ring);

    // ---- The DSP thread.

    // `frames` of the past into `i` and `q`; true when they are the past
    // (or, if the worker is late, the past then silence). False once the
    // newest kept second has been played: the caller feeds the radio's I/Q
    // from then on. Copies only: no lock, no wait, no allocation.
    bool pull(float* i, float* q, int frames) noexcept;

private:
    void push(const qint16* iq, int frames, qint64 keptCentreHz);

    const int m_slice;
    const int m_receiver;
    const int m_rate;
    const qint64 m_fromUtcNs;
    std::atomic<qint64> m_liveCentreHz;

    SpscFloatRing m_ring{kRingFloats};
    std::atomic<qint64> m_readEndUtcNs;      // the end of what the worker has pushed
    std::atomic<bool> m_atNewest{false};     // the worker's last look found nothing newer
    std::atomic<bool> m_primed{false};
    std::atomic<bool> m_finished{false};
    std::atomic<quint64> m_underruns{0};
    std::atomic<qint64> m_framesPlayed{0};

    // Worker thread only.
    qint64 m_nextUtcNs = 0;
    bool m_haveBlock = false;
    DsrBlockHeader m_block;
    qint64 m_frameInBlock = 0;
    double m_phaseRe = 1.0;      // the move to the live centre, running on
    double m_phaseIm = 0.0;
    std::vector<qint16> m_read;
    std::vector<float> m_push;

    // DSP thread only; made here, so the DSP thread never allocates.
    std::vector<float> m_pull;
};

// What rewind needs from the radio. RecordingService uses the radio's
// (RadioModel) unless a test gives it one. Main thread.
class NEREUS_CORE_EXPORT DsrRewindHost {
public:
    virtual ~DsrRewindHost() = default;
    virtual bool sliceOpen(int slice) const = 0;
    virtual qint64 sliceFrequencyHz(int slice) const = 0;
    virtual void setSliceFrequencyHz(int slice, qint64 hz) = 0;
    // The receiver (DDC stream) the slice is on now; -1 for none.
    virtual int sliceReceiver(int slice) const = 0;
    virtual qint64 receiverCentreHz(int receiver) const = 0;
    virtual int receiverRateHz(int receiver) const = 0;
    // The slice is fed from the diversity mix, which rewind does not switch.
    virtual bool sliceOnDiversity(int slice) const = 0;
    // Feeds the slice from `replay`, or (null) from the radio again. False
    // when there is no DSP worker to switch (no radio streaming).
    virtual bool feedSlice(int slice, std::shared_ptr<DsrReplay> replay) = 0;
};

// ---- The past picture (spec 3 "Rewind picture", "Moving through hours",
// 6.7, A12; recording plan Task 23).

// The pan's own FFT settings, as the asking window sends them (the same
// values its display subscription carries): the FFT size and window, and
// the stretch of frequency it shows.
struct DsrPanView {
    int fftSize = 4096;
    int windowType = 1;        // WindowFunction
    qint64 centreHz = 0;
    qint64 spanHz = 0;
    // The pan's width in pixels (Task 24): each row is reduced to this many
    // points as the live display reduces its bins to pixels. 0 keeps the
    // bins as the live crop has them (the wire always asks for pixels).
    int pixels = 0;
};

// Past rows (or one past spectrum frame) in the pan's bin layout: the bins
// the live spectrum has for that pan's span and FFT size, from `firstBinHz`
// up in steps of `hzPerBin`. Each row is dBm per bin, newest first.
struct DsrPastFrames {
    int slice = -1;
    int receiver = -1;
    double firstBinHz = 0.0;
    double hzPerBin = 0.0;
    int binCount = 0;
    QList<qint64> utcNs;            // each row's moment, ns since 1970 UTC
    QList<QVector<float>> rows;
    // Per row: you were transmitting then (Task 24: marked as yours).
    QList<bool> transmit;
    // The rows of the request still to draw, from nextToUtcNs back to the
    // request's start: ask again for them (the rows come in chunks).
    int rowsLeft = 0;
    qint64 nextToUtcNs = 0;
};

// One kept hour of the hours map: its start and how busy the band was, 0
// (silence) to 1.
struct DsrHour {
    qint64 startUtcNs = 0;
    double activity = 0.0;
};

// Draws the past from the ring file with the same FFT engine the spectrum
// thread uses (FFTEngine), one transform per row. The recordings worker
// only: never the spectrum thread or the connection thread.
class NEREUS_CORE_EXPORT DsrPastRenderer {
public:
    // The FFT sizes a pan may ask for (FFTEngine::setFftSize's own limits).
    static constexpr int kMinFftSize = 1024;
    static constexpr int kMaxFftSize = 262144;
    // A request draws at most this many rows.
    static constexpr int kMaxRows = 4096;
    // The widest pan in pixels: the live display's own limit
    // (SpectrumEndpoint::kMaxPixels; checked in DsrReplay.cpp).
    static constexpr int kMaxPixels = 4096;
    // One answer carries at most this many bytes of bins (2 per bin), at
    // least one row; the rest of the request comes in later answers. The
    // size of one recording copy chunk (RecordingsFacade::kExportChunkBytes).
    static constexpr qint64 kAnswerBinBytes = 196608;
    // A bin with nothing kept under it: the floor FFTEngine::processFrame
    // gives a bin with no power.
    static constexpr float kFloorDbm = -200.0f;
    // Bins travel as signed 16-bit tenths of a dB.
    static constexpr float kWireStepsPerDb = 10.0f;
    // Two blocks kept this close are one stretch for a transform.
    static constexpr qint64 kJoinGapNs = 50000000;
    // The hours map: each hour is looked at this many times, evenly, with
    // this FFT; a moment whose strongest bin stands kQuietDb or less above
    // the middle bin counts as silence, and kActivitySpanDb more as fully
    // busy (NereusSDR's own shading, no Thetis equivalent).
    static constexpr qint64 kHourNs = 3600LL * DsrRingFile::kNsPerSecond;
    static constexpr int kHourSamples = 12;
    static constexpr int kHoursFftSize = 4096;
    static constexpr int kHoursWindowType = 1;   // WindowFunction::BlackmanHarris4
    static constexpr double kQuietDb = 15.0;
    static constexpr double kActivitySpanDb = 30.0;

    DsrPastRenderer();
    ~DsrPastRenderer();
    DsrPastRenderer(const DsrPastRenderer&) = delete;
    DsrPastRenderer& operator=(const DsrPastRenderer&) = delete;

    // The bin layout the live spectrum has for `view` on a receiver at
    // `rateHz` around `streamCentreHz` (SpectrumReducer::visibleBinRange).
    // False when it selects no bins.
    static bool layout(int rateHz, qint64 streamCentreHz, const DsrPanView& view, DsrPastFrames* out);
    // The answer's layout: `bins` itself, or with view.pixels set, the same
    // stretch of frequency in that many points.
    static DsrPastFrames pixelLayout(const DsrPastFrames& bins, const DsrPanView& view);

    // `rowCount` rows of `receiver` evenly from `toUtcNs` back to
    // `fromUtcNs` (both ends included), newest first, in the layout for the
    // receiver's live rate and centre (or, with no live rate, its newest
    // kept block's). A row with nothing kept under it is left out. At most
    // `maxBinBytes` of bins; rowsLeft and nextToUtcNs say what remains.
    DsrPastFrames rows(const DsrRingFile& ring, int receiver, int liveRateHz, qint64 liveCentreHz,
                       const DsrPanView& view, qint64 fromUtcNs, qint64 toUtcNs, int rowCount,
                       qint64 maxBinBytes = kAnswerBinBytes);
    // One frame at `utcNs`, as rows() draws a row. False when nothing is
    // kept then.
    bool spectrum(const DsrRingFile& ring, int receiver, int liveRateHz, qint64 liveCentreHz,
                  const DsrPanView& view, qint64 utcNs, DsrPastFrames* out);
    // Every kept hour, newest first.
    QList<DsrHour> hours(const DsrRingFile& ring);

    // The wire form: times as little-endian 64-bit integers, bins as
    // little-endian 16-bit tenths of a dB, row after row.
    static QByteArray packTimes(const QList<qint64>& utcNs);
    static QList<qint64> unpackTimes(const QByteArray& bytes);
    static QByteArray packRows(const QList<QVector<float>>& rows);
    static QList<QVector<float>> unpackRows(const QByteArray& bytes, int binCount);
    // One byte per row, 1 where you were transmitting.
    static QByteArray packTransmit(const QList<bool>& transmit);
    static QList<bool> unpackTransmit(const QByteArray& bytes);
    // A row of dBm reduced to `pixels` points the way the live display
    // reduces bins to pixels (SpectrumReducer::reduce: the Peak detector,
    // ReducerConfig's default, on linear power, no averaging).
    static QVector<float> toPixels(const QVector<float>& rowDbm, int pixels);

private:
    // The FFT (dBm, FFT-shifted, as FFTEngine emits it) of `fftSize` frames
    // of `receiver` from `utcNs` (or, near the newest kept, ending there).
    bool transformAt(const DsrRingFile& ring, int receiver, qint64 utcNs, int fftSize, int windowType,
                     DsrBlockHeader* kept, QVector<float>* binsDbm);
    bool gather(const DsrRingFile& ring, int receiver, qint64 fromUtcNs, int frames, DsrBlockHeader* kept);
    // The kept frame mapped into `layout`'s bins by frequency.
    static QVector<float> mapRow(const QVector<float>& binsDbm, const DsrBlockHeader& kept,
                                 const DsrPastFrames& layout);
    bool liveGrid(const DsrRingFile& ring, int receiver, int liveRateHz, qint64 liveCentreHz,
                  qint64 nearUtcNs, int* rateHz, qint64* centreHz) const;

    std::unique_ptr<FFTEngine> m_fft;
    QVector<float> m_frame;
    bool m_haveFrame = false;
    std::vector<qint16> m_read;
    QVector<float> m_feed;
};

}  // namespace NereusSDR
