// =================================================================
// src/gui/setup/DspOptionsPage.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/setup.cs
//   Project Files/Source/Console/setup.Designer.cs
//   original licence from Thetis source is included below
//
// Thetis version: v2.10.3.13 (git commit 501e3f5)
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-01 — Ported in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via
//                 Anthropic Claude Code.
//                 Task 4.1: DspOptionsPage skeleton + 18 controls.
//                 Mirrors Thetis DSP Options tab (design Section 4A).
//   2026-09-23 - R-R3-21: static high-resolution filter fan-out helpers
//                 for startup. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-24 - R-R3-49 (parity Task 1): the nine TX combos follow the
//                 transmit settings gate. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-09-26 - R-R3-49 (parity Task 16): applyCoreFilterResponse, a
//                 remote window's graphs drawing the Core's curve. J.J.
//                 Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
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

#pragma once

#include "core/WdspTypes.h"  // DSPMode
#include "gui/SetupPage.h"

class QComboBox;
class QCheckBox;
class QLabel;
class QGroupBox;

namespace NereusSDR {

class MeterItem;

// Setup → DSP → Options page.
//
// Mirrors Thetis tpDSPOptions tab layout (design Section 4A).
// Four group boxes + 1 standalone checkbox + readout label + 3 warning icons.
//
// Group 1 — Buffer Size (IQcomp): 4 combos (Phone/CW/Digital/FM)
//   From Thetis setup.Designer.cs grpDSPBufferSize [v2.10.3.13].
//   Each combo has RX and TX in Thetis; NereusSDR collapses to per-mode
//   (Task 4.2 applies the same value to both RX and TX channels).
//
// Group 2 — Filter Size (taps): 4 combos (Phone/CW/Digital/FM)
//   From Thetis setup.Designer.cs grpDSPFilterSize [v2.10.3.13].
//
// Group 3 — Filter Type: 7 combos (Phone RX/TX, CW RX, Dig RX/TX, FM RX/TX)
//   From Thetis setup.Designer.cs grpDSPFiltTypePhone / grpDSPFiltTypeDig /
//   grpDSPFiltTypeCW / grpDSPFiltTypeFM [v2.10.3.13].
//
// Group 4 — Filter Impulse Cache: 2 checkboxes
//   Wired in Task 4.3.
//
// Standalone: High-resolution filter characteristics checkbox (Task 4.4).
// Time-to-last-change readout: subscribes to RadioModel::dspChangeMeasured in Task 4.6.
// Warning icons: visibility logic wired in Task 4.5.

class DspOptionsPage : public SetupPage {
    Q_OBJECT
public:
    explicit DspOptionsPage(RadioModel* model, QWidget* parent = nullptr);

    // ── Public static helpers (also exposed for unit tests) ──────────────────
    //
    // From Thetis console.cs:38797-38803 [v2.10.3.13] — "Different" checks.
    // Returns true when the combo values are NOT all equal (warning condition).
    // 4-way: phone/cw/dig/fm  (RX side — all 4 modes).
    // 3-way: phone/dig/fm     (TX side — CW has no TX buf combo in Thetis).
    static bool comboValuesDiffer4(QComboBox* a, QComboBox* b,
                                   QComboBox* c, QComboBox* d);
    static bool comboValuesDiffer3(QComboBox* a, QComboBox* b, QComboBox* c);

    // R-R3-21: fan the high-resolution filter setting (and channel 0's
    // RxChannel, or none) out to every FilterDisplayItem. The persisted
    // form reads DspOptionsHighResFilterCharacteristics. Neither saves.
    // MainWindow calls the persisted form at startup and when the receive
    // channel is created or destroyed, so the setting survives a restart
    // without opening Setup.
    static void applyHighResFilter(RadioModel* model, bool highRes);
    static void applyPersistedHighResFilter(RadioModel* model);
    // The same for one meter item added while the window runs.
    static void applyPersistedHighResFilterTo(RadioModel* model, MeterItem* item);
    // R-R3-49 (parity Task 16): a remote window's graphs draw the Core's
    // curve (RadioModel::coreFilterResponse). MainWindow calls it when the
    // curve changes. Nothing on a local model.
    static void applyCoreFilterResponse(RadioModel* model);

    QCheckBox* highResolutionFilterCharacteristicsCheckBox() const noexcept
    {
        return m_highResFilterChars;
    }

    // R-R3-49 (parity Task 1): the TX combos (buffer size, filter size
    // and filter type for SSB/AM, FM and Digital) follow the transmit
    // settings gate: a Core at transmitSettingsVersion 1 takes them while
    // its radio is off the air. The RX combos and the rest of the page do
    // not follow it.
    void setTransmitSettingsPermitted(bool permitted, const QString& reason) override;

private:
    void buildUI();

    // The TX combos' transmit settings gate and the Buffer Size (IQcomp)
    // group's on-the-air lock, applied together: Thetis greys
    // grpDSPBufferSize, RX and TX combos alike, while MOX is on
    // (setup.cs:5159 [v2.10.3.15]). A remote window follows its Core's
    // air state (RadioModel::coreOnAirChanged).
    void refreshBufferAndTransmitGates();
    bool    m_transmitSettingsPermitted{true};
    QString m_transmitSettingsReason;
    bool    m_onAir{false};

    // Wire a per-mode combo to persist to AppSettings AND trigger a WDSP
    // channel rebuild if the combo's mode matches the current slice mode
    // (design Section 4B: live-apply if same mode, persist-only otherwise).
    //
    // comboMode: the DSPMode group this combo belongs to (e.g., DSPMode::USB
    //            for the Phone buffer combo, DSPMode::CWU for CW).
    // key: AppSettings key (e.g., "DspOptionsBufferSizePhone").
    void wireComboWithLiveApply(QComboBox* combo, DSPMode comboMode,
                                const QString& key);

    // ── Group 1: Buffer Size (RX + TX per mode, Thetis-faithful split) ──────
    // From Thetis setup.Designer.cs grpDSPBufPhone/FM/CW/Dig [v2.10.3.13].
    // comboDSPPhoneRXBuf Items: "64","128","256","512","1024".
    // CW has RX only — Thetis omits CW TX (firmware-handled per
    // console.cs:38891-38897 [v2.10.3.13]).
    QComboBox* m_bufPhoneRx{nullptr};
    QComboBox* m_bufPhoneTx{nullptr};
    QComboBox* m_bufFmRx{nullptr};
    QComboBox* m_bufFmTx{nullptr};
    QComboBox* m_bufCwRx{nullptr};      // CW RX only — no TX
    QComboBox* m_bufDigRx{nullptr};
    QComboBox* m_bufDigTx{nullptr};

    // ── Group 2: Filter Size (RX + TX per mode) ─────────────────────────────
    // From Thetis setup.Designer.cs grpDSPFiltSizePhone/FM/CW/Dig [v2.10.3.13].
    // comboDSPPhoneRXFiltSize Items: "1024","2048","4096","8192","16384".
    QComboBox* m_filtSizePhoneRx{nullptr};
    QComboBox* m_filtSizePhoneTx{nullptr};
    QComboBox* m_filtSizeFmRx{nullptr};
    QComboBox* m_filtSizeFmTx{nullptr};
    QComboBox* m_filtSizeCwRx{nullptr}; // CW RX only — no TX
    QComboBox* m_filtSizeDigRx{nullptr};
    QComboBox* m_filtSizeDigTx{nullptr};

    // ── Group 3: Filter Type ─────────────────────────────────────────────────
    // From Thetis setup.Designer.cs grpDSPFiltType* [v2.10.3.13].
    // comboDSPPhoneRXFiltType Items: "Linear Phase","Low Latency"
    // CW has RX only — Thetis has no comboDSPCWTXFiltType.
    QComboBox* m_filtTypePhoneRx{nullptr};
    QComboBox* m_filtTypePhoneTx{nullptr};
    QComboBox* m_filtTypeCwRx{nullptr};
    QComboBox* m_filtTypeDigRx{nullptr};
    QComboBox* m_filtTypeDigTx{nullptr};
    QComboBox* m_filtTypeFmRx{nullptr};
    QComboBox* m_filtTypeFmTx{nullptr};

    // ── Group 4: Impulse Cache ───────────────────────────────────────────────
    // Wired to WDSP impulse cache API in Task 4.3.
    QCheckBox* m_cacheImpulse{nullptr};
    QCheckBox* m_cacheImpulseSaveRestore{nullptr};

    // ── Standalone ───────────────────────────────────────────────────────────
    // Task 4.4 wires this to FilterDisplayItem rendering.
    QCheckBox* m_highResFilterChars{nullptr};

    // ── Live readout ─────────────────────────────────────────────────────────
    // Subscribes to RadioModel::dspChangeMeasured(qint64) in Task 4.6.
    QLabel*    m_timeToLastChangeLabel{nullptr};

    // ── Warning icons ────────────────────────────────────────────────────────
    // Visibility driven by recomputeWarnings() (Task 4.5).
    // Ported from Thetis console.cs:38797-38807 + setup.cs:22391-22400 [v2.10.3.13].
    QLabel*    m_warnBufferSize{nullptr};
    QLabel*    m_warnFilterSize{nullptr};
    QLabel*    m_warnBufferType{nullptr};

    // Recomputes and applies warning-icon visibility.
    // Called on every combo change and once at end of constructor.
    //
    // From Thetis console.cs:38797-38807 [v2.10.3.13] — UpdateDSP() validity logic:
    //   bufferSizeDifferentRX  = !(phone==fm && fm==cw && cw==dig)    (4-way RX)
    //   filterSizeDifferentRX  = !(phone==fm && fm==cw && cw==dig)
    //   filterTypeDifferentRX  = !(phone==fm && fm==cw && cw==dig)
    //   bufferSizeDifferentTX  = !(phone==fm && fm==dig)              (3-way TX; no CW TX)
    //   filterSizeDifferentTX  = !(phone==fm && fm==dig)
    //   filterTypeDifferentTX  = !(phone==fm && fm==dig)
    // NereusSDR collapses RX+TX to per-mode combos; RX maps to the 4 mode combos,
    // TX maps to the 3 TX-capable mode combos (phone/dig/fm — no CW TX buf in Thetis).
    void recomputeWarnings();
};

}  // namespace NereusSDR
