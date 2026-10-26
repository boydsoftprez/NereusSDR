// Modification history (NereusSDR):
//   2026-10-26 - "Follow the Multimeter setting": changing Capacity gives
//                 the graph its own length, which the Multimeter history
//                 duration then leaves alone; turning the choice back on
//                 returns the graph to that setting. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.

#pragma once
#include "BaseItemEditor.h"

class QCheckBox;
class QPushButton;
class QSpinBox;
class QComboBox;

namespace NereusSDR {

class HistoryGraphItem;

class HistoryGraphItemEditor : public BaseItemEditor {
    Q_OBJECT
public:
    explicit HistoryGraphItemEditor(QWidget* parent = nullptr);
    void setItem(MeterItem* item) override;

private:
    void buildTypeSpecific() override;

    QSpinBox*    m_spinCapacity{nullptr};
    QCheckBox*   m_chkFollowMultimeter{nullptr};
    QPushButton* m_btnLineColor0{nullptr};
    QPushButton* m_btnLineColor1{nullptr};
    QCheckBox*   m_chkShowGrid{nullptr};
    QCheckBox*   m_chkAutoScale0{nullptr};
    QCheckBox*   m_chkAutoScale1{nullptr};
    QCheckBox*   m_chkShowScale0{nullptr};
    QCheckBox*   m_chkShowScale1{nullptr};
    QComboBox*   m_comboBinding1{nullptr};
};

} // namespace NereusSDR
