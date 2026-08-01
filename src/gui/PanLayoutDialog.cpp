// no-port-check: AetherSDR-derived NereusSDR file. 5-tile visual layout
// picker is adapted structurally from AetherSDR PanLayoutDialog
// [@0cd4559]. Registered in docs/attribution/aethersdr-reconciliation.md.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/gui/PanLayoutDialog.cpp  (NereusSDR)
// =================================================================
//
// Ported (structurally) from AetherSDR PanLayoutDialog [@0cd4559].
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-27  J.J. Boyd / KG4VCF  Phase 3F Sub-Epic D Task 9.
//                                    5-tile picker implementation. Uses
//                                    Style::buttonBaseStyle() so the
//                                    tiles match every other applet in
//                                    NereusSDR. AI-assisted
//                                    transformation via Anthropic
//                                    Claude Code.
// =================================================================

#include "gui/PanLayoutDialog.h"

#include "gui/StyleConstants.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace NereusSDR {

namespace {

struct LayoutTile {
    const char* id;
    const char* label;
    const char* subtitle;
};

constexpr LayoutTile kTiles[] = {
    {"1",   "Single",       "1 pan"},
    {"2v",  "Stacked",      "2 pans (2v)"},
    {"2h",  "Side-by-Side", "2 pans (2h)"},
    {"12h", "Wide + 2",     "3 pans (12h, WIDEBAND)"},
    {"2x2", "Grid",         "4 pans (2x2)"},
};

} // namespace

PanLayoutDialog::PanLayoutDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Pan Layout"));
    setStyleSheet(QStringLiteral("background: %1; color: %2;")
                      .arg(Style::kAppBg, Style::kTextPrimary));

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(10);

    auto* intro = new QLabel(
        QStringLiteral("Pick a layout. Per-pan source is set with right-click after applying."),
        this);
    intro->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;")
                             .arg(Style::kTextSecondary));
    mainLayout->addWidget(intro);

    auto* tileRow = new QHBoxLayout();
    tileRow->setSpacing(8);
    mainLayout->addLayout(tileRow);

    for (const auto& tile : kTiles) {
        auto* btn = new QPushButton(
            QStringLiteral("%1\n%2").arg(QString::fromLatin1(tile.label),
                                          QString::fromLatin1(tile.subtitle)),
            this);
        btn->setFixedSize(120, 100);
        btn->setStyleSheet(Style::buttonBaseStyle());
        const QString tileId = QString::fromLatin1(tile.id);
        connect(btn, &QPushButton::clicked, this, [this, tileId]() {
            m_selected = tileId;
            accept();
        });
        tileRow->addWidget(btn);
    }

    auto* footerRow = new QHBoxLayout();
    mainLayout->addLayout(footerRow);
    footerRow->addStretch(1);
    auto* cancelBtn = new QPushButton(QStringLiteral("Cancel"), this);
    cancelBtn->setStyleSheet(Style::buttonBaseStyle());
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    footerRow->addWidget(cancelBtn);
}

PanLayoutDialog::~PanLayoutDialog() = default;

} // namespace NereusSDR
