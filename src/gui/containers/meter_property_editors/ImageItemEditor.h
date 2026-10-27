#pragma once
#include "BaseItemEditor.h"

class QLineEdit;
class QPushButton;
class QCheckBox;

namespace NereusSDR {

class MeterItem;
class ImageItem;

class ImageItemEditor : public BaseItemEditor {
    Q_OBJECT
public:
    explicit ImageItemEditor(QWidget* parent = nullptr);
    void setItem(MeterItem* item) override;

private:
    void buildTypeSpecific() override;
    void refreshDarkModeAvailability();

    QLineEdit*   m_editPath{nullptr};
    QPushButton* m_btnBrowse{nullptr};
    QCheckBox*   m_chkDarkMode{nullptr};
};

} // namespace NereusSDR
