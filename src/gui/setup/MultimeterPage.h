// =================================================================
// src/gui/setup/MultimeterPage.h  (NereusSDR)
// =================================================================
//
// Task 3.1 — Display → Multimeter setup page.
//
// Ported from Thetis source:
//   Project Files/Source/Console/display.cs (multimeter group controls)
//   Project Files/Source/Console/console.cs (S-meter / dBm / µV unit-mode)
// Original licence from Thetis source is included below.
//
// NereusSDR-original page structure.  Control names and defaults derived from
// Thetis Display→General Multimeter group per design Section 3A.
//
// Controls ported:
//   udDisplayMeterDelay        → MultimeterDelayMs         (MeterPoller interval)
//   udDisplayMultiPeakHoldTime → MultimeterPeakHoldMs      (BarItem peak hold)
//   udDisplayMultiTextHoldTime → MultimeterTextHoldMs      (TextItem hold)
//   udDisplayMeterAvg          → MultimeterAverageWindow   (MeterPoller avg window)
//   udMeterDigitalDelay        → MultimeterDigitalDelayMs  (digital readout delay)
//   chkDisplayMeterShowDecimal → MultimeterShowDecimal     (TextItem decimal flag)
//   radSReading/radDBM/radUV   → MultimeterUnitMode        (S / dBm / uV fan-out)
//   chkSignalHistory           → MultimeterSignalHistoryEnabled
//   udSignalHistoryDuration    → MultimeterSignalHistoryDurationMs
//
// Source cites:
//   From Thetis display.cs (multimeter group controls) [v2.10.3.13]
//   From Thetis console.cs (S-meter / dBm / µV unit-mode radio buttons) [v2.10.3.13]
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-01 — Skeleton created in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-09-23 - R-R3-21 / R-R3-10: the S-meter sample interval is the Core's
//                 setting, disabled while a remote window does not have
//                 the Core's settings.
//                 J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
//   2026-09-23 - R-R3-21: applyPersistedSettings() for startup.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

//=================================================================
// display.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley (W5WC)
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
// Waterfall AGC Modifications Copyright (C) 2013 Phil Harman (VK6APH)
// Transitions to directX and continual modifications Copyright (C) 2020-2025 Richard Samphire (MW0LGE)
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

#pragma once

#include "gui/SetupPage.h"

class QSpinBox;
class QCheckBox;
class QComboBox;
class QPushButton;
class QGroupBox;

namespace NereusSDR {
class HistoryGraphItem;
class MeterItem;

// ---------------------------------------------------------------------------
// Display > Multimeter
// Hosts 8 multimeter polling/display globals + S/dBm/µV unit selector +
// signal history (2 controls).  Cross-link button back to Spectrum Defaults.
//
// Per design Section 3A — folded from Thetis Display→General Multimeter group.
// Unit-mode fan-out across MeterItem subclasses lands in Task 3.2.
// HistoryGraphItem duration wire-up lands in Task 3.3.
// ---------------------------------------------------------------------------
class MultimeterPage : public SetupPage {
    Q_OBJECT
public:
    explicit MultimeterPage(RadioModel* model, QWidget* parent = nullptr);

    // R-R3-21 / R-R3-10: the meter sample interval is the Core's (it samples the
    // meter), so it is disabled while the Core's settings are unavailable;
    // the rest of the page is this computer's display.
    void setStationSettingsAvailable(bool available, const QString& reason) override;

    // R-R3-21: applies the saved averaging window, signal unit,
    // show-decimal and signal-history duration to the live meter
    // poller and meter items, as opening this page does. MainWindow calls
    // it at startup so the values survive a restart without opening Setup.
    static void applyPersistedSettings(RadioModel* model);
    // The same for one meter item (unit, decimal, history duration), for an
    // item added while the window runs.
    static void applyPersistedSettingsTo(MeterItem* item);
    // The saved meter update interval to the meter poller (the Core's value
    // in a remote window, applied again when its settings arrive).
    static void applyPersistedMeterInterval(RadioModel* model);

signals:
    /// Emitted when the user clicks the "← Spectrum defaults" cross-link.
    /// SetupDialog connects this to selectPage("Spectrum Defaults").
    void backToSpectrumDefaultsRequested();

private:
    void buildUI();
    void loadSettings();
    void connectSignals();

    // Multimeter group (6 spinboxes + 1 checkbox)
    // From Thetis display.cs multimeter group [v2.10.3.13]
    QSpinBox*   m_delayMs{nullptr};          // udDisplayMeterDelay
    QSpinBox*   m_peakHoldMs{nullptr};       // udDisplayMultiPeakHoldTime
    QSpinBox*   m_textHoldMs{nullptr};       // udDisplayMultiTextHoldTime
    QSpinBox*   m_avgWindow{nullptr};        // udDisplayMeterAvg
    QSpinBox*   m_digitalDelayMs{nullptr};   // udMeterDigitalDelay
    QCheckBox*  m_showDecimal{nullptr};      // chkDisplayMeterShowDecimal

    // Signal Units group — collapses Thetis radio buttons into combo
    // From Thetis console.cs radSReading / radDBM / radUV [v2.10.3.13]
    QComboBox*  m_unitMode{nullptr};

    // Signal History group
    // From Thetis console.cs chkSignalHistory + udSignalHistoryDuration [v2.10.3.13]
    QCheckBox*  m_signalHistoryEnable{nullptr};
    QSpinBox*   m_signalHistoryDurationMs{nullptr};

    // Cross-link
    QPushButton* m_backBtn{nullptr};
};

}  // namespace NereusSDR
