// no-port-check: NereusSDR-original file; Thetis console.cs reference is an
//   inline comment giving context for spinbox range only, not ported logic.
// =================================================================
// src/gui/widgets/FilterPresetEditDialog.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original file. See FilterPresetEditDialog.h for header.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-02 — Original implementation for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted authoring via Anthropic
//                 Claude Code (Stage C2 filter preset editor).
// =================================================================

#include "FilterPresetEditDialog.h"
#include "gui/StyleConstants.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

namespace NereusSDR {

// ── Construction ──────────────────────────────────────────────────────────────

FilterPresetEditDialog::FilterPresetEditDialog(FilterPresetStore* store,
                                               DSPMode mode,
                                               int slot,
                                               QWidget* parent)
    : QDialog(parent)
    , m_store(store)
    , m_mode(mode)
    , m_slot(slot)
{
    const FilterPreset current = (store != nullptr)
        ? store->presetsForMode(mode).value(slot, FilterPresetStore::defaultPreset(mode, slot))
        : FilterPresetStore::defaultPreset(mode, slot);
    buildUi(current);
}

// ── buildUi ───────────────────────────────────────────────────────────────────

void FilterPresetEditDialog::buildUi(const FilterPreset& current)
{
    setWindowTitle(QStringLiteral("Edit Filter Preset — F%1").arg(m_slot + 1));
    setModal(true);
    setMinimumWidth(300);

    static const QString kDialogStyle = QStringLiteral(
        "QDialog { background: #0f0f1a; color: #c8d8e8; }"
        "QLabel  { color: #c8d8e8; }"
        "QLineEdit, QSpinBox { background: #1a2030; color: #c8d8e8; border: 1px solid #304050; "
        "  border-radius: 3px; padding: 2px 4px; }"
        "QSpinBox::up-button, QSpinBox::down-button { background: #202838; width: 16px; }"
        "QPushButton { background: #203040; color: #c8d8e8; border: 1px solid #304050; "
        "  border-radius: 3px; padding: 4px 10px; }"
        "QPushButton:hover { background: #2a4060; }"
        "QPushButton:pressed { background: #1a2840; }"
    );
    setStyleSheet(kDialogStyle);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(12, 12, 12, 12);

    // ── Form ─────────────────────────────────────────────────────────────────
    auto* formLayout = new QFormLayout;
    formLayout->setSpacing(6);
    formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_nameEdit = new QLineEdit(current.name, this);
    m_nameEdit->setMaxLength(32);
    formLayout->addRow(QStringLiteral("Name:"), m_nameEdit);

    // Low / High spin boxes — range matches Thetis filter extremes.
    // From Thetis console.cs:5180-5575 [v2.10.3.13] — widest presets are
    // ±10000 Hz (AM/SAM) and +10000 Hz (AM high); −10000 safe lower bound.
    m_lowSpin = new QSpinBox(this);
    m_lowSpin->setRange(-10000, 10000);
    m_lowSpin->setValue(current.low);
    m_lowSpin->setSuffix(QStringLiteral(" Hz"));
    formLayout->addRow(QStringLiteral("Low:"), m_lowSpin);

    m_highSpin = new QSpinBox(this);
    m_highSpin->setRange(-10000, 10000);
    m_highSpin->setValue(current.high);
    m_highSpin->setSuffix(QStringLiteral(" Hz"));
    formLayout->addRow(QStringLiteral("High:"), m_highSpin);

    m_widthLbl = new QLabel(this);
    formLayout->addRow(QStringLiteral("Width:"), m_widthLbl);
    updateWidth();

    mainLayout->addLayout(formLayout);

    // Connect spin boxes to live-update the width label.
    connect(m_lowSpin,  QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int) { updateWidth(); });
    connect(m_highSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int) { updateWidth(); });

    // ── Buttons ───────────────────────────────────────────────────────────────
    auto* btnLayout = new QHBoxLayout;
    btnLayout->setSpacing(6);

    auto* saveBtn  = new QPushButton(QStringLiteral("Save"), this);
    auto* resetBtn = new QPushButton(QStringLiteral("Reset to Default"), this);
    auto* cancelBtn = new QPushButton(QStringLiteral("Cancel"), this);

    btnLayout->addWidget(saveBtn);
    btnLayout->addWidget(resetBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(cancelBtn);

    mainLayout->addLayout(btnLayout);

    connect(saveBtn,   &QPushButton::clicked, this, &FilterPresetEditDialog::onSave);
    connect(resetBtn,  &QPushButton::clicked, this, &FilterPresetEditDialog::onResetToDefault);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

// ── updateWidth ───────────────────────────────────────────────────────────────

void FilterPresetEditDialog::updateWidth()
{
    if (!m_widthLbl || !m_lowSpin || !m_highSpin) { return; }
    const int width = qAbs(m_highSpin->value() - m_lowSpin->value());
    if (width >= 1000) {
        m_widthLbl->setText(QStringLiteral("%1 kHz").arg(width / 1000.0, 0, 'f', 1));
    } else {
        m_widthLbl->setText(QStringLiteral("%1 Hz").arg(width));
    }
}

// ── onSave ────────────────────────────────────────────────────────────────────

void FilterPresetEditDialog::onSave()
{
    if (!m_store) {
        accept();
        return;
    }
    FilterPreset p;
    p.name = m_nameEdit->text().trimmed();
    if (p.name.isEmpty()) {
        p.name = QStringLiteral("F%1").arg(m_slot + 1);
    }
    p.low  = m_lowSpin->value();
    p.high = m_highSpin->value();
    m_store->setPreset(m_mode, m_slot, p);
    accept();
}

// ── onResetToDefault ─────────────────────────────────────────────────────────

void FilterPresetEditDialog::onResetToDefault()
{
    if (!m_store) {
        reject();
        return;
    }
    const FilterPreset def = FilterPresetStore::defaultPreset(m_mode, m_slot);
    m_nameEdit->setText(def.name);
    m_lowSpin->setValue(def.low);
    m_highSpin->setValue(def.high);
    // Apply immediately.
    m_store->resetPreset(m_mode, m_slot);
    accept();
}

} // namespace NereusSDR
