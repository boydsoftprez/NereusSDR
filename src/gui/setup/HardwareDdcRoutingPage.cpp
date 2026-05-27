// =================================================================
// src/gui/setup/HardwareDdcRoutingPage.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. See header for scope notes.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-27 Created in C++20/Qt6 for NereusSDR by J.J. Boyd (KG4VCF),
//              with AI-assisted transformation via Anthropic Claude Code.
// =================================================================

#include "gui/setup/HardwareDdcRoutingPage.h"

#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace NereusSDR {

HardwareDdcRoutingPage::HardwareDdcRoutingPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("DDC Routing"), model, parent)
{
    auto* group = addSection(QStringLiteral("DDC Routing (power-user override)"));
    auto* groupLayout = qobject_cast<QVBoxLayout*>(group->layout());

    auto* intro = new QLabel(
        QStringLiteral("By default, the active codec auto-assigns DDCs to slices "
                       "(see Phase 3F Sub-Epic B). This page will surface per-DDC "
                       "override controls in a polish iteration. For now the "
                       "automatic assignment is canonical."),
        group);
    intro->setWordWrap(true);
    if (groupLayout) {
        groupLayout->addWidget(intro);
    } else {
        group->layout()->addWidget(intro);
    }

    auto* resetBtn = new QPushButton(QStringLiteral("Reset to automatic assignment"), group);
    resetBtn->setEnabled(false);  // No-op until override schema lands.
    resetBtn->setToolTip(QStringLiteral("Clear any per-DDC overrides. Disabled until "
                                          "the override schema is finalized."));
    if (groupLayout) {
        groupLayout->addWidget(resetBtn);
    } else {
        group->layout()->addWidget(resetBtn);
    }

    contentLayout()->addStretch(1);
}

} // namespace NereusSDR
