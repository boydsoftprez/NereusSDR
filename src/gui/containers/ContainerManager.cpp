// =================================================================
// src/gui/containers/ContainerManager.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-04 — Follow responsive content height in auto-height containers by
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-04 — Preserve pending container placement and initialize overlay
//                 anchor baselines by J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-04 — Suppress shutdown presentation reconciliation while retaining
//                 persistence by J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-02 — Effective contextual draft properties and portable settings by
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-02 — Draft-only edits and inert cached previews by J.J. Boyd
//                 (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-02 — Atomic container arrangement and reserved chrome by J.J. Boyd
//                 (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-02 — Mixed container ownership, persistence and source routing by
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
// =================================================================

/*  MeterManager.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2026 Richard Samphire MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk
*/
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

#include "ContainerManager.h"
#include "gui/meters/MeterPoller.h"
#include "ContainerArrangeController.h"
#include <QGuiApplication>
#include <QScreen>
#include <QApplication>
#include <QSplitterHandle>
#include "ContainerWidget.h"
#include "ContainerSettingsDialog.h"
#include "FloatingContainer.h"
#include "core/AppSettings.h"
#include "core/LogCategories.h"
#include "gui/applets/AppletPanelWidget.h"
#include "gui/meters/MeterItem.h"
#include "gui/meters/MeterWidget.h"

#include <QSplitter>
#include <QScopedValueRollback>
#include <QJsonArray>
#include <QEvent>
#include <QUuid>
#include "ContainerContentHost.h"
#include "ContainerContentRegistry.h"
#include "ContainerWorkspaceStore.h"
#include "gui/GuiApplication.h"
#include "ContainerDocumentCodec.h"
#include <algorithm>

namespace NereusSDR {

namespace {
// Locate the MeterWidget hosted inside a container's content widget.
// Container shape varies: user-created containers use a bare MeterWidget
// as content; the panel container wraps a MeterWidget inside an
// AppletPanelWidget. findChild<MeterWidget*>() handles both cases — and
// returns the same pointer for the bare case since the search is
// inclusive of the receiver. Returns nullptr for placeholder content.
MeterWidget* innerMeterWidget(QWidget* content)
{
    if (!content) {
        return nullptr;
    }
    if (auto* m = qobject_cast<MeterWidget*>(content)) {
        return m;
    }
    return content->findChild<MeterWidget*>();
}
} // namespace

ContainerManager::ContainerManager(QWidget* dockParent, QSplitter* splitter,
                                   QObject* parent)
    : QObject(parent)
    , m_dockParent(dockParent)
    , m_splitter(splitter)
{
    m_geometryCommit.setSingleShot(true);
    m_geometryCommit.setInterval(200);
    connect(&m_geometryCommit, &QTimer::timeout, this, &ContainerManager::saveState);
    connect(qApp, &QGuiApplication::screenRemoved, this, [this](QScreen *) {
        if (!m_store) {
            return;
        }
        // Screen changes are recovered through one document transaction. A
        // failed save therefore leaves live geometry and retained bytes intact.
        auto draft = m_store->snapshot();
        for (auto &d : draft.containers) {
            if (d.dockMode != DockMode::Floating) {
                continue;
            }
            QScreen *screen = QGuiApplication::screenAt(d.geometry.center());
            if (!screen) {
                screen = QGuiApplication::primaryScreen();
            }
            if (!screen) {
                continue;
            }
            auto *c = container(d.id);
            const QSize minimum =
                c ? c->minimumSizeHint().expandedTo(QSize(260, 24)) : QSize(260, 24);
            d.geometry = FloatingContainer::clampedGeometry(d.geometry, screen->availableGeometry(),
                                                            minimum);
        }
        if (draft != m_store->snapshot()) {
            commitWorkspace(draft, draft.revision);
        }
    });
    qCDebug(lcContainer) << "ContainerManager created";
}

void ContainerManager::setPreviewPoller(MeterPoller* poller) { m_previewPoller=poller; }
MeterPoller* ContainerManager::previewPoller() const { return m_previewPoller; }

ContainerManager::~ContainerManager()
{
    m_geometryCommit.stop();
    if (m_store) { disconnect(m_store, nullptr, this, nullptr); }
    if (m_registry) { disconnect(m_registry, nullptr, this, nullptr); }
    for (auto* c : m_containers) { if (auto* host = contentHost(c->id())) { host->releaseViews(); } }
    const auto forms=m_floatingForms.values();
    for (auto* form : forms) { delete form; }
    qCDebug(lcContainer) << "ContainerManager destroyed —" << m_containers.size() << "containers";
}

// Nereus-origin structured route: commit first, project only committed snapshots.
ContainerWorkspaceStore* ContainerManager::workspaceStore() const { return m_store; }
ContainerContentRegistry* ContainerManager::contentRegistry() const { return m_registry; }
void ContainerManager::setWorkspaceAdapter(ContainerWorkspaceStore* store, ContainerContentRegistry* registry)
{
    m_geometryCommit.stop();
    m_pendingGeometry.clear();
    m_documentGeometry.clear();
    if (m_store) { disconnect(m_store, nullptr, this, nullptr); }
    if (m_registry) { disconnect(m_registry, nullptr, this, nullptr); }
    if (m_arrange) {
        delete m_arrange;
        m_arrange = nullptr;
    }
    m_store = store; m_registry = registry;
    if (!store || !registry) { return; }
    m_arrange = new ContainerArrangeController(*store, this, this);
    m_storageError = store->loadError();
    connect(store, &ContainerWorkspaceStore::committed, this, [this] { reconcileWorkspace(m_store->snapshot()); });
    connect(registry, &ContainerContentRegistry::runtimeChanged, this, [this] {
        if (!m_reconciling && m_store) { reconcileWorkspace(m_store->snapshot()); }
    });
    if (m_splitter) {
        m_splitter->installEventFilter(this);
        connect(m_splitter, &QSplitter::splitterMoved, this, [this] {
            if (!m_reconciling) { m_geometryCommit.start(); }
        });
    }
}
ContainerContentHost* ContainerManager::contentHost(const QString& id) const
{
    auto* c = container(id);
    return c ? qobject_cast<ContainerContentHost*>(c->content()) : nullptr;
}
CommitResult ContainerManager::commitWorkspace(const WorkspaceDocument& document, quint64 expectedRevision)
{
    if (!m_store || !m_registry) { return {CommitStatus::Invalid, 0, QStringLiteral("No workspace adapter")}; }
    // Existing duplicate singleton records remain recoverable; transactions may
    // repair them, but cannot create another supported singleton placement.
    QHash<QString, int> oldCounts, counts;
    for (const auto& c : m_store->snapshot().containers) { for (const auto& e : c.contents) { ++oldCounts[e.typeId]; } }
    for (const auto& c : document.containers) { for (const auto& e : c.contents) { ++counts[e.typeId]; } }
    for (const auto& d : m_registry->descriptors()) {
        if (d.singleton && counts.value(d.typeId) > 1 && counts.value(d.typeId) > oldCounts.value(d.typeId)) {
            return {CommitStatus::Invalid, expectedRevision, QStringLiteral("%1 already has a placement").arg(d.title)};
        }
    }
    const CommitResult result = m_store->commit(document, expectedRevision);
    if (result.status != CommitStatus::Saved) { m_storageError = result.error; emit workspaceError(result.error); }
    else { m_storageError.clear(); }
    return result;
}
bool ContainerManager::effectiveVisible(const ContainerDocument &document) const
{
    return document.visible && document.config.value("enabled").toBool(true) &&
           !document.config.value("hiddenByMacro").toBool(false) &&
           document.config.value(m_transmitting ? "showOnTx" : "showOnRx").toBool(true);
}
void ContainerManager::setTransmitting(bool transmitting)
{
    m_transmitting = transmitting;
    if (!m_store) {
        return;
    }
    for (const auto &document : m_store->snapshot().containers) {
        if (auto *c = container(document.id)) {
            (c->isFloating() ? c->window() : c)->setVisible(effectiveVisible(document));
        }
    }
}
bool ContainerManager::eventFilter(QObject* watched, QEvent* event)
{
    if (qobject_cast<QSplitterHandle *>(watched) &&
        (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseMove ||
         event->type() == QEvent::MouseButtonRelease)) {
        for (auto *c : m_containers) {
            if (c->isPanelDocked() && c->isLocked()) {
                return true;
            }
        }
    }
    if (auto *c = qobject_cast<ContainerWidget *>(watched); c && c->geometryInteractionActive()) {
        return false;
    }
    if (auto *form = qobject_cast<FloatingContainer *>(watched)) {
        if (auto *c = container(form->id()); c && c->geometryInteractionActive()) {
            return false;
        }
    }
    if (m_store && !m_reconciling && m_storageError.isEmpty()
        && (event->type() == QEvent::Move || event->type() == QEvent::Resize)) {
        if (qobject_cast<ContainerWidget*>(watched) || qobject_cast<FloatingContainer*>(watched)) {
            ContainerWidget* changed = qobject_cast<ContainerWidget*>(watched);
            if (!changed) {
                changed = container(qobject_cast<FloatingContainer*>(watched)->id());
            }
            if (changed) {
                m_pendingGeometry.insert(changed->id());
                if (changed->isOverlayDocked() && m_dockParent) {
                    changed->storeLocation();
                    changed->setDelta(QPoint(m_dockParent->width(), m_dockParent->height()));
                }
            }
            m_geometryCommit.start();
        }
    }
    return QObject::eventFilter(watched, event);
}
void ContainerManager::reconcileWorkspace(const WorkspaceDocument& document)
{
    // Final geometry/config commits remain durable without showing closed
    // forms again. This also covers aboutToQuit saves after Qt 6.4's Quit scope.
    if (GuiApplication::applicationQuitInProgress()) { return; }
    if (!m_store || !m_registry || m_reconciling) { return; }
    // Refuse draft/preview state. Dependent controllers commit through the store.
    if (document != m_store->snapshot()) { return; }
    QScopedValueRollback<bool> guard(m_reconciling, true);
    m_geometryCommit.stop();
    QHash<QString, QString> placements;
    QSet<QString> ids;
    for (const auto& d : document.containers) {
        ids.insert(d.id);
        for (const auto& e : d.contents) {
            if (e.typeId.startsWith("applet:") && !placements.contains(e.typeId)) { placements[e.typeId] = e.id; }
        }
    }
    m_registry->setSingletonPlacements(placements);
    QSet<QString> changed;
    for (const auto& d : document.containers) {
        if (!contentHost(d.id) || contentHost(d.id)->needsReconcile(d) || (container(d.id) && container(d.id)->dockMode()!=d.dockMode)) { changed.insert(d.id); }
    }
    // Release all singleton trees before removing a host or moving between hosts.
    for (auto* c : m_containers) {
        if (auto* host = contentHost(c->id())) {
            if (changed.contains(c->id()) || !ids.contains(c->id())) { host->releaseViews(); }
        }
    }
    for (const QString& id : m_containers.keys()) {
        if (!ids.contains(id)) { destroyContainer(id); }
    }
    m_pendingGeometry.intersect(ids);
    for (const QString& id : m_documentGeometry.keys()) {
        if (!ids.contains(id)) { m_documentGeometry.remove(id); }
    }
    m_panelContainerId = document.mainContainerId;
    for (const auto& d : document.containers) {
        auto* c = container(d.id);
        const bool fresh = !c;
        // Runtime content refresh and unrelated document edits must not replay
        // stale placement over an uncommitted move. Explicit placement/mode
        // edits remain authoritative, and no storage commit occurs here.
        const bool preserveGeometry = c && m_storageError.isEmpty() && c->dockMode() == d.dockMode
            && (m_pendingGeometry.contains(d.id) || c->geometryInteractionActive())
            && m_documentGeometry.contains(d.id) && m_documentGeometry.value(d.id) == d.geometry;
        if (preserveGeometry) {
            m_pendingGeometry.insert(d.id);
        } else {
            m_pendingGeometry.remove(d.id);
        }
        if (!c) {
            c = new ContainerWidget(); c->setId(d.id);
            c->setRxSource(d.config.value("rxSource").toInt(1));
            auto* form = new FloatingContainer(c->rxSource());
            form->setProperty("structuredWorkspace", true); form->setId(d.id);
            form->installEventFilter(this); c->installEventFilter(this);
            m_containers[d.id] = c; m_floatingForms[d.id] = form;
            wireContainer(c);
            connect(form, &FloatingContainer::aboutToClose, this, [this, id = d.id] {
                const auto result = m_arrange->closeContainer(id);
                if (!result.ok) {
                    emit workspaceError(result.error);
                }
            });
            connect(c, &ContainerWidget::returnContainerRequested, this, [this, id = d.id] {
                const auto result = m_arrange->closeContainer(id);
                if (!result.ok) {
                    emit workspaceError(result.error);
                }
            });
            connect(c, &ContainerWidget::hideContainerRequested, this,
                    [this, id = d.id] { setContainerVisible(id, false); });
            connect(c, &ContainerWidget::headerModeRequested, this,
                    [this, id = d.id](HeaderMode mode) {
                        auto draft = m_store->snapshot();
                        for (auto &value : draft.containers) {
                            if (value.id == id) {
                                value.header = mode;
                            }
                        }
                        commitWorkspace(draft, draft.revision);
                    });
        }
        auto* host = contentHost(d.id);
        const bool reparent = fresh || c->dockMode() != d.dockMode;
        if (host && reparent) { host->releaseViews(); }
        auto* form = m_floatingForms.value(d.id);
        if (reparent) {
            c->hide(); form->hide(); form->setContainerFloating(d.dockMode == DockMode::Floating);
            if (d.dockMode == DockMode::Floating) { form->takeOwner(c); }
            else if (d.dockMode == DockMode::PanelDocked) { c->setParent(m_splitter); m_splitter->addWidget(c); }
            else { c->setParent(m_dockParent); }
        }
        c->setDockMode(d.dockMode); c->setNotes(d.name); c->setRxSource(d.config.value("rxSource").toInt(1));
        c->setAxisLock(d.anchor); c->setLocked(d.locked); c->setAutoHeight(d.autoHeight);
        c->setPopOutShell(d.popOutShell);
        c->setHeaderMode(d.header);
        c->setContainerEnabled(d.config.value("enabled").toBool(true));
        c->setShowOnRx(d.config.value("showOnRx").toBool(true)); c->setShowOnTx(d.config.value("showOnTx").toBool(true));
        c->setContainerMinimises(d.config.value("containerMinimises").toBool(false));
        c->setContainerHidesWhenRxNotUsed(d.config.value("hidesWhenRxNotUsed").toBool(false));
        c->setBackgroundColor(QColor(d.config.value("backgroundColor").toString("#0f0f1a")));
        c->setBorder(d.config.value("border").toBool(true)); c->setNoControls(d.config.value("noControls").toBool(false));
        c->setPinOnTop(d.config.value("pinOnTop").toBool(false));
        if (fresh && d.dockMode==DockMode::Floating && !d.geometry.isValid()) {
            const QByteArray geometry=QByteArray::fromHex(d.config.value("legacyAppletFloatGeometry").toString().toLatin1());
            if (!geometry.isEmpty()) { form->QWidget::restoreGeometry(geometry); }
        }
        if (!preserveGeometry) {
            if (d.geometry.isValid()) { (d.dockMode == DockMode::Floating ? static_cast<QWidget*>(form) : c)->setGeometry(d.geometry); }
        }
        if (!host) {
            host = new ContainerContentHost(*m_registry, c);
            connect(host, &ContainerContentHost::meterSurfaceReady, this, [this, c](MeterWidget* meter, const QJsonObject& context) {
                // Structured items route through Main's per-entry wiring; never
                // also install the legacy container-source forwarding signals.
                Q_UNUSED(c);
                emit meterContextReady(meter, context);
                emit meterReadyForPolling(meter);
            });
            connect(host, &ContainerContentHost::reconciled, this, &ContainerManager::workspaceReconciled);
            connect(host, &ContainerContentHost::preferredContentHeightChanged, this,
                    [this, id = d.id, content = QPointer<ContainerContentHost>(host),
                     view = QPointer<ContainerWidget>(c), shell = QPointer<FloatingContainer>(form)] {
                if (!m_store || m_reconciling || GuiApplication::applicationQuitInProgress()
                    || !content || !view || container(id) != view.data()
                    || contentHost(id) != content.data()) {
                    return;
                }
                const WorkspaceDocument latest = m_store->snapshot();
                const auto placement = std::find_if(latest.containers.cbegin(), latest.containers.cend(),
                    [&id](const ContainerDocument& value) { return value.id == id; });
                if (placement == latest.containers.cend() || !placement->autoHeight
                    || placement->dockMode == DockMode::PanelDocked) {
                    return;
                }
                QWidget* target = placement->dockMode == DockMode::Floating
                    ? static_cast<QWidget*>(shell.data()) : view.data();
                if (!target) {
                    return;
                }
                int height = content->preferredContentHeight() + ContainerWidget::kTitleBarHeight;
                if (placement->layout == ContentLayout::FreeCanvas && target->screen()) {
                    height = qMin(height, target->screen()->availableGeometry().height());
                }
                const QSize wanted = QSize(qMax(target->width(), view->minimumSizeHint().width()), height)
                    .expandedTo(target->minimumSize()).boundedTo(target->maximumSize());
                if (target->size() != wanted) {
                    // Width changes settle through the host's viewport layout;
                    // use live placement rather than replaying saved geometry.
                    target->resize(wanted);
                }
            }, Qt::QueuedConnection);
            c->setContent(host);
        }
        host->setArrangeController(m_arrange);
        host->reconcile(d);
        if (d.autoHeight && d.dockMode != DockMode::PanelDocked) {
            QWidget *target = d.dockMode == DockMode::Floating ? static_cast<QWidget *>(form) : c;
            int height=host->preferredContentHeight()+ContainerWidget::kTitleBarHeight;
            if(d.layout==ContentLayout::FreeCanvas && target->screen()) {height=qMin(height,target->screen()->availableGeometry().height());}
            target->resize(qMax(target->width(), c->minimumSizeHint().width()),height);
        }
        if (d.dockMode == DockMode::Floating) {
            form->ensureVisiblePosition(m_dockParent);
        } else if (d.dockMode == DockMode::OverlayDocked && m_dockParent) {
            c->setGeometry(FloatingContainer::clampedGeometry(
                c->geometry(), m_dockParent->rect(),
                c->minimumSizeHint().expandedTo(QSize(260, 24))));
            c->storeLocation();
            c->setDelta(QPoint(m_dockParent->width(), m_dockParent->height()));
        }
        if (d.dockMode == DockMode::Floating) {
            c->show();
            form->setVisible(effectiveVisible(d));
        } else {
            c->setVisible(effectiveVisible(d));
        }
        if (fresh) { emit containerAdded(d.id); }
        m_documentGeometry[d.id] = d.geometry;
    }
    if (m_splitter) {
        for (auto *handle : m_splitter->findChildren<QSplitterHandle *>()) {
            handle->installEventFilter(this);
        }
    }
    const bool interactionActive = std::any_of(m_containers.cbegin(), m_containers.cend(),
        [](ContainerWidget* c) { return c->geometryInteractionActive(); });
    if (!m_pendingGeometry.isEmpty() && !interactionActive && m_storageError.isEmpty()) {
        m_geometryCommit.start();
    }
    emit workspaceReconciled();
}
bool ContainerManager::commitDockMode(const QString& id, DockMode mode)
{
    if (!m_store || m_reconciling) { return false; }
    WorkspaceDocument document = m_store->snapshot();
    for (auto &d : document.containers) {
        if (d.id == id) {
            if (d.locked) {
                emit workspaceError(tr("Arrangement is locked"));
                return true;
            }
            d.dockMode = mode;
            commitWorkspace(document, document.revision);
            return true;
        }
    }
    return true;
}

void ContainerManager::wireContainer(ContainerWidget* container)
{
    connect(container, &ContainerWidget::geometryInteractionStarted, this,
            [this] { m_geometryCommit.stop(); });
    connect(container, &ContainerWidget::geometryInteractionFinished, this, [this, container] {
        if (m_store && !m_reconciling) {
            m_pendingGeometry.insert(container->id());
            if (container->isOverlayDocked() && m_dockParent) {
                container->storeLocation();
                container->setDelta(QPoint(m_dockParent->width(), m_dockParent->height()));
            }
            m_geometryCommit.start();
        }
    });
    connect(container, &QObject::destroyed, this, [this, id=container->id()] { m_containers.remove(id); });
    connect(container, &ContainerWidget::floatRequested, this, [this, container]() {
        floatContainer(container->id());
    });
    connect(container, &ContainerWidget::dockRequested, this, [this, container]() {
        dockContainer(container->id());
    });
    connect(container, &ContainerWidget::settingsRequested, this,
            [this, container]() {
        ContainerSettingsDialog dialog(container, container->window(), this);
        dialog.exec();
    });
    connect(container, &ContainerWidget::notesChanged, this,
            [this, container](const QString& notes) {
        emit containerTitleChanged(container->id(), notes);
    });
    // Announce any MeterWidget that becomes a (descendant of) the
    // container's content. Listens for both the create path
    // (createContainer + caller setContent) and the restore path
    // (ContainerManager itself calls setContent after wireContainer).
    // During restoreState() m_suppressMeterAnnouncements is set so
    // the placeholder → setMeterFloating → fresh-meter cascade
    // doesn't emit twice; restoreState emits one manual announcement
    // after the final content is in place.
    connect(container, &ContainerWidget::contentChanged, this,
            [this](QWidget* content) {
        if (m_suppressMeterAnnouncements) { return; }
        if (auto* meter = innerMeterWidget(content)) {
            emit meterReadyForPolling(meter);
        }
    });
}

QString ContainerManager::extractMeterItems(ContainerWidget* container)
{
    if (!container) { return {}; }
    QWidget* content = container->content();
    MeterWidget* meter = innerMeterWidget(content);
    if (!meter) { return {}; }

    const QString payload = meter->serializeItems();

    if (qobject_cast<MeterWidget*>(content) == meter) {
        // Bare MeterWidget as content — clear via setContent(nullptr) so
        // the content holder layout is empty during the upcoming reparent.
        container->setContent(nullptr);
    } else if (auto* panel = qobject_cast<AppletPanelWidget*>(content)) {
        // AppletPanelWidget header — detach the MeterWidget so the panel
        // can reparent with no native-window child.
        panel->clearHeaderWidget();
    }
    qCDebug(lcContainer) << "Extracted meter items from" << container->id()
                          << "bytes:" << payload.size();
    return payload;
}

void ContainerManager::installFreshMeter(ContainerWidget* container, const QString& payload)
{
    if (!container || payload.isEmpty()) { return; }

    auto* fresh = new MeterWidget();
    fresh->deserializeItems(payload);
    fresh->inferStackFromGeometry();
    for (MeterItem* item : fresh->items()) {
        container->wireInteractiveItem(item);
    }

    QWidget* content = container->content();
    if (auto* panel = qobject_cast<AppletPanelWidget*>(content)) {
        panel->setHeaderWidget(fresh, QStringLiteral("Meters"), 1.3f);
        // Same gate as the wireContainer contentChanged listener below
        // — restoreState emits one manual announcement per container
        // once the final content is in place.
        if (!m_suppressMeterAnnouncements) {
            emit meterReadyForPolling(fresh);
        }
    } else {
        // Bare content path (user-created containers) — setContent emits
        // contentChanged which routes into meterReadyForPolling.
        container->setContent(fresh);
    }
    qCDebug(lcContainer) << "Installed fresh meter for" << container->id()
                          << "items:" << fresh->items().size();
}

ContainerWidget* ContainerManager::duplicateContainer(const QString& sourceId)
{
    if (m_store && !m_reconciling) {
        WorkspaceDocument document=m_store->snapshot();
        for (const auto& source : document.containers) {
            if (source.id!=sourceId) { continue; }
            for (const auto& entry : source.contents) { if(entry.typeId.startsWith("applet:")) {
                m_storageError=QStringLiteral("A singleton applet already has a placement; move it instead of duplicating it"); emit workspaceError(m_storageError); return nullptr;
            } }
            auto copy=source; copy.id=QUuid::createUuid().toString(QUuid::WithoutBraces); copy.dockMode=DockMode::Floating;
            for (auto& entry : copy.contents) { entry.id=QUuid::createUuid().toString(QUuid::WithoutBraces); entry.returnLocation.reset(); }
            document.containers.append(copy);
            if(commitWorkspace(document,document.revision).status!=CommitStatus::Saved) { return nullptr; }
            return container(copy.id);
        }
        return nullptr;
    }
    ContainerWidget* src = container(sourceId);
    if (!src) {
        qCWarning(lcContainer) << "duplicateContainer: unknown id:" << sourceId;
        return nullptr;
    }

    ContainerWidget* dup = createContainer(src->rxSource(), DockMode::Floating);
    if (!dup) { return nullptr; }

    // Copy user-editable state. The auto-generated ID on the new
    // container is deliberately preserved; everything else mirrors
    // the source.
    dup->setNotes(src->notes());
    dup->setBorder(src->hasBorder());
    dup->setLocked(src->isLocked());
    dup->setContainerEnabled(src->isContainerEnabled());
    dup->setShowOnRx(src->showOnRx());
    dup->setShowOnTx(src->showOnTx());
    dup->setContainerMinimises(src->containerMinimises());
    dup->setContainerHidesWhenRxNotUsed(src->containerHidesWhenRxNotUsed());
    dup->setAutoHeight(src->autoHeight());
    dup->setTitleBarVisible(src->isTitleBarVisible());
    dup->setPinOnTop(src->isPinOnTop());
    dup->setAxisLock(src->axisLock());
    dup->setDockedSize(src->dockedSize());

    qCDebug(lcContainer) << "Duplicated container:" << sourceId << "->" << dup->id();
    return dup;
}

ContainerWidget* ContainerManager::createContainer(int rxSource, DockMode mode)
{
    if (m_store && !m_reconciling) {
        WorkspaceDocument document = m_store->snapshot();
        ContainerDocument d; d.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        d.name = QStringLiteral("Container"); d.layout = ContentLayout::VerticalStack; d.dockMode = mode; d.config["rxSource"] = rxSource;
        document.containers.append(d);
        if (document.mainContainerId.isEmpty()) { document.mainContainerId = d.id; }
        if (commitWorkspace(document, document.revision).status != CommitStatus::Saved) { return nullptr; }
        return container(d.id);
    }
    // From Thetis MeterManager.cs:5613-5673
    auto* container = new ContainerWidget(nullptr);
    container->setRxSource(rxSource);
    container->setDockMode(mode);

    auto* floatingForm = new FloatingContainer(rxSource);
    floatingForm->setId(container->id());

    wireContainer(container);

    m_containers.insert(container->id(), container);
    m_floatingForms.insert(container->id(), floatingForm);

    // Place container according to dock mode
    switch (mode) {
    case DockMode::PanelDocked:
        container->setParent(m_splitter);
        m_splitter->addWidget(container);
        m_panelContainerId = container->id();
        container->show();
        break;
    case DockMode::OverlayDocked:
        container->setParent(m_dockParent);
        container->show();
        container->raise();
        break;
    case DockMode::Floating:
        setMeterFloating(container, floatingForm);
        break;
    }

    qCDebug(lcContainer) << "Created container:" << container->id()
                          << "rx:" << rxSource << "mode:" << static_cast<int>(mode);
    emit containerAdded(container->id());
    return container;
}

void ContainerManager::destroyContainer(const QString& id)
{
    if (m_store && !m_reconciling) {
        const auto result = m_arrange->removeContainer(id);
        if (!result.ok) {
            emit workspaceError(result.error);
        }
        return;
    }
    // From Thetis MeterManager.cs:6533-6579
    // Upstream inline attribution preserved verbatim (MeterManager.cs:6563):
    //   f.Dispose();//[2.10.3.7]MW0LGE // we have to dispose it because close() prevent this being freed up
    if (!m_containers.contains(id)) {
        qCWarning(lcContainer) << "destroyContainer: unknown id:" << id;
        return;
    }

    if (m_floatingForms.contains(id)) {
        FloatingContainer* form = m_floatingForms.take(id);
        form->hide();
        form->deleteLater();
    }

    ContainerWidget* container = m_containers.take(id);
    container->hide();
    container->setParent(nullptr);
    container->deleteLater();

    if (m_panelContainerId == id) {
        m_panelContainerId.clear();
    }

    qCDebug(lcContainer) << "Destroyed container:" << id;
    emit containerRemoved(id);
}

void ContainerManager::floatContainer(const QString& id)
{
    if (commitDockMode(id, DockMode::Floating)) { return; }
    if (!m_containers.contains(id) || !m_floatingForms.contains(id)) {
        return;
    }
    setMeterFloating(m_containers[id], m_floatingForms[id]);
}

void ContainerManager::dockContainer(const QString& id)
{
    if (!m_containers.contains(id) || !m_floatingForms.contains(id)) {
        return;
    }
    // Return to previous dock mode
    if (id == m_panelContainerId) {
        panelDockContainer(id);
    } else {
        overlayDockContainer(id);
    }
}

void ContainerManager::panelDockContainer(const QString& id)
{
    if (commitDockMode(id, DockMode::PanelDocked)) { return; }
    if (!m_containers.contains(id) || !m_floatingForms.contains(id)) {
        return;
    }
    ContainerWidget* container = m_containers[id];
    FloatingContainer* form = m_floatingForms[id];

    form->setContainerFloating(false);
    form->hide();
    container->hide();
    const QString payload = extractMeterItems(container);
    container->setParent(m_splitter);
    m_splitter->addWidget(container);
    container->setDockMode(DockMode::PanelDocked);
    container->show();
    installFreshMeter(container, payload);
    m_panelContainerId = id;

    qCDebug(lcContainer) << "Panel-docked container:" << id;
}

void ContainerManager::overlayDockContainer(const QString& id)
{
    if (commitDockMode(id, DockMode::OverlayDocked)) { return; }
    if (!m_containers.contains(id) || !m_floatingForms.contains(id)) {
        return;
    }
    ContainerWidget* container = m_containers[id];
    FloatingContainer* form = m_floatingForms[id];

    // From Thetis MeterManager.cs:5867-5893
    form->setContainerFloating(false);
    form->hide();
    container->hide();
    const QString payload = extractMeterItems(container);
    container->setParent(m_dockParent);
    container->setDockMode(DockMode::OverlayDocked);
    container->restoreLocation();
    container->show();
    container->raise();
    installFreshMeter(container, payload);

    qCDebug(lcContainer) << "Overlay-docked container:" << id;
}

void ContainerManager::setMeterFloating(ContainerWidget* container, FloatingContainer* form)
{
    // From Thetis MeterManager.cs:5894-5918
    container->hide();
    const QString payload = extractMeterItems(container);
    form->takeOwner(container);
    form->setContainerFloating(true);
    container->setDockMode(DockMode::Floating);
    container->setTopMost();  // Re-apply pin-on-top now that parent is set
    form->ensureVisiblePosition(m_dockParent);
    form->show();
    installFreshMeter(container, payload);
    qCDebug(lcContainer) << "Floated container:" << container->id();
}

void ContainerManager::returnMeterFromFloating(ContainerWidget* container, FloatingContainer* form)
{
    // From Thetis MeterManager.cs:5867-5893
    form->setContainerFloating(false);
    form->hide();
    container->hide();
    const QString payload = extractMeterItems(container);
    container->setParent(m_dockParent);
    container->setDockMode(DockMode::OverlayDocked);
    container->restoreLocation();
    container->show();
    container->raise();
    installFreshMeter(container, payload);
    qCDebug(lcContainer) << "Docked container:" << container->id();
}

void ContainerManager::recoverContainer(const QString& id)
{
    if (m_store && !m_reconciling) {
        WorkspaceDocument document=m_store->snapshot();
        for (auto& d : document.containers) { if(d.id==id) {
            d.visible=true; d.config["enabled"]=true;
            if(d.dockMode==DockMode::Floating) { d.dockMode=DockMode::OverlayDocked; }
            if(m_dockParent) { const QSize size=d.geometry.isValid()?d.geometry.size():QSize(360,300); d.geometry=QRect(QPoint(qMax(0,(m_dockParent->width()-size.width())/2),qMax(0,(m_dockParent->height()-size.height())/2)),size); }
            commitWorkspace(document,document.revision); return;
        } }
        return;
    }
    // From Thetis MeterManager.cs:6514-6531
    if (!m_containers.contains(id)) {
        return;
    }
    ContainerWidget* container = m_containers[id];

    if (container->isFloating()) {
        overlayDockContainer(id);
    }
    container->setContainerEnabled(true);
    container->show();

    if (m_dockParent) {
        int cx = (m_dockParent->width() / 2) - (container->width() / 2);
        int cy = (m_dockParent->height() / 2) - (container->height() / 2);
        container->move(cx, cy);
        container->storeLocation();
    }
    qCDebug(lcContainer) << "Recovered container:" << id;
}

void ContainerManager::updateDockedPositions(int hDelta, int vDelta)
{
    // From Thetis MeterManager.cs:5812-5865 — overlay-docked only
    for (auto it = m_containers.constBegin(); it != m_containers.constEnd(); ++it) {
        ContainerWidget* c = it.value();
        if (!c->isOverlayDocked()) {
            continue;
        }

        QPoint dockedLoc = c->dockedLocation();
        QPoint delta = c->delta();
        QPoint newLocation;

        switch (c->axisLock()) {
        case AxisLock::Right:
        case AxisLock::BottomRight:
            newLocation = QPoint(dockedLoc.x() - delta.x() + hDelta,
                                 dockedLoc.y() - delta.y() + vDelta);
            break;
        case AxisLock::BottomLeft:
        case AxisLock::Left:
            newLocation = QPoint(dockedLoc.x(),
                                 dockedLoc.y() - delta.y() + vDelta);
            break;
        case AxisLock::TopLeft:
            newLocation = QPoint(dockedLoc.x(), dockedLoc.y());
            break;
        case AxisLock::Top:
        case AxisLock::TopRight:
            newLocation = QPoint(dockedLoc.x() - delta.x() + hDelta,
                                 dockedLoc.y());
            break;
        case AxisLock::Bottom:
            newLocation = QPoint(dockedLoc.x() - delta.x() + hDelta,
                                 dockedLoc.y() - delta.y() + vDelta);
            break;
        }

        if (m_dockParent) {
            int maxX = m_dockParent->width() - c->width();
            int maxY = m_dockParent->height() - c->height();
            newLocation.setX(std::clamp(newLocation.x(), 0, std::max(0, maxX)));
            newLocation.setY(std::clamp(newLocation.y(), 0, std::max(0, maxY)));
        }

        if (newLocation != c->pos()) {
            c->move(newLocation);
        }
    }
}

void ContainerManager::saveSplitterState()
{
    if (m_store) { saveState(); return; }
    if (!m_splitter) {
        return;
    }
    QList<int> sizes = m_splitter->sizes();
    QStringList parts;
    for (int s : sizes) {
        parts << QString::number(s);
    }
    AppSettings::instance().setValue(QStringLiteral("MainSplitterSizes"),
                                    parts.join(QLatin1Char(',')));
}

void ContainerManager::restoreSplitterState()
{
    if (m_store) {
        if (!m_splitter) { return; }
        const QJsonArray saved = m_store->snapshot().extensions.value("splitterSizes").toArray();
        QList<int> sizes; for (const auto& value : saved) { sizes.append(value.toInt()); }
        if (sizes.size() == m_splitter->count()) { QScopedValueRollback<bool> guard(m_reconciling, true); m_splitter->setSizes(sizes); }
        return;
    }
    if (!m_splitter) {
        return;
    }
    QString val = AppSettings::instance().value(QStringLiteral("MainSplitterSizes")).toString();
    if (val.isEmpty()) {
        return;
    }
    QStringList parts = val.split(QLatin1Char(','));
    QList<int> sizes;
    for (const QString& p : parts) {
        bool ok;
        int s = p.toInt(&ok);
        if (ok) {
            sizes << s;
        }
    }
    if (sizes.size() == m_splitter->count()) {
        m_splitter->setSizes(sizes);
    }
}

QList<ContainerWidget*> ContainerManager::allContainers() const
{
    return m_containers.values();
}

ContainerWidget* ContainerManager::container(const QString& id) const
{
    return m_containers.value(id, nullptr);
}

ContainerWidget* ContainerManager::panelContainer() const
{
    return m_containers.value(m_panelContainerId, nullptr);
}

int ContainerManager::containerCount() const
{
    return m_containers.size();
}

void ContainerManager::setContainerVisible(const QString& id, bool visible)
{
    if (m_store && !m_reconciling) {
        WorkspaceDocument document = m_store->snapshot();
        for (auto& d : document.containers) { if (d.id == id) { d.visible = visible; } }
        commitWorkspace(document, document.revision); return;
    }
    if (!m_containers.contains(id)) {
        return;
    }
    ContainerWidget* c = m_containers[id];
    if (c->isFloating() && m_floatingForms.contains(id)) {
        m_floatingForms[id]->setVisible(visible);
    } else {
        c->setVisible(visible);
    }
}

// ---------------------------------------------------------------------------
// forEachMeterItem — Task 3.2 unit-mode fan-out helper
//
// Walks every container, extracts its MeterWidget via innerMeterWidget(),
// and invokes fn on each MeterItem in that widget.  Used by MultimeterPage
// to broadcast setUnitMode / setShowDecimal to all live items without the
// items having to poll AppSettings on every paint redraw.
// ---------------------------------------------------------------------------
void ContainerManager::forEachMeterItem(std::function<void(MeterItem*)> fn)
{
    for (ContainerWidget* container : m_containers) {
        QList<MeterWidget*> meters;
        if (auto* direct = qobject_cast<MeterWidget*>(container->content())) { meters.append(direct); }
        meters += container->content()->findChildren<MeterWidget*>();
        for (MeterWidget* meter : meters) {
        if (!meter) {
            continue;
        }
        for (MeterItem* item : meter->items()) {
            fn(item);
        }
        }
    }
}

void ContainerManager::setContentFactory(ContainerContentFactory factory)
{
    m_contentFactory = std::move(factory);
}

void ContainerManager::saveState()
{
    if (m_store) {
        if (m_reconciling || !m_store->loadError().isEmpty()) { return; }
        // Publication projects the final captured geometry; rejection restores
        // committed placement rather than preserving the rejected live draft.
        m_geometryCommit.stop();
        m_pendingGeometry.clear();
        WorkspaceDocument document = m_store->snapshot();
        for (auto& d : document.containers) {
            auto* c = container(d.id); if (!c) { continue; }
            // Geometry commits retain the committed content verbatim; live readings
            // and stack projection are presentation state, not document edits.
            d.name = c->notes(); d.dockMode = c->dockMode(); d.locked = c->isLocked(); d.autoHeight = c->autoHeight(); d.anchor = c->axisLock();
            d.geometry = c->isFloating() && m_floatingForms.value(d.id) ? m_floatingForms[d.id]->geometry() : c->geometry();
            if (c->rxSource() != d.config.value("rxSource").toInt(1)) { d.config["rxSource"] = c->rxSource(); }
            d.config["enabled"] = c->isContainerEnabled();
            d.config["showOnRx"] = c->showOnRx(); d.config["showOnTx"] = c->showOnTx();
            d.config["border"] = c->hasBorder(); d.config["pinOnTop"] = c->isPinOnTop();
        }
        if (m_splitter) { QJsonArray sizes; for (int size : m_splitter->sizes()) { sizes.append(size); } document.extensions["splitterSizes"] = sizes; }
        if (document == m_store->snapshot()) { return; }
        if (commitWorkspace(document, document.revision).status != CommitStatus::Saved) { reconcileWorkspace(m_store->snapshot()); }
        return;
    }
    // From Thetis MeterManager.cs:6391-6447
    // Upstream inline attribution preserved verbatim (MeterManager.cs:6433):
    //   //a.Add("meterIGSettings_" + ig.Value.ID, igs.ToString()); //[2.10.3.6]MW0LGE not used
    auto& s = AppSettings::instance();

    // Clear old data
    QString oldIdList = s.value(QStringLiteral("ContainerIdList")).toString();
    if (!oldIdList.isEmpty()) {
        for (const QString& oldId : oldIdList.split(QLatin1Char(','))) {
            s.remove(QStringLiteral("ContainerData_%1").arg(oldId));
            s.remove(QStringLiteral("ContainerItems_%1").arg(oldId));
            s.remove(QStringLiteral("MeterDisplay_%1_Geometry").arg(oldId));
        }
    }

    // Save current containers
    QStringList idList;
    for (auto it = m_containers.constBegin(); it != m_containers.constEnd(); ++it) {
        ContainerWidget* c = it.value();
        if (c->isOverlayDocked()) {
            c->storeLocation();
        }
        s.setValue(QStringLiteral("ContainerData_%1").arg(c->id()), c->serialize());
        // Persist the meter items hosted inside the container, if any.
        // Bare-MeterWidget and AppletPanelWidget content shapes both
        // resolve via innerMeterWidget(); placeholder content (the
        // QLabel installed by the constructor) returns nullptr and is
        // skipped. Stored in a parallel key — items payload contains
        // both '|' and '\n' separators that would corrupt the
        // field-tolerant container metadata format if appended.
        if (auto* meter = innerMeterWidget(c->content())) {
            s.setValue(QStringLiteral("ContainerItems_%1").arg(c->id()),
                       meter->serializeItems());
        }
        idList << c->id();
        if (m_floatingForms.contains(c->id())) {
            m_floatingForms[c->id()]->saveGeometry();
        }
    }

    s.setValue(QStringLiteral("ContainerIdList"), idList.join(QLatin1Char(',')));
    s.setValue(QStringLiteral("ContainerCount"), QString::number(m_containers.size()));
    saveSplitterState();

    qCDebug(lcContainer) << "Saved" << m_containers.size() << "container(s)";
}

void ContainerManager::restoreState()
{
    if (m_store) { reconcileWorkspace(m_store->snapshot()); restoreSplitterState(); return; }
    // From Thetis MeterManager.cs:6012-6105
    auto& s = AppSettings::instance();

    QString idList = s.value(QStringLiteral("ContainerIdList")).toString();
    if (idList.isEmpty()) {
        qCDebug(lcContainer) << "No saved containers found";
        return;
    }

    QStringList ids = idList.split(QLatin1Char(','), Qt::SkipEmptyParts);
    int restored = 0;

    for (const QString& id : ids) {
        QString data = s.value(QStringLiteral("ContainerData_%1").arg(id)).toString();
        if (data.isEmpty()) {
            continue;
        }

        auto* container = new ContainerWidget(nullptr);
        if (!container->deserialize(data)) {
            qCWarning(lcContainer) << "Failed to deserialize:" << id;
            delete container;
            continue;
        }

        // wireContainer BEFORE setContent so the contentChanged
        // listener catches the meterReadyForPolling emit on the
        // restore path. (Originally wireContainer ran after setContent
        // and listeners missed the announcement.)
        wireContainer(container);

        // Suppress interim meterReadyForPolling emissions. The
        // setContent below + setMeterFloating's extract/install cycle
        // would otherwise emit twice for a Floating container (once for
        // the placeholder meter, once for the fresh meter after
        // reparent); a single manual emit is sent at the end of this
        // iteration once the final content is in place.
        m_suppressMeterAnnouncements = true;

        // Materialize the inner content widget. MainWindow registers a
        // factory so the panel container gets an AppletPanelWidget;
        // when no factory is set (tests, headless tools) we default to
        // a bare MeterWidget which matches the user-created shape.
        QWidget* content = m_contentFactory
            ? m_contentFactory(container->id(), container->rxSource())
            : new MeterWidget();
        if (content) {
            container->setContent(content);
        }

        // Restore meter items into whichever MeterWidget the content
        // shape exposes (bare or wrapped). Empty payload → leave the
        // fresh meter empty so caller-side seeding can decide what to
        // do (Container #0's default presets, etc.).
        if (auto* meter = innerMeterWidget(content)) {
            const QString itemsPayload =
                s.value(QStringLiteral("ContainerItems_%1").arg(id)).toString();
            if (!itemsPayload.isEmpty()) {
                meter->deserializeItems(itemsPayload);
            }
        }

        auto* floatingForm = new FloatingContainer(container->rxSource());
        floatingForm->setId(container->id());

        m_containers.insert(container->id(), container);
        m_floatingForms.insert(container->id(), floatingForm);

        // Track the first restored container as the panel container, regardless
        // of its current dock mode. This ensures panelContainer() returns it so
        // MainWindow can populate its content (meters + applets).
        if (m_panelContainerId.isEmpty()) {
            m_panelContainerId = container->id();
        }

        switch (container->dockMode()) {
        case DockMode::PanelDocked:
            container->setParent(m_splitter);
            m_splitter->addWidget(container);
            m_panelContainerId = container->id();
            container->show();
            break;
        case DockMode::OverlayDocked:
            container->setParent(m_dockParent);
            container->restoreLocation();
            if (container->isContainerEnabled() && !container->isHiddenByMacro()) {
                container->show();
                container->raise();
            }
            break;
        case DockMode::Floating:
            container->resize(floatingForm->size());
            setMeterFloating(container, floatingForm);
            if (container->isContainerEnabled() && !container->isHiddenByMacro()) {
                floatingForm->show();
            }
            break;
        }

        // Interim emissions are done. Announce the final meter (if the
        // container has one) exactly once, then clear the guard so
        // post-restore user interactions announce normally.
        m_suppressMeterAnnouncements = false;
        if (auto* meter = innerMeterWidget(container->content())) {
            emit meterReadyForPolling(meter);
        }

        restored++;
        emit containerAdded(container->id());
    }

    restoreSplitterState();
    qCDebug(lcContainer) << "Restored" << restored << "container(s)";
}

} // namespace NereusSDR
