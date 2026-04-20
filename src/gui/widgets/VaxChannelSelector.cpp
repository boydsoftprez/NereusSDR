// =================================================================
// src/gui/widgets/VaxChannelSelector.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original file — no Thetis port; no attribution-registry row.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-19 — Written by J.J. Boyd (KG4VCF), with AI-assisted
//                transformation via Anthropic Claude Code.
// =================================================================

#include "VaxChannelSelector.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QPushButton>
#include <algorithm>

namespace NereusSDR {

// Active-blue style: matches kModeBtn in VfoStyles.h — #0070c0 / #0090e0 / #ffffff
static const char* kVaxBtnStyle =
    "QPushButton {"
    "  background: #1a2a3a; border: 1px solid #304050; border-radius: 2px;"
    "  color: #c8d8e8; font-size: 11px; font-weight: bold; padding: 2px 5px;"
    "}"
    "QPushButton:checked {"
    "  background: #0070c0; color: #ffffff; border: 1px solid #0090e0;"
    "}"
    "QPushButton:hover {"
    "  border: 1px solid #0090e0;"
    "}";

VaxChannelSelector::VaxChannelSelector(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    m_group = new QButtonGroup(this);
    m_group->setExclusive(true);

    static const char* kLabels[] = { "Off", "1", "2", "3", "4" };
    static const char* kTooltips[] = {
        "Disable VAX (Virtual Audio Cable) routing for this slice",
        "Route this slice's audio to VAX channel 1",
        "Route this slice's audio to VAX channel 2",
        "Route this slice's audio to VAX channel 3",
        "Route this slice's audio to VAX channel 4",
    };

    for (int i = 0; i < 5; ++i) {
        auto* btn = new QPushButton(QString::fromLatin1(kLabels[i]), this);
        btn->setCheckable(true);
        btn->setStyleSheet(QLatin1String(kVaxBtnStyle));
        btn->setFixedHeight(20);
        btn->setToolTip(QString::fromLatin1(kTooltips[i]));
        m_buttons[i] = btn;
        m_group->addButton(btn, i);
        layout->addWidget(btn);
    }

    // Default to Off (index 0)
    m_buttons[0]->setChecked(true);
    m_value = 0;

    connect(m_group, &QButtonGroup::buttonToggled,
            this, [this](QAbstractButton* btn, bool checked) {
        if (!checked || m_programmaticUpdate) {
            return;
        }
        // Find the index of the clicked button
        for (int i = 0; i < 5; ++i) {
            if (m_buttons[i] == btn) {
                m_value = i;
                emit valueChanged(m_value);
                break;
            }
        }
    });

    setLayout(layout);
}

void VaxChannelSelector::setValue(int ch)
{
    ch = std::clamp(ch, 0, 4);
    m_programmaticUpdate = true;
    m_buttons[ch]->setChecked(true);
    m_value = ch;
    m_programmaticUpdate = false;
    // No signal emitted — programmatic update
}

void VaxChannelSelector::simulateClick(int ch)
{
    ch = std::clamp(ch, 0, 4);
    m_buttons[ch]->click();
}

} // namespace NereusSDR
