// no-port-check: NereusSDR-original.
// =================================================================
// src/core/safety/UnkeyGate.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 34 (R-IOS-03). See UnkeyGate.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 34 (R-IOS-03), with
//               AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include "core/safety/UnkeyGate.h"

#include <QTimer>

#include "core/LogCategories.h"
#include "core/MoxController.h"

namespace NereusSDR {

struct UnkeyGate::Pending {
    QPointer<QObject> context;
    std::function<void(UnkeyOutcome)> done;
    QString reason;
    bool hasContext{false};
    bool settled{false};
};

UnkeyGate::UnkeyGate(MoxController* mox, UnkeyFn unkey, StopNowFn stopNow, QObject* parent)
    : QObject(parent)
    , m_mox(mox)
    , m_unkey(std::move(unkey))
    , m_stopNow(std::move(stopNow))
    , m_scheduler([](int ms, QObject* context, std::function<void()> fire) {
        QTimer::singleShot(ms, context, std::move(fire));
    })
{
    if (m_mox) {
        // Confirmed: MOX reached receive (the TX-to-RX walk's last step).
        connect(m_mox, &MoxController::rxReady, this, [this]() {
            const QList<std::shared_ptr<Pending>> waiting = m_pending;
            for (const std::shared_ptr<Pending>& pending : waiting) {
                settle(pending, UnkeyOutcome::Confirmed);
            }
        });
    }
}

UnkeyGate::~UnkeyGate() = default;

void UnkeyGate::setScheduler(Scheduler scheduler)
{
    m_scheduler = std::move(scheduler);
}

bool UnkeyGate::inReceive() const
{
    return m_mox.isNull() || (!m_mox->isMox() && m_mox->state() == MoxState::Rx);
}

int UnkeyGate::pendingCount() const
{
    return static_cast<int>(m_pending.size());
}

void UnkeyGate::unkey(const QString& reason, QObject* context,
                      std::function<void(UnkeyOutcome)> done)
{
    if (inReceive()) {
        if (done) {
            done(UnkeyOutcome::Confirmed);
        }
        return;
    }
    auto pending = std::make_shared<Pending>();
    pending->context = context;
    pending->hasContext = context != nullptr;
    pending->done = std::move(done);
    pending->reason = reason;
    m_pending.append(pending);

    // The bound first, so a synchronous walk (timers at 0 in a test, or a
    // key already on its way down) is still answered once.
    m_scheduler(kConfirmTimeoutMs, this, [this, pending]() {
        if (pending->settled) {
            return;
        }
        // TimedOut: the transmitter is stopped now, before anyone hears it.
        qCWarning(lcDsp) << "The unkey was not confirmed in" << kConfirmTimeoutMs
                         << "ms; stopping transmit at once:" << pending->reason;
        if (m_stopNow) {
            m_stopNow(pending->reason);
        }
        settle(pending, UnkeyOutcome::TimedOut);
    });

    // The normal unkey (never gated). A key already on its way down is left
    // to finish its walk.
    if (m_mox && m_mox->isMox() && m_unkey) {
        m_unkey();
    }
    if (inReceive()) {
        settle(pending, UnkeyOutcome::Confirmed);
    }
}

void UnkeyGate::settle(const std::shared_ptr<Pending>& pending, UnkeyOutcome outcome)
{
    if (pending->settled) {
        return;
    }
    pending->settled = true;
    m_pending.removeAll(pending);
    if ((!pending->hasContext || !pending->context.isNull()) && pending->done) {
        pending->done(outcome);
    }
}

} // namespace NereusSDR
