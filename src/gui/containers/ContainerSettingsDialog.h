#pragma once

// =================================================================
// src/gui/containers/ContainerSettingsDialog.h  (NereusSDR)
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

#include <QDialog>
#include <QPair>
#include <QPoint>
#include <QStringList>
#include <QUuid>
#include <QVector>

class QListWidget;
class QStackedWidget;
class QSplitter;
class QFormLayout;
class QVBoxLayout;
class QComboBox;
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QLineEdit;
class QPushButton;
class QLabel;

namespace NereusSDR {

class ContainerWidget;
class ContainerManager;
class MeterWidget;
class MeterItem;
class BarItem;
class NeedleItem;
class TextItem;
class ScaleItem;
class SolidColourItem;
class LEDItem;

// Edit-container refactor Task 13 — in-use row wrapper.
//
// Each in-use list entry carries display metadata the raw MeterItem*
// doesn't own: a user-editable displayName (shown in the QListWidget
// rather than the tag-derived auto name) and a stable rowId that
// survives drag-reorder + rename so the UI can track selection
// independently of vector index.
struct ContainerInUseRow {
    MeterItem* item = nullptr;
    QString    displayName;
    QUuid      rowId;
};

class ContainerSettingsDialog : public QDialog {
    Q_OBJECT

public:
    // When a ContainerManager is supplied, the dialog shows a
    // container-switch dropdown at the top listing every container
    // via manager->allContainers(). Passing nullptr preserves the
    // legacy single-container behavior for callers that have not
    // been updated yet.
    explicit ContainerSettingsDialog(ContainerWidget* container,
                                     QWidget* parent = nullptr,
                                     ContainerManager* manager = nullptr);
    ~ContainerSettingsDialog() override;

public slots:
    // Override so Cancel / Esc / window-close all trigger the Task 14
    // rollback of any + Add containers the user didn't commit via
    // Apply/OK. Also drives revertFromSnapshot() for consistency.
    void reject() override;

    // Compute the next y-position for an item being appended to a
    // vertically-stacked layout. Items spanning more than 70% of the
    // container vertically are treated as background / overlay-style
    // items (ANANMM's full-container needles, ImageItem backgrounds,
    // etc.) and excluded from the stack calculation — otherwise an
    // ANANMM container would clamp every newly-added item to y=0.9
    // and pile them on top of each other. Public + static so the
    // tst_container_persistence suite can exercise it without
    // instantiating the dialog.
    static float nextStackYPos(const QVector<MeterItem*>& items);

    // Test-only accessors (Task 11+). Exposing these keeps the
    // dialog-refactor test suite free of Qt event-loop trickery;
    // production callers continue to use the private slots.
    QVector<MeterItem*> workingItems() const;
    void appendPresetRowForTest(const QString& presetName) {
        appendPresetRow(presetName);
    }

    // Task 13 test hooks. The right-click context menu path isn't
    // testable without poking QMenu::exec() timers, so expose the
    // slot bodies directly. rowIdAtIndex / displayNameForRowId let
    // tests assert on the wrapper vector without dragging the
    // QListWidget into the check.
    void    triggerRenameForTest(const QUuid& rowId, const QString& newName);
    void    triggerDuplicateForTest(const QUuid& rowId);
    void    triggerDeleteForTest(const QUuid& rowId);
    QUuid   rowIdAtIndex(int i) const;
    QString displayNameForRowId(const QUuid& id) const;

    // Task 14 test hooks.
    void    triggerAddContainerForTest() { onAddContainer(); }
    QString currentContainerDropdownText() const;

    // Code-review follow-up test hook: probe the enabled/disabled
    // state of an available-list row by its PRESET_* tag. Used by the
    // hybrid-rule tests to assert that every bar flavour correctly
    // disables its available-list entry after being added.
    bool availableRowIsEnabled(const QString& tag) const;

private slots:
    void onItemSelectionChanged();
    void onAddItem();
    void onRemoveItem();
    void onLoadPreset();
    void onExport();
    void onImport();

    // Task 13 — in-use list context menu + driver slots.
    void onInUseContextMenu(const QPoint& pos);
    void onItemRename(const QUuid& rowId);
    void onItemDuplicate(const QUuid& rowId);
    void onItemDelete(const QUuid& rowId);
    // Reconciles m_workingItems with the QListWidget's visual order
    // after a drag-reorder. Looks up each list row's rowId from
    // Qt::UserRole and rebuilds the wrapper vector to match.
    void syncWorkingItemsFromList();

private:
    void buildLayout();
    // Thetis 3-column layout: Available | In-use | Properties.
    void buildAvailablePanel(QWidget* parent);   // left
    void buildInUsePanel(QWidget* parent);       // center
    void buildPropertiesPanel(QWidget* parent);  // right
    void buildContainerPropertiesSection(QVBoxLayout* parentLayout);
    void buildButtonBar();

    // Populate the Available list with every item type the user can
    // insert into the in-use list. Commit 15 will categorize these
    // into RX / TX / Special sections.
    void populateAvailableList();
    void onAddFromAvailable();   // "Add >" button click

    // Edit-container refactor Task 12 — hybrid addition rule.
    // Presets (PRESET_* tags) are single-instance per container;
    // primitives (raw building-block items) are freely re-addable.
    // refreshAvailableList() walks the available QListWidget and
    // disables rows whose preset tag is already present in
    // m_workingItems. presetTagForItem() reverse-maps a live item
    // back to its available-list PRESET_* tag (empty for primitives).
    QString presetTagForItem(MeterItem* it) const;
    void    refreshAvailableList();

    // Edit-container refactor Task 13 — rowId helpers.
    MeterItem* findItemByRowId(const QUuid& id) const;
    QString    defaultDisplayNameFor(MeterItem* item) const;

    static MeterItem* createItemFromSerialized(const QString& data);
    void refreshItemList();
    static QString typeTagDisplayName(const QString& tag);

    void addNewItem(const QString& typeTag);
    static MeterItem* createDefaultItem(const QString& typeTag);
    void loadPresetByName(const QString& name);
    // Phase E — Thetis-parity "Add to Container" stacking flow.
    // Called from onAddFromAvailable for PRESET_* list entries.
    // Builds the named bar row preset and rescales its 0..1 items
    // into the next stack slot so rows pile up vertically instead
    // of overlapping.
    void appendPresetRow(const QString& presetName);

    void populateItemList();
    void updatePreview();
    void applyToContainer();

    // Find the MeterWidget inside the container's content hierarchy.
    // Handles both direct MeterWidget content and MeterWidget nested
    // inside AppletPanelWidget as a header widget.
    MeterWidget* findMeterWidget() const;

    // Phase 3G-6 block 4 dispatch — instantiates the per-item
    // BaseItemEditor subclass for the selected item's type tag.
    QWidget* buildTypeSpecificEditor(MeterItem* item);

    ContainerWidget* m_container{nullptr};
    ContainerManager* m_manager{nullptr};
    // Task 13 — m_workingItems was QVector<MeterItem*>; it is now a
    // parallel vector of ContainerInUseRow wrappers so each row can
    // carry a user-editable displayName and a stable rowId across
    // drag-reorder / rename / duplicate operations.
    QVector<ContainerInUseRow> m_workingItems;

    // Snapshot/revert (commit 14). Captured on dialog open and on
    // every container-switch via dropdown. Cancel restores the
    // currently-selected container to its snapshot; OK / Close /
    // Apply discards the snapshot and the in-progress edits stick.
    QString m_containerSnapshot;
    QString m_itemsSnapshot;
    // Phase 3G-7: MMIO bindings are not part of the text snapshot (block 5
    // kept them in-memory only). Capture them in parallel by item index so
    // Cancel restores them when revertFromSnapshot reloads from text.
    QVector<QPair<QUuid, QString>> m_mmioSnapshot;
    bool    m_snapshotTaken{false};

    void takeSnapshot();
    void revertFromSnapshot();

    // Block 7 commit 2: container-switch handler. Auto-commits the
    // current container's edits, then re-binds the dialog to the
    // new container, repopulates the in-use list, and takes a
    // fresh snapshot.
    void onContainerDropdownChanged(int index);

    // Block 7 commit 2: in-memory copy/paste clipboard, keyed by
    // type tag (the prefix of the serialized form). Paste is only
    // enabled when the clipboard type matches the currently-
    // selected item's type — Thetis behavior.
    QString m_clipboardSerialized;
    QString m_clipboardTypeTag;
    void onCopyItemSettings();
    void onPasteItemSettings();
    void updateCopyPasteButtonState();
    QPushButton* m_btnCopySettings{nullptr};
    QPushButton* m_btnPasteSettings{nullptr};

    // Container-switch dropdown (block 3 commit 13). Read-only until
    // commit 14 wires auto-commit + snapshot on switch.
    QComboBox* m_containerDropdown{nullptr};

    // Edit-container refactor Task 14 — + Add button next to the
    // container dropdown. Creates a new floating container, auto-
    // names it "Meter N" (smallest unused integer), and switches the
    // dialog focus to it so the user can immediately start editing.
    QPushButton* m_btnAddContainer{nullptr};
    void    onAddContainer();
    QString nextAutoName() const;

    // Task 14 follow-up: containers created via + Add during this
    // dialog session are tracked so Cancel can destroy them (matching
    // MainWindow's legacy "New Container..." lifecycle). Apply / OK
    // clear the list because applied containers are committed.
    QStringList m_containersAddedThisSession;

    QSplitter* m_splitter{nullptr};

    // Thetis 3-column layout
    // Left panel: Available item types (catalog).
    QListWidget* m_availableList{nullptr};
    QPushButton* m_btnAddFromAvailable{nullptr};

    // Center panel: In-use items in the container (the old m_itemList).
    // Task 13: Up/Down buttons retired — drag-reorder on m_itemList
    // replaces them. m_btnMoveUp / m_btnMoveDown removed.
    QListWidget* m_itemList{nullptr};
    QPushButton* m_btnAdd{nullptr};       // kept for legacy popup Add path
    QPushButton* m_btnRemove{nullptr};

    // Right panel: properties stack
    QStackedWidget* m_propertyStack{nullptr};
    QWidget* m_emptyPage{nullptr};

    // Currently-installed BaseItemEditor wrapper (a QScrollArea
    // containing the per-item editor). Replaced on every selection
    // change in onItemSelectionChanged.
    QWidget* m_currentTypeEditor{nullptr};

    // Phase 3G-6 block 3 commit 11: m_previewWidget deleted. The Thetis
    // 3-column rewrite lands a Properties column in its place.

    // Container properties
    QLineEdit* m_titleEdit{nullptr};
    QPushButton* m_bgColorBtn{nullptr};
    QCheckBox* m_borderCheck{nullptr};
    QComboBox* m_rxSourceCombo{nullptr};
    QCheckBox* m_showOnRxCheck{nullptr};
    QCheckBox* m_showOnTxCheck{nullptr};
    // Phase 3G-6 block 3 commit 17: additional container-level
    // controls for full Thetis parity on the property bar.
    QCheckBox*   m_lockCheck{nullptr};
    QCheckBox*   m_hideTitleCheck{nullptr};   // inverse of titleBarVisible
    QCheckBox*   m_minimisesCheck{nullptr};
    QCheckBox*   m_autoHeightCheck{nullptr};
    QCheckBox*   m_hidesWhenRxNotUsedCheck{nullptr};
    QCheckBox*   m_highlightCheck{nullptr};
    QPushButton* m_btnDuplicate{nullptr};
    QPushButton* m_btnDelete{nullptr};

    // Bottom buttons
    QPushButton* m_btnPreset{nullptr};
    QPushButton* m_btnImport{nullptr};
    QPushButton* m_btnExport{nullptr};
    // Phase 3G-6 block 3 commits 18+19: footer additions.
    QPushButton* m_btnSave{nullptr};     // serialize container+items to file
    QPushButton* m_btnLoad{nullptr};     // deserialize container+items from file
    QPushButton* m_btnMmio{nullptr};     // opens MMIO Variables dialog (block 5)
    QPushButton* m_btnOk{nullptr};
    QPushButton* m_btnCancel{nullptr};
    QPushButton* m_btnApply{nullptr};

    void onSaveToFile();
    void onLoadFromFile();
    void onOpenMmioDialog();
};

} // namespace NereusSDR
