// no-port-check: NereusSDR-original Qt property adapter.
// Modification history (NereusSDR):
//   2026-10-26 - Fix wave R2: Units offers Follow global and each unit; a
//                 unit chosen here stays when the global setting changes.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
#pragma once
#include "BaseItemEditor.h"

class QCheckBox;
class QPushButton;
class QDoubleSpinBox;
class QComboBox;
class QFontComboBox;
class QSpinBox;

namespace NereusSDR {

class SignalTextItem;

class SignalTextItemEditor : public BaseItemEditor {
    Q_OBJECT
public:
    explicit SignalTextItemEditor(QWidget* parent = nullptr);
    void setItem(MeterItem* item) override;
    // Container drafts: a note that decimal precision and Follow global
    // come from the global multimeter preferences.
    void useGlobalUnits();

private:
    void buildTypeSpecific() override;

    QComboBox*      m_comboUnits{nullptr};
    QCheckBox*      m_chkShowValue{nullptr};
    QCheckBox*      m_chkShowPeak{nullptr};
    QCheckBox*      m_chkShowType{nullptr};
    QCheckBox*      m_chkPeakHold{nullptr};
    QPushButton*    m_btnColour{nullptr};
    QPushButton*    m_btnPeakColour{nullptr};
    QPushButton*    m_btnMarkerColour{nullptr};
    QFontComboBox*  m_fontCombo{nullptr};
    QDoubleSpinBox* m_spinFontSize{nullptr};
    QComboBox*      m_comboBarStyle{nullptr};
    QComboBox*      m_comboTypeStyle{nullptr};
    QPushButton*    m_btnTypeColour{nullptr};
    QCheckBox*      m_chkShowSubMarker{nullptr};
    QPushButton*    m_btnSubMarkerColour{nullptr};
    QPushButton*    m_btnBackgroundColour{nullptr};
    QCheckBox*      m_chkPeakNeedsHold{nullptr};
    QSpinBox*       m_spinHistoryMs{nullptr};
    QSpinBox*       m_spinIgnoreHistoryMs{nullptr};
};

} // namespace NereusSDR
