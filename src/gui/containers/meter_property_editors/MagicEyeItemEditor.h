#pragma once
#include "BaseItemEditor.h"

class QPushButton;
class QLineEdit;

namespace NereusSDR {

class MagicEyeItem;

class MagicEyeItemEditor : public BaseItemEditor {
    Q_OBJECT
public:
    explicit MagicEyeItemEditor(QWidget* parent = nullptr);
    void setItem(MeterItem* item) override;

private:
    void buildTypeSpecific() override;

    QPushButton* m_btnGlowColor{nullptr};
    QLineEdit*   m_editBezelPath{nullptr};
    QPushButton* m_btnBrowseBezel{nullptr};
};

} // namespace NereusSDR
