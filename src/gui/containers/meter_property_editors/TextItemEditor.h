#pragma once
#include "BaseItemEditor.h"

class QLineEdit;
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QPushButton;

namespace NereusSDR {

class TextItem;

class TextItemEditor : public BaseItemEditor {
    Q_OBJECT
public:
    explicit TextItemEditor(QWidget* parent = nullptr);
    void setItem(MeterItem* item) override;

private:
    void buildTypeSpecific() override;

    QLineEdit*      m_editLabel{nullptr};
    QPushButton*    m_btnTextColor{nullptr};
    QSpinBox*       m_spinFontSize{nullptr};
    QCheckBox*      m_chkBold{nullptr};
    QLineEdit*      m_editSuffix{nullptr};
    QSpinBox*       m_spinDecimals{nullptr};
    QLineEdit*      m_editIdleText{nullptr};
    QDoubleSpinBox* m_spinMinValid{nullptr};
};

} // namespace NereusSDR
