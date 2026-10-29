#pragma once
#include "ButtonBoxItemEditor.h"

#include <QVector>

class QCheckBox;

namespace NereusSDR {
class FilterButtonItem;

class FilterButtonItemEditor : public ButtonBoxItemEditor {
    Q_OBJECT
public:
    explicit FilterButtonItemEditor(QWidget* parent = nullptr);
    void setItem(MeterItem* item) override;

private:
    void buildFilterSpecific();
    // One row per button, in button order ("filterVisible_<index>").
    QVector<QCheckBox*> m_visible;
};

} // namespace NereusSDR
