#pragma once
#include "ButtonBoxItemEditor.h"

#include <QVector>

class QCheckBox;

namespace NereusSDR {
class ModeButtonItem;

class ModeButtonItemEditor : public ButtonBoxItemEditor {
    Q_OBJECT
public:
    explicit ModeButtonItemEditor(QWidget* parent = nullptr);
    void setItem(MeterItem* item) override;

private:
    void buildModeSpecific();
    // One row per mode button, in DSPMode order ("modeVisible_<index>").
    QVector<QCheckBox*> m_visible;
};

} // namespace NereusSDR
