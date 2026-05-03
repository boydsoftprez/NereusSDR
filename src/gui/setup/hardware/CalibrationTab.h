// =================================================================
// src/gui/setup/hardware/CalibrationTab.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/setup.cs
//     (tpGeneralCalibration group: udHPSDRFreqCorrectFactor, chkUsing10MHzRef,
//      udHPSDRFreqCorrectFactor10MHz, btnHPSDRFreqCalReset10MHz,
//      udGeneralCalFreq1, btnGeneralCalFreqStart, udGeneralCalFreq2,
//      udGeneralCalLevel, btnGeneralCalLevelStart, btnResetLevelCal,
//      ud6mLNAGainOffset, ud6mRx2LNAGainOffset, udTXDisplayCalOffset
//      -- lines 5137-5144; 6470-6525; 13966-13967; 14036-14050; 14325-14333;
//         17243-17248; 18315-18317; 22690-22706),
//      original licence from Thetis source is included below
//   Project Files/Source/Console/console.cs
//     (CalibrateFreq, CalibrateLevel, RXCalibrationOffset, CalibratedPAPower
//      -- lines 9764-9844; 9844-10215; 21022-21086; 6691-6724),
//      original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 -- Original PaCalibrationTab implementation.
//   2026-04-20 -- Renamed PaCalibrationTab -> CalibrationTab; expanded to 5
//                  group boxes matching Thetis General -> Calibration 1:1;
//                  backed by CalibrationController model (Phase 3P-G).
//                  J.J. Boyd (KG4VCF), with AI-assisted transformation via
//                  Anthropic Claude Code.
// =================================================================

// --- From setup.cs ---

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

// --- From console.cs ---

//=================================================================
// console.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
// Credit is given to Sizenko Alexander of Style-7 (http://www.styleseven.com/) for the Digital-7 font.
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
// Modifications to support the Behringer Midi controllers
// by Chris Codella, W2PA, May 2017.  Indicated by //-W2PA comment lines.
// Modifications for using the new database import function.  W2PA, 29 May 2017
// Support QSK, possible with Protocol-2 firmware v1.7 (Orion-MkI and Orion-MkII), and later.  W2PA, 5 April 2019
// Modfied heavily - Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
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

// Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12

#pragma once

#include <QVariant>
#include <QWidget>

class QCheckBox;
class QDoubleSpinBox;
class QGroupBox;
class QPushButton;

namespace NereusSDR {

class RadioModel;
class CalibrationController;
struct RadioInfo;
struct BoardCapabilities;

// CalibrationTab -- Hardware -> Calibration Setup page.
//
// Hosts 5 group boxes matching Thetis General -> Calibration 1:1:
//   1. Freq Cal -- frequency calibration trigger + helptext
//   2. Level Cal -- reference freq/level + per-RX 6m LNA offsets
//   3. HPSDR Freq Cal Diagnostic -- correction factor + 10 MHz ext ref toggle
//   4. TX Display Cal -- TX display dB offset
//   5. Volts/Amps Calibration -- Amp sensitivity / Amp Voff / log toggle /
//        default-restore button (Thetis groupBoxTS27 contents:
//        setup.designer.cs:11672-11677 [v2.10.3.13])
//
// Setup IA reshape Phase 3A (2026-05-02) migrated the per-board PA forward-
// power cal spinbox group out to PA → Watt Meter (PaWattMeterPage); see
// docs/architecture/2026-05-02-p1-full-parity-plan.md Setup IA reshape Phase 3A.
// Phase 3B (2026-05-02) relabelled Group 5 from the NereusSDR-original
// "PA Current (A) calculation" to the Thetis-faithful "Volts/Amps Calibration"
// (groupBoxTS27) and renamed m_paSensSpin/m_paOffsetSpin/m_paDefaultBtn to
// m_ampSensSpin/m_ampVoffSpin/m_ampDefaultBtn for upstream-naming consistency.
//
// Backed by CalibrationController (Phase 3P-G). Per-MAC persistence via
// AppSettings under hardware/<mac>/cal/ + hardware/<mac>/paCalibration/.
//
// Renamed from PaCalibrationTab (Phase 3P-G commit 2).
class CalibrationTab : public QWidget {
    Q_OBJECT
public:
    explicit CalibrationTab(RadioModel* model, QWidget* parent = nullptr);
    void populate(const RadioInfo& info, const BoardCapabilities& caps);
    void restoreSettings(const QMap<QString, QVariant>& settings);

#ifdef NEREUS_BUILD_TESTS
    // Test seam: counts QGroupBox children of the main layout.
    int groupBoxCountForTest() const;
#endif

signals:
    void settingChanged(const QString& key, const QVariant& value);

private slots:
    void onControllerChanged();

private:
    void syncFromController();

    RadioModel*            m_model{nullptr};
    CalibrationController* m_calCtrl{nullptr};  // non-owning -- owned by RadioModel

    // -- Group 1: Freq Cal -----------------------------------------------------
    // Source: setup.cs:6470-6511 btnGeneralCalFreqStart + udGeneralCalFreq1 [@501e3f5]
    QDoubleSpinBox* m_freqCalFreqSpin{nullptr};  // udGeneralCalFreq1
    QPushButton*    m_freqCalStartBtn{nullptr};  // btnGeneralCalFreqStart

    // -- Group 2: Level Cal ----------------------------------------------------
    // Source: setup.cs:6482-6525 btnGeneralCalLevelStart + udGeneralCalFreq2
    //         + udGeneralCalLevel + btnResetLevelCal [@501e3f5]
    QDoubleSpinBox* m_levelCalFreqSpin{nullptr};   // udGeneralCalFreq2
    QDoubleSpinBox* m_levelCalLevelSpin{nullptr};  // udGeneralCalLevel
    QDoubleSpinBox* m_rx1LnaSpin{nullptr};         // ud6mLNAGainOffset
    QDoubleSpinBox* m_rx2LnaSpin{nullptr};         // ud6mRx2LNAGainOffset
    QPushButton*    m_levelCalStartBtn{nullptr};   // btnGeneralCalLevelStart
    QPushButton*    m_levelCalResetBtn{nullptr};   // btnResetLevelCal

    // -- Group 3: HPSDR Freq Cal Diagnostic ------------------------------------
    // Source: setup.cs:5137-5144; 14036-14050; 22690-22706 [@501e3f5]
    QDoubleSpinBox* m_freqFactorSpin{nullptr};        // udHPSDRFreqCorrectFactor
    QPushButton*    m_freqFactorResetBtn{nullptr};    // reset to 1.0
    QCheckBox*      m_use10MhzCheck{nullptr};         // chkUsing10MHzRef
    QDoubleSpinBox* m_freqFactor10MSpin{nullptr};     // udHPSDRFreqCorrectFactor10MHz
    QPushButton*    m_freqFactor10MResetBtn{nullptr}; // btnHPSDRFreqCalReset10MHz

    // -- Group 4: TX Display Cal -----------------------------------------------
    // Source: setup.cs:14325-14333 udTXDisplayCalOffset [@501e3f5]
    QDoubleSpinBox* m_txDisplayOffsetSpin{nullptr}; // udTXDisplayCalOffset

    // -- Group 5: Volts/Amps Calibration -- Thetis groupBoxTS27 equivalent. ----
    // Source: Thetis setup.designer.cs:11672-11677 (groupBoxTS27 contents:
    //         chkLogVoltsAmps + btnAmpDefault + udAmpSens + udAmpVoff)
    //         + console.cs:24893 _amp_voff = 360.0f default [v2.10.3.13]
    // Field-by-field mapping (NereusSDR <-> Thetis):
    //   m_ampSensSpin       <-> udAmpSens
    //   m_ampVoffSpin       <-> udAmpVoff   (default 360.0f per console.cs:24893)
    //   m_ampDefaultBtn     <-> btnAmpDefault
    //   m_logVoltsAmpsCheck <-> chkLogVoltsAmps
    QDoubleSpinBox* m_ampSensSpin{nullptr};
    QDoubleSpinBox* m_ampVoffSpin{nullptr};
    QPushButton*    m_ampDefaultBtn{nullptr};
    QCheckBox*      m_logVoltsAmpsCheck{nullptr};  // chkLogVoltsAmps -- console.cs:27457 [@501e3f5]

    // Note: the per-board PA forward-power cal spinbox group (PaCalibrationGroup)
    // was migrated to PA → Watt Meter (PaWattMeterPage) on 2026-05-02 as part of
    // Setup IA reshape Phase 3A.  See docs/architecture/2026-05-02-p1-full-parity-plan.md.

    // Echo-loop guard: prevents model->UI update from triggering UI->model write
    bool m_updatingFromModel{false};
};

} // namespace NereusSDR
