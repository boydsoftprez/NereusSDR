// =================================================================
// src/gui/applets/AppletVisibilityController.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. See header for attribution.
//
// =================================================================

#include "AppletVisibilityController.h"
#include "core/AppSettings.h"
#include "gui/containers/ContainerWorkspaceStore.h"
#include "gui/containers/ContainerContentRegistry.h"

namespace NereusSDR {

AppletVisibilityController::AppletVisibilityController(QObject* parent)
    : QObject(parent)
{
}

void AppletVisibilityController::setWorkspaceAdapter(ContainerWorkspaceStore* store, ContainerContentRegistry* registry)
{
    if (m_store) { disconnect(m_store, nullptr, this, nullptr); }
    m_store = store; m_registry = registry;
    if (store) { connect(store, &ContainerWorkspaceStore::committed, this, &AppletVisibilityController::syncWorkspace); syncWorkspace(); }
}
void AppletVisibilityController::syncWorkspace()
{
    if (!m_store) { return; }
    const WorkspaceDocument document = m_store->snapshot();
    for (const QString& id : m_order) {
        const QString type = ContainerContentRegistry::appletTypeForVisibilityId(id);
        bool found = false;
        for (const auto& c : document.containers) {
            for (const auto& content : c.contents) {
                if (content.typeId != type) { continue; }
                found = true; Entry& entry = m_entries[id];
                if (entry.visible != content.visible) {
                    const bool wasEffective = entry.visible && entry.available;
                    entry.visible = content.visible;
                    emit visibilityChanged(id, entry.visible);
                    if (wasEffective != (entry.visible && entry.available)) { emit effectiveVisibilityChanged(id, entry.visible && entry.available); }
                }
                break;
            }
            if (found) { break; }
        }
    }
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

    if (m_store) { e.visible = defaultVisible; syncWorkspace(); return; }
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

    if (m_store) {
        WorkspaceDocument document = m_store->snapshot();
        bool found = false;
        const QString type = ContainerContentRegistry::appletTypeForVisibilityId(id);
        for (auto& c : document.containers) {
            for (auto& content : c.contents) {
                if (content.typeId == type) { content.visible = visible; found = true; break; }
            }
            if (found) { break; }
        }
        if (!found) { return; }
        const CommitResult result = m_store->commit(document, document.revision);
        if (result.status != CommitStatus::Saved) {
            m_storageError = result.error; emit persistenceFailed(result.error);
            // Restore toggled menu UI too: the committed preference still wins.
            emit visibilityChanged(id, it->visible);
        } else { m_storageError.clear(); }
        return;
    }
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
    if (m_registry) { m_registry->setAvailable(ContainerContentRegistry::appletTypeForVisibilityId(id), available); }
    const bool nowEffective = it->visible && it->available;

    emit availabilityChanged(id, available);
    if (wasEffective != nowEffective) {
        emit effectiveVisibilityChanged(id, nowEffective);
    }
}

} // namespace NereusSDR
