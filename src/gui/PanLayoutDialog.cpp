// no-port-check: AetherSDR-derived NereusSDR file. Three-column painted
// thumbnail grid, structurally from AetherSDR PanLayoutDialog [@c6481cb].
// Registered in docs/attribution/aethersdr-contributor-index.md:270.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/gui/PanLayoutDialog.cpp  (NereusSDR)
// =================================================================
//
// Ported (structurally) from AetherSDR PanLayoutDialog [@c6481cb].
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
//   2026-08-02  J.J. Boyd / KG4VCF  Bottom-banner + pan-menu epic,
//                                    Task B3. Rebuilt around AetherSDR's
//                                    three-column painted thumbnail grid
//                                    at @c6481cb: nine layouts from
//                                    Task B2's `kPanLayouts`, current one
//                                    highlighted via `LayoutThumbnail`.
//                                    Layouts the connected board cannot
//                                    host are hidden rather than greyed,
//                                    with a footer sentence naming the
//                                    board and the count (design doc
//                                    §8.4). AI-assisted transformation
//                                    via Anthropic Claude Code.
//   2026-08-02  J.J. Boyd / KG4VCF  Final fix-wave: gating corrected from
//                                    raw maxSlices to qMin(maxSlices,
//                                    userDdcCount); footer wording changed
//                                    from "N slices" to "N pans" to match
//                                    (was misleading twice over on a board
//                                    like HL2 where the two numbers
//                                    differ). AI-assisted transformation
//                                    via Anthropic Claude Code.
// =================================================================

#include "gui/PanLayoutDialog.h"

#include "gui/StyleConstants.h"
#include "gui/widgets/LayoutThumbnail.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace NereusSDR {

namespace {
// From AetherSDR PanLayoutDialog.cpp:138 [@c6481cb].
constexpr int kMaxCols = 3;
} // namespace

PanLayoutDialog::PanLayoutDialog(int maxPanCount, const QString& currentLayoutId,
                                 const QString& boardName, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Panadapter Layout"));
    setStyleSheet(QStringLiteral("background: %1; color: %2;")
                      .arg(Style::kAppBg, Style::kTextPrimary));
    buildUi(maxPanCount, currentLayoutId, boardName);
}

PanLayoutDialog::~PanLayoutDialog() = default;

void PanLayoutDialog::buildUi(int maxPanCount, const QString& currentLayoutId,
                              const QString& boardName)
{
    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(16, 16, 16, 16);
    vbox->setSpacing(8);

    auto* title = new QLabel(tr("Choose panadapter layout"), this);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 13px; font-weight: bold; }")
        .arg(Style::kTextSecondary));
    vbox->addWidget(title);

    auto* grid = new QGridLayout();
    grid->setSpacing(8);
    vbox->addLayout(grid);

    int col = 0;
    int row = 0;
    int hidden = 0;

    for (const PanLayoutGeometry& g : kPanLayouts) {
        // Hide rather than grey. A tile that can never be clicked is noise,
        // and greying nine down to three on a 2-pan board makes the dialog
        // look broken. AetherSDR greys; see design §8.4 for the divergence.
        if (g.panCount > maxPanCount) {
            ++hidden;
            continue;
        }
        m_visibleIds << g.id;

        auto* btn = new QPushButton(this);
        btn->setFixedSize(130, 118);
        btn->setStyleSheet(QStringLiteral(
            "QPushButton { background: transparent; border: none; }"
            "QPushButton:hover { background: rgba(0, 180, 216, 30);"
            " border: 1px solid %1; border-radius: 4px; }")
            .arg(Style::kAccent));

        auto* btnLayout = new QVBoxLayout(btn);
        btnLayout->setContentsMargins(4, 4, 4, 2);
        btnLayout->setSpacing(2);
        btnLayout->addWidget(
            new LayoutThumbnail(g, g.id == currentLayoutId, true, btn),
            0, Qt::AlignCenter);

        auto* caption = new QLabel(
            tr("%1 (%2 pan%3)").arg(g.label)
                               .arg(g.panCount)
                               .arg(g.panCount > 1 ? QStringLiteral("s")
                                                   : QString()),
            btn);
        caption->setAlignment(Qt::AlignCenter);
        caption->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 10px; }").arg(Style::kTextSecondary));
        btnLayout->addWidget(caption);

        const QString layoutId = g.id;
        connect(btn, &QPushButton::clicked, this, [this, layoutId]() {
            m_selected = layoutId;
            accept();
        });

        grid->addWidget(btn, row, col);
        if (++col >= kMaxCols) {
            col = 0;
            ++row;
        }
    }

    if (hidden > 0) {
        // Names the radio, never the app: "this radio does not have that".
        // Built by hand rather than via tr(..., "", n): Qt's plural form
        // does not reliably produce the literal "1 layout" / "N layouts"
        // substrings the gating tests assert on.
        const QString layoutWord = (hidden == 1)
            ? tr("1 layout needs")
            : tr("%1 layouts need").arg(hidden);
        m_footerText = tr("%1 allots %2 pans. %3 a radio with more.")
                           .arg(boardName)
                           .arg(maxPanCount)
                           .arg(layoutWord);
        auto* footer = new QLabel(m_footerText, this);
        footer->setAlignment(Qt::AlignCenter);
        footer->setWordWrap(true);
        footer->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 11px; }").arg(Style::kTextScale));
        vbox->addWidget(footer);
    }

    auto* footerRow = new QHBoxLayout();
    vbox->addLayout(footerRow);
    footerRow->addStretch(1);
    auto* cancelBtn = new QPushButton(tr("Cancel"), this);
    cancelBtn->setStyleSheet(Style::buttonBaseStyle());
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    footerRow->addWidget(cancelBtn);
    footerRow->addStretch(1);
}

} // namespace NereusSDR
