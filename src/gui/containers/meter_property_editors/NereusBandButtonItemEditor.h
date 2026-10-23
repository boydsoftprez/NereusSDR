#pragma once

// no-port-check: NereusSDR-original property editor. Adds the label colour and
// the two gradient colours for the resting face and the selected face.
// BandButtonItemEditor carries no Thetis header, and this file copies no
// Thetis source.
// 2026-10-22 - Brian Sutton, AI-assisted via xAI Grok.

#include "BandButtonItemEditor.h"

class QPushButton;

namespace NereusSDR {

class NereusBandButtonItemEditor : public BandButtonItemEditor {
    Q_OBJECT

public:
    explicit NereusBandButtonItemEditor(QWidget* parent = nullptr);
    void setItem(MeterItem* item) override;

private:
    QPushButton* m_textColorButton{nullptr};
    QPushButton* m_backgroundTopButton{nullptr};
    QPushButton* m_backgroundBottomButton{nullptr};
    QPushButton* m_selectedTopButton{nullptr};
    QPushButton* m_selectedBottomButton{nullptr};
};

} // namespace NereusSDR
