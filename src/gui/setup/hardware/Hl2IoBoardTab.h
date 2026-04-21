#pragma once
// =================================================================
// src/gui/setup/hardware/Hl2IoBoardTab.h  (NereusSDR)
// =================================================================
//
// Ported from mi0bot-Thetis sources:
//   Project Files/Source/Console/setup.cs (~lines 20234-20238 chkHERCULES
//     N2ADR Filter toggle + surrounding HL2 I/O UI)
//   Project Files/Source/Console/console.cs:25781-25945 (UpdateIOBoard
//     state machine driving register state; subscribed via IoBoardHl2 model)
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Phase 3I placeholder (GPIO combos only).
//   2026-04-20 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted transformation via Anthropic
//                Claude Code. Replaces Phase 3I empty placeholder; surfaces
//                IoBoardHl2 + HermesLiteBandwidthMonitor state for HL2
//                diagnostics. NereusSDR spin: register state table + I2C
//                transaction log + state-machine viz + bandwidth mini are
//                pure NereusSDR diagnostic surfaces (mi0bot doesn't expose
//                them in the Thetis UI).
// =================================================================
//
// === Verbatim mi0bot Console/setup.cs header (lines 1-50) ===
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
// =================================================================

#include <QMap>
#include <QVariant>
#include <QWidget>
#include <array>

#include "core/IoBoardHl2.h"

class QCheckBox;
class QFrame;
class QGroupBox;
class QLabel;
class QListWidget;
class QProgressBar;
class QPushButton;
class QTableWidget;
class QTimer;
class QVBoxLayout;

namespace NereusSDR {

class HermesLiteBandwidthMonitor;
class RadioModel;
struct RadioInfo;
struct BoardCapabilities;

// Hl2IoBoardTab — Hardware → HL2 I/O Setup page.
//
// Full diagnostic page for the Hermes Lite 2 I/O board.  Surfaces:
//   - Top status bar: detection LED, I2C address, last-probe timestamp
//   - Configuration panel: N2ADR Filter enable + decoded register rows
//     + Probe / Reset buttons
//   - Register state table: 8 principal registers with addr/hex/decoded cols
//   - 12-step state machine visualiser: per-step circles, current step hilite
//   - I2C transaction log: timestamped enqueue/dequeue listing
//   - Bandwidth monitor mini: EP6/EP2 byte-rate bars + LAN PHY throttle status
//
// Auto-hides the whole widget when the connected board is not HermesLite.
//
// Ports mi0bot setup.cs:20234-20238 (chkHERCULES N2ADR enable) +
// console.cs:25781-25945 (UpdateIOBoard register surface) [@c26a8a4].
// Variant mi0bot.
//
// NereusSDR spin: register state table + I2C transaction log + state-machine
// viz + bandwidth monitor mini are pure NereusSDR diagnostic surfaces
// (mi0bot does not expose them in the Thetis UI).
class Hl2IoBoardTab : public QWidget {
    Q_OBJECT

public:
    explicit Hl2IoBoardTab(RadioModel* model, QWidget* parent = nullptr);
    ~Hl2IoBoardTab() override;

    // Compatibility stubs for HardwarePage generic wiring (populate / restoreSettings
    // / settingChanged).  This tab is self-populating via RadioModel signals, so
    // populate() and restoreSettings() are no-ops.  settingChanged() is emitted by
    // onN2adrToggled() directly via AppSettings — here it satisfies the wire() lambda.
    void populate(const RadioInfo& info, const BoardCapabilities& caps);
    void restoreSettings(const QMap<QString, QVariant>& settings);

signals:
    void settingChanged(const QString& key, const QVariant& value);

private slots:
    void onDetectedChanged(bool detected);
    void onRegisterChanged(NereusSDR::IoBoardHl2::Register reg, quint8 value);
    void onStepAdvanced(int newStep);
    void onI2cQueueChanged();
    void onThrottledChanged(bool throttled);
    void onBwTimerTick();
    void onN2adrToggled(bool checked);
    void onProbeClicked();
    void onResetClicked();

private:
    void buildStatusBar(QVBoxLayout* outer);
    void buildConfigAndRegisterRow(QVBoxLayout* outer);
    void buildStateMachineRow(QVBoxLayout* outer);
    void buildI2cAndBandwidthRow(QVBoxLayout* outer);

    void updateStatusBar(bool detected);
    void refreshAllRegisters();
    void highlightStep(int step);
    void appendI2cLogEntry(const QString& text);
    void updateBwDisplay();

    // Decoded string for a register value (human-readable).
    QString decodeRegister(IoBoardHl2::Register reg, quint8 value) const;

    RadioModel*                   m_model{nullptr};
    IoBoardHl2*                   m_ioBoard{nullptr};
    HermesLiteBandwidthMonitor*   m_bwMonitor{nullptr};

    // ── Status bar ────────────────────────────────────────────────────────────
    QFrame*  m_statusFrame{nullptr};
    QFrame*  m_statusLed{nullptr};
    QLabel*  m_statusLabel{nullptr};
    QLabel*  m_i2cAddrLabel{nullptr};
    QLabel*  m_lastProbeLabel{nullptr};

    // ── Configuration (left column) ───────────────────────────────────────────
    // From mi0bot setup.cs:20234-20238 chkHERCULES [@c26a8a4]
    QCheckBox*   m_n2adrFilter{nullptr};
    QLabel*      m_opModeValue{nullptr};
    QLabel*      m_antennaValue{nullptr};
    QLabel*      m_rfInputsValue{nullptr};
    QLabel*      m_antTunerValue{nullptr};
    QPushButton* m_probeButton{nullptr};
    QPushButton* m_resetButton{nullptr};

    // ── Register table (right column) ─────────────────────────────────────────
    QTableWidget* m_registerTable{nullptr};

    // ── State machine row ─────────────────────────────────────────────────────
    // 12 steps per mi0bot console.cs:25844-25928 UpdateIOBoard [@c26a8a4]
    static constexpr int kSteps = IoBoardHl2::kStateMachineSteps;
    std::array<QFrame*, kSteps>  m_stepFrames{};
    std::array<QLabel*, kSteps>  m_stepNumLabels{};
    std::array<QLabel*, kSteps>  m_stepDescLabels{};
    int m_currentHighlightedStep{-1};

    // ── I2C log ───────────────────────────────────────────────────────────────
    QListWidget* m_i2cLog{nullptr};
    QLabel*      m_queueDepthLabel{nullptr};
    static constexpr int kMaxLogLines = 200;

    // ── Bandwidth mini ────────────────────────────────────────────────────────
    QProgressBar* m_ep6Bar{nullptr};
    QProgressBar* m_ep2Bar{nullptr};
    QLabel*       m_ep6RateLabel{nullptr};
    QLabel*       m_ep2RateLabel{nullptr};
    QLabel*       m_throttleStatusLabel{nullptr};
    QLabel*       m_throttleEventLabel{nullptr};

    // 250 ms timer for live bandwidth readout
    QTimer* m_bwTimer{nullptr};

    // Principal registers shown in the table (in display order).
    struct RegRow {
        IoBoardHl2::Register reg;
        const char*          name;
        int                  displayAddr;  // address shown in addr column
    };
    static const RegRow kRegRows[];
    static constexpr int kRegRowCount = 8;
};

} // namespace NereusSDR
