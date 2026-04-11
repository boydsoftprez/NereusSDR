# Phase 3G-6: Container Settings Dialog — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Create a modal settings dialog for ContainerWidget that lets users compose meter item layouts, edit per-item properties, preview in real-time, import/export configurations, and load presets.

**Architecture:** Three-panel QDialog (content list / property editor / live preview) opened from the existing `ContainerWidget::settingsRequested()` signal. The dialog reads from/writes to the container's MeterWidget items using the existing serialization system. Container-level properties edit ContainerWidget state directly. Preset loading uses existing ItemGroup factory methods.

**Tech Stack:** Qt6 (QDialog, QListWidget, QStackedWidget, QSplitter, QFormLayout), C++20, existing MeterWidget/MeterItem/ItemGroup/ContainerWidget classes.

---

## File Structure

| Action | Path | Responsibility |
|--------|------|----------------|
| Create | `src/gui/containers/ContainerSettingsDialog.h` | Dialog class declaration |
| Create | `src/gui/containers/ContainerSettingsDialog.cpp` | Dialog implementation (3 panels, property editors, presets, import/export) |
| Modify | `src/gui/containers/ContainerManager.cpp` | Wire `settingsRequested()` signal to open dialog |
| Modify | `src/gui/meters/MeterWidget.h/.cpp` | Add type registry deserializer for all 3G-4/5 types (currently only core 6 types in `deserializeItems()`) |
| Modify | `CMakeLists.txt` | Add new source file |

**Note on decomposition:** The dialog is a single UI class but implements distinct subsystems: content list panel, property editor panel, live preview panel, import/export, and preset browser. Each task below handles one subsystem, building incrementally.

---

## Scope Check

This is a single subsystem (container settings dialog). It produces a self-contained, testable feature: clicking the gear button on any container opens a fully functional settings dialog.

---

### Task 1: Dialog Shell + Three-Panel Layout

**Files:**
- Create: `src/gui/containers/ContainerSettingsDialog.h`
- Create: `src/gui/containers/ContainerSettingsDialog.cpp`
- Modify: `CMakeLists.txt:196` (add source file near other container files)

- [ ] **Step 1: Create the header file**

```cpp
// src/gui/containers/ContainerSettingsDialog.h
#pragma once

#include <QDialog>

class QListWidget;
class QStackedWidget;
class QSplitter;
class QFormLayout;
class QComboBox;
class QSpinBox;
class QCheckBox;
class QLineEdit;
class QPushButton;
class QLabel;

namespace NereusSDR {

class ContainerWidget;
class MeterWidget;
class MeterItem;

class ContainerSettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit ContainerSettingsDialog(ContainerWidget* container,
                                     QWidget* parent = nullptr);
    ~ContainerSettingsDialog() override;

private slots:
    void onItemSelectionChanged();
    void onAddItem();
    void onRemoveItem();
    void onMoveItemUp();
    void onMoveItemDown();

private:
    void buildLayout();
    void buildLeftPanel(QWidget* parent);
    void buildCenterPanel(QWidget* parent);
    void buildRightPanel(QWidget* parent);
    void buildContainerPropertiesSection(QVBoxLayout* parentLayout);
    void buildButtonBar();

    void populateItemList();
    void updatePreview();
    void applyToContainer();

    // --- Owning container ---
    ContainerWidget* m_container{nullptr};

    // --- Working copy of items (edit in-place, apply on OK) ---
    QVector<MeterItem*> m_workingItems;

    // --- Top-level layout ---
    QSplitter* m_splitter{nullptr};

    // --- Left panel: content list ---
    QListWidget* m_itemList{nullptr};
    QPushButton* m_btnAdd{nullptr};
    QPushButton* m_btnRemove{nullptr};
    QPushButton* m_btnMoveUp{nullptr};
    QPushButton* m_btnMoveDown{nullptr};

    // --- Center panel: property editor ---
    QStackedWidget* m_propertyStack{nullptr};
    QWidget* m_emptyPage{nullptr};

    // --- Right panel: live preview ---
    MeterWidget* m_previewWidget{nullptr};

    // --- Container properties ---
    QLineEdit* m_titleEdit{nullptr};
    QPushButton* m_bgColorBtn{nullptr};
    QCheckBox* m_borderCheck{nullptr};
    QComboBox* m_rxSourceCombo{nullptr};
    QCheckBox* m_showOnRxCheck{nullptr};
    QCheckBox* m_showOnTxCheck{nullptr};

    // --- Bottom buttons ---
    QPushButton* m_btnPreset{nullptr};
    QPushButton* m_btnImport{nullptr};
    QPushButton* m_btnExport{nullptr};
    QPushButton* m_btnOk{nullptr};
    QPushButton* m_btnCancel{nullptr};
    QPushButton* m_btnApply{nullptr};
};

} // namespace NereusSDR
```

- [ ] **Step 2: Create the implementation file with buildLayout()**

```cpp
// src/gui/containers/ContainerSettingsDialog.cpp
#include "ContainerSettingsDialog.h"
#include "ContainerWidget.h"
#include "gui/meters/MeterWidget.h"
#include "gui/meters/MeterItem.h"
#include "gui/meters/ItemGroup.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QListWidget>
#include <QStackedWidget>
#include <QFormLayout>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QColorDialog>
#include <QMessageBox>
#include <QClipboard>
#include <QApplication>
#include <QMenu>

namespace NereusSDR {

ContainerSettingsDialog::ContainerSettingsDialog(ContainerWidget* container,
                                                 QWidget* parent)
    : QDialog(parent)
    , m_container(container)
{
    setWindowTitle(QStringLiteral("Container Settings — %1").arg(container->id().left(8)));
    setMinimumSize(800, 500);
    resize(900, 600);
    buildLayout();
    populateItemList();
    updatePreview();
}

ContainerSettingsDialog::~ContainerSettingsDialog()
{
    // Working items not yet applied are owned by us
    qDeleteAll(m_workingItems);
}

void ContainerSettingsDialog::buildLayout()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(6);

    // Three-panel splitter
    m_splitter = new QSplitter(Qt::Horizontal, this);

    auto* leftPanel = new QWidget(m_splitter);
    auto* centerPanel = new QWidget(m_splitter);
    auto* rightPanel = new QWidget(m_splitter);

    buildLeftPanel(leftPanel);
    buildCenterPanel(centerPanel);
    buildRightPanel(rightPanel);

    m_splitter->addWidget(leftPanel);
    m_splitter->addWidget(centerPanel);
    m_splitter->addWidget(rightPanel);
    m_splitter->setSizes({200, 400, 200});
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setStretchFactor(2, 0);

    // Container-level properties above the splitter
    buildContainerPropertiesSection(mainLayout);

    mainLayout->addWidget(m_splitter, 1);

    // Bottom button bar
    buildButtonBar();
}

void ContainerSettingsDialog::buildLeftPanel(QWidget* parent)
{
    auto* layout = new QVBoxLayout(parent);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    auto* label = new QLabel(QStringLiteral("Items"), parent);
    label->setStyleSheet(QStringLiteral("color: #8aa8c0; font-weight: bold;"));
    layout->addWidget(label);

    m_itemList = new QListWidget(parent);
    m_itemList->setDragDropMode(QAbstractItemView::InternalMove);
    m_itemList->setStyleSheet(QStringLiteral(
        "QListWidget { background: #0a0a18; color: #c8d8e8; border: 1px solid #203040; }"
        "QListWidget::item:selected { background: #00b4d8; color: #0f0f1a; }"));
    layout->addWidget(m_itemList, 1);

    connect(m_itemList, &QListWidget::currentRowChanged,
            this, &ContainerSettingsDialog::onItemSelectionChanged);

    auto* btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(4);

    const QString btnStyle = QStringLiteral(
        "QPushButton { background: #1a2a3a; color: #c8d8e8; border: 1px solid #205070;"
        " border-radius: 3px; padding: 4px 8px; }"
        "QPushButton:hover { background: #203040; }"
        "QPushButton:pressed { background: #00b4d8; }");

    m_btnAdd = new QPushButton(QStringLiteral("+ Add"), parent);
    m_btnAdd->setStyleSheet(btnStyle);
    m_btnAdd->setAutoDefault(false);
    btnLayout->addWidget(m_btnAdd);

    m_btnRemove = new QPushButton(QStringLiteral("- Remove"), parent);
    m_btnRemove->setStyleSheet(btnStyle);
    m_btnRemove->setAutoDefault(false);
    btnLayout->addWidget(m_btnRemove);

    m_btnMoveUp = new QPushButton(QStringLiteral("\u25b2"), parent);
    m_btnMoveUp->setFixedWidth(30);
    m_btnMoveUp->setStyleSheet(btnStyle);
    m_btnMoveUp->setAutoDefault(false);
    m_btnMoveUp->setToolTip(QStringLiteral("Move up (higher z-order)"));
    btnLayout->addWidget(m_btnMoveUp);

    m_btnMoveDown = new QPushButton(QStringLiteral("\u25bc"), parent);
    m_btnMoveDown->setFixedWidth(30);
    m_btnMoveDown->setStyleSheet(btnStyle);
    m_btnMoveDown->setAutoDefault(false);
    m_btnMoveDown->setToolTip(QStringLiteral("Move down (lower z-order)"));
    btnLayout->addWidget(m_btnMoveDown);

    layout->addLayout(btnLayout);

    connect(m_btnAdd, &QPushButton::clicked, this, &ContainerSettingsDialog::onAddItem);
    connect(m_btnRemove, &QPushButton::clicked, this, &ContainerSettingsDialog::onRemoveItem);
    connect(m_btnMoveUp, &QPushButton::clicked, this, &ContainerSettingsDialog::onMoveItemUp);
    connect(m_btnMoveDown, &QPushButton::clicked, this, &ContainerSettingsDialog::onMoveItemDown);
}

void ContainerSettingsDialog::buildCenterPanel(QWidget* parent)
{
    auto* layout = new QVBoxLayout(parent);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* label = new QLabel(QStringLiteral("Properties"), parent);
    label->setStyleSheet(QStringLiteral("color: #8aa8c0; font-weight: bold;"));
    layout->addWidget(label);

    m_propertyStack = new QStackedWidget(parent);

    // Page 0: empty placeholder (no selection)
    m_emptyPage = new QWidget(m_propertyStack);
    auto* emptyLayout = new QVBoxLayout(m_emptyPage);
    auto* emptyLabel = new QLabel(QStringLiteral("Select an item to edit properties"), m_emptyPage);
    emptyLabel->setAlignment(Qt::AlignCenter);
    emptyLabel->setStyleSheet(QStringLiteral("color: #405060; font-size: 13px;"));
    emptyLayout->addWidget(emptyLabel);
    m_propertyStack->addWidget(m_emptyPage);

    layout->addWidget(m_propertyStack, 1);
}

void ContainerSettingsDialog::buildRightPanel(QWidget* parent)
{
    auto* layout = new QVBoxLayout(parent);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* label = new QLabel(QStringLiteral("Preview"), parent);
    label->setStyleSheet(QStringLiteral("color: #8aa8c0; font-weight: bold;"));
    layout->addWidget(label);

    m_previewWidget = new MeterWidget(parent);
    m_previewWidget->setMinimumSize(180, 200);
    layout->addWidget(m_previewWidget, 1);
}

void ContainerSettingsDialog::buildContainerPropertiesSection(QVBoxLayout* parentLayout)
{
    auto* containerGroup = new QWidget(this);
    parentLayout->addWidget(containerGroup);
    auto* formLayout = new QHBoxLayout(containerGroup);
    formLayout->setContentsMargins(0, 0, 0, 4);
    formLayout->setSpacing(12);

    const QString labelStyle = QStringLiteral("color: #8090a0; font-size: 12px;");
    const QString editStyle = QStringLiteral(
        "background: #0a0a18; color: #c8d8e8; border: 1px solid #1e2e3e;"
        " border-radius: 3px; padding: 2px 4px;");
    const QString btnStyle = QStringLiteral(
        "QPushButton { background: #1a2a3a; color: #c8d8e8; border: 1px solid #205070;"
        " border-radius: 3px; padding: 2px 8px; }"
        "QPushButton:hover { background: #203040; }");

    // Title
    auto* titleLabel = new QLabel(QStringLiteral("Title:"), containerGroup);
    titleLabel->setStyleSheet(labelStyle);
    formLayout->addWidget(titleLabel);
    m_titleEdit = new QLineEdit(m_container->notes(), containerGroup);
    m_titleEdit->setStyleSheet(editStyle);
    m_titleEdit->setMaximumWidth(150);
    formLayout->addWidget(m_titleEdit);

    // Background color
    auto* bgLabel = new QLabel(QStringLiteral("BG:"), containerGroup);
    bgLabel->setStyleSheet(labelStyle);
    formLayout->addWidget(bgLabel);
    m_bgColorBtn = new QPushButton(containerGroup);
    m_bgColorBtn->setFixedSize(40, 22);
    m_bgColorBtn->setAutoDefault(false);
    // Show current color as button background
    m_bgColorBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; border: 1px solid #205070; border-radius: 3px; }")
        .arg(QStringLiteral("#0f0f1a")));
    formLayout->addWidget(m_bgColorBtn);
    connect(m_bgColorBtn, &QPushButton::clicked, this, [this]() {
        QColor c = QColorDialog::getColor(QColor(0x0f, 0x0f, 0x1a), this,
                                           QStringLiteral("Background Color"));
        if (c.isValid()) {
            m_bgColorBtn->setStyleSheet(QStringLiteral(
                "QPushButton { background: %1; border: 1px solid #205070; border-radius: 3px; }")
                .arg(c.name()));
            updatePreview();
        }
    });

    // Border
    m_borderCheck = new QCheckBox(QStringLiteral("Border"), containerGroup);
    m_borderCheck->setChecked(m_container->hasBorder());
    m_borderCheck->setStyleSheet(QStringLiteral("color: #c8d8e8;"));
    formLayout->addWidget(m_borderCheck);

    // RX Source
    auto* rxLabel = new QLabel(QStringLiteral("RX:"), containerGroup);
    rxLabel->setStyleSheet(labelStyle);
    formLayout->addWidget(rxLabel);
    m_rxSourceCombo = new QComboBox(containerGroup);
    m_rxSourceCombo->addItem(QStringLiteral("RX1"), 1);
    m_rxSourceCombo->addItem(QStringLiteral("RX2"), 2);
    m_rxSourceCombo->setCurrentIndex(m_container->rxSource() - 1);
    m_rxSourceCombo->setStyleSheet(editStyle);
    formLayout->addWidget(m_rxSourceCombo);

    // Show on RX/TX
    m_showOnRxCheck = new QCheckBox(QStringLiteral("Show RX"), containerGroup);
    m_showOnRxCheck->setChecked(m_container->showOnRx());
    m_showOnRxCheck->setStyleSheet(QStringLiteral("color: #c8d8e8;"));
    formLayout->addWidget(m_showOnRxCheck);

    m_showOnTxCheck = new QCheckBox(QStringLiteral("Show TX"), containerGroup);
    m_showOnTxCheck->setChecked(m_container->showOnTx());
    m_showOnTxCheck->setStyleSheet(QStringLiteral("color: #c8d8e8;"));
    formLayout->addWidget(m_showOnTxCheck);

    formLayout->addStretch();

}

void ContainerSettingsDialog::buildButtonBar()
{
    auto* btnBar = new QHBoxLayout();
    btnBar->setSpacing(6);

    const QString btnStyle = QStringLiteral(
        "QPushButton { background: #1a2a3a; color: #c8d8e8; border: 1px solid #205070;"
        " border-radius: 3px; padding: 6px 14px; }"
        "QPushButton:hover { background: #203040; }"
        "QPushButton:pressed { background: #00b4d8; }");
    const QString accentStyle = QStringLiteral(
        "QPushButton { background: #00b4d8; color: #0f0f1a; border: 1px solid #00d4f8;"
        " border-radius: 3px; padding: 6px 14px; font-weight: bold; }"
        "QPushButton:hover { background: #00c4e8; }");

    m_btnPreset = new QPushButton(QStringLiteral("Presets..."), this);
    m_btnPreset->setStyleSheet(btnStyle);
    m_btnPreset->setAutoDefault(false);
    btnBar->addWidget(m_btnPreset);

    m_btnImport = new QPushButton(QStringLiteral("Import"), this);
    m_btnImport->setStyleSheet(btnStyle);
    m_btnImport->setAutoDefault(false);
    btnBar->addWidget(m_btnImport);

    m_btnExport = new QPushButton(QStringLiteral("Export"), this);
    m_btnExport->setStyleSheet(btnStyle);
    m_btnExport->setAutoDefault(false);
    btnBar->addWidget(m_btnExport);

    btnBar->addStretch();

    m_btnApply = new QPushButton(QStringLiteral("Apply"), this);
    m_btnApply->setStyleSheet(btnStyle);
    m_btnApply->setAutoDefault(false);
    btnBar->addWidget(m_btnApply);

    m_btnCancel = new QPushButton(QStringLiteral("Cancel"), this);
    m_btnCancel->setStyleSheet(btnStyle);
    m_btnCancel->setAutoDefault(false);
    btnBar->addWidget(m_btnCancel);

    m_btnOk = new QPushButton(QStringLiteral("OK"), this);
    m_btnOk->setStyleSheet(accentStyle);
    m_btnOk->setAutoDefault(false);
    btnBar->addWidget(m_btnOk);

    connect(m_btnOk, &QPushButton::clicked, this, [this]() {
        applyToContainer();
        accept();
    });
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_btnApply, &QPushButton::clicked, this, [this]() {
        applyToContainer();
    });

    static_cast<QVBoxLayout*>(layout())->addLayout(btnBar);
}

// Stub implementations — filled in subsequent tasks
void ContainerSettingsDialog::onItemSelectionChanged() {}
void ContainerSettingsDialog::onAddItem() {}
void ContainerSettingsDialog::onRemoveItem() {}
void ContainerSettingsDialog::onMoveItemUp() {}
void ContainerSettingsDialog::onMoveItemDown() {}

void ContainerSettingsDialog::populateItemList()
{
    // Clone current container items into working copy
    // Items are re-created from serialization to get independent copies
    MeterWidget* source = qobject_cast<MeterWidget*>(m_container->content());
    if (!source) { return; }

    QString serialized = source->serializeItems();
    if (serialized.isEmpty()) { return; }

    // Parse each line and create working copies
    const QStringList lines = serialized.split(QLatin1Char('\n'));
    for (const QString& line : lines) {
        if (line.isEmpty()) { continue; }
        // Use ItemGroup's type registry pattern to create items
        MeterItem* item = createItemFromSerialized(line);
        if (item) {
            m_workingItems.append(item);
        }
    }

    // Populate list widget
    refreshItemList();
}

void ContainerSettingsDialog::updatePreview()
{
    m_previewWidget->clearItems();
    // Add clones of working items to preview
    for (MeterItem* item : m_workingItems) {
        QString data = item->serialize();
        MeterItem* clone = createItemFromSerialized(data);
        if (clone) {
            m_previewWidget->addItem(clone);
        }
    }
    m_previewWidget->update();
}

void ContainerSettingsDialog::applyToContainer()
{
    // Apply container-level properties
    m_container->setNotes(m_titleEdit->text());
    m_container->setBorder(m_borderCheck->isChecked());
    m_container->setRxSource(m_rxSourceCombo->currentData().toInt());
    m_container->setShowOnRx(m_showOnRxCheck->isChecked());
    m_container->setShowOnTx(m_showOnTxCheck->isChecked());

    // Apply item configuration to the real MeterWidget
    MeterWidget* target = qobject_cast<MeterWidget*>(m_container->content());
    if (!target) { return; }

    target->clearItems();
    for (MeterItem* item : m_workingItems) {
        QString data = item->serialize();
        MeterItem* clone = createItemFromSerialized(data);
        if (clone) {
            target->addItem(clone);
            // Re-wire interactive item signals
            m_container->wireInteractiveItem(clone);
        }
    }
    target->update();
}

} // namespace NereusSDR
```

**Note:** `createItemFromSerialized()` and `refreshItemList()` are helper methods added in Task 2. The stubs above reference them to show the intended flow — they will be implemented before this code is compiled.

- [ ] **Step 3: Add source file to CMakeLists.txt**

In `CMakeLists.txt`, after the existing container source files (around line 197), add:

```cmake
    src/gui/containers/ContainerSettingsDialog.cpp
```

- [ ] **Step 4: Verify the file compiles (expect linker errors for stubs)**

Run: `cd /Users/j.j.boyd/NereusSDR && cmake --build build -j$(sysctl -n hw.ncpu) 2>&1 | head -30`
Expected: Compile errors for `createItemFromSerialized` and `refreshItemList` — these are added in Task 2.

- [ ] **Step 5: Commit**

```bash
git add src/gui/containers/ContainerSettingsDialog.h \
        src/gui/containers/ContainerSettingsDialog.cpp \
        CMakeLists.txt
git commit -m "feat(3G-6): add ContainerSettingsDialog shell with 3-panel layout

Three-panel QDialog: content list (left), property editor (center),
live preview (right). Container-level properties bar at top.
OK/Cancel/Apply + Presets/Import/Export button bar."
```

---

### Task 2: Item Type Registry Helper + Content List Population

**Files:**
- Modify: `src/gui/containers/ContainerSettingsDialog.h` (add helper declarations)
- Modify: `src/gui/containers/ContainerSettingsDialog.cpp` (implement helpers)

This task creates the `createItemFromSerialized()` factory that mirrors the type-tag registry in `ItemGroup::deserialize()` and `MeterWidget::deserializeItems()`, and implements `refreshItemList()` + `populateItemList()` to load items from the container.

- [ ] **Step 1: Add helper declarations to the header**

Add to the `private:` section of `ContainerSettingsDialog`:

```cpp
    // Create a MeterItem from a serialized pipe-delimited line.
    // Mirrors the type-tag registry in ItemGroup::deserialize().
    static MeterItem* createItemFromSerialized(const QString& data);

    // Rebuild the QListWidget from m_workingItems.
    void refreshItemList();

    // Human-readable name for a type tag.
    static QString typeTagDisplayName(const QString& tag);
```

- [ ] **Step 2: Implement the type registry factory**

This mirrors the existing `ItemGroup::deserialize()` registry at `ItemGroup.cpp:139-310`. Every type tag that exists in the deserialize registry must appear here.

```cpp
#include "gui/meters/SpacerItem.h"
#include "gui/meters/FadeCoverItem.h"
#include "gui/meters/LEDItem.h"
#include "gui/meters/HistoryGraphItem.h"
#include "gui/meters/MagicEyeItem.h"
#include "gui/meters/NeedleScalePwrItem.h"
#include "gui/meters/SignalTextItem.h"
#include "gui/meters/DialItem.h"
#include "gui/meters/TextOverlayItem.h"
#include "gui/meters/WebImageItem.h"
#include "gui/meters/FilterDisplayItem.h"
#include "gui/meters/RotatorItem.h"
#include "gui/meters/ButtonBoxItem.h"
#include "gui/meters/BandButtonItem.h"
#include "gui/meters/ModeButtonItem.h"
#include "gui/meters/FilterButtonItem.h"
#include "gui/meters/AntennaButtonItem.h"
#include "gui/meters/TuneStepButtonItem.h"
#include "gui/meters/OtherButtonItem.h"
#include "gui/meters/VoiceRecordPlayItem.h"
#include "gui/meters/DiscordButtonItem.h"
#include "gui/meters/VfoDisplayItem.h"
#include "gui/meters/ClockItem.h"
#include "gui/meters/ClickBoxItem.h"
#include "gui/meters/DataOutItem.h"

MeterItem* ContainerSettingsDialog::createItemFromSerialized(const QString& data)
{
    if (data.isEmpty()) { return nullptr; }

    const QString tag = data.section(QLatin1Char('|'), 0, 0);
    MeterItem* item = nullptr;

    // Core types (MeterItem.h)
    if (tag == QLatin1String("BAR"))                { item = new BarItem(); }
    else if (tag == QLatin1String("SOLID"))          { item = new SolidColourItem(); }
    else if (tag == QLatin1String("IMAGE"))          { item = new ImageItem(); }
    else if (tag == QLatin1String("SCALE"))          { item = new ScaleItem(); }
    else if (tag == QLatin1String("TEXT"))           { item = new TextItem(); }
    else if (tag == QLatin1String("NEEDLE"))         { item = new NeedleItem(); }
    // Phase 3G-4 passive types
    else if (tag == QLatin1String("SPACER"))         { item = new SpacerItem(); }
    else if (tag == QLatin1String("FADECOVER"))       { item = new FadeCoverItem(); }
    else if (tag == QLatin1String("LED"))            { item = new LEDItem(); }
    else if (tag == QLatin1String("HISTORY"))        { item = new HistoryGraphItem(); }
    else if (tag == QLatin1String("MAGICEYE"))       { item = new MagicEyeItem(); }
    else if (tag == QLatin1String("NEEDLESCALEPWR")) { item = new NeedleScalePwrItem(); }
    else if (tag == QLatin1String("SIGNALTEXT"))     { item = new SignalTextItem(); }
    else if (tag == QLatin1String("DIAL"))           { item = new DialItem(); }
    else if (tag == QLatin1String("TEXTOVERLAY"))    { item = new TextOverlayItem(); }
    else if (tag == QLatin1String("WEBIMAGE"))       { item = new WebImageItem(); }
    else if (tag == QLatin1String("FILTERDISPLAY"))  { item = new FilterDisplayItem(); }
    else if (tag == QLatin1String("ROTATOR"))        { item = new RotatorItem(); }
    // Phase 3G-5 interactive types
    else if (tag == QLatin1String("BANDBTNS"))       { item = new BandButtonItem(); }
    else if (tag == QLatin1String("MODEBTNS"))       { item = new ModeButtonItem(); }
    else if (tag == QLatin1String("FILTERBTNS"))     { item = new FilterButtonItem(); }
    else if (tag == QLatin1String("ANTENNABTNS"))    { item = new AntennaButtonItem(); }
    else if (tag == QLatin1String("TUNESTEPBTNS"))   { item = new TuneStepButtonItem(); }
    else if (tag == QLatin1String("OTHERBTNS"))      { item = new OtherButtonItem(); }
    else if (tag == QLatin1String("VOICERECPLAY"))   { item = new VoiceRecordPlayItem(); }
    else if (tag == QLatin1String("DISCORDBTNS"))    { item = new DiscordButtonItem(); }
    else if (tag == QLatin1String("VFO"))            { item = new VfoDisplayItem(); }
    else if (tag == QLatin1String("CLOCK"))          { item = new ClockItem(); }
    else if (tag == QLatin1String("CLICKBOX"))       { item = new ClickBoxItem(); }
    else if (tag == QLatin1String("DATAOUT"))        { item = new DataOutItem(); }

    if (item && !item->deserialize(data)) {
        delete item;
        return nullptr;
    }
    return item;
}
```

- [ ] **Step 3: Implement refreshItemList() and typeTagDisplayName()**

```cpp
QString ContainerSettingsDialog::typeTagDisplayName(const QString& tag)
{
    // Map type tags to user-friendly display names.
    // Matches all tags from design spec Serialization Registry.
    static const QMap<QString, QString> names = {
        {QStringLiteral("BAR"),            QStringLiteral("Bar Meter")},
        {QStringLiteral("SOLID"),          QStringLiteral("Solid Color")},
        {QStringLiteral("IMAGE"),          QStringLiteral("Image")},
        {QStringLiteral("SCALE"),          QStringLiteral("Scale")},
        {QStringLiteral("TEXT"),           QStringLiteral("Text Readout")},
        {QStringLiteral("NEEDLE"),         QStringLiteral("Needle Meter")},
        {QStringLiteral("SPACER"),         QStringLiteral("Spacer")},
        {QStringLiteral("FADECOVER"),       QStringLiteral("Fade Cover")},
        {QStringLiteral("LED"),            QStringLiteral("LED Indicator")},
        {QStringLiteral("HISTORY"),        QStringLiteral("History Graph")},
        {QStringLiteral("MAGICEYE"),       QStringLiteral("Magic Eye")},
        {QStringLiteral("NEEDLESCALEPWR"), QStringLiteral("Power Scale")},
        {QStringLiteral("SIGNALTEXT"),     QStringLiteral("Signal Text")},
        {QStringLiteral("DIAL"),           QStringLiteral("Dial Meter")},
        {QStringLiteral("TEXTOVERLAY"),    QStringLiteral("Text Overlay")},
        {QStringLiteral("WEBIMAGE"),       QStringLiteral("Web Image")},
        {QStringLiteral("FILTERDISPLAY"),  QStringLiteral("Filter Display")},
        {QStringLiteral("ROTATOR"),        QStringLiteral("Rotator")},
        {QStringLiteral("BANDBTNS"),       QStringLiteral("Band Buttons")},
        {QStringLiteral("MODEBTNS"),       QStringLiteral("Mode Buttons")},
        {QStringLiteral("FILTERBTNS"),     QStringLiteral("Filter Buttons")},
        {QStringLiteral("ANTENNABTNS"),    QStringLiteral("Antenna Buttons")},
        {QStringLiteral("TUNESTEPBTNS"),   QStringLiteral("Tune Step Buttons")},
        {QStringLiteral("OTHERBTNS"),      QStringLiteral("Other Buttons")},
        {QStringLiteral("VOICERECPLAY"),   QStringLiteral("Voice Rec/Play")},
        {QStringLiteral("DISCORDBTNS"),    QStringLiteral("Discord Buttons")},
        {QStringLiteral("VFO"),            QStringLiteral("VFO Display")},
        {QStringLiteral("CLOCK"),          QStringLiteral("Clock")},
        {QStringLiteral("CLICKBOX"),       QStringLiteral("Click Box")},
        {QStringLiteral("DATAOUT"),        QStringLiteral("Data Output")},
    };
    return names.value(tag, tag);
}

void ContainerSettingsDialog::refreshItemList()
{
    m_itemList->clear();
    for (const MeterItem* item : m_workingItems) {
        QString serialized = item->serialize();
        QString tag = serialized.section(QLatin1Char('|'), 0, 0);
        QString displayName = typeTagDisplayName(tag);

        // Show binding if present
        if (item->bindingId() >= 0) {
            displayName += QStringLiteral(" [%1]").arg(item->bindingId());
        }

        m_itemList->addItem(displayName);
    }
}
```

- [ ] **Step 4: Build and verify**

Run: `cd /Users/j.j.boyd/NereusSDR && cmake --build build -j$(sysctl -n hw.ncpu) 2>&1 | tail -5`
Expected: Clean compile (or only unrelated warnings).

- [ ] **Step 5: Commit**

```bash
git add src/gui/containers/ContainerSettingsDialog.h \
        src/gui/containers/ContainerSettingsDialog.cpp
git commit -m "feat(3G-6): add item type registry + content list population

createItemFromSerialized() factory mirrors ItemGroup deserialize registry
for all 28 MeterItem types. refreshItemList() populates the list widget
with human-readable type names and binding IDs."
```

---

### Task 3: Content List Manipulation (Add/Remove/Reorder)

**Files:**
- Modify: `src/gui/containers/ContainerSettingsDialog.h` (add category picker method)
- Modify: `src/gui/containers/ContainerSettingsDialog.cpp`

- [ ] **Step 1: Implement the Add Item categorized picker**

Replace the `onAddItem()` stub:

```cpp
void ContainerSettingsDialog::onAddItem()
{
    // Categorized picker popup matching design spec categories.
    // From design spec: 6 categories (Meters, Indicators, Controls, Display, Layout, Data)
    QMenu menu(this);
    menu.setStyleSheet(QStringLiteral(
        "QMenu { background: #1a2a3a; color: #c8d8e8; border: 1px solid #205070; }"
        "QMenu::item:selected { background: #00b4d8; color: #0f0f1a; }"
        "QMenu::separator { background: #203040; height: 1px; }"));

    // --- Meters ---
    QMenu* meters = menu.addMenu(QStringLiteral("Meters"));
    meters->addAction(QStringLiteral("Bar Meter"), this, [this]() { addNewItem(QStringLiteral("BAR")); });
    meters->addAction(QStringLiteral("Needle Meter"), this, [this]() { addNewItem(QStringLiteral("NEEDLE")); });
    meters->addAction(QStringLiteral("Dial Meter"), this, [this]() { addNewItem(QStringLiteral("DIAL")); });
    meters->addAction(QStringLiteral("History Graph"), this, [this]() { addNewItem(QStringLiteral("HISTORY")); });
    meters->addAction(QStringLiteral("Magic Eye"), this, [this]() { addNewItem(QStringLiteral("MAGICEYE")); });
    meters->addAction(QStringLiteral("Signal Text"), this, [this]() { addNewItem(QStringLiteral("SIGNALTEXT")); });
    meters->addAction(QStringLiteral("Power Scale"), this, [this]() { addNewItem(QStringLiteral("NEEDLESCALEPWR")); });

    // --- Indicators ---
    QMenu* indicators = menu.addMenu(QStringLiteral("Indicators"));
    indicators->addAction(QStringLiteral("LED Indicator"), this, [this]() { addNewItem(QStringLiteral("LED")); });
    indicators->addAction(QStringLiteral("Text Readout"), this, [this]() { addNewItem(QStringLiteral("TEXT")); });
    indicators->addAction(QStringLiteral("Clock"), this, [this]() { addNewItem(QStringLiteral("CLOCK")); });
    indicators->addAction(QStringLiteral("Text Overlay"), this, [this]() { addNewItem(QStringLiteral("TEXTOVERLAY")); });

    // --- Controls ---
    QMenu* controls = menu.addMenu(QStringLiteral("Controls"));
    controls->addAction(QStringLiteral("Band Buttons"), this, [this]() { addNewItem(QStringLiteral("BANDBTNS")); });
    controls->addAction(QStringLiteral("Mode Buttons"), this, [this]() { addNewItem(QStringLiteral("MODEBTNS")); });
    controls->addAction(QStringLiteral("Filter Buttons"), this, [this]() { addNewItem(QStringLiteral("FILTERBTNS")); });
    controls->addAction(QStringLiteral("Antenna Buttons"), this, [this]() { addNewItem(QStringLiteral("ANTENNABTNS")); });
    controls->addAction(QStringLiteral("Tune Step Buttons"), this, [this]() { addNewItem(QStringLiteral("TUNESTEPBTNS")); });
    controls->addAction(QStringLiteral("Other Buttons"), this, [this]() { addNewItem(QStringLiteral("OTHERBTNS")); });
    controls->addAction(QStringLiteral("Voice Rec/Play"), this, [this]() { addNewItem(QStringLiteral("VOICERECPLAY")); });
    controls->addAction(QStringLiteral("VFO Display"), this, [this]() { addNewItem(QStringLiteral("VFO")); });
    controls->addAction(QStringLiteral("Discord Buttons"), this, [this]() { addNewItem(QStringLiteral("DISCORDBTNS")); });

    // --- Display ---
    QMenu* display = menu.addMenu(QStringLiteral("Display"));
    display->addAction(QStringLiteral("Filter Display"), this, [this]() { addNewItem(QStringLiteral("FILTERDISPLAY")); });
    display->addAction(QStringLiteral("Rotator"), this, [this]() { addNewItem(QStringLiteral("ROTATOR")); });

    // --- Layout ---
    QMenu* layoutMenu = menu.addMenu(QStringLiteral("Layout"));
    layoutMenu->addAction(QStringLiteral("Solid Color"), this, [this]() { addNewItem(QStringLiteral("SOLID")); });
    layoutMenu->addAction(QStringLiteral("Image"), this, [this]() { addNewItem(QStringLiteral("IMAGE")); });
    layoutMenu->addAction(QStringLiteral("Web Image"), this, [this]() { addNewItem(QStringLiteral("WEBIMAGE")); });
    layoutMenu->addAction(QStringLiteral("Spacer"), this, [this]() { addNewItem(QStringLiteral("SPACER")); });
    layoutMenu->addAction(QStringLiteral("Fade Cover"), this, [this]() { addNewItem(QStringLiteral("FADECOVER")); });
    layoutMenu->addAction(QStringLiteral("Click Box"), this, [this]() { addNewItem(QStringLiteral("CLICKBOX")); });
    layoutMenu->addAction(QStringLiteral("Scale"), this, [this]() { addNewItem(QStringLiteral("SCALE")); });

    // --- Data ---
    QMenu* data = menu.addMenu(QStringLiteral("Data"));
    data->addAction(QStringLiteral("Data Output"), this, [this]() { addNewItem(QStringLiteral("DATAOUT")); });

    menu.exec(m_btnAdd->mapToGlobal(QPoint(0, m_btnAdd->height())));
}
```

- [ ] **Step 2: Add the addNewItem() helper**

Add declaration to header `private:` section:

```cpp
    void addNewItem(const QString& typeTag);
```

Implementation — creates item with sensible defaults, inserts after selection:

```cpp
void ContainerSettingsDialog::addNewItem(const QString& typeTag)
{
    // Create a default item of the requested type.
    // Each type gets sensible initial values for position and properties.
    MeterItem* item = nullptr;

    // Default position: stack below existing items, full width
    const int count = m_workingItems.size();
    // Each new item gets a vertical slice: 0.0 to 1.0 spread evenly
    float yPos = (count > 0) ? 0.0f : 0.0f;
    float hgt = 1.0f;

    // For stacking: place new item at bottom of existing stack
    if (count > 0) {
        // Find the lowest y+h in existing items
        float maxBottom = 0.0f;
        for (const MeterItem* existing : m_workingItems) {
            float bottom = existing->y() + existing->itemHeight();
            if (bottom > maxBottom) { maxBottom = bottom; }
        }
        yPos = qMin(maxBottom, 0.9f);
        hgt = 1.0f - yPos;
    }

    if (typeTag == QLatin1String("BAR")) {
        auto* bar = new BarItem();
        bar->setRect(0.0f, yPos, 1.0f, qMin(hgt, 0.15f));
        bar->setRange(-140.0, 0.0);
        bar->setBindingId(0);  // SignalPeak default
        item = bar;
    } else if (typeTag == QLatin1String("NEEDLE")) {
        auto* needle = new NeedleItem();
        needle->setRect(0.0f, yPos, 1.0f, qMin(hgt, 0.5f));
        needle->setBindingId(0);
        item = needle;
    } else if (typeTag == QLatin1String("SOLID")) {
        auto* solid = new SolidColourItem();
        solid->setRect(0.0f, 0.0f, 1.0f, 1.0f);
        solid->setZOrder(-10);
        item = solid;
    } else if (typeTag == QLatin1String("TEXT")) {
        auto* text = new TextItem();
        text->setRect(0.0f, yPos, 1.0f, qMin(hgt, 0.1f));
        text->setBindingId(0);
        item = text;
    } else if (typeTag == QLatin1String("SCALE")) {
        auto* scale = new ScaleItem();
        scale->setRect(0.0f, yPos, 1.0f, qMin(hgt, 0.12f));
        item = scale;
    } else if (typeTag == QLatin1String("IMAGE")) {
        auto* img = new ImageItem();
        img->setRect(0.0f, 0.0f, 1.0f, 1.0f);
        img->setZOrder(-5);
        item = img;
    } else {
        // For all other types, create via serialize/deserialize with default data.
        // The type's default constructor provides sensible defaults.
        // We just need to set position.
        item = createDefaultItem(typeTag);
        if (item) {
            item->setRect(0.0f, yPos, 1.0f, qMin(hgt, 0.2f));
        }
    }

    if (!item) { return; }

    // Insert after current selection, or at end
    int insertIdx = m_itemList->currentRow() + 1;
    if (insertIdx <= 0 || insertIdx > m_workingItems.size()) {
        insertIdx = m_workingItems.size();
    }
    m_workingItems.insert(insertIdx, item);
    refreshItemList();
    m_itemList->setCurrentRow(insertIdx);
    updatePreview();
}
```

- [ ] **Step 3: Add createDefaultItem() helper**

Add declaration to header:

```cpp
    static MeterItem* createDefaultItem(const QString& typeTag);
```

Implementation — uses the type tag to create a default-constructed item:

```cpp
MeterItem* ContainerSettingsDialog::createDefaultItem(const QString& typeTag)
{
    // Create a default-constructed item for types not special-cased in addNewItem().
    if (typeTag == QLatin1String("SPACER"))         { return new SpacerItem(); }
    if (typeTag == QLatin1String("FADECOVER"))       { return new FadeCoverItem(); }
    if (typeTag == QLatin1String("LED"))            { return new LEDItem(); }
    if (typeTag == QLatin1String("HISTORY"))        { return new HistoryGraphItem(); }
    if (typeTag == QLatin1String("MAGICEYE"))       { return new MagicEyeItem(); }
    if (typeTag == QLatin1String("NEEDLESCALEPWR")) { return new NeedleScalePwrItem(); }
    if (typeTag == QLatin1String("SIGNALTEXT"))     { return new SignalTextItem(); }
    if (typeTag == QLatin1String("DIAL"))           { return new DialItem(); }
    if (typeTag == QLatin1String("TEXTOVERLAY"))    { return new TextOverlayItem(); }
    if (typeTag == QLatin1String("WEBIMAGE"))       { return new WebImageItem(); }
    if (typeTag == QLatin1String("FILTERDISPLAY"))  { return new FilterDisplayItem(); }
    if (typeTag == QLatin1String("ROTATOR"))        { return new RotatorItem(); }
    if (typeTag == QLatin1String("BANDBTNS"))       { return new BandButtonItem(); }
    if (typeTag == QLatin1String("MODEBTNS"))       { return new ModeButtonItem(); }
    if (typeTag == QLatin1String("FILTERBTNS"))     { return new FilterButtonItem(); }
    if (typeTag == QLatin1String("ANTENNABTNS"))    { return new AntennaButtonItem(); }
    if (typeTag == QLatin1String("TUNESTEPBTNS"))   { return new TuneStepButtonItem(); }
    if (typeTag == QLatin1String("OTHERBTNS"))      { return new OtherButtonItem(); }
    if (typeTag == QLatin1String("VOICERECPLAY"))   { return new VoiceRecordPlayItem(); }
    if (typeTag == QLatin1String("DISCORDBTNS"))    { return new DiscordButtonItem(); }
    if (typeTag == QLatin1String("VFO"))            { return new VfoDisplayItem(); }
    if (typeTag == QLatin1String("CLOCK"))          { return new ClockItem(); }
    if (typeTag == QLatin1String("CLICKBOX"))       { return new ClickBoxItem(); }
    if (typeTag == QLatin1String("DATAOUT"))        { return new DataOutItem(); }
    return nullptr;
}
```

- [ ] **Step 4: Implement Remove and Move Up/Down**

```cpp
void ContainerSettingsDialog::onRemoveItem()
{
    int row = m_itemList->currentRow();
    if (row < 0 || row >= m_workingItems.size()) { return; }

    delete m_workingItems.takeAt(row);
    refreshItemList();

    // Select nearest remaining item
    if (m_workingItems.size() > 0) {
        m_itemList->setCurrentRow(qMin(row, m_workingItems.size() - 1));
    }
    updatePreview();
}

void ContainerSettingsDialog::onMoveItemUp()
{
    int row = m_itemList->currentRow();
    if (row <= 0 || row >= m_workingItems.size()) { return; }

    m_workingItems.swapItemsAt(row, row - 1);
    refreshItemList();
    m_itemList->setCurrentRow(row - 1);
    updatePreview();
}

void ContainerSettingsDialog::onMoveItemDown()
{
    int row = m_itemList->currentRow();
    if (row < 0 || row >= m_workingItems.size() - 1) { return; }

    m_workingItems.swapItemsAt(row, row + 1);
    refreshItemList();
    m_itemList->setCurrentRow(row + 1);
    updatePreview();
}
```

- [ ] **Step 5: Build and verify**

Run: `cd /Users/j.j.boyd/NereusSDR && cmake --build build -j$(sysctl -n hw.ncpu) 2>&1 | tail -5`
Expected: Clean compile.

- [ ] **Step 6: Commit**

```bash
git add src/gui/containers/ContainerSettingsDialog.h \
        src/gui/containers/ContainerSettingsDialog.cpp
git commit -m "feat(3G-6): add/remove/reorder items with categorized picker

6-category Add picker (Meters, Indicators, Controls, Display, Layout,
Data) matching design spec. Default item positioning stacks below
existing items. Move up/down reorders z-order."
```

---

### Task 4: Common Property Editor (Position, Z-Order, Binding)

**Files:**
- Modify: `src/gui/containers/ContainerSettingsDialog.h`
- Modify: `src/gui/containers/ContainerSettingsDialog.cpp`

All items share common properties: position (x/y/w/h), z-order, and binding ID. This task builds the common property editor that appears for every item type, with type-specific editors added in Task 5.

- [ ] **Step 1: Add property editor members to header**

Add to `private:` section:

```cpp
    // Property editor — common controls (shared by all item types)
    QWidget* m_commonPropsPage{nullptr};
    QDoubleSpinBox* m_propX{nullptr};
    QDoubleSpinBox* m_propY{nullptr};
    QDoubleSpinBox* m_propW{nullptr};
    QDoubleSpinBox* m_propH{nullptr};
    QSpinBox* m_propZOrder{nullptr};
    QComboBox* m_propBinding{nullptr};

    // Type-specific property area within the common page
    QWidget* m_typePropsContainer{nullptr};
    QVBoxLayout* m_typePropsLayout{nullptr};

    // Currently displayed type-specific editor widget (owned by m_typePropsContainer)
    QWidget* m_currentTypeEditor{nullptr};

    void buildCommonPropsPage();
    void loadCommonProperties(int row);
    void saveCommonProperties(int row);
    QWidget* buildTypeSpecificEditor(MeterItem* item);
    void populateBindingCombo();
```

Add `#include <QDoubleSpinBox>` to the header forward declarations:

```cpp
class QDoubleSpinBox;
```

- [ ] **Step 2: Implement buildCommonPropsPage()**

```cpp
void ContainerSettingsDialog::buildCommonPropsPage()
{
    m_commonPropsPage = new QWidget(m_propertyStack);
    auto* layout = new QVBoxLayout(m_commonPropsPage);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(6);

    const QString labelStyle = QStringLiteral("color: #8090a0; font-size: 12px;");
    const QString spinStyle = QStringLiteral(
        "QDoubleSpinBox, QSpinBox { background: #0a0a18; color: #c8d8e8;"
        " border: 1px solid #1e2e3e; border-radius: 3px; padding: 2px; }"
        "QComboBox { background: #0a0a18; color: #c8d8e8;"
        " border: 1px solid #1e2e3e; border-radius: 3px; padding: 2px; }");

    // --- Position section ---
    auto* posLabel = new QLabel(QStringLiteral("Position (normalized 0-1)"), m_commonPropsPage);
    posLabel->setStyleSheet(QStringLiteral("color: #8aa8c0; font-weight: bold; font-size: 12px;"));
    layout->addWidget(posLabel);

    auto* posForm = new QFormLayout();
    posForm->setSpacing(4);

    auto makeNormSpin = [&](const QString& name) -> QDoubleSpinBox* {
        auto* spin = new QDoubleSpinBox(m_commonPropsPage);
        spin->setRange(0.0, 1.0);
        spin->setSingleStep(0.01);
        spin->setDecimals(3);
        spin->setStyleSheet(spinStyle);
        return spin;
    };

    m_propX = makeNormSpin(QStringLiteral("X"));
    m_propY = makeNormSpin(QStringLiteral("Y"));
    m_propW = makeNormSpin(QStringLiteral("W"));
    m_propH = makeNormSpin(QStringLiteral("H"));

    auto* xLabel = new QLabel(QStringLiteral("X:"), m_commonPropsPage);
    xLabel->setStyleSheet(labelStyle);
    posForm->addRow(xLabel, m_propX);
    auto* yLabel = new QLabel(QStringLiteral("Y:"), m_commonPropsPage);
    yLabel->setStyleSheet(labelStyle);
    posForm->addRow(yLabel, m_propY);
    auto* wLabel = new QLabel(QStringLiteral("W:"), m_commonPropsPage);
    wLabel->setStyleSheet(labelStyle);
    posForm->addRow(wLabel, m_propW);
    auto* hLabel = new QLabel(QStringLiteral("H:"), m_commonPropsPage);
    hLabel->setStyleSheet(labelStyle);
    posForm->addRow(hLabel, m_propH);
    layout->addLayout(posForm);

    // --- Z-Order ---
    auto* zForm = new QFormLayout();
    m_propZOrder = new QSpinBox(m_commonPropsPage);
    m_propZOrder->setRange(-100, 100);
    m_propZOrder->setStyleSheet(spinStyle);
    auto* zLabel = new QLabel(QStringLiteral("Z-Order:"), m_commonPropsPage);
    zLabel->setStyleSheet(labelStyle);
    zForm->addRow(zLabel, m_propZOrder);
    layout->addLayout(zForm);

    // --- Binding ---
    auto* bindForm = new QFormLayout();
    m_propBinding = new QComboBox(m_commonPropsPage);
    m_propBinding->setStyleSheet(spinStyle);
    populateBindingCombo();
    auto* bindLabel = new QLabel(QStringLiteral("Binding:"), m_commonPropsPage);
    bindLabel->setStyleSheet(labelStyle);
    bindForm->addRow(bindLabel, m_propBinding);
    layout->addLayout(bindForm);

    // --- Separator ---
    auto* sep = new QFrame(m_commonPropsPage);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet(QStringLiteral("color: #203040;"));
    layout->addWidget(sep);

    // --- Type-specific property area ---
    m_typePropsContainer = new QWidget(m_commonPropsPage);
    m_typePropsLayout = new QVBoxLayout(m_typePropsContainer);
    m_typePropsLayout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_typePropsContainer);

    layout->addStretch();

    // Connect spin boxes to live update
    auto updateFromProps = [this]() {
        int row = m_itemList->currentRow();
        if (row >= 0 && row < m_workingItems.size()) {
            saveCommonProperties(row);
            updatePreview();
        }
    };
    connect(m_propX, &QDoubleSpinBox::valueChanged, this, updateFromProps);
    connect(m_propY, &QDoubleSpinBox::valueChanged, this, updateFromProps);
    connect(m_propW, &QDoubleSpinBox::valueChanged, this, updateFromProps);
    connect(m_propH, &QDoubleSpinBox::valueChanged, this, updateFromProps);
    connect(m_propZOrder, &QSpinBox::valueChanged, this, updateFromProps);
    connect(m_propBinding, &QComboBox::currentIndexChanged, this, updateFromProps);

    m_propertyStack->addWidget(m_commonPropsPage);
}
```

- [ ] **Step 3: Implement populateBindingCombo()**

```cpp
void ContainerSettingsDialog::populateBindingCombo()
{
    // From MeterPoller.h MeterBinding namespace — all known binding IDs
    m_propBinding->addItem(QStringLiteral("None"), -1);
    m_propBinding->addItem(QStringLiteral("Signal Peak"), 0);
    m_propBinding->addItem(QStringLiteral("Signal Avg"), 1);
    m_propBinding->addItem(QStringLiteral("ADC Peak"), 2);
    m_propBinding->addItem(QStringLiteral("ADC Avg"), 3);
    m_propBinding->addItem(QStringLiteral("AGC Gain"), 4);
    m_propBinding->addItem(QStringLiteral("AGC Peak"), 5);
    m_propBinding->addItem(QStringLiteral("AGC Avg"), 6);
    m_propBinding->addItem(QStringLiteral("Signal MaxBin"), 7);
    m_propBinding->addItem(QStringLiteral("PBSNR"), 8);
    m_propBinding->addItem(QStringLiteral("TX Power"), 100);
    m_propBinding->addItem(QStringLiteral("TX Reverse Power"), 101);
    m_propBinding->addItem(QStringLiteral("TX SWR"), 102);
    m_propBinding->addItem(QStringLiteral("TX Mic"), 103);
    m_propBinding->addItem(QStringLiteral("TX Comp"), 104);
    m_propBinding->addItem(QStringLiteral("TX ALC"), 105);
    m_propBinding->addItem(QStringLiteral("TX EQ"), 106);
    m_propBinding->addItem(QStringLiteral("TX Leveler"), 107);
    m_propBinding->addItem(QStringLiteral("TX Leveler Gain"), 108);
    m_propBinding->addItem(QStringLiteral("TX ALC Gain"), 109);
    m_propBinding->addItem(QStringLiteral("TX ALC Group"), 110);
    m_propBinding->addItem(QStringLiteral("TX CFC"), 111);
    m_propBinding->addItem(QStringLiteral("TX CFC Gain"), 112);
    m_propBinding->addItem(QStringLiteral("HW Volts"), 200);
    m_propBinding->addItem(QStringLiteral("HW Amps"), 201);
    m_propBinding->addItem(QStringLiteral("HW Temperature"), 202);
    m_propBinding->addItem(QStringLiteral("Rotator Az"), 300);
    m_propBinding->addItem(QStringLiteral("Rotator Ele"), 301);
}
```

- [ ] **Step 4: Implement load/save common properties and selection handler**

```cpp
void ContainerSettingsDialog::loadCommonProperties(int row)
{
    if (row < 0 || row >= m_workingItems.size()) { return; }

    MeterItem* item = m_workingItems[row];

    // Block signals during load to avoid feedback
    QSignalBlocker xBlock(m_propX);
    QSignalBlocker yBlock(m_propY);
    QSignalBlocker wBlock(m_propW);
    QSignalBlocker hBlock(m_propH);
    QSignalBlocker zBlock(m_propZOrder);
    QSignalBlocker bBlock(m_propBinding);

    m_propX->setValue(item->x());
    m_propY->setValue(item->y());
    m_propW->setValue(item->itemWidth());
    m_propH->setValue(item->itemHeight());
    m_propZOrder->setValue(item->zOrder());

    // Find matching binding in combo
    int bindIdx = m_propBinding->findData(item->bindingId());
    m_propBinding->setCurrentIndex(bindIdx >= 0 ? bindIdx : 0);
}

void ContainerSettingsDialog::saveCommonProperties(int row)
{
    if (row < 0 || row >= m_workingItems.size()) { return; }

    MeterItem* item = m_workingItems[row];
    item->setRect(
        static_cast<float>(m_propX->value()),
        static_cast<float>(m_propY->value()),
        static_cast<float>(m_propW->value()),
        static_cast<float>(m_propH->value())
    );
    item->setZOrder(m_propZOrder->value());
    item->setBindingId(m_propBinding->currentData().toInt());
}

void ContainerSettingsDialog::onItemSelectionChanged()
{
    int row = m_itemList->currentRow();
    if (row < 0 || row >= m_workingItems.size()) {
        m_propertyStack->setCurrentWidget(m_emptyPage);
        return;
    }

    // Ensure common props page exists
    if (!m_commonPropsPage) {
        buildCommonPropsPage();
    }

    m_propertyStack->setCurrentWidget(m_commonPropsPage);
    loadCommonProperties(row);

    // Replace type-specific editor
    if (m_currentTypeEditor) {
        m_typePropsLayout->removeWidget(m_currentTypeEditor);
        delete m_currentTypeEditor;
        m_currentTypeEditor = nullptr;
    }

    m_currentTypeEditor = buildTypeSpecificEditor(m_workingItems[row]);
    if (m_currentTypeEditor) {
        m_typePropsLayout->addWidget(m_currentTypeEditor);
    }
}
```

- [ ] **Step 5: Add stub for buildTypeSpecificEditor()**

```cpp
QWidget* ContainerSettingsDialog::buildTypeSpecificEditor(MeterItem* item)
{
    Q_UNUSED(item);
    // Filled in Task 5 with per-type property forms
    return nullptr;
}
```

- [ ] **Step 6: Call buildCommonPropsPage() from buildLayout(), after creating the stack**

Add after the `m_propertyStack->addWidget(m_emptyPage);` line in `buildCenterPanel()`:

```cpp
    buildCommonPropsPage();
```

- [ ] **Step 7: Build and verify**

Run: `cd /Users/j.j.boyd/NereusSDR && cmake --build build -j$(sysctl -n hw.ncpu) 2>&1 | tail -5`
Expected: Clean compile.

- [ ] **Step 8: Commit**

```bash
git add src/gui/containers/ContainerSettingsDialog.h \
        src/gui/containers/ContainerSettingsDialog.cpp
git commit -m "feat(3G-6): common property editor with position, z-order, binding

Normalized position spinboxes (0-1), z-order spinner (-100 to 100),
binding ID dropdown with all MeterBinding values. Live preview
updates as properties change."
```

---

### Task 5: Type-Specific Property Editors

**Files:**
- Modify: `src/gui/containers/ContainerSettingsDialog.cpp`

Replace the `buildTypeSpecificEditor()` stub with per-type property forms. Each type gets only its most important user-editable properties. The common properties (position, z-order, binding) are already handled in Task 4.

- [ ] **Step 1: Implement buildTypeSpecificEditor() with BarItem editor**

```cpp
QWidget* ContainerSettingsDialog::buildTypeSpecificEditor(MeterItem* item)
{
    if (!item) { return nullptr; }

    QString tag = item->serialize().section(QLatin1Char('|'), 0, 0);

    const QString labelStyle = QStringLiteral("color: #8090a0; font-size: 12px;");
    const QString spinStyle = QStringLiteral(
        "QDoubleSpinBox, QSpinBox { background: #0a0a18; color: #c8d8e8;"
        " border: 1px solid #1e2e3e; border-radius: 3px; padding: 2px; }"
        "QComboBox { background: #0a0a18; color: #c8d8e8;"
        " border: 1px solid #1e2e3e; border-radius: 3px; padding: 2px; }");
    const QString checkStyle = QStringLiteral("color: #c8d8e8;");
    const QString editStyle = QStringLiteral(
        "background: #0a0a18; color: #c8d8e8; border: 1px solid #1e2e3e;"
        " border-radius: 3px; padding: 2px 4px;");

    if (tag == QLatin1String("BAR")) {
        return buildBarItemEditor(static_cast<BarItem*>(item));
    } else if (tag == QLatin1String("NEEDLE")) {
        return buildNeedleItemEditor(static_cast<NeedleItem*>(item));
    } else if (tag == QLatin1String("TEXT")) {
        return buildTextItemEditor(static_cast<TextItem*>(item));
    } else if (tag == QLatin1String("SCALE")) {
        return buildScaleItemEditor(static_cast<ScaleItem*>(item));
    } else if (tag == QLatin1String("SOLID")) {
        return buildSolidItemEditor(static_cast<SolidColourItem*>(item));
    } else if (tag == QLatin1String("LED")) {
        return buildLedItemEditor(static_cast<LEDItem*>(item));
    }

    // Types with no extra editable properties beyond common props
    return nullptr;
}
```

- [ ] **Step 2: Add editor builder declarations to header**

```cpp
    QWidget* buildBarItemEditor(BarItem* item);
    QWidget* buildNeedleItemEditor(NeedleItem* item);
    QWidget* buildTextItemEditor(TextItem* item);
    QWidget* buildScaleItemEditor(ScaleItem* item);
    QWidget* buildSolidItemEditor(SolidColourItem* item);
    QWidget* buildLedItemEditor(LEDItem* item);
```

- [ ] **Step 3: Implement buildBarItemEditor()**

```cpp
QWidget* ContainerSettingsDialog::buildBarItemEditor(BarItem* item)
{
    auto* widget = new QWidget();
    auto* form = new QFormLayout(widget);
    form->setSpacing(4);

    const QString labelStyle = QStringLiteral("color: #8090a0; font-size: 12px;");
    const QString spinStyle = QStringLiteral(
        "QDoubleSpinBox, QSpinBox, QComboBox { background: #0a0a18; color: #c8d8e8;"
        " border: 1px solid #1e2e3e; border-radius: 3px; padding: 2px; }");

    auto* header = new QLabel(QStringLiteral("Bar Meter Properties"), widget);
    header->setStyleSheet(QStringLiteral("color: #8aa8c0; font-weight: bold; font-size: 12px;"));
    form->addRow(header);

    // Range min/max
    auto* minSpin = new QDoubleSpinBox(widget);
    minSpin->setRange(-200.0, 200.0);
    minSpin->setValue(item->minVal());
    minSpin->setStyleSheet(spinStyle);
    auto* minLabel = new QLabel(QStringLiteral("Min:"), widget);
    minLabel->setStyleSheet(labelStyle);
    form->addRow(minLabel, minSpin);

    auto* maxSpin = new QDoubleSpinBox(widget);
    maxSpin->setRange(-200.0, 200.0);
    maxSpin->setValue(item->maxVal());
    maxSpin->setStyleSheet(spinStyle);
    auto* maxLabel = new QLabel(QStringLiteral("Max:"), widget);
    maxLabel->setStyleSheet(labelStyle);
    form->addRow(maxLabel, maxSpin);

    // Red threshold
    auto* redSpin = new QDoubleSpinBox(widget);
    redSpin->setRange(-200.0, 1000.0);
    redSpin->setValue(item->redThreshold());
    redSpin->setStyleSheet(spinStyle);
    auto* redLabel = new QLabel(QStringLiteral("Red threshold:"), widget);
    redLabel->setStyleSheet(labelStyle);
    form->addRow(redLabel, redSpin);

    // Attack / Decay
    auto* attackSpin = new QDoubleSpinBox(widget);
    attackSpin->setRange(0.0, 1.0);
    attackSpin->setSingleStep(0.05);
    attackSpin->setDecimals(2);
    attackSpin->setValue(item->attackRatio());
    attackSpin->setStyleSheet(spinStyle);
    auto* attackLabel = new QLabel(QStringLiteral("Attack:"), widget);
    attackLabel->setStyleSheet(labelStyle);
    form->addRow(attackLabel, attackSpin);

    auto* decaySpin = new QDoubleSpinBox(widget);
    decaySpin->setRange(0.0, 1.0);
    decaySpin->setSingleStep(0.05);
    decaySpin->setDecimals(2);
    decaySpin->setValue(item->decayRatio());
    decaySpin->setStyleSheet(spinStyle);
    auto* decayLabel = new QLabel(QStringLiteral("Decay:"), widget);
    decayLabel->setStyleSheet(labelStyle);
    form->addRow(decayLabel, decaySpin);

    // Bar style (Filled/Edge)
    auto* styleCombo = new QComboBox(widget);
    styleCombo->addItem(QStringLiteral("Filled"), static_cast<int>(BarItem::BarStyle::Filled));
    styleCombo->addItem(QStringLiteral("Edge"), static_cast<int>(BarItem::BarStyle::Edge));
    styleCombo->setCurrentIndex(static_cast<int>(item->barStyle()));
    styleCombo->setStyleSheet(spinStyle);
    auto* styleLabel = new QLabel(QStringLiteral("Style:"), widget);
    styleLabel->setStyleSheet(labelStyle);
    form->addRow(styleLabel, styleCombo);

    // Color button
    auto* colorBtn = new QPushButton(widget);
    colorBtn->setFixedSize(40, 22);
    colorBtn->setAutoDefault(false);
    colorBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; border: 1px solid #205070; border-radius: 3px; }")
        .arg(item->barColor().name()));
    auto* colorLabel = new QLabel(QStringLiteral("Color:"), widget);
    colorLabel->setStyleSheet(labelStyle);
    form->addRow(colorLabel, colorBtn);

    // Connect to live update
    connect(minSpin, &QDoubleSpinBox::valueChanged, this, [this, item](double v) {
        item->setRange(v, item->maxVal());
        updatePreview();
    });
    connect(maxSpin, &QDoubleSpinBox::valueChanged, this, [this, item](double v) {
        item->setRange(item->minVal(), v);
        updatePreview();
    });
    connect(redSpin, &QDoubleSpinBox::valueChanged, this, [this, item](double v) {
        item->setRedThreshold(v);
        updatePreview();
    });
    connect(attackSpin, &QDoubleSpinBox::valueChanged, this, [this, item](double v) {
        item->setAttackRatio(static_cast<float>(v));
        updatePreview();
    });
    connect(decaySpin, &QDoubleSpinBox::valueChanged, this, [this, item](double v) {
        item->setDecayRatio(static_cast<float>(v));
        updatePreview();
    });
    connect(styleCombo, &QComboBox::currentIndexChanged, this, [this, item](int idx) {
        item->setBarStyle(static_cast<BarItem::BarStyle>(idx));
        updatePreview();
    });
    connect(colorBtn, &QPushButton::clicked, this, [this, item, colorBtn]() {
        QColor c = QColorDialog::getColor(item->barColor(), this, QStringLiteral("Bar Color"));
        if (c.isValid()) {
            item->setBarColor(c);
            colorBtn->setStyleSheet(QStringLiteral(
                "QPushButton { background: %1; border: 1px solid #205070; border-radius: 3px; }")
                .arg(c.name()));
            updatePreview();
        }
    });

    return widget;
}
```

- [ ] **Step 4: Implement buildTextItemEditor()**

```cpp
QWidget* ContainerSettingsDialog::buildTextItemEditor(TextItem* item)
{
    auto* widget = new QWidget();
    auto* form = new QFormLayout(widget);
    form->setSpacing(4);

    const QString labelStyle = QStringLiteral("color: #8090a0; font-size: 12px;");
    const QString editStyle = QStringLiteral(
        "background: #0a0a18; color: #c8d8e8; border: 1px solid #1e2e3e;"
        " border-radius: 3px; padding: 2px 4px;");
    const QString spinStyle = QStringLiteral(
        "QSpinBox { background: #0a0a18; color: #c8d8e8;"
        " border: 1px solid #1e2e3e; border-radius: 3px; padding: 2px; }");

    auto* header = new QLabel(QStringLiteral("Text Readout Properties"), widget);
    header->setStyleSheet(QStringLiteral("color: #8aa8c0; font-weight: bold; font-size: 12px;"));
    form->addRow(header);

    auto* labelEdit = new QLineEdit(item->label(), widget);
    labelEdit->setStyleSheet(editStyle);
    auto* lbl = new QLabel(QStringLiteral("Label:"), widget);
    lbl->setStyleSheet(labelStyle);
    form->addRow(lbl, labelEdit);

    auto* suffixEdit = new QLineEdit(item->suffix(), widget);
    suffixEdit->setStyleSheet(editStyle);
    auto* suffLbl = new QLabel(QStringLiteral("Suffix:"), widget);
    suffLbl->setStyleSheet(labelStyle);
    form->addRow(suffLbl, suffixEdit);

    auto* fontSpin = new QSpinBox(widget);
    fontSpin->setRange(8, 48);
    fontSpin->setValue(item->fontSize());
    fontSpin->setStyleSheet(spinStyle);
    auto* fontLbl = new QLabel(QStringLiteral("Font size:"), widget);
    fontLbl->setStyleSheet(labelStyle);
    form->addRow(fontLbl, fontSpin);

    auto* decSpin = new QSpinBox(widget);
    decSpin->setRange(0, 4);
    decSpin->setValue(item->decimals());
    decSpin->setStyleSheet(spinStyle);
    auto* decLbl = new QLabel(QStringLiteral("Decimals:"), widget);
    decLbl->setStyleSheet(labelStyle);
    form->addRow(decLbl, decSpin);

    auto* boldCheck = new QCheckBox(QStringLiteral("Bold"), widget);
    boldCheck->setChecked(item->bold());
    boldCheck->setStyleSheet(QStringLiteral("color: #c8d8e8;"));
    form->addRow(boldCheck);

    connect(labelEdit, &QLineEdit::textChanged, this, [this, item](const QString& t) {
        item->setLabel(t); updatePreview();
    });
    connect(suffixEdit, &QLineEdit::textChanged, this, [this, item](const QString& t) {
        item->setSuffix(t); updatePreview();
    });
    connect(fontSpin, &QSpinBox::valueChanged, this, [this, item](int v) {
        item->setFontSize(v); updatePreview();
    });
    connect(decSpin, &QSpinBox::valueChanged, this, [this, item](int v) {
        item->setDecimals(v); updatePreview();
    });
    connect(boldCheck, &QCheckBox::toggled, this, [this, item](bool v) {
        item->setBold(v); updatePreview();
    });

    return widget;
}
```

- [ ] **Step 5: Implement buildScaleItemEditor()**

```cpp
QWidget* ContainerSettingsDialog::buildScaleItemEditor(ScaleItem* item)
{
    auto* widget = new QWidget();
    auto* form = new QFormLayout(widget);
    form->setSpacing(4);

    const QString labelStyle = QStringLiteral("color: #8090a0; font-size: 12px;");
    const QString spinStyle = QStringLiteral(
        "QDoubleSpinBox, QSpinBox { background: #0a0a18; color: #c8d8e8;"
        " border: 1px solid #1e2e3e; border-radius: 3px; padding: 2px; }");

    auto* header = new QLabel(QStringLiteral("Scale Properties"), widget);
    header->setStyleSheet(QStringLiteral("color: #8aa8c0; font-weight: bold; font-size: 12px;"));
    form->addRow(header);

    auto* minSpin = new QDoubleSpinBox(widget);
    minSpin->setRange(-200.0, 200.0);
    minSpin->setValue(item->minVal());
    minSpin->setStyleSheet(spinStyle);
    form->addRow(new QLabel(QStringLiteral("Min:"), widget), minSpin);

    auto* maxSpin = new QDoubleSpinBox(widget);
    maxSpin->setRange(-200.0, 200.0);
    maxSpin->setValue(item->maxVal());
    maxSpin->setStyleSheet(spinStyle);
    form->addRow(new QLabel(QStringLiteral("Max:"), widget), maxSpin);

    auto* majorSpin = new QSpinBox(widget);
    majorSpin->setRange(2, 20);
    majorSpin->setValue(item->majorTicks());
    majorSpin->setStyleSheet(spinStyle);
    form->addRow(new QLabel(QStringLiteral("Major ticks:"), widget), majorSpin);

    auto* minorSpin = new QSpinBox(widget);
    minorSpin->setRange(0, 10);
    minorSpin->setValue(item->minorTicks());
    minorSpin->setStyleSheet(spinStyle);
    form->addRow(new QLabel(QStringLiteral("Minor ticks:"), widget), minorSpin);

    connect(minSpin, &QDoubleSpinBox::valueChanged, this, [this, item](double v) {
        item->setRange(v, item->maxVal()); updatePreview();
    });
    connect(maxSpin, &QDoubleSpinBox::valueChanged, this, [this, item](double v) {
        item->setRange(item->minVal(), v); updatePreview();
    });
    connect(majorSpin, &QSpinBox::valueChanged, this, [this, item](int v) {
        item->setMajorTicks(v); updatePreview();
    });
    connect(minorSpin, &QSpinBox::valueChanged, this, [this, item](int v) {
        item->setMinorTicks(v); updatePreview();
    });

    return widget;
}
```

- [ ] **Step 6: Implement buildNeedleItemEditor()**

```cpp
QWidget* ContainerSettingsDialog::buildNeedleItemEditor(NeedleItem* item)
{
    auto* widget = new QWidget();
    auto* form = new QFormLayout(widget);
    form->setSpacing(4);

    const QString labelStyle = QStringLiteral("color: #8090a0; font-size: 12px;");
    const QString editStyle = QStringLiteral(
        "background: #0a0a18; color: #c8d8e8; border: 1px solid #1e2e3e;"
        " border-radius: 3px; padding: 2px 4px;");

    auto* header = new QLabel(QStringLiteral("Needle Meter Properties"), widget);
    header->setStyleSheet(QStringLiteral("color: #8aa8c0; font-weight: bold; font-size: 12px;"));
    form->addRow(header);

    auto* labelEdit = new QLineEdit(item->sourceLabel(), widget);
    labelEdit->setStyleSheet(editStyle);
    form->addRow(new QLabel(QStringLiteral("Label:"), widget), labelEdit);

    auto* colorBtn = new QPushButton(widget);
    colorBtn->setFixedSize(40, 22);
    colorBtn->setAutoDefault(false);
    colorBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; border: 1px solid #205070; border-radius: 3px; }")
        .arg(item->needleColor().name()));
    form->addRow(new QLabel(QStringLiteral("Needle color:"), widget), colorBtn);

    connect(labelEdit, &QLineEdit::textChanged, this, [this, item](const QString& t) {
        item->setSourceLabel(t); updatePreview();
    });
    connect(colorBtn, &QPushButton::clicked, this, [this, item, colorBtn]() {
        QColor c = QColorDialog::getColor(item->needleColor(), this, QStringLiteral("Needle Color"));
        if (c.isValid()) {
            item->setNeedleColor(c);
            colorBtn->setStyleSheet(QStringLiteral(
                "QPushButton { background: %1; border: 1px solid #205070; border-radius: 3px; }")
                .arg(c.name()));
            updatePreview();
        }
    });

    return widget;
}
```

- [ ] **Step 7: Implement buildSolidItemEditor() and buildLedItemEditor()**

```cpp
QWidget* ContainerSettingsDialog::buildSolidItemEditor(SolidColourItem* item)
{
    auto* widget = new QWidget();
    auto* form = new QFormLayout(widget);
    form->setSpacing(4);

    auto* header = new QLabel(QStringLiteral("Solid Color Properties"), widget);
    header->setStyleSheet(QStringLiteral("color: #8aa8c0; font-weight: bold; font-size: 12px;"));
    form->addRow(header);

    auto* colorBtn = new QPushButton(widget);
    colorBtn->setFixedSize(40, 22);
    colorBtn->setAutoDefault(false);
    colorBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; border: 1px solid #205070; border-radius: 3px; }")
        .arg(item->colour().name()));
    form->addRow(new QLabel(QStringLiteral("Color:"), widget), colorBtn);

    connect(colorBtn, &QPushButton::clicked, this, [this, item, colorBtn]() {
        QColor c = QColorDialog::getColor(item->colour(), this, QStringLiteral("Fill Color"));
        if (c.isValid()) {
            item->setColour(c);
            colorBtn->setStyleSheet(QStringLiteral(
                "QPushButton { background: %1; border: 1px solid #205070; border-radius: 3px; }")
                .arg(c.name()));
            updatePreview();
        }
    });

    return widget;
}

QWidget* ContainerSettingsDialog::buildLedItemEditor(LEDItem* item)
{
    auto* widget = new QWidget();
    auto* form = new QFormLayout(widget);
    form->setSpacing(4);

    const QString spinStyle = QStringLiteral(
        "QDoubleSpinBox { background: #0a0a18; color: #c8d8e8;"
        " border: 1px solid #1e2e3e; border-radius: 3px; padding: 2px; }");

    auto* header = new QLabel(QStringLiteral("LED Properties"), widget);
    header->setStyleSheet(QStringLiteral("color: #8aa8c0; font-weight: bold; font-size: 12px;"));
    form->addRow(header);

    auto* threshSpin = new QDoubleSpinBox(widget);
    threshSpin->setRange(-200.0, 200.0);
    threshSpin->setValue(item->threshold());
    threshSpin->setStyleSheet(spinStyle);
    form->addRow(new QLabel(QStringLiteral("Threshold:"), widget), threshSpin);

    auto* onColorBtn = new QPushButton(widget);
    onColorBtn->setFixedSize(40, 22);
    onColorBtn->setAutoDefault(false);
    onColorBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; border: 1px solid #205070; border-radius: 3px; }")
        .arg(item->onColor().name()));
    form->addRow(new QLabel(QStringLiteral("On color:"), widget), onColorBtn);

    auto* offColorBtn = new QPushButton(widget);
    offColorBtn->setFixedSize(40, 22);
    offColorBtn->setAutoDefault(false);
    offColorBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; border: 1px solid #205070; border-radius: 3px; }")
        .arg(item->offColor().name()));
    form->addRow(new QLabel(QStringLiteral("Off color:"), widget), offColorBtn);

    connect(threshSpin, &QDoubleSpinBox::valueChanged, this, [this, item](double v) {
        item->setThreshold(v); updatePreview();
    });
    connect(onColorBtn, &QPushButton::clicked, this, [this, item, onColorBtn]() {
        QColor c = QColorDialog::getColor(item->onColor(), this, QStringLiteral("LED On Color"));
        if (c.isValid()) {
            item->setOnColor(c);
            onColorBtn->setStyleSheet(QStringLiteral(
                "QPushButton { background: %1; border: 1px solid #205070; border-radius: 3px; }")
                .arg(c.name()));
            updatePreview();
        }
    });
    connect(offColorBtn, &QPushButton::clicked, this, [this, item, offColorBtn]() {
        QColor c = QColorDialog::getColor(item->offColor(), this, QStringLiteral("LED Off Color"));
        if (c.isValid()) {
            item->setOffColor(c);
            offColorBtn->setStyleSheet(QStringLiteral(
                "QPushButton { background: %1; border: 1px solid #205070; border-radius: 3px; }")
                .arg(c.name()));
            updatePreview();
        }
    });

    return widget;
}
```

- [ ] **Step 8: Build and verify**

Run: `cd /Users/j.j.boyd/NereusSDR && cmake --build build -j$(sysctl -n hw.ncpu) 2>&1 | tail -5`
Expected: Clean compile. If LEDItem::threshold()/onColor()/offColor() accessors don't exist, check the header and adjust method names to match.

- [ ] **Step 9: Commit**

```bash
git add src/gui/containers/ContainerSettingsDialog.h \
        src/gui/containers/ContainerSettingsDialog.cpp
git commit -m "feat(3G-6): type-specific property editors for 6 item types

Per-type editors for BarItem (range, threshold, attack/decay, style,
color), NeedleItem (label, color), TextItem (label, suffix, font,
decimals, bold), ScaleItem (range, ticks), SolidColourItem (color),
LEDItem (threshold, on/off colors). All with live preview update."
```

---

### Task 6: Preset Browser

**Files:**
- Modify: `src/gui/containers/ContainerSettingsDialog.h`
- Modify: `src/gui/containers/ContainerSettingsDialog.cpp`

- [ ] **Step 1: Add preset method declarations to header**

```cpp
    void onLoadPreset();
    void loadPresetByName(const QString& name);
```

- [ ] **Step 2: Implement the preset picker**

Wire `m_btnPreset` click in `buildButtonBar()` — add this connect after the existing button connects:

```cpp
    connect(m_btnPreset, &QPushButton::clicked, this, &ContainerSettingsDialog::onLoadPreset);
```

Implement the preset browser as a categorized menu matching the 30+ presets from the design spec:

```cpp
void ContainerSettingsDialog::onLoadPreset()
{
    QMenu menu(this);
    menu.setStyleSheet(QStringLiteral(
        "QMenu { background: #1a2a3a; color: #c8d8e8; border: 1px solid #205070; }"
        "QMenu::item:selected { background: #00b4d8; color: #0f0f1a; }"
        "QMenu::separator { background: #203040; height: 1px; }"));

    // S-Meter variants
    QMenu* sMeter = menu.addMenu(QStringLiteral("S-Meter"));
    sMeter->addAction(QStringLiteral("S-Meter Only"), this, [this]() { loadPresetByName(QStringLiteral("SMeterOnly")); });
    sMeter->addAction(QStringLiteral("S-Meter Bar (Signal)"), this, [this]() { loadPresetByName(QStringLiteral("SignalBar")); });
    sMeter->addAction(QStringLiteral("S-Meter Bar (Avg)"), this, [this]() { loadPresetByName(QStringLiteral("AvgSignalBar")); });
    sMeter->addAction(QStringLiteral("S-Meter Bar (MaxBin)"), this, [this]() { loadPresetByName(QStringLiteral("MaxBinBar")); });
    sMeter->addAction(QStringLiteral("S-Meter Text"), this, [this]() { loadPresetByName(QStringLiteral("SignalText")); });

    // Composite
    QMenu* composite = menu.addMenu(QStringLiteral("Composite"));
    composite->addAction(QStringLiteral("ANAN Multi Meter"), this, [this]() { loadPresetByName(QStringLiteral("AnanMM")); });
    composite->addAction(QStringLiteral("Cross Needle"), this, [this]() { loadPresetByName(QStringLiteral("CrossNeedle")); });
    composite->addAction(QStringLiteral("Magic Eye"), this, [this]() { loadPresetByName(QStringLiteral("MagicEye")); });
    composite->addAction(QStringLiteral("History"), this, [this]() { loadPresetByName(QStringLiteral("History")); });

    // TX Meters
    QMenu* tx = menu.addMenu(QStringLiteral("TX Meters"));
    tx->addAction(QStringLiteral("Power/SWR"), this, [this]() { loadPresetByName(QStringLiteral("PowerSwr")); });
    tx->addAction(QStringLiteral("ALC"), this, [this]() { loadPresetByName(QStringLiteral("Alc")); });
    tx->addAction(QStringLiteral("ALC Gain"), this, [this]() { loadPresetByName(QStringLiteral("AlcGain")); });
    tx->addAction(QStringLiteral("ALC Group"), this, [this]() { loadPresetByName(QStringLiteral("AlcGroup")); });
    tx->addAction(QStringLiteral("Mic Level"), this, [this]() { loadPresetByName(QStringLiteral("Mic")); });
    tx->addAction(QStringLiteral("Compressor"), this, [this]() { loadPresetByName(QStringLiteral("Comp")); });
    tx->addAction(QStringLiteral("EQ Level"), this, [this]() { loadPresetByName(QStringLiteral("Eq")); });
    tx->addAction(QStringLiteral("Leveler"), this, [this]() { loadPresetByName(QStringLiteral("Leveler")); });
    tx->addAction(QStringLiteral("Leveler Gain"), this, [this]() { loadPresetByName(QStringLiteral("LevelerGain")); });
    tx->addAction(QStringLiteral("CFC"), this, [this]() { loadPresetByName(QStringLiteral("Cfc")); });
    tx->addAction(QStringLiteral("CFC Gain"), this, [this]() { loadPresetByName(QStringLiteral("CfcGain")); });

    // RX Meters
    QMenu* rx = menu.addMenu(QStringLiteral("RX Meters"));
    rx->addAction(QStringLiteral("ADC"), this, [this]() { loadPresetByName(QStringLiteral("Adc")); });
    rx->addAction(QStringLiteral("ADC MaxMag"), this, [this]() { loadPresetByName(QStringLiteral("AdcMaxMag")); });
    rx->addAction(QStringLiteral("AGC"), this, [this]() { loadPresetByName(QStringLiteral("Agc")); });
    rx->addAction(QStringLiteral("AGC Gain"), this, [this]() { loadPresetByName(QStringLiteral("AgcGain")); });
    rx->addAction(QStringLiteral("PBSNR"), this, [this]() { loadPresetByName(QStringLiteral("Pbsnr")); });

    // Interactive
    QMenu* interactive = menu.addMenu(QStringLiteral("Interactive"));
    interactive->addAction(QStringLiteral("VFO Display"), this, [this]() { loadPresetByName(QStringLiteral("Vfo")); });
    interactive->addAction(QStringLiteral("Clock"), this, [this]() { loadPresetByName(QStringLiteral("Clock")); });
    interactive->addAction(QStringLiteral("Contest"), this, [this]() { loadPresetByName(QStringLiteral("Contest")); });

    // Display
    QMenu* displayMenu = menu.addMenu(QStringLiteral("Display"));
    displayMenu->addAction(QStringLiteral("Rotator"), this, [this]() { loadPresetByName(QStringLiteral("Rotator")); });
    displayMenu->addAction(QStringLiteral("Filter Display"), this, [this]() { loadPresetByName(QStringLiteral("FilterDisplay")); });

    // Layout
    menu.addAction(QStringLiteral("Spacer"), this, [this]() { loadPresetByName(QStringLiteral("Spacer")); });

    menu.exec(m_btnPreset->mapToGlobal(QPoint(0, -menu.sizeHint().height())));
}
```

- [ ] **Step 3: Implement loadPresetByName()**

```cpp
void ContainerSettingsDialog::loadPresetByName(const QString& name)
{
    // Confirmation before replacing content
    if (!m_workingItems.isEmpty()) {
        int ret = QMessageBox::question(this, QStringLiteral("Load Preset"),
            QStringLiteral("Replace all current items with the \"%1\" preset?").arg(name),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (ret != QMessageBox::Yes) { return; }
    }

    // Clear working items
    qDeleteAll(m_workingItems);
    m_workingItems.clear();

    // Create preset via ItemGroup factory methods
    ItemGroup* group = nullptr;

    if (name == QLatin1String("SMeterOnly")) {
        group = ItemGroup::createSMeterPreset(0, QStringLiteral("S-Meter"), this);
    } else if (name == QLatin1String("SignalBar")) {
        group = ItemGroup::createSignalBarPreset(this);
    } else if (name == QLatin1String("AvgSignalBar")) {
        group = ItemGroup::createAvgSignalBarPreset(this);
    } else if (name == QLatin1String("MaxBinBar")) {
        group = ItemGroup::createMaxBinBarPreset(this);
    } else if (name == QLatin1String("SignalText")) {
        group = ItemGroup::createSignalTextPreset(0, this);
    } else if (name == QLatin1String("AnanMM")) {
        group = ItemGroup::createAnanMMPreset(this);
    } else if (name == QLatin1String("CrossNeedle")) {
        group = ItemGroup::createCrossNeedlePreset(this);
    } else if (name == QLatin1String("MagicEye")) {
        group = ItemGroup::createMagicEyePreset(0, this);
    } else if (name == QLatin1String("History")) {
        group = ItemGroup::createHistoryPreset(0, this);
    } else if (name == QLatin1String("PowerSwr")) {
        group = ItemGroup::createPowerSwrPreset(QStringLiteral("Power/SWR"), this);
    } else if (name == QLatin1String("Alc")) {
        group = ItemGroup::createAlcPreset(this);
    } else if (name == QLatin1String("AlcGain")) {
        group = ItemGroup::createAlcGainBarPreset(this);
    } else if (name == QLatin1String("AlcGroup")) {
        group = ItemGroup::createAlcGroupBarPreset(this);
    } else if (name == QLatin1String("Mic")) {
        group = ItemGroup::createMicPreset(this);
    } else if (name == QLatin1String("Comp")) {
        group = ItemGroup::createCompPreset(this);
    } else if (name == QLatin1String("Eq")) {
        group = ItemGroup::createEqBarPreset(this);
    } else if (name == QLatin1String("Leveler")) {
        group = ItemGroup::createLevelerBarPreset(this);
    } else if (name == QLatin1String("LevelerGain")) {
        group = ItemGroup::createLevelerGainBarPreset(this);
    } else if (name == QLatin1String("Cfc")) {
        group = ItemGroup::createCfcBarPreset(this);
    } else if (name == QLatin1String("CfcGain")) {
        group = ItemGroup::createCfcGainBarPreset(this);
    } else if (name == QLatin1String("Adc")) {
        group = ItemGroup::createAdcBarPreset(this);
    } else if (name == QLatin1String("AdcMaxMag")) {
        group = ItemGroup::createAdcMaxMagPreset(this);
    } else if (name == QLatin1String("Agc")) {
        group = ItemGroup::createAgcBarPreset(this);
    } else if (name == QLatin1String("AgcGain")) {
        group = ItemGroup::createAgcGainBarPreset(this);
    } else if (name == QLatin1String("Pbsnr")) {
        group = ItemGroup::createPbsnrBarPreset(this);
    } else if (name == QLatin1String("Vfo")) {
        group = ItemGroup::createVfoDisplayPreset(this);
    } else if (name == QLatin1String("Clock")) {
        group = ItemGroup::createClockPreset(this);
    } else if (name == QLatin1String("Contest")) {
        group = ItemGroup::createContestPreset(this);
    } else if (name == QLatin1String("Spacer")) {
        group = ItemGroup::createSpacerPreset(this);
    } else if (name == QLatin1String("Rotator")) {
        // Single RotatorItem, full container
        auto* rotator = new RotatorItem();
        rotator->setRect(0.0f, 0.0f, 1.0f, 1.0f);
        rotator->setBindingId(300);
        m_workingItems.append(rotator);
    } else if (name == QLatin1String("FilterDisplay")) {
        auto* filter = new FilterDisplayItem();
        filter->setRect(0.0f, 0.0f, 1.0f, 1.0f);
        m_workingItems.append(filter);
    }

    // Extract items from group (if factory was used)
    if (group) {
        // Serialize and re-create as independent copies
        // (group items get installed into MeterWidget normally, but here
        //  we want working copies we can edit)
        for (MeterItem* item : group->items()) {
            QString data = item->serialize();
            MeterItem* copy = createItemFromSerialized(data);
            if (copy) {
                m_workingItems.append(copy);
            }
        }
        delete group;
    }

    refreshItemList();
    if (m_workingItems.size() > 0) {
        m_itemList->setCurrentRow(0);
    }
    updatePreview();
}
```

- [ ] **Step 4: Build and verify**

Run: `cd /Users/j.j.boyd/NereusSDR && cmake --build build -j$(sysctl -n hw.ncpu) 2>&1 | tail -5`
Expected: Clean compile.

- [ ] **Step 5: Commit**

```bash
git add src/gui/containers/ContainerSettingsDialog.h \
        src/gui/containers/ContainerSettingsDialog.cpp
git commit -m "feat(3G-6): preset browser with 30+ built-in presets

Categorized preset menu (S-Meter, Composite, TX, RX, Interactive,
Display, Layout) using existing ItemGroup factory methods. Confirmation
prompt before replacing current items."
```

---

### Task 7: Import/Export via Base64

**Files:**
- Modify: `src/gui/containers/ContainerSettingsDialog.h`
- Modify: `src/gui/containers/ContainerSettingsDialog.cpp`

- [ ] **Step 1: Add import/export method declarations**

```cpp
    void onExport();
    void onImport();
```

- [ ] **Step 2: Wire buttons in buildButtonBar()**

Add after existing button connects:

```cpp
    connect(m_btnExport, &QPushButton::clicked, this, &ContainerSettingsDialog::onExport);
    connect(m_btnImport, &QPushButton::clicked, this, &ContainerSettingsDialog::onImport);
```

- [ ] **Step 3: Implement export**

```cpp
void ContainerSettingsDialog::onExport()
{
    // Serialize all working items, pipe-delimited lines, Base64-encoded
    QStringList lines;
    for (const MeterItem* item : m_workingItems) {
        lines << item->serialize();
    }
    QString raw = lines.join(QLatin1Char('\n'));
    QByteArray encoded = raw.toUtf8().toBase64();

    QApplication::clipboard()->setText(QString::fromUtf8(encoded));

    QMessageBox::information(this, QStringLiteral("Export"),
        QStringLiteral("Container configuration copied to clipboard (%1 items).")
        .arg(m_workingItems.size()));
}
```

- [ ] **Step 4: Implement import**

```cpp
void ContainerSettingsDialog::onImport()
{
    QString clipText = QApplication::clipboard()->text().trimmed();
    if (clipText.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Import"),
            QStringLiteral("Clipboard is empty."));
        return;
    }

    // Base64 decode
    QByteArray decoded = QByteArray::fromBase64(clipText.toUtf8());
    if (decoded.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Import"),
            QStringLiteral("Clipboard does not contain valid Base64 data."));
        return;
    }

    QString raw = QString::fromUtf8(decoded);
    QStringList lines = raw.split(QLatin1Char('\n'));

    // Validate: try to parse each line
    QVector<MeterItem*> imported;
    for (const QString& line : lines) {
        if (line.isEmpty()) { continue; }
        MeterItem* item = createItemFromSerialized(line);
        if (item) {
            imported.append(item);
        }
    }

    if (imported.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Import"),
            QStringLiteral("No valid items found in clipboard data."));
        return;
    }

    // Confirmation
    int ret = QMessageBox::question(this, QStringLiteral("Import"),
        QStringLiteral("Replace current items with %1 imported item(s)?")
        .arg(imported.size()),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (ret != QMessageBox::Yes) {
        qDeleteAll(imported);
        return;
    }

    // Replace working items
    qDeleteAll(m_workingItems);
    m_workingItems = imported;

    refreshItemList();
    if (m_workingItems.size() > 0) {
        m_itemList->setCurrentRow(0);
    }
    updatePreview();
}
```

- [ ] **Step 5: Build and verify**

Run: `cd /Users/j.j.boyd/NereusSDR && cmake --build build -j$(sysctl -n hw.ncpu) 2>&1 | tail -5`
Expected: Clean compile.

- [ ] **Step 6: Commit**

```bash
git add src/gui/containers/ContainerSettingsDialog.h \
        src/gui/containers/ContainerSettingsDialog.cpp
git commit -m "feat(3G-6): import/export via Base64-encoded clipboard

Export serializes all items to pipe-delimited text, Base64-encodes,
copies to clipboard. Import decodes, validates, confirms replacement.
Format matches existing MeterWidget serialization."
```

---

### Task 8: Wire ContainerManager to Open Dialog

**Files:**
- Modify: `src/gui/containers/ContainerManager.cpp`

This connects the existing `settingsRequested()` signal (already emitted from `ContainerWidget::btnSettings_clicked`) to open the new dialog.

- [ ] **Step 1: Add include and dialog opening to wireContainer()**

Add include at top of `ContainerManager.cpp`:

```cpp
#include "ContainerSettingsDialog.h"
```

Add to `wireContainer()` method, after the existing float/dock connections:

```cpp
    connect(container, &ContainerWidget::settingsRequested, this, [container]() {
        ContainerSettingsDialog dialog(container, container->window());
        dialog.exec();
    });
```

- [ ] **Step 2: Build and verify**

Run: `cd /Users/j.j.boyd/NereusSDR && cmake --build build -j$(sysctl -n hw.ncpu) 2>&1 | tail -5`
Expected: Clean compile.

- [ ] **Step 3: Commit**

```bash
git add src/gui/containers/ContainerManager.cpp
git commit -m "feat(3G-6): wire container settings button to dialog

ContainerManager::wireContainer() connects settingsRequested() signal
to open ContainerSettingsDialog as a modal dialog."
```

---

### Task 9: Update MeterWidget::deserializeItems() Type Registry

**Files:**
- Modify: `src/gui/meters/MeterWidget.cpp`

The existing `MeterWidget::deserializeItems()` only handles the original 6 core types (BAR, SOLID, IMAGE, SCALE, TEXT, NEEDLE). All 3G-4 and 3G-5 types are missing, which means containers with advanced items can't be restored from settings. Add the complete registry.

- [ ] **Step 1: Add includes for all item types**

Add to the top of `MeterWidget.cpp` (after existing includes):

```cpp
#include "SpacerItem.h"
#include "FadeCoverItem.h"
#include "LEDItem.h"
#include "HistoryGraphItem.h"
#include "MagicEyeItem.h"
#include "NeedleScalePwrItem.h"
#include "SignalTextItem.h"
#include "DialItem.h"
#include "TextOverlayItem.h"
#include "WebImageItem.h"
#include "FilterDisplayItem.h"
#include "RotatorItem.h"
#include "BandButtonItem.h"
#include "ModeButtonItem.h"
#include "FilterButtonItem.h"
#include "AntennaButtonItem.h"
#include "TuneStepButtonItem.h"
#include "OtherButtonItem.h"
#include "VoiceRecordPlayItem.h"
#include "DiscordButtonItem.h"
#include "VfoDisplayItem.h"
#include "ClockItem.h"
#include "ClickBoxItem.h"
#include "DataOutItem.h"
```

- [ ] **Step 2: Extend the type tag registry in deserializeItems()**

Replace the type matching block in `deserializeItems()` (the `if/else if` chain at lines 139-151) with the complete registry:

```cpp
        if (type == QStringLiteral("BAR")) {
            item = new BarItem();
        } else if (type == QStringLiteral("SOLID")) {
            item = new SolidColourItem();
        } else if (type == QStringLiteral("IMAGE")) {
            item = new ImageItem();
        } else if (type == QStringLiteral("SCALE")) {
            item = new ScaleItem();
        } else if (type == QStringLiteral("TEXT")) {
            item = new TextItem();
        } else if (type == QStringLiteral("NEEDLE")) {
            item = new NeedleItem();
        } else if (type == QStringLiteral("SPACER")) {
            item = new SpacerItem();
        } else if (type == QStringLiteral("FADECOVER")) {
            item = new FadeCoverItem();
        } else if (type == QStringLiteral("LED")) {
            item = new LEDItem();
        } else if (type == QStringLiteral("HISTORY")) {
            item = new HistoryGraphItem();
        } else if (type == QStringLiteral("MAGICEYE")) {
            item = new MagicEyeItem();
        } else if (type == QStringLiteral("NEEDLESCALEPWR")) {
            item = new NeedleScalePwrItem();
        } else if (type == QStringLiteral("SIGNALTEXT")) {
            item = new SignalTextItem();
        } else if (type == QStringLiteral("DIAL")) {
            item = new DialItem();
        } else if (type == QStringLiteral("TEXTOVERLAY")) {
            item = new TextOverlayItem();
        } else if (type == QStringLiteral("WEBIMAGE")) {
            item = new WebImageItem();
        } else if (type == QStringLiteral("FILTERDISPLAY")) {
            item = new FilterDisplayItem();
        } else if (type == QStringLiteral("ROTATOR")) {
            item = new RotatorItem();
        } else if (type == QStringLiteral("BANDBTNS")) {
            item = new BandButtonItem();
        } else if (type == QStringLiteral("MODEBTNS")) {
            item = new ModeButtonItem();
        } else if (type == QStringLiteral("FILTERBTNS")) {
            item = new FilterButtonItem();
        } else if (type == QStringLiteral("ANTENNABTNS")) {
            item = new AntennaButtonItem();
        } else if (type == QStringLiteral("TUNESTEPBTNS")) {
            item = new TuneStepButtonItem();
        } else if (type == QStringLiteral("OTHERBTNS")) {
            item = new OtherButtonItem();
        } else if (type == QStringLiteral("VOICERECPLAY")) {
            item = new VoiceRecordPlayItem();
        } else if (type == QStringLiteral("DISCORDBTNS")) {
            item = new DiscordButtonItem();
        } else if (type == QStringLiteral("VFO")) {
            item = new VfoDisplayItem();
        } else if (type == QStringLiteral("CLOCK")) {
            item = new ClockItem();
        } else if (type == QStringLiteral("CLICKBOX")) {
            item = new ClickBoxItem();
        } else if (type == QStringLiteral("DATAOUT")) {
            item = new DataOutItem();
        }
```

- [ ] **Step 3: Build and verify**

Run: `cd /Users/j.j.boyd/NereusSDR && cmake --build build -j$(sysctl -n hw.ncpu) 2>&1 | tail -5`
Expected: Clean compile.

- [ ] **Step 4: Commit**

```bash
git add src/gui/meters/MeterWidget.cpp
git commit -m "fix(3G-6): extend MeterWidget deserializeItems() with all item types

MeterWidget::deserializeItems() now recognizes all 28 type tags from
3G-4 and 3G-5 phases (was only 6 core types). Enables proper container
restoration from AppSettings persistence."
```

---

### Task 10: Build Verification + Manual Test

**Files:** None (verification only)

- [ ] **Step 1: Full clean build**

Run: `cd /Users/j.j.boyd/NereusSDR && cmake --build build --clean-first -j$(sysctl -n hw.ncpu) 2>&1 | tail -10`
Expected: Clean compile with no errors.

- [ ] **Step 2: Launch app and test dialog**

Run: `cd /Users/j.j.boyd/NereusSDR && ./build/NereusSDR &`

Test checklist:
1. Click gear icon on any container title bar → dialog opens
2. Items from current container appear in left list
3. Select an item → properties populate in center panel
4. Change a property → preview updates in real-time
5. Add new item via "+" button → categorized picker appears
6. Remove item → removed from list and preview
7. Move up/down → reorder works
8. Load a preset (e.g., "S-Meter Only") → confirmation, items replaced
9. Export → copies to clipboard
10. Import → paste back, items restored
11. OK → changes applied to container
12. Cancel → changes discarded

- [ ] **Step 3: Kill app**

Run: `pkill -f NereusSDR`

---

### Task 11: Update Design Spec Status + CLAUDE.md

**Files:**
- Modify: `CLAUDE.md` (update phase status)

- [ ] **Step 1: Update phase 3G-6 status in CLAUDE.md**

Change the 3G-6 row in the phase table from `Planned` to `Complete`:

```
| 3G-6: Container Settings Dialog | Full composability UI, import/export | **Complete** |
```

- [ ] **Step 2: Update the implementation plans table**

Add the plan entry:

```
| [phase3g6-container-settings-dialog-plan.md](docs/architecture/phase3g6-container-settings-dialog-plan.md) | 3G-6: Container Settings Dialog | **Complete** |
```

- [ ] **Step 3: Commit**

```bash
git add CLAUDE.md
git commit -m "docs: mark Phase 3G-6 Container Settings Dialog as complete"
```
