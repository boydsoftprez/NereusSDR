// no-port-check: NereusSDR-original. No upstream port.

// SPDX-License-Identifier: GPL-3.0-or-later
#include "gui/widgets/SystemTile.h"

// The notifier class is PaTempUnitNotifier, but it lives in PaTempUnit.h
// alongside the PaTempUnit enum. There is no PaTempUnitNotifier.h.
#include "core/PaTempUnit.h"
#include "gui/widgets/MetricLabel.h"

#include <QLabel>
#include <QMouseEvent>
#include <QVBoxLayout>

namespace NereusSDR {

SystemTile::SystemTile(QWidget* parent)
    : QWidget(parent)
{
    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(0);

    m_paRow = new MetricLabel(QStringLiteral("PA"), QString(), this);
    m_paRow->setVisible(false);
    vbox->addWidget(m_paRow);

    m_cpuRow = new MetricLabel(QStringLiteral("CPU"), QStringLiteral("—"), this);
    vbox->addWidget(m_cpuRow);

    // MetricLabel's child QLabels would otherwise eat the press before it
    // reaches mousePressEvent. Same treatment the old PA-T row needed.
    for (QLabel* child : findChildren<QLabel*>()) {
        child->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    }
}

void SystemTile::setPaVolts(double volts)
{
    m_volts    = volts;
    m_hasVolts = true;
    refreshPaRow();
}

void SystemTile::clearPaVolts()
{
    m_hasVolts = false;
    refreshPaRow();
}

void SystemTile::setPaTempCelsius(double celsius)
{
    m_celsius = celsius;
    m_hasTemp = true;
    refreshPaRow();
}

void SystemTile::clearPaTemp()
{
    m_hasTemp = false;
    refreshPaRow();
}

void SystemTile::setCpuPercent(double percent)
{
    m_cpuRow->setValue(QString::asprintf("%.0f%%", percent));
}

void SystemTile::setPaLabel(const QString& label)
{
    m_paRow->setLabel(label);
}

QString SystemTile::paLabel() const
{
    return m_paRow->label();
}

QString SystemTile::paRowText() const
{
    return m_paRow->value();
}

QString SystemTile::cpuRowText() const
{
    return m_cpuRow->value();
}

void SystemTile::refreshPaRow()
{
    if (!hasPaRow()) {
        m_paRow->setValue(QString());
        m_paRow->setVisible(false);
        setToolTip(QString());
        return;
    }

    QString text;
    if (m_hasVolts) {
        text = QString::asprintf("%.1fV", m_volts);
    }
    if (m_hasTemp) {
        // Both readings share row one rather than evicting CPU (design §4.3).
        if (!text.isEmpty()) {
            text += QLatin1Char(' ');
        }
        text += PaTempUnitNotifier::format(m_celsius);
    }
    m_paRow->setValue(text);
    m_paRow->setVisible(true);

    setCursor(m_hasTemp ? Qt::PointingHandCursor : Qt::ArrowCursor);
    setToolTip(m_hasTemp ? tr("Click to toggle °C / °F") : QString());
}

void SystemTile::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_hasTemp) {
        emit paTempClicked();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

} // namespace NereusSDR
