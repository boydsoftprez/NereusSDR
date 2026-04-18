#pragma once

// =================================================================
// src/gui/containers/ContainerManager.h  (NereusSDR)
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

#include <QObject>
#include <QList>
#include <QMap>
#include <QSize>

#include <functional>

class QSplitter;
class QWidget;

namespace NereusSDR {

class ContainerWidget;
class FloatingContainer;
class MeterWidget;
enum class DockMode;

// Factory that materializes the inner content widget for a restored
// container. MainWindow registers one so that the panel container gets
// an AppletPanelWidget while user-created containers get a bare
// MeterWidget. If unset, ContainerManager defaults to a fresh
// MeterWidget — sufficient for unit tests and the common case.
using ContainerContentFactory =
    std::function<QWidget*(const QString& id, int rxSource)>;

class ContainerManager : public QObject {
    Q_OBJECT

public:
    explicit ContainerManager(QWidget* dockParent, QSplitter* splitter,
                              QObject* parent = nullptr);
    ~ContainerManager() override;

    // --- Container lifecycle ---
    ContainerWidget* createContainer(int rxSource, DockMode mode);
    void destroyContainer(const QString& id);

    // Create a new floating container that clones the user-editable
    // state of an existing container. Meter items are NOT copied here;
    // block 3's dialog rewrite wires the Duplicate button and will
    // copy items via MeterWidget::serializeItems round-trip.
    // From Thetis MeterManager.cs duplicate-container path.
    ContainerWidget* duplicateContainer(const QString& id);

    // --- Dock mode transitions ---
    void floatContainer(const QString& id);
    void dockContainer(const QString& id);
    void panelDockContainer(const QString& id);
    void overlayDockContainer(const QString& id);
    void recoverContainer(const QString& id);

    // --- Axis-lock repositioning (overlay-docked only) ---
    void updateDockedPositions(int hDelta, int vDelta);

    // --- Panel width (QSplitter state) ---
    void saveSplitterState();
    void restoreSplitterState();

    // --- Persistence ---
    void saveState();
    void restoreState();

    // Register the content factory used by restoreState() to populate
    // each restored container. Must be set before restoreState().
    void setContentFactory(ContainerContentFactory factory);

    // --- Queries ---
    QList<ContainerWidget*> allContainers() const;
    ContainerWidget* container(const QString& id) const;
    ContainerWidget* panelContainer() const;
    int containerCount() const;

    // --- Visibility ---
    void setContainerVisible(const QString& id, bool visible);

signals:
    void containerAdded(const QString& id);
    void containerRemoved(const QString& id);
    // Emitted when a container's notes change (notes are the source
    // of the display title). MainWindow consumes this in commit 45 to
    // rebuild the Containers → Edit Container submenu.
    void containerTitleChanged(const QString& id, const QString& title);
    // Emitted whenever a MeterWidget becomes (or appears as a
    // descendant of) a container's content. MainWindow connects this
    // to MeterPoller::addTarget so user-created and restored
    // containers both flow into the WDSP/MMIO poll loop. Without this
    // hookup the poller only knew about the panel container's initial
    // m_meterWidget and every other container's items sat at default
    // values forever — symptom: bars frozen at frac=0, needles never
    // moving.
    void meterReadyForPolling(MeterWidget* meter);

private:
    void setMeterFloating(ContainerWidget* container, FloatingContainer* form);
    void returnMeterFromFloating(ContainerWidget* container, FloatingContainer* form);
    void wireContainer(ContainerWidget* container);

    // Windows D3D11 QRhiWidget (WA_NativeWindow) cannot survive the HWND
    // recreation that setParent() triggers when moving a container between
    // top-level windows. Call extractMeterItems() before a reparent to
    // serialize and detach the MeterWidget, then installFreshMeter() after
    // the reparent to rebuild meter content in the new parent's HWND
    // context. No-op when the container holds placeholder content.
    QString extractMeterItems(ContainerWidget* container);
    void    installFreshMeter(ContainerWidget* container, const QString& payload);

    QWidget* m_dockParent{nullptr};
    QSplitter* m_splitter{nullptr};
    QMap<QString, ContainerWidget*> m_containers;
    QMap<QString, FloatingContainer*> m_floatingForms;
    QString m_panelContainerId;
    ContainerContentFactory m_contentFactory;
};

} // namespace NereusSDR
