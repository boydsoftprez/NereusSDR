// =================================================================
// src/gui/SetupPage.cpp  (NereusSDR)
// =================================================================
//
// Source attribution (AetherSDR — GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 §5 requirements.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-16 — Ported/adapted in C++20/Qt6 for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
//                 Shared setup-page style constants mirror AetherSDR
//                 `src/gui/RadioSetupDialog.{h,cpp}`.
// =================================================================

#include "SetupPage.h"
#include "StyleConstants.h"

#include <QScrollArea>
#include <QFrame>

namespace NereusSDR {

// Shared style strings — mirror AetherSDR RadioSetupDialog constants
static const QString kGroupStyle =
    "QGroupBox { border: 1px solid #304050; border-radius: 4px; "
    "margin-top: 8px; padding-top: 12px; font-weight: bold; color: #8aa8c0; }"
    "QGroupBox::title { subcontrol-origin: margin; left: 10px; "
    "padding: 0 4px; }";

static const QString kLabelStyle =
    "QLabel { color: #c8d8e8; font-size: 12px; }";

// ── Construction ──────────────────────────────────────────────────────────────

SetupPage::SetupPage(const QString& title, RadioModel* model, QWidget* parent)
    : QWidget(parent), m_title(title), m_model(model)
{
    init(title);
}

SetupPage::SetupPage(const QString& title, QWidget* parent)
    : QWidget(parent), m_title(title), m_model(nullptr)
{
    init(title);
}

void SetupPage::init(const QString& title)
{
    // Apply dark-theme checkbox + radio-button styles at the page root
    // so every QCheckBox / QRadioButton in any derived SetupPage
    // inherits visible indicators over the #0f0f1a background.
    // Without this, system-default indicators render black-on-dark
    // and are invisible to users. Fixes regression reported
    // 2026-04-21 on Phase 3P-H pre-PR debug.
    setStyleSheet(QString("%1 %2").arg(Style::kCheckBoxStyle,
                                       Style::kRadioButtonStyle));

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(12, 8, 12, 8);
    rootLayout->setSpacing(6);

    // Page title
    auto* titleLabel = new QLabel(title);
    titleLabel->setStyleSheet(
        "QLabel { color: #c8d8e8; font-size: 16px; font-weight: bold; "
        "border-bottom: 1px solid #304050; padding-bottom: 4px; }");
    rootLayout->addWidget(titleLabel);

    // Scrollable content area
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("QScrollArea { background: #0f0f1a; border: none; }");

    auto* contentWidget = new QWidget;
    contentWidget->setStyleSheet("QWidget { background: #0f0f1a; }");

    m_contentLayout = new QVBoxLayout(contentWidget);
    m_contentLayout->setContentsMargins(0, 4, 0, 4);
    m_contentLayout->setSpacing(6);
    m_contentLayout->addStretch(1);

    scroll->setWidget(contentWidget);
    rootLayout->addWidget(scroll, 1);
}

// ── Virtual ───────────────────────────────────────────────────────────────────

void SetupPage::syncFromModel()
{
    // Base implementation is a no-op; subclasses override to pull from RadioModel.
}

// ── Static NYI marker ─────────────────────────────────────────────────────────

void SetupPage::markNyi(QWidget* widget, const QString& phase)
{
    if (widget == nullptr) { return; }
    widget->setEnabled(false);
    widget->setToolTip(QStringLiteral("NYI — %1").arg(phase));
}

// ── Section helper ────────────────────────────────────────────────────────────

QGroupBox* SetupPage::addSection(const QString& title)
{
    auto* group = new QGroupBox(title);
    group->setStyleSheet(kGroupStyle);

    auto* groupLayout = new QVBoxLayout(group);
    groupLayout->setContentsMargins(8, 4, 8, 8);
    groupLayout->setSpacing(4);

    m_activeSectionLayout = groupLayout;

    // Insert before the trailing stretch
    const int stretchIndex = m_contentLayout->count() - 1;
    m_contentLayout->insertWidget(stretchIndex, group);

    return group;
}

// ── Convenience single-argument row builders ──────────────────────────────────

QPushButton* SetupPage::addLabeledToggle(const QString& label)
{
    auto* btn = new QPushButton;
    btn->setCheckable(true);
    QLayout* target = m_activeSectionLayout ? m_activeSectionLayout : m_contentLayout;
    addLabeledToggle(target, label, btn);
    return btn;
}

QComboBox* SetupPage::addLabeledCombo(const QString& label, const QStringList& items)
{
    auto* combo = new QComboBox;
    combo->addItems(items);
    QLayout* target = m_activeSectionLayout ? m_activeSectionLayout : m_contentLayout;
    addLabeledCombo(target, label, combo);
    return combo;
}

QSlider* SetupPage::addLabeledSlider(const QString& label, int minimum, int maximum, int value)
{
    auto* slider = new QSlider(Qt::Horizontal);
    slider->setRange(minimum, maximum);
    slider->setValue(value);
    QLayout* target = m_activeSectionLayout ? m_activeSectionLayout : m_contentLayout;
    addLabeledSlider(target, label, slider);
    return slider;
}

QSpinBox* SetupPage::addLabeledSpinner(const QString& label, int minimum, int maximum, int value)
{
    auto* spin = new QSpinBox;
    spin->setRange(minimum, maximum);
    spin->setValue(value);
    QLayout* target = m_activeSectionLayout ? m_activeSectionLayout : m_contentLayout;
    addLabeledSpinner(target, label, spin);
    return spin;
}

QPushButton* SetupPage::addLabeledButton(const QString& label, const QString& buttonText)
{
    auto* btn = new QPushButton(buttonText);
    btn->setAutoDefault(false);
    QLayout* target = m_activeSectionLayout ? m_activeSectionLayout : m_contentLayout;
    addLabeledToggle(target, label, btn);  // reuse row layout
    // Re-style as a plain button (not a toggle)
    btn->setCheckable(false);
    btn->setStyleSheet(Style::kButtonStyle);
    return btn;
}

QLabel* SetupPage::addLabeledLabel(const QString& label, const QString& value)
{
    auto* lbl = new QLabel(value);
    QLayout* target = m_activeSectionLayout ? m_activeSectionLayout : m_contentLayout;
    addLabeledLabel(target, label, lbl);
    return lbl;
}

QLineEdit* SetupPage::addLabeledEdit(const QString& label, const QString& placeholder)
{
    auto* edit = new QLineEdit;
    if (!placeholder.isEmpty()) { edit->setPlaceholderText(placeholder); }
    QLayout* target = m_activeSectionLayout ? m_activeSectionLayout : m_contentLayout;
    addLabeledEdit(target, label, edit);
    return edit;
}

// ── Private row builder ───────────────────────────────────────────────────────

QHBoxLayout* SetupPage::makeLabeledRow(QLayout* parent, const QString& labelText, QWidget* control)
{
    auto* row = new QHBoxLayout;
    row->setSpacing(8);

    auto* label = new QLabel(labelText);
    label->setStyleSheet(kLabelStyle);
    label->setFixedWidth(150);
    row->addWidget(label);
    row->addWidget(control, 1);

    // QVBoxLayout and QHBoxLayout both inherit QBoxLayout
    if (auto* box = qobject_cast<QBoxLayout*>(parent)) {
        box->addLayout(row);
    }

    return row;
}

// ── Public helper methods (pre-created control variants) ──────────────────────

QHBoxLayout* SetupPage::addLabeledCombo(QLayout* parent, const QString& label, QComboBox* combo)
{
    combo->setStyleSheet(Style::kComboStyle);
    return makeLabeledRow(parent, label, combo);
}

QHBoxLayout* SetupPage::addLabeledSlider(QLayout* parent, const QString& label,
                                          QSlider* slider, QLabel* valueLabel)
{
    slider->setStyleSheet(Style::kSliderStyle);

    if (valueLabel != nullptr) {
        valueLabel->setStyleSheet("QLabel { color: #00c8ff; font-size: 12px; "
                                  "font-weight: bold; min-width: 40px; }");

        auto* row = new QHBoxLayout;
        row->setSpacing(8);

        auto* lbl = new QLabel(label);
        lbl->setStyleSheet(kLabelStyle);
        lbl->setFixedWidth(150);
        row->addWidget(lbl);
        row->addWidget(slider, 1);
        row->addWidget(valueLabel);

        if (auto* box = qobject_cast<QBoxLayout*>(parent)) {
            box->addLayout(row);
        }
        return row;
    }

    return makeLabeledRow(parent, label, slider);
}

QHBoxLayout* SetupPage::addLabeledToggle(QLayout* parent, const QString& label, QPushButton* toggle)
{
    toggle->setCheckable(true);
    toggle->setStyleSheet(
        "QPushButton { background: #1a2a3a; border: 1px solid #304050; "
        "border-radius: 3px; color: #c8d8e8; font-size: 11px; font-weight: bold; "
        "padding: 3px 10px; }"
        "QPushButton:checked { background: #1a5030; color: #00e060; "
        "border: 1px solid #20a040; }");
    return makeLabeledRow(parent, label, toggle);
}

QHBoxLayout* SetupPage::addLabeledSpinner(QLayout* parent, const QString& label, QSpinBox* spinner)
{
    // No setStyleSheet — matches the AudioAdvancedPage pattern (native
    // QSpinBox styling on macOS has proper up/down buttons). Per user
    // directive 2026-04-23: Audio → Advanced is the canonical style for
    // spinbox-type fields across the Setup dialog.
    return makeLabeledRow(parent, label, spinner);
}

QHBoxLayout* SetupPage::addLabeledEdit(QLayout* parent, const QString& label, QLineEdit* edit)
{
    edit->setStyleSheet(Style::kLineEditStyle);
    return makeLabeledRow(parent, label, edit);
}

QHBoxLayout* SetupPage::addLabeledLabel(QLayout* parent, const QString& label, QLabel* value)
{
    value->setStyleSheet("QLabel { color: #00c8ff; font-size: 12px; font-weight: bold; }");
    return makeLabeledRow(parent, label, value);
}

} // namespace NereusSDR
