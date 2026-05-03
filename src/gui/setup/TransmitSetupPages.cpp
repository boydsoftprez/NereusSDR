// =================================================================
// src/gui/setup/TransmitSetupPages.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/setup.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-04-26 — Phase 3M-1a H.4: Power & PA page activation: Max Power
//                 [page renamed to "Power" 2026-05-02 in IA reshape Phase 6;
//                  PA-specific groups now live under the top-level PA category]
//                 slider wired to TransmitModel::setPower; per-band tune-
//                 power spinboxes wired to TransmitModel::setTunePowerForBand;
//                 ATTOnTX checkbox wired to StepAttenuatorController::setAttOnTxEnabled;
//                 ForceATTwhenPSAoff wired to StepAttenuatorController::setForceAttWhenPsOff.
//   2026-04-29 — Phase 3M-3a-i Batch 5 (Task E): SpeechProcessorPage rewrite
//                 as a NereusSDR-spin TX dashboard — strips the Compressor /
//                 Phase Rotator / CFC NYI stubs (those controls live on
//                 Setup → DSP → CFC and Setup → DSP → AGC/ALC per the
//                 IA decision) and replaces them with an Active Profile
//                 row (read-only label + Manage… button), a Stage Status
//                 grid (one row per WDSP TXA stage with a coloured dot
//                 + state caption + cross-link button) and a Quick Notes
//                 block.  TX EQ + Leveler labels update live from
//                 TransmitModel signals; the active-profile label tracks
//                 MicProfileManager::activeProfileChanged.  Cross-links
//                 emit openSetupRequested(category, page) which MainWindow
//                 routes via SetupDialog::selectPage() (see
//                 MainWindow.cpp:openSetupRequested handler).
//   2026-04-30 — Phase 3M-3a-ii Batch 5: SpeechProcessorPage Phrot / CFC /
//                 CESSB status rows promoted from "future phase"
//                 placeholders to live bindings on the new TransmitModel
//                 signals (phaseRotatorEnabledChanged, phaseRotatorFreqHzChanged,
//                 phaseRotatorStagesChanged, cfcEnabledChanged, cessbOnChanged,
//                 cpdrOnChanged).  Cross-link buttons re-pointed to "Open
//                 Phase Rotator…", "Open CFC…", "Open CESSB…" — all three
//                 land on the co-located CFC Setup page (matches Thetis
//                 tpDSPCFC tab IA).  TxProfilesPage::Compression section
//                 removed (orphan placeholder, master-design meta rule
//                 §2.2.1 violation).
// =================================================================

//=================================================================
// setup.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Continual modifications Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//=================================================================
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

#include "TransmitSetupPages.h"
#include "gui/StyleConstants.h"
#include "core/AppSettings.h"
#include "core/MicProfileManager.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"
#include "models/Band.h"
#include "core/StepAttenuatorController.h"
#include "gui/applets/TxEqDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSlider>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>

namespace NereusSDR {

// ---------------------------------------------------------------------------
// PowerPage
// ---------------------------------------------------------------------------

PowerPage::PowerPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("Power"), model, parent)
{
    buildUI();
}

void PowerPage::buildUI()
{
    NereusSDR::Style::applyDarkPageStyle(this);

    buildPowerGroup();
    buildTunePowerGroup();
    buildSwrProtectionGroup();
    buildExternalTxInhibitGroup();
    buildBlockTxAntennaGroup();
    buildHfPaGroup();

    // TODO(future): Thetis Transmit tab also has these groups not yet
    // covered by NereusSDR. Tracked separately from this PR; pre-existing
    // gaps, not introduced by the IA reshape.
    //   - grpPATune (TUN power mode: fixed/drive-slider/tune-slider radio
    //                + comboTXTUNMeter selector)
    //   - chkPulsedTune + grpPulsedTune (pulsed tune mode)
    //   - chkRecoverPAProfileFromTXProfile (TX↔PA profile linkage)
    //   - chkLimitExtAmpOnOverload (external amp overload limiter)
    //   - udTXFilterLowSave / udTXFilterHighSave (saved TX filter values)
    //   - grpTXAM (AM carrier level — udTXAMCarrierLevel)
    //   - grpTXMonitor (TX audio monitor level — udTXAF)
    //   - grpTXFilter (TX filter cutoff)
    //   - chkTXExpert (Expert mode unlock)
    // Source: Thetis setup.designer.cs:46313-46339 tpTransmit [v2.10.3.13]

    contentLayout()->addStretch();
}

// ---------------------------------------------------------------------------
// PowerPage::buildPowerGroup — H.4
// ---------------------------------------------------------------------------
//
// Wires:
//  1. Max Power slider → TransmitModel::setPower(int)
//     From Thetis console.cs:4822 [v2.10.3.13] TXF power setter.
//  2. chkATTOnTX → StepAttenuatorController::setAttOnTxEnabled(bool)
//     From Thetis setup.designer.cs:5926-5939 [v2.10.3.13] + setup.cs:15452-15455.
//     NereusSDR places this on the Power page (tpAlexAntCtrl in Thetis).
//  3. chkForceATTwhenPSAoff → StepAttenuatorController::setForceAttWhenPsOff(bool)
//     From Thetis setup.designer.cs:5660-5671 [v2.10.3.13] + setup.cs:24264-24268.
//     //MW0LGE [2.9.0.7] added  [original inline comment from console.cs:29285]
void PowerPage::buildPowerGroup()
{
    auto* pwrGroup = new QGroupBox(QStringLiteral("Power"), this);
    auto* pwrForm  = new QFormLayout(pwrGroup);
    pwrForm->setSpacing(6);

    // Max Power slider — wired to TransmitModel::setPower (H.4).
    // From Thetis console.cs:4822 [v2.10.3.13]:
    //   public int PWR { set { ... } }  — overall TX drive/power level.
    m_maxPowerSlider = new QSlider(Qt::Horizontal, pwrGroup);
    m_maxPowerSlider->setRange(0, 100);
    m_maxPowerSlider->setValue(100);
    m_maxPowerSlider->setEnabled(true);   // Phase 3M-1a H.4: wired
    m_maxPowerSlider->setObjectName(QStringLiteral("maxPowerSlider"));
    m_maxPowerSlider->setToolTip(QStringLiteral("RF output power (0–100 W)"));

    if (model()) {
        TransmitModel& tx = model()->transmitModel();

        // Initialise from model
        {
            QSignalBlocker b(m_maxPowerSlider);
            m_maxPowerSlider->setValue(tx.power());
        }

        // Slider → model
        connect(m_maxPowerSlider, &QSlider::valueChanged,
                &tx, &TransmitModel::setPower);

        // Model → slider (reverse)
        connect(&tx, &TransmitModel::powerChanged, m_maxPowerSlider,
                [this](int val) {
                    QSignalBlocker b(m_maxPowerSlider);
                    m_maxPowerSlider->setValue(val);
                });
    }
    pwrForm->addRow(QStringLiteral("Max Power (W):"), m_maxPowerSlider);

    // SWR protection threshold — future use.
    m_swrProtectionSlider = new QSlider(Qt::Horizontal, pwrGroup);
    m_swrProtectionSlider->setRange(10, 50);   // SWR * 10 for integer slider
    m_swrProtectionSlider->setValue(30);        // 3.0:1
    m_swrProtectionSlider->setEnabled(false);   // NYI
    m_swrProtectionSlider->setToolTip(QStringLiteral("SWR protection threshold — not yet implemented"));
    pwrForm->addRow(QStringLiteral("SWR Protection:"), m_swrProtectionSlider);

    // chkATTOnTX — "Enables Attenuator on Mercury during Transmit."
    // From Thetis setup.designer.cs:5935 [v2.10.3.13]:
    //   toolTip1.SetToolTip(chkATTOnTX, "Enables Attenuator on Mercury during Transmit.")
    m_chkAttOnTx = new QCheckBox(QStringLiteral("ATT on TX"), pwrGroup);
    m_chkAttOnTx->setObjectName(QStringLiteral("chkATTOnTX"));
    m_chkAttOnTx->setToolTip(QStringLiteral("Enables Attenuator on Mercury during Transmit."));

    if (model()) {
        if (StepAttenuatorController* att = model()->stepAttController()) {
            m_chkAttOnTx->setChecked(att->attOnTxEnabled());
            connect(m_chkAttOnTx, &QCheckBox::toggled,
                    att, &StepAttenuatorController::setAttOnTxEnabled);
        }
    }
    pwrForm->addRow(QString(), m_chkAttOnTx);

    // chkForceATTwhenPSAoff — "Force ATT on Tx to 31 when PS-A is off"
    // From Thetis setup.designer.cs:5668 [v2.10.3.13]:
    //   chkForceATTwhenPSAoff.Text = "Force ATT on Tx to 31 when PS-A is off"
    //   toolTip1.SetToolTip(chkForceATTwhenPSAoff, "Forces ATT on Tx to 31 when PS-A is off. CW will do this anyway")
    // //MW0LGE [2.9.0.7] added  [original inline comment from console.cs:29285]
    m_chkForceAttWhenPsOff = new QCheckBox(
        QStringLiteral("Force ATT on Tx to 31 when PS-A is off"), pwrGroup);
    m_chkForceAttWhenPsOff->setObjectName(QStringLiteral("chkForceATTwhenPSAoff"));
    m_chkForceAttWhenPsOff->setToolTip(
        QStringLiteral("Forces ATT on Tx to 31 when PS-A is off. CW will do this anyway"));

    if (model()) {
        if (StepAttenuatorController* att = model()->stepAttController()) {
            m_chkForceAttWhenPsOff->setChecked(att->forceAttWhenPsOff());
            connect(m_chkForceAttWhenPsOff, &QCheckBox::toggled,
                    att, &StepAttenuatorController::setForceAttWhenPsOff);
        }
    }
    pwrForm->addRow(QString(), m_chkForceAttWhenPsOff);

    contentLayout()->addWidget(pwrGroup);
}

// ---------------------------------------------------------------------------
// PowerPage::buildTunePowerGroup — H.4
// ---------------------------------------------------------------------------
//
// Per-band tune-power spinboxes.
// NereusSDR extension: exposes tunePower_by_band[14] (console.cs:12094 [v2.10.3.13])
// directly in the setup dialog so the operator can set per-band tune power without
// having to visit each band from the main VFO.  Thetis uses a single udTXTunePower
// (setup.cs:5262 [v2.10.3.13]) that updates one slot on band change.
void PowerPage::buildTunePowerGroup()
{
    auto* group  = new QGroupBox(QStringLiteral("Tune Power (per band)"), this);
    auto* grid   = new QGridLayout(group);
    grid->setSpacing(4);

    // Column headers
    grid->addWidget(new QLabel(QStringLiteral("Band"), group), 0, 0);
    grid->addWidget(new QLabel(QStringLiteral("Watts"), group), 0, 1);

    for (int i = 0; i < kBandCount; ++i) {
        const Band band = static_cast<Band>(i);
        const QString label = bandLabel(band);

        auto* lbl  = new QLabel(label, group);
        auto* spin = new QSpinBox(group);
        spin->setRange(0, 100);
        spin->setSuffix(QStringLiteral(" W"));
        spin->setObjectName(QStringLiteral("spinTunePwr_") + label);
        spin->setToolTip(QStringLiteral("Tune carrier power for %1 (0–100 W)").arg(label));
        m_tunePwrSpins[i] = spin;

        grid->addWidget(lbl,  i + 1, 0);
        grid->addWidget(spin, i + 1, 1);

        if (model()) {
            TransmitModel& tx = model()->transmitModel();

            // Initialise from model
            {
                QSignalBlocker b(spin);
                spin->setValue(tx.tunePowerForBand(band));
            }

            // Spinbox → model
            connect(spin, &QSpinBox::valueChanged, this, [this, band](int val) {
                model()->transmitModel().setTunePowerForBand(band, val);
            });

            // Model → spinbox (reverse — needed when TxApplet slider changes the value)
            connect(&tx, &TransmitModel::tunePowerByBandChanged,
                    spin, [band, spin](Band changedBand, int watts) {
                if (changedBand != band) { return; }
                QSignalBlocker b(spin);
                spin->setValue(watts);
            });
        }
    }

    contentLayout()->addWidget(group);
}

// ---------------------------------------------------------------------------
// PowerPage helpers (Tasks 9-11)
// ---------------------------------------------------------------------------

// Task 9 — SWR Protection
// From Thetis setup.designer.cs:5793-5924 [v2.10.3.13]
void PowerPage::buildSwrProtectionGroup()
{
    auto& s = AppSettings::instance();

    auto* group = new QGroupBox(tr("SWR Protection"), this);
    group->setObjectName(QStringLiteral("grpSWRProtectionControl"));
    auto* layout = new QFormLayout(group);
    layout->setSpacing(6);

    // chkSWRProtection — From Thetis setup.designer.cs:5913-5924 [v2.10.3.13]
    m_chkSWRProtection = new QCheckBox(tr("Enable Protection SWR >"), group);
    m_chkSWRProtection->setObjectName(QStringLiteral("chkSWRProtection"));
    // From Thetis setup.designer.cs:5922 [v2.10.3.13]
    m_chkSWRProtection->setToolTip(tr("Show a visual SWR warning in the spectral area"));
    m_chkSWRProtection->setChecked(
        s.value(QStringLiteral("SwrProtectionEnabled"), QStringLiteral("False")).toString() == QStringLiteral("True"));
    connect(m_chkSWRProtection, &QCheckBox::toggled, this, [](bool on) {
        AppSettings::instance().setValue(QStringLiteral("SwrProtectionEnabled"), on ? QStringLiteral("True") : QStringLiteral("False"));
    });
    layout->addRow(QString(), m_chkSWRProtection);

    // udSwrProtectionLimit — From Thetis setup.designer.cs:5832-5860 [v2.10.3.13]
    // Min=1.0, Max=5.0, Increment=0.1, DecimalPlaces=1, Default=2.0 (Value=20, 65536→one decimal)
    m_udSwrProtectionLimit = new QDoubleSpinBox(group);
    m_udSwrProtectionLimit->setObjectName(QStringLiteral("udSwrProtectionLimit"));
    m_udSwrProtectionLimit->setRange(1.0, 5.0);
    m_udSwrProtectionLimit->setSingleStep(0.1);
    m_udSwrProtectionLimit->setDecimals(1);
    m_udSwrProtectionLimit->setValue(
        s.value(QStringLiteral("SwrProtectionLimit"), QStringLiteral("2.0")).toDouble());
    connect(m_udSwrProtectionLimit, &QDoubleSpinBox::valueChanged, this, [](double v) {
        AppSettings::instance().setValue(QStringLiteral("SwrProtectionLimit"), QString::number(v, 'f', 1));
    });
    layout->addRow(tr("SWR Limit:"), m_udSwrProtectionLimit);

    // chkSWRTuneProtection — From Thetis setup.designer.cs:5901-5911 [v2.10.3.13]
    m_chkSWRTuneProtection = new QCheckBox(tr("Ignore when Tune Pwr <"), group);
    m_chkSWRTuneProtection->setObjectName(QStringLiteral("chkSWRTuneProtection"));
    // From Thetis setup.designer.cs:5909 [v2.10.3.13]
    m_chkSWRTuneProtection->setToolTip(tr("Disables SWR Protection during Tune."));
    m_chkSWRTuneProtection->setChecked(
        s.value(QStringLiteral("SwrTuneProtectionEnabled"), QStringLiteral("False")).toString() == QStringLiteral("True"));
    connect(m_chkSWRTuneProtection, &QCheckBox::toggled, this, [](bool on) {
        AppSettings::instance().setValue(QStringLiteral("SwrTuneProtectionEnabled"), on ? QStringLiteral("True") : QStringLiteral("False"));
    });
    layout->addRow(QString(), m_chkSWRTuneProtection);

    // udTunePowerSwrIgnore — From Thetis setup.designer.cs:5872-5899 [v2.10.3.13]
    // Min=5, Max=50, Increment=1, Default=35
    m_udTunePowerSwrIgnore = new QSpinBox(group);
    m_udTunePowerSwrIgnore->setObjectName(QStringLiteral("udTunePowerSwrIgnore"));
    m_udTunePowerSwrIgnore->setRange(5, 50);
    m_udTunePowerSwrIgnore->setSingleStep(1);
    m_udTunePowerSwrIgnore->setValue(
        s.value(QStringLiteral("TunePowerSwrIgnore"), QStringLiteral("35")).toInt());
    connect(m_udTunePowerSwrIgnore, &QSpinBox::valueChanged, this, [](int v) {
        AppSettings::instance().setValue(QStringLiteral("TunePowerSwrIgnore"), QString::number(v));
    });
    layout->addRow(tr("Tune Pwr (W):"), m_udTunePowerSwrIgnore);

    // chkWindBackPowerSWR — From Thetis setup.designer.cs:5809-5820 [v2.10.3.13]
    m_chkWindBackPowerSWR = new QCheckBox(tr("Reduce Pwr if protected"), group);
    m_chkWindBackPowerSWR->setObjectName(QStringLiteral("chkWindBackPowerSWR"));
    // From Thetis setup.designer.cs:5818 [v2.10.3.13]
    m_chkWindBackPowerSWR->setToolTip(tr("Winds back the power if high swr protection kicks in"));
    m_chkWindBackPowerSWR->setChecked(
        s.value(QStringLiteral("WindBackPowerSwr"), QStringLiteral("False")).toString() == QStringLiteral("True"));
    connect(m_chkWindBackPowerSWR, &QCheckBox::toggled, this, [](bool on) {
        AppSettings::instance().setValue(QStringLiteral("WindBackPowerSwr"), on ? QStringLiteral("True") : QStringLiteral("False"));
    });
    layout->addRow(QString(), m_chkWindBackPowerSWR);

    contentLayout()->addWidget(group);
}

// Task 10 — External TX Inhibit
// From Thetis setup.designer.cs:46626-46657 [v2.10.3.13]
void PowerPage::buildExternalTxInhibitGroup()
{
    auto& s = AppSettings::instance();

    auto* group = new QGroupBox(tr("External TX Inhibit"), this);
    group->setObjectName(QStringLiteral("grpExtTXInhibit"));
    auto* layout = new QVBoxLayout(group);
    layout->setSpacing(6);

    // chkTXInhibit — From Thetis setup.designer.cs:46637-46646 [v2.10.3.13]
    m_chkTXInhibit = new QCheckBox(tr("Update with TX Inhibit state"), group);
    m_chkTXInhibit->setObjectName(QStringLiteral("chkTXInhibit"));
    // From Thetis setup.designer.cs:46645 [v2.10.3.13]
    m_chkTXInhibit->setToolTip(tr("Thetis will update on TX inhibit state change"));
    m_chkTXInhibit->setChecked(
        s.value(QStringLiteral("TxInhibitMonitorEnabled"), QStringLiteral("False")).toString() == QStringLiteral("True"));
    connect(m_chkTXInhibit, &QCheckBox::toggled, this, [](bool on) {
        AppSettings::instance().setValue(QStringLiteral("TxInhibitMonitorEnabled"), on ? QStringLiteral("True") : QStringLiteral("False"));
    });
    layout->addWidget(m_chkTXInhibit);

    // chkTXInhibitReverse — From Thetis setup.designer.cs:46648-46657 [v2.10.3.13]
    m_chkTXInhibitReverse = new QCheckBox(tr("Reversed logic"), group);
    m_chkTXInhibitReverse->setObjectName(QStringLiteral("chkTXInhibitReverse"));
    // From Thetis setup.designer.cs:46656 [v2.10.3.13]
    m_chkTXInhibitReverse->setToolTip(tr("Reverse the input state logic"));
    m_chkTXInhibitReverse->setChecked(
        s.value(QStringLiteral("TxInhibitMonitorReversed"), QStringLiteral("False")).toString() == QStringLiteral("True"));
    connect(m_chkTXInhibitReverse, &QCheckBox::toggled, this, [](bool on) {
        AppSettings::instance().setValue(QStringLiteral("TxInhibitMonitorReversed"), on ? QStringLiteral("True") : QStringLiteral("False"));
    });
    layout->addWidget(m_chkTXInhibitReverse);

    contentLayout()->addWidget(group);
}

// Task 11a — Block TX on RX antennas
// NereusSDR-original label and tooltip — Thetis ships these as unlabelled
// column-header checkboxes per setup.designer.cs:6704-6724 [v2.10.3.13];
// we add accessible copy. AppSettings keys mirror AlexController (Task 3P-F).
void PowerPage::buildBlockTxAntennaGroup()
{
    auto& s = AppSettings::instance();

    auto* group = new QGroupBox(tr("Block TX on RX antennas"), this);
    group->setObjectName(QStringLiteral("grpBlockTxAntennas"));
    auto* layout = new QVBoxLayout(group);
    layout->setSpacing(6);

    // chkBlockTxAnt2 — From Thetis setup.designer.cs:6715-6724 [v2.10.3.13]
    // NereusSDR-original label/tooltip (Thetis has no text on these checkboxes)
    m_chkBlockTxAnt2 = new QCheckBox(tr("Block TX on Ant 2"), group);
    m_chkBlockTxAnt2->setObjectName(QStringLiteral("chkBlockTxAnt2"));
    m_chkBlockTxAnt2->setToolTip(tr("When checked, the radio cannot transmit on Antenna 2"));
    m_chkBlockTxAnt2->setChecked(
        s.value(QStringLiteral("AlexAnt2RxOnly"), QStringLiteral("False")).toString() == QStringLiteral("True"));
    connect(m_chkBlockTxAnt2, &QCheckBox::toggled, this, [](bool on) {
        AppSettings::instance().setValue(QStringLiteral("AlexAnt2RxOnly"), on ? QStringLiteral("True") : QStringLiteral("False"));
    });
    layout->addWidget(m_chkBlockTxAnt2);

    // chkBlockTxAnt3 — From Thetis setup.designer.cs:6704-6713 [v2.10.3.13]
    // NereusSDR-original label/tooltip (Thetis has no text on these checkboxes)
    m_chkBlockTxAnt3 = new QCheckBox(tr("Block TX on Ant 3"), group);
    m_chkBlockTxAnt3->setObjectName(QStringLiteral("chkBlockTxAnt3"));
    m_chkBlockTxAnt3->setToolTip(tr("When checked, the radio cannot transmit on Antenna 3"));
    m_chkBlockTxAnt3->setChecked(
        s.value(QStringLiteral("AlexAnt3RxOnly"), QStringLiteral("False")).toString() == QStringLiteral("True"));
    connect(m_chkBlockTxAnt3, &QCheckBox::toggled, this, [](bool on) {
        AppSettings::instance().setValue(QStringLiteral("AlexAnt3RxOnly"), on ? QStringLiteral("True") : QStringLiteral("False"));
    });
    layout->addWidget(m_chkBlockTxAnt3);

    contentLayout()->addWidget(group);
}

// Task 11b — PA Control (Disable HF PA)
// From Thetis setup.designer.cs:5780-5791 [v2.10.3.13]
void PowerPage::buildHfPaGroup()
{
    auto& s = AppSettings::instance();

    auto* group = new QGroupBox(tr("PA Control"), this);
    group->setObjectName(QStringLiteral("grpHfPaControl"));
    auto* layout = new QVBoxLayout(group);
    layout->setSpacing(6);

    // chkHFTRRelay — From Thetis setup.designer.cs:5780-5791 [v2.10.3.13]
    m_chkHFTRRelay = new QCheckBox(tr("Disable HF PA"), group);
    m_chkHFTRRelay->setObjectName(QStringLiteral("chkHFTRRelay"));
    // From Thetis setup.designer.cs:5789 [v2.10.3.13]
    m_chkHFTRRelay->setToolTip(tr("Disables HF PA."));
    m_chkHFTRRelay->setChecked(
        s.value(QStringLiteral("DisableHfPa"), QStringLiteral("False")).toString() == QStringLiteral("True"));
    connect(m_chkHFTRRelay, &QCheckBox::toggled, this, [](bool on) {
        AppSettings::instance().setValue(QStringLiteral("DisableHfPa"), on ? QStringLiteral("True") : QStringLiteral("False"));
    });
    layout->addWidget(m_chkHFTRRelay);

    contentLayout()->addWidget(group);
}

// ---------------------------------------------------------------------------
// TxProfilesPage
// ---------------------------------------------------------------------------

TxProfilesPage::TxProfilesPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("TX Profiles"), model, parent)
{
    buildUI();
}

void TxProfilesPage::buildUI()
{
    NereusSDR::Style::applyDarkPageStyle(this);

    // --- Section: Profile ---
    auto* profGroup = new QGroupBox(QStringLiteral("Profile"), this);
    auto* profLayout = new QVBoxLayout(profGroup);
    profLayout->setSpacing(6);

    m_profileListLabel = new QLabel(QStringLiteral("(Profile list — not yet implemented)"), profGroup);
    m_profileListLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #607080; font-style: italic; "
        " background: #1a2a3a; border: 1px solid #203040; "
        " border-radius: 3px; padding: 8px; }"));
    m_profileListLabel->setMinimumHeight(80);
    m_profileListLabel->setEnabled(false);
    m_profileListLabel->setAlignment(Qt::AlignCenter);
    profLayout->addWidget(m_profileListLabel);

    auto* nameRow = new QHBoxLayout();
    nameRow->addWidget(new QLabel(QStringLiteral("Name:"), profGroup));
    m_nameEdit = new QLineEdit(profGroup);
    m_nameEdit->setPlaceholderText(QStringLiteral("Profile name"));
    m_nameEdit->setEnabled(false);  // NYI
    m_nameEdit->setToolTip(QStringLiteral("TX profile name — not yet implemented"));
    nameRow->addWidget(m_nameEdit, 1);
    profLayout->addLayout(nameRow);

    auto* btnRow = new QHBoxLayout();
    m_newBtn = new QPushButton(QStringLiteral("New"), profGroup);
    m_newBtn->setEnabled(false);  // NYI
    m_newBtn->setToolTip(QStringLiteral("New TX profile — not yet implemented"));
    m_newBtn->setAutoDefault(false);
    btnRow->addWidget(m_newBtn);

    m_deleteBtn = new QPushButton(QStringLiteral("Delete"), profGroup);
    m_deleteBtn->setEnabled(false);  // NYI
    m_deleteBtn->setToolTip(QStringLiteral("Delete TX profile — not yet implemented"));
    m_deleteBtn->setAutoDefault(false);
    btnRow->addWidget(m_deleteBtn);

    m_copyBtn = new QPushButton(QStringLiteral("Copy"), profGroup);
    m_copyBtn->setEnabled(false);  // NYI
    m_copyBtn->setToolTip(QStringLiteral("Copy TX profile — not yet implemented"));
    m_copyBtn->setAutoDefault(false);
    btnRow->addWidget(m_copyBtn);

    btnRow->addStretch();
    profLayout->addLayout(btnRow);

    contentLayout()->addWidget(profGroup);

    // ── Phase 3M-3a-ii Batch 5: Compression section removed.
    // CPDR enable + gain + CESSB live on Setup → DSP → CFC (Batch 5) and
    // on the TxApplet [PROC] quick-toggle / dashboard.  The previous
    // disabled-stub trio violated master-design meta rule §2.2.1 (no
    // half-shipped placeholders).

    contentLayout()->addStretch();
}

// ---------------------------------------------------------------------------
// SpeechProcessorPage — TX dashboard (NereusSDR-spin)
//
// No direct Thetis equivalent.  This page summarises every WDSP TXA speech-
// chain stage (txa[ch] members in `wdsp/TXA.c:create_txa` [v2.10.3.13]:
// eqp / leveler / cfcomp+cfir / compressor / phrot / amsq / alc) and offers
// fast jumps to the per-stage Setup pages where each is configured.
// ---------------------------------------------------------------------------

namespace {

// Filled / hollow circle Unicode characters used for the stage status dot.
constexpr QChar kFilledCircle = QChar(0x25CF);  // ●
constexpr QChar kHollowCircle = QChar(0x25CB);  // ○

// Coloured stylesheet applied to the dot QLabel based on its state.
//   on   → bright green (#20e070)
//   off  → muted grey  (#607080)
//   alc  → cyan         (#00b4d8)  for the "always-on" callout
QString dotStyleFor(bool on)
{
    const QString colour = on ? QStringLiteral("#20e070") : QStringLiteral("#607080");
    return QStringLiteral("QLabel { color: %1; font-size: 14px; font-weight: bold; }").arg(colour);
}

}  // namespace

SpeechProcessorPage::SpeechProcessorPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("Speech Processor"), model, parent)
{
    buildUI();
}

void SpeechProcessorPage::buildUI()
{
    NereusSDR::Style::applyDarkPageStyle(this);

    buildActiveProfileSection();
    buildStageStatusSection();
    buildQuickNotesSection();
}

// ---------------------------------------------------------------------------
// SpeechProcessorPage::buildActiveProfileSection
//
// Single read-only label showing MicProfileManager::activeProfileName(), with
// a "Manage…" button that opens TxEqDialog (which hosts the profile combo +
// Save / Save As / Delete buttons added in 3M-3a-i Batch 4).  Without a
// connected radio MicProfileManager is unscoped and returns "Default" — the
// label still reads meaningfully.
// ---------------------------------------------------------------------------
void SpeechProcessorPage::buildActiveProfileSection()
{
    auto* group = addSection(QStringLiteral("Active Profile"));
    group->setObjectName(QStringLiteral("grpSpeechActiveProfile"));

    auto* row = new QHBoxLayout;
    row->setSpacing(8);

    auto* nameLabel = new QLabel(QStringLiteral("Profile:"));
    nameLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #c8d8e8; font-size: 12px; }"));
    nameLabel->setFixedWidth(150);

    m_activeProfileLabel = new QLabel(QStringLiteral("Default"));
    m_activeProfileLabel->setObjectName(QStringLiteral("lblActiveProfile"));
    m_activeProfileLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #00c8ff; font-size: 12px; font-weight: bold; }"));

    m_manageProfileBtn = new QPushButton(QStringLiteral("Manage..."));
    m_manageProfileBtn->setObjectName(QStringLiteral("btnManageProfile"));
    m_manageProfileBtn->setAutoDefault(false);
    m_manageProfileBtn->setToolTip(QStringLiteral(
        "Open the TX EQ editor (Tools → TX Equalizer) — the profile combo "
        "and Save / Save As / Delete buttons live there."));
    m_manageProfileBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: #1a2a3a; border: 1px solid #304050;"
        "  border-radius: 3px; color: #c8d8e8; font-size: 12px; padding: 3px 10px; }"
        "QPushButton:hover { background: #203040; }"
        "QPushButton:pressed { background: #00b4d8; color: #0f0f1a; }"));

    row->addWidget(nameLabel);
    row->addWidget(m_activeProfileLabel, 1);
    row->addWidget(m_manageProfileBtn);

    auto* groupLayout = qobject_cast<QVBoxLayout*>(group->layout());
    if (groupLayout) {
        groupLayout->addLayout(row);
    }

    // ── Initial state from MicProfileManager ────────────────────────────────
    if (model() != nullptr) {
        if (auto* mgr = model()->micProfileManager()) {
            m_activeProfileLabel->setText(mgr->activeProfileName());

            // Live-update on profile change.
            connect(mgr, &MicProfileManager::activeProfileChanged,
                    this, [this](const QString& name) {
                if (m_activeProfileLabel) {
                    m_activeProfileLabel->setText(name);
                }
            });
        }
    }

    // ── Manage… → TxEqDialog (modeless singleton) ───────────────────────────
    // Same launch pattern as TxApplet::onEqRightClick (TxApplet.cpp:920) and
    // the Tools → TX Equalizer menu hook (MainWindow.cpp:2043).
    connect(m_manageProfileBtn, &QPushButton::clicked, this, [this]() {
        if (model() == nullptr) { return; }
        TxEqDialog* dlg = TxEqDialog::instance(model(), this);
        if (dlg != nullptr) {
            dlg->show();
            dlg->raise();
            dlg->activateWindow();
        }
    });
}

// ---------------------------------------------------------------------------
// SpeechProcessorPage::addStageRow
//
// Builds one "Stage  ●  state    [button]   (future-phase tag)" row inside
// the Stage Status grid.  Returns the state QLabel so callers can wire it
// to a TransmitModel signal for live updates.
//
// linkPage:  empty → button is a visible-but-disabled placeholder
//            non-empty → button click emits openSetupRequested("DSP", linkPage)
// futurePhaseTag: empty → no suffix label appended
// ---------------------------------------------------------------------------
QLabel* SpeechProcessorPage::addStageRow(QGridLayout* grid, int row,
                                          const QString& stageName,
                                          const QString& initialState,
                                          bool initiallyOn,
                                          const QString& buttonText,
                                          const QString& buttonTooltip,
                                          const QString& linkPage,
                                          const QString& futurePhaseTag)
{
    auto* nameLbl = new QLabel(stageName);
    nameLbl->setStyleSheet(QStringLiteral(
        "QLabel { color: #c8d8e8; font-size: 12px; font-weight: bold; }"));
    nameLbl->setMinimumWidth(110);

    auto* dotLbl = new QLabel(initiallyOn ? QString(kFilledCircle)
                                          : QString(kHollowCircle));
    dotLbl->setObjectName(QStringLiteral("dot_") + stageName);
    dotLbl->setStyleSheet(dotStyleFor(initiallyOn));
    dotLbl->setFixedWidth(20);
    dotLbl->setAlignment(Qt::AlignCenter);

    auto* stateLbl = new QLabel(initialState);
    stateLbl->setObjectName(QStringLiteral("state_") + stageName);
    stateLbl->setStyleSheet(QStringLiteral(
        "QLabel { color: #00c8ff; font-size: 12px; }"));
    stateLbl->setMinimumWidth(90);
    // Stash the dot sibling on the state label so live-update lambdas can
    // reach it without having to re-find it from the page root.
    stateLbl->setProperty("dotSibling", QVariant::fromValue<QObject*>(dotLbl));

    auto* btn = new QPushButton(buttonText);
    btn->setObjectName(QStringLiteral("btn_") + stageName);
    btn->setAutoDefault(false);
    btn->setToolTip(buttonTooltip);
    btn->setStyleSheet(QStringLiteral(
        "QPushButton { background: #1a2a3a; border: 1px solid #304050;"
        "  border-radius: 3px; color: #c8d8e8; font-size: 11px; padding: 2px 8px; }"
        "QPushButton:hover:enabled { background: #203040; }"
        "QPushButton:pressed:enabled { background: #00b4d8; color: #0f0f1a; }"
        "QPushButton:disabled { color: #607080; border: 1px solid #203040; }"));

    if (linkPage.isEmpty()) {
        // Future-phase placeholder — visible-but-disabled.
        btn->setEnabled(false);
    } else {
        connect(btn, &QPushButton::clicked, this, [this, linkPage]() {
            emit openSetupRequested(QStringLiteral("DSP"), linkPage);
        });
    }

    grid->addWidget(nameLbl, row, 0);
    grid->addWidget(dotLbl,  row, 1);
    grid->addWidget(stateLbl, row, 2);
    grid->addWidget(btn,     row, 3);

    if (!futurePhaseTag.isEmpty()) {
        auto* tagLbl = new QLabel(QStringLiteral("(") + futurePhaseTag + QStringLiteral(")"));
        tagLbl->setStyleSheet(QStringLiteral(
            "QLabel { color: #607080; font-size: 10px; font-style: italic; }"));
        grid->addWidget(tagLbl, row, 4);
    }

    return stateLbl;
}

// ---------------------------------------------------------------------------
// SpeechProcessorPage::buildStageStatusSection
//
// Grid of TXA stage rows.  TX EQ + Leveler + Phrot + CFC + CESSB are wired
// to live model signals; ALC is hard-labelled "always-on" (no toggle in
// Thetis schema either — `txa[ch].alc` is always created with run=1 in
// `TXA.c:create_txa` [v2.10.3.13]).  AM-SQ/DEXP lands in 3M-3a-iii — that
// row remains a placeholder showing "off".
//
// 3M-3a-ii Batch 5 (Task E):
//   * Phrot label format: "ON · <freq> Hz · <stages> stages" / "OFF"
//     wired to phaseRotatorEnabledChanged + phaseRotatorFreqHzChanged +
//     phaseRotatorStagesChanged.
//   * CFC label format: "ON · 10 bands" / "OFF" wired to cfcEnabledChanged.
//   * CESSB label format: "ON" / "OFF (gated on CPDR)" — derived from
//     (cessbOn, cpdrOn) so the off-text reflects whether CPDR is the
//     blocker (per WDSP TXA.c:843-851 [v2.10.3.13]) or just a clean off.
// ---------------------------------------------------------------------------

namespace {

// Compose Phase Rotator label text — "ON · 338 Hz · 8 stages" / "OFF".
QString phRotLabelText(bool on, int freqHz, int stages)
{
    if (!on) {
        return QStringLiteral("OFF");
    }
    return QStringLiteral("ON · %1 Hz · %2 stages").arg(freqHz).arg(stages);
}

// Compose CFC label text — "ON · 10 bands" / "OFF".  10-band CFC is a
// fixed Thetis schema constant (cfcomp.c CFC_NFREQS [v2.10.3.13]).
QString cfcLabelText(bool on)
{
    return on ? QStringLiteral("ON · 10 bands") : QStringLiteral("OFF");
}

// Compose CESSB label text — "ON" / "OFF" / "OFF (gated on CPDR)".
// Per WDSP TXA.c:843-851 [v2.10.3.13], CESSB's bp2 stage only activates
// when CPDR (compressor) run-flag is set.  Surfacing the gating in the
// off-text helps users see why flipping CESSB without CPDR is a no-op.
QString cessbLabelText(bool cessbOn, bool cpdrOn)
{
    if (cessbOn) { return QStringLiteral("ON"); }
    return cpdrOn ? QStringLiteral("OFF") : QStringLiteral("OFF (gated on CPDR)");
}

}  // namespace

void SpeechProcessorPage::buildStageStatusSection()
{
    auto* group = addSection(QStringLiteral("Stage Status"));
    group->setObjectName(QStringLiteral("grpSpeechStageStatus"));

    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(4);

    const bool txEqOn  = (model() != nullptr) && model()->transmitModel().txEqEnabled();
    const bool levelOn = (model() != nullptr) && model()->transmitModel().txLevelerOn();
    const bool phRotOn = (model() != nullptr) && model()->transmitModel().phaseRotatorEnabled();
    const bool cfcOn   = (model() != nullptr) && model()->transmitModel().cfcEnabled();
    const bool cessbOn = (model() != nullptr) && model()->transmitModel().cessbOn();
    const bool cpdrOn  = (model() != nullptr) && model()->transmitModel().cpdrOn();

    const int phRotFreq   = (model() != nullptr) ? model()->transmitModel().phaseRotatorFreqHz() : 338;
    const int phRotStages = (model() != nullptr) ? model()->transmitModel().phaseRotatorStages() : 8;

    int row = 0;

    // TX EQ — wired to TransmitModel::txEqEnabledChanged.
    m_txEqStatusLabel = addStageRow(grid, row++,
        QStringLiteral("TX EQ"),
        txEqOn ? QStringLiteral("enabled") : QStringLiteral("off"),
        txEqOn,
        QStringLiteral("Open TX EQ Editor..."),
        QStringLiteral("Open the modeless TX Equalizer dialog (10-band sliders)"),
        QString(),                                         // not a setup-page jump
        QString());

    // The TX EQ row's button doesn't cross-link to a setup page — it opens
    // the modeless TxEqDialog directly (same launch pattern as
    // TxApplet::onEqRightClick / Tools → TX Equalizer).  addStageRow()
    // initially leaves the button disabled because we passed an empty
    // linkPage; here we re-enable it and wire the dialog launch.
    if (auto* btn = group->findChild<QPushButton*>(QStringLiteral("btn_TX EQ"))) {
        m_openTxEqBtn = btn;
        m_openTxEqBtn->setEnabled(true);
        connect(m_openTxEqBtn, &QPushButton::clicked, this, [this]() {
            if (model() == nullptr) { return; }
            TxEqDialog* dlg = TxEqDialog::instance(model(), this);
            if (dlg != nullptr) {
                dlg->show();
                dlg->raise();
                dlg->activateWindow();
            }
        });
    }

    // Leveler — wired to TransmitModel::txLevelerOnChanged.  Cross-links to
    // Setup → DSP → AGC/ALC.
    m_levelerStatusLabel = addStageRow(grid, row++,
        QStringLiteral("Leveler"),
        levelOn ? QStringLiteral("enabled") : QStringLiteral("off"),
        levelOn,
        QStringLiteral("Open AGC/ALC Setup"),
        QStringLiteral("Open Setup → DSP → AGC/ALC (Leveler controls live there)"),
        QStringLiteral("AGC/ALC"),
        QString());

    // ALC — always-on; static row.  Dot is cyan (always-on callout).
    m_alcStatusLabel = addStageRow(grid, row++,
        QStringLiteral("ALC"),
        QStringLiteral("always-on"),
        true,
        QStringLiteral("Open AGC/ALC Setup"),
        QStringLiteral("Open Setup → DSP → AGC/ALC (ALC max-gain + decay live there)"),
        QStringLiteral("AGC/ALC"),
        QString());
    // Override the dot colour to the cyan callout (rather than the default
    // green-for-on) so users can tell at a glance that ALC isn't a toggle.
    if (auto* dot = group->findChild<QLabel*>(QStringLiteral("dot_ALC"))) {
        dot->setStyleSheet(QStringLiteral(
            "QLabel { color: #00b4d8; font-size: 14px; font-weight: bold; }"));
    }

    // Phase Rotator — wired live (3M-3a-ii Batch 5).  Cross-links to CFC
    // page (PhRot lives there per the IA decision).
    m_phrotStatusLabel = addStageRow(grid, row++,
        QStringLiteral("Phase Rot."),
        phRotLabelText(phRotOn, phRotFreq, phRotStages),
        phRotOn,
        QStringLiteral("Open Phase Rotator..."),
        QStringLiteral("Open Setup → DSP → CFC (Phase Rotator group is at the top)"),
        QStringLiteral("CFC"),
        QString());

    // CFC — wired live (3M-3a-ii Batch 5).
    m_cfcStatusLabel = addStageRow(grid, row++,
        QStringLiteral("CFC"),
        cfcLabelText(cfcOn),
        cfcOn,
        QStringLiteral("Open CFC..."),
        QStringLiteral("Open Setup → DSP → CFC (Continuous Frequency Compressor)"),
        QStringLiteral("CFC"),
        QString());

    // CESSB — wired live (3M-3a-ii Batch 5).
    m_cessbStatusLabel = addStageRow(grid, row++,
        QStringLiteral("CESSB"),
        cessbLabelText(cessbOn, cpdrOn),
        cessbOn,
        QStringLiteral("Open CESSB..."),
        QStringLiteral("Open Setup → DSP → CFC (CESSB group is at the bottom)"),
        QStringLiteral("CFC"),
        QString());

    // AM-SQ / DEXP — placeholder (3M-3a-iii target; cross-links to VOX/DEXP).
    m_amSqDexpStatusLabel = addStageRow(grid, row++,
        QStringLiteral("AM-SQ / DEXP"),
        QStringLiteral("off"),
        false,
        QStringLiteral("Open VOX/DEXP Setup"),
        QStringLiteral("Open Setup → DSP → VOX/DEXP (AM-Squelch + Downward Expander)"),
        QStringLiteral("VOX/DEXP"),
        QStringLiteral("3M-3a-iii"));

    auto* groupLayout = qobject_cast<QVBoxLayout*>(group->layout());
    if (groupLayout) {
        groupLayout->addLayout(grid);
    }

    // ── Live wiring: TransmitModel → status labels ──────────────────────────
    if (model() != nullptr) {
        auto& tx = model()->transmitModel();

        connect(&tx, &TransmitModel::txEqEnabledChanged,
                this, [this](bool on) {
            if (!m_txEqStatusLabel) { return; }
            m_txEqStatusLabel->setText(on ? QStringLiteral("enabled")
                                          : QStringLiteral("off"));
            // Update sibling dot (looked up via the property we stashed).
            if (auto* dot = qobject_cast<QLabel*>(
                    m_txEqStatusLabel->property("dotSibling").value<QObject*>())) {
                dot->setText(on ? QString(kFilledCircle) : QString(kHollowCircle));
                dot->setStyleSheet(dotStyleFor(on));
            }
        });

        connect(&tx, &TransmitModel::txLevelerOnChanged,
                this, [this](bool on) {
            if (!m_levelerStatusLabel) { return; }
            m_levelerStatusLabel->setText(on ? QStringLiteral("enabled")
                                              : QStringLiteral("off"));
            if (auto* dot = qobject_cast<QLabel*>(
                    m_levelerStatusLabel->property("dotSibling").value<QObject*>())) {
                dot->setText(on ? QString(kFilledCircle) : QString(kHollowCircle));
                dot->setStyleSheet(dotStyleFor(on));
            }
        });

        // ── Phase Rotator: re-render on enable / freq / stages change ───────
        auto refreshPhRot = [this]() {
            if (!m_phrotStatusLabel || model() == nullptr) { return; }
            const auto& tx = model()->transmitModel();
            const bool on = tx.phaseRotatorEnabled();
            m_phrotStatusLabel->setText(phRotLabelText(
                on, tx.phaseRotatorFreqHz(), tx.phaseRotatorStages()));
            if (auto* dot = qobject_cast<QLabel*>(
                    m_phrotStatusLabel->property("dotSibling").value<QObject*>())) {
                dot->setText(on ? QString(kFilledCircle) : QString(kHollowCircle));
                dot->setStyleSheet(dotStyleFor(on));
            }
        };
        connect(&tx, &TransmitModel::phaseRotatorEnabledChanged, this, refreshPhRot);
        connect(&tx, &TransmitModel::phaseRotatorFreqHzChanged,  this, refreshPhRot);
        connect(&tx, &TransmitModel::phaseRotatorStagesChanged,  this, refreshPhRot);

        // ── CFC: re-render on enable change ─────────────────────────────────
        connect(&tx, &TransmitModel::cfcEnabledChanged,
                this, [this](bool on) {
            if (!m_cfcStatusLabel) { return; }
            m_cfcStatusLabel->setText(cfcLabelText(on));
            if (auto* dot = qobject_cast<QLabel*>(
                    m_cfcStatusLabel->property("dotSibling").value<QObject*>())) {
                dot->setText(on ? QString(kFilledCircle) : QString(kHollowCircle));
                dot->setStyleSheet(dotStyleFor(on));
            }
        });

        // ── CESSB: re-render on cessbOn / cpdrOn change ─────────────────────
        auto refreshCessb = [this]() {
            if (!m_cessbStatusLabel || model() == nullptr) { return; }
            const auto& tx = model()->transmitModel();
            const bool on = tx.cessbOn();
            m_cessbStatusLabel->setText(cessbLabelText(on, tx.cpdrOn()));
            if (auto* dot = qobject_cast<QLabel*>(
                    m_cessbStatusLabel->property("dotSibling").value<QObject*>())) {
                dot->setText(on ? QString(kFilledCircle) : QString(kHollowCircle));
                dot->setStyleSheet(dotStyleFor(on));
            }
        };
        connect(&tx, &TransmitModel::cessbOnChanged, this, refreshCessb);
        connect(&tx, &TransmitModel::cpdrOnChanged,  this, refreshCessb);
    }
}

// ---------------------------------------------------------------------------
// SpeechProcessorPage::buildQuickNotesSection
//
// Static informational block.  Documents the WDSP TXA speech-chain order
// (matches `wdsp/TXA.c:create_txa` execution order [v2.10.3.13]: eqp →
// leveler → cfcomp/cfir → compressor → phrot → amsq → alc → output) and
// reminds users where each stage is configured.
// ---------------------------------------------------------------------------
void SpeechProcessorPage::buildQuickNotesSection()
{
    auto* group = addSection(QStringLiteral("Quick Notes"));
    group->setObjectName(QStringLiteral("grpSpeechQuickNotes"));

    auto* note = new QLabel(QStringLiteral(
        "Speech chain order: EQ → Leveler → CFC → CPDR → CESSB → AM-SQ → ALC → Output\n"
        "Configure each stage on its dedicated DSP setup tab.\n"
        "TX EQ has its own modeless editor (Tools → TX Equalizer)."));
    note->setObjectName(QStringLiteral("lblQuickNotes"));
    note->setWordWrap(true);
    note->setStyleSheet(QStringLiteral(
        "QLabel { color: #8aa8c0; font-size: 11px; }"));

    auto* groupLayout = qobject_cast<QVBoxLayout*>(group->layout());
    if (groupLayout) {
        groupLayout->addWidget(note);
    }
}

// ---------------------------------------------------------------------------
// PureSignalPage
// ---------------------------------------------------------------------------

PureSignalPage::PureSignalPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("PureSignal"), model, parent)
{
    buildUI();
}

void PureSignalPage::buildUI()
{
    NereusSDR::Style::applyDarkPageStyle(this);

    // --- Section: PureSignal ---
    auto* psGroup = new QGroupBox(QStringLiteral("PureSignal"), this);
    auto* psForm  = new QFormLayout(psGroup);
    psForm->setSpacing(6);

    m_enableToggle = new QCheckBox(QStringLiteral("Enable PureSignal"), psGroup);
    m_enableToggle->setEnabled(false);  // NYI — requires Phase 3I-4
    m_enableToggle->setToolTip(QStringLiteral("PureSignal linearization — not yet implemented (Phase 3I-4)"));
    psForm->addRow(QString(), m_enableToggle);

    m_autoCalToggle = new QCheckBox(QStringLiteral("Auto-calibrate"), psGroup);
    m_autoCalToggle->setEnabled(false);  // NYI
    m_autoCalToggle->setToolTip(QStringLiteral("PureSignal auto-calibration — not yet implemented"));
    psForm->addRow(QString(), m_autoCalToggle);

    m_feedbackDdcCombo = new QComboBox(psGroup);
    // From Thetis setup.cs: PS feedback DDC choices depend on board DDC count
    m_feedbackDdcCombo->addItems({QStringLiteral("DDC 0"), QStringLiteral("DDC 1"),
                                  QStringLiteral("DDC 2"), QStringLiteral("DDC 3"),
                                  QStringLiteral("DDC 4"), QStringLiteral("DDC 5"),
                                  QStringLiteral("DDC 6"), QStringLiteral("DDC 7")});
    m_feedbackDdcCombo->setEnabled(false);  // NYI
    m_feedbackDdcCombo->setToolTip(QStringLiteral("Feedback DDC selection — not yet implemented"));
    psForm->addRow(QStringLiteral("Feedback DDC:"), m_feedbackDdcCombo);

    m_attentionSlider = new QSlider(Qt::Horizontal, psGroup);
    m_attentionSlider->setRange(0, 31);
    m_attentionSlider->setValue(0);
    m_attentionSlider->setEnabled(false);  // NYI
    m_attentionSlider->setToolTip(QStringLiteral("PureSignal feedback attention (dB) — not yet implemented"));
    psForm->addRow(QStringLiteral("Attention (dB):"), m_attentionSlider);

    m_infoLabel = new QLabel(QStringLiteral("Status: Not active"), psGroup);
    m_infoLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #607080; font-style: italic; }"));
    m_infoLabel->setEnabled(false);
    psForm->addRow(QStringLiteral("Status:"), m_infoLabel);

    contentLayout()->addWidget(psGroup);
    contentLayout()->addStretch();
}

} // namespace NereusSDR
