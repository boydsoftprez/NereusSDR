#pragma once
#include "ButtonBoxItemEditor.h"

#include <QVector>

class QCheckBox;

namespace NereusSDR {
class AntennaButtonItem;

class AntennaButtonItemEditor : public ButtonBoxItemEditor {
    Q_OBJECT
public:
    explicit AntennaButtonItemEditor(QWidget* parent = nullptr);
    void setItem(MeterItem* item) override;

private:
    void buildAntennaSpecific();
    // One row per button, in button order ("antennaVisible_<index>").
    QVector<QCheckBox*> m_visible;
};

} // namespace NereusSDR
