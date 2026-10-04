// no-port-check: NereusSDR-original CAT accepted-activation ownership.
// 2026-10-04 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include "CatTxCoordinator.h"
#include "core/TwoToneController.h"
#include "core/TxSliceArbiter.h"
#include "core/session/SliceAccessPolicy.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include <QScopeGuard>
#include <limits>
namespace NereusSDR {
CatTxCoordinator::CatTxCoordinator(RadioModel& model) : QObject(&model), m_model(&model)
{
    connect(model.moxController(), &MoxController::requestAccepted, this,
        [this](const KeyerIdentity& requester, quint64 generation, bool requestedOn) {
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
bool CatTxCoordinator::requestTransmit(quint64 id, int sliceId, CatTransmitKind kind)
{
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
    // A command-specific OFF only releases that operation's claim. The
    // existing retire path still verifies activation generation and keyer.
    if (m_kind == kind) { releaseTransmit(id); }
}
void CatTxCoordinator::cancelSession(quint64 id) { releaseTransmit(id); }
void CatTxCoordinator::cancelAll() { retire(true); }
} // namespace NereusSDR
