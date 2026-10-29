#pragma once
#include "BaseItemEditor.h"

class QComboBox;
class QLineEdit;
class QSpinBox;

namespace NereusSDR {
class DataOutItem;

class DataOutItemEditor : public BaseItemEditor {
    Q_OBJECT
public:
    explicit DataOutItemEditor(QWidget* parent = nullptr);
    void setItem(MeterItem* item) override;

private:
    void buildTypeSpecific() override;

    QComboBox* m_comboFormat{nullptr};
    QComboBox* m_comboTransport{nullptr};
    QLineEdit* m_editGuid{nullptr};
    QLineEdit* m_editVariable{nullptr};
    QLineEdit* m_editFourChar{nullptr};
    QSpinBox*  m_spinSendInterval{nullptr};
};

} // namespace NereusSDR
