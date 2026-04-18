// =================================================================
// src/gui/applets/CatApplet.cpp  (NereusSDR)
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
//                 Layout mirrors AetherSDR `src/gui/CatApplet.{h,cpp}`
//                 (serial CAT / rigctl / TCI enable rows + PTT LEDs).
//                 All controls NYI — wired in later phase.
// =================================================================

#include "CatApplet.h"
#include "NyiOverlay.h"
#include "gui/ComboStyle.h"
#include "gui/StyleConstants.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>

namespace NereusSDR {

static QLabel* makeLed(const QString& name, QWidget* parent)
{
    auto* led = new QLabel(name, parent);
    led->setFixedSize(24, 14);
    led->setAlignment(Qt::AlignCenter);
    led->setStyleSheet(QStringLiteral(
        "QLabel { background: #405060; color: #c8d8e8; border-radius: 2px;"
        " font-size: 8px; font-weight: bold; }"));
    return led;
}

static QLineEdit* makePortEdit(const QString& def, int w, QWidget* parent)
{
    auto* ed = new QLineEdit(def, parent);
    ed->setFixedWidth(w);
    ed->setStyleSheet(QStringLiteral(
        "QLineEdit {"
        "  background: %1; border: 1px solid %2;"
        "  border-radius: 3px; color: %3; font-size: 10px;"
        "}").arg(Style::kInsetBg, Style::kInsetBorder, Style::kTextPrimary));
    return ed;
}

static QLabel* makePathLabel(const QString& text, QWidget* parent)
{
    auto* lbl = new QLabel(text, parent);
    lbl->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 9px; }").arg(Style::kTextSecondary));
    return lbl;
}

CatApplet::CatApplet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    buildUI();
}

void CatApplet::buildUI()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(appletTitleBar(QStringLiteral("CAT / TCI")));

    auto* body = new QWidget(this);
    auto* vbox = new QVBoxLayout(body);
    vbox->setContentsMargins(4, 2, 4, 4);
    vbox->setSpacing(2);

    // --- Control 1: CAT TCP enable + 4 status LEDs (A/B/C/D) ---
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_tcpBtn = greenToggle(QStringLiteral("TCP"));
        m_tcpBtn->setCheckable(true);
        row->addWidget(m_tcpBtn);

        const QString ledNames[4] = {
            QStringLiteral("A"), QStringLiteral("B"),
            QStringLiteral("C"), QStringLiteral("D")
        };
        for (int i = 0; i < 4; ++i) {
            m_tcpLed[i] = makeLed(ledNames[i], this);
            row->addWidget(m_tcpLed[i]);
        }
        row->addStretch();

        vbox->addLayout(row);
        NyiOverlay::markNyi(m_tcpBtn, QStringLiteral("3K"));
    }

    // --- Control 2: CAT PTY enable + 4 path labels ---
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_ptyBtn = greenToggle(QStringLiteral("PTY"));
        m_ptyBtn->setCheckable(true);
        row->addWidget(m_ptyBtn);

        for (int i = 0; i < 4; ++i) {
            m_ptyPath[i] = makePathLabel(
                QStringLiteral("/dev/ptyp%1").arg(i), this);
            row->addWidget(m_ptyPath[i]);
        }
        row->addStretch();

        vbox->addLayout(row);
        NyiOverlay::markNyi(m_ptyBtn, QStringLiteral("3K"));
    }

    vbox->addWidget(divider());

    // --- Control 3: TCI enable + port + status LED ---
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_tciBtn = greenToggle(QStringLiteral("TCI"));
        m_tciBtn->setCheckable(true);
        row->addWidget(m_tciBtn);

        m_tciPort = makePortEdit(QStringLiteral("40001"), 55, this);
        row->addWidget(m_tciPort);

        m_tciLed = makeLed(QStringLiteral("\u2022"), this);
        row->addWidget(m_tciLed);
        row->addStretch();

        vbox->addLayout(row);

        NyiOverlay::markNyi(m_tciBtn,  QStringLiteral("3J"));
        NyiOverlay::markNyi(m_tciPort, QStringLiteral("3J"));
    }

    vbox->addWidget(divider());

    // --- Control 4: DAX enable + 4 channel status labels ---
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_daxBtn = greenToggle(QStringLiteral("DAX"));
        m_daxBtn->setCheckable(true);
        row->addWidget(m_daxBtn);

        for (int i = 0; i < 4; ++i) {
            m_daxStatus[i] = makeLed(QStringLiteral("Ch%1").arg(i + 1), this);
            row->addWidget(m_daxStatus[i]);
        }
        row->addStretch();

        vbox->addLayout(row);
        NyiOverlay::markNyi(m_daxBtn, QStringLiteral("3-DAX"));
    }

    // --- Control 5: DAX IQ enable + rate combo ---
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_iqBtn = greenToggle(QStringLiteral("IQ"));
        m_iqBtn->setCheckable(true);
        row->addWidget(m_iqBtn);

        m_iqRateCombo = new QComboBox(this);
        m_iqRateCombo->addItems({
            QStringLiteral("48000"),
            QStringLiteral("96000"),
            QStringLiteral("192000")
        });
        applyComboStyle(m_iqRateCombo);
        row->addWidget(m_iqRateCombo, 1);

        vbox->addLayout(row);

        NyiOverlay::markNyi(m_iqBtn,       QStringLiteral("3-DAX"));
        NyiOverlay::markNyi(m_iqRateCombo, QStringLiteral("3-DAX"));
    }

    vbox->addStretch();
    root->addWidget(body);
}

void CatApplet::syncFromModel()
{
    // NYI — Phase 3K / 3J / 3-DAX
}

} // namespace NereusSDR
