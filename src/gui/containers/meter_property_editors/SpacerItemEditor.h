#pragma once

// Independently implemented from Thetis interface: this header declares
// the NereusSDR editor's rows; the Thetis Setup ranges and their licence
// are in SpacerItemEditor.cpp.
//
// Modification history (NereusSDR):
//   2026-10-26 - PR 677 review fix wave (I3): the marker above. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
#include "BaseItemEditor.h"

class QCheckBox;
class QDoubleSpinBox;
class QPushButton;

namespace NereusSDR {

class MeterItem;
class SpacerItem;

class SpacerItemEditor : public BaseItemEditor {
    Q_OBJECT
public:
    explicit SpacerItemEditor(QWidget* parent = nullptr);
    void setItem(MeterItem* item) override;

private:
    void buildTypeSpecific() override;

    QPushButton*    m_btnColour1{nullptr};
    QPushButton*    m_btnColour2{nullptr};
    QDoubleSpinBox* m_spinPadding{nullptr};
    QCheckBox*      m_chkRxTxColours{nullptr};
    QCheckBox*      m_chkPaddingSetsHeight{nullptr};
};

} // namespace NereusSDR
