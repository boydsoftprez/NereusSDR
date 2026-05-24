// =================================================================
// src/gui/applets/AppletVisibilityController.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. See header for attribution.
//
// =================================================================

#include "AppletVisibilityController.h"
#include "core/AppSettings.h"

namespace NereusSDR {

AppletVisibilityController::AppletVisibilityController(QObject* parent)
    : QObject(parent)
{
}

QString AppletVisibilityController::settingsKey(const QString& id)
{
    return QStringLiteral("Applet") + id + QStringLiteral("Visible");
}

void AppletVisibilityController::registerApplet(const QString& id,
                                                 const QString& displayName,
                                                 bool defaultVisible)
{
    if (id.isEmpty()) { return; }

    if (!m_entries.contains(id)) {
        m_order.append(id);
    }

    Entry& e = m_entries[id];
    e.displayName = displayName;

    const QString stored = AppSettings::instance()
        .value(settingsKey(id), QString{}).toString();
    if (stored == QStringLiteral("True")) {
        e.visible = true;
    } else if (stored == QStringLiteral("False")) {
        e.visible = false;
    } else {
        e.visible = defaultVisible;
    }
}

bool AppletVisibilityController::isVisible(const QString& id) const
{
    auto it = m_entries.find(id);
    return it != m_entries.end() ? it->visible : false;
}

bool AppletVisibilityController::isAvailable(const QString& id) const
{
    auto it = m_entries.find(id);
    return it != m_entries.end() ? it->available : false;
}

bool AppletVisibilityController::isEffectivelyVisible(const QString& id) const
{
    auto it = m_entries.find(id);
    if (it == m_entries.end()) { return false; }
    return it->visible && it->available;
}

QStringList AppletVisibilityController::registeredIds() const
{
    return m_order;
}

QString AppletVisibilityController::displayName(const QString& id) const
{
    auto it = m_entries.find(id);
    return it != m_entries.end() ? it->displayName : QString{};
}

void AppletVisibilityController::setVisible(const QString& id, bool visible)
{
    auto it = m_entries.find(id);
    if (it == m_entries.end()) { return; }
    if (it->visible == visible) { return; }

    const bool wasEffective = it->visible && it->available;
    it->visible = visible;
    const bool nowEffective = it->visible && it->available;

    AppSettings::instance().setValue(
        settingsKey(id),
        visible ? QStringLiteral("True") : QStringLiteral("False"));
    emit visibilityChanged(id, visible);
    if (wasEffective != nowEffective) {
        emit effectiveVisibilityChanged(id, nowEffective);
    }
}

void AppletVisibilityController::setAvailable(const QString& id, bool available)
{
    auto it = m_entries.find(id);
    if (it == m_entries.end()) { return; }
    if (it->available == available) { return; }

    const bool wasEffective = it->visible && it->available;
    it->available = available;
    const bool nowEffective = it->visible && it->available;

    emit availabilityChanged(id, available);
    if (wasEffective != nowEffective) {
        emit effectiveVisibilityChanged(id, nowEffective);
    }
}

} // namespace NereusSDR
