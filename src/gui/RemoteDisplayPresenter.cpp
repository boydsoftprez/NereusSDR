// =================================================================
// src/gui/RemoteDisplayPresenter.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original. See RemoteDisplayPresenter.h.
//
// Modification history (NereusSDR):
//   2026-10-02: Capture identity separates incompatible presentation chains.
//               J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-09-26: created for R-R3-21 / R-R3-08 (display in step with audio).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "gui/RemoteDisplayPresenter.h"
#include "core/session/media/SpectrumEndpoint.h"

#include <algorithm>
#include <cmath>

namespace NereusSDR {

RemoteSpectrumCapture::RemoteSpectrumCapture(const SpectrumEndpointContext& context,
                                               double centreHz, double sampleRateHz)
    : sourceCentreHz(centreHz), sourceSampleRateHz(sampleRateHz),
      m_context(std::make_shared<const SpectrumEndpointContext>(context))
{
}

const SpectrumEndpointContext& RemoteSpectrumCapture::context() const
{
    static const SpectrumEndpointContext unavailable;
    return m_context ? *m_context : unavailable;
}

bool RemoteSpectrumCapture::operator==(const RemoteSpectrumCapture& other) const
{
    if (sourceCentreHz != other.sourceCentreHz
        || sourceSampleRateHz != other.sourceSampleRateHz) { return false; }
    if (m_context == other.m_context) { return true; }
    const auto& a = context();
    const auto& b = other.context();
    return a.codec.endpointId == b.codec.endpointId
        && a.codec.contextGeneration == b.codec.contextGeneration
        && a.codec.minDbm == b.codec.minDbm && a.codec.maxDbm == b.codec.maxDbm
        && a.codec.traceSamples == b.codec.traceSamples
        && a.codec.waterfallSamples == b.codec.waterfallSamples
        && a.codec.wideSamples == b.codec.wideSamples
        && a.source == b.source && a.sourceGeneration == b.sourceGeneration
        && a.exactCentreHz == b.exactCentreHz && a.exactSpanHz == b.exactSpanHz
        && a.wideCentreHz == b.wideCentreHz && a.wideSpanHz == b.wideSpanHz
        && a.targetFps == b.targetFps && a.framesPerLine == b.framesPerLine
        && a.wideband == b.wideband;
}

std::optional<qint64> audioPresentationMapNs(const AudioDelayInputs& inputs)
{
    const std::optional<AudioDelayEstimate> estimate = measureAudioDelay(inputs);
    if (!estimate || !inputs.offset) {
        return std::nullopt;
    }
    // delay = play time - (capture - offset), so play time - capture is the
    // delay minus the offset, whatever error the offset carries.
    return std::llround(estimate->delayMs * 1'000'000.0) - inputs.offset->offsetNs;
}

void DisplayDelayFollower::reset()
{
    m_mapNs.reset();
    m_pendingStepNs.reset();
    m_holdNs.reset();
    m_smoothNs = 0;
    m_lastNs = 0;
}

void DisplayDelayFollower::set(qint64 mapNs, qint64 nowNs)
{
    m_mapNs = mapNs;
    m_smoothNs = double(mapNs);
    m_lastNs = nowNs;
    m_pendingStepNs.reset();
}

void DisplayDelayFollower::observe(qint64 mapNs, qint64 nowNs, qint64 accuracyNs,
                                   std::optional<qint64> holdNs)
{
    const bool holdStepped = holdNs && m_holdNs && std::llabs(*holdNs - *m_holdNs) >= kStepNs;
    if (holdNs) {
        m_holdNs = holdNs;
    }
    if (!m_mapNs || holdStepped) {
        set(mapNs, nowNs);
        return;
    }
    const qint64 thresholdNs = std::max<qint64>(kStepNs, 2 * std::max<qint64>(0, accuracyNs));
    if (std::llabs(mapNs - *m_mapNs) >= thresholdNs) {
        // One reading this far off may be the reading's noise; two in a row
        // that agree are a change of delay.
        if (m_pendingStepNs && std::llabs(mapNs - *m_pendingStepNs) < thresholdNs) {
            set(mapNs, nowNs);
        } else {
            m_pendingStepNs = mapNs;
        }
        return;
    }
    m_pendingStepNs.reset();
    const double elapsed = double(std::max<qint64>(0, nowNs - m_lastNs));
    m_lastNs = nowNs;
    const double alpha = std::min(1.0, elapsed / double(kSmoothingNs));
    m_smoothNs += (double(mapNs) - m_smoothNs) * alpha;
    m_mapNs = std::llround(m_smoothNs);
}

void RemoteDisplayPresenter::reset()
{
    m_queue.clear();
    restartChain();
}

void RemoteDisplayPresenter::restartChain()
{
    m_lastRow.reset();
    m_repeats = 0;
    m_rowDebt = 0;
    m_waiting = false;
}

void RemoteDisplayPresenter::enqueue(Item item)
{
    m_queue.push_back(std::move(item));
    while (int(m_queue.size()) > kMaxQueued) {
        m_queue.pop_front();
        ++m_counters.itemsDropped;
    }
}

void RemoteDisplayPresenter::push(const DisplayCodecFrame& frame, double centreHz,
                                  double spanHz)
{
    SpectrumEndpointContext context;
    context.codec = frame.context;
    context.exactCentreHz = centreHz;
    context.exactSpanHz = spanHz;
    push(frame, RemoteSpectrumCapture{context, 0.0, 0.0});
}

void RemoteDisplayPresenter::push(const DisplayCodecFrame& frame,
                                  const RemoteSpectrumCapture& capture)
{
    const double centreHz = capture.context().exactCentreHz;
    const double spanHz = capture.context().exactSpanHz;
    Item item;
    item.kind = Kind::Frame;
    item.frame = frame;
    item.capture = capture;
    item.centreHz = centreHz;
    item.spanHz = spanHz;
    item.producerNs = qint64(frame.producerTimestamp);
    if (frame.waterfallAdvance) {
        const bool sameChain = m_lastRow && m_rowPeriodNs > 0
            && m_lastRow->centreHz == centreHz && m_lastRow->spanHz == spanHz
            && m_lastRow->capture == capture
            && m_lastRow->waterfallDbm.size() == frame.waterfallDbm.size()
            && m_lastRow->wideDbm.size() == frame.wideDbm.size()
            && item.producerNs > m_lastRow->producerNs;
        if (sameChain) {
            // The row gapSlots between the last good row and this one: a lost
            // message's rows, or rows the Core never sent. Those already
            // shown as repeats keep their place; the rest blend from the
            // last good row to this one, so the time axis stays true.
            const qint64 gapNs = item.producerNs - m_lastRow->producerNs;
            const qint64 gapSlots =
                std::llround(double(gapNs) / double(m_rowPeriodNs)) - 1;
            if (m_repeats > gapSlots) {
                // The keyframe came after the slots already repeated: those
                // rows stood in for this one and the next ones too.
                m_rowDebt = m_repeats - int(std::max<qint64>(0, gapSlots));
            }
            if (gapSlots > 0 && gapSlots <= kMaxGapRows) {
                const bool blendWide = !m_lastRow->wideDbm.isEmpty()
                    && m_lastRow->wideDbm.size() == frame.wideDbm.size();
                for (qint64 slot = m_repeats + 1; slot <= gapSlots; ++slot) {
                    const float weight = float(double(slot) / double(gapSlots + 1));
                    Item row;
                    row.kind = Kind::Blended;
                    row.capture = capture;
                    row.centreHz = centreHz;
                    row.spanHz = spanHz;
                    row.producerNs = m_lastRow->producerNs + gapNs * slot / (gapSlots + 1);
                    row.frame.context = frame.context;
                    row.frame.producerTimestamp = quint64(row.producerNs);
                    row.frame.waterfallAdvance = true;
                    row.frame.waterfallDbm.resize(frame.waterfallDbm.size());
                    for (int i = 0; i < frame.waterfallDbm.size(); ++i) {
                        const float from = m_lastRow->waterfallDbm.at(i);
                        row.frame.waterfallDbm[i] =
                            from + (frame.waterfallDbm.at(i) - from) * weight;
                    }
                    if (blendWide) {
                        row.frame.wideDbm.resize(frame.wideDbm.size());
                        for (int i = 0; i < frame.wideDbm.size(); ++i) {
                            const float from = m_lastRow->wideDbm.at(i);
                            row.frame.wideDbm[i] = from + (frame.wideDbm.at(i) - from) * weight;
                        }
                    } else {
                        row.frame.wideDbm = m_lastRow->wideDbm;
                    }
                    enqueue(std::move(row));
                    ++m_counters.rowsBlended;
                }
            }
        }
        if (!sameChain) { m_rowDebt = 0; }
        m_lastRow = LastRow{item.producerNs, frame.waterfallDbm, frame.wideDbm,
                            frame.context, capture, centreHz, spanHz, std::nullopt};
        m_repeats = 0;
        if (m_rowDebt > 0) {
            // Its slot was already drawn by a repeat: the trace only.
            item.frame.waterfallAdvance = false;
            --m_rowDebt;
        }
    }
    // Any decoded frame ends a wait: the decoder took a keyframe.
    m_waiting = false;
    enqueue(std::move(item));
}

void RemoteDisplayPresenter::noteLoss()
{
    if (!m_waiting) {
        m_waiting = true;
        ++m_counters.keyframeWaits;
    }
}

qint64 RemoteDisplayPresenter::dueNs(const Item& item, qint64 nowNs,
                                     std::optional<qint64> mapNs) const
{
    if (!mapNs) {
        return nowNs;
    }
    const qint64 due = item.producerNs + *mapNs;
    return due > nowNs + kMaxHoldNs ? nowNs : due;
}

std::optional<qint64> RemoteDisplayPresenter::nextRepeatNs() const
{
    if (!m_waiting || !m_queue.empty() || !m_lastRow || !m_lastRow->presentedNs
        || m_rowPeriodNs <= 0 || m_repeats >= kMaxGapRows) {
        return std::nullopt;
    }
    return *m_lastRow->presentedNs + qint64(m_repeats + 1) * m_rowPeriodNs;
}

std::vector<RemoteDisplayPresenter::Item> RemoteDisplayPresenter::takeDue(
    qint64 nowNs, std::optional<qint64> mapNs)
{
    std::vector<Item> due;
    while (!m_queue.empty() && dueNs(m_queue.front(), nowNs, mapNs) <= nowNs) {
        Item item = std::move(m_queue.front());
        m_queue.pop_front();
        if (item.kind == Kind::Frame && m_lastRow
            && item.producerNs == m_lastRow->producerNs) {
            m_lastRow->presentedNs = nowNs;
        }
        due.push_back(std::move(item));
    }
    // Waiting for a keyframe with nothing queued: each row slot that passes
    // shows the last good row again, until the keyframe's rows blend in.
    while (const std::optional<qint64> repeatNs = nextRepeatNs()) {
        if (*repeatNs > nowNs) {
            break;
        }
        ++m_repeats;
        Item row;
        row.kind = Kind::Repeated;
        row.capture = m_lastRow->capture;
        row.centreHz = m_lastRow->centreHz;
        row.spanHz = m_lastRow->spanHz;
        row.producerNs = m_lastRow->producerNs + qint64(m_repeats) * m_rowPeriodNs;
        row.frame.context = m_lastRow->context;
        row.frame.producerTimestamp = quint64(row.producerNs);
        row.frame.waterfallAdvance = true;
        row.frame.waterfallDbm = m_lastRow->waterfallDbm;
        row.frame.wideDbm = m_lastRow->wideDbm;
        due.push_back(std::move(row));
        ++m_counters.rowsRepeated;
    }
    return due;
}

std::optional<qint64> RemoteDisplayPresenter::nextDueNs(qint64 nowNs,
                                                        std::optional<qint64> mapNs) const
{
    if (!m_queue.empty()) {
        return dueNs(m_queue.front(), nowNs, mapNs);
    }
    return nextRepeatNs();
}

} // namespace NereusSDR
