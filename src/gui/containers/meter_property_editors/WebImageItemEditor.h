#pragma once
#include "BaseItemEditor.h"

class QLineEdit;
class QSpinBox;
class QPushButton;

namespace NereusSDR {
class WebImageItem;

class WebImageItemEditor : public BaseItemEditor {
    Q_OBJECT
public:
    explicit WebImageItemEditor(QWidget* parent = nullptr);
    void setItem(MeterItem* item) override;

private:
    void buildTypeSpecific() override;

    QLineEdit*   m_editUrl{nullptr};
    QSpinBox*    m_spinRefresh{nullptr};
    QPushButton* m_btnFallback{nullptr};
};

} // namespace NereusSDR
