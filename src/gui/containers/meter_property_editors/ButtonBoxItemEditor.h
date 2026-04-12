#pragma once
#include "BaseItemEditor.h"

class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QComboBox;
class QPushButton;

namespace NereusSDR {
class ButtonBoxItem;

// Shared base editor for all ButtonBoxItem subclass editors.
// Exposes every common button-box field defined on ButtonBoxItem.
// Subclass editors inherit from this and call ButtonBoxItemEditor::setItem()
// first, then populate their own extra fields.
class ButtonBoxItemEditor : public BaseItemEditor {
    Q_OBJECT
public:
    explicit ButtonBoxItemEditor(QWidget* parent = nullptr);
    void setItem(MeterItem* item) override;

protected:
    // Called by the constructor to build the shared button-box section.
    // Subclasses that add extra fields call buildTypeSpecific() themselves
    // from their own constructor AFTER the base chain completes.
    void buildButtonBoxSection();

private:
    void buildTypeSpecific() override;

    // Grid configuration
    QSpinBox*       m_spinColumns{nullptr};
    QDoubleSpinBox* m_spinBorderWidth{nullptr};
    QDoubleSpinBox* m_spinMargin{nullptr};
    QDoubleSpinBox* m_spinCornerRadius{nullptr};
    QDoubleSpinBox* m_spinHeightRatio{nullptr};

    // FadeOnRx / FadeOnTx
    QCheckBox*      m_chkFadeOnRx{nullptr};
    QCheckBox*      m_chkFadeOnTx{nullptr};
};

} // namespace NereusSDR
