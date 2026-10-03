#pragma once
// =================================================================
// src/gui/RemoteDisplayPresenter.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original. R-R3-21 / R-R3-08: a remote window's
// spectrum and waterfall presented on its audio's playout clock, and
// ridden through a stalling link: decoded display frames wait until their
// Core timestamp plus the audio's delay, a lost message's missing waterfall
// rows are blended from the last good row to the next (or repeat the last
// while none has come), and a late burst plays out instead of being
// dropped. Pure arithmetic on injected times; it owns no timer, widget or
// session.
//
// Modification history (NereusSDR):
//   2026-10-02: Capture metadata retained through frame, blend and repeat presentation.
//               J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-09-26: created for R-R3-21 / R-R3-08 (display in step with audio).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/session/media/DisplayCodec.h"
#include "gui/AudioClockEstimator.h"
#include "gui/RemoteSpectrumCapture.h"

#include <QtGlobal>

#include <deque>
#include <optional>
#include <vector>

namespace NereusSDR {

/// The Core-to-local presentation map from one audio measurement: the local
/// time a sample plays minus the Core time it was captured, that is the
/// measured audio delay minus the Core-minus-local clock offset. A display
/// frame stamped T on the Core clock is presented at T plus this. The
/// offset's own error cancels: it enters the delay and the offset alike.
/// Nothing when the delay cannot be measured (see measureAudioDelay).
std::optional<qint64> audioPresentationMapNs(const AudioDelayInputs& inputs);

/// Follows the presentation map as the audio's delay changes, so the
/// picture stays with the sound without taking the playout reading's own
/// noise for a change of delay:
/// - a change of the audio's jitter hold of kStepNs or more (deepened after
///   a stall, or shed) is taken at once, with the map measured with it;
/// - otherwise a change of at least max(kStepNs, twice the reading's
///   accuracy) is taken only when the next reading confirms it (two in a
///   row, within that threshold of each other);
/// - smaller changes (reading jitter, the rate matcher's slow correction)
///   are smoothed over kSmoothingNs.
class DisplayDelayFollower {
public:
    static constexpr qint64 kStepNs = 15'000'000;
    static constexpr qint64 kSmoothingNs = 500'000'000;

    void reset();
    /// A measurement of the map at local time nowNs: accuracyNs is how far
    /// the playout reading may be off (RemoteAudioPlayoutPoint::accuracyNs),
    /// holdNs the audio's jitter hold with it, when known.
    void observe(qint64 mapNs, qint64 nowNs, qint64 accuracyNs = 0,
                 std::optional<qint64> holdNs = std::nullopt);
    /// Take mapNs as it is (a map not measured from audio).
    void set(qint64 mapNs, qint64 nowNs);
    std::optional<qint64> mapNs() const { return m_mapNs; }

private:
    std::optional<qint64> m_mapNs;
    std::optional<qint64> m_pendingStepNs;
    std::optional<qint64> m_holdNs;
    double m_smoothNs = 0;
    qint64 m_lastNs = 0;
};

class RemoteDisplayPresenter {
public:
    enum class Kind { Frame, Blended, Repeated };
    struct Item {
        Kind kind = Kind::Frame;
        /// Frame: as decoded. Blended and Repeated: a waterfall row only
        /// (waterfallDbm, wideDbm, waterfallAdvance true), no trace.
        DisplayCodecFrame frame;
        RemoteSpectrumCapture capture;
        /// The RF window the row was captured at (the accepted context's
        /// exact centre and span), so it is drawn at that frequency even
        /// after a tune.
        double centreHz = 0;
        double spanHz = 0;
        /// Core clock; synthesized for Blended rows at their row slot.
        qint64 producerNs = 0;
    };
    struct Counters {
        /// Lost messages that started a wait for a keyframe.
        quint64 keyframeWaits = 0;
        quint64 rowsBlended = 0;
        quint64 rowsRepeated = 0;
        /// Items dropped because the queue was full (never expected).
        quint64 itemsDropped = 0;
    };

    /// Bounds the queue: 96 items is more than 3 s at 30 frames a second,
    /// beyond the audio's 500 ms hold ceiling plus a stall.
    static constexpr int kMaxQueued = 96;
    /// A gap longer than 64 rows (about 2 s) is a pause, not a loss: it is
    /// not filled, and no more than 64 rows repeat while waiting.
    static constexpr int kMaxGapRows = 64;
    /// A map that would hold an item longer than this is not trusted: the
    /// item is presented now.
    static constexpr qint64 kMaxHoldNs = 1'500'000'000;

    /// Forget every queued item, the last row and the wait; counters stay.
    void reset();
    /// Forget the last row and the wait (a new codec context: nothing is
    /// blended across it); queued items stay and play out.
    void restartChain();
    /// The waterfall's row period on the Core clock: framesPerLine frames
    /// at the context's frame rate. 0 disables gap filling and repeats.
    void setRowPeriodNs(qint64 periodNs) { m_rowPeriodNs = periodNs; }
    qint64 rowPeriodNs() const { return m_rowPeriodNs; }

    /// A decoded frame. A waterfall row after a gap first queues the blended
    /// rows for the gap's slots that have not already repeated.
    void push(const DisplayCodecFrame& frame, double centreHz, double spanHz);
    void push(const DisplayCodecFrame& frame, const RemoteSpectrumCapture& capture);
    /// A lost message: the decoder waits for a keyframe. While it waits,
    /// each row slot that passes with nothing to show repeats the last row.
    /// A keyframe later than the slots already repeated owes those rows back:
    /// its row and the next ones up to the overrun draw their trace only, so
    /// the waterfall keeps one row per slot.
    void noteLoss();
    bool waitingForKeyframe() const { return m_waiting; }

    /// Every item due at nowNs, in order, plus any repeats that fell due.
    /// Without a map everything queued is due now (an older Core).
    std::vector<Item> takeDue(qint64 nowNs, std::optional<qint64> mapNs);
    /// When the next item or repeat falls due, or nothing.
    std::optional<qint64> nextDueNs(qint64 nowNs, std::optional<qint64> mapNs) const;
    int queued() const { return int(m_queue.size()); }
    const Counters& counters() const { return m_counters; }

private:
    struct LastRow {
        qint64 producerNs = 0;
        QVector<float> waterfallDbm;
        QVector<float> wideDbm;
        DisplayCodecContext context;
        RemoteSpectrumCapture capture;
        double centreHz = 0;
        double spanHz = 0;
        /// Local time it (or its latest repeat's slot) was taken for
        /// presentation, or nothing while it is still queued.
        std::optional<qint64> presentedNs;
    };
    qint64 dueNs(const Item& item, qint64 nowNs, std::optional<qint64> mapNs) const;
    std::optional<qint64> nextRepeatNs() const;
    void enqueue(Item item);

    std::deque<Item> m_queue;
    std::optional<LastRow> m_lastRow;
    /// Rows repeated since the last good row, each one of its slots.
    int m_repeats = 0;
    /// Rows repeated beyond a gap's end, taken back from the next rows.
    int m_rowDebt = 0;
    bool m_waiting = false;
    qint64 m_rowPeriodNs = 0;
    Counters m_counters;
};

} // namespace NereusSDR
