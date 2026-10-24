// =================================================================
// src/gui/spectrum/RemoteWaterfallRowClock.cpp  (NereusSDR-native)
// =================================================================
// 2026-10-21  J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// 2026-10-22  J.J. Boyd (KG4VCF), AI-assisted via Claude Code: the
//               optional trace observer.
// See RemoteWaterfallRowClock.h for design rationale.
// =================================================================
#include "RemoteWaterfallRowClock.h"

#include <algorithm>
#include <cmath>

namespace NereusSDR {

void RemoteWaterfallRowClock::setPeriodNs(qint64 periodNs)
{
    periodNs = std::max<qint64>(0, periodNs);
    if (periodNs == m_periodNs) {
        return;
    }
    m_periodNs = periodNs;
    resetState();
    if (m_observer) {
        Event event;
        event.kind = Event::Kind::Reanchor;
        event.reason = Reason::PeriodChange;
        notify(event);
    }
}

void RemoteWaterfallRowClock::notify(const Event& event) const
{
    if (m_observer) {
        m_observer(event);
    }
}

void RemoteWaterfallRowClock::reset(const char* cause)
{
    const bool wasAnchored = m_anchored;
    resetState();
    if (wasAnchored && m_observer) {
        Event event;
        event.kind = Event::Kind::Reanchor;
        event.reason = Reason::Reset;
        event.cause = cause;
        notify(event);
    }
}

void RemoteWaterfallRowClock::resetState()
{
    m_anchored = false;
    m_starved = false;
    m_slideStartNs = 0.0;
    m_slotNs = 0.0;
    m_glideStartNs = 0.0;
    m_glideEndNs = 0.0;
    m_progress = 1.0;
    m_rate = 1.0;
    m_errorNs = 0.0;
    m_starvedSlots = 0;
    m_reanchors = 0;
}

void RemoteWaterfallRowClock::anchorAt(double slotNs)
{
    m_slideStartNs = slotNs - double(m_periodNs) / m_rate;
    m_slotNs = slotNs;
    m_anchored = true;
    m_starved = false;
    if (m_observer) {
        Event event;
        event.kind = Event::Kind::Reanchor;
        event.reason = Reason::FirstRow;
        event.slotNs = slotNs;
        notify(event);
    }
}

void RemoteWaterfallRowClock::startAgain(Reason reason, double errorNs)
{
    // The grid starts again at the next row, at the nominal rate: a rate
    // learnt against rows that are gone says nothing of the next ones.
    ++m_reanchors;
    m_errorNs = 0.0;
    m_rate = 1.0;
    m_anchored = false;
    if (m_observer) {
        Event event;
        event.kind = Event::Kind::Reanchor;
        event.reason = reason;
        event.errorNs = errorNs;
        notify(event);
    }
}

int RemoteWaterfallRowClock::advance(qint64 nowNs, int queuedRows, qint64 frontDueNs,
                                     qint64 frontQueuedNs, int maxRows)
{
    if (!running()) {
        return 0;
    }
    const double now = double(nowNs);
    const double period = double(m_periodNs);
    const double reanchorNs = kReanchorRows * period;
    maxRows = std::clamp(maxRows, 0, kMaxRowsPerFrame);
    int rows = 0;
    while (rows < maxRows && queuedRows - rows > 0) {
        const double step = period / m_rate;
        // The slot this row should have: its due time, but never sooner than
        // a row period and a margin for the hand-off's jitter after it was
        // handed over, the shortest glide it can have with the row in hand
        // at its slide start. Only the oldest row's times are known; a
        // second row this frame (playing out rows far behind) takes the
        // grid's next slot.
        double target = m_slotNs + step;
        if (rows == 0) {
            const double earliest = double(frontQueuedNs) + period + double(kLateMarginNs);
            target = std::max(double(frontDueNs), earliest);
        }
        if (!m_anchored) {
            // The grid starts at this row: it slides in from one row period
            // before its due time.
            if (now < target - step) {
                break;
            }
            anchorAt(target);
            ++rows;
            continue;
        }
        if (m_starved && rows == 0) {
            // It was not in hand when its slide should have started, and the
            // glide rested. It goes in now, its slot a row period on, unless
            // its due time is later than that (the waterfall rests rather
            // than plays a row early) or a few rows earlier: then the grid
            // starts again at it. Resting is decided by the due time alone.
            // A row bound by its hand-off (its target the hand-off plus a
            // row period and the margin) takes the later of the two slots
            // and keeps the rate: starting again there reset the rate
            // learnt against a drifting Core on most late hand-offs (fix
            // wave, second review).
            m_starved = false;
            const double lateSlot = now + step;
            if (double(frontDueNs) > lateSlot) {
                startAgain(Reason::LateRowDueLater, double(frontDueNs) - lateSlot);
                continue;
            }
            if (lateSlot - target > reanchorNs) {
                startAgain(Reason::LateRowFarBehind, target - lateSlot);
                continue;
            }
            m_slideStartNs = now;
            m_slotNs = std::max(lateSlot, target);
            ++rows;
            continue;
        }
        // The next row's slide starts at the last row's slot.
        if (now < m_slotNs) {
            break;
        }
        const double scheduled = m_slotNs + step;
        const double error = target - scheduled;
        if (rows == 0 && std::abs(error) > reanchorNs) {
            startAgain(Reason::ErrorBeyondBound, error);
            continue;
        }
        // The error is taken both ways for the phase and the rate. A row
        // whose slot is bound by its hand-off (no due time, or due sooner
        // than a row period after it came) has a target with the margin in
        // it, so the grid settles that margin behind the rows' arrival
        // instead of pressing against it; clamping its early errors let the
        // rate only fall, and a faster Core's rows piled up until the grid
        // started again (fix wave finding 2).
        double correction = 0.0;
        if (rows == 0) {
            m_errorNs += kErrorSmoothing * (error - m_errorNs);
            const double maxStep = kMaxPhaseStepRows * step;
            correction = std::clamp(kPhaseGain * m_errorNs, -maxStep, maxStep);
            m_rate = std::clamp(m_rate - kRateGain * m_errorNs / period,
                                1.0 - kMaxRateTrim, 1.0 + kMaxRateTrim);
        }
        m_slideStartNs = m_slotNs;
        m_slotNs = scheduled + correction;
        ++rows;
    }
    if (rows == 0 && m_anchored && queuedRows <= 0 && now >= m_slotNs && !m_starved) {
        // At the slide start with no row in hand: the glide rests at the
        // full row until one comes.
        m_starved = true;
        ++m_starvedSlots;
        if (m_observer) {
            notify(Event{});
        }
    }
    if (rows > 0) {
        if (m_slotNs <= now) {
            // Rows far behind the grid (a stall): the last one put in this
            // frame glides in over a part of a row period, so the play-out
            // moves on every frame instead of jumping whole rows at rest.
            m_glideStartNs = now;
            m_glideEndNs = now + period / m_rate / double(kMaxRowsPerFrame);
        } else {
            m_glideStartNs = m_slideStartNs;
            m_glideEndNs = m_slotNs;
        }
    }
    if (m_anchored && m_glideEndNs > m_glideStartNs) {
        m_progress = std::clamp((now - m_glideStartNs) / (m_glideEndNs - m_glideStartNs),
                                0.0, 1.0);
    } else {
        m_progress = 1.0;
    }
    return rows;
}

} // namespace NereusSDR
