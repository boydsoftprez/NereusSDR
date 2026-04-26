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

#include <QObject>
#include <QTimer>
#include <cstdint>
#include <functional>

namespace NereusSDR::safety {

/// GPIO-poll-based TX inhibit monitor ported from Thetis PollTXInhibit.
///
/// Aggregates four inhibit predicates into a single inhibited() state and
/// emits txInhibitedChanged(bool, Source) on transitions only (not per tick).
///
/// Source priority (highest → lowest):
///   UserIo01 > Rx2OnlyRadio > OutOfBand > BlockTxAntenna > None
///
/// UserIo01 — per-board GPIO pin polled at 100 ms.
///   Cite: console.cs:25801-25839 [v2.10.3.13] (PollTXInhibit loop).
///   Per-board active-low → bool flip is the caller's responsibility; the
///   stored reader returns the logical ASSERTED (inhibit-requested) state.
///   setReverseLogic() applies an additional inversion on top of that.
///
/// Rx2OnlyRadio — radio is hardware-only-RX (no PA fitted).
///   Cite: console.cs:15283-15307 [v2.10.3.13] (RXOnly property setter).
///
/// OutOfBand — VFO frequency falls outside a legal TX band.
///   Cite: console.cs:6770-6806 [v2.10.3.13] (CheckValidTXFreq).
///
/// BlockTxAntenna — Alex RX-only antenna is selected for TX port.
///   Cite: console.cs:29435-29481 [v2.10.3.13] (MOX-entry rejection);
///         Andromeda.cs:285-306 [v2.10.3.13] (AlexANT[2,3]RXOnly).
///
/// The controller is inert (signals only, no radio I/O) until 3M-1a wires it.
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
        Rx2OnlyRadio   = 2,
        OutOfBand      = 3,
        BlockTxAntenna = 4,
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

    /// Notify that the radio's RX-only flag changed.
    /// Cite: console.cs:15283-15307 [v2.10.3.13] (RXOnly property setter).
    void notifyRxOnly(bool isRxOnly);

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

private slots:
    void recompute();

private:
    // ── State ──────────────────────────────────────────────────────────────
    bool m_enabled            = false;
    bool m_reverseLogic       = false;

    // Per-source predicates
    bool m_userIoAsserted     = false;  // result of last reader() call
    bool m_rxOnly             = false;
    bool m_outOfBand          = false;
    bool m_blockTxAntenna     = false;

    // Current aggregated state
    bool   m_currentInhibited = false;
    Source m_lastSource       = Source::None;

    std::function<bool()> m_userIoReader;

    QTimer* m_pollTimer = nullptr;

    // Poll cadence — From Thetis console.cs:25838 [v2.10.3.13]:
    //   await Task.Delay(100); // PollTXInhibit loop
    static constexpr int kPollIntervalMs = 100;
};

} // namespace NereusSDR::safety
