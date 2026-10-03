#pragma once

// =================================================================
// src/gui/setup/hardware/Hl2OptionsTab.h  (NereusSDR)
// =================================================================
//
// Ported from mi0bot-Thetis source:
//   Project Files/Source/Console/setup.designer.cs:tpHL2Options
//   (mi0bot v2.10.3.13-beta2 / @c26a8a4, lines 11075-11718)
//   Project Files/Source/Console/setup.cs handlers:
//     chkSwapAudioChannels_CheckedChanged   (line 38065)
//     chkCl2Enable_CheckedChanged           (line 21732)
//     udCl2Freq_ValueChanged                (line 21738)
//     chkExt10MHz_CheckedChanged            (line 21744)
//     chkDisconnectReset_CheckedChanged     (line 21258)
//     udPTTHang_ValueChanged                (line 21245)
//     udTxBufferLat_ValueChanged            (line 21238)
//     chkHL2PsSync_CheckedChanged           (line 13385)
//     chkHL2BandVolts_CheckedChanged        (line 13377)
//
// HL2 Options page — three group boxes (mi0bot tpHL2Options layout):
//   1. Hermes Lite Options (groupBoxHL2RXOptions, designer.cs:11086-11313)
//      — 9 HL2-specific radio behavior controls bound to Hl2OptionsModel
//   2. I2C Control       (groupBoxI2CControl,  designer.cs:11315-11645)
//      — chkI2CEnable + bus radio + addr/reg/data spinboxes + Read/Write
//   3. I/O Pin State     (grpIOPinState,       designer.cs:11646-11718)
//      — output strip (8 LEDs, click-to-toggle when chkI2CEnable) + input
//        strip (6 LEDs, read-only)
//
// Eight of the groupbox-1 options reach the radio (Band Volts, Disable
// power supply sync, TX buffer latency, PTT hang, Reset on Ethernet
// disconnect, and the clock options Enable CL2, CL2 frequency and External
// 10 MHz; RadioModel::applyHl2Options). Swap audio channels is stored
// per-MAC but not sent (NereusSDR sends the radio no audio of its own over
// P1), so it shows disabled with a plain reason.
//
// Bus 0 surface in I2C Control is **also** deferred per design §4 —
// today only bus 1 is wired in NereusSDR's I2cTxn path.  Rendered as a
// disabled radio with tooltip pointing to the deferral.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-30 — New for Phase 3L HL2 Filter visibility brainstorm.
//                Phase 3L commit #9.  Closes the "no place to map LPF/HPF
//                or see bank state like in Thetis" gap from the design
//                doc problem statement (table row 2).
//                J.J. Boyd (KG4VCF), with AI-assisted transformation
//                via Anthropic Claude Code.
//   2026-09-23 - R-R3-46: the TX buffer latency and PTT hang follow the
//                transmit permission. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-26 - R-R3-46 (remote-window parity Task 14): the I2C tool and
//                Pin Control go through RadioModel (the Core's radio in a
//                remote window), the output strip shows the I/O board's
//                output register read back (mi0bot's ucOutPinsLedStripHF),
//                and the tool follows the Core's offer and the on-air rule.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - Swap audio channels, Enable CL2, CL2 frequency and
//                External 10 MHz are stored but not sent to the radio, so
//                they show disabled with a plain reason; the "wire emission"
//                warnings are gone (five options reach the radio).
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - HL2 clock options: Enable CL2, CL2 frequency and External
//                10 MHz reach the radio and are enabled; a remote window
//                needs a Core that sends them (setClockControlAvailable).
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30 - Radio codec lane: Swap audio channels reaches the radio
//                and is enabled; a remote window needs a Core at
//                radioHardwareVersion 13 (setSwapAudioAvailable).
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================
//
//=================================================================
// setup.cs (mi0bot fork)
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
//=================================================================
// Continual modifications Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//=================================================================
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves his       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

#include <QMap>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QWidget>

class QCheckBox;
class QLabel;
class QShowEvent;
class QPushButton;
class QDoubleSpinBox;
class QSpinBox;

namespace NereusSDR {

class Hl2OptionsModel;
class IoBoardHl2;
class OcLedStripWidget;
class RadioModel;
struct BoardCapabilities;
struct RadioInfo;

class Hl2OptionsTab : public QWidget {
    Q_OBJECT
public:
    explicit Hl2OptionsTab(RadioModel* model, QWidget* parent = nullptr);
    ~Hl2OptionsTab() override;

    // HardwarePage contract — see other hardware sub-tabs.
    void populate(const RadioInfo& info, const BoardCapabilities& caps);
    void restoreSettings(const QMap<QString, QVariant>& settings);

    // R-R3-46: the TX buffer latency and PTT hang are transmit settings;
    // they follow the transmit permission with its reason. Always permitted
    // locally.
    void setTransmitPermitted(bool permitted, const QString& reason);

    // R-R3-46 (parity Task 14): whether this window reaches the radio's
    // I2C bus (always locally; in a remote window, a Core at
    // radioHardwareVersion 7), with the reason when it does not. The I2C
    // tool and Pin Control are disabled with the reason, never hidden.
    void setIoBoardControlAvailable(bool available, const QString& reason);

    // HL2 clock options: whether this window's changes to Enable CL2, CL2
    // frequency and External 10 MHz reach the radio (always locally; in a
    // remote window, a Core at radioHardwareVersion 11), with the reason
    // when they do not. The rows are disabled with the reason, never hidden.
    void setClockControlAvailable(bool available, const QString& reason);

    // HL2 Swap audio channels: whether this window's change reaches the
    // radio (always locally; in a remote window, a Core at
    // radioHardwareVersion 13, which sends its radio the receive audio).
    // Disabled with the reason, never hidden.
    void setSwapAudioAvailable(bool available, const QString& reason);

#ifdef NEREUS_BUILD_TESTS
    bool   transmitTimingsEnabledForTest() const;
    // Test seams — read the underlying state without depending on the
    // QWidget show/hide cycle.
    bool   swapAudioChannelsCheckedForTest() const;
    int    pttHangMsForTest() const;
    int    txLatencyMsForTest() const;
    quint8 outputBitsForTest() const;
    quint8 inputBitsForTest()  const;
    // The input strip draws its lit pins as transmitting.
    bool   inputStripTxForTest() const;
    bool   isI2cWriteEnabledForTest() const;
    bool   isI2cReadEnabledForTest() const;
    bool   isPinControlEnabledForTest() const;
    QString i2cWriteToolTipForTest() const;
    QString pinControlToolTipForTest() const;
    // The four response bytes as shown, space separated.
    QString i2cResponseTextForTest() const;
    // The four byte boxes' tooltips, left to right (C1 box first).
    QStringList i2cByteToolTipsForTest() const;
    QString i2cStatusTextForTest() const;
    // Drive the tool as a click does: enable it (and writes), set the
    // address, register and data, then Read or Write.
    void readI2cForTest(int address, int reg);
    void writeI2cForTest(int address, int reg, int data);
    // A click on output LED `pin` with Pin Control on.
    void clickOutputPinForTest(int pin);
#endif

signals:
    // HardwarePage API compatibility.  This tab persists via Hl2OptionsModel
    // (per-MAC) directly, so this signal is never emitted from this tab —
    // HardwarePage connects to it but never receives a firing.
    void settingChanged(const QString& key, const QVariant& value);

private slots:
    void onI2cEnableToggled(bool on);
    void onI2cReadClicked();
    void onI2cWriteClicked();
    void onOutputPinClicked(int idx);
    void onOutputsChanged();

    // Recomputes m_btnWrite's enabled state from the two gating checkboxes
    // (chkI2cEnable + chkI2cWriteEnable). Wired once in buildI2cControl, and
    // also called directly from onI2cEnableToggled. Replaces an earlier
    // lambda-based connect with Qt::UniqueConnection that fired the Qt
    // "unique connections require a pointer to member function" warning on
    // every Settings dialog open (lambdas can't be deduped) — #272.
    void syncI2cWriteButtonEnabled();

private:
    void buildHermesLiteOptions(QWidget* parent);
    void buildI2cControl(QWidget* parent);
    void buildIoPinState(QWidget* parent);
    void syncFromModel();
    // R-R3-46 (parity Task 14): the tool's enables from the two check
    // boxes, the Core's offer and the on-air rule.
    void applyIoGates();
    // The clock rows' enables from the Core's offer and Enable CL2.
    void applyClockGates();
    void showI2cStatus(const QString& text);

    RadioModel*       m_model{nullptr};
    Hl2OptionsModel*  m_options{nullptr};   // owned by RadioModel
    IoBoardHl2*       m_ioBoard{nullptr};   // owned by RadioModel

    // Hermes Lite Options — 9 controls.
    QCheckBox* m_chkSwapAudio{nullptr};
    QCheckBox* m_chkCl2Enable{nullptr};
    QDoubleSpinBox* m_udCl2Freq{nullptr};
    QCheckBox* m_chkExt10MHz{nullptr};
    QCheckBox* m_chkDisconnectReset{nullptr};
    QSpinBox*  m_udPttHang{nullptr};
    QSpinBox*  m_udTxLatency{nullptr};
    QCheckBox* m_chkPsSync{nullptr};
    QCheckBox* m_chkBandVolts{nullptr};

    // I2C Control — manual R/W tool.
    QCheckBox*   m_chkI2cEnable{nullptr};      // gates the rest of the group
    QCheckBox*   m_chkI2cWriteEnable{nullptr};
    QSpinBox*    m_udI2cAddress{nullptr};      // hex 0x00..0x7F
    QSpinBox*    m_udI2cRegister{nullptr};     // hex 0x00..0xFF (Reg/Control)
    QSpinBox*    m_udI2cWriteData{nullptr};    // hex 0x00..0xFF
    QPushButton* m_btnRead{nullptr};
    QPushButton* m_btnWrite{nullptr};
    // mi0bot's txtI2CByte0..3: byte0 is C4 (the register, rightmost) ..
    // byte3 is C1 (register + 3, leftmost).
    QLabel*      m_byte0Label{nullptr};
    QLabel*      m_byte1Label{nullptr};
    QLabel*      m_byte2Label{nullptr};
    QLabel*      m_byte3Label{nullptr};
    // Why the last request was not done (a refusal or no answer), or empty.
    QLabel*      m_i2cStatusLabel{nullptr};
    bool         m_ioAvailable{true};
    QString      m_ioUnavailableReason;
    bool         m_clockAvailable{true};
    QString      m_clockUnavailableReason;
    bool         m_swapAudioAvailable{true};
    QString      m_swapAudioUnavailableReason;

    // I/O Pin State — two LED strips + Pin Control gate.
    OcLedStripWidget* m_outputStrip{nullptr}; // 8 LEDs (interactive when chkI2CEnable)
    OcLedStripWidget* m_inputStrip{nullptr};  // 6 LEDs (read-only — HL2 6-bit input port)
    QCheckBox*        m_chkPinControl{nullptr};

    // Re-entrancy guards (model→UI vs UI→model loop).
    bool m_syncing{false};

protected:
    // From mi0bot setup.designer.cs:11084 [@c26a8a4]: entering the tab
    // reads the output register back (tpHL2Options.Enter +=
    // ucOutPinsLedStripHF_Click).
    void showEvent(QShowEvent* event) override;
};

} // namespace NereusSDR
