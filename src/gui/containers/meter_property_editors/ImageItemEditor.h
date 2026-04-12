#pragma once
#include "BaseItemEditor.h"

class QLineEdit;
class QPushButton;

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

    QLineEdit*   m_editPath{nullptr};
    QPushButton* m_btnBrowse{nullptr};
};

} // namespace NereusSDR
