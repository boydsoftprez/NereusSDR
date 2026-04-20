// =================================================================
// src/gui/containers/ContainerSettingsDialog.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/setup.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
// =================================================================

//=================================================================
// setup.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Continual modifications Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//=================================================================
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

#include "ContainerSettingsDialog.h"
#include "ContainerManager.h"
#include "MmioEndpointsDialog.h"

// Phase 3G-6 block 4 — per-item property editors
#include "meter_property_editors/BaseItemEditor.h"
#include "meter_property_editors/BarItemEditor.h"
#include "meter_property_editors/SolidColourItemEditor.h"
#include "meter_property_editors/SpacerItemEditor.h"
#include "meter_property_editors/FadeCoverItemEditor.h"
#include "meter_property_editors/ImageItemEditor.h"
#include "meter_property_editors/ScaleItemEditor.h"
#include "meter_property_editors/NeedleItemEditor.h"
#include "meter_property_editors/NeedleScalePwrItemEditor.h"
#include "meter_property_editors/TextItemEditor.h"
#include "meter_property_editors/TextOverlayItemEditor.h"
#include "meter_property_editors/SignalTextItemEditor.h"
#include "meter_property_editors/LedItemEditor.h"
#include "meter_property_editors/HistoryGraphItemEditor.h"
#include "meter_property_editors/MagicEyeItemEditor.h"
#include "meter_property_editors/DialItemEditor.h"
#include "meter_property_editors/WebImageItemEditor.h"
#include "meter_property_editors/FilterDisplayItemEditor.h"
#include "meter_property_editors/RotatorItemEditor.h"
#include "meter_property_editors/ClockItemEditor.h"
#include "meter_property_editors/VfoDisplayItemEditor.h"
#include "meter_property_editors/ClickBoxItemEditor.h"
#include "meter_property_editors/DataOutItemEditor.h"
#include "meter_property_editors/BandButtonItemEditor.h"
#include "meter_property_editors/ModeButtonItemEditor.h"
#include "meter_property_editors/FilterButtonItemEditor.h"
#include "meter_property_editors/AntennaButtonItemEditor.h"
#include "meter_property_editors/TuneStepButtonItemEditor.h"
#include "meter_property_editors/OtherButtonItemEditor.h"
#include "meter_property_editors/VoiceRecordPlayItemEditor.h"
#include "meter_property_editors/DiscordButtonItemEditor.h"
#include "ContainerWidget.h"
#include "../meters/MeterWidget.h"
#include "../meters/MeterItem.h"

// Edit-container refactor Task 11 — first-class preset MeterItem classes.
// These replace the factory-expansion path that flattened a "preset" into
// N discrete ItemGroup children; each class is now a single MeterItem that
// renders the whole preset in one row.
#include "../meters/presets/AnanMultiMeterItem.h"
#include "../meters/presets/BarPresetItem.h"
#include "../meters/presets/ClockPresetItem.h"
#include "../meters/presets/ContestPresetItem.h"
#include "../meters/presets/CrossNeedleItem.h"
#include "../meters/presets/HistoryGraphPresetItem.h"
#include "../meters/presets/MagicEyePresetItem.h"
#include "../meters/presets/PowerSwrPresetItem.h"
#include "../meters/presets/SignalTextPresetItem.h"
#include "../meters/presets/SMeterPresetItem.h"
#include "../meters/presets/VfoDisplayPresetItem.h"

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

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
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QScrollArea>
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

    // Phase 3G-6 block 4b: grown defaults so the Properties column
    // has usable vertical and horizontal room for the type-specific
    // editors landed in block 4. Users can still resize smaller, but
    // the opening size shows the typical editor comfortably.
    setMinimumSize(960, 600);
    resize(1100, 750);
    setSizeGripEnabled(true);

    buildLayout();

    // Phase 3G-6 block 3 commit 14: snapshot the current container
    // state so Cancel can fully revert the in-progress edits. Must
    // happen AFTER buildLayout so populateItemList has already
    // captured items into m_workingItems from the live MeterWidget.
    takeSnapshot();
}

ContainerSettingsDialog::~ContainerSettingsDialog()
{
    for (const ContainerInUseRow& row : m_workingItems) {
        delete row.item;
    }
}

QVector<MeterItem*> ContainerSettingsDialog::workingItems() const
{
    QVector<MeterItem*> out;
    out.reserve(m_workingItems.size());
    for (const ContainerInUseRow& r : m_workingItems) {
        out.append(r.item);
    }
    return out;
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

    // Phase 3G-6 block 4b: Properties column gets the lion's share
    // because it houses 20+ form rows per editor; Available and
    // In-use are content-sized catalogs that don't grow much.
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 0);
    m_splitter->setStretchFactor(2, 1);
    m_splitter->setSizes({200, 200, 700});

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
    // Task 13 — enable drag-reorder inside the list. InternalMove +
    // MoveAction drives z-order directly; the rowsMoved signal on the
    // underlying model triggers syncWorkingItemsFromList() which
    // reconciles the wrapper vector to the new visual order and
    // pushes the change to the live MeterWidget via applyToContainer.
    m_itemList->setDragDropMode(QAbstractItemView::InternalMove);
    m_itemList->setDragEnabled(true);
    m_itemList->setAcceptDrops(true);
    m_itemList->setDefaultDropAction(Qt::MoveAction);
    m_itemList->setContextMenuPolicy(Qt::CustomContextMenu);
    layout->addWidget(m_itemList, 1);

    connect(m_itemList, &QListWidget::currentRowChanged,
            this, &ContainerSettingsDialog::onItemSelectionChanged);
    connect(m_itemList, &QListWidget::customContextMenuRequested,
            this, &ContainerSettingsDialog::onInUseContextMenu);
    connect(m_itemList->model(), &QAbstractItemModel::rowsMoved,
            this, [this]() {
                syncWorkingItemsFromList();
                applyToContainer();
            });

    // Action buttons row — Task 13 retired Up/Down (drag-reorder
    // replaces them). The legacy + (popup) button is kept for parity
    // with the old flow.
    QHBoxLayout* btnRow = new QHBoxLayout;
    btnRow->setSpacing(3);

    m_btnAdd    = makeBtn(QStringLiteral("+"),      parent);
    m_btnRemove = makeBtn(QStringLiteral("\u2212"), parent);

    m_btnAdd->setToolTip(QStringLiteral("Add item (popup)"));
    m_btnRemove->setToolTip(QStringLiteral("Remove selected item"));

    btnRow->addWidget(m_btnAdd);
    btnRow->addWidget(m_btnRemove);
    btnRow->addStretch();

    layout->addLayout(btnRow);

    connect(m_btnAdd,    &QPushButton::clicked, this, &ContainerSettingsDialog::onAddItem);
    connect(m_btnRemove, &QPushButton::clicked, this, &ContainerSettingsDialog::onRemoveItem);

    populateItemList();
    // Task 12 — apply the hybrid rule to the just-populated available
    // list based on whatever presets are already in use on this
    // container's MeterWidget. buildAvailablePanel() has already run
    // by this point (buildInUsePanel is called after it inside
    // buildLayout), so m_availableList is non-null.
    refreshAvailableList();
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

    // Phase 3G-6 block 7 commit 2: Copy / Paste Settings buttons
    // under the property stack. Mirrors Thetis's per-item clipboard
    // affordance — copies the entire serialized item form, paste
    // only enabled when clipboard's type tag matches the selected
    // item's type.
    QHBoxLayout* cpRow = new QHBoxLayout();
    cpRow->setContentsMargins(0, 0, 0, 0);
    cpRow->setSpacing(4);
    m_btnCopySettings  = makeBtn(QStringLiteral("Copy settings"),  parent);
    m_btnPasteSettings = makeBtn(QStringLiteral("Paste settings"), parent);
    m_btnCopySettings->setEnabled(false);
    m_btnPasteSettings->setEnabled(false);
    cpRow->addWidget(m_btnCopySettings);
    cpRow->addWidget(m_btnPasteSettings);
    cpRow->addStretch();
    layout->addLayout(cpRow);
    connect(m_btnCopySettings,  &QPushButton::clicked,
            this, &ContainerSettingsDialog::onCopyItemSettings);
    connect(m_btnPasteSettings, &QPushButton::clicked,
            this, &ContainerSettingsDialog::onPasteItemSettings);
}

// ---------------------------------------------------------------------------
// Available list catalog and Add-from-available flow
// ---------------------------------------------------------------------------

void ContainerSettingsDialog::populateAvailableList()
{
    if (!m_availableList) { return; }
    m_availableList->clear();

    // Phase 3G-6 block 3 commit 15: Thetis lstMetersAvailable
    // categorization (setup.cs:24522-24566) — three sections, each
    // alphabetical. Section headers are non-selectable disabled
    // QListWidgetItems in a highlight color. Block 4 will revisit
    // the per-entry mapping once the per-item editors are in place
    // and we know exactly which default bindings each type ships
    // with.
    struct Entry { const char* tag; const char* label; };

    // Phase E port of Thetis Setup → Appearance → Meters/Gadgets
    // lstMetersAvailable left column. Each PRESET_* tag dispatches to
    // a factory in ItemGroup that returns a full Thetis-parity bar
    // row composition (SolidColour backdrop + BarItem with Line style
    // + ScaleItem with ShowType red title). When the user double-
    // clicks or presses the "Add →" button on one of these, the
    // dispatch lands in appendPresetRow() which rescales the preset's
    // 0..1 items into the next stack slot so rows naturally pile up
    // in the container — matching how Thetis's "Add Container"
    // right-arrow works in the Appearance dialog.
    static const Entry kMeterRxItems[] = {
        // Composite presets (first-class MeterItem subclasses, Task 11).
        // Each adds as one in-use row rather than N flattened children.
        {"PRESET_SMeter",          "S-Meter"},
        {"PRESET_MagicEyePreset",  "Magic Eye"},
        {"PRESET_HistoryPreset",   "History Graph"},
        // Bar-row presets (BarPresetItem flavours).
        {"PRESET_SignalBar",       "Signal Strength Peak"},
        {"PRESET_AvgSignalBar",    "Signal Strength Avg"},
        {"PRESET_MaxBinBar",       "Signal Max FFT Bin"},
        // PRESET_SignalText routes to SignalTextPresetItem (Task 11).
        // Tag is preserved for save-compat with legacy containers.
        {"PRESET_SignalText",      "Signal Strength Text"},
        {"PRESET_AdcBar",          "ADC"},
        {"PRESET_AdcMaxMag",       "Max ADC Magnitude"},
        {"PRESET_AgcBar",          "AGC"},
        {"PRESET_AgcGainBar",      "AGC Gain"},
        {"PRESET_PbsnrBar",        "Estimated PBSNR"},
    };
    static const Entry kMeterTxItems[] = {
        // Composite presets (first-class MeterItem subclasses, Task 11).
        {"PRESET_AnanMM",       "ANAN Multi Meter"},
        {"PRESET_CrossNeedle",  "Cross-Needle"},
        {"PRESET_PowerSwr",     "Power / SWR"},
        // Bar-row presets (BarPresetItem flavours).
        {"PRESET_Alc",          "ALC"},
        {"PRESET_AlcGain",      "ALC Compression"},
        {"PRESET_AlcGroup",     "ALC Group"},
        {"PRESET_CfcBar",       "CFC Compression Average"},
        {"PRESET_CfcGainBar",   "CFC Compression"},
        {"PRESET_Comp",         "Compression"},
        {"PRESET_Eq",           "EQ"},
        {"PRESET_Leveler",      "Leveler"},
        {"PRESET_LevelerGain",  "Leveler Gain"},
        {"PRESET_Mic",          "Mic"},
    };
    // --- Raw building-block items (low-level MeterItem subclasses) ---
    static const Entry kRxItems[] = {
        {"NEEDLE",         "Needle Meter"},
        {"SIGNALTEXT",     "Signal Text"},
        {"TEXT",           "Text Readout"},
    };
    static const Entry kTxItems[] = {
        {"BAR",            "Bar Meter"},
        {"LED",            "LED"},
        {"NEEDLESCALEPWR", "Needle Scale Power"},
        {"SCALE",          "Scale"},
    };
    static const Entry kSpecialItems[] = {
        // Composite presets added in Task 11 (first-class MeterItem
        // subclasses). Raw primitive entries (VFODISPLAY, CLOCK,
        // HISTORY, MAGICEYE below) remain freely addable per the
        // hybrid rule in Task 12.
        {"PRESET_VfoDisplayPreset", "VFO Display (preset)"},
        {"PRESET_ClockPreset",      "Clock (preset)"},
        {"PRESET_ContestPreset",    "Contest Layout"},
        // Raw building-block primitives.
        {"CLICKBOX",       "Click Box"},
        {"CLOCK",          "Clock"},
        {"DATAOUT",        "Data Out"},
        {"DIAL",           "Dial Meter"},
        {"FADECOVER",      "Fade Cover"},
        {"FILTERDISPLAY",  "Filter Display"},
        {"HISTORY",        "History Graph"},
        {"IMAGE",          "Image"},
        {"MAGICEYE",       "Magic Eye"},
        {"ROTATOR",        "Rotator"},
        {"SOLID",          "Solid Colour"},
        {"SPACER",         "Spacer"},
        {"TEXTOVERLAY",    "Text Overlay"},
        {"VFODISPLAY",     "VFO Display"},
        {"WEBIMAGE",       "Web Image"},
    };

    auto addHeader = [this](const QString& text) {
        auto* h = new QListWidgetItem(text, m_availableList);
        QFont f = h->font();
        f.setBold(true);
        h->setFont(f);
        h->setForeground(QColor(0x8a, 0xa8, 0xc0));
        h->setBackground(QColor(0x1a, 0x2a, 0x38));
        h->setFlags(Qt::NoItemFlags);   // non-selectable, non-enabled
    };
    auto addEntry = [this](const Entry& e) {
        auto* item = new QListWidgetItem(
            QStringLiteral("  ") + QString::fromLatin1(e.label),
            m_availableList);
        item->setData(Qt::UserRole, QString::fromLatin1(e.tag));
    };

    // High-level Thetis-style meter compositions first — these are
    // what most users reach for when building a multimeter container.
    addHeader(QStringLiteral("RX Meters (Thetis)"));
    for (const auto& e : kMeterRxItems) { addEntry(e); }

    addHeader(QStringLiteral("TX Meters (Thetis)"));
    for (const auto& e : kMeterTxItems) { addEntry(e); }

    // Raw building blocks below — for custom meter construction.
    addHeader(QStringLiteral("RX items"));
    for (const auto& e : kRxItems) { addEntry(e); }

    addHeader(QStringLiteral("TX items"));
    for (const auto& e : kTxItems) { addEntry(e); }

    addHeader(QStringLiteral("Special"));
    for (const auto& e : kSpecialItems) { addEntry(e); }
}

void ContainerSettingsDialog::onAddFromAvailable()
{
    if (!m_availableList) { return; }
    QListWidgetItem* sel = m_availableList->currentItem();
    if (!sel) { return; }
    // Edit-container refactor Task 12 — hybrid addition rule. Rows
    // for presets that are already in use on this container are
    // rendered disabled by refreshAvailableList(); block the click
    // defensively too so programmatic paths (keyboard Enter, etc.)
    // honour the one-instance-per-container invariant.
    if (!(sel->flags() & Qt::ItemIsEnabled)) { return; }
    const QString tag = sel->data(Qt::UserRole).toString();
    if (tag.isEmpty()) { return; }
    // Phase E — PRESET_* tags map to full Thetis-parity meter row
    // compositions rather than raw building-block items. Route to
    // appendPresetRow() which rescales the factory's 0..1 preset
    // into the next stack slot so rows pile up vertically.
    if (tag.startsWith(QLatin1String("PRESET_"))) {
        const QString presetName = tag.mid(7);   // strip "PRESET_"
        appendPresetRow(presetName);
        return;
    }
    addNewItem(tag);
}

// ---------------------------------------------------------------------------
// Edit-container refactor Task 12 — hybrid addition rule
// ---------------------------------------------------------------------------

QString ContainerSettingsDialog::presetTagForItem(MeterItem* it) const
{
    if (!it) { return QString(); }
    if (dynamic_cast<AnanMultiMeterItem*>(it))     { return QStringLiteral("PRESET_AnanMM"); }
    if (dynamic_cast<CrossNeedleItem*>(it))        { return QStringLiteral("PRESET_CrossNeedle"); }
    if (dynamic_cast<SMeterPresetItem*>(it))       { return QStringLiteral("PRESET_SMeter"); }
    if (dynamic_cast<PowerSwrPresetItem*>(it))     { return QStringLiteral("PRESET_PowerSwr"); }
    if (dynamic_cast<MagicEyePresetItem*>(it))     { return QStringLiteral("PRESET_MagicEyePreset"); }
    if (dynamic_cast<SignalTextPresetItem*>(it))   { return QStringLiteral("PRESET_SignalText"); }
    if (dynamic_cast<HistoryGraphPresetItem*>(it)) { return QStringLiteral("PRESET_HistoryPreset"); }
    if (dynamic_cast<VfoDisplayPresetItem*>(it))   { return QStringLiteral("PRESET_VfoDisplayPreset"); }
    if (dynamic_cast<ClockPresetItem*>(it))        { return QStringLiteral("PRESET_ClockPreset"); }
    if (dynamic_cast<ContestPresetItem*>(it))      { return QStringLiteral("PRESET_ContestPreset"); }
    // BarPresetItem tags follow the existing availability-list naming
    // (PRESET_Alc, PRESET_SignalBar, etc.) — kindString() returns e.g.
    // "SignalBar", "Alc", "AlcGain" which matches the stored tag
    // minus the "PRESET_" prefix for most flavours. Five flavours
    // (Agc / AgcGain / Pbsnr / Cfc / CfcGain) use a "*Bar" suffix in
    // the available-list table at buildAvailablePanel() but
    // kindString() returns the bare form — map those back to the
    // table-form here so refreshAvailableList() can match and disable
    // the row per the hybrid rule.
    if (auto* bar = dynamic_cast<BarPresetItem*>(it)) {
        const QString kind = bar->kindString();
        static const QMap<QString, QString> kBarTagOverrides = {
            {QStringLiteral("Agc"),     QStringLiteral("AgcBar")},
            {QStringLiteral("AgcGain"), QStringLiteral("AgcGainBar")},
            {QStringLiteral("Pbsnr"),   QStringLiteral("PbsnrBar")},
            {QStringLiteral("Cfc"),     QStringLiteral("CfcBar")},
            {QStringLiteral("CfcGain"), QStringLiteral("CfcGainBar")},
        };
        const QString tagSuffix = kBarTagOverrides.value(kind, kind);
        return QStringLiteral("PRESET_") + tagSuffix;
    }
    return QString();   // primitive
}

void ContainerSettingsDialog::refreshAvailableList()
{
    if (!m_availableList) { return; }

    // Build the set of currently-in-use preset tags.
    QSet<QString> inUseTags;
    for (const ContainerInUseRow& r : m_workingItems) {
        const QString tag = presetTagForItem(r.item);
        if (!tag.isEmpty()) { inUseTags.insert(tag); }
    }

    constexpr QRgb kNormalFg   = 0xffc8d8e8;   // enabled text (from kListStyle)
    constexpr QRgb kDisabledFg = 0xff5a6a7a;   // muted "in use" text

    for (int i = 0; i < m_availableList->count(); ++i) {
        QListWidgetItem* row = m_availableList->item(i);
        if (!row) { continue; }
        // Skip the non-selectable section headers — they have
        // Qt::NoItemFlags and don't carry a UserRole tag.
        const QString tag = row->data(Qt::UserRole).toString();
        if (tag.isEmpty()) { continue; }

        if (!tag.startsWith(QLatin1String("PRESET_"))) {
            // Primitives are always enabled (hybrid rule — rule C).
            row->setFlags(row->flags() | Qt::ItemIsEnabled);
            row->setForeground(QColor::fromRgba(kNormalFg));
            continue;
        }

        const bool used = inUseTags.contains(tag);
        if (used) {
            row->setFlags(row->flags() & ~Qt::ItemIsEnabled);
            row->setForeground(QColor::fromRgba(kDisabledFg));
        } else {
            row->setFlags(row->flags() | Qt::ItemIsEnabled);
            row->setForeground(QColor::fromRgba(kNormalFg));
        }
    }
}

void ContainerSettingsDialog::appendPresetRow(const QString& presetName)
{
    // Edit-container refactor Task 11 — first-class preset dispatch.
    //
    // Each PRESET_* tag maps directly to a single MeterItem subclass
    // that owns its internal parts privately. The old ItemGroup
    // expansion path (which flattened e.g. ANAN MM into 8 discrete
    // rows) is retired for this dialog; composite presets now appear
    // as exactly one row in the in-use list. Bar-row presets collapse
    // to BarPresetItem + configureAs<kind>(). ItemGroup was removed in
    // Task 19; its provenance lives on in the per-class attribution
    // blocks that cite each factory's original line range.
    //
    // Layout parity:
    //   - Composite presets carry their Thetis-nominal full-container
    //     rect (set inside each class's constructor, e.g. ANAN MM
    //     defaults to (0, 0, 1, 0.441) from MeterManager.cs:22472).
    //     We leave that rect alone — they don't participate in the
    //     stack layout.
    //   - Bar-row presets (BarPresetItem flavours) tag themselves
    //     with a stack slot index + within-slot 0..1 local rect
    //     snapshot so MeterWidget::reflowStackedItems() lays them
    //     in vertical slots of height max(0.05 * widgetH, 24 px).
    MeterItem* created = nullptr;
    bool isBarRow = false;

    if      (presetName == QLatin1String("AnanMM"))           { created = new AnanMultiMeterItem(this); }
    else if (presetName == QLatin1String("CrossNeedle"))      { created = new CrossNeedleItem(this); }
    else if (presetName == QLatin1String("SMeter"))           { created = new SMeterPresetItem(this); }
    else if (presetName == QLatin1String("PowerSwr"))         { created = new PowerSwrPresetItem(this); }
    else if (presetName == QLatin1String("MagicEyePreset"))   { created = new MagicEyePresetItem(this); }
    // PRESET_SignalText keeps its existing tag form for save-compat
    // with existing containers but now routes to the first-class
    // SignalTextPresetItem (previously it flattened to a 2-item
    // ItemGroup built via createSignalTextPreset). PRESET_Signal-
    // TextPreset is deliberately NOT registered as a duplicate entry
    // in kMeterRxItems — the single PRESET_SignalText tag is all
    // users see in the available list.
    else if (presetName == QLatin1String("SignalText") ||
             presetName == QLatin1String("SignalTextPreset")) { created = new SignalTextPresetItem(this); }
    else if (presetName == QLatin1String("HistoryPreset"))    { created = new HistoryGraphPresetItem(this); }
    else if (presetName == QLatin1String("VfoDisplayPreset")) { created = new VfoDisplayPresetItem(this); }
    else if (presetName == QLatin1String("ClockPreset"))      { created = new ClockPresetItem(this); }
    else if (presetName == QLatin1String("ContestPreset"))    { created = new ContestPresetItem(this); }
    else {
        // Bar-row family. Accepts both the short loadPresetByName
        // names (Adc, Agc, Pbsnr, Cfc, CfcGain) AND the longer "*Bar"
        // names used by the left "Meter Types" list entries.
        auto* bar = new BarPresetItem(this);
        bool matched = true;
        if      (presetName == QLatin1String("SignalBar"))    { bar->configureAsSignalBar(); }
        else if (presetName == QLatin1String("AvgSignalBar")) { bar->configureAsAvgSignalBar(); }
        else if (presetName == QLatin1String("MaxBinBar"))    { bar->configureAsMaxBinBar(); }
        else if (presetName == QLatin1String("Adc") ||
                 presetName == QLatin1String("AdcBar"))       { bar->configureAsAdcBar(); }
        else if (presetName == QLatin1String("AdcMaxMag"))    { bar->configureAsAdcMaxMag(); }
        else if (presetName == QLatin1String("Agc") ||
                 presetName == QLatin1String("AgcBar"))       { bar->configureAsAgc(); }
        else if (presetName == QLatin1String("AgcGain") ||
                 presetName == QLatin1String("AgcGainBar"))   { bar->configureAsAgcGain(); }
        else if (presetName == QLatin1String("Pbsnr") ||
                 presetName == QLatin1String("PbsnrBar"))     { bar->configureAsPbsnr(); }
        else if (presetName == QLatin1String("Alc"))          { bar->configureAsAlc(); }
        else if (presetName == QLatin1String("AlcGain"))      { bar->configureAsAlcGain(); }
        else if (presetName == QLatin1String("AlcGroup"))     { bar->configureAsAlcGroup(); }
        else if (presetName == QLatin1String("Cfc") ||
                 presetName == QLatin1String("CfcBar"))       { bar->configureAsCfc(); }
        else if (presetName == QLatin1String("CfcGain") ||
                 presetName == QLatin1String("CfcGainBar"))   { bar->configureAsCfcGain(); }
        else if (presetName == QLatin1String("Comp"))         { bar->configureAsComp(); }
        else if (presetName == QLatin1String("Eq"))           { bar->configureAsEq(); }
        else if (presetName == QLatin1String("Leveler"))      { bar->configureAsLeveler(); }
        else if (presetName == QLatin1String("LevelerGain"))  { bar->configureAsLevelerGain(); }
        else if (presetName == QLatin1String("Mic"))          { bar->configureAsMic(); }
        else {
            matched = false;
            delete bar;
        }
        if (matched) {
            created = bar;
            isBarRow = true;
        }
    }

    if (!created) { return; }

    if (isBarRow) {
        // Bar-row presets participate in the Thetis-parity stack
        // layout. Next slot index = max existing stack slot + 1.
        // Within-slot 0..1 local layout is the full slot.
        int nextSlot = 0;
        for (const ContainerInUseRow& r : m_workingItems) {
            if (!r.item) { continue; }
            if (r.item->stackSlot() >= nextSlot) {
                nextSlot = r.item->stackSlot() + 1;
            }
        }
        created->setSlotLocalY(0.0f);
        created->setSlotLocalH(1.0f);
        created->setStackSlot(nextSlot);
        created->setRect(0.0f, 0.0f, 1.0f, 0.05f);
    }
    // Composite presets keep whatever rect their constructor chose.

    ContainerInUseRow newRow;
    newRow.item        = created;
    newRow.displayName = defaultDisplayNameFor(created);
    newRow.rowId       = QUuid::createUuid();
    m_workingItems.append(newRow);

    // Reflow immediately so the preview reflects the new stack
    // layout at the current target-widget size. The widget's own
    // resizeEvent keeps this in sync going forward.
    if (MeterWidget* meter = findMeterWidget()) {
        meter->reflowStackedItems();
    }

    refreshItemList();
    refreshAvailableList();
    if (!m_workingItems.isEmpty()) {
        m_itemList->setCurrentRow(m_workingItems.size() - 1);
    }
    applyToContainer();
    updatePreview();
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

    // Phase 3G-6 block 3 commit 17: two-row layout. Row 1 holds the
    // container-switch dropdown, title, bg, rx, and visibility
    // controls. Row 2 holds the remaining container-level toggles
    // plus the Duplicate / Delete actions on the right.
    QVBoxLayout* outer = new QVBoxLayout(bar);
    outer->setContentsMargins(8, 4, 8, 4);
    outer->setSpacing(4);

    QHBoxLayout* barLayout = new QHBoxLayout;
    barLayout->setContentsMargins(0, 0, 0, 0);
    barLayout->setSpacing(10);
    outer->addLayout(barLayout);

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
    // Phase 3G-6 block 7 commit 2: dropdown is fully active when a
    // manager is available and there's more than one container.
    m_containerDropdown->setEnabled(m_manager != nullptr
                                    && m_containerDropdown->count() > 1);
    if (m_manager) {
        connect(m_containerDropdown,
                qOverload<int>(&QComboBox::currentIndexChanged),
                this,
                &ContainerSettingsDialog::onContainerDropdownChanged);
    }

    barLayout->addWidget(contLabel);
    barLayout->addWidget(m_containerDropdown);

    // Edit-container refactor Task 14 — + Add button next to the
    // container dropdown. Creates a new floating container and
    // switches to it without closing the dialog. Only enabled when a
    // manager is available (single-container legacy mode has no way
    // to spawn siblings).
    m_btnAddContainer = new QPushButton(QStringLiteral("+ Add"), bar);
    m_btnAddContainer->setToolTip(
        tr("Create a new container and switch to editing it"));
    m_btnAddContainer->setAutoDefault(false);
    m_btnAddContainer->setDefault(false);
    m_btnAddContainer->setStyleSheet(
        "QPushButton {"
        "  background: #0d3f4a;"
        "  color: #7fffd4;"
        "  border: 1px solid #00b4d8;"
        "  border-radius: 3px;"
        "  padding: 3px 8px;"
        "  font-weight: 600;"
        "}"
        "QPushButton:hover { background: #104a57; }"
        "QPushButton:disabled {"
        "  background: #1a2a3a;"
        "  color: #5a6a7a;"
        "  border-color: #203040;"
        "}");
    m_btnAddContainer->setEnabled(m_manager != nullptr);
    connect(m_btnAddContainer, &QPushButton::clicked,
            this, &ContainerSettingsDialog::onAddContainer);
    barLayout->addWidget(m_btnAddContainer);

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

    // Populate row 1 from container
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

    // -------- Row 2: additional container-level controls --------
    QHBoxLayout* row2 = new QHBoxLayout;
    row2->setContentsMargins(0, 0, 0, 0);
    row2->setSpacing(10);
    outer->addLayout(row2);

    auto makeCheck = [bar](const QString& label, bool initial) {
        auto* cb = new QCheckBox(label, bar);
        cb->setStyleSheet("QCheckBox { color: #c8d8e8; }");
        cb->setChecked(initial);
        return cb;
    };

    m_lockCheck              = makeCheck(QStringLiteral("Lock"),
                                         m_container && m_container->isLocked());
    m_hideTitleCheck         = makeCheck(QStringLiteral("Hide title"),
                                         m_container && !m_container->isTitleBarVisible());
    m_minimisesCheck         = makeCheck(QStringLiteral("Minimises"),
                                         m_container && m_container->containerMinimises());
    m_autoHeightCheck        = makeCheck(QStringLiteral("Auto height"),
                                         m_container && m_container->autoHeight());
    m_hidesWhenRxNotUsedCheck = makeCheck(QStringLiteral("Hide when RX unused"),
                                         m_container && m_container->containerHidesWhenRxNotUsed());
    m_highlightCheck         = makeCheck(QStringLiteral("Highlight"),
                                         m_container && m_container->isHighlighted());

    row2->addWidget(m_lockCheck);
    row2->addWidget(m_hideTitleCheck);
    row2->addWidget(m_minimisesCheck);
    row2->addWidget(m_autoHeightCheck);
    row2->addWidget(m_hidesWhenRxNotUsedCheck);
    row2->addWidget(m_highlightCheck);
    row2->addStretch();

    m_btnDuplicate = makeBtn(QStringLiteral("Duplicate"), bar);
    m_btnDelete    = makeBtn(QStringLiteral("Delete"),    bar);
    m_btnDelete->setStyleSheet(
        "QPushButton { background: #401010; color: #ffb0b0; border: 1px solid #802020;"
        "  border-radius: 3px; padding: 2px 8px; }"
        "QPushButton:hover { background: #602020; }");
    row2->addWidget(m_btnDuplicate);
    row2->addWidget(m_btnDelete);

    // Highlight is the only one that takes effect immediately — it's
    // a pure runtime UI affordance showing the user which container
    // is being edited. The others latch into the container on Apply
    // / OK via applyToContainer (live-edit push machinery is block
    // 4 territory).
    if (m_container) {
        m_container->setHighlighted(m_highlightCheck->isChecked());
    }
    connect(m_highlightCheck, &QCheckBox::toggled, this, [this](bool on) {
        if (m_container) { m_container->setHighlighted(on); }
    });

    connect(m_btnDuplicate, &QPushButton::clicked, this, [this]() {
        if (m_manager && m_container) {
            ContainerWidget* dup = m_manager->duplicateContainer(m_container->id());
            if (dup && m_containerDropdown) {
                const QString label = dup->notes().isEmpty() ? dup->id().left(8) : dup->notes();
                m_containerDropdown->addItem(label, dup->id());
            }
        }
    });
    connect(m_btnDelete, &QPushButton::clicked, this, [this]() {
        if (!m_manager || !m_container) { return; }
        const QString id = m_container->id();
        // Leaving the dialog open pointing at a freed container is a
        // crash waiting to happen, so destroy + close together.
        m_snapshotTaken = false;   // prevent revert-into-freed-container on reject
        m_container = nullptr;
        m_manager->destroyContainer(id);
        reject();
    });

    parentLayout->addWidget(bar);
}

void ContainerSettingsDialog::buildButtonBar()
{
    QHBoxLayout* barLayout = new QHBoxLayout;
    barLayout->setSpacing(6);

    // Left cluster: Save / Load / Presets / Import / Export / MMIO
    m_btnSave   = makeBtn(QStringLiteral("Save\u2026"),    this);
    m_btnLoad   = makeBtn(QStringLiteral("Load\u2026"),    this);
    m_btnPreset = makeBtn(QStringLiteral("Presets\u2026"), this);
    m_btnImport = makeBtn(QStringLiteral("Import"),        this);
    m_btnExport = makeBtn(QStringLiteral("Export"),        this);
    m_btnMmio   = makeBtn(QStringLiteral("MMIO Variables\u2026"), this);

    barLayout->addWidget(m_btnSave);
    barLayout->addWidget(m_btnLoad);
    barLayout->addWidget(m_btnPreset);
    barLayout->addWidget(m_btnImport);
    barLayout->addWidget(m_btnExport);
    barLayout->addWidget(m_btnMmio);
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
    connect(m_btnSave,   &QPushButton::clicked, this, &ContainerSettingsDialog::onSaveToFile);
    connect(m_btnLoad,   &QPushButton::clicked, this, &ContainerSettingsDialog::onLoadFromFile);
    connect(m_btnMmio,   &QPushButton::clicked, this, &ContainerSettingsDialog::onOpenMmioDialog);
    connect(m_btnApply,  &QPushButton::clicked, this, [this]() {
        applyToContainer();
        // + Add containers become committed once Apply is clicked —
        // Cancel after Apply must not destroy them.
        m_containersAddedThisSession.clear();
    });
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_btnOk,     &QPushButton::clicked, this, [this]() {
        applyToContainer();
        // + Add containers become committed on OK too.
        m_containersAddedThisSession.clear();
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
        updateCopyPasteButtonState();
        return;
    }
    updateCopyPasteButtonState();

    // Phase 3G-6 block 4: replace the legacy "common props + type
    // editor" pair with a single BaseItemEditor subclass that
    // handles everything for the selected item's type. The editor
    // is built on demand and installed as a fresh page in the
    // property stack.
    if (m_currentTypeEditor) {
        m_propertyStack->removeWidget(m_currentTypeEditor);
        delete m_currentTypeEditor;
        m_currentTypeEditor = nullptr;
    }

    QWidget* editor = buildTypeSpecificEditor(m_workingItems[row].item);
    if (!editor) {
        m_propertyStack->setCurrentWidget(m_emptyPage);
        return;
    }

    // Phase 3G-6 block 4b: wrap every editor page in a QScrollArea
    // so tall editors (NeedleItemEditor with 26+ rows + calibration
    // table) don't get clipped by the bottom edge of the dialog.
    // setWidgetResizable(true) makes the inner widget track the
    // viewport width so the form rows don't squeeze horizontally.
    auto* scroll = new QScrollArea(m_propertyStack);
    scroll->setWidget(editor);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet(
        "QScrollArea { background: #0a0a18; border: 1px solid #203040; }"
        "QScrollArea > QWidget > QWidget { background: #0a0a18; }"
        "QScrollBar:vertical {"
        "  background: #0f0f1a; width: 10px; margin: 0;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: #205070; border-radius: 4px; min-height: 24px;"
        "}"
        "QScrollBar::handle:vertical:hover { background: #00b4d8; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "  height: 0;"
        "}");

    // Track the scroll area as the "current type editor" so the
    // next selection change removes and deletes the whole wrapper
    // (the inner editor is re-parented to the scroll area, so
    // deleting the scroll area cleans up both).
    m_currentTypeEditor = scroll;
    m_propertyStack->addWidget(scroll);
    m_propertyStack->setCurrentWidget(scroll);

    // Live-edit push: every property change on the working item
    // flushes m_workingItems back to the real container. Block 4
    // can refine this to only redraw the affected item, but for
    // now a full applyToContainer on every edit is correct and
    // cheap enough for interactive use.
    if (auto* base = qobject_cast<BaseItemEditor*>(editor)) {
        connect(base, &BaseItemEditor::propertyChanged,
                this, &ContainerSettingsDialog::applyToContainer);
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

    delete m_workingItems[row].item;
    m_workingItems.remove(row);
    refreshItemList();
    refreshAvailableList();
    // Code-review follow-up: push the change to the live MeterWidget
    // so the "-" button matches right-click Delete (onItemDelete)
    // which already calls applyToContainer().
    applyToContainer();

    // Select nearest remaining item
    if (!m_workingItems.isEmpty()) {
        const int newRow = qMin(row, m_workingItems.size() - 1);
        m_itemList->setCurrentRow(newRow);
    }

    updatePreview();
}

// Task 13 — onMoveItemUp / onMoveItemDown retired. The in-use list
// now supports InternalMove drag-reorder, which drives z-order via
// the QAbstractItemModel::rowsMoved signal -> syncWorkingItemsFrom-
// List() -> applyToContainer() chain. See buildInUsePanel().

// ---------------------------------------------------------------------------
// Task 13 — in-use row helpers + context menu + rename / duplicate / delete
// ---------------------------------------------------------------------------

MeterItem* ContainerSettingsDialog::findItemByRowId(const QUuid& id) const
{
    if (id.isNull()) { return nullptr; }
    for (const ContainerInUseRow& row : m_workingItems) {
        if (row.rowId == id) { return row.item; }
    }
    return nullptr;
}

QString ContainerSettingsDialog::defaultDisplayNameFor(MeterItem* item) const
{
    if (!item) { return QString(); }
    // Preset items (JSON-serialized) expose their kind via the "kind"
    // discriminator; primitives (pipe-serialized) expose their tag as
    // the first field. Normalize both into the tag form used by
    // typeTagDisplayName().
    const QString serialized = item->serialize();
    QString typeTag;
    if (serialized.startsWith(QLatin1Char('{'))) {
        const QJsonDocument doc = QJsonDocument::fromJson(serialized.toUtf8());
        if (doc.isObject()) {
            typeTag = doc.object().value(QStringLiteral("kind")).toString();
        }
    }
    if (typeTag.isEmpty()) {
        const int pipeIdx = serialized.indexOf(QLatin1Char('|'));
        typeTag = (pipeIdx >= 0) ? serialized.left(pipeIdx) : serialized;
    }
    return typeTagDisplayName(typeTag);
}

void ContainerSettingsDialog::syncWorkingItemsFromList()
{
    if (!m_itemList) { return; }

    QVector<ContainerInUseRow> reordered;
    reordered.reserve(m_itemList->count());

    for (int i = 0; i < m_itemList->count(); ++i) {
        const QListWidgetItem* lwItem = m_itemList->item(i);
        if (!lwItem) { continue; }
        const QUuid id = QUuid::fromString(
            lwItem->data(Qt::UserRole).toString());
        for (const ContainerInUseRow& row : m_workingItems) {
            if (row.rowId == id) {
                reordered.append(row);
                break;
            }
        }
    }

    if (reordered.size() == m_workingItems.size()) {
        m_workingItems = reordered;
    }
    // If the reconciliation came up short (shouldn't happen in normal
    // drag-reorder flows), leave the wrapper vector untouched so no
    // items are dropped.
}

void ContainerSettingsDialog::onInUseContextMenu(const QPoint& pos)
{
    if (!m_itemList) { return; }
    QListWidgetItem* lwItem = m_itemList->itemAt(pos);
    if (!lwItem) { return; }

    const QUuid rowId = QUuid::fromString(
        lwItem->data(Qt::UserRole).toString());
    if (rowId.isNull()) { return; }

    QMenu menu(this);
    menu.setStyleSheet(
        QStringLiteral("QMenu { background: #1a2a3a; color: #c8d8e8; "
                       "  border: 1px solid #205070; }"
                       "QMenu::item:selected { background: #00b4d8; "
                       "  color: #0f0f1a; }"));

    QAction* actRename = menu.addAction(tr("Rename..."));
    QAction* actDup    = menu.addAction(tr("Duplicate"));
    QAction* actDel    = menu.addAction(tr("Delete"));

    // Block Duplicate on presets — the hybrid rule would refuse the
    // second instance anyway, so fail visibly rather than silently.
    MeterItem* target = findItemByRowId(rowId);
    if (target && !presetTagForItem(target).isEmpty()) {
        actDup->setEnabled(false);
        actDup->setText(tr("Duplicate (presets are single-instance)"));
    }

    QAction* chosen = menu.exec(
        m_itemList->viewport()->mapToGlobal(pos));
    if      (chosen == actRename) { onItemRename(rowId); }
    else if (chosen == actDup)    { onItemDuplicate(rowId); }
    else if (chosen == actDel)    { onItemDelete(rowId); }
}

void ContainerSettingsDialog::onItemRename(const QUuid& rowId)
{
    const QString oldName = displayNameForRowId(rowId);
    bool ok = false;
    const QString newName = QInputDialog::getText(
        this,
        tr("Rename item"),
        tr("Display name:"),
        QLineEdit::Normal,
        oldName,
        &ok);
    if (!ok || newName == oldName) { return; }
    for (ContainerInUseRow& row : m_workingItems) {
        if (row.rowId == rowId) {
            row.displayName = newName;
            break;
        }
    }
    refreshItemList();
    applyToContainer();
}

void ContainerSettingsDialog::onItemDuplicate(const QUuid& rowId)
{
    MeterItem* orig = findItemByRowId(rowId);
    if (!orig) { return; }
    // Hybrid rule: presets are single-instance per container. The
    // context-menu path disables Duplicate on presets; this is the
    // belt-and-braces guard for programmatic callers (tests).
    if (!presetTagForItem(orig).isEmpty()) { return; }

    MeterItem* copy = createItemFromSerialized(orig->serialize());
    if (!copy) { return; }
    if (orig->hasMmioBinding()) {
        copy->setMmioBinding(orig->mmioGuid(), orig->mmioVariable());
    }
    if (orig->stackSlot() >= 0) {
        // Duplicate lands in a fresh slot below every existing
        // stacked item. Without this bump the copy lands in the same
        // vertical slot as the original and MeterWidget::reflow-
        // StackedItems() overlaps them on the next redraw. Mirror
        // the slot-math used in appendPresetRow above.
        int nextSlot = 0;
        for (const ContainerInUseRow& r : m_workingItems) {
            if (!r.item) { continue; }
            if (r.item->stackSlot() >= nextSlot) {
                nextSlot = r.item->stackSlot() + 1;
            }
        }
        copy->setStackSlot(nextSlot);
        copy->setSlotLocalY(orig->slotLocalY());
        copy->setSlotLocalH(orig->slotLocalH());
    }

    ContainerInUseRow newRow;
    newRow.item        = copy;
    newRow.displayName = displayNameForRowId(rowId)
                       + QStringLiteral(" (copy)");
    newRow.rowId       = QUuid::createUuid();
    m_workingItems.append(newRow);

    refreshItemList();
    refreshAvailableList();
    applyToContainer();
}

void ContainerSettingsDialog::onItemDelete(const QUuid& rowId)
{
    for (int i = 0; i < m_workingItems.size(); ++i) {
        if (m_workingItems[i].rowId == rowId) {
            delete m_workingItems[i].item;
            m_workingItems.remove(i);
            break;
        }
    }
    refreshItemList();
    refreshAvailableList();
    applyToContainer();
}

void ContainerSettingsDialog::triggerRenameForTest(const QUuid& rowId,
                                                   const QString& newName)
{
    for (ContainerInUseRow& row : m_workingItems) {
        if (row.rowId == rowId) {
            row.displayName = newName;
            break;
        }
    }
    refreshItemList();
    applyToContainer();
}

void ContainerSettingsDialog::triggerDuplicateForTest(const QUuid& rowId)
{
    onItemDuplicate(rowId);
}

void ContainerSettingsDialog::triggerDeleteForTest(const QUuid& rowId)
{
    onItemDelete(rowId);
}

QUuid ContainerSettingsDialog::rowIdAtIndex(int i) const
{
    if (i < 0 || i >= m_workingItems.size()) { return QUuid(); }
    return m_workingItems[i].rowId;
}

QString ContainerSettingsDialog::displayNameForRowId(const QUuid& id) const
{
    for (const ContainerInUseRow& row : m_workingItems) {
        if (row.rowId == id) { return row.displayName; }
    }
    return QString();
}

// ---------------------------------------------------------------------------
// addNewItem — create and insert a new item after the current selection
// ---------------------------------------------------------------------------

float ContainerSettingsDialog::nextStackYPos(const QVector<MeterItem*>& items)
{
    // Compute the next vertical-stack y-position. Skip items that
    // span more than 70% of the container vertically — those are
    // background / overlay-style items (ImageItem backgrounds, ANANMM
    // full-container needles) and don't participate in the stack.
    // Without this filter the next-position math would clamp every
    // newly-added item at y=0.9 the moment a full-container item
    // existed, piling new items on top of each other.
    //
    // Return the true bottom-of-stack (unclamped). The caller is
    // responsible for checking whether there is enough room for a
    // new row via `slotH = 0.95 - yPos`; an earlier version of this
    // function clamped the return to 0.9 which caused the NEXT
    // append to overlap the last row once the stack reached the
    // bottom — `slotH` became `0.05` again instead of going negative,
    // and the bail-out at the caller never triggered. Callers now
    // see a monotonically increasing yPos and refuse the append
    // cleanly when slotH drops below the threshold.
    constexpr float kBackgroundSpanThreshold = 0.7f;
    float yPos = 0.0f;
    for (const MeterItem* item : items) {
        if (!item) { continue; }
        if (item->itemHeight() > kBackgroundSpanThreshold) {
            continue;
        }
        const float bottom = item->y() + item->itemHeight();
        if (bottom > yPos) {
            yPos = bottom;
        }
    }
    return yPos;
}

void ContainerSettingsDialog::addNewItem(const QString& typeTag)
{
    const float yPos = nextStackYPos(workingItems());

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
    ContainerInUseRow newRow;
    newRow.item        = newItem;
    newRow.displayName = defaultDisplayNameFor(newItem);
    newRow.rowId       = QUuid::createUuid();
    m_workingItems.insert(insertAt, newRow);

    refreshItemList();
    refreshAvailableList();
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

    // Edit-container refactor Task 11 — first-class preset classes
    // serialize as JSON blobs with a "kind" discriminator. Detect
    // the JSON prefix and dispatch to the matching subclass before
    // falling through to the pipe-delimited primitive registry.
    if (data.startsWith(QLatin1Char('{'))) {
        const QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8());
        if (doc.isObject()) {
            const QString kind = doc.object()
                .value(QStringLiteral("kind")).toString();
            MeterItem* preset = nullptr;
            if      (kind == QLatin1String("AnanMM"))             { preset = new AnanMultiMeterItem(); }
            else if (kind == QLatin1String("CrossNeedle"))        { preset = new CrossNeedleItem(); }
            else if (kind == QLatin1String("SMeterPreset"))       { preset = new SMeterPresetItem(); }
            else if (kind == QLatin1String("PowerSwrPreset"))     { preset = new PowerSwrPresetItem(); }
            else if (kind == QLatin1String("MagicEyePreset"))     { preset = new MagicEyePresetItem(); }
            else if (kind == QLatin1String("SignalTextPreset"))   { preset = new SignalTextPresetItem(); }
            else if (kind == QLatin1String("HistoryGraphPreset")) { preset = new HistoryGraphPresetItem(); }
            else if (kind == QLatin1String("VfoDisplayPreset"))   { preset = new VfoDisplayPresetItem(); }
            else if (kind == QLatin1String("ClockPreset"))        { preset = new ClockPresetItem(); }
            else if (kind == QLatin1String("ContestPreset"))      { preset = new ContestPresetItem(); }
            else if (kind == QLatin1String("BarPreset"))          { preset = new BarPresetItem(); }
            if (preset) {
                if (preset->deserialize(data)) { return preset; }
                delete preset;
                return nullptr;
            }
        }
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
        // Edit-container refactor Task 11: first-class preset classes
        // use JSON-format serialize(); their "kind" discriminator
        // arrives here as the typeTag when refreshItemList feeds it
        // through the JSON-aware path below.
        { QStringLiteral("AnanMM"),             QStringLiteral("ANAN Multi Meter") },
        { QStringLiteral("CrossNeedle"),        QStringLiteral("Cross-Needle") },
        { QStringLiteral("SMeterPreset"),       QStringLiteral("S-Meter") },
        { QStringLiteral("PowerSwrPreset"),     QStringLiteral("Power / SWR") },
        { QStringLiteral("MagicEyePreset"),     QStringLiteral("Magic Eye") },
        { QStringLiteral("SignalTextPreset"),   QStringLiteral("Signal Text (preset)") },
        { QStringLiteral("HistoryGraphPreset"), QStringLiteral("History Graph (preset)") },
        { QStringLiteral("VfoDisplayPreset"),   QStringLiteral("VFO Display (preset)") },
        { QStringLiteral("ClockPreset"),        QStringLiteral("Clock (preset)") },
        { QStringLiteral("ContestPreset"),      QStringLiteral("Contest Layout") },
        { QStringLiteral("BarPreset"),          QStringLiteral("Bar (preset)") },
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
    for (const ContainerInUseRow& row : m_workingItems) {
        MeterItem* item = row.item;
        if (!item) { continue; }

        // Task 13 — prefer the user-editable displayName on the
        // wrapper. Fall back to the tag-derived human name when the
        // wrapper carries nothing yet (e.g. legacy containers loaded
        // via populateItemList before defaultDisplayNameFor seeded).
        QString label = row.displayName;
        if (label.isEmpty()) {
            label = defaultDisplayNameFor(item);
        }

        const int bindingId = item->bindingId();
        if (bindingId >= 0) {
            label += QStringLiteral(" [id:%1]").arg(bindingId);
        }

        auto* lwItem = new QListWidgetItem(label, m_itemList);
        // Store the stable rowId so drag-reorder can reconcile the
        // wrapper vector from the visual list order, and the context
        // menu slots can look the row up without caring about index
        // instability across moves.
        lwItem->setData(Qt::UserRole, row.rowId.toString());
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
    for (const ContainerInUseRow& row : m_workingItems) {
        delete row.item;
    }
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
            ContainerInUseRow row;
            row.item        = item;
            row.displayName = defaultDisplayNameFor(item);
            row.rowId       = QUuid::createUuid();
            m_workingItems.append(row);
        }
    }

    // Phase 3G-7: serializeItems() above strips MMIO bindings (block 5 kept
    // them in-memory only). Re-attach each binding from the live meter into
    // its corresponding working item by index.
    // Phase 3G-9 post-revert: same story for the runtime-only stack
    // metadata (m_stackSlot/m_slotLocalY/H/m_stackBandTop). Copy it
    // from each live item into the matching clone so the dialog's
    // working copies retain their stack tagging through the
    // open/edit/Apply cycle.
    const QVector<MeterItem*> liveItems = meter->items();
    const int n = qMin(liveItems.size(), m_workingItems.size());
    for (int i = 0; i < n; ++i) {
        MeterItem* live = liveItems[i];
        MeterItem* mine = m_workingItems[i].item;
        if (!live || !mine) { continue; }
        if (live->hasMmioBinding()) {
            mine->setMmioBinding(live->mmioGuid(), live->mmioVariable());
        }
        if (live->stackSlot() >= 0) {
            mine->setStackSlot(live->stackSlot());
            mine->setSlotLocalY(live->slotLocalY());
            mine->setSlotLocalH(live->slotLocalH());
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
// Phase 3G-6 block 3 commits 18 + 19: footer buttons
// ---------------------------------------------------------------------------

void ContainerSettingsDialog::onSaveToFile()
{
    if (!m_container) { return; }
    const QString path = QFileDialog::getSaveFileName(this,
        QStringLiteral("Save Container"),
        QString(),
        QStringLiteral("NereusSDR Container (*.nscontainer);;All Files (*)"));
    if (path.isEmpty()) { return; }

    // Pipe-delimited container serialize + newline + MeterWidget
    // serialized items, so the two halves round-trip together.
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, QStringLiteral("Save Failed"),
            QStringLiteral("Could not open %1 for writing.").arg(path));
        return;
    }
    QTextStream out(&f);
    out << m_container->serialize() << '\n';
    if (MeterWidget* meter = findMeterWidget()) {
        out << meter->serializeItems();
    }
}

void ContainerSettingsDialog::onLoadFromFile()
{
    if (!m_container) { return; }
    const QString path = QFileDialog::getOpenFileName(this,
        QStringLiteral("Load Container"),
        QString(),
        QStringLiteral("NereusSDR Container (*.nscontainer);;All Files (*)"));
    if (path.isEmpty()) { return; }

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, QStringLiteral("Load Failed"),
            QStringLiteral("Could not open %1 for reading.").arg(path));
        return;
    }
    QTextStream in(&f);
    const QString containerLine = in.readLine();
    const QString itemsPayload  = in.readAll();

    if (!containerLine.isEmpty()) {
        m_container->deserialize(containerLine);
    }
    if (MeterWidget* meter = findMeterWidget()) {
        meter->deserializeItems(itemsPayload);
        meter->update();
    }
    // Refresh dialog state from the newly-loaded container.
    populateItemList();
    refreshAvailableList();
    m_container->update();
}

void ContainerSettingsDialog::onOpenMmioDialog()
{
    // Phase 3G-6 block 5 phase 3: real MMIO endpoints dialog. The
    // block 3 commit 18 stub is gone — this now opens the endpoint
    // manager with live add/edit/remove backed by
    // ExternalVariableEngine.
    MmioEndpointsDialog dlg(this);
    dlg.exec();
}

// ---------------------------------------------------------------------------
// Phase 3G-6 block 7 commit 2: container-dropdown auto-commit + switch
// ---------------------------------------------------------------------------

void ContainerSettingsDialog::onContainerDropdownChanged(int index)
{
    if (!m_manager || index < 0) { return; }
    if (!m_containerDropdown) { return; }

    const QString newId = m_containerDropdown->itemData(index).toString();
    if (newId.isEmpty()) { return; }

    ContainerWidget* newContainer = m_manager->container(newId);
    if (!newContainer || newContainer == m_container) { return; }

    // Auto-commit current edits to the outgoing container so the
    // user doesn't lose work just by switching the dropdown.
    applyToContainer();

    // Switch the dialog's bound container.
    m_container = newContainer;

    // Repopulate everything from the new container's state.
    populateItemList();

    // Reload the property bar checkboxes / spinners from the new
    // container — the bar widgets are persistent, only their values
    // come from m_container.
    if (m_titleEdit) { m_titleEdit->setText(m_container->notes()); }
    if (m_borderCheck) { m_borderCheck->setChecked(m_container->hasBorder()); }
    if (m_rxSourceCombo) {
        const int idx = m_rxSourceCombo->findData(m_container->rxSource());
        if (idx >= 0) { m_rxSourceCombo->setCurrentIndex(idx); }
    }
    if (m_showOnRxCheck) { m_showOnRxCheck->setChecked(m_container->showOnRx()); }
    if (m_showOnTxCheck) { m_showOnTxCheck->setChecked(m_container->showOnTx()); }
    if (m_lockCheck) { m_lockCheck->setChecked(m_container->isLocked()); }
    if (m_hideTitleCheck) { m_hideTitleCheck->setChecked(!m_container->isTitleBarVisible()); }
    if (m_minimisesCheck) { m_minimisesCheck->setChecked(m_container->containerMinimises()); }
    if (m_autoHeightCheck) { m_autoHeightCheck->setChecked(m_container->autoHeight()); }
    if (m_hidesWhenRxNotUsedCheck) {
        m_hidesWhenRxNotUsedCheck->setChecked(m_container->containerHidesWhenRxNotUsed());
    }
    if (m_highlightCheck) { m_highlightCheck->setChecked(m_container->isHighlighted()); }

    // Task 12 — hybrid rule is per-container; recompute the
    // available-list enable state against the newly-bound container's
    // working items.
    refreshAvailableList();

    // Take a fresh snapshot of the new container so Cancel reverts
    // to its state-on-switch, not the original opening state.
    takeSnapshot();
}

// ---------------------------------------------------------------------------
// Phase 3G-6 block 7 commit 2: copy/paste item settings clipboard
// ---------------------------------------------------------------------------

void ContainerSettingsDialog::onCopyItemSettings()
{
    const int row = m_itemList ? m_itemList->currentRow() : -1;
    if (row < 0 || row >= m_workingItems.size()) { return; }
    MeterItem* item = m_workingItems[row].item;
    if (!item) { return; }

    m_clipboardSerialized = item->serialize();
    const int pipeIdx = m_clipboardSerialized.indexOf(QLatin1Char('|'));
    m_clipboardTypeTag = (pipeIdx >= 0)
        ? m_clipboardSerialized.left(pipeIdx)
        : m_clipboardSerialized;
    updateCopyPasteButtonState();
}

void ContainerSettingsDialog::onPasteItemSettings()
{
    if (m_clipboardSerialized.isEmpty()) { return; }
    const int row = m_itemList ? m_itemList->currentRow() : -1;
    if (row < 0 || row >= m_workingItems.size()) { return; }
    MeterItem* item = m_workingItems[row].item;
    if (!item) { return; }

    // Type-tag check: only allow paste between items of the same
    // class so we don't try to deserialize NEEDLE bytes into a TEXT
    // item. Matches Thetis paste-settings behavior.
    const QString cur = item->serialize();
    const int pipeIdx = cur.indexOf(QLatin1Char('|'));
    const QString curTag = (pipeIdx >= 0) ? cur.left(pipeIdx) : cur;
    if (curTag != m_clipboardTypeTag) { return; }

    item->deserialize(m_clipboardSerialized);
    applyToContainer();

    // Force the editor stack to rebuild so the property fields
    // reload from the freshly-pasted item state.
    onItemSelectionChanged();
}

void ContainerSettingsDialog::updateCopyPasteButtonState()
{
    const int row = m_itemList ? m_itemList->currentRow() : -1;
    const bool haveSelection = (row >= 0 && row < m_workingItems.size());
    if (m_btnCopySettings) {
        m_btnCopySettings->setEnabled(haveSelection);
    }
    if (m_btnPasteSettings) {
        bool typeMatches = false;
        if (haveSelection && !m_clipboardTypeTag.isEmpty()
            && m_workingItems[row].item) {
            const QString cur = m_workingItems[row].item->serialize();
            const int pipeIdx = cur.indexOf(QLatin1Char('|'));
            const QString curTag = (pipeIdx >= 0) ? cur.left(pipeIdx) : cur;
            typeMatches = (curTag == m_clipboardTypeTag);
        }
        m_btnPasteSettings->setEnabled(typeMatches);
    }
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
    // Phase 3G-7: parallel snapshot of MMIO bindings (not in text snapshot).
    m_mmioSnapshot.clear();
    if (meter) {
        for (MeterItem* item : meter->items()) {
            if (item) {
                m_mmioSnapshot.append({item->mmioGuid(), item->mmioVariable()});
            } else {
                m_mmioSnapshot.append({QUuid(), QString()});
            }
        }
    }
    m_snapshotTaken = true;
}

void ContainerSettingsDialog::revertFromSnapshot()
{
    if (!m_snapshotTaken || !m_container) { return; }
    m_container->deserialize(m_containerSnapshot);
    if (MeterWidget* meter = findMeterWidget()) {
        meter->deserializeItems(m_itemsSnapshot);
        // Phase 3G-7: restore MMIO bindings the text snapshot couldn't carry.
        const QVector<MeterItem*> liveItems = meter->items();
        const int n = qMin(liveItems.size(), m_mmioSnapshot.size());
        for (int i = 0; i < n; ++i) {
            if (liveItems[i] && !m_mmioSnapshot[i].first.isNull()) {
                liveItems[i]->setMmioBinding(
                    m_mmioSnapshot[i].first,
                    m_mmioSnapshot[i].second);
            }
        }
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

    // Phase 3G-6 block 3 commit 17: write the new toggles back too.
    if (m_lockCheck) {
        m_container->setLocked(m_lockCheck->isChecked());
    }
    if (m_hideTitleCheck) {
        m_container->setTitleBarVisible(!m_hideTitleCheck->isChecked());
    }
    if (m_minimisesCheck) {
        m_container->setContainerMinimises(m_minimisesCheck->isChecked());
    }
    if (m_autoHeightCheck) {
        m_container->setAutoHeight(m_autoHeightCheck->isChecked());
    }
    if (m_hidesWhenRxNotUsedCheck) {
        m_container->setContainerHidesWhenRxNotUsed(
            m_hidesWhenRxNotUsedCheck->isChecked());
    }

    // Write items back to the real MeterWidget
    MeterWidget* target = findMeterWidget();
    if (!target) {
        return;
    }

    target->clearItems();
    for (const ContainerInUseRow& row : m_workingItems) {
        const MeterItem* item = row.item;
        if (!item) { continue; }
        const QString data = item->serialize();
        MeterItem* clone = createItemFromSerialized(data);
        if (clone) {
            // Phase 3G-7: serialize() drops MMIO bindings; copy them
            // directly so Apply doesn't silently break the binding the
            // user just made via the "Variable…" picker.
            if (item->hasMmioBinding()) {
                clone->setMmioBinding(item->mmioGuid(), item->mmioVariable());
            }
            // Phase 3G-9 post-revert: serialize() also drops the
            // runtime-only stack metadata. Copy it explicitly so the
            // cloned item on the target MeterWidget participates in
            // reflowStackedItems() on the next resize (and right
            // below, when we kick a reflow so the Apply preview
            // reflects the Thetis-style slot layout immediately).
            if (item->stackSlot() >= 0) {
                clone->setStackSlot(item->stackSlot());
                clone->setSlotLocalY(item->slotLocalY());
                clone->setSlotLocalH(item->slotLocalH());
            }
            target->addItem(clone);
            // Re-wire interactive item signals through the container
            m_container->wireInteractiveItem(clone);
        }
    }
    target->reflowStackedItems();
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
    // Phase E — bar-row presets append to the current stack instead
    // of replacing. This matches Thetis's Appearance → Meters/Gadgets
    // flow where each meter type the user picks adds a new row below
    // the previous one. Composite presets (ANAN MM, Cross Needle,
    // Magic Eye, full S-Meter, Power/SWR) still replace on load
    // because they occupy the entire container.
    static const QSet<QString> kBarRowPresets = {
        QStringLiteral("Alc"),       QStringLiteral("AlcGain"),
        QStringLiteral("AlcGroup"),  QStringLiteral("Mic"),
        QStringLiteral("Comp"),      QStringLiteral("Eq"),
        QStringLiteral("Leveler"),   QStringLiteral("LevelerGain"),
        QStringLiteral("Cfc"),       QStringLiteral("CfcGain"),
        QStringLiteral("Adc"),       QStringLiteral("AdcMaxMag"),
        QStringLiteral("Agc"),       QStringLiteral("AgcGain"),
        QStringLiteral("Pbsnr"),     QStringLiteral("SignalBar"),
        QStringLiteral("AvgSignalBar"), QStringLiteral("MaxBinBar"),
    };
    if (kBarRowPresets.contains(name)) {
        appendPresetRow(name);
        return;
    }

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

    for (const ContainerInUseRow& r : m_workingItems) { delete r.item; }
    m_workingItems.clear();

    // Edit-container refactor Task 19 — ItemGroup is gone. Whole-meter
    // preset names map directly to a first-class preset MeterItem
    // subclass; each preset fills the container with a single row whose
    // rect is the full (0, 0, 1, 1) area. Bar-row presets were already
    // delegated to appendPresetRow() above and never reach this branch.
    // Rotator / FilterDisplay stay as direct single-item constructions
    // (no ItemGroup provenance — they never had factories).
    MeterItem* created = nullptr;

    if (name == QLatin1String("SMeterOnly")) {
        created = new SMeterPresetItem(this);
    } else if (name == QLatin1String("SignalText")) {
        created = new SignalTextPresetItem(this);
    } else if (name == QLatin1String("AnanMM")) {
        created = new AnanMultiMeterItem(this);
    } else if (name == QLatin1String("CrossNeedle")) {
        created = new CrossNeedleItem(this);
    } else if (name == QLatin1String("MagicEye")) {
        created = new MagicEyePresetItem(this);
    } else if (name == QLatin1String("History")) {
        created = new HistoryGraphPresetItem(this);
    } else if (name == QLatin1String("PowerSwr")) {
        created = new PowerSwrPresetItem(this);
    } else if (name == QLatin1String("Vfo")) {
        created = new VfoDisplayPresetItem(this);
    } else if (name == QLatin1String("Clock")) {
        created = new ClockPresetItem(this);
    } else if (name == QLatin1String("Contest")) {
        created = new ContestPresetItem(this);
    } else if (name == QLatin1String("Spacer")) {
        auto* spacer = new SpacerItem();
        spacer->setRect(0.0f, 0.0f, 1.0f, 1.0f);
        created = spacer;
    } else if (name == QLatin1String("Rotator")) {
        auto* rotator = new RotatorItem();
        rotator->setRect(0.0f, 0.0f, 1.0f, 1.0f);
        rotator->setBindingId(300);
        created = rotator;
    } else if (name == QLatin1String("FilterDisplay")) {
        auto* filter = new FilterDisplayItem();
        filter->setRect(0.0f, 0.0f, 1.0f, 1.0f);
        created = filter;
    }

    if (created) {
        // Composite presets take the full container rect. Their
        // constructors already install a sensible default rect (e.g.
        // ANAN MM defaults to (0, 0, 1, 0.441)); for whole-container
        // "replace" semantics we expand that to the full (0, 0, 1, 1)
        // area so the preset fills the container immediately after
        // load. This matches the pre-refactor ItemGroup::installInto
        // behavior which mapped factory-local rects to the full target
        // rect the caller passed in.
        created->setRect(0.0f, 0.0f, 1.0f, 1.0f);
        ContainerInUseRow newRow;
        newRow.item        = created;
        newRow.displayName = defaultDisplayNameFor(created);
        newRow.rowId       = QUuid::createUuid();
        m_workingItems.append(newRow);
    }

    refreshItemList();
    refreshAvailableList();
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
    for (const ContainerInUseRow& row : m_workingItems) {
        if (row.item) { lines << row.item->serialize(); }
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

    for (const ContainerInUseRow& r : m_workingItems) { delete r.item; }
    m_workingItems.clear();
    for (MeterItem* imp : imported) {
        ContainerInUseRow newRow;
        newRow.item        = imp;
        newRow.displayName = defaultDisplayNameFor(imp);
        newRow.rowId       = QUuid::createUuid();
        m_workingItems.append(newRow);
    }

    refreshItemList();
    refreshAvailableList();
    if (!m_workingItems.isEmpty()) {
        m_itemList->setCurrentRow(0);
    }
    updatePreview();
}

// ---------------------------------------------------------------------------
// buildTypeSpecificEditor — type-tag dispatch to BaseItemEditor subclass
// (block 4 phase 3, restored after block 7 commit 1 deleted the legacy
// buildXItemEditor methods that lived above this in the file).
// ---------------------------------------------------------------------------

QWidget* ContainerSettingsDialog::buildTypeSpecificEditor(MeterItem* item)
{
    if (!item) {
        return nullptr;
    }

    const QString serialized = item->serialize();
    const int pipeIdx = serialized.indexOf(QLatin1Char('|'));
    const QString typeTag = (pipeIdx >= 0) ? serialized.left(pipeIdx) : serialized;

    BaseItemEditor* ed = nullptr;

    if      (typeTag == QLatin1String("BAR"))            ed = new BarItemEditor(this);
    else if (typeTag == QLatin1String("SOLID"))          ed = new SolidColourItemEditor(this);
    else if (typeTag == QLatin1String("SPACER"))         ed = new SpacerItemEditor(this);
    else if (typeTag == QLatin1String("FADECOVER"))      ed = new FadeCoverItemEditor(this);
    else if (typeTag == QLatin1String("IMAGE"))          ed = new ImageItemEditor(this);
    else if (typeTag == QLatin1String("SCALE"))          ed = new ScaleItemEditor(this);
    else if (typeTag == QLatin1String("NEEDLE"))         ed = new NeedleItemEditor(this);
    else if (typeTag == QLatin1String("NEEDLESCALEPWR")) ed = new NeedleScalePwrItemEditor(this);
    else if (typeTag == QLatin1String("TEXT"))           ed = new TextItemEditor(this);
    else if (typeTag == QLatin1String("TEXTOVERLAY"))    ed = new TextOverlayItemEditor(this);
    else if (typeTag == QLatin1String("SIGNALTEXT"))     ed = new SignalTextItemEditor(this);
    else if (typeTag == QLatin1String("LED"))            ed = new LedItemEditor(this);
    else if (typeTag == QLatin1String("HISTORY"))        ed = new HistoryGraphItemEditor(this);
    else if (typeTag == QLatin1String("MAGICEYE"))       ed = new MagicEyeItemEditor(this);
    else if (typeTag == QLatin1String("DIAL"))           ed = new DialItemEditor(this);
    else if (typeTag == QLatin1String("WEBIMAGE"))       ed = new WebImageItemEditor(this);
    else if (typeTag == QLatin1String("FILTERDISPLAY"))  ed = new FilterDisplayItemEditor(this);
    else if (typeTag == QLatin1String("ROTATOR"))        ed = new RotatorItemEditor(this);
    else if (typeTag == QLatin1String("CLOCK"))          ed = new ClockItemEditor(this);
    else if (typeTag == QLatin1String("VFODISPLAY"))     ed = new VfoDisplayItemEditor(this);
    else if (typeTag == QLatin1String("CLICKBOX"))       ed = new ClickBoxItemEditor(this);
    else if (typeTag == QLatin1String("DATAOUT"))        ed = new DataOutItemEditor(this);
    else if (typeTag == QLatin1String("BANDBUTTON"))     ed = new BandButtonItemEditor(this);
    else if (typeTag == QLatin1String("MODEBUTTON"))     ed = new ModeButtonItemEditor(this);
    else if (typeTag == QLatin1String("FILTERBUTTON"))   ed = new FilterButtonItemEditor(this);
    else if (typeTag == QLatin1String("ANTENNABUTTON"))  ed = new AntennaButtonItemEditor(this);
    else if (typeTag == QLatin1String("TUNESTEPBUTTON")) ed = new TuneStepButtonItemEditor(this);
    else if (typeTag == QLatin1String("OTHERBUTTON"))    ed = new OtherButtonItemEditor(this);
    else if (typeTag == QLatin1String("VOICERECPLAY"))   ed = new VoiceRecordPlayItemEditor(this);
    else if (typeTag == QLatin1String("DISCORDBUTTON"))  ed = new DiscordButtonItemEditor(this);

    if (ed) {
        ed->setItem(item);
    }
    return ed;
}

// ---------------------------------------------------------------------------
// Edit-container refactor Task 14 — + Add container
// ---------------------------------------------------------------------------

QString ContainerSettingsDialog::nextAutoName() const
{
    // Task 17 — delegate to the canonical helper on ContainerManager
    // so MainWindow's "New Container..." flow and this + Add flow
    // produce identical auto-names.
    if (!m_manager) { return QStringLiteral("Meter 1"); }
    return m_manager->nextMeterAutoName();
}

void ContainerSettingsDialog::onAddContainer()
{
    if (!m_manager) { return; }

    // Commit any pending edits to the outgoing container so the user
    // doesn't lose work just by clicking + Add.
    applyToContainer();

    ContainerWidget* created = m_manager->createContainer(
        1, DockMode::Floating);
    if (!created) { return; }

    created->setNotes(nextAutoName());
    // Populate with an empty MeterWidget so the user can start adding
    // items immediately after the switch.
    created->setContent(new MeterWidget());

    // Track this-session additions so Cancel can roll them back,
    // matching MainWindow's legacy "New Container..." lifecycle where
    // Cancel on the settings dialog undoes pending creations.
    m_containersAddedThisSession.append(created->id());

    // Update the dropdown: createContainer() emits containerAdded
    // which some hosts listen to, but the dialog's dropdown is
    // internal — insert the new entry directly, then switch to it.
    const QString label = created->notes().isEmpty()
        ? created->id().left(8)
        : created->notes();
    if (m_containerDropdown) {
        // If a listener already appended the new row (e.g. via
        // containerAdded), re-use that entry; otherwise add here.
        int idx = m_containerDropdown->findData(created->id());
        if (idx < 0) {
            m_containerDropdown->addItem(label, created->id());
            idx = m_containerDropdown->count() - 1;
        }
        // Re-enable dropdown now that we have >1 container.
        m_containerDropdown->setEnabled(
            m_containerDropdown->count() > 1);
        m_containerDropdown->setCurrentIndex(idx);
    }
}

QString ContainerSettingsDialog::currentContainerDropdownText() const
{
    if (!m_containerDropdown) { return QString(); }
    return m_containerDropdown->currentText();
}

void ContainerSettingsDialog::reject()
{
    // Phase 3G-6 block 3 commit 14: revert the container and its
    // meter items to the snapshot captured on dialog open. Applied
    // changes from Apply clicks are NOT reverted — Apply is a commit
    // action and the snapshot is only a safety net for un-Applied
    // edits.
    revertFromSnapshot();
    // Task 14 follow-up: destroy any containers the user added via
    // + Add this session but didn't commit via Apply/OK. This matches
    // MainWindow's legacy "New Container..." cancel semantics.
    if (m_manager) {
        for (const QString& id : m_containersAddedThisSession) {
            m_manager->destroyContainer(id);
        }
    }
    m_containersAddedThisSession.clear();
    QDialog::reject();
}

bool ContainerSettingsDialog::availableRowIsEnabled(const QString& tag) const
{
    if (!m_availableList) { return false; }
    for (int i = 0; i < m_availableList->count(); ++i) {
        const QListWidgetItem* row = m_availableList->item(i);
        if (!row) { continue; }
        if (row->data(Qt::UserRole).toString() == tag) {
            return (row->flags() & Qt::ItemIsEnabled) != 0;
        }
    }
    return false;
}

} // namespace NereusSDR
