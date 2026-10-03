// =================================================================
// src/core/safety/TxInhibitMonitor.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis [v2.10.3.13 @501e3f5]:
//   Project Files/Source/Console/console.cs
//
// Original licence from the Thetis source file is included below,
// verbatim, with // --- From [filename] --- marker per
// CLAUDE.md "Byte-for-byte headers and multi-file attribution".
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-25 — Ported to C++20/Qt6 for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted transformation via
//                Anthropic Claude Code.
//                Task: Phase 3M-0 Task 4 — TxInhibitMonitor
//                Ports PollTXInhibit (console.cs:25801-25839 [v2.10.3.13]).
//                notifyRxOnly, notifyOutOfBand, notifyBlockTxAntenna are
//                NereusSDR-native aggregation paths; doc-comment cites
//                reference console.cs:15283-15307, console.cs:6770-6806,
//                console.cs:29435-29481, and Andromeda.cs:285-306 as the
//                Thetis upstream context for each inhibit source.
//                (notifyRxOnly removed by Task 16, below.)
//   2026-09-25 - Task 13 (receiver and transmit gaps plan): reads the
//                radio's own TX inhibit input the way PollTXInhibit does
//                (console.cs:25849-25887 [v2.10.3.15]): attachRadioInput,
//                setRadioModel, notifyUserDigitalInputs and the per-model
//                bit choice inhibitInputFromUserIo. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-25 - Task 16 (receiver and transmit gaps plan): notifyRxOnly
//                and the Rx2OnlyRadio source removed. It never had a
//                caller, and this monitor acts only while External TX
//                Inhibit is on; Thetis keeps _rx_only apart from
//                _tx_inhibit (console.cs:15312-15334, 25470 [v2.10.3.15]),
//                so receive only is MoxController::setRxOnly. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - HL2 port part 2: the Hermes Lite 2 I/O board's fault
//                register is a source (IoBoardFault, notifyIoBoardFault),
//                following mi0bot-Thetis console.cs UpdateIOBoard
//                25874-25888 [@c26a8a4], which sets TXInhibit on a
//                non-zero REG_FAULT whatever the External TX Inhibit box
//                says. Held while the last read is non-zero and cleared
//                by a read of zero (operator ruling 2026-09-29; mi0bot
//                never clears it). J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
// =================================================================

// --- From console.cs ---
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
//
// Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12

#pragma once

#include "core/HpsdrModel.h"

#include <QObject>
#include <QTimer>
#include <cstdint>
#include <functional>

namespace NereusSDR::safety {

/// GPIO-poll-based TX inhibit monitor ported from Thetis PollTXInhibit.
///
/// Aggregates three inhibit predicates into a single inhibited() state and
/// emits txInhibitedChanged(bool, Source) on transitions only (not per tick).
///
/// Source priority (highest → lowest):
///   IoBoardFault > UserIo01 > OutOfBand > BlockTxAntenna > None
///
/// IoBoardFault: the Hermes Lite 2 I/O board reported a non-zero fault
///   code (REG_FAULT). It holds transmit off whether or not External TX
///   Inhibit is on, as mi0bot's UpdateIOBoard sets TXInhibit without
///   looking at _useTxInhibit.
///   Cite: mi0bot-Thetis console.cs:25876-25885 [@c26a8a4].
///
/// UserIo01 — per-board GPIO pin polled at 100 ms.
///   Cite: console.cs:25801-25839 [v2.10.3.13] (PollTXInhibit loop).
///   In production the radio's input is read through attachRadioInput /
///   notifyUserDigitalInputs, with the per-model bit choice and the
///   active-low flip made here (inhibitInputFromUserIo, Task 13). A test
///   reader (setUserIoReader) returns the logical ASSERTED state instead.
///   setReverseLogic() applies an additional inversion on top of either.
///
/// Receive only is not an inhibit source here (Task 16): it is
///   MoxController::setRxOnly, Thetis _rx_only, which gates keying whether
///   or not External TX Inhibit is on.
///
/// OutOfBand — VFO frequency falls outside a legal TX band.
///   Cite: console.cs:6770-6806 [v2.10.3.13] (CheckValidTXFreq).
///
/// BlockTxAntenna — Alex RX-only antenna is selected for TX port.
///   Cite: console.cs:29435-29481 [v2.10.3.13] (MOX-entry rejection);
///         Andromeda.cs:285-306 [v2.10.3.13] (AlexANT[2,3]RXOnly).
///
/// RadioModel feeds it from the connection's status frames (Task 13).
///
/// Thread safety: all public methods (notify*(), setEnabled(), setReverseLogic(),
/// setUserIoReader()) must be called on the same thread the object lives on.
/// The QTimer fires on that same thread. Cross-thread callers in Task 17 must
/// use QMetaObject::invokeMethod(monitor, [=]{ ... }, Qt::QueuedConnection)
/// or queued signal-slot connections.
class TxInhibitMonitor : public QObject
{
    Q_OBJECT
public:
    /// Identifies which predicate caused the inhibit.
    enum class Source : std::uint8_t {
        None           = 0,
        UserIo01       = 1,
        // 2 was Rx2OnlyRadio; receive only is MoxController::setRxOnly
        // (Task 16).
        OutOfBand      = 3,
        BlockTxAntenna = 4,
        /// HL2 port part 2: the HL2 I/O board's fault register.
        IoBoardFault   = 5,
    };
    Q_ENUM(Source)

    explicit TxInhibitMonitor(QObject* parent = nullptr);

    /// Enable or disable the entire monitor.
    /// When disabled, inhibit state is immediately forced to (false, None).
    void setEnabled(bool on);

    bool isEnabled() const noexcept;

    /// Invert the UserIO pin's logical sense on top of the reader's output.
    /// Cite: console.cs:25830 [v2.10.3.13] (if (_reverseTxInhibit) inhibit_input = !inhibit_input)
    void setReverseLogic(bool on);

    /// Provide a callable that returns the UserIO pin's logical ASSERTED state
    /// (true = inhibit requested). The per-board active-low flip is the
    /// caller's responsibility; reverseLogic is applied on top.
    /// Cite: console.cs:25814-25820 [v2.10.3.13] (inhibit_input = !getUserI01())
    void setUserIoReader(std::function<bool()> reader);

    /// Task 13: the per-model choice of which user input is the TX inhibit
    /// input, as PollTXInhibit makes it. `userDigIn` is the radio's
    /// user_dig_in byte (RadioConnection::userDigitalInputsChanged). Returns
    /// inhibit_input before the reverse option; false for the HPSDR model,
    /// which PollTXInhibit never reads.
    /// From Thetis console.cs:25855-25876 [v2.10.3.15]:
    ///   if (_useTxInhibit && HardwareSpecific.Model != HPSDRModel.HPSDR)
    ///   P1: G2E/7000D/8000D/RedPitaya !getUserI02(), else !getUserI01()
    ///       //DH1KLM should be in P1  //N1GP G2E added
    ///   P2: 7000D/8000D/G2/G2_1K/ANVELINAPRO3/RedPitaya !getUserI05_p2(),
    ///       else !getUserI04_p2()
    static bool inhibitInputFromUserIo(HPSDRModel model, int protocolVersion,
                                       quint8 userDigIn);

    /// Task 13: read the connected radio's TX inhibit input. The input
    /// reads as 0 (asserted, as Thetis's prn->user_dig_in starts) until the
    /// first notifyUserDigitalInputs. Takes precedence over a test reader.
    void attachRadioInput(HPSDRModel model, int protocolVersion);

    /// Task 13: the radio has gone; its input no longer inhibits. HL2 port
    /// part 2: nor does its I/O board's fault (cleared to 0).
    void detachRadioInput();

    /// Task 13: the connected radio's model changed (model override).
    void setRadioModel(HPSDRModel model);

    /// Task 13: the radio reported its user digital inputs. The change
    /// reaches txInhibitedChanged now, not on the next 100 ms poll.
    void notifyUserDigitalInputs(quint8 userDigIn);

    /// HL2 port part 2: the HL2 I/O board's last read of REG_FAULT. A
    /// non-zero code holds transmit off until a read of zero; the same
    /// code again changes nothing. Acts whether or not the monitor is
    /// enabled (the External TX Inhibit box).
    /// From mi0bot console.cs:25876-25885 [@c26a8a4]:
    ///   if (0 != ioBoard.readRegister(IOBoard.Registers.REG_FAULT))
    ///   { TXInhibit = true; infoBar.Warning("I/O Board: Fault Code " + ...
    void notifyIoBoardFault(quint8 code);

    /// The fault code notifyIoBoardFault last reported; 0 for none.
    quint8 ioBoardFaultCode() const noexcept { return m_ioBoardFault; }

    bool isReverseLogic() const noexcept { return m_reverseLogic; }
    bool hasRadioInput() const noexcept { return m_radioInputAttached; }

    /// Notify that the VFO frequency moved outside a legal TX band.
    /// Cite: console.cs:6770-6806 [v2.10.3.13] (CheckValidTXFreq).
    void notifyOutOfBand(bool isOutOfBand);

    /// Notify that a TX-blocked antenna is currently selected.
    /// Cite: console.cs:29435-29481 [v2.10.3.13] (MOX-entry rejection).
    void notifyBlockTxAntenna(bool isBlocked);

    /// True when any inhibit source is currently active.
    bool inhibited() const noexcept;

    /// The highest-priority source that is currently active, or None.
    Source lastSource() const noexcept;

signals:
    void txInhibitedChanged(bool inhibited, NereusSDR::safety::TxInhibitMonitor::Source source);
    /// HL2 port part 2: the I/O board's fault code changed (0 = cleared).
    void ioBoardFaultChanged(quint8 code);

private slots:
    void recompute();

private:
    // ── State ──────────────────────────────────────────────────────────────
    bool m_enabled            = false;
    bool m_reverseLogic       = false;

    // Per-source predicates
    bool m_userIoAsserted     = false;  // result of last reader() call
    bool m_outOfBand          = false;
    bool m_blockTxAntenna     = false;
    quint8 m_ioBoardFault     = 0;      // HL2 I/O board REG_FAULT

    // Current aggregated state
    bool   m_currentInhibited = false;
    Source m_lastSource       = Source::None;

    std::function<bool()> m_userIoReader;

    // Task 13: the connected radio's input (attachRadioInput).
    bool       m_radioInputAttached = false;
    HPSDRModel m_radioModel         = HPSDRModel::HPSDR;
    int        m_radioProtocol      = 1;
    quint8     m_userDigIn          = 0;

    QTimer* m_pollTimer = nullptr;

    // Poll cadence — From Thetis console.cs:25838 [v2.10.3.13]:
    //   await Task.Delay(100); // PollTXInhibit loop
    static constexpr int kPollIntervalMs = 100;
};

} // namespace NereusSDR::safety
