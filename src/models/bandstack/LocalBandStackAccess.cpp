// no-port-check: NereusSDR-original.
// =================================================================
// src/models/bandstack/LocalBandStackAccess.cpp  (NereusSDR)
// =================================================================
//
// See LocalBandStackAccess.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-05 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
// =================================================================

#include "models/bandstack/LocalBandStackAccess.h"

#include <QJsonDocument>

#include "core/SliceOwnership.h"
#include "core/session/BandStackFacade.h"
#include "models/RadioModel.h"
#include "models/bandstack/BandStackCommand.h"

namespace NereusSDR {

LocalBandStackAccess::LocalBandStackAccess(RadioModel* model)
    : BandStackAccess(model)
    , m_model(model)
{
    connect(model->bandStackFacade(), &BandStackFacade::changed, this,
            &LocalBandStackAccess::onFacadeChanged);
    connect(model, &RadioModel::connectionStateChanged, this, &LocalBandStackAccess::refreshState);
}

void LocalBandStackAccess::setInvoker(std::function<void(const SessionMessage&)> invoker)
{
    m_invoker = std::move(invoker);
}

void LocalBandStackAccess::reportInvokerRefusal(const QString& reason)
{
    emit refused(reason);
}

BandStackAccess::State LocalBandStackAccess::state() const
{
    return m_model && m_model->bandStackController() != nullptr && m_model->isConnected()
        ? State::Ready
        : State::NotConnected;
}

QString LocalBandStackAccess::unavailableReason() const
{
    return state() == State::Ready ? QString() : notConnectedReason();
}

BandStackController* LocalBandStackAccess::begin(const BandStackCommand& command)
{
    if (state() != State::Ready) {
        emit refused(unavailableReason());
        return nullptr;
    }
    if (m_invoker) {
        m_invoker(SessionMessages::commandInvoke(command.verb.toUtf8(), 0, command.wireArguments()));
        return nullptr;
    }
    return m_model->bandStackController();
}

void LocalBandStackAccess::finish(bool accepted, const QString& refusal)
{
    if (!accepted) {
        emit refused(refusal);
    }
}

void LocalBandStackAccess::press(int sliceId, Band band, BandStackDirection d,
                                 const BandStackView& v)
{
    if (BandStackController* controller = begin(BandStackCommand::press(sliceId, band, d, v))) {
        BandStackRecallRequest request;
        request.kind = BandStackRecallRequest::Press;
        request.sliceId = sliceId;
        request.band = band;
        request.direction = d;
        request.view = v;
        QString refusal;
        finish(controller->performRecall(request, &refusal), refusal);
    }
}

void LocalBandStackAccess::step(int sliceId, BandStackDirection d, const BandStackView& v)
{
    if (BandStackController* controller = begin(BandStackCommand::step(sliceId, d, v))) {
        BandStackRecallRequest request;
        request.kind = BandStackRecallRequest::Step;
        request.sliceId = sliceId;
        request.direction = d;
        request.view = v;
        QString refusal;
        finish(controller->performRecall(request, &refusal), refusal);
    }
}

void LocalBandStackAccess::goTo(int sliceId, const QString& entryId, const BandStackView& v)
{
    if (BandStackController* controller = begin(BandStackCommand::goTo(sliceId, entryId, v))) {
        BandStackRecallRequest request;
        request.kind = BandStackRecallRequest::GoTo;
        request.sliceId = sliceId;
        request.entryId = entryId;
        request.view = v;
        QString refusal;
        finish(controller->performRecall(request, &refusal), refusal);
    }
}

void LocalBandStackAccess::add(int sliceId, const BandStackView& v)
{
    if (BandStackController* controller = begin(BandStackCommand::add(sliceId, v))) {
        QString refusal;
        finish(controller->add(sliceId, v, &refusal), refusal);
    }
}

void LocalBandStackAccess::saveHere(int sliceId, const QString& entryId, const BandStackView& v)
{
    if (BandStackController* controller = begin(BandStackCommand::saveHere(sliceId, entryId, v))) {
        QString refusal;
        finish(controller->saveHere(sliceId, entryId, v, &refusal), refusal);
    }
}

void LocalBandStackAccess::edit(const QString& id, const std::optional<QString>& name,
                                std::optional<bool> locked)
{
    if (BandStackController* controller = begin(BandStackCommand::edit(id, name, locked))) {
        QString refusal;
        finish(controller->edit(id, name, locked, &refusal), refusal);
    }
}

void LocalBandStackAccess::remove(const QString& id)
{
    if (BandStackController* controller = begin(BandStackCommand::remove(id))) {
        QString refusal;
        finish(controller->remove(id, &refusal), refusal);
    }
}

void LocalBandStackAccess::reorder(Band band, const QStringList& ids)
{
    if (BandStackController* controller = begin(BandStackCommand::reorder(band, ids))) {
        QString refusal;
        finish(controller->reorder(band, ids, &refusal), refusal);
    }
}

void LocalBandStackAccess::setReturn(Band band, BandStackReturnMode mode, const QString& entryId)
{
    if (BandStackController* controller = begin(BandStackCommand::setReturn(band, mode, entryId))) {
        QString refusal;
        finish(controller->setReturn(band, mode, entryId, &refusal), refusal);
    }
}

void LocalBandStackAccess::clear(std::optional<Band> band)
{
    if (BandStackController* controller = begin(BandStackCommand::clear(band))) {
        QString refusal;
        finish(controller->clear(band, &refusal), refusal);
    }
}

void LocalBandStackAccess::reseed()
{
    if (BandStackController* controller = begin(BandStackCommand::reseed())) {
        QString refusal;
        finish(controller->reseed(&refusal), refusal);
    }
}

void LocalBandStackAccess::options(std::optional<bool> dupes, std::optional<int> expiryMinutes,
                                   std::optional<int> dwellSeconds)
{
    if (BandStackController* controller =
            begin(BandStackCommand::options(dupes, expiryMinutes, dwellSeconds))) {
        QString refusal;
        finish(controller->options(dupes, expiryMinutes, dwellSeconds, &refusal), refusal);
    }
}

void LocalBandStackAccess::restoreExpired(const QString& batchId)
{
    if (BandStackController* controller = begin(BandStackCommand::restoreExpired(batchId))) {
        QString refusal;
        finish(controller->restoreExpired(batchId, &refusal), refusal);
    }
}

void LocalBandStackAccess::onFacadeChanged()
{
    // The facade binds as the controller starts, so this is where it first shows.
    BandStackController* const controller = m_model ? m_model->bandStackController() : nullptr;
    if (controller != m_followed) {
        m_followed = controller;
        if (controller != nullptr) {
            connect(controller, &BandStackController::reportRecall, this,
                    &LocalBandStackAccess::onRecallReported);
        }
    }
    const QString json = m_model ? m_model->bandStackFacade()->json() : QString();
    if (json != m_json) {
        m_json = json;
        m_document = QJsonDocument::fromJson(json.toUtf8()).object();
        emit documentChanged();
    }
    refreshState();
}

void LocalBandStackAccess::onRecallReported(int sliceId, const QString& text)
{
    // A slice another device controls is that device's to report.
    const SliceOwnership* ownership = m_model ? m_model->sliceOwnership() : nullptr;
    const QByteArray owner = ownership ? ownership->mark(sliceId).owner : QByteArray();
    if (owner.isEmpty() || owner == SliceOwnership::stationDevice()) {
        emit reported(text);
    }
}

void LocalBandStackAccess::refreshState()
{
    const State now = state();
    if (now != m_lastState) {
        m_lastState = now;
        emit stateChanged();
    }
}

} // namespace NereusSDR
