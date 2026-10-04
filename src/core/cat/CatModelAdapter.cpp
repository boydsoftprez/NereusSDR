// no-port-check: NereusSDR-original CAT station admission/lifecycle.
// 2026-10-04 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include "CatModelAdapter.h"
#include "core/SliceOwnership.h"
#include "core/session/SliceAccessPolicy.h"
#include "core/safety/StationSliceFreeze.h"
#include "core/MoxController.h"
#include "core/RxChannel.h"
#include <cmath>
#include "models/RadioModel.h"
#include "models/SliceModel.h"
namespace NereusSDR {
CatModelAdapter::CatModelAdapter(RadioModel& model) : m_model(&model) {}
CatBinding CatModelAdapter::snapshotBinding(const CatBinding& configured) const
{
    CatBinding result = configured;
    result.primaryIncarnation = 0; result.secondaryIncarnation.reset();
    if (!m_model) { return result; }
    const SliceOwnership& ownership = *m_model->sliceOwnership();
    result.primaryIncarnation = ownership.refOf(result.primarySliceId).incarnation;
    if (result.secondarySliceId) { result.secondaryIncarnation = ownership.refOf(*result.secondarySliceId).incarnation; }
    return result;
}
SliceModel* CatModelAdapter::resolveSlice(const CatBinding& binding, CatVfo vfo) const
{
    if (!m_model) { return nullptr; }
    const int id = vfo == CatVfo::Primary ? binding.primarySliceId : binding.secondarySliceId.value_or(-1);
    const quint64 incarnation = vfo == CatVfo::Primary ? binding.primaryIncarnation : binding.secondaryIncarnation.value_or(0);
    if (!m_model->sliceOwnership()->matches({id, incarnation})) { return nullptr; }
    return m_model->sliceById(id);
}
bool CatModelAdapter::mayRead(const CatBinding& binding, CatVfo vfo) const
{
    const SliceModel* slice = resolveSlice(binding, vfo);
    return slice && SliceAccessPolicy::maySee(*m_model->sliceOwnership(), SliceOwnership::stationDevice(), slice->sliceIndex());
}
bool CatModelAdapter::mayChange(const CatBinding& binding, CatVfo vfo, const QByteArray& property) const
{
    const SliceModel* slice = resolveSlice(binding, vfo);
    if (!slice) { return false; }
    const SliceOwnership& ownership = *m_model->sliceOwnership();
    const int id = slice->sliceIndex();
    if (!SliceAccessPolicy::mayChange(ownership, SliceOwnership::stationDevice(), id)
        && !SliceAccessPolicy::stationMayChangeUnclaimed(ownership, id)) { return false; }
    static const QList<QByteArray> kFrozenProperties{"frequency", "dspMode", "filterLow", "filterHigh", "txAntenna", "band", "xitEnabled", "xitHz"};
    if (!kFrozenProperties.contains(property)) { return true; }
    const MoxController* controller = m_model->moxController();
    const bool stationKeyed = controller && controller->isMox() && controller->currentKeyer().isStation();
    const SliceModel* txSlice = m_model->txBoundSlice();
    return StationSliceFreeze::refusal(id,
        StationSliceFreeze::frozenSlice(stationKeyed, txSlice ? txSlice->sliceIndex() : -1),
        TxRefusals::holderOnAir(QString(), true)).isEmpty();
}
TransmitModel& CatModelAdapter::transmitModel() const { return m_model->transmitModel(); }
RadioModel& CatModelAdapter::radioModel() const { return *m_model; }
CatWriteToken CatModelAdapter::prepareWrite(const CatBinding& binding, CatVfo vfo, const QByteArray& property) const
{
    CatWriteToken token{binding, vfo, property};
    const SliceModel* slice = resolveSlice(binding, vfo);
    token.admitted = mayChange(binding, vfo, property);
    if (slice) { token.controlRevision = m_model->sliceOwnership()->controlRevision(slice->sliceIndex()); }
    return token;
}
bool CatModelAdapter::revalidateWrite(const CatWriteToken& token) const
{
    const SliceModel* slice = resolveSlice(token.binding, token.vfo);
    return token.admitted && slice
        && m_model->sliceOwnership()->controlRevision(slice->sliceIndex()) == token.controlRevision
        && mayChange(token.binding, token.vfo, token.property);
}
bool CatModelAdapter::mayChangeGlobalDsp(QString* reason) const
{
    return m_model && m_model->ownsLocalDsp() && !m_model->stationOnAirRefusal(reason);
}
bool CatModelAdapter::readRxMeter(const CatBinding& binding, CatVfo vfo, RxMeterType type, double& value) const
{
    const SliceModel* slice=resolveSlice(binding,vfo);
    if (!slice || !mayRead(binding,vfo) || !m_model->isConnected()
        || m_model->moxController()->isMox()) { return false; }
    const RxChannel* channel=m_model->rxChannelForSlice(slice->sliceIndex());
    if (!channel || !channel->isActive() || !channel->isWdspReady()) { return false; }
    // Existing lane getter requests refresh before checking the cold cache.
    // CoreSliceMeterPump uses this same pipeline; no GUI or polling thread.
    const double raw=channel->getMeter(type);
    if (!channel->meterReadingReady() || !std::isfinite(raw)) { return false; }
    value=raw;
    if (type == RxMeterType::SignalPeak || type == RxMeterType::SignalAvg) {
        value+=m_model->rxMeterOffsetDbForSlice(slice->sliceIndex());
    }
    return std::isfinite(value);
}
} // namespace NereusSDR
