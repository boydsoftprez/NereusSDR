// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original Digital Spectrum Recorder rewind.
// Modification history (NereusSDR):
// 2026-11-16  J.J. Boyd / KG4VCF; rewind sound (spec 3, 6.7, recording plan
//             Task 22). AI tooling: Anthropic Claude Code.
// 2026-11-16  J.J. Boyd / KG4VCF; DsrPastRenderer: past rows, a past
//             spectrum frame and the hours map (spec 3, 6.7, A12, recording
//             plan Task 23). AI tooling: Anthropic Claude Code.
// 2026-11-16  J.J. Boyd / KG4VCF; rows reduced to the pan's pixels and
//             your transmit marked per row (recording plan Task 24). AI
//             tooling: Anthropic Claude Code.

#include "DsrReplay.h"

#include "core/FFTEngine.h"
#include "core/session/media/SpectrumEndpoint.h"
#include "core/spectrum/SpectrumDetector.h"
#include "core/spectrum/SpectrumReducer.h"

#include <QtEndian>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace NereusSDR {

static_assert(DsrPastRenderer::kMaxPixels == SpectrumEndpoint::kMaxPixels,
              "a past row is as wide as the live display allows");

DsrReplay::DsrReplay(int slice, int receiver, int sampleRateHz, qint64 fromUtcNs, qint64 liveCentreHz)
    : m_slice(slice)
    , m_receiver(receiver)
    , m_rate(sampleRateHz)
    , m_fromUtcNs(fromUtcNs)
    , m_liveCentreHz(liveCentreHz)
    , m_readEndUtcNs(fromUtcNs)
    , m_nextUtcNs(fromUtcNs)
    , m_read(static_cast<size_t>(kReadFrames) * 2)
    , m_push(static_cast<size_t>(kReadFrames) * 2)
    , m_pull(static_cast<size_t>(kMaxPullFrames) * 2)
{
}

qint64 DsrReplay::positionUtcNs() const
{
    const qint64 waitingFrames = m_ring.available() / 2;
    return m_readEndUtcNs.load(std::memory_order_acquire) - DsrRingFile::durationNs(waitingFrames, m_rate);
}

void DsrReplay::fill(const DsrRingFile& ring)
{
    if (m_rate <= 0 || m_finished.load(std::memory_order_acquire)) {
        return;
    }
    const int capacityFrames = m_ring.capacity() / 2 - kReadFrames;
    const int target = std::min(capacityFrames, int(qint64(m_rate) * kReadAheadMs / 1000));
    bool atNewest = false;
    // Every pass either pushes frames or moves to the next block, so this
    // bound only guards against a ring file that changes under the reads.
    for (int pass = 0; pass < 256; ++pass) {
        const int waiting = m_ring.available() / 2;
        if (waiting >= target) {
            break;
        }
        if (!m_haveBlock) {
            DsrBlockHeader h;
            if (!ring.blockAt(m_receiver, m_nextUtcNs, &h) && !ring.nextBlock(m_receiver, m_nextUtcNs, &h)) {
                // Past the newest second kept now; it may come on the next
                // round.
                atNewest = true;
                break;
            }
            if (h.sampleRateHz != m_rate) {
                // The receiver's rate changed here: the slice's demodulator
                // runs at the old one, so the past ends here.
                atNewest = true;
                break;
            }
            m_block = h;
            m_haveBlock = true;
            const qint64 from = std::max(m_nextUtcNs, h.utcNs);
            m_frameInBlock = std::clamp<qint64>((from - h.utcNs) * m_rate / DsrRingFile::kNsPerSecond, 0,
                                                qint64(h.frames));
        }
        const qint64 left = qint64(m_block.frames) - m_frameInBlock;
        const int want = int(std::min<qint64>({qint64(kReadFrames), qint64(target - waiting), left}));
        DsrBlockHeader got;
        const int n = want > 0 ? ring.read(m_receiver, DsrRingFile::frameUtcNs(m_block.utcNs, m_frameInBlock, m_rate),
                                           m_read.data(), want, &got)
                               : 0;
        if (n <= 0) {
            // The block went (the loop wrote over it) or is done.
            m_haveBlock = false;
            m_nextUtcNs = m_block.utcNs + DsrRingFile::durationNs(m_block.frames, m_rate);
            continue;
        }
        push(m_read.data(), n, m_block.centreHz);
        m_frameInBlock += n;
        m_nextUtcNs = DsrRingFile::frameUtcNs(m_block.utcNs, m_frameInBlock, m_rate);
        if (m_frameInBlock >= qint64(m_block.frames)) {
            m_haveBlock = false;
            m_nextUtcNs = m_block.utcNs + DsrRingFile::durationNs(m_block.frames, m_rate);
        }
        m_readEndUtcNs.store(m_nextUtcNs, std::memory_order_release);
    }
    m_atNewest.store(atNewest, std::memory_order_release);
    m_primed.store(true, std::memory_order_release);
}

void DsrReplay::push(const qint16* iq, int frames, qint64 keptCentreHz)
{
    // A signal at RF f was kept at baseband f - keptCentre; the slice's
    // shift looks for it at f - liveCentre. Multiplying by
    // e^(j 2 pi (keptCentre - liveCentre) n / rate) moves it there.
    const double offsetHz = double(keptCentreHz - m_liveCentreHz.load(std::memory_order_acquire));
    float* out = m_push.data();
    if (offsetHz == 0.0) {
        for (int k = 0; k < frames * 2; ++k) {
            out[k] = float(iq[k]) / kIqScale;
        }
    } else {
        const double step = 2.0 * std::numbers::pi * offsetHz / double(m_rate);
        const double stepRe = std::cos(step);
        const double stepIm = std::sin(step);
        double pr = m_phaseRe;
        double pIm = m_phaseIm;
        for (int k = 0; k < frames; ++k) {
            const double i = double(iq[2 * k]) / kIqScale;
            const double q = double(iq[2 * k + 1]) / kIqScale;
            out[2 * k] = float(i * pr - q * pIm);
            out[2 * k + 1] = float(i * pIm + q * pr);
            const double nr = pr * stepRe - pIm * stepIm;
            pIm = pr * stepIm + pIm * stepRe;
            pr = nr;
        }
        // Back to unit length, so rounding never grows the signal.
        const double mag = std::hypot(pr, pIm);
        m_phaseRe = mag > 0.0 ? pr / mag : 1.0;
        m_phaseIm = mag > 0.0 ? pIm / mag : 0.0;
    }
    // fill() keeps the ring below its capacity, so this fits.
    m_ring.tryPush(out, frames * 2);
}

bool DsrReplay::pull(float* i, float* q, int frames) noexcept
{
    if (m_finished.load(std::memory_order_acquire) || frames <= 0 || frames > kMaxPullFrames) {
        return false;
    }
    const int waiting = m_ring.available() / 2;
    if (waiting < frames && m_atNewest.load(std::memory_order_acquire)) {
        // Played up to the newest second kept: the radio's I/Q from here.
        m_finished.store(true, std::memory_order_release);
        return false;
    }
    const int take = std::min(waiting, frames);
    const int got = m_ring.pop(m_pull.data(), take * 2) / 2;
    for (int k = 0; k < got; ++k) {
        i[k] = m_pull[static_cast<size_t>(2 * k)];
        q[k] = m_pull[static_cast<size_t>(2 * k + 1)];
    }
    if (got < frames) {
        // The worker is late: silence for the rest, never the radio's I/Q.
        std::fill(i + got, i + frames, 0.0f);
        std::fill(q + got, q + frames, 0.0f);
        m_underruns.fetch_add(1, std::memory_order_relaxed);
    }
    m_framesPlayed.fetch_add(got, std::memory_order_relaxed);
    return true;
}

// ---- The past picture (recording plan Task 23).

DsrPastRenderer::DsrPastRenderer() = default;

DsrPastRenderer::~DsrPastRenderer() = default;

bool DsrPastRenderer::layout(int rateHz, qint64 streamCentreHz, const DsrPanView& view, DsrPastFrames* out)
{
    if (rateHz <= 0 || view.fftSize <= 0 || out == nullptr) {
        return false;
    }
    // The live spectrum's crop of the FFT for the pan's centre and span.
    ReducerConfig cfg;
    cfg.centreHz = double(view.centreHz);
    cfg.spanHz = double(view.spanHz);
    cfg.streamCentreHz = double(streamCentreHz);
    cfg.sampleRateHz = double(rateHz);
    const auto [first, last] = SpectrumReducer::visibleBinRange(view.fftSize, cfg);
    const int count = last - first + 1;
    if (count <= 0) {
        return false;
    }
    out->hzPerBin = double(rateHz) / double(view.fftSize);
    out->firstBinHz = double(streamCentreHz) - double(rateHz) / 2.0 + double(first) * out->hzPerBin;
    out->binCount = count;
    return true;
}

bool DsrPastRenderer::liveGrid(const DsrRingFile& ring, int receiver, int liveRateHz, qint64 liveCentreHz,
                               qint64 nearUtcNs, int* rateHz, qint64* centreHz) const
{
    if (liveRateHz > 0) {
        *rateHz = liveRateHz;
        *centreHz = liveCentreHz;
        return true;
    }
    // The receiver streams no more: the layout it had when last kept.
    DsrBlockHeader h;
    if (!ring.blockAt(receiver, nearUtcNs, &h) && !ring.blockAt(receiver, ring.newestUtcNs(receiver) - 1, &h)) {
        return false;
    }
    *rateHz = h.sampleRateHz;
    *centreHz = h.centreHz;
    return true;
}

bool DsrPastRenderer::gather(const DsrRingFile& ring, int receiver, qint64 fromUtcNs, int frames,
                             DsrBlockHeader* kept)
{
    m_read.resize(static_cast<size_t>(frames) * 2);
    int got = 0;
    qint64 t = fromUtcNs;
    qint64 previousStart = 0;
    while (got < frames) {
        DsrBlockHeader blk;
        if (!ring.blockAt(receiver, t, &blk)) {
            if (got == 0) {
                return false;
            }
            // The block after, when it follows on.
            if (!ring.nextBlock(receiver, previousStart + 1, &blk) || blk.utcNs - t > kJoinGapNs) {
                return false;
            }
            t = std::max(t, blk.utcNs);
        }
        if (got == 0) {
            *kept = blk;
        } else if (blk.sampleRateHz != kept->sampleRateHz) {
            return false;
        }
        DsrBlockHeader part;
        const int n = ring.read(receiver, t, m_read.data() + static_cast<size_t>(got) * 2, frames - got, &part);
        if (n <= 0) {
            return false;
        }
        got += n;
        previousStart = blk.utcNs;
        t = blk.utcNs + DsrRingFile::durationNs(blk.frames, blk.sampleRateHz);
    }
    m_feed.resize(frames * 2);
    for (int k = 0; k < frames * 2; ++k) {
        m_feed[k] = float(m_read[static_cast<size_t>(k)]) / DsrReplay::kIqScale;
    }
    return true;
}

bool DsrPastRenderer::transformAt(const DsrRingFile& ring, int receiver, qint64 utcNs, int fftSize,
                                  int windowType, DsrBlockHeader* kept, QVector<float>* binsDbm)
{
    // FFTEngine transforms once its window is full and one more pair comes.
    const int frames = fftSize + 1;
    if (!gather(ring, receiver, utcNs, frames, kept)) {
        // Near the newest kept second: the frames that end at that moment.
        DsrBlockHeader h;
        if (!ring.blockAt(receiver, utcNs, &h)
            || !gather(ring, receiver, utcNs - DsrRingFile::durationNs(frames, h.sampleRateHz), frames, kept)) {
            return false;
        }
    }
    if (!m_fft) {
        m_fft = std::make_unique<FFTEngine>(-1);
        QObject::connect(m_fft.get(), &FFTEngine::fftReady, m_fft.get(),
                         [this](int, const QVector<float>& bins) {
                             m_frame = bins;
                             m_haveFrame = true;
                         },
                         Qt::DirectConnection);
    }
    m_fft->setFftSize(fftSize);
    m_fft->setWindowFunction(static_cast<WindowFunction>(windowType));
    m_fft->setSampleRate(double(kept->sampleRateHz));
    // One transform of these frames alone: no overlap from the row before,
    // and no frame-rate hold.
    m_fft->resetInputHistory();
    m_haveFrame = false;
    m_fft->feedIQ(m_feed);
    if (!m_haveFrame || m_frame.size() != fftSize) {
        return false;
    }
    *binsDbm = m_frame;
    return true;
}

QVector<float> DsrPastRenderer::mapRow(const QVector<float>& binsDbm, const DsrBlockHeader& kept,
                                       const DsrPastFrames& layout)
{
    QVector<float> row(layout.binCount, kFloorDbm);
    const int n = int(binsDbm.size());
    if (n <= 0 || kept.sampleRateHz <= 0) {
        return row;
    }
    const double keptHzPerBin = double(kept.sampleRateHz) / double(n);
    const double keptLowHz = double(kept.centreHz) - double(kept.sampleRateHz) / 2.0;
    for (int j = 0; j < layout.binCount; ++j) {
        const double hz = layout.firstBinHz + double(j) * layout.hzPerBin;
        const qint64 src = std::llround((hz - keptLowHz) / keptHzPerBin);
        if (src >= 0 && src < n) {
            row[j] = binsDbm[int(src)];
        }
    }
    return row;
}

DsrPastFrames DsrPastRenderer::pixelLayout(const DsrPastFrames& bins, const DsrPanView& view)
{
    DsrPastFrames out = bins;
    if (view.pixels > 0 && bins.binCount > 0) {
        out.hzPerBin = bins.hzPerBin * double(bins.binCount) / double(view.pixels);
        out.binCount = view.pixels;
    }
    return out;
}

QVector<float> DsrPastRenderer::toPixels(const QVector<float>& rowDbm, int pixels)
{
    if (pixels <= 0 || rowDbm.isEmpty()) {
        return rowDbm;
    }
    // SpectrumReducer::reduce runs the detector on linear power (FFTEngine's
    // fftReadyLinear) and takes 10 log10 of each pixel after it.
    QVector<float> linear(rowDbm.size());
    for (qsizetype k = 0; k < rowDbm.size(); ++k) {
        linear[k] = std::pow(10.0f, rowDbm[k] / 10.0f);
    }
    QVector<float> out;
    applySpectrumDetector(SpectrumDetectorMode::Peak, linear, pixels, out, 1.0);
    for (float& v : out) {
        v = v > 0.0f ? 10.0f * std::log10(v) : kFloorDbm;
    }
    return out;
}

DsrPastFrames DsrPastRenderer::rows(const DsrRingFile& ring, int receiver, int liveRateHz, qint64 liveCentreHz,
                                    const DsrPanView& view, qint64 fromUtcNs, qint64 toUtcNs, int rowCount,
                                    qint64 maxBinBytes)
{
    DsrPastFrames out;
    out.receiver = receiver;
    rowCount = std::clamp(rowCount, 1, kMaxRows);
    int rate = 0;
    qint64 centre = 0;
    DsrPastFrames grid;     // the live crop's bins, which each row is mapped into
    if (toUtcNs < fromUtcNs || !liveGrid(ring, receiver, liveRateHz, liveCentreHz, toUtcNs, &rate, &centre)
        || !layout(rate, centre, view, &grid)) {
        return out;
    }
    out = pixelLayout(grid, view);
    out.receiver = receiver;
    const qint64 step = rowCount > 1 ? (toUtcNs - fromUtcNs) / (rowCount - 1) : 0;
    const qint64 rowBytes = qint64(out.binCount) * 2;
    const int maxRows = int(std::clamp<qint64>(maxBinBytes / rowBytes, 1, kMaxRows));
    int i = 0;
    for (; i < rowCount && out.rows.size() < maxRows; ++i) {
        const qint64 t = toUtcNs - qint64(i) * step;
        DsrBlockHeader kept;
        QVector<float> bins;
        if (transformAt(ring, receiver, t, view.fftSize, view.windowType, &kept, &bins)) {
            out.utcNs.append(t);
            out.rows.append(toPixels(mapRow(bins, kept, grid), view.pixels));
            out.transmit.append(kept.transmit != 0);
        }
    }
    out.rowsLeft = rowCount - i;
    out.nextToUtcNs = out.rowsLeft > 0 ? toUtcNs - qint64(i) * step : 0;
    return out;
}

bool DsrPastRenderer::spectrum(const DsrRingFile& ring, int receiver, int liveRateHz, qint64 liveCentreHz,
                               const DsrPanView& view, qint64 utcNs, DsrPastFrames* out)
{
    out->receiver = receiver;
    int rate = 0;
    qint64 centre = 0;
    DsrPastFrames grid;
    if (!liveGrid(ring, receiver, liveRateHz, liveCentreHz, utcNs, &rate, &centre)
        || !layout(rate, centre, view, &grid)) {
        return false;
    }
    DsrBlockHeader kept;
    QVector<float> bins;
    if (!transformAt(ring, receiver, utcNs, view.fftSize, view.windowType, &kept, &bins)) {
        return false;
    }
    *out = pixelLayout(grid, view);
    out->receiver = receiver;
    out->utcNs = {utcNs};
    out->rows = {toPixels(mapRow(bins, kept, grid), view.pixels)};
    out->transmit = {kept.transmit != 0};
    return true;
}

QList<DsrHour> DsrPastRenderer::hours(const DsrRingFile& ring)
{
    QList<DsrHour> out;
    const qint64 oldest = ring.oldestUtcNs();
    const qint64 newest = ring.newestUtcNs();
    if (oldest <= 0 || newest <= oldest) {
        return out;
    }
    const QList<int> receivers = ring.receivers();
    for (qint64 hour = (newest - 1) / kHourNs; hour >= oldest / kHourNs; --hour) {
        const qint64 start = hour * kHourNs;
        bool kept = false;
        double activity = 0.0;
        for (const int r : receivers) {
            double sum = 0.0;
            int count = 0;
            for (int s = 0; s < kHourSamples; ++s) {
                const qint64 at = start + qint64(s) * kHourNs / kHourSamples;
                const qint64 end = start + qint64(s + 1) * kHourNs / kHourSamples;
                DsrBlockHeader b;
                qint64 t = at;
                if (!ring.blockAt(r, at, &b)) {
                    if (!ring.nextBlock(r, at, &b) || b.utcNs >= end) {
                        continue;
                    }
                    t = b.utcNs;
                }
                kept = true;
                DsrBlockHeader used;
                QVector<float> bins;
                double a = 0.0;
                if (transformAt(ring, r, t, kHoursFftSize, kHoursWindowType, &used, &bins)) {
                    const float peak = *std::max_element(bins.cbegin(), bins.cend());
                    const auto middle = bins.begin() + bins.size() / 2;
                    std::nth_element(bins.begin(), middle, bins.end());
                    a = std::clamp((double(peak) - double(*middle) - kQuietDb) / kActivitySpanDb, 0.0, 1.0);
                }
                sum += a;
                ++count;
            }
            if (count > 0) {
                activity = std::max(activity, sum / count);
            }
        }
        if (kept) {
            out.append(DsrHour{start, activity});
        }
    }
    return out;
}

QByteArray DsrPastRenderer::packTimes(const QList<qint64>& utcNs)
{
    QByteArray bytes(utcNs.size() * qsizetype(sizeof(qint64)), Qt::Uninitialized);
    for (qsizetype i = 0; i < utcNs.size(); ++i) {
        qToLittleEndian<qint64>(utcNs.at(i), bytes.data() + i * qsizetype(sizeof(qint64)));
    }
    return bytes;
}

QList<qint64> DsrPastRenderer::unpackTimes(const QByteArray& bytes)
{
    QList<qint64> out;
    const qsizetype count = bytes.size() / qsizetype(sizeof(qint64));
    out.reserve(count);
    for (qsizetype i = 0; i < count; ++i) {
        out.append(qFromLittleEndian<qint64>(bytes.constData() + i * qsizetype(sizeof(qint64))));
    }
    return out;
}

QByteArray DsrPastRenderer::packRows(const QList<QVector<float>>& rows)
{
    qsizetype total = 0;
    for (const QVector<float>& row : rows) {
        total += row.size();
    }
    QByteArray bytes(total * qsizetype(sizeof(qint16)), Qt::Uninitialized);
    qsizetype at = 0;
    for (const QVector<float>& row : rows) {
        for (const float dbm : row) {
            const long steps = std::lround(double(dbm) * kWireStepsPerDb);
            const qint16 v = static_cast<qint16>(std::clamp<long>(steps, -32768, 32767));
            qToLittleEndian<qint16>(v, bytes.data() + at * qsizetype(sizeof(qint16)));
            ++at;
        }
    }
    return bytes;
}

QList<QVector<float>> DsrPastRenderer::unpackRows(const QByteArray& bytes, int binCount)
{
    QList<QVector<float>> out;
    if (binCount <= 0) {
        return out;
    }
    const qsizetype values = bytes.size() / qsizetype(sizeof(qint16));
    const qsizetype rowCount = values / binCount;
    for (qsizetype r = 0; r < rowCount; ++r) {
        QVector<float> row(binCount);
        for (int k = 0; k < binCount; ++k) {
            const qint16 v = qFromLittleEndian<qint16>(bytes.constData()
                                                       + (r * binCount + k) * qsizetype(sizeof(qint16)));
            row[k] = float(v) / kWireStepsPerDb;
        }
        out.append(row);
    }
    return out;
}

QByteArray DsrPastRenderer::packTransmit(const QList<bool>& transmit)
{
    QByteArray bytes(transmit.size(), '\0');
    for (qsizetype i = 0; i < transmit.size(); ++i) {
        bytes[i] = transmit.at(i) ? '\1' : '\0';
    }
    return bytes;
}

QList<bool> DsrPastRenderer::unpackTransmit(const QByteArray& bytes)
{
    QList<bool> out;
    out.reserve(bytes.size());
    for (const char b : bytes) {
        out.append(b != 0);
    }
    return out;
}

}  // namespace NereusSDR
