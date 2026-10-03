// no-port-check: NereusSDR-original. See PaProfilesFacade.h.
#include "core/session/PaProfilesFacade.h"

#include "core/PaProfile.h"
#include "core/PaProfileManager.h"
#include "models/Band.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <cmath>

namespace NereusSDR {

namespace {

// The page's spin boxes show one decimal place; so does the mirror.
double oneDecimal(float value)
{
    return std::round(static_cast<double>(value) * 10.0) / 10.0;
}

} // namespace

PaProfilesFacade::PaProfilesFacade(QObject* parent) : QObject(parent) {}

void PaProfilesFacade::bind(PaProfileManager* manager, std::function<HPSDRModel()> model)
{
    if (m_manager) {
        disconnect(m_manager, nullptr, this, nullptr);
    }
    m_manager = manager;
    m_model = std::move(model);
    if (m_manager) {
        connect(m_manager, &PaProfileManager::profileListChanged, this, &PaProfilesFacade::refresh);
        connect(m_manager, &PaProfileManager::activeProfileChanged, this, &PaProfilesFacade::refresh);
        connect(m_manager, &PaProfileManager::profileDataChanged, this, &PaProfilesFacade::refresh);
        connect(m_manager, &PaProfileManager::profileSaved, this, &PaProfilesFacade::refresh);
        connect(m_manager, &PaProfileManager::bankLoaded, this, &PaProfilesFacade::refresh);
    }
    refresh();
}

void PaProfilesFacade::refresh()
{
    const QString next = m_manager
        ? jsonFor(*m_manager, m_model ? m_model() : HPSDRModel::FIRST) : QString();
    if (next == m_json) {
        return;
    }
    m_json = next;
    ++m_revision;
    emit changed();
}

QString PaProfilesFacade::jsonFor(const PaProfileManager& manager, HPSDRModel model)
{
    const QString active = manager.activeProfileName();
    const PaProfile* profile = manager.activeProfile();
    if (profile == nullptr) {
        return {};
    }
    QJsonArray names;
    for (const QString& name : manager.userVisibleProfileNames(model, active)) {
        names.append(name);
    }
    QJsonArray bands;
    for (int n = 0; n < PaProfile::kBandCount; ++n) {
        const Band band = static_cast<Band>(n);
        QJsonArray adjust;
        for (int step = 0; step < PaProfile::kDriveSteps; ++step) {
            adjust.append(oneDecimal(profile->getAdjust(band, step)));
        }
        bands.append(QJsonObject{
            {QStringLiteral("band"), bandLabel(band)},
            {QStringLiteral("gain"), oneDecimal(profile->getGainForBand(band))},
            {QStringLiteral("adjust"), adjust},
            {QStringLiteral("maxPower"), oneDecimal(profile->getMaxPower(band))},
            {QStringLiteral("useMax"), profile->getMaxPowerUse(band)}});
    }
    const QJsonObject root{{QStringLiteral("names"), names},
                           {QStringLiteral("active"), active},
                           {QStringLiteral("factory"), profile->isFactoryDefault()},
                           {QStringLiteral("bands"), bands}};
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

} // namespace NereusSDR
