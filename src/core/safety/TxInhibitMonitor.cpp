// =================================================================
// src/core/safety/TxInhibitMonitor.cpp  (NereusSDR)
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

#include "core/safety/TxInhibitMonitor.h"

namespace NereusSDR::safety {

TxInhibitMonitor::TxInhibitMonitor(QObject* parent)
    : QObject(parent)
    , m_pollTimer(new QTimer(this))
{
    m_pollTimer->setInterval(kPollIntervalMs);
    connect(m_pollTimer, &QTimer::timeout, this, &TxInhibitMonitor::recompute);
    // The QTimer runs continuously regardless of enabled state, so m_userIoAsserted
    // stays current even while the monitor is disabled. This avoids a stale-startup
    // emission when re-enabled (the first poll after enable returns the live pin
    // state, not a stale pre-construction value). Intentional improvement over
    // Thetis PollTXInhibit which gates the entire body on _useTxInhibit.
    m_pollTimer->start();
}

void TxInhibitMonitor::setEnabled(bool on)
{
    if (m_enabled == on) {
        return;
    }
    m_enabled = on;
    recompute();
}

bool TxInhibitMonitor::isEnabled() const noexcept
{
    return m_enabled;
}

void TxInhibitMonitor::setReverseLogic(bool on)
{
    if (m_reverseLogic == on) {
        return;
    }
    m_reverseLogic = on;
    recompute();
}

void TxInhibitMonitor::setUserIoReader(std::function<bool()> reader)
{
    m_userIoReader = std::move(reader);
    recompute();
}

void TxInhibitMonitor::notifyRxOnly(bool isRxOnly)
{
    if (m_rxOnly == isRxOnly) {
        return;
    }
    m_rxOnly = isRxOnly;
    recompute();
}

void TxInhibitMonitor::notifyOutOfBand(bool isOutOfBand)
{
    if (m_outOfBand == isOutOfBand) {
        return;
    }
    m_outOfBand = isOutOfBand;
    recompute();
}

void TxInhibitMonitor::notifyBlockTxAntenna(bool isBlocked)
{
    if (m_blockTxAntenna == isBlocked) {
        return;
    }
    m_blockTxAntenna = isBlocked;
    recompute();
}

bool TxInhibitMonitor::inhibited() const noexcept
{
    return m_currentInhibited;
}

TxInhibitMonitor::Source TxInhibitMonitor::lastSource() const noexcept
{
    return m_lastSource;
}

// ── Private ──────────────────────────────────────────────────────────────────

void TxInhibitMonitor::recompute()
{
    // From Thetis console.cs:25801-25839 [v2.10.3.13] (PollTXInhibit loop)
    // Tag preserved: //DH1KLM (console.cs:25814 — REDPITAYA/ANAN7000D/8000D use getUserI02 in P1)

    // Step 1 — read the UserIO pin if a reader is installed.
    // From Thetis console.cs:25814-25820 [v2.10.3.13]:
    //   if (NetworkIO.CurrentRadioProtocol == RadioProtocol.USB)
    //       inhibit_input = !NetworkIO.getUserI01();  // bit[1] of C1 (C&C)
    //   else
    //       inhibit_input = !NetworkIO.getUserI04_p2();  // bit[0] of byte 59
    if (m_userIoReader) {
        bool pinAsserted = m_userIoReader();
        // From Thetis console.cs:25830 [v2.10.3.13]:
        //   if (_reverseTxInhibit) inhibit_input = !inhibit_input;
        if (m_reverseLogic) {
            pinAsserted = !pinAsserted;
        }
        m_userIoAsserted = pinAsserted;
    }

    // Step 2 — if disabled, force inhibit clear.
    if (!m_enabled) {
        if (m_currentInhibited) {
            m_currentInhibited = false;
            m_lastSource       = Source::None;
            emit txInhibitedChanged(false, Source::None);
        }
        return;
    }

    // Step 3 — compute highest-priority active source.
    // Priority: UserIo01 > Rx2OnlyRadio > OutOfBand > BlockTxAntenna > None.
    Source newSource = Source::None;
    if (m_userIoAsserted) {
        newSource = Source::UserIo01;
    } else if (m_rxOnly) {
        newSource = Source::Rx2OnlyRadio;
    } else if (m_outOfBand) {
        newSource = Source::OutOfBand;
    } else if (m_blockTxAntenna) {
        newSource = Source::BlockTxAntenna;
    }

    // Step 4 — derive new inhibited flag.
    const bool newInhibited = (newSource != Source::None);

    // Step 5 — emit only on transitions (state change OR source change).
    // From Thetis console.cs:25832 [v2.10.3.13]:
    //   if (TXInhibit != inhibit_input) TXInhibitChangedHandlers?.Invoke(...)
    if (newInhibited != m_currentInhibited || newSource != m_lastSource) {
        m_currentInhibited = newInhibited;
        m_lastSource       = newSource;
        emit txInhibitedChanged(m_currentInhibited, m_lastSource);
    }
}

} // namespace NereusSDR::safety
