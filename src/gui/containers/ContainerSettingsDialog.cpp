#include "ContainerSettingsDialog.h"
#include "ContainerManager.h"
#include "ContainerWidget.h"
#include "../meters/MeterWidget.h"
#include "../meters/MeterItem.h"
#include "../meters/ItemGroup.h"

// Core meter item types (MeterItem.h defines: BarItem, SolidColourItem, ImageItem,
//                                               ScaleItem, TextItem, NeedleItem)
// Phase 3G-4 passive item types
#include "../meters/SpacerItem.h"
#include "../meters/FadeCoverItem.h"
#include "../meters/LEDItem.h"
#include "../meters/HistoryGraphItem.h"
#include "../meters/MagicEyeItem.h"
#include "../meters/NeedleScalePwrItem.h"
#include "../meters/SignalTextItem.h"
#include "../meters/DialItem.h"
#include "../meters/TextOverlayItem.h"
#include "../meters/WebImageItem.h"
#include "../meters/FilterDisplayItem.h"
#include "../meters/RotatorItem.h"
// Phase 3G-5 interactive item types
#include "../meters/BandButtonItem.h"
#include "../meters/ModeButtonItem.h"
#include "../meters/FilterButtonItem.h"
#include "../meters/AntennaButtonItem.h"
#include "../meters/TuneStepButtonItem.h"
#include "../meters/OtherButtonItem.h"
#include "../meters/VoiceRecordPlayItem.h"
#include "../meters/DiscordButtonItem.h"
#include "../meters/VfoDisplayItem.h"
#include "../meters/ClockItem.h"
#include "../meters/ClickBoxItem.h"
#include "../meters/DataOutItem.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QListWidget>
#include <QStackedWidget>
#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QColorDialog>
#include <QColor>
#include <QMap>
#include <QMenu>
#include <QStringList>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QScrollArea>
#include <QMessageBox>
#include <QClipboard>
#include <QApplication>

namespace NereusSDR {

// ---------------------------------------------------------------------------
// Style helpers (scoped to this translation unit)
// ---------------------------------------------------------------------------
namespace {

constexpr const char* kBtnStyle =
    "QPushButton {"
    "  background: #1a2a3a;"
    "  color: #c8d8e8;"
    "  border: 1px solid #205070;"
    "  border-radius: 3px;"
    "  padding: 4px 8px;"
    "}"
    "QPushButton:hover { background: #203040; }";

constexpr const char* kOkBtnStyle =
    "QPushButton {"
    "  background: #00b4d8;"
    "  color: #0f0f1a;"
    "  border: 1px solid #00d4f8;"
    "  border-radius: 3px;"
    "  padding: 4px 8px;"
    "  font-weight: bold;"
    "}"
    "QPushButton:hover { background: #00c8e8; }";

constexpr const char* kEditStyle =
    "background: #0a0a18;"
    "color: #c8d8e8;"
    "border: 1px solid #1e2e3e;"
    "border-radius: 3px;"
    "padding: 2px 4px;";

constexpr const char* kListStyle =
    "QListWidget {"
    "  background: #0a0a18;"
    "  color: #c8d8e8;"
    "  border: 1px solid #203040;"
    "}"
    "QListWidget::item:selected {"
    "  background: #00b4d8;"
    "  color: #0f0f1a;"
    "}";

constexpr const char* kLabelStyle =
    "color: #8090a0; font-size: 12px;";

constexpr const char* kSectionHeaderStyle =
    "color: #8aa8c0; font-weight: bold; font-size: 12px;";

constexpr const char* kSpinStyle =
    "background: #0a0a18;"
    "color: #c8d8e8;"
    "border: 1px solid #1e2e3e;"
    "border-radius: 3px;"
    "padding: 2px;";

QPushButton* makeBtn(const QString& text, QWidget* parent)
{
    QPushButton* btn = new QPushButton(text, parent);
    btn->setStyleSheet(kBtnStyle);
    btn->setAutoDefault(false);
    btn->setDefault(false);
    return btn;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

ContainerSettingsDialog::ContainerSettingsDialog(ContainerWidget* container,
                                                 QWidget* parent,
                                                 ContainerManager* manager)
    : QDialog(parent)
    , m_container(container)
    , m_manager(manager)
{
    // Window title uses the first 8 chars of the container ID
    const QString shortId = m_container ? m_container->id().left(8) : QStringLiteral("????????");
    setWindowTitle(QStringLiteral("Container Settings \u2014 ") + shortId);

    setMinimumSize(800, 500);
    resize(900, 600);

    buildLayout();

    // Phase 3G-6 block 3 commit 14: snapshot the current container
    // state so Cancel can fully revert the in-progress edits. Must
    // happen AFTER buildLayout so populateItemList has already
    // captured items into m_workingItems from the live MeterWidget.
    takeSnapshot();
}

ContainerSettingsDialog::~ContainerSettingsDialog()
{
    qDeleteAll(m_workingItems);
}

// ---------------------------------------------------------------------------
// Layout construction
// ---------------------------------------------------------------------------

void ContainerSettingsDialog::buildLayout()
{
    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

    // Dark dialog background
    setStyleSheet("QDialog { background: #0f0f1a; color: #c8d8e8; }");

    // --- Container-level properties bar (top) ---
    buildContainerPropertiesSection(root);

    // --- Three-panel splitter ---
    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setChildrenCollapsible(false);
    m_splitter->setStyleSheet(
        "QSplitter::handle { background: #203040; width: 3px; }");

    // Phase 3G-6 block 3 commit 12: Thetis 3-column layout.
    // Left = Available (catalog), center = In-use, right = Properties.
    QWidget* availWrapper = new QWidget(m_splitter);
    buildAvailablePanel(availWrapper);
    m_splitter->addWidget(availWrapper);

    QWidget* inUseWrapper = new QWidget(m_splitter);
    buildInUsePanel(inUseWrapper);
    m_splitter->addWidget(inUseWrapper);

    QWidget* propsWrapper = new QWidget(m_splitter);
    buildPropertiesPanel(propsWrapper);
    m_splitter->addWidget(propsWrapper);

    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 0);
    m_splitter->setStretchFactor(2, 1);
    m_splitter->setSizes({200, 200, 460});

    root->addWidget(m_splitter, 1);

    // --- Button bar (bottom) ---
    buildButtonBar();
}

// ---------------------------------------------------------------------------
// Phase 3G-6 block 3 commit 12: Thetis 3-column layout
// ---------------------------------------------------------------------------

void ContainerSettingsDialog::buildAvailablePanel(QWidget* parent)
{
    // Mirrors Thetis lstMetersAvailable (setup.cs:24522-24566). Commit 15
    // adds RX / TX / Special category headers; for this commit the list
    // is flat alphabetical.
    QVBoxLayout* layout = new QVBoxLayout(parent);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    QLabel* header = new QLabel(QStringLiteral("Available"), parent);
    header->setStyleSheet(kSectionHeaderStyle);
    layout->addWidget(header);

    m_availableList = new QListWidget(parent);
    m_availableList->setStyleSheet(kListStyle);
    layout->addWidget(m_availableList, 1);

    QHBoxLayout* btnRow = new QHBoxLayout;
    btnRow->setSpacing(3);
    m_btnAddFromAvailable = makeBtn(QStringLiteral("Add \u2192"), parent);
    m_btnAddFromAvailable->setToolTip(
        QStringLiteral("Add the selected available item to the in-use list"));
    btnRow->addStretch();
    btnRow->addWidget(m_btnAddFromAvailable);
    layout->addLayout(btnRow);

    connect(m_btnAddFromAvailable, &QPushButton::clicked,
            this, &ContainerSettingsDialog::onAddFromAvailable);
    connect(m_availableList, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem*) { onAddFromAvailable(); });

    populateAvailableList();
}

void ContainerSettingsDialog::buildInUsePanel(QWidget* parent)
{
    QVBoxLayout* layout = new QVBoxLayout(parent);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    QLabel* header = new QLabel(QStringLiteral("In-use items"), parent);
    header->setStyleSheet(kSectionHeaderStyle);
    layout->addWidget(header);

    m_itemList = new QListWidget(parent);
    m_itemList->setStyleSheet(kListStyle);
    layout->addWidget(m_itemList, 1);

    connect(m_itemList, &QListWidget::currentRowChanged,
            this, &ContainerSettingsDialog::onItemSelectionChanged);

    // Action buttons row — the legacy + (popup) button is kept for
    // parity with the old flow; it sits alongside the new Remove /
    // Up / Down controls.
    QHBoxLayout* btnRow = new QHBoxLayout;
    btnRow->setSpacing(3);

    m_btnAdd      = makeBtn(QStringLiteral("+"),           parent);
    m_btnRemove   = makeBtn(QStringLiteral("\u2212"),      parent);
    m_btnMoveUp   = makeBtn(QStringLiteral("\u25b2"),      parent);
    m_btnMoveDown = makeBtn(QStringLiteral("\u25bc"),      parent);

    m_btnAdd->setToolTip(QStringLiteral("Add item (popup)"));
    m_btnRemove->setToolTip(QStringLiteral("Remove selected item"));
    m_btnMoveUp->setToolTip(QStringLiteral("Move item up"));
    m_btnMoveDown->setToolTip(QStringLiteral("Move item down"));

    btnRow->addWidget(m_btnAdd);
    btnRow->addWidget(m_btnRemove);
    btnRow->addStretch();
    btnRow->addWidget(m_btnMoveUp);
    btnRow->addWidget(m_btnMoveDown);

    layout->addLayout(btnRow);

    connect(m_btnAdd,      &QPushButton::clicked, this, &ContainerSettingsDialog::onAddItem);
    connect(m_btnRemove,   &QPushButton::clicked, this, &ContainerSettingsDialog::onRemoveItem);
    connect(m_btnMoveUp,   &QPushButton::clicked, this, &ContainerSettingsDialog::onMoveItemUp);
    connect(m_btnMoveDown, &QPushButton::clicked, this, &ContainerSettingsDialog::onMoveItemDown);

    populateItemList();
}

void ContainerSettingsDialog::buildPropertiesPanel(QWidget* parent)
{
    QVBoxLayout* layout = new QVBoxLayout(parent);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    QLabel* header = new QLabel(QStringLiteral("Properties"), parent);
    header->setStyleSheet(kSectionHeaderStyle);
    layout->addWidget(header);

    m_propertyStack = new QStackedWidget(parent);
    m_propertyStack->setStyleSheet(
        "QStackedWidget { background: #0a0a18; border: 1px solid #203040; }");

    // Page 0: empty placeholder
    m_emptyPage = new QWidget(m_propertyStack);
    QVBoxLayout* emptyLayout = new QVBoxLayout(m_emptyPage);
    QLabel* emptyLabel = new QLabel(
        QStringLiteral("Select an item to edit properties"), m_emptyPage);
    emptyLabel->setStyleSheet(kLabelStyle);
    emptyLabel->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(emptyLabel);
    m_propertyStack->addWidget(m_emptyPage);

    layout->addWidget(m_propertyStack, 1);

    buildCommonPropsPage();
}

// ---------------------------------------------------------------------------
// Available list catalog and Add-from-available flow
// ---------------------------------------------------------------------------

void ContainerSettingsDialog::populateAvailableList()
{
    if (!m_availableList) { return; }
    m_availableList->clear();

    // Flat alphabetical catalog. Commit 15 categorizes this into
    // RX / TX / Special sections per Thetis lstMetersAvailable.
    struct Entry { const char* tag; const char* label; };
    static const Entry kCatalog[] = {
        {"BAR",            "Bar Meter"},
        {"CLICKBOX",       "Click Box"},
        {"CLOCK",          "Clock"},
        {"DATAOUT",        "Data Out"},
        {"DIAL",           "Dial Meter"},
        {"FADECOVER",      "Fade Cover"},
        {"FILTERDISPLAY",  "Filter Display"},
        {"HISTORY",        "History Graph"},
        {"IMAGE",          "Image"},
        {"LED",            "LED"},
        {"MAGICEYE",       "Magic Eye"},
        {"NEEDLE",         "Needle Meter"},
        {"NEEDLESCALEPWR", "Needle Scale Power"},
        {"ROTATOR",        "Rotator"},
        {"SCALE",          "Scale"},
        {"SIGNALTEXT",     "Signal Text"},
        {"SOLID",          "Solid Colour"},
        {"SPACER",         "Spacer"},
        {"TEXT",           "Text Readout"},
        {"TEXTOVERLAY",    "Text Overlay"},
        {"VFODISPLAY",     "VFO Display"},
        {"WEBIMAGE",       "Web Image"},
    };

    for (const auto& e : kCatalog) {
        auto* item = new QListWidgetItem(QString::fromLatin1(e.label), m_availableList);
        item->setData(Qt::UserRole, QString::fromLatin1(e.tag));
    }
}

void ContainerSettingsDialog::onAddFromAvailable()
{
    if (!m_availableList) { return; }
    QListWidgetItem* sel = m_availableList->currentItem();
    if (!sel) { return; }
    const QString tag = sel->data(Qt::UserRole).toString();
    if (tag.isEmpty()) { return; }
    addNewItem(tag);
}

// Phase 3G-6 block 3 commit 11: buildRightPanel / live preview deleted.
// Commit 12 reintroduces a Properties column here under the Thetis
// 3-column layout.

void ContainerSettingsDialog::buildContainerPropertiesSection(QVBoxLayout* parentLayout)
{
    QFrame* bar = new QFrame(this);
    bar->setFrameShape(QFrame::StyledPanel);
    bar->setStyleSheet(
        "QFrame { background: #111122; border: 1px solid #203040; border-radius: 4px; }");

    QHBoxLayout* barLayout = new QHBoxLayout(bar);
    barLayout->setContentsMargins(8, 4, 8, 4);
    barLayout->setSpacing(10);

    // Phase 3G-6 block 3 commit 13: container-switch dropdown.
    // Populated from ContainerManager::allContainers() when a manager
    // is available. The switching behavior (auto-commit + new
    // snapshot) lands in commit 14 alongside the snapshot/revert
    // machinery; this commit just lays down the UI.
    QLabel* contLabel = new QLabel(QStringLiteral("Container:"), bar);
    contLabel->setStyleSheet(kLabelStyle);
    m_containerDropdown = new QComboBox(bar);
    m_containerDropdown->setStyleSheet(
        "QComboBox { background: #0a0a18; color: #c8d8e8;"
        "  border: 1px solid #1e2e3e; border-radius: 3px; padding: 2px 4px;"
        "  min-width: 160px; }"
        "QComboBox QAbstractItemView { background: #0a0a18; color: #c8d8e8;"
        "  border: 1px solid #205070; selection-background-color: #00b4d8; }");
    if (m_manager) {
        const QList<ContainerWidget*> all = m_manager->allContainers();
        int activeIdx = -1;
        for (ContainerWidget* c : all) {
            const QString label = c->notes().isEmpty()
                ? c->id().left(8)
                : c->notes();
            m_containerDropdown->addItem(label, c->id());
            if (c == m_container) {
                activeIdx = m_containerDropdown->count() - 1;
            }
        }
        if (activeIdx >= 0) {
            m_containerDropdown->setCurrentIndex(activeIdx);
        }
    } else if (m_container) {
        // Legacy single-container path: show just the current container.
        const QString label = m_container->notes().isEmpty()
            ? m_container->id().left(8)
            : m_container->notes();
        m_containerDropdown->addItem(label, m_container->id());
    }
    // Disable until commit 14 wires the switch handler so the user
    // cannot trigger an unimplemented transition.
    m_containerDropdown->setEnabled(m_manager != nullptr
                                    && m_containerDropdown->count() > 1);

    barLayout->addWidget(contLabel);
    barLayout->addWidget(m_containerDropdown);

    // Title
    QLabel* titleLabel = new QLabel(QStringLiteral("Title:"), bar);
    titleLabel->setStyleSheet(kLabelStyle);
    m_titleEdit = new QLineEdit(bar);
    m_titleEdit->setStyleSheet(kEditStyle);
    m_titleEdit->setPlaceholderText(QStringLiteral("Container title..."));
    m_titleEdit->setFixedWidth(140);

    // BG Color
    QLabel* bgLabel = new QLabel(QStringLiteral("BG:"), bar);
    bgLabel->setStyleSheet(kLabelStyle);
    m_bgColorBtn = new QPushButton(QStringLiteral("  "), bar);
    m_bgColorBtn->setStyleSheet(
        "QPushButton { background: #0f0f1a; border: 1px solid #205070;"
        "  border-radius: 3px; min-width: 32px; min-height: 18px; }"
        "QPushButton:hover { border-color: #00b4d8; }");
    m_bgColorBtn->setAutoDefault(false);
    m_bgColorBtn->setDefault(false);
    m_bgColorBtn->setToolTip(QStringLiteral("Choose background color"));
    connect(m_bgColorBtn, &QPushButton::clicked, this, [this]() {
        QColor initial(QStringLiteral("#0f0f1a"));
        QColor chosen = QColorDialog::getColor(initial, this,
                                               QStringLiteral("Background Color"));
        if (chosen.isValid()) {
            m_bgColorBtn->setStyleSheet(
                QStringLiteral("QPushButton { background: %1; border: 1px solid #205070;"
                                "  border-radius: 3px; min-width: 32px; min-height: 18px; }"
                                "QPushButton:hover { border-color: #00b4d8; }").arg(chosen.name()));
        }
    });

    // Border
    QLabel* borderLabel = new QLabel(QStringLiteral("Border:"), bar);
    borderLabel->setStyleSheet(kLabelStyle);
    m_borderCheck = new QCheckBox(bar);
    m_borderCheck->setStyleSheet("QCheckBox { color: #c8d8e8; }");

    // RX Source
    QLabel* rxLabel = new QLabel(QStringLiteral("RX:"), bar);
    rxLabel->setStyleSheet(kLabelStyle);
    m_rxSourceCombo = new QComboBox(bar);
    m_rxSourceCombo->addItem(QStringLiteral("RX1"), 1);
    m_rxSourceCombo->addItem(QStringLiteral("RX2"), 2);
    m_rxSourceCombo->setStyleSheet(
        "QComboBox { background: #0a0a18; color: #c8d8e8;"
        "  border: 1px solid #1e2e3e; border-radius: 3px; padding: 2px 4px; }"
        "QComboBox QAbstractItemView { background: #0a0a18; color: #c8d8e8;"
        "  border: 1px solid #205070; selection-background-color: #00b4d8; }");
    m_rxSourceCombo->setFixedWidth(64);

    // Show on RX / TX
    QLabel* showRxLabel = new QLabel(QStringLiteral("Show RX:"), bar);
    showRxLabel->setStyleSheet(kLabelStyle);
    m_showOnRxCheck = new QCheckBox(bar);
    m_showOnRxCheck->setStyleSheet("QCheckBox { color: #c8d8e8; }");

    QLabel* showTxLabel = new QLabel(QStringLiteral("Show TX:"), bar);
    showTxLabel->setStyleSheet(kLabelStyle);
    m_showOnTxCheck = new QCheckBox(bar);
    m_showOnTxCheck->setStyleSheet("QCheckBox { color: #c8d8e8; }");

    // Assemble bar
    barLayout->addWidget(titleLabel);
    barLayout->addWidget(m_titleEdit);
    barLayout->addWidget(bgLabel);
    barLayout->addWidget(m_bgColorBtn);
    barLayout->addWidget(borderLabel);
    barLayout->addWidget(m_borderCheck);
    barLayout->addWidget(rxLabel);
    barLayout->addWidget(m_rxSourceCombo);
    barLayout->addWidget(showRxLabel);
    barLayout->addWidget(m_showOnRxCheck);
    barLayout->addWidget(showTxLabel);
    barLayout->addWidget(m_showOnTxCheck);
    barLayout->addStretch();

    // Populate from container
    if (m_container) {
        m_titleEdit->setText(m_container->notes());
        m_borderCheck->setChecked(m_container->hasBorder());
        const int rxSrc = m_container->rxSource();
        const int idx = m_rxSourceCombo->findData(rxSrc);
        if (idx >= 0) {
            m_rxSourceCombo->setCurrentIndex(idx);
        }
        m_showOnRxCheck->setChecked(m_container->showOnRx());
        m_showOnTxCheck->setChecked(m_container->showOnTx());
    }

    parentLayout->addWidget(bar);
}

void ContainerSettingsDialog::buildButtonBar()
{
    QHBoxLayout* barLayout = new QHBoxLayout;
    barLayout->setSpacing(6);

    // Left cluster: Presets / Import / Export
    m_btnPreset = makeBtn(QStringLiteral("Presets\u2026"), this);
    m_btnImport = makeBtn(QStringLiteral("Import"),        this);
    m_btnExport = makeBtn(QStringLiteral("Export"),        this);

    barLayout->addWidget(m_btnPreset);
    barLayout->addWidget(m_btnImport);
    barLayout->addWidget(m_btnExport);
    barLayout->addStretch();

    // Right cluster: Apply / Cancel / OK
    m_btnApply  = makeBtn(QStringLiteral("Apply"),  this);
    m_btnCancel = makeBtn(QStringLiteral("Cancel"), this);

    m_btnOk = new QPushButton(QStringLiteral("OK"), this);
    m_btnOk->setStyleSheet(kOkBtnStyle);
    m_btnOk->setAutoDefault(false);
    m_btnOk->setDefault(false);

    barLayout->addWidget(m_btnApply);
    barLayout->addWidget(m_btnCancel);
    barLayout->addWidget(m_btnOk);

    // Wire buttons
    connect(m_btnPreset, &QPushButton::clicked, this, &ContainerSettingsDialog::onLoadPreset);
    connect(m_btnExport, &QPushButton::clicked, this, &ContainerSettingsDialog::onExport);
    connect(m_btnImport, &QPushButton::clicked, this, &ContainerSettingsDialog::onImport);
    connect(m_btnApply,  &QPushButton::clicked, this, &ContainerSettingsDialog::applyToContainer);
    connect(m_btnCancel, &QPushButton::clicked, this, [this]() {
        // Phase 3G-6 block 3 commit 14: Cancel reverts the container
        // and its meter items to the snapshot captured on dialog
        // open, then closes. Applied changes from Apply clicks are
        // NOT reverted — that's intentional: Apply is a "commit"
        // action and the snapshot is only a safety net for
        // un-Applied edits.
        revertFromSnapshot();
        reject();
    });
    connect(m_btnOk,     &QPushButton::clicked, this, [this]() {
        applyToContainer();
        accept();
    });

    // Add bar to the root layout
    QVBoxLayout* root = qobject_cast<QVBoxLayout*>(layout());
    if (root) {
        root->addLayout(barLayout);
    }
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void ContainerSettingsDialog::onItemSelectionChanged()
{
    const int row = m_itemList->currentRow();

    if (row < 0 || row >= m_workingItems.size()) {
        m_propertyStack->setCurrentWidget(m_emptyPage);
        return;
    }

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

void ContainerSettingsDialog::onAddItem()
{
    // Styled popup menu with categorized sub-menus
    auto* menu = new QMenu(this);
    menu->setStyleSheet(
        QStringLiteral("QMenu {"
                       "  background: #1a2a3a;"
                       "  color: #c8d8e8;"
                       "  border: 1px solid #205070;"
                       "}"
                       "QMenu::item:selected {"
                       "  background: #00b4d8;"
                       "  color: #0f0f1a;"
                       "}"
                       "QMenu::separator {"
                       "  background: #203040;"
                       "  height: 1px;"
                       "}"));

    // --- Meters ---
    QMenu* metersMenu = menu->addMenu(QStringLiteral("Meters"));
    metersMenu->setStyleSheet(menu->styleSheet());
    metersMenu->addAction(QStringLiteral("Bar Meter"),     this, [this]{ addNewItem(QStringLiteral("BAR")); });
    metersMenu->addAction(QStringLiteral("Needle Meter"),  this, [this]{ addNewItem(QStringLiteral("NEEDLE")); });
    metersMenu->addAction(QStringLiteral("Dial Meter"),    this, [this]{ addNewItem(QStringLiteral("DIAL")); });
    metersMenu->addAction(QStringLiteral("History Graph"), this, [this]{ addNewItem(QStringLiteral("HISTORY")); });
    metersMenu->addAction(QStringLiteral("Magic Eye"),     this, [this]{ addNewItem(QStringLiteral("MAGICEYE")); });
    metersMenu->addAction(QStringLiteral("Signal Text"),   this, [this]{ addNewItem(QStringLiteral("SIGNALTEXT")); });
    metersMenu->addAction(QStringLiteral("Power Scale"),   this, [this]{ addNewItem(QStringLiteral("NEEDLESCALEPWR")); });

    // --- Indicators ---
    QMenu* indicatorsMenu = menu->addMenu(QStringLiteral("Indicators"));
    indicatorsMenu->setStyleSheet(menu->styleSheet());
    indicatorsMenu->addAction(QStringLiteral("LED Indicator"), this, [this]{ addNewItem(QStringLiteral("LED")); });
    indicatorsMenu->addAction(QStringLiteral("Text Readout"),  this, [this]{ addNewItem(QStringLiteral("TEXT")); });
    indicatorsMenu->addAction(QStringLiteral("Clock"),         this, [this]{ addNewItem(QStringLiteral("CLOCK")); });
    indicatorsMenu->addAction(QStringLiteral("Text Overlay"),  this, [this]{ addNewItem(QStringLiteral("TEXTOVERLAY")); });

    // --- Controls ---
    QMenu* controlsMenu = menu->addMenu(QStringLiteral("Controls"));
    controlsMenu->setStyleSheet(menu->styleSheet());
    controlsMenu->addAction(QStringLiteral("Band Buttons"),      this, [this]{ addNewItem(QStringLiteral("BANDBTNS")); });
    controlsMenu->addAction(QStringLiteral("Mode Buttons"),      this, [this]{ addNewItem(QStringLiteral("MODEBTNS")); });
    controlsMenu->addAction(QStringLiteral("Filter Buttons"),    this, [this]{ addNewItem(QStringLiteral("FILTERBTNS")); });
    controlsMenu->addAction(QStringLiteral("Antenna Buttons"),   this, [this]{ addNewItem(QStringLiteral("ANTENNABTNS")); });
    controlsMenu->addAction(QStringLiteral("Tune Step Buttons"), this, [this]{ addNewItem(QStringLiteral("TUNESTEPBTNS")); });
    controlsMenu->addAction(QStringLiteral("Other Buttons"),     this, [this]{ addNewItem(QStringLiteral("OTHERBTNS")); });
    controlsMenu->addAction(QStringLiteral("Voice Rec/Play"),    this, [this]{ addNewItem(QStringLiteral("VOICERECPLAY")); });
    controlsMenu->addAction(QStringLiteral("VFO Display"),       this, [this]{ addNewItem(QStringLiteral("VFO")); });
    controlsMenu->addAction(QStringLiteral("Discord Buttons"),   this, [this]{ addNewItem(QStringLiteral("DISCORDBTNS")); });

    // --- Display ---
    QMenu* displayMenu = menu->addMenu(QStringLiteral("Display"));
    displayMenu->setStyleSheet(menu->styleSheet());
    displayMenu->addAction(QStringLiteral("Filter Display"), this, [this]{ addNewItem(QStringLiteral("FILTERDISPLAY")); });
    displayMenu->addAction(QStringLiteral("Rotator"),        this, [this]{ addNewItem(QStringLiteral("ROTATOR")); });

    // --- Layout ---
    QMenu* layoutMenu = menu->addMenu(QStringLiteral("Layout"));
    layoutMenu->setStyleSheet(menu->styleSheet());
    layoutMenu->addAction(QStringLiteral("Solid Color"), this, [this]{ addNewItem(QStringLiteral("SOLID")); });
    layoutMenu->addAction(QStringLiteral("Image"),       this, [this]{ addNewItem(QStringLiteral("IMAGE")); });
    layoutMenu->addAction(QStringLiteral("Web Image"),   this, [this]{ addNewItem(QStringLiteral("WEBIMAGE")); });
    layoutMenu->addAction(QStringLiteral("Spacer"),      this, [this]{ addNewItem(QStringLiteral("SPACER")); });
    layoutMenu->addAction(QStringLiteral("Fade Cover"),  this, [this]{ addNewItem(QStringLiteral("FADECOVER")); });
    layoutMenu->addAction(QStringLiteral("Click Box"),   this, [this]{ addNewItem(QStringLiteral("CLICKBOX")); });
    layoutMenu->addAction(QStringLiteral("Scale"),       this, [this]{ addNewItem(QStringLiteral("SCALE")); });

    // --- Data ---
    QMenu* dataMenu = menu->addMenu(QStringLiteral("Data"));
    dataMenu->setStyleSheet(menu->styleSheet());
    dataMenu->addAction(QStringLiteral("Data Output"), this, [this]{ addNewItem(QStringLiteral("DATAOUT")); });

    menu->popup(m_btnAdd->mapToGlobal(QPoint(0, m_btnAdd->height())));
}

void ContainerSettingsDialog::onRemoveItem()
{
    const int row = m_itemList->currentRow();
    if (row < 0 || row >= m_workingItems.size()) {
        return;
    }

    delete m_workingItems.takeAt(row);
    refreshItemList();

    // Select nearest remaining item
    if (!m_workingItems.isEmpty()) {
        const int newRow = qMin(row, m_workingItems.size() - 1);
        m_itemList->setCurrentRow(newRow);
    }

    updatePreview();
}

void ContainerSettingsDialog::onMoveItemUp()
{
    const int row = m_itemList->currentRow();
    if (row <= 0) {
        return;
    }

    m_workingItems.swapItemsAt(row, row - 1);
    refreshItemList();
    m_itemList->setCurrentRow(row - 1);
    updatePreview();
}

void ContainerSettingsDialog::onMoveItemDown()
{
    const int row = m_itemList->currentRow();
    if (row < 0 || row >= m_workingItems.size() - 1) {
        return;
    }

    m_workingItems.swapItemsAt(row, row + 1);
    refreshItemList();
    m_itemList->setCurrentRow(row + 1);
    updatePreview();
}

// ---------------------------------------------------------------------------
// addNewItem — create and insert a new item after the current selection
// ---------------------------------------------------------------------------

void ContainerSettingsDialog::addNewItem(const QString& typeTag)
{
    // Calculate yPos: max (y + height) of existing items, clamped to 0.9
    float yPos = 0.0f;
    for (const MeterItem* item : m_workingItems) {
        const float bottom = item->y() + item->itemHeight();
        if (bottom > yPos) {
            yPos = bottom;
        }
    }
    if (yPos > 0.9f) {
        yPos = 0.9f;
    }

    MeterItem* newItem = nullptr;

    if (typeTag == QLatin1String("BAR")) {
        auto* item = new BarItem();
        item->setRect(0.0f, yPos, 1.0f, 0.15f);
        item->setRange(-140.0, 0.0);
        item->setBindingId(0);
        newItem = item;
    } else if (typeTag == QLatin1String("NEEDLE")) {
        auto* item = new NeedleItem();
        item->setRect(0.0f, yPos, 1.0f, 0.5f);
        item->setBindingId(0);
        newItem = item;
    } else if (typeTag == QLatin1String("SOLID")) {
        auto* item = new SolidColourItem();
        item->setRect(0.0f, 0.0f, 1.0f, 1.0f);
        item->setZOrder(-10);
        newItem = item;
    } else if (typeTag == QLatin1String("TEXT")) {
        auto* item = new TextItem();
        item->setRect(0.0f, yPos, 1.0f, 0.1f);
        item->setBindingId(0);
        newItem = item;
    } else if (typeTag == QLatin1String("SCALE")) {
        auto* item = new ScaleItem();
        item->setRect(0.0f, yPos, 1.0f, 0.12f);
        newItem = item;
    } else if (typeTag == QLatin1String("IMAGE")) {
        auto* item = new ImageItem();
        item->setRect(0.0f, 0.0f, 1.0f, 1.0f);
        item->setZOrder(-5);
        newItem = item;
    } else {
        newItem = createDefaultItem(typeTag);
        if (newItem) {
            newItem->setRect(0.0f, yPos, 1.0f, 0.2f);
        }
    }

    if (!newItem) {
        return;
    }

    // Insert after current selection, or at end if nothing is selected
    const int currentRow = m_itemList->currentRow();
    const int insertAt = (currentRow >= 0) ? currentRow + 1 : m_workingItems.size();
    m_workingItems.insert(insertAt, newItem);

    refreshItemList();
    m_itemList->setCurrentRow(insertAt);
    updatePreview();
}

// ---------------------------------------------------------------------------
// createDefaultItem — static factory for types not special-cased in addNewItem
// ---------------------------------------------------------------------------

MeterItem* ContainerSettingsDialog::createDefaultItem(const QString& typeTag)
{
    if (typeTag == QLatin1String("SPACER")) {
        return new SpacerItem();
    } else if (typeTag == QLatin1String("FADECOVER")) {
        return new FadeCoverItem();
    } else if (typeTag == QLatin1String("LED")) {
        return new LEDItem();
    } else if (typeTag == QLatin1String("HISTORY")) {
        return new HistoryGraphItem();
    } else if (typeTag == QLatin1String("MAGICEYE")) {
        return new MagicEyeItem();
    } else if (typeTag == QLatin1String("NEEDLESCALEPWR")) {
        return new NeedleScalePwrItem();
    } else if (typeTag == QLatin1String("SIGNALTEXT")) {
        return new SignalTextItem();
    } else if (typeTag == QLatin1String("DIAL")) {
        return new DialItem();
    } else if (typeTag == QLatin1String("TEXTOVERLAY")) {
        return new TextOverlayItem();
    } else if (typeTag == QLatin1String("WEBIMAGE")) {
        return new WebImageItem();
    } else if (typeTag == QLatin1String("FILTERDISPLAY")) {
        return new FilterDisplayItem();
    } else if (typeTag == QLatin1String("ROTATOR")) {
        return new RotatorItem();
    } else if (typeTag == QLatin1String("BANDBTNS")) {
        return new BandButtonItem();
    } else if (typeTag == QLatin1String("MODEBTNS")) {
        return new ModeButtonItem();
    } else if (typeTag == QLatin1String("FILTERBTNS")) {
        return new FilterButtonItem();
    } else if (typeTag == QLatin1String("ANTENNABTNS")) {
        return new AntennaButtonItem();
    } else if (typeTag == QLatin1String("TUNESTEPBTNS")) {
        return new TuneStepButtonItem();
    } else if (typeTag == QLatin1String("OTHERBTNS")) {
        return new OtherButtonItem();
    } else if (typeTag == QLatin1String("VOICERECPLAY")) {
        return new VoiceRecordPlayItem();
    } else if (typeTag == QLatin1String("DISCORDBTNS")) {
        return new DiscordButtonItem();
    } else if (typeTag == QLatin1String("VFO")) {
        return new VfoDisplayItem();
    } else if (typeTag == QLatin1String("CLOCK")) {
        return new ClockItem();
    } else if (typeTag == QLatin1String("CLICKBOX")) {
        return new ClickBoxItem();
    } else if (typeTag == QLatin1String("DATAOUT")) {
        return new DataOutItem();
    }

    return nullptr;
}

// ---------------------------------------------------------------------------
// createItemFromSerialized — factory matching ItemGroup::deserialize() pattern
// ---------------------------------------------------------------------------

MeterItem* ContainerSettingsDialog::createItemFromSerialized(const QString& data)
{
    if (data.isEmpty()) {
        return nullptr;
    }

    const int pipeIdx = data.indexOf(QLatin1Char('|'));
    const QString typeTag = (pipeIdx >= 0) ? data.left(pipeIdx) : data;

    // Core types (defined in MeterItem.h)
    if (typeTag == QLatin1String("BAR")) {
        BarItem* item = new BarItem();
        if (item->deserialize(data)) { return item; }
        delete item;
    } else if (typeTag == QLatin1String("SOLID")) {
        SolidColourItem* item = new SolidColourItem();
        if (item->deserialize(data)) { return item; }
        delete item;
    } else if (typeTag == QLatin1String("IMAGE")) {
        ImageItem* item = new ImageItem();
        if (item->deserialize(data)) { return item; }
        delete item;
    } else if (typeTag == QLatin1String("SCALE")) {
        ScaleItem* item = new ScaleItem();
        if (item->deserialize(data)) { return item; }
        delete item;
    } else if (typeTag == QLatin1String("TEXT")) {
        TextItem* item = new TextItem();
        if (item->deserialize(data)) { return item; }
        delete item;
    } else if (typeTag == QLatin1String("NEEDLE")) {
        NeedleItem* item = new NeedleItem();
        if (item->deserialize(data)) { return item; }
        delete item;
    }
    // Phase 3G-4 passive types
    else if (typeTag == QLatin1String("SPACER")) {
        SpacerItem* item = new SpacerItem();
        if (item->deserialize(data)) { return item; }
        delete item;
    } else if (typeTag == QLatin1String("FADECOVER")) {
        FadeCoverItem* item = new FadeCoverItem();
        if (item->deserialize(data)) { return item; }
        delete item;
    } else if (typeTag == QLatin1String("LED")) {
        LEDItem* item = new LEDItem();
        if (item->deserialize(data)) { return item; }
        delete item;
    } else if (typeTag == QLatin1String("HISTORY")) {
        HistoryGraphItem* item = new HistoryGraphItem();
        if (item->deserialize(data)) { return item; }
        delete item;
    } else if (typeTag == QLatin1String("MAGICEYE")) {
        MagicEyeItem* item = new MagicEyeItem();
        if (item->deserialize(data)) { return item; }
        delete item;
    } else if (typeTag == QLatin1String("NEEDLESCALEPWR")) {
        NeedleScalePwrItem* item = new NeedleScalePwrItem();
        if (item->deserialize(data)) { return item; }
        delete item;
    } else if (typeTag == QLatin1String("SIGNALTEXT")) {
        SignalTextItem* item = new SignalTextItem();
        if (item->deserialize(data)) { return item; }
        delete item;
    } else if (typeTag == QLatin1String("DIAL")) {
        DialItem* item = new DialItem();
        if (item->deserialize(data)) { return item; }
        delete item;
    } else if (typeTag == QLatin1String("TEXTOVERLAY")) {
        TextOverlayItem* item = new TextOverlayItem();
        if (item->deserialize(data)) { return item; }
        delete item;
    } else if (typeTag == QLatin1String("WEBIMAGE")) {
        WebImageItem* item = new WebImageItem();
        if (item->deserialize(data)) { return item; }
        delete item;
    } else if (typeTag == QLatin1String("FILTERDISPLAY")) {
        FilterDisplayItem* item = new FilterDisplayItem();
        if (item->deserialize(data)) { return item; }
        delete item;
    } else if (typeTag == QLatin1String("ROTATOR")) {
        RotatorItem* item = new RotatorItem();
        if (item->deserialize(data)) { return item; }
        delete item;
    }
    // Phase 3G-5 interactive types
    else if (typeTag == QLatin1String("BANDBTNS")) {
        BandButtonItem* item = new BandButtonItem();
        if (item->deserialize(data)) { return item; }
        delete item;
    } else if (typeTag == QLatin1String("MODEBTNS")) {
        ModeButtonItem* item = new ModeButtonItem();
        if (item->deserialize(data)) { return item; }
        delete item;
    } else if (typeTag == QLatin1String("FILTERBTNS")) {
        FilterButtonItem* item = new FilterButtonItem();
        if (item->deserialize(data)) { return item; }
        delete item;
    } else if (typeTag == QLatin1String("ANTENNABTNS")) {
        AntennaButtonItem* item = new AntennaButtonItem();
        if (item->deserialize(data)) { return item; }
        delete item;
    } else if (typeTag == QLatin1String("TUNESTEPBTNS")) {
        TuneStepButtonItem* item = new TuneStepButtonItem();
        if (item->deserialize(data)) { return item; }
        delete item;
    } else if (typeTag == QLatin1String("OTHERBTNS")) {
        OtherButtonItem* item = new OtherButtonItem();
        if (item->deserialize(data)) { return item; }
        delete item;
    } else if (typeTag == QLatin1String("VOICERECPLAY")) {
        VoiceRecordPlayItem* item = new VoiceRecordPlayItem();
        if (item->deserialize(data)) { return item; }
        delete item;
    } else if (typeTag == QLatin1String("DISCORDBTNS")) {
        DiscordButtonItem* item = new DiscordButtonItem();
        if (item->deserialize(data)) { return item; }
        delete item;
    } else if (typeTag == QLatin1String("VFO")) {
        VfoDisplayItem* item = new VfoDisplayItem();
        if (item->deserialize(data)) { return item; }
        delete item;
    } else if (typeTag == QLatin1String("CLOCK")) {
        ClockItem* item = new ClockItem();
        if (item->deserialize(data)) { return item; }
        delete item;
    } else if (typeTag == QLatin1String("CLICKBOX")) {
        ClickBoxItem* item = new ClickBoxItem();
        if (item->deserialize(data)) { return item; }
        delete item;
    } else if (typeTag == QLatin1String("DATAOUT")) {
        DataOutItem* item = new DataOutItem();
        if (item->deserialize(data)) { return item; }
        delete item;
    }

    return nullptr;
}

// ---------------------------------------------------------------------------
// typeTagDisplayName — human-readable label for each type tag
// ---------------------------------------------------------------------------

QString ContainerSettingsDialog::typeTagDisplayName(const QString& tag)
{
    static const QMap<QString, QString> kDisplayNames = {
        // Core types
        { QStringLiteral("BAR"),           QStringLiteral("Bar Meter") },
        { QStringLiteral("SOLID"),         QStringLiteral("Solid Colour") },
        { QStringLiteral("IMAGE"),         QStringLiteral("Image") },
        { QStringLiteral("SCALE"),         QStringLiteral("Scale") },
        { QStringLiteral("TEXT"),          QStringLiteral("Text Readout") },
        { QStringLiteral("NEEDLE"),        QStringLiteral("Needle Meter") },
        // Phase 3G-4 passive types
        { QStringLiteral("SPACER"),        QStringLiteral("Spacer") },
        { QStringLiteral("FADECOVER"),     QStringLiteral("Fade Cover") },
        { QStringLiteral("LED"),           QStringLiteral("LED Indicator") },
        { QStringLiteral("HISTORY"),       QStringLiteral("History Graph") },
        { QStringLiteral("MAGICEYE"),      QStringLiteral("Magic Eye") },
        { QStringLiteral("NEEDLESCALEPWR"),QStringLiteral("Needle Scale Power") },
        { QStringLiteral("SIGNALTEXT"),    QStringLiteral("Signal Text") },
        { QStringLiteral("DIAL"),          QStringLiteral("Dial") },
        { QStringLiteral("TEXTOVERLAY"),   QStringLiteral("Text Overlay") },
        { QStringLiteral("WEBIMAGE"),      QStringLiteral("Web Image") },
        { QStringLiteral("FILTERDISPLAY"), QStringLiteral("Filter Display") },
        { QStringLiteral("ROTATOR"),       QStringLiteral("Rotator") },
        // Phase 3G-5 interactive types
        { QStringLiteral("BANDBTNS"),      QStringLiteral("Band Buttons") },
        { QStringLiteral("MODEBTNS"),      QStringLiteral("Mode Buttons") },
        { QStringLiteral("FILTERBTNS"),    QStringLiteral("Filter Buttons") },
        { QStringLiteral("ANTENNABTNS"),   QStringLiteral("Antenna Buttons") },
        { QStringLiteral("TUNESTEPBTNS"),  QStringLiteral("Tune Step Buttons") },
        { QStringLiteral("OTHERBTNS"),     QStringLiteral("Other Buttons") },
        { QStringLiteral("VOICERECPLAY"),  QStringLiteral("Voice Rec/Play") },
        { QStringLiteral("DISCORDBTNS"),   QStringLiteral("Discord Buttons") },
        { QStringLiteral("VFO"),           QStringLiteral("VFO Display") },
        { QStringLiteral("CLOCK"),         QStringLiteral("Clock") },
        { QStringLiteral("CLICKBOX"),      QStringLiteral("Click Box") },
        { QStringLiteral("DATAOUT"),       QStringLiteral("Data Out") },
    };

    const auto it = kDisplayNames.constFind(tag);
    if (it != kDisplayNames.constEnd()) {
        return it.value();
    }
    return tag;  // fall back to raw tag if unknown
}

// ---------------------------------------------------------------------------
// refreshItemList — rebuild the QListWidget from m_workingItems
// ---------------------------------------------------------------------------

void ContainerSettingsDialog::refreshItemList()
{
    m_itemList->clear();
    for (const MeterItem* item : m_workingItems) {
        // Derive the type tag from serialize() (first pipe-delimited field)
        const QString serialized = item->serialize();
        const int pipeIdx = serialized.indexOf(QLatin1Char('|'));
        const QString typeTag = (pipeIdx >= 0) ? serialized.left(pipeIdx) : serialized;

        const QString displayName = typeTagDisplayName(typeTag);
        const int bindingId = item->bindingId();

        QString label = displayName;
        if (bindingId >= 0) {
            label += QStringLiteral(" [id:%1]").arg(bindingId);
        }
        m_itemList->addItem(label);
    }
}

// ---------------------------------------------------------------------------
// Helpers (stubs / partial — filled in later tasks)
// ---------------------------------------------------------------------------

MeterWidget* ContainerSettingsDialog::findMeterWidget() const
{
    if (!m_container) {
        return nullptr;
    }

    // Direct content: container holds a MeterWidget (floating containers, new containers)
    MeterWidget* meter = qobject_cast<MeterWidget*>(m_container->content());
    if (meter) {
        return meter;
    }

    // Nested: Container #0 has an AppletPanelWidget with MeterWidget as header child.
    // findChild traverses the widget tree to locate it.
    QWidget* content = m_container->content();
    if (content) {
        meter = content->findChild<MeterWidget*>();
    }
    return meter;
}

void ContainerSettingsDialog::populateItemList()
{
    qDeleteAll(m_workingItems);
    m_workingItems.clear();

    MeterWidget* meter = findMeterWidget();
    if (!meter) {
        return;
    }

    const QString serialized = meter->serializeItems();
    const QStringList lines = serialized.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        MeterItem* item = createItemFromSerialized(line);
        if (item) {
            m_workingItems.append(item);
        }
    }

    refreshItemList();
    updatePreview();
}

void ContainerSettingsDialog::updatePreview()
{
    // Phase 3G-6 block 3 commit 11: live preview removed. In-place
    // editing with snapshot/revert (commit 14) will push working-item
    // changes directly to the target container's MeterWidget; until
    // then this is a no-op so the existing ~30 callsites keep
    // compiling without being rewritten one-by-one.
}

// ---------------------------------------------------------------------------
// Phase 3G-6 block 3 commit 14: snapshot + revert
// ---------------------------------------------------------------------------

void ContainerSettingsDialog::takeSnapshot()
{
    if (!m_container) {
        m_snapshotTaken = false;
        return;
    }
    m_containerSnapshot = m_container->serialize();
    MeterWidget* meter = findMeterWidget();
    m_itemsSnapshot = meter ? meter->serializeItems() : QString();
    m_snapshotTaken = true;
}

void ContainerSettingsDialog::revertFromSnapshot()
{
    if (!m_snapshotTaken || !m_container) { return; }
    m_container->deserialize(m_containerSnapshot);
    if (MeterWidget* meter = findMeterWidget()) {
        meter->deserializeItems(m_itemsSnapshot);
        meter->update();
    }
    m_container->update();
}

void ContainerSettingsDialog::applyToContainer()
{
    if (!m_container) {
        return;
    }

    // Apply container-level properties
    m_container->setNotes(m_titleEdit->text());
    m_container->setBorder(m_borderCheck->isChecked());

    const int rxData = m_rxSourceCombo->currentData().toInt();
    m_container->setRxSource(rxData);

    m_container->setShowOnRx(m_showOnRxCheck->isChecked());
    m_container->setShowOnTx(m_showOnTxCheck->isChecked());

    // Write items back to the real MeterWidget
    MeterWidget* target = findMeterWidget();
    if (!target) {
        return;
    }

    target->clearItems();
    for (const MeterItem* item : m_workingItems) {
        const QString data = item->serialize();
        MeterItem* clone = createItemFromSerialized(data);
        if (clone) {
            target->addItem(clone);
            // Re-wire interactive item signals through the container
            m_container->wireInteractiveItem(clone);
        }
    }
    target->update();
}

// ---------------------------------------------------------------------------
// Task 6: Preset browser
// ---------------------------------------------------------------------------

void ContainerSettingsDialog::onLoadPreset()
{
    constexpr const char* kMenuStyle =
        "QMenu { background: #1a2a3a; color: #c8d8e8; border: 1px solid #205070; }"
        "QMenu::item:selected { background: #00b4d8; color: #0f0f1a; }"
        "QMenu::separator { background: #203040; height: 1px; }";

    QMenu menu(this);
    menu.setStyleSheet(QLatin1String(kMenuStyle));

    // S-Meter sub-menu
    QMenu* sMeterMenu = menu.addMenu(QStringLiteral("S-Meter"));
    sMeterMenu->setStyleSheet(QLatin1String(kMenuStyle));
    sMeterMenu->addAction(QStringLiteral("S-Meter Only"),        this, [this]{ loadPresetByName(QStringLiteral("SMeterOnly")); });
    sMeterMenu->addAction(QStringLiteral("S-Meter Bar Signal"),  this, [this]{ loadPresetByName(QStringLiteral("SignalBar")); });
    sMeterMenu->addAction(QStringLiteral("S-Meter Bar Avg"),     this, [this]{ loadPresetByName(QStringLiteral("AvgSignalBar")); });
    sMeterMenu->addAction(QStringLiteral("S-Meter Bar MaxBin"),  this, [this]{ loadPresetByName(QStringLiteral("MaxBinBar")); });
    sMeterMenu->addAction(QStringLiteral("S-Meter Text"),        this, [this]{ loadPresetByName(QStringLiteral("SignalText")); });

    // Composite sub-menu
    QMenu* compositeMenu = menu.addMenu(QStringLiteral("Composite"));
    compositeMenu->setStyleSheet(QLatin1String(kMenuStyle));
    compositeMenu->addAction(QStringLiteral("ANAN Multi Meter"), this, [this]{ loadPresetByName(QStringLiteral("AnanMM")); });
    compositeMenu->addAction(QStringLiteral("Cross Needle"),     this, [this]{ loadPresetByName(QStringLiteral("CrossNeedle")); });
    compositeMenu->addAction(QStringLiteral("Magic Eye"),        this, [this]{ loadPresetByName(QStringLiteral("MagicEye")); });
    compositeMenu->addAction(QStringLiteral("History"),          this, [this]{ loadPresetByName(QStringLiteral("History")); });

    // TX Meters sub-menu
    QMenu* txMenu = menu.addMenu(QStringLiteral("TX Meters"));
    txMenu->setStyleSheet(QLatin1String(kMenuStyle));
    txMenu->addAction(QStringLiteral("Power/SWR"),      this, [this]{ loadPresetByName(QStringLiteral("PowerSwr")); });
    txMenu->addAction(QStringLiteral("ALC"),            this, [this]{ loadPresetByName(QStringLiteral("Alc")); });
    txMenu->addAction(QStringLiteral("ALC Gain"),       this, [this]{ loadPresetByName(QStringLiteral("AlcGain")); });
    txMenu->addAction(QStringLiteral("ALC Group"),      this, [this]{ loadPresetByName(QStringLiteral("AlcGroup")); });
    txMenu->addAction(QStringLiteral("Mic Level"),      this, [this]{ loadPresetByName(QStringLiteral("Mic")); });
    txMenu->addAction(QStringLiteral("Compressor"),     this, [this]{ loadPresetByName(QStringLiteral("Comp")); });
    txMenu->addAction(QStringLiteral("EQ Level"),       this, [this]{ loadPresetByName(QStringLiteral("Eq")); });
    txMenu->addAction(QStringLiteral("Leveler"),        this, [this]{ loadPresetByName(QStringLiteral("Leveler")); });
    txMenu->addAction(QStringLiteral("Leveler Gain"),   this, [this]{ loadPresetByName(QStringLiteral("LevelerGain")); });
    txMenu->addAction(QStringLiteral("CFC"),            this, [this]{ loadPresetByName(QStringLiteral("Cfc")); });
    txMenu->addAction(QStringLiteral("CFC Gain"),       this, [this]{ loadPresetByName(QStringLiteral("CfcGain")); });

    // RX Meters sub-menu
    QMenu* rxMenu = menu.addMenu(QStringLiteral("RX Meters"));
    rxMenu->setStyleSheet(QLatin1String(kMenuStyle));
    rxMenu->addAction(QStringLiteral("ADC"),         this, [this]{ loadPresetByName(QStringLiteral("Adc")); });
    rxMenu->addAction(QStringLiteral("ADC MaxMag"),  this, [this]{ loadPresetByName(QStringLiteral("AdcMaxMag")); });
    rxMenu->addAction(QStringLiteral("AGC"),         this, [this]{ loadPresetByName(QStringLiteral("Agc")); });
    rxMenu->addAction(QStringLiteral("AGC Gain"),    this, [this]{ loadPresetByName(QStringLiteral("AgcGain")); });
    rxMenu->addAction(QStringLiteral("PBSNR"),       this, [this]{ loadPresetByName(QStringLiteral("Pbsnr")); });

    // Interactive sub-menu
    QMenu* interactiveMenu = menu.addMenu(QStringLiteral("Interactive"));
    interactiveMenu->setStyleSheet(QLatin1String(kMenuStyle));
    interactiveMenu->addAction(QStringLiteral("VFO Display"), this, [this]{ loadPresetByName(QStringLiteral("Vfo")); });
    interactiveMenu->addAction(QStringLiteral("Clock"),       this, [this]{ loadPresetByName(QStringLiteral("Clock")); });
    interactiveMenu->addAction(QStringLiteral("Contest"),     this, [this]{ loadPresetByName(QStringLiteral("Contest")); });

    // Display sub-menu
    QMenu* displayMenu = menu.addMenu(QStringLiteral("Display"));
    displayMenu->setStyleSheet(QLatin1String(kMenuStyle));
    displayMenu->addAction(QStringLiteral("Rotator"),        this, [this]{ loadPresetByName(QStringLiteral("Rotator")); });
    displayMenu->addAction(QStringLiteral("Filter Display"), this, [this]{ loadPresetByName(QStringLiteral("FilterDisplay")); });

    // Top-level
    menu.addSeparator();
    menu.addAction(QStringLiteral("Spacer"), this, [this]{ loadPresetByName(QStringLiteral("Spacer")); });

    menu.exec(m_btnPreset->mapToGlobal(QPoint(0, -menu.sizeHint().height())));
}

void ContainerSettingsDialog::loadPresetByName(const QString& name)
{
    if (!m_workingItems.isEmpty()) {
        const QMessageBox::StandardButton result = QMessageBox::question(
            this,
            QStringLiteral("Load Preset"),
            QStringLiteral("This will replace all current items. Continue?"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (result != QMessageBox::Yes) {
            return;
        }
    }

    qDeleteAll(m_workingItems);
    m_workingItems.clear();

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
        auto* item = new RotatorItem();
        item->setRect(0.0f, 0.0f, 1.0f, 1.0f);
        item->setBindingId(300);
        m_workingItems.append(item);
    } else if (name == QLatin1String("FilterDisplay")) {
        auto* item = new FilterDisplayItem();
        item->setRect(0.0f, 0.0f, 1.0f, 1.0f);
        m_workingItems.append(item);
    }

    if (group) {
        for (MeterItem* src : group->items()) {
            MeterItem* clone = createItemFromSerialized(src->serialize());
            if (clone) {
                m_workingItems.append(clone);
            }
        }
        delete group;
    }

    refreshItemList();
    if (!m_workingItems.isEmpty()) {
        m_itemList->setCurrentRow(0);
    }
    updatePreview();
}

// ---------------------------------------------------------------------------
// Task 7: Import/Export via Base64
// ---------------------------------------------------------------------------

void ContainerSettingsDialog::onExport()
{
    QStringList lines;
    for (const MeterItem* item : m_workingItems) {
        lines << item->serialize();
    }
    const QString raw = lines.join(QLatin1Char('\n'));
    const QString encoded = QString::fromLatin1(raw.toUtf8().toBase64());
    QApplication::clipboard()->setText(encoded);
    QMessageBox::information(
        this,
        QStringLiteral("Export"),
        QStringLiteral("Layout copied to clipboard as Base64.\nPaste it into another container to import."));
}

void ContainerSettingsDialog::onImport()
{
    const QString clipText = QApplication::clipboard()->text().trimmed();
    if (clipText.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Import"),
                             QStringLiteral("Clipboard is empty."));
        return;
    }

    const QByteArray decoded = QByteArray::fromBase64(clipText.toUtf8());
    const QString raw = QString::fromUtf8(decoded);
    const QStringList lines = raw.split(QLatin1Char('\n'), Qt::SkipEmptyParts);

    QVector<MeterItem*> imported;
    for (const QString& line : lines) {
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

    const QMessageBox::StandardButton result = QMessageBox::question(
        this,
        QStringLiteral("Import"),
        QStringLiteral("Replace all current items with %1 imported item(s)?")
            .arg(imported.size()),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (result != QMessageBox::Yes) {
        qDeleteAll(imported);
        return;
    }

    qDeleteAll(m_workingItems);
    m_workingItems.clear();
    m_workingItems = imported;

    refreshItemList();
    if (!m_workingItems.isEmpty()) {
        m_itemList->setCurrentRow(0);
    }
    updatePreview();
}

// ---------------------------------------------------------------------------
// buildCommonPropsPage — property editor page added to m_propertyStack
// ---------------------------------------------------------------------------

void ContainerSettingsDialog::buildCommonPropsPage()
{
    if (m_commonPropsPage) {
        return;  // Already built
    }

    // Wrap in a scroll area so small dialogs don't clip content.
    // m_commonPropsPage must point to the scroll area (the widget added to
    // m_propertyStack) so that setCurrentWidget(m_commonPropsPage) works.
    QScrollArea* scroll = new QScrollArea(m_propertyStack);
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(
        "QScrollArea { background: #0a0a18; border: none; }"
        "QScrollBar:vertical { background: #111122; width: 8px; }"
        "QScrollBar::handle:vertical { background: #203040; border-radius: 3px; }");

    m_commonPropsPage = scroll;  // Stack switches on this widget

    QWidget* innerPage = new QWidget();
    innerPage->setStyleSheet("background: #0a0a18;");
    scroll->setWidget(innerPage);

    QVBoxLayout* pageLayout = new QVBoxLayout(innerPage);
    pageLayout->setContentsMargins(8, 8, 8, 8);
    pageLayout->setSpacing(6);

    // --- Position section ---
    QLabel* posHeader = new QLabel(QStringLiteral("Position (normalized 0\u20131)"), innerPage);
    posHeader->setStyleSheet(kSectionHeaderStyle);
    pageLayout->addWidget(posHeader);

    auto makeDoubleSpinBox = [&](double min, double max, double step, int decimals) -> QDoubleSpinBox* {
        QDoubleSpinBox* sb = new QDoubleSpinBox(innerPage);
        sb->setRange(min, max);
        sb->setSingleStep(step);
        sb->setDecimals(decimals);
        sb->setStyleSheet(kSpinStyle);
        return sb;
    };

    QGridLayout* posGrid = new QGridLayout();
    posGrid->setSpacing(4);

    QLabel* xLabel = new QLabel(QStringLiteral("X:"), innerPage);
    xLabel->setStyleSheet(kLabelStyle);
    QLabel* yLabel = new QLabel(QStringLiteral("Y:"), innerPage);
    yLabel->setStyleSheet(kLabelStyle);
    QLabel* wLabel = new QLabel(QStringLiteral("W:"), innerPage);
    wLabel->setStyleSheet(kLabelStyle);
    QLabel* hLabel = new QLabel(QStringLiteral("H:"), innerPage);
    hLabel->setStyleSheet(kLabelStyle);

    m_propX = makeDoubleSpinBox(0.0, 1.0, 0.01, 3);
    m_propY = makeDoubleSpinBox(0.0, 1.0, 0.01, 3);
    m_propW = makeDoubleSpinBox(0.0, 1.0, 0.01, 3);
    m_propH = makeDoubleSpinBox(0.0, 1.0, 0.01, 3);

    posGrid->addWidget(xLabel, 0, 0);
    posGrid->addWidget(m_propX, 0, 1);
    posGrid->addWidget(yLabel, 0, 2);
    posGrid->addWidget(m_propY, 0, 3);
    posGrid->addWidget(wLabel, 1, 0);
    posGrid->addWidget(m_propW, 1, 1);
    posGrid->addWidget(hLabel, 1, 2);
    posGrid->addWidget(m_propH, 1, 3);
    pageLayout->addLayout(posGrid);

    // --- Z-Order section ---
    QLabel* zHeader = new QLabel(QStringLiteral("Z-Order"), innerPage);
    zHeader->setStyleSheet(kSectionHeaderStyle);
    pageLayout->addWidget(zHeader);

    QHBoxLayout* zRow = new QHBoxLayout();
    QLabel* zLabel = new QLabel(QStringLiteral("Z:"), innerPage);
    zLabel->setStyleSheet(kLabelStyle);
    m_propZOrder = new QSpinBox(innerPage);
    m_propZOrder->setRange(-100, 100);
    m_propZOrder->setStyleSheet(kSpinStyle);
    zRow->addWidget(zLabel);
    zRow->addWidget(m_propZOrder);
    zRow->addStretch();
    pageLayout->addLayout(zRow);

    // --- Binding section ---
    QLabel* bindHeader = new QLabel(QStringLiteral("Data Binding"), innerPage);
    bindHeader->setStyleSheet(kSectionHeaderStyle);
    pageLayout->addWidget(bindHeader);

    QHBoxLayout* bindRow = new QHBoxLayout();
    QLabel* bindLabel = new QLabel(QStringLiteral("Source:"), innerPage);
    bindLabel->setStyleSheet(kLabelStyle);
    m_propBinding = new QComboBox(innerPage);
    m_propBinding->setStyleSheet(
        "QComboBox { background: #0a0a18; color: #c8d8e8;"
        "  border: 1px solid #1e2e3e; border-radius: 3px; padding: 2px; }"
        "QComboBox QAbstractItemView { background: #0a0a18; color: #c8d8e8;"
        "  border: 1px solid #205070; selection-background-color: #00b4d8; }");
    populateBindingCombo();
    bindRow->addWidget(bindLabel);
    bindRow->addWidget(m_propBinding, 1);
    pageLayout->addLayout(bindRow);

    // --- Separator ---
    QFrame* sep = new QFrame(innerPage);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("QFrame { color: #203040; }");
    pageLayout->addWidget(sep);

    // --- Type-specific area ---
    m_typePropsContainer = new QWidget(innerPage);
    m_typePropsLayout = new QVBoxLayout(m_typePropsContainer);
    m_typePropsLayout->setContentsMargins(0, 0, 0, 0);
    m_typePropsLayout->setSpacing(4);
    pageLayout->addWidget(m_typePropsContainer);

    // Stretch at bottom
    pageLayout->addStretch();

    // Connect all controls to save + preview (no list rebuild to avoid re-trigger)
    auto connectSave = [this]() {
        const int row = m_itemList->currentRow();
        if (row >= 0 && row < m_workingItems.size()) {
            saveCommonProperties(row);
            updatePreview();
        }
    };

    connect(m_propX,       QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, connectSave);
    connect(m_propY,       QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, connectSave);
    connect(m_propW,       QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, connectSave);
    connect(m_propH,       QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, connectSave);
    connect(m_propZOrder,  QOverload<int>::of(&QSpinBox::valueChanged),          this, connectSave);
    connect(m_propBinding, QOverload<int>::of(&QComboBox::currentIndexChanged),  this, connectSave);

    // Add scroll area (m_commonPropsPage) to stack
    m_propertyStack->addWidget(m_commonPropsPage);
}

// ---------------------------------------------------------------------------
// populateBindingCombo — fill the data-source combo box
// ---------------------------------------------------------------------------

void ContainerSettingsDialog::populateBindingCombo()
{
    if (!m_propBinding) {
        return;
    }

    struct BindingEntry { const char* name; int id; };
    static constexpr BindingEntry kEntries[] = {
        { "None",              -1  },
        { "Signal Peak",        0  },
        { "Signal Avg",         1  },
        { "ADC Peak",           2  },
        { "ADC Avg",            3  },
        { "AGC Gain",           4  },
        { "AGC Peak",           5  },
        { "AGC Avg",            6  },
        { "Signal MaxBin",      7  },
        { "PBSNR",              8  },
        { "TX Power",         100  },
        { "TX Reverse Power", 101  },
        { "TX SWR",           102  },
        { "TX Mic",           103  },
        { "TX Comp",          104  },
        { "TX ALC",           105  },
        { "TX EQ",            106  },
        { "TX Leveler",       107  },
        { "TX Leveler Gain",  108  },
        { "TX ALC Gain",      109  },
        { "TX ALC Group",     110  },
        { "TX CFC",           111  },
        { "TX CFC Gain",      112  },
        { "HW Volts",         200  },
        { "HW Amps",          201  },
        { "HW Temperature",   202  },
        { "Rotator Az",       300  },
        { "Rotator Ele",      301  },
    };

    m_propBinding->clear();
    for (const BindingEntry& e : kEntries) {
        m_propBinding->addItem(QString::fromLatin1(e.name), e.id);
    }
}

// ---------------------------------------------------------------------------
// loadCommonProperties — populate controls from m_workingItems[row]
// ---------------------------------------------------------------------------

void ContainerSettingsDialog::loadCommonProperties(int row)
{
    if (row < 0 || row >= m_workingItems.size()) {
        return;
    }

    MeterItem* item = m_workingItems[row];

    QSignalBlocker bX(m_propX);
    QSignalBlocker bY(m_propY);
    QSignalBlocker bW(m_propW);
    QSignalBlocker bH(m_propH);
    QSignalBlocker bZ(m_propZOrder);
    QSignalBlocker bBind(m_propBinding);

    m_propX->setValue(static_cast<double>(item->x()));
    m_propY->setValue(static_cast<double>(item->y()));
    m_propW->setValue(static_cast<double>(item->itemWidth()));
    m_propH->setValue(static_cast<double>(item->itemHeight()));
    m_propZOrder->setValue(item->zOrder());

    // Find binding index by data value
    const int bindId = item->bindingId();
    const int comboIdx = m_propBinding->findData(bindId);
    m_propBinding->setCurrentIndex(comboIdx >= 0 ? comboIdx : 0);
}

// ---------------------------------------------------------------------------
// saveCommonProperties — read controls and apply to m_workingItems[row]
// ---------------------------------------------------------------------------

void ContainerSettingsDialog::saveCommonProperties(int row)
{
    if (row < 0 || row >= m_workingItems.size()) {
        return;
    }

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

// ---------------------------------------------------------------------------
// buildTypeSpecificEditor — dispatch to per-type editor builders (Task 5)
// ---------------------------------------------------------------------------

QWidget* ContainerSettingsDialog::buildTypeSpecificEditor(MeterItem* item)
{
    if (!item) {
        return nullptr;
    }

    const QString serialized = item->serialize();
    const int pipeIdx = serialized.indexOf(QLatin1Char('|'));
    const QString typeTag = (pipeIdx >= 0) ? serialized.left(pipeIdx) : serialized;

    if (typeTag == QLatin1String("BAR")) {
        return buildBarItemEditor(static_cast<BarItem*>(item));
    } else if (typeTag == QLatin1String("NEEDLE")) {
        return buildNeedleItemEditor(static_cast<NeedleItem*>(item));
    } else if (typeTag == QLatin1String("TEXT")) {
        return buildTextItemEditor(static_cast<TextItem*>(item));
    } else if (typeTag == QLatin1String("SCALE")) {
        return buildScaleItemEditor(static_cast<ScaleItem*>(item));
    } else if (typeTag == QLatin1String("SOLID")) {
        return buildSolidItemEditor(static_cast<SolidColourItem*>(item));
    } else if (typeTag == QLatin1String("LED")) {
        return buildLedItemEditor(static_cast<LEDItem*>(item));
    }

    return nullptr;
}

// ---------------------------------------------------------------------------
// Helper: create a color picker button
// ---------------------------------------------------------------------------

namespace {

QPushButton* makeColorBtn(const QColor& color, QWidget* parent)
{
    QPushButton* btn = new QPushButton(parent);
    btn->setFixedSize(40, 22);
    btn->setAutoDefault(false);
    btn->setDefault(false);
    btn->setStyleSheet(
        QStringLiteral("background: %1; border: 1px solid #205070; border-radius: 3px;")
            .arg(color.name()));
    return btn;
}

QLabel* makePropLabel(const QString& text, QWidget* parent)
{
    QLabel* lbl = new QLabel(text, parent);
    lbl->setStyleSheet(kLabelStyle);
    return lbl;
}

QLabel* makeSectionLabel(const QString& text, QWidget* parent)
{
    QLabel* lbl = new QLabel(text, parent);
    lbl->setStyleSheet(kSectionHeaderStyle);
    return lbl;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// buildBarItemEditor
// ---------------------------------------------------------------------------

QWidget* ContainerSettingsDialog::buildBarItemEditor(BarItem* item)
{
    QWidget* w = new QWidget(m_typePropsContainer);
    QVBoxLayout* layout = new QVBoxLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    layout->addWidget(makeSectionLabel(QStringLiteral("Bar Meter Properties"), w));

    // Min / Max
    QGridLayout* grid = new QGridLayout();
    grid->setSpacing(4);

    QDoubleSpinBox* minSpin = new QDoubleSpinBox(w);
    minSpin->setRange(-200.0, 200.0);
    minSpin->setDecimals(1);
    minSpin->setStyleSheet(kSpinStyle);
    minSpin->setValue(item->minVal());

    QDoubleSpinBox* maxSpin = new QDoubleSpinBox(w);
    maxSpin->setRange(-200.0, 200.0);
    maxSpin->setDecimals(1);
    maxSpin->setStyleSheet(kSpinStyle);
    maxSpin->setValue(item->maxVal());

    QDoubleSpinBox* redSpin = new QDoubleSpinBox(w);
    redSpin->setRange(-200.0, 1000.0);
    redSpin->setDecimals(1);
    redSpin->setStyleSheet(kSpinStyle);
    redSpin->setValue(item->redThreshold());

    QDoubleSpinBox* attackSpin = new QDoubleSpinBox(w);
    attackSpin->setRange(0.0, 1.0);
    attackSpin->setSingleStep(0.05);
    attackSpin->setDecimals(2);
    attackSpin->setStyleSheet(kSpinStyle);
    attackSpin->setValue(static_cast<double>(item->attackRatio()));

    QDoubleSpinBox* decaySpin = new QDoubleSpinBox(w);
    decaySpin->setRange(0.0, 1.0);
    decaySpin->setSingleStep(0.05);
    decaySpin->setDecimals(2);
    decaySpin->setStyleSheet(kSpinStyle);
    decaySpin->setValue(static_cast<double>(item->decayRatio()));

    QComboBox* styleCombo = new QComboBox(w);
    styleCombo->addItem(QStringLiteral("Filled"), static_cast<int>(BarItem::BarStyle::Filled));
    styleCombo->addItem(QStringLiteral("Edge"),   static_cast<int>(BarItem::BarStyle::Edge));
    styleCombo->setStyleSheet(
        "QComboBox { background: #0a0a18; color: #c8d8e8;"
        "  border: 1px solid #1e2e3e; border-radius: 3px; padding: 2px; }"
        "QComboBox QAbstractItemView { background: #0a0a18; color: #c8d8e8;"
        "  border: 1px solid #205070; selection-background-color: #00b4d8; }");
    styleCombo->setCurrentIndex(item->barStyle() == BarItem::BarStyle::Edge ? 1 : 0);

    QPushButton* colorBtn = makeColorBtn(item->barColor(), w);

    grid->addWidget(makePropLabel(QStringLiteral("Min:"),    w), 0, 0);
    grid->addWidget(minSpin,                                      0, 1);
    grid->addWidget(makePropLabel(QStringLiteral("Max:"),    w), 0, 2);
    grid->addWidget(maxSpin,                                      0, 3);
    grid->addWidget(makePropLabel(QStringLiteral("Red >:"),  w), 1, 0);
    grid->addWidget(redSpin,                                      1, 1);
    grid->addWidget(makePropLabel(QStringLiteral("Attack:"), w), 2, 0);
    grid->addWidget(attackSpin,                                   2, 1);
    grid->addWidget(makePropLabel(QStringLiteral("Decay:"),  w), 2, 2);
    grid->addWidget(decaySpin,                                    2, 3);
    grid->addWidget(makePropLabel(QStringLiteral("Style:"),  w), 3, 0);
    grid->addWidget(styleCombo,                                   3, 1, 1, 3);
    grid->addWidget(makePropLabel(QStringLiteral("Color:"),  w), 4, 0);
    grid->addWidget(colorBtn,                                     4, 1);

    layout->addLayout(grid);

    // Live update connections
    connect(minSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), w,
        [this, item, minSpin, maxSpin]() {
            item->setRange(minSpin->value(), maxSpin->value());
            updatePreview();
        });
    connect(maxSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), w,
        [this, item, minSpin, maxSpin]() {
            item->setRange(minSpin->value(), maxSpin->value());
            updatePreview();
        });
    connect(redSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), w,
        [this, item](double v) { item->setRedThreshold(v); updatePreview(); });
    connect(attackSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), w,
        [this, item](double v) { item->setAttackRatio(static_cast<float>(v)); updatePreview(); });
    connect(decaySpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), w,
        [this, item](double v) { item->setDecayRatio(static_cast<float>(v)); updatePreview(); });
    connect(styleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), w,
        [this, item, styleCombo](int) {
            item->setBarStyle(static_cast<BarItem::BarStyle>(styleCombo->currentData().toInt()));
            updatePreview();
        });
    connect(colorBtn, &QPushButton::clicked, w, [this, item, colorBtn]() {
        QColor chosen = QColorDialog::getColor(item->barColor(), this, QStringLiteral("Bar Color"));
        if (chosen.isValid()) {
            item->setBarColor(chosen);
            colorBtn->setStyleSheet(
                QStringLiteral("background: %1; border: 1px solid #205070; border-radius: 3px;")
                    .arg(chosen.name()));
            updatePreview();
        }
    });

    return w;
}

// ---------------------------------------------------------------------------
// buildTextItemEditor
// ---------------------------------------------------------------------------

QWidget* ContainerSettingsDialog::buildTextItemEditor(TextItem* item)
{
    QWidget* w = new QWidget(m_typePropsContainer);
    QVBoxLayout* layout = new QVBoxLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    layout->addWidget(makeSectionLabel(QStringLiteral("Text Readout Properties"), w));

    QGridLayout* grid = new QGridLayout();
    grid->setSpacing(4);

    QLineEdit* labelEdit = new QLineEdit(w);
    labelEdit->setStyleSheet(kEditStyle);
    labelEdit->setText(item->label());

    QLineEdit* suffixEdit = new QLineEdit(w);
    suffixEdit->setStyleSheet(kEditStyle);
    suffixEdit->setText(item->suffix());

    QSpinBox* fontSpin = new QSpinBox(w);
    fontSpin->setRange(8, 48);
    fontSpin->setStyleSheet(kSpinStyle);
    fontSpin->setValue(item->fontSize());

    QSpinBox* decimalsSpin = new QSpinBox(w);
    decimalsSpin->setRange(0, 4);
    decimalsSpin->setStyleSheet(kSpinStyle);
    decimalsSpin->setValue(item->decimals());

    QCheckBox* boldCheck = new QCheckBox(QStringLiteral("Bold"), w);
    boldCheck->setStyleSheet("QCheckBox { color: #c8d8e8; }");
    boldCheck->setChecked(item->bold());

    grid->addWidget(makePropLabel(QStringLiteral("Label:"),    w), 0, 0);
    grid->addWidget(labelEdit,                                      0, 1, 1, 3);
    grid->addWidget(makePropLabel(QStringLiteral("Suffix:"),   w), 1, 0);
    grid->addWidget(suffixEdit,                                     1, 1, 1, 3);
    grid->addWidget(makePropLabel(QStringLiteral("Font sz:"),  w), 2, 0);
    grid->addWidget(fontSpin,                                       2, 1);
    grid->addWidget(makePropLabel(QStringLiteral("Decimals:"), w), 2, 2);
    grid->addWidget(decimalsSpin,                                   2, 3);
    grid->addWidget(boldCheck,                                      3, 0, 1, 2);

    layout->addLayout(grid);

    connect(labelEdit,    &QLineEdit::textChanged,                     w,
        [this, item](const QString& t) { item->setLabel(t); updatePreview(); });
    connect(suffixEdit,   &QLineEdit::textChanged,                     w,
        [this, item](const QString& t) { item->setSuffix(t); updatePreview(); });
    connect(fontSpin,     QOverload<int>::of(&QSpinBox::valueChanged), w,
        [this, item](int v) { item->setFontSize(v); updatePreview(); });
    connect(decimalsSpin, QOverload<int>::of(&QSpinBox::valueChanged), w,
        [this, item](int v) { item->setDecimals(v); updatePreview(); });
    connect(boldCheck,    &QCheckBox::toggled,                         w,
        [this, item](bool v) { item->setBold(v); updatePreview(); });

    return w;
}

// ---------------------------------------------------------------------------
// buildScaleItemEditor
// ---------------------------------------------------------------------------

QWidget* ContainerSettingsDialog::buildScaleItemEditor(ScaleItem* item)
{
    QWidget* w = new QWidget(m_typePropsContainer);
    QVBoxLayout* layout = new QVBoxLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    layout->addWidget(makeSectionLabel(QStringLiteral("Scale Properties"), w));

    QGridLayout* grid = new QGridLayout();
    grid->setSpacing(4);

    QDoubleSpinBox* minSpin = new QDoubleSpinBox(w);
    minSpin->setRange(-200.0, 200.0);
    minSpin->setDecimals(1);
    minSpin->setStyleSheet(kSpinStyle);
    minSpin->setValue(item->minVal());

    QDoubleSpinBox* maxSpin = new QDoubleSpinBox(w);
    maxSpin->setRange(-200.0, 200.0);
    maxSpin->setDecimals(1);
    maxSpin->setStyleSheet(kSpinStyle);
    maxSpin->setValue(item->maxVal());

    QSpinBox* majorSpin = new QSpinBox(w);
    majorSpin->setRange(2, 20);
    majorSpin->setStyleSheet(kSpinStyle);
    majorSpin->setValue(item->majorTicks());

    QSpinBox* minorSpin = new QSpinBox(w);
    minorSpin->setRange(0, 10);
    minorSpin->setStyleSheet(kSpinStyle);
    minorSpin->setValue(item->minorTicks());

    grid->addWidget(makePropLabel(QStringLiteral("Min:"),         w), 0, 0);
    grid->addWidget(minSpin,                                           0, 1);
    grid->addWidget(makePropLabel(QStringLiteral("Max:"),         w), 0, 2);
    grid->addWidget(maxSpin,                                           0, 3);
    grid->addWidget(makePropLabel(QStringLiteral("Major ticks:"), w), 1, 0);
    grid->addWidget(majorSpin,                                         1, 1);
    grid->addWidget(makePropLabel(QStringLiteral("Minor ticks:"), w), 1, 2);
    grid->addWidget(minorSpin,                                         1, 3);

    layout->addLayout(grid);

    connect(minSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), w,
        [this, item, minSpin, maxSpin]() {
            item->setRange(minSpin->value(), maxSpin->value());
            updatePreview();
        });
    connect(maxSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), w,
        [this, item, minSpin, maxSpin]() {
            item->setRange(minSpin->value(), maxSpin->value());
            updatePreview();
        });
    connect(majorSpin, QOverload<int>::of(&QSpinBox::valueChanged), w,
        [this, item](int v) { item->setMajorTicks(v); updatePreview(); });
    connect(minorSpin, QOverload<int>::of(&QSpinBox::valueChanged), w,
        [this, item](int v) { item->setMinorTicks(v); updatePreview(); });

    return w;
}

// ---------------------------------------------------------------------------
// buildNeedleItemEditor
// ---------------------------------------------------------------------------

QWidget* ContainerSettingsDialog::buildNeedleItemEditor(NeedleItem* item)
{
    QWidget* w = new QWidget(m_typePropsContainer);
    QVBoxLayout* layout = new QVBoxLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    layout->addWidget(makeSectionLabel(QStringLiteral("Needle Meter Properties"), w));

    QGridLayout* grid = new QGridLayout();
    grid->setSpacing(4);

    QLineEdit* labelEdit = new QLineEdit(w);
    labelEdit->setStyleSheet(kEditStyle);
    labelEdit->setText(item->sourceLabel());

    QPushButton* needleColorBtn = makeColorBtn(item->needleColor(), w);

    grid->addWidget(makePropLabel(QStringLiteral("Label:"),        w), 0, 0);
    grid->addWidget(labelEdit,                                          0, 1, 1, 3);
    grid->addWidget(makePropLabel(QStringLiteral("Needle color:"), w), 1, 0);
    grid->addWidget(needleColorBtn,                                     1, 1);

    layout->addLayout(grid);

    connect(labelEdit, &QLineEdit::textChanged, w,
        [this, item](const QString& t) { item->setSourceLabel(t); updatePreview(); });
    connect(needleColorBtn, &QPushButton::clicked, w, [this, item, needleColorBtn]() {
        QColor chosen = QColorDialog::getColor(item->needleColor(), this,
                                               QStringLiteral("Needle Color"));
        if (chosen.isValid()) {
            item->setNeedleColor(chosen);
            needleColorBtn->setStyleSheet(
                QStringLiteral("background: %1; border: 1px solid #205070; border-radius: 3px;")
                    .arg(chosen.name()));
            updatePreview();
        }
    });

    return w;
}

// ---------------------------------------------------------------------------
// buildSolidItemEditor
// ---------------------------------------------------------------------------

QWidget* ContainerSettingsDialog::buildSolidItemEditor(SolidColourItem* item)
{
    QWidget* w = new QWidget(m_typePropsContainer);
    QVBoxLayout* layout = new QVBoxLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    layout->addWidget(makeSectionLabel(QStringLiteral("Solid Colour Properties"), w));

    QHBoxLayout* row = new QHBoxLayout();
    row->addWidget(makePropLabel(QStringLiteral("Color:"), w));

    QPushButton* colorBtn = makeColorBtn(item->colour(), w);
    row->addWidget(colorBtn);
    row->addStretch();

    layout->addLayout(row);

    connect(colorBtn, &QPushButton::clicked, w, [this, item, colorBtn]() {
        QColor chosen = QColorDialog::getColor(item->colour(), this,
                                               QStringLiteral("Fill Color"));
        if (chosen.isValid()) {
            item->setColour(chosen);
            colorBtn->setStyleSheet(
                QStringLiteral("background: %1; border: 1px solid #205070; border-radius: 3px;")
                    .arg(chosen.name()));
            updatePreview();
        }
    });

    return w;
}

// ---------------------------------------------------------------------------
// buildLedItemEditor
// ---------------------------------------------------------------------------

QWidget* ContainerSettingsDialog::buildLedItemEditor(LEDItem* item)
{
    QWidget* w = new QWidget(m_typePropsContainer);
    QVBoxLayout* layout = new QVBoxLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    layout->addWidget(makeSectionLabel(QStringLiteral("LED Indicator Properties"), w));

    QGridLayout* grid = new QGridLayout();
    grid->setSpacing(4);

    QDoubleSpinBox* threshSpin = new QDoubleSpinBox(w);
    threshSpin->setRange(-200.0, 1000.0);
    threshSpin->setDecimals(1);
    threshSpin->setStyleSheet(kSpinStyle);
    threshSpin->setValue(item->greenThreshold());

    QPushButton* onColorBtn  = makeColorBtn(item->trueColour(),  w);
    QPushButton* offColorBtn = makeColorBtn(item->falseColour(), w);

    grid->addWidget(makePropLabel(QStringLiteral("Threshold:"), w), 0, 0);
    grid->addWidget(threshSpin,                                       0, 1);
    grid->addWidget(makePropLabel(QStringLiteral("On color:"),  w), 1, 0);
    grid->addWidget(onColorBtn,                                       1, 1);
    grid->addWidget(makePropLabel(QStringLiteral("Off color:"), w), 2, 0);
    grid->addWidget(offColorBtn,                                      2, 1);

    layout->addLayout(grid);

    connect(threshSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), w,
        [this, item](double v) { item->setGreenThreshold(v); updatePreview(); });
    connect(onColorBtn, &QPushButton::clicked, w, [this, item, onColorBtn]() {
        QColor chosen = QColorDialog::getColor(item->trueColour(), this,
                                               QStringLiteral("On Color"));
        if (chosen.isValid()) {
            item->setTrueColour(chosen);
            onColorBtn->setStyleSheet(
                QStringLiteral("background: %1; border: 1px solid #205070; border-radius: 3px;")
                    .arg(chosen.name()));
            updatePreview();
        }
    });
    connect(offColorBtn, &QPushButton::clicked, w, [this, item, offColorBtn]() {
        QColor chosen = QColorDialog::getColor(item->falseColour(), this,
                                               QStringLiteral("Off Color"));
        if (chosen.isValid()) {
            item->setFalseColour(chosen);
            offColorBtn->setStyleSheet(
                QStringLiteral("background: %1; border: 1px solid #205070; border-radius: 3px;")
                    .arg(chosen.name()));
            updatePreview();
        }
    });

    return w;
}

} // namespace NereusSDR
