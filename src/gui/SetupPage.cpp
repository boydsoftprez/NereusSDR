#include "SetupPage.h"

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

// ── Section helper ────────────────────────────────────────────────────────────

QGroupBox* SetupPage::addSection(const QString& title, int wired, int total)
{
    QString heading = title;
    if (total > 0) {
        heading = QStringLiteral("%1 (%2/%3 wired)").arg(title).arg(wired).arg(total);
    }

    auto* group = new QGroupBox(heading);
    group->setStyleSheet(kGroupStyle);

    auto* groupLayout = new QVBoxLayout(group);
    groupLayout->setContentsMargins(8, 4, 8, 8);
    groupLayout->setSpacing(4);

    // Insert before the trailing stretch
    const int stretchIndex = m_contentLayout->count() - 1;
    m_contentLayout->insertWidget(stretchIndex, group);

    return group;
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

// ── Public helper methods ─────────────────────────────────────────────────────

QHBoxLayout* SetupPage::addLabeledCombo(QLayout* parent, const QString& label, QComboBox* combo)
{
    combo->setStyleSheet(
        "QComboBox { background: #1a2a3a; border: 1px solid #304050; "
        "border-radius: 3px; color: #c8d8e8; font-size: 12px; padding: 2px 4px; }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView { background: #1a2a3a; color: #c8d8e8; "
        "selection-background-color: #00b4d8; }");
    return makeLabeledRow(parent, label, combo);
}

QHBoxLayout* SetupPage::addLabeledSlider(QLayout* parent, const QString& label,
                                          QSlider* slider, QLabel* valueLabel)
{
    slider->setStyleSheet(
        "QSlider::groove:horizontal { background: #1a2a3a; height: 4px; "
        "border-radius: 2px; }"
        "QSlider::handle:horizontal { background: #00b4d8; width: 12px; "
        "height: 12px; border-radius: 6px; margin: -4px 0; }");

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
    spinner->setStyleSheet(
        "QSpinBox { background: #1a2a3a; border: 1px solid #304050; "
        "border-radius: 3px; color: #c8d8e8; font-size: 12px; padding: 2px 4px; }");
    return makeLabeledRow(parent, label, spinner);
}

QHBoxLayout* SetupPage::addLabeledEdit(QLayout* parent, const QString& label, QLineEdit* edit)
{
    edit->setStyleSheet(
        "QLineEdit { background: #1a2a3a; border: 1px solid #304050; "
        "border-radius: 3px; color: #c8d8e8; font-size: 12px; padding: 2px 4px; }");
    return makeLabeledRow(parent, label, edit);
}

QHBoxLayout* SetupPage::addLabeledLabel(QLayout* parent, const QString& label, QLabel* value)
{
    value->setStyleSheet("QLabel { color: #00c8ff; font-size: 12px; font-weight: bold; }");
    return makeLabeledRow(parent, label, value);
}

} // namespace NereusSDR
