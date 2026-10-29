#pragma once
// NereusSDR-original declarations. Independently implemented from the
// Thetis setup.cs interface; the ported ranges and row enabling live in
// RotatorItemEditor.cpp, which carries the headers.
// Modification history (NereusSDR):
//   2026-10-26 - Thetis rotator settings: full background row, beam width
//                 rows disabled when the beam is off. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
#include "BaseItemEditor.h"

class QComboBox;
class QDoubleSpinBox;
class QCheckBox;
class QLineEdit;
class QPushButton;

namespace NereusSDR {
class RotatorItem;

class RotatorItemEditor : public BaseItemEditor {
    Q_OBJECT
public:
    explicit RotatorItemEditor(QWidget* parent = nullptr);
    void setItem(MeterItem* item) override;

private:
    void buildTypeSpecific() override;
    // The beam width, alpha and colour rows follow "Show beam width".
    void updateBeamWidthRows();
    // The control rows follow "Allow control"; the data source rows also
    // need the data source command path.
    void updateControlRows();

    QComboBox*      m_comboMode{nullptr};
    QCheckBox*      m_chkShowValue{nullptr};
    QCheckBox*      m_chkShowCardinals{nullptr};
    QCheckBox*      m_chkShowBeamWidth{nullptr};
    QDoubleSpinBox* m_spinBeamWidth{nullptr};
    QDoubleSpinBox* m_spinBeamAlpha{nullptr};
    QCheckBox*      m_chkDarkMode{nullptr};
    QDoubleSpinBox* m_spinPadding{nullptr};
    QCheckBox*      m_chkFullBackground{nullptr};

    // Background image path
    QLineEdit*      m_editBgPath{nullptr};

    // Colors
    QPushButton*    m_btnBigBlob{nullptr};
    QPushButton*    m_btnSmallBlob{nullptr};
    QPushButton*    m_btnOuterText{nullptr};
    QPushButton*    m_btnArrow{nullptr};
    QPushButton*    m_btnBeamWidth{nullptr};
    QPushButton*    m_btnBackground{nullptr};

    // Control
    QCheckBox*      m_chkAllowControl{nullptr};
    QPushButton*    m_btnControlColour{nullptr};
    QComboBox*      m_comboCommandPath{nullptr};
    QLineEdit*      m_editFourChar{nullptr};
    QLineEdit*      m_editAzCommand{nullptr};
    QLineEdit*      m_editEleCommand{nullptr};
    QLineEdit*      m_editStopCommand{nullptr};
    QPushButton*    m_btnPstRotator{nullptr};
};

} // namespace NereusSDR
