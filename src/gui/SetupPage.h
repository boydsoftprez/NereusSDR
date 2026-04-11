#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QComboBox>
#include <QSlider>
#include <QPushButton>
#include <QSpinBox>
#include <QLineEdit>
#include <QStringList>

namespace NereusSDR {

class RadioModel;

// Base class for all setup dialog pages.
// Provides a consistent dark-themed layout with helper methods for
// building labeled rows of controls.
//
// Two constructor forms:
//   SetupPage(title, model, parent)  — for pages that may later need model access
//   SetupPage(title, parent)         — for pages that only use raw Qt widgets
class SetupPage : public QWidget {
    Q_OBJECT
public:
    // Construct with a RadioModel pointer (may be nullptr).
    explicit SetupPage(const QString& title, RadioModel* model, QWidget* parent = nullptr);
    // Convenience overload for pages that don't need RadioModel access.
    explicit SetupPage(const QString& title, QWidget* parent = nullptr);
    virtual ~SetupPage() = default;

    QString pageTitle() const { return m_title; }
    virtual void syncFromModel();

    // ── Static NYI marker ─────────────────────────────────────────────────────
    // Marks a widget as Not Yet Implemented: disables it and sets a tooltip.
    static void markNyi(QWidget* widget, const QString& phase);

    // ── Section builder ───────────────────────────────────────────────────────
    // Add a titled group box section to the page content area.
    QGroupBox* addSection(const QString& title);

    // ── Convenience row builders ──────────────────────────────────────────────
    // Each creates a labeled row (150px label + control) and adds it to the
    // current section. Returns the created control widget.

    // Toggle button (checkable QPushButton styled as an LED toggle).
    QPushButton* addLabeledToggle(const QString& label);

    // Combo box populated with the given items.
    QComboBox* addLabeledCombo(const QString& label, const QStringList& items);

    // Horizontal slider.
    QSlider* addLabeledSlider(const QString& label, int minimum, int maximum, int value);

    // Integer spin box.
    QSpinBox* addLabeledSpinner(const QString& label, int minimum, int maximum, int value);

    // Push button with custom text.
    QPushButton* addLabeledButton(const QString& label, const QString& buttonText);

    // Read-only label showing a value string.
    QLabel* addLabeledLabel(const QString& label, const QString& value);

    // Single-line text edit with optional placeholder.
    QLineEdit* addLabeledEdit(const QString& label, const QString& placeholder = {});

protected:
    QVBoxLayout* contentLayout() { return m_contentLayout; }
    RadioModel*  model()         { return m_model; }

    // ── Low-level helpers (for subclasses that build complex layouts) ─────────
    // Add a labeled row where the control widget is pre-created.
    QHBoxLayout* addLabeledCombo(QLayout* parent, const QString& label, QComboBox* combo);
    QHBoxLayout* addLabeledSlider(QLayout* parent, const QString& label, QSlider* slider,
                                   QLabel* valueLabel = nullptr);
    QHBoxLayout* addLabeledToggle(QLayout* parent, const QString& label, QPushButton* toggle);
    QHBoxLayout* addLabeledSpinner(QLayout* parent, const QString& label, QSpinBox* spinner);
    QHBoxLayout* addLabeledEdit(QLayout* parent, const QString& label, QLineEdit* edit);
    QHBoxLayout* addLabeledLabel(QLayout* parent, const QString& label, QLabel* value);

private:
    void init(const QString& title);
    QHBoxLayout* makeLabeledRow(QLayout* parent, const QString& labelText, QWidget* control);

    QString       m_title;
    RadioModel*   m_model          = nullptr;
    QVBoxLayout*  m_contentLayout  = nullptr;
    QVBoxLayout*  m_activeSectionLayout = nullptr;  // layout of the last added section
};

} // namespace NereusSDR
