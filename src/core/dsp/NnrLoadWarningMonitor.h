// no-port-check: NereusSDR-original receive-input warning policy.
// 2026-10-31 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#pragma once

#include <QHash>
#include <QtGlobal>
#include <optional>

namespace NereusSDR {

// Main-thread bookkeeping only. Busy time and late DSP blocks deliberately
// have no input here: neither establishes that receive input was missed.
class NnrLoadWarningMonitor {
public:
    static constexpr qint64 kLagMs = 250;
    static constexpr qint64 kTroubleHoldMs = 2000;
    static constexpr qint64 kHealthyHoldMs = 5000;
    // Two sampler intervals tolerate scheduling jitter without letting a
    // paused event loop fill a dwell window with an old batch observation.
    static constexpr qint64 kFreshForMs = 1000;
    static constexpr qint64 kUnsupportedExpiryMs = 5000;

    struct Context {
        quintptr worker{0};
        quintptr channel{0};
        quint64 epoch{0};
        int stream{0};
        int rate{0};
        int model{0};
        int sourceKind{0};
        quint64 routeIdentity{0};
        bool operator==(const Context&) const = default;
    };
    struct Observation {
        Context context;
        quint64 sequence{0};
        qint64 observedMs{0};
        qint64 delayMs{0};
        qint64 droppedMs{0};
    };

    bool observe(int sliceId, qint64 nowMs, bool nnrRunning,
                 const std::optional<Observation>& input)
    {
        if (!nnrRunning) {
            forget(sliceId);
            return false;
        }
        State& state = m_states[sliceId];
        if (input && (!state.context || *state.context != input->context)) {
            state = State{};
            state.context = input->context;
        }
        if (!input || input->sequence == 0 || input->sequence == state.sequence
            || input->observedMs > nowMs || nowMs - input->observedMs > kFreshForMs
            || input->delayMs < 0 || input->droppedMs < 0) {
            state.badSince.reset();
            state.healthySince.reset();
            if (state.lastFresh && nowMs - *state.lastFresh >= kUnsupportedExpiryMs) {
                state.warning = false;
            }
            return state.warning;
        }
        // A gap is not evidence about the time between the observations.
        if (state.lastFresh && (nowMs < *state.lastFresh
            || nowMs - *state.lastFresh > kFreshForMs)) {
            state.badSince.reset();
            state.healthySince.reset();
            if (nowMs < *state.lastFresh
                || nowMs - *state.lastFresh >= kUnsupportedExpiryMs) {
                state.warning = false;
            }
        }
        if (state.sequence != 0 && (input->sequence < state.sequence
            || input->droppedMs < state.droppedMs
            || input->observedMs <= state.observedMs)) {
            state = State{};
            state.context = input->context;
        }
        // A cumulative increase across an unsupported gap may be old loss,
        // even when the latest batch is fresh. Seed again before judging it.
        const bool continuousBaseline = state.lastFresh && nowMs >= *state.lastFresh
            && nowMs - *state.lastFresh <= kFreshForMs;
        const bool lostInput = state.sequence != 0 && continuousBaseline
            && input->droppedMs > state.droppedMs;
        state.sequence = input->sequence;
        state.observedMs = input->observedMs;
        state.droppedMs = input->droppedMs;
        state.lastFresh = nowMs;
        if (lostInput) {
            state.warning = true;
        }
        if (lostInput || input->delayMs >= kLagMs) {
            state.healthySince.reset();
            if (!state.badSince) {
                state.badSince = nowMs;
            }
            if (nowMs - *state.badSince >= kTroubleHoldMs) {
                state.warning = true;
            }
        } else {
            state.badSince.reset();
            if (!state.healthySince) {
                state.healthySince = nowMs;
            }
            if (nowMs - *state.healthySince >= kHealthyHoldMs) {
                state.warning = false;
            }
        }
        return state.warning;
    }

    void forget(int sliceId) { m_states.remove(sliceId); }
    void clear() { m_states.clear(); }

private:
    struct State {
        std::optional<Context> context;
        quint64 sequence{0};
        qint64 observedMs{0};
        qint64 droppedMs{0};
        std::optional<qint64> lastFresh;
        std::optional<qint64> badSince;
        std::optional<qint64> healthySince;
        bool warning{false};
    };
    QHash<int, State> m_states;
};

} // namespace NereusSDR
