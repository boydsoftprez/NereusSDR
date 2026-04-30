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
// Wire-format emission for the 8 NetworkIO.Set*() handlers in groupbox 1
// is **explicitly deferred** to a Phase 3L follow-up PR per the design
// doc §4.  This commit ships UI + per-MAC AppSettings persistence only;
// the C&C bank-2 byte composition is a separate source-first port
// session.  Toggling a control here updates Hl2OptionsModel and
// persists, with a `qCWarning` flagging the missing wire emission.
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
#include <QVariant>
#include <QWidget>

class QCheckBox;
class QLabel;
class QPushButton;
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

#ifdef NEREUS_BUILD_TESTS
    // Test seams — read the underlying state without depending on the
    // QWidget show/hide cycle.
    bool   swapAudioChannelsCheckedForTest() const;
    int    pttHangMsForTest() const;
    int    txLatencyMsForTest() const;
    quint8 outputBitsForTest() const;
    quint8 inputBitsForTest()  const;
    bool   isI2cWriteEnabledForTest() const;
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
    void onIoBoardOcByteChanged(quint8 ocByte, int bandIdx, bool mox);

private:
    void buildHermesLiteOptions(QWidget* parent);
    void buildI2cControl(QWidget* parent);
    void buildIoPinState(QWidget* parent);
    void syncFromModel();

    RadioModel*       m_model{nullptr};
    Hl2OptionsModel*  m_options{nullptr};   // owned by RadioModel
    IoBoardHl2*       m_ioBoard{nullptr};   // owned by RadioModel

    // Hermes Lite Options — 9 controls.
    QCheckBox* m_chkSwapAudio{nullptr};
    QCheckBox* m_chkCl2Enable{nullptr};
    QSpinBox*  m_udCl2Freq{nullptr};
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
    QLabel*      m_byte0Label{nullptr};        // C1 = data[0] etc.
    QLabel*      m_byte1Label{nullptr};
    QLabel*      m_byte2Label{nullptr};
    QLabel*      m_byte3Label{nullptr};

    // I/O Pin State — two LED strips + Pin Control gate.
    OcLedStripWidget* m_outputStrip{nullptr}; // 8 LEDs (interactive when chkI2CEnable)
    OcLedStripWidget* m_inputStrip{nullptr};  // 6 LEDs (read-only — HL2 6-bit input port)
    QCheckBox*        m_chkPinControl{nullptr};

    // Re-entrancy guards (model→UI vs UI→model loop).
    bool m_syncing{false};
};

} // namespace NereusSDR
