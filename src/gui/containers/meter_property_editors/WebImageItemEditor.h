#pragma once
// Modification history (NereusSDR):
//   2026-10-26 - Web image settings: presets, width scale, bypass cache,
//                 the known-host lock, the fetch state and the 4-char ID
//                 with a Copy button. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
#include "BaseItemEditor.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QSpinBox;
class QPushButton;

namespace NereusSDR {
class WebImageItem;

class WebImageItemEditor : public BaseItemEditor {
    Q_OBJECT
public:
    explicit WebImageItemEditor(QWidget* parent = nullptr);
    void setItem(MeterItem* item) override;

private:
    void buildTypeSpecific() override;
    // Sets the item's URL (BSDWorld fix first) and refreshes the
    // controls the known-host lock changes.
    void applyUrl(const QString& text);
    void refreshLockedControls();
    void refreshState();

    QLineEdit*      m_editUrl{nullptr};
    QList<QComboBox*> m_presetCombos;
    QCheckBox*      m_chkBsdDark{nullptr};
    QSpinBox*       m_spinRefresh{nullptr};
    QDoubleSpinBox* m_spinWidthScale{nullptr};
    QCheckBox*      m_chkBypassCache{nullptr};
    QLabel*         m_lblState{nullptr};
    QLineEdit*      m_editFourChar{nullptr};
    QPushButton*    m_btnFallback{nullptr};
    QMetaObject::Connection m_stateConnection;
};

} // namespace NereusSDR
