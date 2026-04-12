#pragma once
#include "BaseItemEditor.h"

class QPushButton;

namespace NereusSDR {
class DialItem;

class DialItemEditor : public BaseItemEditor {
    Q_OBJECT
public:
    explicit DialItemEditor(QWidget* parent = nullptr);
    void setItem(MeterItem* item) override;

private:
    void buildTypeSpecific() override;

    // Text/shape colors
    QPushButton* m_btnTextColour{nullptr};
    QPushButton* m_btnCircleColour{nullptr};
    QPushButton* m_btnPadColour{nullptr};
    QPushButton* m_btnRingColour{nullptr};

    // Button state colors
    QPushButton* m_btnBtnOn{nullptr};
    QPushButton* m_btnBtnOff{nullptr};
    QPushButton* m_btnBtnHighlight{nullptr};

    // Speed indicator colors
    QPushButton* m_btnSlowColour{nullptr};
    QPushButton* m_btnHoldColour{nullptr};
    QPushButton* m_btnFastColour{nullptr};
};

} // namespace NereusSDR
