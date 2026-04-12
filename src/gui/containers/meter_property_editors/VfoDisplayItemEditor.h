#pragma once
#include "BaseItemEditor.h"

class QComboBox;
class QCheckBox;
class QPushButton;

namespace NereusSDR {
class VfoDisplayItem;

class VfoDisplayItemEditor : public BaseItemEditor {
    Q_OBJECT
public:
    explicit VfoDisplayItemEditor(QWidget* parent = nullptr);
    void setItem(MeterItem* item) override;

private:
    void buildTypeSpecific() override;

    QComboBox*   m_comboDisplayMode{nullptr};

    // Colors
    QPushButton* m_btnFreqColour{nullptr};
    QPushButton* m_btnModeColour{nullptr};
    QPushButton* m_btnFilterColour{nullptr};
    QPushButton* m_btnBandColour{nullptr};
    QPushButton* m_btnRxColour{nullptr};
    QPushButton* m_btnTxColour{nullptr};
};

} // namespace NereusSDR
