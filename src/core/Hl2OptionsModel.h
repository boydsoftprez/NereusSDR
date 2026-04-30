#pragma once

// =================================================================
// src/core/Hl2OptionsModel.h  (NereusSDR)
// =================================================================
//
// Ported from mi0bot-Thetis source:
//   Project Files/Source/Console/setup.designer.cs:groupBoxHL2RXOptions
//   (mi0bot v2.10.3.13-beta2 / @c26a8a4, lines 11086-11313)
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
// State container for the 9 HL2-specific radio behavior knobs surfaced
// in mi0bot's tpHL2Options.groupBoxHL2RXOptions.  Per-MAC AppSettings
// persistence under hardware/<mac>/hl2/<key>.  Defaults match mi0bot's
// designer-set initial values verbatim.
//
// Wire-format emission for the 8 NetworkIO.Set*() calls those handlers
// make is **explicitly deferred** to a follow-up Phase 3L PR (per the
// design doc §4 Out-of-scope list).  This commit ships UI + persistence
// only; the C&C bank-2 byte composition for the 8 NetworkIO setters is
// a separate source-first port session.  Toggling a knob here updates
// the persisted state and emits a Qt signal — wire emission is a TODO.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-30 — New for Phase 3L HL2 Filter visibility brainstorm.
//                Phase 3L commit #9 — Hl2OptionsTab.
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

#include <QObject>
#include <QString>

namespace NereusSDR {

// HL2-specific radio behavior knobs (mi0bot tpHL2Options.groupBoxHL2RXOptions).
class Hl2OptionsModel : public QObject {
    Q_OBJECT
public:
    explicit Hl2OptionsModel(QObject* parent = nullptr);

    // ── Defaults — mirror mi0bot setup.designer.cs values [v2.10.3.13-beta2] ─

    // From mi0bot setup.designer.cs:11252 — udPTTHang.Value = 12 (range 0..30)
    static constexpr int kDefaultPttHangMs   = 12;
    static constexpr int kPttHangMinMs       = 0;
    static constexpr int kPttHangMaxMs       = 30;

    // From mi0bot setup.designer.cs:11283 — udTxBufferLat.Value = 20 (range 0..70)
    static constexpr int kDefaultTxLatencyMs = 20;
    static constexpr int kTxLatencyMinMs     = 0;
    static constexpr int kTxLatencyMaxMs     = 70;

    // From mi0bot setup.designer.cs:11159 — udCl2Freq.Value = 116 (range 1..200, MHz)
    static constexpr int kDefaultCl2FreqMHz  = 116;
    static constexpr int kCl2FreqMinMHz      = 1;
    static constexpr int kCl2FreqMaxMHz      = 200;

    // ── Per-MAC AppSettings rooted at hardware/<mac>/hl2/<key> ───────────────
    void setMacAddress(const QString& mac);
    QString macAddress() const { return m_mac; }

    void load();
    void save() const;

    // ── 9 properties (4 ints + 5 bools — bool count includes the 4 not-paired
    //     checkboxes plus chkCl2Enable; udCl2Freq + chkCl2Enable are paired) ──
    bool swapAudioChannels()   const { return m_swapAudioChannels; }
    bool cl2Enabled()          const { return m_cl2Enabled; }
    int  cl2FreqMHz()          const { return m_cl2FreqMHz; }
    bool ext10MHz()            const { return m_ext10MHz; }
    bool disconnectReset()     const { return m_disconnectReset; }
    int  pttHangMs()           const { return m_pttHangMs; }
    int  txLatencyMs()         const { return m_txLatencyMs; }
    bool psSync()              const { return m_psSync; }
    bool bandVolts()           const { return m_bandVolts; }

public slots:
    void setSwapAudioChannels(bool on);
    void setCl2Enabled(bool on);
    void setCl2FreqMHz(int mhz);
    void setExt10MHz(bool on);
    void setDisconnectReset(bool on);
    void setPttHangMs(int ms);
    void setTxLatencyMs(int ms);
    void setPsSync(bool on);
    void setBandVolts(bool on);

signals:
    void swapAudioChannelsChanged(bool on);
    void cl2EnabledChanged(bool on);
    void cl2FreqMHzChanged(int mhz);
    void ext10MHzChanged(bool on);
    void disconnectResetChanged(bool on);
    void pttHangMsChanged(int ms);
    void txLatencyMsChanged(int ms);
    void psSyncChanged(bool on);
    void bandVoltsChanged(bool on);

    // Fires whenever any property mutates — UI re-syncs.
    void changed();

private:
    QString m_mac;

    // Defaults match mi0bot designer values (chk*.Checked = false by default
    // for all 8 checkboxes; spinbox defaults from setup.designer.cs above).
    bool m_swapAudioChannels{false};
    bool m_cl2Enabled{false};
    int  m_cl2FreqMHz{kDefaultCl2FreqMHz};
    bool m_ext10MHz{false};
    bool m_disconnectReset{false};
    int  m_pttHangMs{kDefaultPttHangMs};
    int  m_txLatencyMs{kDefaultTxLatencyMs};
    bool m_psSync{false};
    bool m_bandVolts{false};
};

} // namespace NereusSDR
