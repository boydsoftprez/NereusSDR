// =================================================================
// src/gui/applets/Rf2ksApplet.cpp  (NereusSDR-native)
// =================================================================
//   2026-05-24  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude.
//   Layout patterns from src/gui/applets/AmpApplet.{h,cpp} (which is
//   an AetherSDR port). The RF-Kit-specific content is original.
// =================================================================

#include "Rf2ksApplet.h"
#include "gui/HGauge.h"
#include "models/RadioModel.h"

#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QVBoxLayout>

namespace NereusSDR {

Rf2ksApplet::Rf2ksApplet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Section A: header row.
    // Device label + nickname/version on the left;
    // status dot + OPERATE/STANDBY button on the right.
    auto* headerWrap = new QWidget(this);
    auto* header = new QHBoxLayout(headerWrap);
    header->setContentsMargins(8, 6, 8, 6);

    auto* leftCol = new QVBoxLayout();
    m_deviceLabel   = new QLabel(QStringLiteral("RF-Kit RF2K-S"), headerWrap);
    m_nicknameLabel = new QLabel(QString(), headerWrap);
    m_deviceLabel->setStyleSheet(QStringLiteral("font-weight:600;"));
    m_nicknameLabel->setStyleSheet(QStringLiteral("color:#7d8893; font-size:10px;"));
    leftCol->addWidget(m_deviceLabel);
    leftCol->addWidget(m_nicknameLabel);
    header->addLayout(leftCol);
    header->addStretch();

    m_statusDot = new QLabel(headerWrap);
    m_statusDot->setFixedSize(10, 10);
    setConnectedState(false);
    header->addWidget(m_statusDot);

    m_operateBtn = new QPushButton(QStringLiteral("STANDBY"), headerWrap);
    m_operateMode = QStringLiteral("STANDBY");
    connect(m_operateBtn, &QPushButton::clicked, this, [this]() {
        emit operateToggled(m_operateMode != QStringLiteral("OPERATE"));
    });
    header->addWidget(m_operateBtn);

    root->addWidget(headerWrap);

    // Section B: gauges + telemetry strip.
    auto* gaugesWrap = new QWidget(this);
    auto* gaugesLay  = new QVBoxLayout(gaugesWrap);
    gaugesLay->setContentsMargins(8, 6, 8, 6);

    m_fwdGauge  = new HGauge(gaugesWrap);
    m_fwdGauge->setRange(0.0, 2000.0);
    m_fwdGauge->setYellowStart(1500.0);
    m_fwdGauge->setRedStart(1800.0);
    m_fwdGauge->setTitle(QStringLiteral("Fwd"));
    m_fwdGauge->setUnit(QStringLiteral("W"));
    gaugesLay->addWidget(m_fwdGauge);

    // SWR stored as x100 fixed-point (1.0 -> 100, 3.0 -> 300) so we can
    // use the integer-friendly gauge range while still resolving to 2 d.p.
    m_swrGauge  = new HGauge(gaugesWrap);
    m_swrGauge->setRange(100.0, 300.0);
    m_swrGauge->setYellowStart(200.0);
    m_swrGauge->setRedStart(250.0);
    m_swrGauge->setTitle(QStringLiteral("SWR"));
    gaugesLay->addWidget(m_swrGauge);

    m_tempGauge = new HGauge(gaugesWrap);
    m_tempGauge->setRange(0.0, 80.0);
    m_tempGauge->setYellowStart(60.0);
    m_tempGauge->setRedStart(70.0);
    m_tempGauge->setTitle(QStringLiteral("Temp"));
    m_tempGauge->setUnit(QStringLiteral("C"));
    gaugesLay->addWidget(m_tempGauge);

    m_telemetryLabel = new QLabel(gaugesWrap);
    m_telemetryLabel->setStyleSheet(QStringLiteral("color:#9aa5b1; font-size:10px;"));
    gaugesLay->addWidget(m_telemetryLabel);
    root->addWidget(gaugesWrap);

    // Section C: antennas + tuner status + greyed TUNE/BYPASS.
    auto* tunerWrap = new QWidget(this);
    auto* tunerLay  = new QVBoxLayout(tunerWrap);
    tunerLay->setContentsMargins(8, 6, 8, 6);

    auto* antennaRow = new QHBoxLayout();
    for (int i = 1; i <= 4; ++i) {
        auto* btn = new QPushButton(QStringLiteral("ANT %1").arg(i), tunerWrap);
        connect(btn, &QPushButton::clicked, this, [this, i]() {
            emit antennaRequested(RfKitAntenna::Type::Internal, i);
        });
        m_antennaButtons[i] = btn;
        antennaRow->addWidget(btn);
    }
    tunerLay->addLayout(antennaRow);

    m_tunerStatusLabel = new QLabel(QStringLiteral("Tuner: -"), tunerWrap);
    tunerLay->addWidget(m_tunerStatusLabel);

    auto* actionRow = new QHBoxLayout();
    m_tuneBtn   = new QPushButton(QStringLiteral("TUNE"),   tunerWrap);
    m_bypassBtn = new QPushButton(QStringLiteral("BYPASS"), tunerWrap);
    const QString tip = QStringLiteral(
        "RF2K-S firmware (G200C267) does not expose a tuner write verb.\n"
        "Press TUNE / BYPASS on the amp's front panel.\n"
        "Feature request to RF-Power filed; this button un-greys when firmware ships it.");
    m_tuneBtn->setEnabled(false);
    m_bypassBtn->setEnabled(false);
    m_tuneBtn->setToolTip(tip);
    m_bypassBtn->setToolTip(tip);
    actionRow->addWidget(m_tuneBtn,   2);
    actionRow->addWidget(m_bypassBtn, 1);
    tunerLay->addLayout(actionRow);

    root->addWidget(tunerWrap);
}

// ---------- Section A slots ----------

void Rf2ksApplet::setNicknameAndVersion(const QString& nickname, const QString& version)
{
    m_nicknameLabel->setText(QStringLiteral("%1  %2").arg(nickname, version));
}

void Rf2ksApplet::setOperateMode(const QString& mode)
{
    m_operateMode = mode;
    m_operateBtn->setText(mode);
}

void Rf2ksApplet::setConnectedState(bool connected)
{
    m_connected = connected;
    const QString color = connected
        ? QStringLiteral("#34c759")
        : QStringLiteral("#e64949");
    m_statusDot->setStyleSheet(
        QStringLiteral("background:%1; border-radius:5px;").arg(color));
}

// ---------- Section B slots ----------

void Rf2ksApplet::setPower(const RfKitPowerSnapshot& snap)
{
    m_fwdGauge->setValue(static_cast<double>(snap.forwardW));
    // SWR stored x100 so the gauge range (100..300) maps to 1.0..3.0.
    m_swrGauge->setValue(static_cast<double>(snap.swr) * 100.0);
    m_tempGauge->setValue(static_cast<double>(snap.temperatureC));
    m_telemetryLabel->setText(QStringLiteral("%1 V  %2 A")
        .arg(snap.voltageV, 0, 'f', 0)
        .arg(snap.currentA, 0, 'f', 1));
}

// ---------- Section C slots ----------

void Rf2ksApplet::setTuner(const RfKitTunerSnapshot& snap)
{
    using Mode = RfKitTunerSnapshot::Mode;
    QString text;
    switch (snap.mode) {
        case Mode::AutoTuning:
            text = QStringLiteral("TUNING...");
            break;
        case Mode::Bypass:
            text = QStringLiteral("BYPASS");
            break;
        case Mode::Auto:
        case Mode::Manual:
            if (snap.tunedFrequencyKHz > 0) {
                text = QStringLiteral("TUNED %1 MHz (%2)")
                          .arg(snap.tunedFrequencyKHz / 1000.0, 0, 'f', 3)
                          .arg(snap.setup);
            } else {
                text = QStringLiteral("NOT TUNED");
            }
            break;
        case Mode::Unknown:
            text = QStringLiteral("Tuner: -");
            break;
    }
    m_tunerStatusLabel->setText(text);
}

void Rf2ksApplet::setAntennas(const QList<RfKitAntenna>& list)
{
    for (const auto& a : list) {
        if (a.type != RfKitAntenna::Type::Internal) {
            continue;
        }
        auto* btn = m_antennaButtons.value(a.number, nullptr);
        if (!btn) {
            continue;
        }
        const QString label = m_antennaLabels.value(a.number,
            QStringLiteral("ANT %1").arg(a.number));
        btn->setText(label);
        btn->setEnabled(a.state != RfKitAntenna::State::Disabled);
        btn->setProperty("active", a.state == RfKitAntenna::State::Active);
    }
}

void Rf2ksApplet::setActiveAntenna(const RfKitAntenna& a)
{
    for (auto it = m_antennaButtons.begin(); it != m_antennaButtons.end(); ++it) {
        it.value()->setProperty("active", it.key() == a.number);
    }
}

void Rf2ksApplet::setAntennaLabel(int number, const QString& label)
{
    m_antennaLabels[number] = label;
    if (auto* btn = m_antennaButtons.value(number, nullptr)) {
        btn->setText(label.isEmpty()
            ? QStringLiteral("ANT %1").arg(number)
            : label);
    }
}

// ---------- Section A test seams ----------

QString Rf2ksApplet::deviceLabelTextForTesting()   const { return m_deviceLabel->text(); }
QString Rf2ksApplet::nicknameLabelTextForTesting() const { return m_nicknameLabel->text(); }
QString Rf2ksApplet::operateButtonTextForTesting() const { return m_operateBtn->text(); }
void    Rf2ksApplet::clickOperateButtonForTesting() { m_operateBtn->click(); }

// ---------- Section B+C test seams ----------

int     Rf2ksApplet::fwdGaugeValueForTesting()      const { return static_cast<int>(m_fwdGauge->value()); }
float   Rf2ksApplet::swrGaugeValueForTesting()      const { return static_cast<float>(m_swrGauge->value() / 100.0); }
float   Rf2ksApplet::tempGaugeValueForTesting()     const { return static_cast<float>(m_tempGauge->value()); }
QString Rf2ksApplet::telemetryStripTextForTesting() const { return m_telemetryLabel->text(); }

QString Rf2ksApplet::antennaButtonTextForTesting(int number) const
{
    const auto* btn = m_antennaButtons.value(number, nullptr);
    return btn ? btn->text() : QString();
}

bool Rf2ksApplet::antennaButtonIsActiveForTesting(int number) const
{
    const auto* btn = m_antennaButtons.value(number, nullptr);
    return btn && btn->property("active").toBool();
}

bool Rf2ksApplet::antennaButtonIsEnabledForTesting(int number) const
{
    const auto* btn = m_antennaButtons.value(number, nullptr);
    return btn && btn->isEnabled();
}

void Rf2ksApplet::clickAntennaButtonForTesting(int number)
{
    if (auto* btn = m_antennaButtons.value(number, nullptr)) {
        btn->click();
    }
}

QString Rf2ksApplet::tunerStatusTextForTesting()    const { return m_tunerStatusLabel->text(); }
bool    Rf2ksApplet::tuneButtonIsEnabledForTesting()   const { return m_tuneBtn->isEnabled(); }
bool    Rf2ksApplet::bypassButtonIsEnabledForTesting() const { return m_bypassBtn->isEnabled(); }
QString Rf2ksApplet::tuneButtonTooltipForTesting()  const { return m_tuneBtn->toolTip(); }

// ---------- Section D: right-click context menu ----------

QMenu* Rf2ksApplet::buildContextMenu(QObject* menuParent)
{
    auto* menu = new QMenu(qobject_cast<QWidget*>(menuParent));
    auto* openAdv = menu->addAction(QStringLiteral("Open RF-Kit Advanced..."));
    menu->addSeparator();
    auto* disco = menu->addAction(m_connected
                                    ? QStringLiteral("Disconnect")
                                    : QStringLiteral("Reconnect"));
    auto* diag  = menu->addAction(QStringLiteral("Copy diagnostics to clipboard"));

    connect(openAdv, &QAction::triggered, this, [this] {
        emit navigationRequested(QStringLiteral("rfKit"));
    });
    connect(disco, &QAction::triggered, this, &Rf2ksApplet::connectionToggleRequested);
    connect(diag,  &QAction::triggered, this, &Rf2ksApplet::diagnosticsCopyRequested);
    return menu;
}

void Rf2ksApplet::contextMenuEvent(QContextMenuEvent* ev)
{
    auto* menu = buildContextMenu(this);
    menu->exec(ev->globalPos());
    delete menu;
}

} // namespace NereusSDR
