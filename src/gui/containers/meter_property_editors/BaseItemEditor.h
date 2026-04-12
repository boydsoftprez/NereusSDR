#pragma once

#include <QWidget>
#include <QString>

class QFormLayout;
class QVBoxLayout;
class QDoubleSpinBox;
class QSpinBox;
class QCheckBox;
class QComboBox;

namespace NereusSDR {

class MeterItem;

// Phase 3G-6 block 4 — shared base class for every per-item property
// editor. Each subclass owns its own widget in a QStackedWidget page
// and writes changes directly to the currently-selected MeterItem
// through setItem() + internal signal wiring.
//
// The base class exposes ONLY fields that live on MeterItem itself:
//   - Position x/y, Size w/h  (normalized 0-1)
//   - Data binding id         (QComboBox, populated from MeterBinding)
//   - Z-order
//   - onlyWhenRx / onlyWhenTx / displayGroup (block 1 filter)
//
// Subclasses add their own type-specific fields in buildTypeSpecific()
// which is called from the constructor after the base form is laid
// out. Use makeDoubleRow/makeIntRow/makeCheckRow/addRow() helpers to
// keep form layout consistent across editors.
//
// On every interactive change the editor emits propertyChanged().
// The dialog wires that to an "apply-and-redraw" slot so edits
// propagate live to the target container's MeterWidget.
class BaseItemEditor : public QWidget {
    Q_OBJECT

public:
    explicit BaseItemEditor(QWidget* parent = nullptr);
    ~BaseItemEditor() override = default;

    // Bind this editor to an item. Populates all controls from the
    // item's current state. Passing nullptr clears the binding and
    // disables the editor.
    virtual void setItem(MeterItem* item);
    MeterItem* item() const { return m_item; }

signals:
    // Emitted whenever any base or subclass control changes the
    // bound item's state. The dialog listens on this to repaint
    // the target MeterWidget live.
    void propertyChanged();

protected:
    // Subclasses override to append their own rows to m_form after
    // the base fields. Called at the end of the constructor.
    virtual void buildTypeSpecific() {}

    // Helper factories that return the created widget pre-wired to
    // the base class's change handler. Subclasses add them via
    // addRow() so the uniform form style is preserved.
    QDoubleSpinBox* makeDoubleRow(const QString& label,
                                  double min, double max,
                                  double step = 0.01,
                                  int decimals = 3);
    QSpinBox*       makeIntRow(const QString& label, int min, int max);
    QCheckBox*      makeCheckRow(const QString& label);

    // Append a pre-made widget under a new label.
    void addRow(const QString& label, QWidget* widget);

    // Append a section header row (bold, slightly-highlighted).
    void addHeader(const QString& text);

    // Must be called when any control has already been populated
    // programmatically so the propertyChanged signal is suppressed
    // during loadFromItem().
    void beginProgrammaticUpdate() { ++m_programmaticDepth; }
    void endProgrammaticUpdate()   { --m_programmaticDepth; }
    bool isProgrammaticUpdate() const { return m_programmaticDepth > 0; }

    // Emit propertyChanged() unless we're in a programmatic update.
    void notifyChanged();

    MeterItem*   m_item{nullptr};
    QFormLayout* m_form{nullptr};

private:
    QVBoxLayout* m_root{nullptr};

    // Base-field widgets
    QDoubleSpinBox* m_spinX{nullptr};
    QDoubleSpinBox* m_spinY{nullptr};
    QDoubleSpinBox* m_spinW{nullptr};
    QDoubleSpinBox* m_spinH{nullptr};
    QSpinBox*       m_spinZ{nullptr};
    QComboBox*      m_comboBinding{nullptr};
    QCheckBox*      m_chkOnlyRx{nullptr};
    QCheckBox*      m_chkOnlyTx{nullptr};
    QSpinBox*       m_spinDisplayGroup{nullptr};

    int m_programmaticDepth{0};

    void buildBaseForm();
    void loadBaseFields();
    void populateBindingCombo();
};

} // namespace NereusSDR
