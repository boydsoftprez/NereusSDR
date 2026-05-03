// =================================================================
// src/gui/setup/FilterPresetsSetupPage.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original file. See FilterPresetsSetupPage.h for header.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-02 — Original implementation for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted authoring via Anthropic
//                 Claude Code (Stage C2 filter preset editor).
// =================================================================

#include "FilterPresetsSetupPage.h"
#include "models/FilterPresetStore.h"
#include "models/SliceModel.h"

#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace NereusSDR {

// ── Construction ──────────────────────────────────────────────────────────────

FilterPresetsSetupPage::FilterPresetsSetupPage(FilterPresetStore* store,
                                               RadioModel* model,
                                               QWidget* parent)
    : SetupPage(QStringLiteral("Filter Presets"), model, parent)
    , m_store(store)
{
    buildUi();

    if (m_store) {
        connect(m_store, &FilterPresetStore::presetsChanged,
                this,    &FilterPresetsSetupPage::onPresetsChanged);
    }
}

// ── buildUi ───────────────────────────────────────────────────────────────────

void FilterPresetsSetupPage::buildUi()
{
    // ── Mode selector ─────────────────────────────────────────────────────────
    QGroupBox* modeBox = addSection(QStringLiteral("Mode"));
    auto* modeRow = new QHBoxLayout;
    modeRow->setContentsMargins(0, 0, 0, 0);
    modeRow->setSpacing(8);

    auto* modeLbl = new QLabel(QStringLiteral("DSP Mode:"), this);
    modeLbl->setStyleSheet(QStringLiteral("color: #c8d8e8;"));

    m_modeCombo = new QComboBox(this);
    m_modeCombo->setStyleSheet(
        QStringLiteral("QComboBox { background: #1a2030; color: #c8d8e8; "
                       "border: 1px solid #304050; border-radius: 3px; padding: 2px 6px; }"
                       "QComboBox::drop-down { background: #202838; }"
                       "QComboBox QAbstractItemView { background: #131326; color: #c8d8e8; }"));

    // Populate with all DSPMode values in enum order.
    static const DSPMode kModes[] = {
        DSPMode::LSB, DSPMode::USB, DSPMode::DSB,
        DSPMode::CWL, DSPMode::CWU, DSPMode::FM,
        DSPMode::AM,  DSPMode::DIGU, DSPMode::SPEC,
        DSPMode::DIGL, DSPMode::SAM, DSPMode::DRM
    };
    for (DSPMode m : kModes) {
        m_modeCombo->addItem(SliceModel::modeName(m),
                             static_cast<int>(m));
    }
    // Default to USB (index 1 in enum order).
    m_modeCombo->setCurrentIndex(1);

    modeRow->addWidget(modeLbl);
    modeRow->addWidget(m_modeCombo);
    modeRow->addStretch();

    modeBox->layout()->addItem(modeRow);

    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FilterPresetsSetupPage::onModeChanged);

    // ── Preset table ──────────────────────────────────────────────────────────
    QGroupBox* tableBox = addSection(QStringLiteral("Presets"));

    m_table = new QTableWidget(0, 6, this);  // rows filled by populateTable()
    m_table->setHorizontalHeaderLabels({
        QStringLiteral("#"),
        QStringLiteral("Name"),
        QStringLiteral("Low (Hz)"),
        QStringLiteral("High (Hz)"),
        QStringLiteral("Width (Hz)"),
        QStringLiteral("Reorder")
    });
    m_table->setStyleSheet(
        QStringLiteral("QTableWidget { background: #131326; color: #c8d8e8; "
                       "  gridline-color: #304050; border: 1px solid #304050; }"
                       "QTableWidget::item { padding: 2px 4px; }"
                       "QTableWidget::item:selected { background: #1a3050; }"
                       "QHeaderView::section { background: #1a2030; color: #8aa8c0; "
                       "  border: 1px solid #304050; padding: 4px; }"));
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed);
    m_table->setColumnWidth(0, 30);
    m_table->setColumnWidth(2, 90);
    m_table->setColumnWidth(3, 90);
    m_table->setColumnWidth(4, 90);
    m_table->setColumnWidth(5, 70);
    m_table->verticalHeader()->setVisible(false);

    tableBox->layout()->addItem(new QWidgetItem(m_table));

    // ── Row/Mode reset buttons ────────────────────────────────────────────────
    QGroupBox* actBox = addSection(QStringLiteral("Actions"));
    auto* actRow = new QHBoxLayout;
    actRow->setContentsMargins(0, 0, 0, 0);
    actRow->setSpacing(8);

    static const QString kBtnStyle = QStringLiteral(
        "QPushButton { background: #203040; color: #c8d8e8; border: 1px solid #304050; "
        "  border-radius: 3px; padding: 4px 10px; }"
        "QPushButton:hover { background: #2a4060; }"
        "QPushButton:pressed { background: #1a2840; }");

    auto* resetRowBtn  = new QPushButton(QStringLiteral("Reset Selected Row"), this);
    auto* resetModeBtn = new QPushButton(QStringLiteral("Reset All Rows for This Mode"), this);
    resetRowBtn->setStyleSheet(kBtnStyle);
    resetModeBtn->setStyleSheet(kBtnStyle);

    actRow->addWidget(resetRowBtn);
    actRow->addWidget(resetModeBtn);
    actRow->addStretch();

    actBox->layout()->addItem(actRow);

    connect(resetRowBtn,  &QPushButton::clicked, this, &FilterPresetsSetupPage::onResetThisRow);
    connect(resetModeBtn, &QPushButton::clicked, this, &FilterPresetsSetupPage::onResetThisMode);

    // ── Global reset ─────────────────────────────────────────────────────────
    QGroupBox* globalBox = addSection(QStringLiteral("Global"));
    auto* globalRow = new QHBoxLayout;
    globalRow->setContentsMargins(0, 0, 0, 0);
    globalRow->setSpacing(8);

    auto* resetAllBtn = new QPushButton(
        QStringLiteral("Reset Every Mode to Thetis Defaults"), this);
    resetAllBtn->setStyleSheet(kBtnStyle);

    globalRow->addWidget(resetAllBtn);
    globalRow->addStretch();

    globalBox->layout()->addItem(globalRow);

    connect(resetAllBtn, &QPushButton::clicked, this, &FilterPresetsSetupPage::onResetAll);

    // Initial population
    populateTable();
}

// ── syncFromModel ─────────────────────────────────────────────────────────────

void FilterPresetsSetupPage::syncFromModel()
{
    populateTable();
}

// ── onModeChanged ─────────────────────────────────────────────────────────────

void FilterPresetsSetupPage::onModeChanged(int /*index*/)
{
    populateTable();
}

// ── currentMode ──────────────────────────────────────────────────────────────

DSPMode FilterPresetsSetupPage::currentMode() const
{
    if (!m_modeCombo) { return DSPMode::USB; }
    return static_cast<DSPMode>(m_modeCombo->currentData().toInt());
}

// ── populateTable ─────────────────────────────────────────────────────────────

void FilterPresetsSetupPage::populateTable()
{
    if (!m_store || !m_table) { return; }
    m_updatingTable = true;

    const DSPMode mode = currentMode();
    const QList<FilterPreset> presets = m_store->presetsForMode(mode);
    const int count = presets.size();

    m_table->setRowCount(count);

    for (int i = 0; i < count; ++i) {
        const FilterPreset& p = presets[i];
        const int width = qAbs(p.high - p.low);

        // Col 0: slot number (read-only)
        auto* slotItem = new QTableWidgetItem(QString::number(i + 1));
        slotItem->setFlags(Qt::ItemIsEnabled);
        slotItem->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(i, 0, slotItem);

        // Col 1: name (editable QLineEdit)
        auto* nameEdit = new QLineEdit(p.name, m_table);
        nameEdit->setMaxLength(32);
        nameEdit->setStyleSheet(QStringLiteral(
            "QLineEdit { background: #1a2030; color: #c8d8e8; "
            "border: 1px solid #304050; border-radius: 2px; padding: 1px 3px; }"));
        // Connect editing to live persist
        const int row = i;
        connect(nameEdit, &QLineEdit::editingFinished, this, [this, row] {
            commitTableRow(row);
        });
        m_table->setCellWidget(i, 1, nameEdit);

        // Col 2: low (QSpinBox)
        auto* lowSpin = new QSpinBox(m_table);
        lowSpin->setRange(-10000, 10000);
        lowSpin->setValue(p.low);
        lowSpin->setSuffix(QStringLiteral(" Hz"));
        lowSpin->setStyleSheet(QStringLiteral(
            "QSpinBox { background: #1a2030; color: #c8d8e8; "
            "border: 1px solid #304050; border-radius: 2px; padding: 1px; }"
            "QSpinBox::up-button, QSpinBox::down-button { background: #202838; width: 14px; }"));
        connect(lowSpin, &QSpinBox::editingFinished, this, [this, row] {
            commitTableRow(row);
        });
        m_table->setCellWidget(i, 2, lowSpin);

        // Col 3: high (QSpinBox)
        auto* highSpin = new QSpinBox(m_table);
        highSpin->setRange(-10000, 10000);
        highSpin->setValue(p.high);
        highSpin->setSuffix(QStringLiteral(" Hz"));
        highSpin->setStyleSheet(lowSpin->styleSheet());
        connect(highSpin, &QSpinBox::editingFinished, this, [this, row] {
            commitTableRow(row);
        });
        m_table->setCellWidget(i, 3, highSpin);

        // Col 4: width (read-only label)
        auto* widthItem = new QTableWidgetItem(QStringLiteral("%1").arg(width));
        widthItem->setFlags(Qt::ItemIsEnabled);
        widthItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_table->setItem(i, 4, widthItem);

        // Col 5: reorder buttons (↑ and ↓)
        auto* reorderWidget = new QWidget(m_table);
        auto* reorderLayout = new QHBoxLayout(reorderWidget);
        reorderLayout->setContentsMargins(2, 1, 2, 1);
        reorderLayout->setSpacing(2);

        static const QString kArrowStyle = QStringLiteral(
            "QPushButton { background: #203040; color: #c8d8e8; border: 1px solid #304050; "
            "  border-radius: 2px; padding: 0px 4px; font-size: 10px; }"
            "QPushButton:hover { background: #2a4060; }"
            "QPushButton:disabled { color: #505060; border-color: #202030; }");

        if (i > 0) {
            auto* upBtn = new QPushButton(QStringLiteral("↑"), reorderWidget);
            upBtn->setFixedSize(22, 20);
            upBtn->setStyleSheet(kArrowStyle);
            connect(upBtn, &QPushButton::clicked, this, [this, i] {
                moveRow(i, -1);
            });
            reorderLayout->addWidget(upBtn);
        } else {
            reorderLayout->addSpacing(24);
        }

        if (i < count - 1) {
            auto* downBtn = new QPushButton(QStringLiteral("↓"), reorderWidget);
            downBtn->setFixedSize(22, 20);
            downBtn->setStyleSheet(kArrowStyle);
            connect(downBtn, &QPushButton::clicked, this, [this, i] {
                moveRow(i, +1);
            });
            reorderLayout->addWidget(downBtn);
        } else {
            reorderLayout->addSpacing(24);
        }

        reorderLayout->addStretch();
        m_table->setCellWidget(i, 5, reorderWidget);

        m_table->setRowHeight(i, 26);
    }

    m_updatingTable = false;
}

// ── commitTableRow ────────────────────────────────────────────────────────────

void FilterPresetsSetupPage::commitTableRow(int row)
{
    if (m_updatingTable || !m_store) { return; }

    auto* nameEdit = qobject_cast<QLineEdit*>(m_table->cellWidget(row, 1));
    auto* lowSpin  = qobject_cast<QSpinBox*>(m_table->cellWidget(row, 2));
    auto* highSpin = qobject_cast<QSpinBox*>(m_table->cellWidget(row, 3));

    if (!nameEdit || !lowSpin || !highSpin) { return; }

    FilterPreset p;
    p.name = nameEdit->text().trimmed();
    if (p.name.isEmpty()) {
        p.name = QStringLiteral("F%1").arg(row + 1);
        nameEdit->setText(p.name);
    }
    p.low  = lowSpin->value();
    p.high = highSpin->value();

    // Update the read-only width column.
    if (auto* widthItem = m_table->item(row, 4)) {
        widthItem->setText(QString::number(qAbs(p.high - p.low)));
    }

    // setPreset emits presetsChanged(mode) which re-enters onPresetsChanged.
    // Guard with m_updatingTable to avoid infinite re-population.
    m_updatingTable = true;
    m_store->setPreset(currentMode(), row, p);
    m_updatingTable = false;
}

// ── moveRow ───────────────────────────────────────────────────────────────────

void FilterPresetsSetupPage::moveRow(int row, int direction)
{
    if (!m_store) { return; }
    const DSPMode mode = currentMode();
    QList<FilterPreset> presets = m_store->presetsForMode(mode);
    const int other = row + direction;
    if (other < 0 || other >= presets.size()) { return; }
    presets.swapItemsAt(row, other);
    // setPresetsForMode will emit presetsChanged → onPresetsChanged → populateTable.
    m_store->setPresetsForMode(mode, presets);
}

// ── onResetThisRow ────────────────────────────────────────────────────────────

void FilterPresetsSetupPage::onResetThisRow()
{
    if (!m_store) { return; }
    const int row = m_table->currentRow();
    if (row < 0) { return; }
    m_store->resetPreset(currentMode(), row);
}

// ── onResetThisMode ───────────────────────────────────────────────────────────

void FilterPresetsSetupPage::onResetThisMode()
{
    if (!m_store) { return; }
    const auto answer = QMessageBox::question(
        this,
        QStringLiteral("Reset Mode Presets"),
        QStringLiteral("Reset all presets for %1 to Thetis defaults?")
            .arg(SliceModel::modeName(currentMode())),
        QMessageBox::Yes | QMessageBox::No);
    if (answer == QMessageBox::Yes) {
        m_store->resetMode(currentMode());
    }
}

// ── onResetAll ────────────────────────────────────────────────────────────────

void FilterPresetsSetupPage::onResetAll()
{
    if (!m_store) { return; }
    const auto answer = QMessageBox::question(
        this,
        QStringLiteral("Reset All Presets"),
        QStringLiteral("Reset ALL filter presets for ALL modes to Thetis defaults?\n\n"
                       "This cannot be undone."),
        QMessageBox::Yes | QMessageBox::No);
    if (answer == QMessageBox::Yes) {
        m_store->resetAll();
    }
}

// ── onPresetsChanged ─────────────────────────────────────────────────────────

void FilterPresetsSetupPage::onPresetsChanged(DSPMode mode)
{
    if (m_updatingTable) { return; }
    // Only repopulate if the changed mode matches the currently-shown mode.
    if (mode == currentMode()) {
        populateTable();
    }
}

} // namespace NereusSDR
