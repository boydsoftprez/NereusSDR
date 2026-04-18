// =================================================================
// src/gui/applets/DvkApplet.cpp  (NereusSDR)
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
//                 Port of AetherSDR `src/gui/DvkPanel.{h,cpp}` (DVK F-key
//                 slot grid + record/play controls). Renamed to
//                 DvkApplet in NereusSDR. All controls NYI.
// =================================================================

#include "DvkApplet.h"
#include "NyiOverlay.h"
#include "gui/HGauge.h"
#include "gui/StyleConstants.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QSlider>

namespace NereusSDR {

DvkApplet::DvkApplet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    buildUI();
}

void DvkApplet::buildUI()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(appletTitleBar(QStringLiteral("Voice Keyer")));

    auto* body = new QWidget(this);
    auto* vbox = new QVBoxLayout(body);
    vbox->setContentsMargins(4, 2, 4, 4);
    vbox->setSpacing(2);

    // --- Control 1: Voice keyer slots (4 rows) ---
    const QString slotNames[4] = {
        QStringLiteral("Slot 1"), QStringLiteral("Slot 2"),
        QStringLiteral("Slot 3"), QStringLiteral("Slot 4")
    };

    for (int i = 0; i < 4; ++i) {
        auto* row = new QHBoxLayout;
        row->setSpacing(3);

        m_slotLabel[i] = new QLabel(slotNames[i], this);
        m_slotLabel[i]->setFixedWidth(44);
        m_slotLabel[i]->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 10px; }").arg(Style::kTextSecondary));
        row->addWidget(m_slotLabel[i]);

        m_recBtn[i]  = styledButton(QStringLiteral("\u25CF Rec"));
        m_playBtn[i] = styledButton(QStringLiteral("\u25BA Play"));
        m_stopBtn[i] = styledButton(QStringLiteral("\u25A0 Stop"));

        row->addWidget(m_recBtn[i]);
        row->addWidget(m_playBtn[i]);
        row->addWidget(m_stopBtn[i]);

        vbox->addLayout(row);

        NyiOverlay::markNyi(m_recBtn[i],  QStringLiteral("3I-1"));
        NyiOverlay::markNyi(m_playBtn[i], QStringLiteral("3I-1"));
        NyiOverlay::markNyi(m_stopBtn[i], QStringLiteral("3I-1"));
    }

    // --- Control 2: Record level gauge (0-100) ---
    m_recLevel = new HGauge(this);
    m_recLevel->setRange(0.0, 100.0);
    m_recLevel->setYellowStart(70.0);
    m_recLevel->setRedStart(90.0);
    m_recLevel->setTitle(QStringLiteral("Rec Level"));
    vbox->addWidget(m_recLevel);
    NyiOverlay::markNyi(m_recLevel, QStringLiteral("3I-1"));

    // --- Control 3: Repeat toggle + interval slider (1..60s) ---
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

        NyiOverlay::markNyi(m_repeatBtn,    QStringLiteral("3I-1"));
        NyiOverlay::markNyi(m_repeatSlider, QStringLiteral("3I-1"));
    }

    // --- Control 4: Semi break-in toggle ---
    m_semiBkBtn = greenToggle(QStringLiteral("Semi BK"));
    m_semiBkBtn->setCheckable(true);
    vbox->addWidget(m_semiBkBtn);
    NyiOverlay::markNyi(m_semiBkBtn, QStringLiteral("3I-1"));

    // --- Control 5: WAV file import button ---
    m_importBtn = styledButton(QStringLiteral("Import WAV\u2026"));
    vbox->addWidget(m_importBtn);
    NyiOverlay::markNyi(m_importBtn, QStringLiteral("3I-1"));

    vbox->addStretch();
    root->addWidget(body);
}

void DvkApplet::syncFromModel()
{
    // NYI — Phase 3I-1
}

} // namespace NereusSDR
