// =================================================================
// src/gui/applets/AppletVisibilityController.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. See header for attribution.
//
// Modification history (NereusSDR):
//   2026-10-16 - Bench fix: an applet reads ticked only while its container
//                 is shown too, and ticking it shows that container (its
//                 hidden, disabled, receive and macro flags cleared) and asks
//                 for its window to be brought onto a screen. setAvailable
//                 carries the reason the grayed applet shows. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 — Bench fix: missing workspace entries are added (see the
//                 header), by J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-11-05 - Corrections for review findings. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//
// =================================================================

#include "AppletVisibilityController.h"
#include "core/AppSettings.h"
#include "gui/containers/ContainerWorkspaceStore.h"
#include "gui/containers/ContainerContentRegistry.h"

#include <QUuid>

namespace NereusSDR {

namespace {

// The container's own flags that take it off the screen, read as
// ContainerManager::effectiveVisible reads them: showOnTx while
// transmitting, showOnRx on receive.
QString shownNowKey(bool transmitting)
{
    return transmitting ? QStringLiteral("showOnTx") : QStringLiteral("showOnRx");
}
bool containerShown(const ContainerDocument& c, bool transmitting)
{
    return c.visible && c.config.value(QStringLiteral("enabled")).toBool(true)
        && !c.config.value(QStringLiteral("hiddenByMacro")).toBool(false)
        && c.config.value(shownNowKey(transmitting)).toBool(true);
}

// Clears those flags; true when any was set. Only a flag that hides the
// container is written, so a shown container keeps its exact bytes. Only
// the flag for the current state is cleared (as Containers > Hidden does).
bool showContainer(ContainerDocument& c, bool transmitting)
{
    bool changed = false;
    if (!c.visible) { c.visible = true; changed = true; }
    if (!c.config.value(QStringLiteral("enabled")).toBool(true)) {
        c.config[QStringLiteral("enabled")] = true; changed = true;
    }
    if (c.config.value(QStringLiteral("hiddenByMacro")).toBool(false)) {
        c.config[QStringLiteral("hiddenByMacro")] = false; changed = true;
    }
    if (const QString key = shownNowKey(transmitting); !c.config.value(key).toBool(true)) {
        c.config[key] = true; changed = true;
    }
    return changed;
}

ContainerDocument* containerHolding(WorkspaceDocument& document, const QString& type)
{
    for (auto& c : document.containers) {
        for (const auto& content : c.contents) {
            if (content.typeId == type) { return &c; }
        }
    }
    return nullptr;
}

} // namespace

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
                // Bench fix: on only while its container is on the screen
                // too, so a tick never claims an applet nobody can see.
                const bool shown = content.visible && containerShown(c, m_transmitting);
                if (entry.visible != shown) {
                    const bool wasEffective = entry.visible && entry.available;
                    entry.visible = shown;
                    emit visibilityChanged(id, entry.visible);
                    if (wasEffective != (entry.visible && entry.available)) { emit effectiveVisibilityChanged(id, entry.visible && entry.available); }
                }
                break;
            }
            if (found) { break; }
        }
    }
}
void AppletVisibilityController::setTransmitting(bool transmitting)
{
    if (m_transmitting == transmitting) { return; }
    m_transmitting = transmitting;
    // A container shown only on receive (or only on transmit) changes what
    // the ticks read.
    syncWorkspace();
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

    if (m_store) {
        e.visible = defaultVisible;
        // Bench fix: a workspace saved before this applet existed has no
        // entry for it, so it could never show and its menu toggle did
        // nothing. Add it once with its default visibility; the saved entry
        // is authoritative from then on (hidden stays hidden).
        WorkspaceDocument document = m_store->snapshot();
        if (m_store->loadError().isEmpty()
            && placeInWorkspace(document, ContainerContentRegistry::appletTypeForVisibilityId(id),
                                defaultVisible)) {
            commitVisibility(id, document);
        }
        syncWorkspace();
        return;
    }
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

QString AppletVisibilityController::unavailableReason(const QString& id) const
{
    auto it = m_entries.find(id);
    return it != m_entries.end() ? it->unavailableReason : QString{};
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

    if (m_store) {
        WorkspaceDocument document = m_store->snapshot();
        bool found = false;
        bool changed = false;
        const QString type = ContainerContentRegistry::appletTypeForVisibilityId(id);
        for (auto& c : document.containers) {
            for (auto& content : c.contents) {
                if (content.typeId == type) {
                    changed = content.visible != visible;
                    content.visible = visible; found = true; break;
                }
            }
            if (found) { break; }
        }
        // Bench fix: an applet with no workspace entry is added, not ignored.
        if (!found) {
            if (!placeInWorkspace(document, type, visible)) { return; }
            changed = true;
        }
        // Bench fix 2026-10-16: ticked on, the applet is shown, so is the
        // container holding it. Unticking leaves the container alone.
        if (visible) {
            if (ContainerDocument* holder = containerHolding(document, type); holder && showContainer(*holder, m_transmitting)) {
                changed = true;
            }
        }
        bool saved = true;
        if (changed) {
            commitVisibility(id, document);
            saved = m_storageError.isEmpty();
        }
        if (visible && saved) {
            emit bringOnScreenRequested(type);
        }
        return;
    }
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

bool AppletVisibilityController::placeInWorkspace(WorkspaceDocument& document,
                                                  const QString& type, bool visible) const
{
    if (type.isEmpty() || document.containers.isEmpty()) { return false; }
    for (const auto& c : document.containers) {
        for (const auto& content : c.contents) {
            if (content.typeId == type) { return false; }
        }
    }
    ContentEntry entry;
    if (m_registry) {
        entry = m_registry->makeEntry(type);
    } else {
        entry.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        entry.typeId = type;
        entry.name = type;
    }
    entry.visible = visible;
    // The SSB Mod Monitor sits directly under the AM Mod Monitor, wherever
    // that panel is, unless it is popped out.
    if (type == QLatin1String("applet:ssb_mod_monitor")) {
        for (auto& c : document.containers) {
            if (c.popOutShell) { continue; }
            for (int i = 0; i < c.contents.size(); ++i) {
                if (c.contents[i].typeId == QLatin1String("applet:mod_monitor")) {
                    c.contents.insert(i + 1, entry);
                    return true;
                }
            }
        }
    }
    // The panel the station accessory applets (Power Genius, Tuner Genius,
    // RF-Kit) live in; a popped-out applet's own shell does not count.
    static const QStringList kAccessoryTypes = {
        QStringLiteral("applet:RfKit"), QStringLiteral("applet:tuner"), QStringLiteral("applet:amp")};
    ContainerDocument* target = nullptr;
    for (const QString& accessory : kAccessoryTypes) {
        for (auto& c : document.containers) {
            if (c.popOutShell) { continue; }
            for (const auto& content : c.contents) {
                if (content.typeId == accessory) { target = &c; break; }
            }
            if (target) { break; }
        }
        if (target) { break; }
    }
    if (!target) {
        for (auto& c : document.containers) {
            if (c.id == document.mainContainerId) { target = &c; break; }
        }
    }
    if (!target) { target = &document.containers.first(); }

    target->contents.append(entry);
    return true;
}

void AppletVisibilityController::commitVisibility(const QString& id, const WorkspaceDocument& document)
{
    const CommitResult result = m_store->commit(document, document.revision);
    if (result.status != CommitStatus::Saved) {
        m_storageError = result.error; emit persistenceFailed(result.error);
        // Restore toggled menu UI too: the committed preference still wins.
        emit visibilityChanged(id, isVisible(id));
    } else { m_storageError.clear(); }
}

void AppletVisibilityController::setAvailable(const QString& id, bool available, const QString& reason)
{
    auto it = m_entries.find(id);
    if (it == m_entries.end()) { return; }
    const QString unavailable = available ? QString() : reason;
    if (it->available == available && it->unavailableReason == unavailable) { return; }
    const bool flipped = it->available != available;

    const bool wasEffective = it->visible && it->available;
    it->available = available;
    it->unavailableReason = unavailable;
    if (m_registry) { m_registry->setAvailable(ContainerContentRegistry::appletTypeForVisibilityId(id), available, unavailable); }
    if (!flipped) { return; }
    const bool nowEffective = it->visible && it->available;

    emit availabilityChanged(id, available);
    if (wasEffective != nowEffective) {
        emit effectiveVisibilityChanged(id, nowEffective);
    }
}

} // namespace NereusSDR
