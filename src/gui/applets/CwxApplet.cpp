// =================================================================
// src/gui/applets/CwxApplet.cpp  (NereusSDR)
// =================================================================
//
// Source attribution (AetherSDR — GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 §5 requirements.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-18 — Ported/adapted in C++20/Qt6 for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
//                 Port of AetherSDR `src/gui/CwxPanel.{h,cpp}` (CW text
//                 entry + WPM + message-slot buttons). Renamed to
//                 CwxApplet in NereusSDR. All controls NYI.
// =================================================================

#include "CwxApplet.h"
#include "NyiOverlay.h"
#include "gui/StyleConstants.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QTextEdit>
#include <QSlider>
#include <QLabel>

namespace NereusSDR {

CwxApplet::CwxApplet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    buildUI();
}

void CwxApplet::buildUI()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(appletTitleBar(QStringLiteral("CW Keyer")));

    auto* body = new QWidget(this);
    auto* vbox = new QVBoxLayout(body);
    vbox->setContentsMargins(4, 2, 4, 4);
    vbox->setSpacing(2);

    // --- Control 1: Text input (60px, dark styled) ---
    m_textEdit = new QTextEdit(this);
    m_textEdit->setFixedHeight(60);
    m_textEdit->setStyleSheet(QStringLiteral(
        "QTextEdit {"
        "  background: %1; border: 1px solid %2;"
        "  border-radius: 3px; color: %3; font-size: 11px;"
        "}").arg(Style::kInsetBg, Style::kInsetBorder, Style::kTextPrimary));
    vbox->addWidget(m_textEdit);
    NyiOverlay::markNyi(m_textEdit, QStringLiteral("3I-2"));

    // --- Control 2: Send button ---
    m_sendBtn = styledButton(QStringLiteral("Send"));
    vbox->addWidget(m_sendBtn);
    NyiOverlay::markNyi(m_sendBtn, QStringLiteral("3I-2"));

    // --- Control 3: Speed override slider (5..60 WPM) ---
    m_speedSlider = new QSlider(Qt::Horizontal, this);
    m_speedSlider->setRange(5, 60);
    m_speedSlider->setValue(20);
    m_speedValue  = insetValue(QStringLiteral("20"));
    vbox->addLayout(sliderRow(QStringLiteral("Speed"), m_speedSlider, m_speedValue));
    NyiOverlay::markNyi(m_speedSlider, QStringLiteral("3I-2"));

    // --- Control 4: Memory buttons M1-M8 (2 rows of 4) ---
    const QString memLabels[8] = {
        QStringLiteral("M1"), QStringLiteral("M2"),
        QStringLiteral("M3"), QStringLiteral("M4"),
        QStringLiteral("M5"), QStringLiteral("M6"),
        QStringLiteral("M7"), QStringLiteral("M8")
    };

    for (int row = 0; row < 2; ++row) {
        auto* hrow = new QHBoxLayout;
        hrow->setSpacing(2);
        for (int col = 0; col < 4; ++col) {
            int idx = row * 4 + col;
            m_memBtn[idx] = styledButton(memLabels[idx]);
            hrow->addWidget(m_memBtn[idx]);
            NyiOverlay::markNyi(m_memBtn[idx], QStringLiteral("3I-2"));
        }
        vbox->addLayout(hrow);
    }

    // --- Control 5: Repeat toggle + interval slider (1..60s) ---
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_repeatBtn = greenToggle(QStringLiteral("Rpt"));
        m_repeatBtn->setCheckable(true);
        row->addWidget(m_repeatBtn);

        m_repeatSlider = new QSlider(Qt::Horizontal, this);
        m_repeatSlider->setRange(1, 60);
        m_repeatSlider->setValue(5);
        m_repeatSlider->setFixedHeight(18);
        row->addWidget(m_repeatSlider, 1);

        m_repeatValue = insetValue(QStringLiteral("5"));
        row->addWidget(m_repeatValue);

        vbox->addLayout(row);

        NyiOverlay::markNyi(m_repeatBtn,    QStringLiteral("3I-2"));
        NyiOverlay::markNyi(m_repeatSlider, QStringLiteral("3I-2"));
    }

    // --- Control 6: Keyboard-to-CW toggle ---
    m_kbCwBtn = greenToggle(QStringLiteral("KB\u2192CW"));
    m_kbCwBtn->setCheckable(true);
    vbox->addWidget(m_kbCwBtn);
    NyiOverlay::markNyi(m_kbCwBtn, QStringLiteral("3I-2"));

    vbox->addStretch();
    root->addWidget(body);
}

void CwxApplet::syncFromModel()
{
    // NYI — Phase 3I-2
}

} // namespace NereusSDR
