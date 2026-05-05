// =================================================================
// src/gui/setup/TransmitSetupPages.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/setup.cs, original licence from Thetis source is included below
//
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
//   2026-05-03 — Phase 3M-3a-iii Task 14: new DexpVoxPage that mirrors
//                 Thetis tpDSPVOXDE 1:1 (setup.designer.cs:44763-45260
//                 [v2.10.3.13]).  Three QGroupBox containers with 14
//                 controls bidirectionally bound to TransmitModel via
//                 QSignalBlocker guards (no feedback loops).  Group titles
//                 + label captions copied verbatim from the Thetis
//                 Designer.  Registered as the "DEXP/VOX" leaf under the
//                 "Transmit" SetupDialog category.  PhoneCwApplet
//                 right-click in Task 15 will route here via
//                 SetupDialog::selectPage("DEXP/VOX").
//   2026-05-03 — Bench polish: DexpVoxPage outer layout switched from
//                 a single QHBoxLayout (three boxes side-by-side) to a
//                 2x2 QGridLayout (VOX/DEXP big box spanning column 0
//                 rows 0+1; Audio LookAhead top-right; Side-Channel
//                 Trigger Filter bottom-right).  The horizontal layout
//                 exceeded standard Setup-dialog width and forced
//                 horizontal scroll; the grid keeps everything inside
//                 the dialog without panning.  Bidirectional bindings
//                 unchanged.
//   2026-05-04 — Bench polish: DexpVoxPage QGridLayout now inserts BEFORE
//                 the auto-trailing stretch in SetupPage::init() rather
//                 than appending after it.  The previous code sandwiched
//                 the grid between the auto-stretch and an explicit
//                 trailing addStretch(), centring the boxes vertically
//                 when the dialog page area was taller than the grid.
//                 The fix mirrors SetupPage::addSection's
//                 insertWidget(stretchIndex, group) pattern so the boxes
//                 cluster at the top of the page like every other
//                 Setup-page family.
//   2026-05-04 — Issue #175 Task 9: per-band Tune Power grid SKU-aware.
//                 m_tunePwrSpins[14] flips QSpinBox* -> QDoubleSpinBox* so
//                 the per-SKU display semantics can vary the decimal count.
//                 On HL2: range -16.5..0 dB / 0.5 step / 1 dp / " dB"
//                 suffix; displayed = (stored / 3.0 - 33.0) / 2.0; on user
//                 edit, stored = round((33 + dB*2) * 3).  On other SKUs the
//                 grid behaves exactly as before (decimals=0, " W" suffix,
//                 display == stored).  Storage stays as int under the
//                 existing tunePower_by_band/<band> AppSettings key — same
//                 key, polymorphic semantic by SKU (mi0bot pattern, no
//                 migration step).  applyHpsdrModel() extends to refresh
//                 the per-band grid bounds + displayed values on
//                 RadioModel::currentRadioChanged.
//   2026-05-04 — Issue #175 Wave 1: TxProfilesPage placeholder body
//                 dropped (5 disabled-stub controls with no Thetis
//                 upstream).  Live TX-profile editor lives at Setup →
//                 Audio → TX Profile.
//   2026-05-04 — Issue #175 PR #194 review fix: udTXTunePower spinbox
//                 wired bidirectionally to TransmitModel::tunePower /
//                 setTunePower with SKU-aware unit conversion
//                 (tunePowerDisplayFromStored / tunePowerStoredFromDisplay).
//                 Codex review found the spinbox had bounds + enabled-state
//                 wiring but no model read/write, so edits were ignored
//                 and "Use Fixed Drive" left the actual tune-power value at
//                 default.  Conversion formulae from mi0bot setup.cs:
//                 5305-5311 (stored→display) + 9395-9398 (display→stored)
//                 [v2.10.3.13-beta2] HERMESLITE branch; non-HL2 SKUs use
//                 identity (display == stored, watts).  applyHpsdrModel()
//                 now also re-renders the cached value in the new SKU's
//                 display units on RadioModel::currentRadioChanged.
//   2026-05-04 — Issue #175 Wave 1 (cont): PowerPage source-first
//                 strip-out — dropped dead "SWR Protection" slider stub,
//                 per-band Tune Power 14-spinbox grid + helpers, Reset
//                 Tune Power Defaults button + slot, and Block-TX-on-RX
//                 antennas group (was writing dead AppSettings keys; live
//                 editor lives at Hardware → Antenna/ALEX → Antenna
//                 Control).  TX TUN Meter combo items now mi0bot-verbatim
//                 (Fwd Pwr / Ref Pwr / Fwd SWR / SWR / Off).
// =================================================================
#include "TransmitSetupPages.h"
#include "gui/StyleConstants.h"
#include "core/AppSettings.h"
#include "core/MicProfileManager.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"
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
#include <QPushButton>
#include <QRadioButton>
#include <QButtonGroup>
#include <QAbstractButton>
#include <QSignalBlocker>

#include <cmath>

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
    buildTuneGroup();           // Issue #175 Task 8 — grpPATune
    buildSwrProtectionGroup();
    buildExternalTxInhibitGroup();
    buildHfPaGroup();

    // Issue #175 Task 8 — reapply SKU-specific spinbox bounds whenever the
    // active radio changes.  The initial apply runs at construction time
    // inside buildTuneGroup() so cold-launch lands the right bounds before
    // the first connection.
    if (model()) {
        connect(model(), &RadioModel::currentRadioChanged, this,
                [this](const NereusSDR::RadioInfo&) {
            applyHpsdrModel(model()->hardwareProfile().model);
        });
        applyHpsdrModel(model()->hardwareProfile().model);
    }

    // TODO(future): Thetis Transmit tab also has these groups not yet
    // covered by NereusSDR. Tracked separately from this PR; pre-existing
    // gaps, not introduced by the IA reshape.
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

    // Issue #175 Wave 1: dropped dead "SWR Protection" slider stub (NYI,
    // disabled).  The real udSwrProtectionLimit + 4 other SWR controls
    // live in buildSwrProtectionGroup() below; that group is the verified
    // port from Thetis grpSWRProtectionControl
    // (setup.designer.cs:5847-5853 [v2.10.3.13]).

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
// PowerPage::buildTuneGroup — Issue #175 Task 8
// ---------------------------------------------------------------------------
//
// "Tune" group box on Setup → Transmit → Power.  Mirrors mi0bot-Thetis
// grpPATune layout from setup.designer.cs:47874-47930 [v2.10.3.13-beta2]:
//   3 drive-source radios (radUseFixedDriveTune / radUseTuneSliderTune /
//   radUseDriveSliderTune) + comboTXTUNMeter (Fwd/Ref/SWR/Mic/ALC) +
//   udTXTunePower spinbox (Fixed-mode tune drive).
//
// The Fixed-mode spinbox is enabled only when "Use Fixed Drive" is the
// selected radio (mi0bot setup.cs udTXTunePower wiring).  Spinbox bounds
// polymorph by SKU via applyHpsdrModel() — HL2 swings to the dB attenuation
// range per setup.cs:20328-20331 [v2.10.3.13-beta2].
//
// The default radio is "Use Tune Slider" matching the mi0bot designer
// initial state (radUseTuneSliderTune.TabIndex = 10 — first selectable).
void PowerPage::buildTuneGroup()
{
    m_grpPATune = new QGroupBox(QStringLiteral("Tune"), this);
    m_grpPATune->setObjectName(QStringLiteral("grpPATune"));

    auto* form = new QFormLayout(m_grpPATune);
    form->setSpacing(4);

    // ── Drive-source radios ────────────────────────────────────────────────
    // Captions match mi0bot setup.designer.cs Text properties verbatim:
    //   radUseFixedDriveTune.Text   = "Use Fixed Drive"   (line 47898)
    //   radUseDriveSliderTune.Text  = "Use Drive Slider"  (line 47912)
    //   radUseTuneSliderTune.Text   = "Use Tune Slider"   (line 47925)
    m_radFixedDrive  = new QRadioButton(QStringLiteral("Use Fixed Drive"),  m_grpPATune);
    m_radFixedDrive->setObjectName(QStringLiteral("radUseFixedDriveTune"));
    m_radDriveSlider = new QRadioButton(QStringLiteral("Use Drive Slider"), m_grpPATune);
    m_radDriveSlider->setObjectName(QStringLiteral("radUseDriveSliderTune"));
    m_radTuneSlider  = new QRadioButton(QStringLiteral("Use Tune Slider"),  m_grpPATune);
    m_radTuneSlider->setObjectName(QStringLiteral("radUseTuneSliderTune"));

    m_tuneDriveButtons = new QButtonGroup(m_grpPATune);
    m_tuneDriveButtons->addButton(m_radFixedDrive,  static_cast<int>(DrivePowerSource::Fixed));
    m_tuneDriveButtons->addButton(m_radDriveSlider, static_cast<int>(DrivePowerSource::DriveSlider));
    m_tuneDriveButtons->addButton(m_radTuneSlider,  static_cast<int>(DrivePowerSource::TuneSlider));

    // mi0bot designer order: radUseDriveSliderTune (top, y=50) →
    // radUseTuneSliderTune (middle, y=73) → radUseFixedDriveTune (bottom, y=96).
    auto* radioBox = new QVBoxLayout;
    radioBox->setSpacing(2);
    radioBox->addWidget(m_radDriveSlider);
    radioBox->addWidget(m_radTuneSlider);
    radioBox->addWidget(m_radFixedDrive);
    form->addRow(QStringLiteral("Drive Source:"), radioBox);

    // ── TX TUN Meter combo ─────────────────────────────────────────────────
    // Items list mi0bot-verbatim: TUNE injects a constant carrier and
    // disables ALC, so Mic and ALC cannot logically source the TUN meter
    // — the prior items were fabrications.  Full meter routing wiring
    // is a follow-up task; this surface lands the combo so the page
    // matches mi0bot 1:1.
    m_comboTxTunMeter = new QComboBox(m_grpPATune);
    m_comboTxTunMeter->setObjectName(QStringLiteral("comboTXTUNMeter"));
    // From mi0bot-Thetis setup.designer.cs:47933-47938 [v2.10.3.13-beta2]
    m_comboTxTunMeter->addItem(QStringLiteral("Fwd Pwr"));
    m_comboTxTunMeter->addItem(QStringLiteral("Ref Pwr"));
    m_comboTxTunMeter->addItem(QStringLiteral("Fwd SWR"));
    m_comboTxTunMeter->addItem(QStringLiteral("SWR"));
    m_comboTxTunMeter->addItem(QStringLiteral("Off"));
    form->addRow(QStringLiteral("TX TUN Meter:"), m_comboTxTunMeter);

    // ── Fixed-mode tune-power spinbox ──────────────────────────────────────
    // Bounds / step / decimals / suffix are set in applyHpsdrModel() and
    // re-applied on every RadioModel::currentRadioChanged.  The default
    // (FIRST = ANAN100) keeps a sane Watts range until the first connect.
    m_fixedTunePwrSpin = new QDoubleSpinBox(m_grpPATune);
    m_fixedTunePwrSpin->setObjectName(QStringLiteral("udTXTunePower"));
    form->addRow(QStringLiteral("Fixed Tune Power:"), m_fixedTunePwrSpin);

    // Apply default-SKU bounds so a no-model test still gets a sensible
    // initial range; the constructor's applyHpsdrModel() call after
    // buildUI() overwrites this once the model knows its hardware.
    applyHpsdrModel(HPSDRModel::FIRST);

    // Issue #175 PR #194 review fix — bidirectional binding to
    // TransmitModel::tunePower.  Codex review found the spinbox was
    // constructed with bounds + enabled-state but no read/write to the
    // model: edits did nothing and "Use Fixed Drive" left the actual
    // tune-power value at default.
    //
    // SKU-aware unit conversion via tunePowerDisplayFromStored /
    // tunePowerStoredFromDisplay (mi0bot setup.cs:5305-5311 + 9395-9398
    // [v2.10.3.13-beta2]).  Forward and reverse paths are blocked through
    // QSignalBlocker on the reverse path to prevent feedback loops.
    if (model()) {
        TransmitModel& tx = model()->transmitModel();

        // Initial value from model (non-blocking — no slot connected yet).
        m_fixedTunePwrSpin->setValue(tunePowerDisplayFromStored(tx.tunePower()));

        // Spinbox → model.
        connect(m_fixedTunePwrSpin,
                qOverload<double>(&QDoubleSpinBox::valueChanged),
                this, [this](double display) {
            if (!model()) { return; }
            model()->transmitModel().setTunePower(
                tunePowerStoredFromDisplay(display));
        });

        // Model → spinbox (reverse, e.g. settings load or another editor).
        connect(&tx, &TransmitModel::tunePowerChanged, m_fixedTunePwrSpin,
                [this](int stored) {
            QSignalBlocker b(m_fixedTunePwrSpin);
            m_fixedTunePwrSpin->setValue(tunePowerDisplayFromStored(stored));
        });
    }

    // ── Wire model ↔ radios ────────────────────────────────────────────────
    if (model()) {
        TransmitModel& tx = model()->transmitModel();

        // Initial state from model.  Selects the matching radio without
        // emitting buttonClicked (setChecked-driven idToggled is suppressed
        // by the QSignalBlocker on the button group below).
        const DrivePowerSource initial = tx.tuneDrivePowerSource();
        QRadioButton* initialBtn = m_radTuneSlider;
        switch (initial) {
            case DrivePowerSource::Fixed:       initialBtn = m_radFixedDrive;  break;
            case DrivePowerSource::DriveSlider: initialBtn = m_radDriveSlider; break;
            case DrivePowerSource::TuneSlider:  initialBtn = m_radTuneSlider;  break;
        }
        {
            QSignalBlocker b(m_tuneDriveButtons);
            initialBtn->setChecked(true);
        }
        onTuneDriveSourceChanged(initial);

        // Radio → model.  Use idToggled (not buttonClicked) so the slot
        // fires both on user clicks and on programmatic setChecked() — the
        // latter matters for tests and for sync with the model-→-radio
        // reverse path.  Filter on `checked` so we react only to the
        // positive edge of the newly-selected button (the deselect of the
        // previous button fires checked=false which we ignore).
        connect(m_tuneDriveButtons, &QButtonGroup::idToggled,
                this, [this](int id, bool checked) {
            if (!checked) { return; }
            if (!model()) { return; }
            const auto src = static_cast<DrivePowerSource>(id);
            model()->transmitModel().setTuneDrivePowerSource(src);
            onTuneDriveSourceChanged(src);
        });

        // Model → radio (reverse).  When the active source flips elsewhere
        // (TxApplet RF-slider auto-switch in console.cs:46553 [v2.10.3.13]
        // path), reflect the change on the page.
        connect(&tx, &TransmitModel::tuneDrivePowerSourceChanged,
                this, [this](DrivePowerSource src) {
            QRadioButton* target = m_radTuneSlider;
            switch (src) {
                case DrivePowerSource::Fixed:       target = m_radFixedDrive;  break;
                case DrivePowerSource::DriveSlider: target = m_radDriveSlider; break;
                case DrivePowerSource::TuneSlider:  target = m_radTuneSlider;  break;
            }
            QSignalBlocker b(m_tuneDriveButtons);
            target->setChecked(true);
            onTuneDriveSourceChanged(src);
        });
    } else {
        // No-model path (test fixture).  Default to "Use Tune Slider" per
        // mi0bot designer initial state and keep the spinbox disabled.
        m_radTuneSlider->setChecked(true);
        onTuneDriveSourceChanged(DrivePowerSource::TuneSlider);
        // Even without a RadioModel the radio toggle must still update the
        // spinbox enabled state so users get visual feedback on the page.
        // Use idToggled (not buttonClicked) so programmatic setChecked()
        // also updates the spinbox state.
        connect(m_tuneDriveButtons, &QButtonGroup::idToggled,
                this, [this](int id, bool checked) {
            if (!checked) { return; }
            onTuneDriveSourceChanged(static_cast<DrivePowerSource>(id));
        });
    }

    contentLayout()->addWidget(m_grpPATune);
}

// Issue #175 Task 8 — flip the udTXTunePower enabled state so it tracks
// whether "Use Fixed Drive" is the active radio.  Mirrors the mi0bot
// designer wiring where the spinbox is meaningful only in Fixed mode.
void PowerPage::onTuneDriveSourceChanged(DrivePowerSource src)
{
    if (m_fixedTunePwrSpin) {
        m_fixedTunePwrSpin->setEnabled(src == DrivePowerSource::Fixed);
    }
}

// Issue #175 Task 8 — per-SKU bounds on the Fixed-mode spinbox.
// From mi0bot-Thetis setup.cs:20328-20331 [v2.10.3.13-beta2] HERMESLITE
// branch; ANAN/Saturn/Orion paths keep the canonical 0..100 W range.
//
// Issue #175 PR #194 review fix: also refresh the displayed value via the
// SKU-aware conversion so a runtime SKU swap (RadioModel::currentRadioChanged)
// re-renders the cached TransmitModel::tunePower() in the new units.
void PowerPage::applyHpsdrModel(HPSDRModel m)
{
    // Fixed-mode spinbox (Task 8).
    if (m_fixedTunePwrSpin) {
        m_fixedTunePwrSpin->setRange(static_cast<double>(fixedTuneSpinboxMinFor(m)),
                                     static_cast<double>(fixedTuneSpinboxMaxFor(m)));
        m_fixedTunePwrSpin->setSingleStep(static_cast<double>(fixedTuneSpinboxStepFor(m)));
        m_fixedTunePwrSpin->setDecimals(fixedTuneSpinboxDecimalsFor(m));
        m_fixedTunePwrSpin->setSuffix(QString::fromLatin1(fixedTuneSpinboxSuffixFor(m)));

        // Re-render the stored tunePower() in the new SKU's display units.
        // Use the model's hpsdrModel() rather than the parameter `m` so the
        // conversion matches the live SKU (caller invariant: applyHpsdrModel
        // is called after RadioModel::currentRadioChanged commits the
        // hardware profile).  Block to suppress the forward connect.
        if (model()) {
            QSignalBlocker b(m_fixedTunePwrSpin);
            m_fixedTunePwrSpin->setValue(
                tunePowerDisplayFromStored(model()->transmitModel().tunePower()));
        }
    }
}

// Issue #175 PR #194 review fix — SKU-aware unit conversion for the
// Fixed-mode tune-power spinbox.  See declarations in TransmitSetupPages.h
// for the formulae.  Conversions read TransmitModel::hpsdrModel() so they
// always match the live SKU and stay correct across runtime swaps.
double PowerPage::tunePowerDisplayFromStored(int stored)
{
    if (model() &&
        model()->transmitModel().hpsdrModel() == HPSDRModel::HERMESLITE) {
        // mi0bot setup.cs:5307 [v2.10.3.13-beta2]:
        //   udTXTunePower.Value = (decimal)(value/3 - 33)/2;
        return (static_cast<double>(stored) / 3.0 - 33.0) / 2.0;
    }
    return static_cast<double>(stored);
}

int PowerPage::tunePowerStoredFromDisplay(double display)
{
    if (model() &&
        model()->transmitModel().hpsdrModel() == HPSDRModel::HERMESLITE) {
        // mi0bot setup.cs:9397 [v2.10.3.13-beta2]:
        //   console.TunePower = (int) ((33 + (udTXTunePower.Value * 2)) * 3);
        // The int cast in C# is truncation-toward-zero; std::lround better
        // matches the user-visible "0.5 dB step → 3 sub-step increment"
        // expectation (avoids accumulating floor() truncation drift).  For
        // exact half-step inputs both behave identically; for non-step
        // inputs (e.g. mid-cell scroll) round() picks the nearest legal
        // sub-step instead of always biasing low.
        return static_cast<int>(std::lround((33.0 + display * 2.0) * 3.0));
    }
    return static_cast<int>(std::lround(display));
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

// Issue #175 Wave 1: dropped Block TX on RX antennas group from PowerPage.
// The checkboxes were writing dead AppSettings keys (AlexAnt2RxOnly /
// AlexAnt3RxOnly) that nothing ever read.  The live editor for the same
// setting lives at Hardware → Antenna/ALEX → Antenna Control
// (AntennaAlexAntennaControlTab via AlexController, persisted per-MAC at
// hardware/<mac>/alex/antenna/blockTxAnt2).  Thetis hosts the checkboxes
// once on panelAlexRXAntControl (setup.designer.cs:6720-6721
// [v2.10.3.13-beta2]).

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

    // Issue #175 Wave 1 follow-up — page body was a fully disabled
    // placeholder (5 controls all setEnabled(false), zero connect()).
    // The live TX-profile editor lives at Setup → Audio → TX Profile
    // (TxProfileSetupPage, fully wired to MicProfileManager).  Thetis
    // ships grpTXProfile (combo + Save/Delete) directly on tpTransmit
    // at mi0bot setup.designer.cs:47829-47836 [v2.10.3.13-beta2];
    // no dedicated "TX Profiles" tab page exists upstream.  Leaf
    // retained in SetupDialog tree per JJ's direction (IA reshape
    // decisions deferred to a separate audit).
    auto* info = new QLabel(
        QStringLiteral("TX Profile editing has moved to Setup → Audio "
                       "→ TX Profile."),
        this);
    info->setAlignment(Qt::AlignCenter);
    info->setWordWrap(true);
    info->setStyleSheet(QStringLiteral(
        "QLabel { color: #b0c0d0; font-style: italic; "
        " background: #1a2a3a; border: 1px solid #203040; "
        " border-radius: 3px; padding: 12px; }"));
    contentLayout()->addWidget(info);

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

// ══════════════════════════════════════════════════════════════════════════════
// DexpVoxPage  (Phase 3M-3a-iii Task 14)
// ══════════════════════════════════════════════════════════════════════════════
//
// Mirrors Thetis tpDSPVOXDE 1:1 (setup.designer.cs:44763-45260 [v2.10.3.13]).
// Three QGroupBox containers laid out left-to-right inside a QHBoxLayout:
//   1. grpDEXPVOX           ("VOX / DEXP")               — line 44843
//   2. grpDEXPLookAhead     ("Audio LookAhead")          — line 44763
//   3. grpScf               ("Side-Channel Trigger Filter") — line 45165
//
// 14 controls total: 9 in the DEXP/VOX group, 2 in Audio LookAhead, 3 in SCF.
// Object names + label captions copied verbatim from the Thetis Designer file
// so that future cross-tooling (or PhoneCwApplet right-click target traversal)
// can rely on stable identifiers.
//
// Bidirectional binding to TransmitModel uses the established CfcSetupPage
// pattern: spinbox/checkbox -> tx.setX(); tx.xChanged -> QSignalBlocker-guarded
// widget setter.  No feedback loops.
//
// Special property mappings (some are 3M-1b VOX properties shared with DEXP
// per the Thetis surface design — udDEXPThreshold and udDEXPHold have no
// dedicated DEXP-only TM properties; they re-use the VOX values, matching
// audio.cs:1015-1110 [v2.10.3.13] where vox_threshold + vox_hang_time drive
// both VOX gating and DEXP detection):
//   chkVOXEnable          <-> voxEnabled
//   chkDEXPEnable         <-> dexpEnabled
//   udDEXPThreshold       <-> voxThresholdDb              (shared)
//   udDEXPHysteresisRatio <-> dexpHysteresisRatioDb
//   udDEXPExpansionRatio  <-> dexpExpansionRatioDb
//   udDEXPAttack          <-> dexpAttackTimeMs
//   udDEXPHold            <-> voxHangTimeMs               (shared)
//   udDEXPRelease         <-> dexpReleaseTimeMs
//   udDEXPDetTau          <-> dexpDetectorTauMs
//   chkDEXPLookAheadEnable<-> dexpLookAheadEnabled
//   udDEXPLookAhead       <-> dexpLookAheadMs
//   chkSCFEnable          <-> dexpSideChannelFilterEnabled
//   udSCFLowCut           <-> dexpLowCutHz
//   udSCFHighCut          <-> dexpHighCutHz

namespace {

// Helper: build a labelled row "[label]    [control]" inside a QGridLayout.
// Mirrors the Thetis label-on-left / spinbox-on-right designer layout that
// every grp* container uses on tpDSPVOXDE.
void addLabelledRow(QGridLayout* grid, int row, const QString& labelText, QWidget* control)
{
    auto* lbl = new QLabel(labelText);
    grid->addWidget(lbl,     row, 0, Qt::AlignLeft | Qt::AlignVCenter);
    grid->addWidget(control, row, 1, Qt::AlignLeft | Qt::AlignVCenter);
}

} // namespace

DexpVoxPage::DexpVoxPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("DEXP/VOX"), model, parent)
{
    // 2026-05-04 bench polish: align with the Setup-page family by
    // applying the canonical dark page stylesheet.  Without this the
    // QSpinBox / QDoubleSpinBox / QGroupBox widgets ship with Qt-default
    // borders that look mismatched against CfcSetupPage / AgcAlcSetupPage
    // and the rest of the Setup pages.  Same one-line call PowerPage
    // (line 160), TxProfilesPage (543), SpeechProcessorPage (637),
    // PureSignalPage (1084) all use.
    NereusSDR::Style::applyDarkPageStyle(this);

    if (!model) {
        // No model — show empty placeholder (matches CfcSetupPage no-model path).
        return;
    }

    TransmitModel& tx = model->transmitModel();

    // ── 2x2 grid: VOX/DEXP big box spans col-0 rows; small boxes stacked col-1 ─
    //
    // 2026-05-03 bench polish: switched from a single horizontal QHBoxLayout
    // (three boxes side-by-side) to a 2x2 QGridLayout because the original
    // arrangement exceeded the standard Setup-dialog width and forced
    // horizontal scroll.  New shape:
    //   col 0 (rows 0+1, spanned)  : VOX / DEXP big box
    //   col 1 row 0                : Audio LookAhead small box
    //   col 1 row 1                : Side-Channel Trigger Filter small box
    // Top-justified by inserting the QGridLayout BEFORE the auto-trailing
    // stretch added in SetupPage::init() (SetupPage.cpp:90).  See the
    // insertLayout block at the bottom of the buildUI for the mechanism.
    auto* row = new QGridLayout;
    row->setSpacing(12);

    // ══════════════════════════════════════════════════════════════════════════
    // ── Group 1: VOX / DEXP ──────────────────────────────────────────────────
    // ══════════════════════════════════════════════════════════════════════════
    //
    // From Thetis setup.designer.cs:44820-45151 [v2.10.3.13] — grpDEXPVOX.
    // Title "VOX / DEXP" verbatim from line 44843.

    auto* dexpVoxGrp = new QGroupBox(QStringLiteral("VOX / DEXP"));
    dexpVoxGrp->setObjectName(QStringLiteral("grpDEXPVOX"));
    auto* dexpVoxLay = new QVBoxLayout(dexpVoxGrp);

    // chkVOXEnable — "Enable VOX" — line 45065
    m_chkVOXEnable = new QCheckBox(QStringLiteral("Enable VOX"));
    m_chkVOXEnable->setObjectName(QStringLiteral("chkVOXEnable"));
    m_chkVOXEnable->setChecked(tx.voxEnabled());
    // From Thetis setup.designer.cs:45066 [v2.10.3.13] — chkVOXEnable tooltip.
    m_chkVOXEnable->setToolTip(QStringLiteral("Enable voice activated transmit"));
    dexpVoxLay->addWidget(m_chkVOXEnable);

    // chkDEXPEnable — "Enable DEXP" — line 45148
    m_chkDEXPEnable = new QCheckBox(QStringLiteral("Enable DEXP"));
    m_chkDEXPEnable->setObjectName(QStringLiteral("chkDEXPEnable"));
    m_chkDEXPEnable->setChecked(tx.dexpEnabled());
    // From Thetis setup.designer.cs:45149 [v2.10.3.13] — chkDEXPEnable tooltip.
    m_chkDEXPEnable->setToolTip(QStringLiteral("Enable Downward Expander"));
    dexpVoxLay->addWidget(m_chkDEXPEnable);

    // 7 numeric spinboxes laid out in a 4-row × 4-col grid (label / control /
    // label / control), matching the Thetis Designer two-column spinbox grid.
    auto* dexpVoxGrid = new QGridLayout;
    dexpVoxGrid->setHorizontalSpacing(10);
    dexpVoxGrid->setVerticalSpacing(6);

    // Left column: Attack, Hold, Release  (lines 45120-45128 / 45110-45118 / 45100-45108)
    // Right column: Threshold, ExpRatio, HystRatio, DetTau (lines 44937-44945 / 44957-44965 /
    //               44947-44955 / 45130-45138)

    // udDEXPAttack — range 2..100 default 2 — line 45027-45055
    m_udDEXPAttack = new QSpinBox;
    m_udDEXPAttack->setObjectName(QStringLiteral("udDEXPAttack"));
    m_udDEXPAttack->setRange(2, 100);
    m_udDEXPAttack->setSingleStep(1);
    m_udDEXPAttack->setValue(static_cast<int>(tx.dexpAttackTimeMs()));
    // From Thetis setup.designer.cs:45049 [v2.10.3.13] — udDEXPAttack tooltip.
    m_udDEXPAttack->setToolTip(QStringLiteral(
        "Time from low to high gain of downward expander"));

    // udDEXPHold — range 1..2000 default 500 step 10 — line 44997-45025
    m_udDEXPHold = new QSpinBox;
    m_udDEXPHold->setObjectName(QStringLiteral("udDEXPHold"));
    m_udDEXPHold->setRange(1, 2000);
    m_udDEXPHold->setSingleStep(10);
    m_udDEXPHold->setValue(tx.voxHangTimeMs());
    // From Thetis setup.designer.cs:45019 [v2.10.3.13] — udDEXPHold tooltip.
    m_udDEXPHold->setToolTip(QStringLiteral(
        "Time from signal drop to initiation of gain increase"));

    // udDEXPRelease — range 2..1000 default 100 — line 44967-44995
    m_udDEXPRelease = new QSpinBox;
    m_udDEXPRelease->setObjectName(QStringLiteral("udDEXPRelease"));
    m_udDEXPRelease->setRange(2, 1000);
    m_udDEXPRelease->setSingleStep(1);
    m_udDEXPRelease->setValue(static_cast<int>(tx.dexpReleaseTimeMs()));
    // From Thetis setup.designer.cs:44989 [v2.10.3.13] — udDEXPRelease tooltip.
    m_udDEXPRelease->setToolTip(QStringLiteral(
        "Time from high to low gain of downward expander"));

    // udDEXPThreshold — range -80..0 default -20 — line 44907-44935
    // (NereusSDR ships voxThresholdDb default -40 per TM.h:1576; the runtime
    // value is what reaches the spinbox, range matches Thetis verbatim.)
    m_udDEXPThreshold = new QSpinBox;
    m_udDEXPThreshold->setObjectName(QStringLiteral("udDEXPThreshold"));
    m_udDEXPThreshold->setRange(-80, 0);
    m_udDEXPThreshold->setSingleStep(1);
    m_udDEXPThreshold->setValue(tx.voxThresholdDb());
    // From Thetis setup.designer.cs:44929 [v2.10.3.13] — udDEXPThreshold tooltip.
    m_udDEXPThreshold->setToolTip(QStringLiteral("Activation threshold"));

    // udDEXPExpansionRatio — range 0..30 default 10.0 step 0.1 dp 1 — line 44876-44905
    m_udDEXPExpansionRatio = new QDoubleSpinBox;
    m_udDEXPExpansionRatio->setObjectName(QStringLiteral("udDEXPExpansionRatio"));
    m_udDEXPExpansionRatio->setDecimals(1);
    m_udDEXPExpansionRatio->setRange(0.0, 30.0);
    m_udDEXPExpansionRatio->setSingleStep(0.1);
    m_udDEXPExpansionRatio->setValue(tx.dexpExpansionRatioDb());
    // From Thetis setup.designer.cs:44899 [v2.10.3.13] — udDEXPExpansionRatio tooltip.
    m_udDEXPExpansionRatio->setToolTip(QStringLiteral(
        "Ratio between full gain and reduced gain of expander"));

    // udDEXPHysteresisRatio — range 0..10 default 2.0 step 0.1 dp 1 — line 44845-44874
    m_udDEXPHysteresisRatio = new QDoubleSpinBox;
    m_udDEXPHysteresisRatio->setObjectName(QStringLiteral("udDEXPHysteresisRatio"));
    m_udDEXPHysteresisRatio->setDecimals(1);
    m_udDEXPHysteresisRatio->setRange(0.0, 10.0);
    m_udDEXPHysteresisRatio->setSingleStep(0.1);
    m_udDEXPHysteresisRatio->setValue(tx.dexpHysteresisRatioDb());
    // From Thetis setup.designer.cs:44868 [v2.10.3.13] — udDEXPHysteresisRatio tooltip.
    m_udDEXPHysteresisRatio->setToolTip(QStringLiteral(
        "Ratio of activation level and hold level"));

    // udDEXPDetTau — range 1..100 default 20 — line 45070-45098
    m_udDEXPDetTau = new QSpinBox;
    m_udDEXPDetTau->setObjectName(QStringLiteral("udDEXPDetTau"));
    m_udDEXPDetTau->setRange(1, 100);
    m_udDEXPDetTau->setSingleStep(1);
    m_udDEXPDetTau->setValue(static_cast<int>(tx.dexpDetectorTauMs()));
    // From Thetis setup.designer.cs:45092 [v2.10.3.13] — udDEXPDetTau tooltip.
    m_udDEXPDetTau->setToolTip(QStringLiteral(
        "Time-constant for low-pass filtering of input"));

    // Layout: left column = time controls, right column = level/ratio controls.
    // Row labels copied verbatim from the Thetis Designer (see comments).
    int r = 0;
    // Row 0:  "Attack (ms)"      / "Threshold (dBV)"
    //         (lblDEXPAttack:45128 / lblDEXPThreshold:44945)
    addLabelledRow(dexpVoxGrid, r, QStringLiteral("Attack (ms)"),     m_udDEXPAttack);
    {
        auto* lbl = new QLabel(QStringLiteral("Threshold (dBV)"));
        dexpVoxGrid->addWidget(lbl,                r, 2, Qt::AlignLeft | Qt::AlignVCenter);
        dexpVoxGrid->addWidget(m_udDEXPThreshold,  r, 3, Qt::AlignLeft | Qt::AlignVCenter);
    }
    ++r;

    // Row 1:  "Hold (ms)"        / "Exp. Ratio (dB)"
    //         (lblDEXPHold:45118  / lblDEXPExpRatio:44965)
    addLabelledRow(dexpVoxGrid, r, QStringLiteral("Hold (ms)"),       m_udDEXPHold);
    {
        auto* lbl = new QLabel(QStringLiteral("Exp. Ratio (dB)"));
        dexpVoxGrid->addWidget(lbl,                       r, 2, Qt::AlignLeft | Qt::AlignVCenter);
        dexpVoxGrid->addWidget(m_udDEXPExpansionRatio,    r, 3, Qt::AlignLeft | Qt::AlignVCenter);
    }
    ++r;

    // Row 2:  "Release (ms)"     / "Hyst.Ratio (dB)"
    //         (lblDEXPRelease:45108 / lblDEXPHystRatio:44955)
    addLabelledRow(dexpVoxGrid, r, QStringLiteral("Release (ms)"),    m_udDEXPRelease);
    {
        auto* lbl = new QLabel(QStringLiteral("Hyst.Ratio (dB)"));
        dexpVoxGrid->addWidget(lbl,                       r, 2, Qt::AlignLeft | Qt::AlignVCenter);
        dexpVoxGrid->addWidget(m_udDEXPHysteresisRatio,   r, 3, Qt::AlignLeft | Qt::AlignVCenter);
    }
    ++r;

    // Row 3:  (spacer)           / "Det.Tau (ms)"
    //                              (lblDetTau:45138 — verbatim "Det.Tau (ms)" with the dot)
    {
        auto* lbl = new QLabel(QStringLiteral("Det.Tau (ms)"));
        dexpVoxGrid->addWidget(lbl,             r, 2, Qt::AlignLeft | Qt::AlignVCenter);
        dexpVoxGrid->addWidget(m_udDEXPDetTau,  r, 3, Qt::AlignLeft | Qt::AlignVCenter);
    }
    ++r;

    dexpVoxLay->addLayout(dexpVoxGrid);
    dexpVoxLay->addStretch();

    // VOX/DEXP big box — column 0, spans both rows (row=0, col=0, rowSpan=2, colSpan=1).
    row->addWidget(dexpVoxGrp, 0, 0, 2, 1);

    // ══════════════════════════════════════════════════════════════════════════
    // ── Group 2: Audio LookAhead ─────────────────────────────────────────────
    // ══════════════════════════════════════════════════════════════════════════
    //
    // From Thetis setup.designer.cs:44758-44818 [v2.10.3.13] — grpDEXPLookAhead.
    // Title "Audio LookAhead" verbatim from line 44763.

    auto* lookAheadGrp = new QGroupBox(QStringLiteral("Audio LookAhead"));
    lookAheadGrp->setObjectName(QStringLiteral("grpDEXPLookAhead"));
    auto* lookAheadLay = new QVBoxLayout(lookAheadGrp);

    // chkDEXPLookAheadEnable — "Enable" — line 44815, default checked
    m_chkDEXPLookAheadEnable = new QCheckBox(QStringLiteral("Enable"));
    m_chkDEXPLookAheadEnable->setObjectName(QStringLiteral("chkDEXPLookAheadEnable"));
    m_chkDEXPLookAheadEnable->setChecked(tx.dexpLookAheadEnabled());
    // From Thetis setup.designer.cs:44816 [v2.10.3.13] — chkDEXPLookAheadEnable tooltip.
    m_chkDEXPLookAheadEnable->setToolTip(QStringLiteral(
        "Trigger VOX ahead of transmit audio"));
    lookAheadLay->addWidget(m_chkDEXPLookAheadEnable);

    // udDEXPLookAhead — range 10..999 default 60 — line 44765-44793
    m_udDEXPLookAhead = new QSpinBox;
    m_udDEXPLookAhead->setObjectName(QStringLiteral("udDEXPLookAhead"));
    m_udDEXPLookAhead->setRange(10, 999);
    m_udDEXPLookAhead->setSingleStep(1);
    m_udDEXPLookAhead->setValue(static_cast<int>(tx.dexpLookAheadMs()));
    // From Thetis setup.designer.cs:44787 [v2.10.3.13] — udDEXPLookAhead tooltip.
    m_udDEXPLookAhead->setToolTip(QStringLiteral("Time for VOX audio lookahead"));

    auto* lookAheadGrid = new QGridLayout;
    lookAheadGrid->setHorizontalSpacing(10);
    lookAheadGrid->setVerticalSpacing(6);
    // "Look Ahead (ms)" — verbatim from lblDEXPAudioLookAhead.Text:44803
    addLabelledRow(lookAheadGrid, 0,
                   QStringLiteral("Look Ahead (ms)"), m_udDEXPLookAhead);
    lookAheadLay->addLayout(lookAheadGrid);
    lookAheadLay->addStretch();

    // Audio LookAhead small box — column 1, row 0 (top-right cell).
    // AlignTop keeps the box from stretching vertically inside its grid cell.
    row->addWidget(lookAheadGrp, 0, 1, Qt::AlignTop);

    // ══════════════════════════════════════════════════════════════════════════
    // ── Group 3: Side-Channel Trigger Filter ─────────────────────────────────
    // ══════════════════════════════════════════════════════════════════════════
    //
    // From Thetis setup.designer.cs:45153-45260 [v2.10.3.13] — grpSCF.
    // Title "Side-Channel Trigger Filter" verbatim from line 45165.

    auto* scfGrp = new QGroupBox(QStringLiteral("Side-Channel Trigger Filter"));
    scfGrp->setObjectName(QStringLiteral("grpSCF"));
    auto* scfLay = new QVBoxLayout(scfGrp);

    // chkSCFEnable — "Enable" — line 45257, default checked
    m_chkSCFEnable = new QCheckBox(QStringLiteral("Enable"));
    m_chkSCFEnable->setObjectName(QStringLiteral("chkSCFEnable"));
    m_chkSCFEnable->setChecked(tx.dexpSideChannelFilterEnabled());
    // From Thetis setup.designer.cs:45258 [v2.10.3.13] — chkSCFEnable tooltip.
    m_chkSCFEnable->setToolTip(QStringLiteral("Filter audio that triggers VOX"));
    scfLay->addWidget(m_chkSCFEnable);

    // udSCFLowCut — range 100..10000 default 500 step 10 — line 45217-45245
    m_udSCFLowCut = new QSpinBox;
    m_udSCFLowCut->setObjectName(QStringLiteral("udSCFLowCut"));
    m_udSCFLowCut->setRange(100, 10000);
    m_udSCFLowCut->setSingleStep(10);
    m_udSCFLowCut->setValue(static_cast<int>(tx.dexpLowCutHz()));
    // From Thetis setup.designer.cs:45239 [v2.10.3.13] — udSCFLowCut tooltip.
    m_udSCFLowCut->setToolTip(QStringLiteral(
        "Low frequency cut-off for VOX trigger filter"));

    // udSCFHighCut — range 100..10000 default 1500 step 10 — line 45187-45215
    m_udSCFHighCut = new QSpinBox;
    m_udSCFHighCut->setObjectName(QStringLiteral("udSCFHighCut"));
    m_udSCFHighCut->setRange(100, 10000);
    m_udSCFHighCut->setSingleStep(10);
    m_udSCFHighCut->setValue(static_cast<int>(tx.dexpHighCutHz()));
    // From Thetis setup.designer.cs:45209 [v2.10.3.13] — udSCFHighCut tooltip.
    m_udSCFHighCut->setToolTip(QStringLiteral(
        "High frequency cut-off for VOX trigger filter"));

    auto* scfGrid = new QGridLayout;
    scfGrid->setHorizontalSpacing(10);
    scfGrid->setVerticalSpacing(6);
    // "Low  Cut (Hz)"  — verbatim from lblSCFLowCut.Text:45185 (note: the
    //                    Thetis Designer ships TWO spaces between "Low" and
    //                    "Cut"; we preserve that spacing rather than collapse).
    // "High Cut (Hz)"  — verbatim from lblSCFHighCut.Text:45175
    addLabelledRow(scfGrid, 0, QStringLiteral("Low  Cut (Hz)"),  m_udSCFLowCut);
    addLabelledRow(scfGrid, 1, QStringLiteral("High Cut (Hz)"),  m_udSCFHighCut);
    scfLay->addLayout(scfGrid);
    scfLay->addStretch();

    // Side-Channel Trigger Filter small box — column 1, row 1 (bottom-right cell).
    // AlignTop keeps the box from stretching vertically inside its grid cell.
    row->addWidget(scfGrp, 1, 1, Qt::AlignTop);

    // Top-justify the grid: insert the QGridLayout BEFORE the auto-trailing
    // stretch added by SetupPage::init() (SetupPage.cpp:90 [m_contentLayout
    // ->addStretch(1)]).  The previous code called contentLayout()->addLayout
    // followed by addStretch(), which left the grid sandwiched between the
    // auto-stretch (above) and the new explicit stretch (below) — vertically
    // centring the boxes when the dialog page area was taller than the grid.
    // Mirrors the pattern in SetupPage::addSection (SetupPage.cpp:126-127)
    // which uses insertWidget(stretchIndex, group) for the same reason.
    // Bench feedback 2026-05-04.
    {
        const int stretchIndex = contentLayout()->count() - 1;
        contentLayout()->insertLayout(stretchIndex, row);
    }

    // ══════════════════════════════════════════════════════════════════════════
    // ── Bidirectional bindings ───────────────────────────────────────────────
    // ══════════════════════════════════════════════════════════════════════════

    // chkVOXEnable <-> voxEnabled
    connect(m_chkVOXEnable, &QCheckBox::toggled,
            &tx, &TransmitModel::setVoxEnabled);
    connect(&tx, &TransmitModel::voxEnabledChanged,
            m_chkVOXEnable, [this](bool on) {
        QSignalBlocker b(m_chkVOXEnable);
        m_chkVOXEnable->setChecked(on);
    });

    // chkDEXPEnable <-> dexpEnabled
    connect(m_chkDEXPEnable, &QCheckBox::toggled,
            &tx, &TransmitModel::setDexpEnabled);
    connect(&tx, &TransmitModel::dexpEnabledChanged,
            m_chkDEXPEnable, [this](bool on) {
        QSignalBlocker b(m_chkDEXPEnable);
        m_chkDEXPEnable->setChecked(on);
    });

    // udDEXPThreshold <-> voxThresholdDb (shared param)
    connect(m_udDEXPThreshold, QOverload<int>::of(&QSpinBox::valueChanged),
            &tx, &TransmitModel::setVoxThresholdDb);
    connect(&tx, &TransmitModel::voxThresholdDbChanged,
            m_udDEXPThreshold, [this](int dB) {
        QSignalBlocker b(m_udDEXPThreshold);
        m_udDEXPThreshold->setValue(dB);
    });

    // udDEXPHysteresisRatio <-> dexpHysteresisRatioDb
    connect(m_udDEXPHysteresisRatio, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            &tx, &TransmitModel::setDexpHysteresisRatioDb);
    connect(&tx, &TransmitModel::dexpHysteresisRatioDbChanged,
            m_udDEXPHysteresisRatio, [this](double dB) {
        QSignalBlocker b(m_udDEXPHysteresisRatio);
        m_udDEXPHysteresisRatio->setValue(dB);
    });

    // udDEXPExpansionRatio <-> dexpExpansionRatioDb
    connect(m_udDEXPExpansionRatio, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            &tx, &TransmitModel::setDexpExpansionRatioDb);
    connect(&tx, &TransmitModel::dexpExpansionRatioDbChanged,
            m_udDEXPExpansionRatio, [this](double dB) {
        QSignalBlocker b(m_udDEXPExpansionRatio);
        m_udDEXPExpansionRatio->setValue(dB);
    });

    // udDEXPAttack <-> dexpAttackTimeMs (TM stores double; spinbox int — exact
    // round-trip up to integer ms, which is what the Thetis Designer ships)
    connect(m_udDEXPAttack, QOverload<int>::of(&QSpinBox::valueChanged),
            &tx, [&tx](int ms) { tx.setDexpAttackTimeMs(static_cast<double>(ms)); });
    connect(&tx, &TransmitModel::dexpAttackTimeMsChanged,
            m_udDEXPAttack, [this](double ms) {
        QSignalBlocker b(m_udDEXPAttack);
        m_udDEXPAttack->setValue(static_cast<int>(ms));
    });

    // udDEXPHold <-> voxHangTimeMs (shared param, both int ms)
    connect(m_udDEXPHold, QOverload<int>::of(&QSpinBox::valueChanged),
            &tx, &TransmitModel::setVoxHangTimeMs);
    connect(&tx, &TransmitModel::voxHangTimeMsChanged,
            m_udDEXPHold, [this](int ms) {
        QSignalBlocker b(m_udDEXPHold);
        m_udDEXPHold->setValue(ms);
    });

    // udDEXPRelease <-> dexpReleaseTimeMs
    connect(m_udDEXPRelease, QOverload<int>::of(&QSpinBox::valueChanged),
            &tx, [&tx](int ms) { tx.setDexpReleaseTimeMs(static_cast<double>(ms)); });
    connect(&tx, &TransmitModel::dexpReleaseTimeMsChanged,
            m_udDEXPRelease, [this](double ms) {
        QSignalBlocker b(m_udDEXPRelease);
        m_udDEXPRelease->setValue(static_cast<int>(ms));
    });

    // udDEXPDetTau <-> dexpDetectorTauMs
    connect(m_udDEXPDetTau, QOverload<int>::of(&QSpinBox::valueChanged),
            &tx, [&tx](int ms) { tx.setDexpDetectorTauMs(static_cast<double>(ms)); });
    connect(&tx, &TransmitModel::dexpDetectorTauMsChanged,
            m_udDEXPDetTau, [this](double ms) {
        QSignalBlocker b(m_udDEXPDetTau);
        m_udDEXPDetTau->setValue(static_cast<int>(ms));
    });

    // chkDEXPLookAheadEnable <-> dexpLookAheadEnabled
    connect(m_chkDEXPLookAheadEnable, &QCheckBox::toggled,
            &tx, &TransmitModel::setDexpLookAheadEnabled);
    connect(&tx, &TransmitModel::dexpLookAheadEnabledChanged,
            m_chkDEXPLookAheadEnable, [this](bool on) {
        QSignalBlocker b(m_chkDEXPLookAheadEnable);
        m_chkDEXPLookAheadEnable->setChecked(on);
    });

    // udDEXPLookAhead <-> dexpLookAheadMs
    connect(m_udDEXPLookAhead, QOverload<int>::of(&QSpinBox::valueChanged),
            &tx, [&tx](int ms) { tx.setDexpLookAheadMs(static_cast<double>(ms)); });
    connect(&tx, &TransmitModel::dexpLookAheadMsChanged,
            m_udDEXPLookAhead, [this](double ms) {
        QSignalBlocker b(m_udDEXPLookAhead);
        m_udDEXPLookAhead->setValue(static_cast<int>(ms));
    });

    // chkSCFEnable <-> dexpSideChannelFilterEnabled
    connect(m_chkSCFEnable, &QCheckBox::toggled,
            &tx, &TransmitModel::setDexpSideChannelFilterEnabled);
    connect(&tx, &TransmitModel::dexpSideChannelFilterEnabledChanged,
            m_chkSCFEnable, [this](bool on) {
        QSignalBlocker b(m_chkSCFEnable);
        m_chkSCFEnable->setChecked(on);
    });

    // udSCFLowCut <-> dexpLowCutHz
    connect(m_udSCFLowCut, QOverload<int>::of(&QSpinBox::valueChanged),
            &tx, [&tx](int hz) { tx.setDexpLowCutHz(static_cast<double>(hz)); });
    connect(&tx, &TransmitModel::dexpLowCutHzChanged,
            m_udSCFLowCut, [this](double hz) {
        QSignalBlocker b(m_udSCFLowCut);
        m_udSCFLowCut->setValue(static_cast<int>(hz));
    });

    // udSCFHighCut <-> dexpHighCutHz
    connect(m_udSCFHighCut, QOverload<int>::of(&QSpinBox::valueChanged),
            &tx, [&tx](int hz) { tx.setDexpHighCutHz(static_cast<double>(hz)); });
    connect(&tx, &TransmitModel::dexpHighCutHzChanged,
            m_udSCFHighCut, [this](double hz) {
        QSignalBlocker b(m_udSCFHighCut);
        m_udSCFHighCut->setValue(static_cast<int>(hz));
    });
}


} // namespace NereusSDR
