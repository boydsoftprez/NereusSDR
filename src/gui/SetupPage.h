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

    // R-R3-21: a receive page can hold a transmit section. SetupDialog pushes
    // the negotiated transmit permission to every realized page; a page with
    // such a section overrides this and gates just that section, leaving the
    // rest of the page live. The default does nothing.
    virtual void setTransmitPermitted(bool permitted, const QString& reason);

    // Fix wave 2 (M8): in a remote window VOX also needs this computer's
    // microphone line to the Core. SetupDialog pushes it to every realized
    // page; a page holding a VOX control overrides this and shows it
    // disabled with `reason` while there is none. The default does nothing.
    virtual void setVoxPermitted(bool permitted, const QString& reason);

    // R-R3-49 (parity Task 1): a page can hold transmit settings that key
    // nothing (DSP > Options TX combos). SetupDialog pushes the transmit
    // settings gate to every realized page; a page with such settings
    // overrides this and gates just them. The default does nothing.
    virtual void setTransmitSettingsPermitted(bool permitted, const QString& reason);

    // R-R3-49 (parity Task 3): the same gate for settings a later
    // transmitSettingsVersion brought (3: the radio microphone settings and
    // the TX profiles). SetupDialog pushes each version MainWindow gives it;
    // a page overrides this and gates the settings of the versions it
    // holds. Never called in a local window. The default does nothing.
    virtual void setTransmitSettingsPermittedAt(int version, bool permitted,
                                                const QString& reason);

    // R-R3-21 / R-R3-10: whether the Core's settings can be changed from
    // this window right now. False in a remote window while it is not
    // connected to its Core (or has not received the Core's settings yet).
    // SetupDialog pushes it to every realized page; a Mixed page overrides
    // this and gates just the controls that write the Core's settings,
    // leaving this computer's own controls live. The default does nothing.
    // Always true in a local window.
    virtual void setStationSettingsAvailable(bool available, const QString& reason);

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
    // R-R3-21: disables each control with `reason` as its tooltip and
    // accessible description while transmit is not permitted, and puts back
    // the enabled state, tooltip and description each had once it is. Safe
    // to call repeatedly with the same state.
    static void gateTransmitControls(const QList<QWidget*>& controls, bool permitted,
                                     const QString& reason);

    // R-R3-21 / R-R3-10: the same, for the controls that write the Core's
    // settings on a Mixed page, while those settings are unavailable. It
    // keeps its own saved state, so a control must follow only one of the
    // two helpers; a page with a control held for both combines the two
    // conditions and calls one of them.
    static void gateStationControls(const QList<QWidget*>& controls, bool available,
                                    const QString& reason);

    // The same again, for controls locked while the radio is on the air
    // (a page's own on-the-air rule). It keeps its own saved state, so a
    // control follows only this helper; a control also held for transmit
    // combines the two conditions and calls gateTransmitControls.
    static void gateOnAirControls(const QList<QWidget*>& controls, bool offAir,
                                  const QString& reason);

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
