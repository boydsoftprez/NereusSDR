// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/multidevice/ConfirmChangeDialog.cpp  (NereusSDR)
// =================================================================
//
// See ConfirmChangeDialog.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 78 (R-IOS-30), with
//               AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include "gui/multidevice/ConfirmChangeDialog.h"

#include "gui/multidevice/DeviceWords.h"

#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace NereusSDR {

QString ConfirmChangeDialog::changeText(const SessionPrompt& prompt)
{
    if (!prompt.change) {
        return {};
    }
    const QJsonObject& c = *prompt.change;
    const QString label = c.value(QStringLiteral("label")).toString();
    const QString from = c.value(QStringLiteral("from")).toString();
    const QString to = c.value(QStringLiteral("to")).toString();
    if (from.isEmpty() && to.isEmpty()) {
        return label;
    }
    return QStringLiteral("%1: %2 to %3").arg(label, from, to);
}

QString ConfirmChangeDialog::effectText(const QString& effect)
{
    if (effect == QStringLiteral("moves")) {
        return QStringLiteral("moves to another receiver");
    }
    if (effect == QStringLiteral("closes")) {
        return QStringLiteral("closes");
    }
    if (effect == QStringLiteral("pausesWhileTransmitting")) {
        return QStringLiteral("pauses while the radio transmits");
    }
    return QStringLiteral("keeps receiving, with the change");
}

QStringList ConfirmChangeDialog::affectedLines(const SessionPrompt& prompt)
{
    QStringList lines;
    for (const QJsonValue& v : prompt.affected) {
        const QJsonObject d = v.toObject();
        QString who = d.value(QStringLiteral("deviceName")).toString();
        if (who.isEmpty()) {
            who = d.value(QStringLiteral("deviceShortName")).toString();
        }
        const QString state = d.value(QStringLiteral("state")).toString();
        if (state == QStringLiteral("away")) {
            who += QStringLiteral(" (away)");
        } else if (state == QStringLiteral("transmitting")) {
            who += QStringLiteral(" (on the air)");
        }
        const QJsonArray slices = d.value(QStringLiteral("slices")).toArray();
        if (slices.isEmpty() && d.value(QStringLiteral("holdsTransmit")).toBool()) {
            lines << QStringLiteral("%1 has the transmitter; this changes it.").arg(who);
            continue;
        }
        lines << who;
        for (const QJsonValue& s : slices) {
            const QJsonObject slice = s.toObject();
            QString line = DeviceWords::sliceLine(slice);
            const QString rx = DeviceWords::receiver(slice.value(QStringLiteral("streamIndex")).toInt(-1));
            if (!rx.isEmpty()) {
                line += QStringLiteral(" on ") + rx;
            }
            lines << QStringLiteral("    %1 %2.").arg(
                line, effectText(slice.value(QStringLiteral("effect")).toString()));
        }
    }
    return lines;
}

ConfirmChangeDialog::ConfirmChangeDialog(const SessionPrompt& prompt, QWidget* parent)
    : QDialog(parent)
    , m_id(prompt.id)
{
    setObjectName(QStringLiteral("ConfirmChangeDialog"));
    setWindowTitle(QStringLiteral("Confirm the change"));
    auto* layout = new QVBoxLayout(this);
    const QString change = changeText(prompt);
    m_change = new QLabel(change.isEmpty() ? QStringLiteral("This change reaches other devices.")
                                           : change,
                          this);
    QFont bold = m_change->font();
    bold.setBold(true);
    m_change->setFont(bold);
    m_change->setWordWrap(true);
    layout->addWidget(m_change);
    auto* intro = new QLabel(QStringLiteral("It reaches:"), this);
    layout->addWidget(intro);
    m_affected = new QLabel(affectedLines(prompt).join(QLatin1Char('\n')), this);
    m_affected->setObjectName(QStringLiteral("confirmChangeAffected"));
    m_affected->setWordWrap(true);
    layout->addWidget(m_affected);

    auto* buttons = new QHBoxLayout;
    buttons->addStretch();
    m_cancel = new QPushButton(QStringLiteral("Cancel"), this);
    m_cancel->setObjectName(QStringLiteral("confirmChangeCancel"));
    m_confirm = new QPushButton(QStringLiteral("Confirm"), this);
    m_confirm->setObjectName(QStringLiteral("confirmChangeConfirm"));
    m_cancel->setDefault(true);
    buttons->addWidget(m_cancel);
    buttons->addWidget(m_confirm);
    layout->addLayout(buttons);
    connect(m_cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_confirm, &QPushButton::clicked, this, &QDialog::accept);
    setMinimumWidth(420);
}

} // namespace NereusSDR
