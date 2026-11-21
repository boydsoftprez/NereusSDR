// NereusSDR-original: HF-Auto MMIO control strip for the HF-Auto container.
#include "HfAutoControls.h"
#include "gui/HfAutoMmio.h"
#include "core/AppSettings.h"
#include "core/mmio/ExternalVariableEngine.h"
#include "core/mmio/MmioEndpoint.h"
#include "gui/StyleConstants.h"
#include "gui/styles/WorkspaceStyle.h"
#include <QButtonGroup>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTimer>

namespace NereusSDR {

HfAutoControls::HfAutoControls(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("hfAutoMmioControls"));
    // Start from the console's canonical button style so the strip matches
    // Nereus's other buttons, then compact it for the container width.
    QString style = QString::fromLatin1(Style::kButtonStyle);
    style += QStringLiteral(
        "QPushButton { font-size: 10px; padding: 2px 5px; }"
        "QPushButton:checked { background: %1; border-color: %1; color: #0f0f1a; }")
        .arg(Workspace::role("accent", Style::kAccent));
    // The same disabled trio the rest of Nereus uses: a button that cannot
    // act must not look enabled. Appended last so it also wins for checked
    // buttons (Fine/C-L controls outside Manual).
    style += Style::darkPageDisabledRules();
    style += QStringLiteral("QLabel { color: %1; font-size: 10px; }")
        .arg(Workspace::role("text-secondary", "#8090a0"));
    setStyleSheet(style);

    auto* grid = new QGridLayout(this);
    grid->setContentsMargins(4, 2, 4, 2);
    grid->setSpacing(2);

    m_auto = makeButton(QStringLiteral("Auto"), tr("Tuner mode: Auto"));
    m_manual = makeButton(QStringLiteral("Man"), tr("Tuner mode: Manual"));
    m_bypass = makeButton(QStringLiteral("Byp"), tr("Tuner mode: Bypass"));
    m_ant1 = makeButton(QStringLiteral("A1"), tr("Antenna 1"));
    m_ant2 = makeButton(QStringLiteral("A2"), tr("Antenna 2"));
    m_ant3 = makeButton(QStringLiteral("A3"), tr("Antenna 3"));
    m_dummy = makeButton(QStringLiteral("DL"), tr("Dummy load"));

    const auto mode = [](QPushButton* button) { button->setCheckable(true); };
    mode(m_auto);
    mode(m_manual);
    mode(m_bypass);
    mode(m_ant1);
    mode(m_ant2);
    mode(m_ant3);
    m_dummy->setCheckable(true);

    auto* modeGroup = new QButtonGroup(this);
    modeGroup->setExclusive(true);
    modeGroup->addButton(m_auto);
    modeGroup->addButton(m_manual);
    modeGroup->addButton(m_bypass);

    auto* antennaGroup = new QButtonGroup(this);
    antennaGroup->setExclusive(true);
    antennaGroup->addButton(m_ant1);
    antennaGroup->addButton(m_ant2);
    antennaGroup->addButton(m_ant3);

    grid->addWidget(m_auto, 0, 0);
    grid->addWidget(m_manual, 0, 1);
    grid->addWidget(m_bypass, 0, 2);
    grid->addWidget(m_ant1, 0, 3);
    grid->addWidget(m_ant2, 0, 4);
    grid->addWidget(m_ant3, 0, 5);
    grid->addWidget(m_dummy, 0, 6);

    m_tf = makeButton(QStringLiteral("TF"), tr("Tuner Follow: let HF-Auto track the TX frequency"));
    m_fine = makeButton(QStringLiteral("Fine"), tr("Fine C/L stepping (Manual only)"));
    m_cl = makeButton(QStringLiteral("C/L C"), tr("Switch the Manual C/L adjustment target (Manual only)"));
    m_minus = makeButton(QStringLiteral("\u2212"), tr("Step the selected C/L down (Manual only)"));
    m_plus = makeButton(QStringLiteral("+"), tr("Step the selected C/L up (Manual only)"));
    m_store = makeButton(QStringLiteral("Store"), tr("Store the Manual C/L point (Manual only; needs 20-40 W RF)"));
    m_hold = makeButton(QStringLiteral("Hold"), tr("Hold the last RF reading when not transmitting"));
    m_tf->setCheckable(true);
    m_fine->setCheckable(true);
    m_hold->setCheckable(true);

    grid->addWidget(m_tf, 1, 0);
    grid->addWidget(m_fine, 1, 1);
    grid->addWidget(m_cl, 1, 2);
    grid->addWidget(m_minus, 1, 3);
    grid->addWidget(m_plus, 1, 4);
    grid->addWidget(m_store, 1, 5);
    grid->addWidget(m_hold, 1, 6);

    m_status = new QLabel(tr("MMIO starting..."), this);
    m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_status->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    grid->addWidget(m_status, 0, 7);

    connect(m_auto, &QPushButton::clicked, this, [this] { sendMode("auto"); });
    connect(m_manual, &QPushButton::clicked, this, [this] { sendMode("manual"); });
    connect(m_bypass, &QPushButton::clicked, this, [this] { sendMode("bypass"); });
    connect(m_ant1, &QPushButton::clicked, this, [this] {
        if (!m_refreshing) { HfAutoMmio::instance().sendCommand(QStringLiteral("antenna"), 1); }
    });
    connect(m_ant2, &QPushButton::clicked, this, [this] {
        if (!m_refreshing) { HfAutoMmio::instance().sendCommand(QStringLiteral("antenna"), 2); }
    });
    connect(m_ant3, &QPushButton::clicked, this, [this] {
        if (!m_refreshing) { HfAutoMmio::instance().sendCommand(QStringLiteral("antenna"), 3); }
    });
    connect(m_dummy, &QPushButton::clicked, this, [this](bool checked) {
        if (!m_refreshing) {
            HfAutoMmio::instance().sendCommand(QStringLiteral("dummy"), checked ? 1 : 0);
        }
    });
    connect(m_tf, &QPushButton::clicked, this, [this](bool checked) {
        if (m_refreshing) { return; }
        AppSettings::instance().setValue(QStringLiteral("Helpers/HfAutoFollow"), checked);
        AppSettings::instance().save();
        HfAutoMmio::instance().setFollow(checked);
    });
    connect(m_fine, &QPushButton::clicked, this, [this](bool checked) {
        if (m_refreshing || !m_fine->isEnabled()) { return; }
        HfAutoMmio::instance().sendCommand(QStringLiteral("fine"), checked ? 1 : 0);
    });
    connect(m_cl, &QPushButton::clicked, this, [this] {
        if (!m_refreshing && m_cl->isEnabled()) {
            HfAutoMmio::instance().sendCommand(QStringLiteral("cl"));
        }
    });
    connect(m_minus, &QPushButton::clicked, this, [this] {
        if (!m_refreshing && m_minus->isEnabled()) {
            HfAutoMmio::instance().sendCommand(QStringLiteral("minus"));
        }
    });
    connect(m_plus, &QPushButton::clicked, this, [this] {
        if (!m_refreshing && m_plus->isEnabled()) {
            HfAutoMmio::instance().sendCommand(QStringLiteral("plus"));
        }
    });
    connect(m_store, &QPushButton::clicked, this, [this] {
        if (!m_refreshing && m_store->isEnabled()) {
            HfAutoMmio::instance().sendCommand(QStringLiteral("store"));
        }
    });
    connect(m_hold, &QPushButton::clicked, this, [this](bool checked) {
        if (m_refreshing) { return; }
        HfAutoMmio::instance().sendCommand(QStringLiteral("hold"), checked ? 1 : 0);
    });

    m_timer = new QTimer(this);
    m_timer->setInterval(1000);
    connect(m_timer, &QTimer::timeout, this, &HfAutoControls::refresh);
    refresh();
}

void HfAutoControls::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (m_timer) { m_timer->start(); }
    refresh();
}

void HfAutoControls::hideEvent(QHideEvent* event)
{
    if (m_timer) { m_timer->stop(); }
    QWidget::hideEvent(event);
}

QPushButton* HfAutoControls::makeButton(const QString& text, const QString& tooltip)
{
    auto* button = new QPushButton(text, this);
    button->setToolTip(tooltip);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    button->setMinimumHeight(20);
    return button;
}

void HfAutoControls::sendMode(const char* command)
{
    if (!m_refreshing) { HfAutoMmio::instance().sendCommand(QString::fromLatin1(command)); }
}

void HfAutoControls::refresh()
{
    MmioEndpoint* endpoint = ExternalVariableEngine::instance()
                                 .endpointByFourChar(QStringLiteral("QxG4"));
    if (!endpoint) {
        m_status->setText(tr("No QxG4"));
        m_status->setStyleSheet(QStringLiteral("color: #ff8080;"));
        setEnabled(false);
        return;
    }

    const auto value = [endpoint](const char* name) {
        return endpoint->valueForName(QLatin1String(name));
    };
    const QVariant connectedValue = value("HelperConnected");
    const bool connected = !connectedValue.isValid() || connectedValue.toBool();
    setEnabled(connected);

    const bool held = value("HelperReadingHeld").toBool();
    if (connectedValue.isValid()) {
        m_status->setText(connected ? (held ? tr("MMIO held") : tr("MMIO"))
                                    : tr("Disconnected"));
        m_status->setStyleSheet(connected
            ? (held ? QStringLiteral("color: #f0c060;")
                    : QStringLiteral("color: #6fcf97;"))
            : QStringLiteral("color: #ff8080;"));
        m_status->setToolTip(held ? tr("Last valid RF reading held (no RF)") : QString());
    } else {
        m_status->setText(tr("MMIO..."));
        m_status->setStyleSheet(QStringLiteral("color: #8090a0;"));
    }
    if (!connected) { return; }

    const int modeValue = value("Mode").toInt();
    const int antenna = value("AntennaSelected").toInt();
    const bool manual = modeValue == 2;

    m_refreshing = true;
    {
        const QSignalBlocker a(m_auto);
        const QSignalBlocker b(m_manual);
        const QSignalBlocker c(m_bypass);
        m_auto->setChecked(modeValue == 1);
        m_manual->setChecked(manual);
        m_bypass->setChecked(modeValue == 3);
    }
    {
        const QSignalBlocker a(m_ant1);
        const QSignalBlocker b(m_ant2);
        const QSignalBlocker c(m_ant3);
        m_ant1->setChecked(antenna == 1);
        m_ant2->setChecked(antenna == 2);
        m_ant3->setChecked(antenna == 3);
    }
    {
        const QSignalBlocker blocker(m_tf);
        m_tf->setChecked(value("HelperTunerFollow").toBool());
    }
    {
        const QSignalBlocker blocker(m_fine);
        m_fine->setChecked(value("HelperFine").toBool());
    }
    {
        const QSignalBlocker blocker(m_hold);
        m_hold->setChecked(value("HelperHoldReadings").toBool());
    }
    {
        const QSignalBlocker blocker(m_dummy);
        m_dummy->setChecked(value("HelperDummy").toBool());
    }
    const QString target = value("HelperAdjustTarget").toString();
    m_cl->setText(target.compare(QLatin1String("L"), Qt::CaseInsensitive) == 0
                      ? tr("C/L L") : tr("C/L C"));

    m_fine->setEnabled(manual);
    m_cl->setEnabled(manual);
    m_minus->setEnabled(manual);
    m_plus->setEnabled(manual);
    m_store->setEnabled(manual);
    m_refreshing = false;
}

} // namespace NereusSDR
