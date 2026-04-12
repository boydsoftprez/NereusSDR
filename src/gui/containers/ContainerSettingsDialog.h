#pragma once

#include <QDialog>

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

private slots:
    void onItemSelectionChanged();
    void onAddItem();
    void onRemoveItem();
    void onMoveItemUp();
    void onMoveItemDown();
    void onLoadPreset();
    void onExport();
    void onImport();

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

    static MeterItem* createItemFromSerialized(const QString& data);
    void refreshItemList();
    static QString typeTagDisplayName(const QString& tag);

    void addNewItem(const QString& typeTag);
    static MeterItem* createDefaultItem(const QString& typeTag);
    void loadPresetByName(const QString& name);

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
    QVector<MeterItem*> m_workingItems;

    // Snapshot/revert (commit 14). Captured on dialog open and on
    // every container-switch via dropdown. Cancel restores the
    // currently-selected container to its snapshot; OK / Close /
    // Apply discards the snapshot and the in-progress edits stick.
    QString m_containerSnapshot;
    QString m_itemsSnapshot;
    bool    m_snapshotTaken{false};

    void takeSnapshot();
    void revertFromSnapshot();

    // Container-switch dropdown (block 3 commit 13). Read-only until
    // commit 14 wires auto-commit + snapshot on switch.
    QComboBox* m_containerDropdown{nullptr};

    QSplitter* m_splitter{nullptr};

    // Thetis 3-column layout
    // Left panel: Available item types (catalog).
    QListWidget* m_availableList{nullptr};
    QPushButton* m_btnAddFromAvailable{nullptr};

    // Center panel: In-use items in the container (the old m_itemList).
    QListWidget* m_itemList{nullptr};
    QPushButton* m_btnAdd{nullptr};       // kept for legacy popup Add path
    QPushButton* m_btnRemove{nullptr};
    QPushButton* m_btnMoveUp{nullptr};
    QPushButton* m_btnMoveDown{nullptr};

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
