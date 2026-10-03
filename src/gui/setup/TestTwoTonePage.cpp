// =================================================================
// src/gui/setup/TestTwoTonePage.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original Setup → Test → Two-Tone IMD page.
// See TestTwoTonePage.h for the full header.
//
// Phase 3M-1c Task H (2026-04-29): page UI + model↔UI two-way sync.
// The chkTestIMD activate handler (MOX, TXPostGen* setters, PWR-restore)
// is Phase I and does NOT live on this page.
//
// Written by J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//
// 2026-09-25 - R-R3-49 (parity Task 5): in a remote window the page
// changes the Core's two-tone settings while its radio is off the air,
// behind the transmit settings gate at version 5. J.J. Boyd (KG4VCF),
// AI-assisted via Anthropic Claude Code.
// =================================================================

// no-port-check: NereusSDR-original file; no Thetis logic ported here.

#include "TestTwoTonePage.h"

#include "core/session/IStationLink.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace NereusSDR {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

TestTwoTonePage::TestTwoTonePage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("Two-Tone IMD"), model, parent)
{
    buildUi();
    seedFromModel();
    wireModelSignals();
    // R-R3-49 (parity Task 5): closed in a remote window until SetupDialog
    // pushes the version 5 gate.
    if (model && !model->ownsLocalDsp()) {
        setTransmitSettingsPermittedAt(5, false, QString());
    }
}

void TestTwoTonePage::setTransmitSettingsPermittedAt(int version, bool permitted,
                                                     const QString& reason)
{
    if (version != 5) {
        return;
    }
    gateTransmitControls({m_freq1Spin, m_freq2Spin, m_defaultsBtn, m_stealthBtn, m_levelSpin,
                          m_powerSpin, m_pulsedCheck, m_invertCheck, m_freq2DelaySpin,
                          m_driveSliderRadio, m_tuneSliderRadio, m_fixedDriveRadio},
                         permitted,
                         reason.isEmpty() ? IStationLink::transmitSettingsUnavailableReason()
                                          : reason);
}

// ---------------------------------------------------------------------------
// UI construction
// ---------------------------------------------------------------------------

void TestTwoTonePage::buildUi()
{
    // ── Group 1: Tone Frequencies ─────────────────────────────────────────────
    QGroupBox* freqGroup = addSection(QStringLiteral("Tone Frequencies"));

    auto* freqForm = new QFormLayout();
    freqForm->setContentsMargins(0, 0, 0, 0);

    m_freq1Spin = new QSpinBox(freqGroup);
    m_freq1Spin->setProperty("nereusSetupId", "test.twoToneImd.freq1");
    m_freq1Spin->setRange(TransmitModel::kTwoToneFreq1HzMin,
                          TransmitModel::kTwoToneFreq1HzMax);
    m_freq1Spin->setSingleStep(1);
    m_freq1Spin->setSuffix(QStringLiteral(" Hz"));
    freqForm->addRow(QStringLiteral("Freq #1:"), m_freq1Spin);

    m_freq2Spin = new QSpinBox(freqGroup);
    m_freq2Spin->setProperty("nereusSetupId", "test.twoToneImd.freq2");
    m_freq2Spin->setRange(TransmitModel::kTwoToneFreq2HzMin,
                          TransmitModel::kTwoToneFreq2HzMax);
    m_freq2Spin->setSingleStep(1);
    m_freq2Spin->setSuffix(QStringLiteral(" Hz"));
    freqForm->addRow(QStringLiteral("Freq #2:"), m_freq2Spin);

    // Preset buttons row.
    auto* presetsRow = new QHBoxLayout();
    m_defaultsBtn = new QPushButton(QStringLiteral("Defaults"), freqGroup);
    m_defaultsBtn->setProperty("nereusSetupId", "test.twoToneImd.defaults");
    m_stealthBtn  = new QPushButton(QStringLiteral("Stealth"),  freqGroup);
    m_stealthBtn->setProperty("nereusSetupId", "test.twoToneImd.stealth");
    presetsRow->addWidget(m_defaultsBtn);
    presetsRow->addWidget(m_stealthBtn);
    presetsRow->addStretch(1);
    freqForm->addRow(QStringLiteral("Presets:"), presetsRow);

    // QGroupBox uses a QVBoxLayout from addSection() — append our form.
    if (auto* vlay = qobject_cast<QVBoxLayout*>(freqGroup->layout())) {
        vlay->addLayout(freqForm);
    }

    connect(m_freq1Spin,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this, &TestTwoTonePage::onFreq1Changed);
    connect(m_freq2Spin,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this, &TestTwoTonePage::onFreq2Changed);
    connect(m_defaultsBtn, &QPushButton::clicked,
            this, &TestTwoTonePage::onDefaultsClicked);
    connect(m_stealthBtn,  &QPushButton::clicked,
            this, &TestTwoTonePage::onStealthClicked);

    // ── Group 2: Output Level ─────────────────────────────────────────────────
    QGroupBox* levelGroup = addSection(QStringLiteral("Output Level"));

    auto* levelForm = new QFormLayout();
    levelForm->setContentsMargins(0, 0, 0, 0);

    m_levelSpin = new QDoubleSpinBox(levelGroup);
    m_levelSpin->setProperty("nereusSetupId", "test.twoToneImd.level");
    m_levelSpin->setRange(TransmitModel::kTwoToneLevelDbMin,
                          TransmitModel::kTwoToneLevelDbMax);
    m_levelSpin->setDecimals(3);
    m_levelSpin->setSingleStep(0.001);
    m_levelSpin->setSuffix(QStringLiteral(" dB"));
    levelForm->addRow(QStringLiteral("Level:"), m_levelSpin);

    m_powerSpin = new QSpinBox(levelGroup);
    m_powerSpin->setProperty("nereusSetupId", "test.twoToneImd.power");
    m_powerSpin->setRange(TransmitModel::kTwoTonePowerMin,
                          TransmitModel::kTwoTonePowerMax);
    m_powerSpin->setSingleStep(1);
    m_powerSpin->setSuffix(QStringLiteral(" %"));
    levelForm->addRow(QStringLiteral("Drive Power (Fixed mode):"), m_powerSpin);

    if (auto* vlay = qobject_cast<QVBoxLayout*>(levelGroup->layout())) {
        vlay->addLayout(levelForm);
    }

    connect(m_levelSpin,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &TestTwoTonePage::onLevelChanged);
    connect(m_powerSpin,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this, &TestTwoTonePage::onPowerChanged);

    // ── Group 3: Mode ─────────────────────────────────────────────────────────
    QGroupBox* modeGroup = addSection(QStringLiteral("Mode"));

    auto* modeForm = new QFormLayout();
    modeForm->setContentsMargins(0, 0, 0, 0);

    m_pulsedCheck = new QCheckBox(QStringLiteral("Pulsed two-tone"), modeGroup);
    m_pulsedCheck->setProperty("nereusSetupId", "test.twoToneImd.pulsed");
    modeForm->addRow(QString(), m_pulsedCheck);

    m_invertCheck = new QCheckBox(QStringLiteral("Invert for LS Modes"), modeGroup);
    m_invertCheck->setProperty("nereusSetupId", "test.twoToneImd.invert");
    // Verbatim Thetis tooltip — setup.Designer.cs:61971 [v2.10.3.13].
    m_invertCheck->setToolTip(
        QStringLiteral("Swap F1 and F2 for lower side band modes"));
    modeForm->addRow(QString(), m_invertCheck);

    m_freq2DelaySpin = new QSpinBox(modeGroup);
    m_freq2DelaySpin->setProperty("nereusSetupId", "test.twoToneImd.freq2Delay");
    m_freq2DelaySpin->setRange(TransmitModel::kTwoToneFreq2DelayMsMin,
                               TransmitModel::kTwoToneFreq2DelayMsMax);
    m_freq2DelaySpin->setSingleStep(1);
    m_freq2DelaySpin->setSuffix(QStringLiteral(" ms"));
    // Verbatim Thetis tooltip — setup.Designer.cs:61942 [v2.10.3.13].
    m_freq2DelaySpin->setToolTip(
        QStringLiteral("Applies a wait delay before Freq#2 is enabled"));
    modeForm->addRow(QStringLiteral("Freq #2 delay:"), m_freq2DelaySpin);

    if (auto* vlay = qobject_cast<QVBoxLayout*>(modeGroup->layout())) {
        vlay->addLayout(modeForm);
    }

    connect(m_pulsedCheck, &QCheckBox::toggled,
            this, &TestTwoTonePage::onPulsedToggled);
    connect(m_invertCheck, &QCheckBox::toggled,
            this, &TestTwoTonePage::onInvertToggled);
    connect(m_freq2DelaySpin,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this, &TestTwoTonePage::onFreq2DelayChanged);

    // ── Group 4: Drive Power Source ───────────────────────────────────────────
    QGroupBox* driveGroup = addSection(QStringLiteral("Drive Power Source"));

    auto* driveCol = new QVBoxLayout();
    driveCol->setContentsMargins(0, 0, 0, 0);

    m_driveSliderRadio = new QRadioButton(QStringLiteral("Drive Slider"), driveGroup);
    m_tuneSliderRadio  = new QRadioButton(QStringLiteral("Tune Slider"),  driveGroup);
    m_fixedDriveRadio  = new QRadioButton(QStringLiteral("Fixed"),        driveGroup);

    m_driveButtonGroup = new QButtonGroup(this);
    m_driveButtonGroup->setProperty("nereusSetupId", "test.twoToneImd.drivePowerSource");
    m_driveButtonGroup->addButton(m_driveSliderRadio,
                                  static_cast<int>(DrivePowerSource::DriveSlider));
    m_driveButtonGroup->addButton(m_tuneSliderRadio,
                                  static_cast<int>(DrivePowerSource::TuneSlider));
    m_driveButtonGroup->addButton(m_fixedDriveRadio,
                                  static_cast<int>(DrivePowerSource::Fixed));

    driveCol->addWidget(m_driveSliderRadio);
    driveCol->addWidget(m_tuneSliderRadio);
    driveCol->addWidget(m_fixedDriveRadio);

    if (auto* vlay = qobject_cast<QVBoxLayout*>(driveGroup->layout())) {
        vlay->addLayout(driveCol);
    }

    connect(m_driveButtonGroup, &QButtonGroup::idToggled,
            this, &TestTwoTonePage::onDrivePowerRadioToggled);
}

// ---------------------------------------------------------------------------
// Initial value seeding from TransmitModel
// ---------------------------------------------------------------------------

void TestTwoTonePage::seedFromModel()
{
    if (!model()) { return; }
    const TransmitModel* tx = &model()->transmitModel();

    // Use QSignalBlocker on each control to avoid round-trip-back-into-model.
    {
        QSignalBlocker b1(m_freq1Spin);
        QSignalBlocker b2(m_freq2Spin);
        QSignalBlocker b3(m_levelSpin);
        QSignalBlocker b4(m_powerSpin);
        QSignalBlocker b5(m_freq2DelaySpin);
        QSignalBlocker b6(m_invertCheck);
        QSignalBlocker b7(m_pulsedCheck);

        m_freq1Spin->setValue(tx->twoToneFreq1());
        m_freq2Spin->setValue(tx->twoToneFreq2());
        m_levelSpin->setValue(tx->twoToneLevel());
        m_powerSpin->setValue(tx->twoTonePower());
        m_freq2DelaySpin->setValue(tx->twoToneFreq2Delay());
        m_invertCheck->setChecked(tx->twoToneInvert());
        m_pulsedCheck->setChecked(tx->twoTonePulsed());
    }

    // Drive-power radio: pick the matching button.
    selectDriveRadioFor(tx->twoToneDrivePowerSource());
}

void TestTwoTonePage::selectDriveRadioFor(DrivePowerSource source)
{
    QRadioButton* target = m_driveSliderRadio;
    switch (source) {
        case DrivePowerSource::DriveSlider: target = m_driveSliderRadio; break;
        case DrivePowerSource::TuneSlider:  target = m_tuneSliderRadio;  break;
        case DrivePowerSource::Fixed:       target = m_fixedDriveRadio;  break;
    }
    if (!target) { return; }

    // QSignalBlocker on the button group to suppress the idToggled echo
    // that would otherwise call back into the model.
    QSignalBlocker blk(m_driveButtonGroup);
    target->setChecked(true);
}

// ---------------------------------------------------------------------------
// Model → UI signal wiring
// ---------------------------------------------------------------------------

void TestTwoTonePage::wireModelSignals()
{
    if (!model()) { return; }
    TransmitModel* tx = &model()->transmitModel();

    connect(tx, &TransmitModel::twoToneFreq1Changed,
            this, &TestTwoTonePage::onModelFreq1Changed);
    connect(tx, &TransmitModel::twoToneFreq2Changed,
            this, &TestTwoTonePage::onModelFreq2Changed);
    connect(tx, &TransmitModel::twoToneLevelChanged,
            this, &TestTwoTonePage::onModelLevelChanged);
    connect(tx, &TransmitModel::twoTonePowerChanged,
            this, &TestTwoTonePage::onModelPowerChanged);
    connect(tx, &TransmitModel::twoToneFreq2DelayChanged,
            this, &TestTwoTonePage::onModelFreq2DelayChanged);
    connect(tx, &TransmitModel::twoToneInvertChanged,
            this, &TestTwoTonePage::onModelInvertChanged);
    connect(tx, &TransmitModel::twoTonePulsedChanged,
            this, &TestTwoTonePage::onModelPulsedChanged);
    connect(tx, &TransmitModel::twoToneDrivePowerSourceChanged,
            this, &TestTwoTonePage::onModelDrivePowerSourceChanged);
}

// ---------------------------------------------------------------------------
// UI → Model slot handlers
// ---------------------------------------------------------------------------

void TestTwoTonePage::onFreq1Changed(int hz)
{
    if (model()) { model()->transmitModel().setTwoToneFreq1(hz); }
}

void TestTwoTonePage::onFreq2Changed(int hz)
{
    if (model()) { model()->transmitModel().setTwoToneFreq2(hz); }
}

void TestTwoTonePage::onLevelChanged(double db)
{
    if (model()) { model()->transmitModel().setTwoToneLevel(db); }
}

void TestTwoTonePage::onPowerChanged(int pct)
{
    if (model()) { model()->transmitModel().setTwoTonePower(pct); }
}

void TestTwoTonePage::onFreq2DelayChanged(int ms)
{
    if (model()) { model()->transmitModel().setTwoToneFreq2Delay(ms); }
}

void TestTwoTonePage::onInvertToggled(bool on)
{
    if (model()) { model()->transmitModel().setTwoToneInvert(on); }
}

void TestTwoTonePage::onPulsedToggled(bool on)
{
    if (model()) { model()->transmitModel().setTwoTonePulsed(on); }
}

void TestTwoTonePage::onDrivePowerRadioToggled(int id, bool checked)
{
    if (!checked) { return; }   // fire only on the newly-selected button
    if (!model()) { return; }
    const auto source = static_cast<DrivePowerSource>(id);
    model()->transmitModel().setTwoToneDrivePowerSource(source);
}

// ---------------------------------------------------------------------------
// Preset button handlers — match Thetis btnTwoToneF_defaults_Click /
// btnTwoToneF_stealth_Click (setup.cs:34224-34234 [v2.10.3.13]):
// only Freq1/Freq2 are touched; level / power / delay / mode flags / drive
// source are left intact.
// ---------------------------------------------------------------------------

void TestTwoTonePage::onDefaultsClicked()
{
    if (!model()) { return; }
    auto& tx = model()->transmitModel();
    tx.setTwoToneFreq1(700);
    tx.setTwoToneFreq2(1900);
}

void TestTwoTonePage::onStealthClicked()
{
    if (!model()) { return; }
    auto& tx = model()->transmitModel();
    tx.setTwoToneFreq1(70);
    tx.setTwoToneFreq2(190);
}

// ---------------------------------------------------------------------------
// Model → UI slot handlers (QSignalBlocker prevents feedback loops).
// ---------------------------------------------------------------------------

void TestTwoTonePage::onModelFreq1Changed(int hz)
{
    if (!m_freq1Spin) { return; }
    QSignalBlocker blk(m_freq1Spin);
    m_freq1Spin->setValue(hz);
}

void TestTwoTonePage::onModelFreq2Changed(int hz)
{
    if (!m_freq2Spin) { return; }
    QSignalBlocker blk(m_freq2Spin);
    m_freq2Spin->setValue(hz);
}

void TestTwoTonePage::onModelLevelChanged(double db)
{
    if (!m_levelSpin) { return; }
    QSignalBlocker blk(m_levelSpin);
    m_levelSpin->setValue(db);
}

void TestTwoTonePage::onModelPowerChanged(int pct)
{
    if (!m_powerSpin) { return; }
    QSignalBlocker blk(m_powerSpin);
    m_powerSpin->setValue(pct);
}

void TestTwoTonePage::onModelFreq2DelayChanged(int ms)
{
    if (!m_freq2DelaySpin) { return; }
    QSignalBlocker blk(m_freq2DelaySpin);
    m_freq2DelaySpin->setValue(ms);
}

void TestTwoTonePage::onModelInvertChanged(bool on)
{
    if (!m_invertCheck) { return; }
    QSignalBlocker blk(m_invertCheck);
    m_invertCheck->setChecked(on);
}

void TestTwoTonePage::onModelPulsedChanged(bool on)
{
    if (!m_pulsedCheck) { return; }
    QSignalBlocker blk(m_pulsedCheck);
    m_pulsedCheck->setChecked(on);
}

void TestTwoTonePage::onModelDrivePowerSourceChanged(DrivePowerSource source)
{
    selectDriveRadioFor(source);
}

} // namespace NereusSDR
