#pragma once
#include "BaseItemEditor.h"

#include <QColor>
#include <QVector>
#include <functional>

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

    struct ColourRow {
        QPushButton* button{nullptr};
        QString field;
        std::function<QColor(const VfoDisplayItem&)> get;
    };

    QComboBox*   m_comboDisplayMode{nullptr};
    QComboBox*   m_comboLayout{nullptr};
    QCheckBox*   m_chkBandText{nullptr};

    // Colors
    QPushButton* m_btnFreqColour{nullptr};
    QPushButton* m_btnFreqSmallColour{nullptr};
    QPushButton* m_btnCopyToSmall{nullptr};
    QPushButton* m_btnModeColour{nullptr};
    QPushButton* m_btnFilterColour{nullptr};
    QPushButton* m_btnBandColour{nullptr};
    QPushButton* m_btnRxColour{nullptr};
    QPushButton* m_btnTxColour{nullptr};
    QPushButton* m_btnBandTextColour{nullptr};
    QVector<ColourRow> m_colourRows;
};

} // namespace NereusSDR
