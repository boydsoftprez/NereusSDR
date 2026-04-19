#pragma once

// =================================================================
// src/gui/widgets/VaxChannelSelector.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original file — no Thetis port; no attribution-registry row.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-19 — Written by J.J. Boyd (KG4VCF), with AI-assisted
//                transformation via Anthropic Claude Code.
// =================================================================

#include <QWidget>

class QPushButton;
class QButtonGroup;

namespace NereusSDR {

class VaxChannelSelector : public QWidget {
    Q_OBJECT
public:
    explicit VaxChannelSelector(QWidget* parent = nullptr);

    int  value() const { return m_value; }
    void setValue(int ch);  // 0..4; no signal

    void simulateClick(int ch);  // test-hook

signals:
    void valueChanged(int ch);

private:
    QButtonGroup* m_group{nullptr};
    QPushButton*  m_buttons[5]{};
    int m_value{0};
    bool m_programmaticUpdate{false};
};

} // namespace NereusSDR
