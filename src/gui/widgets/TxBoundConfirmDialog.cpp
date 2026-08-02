// =================================================================
// src/gui/widgets/TxBoundConfirmDialog.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. See header for description.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-27 Created in C++20/Qt6 for NereusSDR by J.J. Boyd (KG4VCF),
//              with AI-assisted transformation via Anthropic Claude Code.
// =================================================================
//
// no-port-check: NereusSDR-original

#include "gui/widgets/TxBoundConfirmDialog.h"

#include "gui/StyleConstants.h"
#include "models/SliceModel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace NereusSDR {

TxBoundConfirmDialog::TxBoundConfirmDialog(const QString& proposedAntenna,
                                           const QString& existingAntenna,
                                           const QVector<SliceModel*>& atRiskSlices,
                                           QWidget* parent)
    : QDialog(parent)
{
    Q_UNUSED(atRiskSlices); // reserved for a future iteration; see header.

    setWindowTitle(QStringLiteral("[!] TX-bound antenna re-route"));
    setStyleSheet(
        QStringLiteral("background: %1; color: %2;")
            .arg(Style::kPanelBg, Style::kTextPrimary));
    setFixedWidth(500);

    auto* main = new QVBoxLayout(this);
    main->setContentsMargins(14, 14, 14, 14);

    main->addWidget(new QLabel(
        QStringLiteral("Adding new slice on %1 requires re-routing chain from %2 to %1.")
            .arg(proposedAntenna, existingAntenna),
        this));

    auto* warnLbl = new QLabel(
        QStringLiteral("[!] TX-bound slice is on this chain. Re-routing changes the antenna"
                       " your transmit signal goes to."),
        this);
    warnLbl->setStyleSheet(QStringLiteral("color: #ffb800; font-size: 11px;"));
    warnLbl->setWordWrap(true);
    main->addWidget(warnLbl);

    main->addWidget(new QLabel(
        QStringLiteral("Verify %1 is rated for the current band and TX power.")
            .arg(proposedAntenna),
        this));

    auto* footer = new QHBoxLayout();
    auto* cancelBtn = new QPushButton(QStringLiteral("Cancel"), this);
    auto* useExistingBtn = new QPushButton(
        QStringLiteral("Use %1 instead").arg(existingAntenna), this);
    auto* confirmBtn = new QPushButton(
        QStringLiteral("Re-route %1").arg(proposedAntenna), this);
    cancelBtn->setStyleSheet(Style::buttonBaseStyle());
    useExistingBtn->setStyleSheet(Style::buttonBaseStyle());
    confirmBtn->setStyleSheet(Style::buttonBaseStyle() + Style::blueCheckedStyle());

    connect(cancelBtn, &QPushButton::clicked, this, [this]() {
        m_outcome = Cancelled;
        reject();
    });
    connect(useExistingBtn, &QPushButton::clicked, this, [this]() {
        m_outcome = UseExistingAntenna;
        accept();
    });
    connect(confirmBtn, &QPushButton::clicked, this, [this]() {
        m_outcome = ConfirmReroute;
        accept();
    });

    footer->addWidget(cancelBtn);
    footer->addStretch(1);
    footer->addWidget(useExistingBtn);
    footer->addWidget(confirmBtn);
    main->addLayout(footer);
}

} // namespace NereusSDR
