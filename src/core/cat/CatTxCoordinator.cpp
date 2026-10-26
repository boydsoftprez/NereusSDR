// no-port-check: NereusSDR-original CAT accepted-activation ownership.
// 2026-10-04 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
// 2026-10-26 - A CAT TX during the walk back to receive is held and runs again
//              at Rx (JJ's ruling, like Thetis). J.J. Boyd (KG4VCF),
//              AI-assisted via Anthropic Claude Code.
#include "CatTxCoordinator.h"
#include "core/LogCategories.h"
#include "core/TwoToneController.h"
#include "core/TxSliceArbiter.h"
#include "core/session/SliceAccessPolicy.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include <QScopeGuard>
#include <limits>
namespace NereusSDR {
namespace {
// How long a TX held during the walk back to receive waits for Rx: the
// longest MoxController's TX-to-RX walk runs on its own bounds
// (space_mox_delay, the TX drain bound, mox_delay/key_up_delay and
// ptt_out_delay; MoxController.h). The send ring's bound is added when held.
// Thetis has no such bound: its CATPTT level simply waits for PollPTT
// (console.cs:25476-25517 [v2.10.3.15]).
constexpr int kHeldTxTimeoutMs = MoxController::kSpaceDelayMs + MoxController::kTxDrainTimeoutMs
    + MoxController::kKeyUpDelayMs + MoxController::kPttOutDelayMs;
} // namespace
CatTxCoordinator::CatTxCoordinator(RadioModel& model) : QObject(&model), m_model(&model)
{
    m_heldTimer.setSingleShot(true);
    connect(&m_heldTimer, &QTimer::timeout, this, [this] {
        if (!m_heldSession) { return; }
        qCWarning(lcCat) << "held CAT TX dropped: the return to receive took longer than"
                         << m_heldTimer.interval() << "ms";
        m_heldSession = 0; m_heldReplay = {};
    });
    connect(model.moxController(), &MoxController::stateChanged, this, [this](MoxState state) {
        if (state != MoxState::Rx || !m_heldSession) { return; }
        // The walk reached receive in time. Act once it has fully ended.
        m_heldTimer.stop();
        QMetaObject::invokeMethod(this, [this] { runHeld(); }, Qt::QueuedConnection);
    });
    connect(model.moxController(), &MoxController::requestAccepted, this,
        [this](const KeyerIdentity& requester, quint64 generation, bool requestedOn) {
            // Another keyer took transmit while a TX was held: it keeps it.
            if (requestedOn && !m_replaying) { dropHeld("another keyer took transmit"); }
            if (!m_tag) { return; }
            if (requester.requestTag == m_tag) {
                m_stamp = generation;
                if (!requestedOn && !m_requestInFlight && !m_releasing) { retire(false); }
            } else { retire(false); }
        });
    connect(&model, &RadioModel::connectionStateChanged, this, [this](ConnectionState state) {
        if (state != ConnectionState::Connected) { cancelAll(); }
    });
    connect(&model, &RadioModel::sliceRemoved, this, [this](int id) {
        if (id == m_target.sliceId) { cancelAll(); }
    });
    connect(model.sliceOwnership(), &SliceOwnership::controlRevisionChanged, this,
        [this](int id, quint64) { if (m_tag && id == m_target.sliceId) { cancelAll(); } });
    connect(model.txSliceArbiter(), &TxSliceArbiter::txBoundSliceChanged, this,
        [this](int, int id) {
            if (m_tag && (!m_selectionInFlight || id != m_target.sliceId)) { cancelAll(); }
        });
    connect(model.txSliceArbiter(), &TxSliceArbiter::pendingHandoffChanged, this,
        [this](int id) { if (m_tag && id >= 0) { cancelAll(); } });
    connect(model.moxController(), &MoxController::moxStateChanged, this,
        [this](bool on) { if (!on && m_tag && !m_releasing) { retire(false); } });
}
bool CatTxCoordinator::idle() const
{
    return m_model && m_model->ownsLocalDsp() && m_model->isConnected()
        && m_model->moxController()->state() == MoxState::Rx
        && !m_model->moxController()->isMox()
        && m_model->txSliceArbiter()->pendingHandoffSliceId() < 0;
}
bool CatTxCoordinator::targetValid() const
{
    return m_model && m_model->sliceOwnership()->matches(m_target)
        && m_model->sliceOwnership()->controlRevision(m_target.sliceId) == m_controlRevision
        && SliceAccessPolicy::mayTransmitOn(*m_model->sliceOwnership(),
            SliceOwnership::stationDevice(), m_target.sliceId)
        && m_model->txSliceArbiter()->txBoundSliceId() == m_target.sliceId
        && m_model->txSliceArbiter()->pendingHandoffSliceId() < 0;
}
bool CatTxCoordinator::requestPtt(quint64 id, int sliceId)
{ return requestTransmit(id, sliceId, CatTransmitKind::Ptt); }
void CatTxCoordinator::releasePtt(quint64 id) { releaseTransmit(id,CatTransmitKind::Ptt); }
bool CatTxCoordinator::requestTxSelection(quint64 id, int sliceId)
{
    if (!id || m_tag || m_requestInFlight || m_releasing || !idle()) { return false; }
    const QPointer<CatTxCoordinator> lifetime(this);
    m_requestInFlight = true;
    const auto request = qScopeGuard([lifetime] { if (lifetime) { lifetime->m_requestInFlight = false; } });
    const quint64 before = m_model->moxController()->acceptedRequestGeneration();
    const SliceOwnership::SliceRef target = m_model->sliceOwnership()->refOf(sliceId);
    const quint64 revision = m_model->sliceOwnership()->controlRevision(sliceId);
    if (!m_model->sliceOwnership()->matches(target)) { return false; }
    const bool accepted = m_model->txSliceArbiter()->requestHandoff(sliceId, SliceOwnership::stationDevice());
    return lifetime && accepted && idle() && m_model->sliceOwnership()->matches(target)
        && m_model->sliceOwnership()->controlRevision(sliceId) == revision
        && m_model->txSliceArbiter()->txBoundSliceId() == sliceId
        && m_model->moxController()->acceptedRequestGeneration() == before;
}
bool CatTxCoordinator::mayHold() const
{
    if (!m_model || m_tag || m_requestInFlight || m_releasing || m_replaying) { return false; }
    const MoxState state = m_model->moxController()->state();
    return m_model->ownsLocalDsp() && m_model->isConnected()
        && (state == MoxState::TxToRxInFlight || state == MoxState::TxToRxFlush)
        && !m_model->moxController()->isMox()
        && m_model->txSliceArbiter()->pendingHandoffSliceId() < 0;
}
void CatTxCoordinator::dropHeld(const char* reason)
{
    if (!m_heldSession) { return; }
    qCDebug(lcCat) << "held CAT TX cancelled:" << reason;
    m_heldSession = 0; m_heldReplay = {};
    m_heldTimer.stop();
}
void CatTxCoordinator::runHeld()
{
    if (!m_heldSession || !m_model) { return; }
    if (m_model->moxController()->state() != MoxState::Rx) { dropHeld("transmit is busy again"); return; }
    const std::function<void()> replay = std::move(m_heldReplay);
    m_heldSession = 0; m_heldReplay = {};
    const QPointer<CatTxCoordinator> lifetime(this);
    m_replaying = true;
    const auto replaying = qScopeGuard([lifetime] { if (lifetime) { lifetime->m_replaying = false; } });
    if (replay) { replay(); }
}
bool CatTxCoordinator::requestTransmit(quint64 id, int sliceId, CatTransmitKind kind,
    std::function<void()> replayAtRx)
{
    if (id && kind == CatTransmitKind::Ptt && replayAtRx && mayHold()) {
        // One held request: a newer one, from any session, replaces it. The
        // walk's bound runs from the first one held.
        m_heldSession = id; m_heldReplay = std::move(replayAtRx);
        if (!m_heldTimer.isActive()) {
            m_heldTimer.start(m_heldTimeoutForTestMs >= 0 ? m_heldTimeoutForTestMs
                : kHeldTxTimeoutMs + m_model->moxController()->sendRingWaitCeilingMs());
        }
        return true;
    }
    return requestTransmit(id, sliceId, kind);
}
bool CatTxCoordinator::requestTransmit(quint64 id, int sliceId, CatTransmitKind kind)
{
    // A newer transmit request wins over a held one.
    if (!m_replaying) { dropHeld("a newer request"); }
    if (!id || m_requestInFlight || m_releasing || !m_model) { return false; }
    if (m_tag) {
        if (m_kind != kind || m_target.sliceId != sliceId || !targetValid()
            || m_stamp != m_model->moxController()->acceptedRequestGeneration()) { return false; }
        if (m_claims.contains(id)) { return true; }
        if (kind != CatTransmitKind::Ptt) { return false; }
        m_claims.insert(id); return true;
    }
    if (!idle() || m_nextTag == std::numeric_limits<quint64>::max()) { return false; }
    m_target = m_model->sliceOwnership()->refOf(sliceId);
    m_controlRevision = m_model->sliceOwnership()->controlRevision(sliceId);
    if (!m_model->sliceOwnership()->matches(m_target)
        || !SliceAccessPolicy::mayTransmitOn(*m_model->sliceOwnership(), SliceOwnership::stationDevice(), sliceId)) { return false; }
    m_tag = ++m_nextTag; m_kind = kind;
    m_stamp = m_model->moxController()->acceptedRequestGeneration();
    m_claims.insert(id);
    const quint64 tag = m_tag;
    const quint64 before = m_stamp;
    const QPointer<CatTxCoordinator> lifetime(this);
    m_requestInFlight = true;
    const auto request = qScopeGuard([lifetime] { if (lifetime) { lifetime->m_requestInFlight = false; } });
    bool moved = false;
    {
        m_selectionInFlight = true;
        const auto selection = qScopeGuard([lifetime] { if (lifetime) { lifetime->m_selectionInFlight = false; } });
        moved = m_model->txSliceArbiter()->requestHandoff(sliceId, SliceOwnership::stationDevice());
    }
    if (!lifetime) { return false; }
    if (!moved || m_tag != tag || !idle() || !targetValid()
        || m_model->moxController()->acceptedRequestGeneration() != before) { retire(false); return false; }
    KeyerIdentity requester = KeyerIdentity::station(kind == CatTransmitKind::Ptt ? PttMode::Cat : PttMode::Manual);
    requester.program = true; requester.requestTag = tag;
    if (kind == CatTransmitKind::Ptt) { m_model->moxController()->onCatPtt(true, requester); }
    else if (kind == CatTransmitKind::Tune) { m_model->setTune(true, requester); }
    else { m_model->twoToneController()->setActive(true, requester); }
    if (!lifetime) { return false; }
    const bool accepted = m_tag == tag && targetValid() && m_stamp > before
        && m_stamp == m_model->moxController()->acceptedRequestGeneration()
        && (m_model->moxController()->isMox()
            || (kind == CatTransmitKind::TwoTone && m_model->twoToneController()->isActivationInFlight()));
    if (!accepted && m_tag == tag) { retire(false); }
    return accepted;
}
void CatTxCoordinator::retire(bool releaseOwned)
{
    if (!m_tag || m_releasing || !m_model) { return; }
    const QPointer<CatTxCoordinator> lifetime(this);
    m_releasing = true;
    const auto releasing = qScopeGuard([lifetime] { if (lifetime) { lifetime->m_releasing = false; } });
    const quint64 tag = m_tag;
    const quint64 stamp = m_stamp;
    const CatTransmitKind kind = m_kind;
    const bool owns = stamp == m_model->moxController()->acceptedRequestGeneration();
    m_claims.clear();
    m_model->moxController()->cancelPendingAdmissionIfRequest(tag);
    // Keep the tag until synchronous OFF notifications have refreshed our stamp.
    if (releaseOwned && owns) {
        KeyerIdentity requester = KeyerIdentity::station(kind == CatTransmitKind::Ptt ? PttMode::Cat : PttMode::Manual);
        requester.program = true; requester.requestTag = tag;
        if (kind == CatTransmitKind::Ptt) {
            const MoxController* mox = m_model->moxController();
            if (!mox->isMox() || (mox->currentKeyer().isStation()
                    && mox->currentKeyer().source == PttMode::Cat
                    && mox->currentKeyer().requestTag == tag
                    && m_model->sliceOwnership()->matches(m_target)
                    && m_model->txSliceArbiter()->txBoundSliceId() == m_target.sliceId)) {
                m_model->moxController()->onCatPtt(false, requester);
            }
        } else {
            const bool ended = kind == CatTransmitKind::Tune
                ? m_model->endTuneIfRequest(tag, stamp)
                : m_model->twoToneController()->endIfRequest(tag, stamp);
            if (lifetime && !ended && !m_model->moxController()->isMox()) {
                // A high-level accepted intent may still be inside its observation,
                // before the tone cycle is latched. Only that exact pending intent
                // admits this idle typed OFF; it aborts the older continuation.
                m_model->moxController()->setMox(false, requester);
            }
        }
    }
    if (!lifetime) { return; }
    m_tag = 0; m_stamp = 0;
    // Stale CAT-held input cannot fall back or release a superseding intent.
    m_model->moxController()->discardCatPttIfRequest(tag);
}
void CatTxCoordinator::releaseTransmit(quint64 id)
{
    if (!m_claims.remove(id)) { return; }
    if (m_claims.isEmpty()) { retire(true); }
}
void CatTxCoordinator::releaseTransmit(quint64 id,CatTransmitKind kind)
{
    // A PTT release from any session clears a held TX, as RX; clears
    // Thetis's CATPTT level (CAT/CATCommands.cs:822-827 [v2.10.3.15]).
    if (kind == CatTransmitKind::Ptt) { dropHeld("released"); }
    // A command-specific OFF only releases that operation's claim. The
    // existing retire path still verifies activation generation and keyer.
    if (m_kind == kind) { releaseTransmit(id); }
}
void CatTxCoordinator::cancelSession(quint64 id)
{
    if (id && id == m_heldSession) { dropHeld("its session closed"); }
    releaseTransmit(id);
}
void CatTxCoordinator::cancelAll() { dropHeld("cancelled"); retire(true); }
void CatTxCoordinator::setHeldTxTimeoutMsForTest(int ms) { m_heldTimeoutForTestMs = ms; }
} // namespace NereusSDR
