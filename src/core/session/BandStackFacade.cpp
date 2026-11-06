// no-port-check: NereusSDR-original. See BandStackFacade.h.
#include "core/session/BandStackFacade.h"

#include <QJsonDocument>
#include <QJsonObject>

#include "models/bandstack/BandStackController.h"

namespace NereusSDR {

BandStackFacade::BandStackFacade(QObject* parent) : QObject(parent) {}

void BandStackFacade::bind(BandStackController* controller)
{
    if (m_controller) {
        disconnect(m_controller, nullptr, this, nullptr);
    }
    m_controller = controller;
    if (m_controller) {
        connect(m_controller, &BandStackController::changed, this, &BandStackFacade::refresh);
    }
    refresh();
}

void BandStackFacade::refresh()
{
    const QString next = m_controller ? jsonFor(*m_controller) : QString();
    if (next == m_json) {
        return;
    }
    m_json = next;
    ++m_revision;
    emit changed();
}

QString BandStackFacade::jsonFor(const BandStackController& controller)
{
    QJsonObject document = controller.store().toJson();
    document.insert(QStringLiteral("lastExpiry"),
                    QJsonObject{{QStringLiteral("batchId"), controller.lastExpiryBatchId()},
                                {QStringLiteral("count"), controller.lastExpiryCount()}});
    const BandStackLastRecall recall = controller.lastRecall();
    document.insert(QStringLiteral("lastRecall"),
                    QJsonObject{{QStringLiteral("seq"), static_cast<qint64>(recall.seq)},
                                {QStringLiteral("sliceId"), recall.sliceId},
                                {QStringLiteral("applyView"), recall.applyView},
                                {QStringLiteral("centerHz"), recall.centerHz},
                                {QStringLiteral("spanHz"), recall.spanHz}});
    return QString::fromUtf8(QJsonDocument(document).toJson(QJsonDocument::Compact));
}

bool BandStackFacade::applyStationValue(const QByteArray& propertyName, const QVariant& value)
{
    if (propertyName == "json") {
        const QString next = value.toString();
        if (!m_controller && next != m_json) {
            m_json = next;
            emit changed();
        }
        return true;
    }
    if (propertyName == "revision") {
        const quint32 next = value.toUInt();
        if (!m_controller && next != m_revision) {
            m_revision = next;
            emit changed();
        }
        return true;
    }
    return false;
}

void BandStackFacade::clearStationValues()
{
    if (m_controller || (m_json.isEmpty() && m_revision == 0)) {
        return;
    }
    m_json.clear();
    m_revision = 0;
    emit changed();
}

} // namespace NereusSDR
