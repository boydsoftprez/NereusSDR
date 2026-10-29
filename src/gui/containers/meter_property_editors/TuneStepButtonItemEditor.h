#pragma once
#include "ButtonBoxItemEditor.h"

#include <QVector>

class QCheckBox;

namespace NereusSDR {
class TuneStepButtonItem;

class TuneStepButtonItemEditor : public ButtonBoxItemEditor {
    Q_OBJECT
public:
    explicit TuneStepButtonItemEditor(QWidget* parent = nullptr);
    void setItem(MeterItem* item) override;

private:
    void buildTuneStepSpecific();
    // One row per step button, in kTuneStepList order ("stepVisible_<index>").
    QVector<QCheckBox*> m_visible;
};

} // namespace NereusSDR
