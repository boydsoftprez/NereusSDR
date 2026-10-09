// =================================================================
// src/gui/setup/AsioSwitchAllDialog.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  See AsioSwitchAllDialog.h.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 17 (R-AUD-19). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
// =================================================================

#include "gui/setup/AsioSwitchAllDialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace NereusSDR {

AsioSwitchAllDialog::AsioSwitchAllDialog(const QString& device, const QString& driver,
                                         const QList<QPair<QString, QString>>& moves,
                                         QWidget* parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("asioSwitchAllDialog"));
    setWindowTitle(QStringLiteral("One ASIO driver at a time"));
    setModal(true);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(8);

    auto* text = new QLabel(
        QStringLiteral("NereusSDR can use one ASIO driver at a time. Switching %1 to %2 also moves:")
            .arg(device, driver),
        this);
    text->setObjectName(QStringLiteral("asioSwitchAllText"));
    text->setWordWrap(true);
    layout->addWidget(text);

    auto* list = new QVBoxLayout;
    list->setContentsMargins(16, 0, 0, 0);
    list->setSpacing(2);
    for (const QPair<QString, QString>& move : moves) {
        auto* line = new QLabel(QStringLiteral("%1: %2").arg(move.first, move.second), this);
        line->setObjectName(QStringLiteral("asioSwitchAllMove"));
        list->addWidget(line);
    }
    layout->addLayout(list);

    auto* note = new QLabel(QStringLiteral("Cancel keeps everything as it is. To keep a device "
                                           "where it is, cancel and give it a Windows audio "
                                           "device or cable first."),
                            this);
    note->setObjectName(QStringLiteral("asioSwitchAllNote"));
    note->setWordWrap(true);
    layout->addWidget(note);

    auto* buttons = new QHBoxLayout;
    buttons->addStretch(1);
    auto* cancel = new QPushButton(QStringLiteral("Cancel"), this);
    cancel->setObjectName(QStringLiteral("asioSwitchAllCancel"));
    auto* ok = new QPushButton(QStringLiteral("Switch all to %1").arg(driver), this);
    ok->setObjectName(QStringLiteral("asioSwitchAllOk"));
    ok->setDefault(true);
    buttons->addWidget(cancel);
    buttons->addWidget(ok);
    layout->addLayout(buttons);

    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(ok, &QPushButton::clicked, this, &QDialog::accept);
    setMinimumWidth(420);
}

} // namespace NereusSDR
