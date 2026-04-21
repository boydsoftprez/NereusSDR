// =================================================================
// src/gui/containers/ContainerManager.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
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
#include "ContainerWidget.h"
#include "ContainerSettingsDialog.h"
#include "FloatingContainer.h"
#include "core/AppSettings.h"
#include "core/LogCategories.h"
#include "gui/applets/AppletPanelWidget.h"
#include "gui/meters/MeterItem.h"
#include "gui/meters/MeterWidget.h"

#include <QSplitter>
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
    qCDebug(lcContainer) << "ContainerManager created";
}

ContainerManager::~ContainerManager()
{
    qCDebug(lcContainer) << "ContainerManager destroyed —" << m_containers.size() << "containers";
}

void ContainerManager::wireContainer(ContainerWidget* container)
{
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
    connect(container, &ContainerWidget::contentChanged, this,
            [this](QWidget* content) {
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
        emit meterReadyForPolling(fresh);
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

void ContainerManager::setContentFactory(ContainerContentFactory factory)
{
    m_contentFactory = std::move(factory);
}

void ContainerManager::saveState()
{
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

        restored++;
        emit containerAdded(container->id());
    }

    restoreSplitterState();
    qCDebug(lcContainer) << "Restored" << restored << "container(s)";
}

} // namespace NereusSDR
