// no-port-check: NereusSDR-original.
// =================================================================
// src/models/bandstack/RemoteBandStackAccess.cpp  (NereusSDR)
// =================================================================
//
// See RemoteBandStackAccess.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-05 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
// =================================================================

#include "models/bandstack/RemoteBandStackAccess.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>

#include "core/session/BandStackFacade.h"
#include "core/session/IStationLink.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/bandstack/BandStackCommand.h"

namespace NereusSDR {

namespace {

QHash<QString, QJsonObject> entriesById(const QJsonObject& document)
{
    QHash<QString, QJsonObject> entries;
    for (const QJsonValue& value : document.value(QStringLiteral("entries")).toArray()) {
        const QJsonObject entry = value.toObject();
        entries.insert(entry.value(QStringLiteral("id")).toString(), entry);
    }
    return entries;
}

QJsonArray orderOn(const QJsonObject& document, const QString& band)
{
    return document.value(QStringLiteral("bands")).toObject().value(band).toObject()
        .value(QStringLiteral("order")).toArray();
}

} // namespace

RemoteBandStackAccess::RemoteBandStackAccess(RadioModel* model)
    : BandStackAccess(model)
    , m_model(model)
{
    connect(model->bandStackFacade(), &BandStackFacade::changed, this,
            &RemoteBandStackAccess::onFacadeChanged);
    connect(model, &RadioModel::stationLinkStateChanged, this, &RemoteBandStackAccess::refreshState);
    connect(model, &RadioModel::stationBandStackRefused, this, &BandStackAccess::refused);
}

BandStackAccess::State RemoteBandStackAccess::state() const
{
    IStationLink* const link = m_model ? m_model->stationLink() : nullptr;
    if (link == nullptr || !link->stationLinkReady()) {
        return State::NotConnected;
    }
    if (!link->bandStackAvailable()) {
        return State::NeedsNewerCore;
    }
    // The Core has the stack but no radio yet.
    return m_document.isEmpty() ? State::NotConnected : State::Ready;
}

QString RemoteBandStackAccess::unavailableReason() const
{
    switch (state()) {
    case State::Ready:
        return {};
    case State::NeedsNewerCore:
        return needsNewerCoreReason();
    case State::NotConnected:
        break;
    }
    return notConnectedReason();
}

void RemoteBandStackAccess::send(const BandStackCommand& command)
{
    if (state() != State::Ready) {
        emit refused(unavailableReason());
        return;
    }
    const IStationLink::CommandOutcome outcome =
        m_model->stationLink()->sendBandStackCommand(command.verb, command.arguments);
    // What the Core refuses comes back as stationBandStackRefused.
    if (!outcome.sent) {
        emit refused(outcome.reason);
    }
}

void RemoteBandStackAccess::press(int sliceId, Band band, BandStackDirection d,
                                  const BandStackView& v)
{
    send(BandStackCommand::press(sliceId, band, d, v));
}

void RemoteBandStackAccess::step(int sliceId, BandStackDirection d, const BandStackView& v)
{
    send(BandStackCommand::step(sliceId, d, v));
}

void RemoteBandStackAccess::goTo(int sliceId, const QString& entryId, const BandStackView& v)
{
    send(BandStackCommand::goTo(sliceId, entryId, v));
}

void RemoteBandStackAccess::add(int sliceId, const BandStackView& v)
{
    send(BandStackCommand::add(sliceId, v));
}

void RemoteBandStackAccess::saveHere(int sliceId, const QString& entryId, const BandStackView& v)
{
    send(BandStackCommand::saveHere(sliceId, entryId, v));
}

void RemoteBandStackAccess::edit(const QString& id, const std::optional<QString>& name,
                                 std::optional<bool> locked)
{
    send(BandStackCommand::edit(id, name, locked));
}

void RemoteBandStackAccess::remove(const QString& id)
{
    send(BandStackCommand::remove(id));
}

void RemoteBandStackAccess::reorder(Band band, const QStringList& ids)
{
    send(BandStackCommand::reorder(band, ids));
}

void RemoteBandStackAccess::setReturn(Band band, BandStackReturnMode mode, const QString& entryId)
{
    send(BandStackCommand::setReturn(band, mode, entryId));
}

void RemoteBandStackAccess::clear(std::optional<Band> band)
{
    send(BandStackCommand::clear(band));
}

void RemoteBandStackAccess::reseed()
{
    send(BandStackCommand::reseed());
}

void RemoteBandStackAccess::options(std::optional<bool> dupes, std::optional<int> expiryMinutes,
                                    std::optional<int> dwellSeconds)
{
    send(BandStackCommand::options(dupes, expiryMinutes, dwellSeconds));
}

void RemoteBandStackAccess::restoreExpired(const QString& batchId)
{
    send(BandStackCommand::restoreExpired(batchId));
}

void RemoteBandStackAccess::onFacadeChanged()
{
    // The mirror sets `json` and `revision` one at a time.
    const QString json = m_model ? m_model->bandStackFacade()->json() : QString();
    if (json != m_json) {
        const QJsonObject before = m_document;
        m_json = json;
        m_document = QJsonDocument::fromJson(json.toUtf8()).object();
        emit documentChanged();
        reportRecall(before);
    }
    refreshState();
}

void RemoteBandStackAccess::reportRecall(const QJsonObject& before)
{
    if (m_document.isEmpty()) {
        m_seenRecall.reset();
        return;
    }
    const QJsonObject recall = m_document.value(QStringLiteral("lastRecall")).toObject();
    const qint64 seq = recall.value(QStringLiteral("seq")).toInteger();
    if (!m_seenRecall.has_value() || *m_seenRecall == seq) {
        m_seenRecall = seq;
        return;
    }
    m_seenRecall = seq;
    const int sliceId = recall.value(QStringLiteral("sliceId")).toInt(-1);
    IStationLink* const link = m_model ? m_model->stationLink() : nullptr;
    if (link == nullptr || !link->sliceControlledHere(sliceId)) {
        return;
    }
    const QString text = recallText(before, sliceId);
    if (!text.isEmpty()) {
        emit reported(text);
    }
}

// The words BandStackController::resolve gives. The entry a recall landed on is
// the slice's place on its band, stamped by the recall, and lastRecall.centerHz is
// that entry's center. Checking the center matters: the entry the slice left is
// stamped in the same millisecond and is still its place on the other band. A
// recall with no entry (last used, or an empty band) has only where it tuned in
// the document, and the mode comes from the slice.
QString RemoteBandStackAccess::recallText(const QJsonObject& before, int sliceId) const
{
    const SliceModel* const slice = m_model->sliceById(sliceId);
    if (slice == nullptr) {
        return {};
    }
    const QHash<QString, QJsonObject> entries = entriesById(m_document);
    const QHash<QString, QJsonObject> entriesBefore = entriesById(before);
    const QJsonObject places = m_document.value(QStringLiteral("places")).toObject()
                                   .value(QString(slice->sliceLetter())).toObject();
    const qint64 center = m_document.value(QStringLiteral("lastRecall")).toObject()
                              .value(QStringLiteral("centerHz")).toInteger();
    QJsonObject landed;
    qint64 latest = -1;
    for (auto it = places.constBegin(); it != places.constEnd(); ++it) {
        const QJsonObject entry = entries.value(it.value().toString());
        const qint64 used = entry.value(QStringLiteral("lastUsedMs")).toInteger(-1);
        const qint64 usedBefore = entriesBefore.value(it.value().toString())
                                      .value(QStringLiteral("lastUsedMs")).toInteger(-1);
        const qint64 entryCenter = entry.value(QStringLiteral("centerHz")).toInteger();
        const qint64 entryAt = entryCenter != 0
            ? entryCenter
            : entry.value(QStringLiteral("frequencyHz")).toInteger();
        if (entryAt == center && used > usedBefore && used > latest) {
            latest = used;
            landed = entry;
        }
    }
    if (!landed.isEmpty()) {
        const QString band = landed.value(QStringLiteral("band")).toString();
        const QJsonArray order = orderOn(m_document, band);
        qsizetype position = 0;
        while (position < order.size()
               && order.at(position).toString() != landed.value(QStringLiteral("id")).toString()) {
            ++position;
        }
        return QStringLiteral("%1: %2 %3, %4 of %5")
            .arg(band,
                 BandStackStore::displayFrequency(
                     landed.value(QStringLiteral("frequencyHz")).toInteger()),
                 landed.value(QStringLiteral("mode")).toString())
            .arg(position + 1)
            .arg(order.size());
    }
    const QString band = bandKeyName(bandFromFrequency(static_cast<double>(center)));
    if (orderOn(m_document, band).isEmpty()) {
        return QStringLiteral("%1: empty stack, tuned to the middle of the band").arg(band);
    }
    return QStringLiteral("%1: %2 %3")
        .arg(band, BandStackStore::displayFrequency(center), SliceModel::modeName(slice->dspMode()));
}

void RemoteBandStackAccess::refreshState()
{
    const State now = state();
    if (now != m_lastState) {
        m_lastState = now;
        emit stateChanged();
    }
}

} // namespace NereusSDR
