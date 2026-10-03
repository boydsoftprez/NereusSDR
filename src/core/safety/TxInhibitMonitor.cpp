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
//   2026-09-25 - Task 13 (receiver and transmit gaps plan): the radio's
//                own input, read per model and protocol as PollTXInhibit
//                reads it (console.cs:25849-25887 [v2.10.3.15]). J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 - Task 16 (receiver and transmit gaps plan): notifyRxOnly
//                removed; receive only is MoxController::setRxOnly.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - HL2 port part 2: notifyIoBoardFault, the Hermes Lite 2
//                I/O board's fault register as the highest source, acting
//                whether or not the monitor is enabled, per mi0bot-Thetis
//                console.cs UpdateIOBoard 25874-25888 [@c26a8a4]. Held
//                while the last read is non-zero, cleared by a read of
//                zero (operator ruling 2026-09-29). J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
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

// From Thetis console.cs:25855-25876 [v2.10.3.15] (PollTXInhibit):
//   //MW0LGE_22b converted to protocol, so we use correctly named userI functions
//   if (_useTxInhibit && HardwareSpecific.Model != HPSDRModel.HPSDR)
//   {
//       if (NetworkIO.CurrentRadioProtocol == RadioProtocol.USB)
//       {
//           // protocol 1
//           if (Model == ANAN_G2E || Model == ANAN7000D || Model == ANAN8000D || Model == REDPITAYA) //DH1KLM should be in P1  //N1GP G2E added
//               inhibit_input = !NetworkIO.getUserI02(); // bit[2] of C1 where C0 = 00000000 (C&C)
//           else
//               inhibit_input = !NetworkIO.getUserI01(); // bit[1] of C1 where C0 = 00000000 (C&C)
//       }
//       else
//       {
//           // protocol 2
//           if (Model == ANAN7000D || Model == ANAN8000D || Model == ANAN_G2 || Model == ANAN_G2_1K ||
//               Model == ANVELINAPRO3 || Model == REDPITAYA)
//               inhibit_input = !NetworkIO.getUserI05_p2(); // bit[1] of byte 59 from the HPSP 1025 packet
//           else
//               inhibit_input = !NetworkIO.getUserI04_p2(); // bit[0] of byte 59 from the HPSP 1025 packet
//       }
// The accessors, netInterface.c:245-289 [v2.10.3.15]:
//   getUserI01 = user_dig_in & 0x1, getUserI02 = user_dig_in & 0x2 (P1 names)
//   getUserI04_p2 = user_dig_in & 0x1, getUserI05_p2 = user_dig_in & 0x2 (P2)
// The "bit[1]/bit[2] of C1" comments count C1's bits: user_dig_in is
// (C1 >> 1) & 0xf, so bit 0 of user_dig_in is C1 bit 1. "Byte 59" counts
// the datagram; network.c reads ReadBufp[55], the same byte.
// mi0bot-Thetis PollTXInhibit [v2.10.3.13-beta2] has no Hermes Lite 2
// branch, so the HL2 takes the P1 default, !getUserI01().
bool TxInhibitMonitor::inhibitInputFromUserIo(HPSDRModel model, int protocolVersion,
                                              quint8 userDigIn)
{
    if (model == HPSDRModel::HPSDR) {
        return false;
    }
    constexpr quint8 kUserIoBit0 = 0x01;   // getUserI01 / getUserI04_p2
    constexpr quint8 kUserIoBit1 = 0x02;   // getUserI02 / getUserI05_p2
    quint8 mask = kUserIoBit0;
    if (protocolVersion == 1) {
        // protocol 1
        switch (model) {
        case HPSDRModel::ANAN_G2E:   //N1GP G2E added
        case HPSDRModel::ANAN7000D:
        case HPSDRModel::ANAN8000D:
        case HPSDRModel::REDPITAYA:  //DH1KLM should be in P1
            mask = kUserIoBit1;      // bit[2] of C1 where C0 = 00000000 (C&C)
            break;
        default:
            mask = kUserIoBit0;      // bit[1] of C1 where C0 = 00000000 (C&C)
            break;
        }
    } else {
        // protocol 2
        switch (model) {
        case HPSDRModel::ANAN7000D:
        case HPSDRModel::ANAN8000D:
        case HPSDRModel::ANAN_G2:
        case HPSDRModel::ANAN_G2_1K:
        case HPSDRModel::ANVELINAPRO3:
        case HPSDRModel::REDPITAYA:
            mask = kUserIoBit1;      // bit[1] of byte 59 from the HPSP 1025 packet
            break;
        default:
            mask = kUserIoBit0;      // bit[0] of byte 59 from the HPSP 1025 packet
            break;
        }
    }
    return (userDigIn & mask) == 0;
}

void TxInhibitMonitor::attachRadioInput(HPSDRModel model, int protocolVersion)
{
    m_radioInputAttached = true;
    m_radioModel         = model;
    m_radioProtocol      = protocolVersion;
    m_userDigIn          = 0;
    recompute();
}

void TxInhibitMonitor::detachRadioInput()
{
    // HL2 port part 2: the I/O board's fault goes with the radio too; the
    // next radio's first read sets it again if the board still reports it.
    const bool hadFault = m_ioBoardFault != 0;
    m_ioBoardFault = 0;
    if (!m_radioInputAttached && !hadFault) {
        return;
    }
    m_radioInputAttached = false;
    m_userDigIn          = 0;
    recompute();
    if (hadFault) {
        emit ioBoardFaultChanged(0);
    }
}

void TxInhibitMonitor::setRadioModel(HPSDRModel model)
{
    if (m_radioModel == model) {
        return;
    }
    m_radioModel = model;
    recompute();
}

void TxInhibitMonitor::notifyUserDigitalInputs(quint8 userDigIn)
{
    m_userDigIn = userDigIn;
    recompute();
}

void TxInhibitMonitor::notifyIoBoardFault(quint8 code)
{
    // From mi0bot console.cs:25876-25885 [@c26a8a4] (UpdateIOBoard):
    //   if (0 != ioBoard.readRegister(IOBoard.Registers.REG_FAULT))
    //   {
    //       TXInhibit = true;
    //       infoBar.Warning("I/O Board: Fault Code " + ioBoard.readRegister(IOBoard.Registers.REG_FAULT).ToString());
    //       AutoTuningHL2(ProtocolEvent.Idle);
    //   }
    // mi0bot never clears TXInhibit here; its PollTXInhibit rewrites it
    // every 100 ms, so the fault flickers. NereusSDR holds it while the
    // last read is non-zero and clears it on a read of zero (operator
    // ruling 2026-09-29). The auto-tune reaction is not ported here.
    if (m_ioBoardFault == code) {
        return;
    }
    m_ioBoardFault = code;
    recompute();
    emit ioBoardFaultChanged(code);
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
    // Upstream tags preserved: //N1GP (from cited upstream lines) [v2.10.3.15]
    // Tag preserved: //DH1KLM (console.cs:25814 — REDPITAYA/ANAN7000D/8000D use getUserI02 in P1)

    // Step 1: read the UserIO pin, the connected radio's input
    // (Task 13), else a test reader, else nothing.
    // Tag preserved: //DH1KLM //N1GP G2E added (console.cs:25862, the
    // per-model choice in inhibitInputFromUserIo above)
    if (m_radioInputAttached) {
        // From Thetis console.cs:25855 [v2.10.3.15]:
        //   if (_useTxInhibit && HardwareSpecific.Model != HPSDRModel.HPSDR)
        // The HPSDR model is never read, so the reverse below does not
        // apply to it either.
        bool pinAsserted = false;
        if (m_radioModel != HPSDRModel::HPSDR) {
            pinAsserted = inhibitInputFromUserIo(m_radioModel, m_radioProtocol, m_userDigIn);
            // From Thetis console.cs:25878 [v2.10.3.15]:
            //   if (_reverseTxInhibit) inhibit_input = !inhibit_input;
            if (m_reverseLogic) {
                pinAsserted = !pinAsserted;
            }
        }
        m_userIoAsserted = pinAsserted;
    } else if (m_userIoReader) {
        bool pinAsserted = m_userIoReader();
        // From Thetis console.cs:25830 [v2.10.3.13]:
        // Upstream tags preserved: //N1GP (from cited console.cs:25833) [v2.10.3.15]
        //   if (_reverseTxInhibit) inhibit_input = !inhibit_input;
        if (m_reverseLogic) {
            pinAsserted = !pinAsserted;
        }
        m_userIoAsserted = pinAsserted;
    } else {
        // No radio and no reader: nothing asserts the input.
        m_userIoAsserted = false;
    }

    // Step 2 — compute highest-priority active source.
    // Priority: IoBoardFault > UserIo01 > OutOfBand > BlockTxAntenna >
    // None. (Receive only is MoxController::setRxOnly, Task 16.)
    // The HL2 I/O board fault acts whether or not the monitor is enabled:
    // mi0bot's UpdateIOBoard sets TXInhibit without reading _useTxInhibit
    // (console.cs:25876-25885 [@c26a8a4]). Disabled, every other source
    // is forced clear (console.cs:25882-25883 [v2.10.3.15]).
    Source newSource = Source::None;
    if (m_ioBoardFault != 0) {
        newSource = Source::IoBoardFault;
    } else if (!m_enabled) {
        newSource = Source::None;
    } else if (m_userIoAsserted) {
        newSource = Source::UserIo01;
    } else if (m_outOfBand) {
        newSource = Source::OutOfBand;
    } else if (m_blockTxAntenna) {
        newSource = Source::BlockTxAntenna;
    }

    // Step 3 — derive new inhibited flag.
    const bool newInhibited = (newSource != Source::None);

    // Step 4 — emit only on transitions (state change OR source change).
    // From Thetis console.cs:25832 [v2.10.3.13]:
    // Upstream tags preserved: //DH1KLM //N1GP (from cited upstream lines) [v2.10.3.15]
    //   if (TXInhibit != inhibit_input) TXInhibitChangedHandlers?.Invoke(...)
    if (newInhibited != m_currentInhibited || newSource != m_lastSource) {
        m_currentInhibited = newInhibited;
        m_lastSource       = newSource;
        emit txInhibitedChanged(m_currentInhibited, m_lastSource);
    }
}

} // namespace NereusSDR::safety
