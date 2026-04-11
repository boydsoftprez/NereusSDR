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

namespace NereusSDR {

class RadioModel;

// Base class for all setup dialog pages.
// Provides a consistent dark-themed layout with helper methods for
// building labeled rows of controls.
class SetupPage : public QWidget {
    Q_OBJECT
public:
    explicit SetupPage(const QString& title, RadioModel* model, QWidget* parent = nullptr);
    virtual ~SetupPage() = default;

    QString pageTitle() const { return m_title; }
    virtual void syncFromModel();

protected:
    QVBoxLayout* contentLayout() { return m_contentLayout; }
    RadioModel*  model()         { return m_model; }

    // Add a titled section group box. wired/total show "Title (N/M wired)" if total > 0.
    QGroupBox* addSection(const QString& title, int wired = 0, int total = 0);

    // Helper methods — create a horizontal row with a fixed-width label and a control.
    QHBoxLayout* addLabeledCombo(QLayout* parent, const QString& label, QComboBox* combo);
    QHBoxLayout* addLabeledSlider(QLayout* parent, const QString& label, QSlider* slider, QLabel* valueLabel = nullptr);
    QHBoxLayout* addLabeledToggle(QLayout* parent, const QString& label, QPushButton* toggle);
    QHBoxLayout* addLabeledSpinner(QLayout* parent, const QString& label, QSpinBox* spinner);
    QHBoxLayout* addLabeledEdit(QLayout* parent, const QString& label, QLineEdit* edit);
    QHBoxLayout* addLabeledLabel(QLayout* parent, const QString& label, QLabel* value);

private:
    QHBoxLayout* makeLabeledRow(QLayout* parent, const QString& labelText, QWidget* control);

    QString       m_title;
    RadioModel*   m_model;
    QVBoxLayout*  m_contentLayout = nullptr;
};

} // namespace NereusSDR
