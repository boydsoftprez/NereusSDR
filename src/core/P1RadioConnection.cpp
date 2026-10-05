// =================================================================
// src/core/P1RadioConnection.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/ChannelMaster/networkproto1.c, original licence from Thetis source is included below
//   Project Files/Source/ChannelMaster/netInterface.c, original licence from Thetis source is included below
//   Project Files/Source/ChannelMaster/network.c, original licence from Thetis source is included below
//   Project Files/Source/Console/HPSDR/NetworkIO.cs (upstream has no top-of-file header — project-level LICENSE applies)
//   Project Files/Source/Console/cmaster.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//   Project Files/Source/ChannelMaster/bandwidth_monitor.c, original licence from Thetis source is included below
//   Project Files/Source/ChannelMaster/bandwidth_monitor.h, original licence from Thetis source is included below
//   Project Files/Source/Console/HPSDR/IoBoardHl2.cs (mi0bot/OpenHPSDR-Thetis fork), original licence from upstream included below
//   Project Files/Source/Console/setup.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/HPSDR/Penny.cs (mi0bot/OpenHPSDR-Thetis fork), original licence from upstream included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-09-23 - R-R3-21: EP6 sequence error count ported from Thetis
//                 networkproto1.c MetisReadDirect [v2.10.3.15] for
//                 Diagnostics > Connection Quality. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-24 - R-R3-49: the Network Watchdog setting sets the wait for
//                 data before the radio is declared lost (3000 ms on, none
//                 off) and no longer reaches the start/stop packet, from
//                 Thetis networkproto1.c:50, 85, 294 and setup.cs:18024
//                 [v2.10.3.15]. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-24 - R-R3-49 fix wave: setup.cs's header added below, since
//                 setWatchdogEnabled quotes setup.cs:18024-18028
//                 [v2.10.3.15]. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-25 - R-R3-32 / R-R3-49 (remote-window parity Task 6): every
//                 datagram from the radio, the ep6 sequence errors and the
//                 ep6 arrivals feed the link counters (RadioLinkStats: UDP
//                 packets seen, packet loss, RFC 3550 jitter, packet gap).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 - Receiver and transmit gaps plan, Task 13: case-0x00
//                 status C1 bits 1..4 reported as the user digital inputs
//                 (networkproto1.c:336 [v2.10.3.15]). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-26 - Parity Task 14 follow-up (R-R3-46): the HL2 I/O board
//                 probe advances on the answer to the read it sent
//                 (IoBoardHl2::i2cReadAnswered), not on C0's address bits,
//                 which mi0bot never reads (networkproto1.c:478-493
//                 [@c26a8a4]). J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-28 - R-R3-49 / R-R3-46: Setup > Transmit > Power's Disable HF PA
//                applied (Thetis DisablePA and hf_tr_relay,
//                transmitSettingsVersion 11). J.J. Boyd (KG4VCF), AI-assisted
//                via Anthropic Claude Code.
//   2026-09-28 - R-R3-46 / R-R3-49: the Alex Filters tabs' receive filter rows
//                (per-row bypass and edges, Alex-2 master bypass) select the
//                receive high-pass as Thetis's setAlexHPF /
//                setBPF1ForOrionIISaturn / setAlex2HPF do (radioHardwareVersion
//                8). J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - HL2 Band Volts and Disable PS Sync: on a Hermes Lite 2,
//                bank 0 C3 bits 3 and 4 follow the saved HL2 options (off by
//                default), as mi0bot setup.cs:2843-2848 and 13376-13390
//                [@c26a8a4] do; other boards keep Thetis's dither and random
//                on. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - HL2 port part 1: the HL2 TX buffer latency and PTT hang
//                (bank 17) are the saved HL2 options, as mi0bot
//                setup.cs:21236-21248 [@c26a8a4] sends them. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - HL2 port part 1: bank 18 carries the saved Reset on
//                Ethernet disconnect, as mi0bot setup.cs:21257-21262
//                [@c26a8a4] sends it. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-29 - HL2 port part 1: the HL2 I/O board's ongoing poll (mode,
//                TX frequency, aerial registers, input-pin reads) and mi0bot's
//                I2C frame spacing with the round-robin kept on an I2C
//                subframe (console.cs:25781-25945, networkproto1.c:898-906
//                [@c26a8a4]). J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                Claude Code.
//   2026-09-29 - HL2 clock: Enable CL2, CL2 frequency and External 10 MHz
//                program the HL2 clock chip over I2C at connect and on a
//                change (setHl2Clock; mi0bot setup.cs:21558-21756,
//                console.cs:28033-28040 [@c26a8a4]). J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-29 - HL2 port part 2: the I/O board poll sends RADE_U as DIGU
//                and RADE_L as DIGL on REG_OP_MODE (operator ruling
//                2026-09-29). J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                Claude Code.
//   2026-09-29 - HL2 port part 2: the I/O board poll waits on its step
//                while the manual I2C tool holds the polling pause (mi0bot
//                console.cs:25930-25935 [@c26a8a4]). J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-29 - G-05: txIqRingDrained and txIqRingLengthMs, so an
//                operator's unkey waits for the transmit I/Q ring to drain,
//                for at most its 84 ms length. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-29 - G-05 follow-up: the unkey discards what the ring still
//                holds (a remainder under one zone after a normal release),
//                as Thetis sends no transmit I/Q while not transmitting
//                (networkproto1.c:723 [v2.10.3.15]). J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-28 - R-R3-49 (found bug): the VFO frequencies sent carry the
//                 calibration correction factor, as Thetis NetworkIO.VFOfreq
//                 does on Protocol 1 [v2.10.3.15] (setCalibrationController,
//                 wireFrequencyHz). J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-29 - Level Cal: the Alex receive attenuator (Thetis SetAlexAtten,
//                netInterface.c:421-432 [v2.10.3.15]) on the wire, and the step
//                attenuator range above 31 dB on Alex boards (value + 2,
//                console.cs:11044-11056 [v2.10.3.15]). J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-29 - Level Cal fix wave: setRx2Preamp, the second receiver's
//                preamp bit (Thetis SetRX2Preamp, netInterface.c:758-767
//                [v2.10.3.15]). J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-30 - Radio codec: the EP2 L/R bytes carry the receive audio
//                for the radio's own speaker out, swapped as Thetis
//                sendProtocol1Samples does (networkproto1.c:726-731
//                [v2.10.3.15]); on the HL2 only when Swap audio channels is
//                on, as mi0bot (networkproto1.c:1231-1239 [@c26a8a4]).
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30 - Shared-input filters (ruling (c)): the receive low-pass
//                follows the highest slice the model counted on the input
//                (Thetis UpdateAlexTXFilter, console.cs:15487-15498
//                [v2.10.3.15]), and on the HL2 the OC receive band follows
//                the highest band among them (mi0bot Penny.cs UpdateExtCtrl,
//                183-189 [@c26a8a4]). J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-30 - Shared-input filters, follow-up: both choices go through
//                SharedInputLowPass::highest over countedCandidates (the
//                DDC centre for the Alex low-pass, the VFO's band for the
//                HL2 pins), the call RadioModel's reason makes. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30 - Review fix: ocBandFrequencyHz notes the out-of-range RX1
//                band divergence from mi0bot. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-30 - HL2 receive pins follow the counted slice with the
//                highest frequency, not mi0bot's band enum order
//                (maintainer ruling). J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-30 - HL2 unkeyed pins through SharedInputLowPass::hl2ReceivePins:
//                the N2ADR high-pass (bit 6) cleared when a counted slice's
//                pins lack it; only ForceBypass and WidebandLocked send 0x00
//                (JJ's ruling). J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-30 - Review fix: ocBandFrequencyHz reports the receiver
//                hl2ReceivePins takes the pins from. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-30 - The receive socket binds to the local address that
//                reaches the radio, as Thetis binds the network card's
//                address (NetworkIO.cs:69-70, 149; network.c:116-118, 203
//                [v2.10.3.15]); network.c's header added below. On macOS a
//                socket bound to Any could share its port with another
//                socket on that address, which then took the radio's
//                frames. Bound again on each reconnect attempt; with no
//                route it warns and binds every address. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30 - TX safety: a lost link and every reconnect start unkeyed
//                (dropTransmitForNewLink) and a key after the loss is refused
//                until released (m_linkLossLatched), as the priming frames of
//                Thetis networkproto1.c:106-138 [v2.10.3.15] never carry MOX.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30 - TX safety fix round 1: the latch is set only when the
//                link was lost keyed and is lifted when the reconnect
//                reaches Connected unkeyed, so a loss while unkeyed never
//                refuses the next key. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-30 - TX safety follow-up: the refusal's log line names both
//                ways the latch lifts. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-10-01 - Diversity lane B4: setDiversity writes m_diversity, the
//                VFO lock bit (bank 0 C4 bit 7) that had no writer, from
//                Protocol1DDCConfig's en_diversity (console.cs:8215-8216,
//                8544; netInterface.c:1249-1252; networkproto1.c:471
//                [v2.10.3.15]). J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
// =================================================================

/*
 * networkprot1.c
 * Copyright (C) 2020 Doug Wigley (W5WC)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

// --- From netInterface.c ---
/*
 * netinterface.c
 * Copyright (C) 2006,2007  Bill Tracey (bill@ejwt.com) (KD5TFD)
 * Copyright (C) 2010-2020 Doug Wigley (W5WC)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */


// --- From network.c ---
/*
 * network.c
 * Copyright (C) 2015-2020 Doug Wigley (W5WC)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

//
// Upstream source 'Project Files/Source/Console/HPSDR/NetworkIO.cs' has no top-of-file GPL header —
// project-level Thetis LICENSE applies.

/*  cmaster.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2000-2025 Original authors
Copyright (C) 2020-2025 Richard Samphire MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk
*/
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

//=================================================================
// console.cs
//=================================================================
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
//=================================================================
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

/*  bandwidth_monitor.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2025 Richard Samphire, MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk

*/
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


/*  bandwidth_monitor.h

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2025 Richard Samphire, MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk

*/
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


/*
*
* Copyright (C) 2025 Reid Campbell, MI0BOT, mi0bot@trom.uk 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
//
// (mi0bot HL2 fork's IOBoard logic; the C# class wraps closed-source
// I2C register code in ChannelMaster.dll — only the public API surface
// has been ported into NereusSDR's P1 path.)

// Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12

// --- From setup.cs ---

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

// --- From Penny.cs (mi0bot/OpenHPSDR-Thetis fork) ---

/*
*
* Copyright (C) 2008 Bill Tracey, KD5TFD, bill@ewjt.com 
* Copyright (C) 2010-2020  Doug Wigley
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "P1RadioConnection.h"
#include "CalibrationController.h"
#include "LogCategories.h"
#include "OcMatrix.h"
#include "IoBoardHl2.h"
#include "WdspTypes.h"
#include "HermesLiteBandwidthMonitor.h"
#include "PerfMonitor.h"
#include "audio/TxMicSource.h"
#include "codec/P1CodecStandard.h"
#include "codec/P1CodecAnvelinaPro3.h"
#include "codec/P1CodecRedPitaya.h"
#include "codec/P1CodecHl2.h"
#include "codec/AlexFilterMap.h"
#include "models/Band.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <atomic>
#include <cstdlib>
#include <cstring>    // memset
#include <vector>
#include <QtEndian>
#include <QNetworkDatagram>
#include <QThread>
#include <QVariant>
#include <QCoreApplication>

namespace NereusSDR {

// ---------------------------------------------------------------------------
// composeEp2Frame
//
// Builds the 1032-byte Metis ep2 UDP payload that is sent TO the radio.
// Layout (source: networkproto1.c:216-236 MetisWriteFrame, and :597-864
// WriteMainLoop which populates the two 512-byte USB subframes):
//
//   [0..3]    Magic header: EF FE 01 02
//   [4..7]    Sequence number, big-endian uint32
//   [8..519]  USB subframe 0 (512 bytes):
//               [8..10]   sync 7F 7F 7F  (networkproto1.c:600-602)
//               [11..15]  C0..C4 command/control bytes
//               [16..519] TX I/Q + mic samples (zeros for RX-only)
//   [520..1031] USB subframe 1 (512 bytes):
//               [520..522] sync 7F 7F 7F  (networkproto1.c:881-883)
//               [523..527] C0..C4 for second subframe (same bank for now)
//               [528..1031] TX I/Q + mic samples (zeros for RX-only)
// ---------------------------------------------------------------------------
void P1RadioConnection::composeEp2Frame(quint8 out[1032], quint32 seq,
                                         int /*ccAddress*/,
                                         int sampleRate, bool mox,
                                         quint64 rx1FreqHz,
                                         int activeRxCount) noexcept
{
    // Source: networkproto1.c:223-230 — MetisWriteFrame() header + sequence
    out[0] = 0xEF;
    out[1] = 0xFE;
    out[2] = 0x01;
    out[3] = 0x02;  // endpoint 2

    // Sequence number big-endian
    out[4] = static_cast<quint8>((seq >> 24) & 0xFF);
    out[5] = static_cast<quint8>((seq >> 16) & 0xFF);
    out[6] = static_cast<quint8>((seq >>  8) & 0xFF);
    out[7] = static_cast<quint8>( seq        & 0xFF);

    // Source: networkproto1.c:597-602 — WriteMainLoop() USB subframe 0 sync
    out[8]  = 0x7F;
    out[9]  = 0x7F;
    out[10] = 0x7F;

    // C0..C4 for subframe 0 -- compose bank 0 (general settings)
    quint8 cc0[5] = {};
    composeCcBank0(cc0, sampleRate, mox, activeRxCount);
    out[11] = cc0[0];
    out[12] = cc0[1];
    out[13] = cc0[2];
    out[14] = cc0[3];
    out[15] = cc0[4];

    // Bytes 16..519: TX I/Q + mic data — zeros (RX-only; TX producer added in TX phase)

    // Source: networkproto1.c:878-883 — WriteMainLoop_HL2() USB subframe 1 sync
    // (same layout in WriteMainLoop at :880-883)
    out[520] = 0x7F;
    out[521] = 0x7F;
    out[522] = 0x7F;

    // C0..C4 for subframe 1 — RX1 frequency bank (address 0x02, Thetis case 2).
    // Without this, the radio streams ADC data at LO=0 → huge DC spike with no
    // visible signals. Partial round-robin: bank 0 on subframe 0, RX1 freq on
    // subframe 1. Remaining banks (TX freq, Alex, atten, OC, RX2+) come with
    // the full round-robin in the outstanding P1 tasks.
    quint8 cc1[5] = {};
    composeCcBankRxFreq(cc1, 0 /* rxIndex */, rx1FreqHz);
    out[523] = cc1[0];
    out[524] = cc1[1];
    out[525] = cc1[2];
    out[526] = cc1[3];
    out[527] = cc1[4];

    // Bytes 528..1031: TX I/Q + mic data — zeros (RX-only)
}

// ---------------------------------------------------------------------------
// composeCcBank0
//
// Source: networkproto1.c:619-641 — WriteMainLoop() case 0 (general settings)
//   C0 = XmitBit (MOX, bit 0); address bits 7..1 = 0x00 (no C0 |= address)
//   C1 = SampleRateIn2Bits & 3  (48k=0, 96k=1, 192k=2, 384k=3)
//   C2 = OC output mask (bits 7..1) | EER bit (bit 0) — zero for stub
//   C3 = BPF/atten/preamp flags — zero for stub
//   C4 = antenna, duplex, NDDCs — zero for stub
// ---------------------------------------------------------------------------
void P1RadioConnection::composeCcBank0(quint8 out[5], int sampleRate, bool mox,
                                        int activeRxCount) noexcept
{
    // Source: networkproto1.c:615 -- C0 = (unsigned char)XmitBit
    // (Nearby upstream context — case 12 Step ATT control — carries the
    //  RedPitaya guard: `//[2.10.3.9]DH1KLM  //model needed as board type
    //  (prn->discovery.BoardType) is an OrionII` on networkproto1.c:612.)
    out[0] = mox ? 0x01 : 0x00;

    // Source: networkproto1.c:620 -- C1 = (SampleRateIn2Bits & 3)
    // 48000->0, 96000->1, 192000->2, 384000->3
    quint8 srBits = 0;
    if      (sampleRate >= 384000) { srBits = 3; }
    else if (sampleRate >= 192000) { srBits = 2; }
    else if (sampleRate >= 96000)  { srBits = 1; }
    else                            { srBits = 0; }
    out[1] = srBits & 0x03;

    // C2, C3: OC outputs / filter bits -- zero for Task 7 scope.
    out[2] = 0;
    out[3] = 0;

    // C4: number of DDCs to run, encoded as (nddc - 1) << 3
    // Source: networkproto1.c:470. Thetis sends 0x08 for nddc=2 even on
    // single-RX setups (diversity pre-allocation). We send the actual
    // count so 1-RX configurations write 0x00. The Hermes firmware
    // accepts both.
    int nddc = (activeRxCount < 1) ? 1 : (activeRxCount > 7 ? 7 : activeRxCount);
    out[4] = static_cast<quint8>((nddc - 1) << 3);
}

// ---------------------------------------------------------------------------
// composeCcBankRxFreq
//
// Source: networkproto1.c:484-494 — case 2 (RX1/DDC0 frequency)
//   rxIndex 0 → C0 |= 4 (address 0x02)
//   rxIndex 1 → C0 |= 6 (address 0x03)  [case 3]
//   rxIndex 2 → C0 |= 8 (address 0x04)  [case 5]
//   C1..C4 = (prn->rx[id].frequency >> {24,16,8,0}) & 0xff — raw Hz, big-endian
//
// P1 (Protocol 1 / ENC "USB") sends **raw Hz** on the wire — NOT a phase
// word. Thetis NetworkIO.cs:215-223 VFOfreq() selects the encoding based on
// CurrentRadioProtocol:
//
//     if (CurrentRadioProtocol == RadioProtocol.USB)   // USB == P1
//         SetVFOfreq(id, f_freq, tx);                   // raw Hz
//     else SetVFOfreq(id, Freq2PhaseWord(f_freq), tx);  // phase word (P2)
//
// The RadioProtocol.USB enum value maps to Protocol 1 (see cmaster.cs:520
// "case RadioProtocol.USB: //p1"). The native side (networkproto1.c:491-494)
// then splats prn->rx[0].frequency directly into C1..C4 with no conversion.
//
// Previous revisions of this helper pre-converted to a phase word and the
// on-wire bytes were interpreted by Hermes firmware as raw Hz far above
// Nyquist, which produced an aliased waterfall and a non-tracking VFO on
// ANAN-10E (pcap4 from alpha tester, 2026-04-15).
// ---------------------------------------------------------------------------
void P1RadioConnection::composeCcBankRxFreq(quint8 out[5], int rxIndex, quint64 freqHz) noexcept
{
    // Address assignments from networkproto1.c (C0 OR bits; the table stores
    // the already-shifted value so callers just assign out[0] = addrBits):
    //   rxIndex 0 → case 2 (:485):  C0 |= 0x04  (RX1 / DDC0)
    //   rxIndex 1 → case 3 (:498):  C0 |= 0x06  (RX2 / DDC1)
    //   rxIndex 2 → case 5 (:526):  C0 |= 0x08  (RX3 / DDC2)
    //   rxIndex 3 → case 6 (:539):  C0 |= 0x0A  (RX4 / DDC3)
    //   rxIndex 4 → case 7 (:549):  C0 |= 0x0C  (RX5 / DDC4)
    //   rxIndex 5 → case 8      :  C0 |= 0x0E  (RX6 / DDC5)
    //   rxIndex 6 → case 9 (:569):  C0 |= 0x10  (RX7 / DDC6)
    //
    // History: prior revision had {4,6,8,0x0C,0x0E,0x10,0x12}, dropping 0x0A
    // entirely and aliasing rxIndex 6 onto bank 10's 0x12 slot. Fixed per
    // pcap analysis of RedPitaya (#38).
    static const quint8 kRxC0Address[] = { 0x04, 0x06, 0x08, 0x0A, 0x0C, 0x0E, 0x10 };
    quint8 addrBits = (rxIndex >= 0 && rxIndex < 7) ? kRxC0Address[rxIndex] : 4;
    out[0] = addrBits;  // MOX=0; address is already left-shifted in the table

    // Raw Hz, big-endian — see header comment for Thetis source citations.
    // Thetis stores freq as int32 in prn->rx[0].frequency; clamp to 32 bits
    // here so anything above ~4.29 GHz (impossible for HF) truncates cleanly.
    const quint32 hz = static_cast<quint32>(freqHz);
    out[1] = static_cast<quint8>((hz >> 24) & 0xFF);
    out[2] = static_cast<quint8>((hz >> 16) & 0xFF);
    out[3] = static_cast<quint8>((hz >>  8) & 0xFF);
    out[4] = static_cast<quint8>( hz        & 0xFF);
}

// ---------------------------------------------------------------------------
// composeCcBankTxFreq
//
// Source: networkproto1.c:476-482 — case 1 (TX VFO frequency)
//   C0 |= 2 → address 0x01
//   C1..C4 = (prn->tx[0].frequency >> {24,16,8,0}) & 0xff — raw Hz, big-endian
//
// Same raw-Hz encoding as composeCcBankRxFreq — see that function's header
// comment for the NetworkIO.cs:215-223 branching rule.
// ---------------------------------------------------------------------------
void P1RadioConnection::composeCcBankTxFreq(quint8 out[5], quint64 freqHz) noexcept
{
    // Source: networkproto1.c:477 — C0 |= 2  (case 1 = TX VFO)
    out[0] = 0x02;  // address 0x01, MOX=0

    const quint32 hz = static_cast<quint32>(freqHz);
    out[1] = static_cast<quint8>((hz >> 24) & 0xFF);
    out[2] = static_cast<quint8>((hz >> 16) & 0xFF);
    out[3] = static_cast<quint8>((hz >>  8) & 0xFF);
    out[4] = static_cast<quint8>( hz        & 0xFF);
}

// ---------------------------------------------------------------------------
// composeCcBankAtten
//
// Source: networkproto1.c:762-771 — case 11 (Preamp control / step attenuator)
//   C0 |= 0x14 → address 0x0A (networkproto1.c:763)
//   C4 = (adc[0].rx_step_attn & 0b00011111) | 0b00100000
//        Low 5 bits = dB value; bit 5 = "enable step attenuator" flag
// ---------------------------------------------------------------------------
void P1RadioConnection::composeCcBankAtten(quint8 out[5], int dB) noexcept
{
    // Source: networkproto1.c:763 — C0 |= 0x14 → address 0x0A
    out[0] = 0x14;  // MOX=0; address bits = 0x14

    // C1..C3: preamp/mic/dig-out flags — zero for Task 7 scope
    out[1] = 0;
    out[2] = 0;
    out[3] = 0;

    // Source: networkproto1.c:770 — C4 = (rx_step_attn & 0b00011111) | 0b00100000
    out[4] = static_cast<quint8>((dB & 0x1F) | 0x20);
}

// ---------------------------------------------------------------------------
// composeCcBankAlexRx
//
// Source: networkproto1.c:826-835 — case 16 (BPF2 / ALEX RX filter mask)
//   C0 |= 0x24 → address 0x12
//   C1 = BPF HPF filter bits (low 7 bits of alexRxMask)
//   C2 = xvtr_enable + puresignal flags (bits 0 + 6 of upper byte)
//   C3, C4 = 0
//
// TODO(3I-T7): The "ALEX RX antenna routing" (RX1 IN, RX2 IN, XVTR) is
// encoded in bank 0 C3 bits 5..6 (networkproto1.c:625-630), not a separate
// bank. The alexRxMask here is treated as the BPF2 HPF filter bitmap
// (case 16). Full ALEX RX antenna routing requires bank 0 state.
// ---------------------------------------------------------------------------
void P1RadioConnection::composeCcBankAlexRx(quint8 out[5], quint32 alexRxMask) noexcept
{
    // Source: networkproto1.c:827 — C0 |= 0x24
    out[0] = 0x24;
    out[1] = static_cast<quint8>(alexRxMask & 0x7F);  // HPF filter bits
    out[2] = static_cast<quint8>((alexRxMask >> 8) & 0x41); // xvtr + puresignal flags
    out[3] = 0;
    out[4] = 0;
}

// ---------------------------------------------------------------------------
// composeCcBankAlexTx
//
// Source: networkproto1.c:747-760 — case 10 (TX drive level / ALEX TX LPF)
//   C0 |= 0x12 → address 0x09
//   C3 = HPF filter bits (bits from alexTxMask low byte)
//   C4 = LPF filter bits (bits from alexTxMask high byte)
//   C1 = drive level (0 for stub), C2 = mic/apollo flags (0 for stub)
//
// TODO(3I-T7): Drive level and mic/apollo flags are separate state fields not
// carried in alexTxMask. Full encoding requires those fields; this stub
// encodes only the filter mask portion as mapped from Task 7 scope.
// ---------------------------------------------------------------------------
void P1RadioConnection::composeCcBankAlexTx(quint8 out[5], quint32 alexTxMask) noexcept
{
    // Source: networkproto1.c:748 — C0 |= 0x12
    out[0] = 0x12;
    out[1] = 0;  // drive level — TODO(3I-T7): wire from state
    out[2] = 0;  // mic/apollo flags — TODO(3I-T7): wire from state
    out[3] = static_cast<quint8>(alexTxMask & 0xFF);        // HPF bits
    out[4] = static_cast<quint8>((alexTxMask >> 8) & 0x7F); // LPF bits
}

// ---------------------------------------------------------------------------
// composeCcBankOcOutputs
//
// Source: networkproto1.c:621 — case 0: C2 = (cw.eer & 1) | ((oc_output << 1) & 0xFE)
//   OC output bits live in bank 0 C2, bits 7..1. Not a separate bank.
//   For Task 7 scope this helper encodes only the OC mask into C2 of a
//   bank-0-shaped output buffer (C0 address = 0x00, C1 = default 48k).
//
// TODO(3I-T7): In Thetis the OC mask is always sent as part of bank 0 C2
// (networkproto1.c:621). This standalone helper is for the test interface;
// in the real send path it will be merged into composeCcBank0 state.
// ---------------------------------------------------------------------------
void P1RadioConnection::composeCcBankOcOutputs(quint8 out[5], quint8 ocMask) noexcept
{
    // Source: networkproto1.c:621 — C2 = (cw.eer & 1) | ((oc_output << 1) & 0xFE)
    out[0] = 0;  // address 0x00, MOX=0
    out[1] = 0;
    out[2] = static_cast<quint8>((ocMask << 1) & 0xFE);  // OC bits in C2 bits 7..1
    out[3] = 0;
    out[4] = 0;
}



P1RadioConnection::P1RadioConnection(QObject* parent)
    : RadioConnection(parent)
{
}

P1RadioConnection::~P1RadioConnection()
{
    if (m_running) {
        disconnect();
    }
}

// ---------------------------------------------------------------------------
// init
//
// Creates the UDP socket and watchdog timer on the worker thread.
// Must be called after moveToThread().
// Source: P2RadioConnection::init() pattern — socket + timer created here,
// not in constructor, to ensure thread affinity is correct.
// ---------------------------------------------------------------------------
void P1RadioConnection::init()
{
    // A placeholder binding until connectToRadio() binds the address that
    // reaches the radio (bindToRadioFacingAddress), as Thetis binds the
    // network card's address in nativeInitMetis (network.c:116-118, 203
    // [v2.10.3.15]).
    m_socket = new QUdpSocket(this);

    if (!m_socket->bind(QHostAddress::Any, 0)) {
        qCWarning(lcConnection) << "P1: Failed to bind UDP socket";
    } else {
        applySocketBufferSizes();
    }

    // Finish worker initialization even after a failed placeholder bind.
    // connectToRadio() retries the bind and needs receive wiring and timers.

    connect(m_socket, &QUdpSocket::readyRead, this, &P1RadioConnection::onReadyRead);

    // HL2 I/O board poll: one step every 40 ms once the probe finds the
    // board (mi0bot console.cs:25929-25934 [@c26a8a4], await Task.Delay(40)).
    m_ioBoardPollTimer = new QTimer(this);
    m_ioBoardPollTimer->setInterval(kIoBoardPollMs);
    connect(m_ioBoardPollTimer, &QTimer::timeout, this, &P1RadioConnection::ioBoardPollTick);

    // Watchdog timer — polls every kWatchdogTickMs ms; started in connectToRadio.
    // Source: NereusSDR design doc §3.6 — silence detection + reconnect state machine.
    m_watchdogTimer = new QTimer(this);
    m_watchdogTimer->setInterval(kWatchdogTickMs);
    connect(m_watchdogTimer, &QTimer::timeout, this, &P1RadioConnection::onWatchdogTick);

    // EP2 pacer — dedicated high-resolution timer that feeds the radio's
    // 48 kHz audio DAC. Kept separate from the watchdog so silence detection
    // cadence (25 ms) and EP2 cadence (2 ms) can evolve independently.
    m_ep2PacerTimer = new QTimer(this);
    m_ep2PacerTimer->setInterval(kEp2PacerIntervalMs);
    m_ep2PacerTimer->setTimerType(Qt::PreciseTimer);
    connect(m_ep2PacerTimer, &QTimer::timeout, this, &P1RadioConnection::onEp2PacerTick);

    // Reconnect timer — single-shot; fires kReconnectIntervalMs after watchdog trips.
    // Source: NereusSDR design doc §3.6 — 5-second reconnect interval, max 3 retries.
    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &P1RadioConnection::onReconnectTimeout);

    // Connect watchdog — single-shot; fires kConnectTimeoutMs after connectToRadio()
    // if no first ep6 frame arrives. Emits connectFailed(Timeout, ...).
    // Cancelled on first good ep6 in onReadyRead(). Design §4.1.
    m_connectWatchdog = new QTimer(this);
    m_connectWatchdog->setSingleShot(true);
    connect(m_connectWatchdog, &QTimer::timeout, this, &P1RadioConnection::onConnectTimeout);

    qCDebug(lcConnection) << "P1: init() socket port:" << m_socket->localPort();
}

// ---------------------------------------------------------------------------
// applySocketBufferSizes
//
// The send and receive buffer sizes, set on every bind (init() and
// bindToRadioFacingAddress(); a closed socket loses them).
// ---------------------------------------------------------------------------
void P1RadioConnection::applySocketBufferSizes()
{
    // 2026-05-26 KG4VCF bench fix: bumped recv buffer from Thetis's
    // 1000 KB (0xfa000) to 4 MB.  See the matching block in
    // P2RadioConnection::init() for the full rationale -- short
    // summary: even with the ConnectionThread elevated to
    // USER_INTERACTIVE QoS, brief preemption windows under heavy
    // build load can stall the kernel-to-userspace handoff long
    // enough to drop I/Q frames at the smaller buffer size.
    m_socket->setSocketOption(QAbstractSocket::SendBufferSizeSocketOption,
                              QVariant(0xfa000));
    m_socket->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption,
                              QVariant(0x400000));  // 4 MB requested; kernel may cap
}

// ---------------------------------------------------------------------------
// bindToRadioFacingAddress
//
// Binds the socket to the local address this host reaches the radio from,
// on a port the OS chooses. Thetis binds its listening socket the same way,
// to the selected network card's own IPv4 address, port 0 unless set:
//   From Thetis NetworkIO.cs:69-70 [v2.10.3.15]:
//     string hostIP = nic.LocalIPv4.ToString();
//     int hostPort = c.SetupForm.ListenToRadioOnUDPPort; // will be any os available port if 0, or specific if set
//   From Thetis NetworkIO.cs:149 [v2.10.3.15]:
//     ret = nativeInitMetis(radioIP, ratioPort, hostIP, hostPort, protocol, model_id);
//   From Thetis network.c:116-118 and 203 [v2.10.3.15]:
//     local.sin_port = htons((u_short)localport);
//     local.sin_family = AF_INET;
//     local.sin_addr.s_addr = inet_addr(localaddr);
//     rc = bind(listenSock, (SOCKADDR*)&local, sizeof(local));
//
// NereusSDR bound to every address (Any) instead. On macOS the OS can give
// a socket bound to Any a port that another socket already holds on one
// address, and a datagram to that address and port goes to the other
// socket: the radio streams, its frames go elsewhere, and the connect
// watchdog fires (tst_slice_cap_every_path lost a reconnect this way to
// another program's socket on 127.0.0.1). A socket bound to the address
// itself gets a port no other socket holds there.
//
// NereusSDR has no network card selection, so the address is the one the
// OS routes to the radio from: the source address the radio answers (on a
// loopback radio, 127.0.0.1; with several interfaces, the one the route to
// the radio leaves by). A UDP connect sends nothing. With no route, or if
// that address cannot be bound, it says so with a warning and binds every
// address. disconnect() and the connect timeout close the socket, so this
// always leaves it bound, with the buffer sizes applied. Called on every
// connect and reconnect attempt; it returns at once when nothing changed.
// ---------------------------------------------------------------------------
void P1RadioConnection::bindToRadioFacingAddress()
{
    if (!m_socket || m_radioInfo.address.isNull()) { return; }

    QHostAddress local = radioFacingAddress();
    if (m_socket->state() == QAbstractSocket::BoundState
        && !local.isNull() && m_socket->localAddress() == local) {
        return;
    }

    m_socket->close();
    if (local.isNull()) {
        qCWarning(lcConnection) << "P1: no local address reaches"
                                << m_radioInfo.address.toString()
                                << "; listening on every address";
    } else if (!m_socket->bind(local, 0)) {
        qCWarning(lcConnection) << "P1: could not listen on" << local.toString()
                                << "(" << m_socket->errorString()
                                << "); listening on every address";
        local = QHostAddress();
    }
    if (local.isNull() && !m_socket->bind(QHostAddress::Any, 0)) {
        qCWarning(lcConnection) << "P1: Failed to bind UDP socket";
        return;
    }
    applySocketBufferSizes();
}

// ---------------------------------------------------------------------------
// radioFacingAddress
//
// The local address the OS routes to m_radioInfo.address from, or a null
// address when none does within kRouteLookupMs. Tests may stand in for the
// lookup (setRadioFacingAddressForTest) to reach the fallback paths.
// ---------------------------------------------------------------------------
QHostAddress P1RadioConnection::radioFacingAddress() const
{
    if (m_radioFacingOverridden) { return m_radioFacingOverride; }
    QUdpSocket route;
    route.connectToHost(m_radioInfo.address, m_radioInfo.port);
    if (route.waitForConnected(kRouteLookupMs)) {
        return route.localAddress();
    }
    return QHostAddress();
}

// ---------------------------------------------------------------------------
// connectToRadio
//
// Binds the socket, sends metis-start, transitions to Connected.
// Source: networkproto1.c:33 SendStartToMetis — sends EF FE 04 01 then
// waits for the first ep6 frame. For Phase 3I Task 9 we transition to
// Connected immediately after sending start, matching P2 behavior.
// ---------------------------------------------------------------------------
void P1RadioConnection::connectToRadio(const RadioInfo& info)
{
    if (m_running) {
        disconnect();
    }

    m_radioInfo = info;
    // Listen on the address the radio answers, before anything is sent.
    bindToRadioFacingAddress();
    // Use HardwareProfile for caps (Phase 3I-RP).
    // Fall back to board-byte lookup if setHardwareProfile() was never called
    // (e.g. direct construction in tests without RadioModel).
    m_caps = m_hardwareProfile.caps
             ? m_hardwareProfile.caps
             : &BoardCapsTable::forBoard(info.boardType);

    // Initialize per-ADC state from profile.
    // Dither and random: Thetis turns both on for every board
    //   Thetis Console/setup.cs:296-298 [v2.10.3.15]
    //     //MW0LGE_21k8 initialise these
    //     chkMercDither.Checked = true;
    //     chkMercRandom.Checked = true;
    // but on a Hermes Lite 2 the same two bits (bank 0 C3 bits 3 and 4) are
    // the HL2 options Band Volts and Disable PS Sync, restored from the HL2
    // option page instead, both off unless the operator turned them on
    //   mi0bot Console/setup.cs:2843-2848 [@c26a8a4]
    //     if (HPSDRModel.HERMESLITE == HardwareSpecific.Model)
    //     {
    //         chkHL2BandVolts_CheckedChanged(this, e);        // MI0BOT: HL2 option page now doesn't share ditter and random
    //         chkHL2PsSync_CheckedChanged(this, e);
    //     }
    // The Thetis HL2 capture shows bank 0 C3 = 0x00
    // (docs/protocols/openhpsdr-protocol1-capture-reference.md, bank 0 table).
    const bool hl2 = isHl2();
    for (int i = 0; i < 3; ++i) {
        m_dither[i] = hl2 ? m_hl2BandVolts : true;
        m_random[i] = hl2 ? m_hl2PsSync : true;
        m_rxPreamp[i] = false;
        m_stepAttn[i] = 0;
    }
    m_txStepAttn = 0;
    m_paEnabled = m_caps->hasPaProfile;
    m_duplex = true;
    m_reconnectedLogged = false;

    m_intentionalDisconnect = false;
    m_epSendSeq = 0;
    m_epRecvSeqExpected = 0;
    m_ccRoundRobinIdx = 0;

    // TX safety: a new link starts unkeyed, whatever a setter delivered
    // before it (see dropTransmitForNewLink). The operator connecting is a
    // fresh start, so the link-loss latch is lifted, as P2's connectToRadio
    // lifts its own.
    dropTransmitForNewLink();
    m_linkLossLatched = false;
    m_linkLossRefusalLogged = false;

    // Thetis hardcodes nddc=4 for plain Hermes/ANAN10/ANAN100 and nddc=2 for
    // ANAN10E/100B (HermesII) in console.cs:8378-8454. No current-source P1
    // path uses nddc=1 for Hermes-class boards. Captured Thetis traffic with
    // the tester's radio shows nddc=2 on the wire (sub0 C4 = 0x08 bits).
    // Default m_activeRxCount to 2 for Hermes-class P1 until we have a
    // model-override setting. parseEp6Frame uses this same value to select
    // the 14-byte nddc=2 slot layout, so sent and received must agree.
    if (m_radioInfo.protocol == ProtocolVersion::Protocol1) {
        // Seeds the DDC-configuration axis; announceRxCount folds in the
        // panadapter axis and writes m_activeRxCount. Not yet running, so
        // this records the value rather than restarting anything.
        m_codecRxCount = 2;
        announceRxCount();
    }

    // Reset reconnect state — fresh connection resets the retry counter.
    // Source: NereusSDR design doc §3.6 — explicit user reconnect clears attempts.
    m_reconnectAttempts = 0;
    m_lastEp6At = QDateTime();
    m_firstEp6Logged = false;
    // R-R3-32 (parity Task 6): the link counters start with the connection.
    m_linkStats.reset();
    m_ep6SeqPrimed = false;
    m_parseFailLogged = false;
    m_firstEmitLogged = false;

    // Apply per-board quirks (attenuator clamp, OC zero-force, etc.) now that
    // m_caps is set. Source: specHPSDR.cs per-HPSDRHW branches.
    applyBoardQuirks();

    // HL2 I/O board init — probe the I/O board via I2C after quirks are set.
    // Source: mi0bot IoBoardHl2.cs:129-145 IOBoard.readRequest(); Task 12.
    if (m_caps->hasIoBoardHl2) {
        hl2SendIoBoardInit();
    }

    // Firmware version refusal/stale-warning paths removed 2026-04-13.
    // Background: Thetis enforces only one firmware refusal in its entire
    // connect path (NetworkIO.cs:136-143, HermesII < 103 only) and has no
    // "stale firmware" warning concept at all. The previous per-board
    // BoardCapabilities thresholds were unattested guesses (see TODO(3I-T2))
    // and were locking out legitimate radios — most visibly, plain Hermes
    // running stock v15 firmware which Thetis accepts without complaint.
    // The RadioConnectionError::FirmwareTooOld / FirmwareStale enum entries
    // remain defined but are no longer emitted from this path.

    setState(ConnectionState::Connecting);
    qCDebug(lcConnection) << "P1: Connecting to" << m_caps->displayName
                          << "at" << info.address.toString() << "port" << info.port
                          << "from local port" << (m_socket ? m_socket->localPort() : 0);

    // Prime the radio with alternating TX-VFO and RX1-VFO ep2 frames BEFORE
    // metis-start so the DDC initializes with the correct phase words,
    // sample rate, and nddc. Without priming, the Hermes firmware streams
    // ADC-idle data (I=DC offset, Q=0) -- confirmed in the tester's pcap
    // where every sample had Q=0 for 20 seconds.
    //
    // Port of Thetis networkproto1.c:35-68 SendStartToMetis + :281
    // MetisReadThreadMainLoop. Thetis sends up to 16 primed ep2 frames
    // (5x ForceCandCFrame(1) inside SendStartToMetis + ForceCandCFrame(3)
    // at the top of MetisReadThreadMainLoop). We collapse that into two
    // ForceCandCFrame(3) equivalents: 3 TX + 3 RX1 before metis-start, and
    // 3 TX + 3 RX1 after.
    m_running = true;
    sendPrimingBurst(3);

    // Send metis-start to begin ep6 stream.
    // Source: networkproto1.c:33-68 SendStartToMetis -- cmd byte 0x01 = IQ only
    sendMetisStart(false);

    // Second priming burst after start, matching Thetis's ForceCandCFrame(3)
    // at MetisReadThreadMainLoop:281.
    sendPrimingBurst(3);

    m_watchdogTimer->start();
    m_ep2PacerClock.restart();
    m_ep2PacketsSent = 0;
    m_ep2PacerTimer->start();
    // State stays Connecting; onReadyRead() promotes Connecting -> Connected
    // on the first ep6 frame. ConnectionState.h:17-20: Connecting means
    // "metis-start sent, awaiting first ep6"; Connected means "data flowing".
    // Issue #239: previously transitioned to Connected here, which made the
    // UI claim "Connected" for the full 2 s connect-watchdog window even when
    // the radio was powered off.

    // Arm the connect watchdog: if no first ep6 arrives within kConnectTimeoutMs,
    // emit connectFailed(Timeout, ...). onReadyRead() cancels this on first good frame.
    if (m_connectWatchdog) {
        m_connectWatchdog->start(kConnectTimeoutMs);
    }

    qCDebug(lcConnection) << "P1: Connecting (metis-start sent, awaiting first ep6)";
}

// ---------------------------------------------------------------------------
// disconnect
//
// Sends metis-stop and closes the socket.
// Source: networkproto1.c:72-110 SendStopToMetis — EF FE 04 00
// ---------------------------------------------------------------------------
void P1RadioConnection::disconnect()
{
    m_intentionalDisconnect = true;

    if (m_watchdogTimer) {
        m_watchdogTimer->stop();
    }
    if (m_ioBoardPollTimer) {
        m_ioBoardPollTimer->stop();
    }
    // Clock chip writes not yet queued are dropped, and lists queued but
    // not known to have reached the radio count as not finished; the next
    // connect sends the options that are on, and those whose lists did not
    // finish (hl2ClockOnDataFlowing). The shared I2C queue is left as it
    // is: the I/O board poll and the manual I2C tool write to it too, and
    // a clock write it still holds is sent again anyway.
    hl2ClockDropPending();
    hl2ClockMarkUnconfirmed();
    if (m_ep2PacerTimer) {
        m_ep2PacerTimer->stop();
    }
    if (m_reconnectTimer) {
        m_reconnectTimer->stop();
    }
    // Cancel the connect watchdog so intentional disconnect() does not
    // trigger connectFailed() after the user has already moved on.
    if (m_connectWatchdog) {
        m_connectWatchdog->stop();
    }

    // Clear reconnect state on explicit disconnect.
    // Source: NereusSDR design doc §3.6 — user reconnect resets the cycle.
    m_reconnectAttempts = 0;
    m_lastEp6At = QDateTime();
    m_reconnectedLogged = false;

    if (m_running && m_socket && !m_radioInfo.address.isNull()) {
        m_running = false;
        sendMetisStop();
        qCDebug(lcConnection) << "P1: metis-stop sent";
    }

    m_running = false;

    // Close the UDP socket so the OS releases the bound port and any
    // pending I/O is drained on this (worker) thread. P2 already does
    // this; P1 previously kept the socket open "for re-use in Task 10"
    // but that never materialized and no caller reuses the socket.
    //
    // Issue #83: on Windows, handing the socket to Qt's parent-chain
    // destructor during process teardown (rather than closing it
    // explicitly while the worker's event loop is still servicing it)
    // left the HL2 UDP endpoint in a state that cascaded into a
    // Winsock stack that could not accept new binds until reboot —
    // Thetis failed to bind port 51188 the next time it was launched.
    if (m_socket) {
        m_socket->close();
    }

    setState(ConnectionState::Disconnected);
    qCDebug(lcConnection) << "P1: Disconnected";
}

void P1RadioConnection::setReceiverFrequency(int receiverIndex, quint64 frequencyHz)
{
    if (receiverIndex < 0 || receiverIndex >= 7) { return; }
    m_rxFreqHz[receiverIndex] = frequencyHz;
    recomputeReceiveFilters(receiverIndex);
}

// ---------------------------------------------------------------------------
// setLiveReceiverSlots: which frame slots carry a live receiver.
//
// Phase 3F section 16.3.2. Thetis takes every receive-side filter selection
// from RX1: the high-pass (UpdateRX1DDSFreq -> setAlex1HPF(_rx1_dds_freq)),
// the receive low-pass (UpdateAlexTXFilter -> setAlexLPF(rx1_dds_freq_mhz))
// and the OC outputs (ExtCtrlEnable(lo_band = band of VFOAFreq, ...)):
//   From Thetis console.cs:15398-15403 [v2.10.3.15]
//     private void UpdateRX1DDSFreq()
//     {
//         if (initializing) return;
//         setAlex1HPF(_rx1_dds_freq);
//         UpdateAlexTXFilter();
//         UpdateAlexRXFilter();
// Thetis's RX1 cannot be closed. NereusSDR's slice A can, and routing by
// frame slot then leaves slice B on slot 1 (slot 2 on the Orion class) with
// slot 0 holding A's last frequency. Reading slot 0 kept every one of those
// filters on A's band: with A on 40 m closed and B on 20 m, a 40 m low-pass
// sat in front of B.
//
// The ruling for the multi-slice case: the live receiver in the LOWEST slot
// stands in for RX1, until a receiver returns on a lower slot. With slot 0
// live this is slot 0, so every state Thetis itself can reach is unchanged.
// ---------------------------------------------------------------------------
void P1RadioConnection::setLiveReceiverSlots(quint32 slotMask)
{
    const quint32 previousMask = m_liveSlotMask;
    m_liveSlotMask = slotMask;

    // The announced-count axis: highest live slot + 1. Zero when nothing is
    // live, so an empty mask never raises the announcement on its own. See
    // the m_slotRxCount note on announceRxCount's declaration.
    int highest = -1;
    for (int slot = 31; slot >= 0; --slot) {
        if (slotMask & (1u << slot)) { highest = slot; break; }
    }
    m_slotRxCount = highest + 1;

    // The RX1 stand-in. An empty mask keeps the previous one: see the
    // declaration.
    if (slotMask != 0) {
        int lowest = 0;
        while (lowest < 31 && (slotMask & (1u << lowest)) == 0) { ++lowest; }
        // m_rxFreqHz holds seven slots; a slot beyond it has no frequency
        // this connection could read, so it cannot stand in.
        if (lowest < 7) {
            m_rx1Slot = lowest;
        }
        // The stand-in or the receivers beside it may have changed; the
        // receive low-pass reads every live slot (fix wave M6).
        if (slotMask != previousMask) {
            recomputeReceiveFilters(-1);
        }
    }

    announceRxCount();
}

// ---------------------------------------------------------------------------
// recomputeReceiveFilters: the receive-side Alex selections, from the RX1
// stand-in (m_rx1Slot, see setLiveReceiverSlots).
// ---------------------------------------------------------------------------
void P1RadioConnection::recomputeReceiveFilters(int changedSlot)
{
    // The RX1 stand-in's frequency. Zero means it has never been tuned.
    const quint64 rx1Hz = m_rxFreqHz[m_rx1Slot];
    // The frequency that just moved: the changed slot's, or the stand-in's
    // when the stand-in itself moved.
    const quint64 frequencyHz = (changedSlot >= 0) ? m_rxFreqHz[changedSlot] : rx1Hz;
    if (frequencyHz == 0) { return; }

    // RX0 drives the Alex HPF bank — recompute on every change.
    // (Phase 3F section 16.3.2: "RX0" is now the RX1 stand-in, m_rx1Slot.)
    // Source: console.cs:6830-6942 [@501e3f5]
    // Upstream tags preserved: //N1GP (from cited console.cs:6830) [v2.10.3.15]
    // Upstream inline attribution preserved verbatim:
    //   :6830  || (HardwareSpecific.Hardware == HPSDRHW.HermesIII)) //DK1HLM
    //
    // HL2 carve-out: HL2 has hasAlexFilters=false (no Alex card slot) but
    // mi0bot's WriteMainLoop_HL2 still emits these bits at bank 10 C3/C4
    // (networkproto1.c:1081-1088 [v2.10.3.14-beta1], C3 then C4; the range
    // here used to read 1086-1093, which lands in the case-11 preamp block
    // rather than on these two bytes) the HL2 firmware
    // and the optional N2ADR I/O board read them to engage the band-correct
    // LPF/HPF.  Without them the HL2 PA can't safely engage on TX and the
    // T/R relay flutters.  Compute the bits for HL2 too, gated on the
    // hardware profile so other "no Alex" boards (Atlas, basic Hermes)
    // remain byte-identical to pre-fix behavior.
    //
    // Ladder selection is a property of the RF board, not of the protocol:
    // Thetis's setAlex1HPF dispatches purely on HardwareSpecific.Hardware with
    // no protocol test anywhere in it, and an ANAN-7000DLE running P1 has the
    // same band-pass front end it has on P2.  So P1 routes through
    // computeRxPreselector exactly like P2 does.
    // From Thetis console.cs:6827-6837 setAlex1HPF [v2.10.3.15]
    // Upstream inline attribution preserved verbatim (console.cs:6830):
    //    || (HardwareSpecific.Hardware == HPSDRHW.HermesC10))  //N1GP G2E added (HermesC10) //DK1HLM
    //
    // HL2 keeps the legacy ladder: HermesLite is not one of the three boards
    // Thetis names, and mi0bot's HL2 path drives the N2ADR filter board from
    // the same high-pass selection it always did.
    // filterCaps() rather than m_caps: the connect-time push runs before
    // connectToRadio assigns m_caps. See the declaration for why.
    const BoardCapabilities* const fcaps = filterCaps();
    if ((changedSlot < 0 || changedSlot == m_rx1Slot) && rx1Hz != 0
        && (   (fcaps && fcaps->hasAlexFilters)
            || m_hardwareProfile.model == HPSDRModel::HERMESLITE)) {
        // With the Alex tab's saved rows (setAlexHpfEdges).
        m_alexHpfBits = codec::alex::computeRxPreselector(
            double(rx1Hz) / 1e6,
            fcaps ? fcaps->board : HPSDRHW::Unknown, m_alexHpfEdges);
    }

    // ── The receive-derived low-pass ────────────────────────────────────
    //
    // Bank 10 C4 is the Alex0 word (networkproto1.c:587-590 [v2.10.3.15]),
    // and while the radio is not keyed Thetis fills it from a RECEIVE
    // frequency, not the transmit one. See m_alexLpfBitsRx for the full
    // routing quote. This half did not exist before Phase 3F: a single slice
    // transmitted and received on the same frequency, so the transmit-derived
    // mask happened to be right. It stops being right the moment the
    // transmitter binds to one slice and the operator listens on another.
    //
    // Gated on MOX because Thetis cannot reach this write while keyed at all:
    //   From Thetis console.cs:15487-15498 UpdateAlexTXFilter [v2.10.3.15]
    //     private void UpdateAlexTXFilter()
    //     { if (!_mox) { ... setAlexLPF(rx1_dds_freq_mhz, false); } }
    //
    // RF-SAFETY: the gate is load-bearing. setAlexLPF writes BOTH masks
    // while keyed (SetAlexLPFBits `isMox || ...`), so a receive selection
    // reaching it mid-transmission would put the receive frequency's
    // low-pass on the transmitter. Thetis never calls it keyed; neither
    // does this.
    //
    // Which receive frequency wins is receiveLpfFrequencyMhz's job. On a
    // board whose RX2 shares this filter it is the HIGHER of the two, because
    // a low-pass passes everything below its corner and the lower receiver's
    // filter would attenuate the higher one.
    //
    // Same hardware carve-out as the high-pass above: the HL2 has no Alex
    // card, but its firmware and the optional N2ADR I/O board read these bits
    // (mi0bot networkproto1.c:1085-1088 [v2.10.3.14-beta1]).
    if (!m_mox) {
        applyReceiveAlexLpf(rx1Hz, frequencyHz);
    }
}

// ---------------------------------------------------------------------------
// applyReceiveAlexLpf: UpdateAlexTXFilter's receive-frequency selection.
// ---------------------------------------------------------------------------
void P1RadioConnection::applyReceiveAlexLpf(quint64 rx1Hz, quint64 fallbackHz)
{
    if (rx1Hz == 0 && fallbackHz == 0) {
        return;  // no receiver tuned yet: nothing to select from
    }
    const BoardCapabilities* const fcaps = filterCaps();

    // Shared-input filters, ruling (c) 2026-09-30: the low-pass follows the
    // highest receiver among the slices the model counted on the input
    // (AlexRxBpf::countedSlotsAdc0), the same set the band-pass was chosen
    // over. Protocol 1 has one filter chain, so that is every counted slice.
    // Thetis's "higher of the two", generalised to every slice that shares
    // the input:
    //   From Thetis console.cs:15491-15495 UpdateAlexTXFilter [v2.10.3.15]
    //     if (!_rx2_preamp_present && chkRX2.Checked)
    //     {
    //         if (rx1_dds_freq_mhz > rx2_dds_freq_mhz) setAlexLPF(rx1_dds_freq_mhz, false);
    //         else setAlexLPF(rx2_dds_freq_mhz, false);
    //     }
    // With nothing counted the rule below stands, as the band-pass falls
    // back to its frequency-derived bits.
    //
    // The frequency compared is each receiver's DDC centre, Thetis's DDS
    // frequency (SharedInputLowPass::Rule::HighestCentre), and the choice
    // is the one RadioModel names in the low-pass reason.
    {
        const QList<SharedInputLowPass::Candidate> candidates = countedCandidates();
        const int best = SharedInputLowPass::highest(
            SharedInputLowPass::Rule::HighestCentre, candidates);
        const quint64 highestHz = (best >= 0) ? candidates.at(best).centreHz : 0;
        if (highestHz != 0) {
            const quint8 oldRxLpf = m_alexLpfBitsRx;
            applyAlexLpf(double(highestHz) / 1e6, /*freqIsTx=*/false);
            if (m_alexLpfBitsRx != oldRxLpf) {
                qCDebug(lcConnection) << "P1::applyReceiveAlexLpf counted"
                                      << "rxLpf=" << Qt::hex << m_alexLpfBitsRx << Qt::dec
                                      << "rx=" << highestHz << "Hz";
            }
            return;
        }
    }

    {
        // m_rxFreqHz[m_rx1Slot] / [1] are Thetis's rx1_dds_freq_mhz /
        // rx2_dds_freq_mhz; m_rx1Slot is 0 whenever slot 0 is live (Phase 3F
        // section 16.3.2, see setLiveReceiverSlots).
        // A zero frequency means that receiver has never been tuned, which is
        // the state chkRX2.Checked reports as unchecked; upstream's rx1 is
        // always tuned by the time this runs, so the fallback below only
        // covers the ordering case where a later receiver is tuned first.
        //
        // Fix wave M6: once the model has told this connection which slots
        // are live (setLiveReceiverSlots), "RX2" is the highest live
        // receiver other than the stand-in. Upstream has exactly RX1 and
        // RX2 and takes the higher of the two; with more live slices on a
        // shared front end the low-pass has to pass the highest of them, or
        // a third slice on a higher band is filtered out. Only live slots
        // count, so a closed slice's stale frequency no longer holds the
        // corner up.
        //
        // Before any slot set arrives (connection-only use), slot 1 stands
        // in for RX2 as before. m_rxFreqHz[1] is never cleared when a slice
        // goes away, so a stale value can outlive its receiver there. That
        // is bounded and benign in one direction: the rule takes a maximum,
        // so a stale entry can only ever hold the corner HIGHER than needed.
        const double rx1Mhz = (rx1Hz != 0)
            ? double(rx1Hz) / 1e6
            : double(fallbackHz) / 1e6;
        double rx2Mhz = 0.0;
        bool rx2Live  = false;
        if (m_liveSlotMask != 0) {
            for (int slot = 0; slot < 7; ++slot) {
                if (slot == m_rx1Slot || (m_liveSlotMask & (1u << slot)) == 0
                    || m_rxFreqHz[slot] == 0) {
                    continue;
                }
                rx2Live = true;
                rx2Mhz = std::max(rx2Mhz, double(m_rxFreqHz[slot]) / 1e6);
            }
        } else {
            rx2Mhz  = double(m_rxFreqHz[1]) / 1e6;
            rx2Live = (m_rxFreqHz[1] != 0);
        }

        // setAlexLPF(freq, false) over the saved rows, with 6m/ByPass on RX.
        const quint8 oldRxLpf = m_alexLpfBitsRx;
        applyAlexLpf(codec::alex::receiveLpfFrequencyMhz(
                         rx1Mhz, rx2Mhz, rx2Live,
                         fcaps ? fcaps->rx2PreampPresent : false),
                     /*freqIsTx=*/false);
        const quint8 newRxLpf = m_alexLpfBitsRx;
        if (newRxLpf != oldRxLpf) {
            // Receive-side counterpart of the setTxFrequency line. Logged on
            // change so a bench can see whether a band button actually
            // reaches the receive filter path at all.
            qCDebug(lcConnection) << "P1::setReceiverFrequency"
                                  << "rxLpf=" << Qt::hex << newRxLpf << Qt::dec
                                  << "hpf=" << Qt::hex << m_alexHpfBits << Qt::dec
                                  << "for" << bandLabel(bandFromFrequency(
                                         double(fallbackHz)))
                                  << "rx=" << fallbackHz << "Hz";
        }
    }
}

void P1RadioConnection::setTxFrequency(quint64 frequencyHz)
{
    m_txFreqHz = frequencyHz;
    // TX freq drives Alex LPF — recompute on every change.
    // Source: console.cs:7177-7243 [v2.10.3.15]
    //
    // RF-SAFETY: this is the only writer of the transmit mask, and it is fed
    // from the TX-bound slice's frequency (plus XIT) by
    // RadioModel::pushTxFrequencyFromTxSlice, never from the active slice and
    // never from a receive retune.
    //   From Thetis console.cs:15464-15468 UpdateTXDDSFreq [v2.10.3.15]
    //     private void UpdateTXDDSFreq()
    //     { if (initializing) return;
    //       setAlexLPF(tx_dds_freq_mhz, true); ... }
    // Upstream inline attribution preserved verbatim (console.cs:15471):
    //   if (MOX)//[2.10.3.13]MW0LGE
    //
    // Deliberately NOT gated on MOX, unlike the receive-derived write in
    // setReceiverFrequency: UpdateTXDDSFreq has no MOX guard, so the
    // transmit selection stays live whether the radio is keyed or not and is
    // ready the instant it is.
    //
    // HL2 carve-out: see setReceiverFrequency above.  mi0bot
    // networkproto1.c:1085-1088 [v2.10.3.14-beta1] emits these bits on HL2
    // even though it has no Alex card. (Was cited as 1090-1093, which is the
    // case-11 preamp block, not the C4 low-pass byte.)
    // setAlexLPF(tx_dds_freq_mhz, true) over the saved rows: Alex1 unkeyed,
    // both words keyed, and 6m/ByPass on RX on Alex0 while unkeyed.
    {
        const quint8 oldLpfBitsTx = m_alexLpfBitsTx;
        applyAlexLpf(double(frequencyHz) / 1e6, /*freqIsTx=*/true);
        const quint8 newLpfBitsTx = m_alexLpfBitsTx;
        if (newLpfBitsTx != oldLpfBitsTx) {
            // The one line that makes the transmit low-pass observable on a
            // bench. Logged on change only, so it marks the event rather than
            // the C&C round-robin cadence.
            qCDebug(lcConnection) << "P1::setTxFrequency txLpf="
                                  << Qt::hex << newLpfBitsTx << Qt::dec
                                  << "for" << bandLabel(bandFromFrequency(
                                         double(frequencyHz)))
                                  << "tx=" << frequencyHz << "Hz";
        }
    }
}

// ---------------------------------------------------------------------------
// applyAlexLpf: Thetis's setAlexLPF on this connection's two masks.
// From Thetis console.cs:7177-7243 [v2.10.3.15]
//   if (!_mox && lpf_bypass) { NetworkIO.SetAlexLPFBits(0x10, false, _mox); ... }
//   if (alexpresent && !initializing) { ... SetAlexLPFBits(bits, freqIsTX, _mox); }
// Bank 10 C4 carries Alex0 (networkproto1.c:587-590 [v2.10.3.15]).
// ---------------------------------------------------------------------------
void P1RadioConnection::applyAlexLpf(double freqMhz, bool freqIsTx)
{
    codec::alex::AlexLpfMasks masks{m_alexLpfBitsRx, m_alexLpfBitsTx};
    codec::alex::setAlexLpf(masks, freqMhz, freqIsTx, m_mox, m_alexLpfBypass,
                            alexLpfPresent(), m_alexLpfEdges);
    m_alexLpfBitsRx = masks.alex0;
    m_alexLpfBitsTx = masks.alex1;
    publishAlexLpfBits(effectiveAlexLpfBits());
}

// HL2 carve-out: mi0bot networkproto1.c:1085-1088 [v2.10.3.14-beta1] emits
// these bits on HL2 even though it has no Alex card.
bool P1RadioConnection::alexLpfPresent() const
{
    const BoardCapabilities* const fcaps = filterCaps();
    return (fcaps && fcaps->hasAlexFilters)
        || m_hardwareProfile.model == HPSDRModel::HERMESLITE;
}
// The panadapter axis of the announced receiver count. Was a bare assignment
// to m_activeRxCount, which both ignored the DDC configuration's needs and
// skipped the stream restart that makes the radio and parseEp6Frame agree on
// the slot layout.
void P1RadioConnection::setActiveReceiverCount(int count)
{
    m_panRxCount = qMax(1, count);
    announceRxCount();
}
void P1RadioConnection::setSampleRate(int sampleRate)
{
    m_sampleRate = sampleRate;
    // From Thetis ChannelMaster/netInterface.c:1287-1310 [v2.10.3.14]:
    // mic_decimation_factor is set from sample rate; counter is reset.
    const int newFactor = micDecimationFactorFor(sampleRate);
    if (newFactor != m_micDecimationFactor) {
        m_micDecimationFactor = newFactor;
        m_micDecimationCount  = 0;  // matches netInterface.c:1310
    }
}

// ---------------------------------------------------------------------------
// micDecimationFactorFor — sample-rate → factor lookup.
// Source: Thetis ChannelMaster/netInterface.c:1287-1310 [v2.10.3.14]
//   case 48000:  mic_decimation_factor = 1; break;
//   case 96000:  mic_decimation_factor = 2; break;
//   case 192000: mic_decimation_factor = 4; break;
//   case 384000: mic_decimation_factor = 8; break;
//   default:     mic_decimation_factor = 4; break;
// ---------------------------------------------------------------------------
int P1RadioConnection::micDecimationFactorFor(int sampleRate) noexcept
{
    switch (sampleRate) {
        case 48000:  return 1;
        case 96000:  return 2;
        case 192000: return 4;
        case 384000: return 8;
        default:     return 4;  // matches netInterface.c:1306-1307 default branch
    }
}

// ---------------------------------------------------------------------------
// decimateMicSamples — downsample radio-rate mic to 48 kHz.
//
// Source: Thetis ChannelMaster/networkproto1.c:391-410 [v2.10.3.14] —
//   for each input sample:
//     mic_decimation_count++;
//     if (mic_decimation_count == mic_decimation_factor) {
//         mic_decimation_count = 0;
//         <emit sample>;
//     }
//
// `counter` is the persistent state — caller owns it (matches the global
// static mic_decimation_count in Thetis, reset once at thread init per
// networkproto1.c:275 [v2.10.3.14]).
//
// factor==1 fast path: copy through unchanged (matches Thetis behavior at
// 48 kHz where the equality check fires every sample).
// ---------------------------------------------------------------------------
void P1RadioConnection::decimateMicSamples(const float* in, int n, int factor,
                                            int& counter,
                                            std::vector<float>& out) noexcept
{
    if (n <= 0 || in == nullptr) { return; }
    if (factor <= 1) {
        out.insert(out.end(), in, in + n);
        return;
    }
    out.reserve(out.size() + static_cast<size_t>(n / factor) + 1);
    for (int i = 0; i < n; ++i) {
        ++counter;
        if (counter == factor) {
            counter = 0;
            out.push_back(in[i]);
        }
    }
}

// ---------------------------------------------------------------------------
// restartStreamWithRate — Task 1.6 (sample-rate live-apply, P1 path)
//
// Issues sendMetisStop() + sendPrimingBurst(3) + sendMetisStart() so the
// radio re-arms its EP6 sender with the new sample rate encoded in bank-0
// C0 bits 24-25.  Pattern mirrors the onReconnectTimeout() restart which
// already demonstrates that stop→prime→start is safe mid-session.
//
// Calls setSampleRate() (not direct m_sampleRate assignment) so the mic
// decimation factor is recomputed for the new rate per Thetis netInterface.c
// [v2.10.3.14].
//
// Cite: networkproto1.c WriteMainLoop / MetisReadThreadMainLoop restart
// pattern [v2.10.3.13] — see onReconnectTimeout for the NereusSDR
// adaptation of the same sequence.
// ---------------------------------------------------------------------------
void P1RadioConnection::restartStreamWithRate(int newSampleRate)
{
    if (!m_running) {
        // Not yet streaming — record the rate via the full setter so mic
        // decimation factor is updated; sendMetisStart() will pick it up.
        setSampleRate(newSampleRate);
        return;
    }
    if (newSampleRate == m_sampleRate) {
        return;  // idempotent
    }
    setSampleRate(newSampleRate);  // updates rate + mic decimation factor
    sendMetisStop();
    sendPrimingBurst(3);
    sendMetisStart(false);
    sendPrimingBurst(3);
}

// ---------------------------------------------------------------------------
// announceRxCount — the single writer for the wire's receiver count
//
// The DDC configuration, the panadapters and the live frame slots each get a
// say, and none can see the others, so the announcement is the max of the
// three (the slot axis: Phase 3F section 16.3.2). See the
// declaration in P1RadioConnection.h for the bench defect that made this a
// derived value rather than three call sites writing one field.
//
// Cheap to over-announce briefly, ruinous to under-announce: the count is the
// ep6 slot layout, so a receiver the codec is expecting simply is not in the
// frame.
// ---------------------------------------------------------------------------
void P1RadioConnection::announceRxCount()
{
    restartStreamWithCount(qMax(qMax(m_codecRxCount, m_panRxCount), m_slotRxCount));
}

// ---------------------------------------------------------------------------
// restartStreamWithCount — Task 1.7 (active-RX-count live-apply, P1 path)
//
// The mechanism behind announceRxCount, private so that no caller can set the
// count knowing only one of its two axes.
//
// Updates m_activeRxCount then stops → primes → starts the EP6 stream so
// the radio re-arms with the new per-frame slot count (bank-0 C0 bits 8-10
// encode nrx-1).  parseEp6Frame() reads m_activeRxCount fresh on every call;
// there is no per-receiver cache to invalidate.
//
// Mirrors restartStreamWithRate() (Task 1.6) — same stop+prime+start cycle.
//
// Cite: networkproto1.c WriteMainLoop bank-0 C0 nrx field [v2.10.3.13].
// ---------------------------------------------------------------------------
void P1RadioConnection::restartStreamWithCount(int newActiveRxCount)
{
    if (!m_running) {
        // Not yet streaming — record the count; bank-0 picks it up at start.
        m_activeRxCount = newActiveRxCount;
        return;
    }
    if (newActiveRxCount == m_activeRxCount) {
        return;  // idempotent
    }
    m_activeRxCount = newActiveRxCount;
    sendMetisStop();
    sendPrimingBurst(3);
    sendMetisStart(false);
    sendPrimingBurst(3);
}

void P1RadioConnection::setAttenuator(int dB)
{
    // v0.4.1 hotfix — flush bank 11 on the next EP2 frame so the new ADC0
    // step-att value lands within ≤2.6 ms instead of waiting up to ~22 ms
    // for the next natural round-robin visit (maxBank=16, two banks per
    // frame at 380.95 fps).  Codex P2 pattern: set the flag BEFORE any
    // clamp / idempotent guard so even no-op writes still flush.  Mirrors
    // peer setters setMicTipRing / setMicBoost / setLineIn / setUserDigOut /
    // setMicPTTDisabled / setPuresignalRun.
    m_forceBank11Next = true;

    // Source: specHPSDR.cs per-HPSDRHW branches + BoardCapabilities registry.
    // Clamp to board-reported range so UI callers can't exceed hardware limits.
    if (m_caps && m_caps->attenuator.present) {
        // Level Cal: an Alex board's wire range reaches the Alex range + 2
        // (console.cs:11044-11056 [v2.10.3.15]).
        const int wireMax = BoardCapsTable::stepAttWireMaxDb(*m_caps);
        if (dB > wireMax) { dB = wireMax; }
        if (dB < m_caps->attenuator.minDb) { dB = m_caps->attenuator.minDb; }
    } else if (m_caps && !m_caps->attenuator.present) {
        dB = 0;
    }
    m_stepAttn[0] = dB;
}

void P1RadioConnection::setAlexAtten(int bits)
{
    // Level Cal: the Alex receive attenuator, bank 0 C3 bits 0-1.
    // From Thetis ChannelMaster/netInterface.c:421-432 [v2.10.3.15]:
    //   void SetAlexAtten(int bits)
    //   {
    //       if (mkiibpf) return;
    //
    //       if ((prbpfilter->_20_dB_Atten | prbpfilter->_10_dB_Atten) != bits)
    //       {
    //           prbpfilter->_20_dB_Atten = (bits & 0x2) == 0x2;
    //           prbpfilter->_10_dB_Atten = bits & 0x1;
    // (The OR compare drops a change to 1 from 2 or 3, as in Thetis.)
    if (m_hardwareProfile.mkiiBpf) {
        return;
    }
    const int current = (m_alex20dB ? 1 : 0) | (m_alex10dB ? 1 : 0);
    if (current == bits) {
        return;
    }
    m_alex20dB = (bits & 0x2) == 0x2;
    m_alex10dB = (bits & 0x1) != 0;
    m_forceBank0Next = true;
}
// ---------------------------------------------------------------------------
// setHl2BandVolts / setHl2PsSync
//
// The HL2 option page's Band Volts and Disable PS Sync. mi0bot sends them as
// the ADC dither and random bits (bank 0 C3 bits 3 and 4), which the HL2
// reads as these two options:
//   mi0bot Console/setup.cs:13376-13390 [@c26a8a4]
//     // MI0BOT: Control band volts for the HL2
//     private void chkHL2BandVolts_CheckedChanged(object sender, System.EventArgs e)
//     {
//         if (initializing) return;
//         int v = chkHL2BandVolts.Checked ? 1 : 0;
//         NetworkIO.SetADCDither(v);
//     }
//     // MI0BOT: Control power supply sync for the HL2
//     private void chkHL2PsSync_CheckedChanged(object sender, System.EventArgs e)
//     {
//         if (initializing) return;
//         int v = chkHL2PsSync.Checked ? 1 : 0;
//         NetworkIO.SetADCRandom(v);
//     }
//   mi0bot ChannelMaster/netInterface.c:435-456 [@c26a8a4]: SetADCDither /
//     SetADCRandom set adc[0..2].dither / .random and send the frame (CmdRx).
//   mi0bot ChannelMaster/networkproto1.c:950-953 [@c26a8a4] (WriteMainLoop_HL2):
//     C3 = ... | ((prn->adc[0].dither << 3) & 0b00001000) |
//          ((prn->adc[0].random << 4) & 0b00010000) | ...
// On an HL2 the change reaches the wire on the next frame; on any other board
// only the stored value changes (it is never sent there).
// ---------------------------------------------------------------------------
bool P1RadioConnection::isHl2() const
{
    return m_hardwareProfile.model == HPSDRModel::HERMESLITE;
}

void P1RadioConnection::setHl2BandVolts(bool on)
{
    m_hl2BandVolts = on;
    if (isHl2()) {
        for (bool& bit : m_dither) {
            bit = on;
        }
        m_forceBank0Next = true;
    }
}

void P1RadioConnection::setHl2PsSync(bool on)
{
    m_hl2PsSync = on;
    if (isHl2()) {
        for (bool& bit : m_random) {
            bit = on;
        }
        m_forceBank0Next = true;
    }
}

void P1RadioConnection::setHl2TxLatency(int ms)
{
    m_hl2TxLatencyMs = ms;
}

void P1RadioConnection::setHl2PttHang(int ms)
{
    m_hl2PttHangMs = ms;
}

void P1RadioConnection::setHl2ResetOnDisconnect(bool on)
{
    m_hl2ResetOnDisconnect = on;
}

void P1RadioConnection::setHl2SwapAudioChannels(bool on)
{
    m_hl2SwapAudioChannels = on;
}

// ---------------------------------------------------------------------------
// HL2 clock chip: External 10 MHz (CL1 input) and the CL2 output
//
// From mi0bot Console/setup.cs:21572-21629 [@c26a8a4]:
//   // MI0BOT: Data to program clock generator in HL2 to accept external 10MHz on CL2
//   //         Data in format of Address, Data
// That comment heads all four tables below; upstream has it once, above
// the first. Four tables of (register, data) pairs, written in order to
// I2C bus 0, address 0xd4 (WriteVersaClockAsync, setup.cs:21647).
// ---------------------------------------------------------------------------
namespace {

using ClockWrites = std::vector<std::pair<quint8, quint8>>;

// From mi0bot Console/setup.cs:21575-21589 [@c26a8a4] clockRegisterData10MhzEnable
// MI0BOT: Data to program clock generator in HL2 to accept external 10MHz on CL2
const ClockWrites kHl2Clock10MhzEnable = {
    {0x10, 0xc0}, {0x13, 0x03}, {0x10, 0x40}, {0x2d, 0x01}, {0x2e, 0x20},
    {0x22, 0x03}, {0x23, 0x00}, {0x24, 0x00}, {0x25, 0x00}, {0x19, 0x00},
    {0x1A, 0x00}, {0x1B, 0x00}, {0x18, 0x00}, {0x17, 0x12}};

// From mi0bot Console/setup.cs:21591-21605 [@c26a8a4] clockRegisterData10MhzDisable
const ClockWrites kHl2Clock10MhzDisable = {
    {0x10, 0xc0}, {0x13, 0x00}, {0x10, 0x80}, {0x2d, 0x01}, {0x2e, 0x10},
    {0x22, 0x00}, {0x23, 0x00}, {0x24, 0x00}, {0x25, 0x00}, {0x19, 0x00},
    {0x1A, 0x00}, {0x1B, 0x00}, {0x18, 0x40}, {0x17, 0x04}};

// From mi0bot Console/setup.cs:21607-21617 [@c26a8a4] clockRegisterDataCl2
// Registers 0x3d, 0x3e and 0x32..0x35 (pairs 3..8) carry the divider
// ControlCl2 computes; the data here is overwritten before every send.
const ClockWrites kHl2ClockCl2 = {
    {0x62, 0x3b}, {0x2c, 0x00}, {0x31, 0x81}, {0x3d, 0x01}, {0x3e, 0x10},
    {0x32, 0x00}, {0x33, 0x00}, {0x34, 0x00}, {0x35, 0x00}, {0x63, 0x01}};

// From mi0bot Console/setup.cs:21619-21629 [@c26a8a4] clockRegisterDataCl2Off
const ClockWrites kHl2ClockCl2Off = {
    {0x62, 0x5b}, {0x2c, 0x00}, {0x31, 0x00}, {0x3d, 0x00}, {0x3e, 0x00},
    {0x32, 0x00}, {0x33, 0x00}, {0x34, 0x00}, {0x35, 0x00}, {0x63, 0x00}};

// The CL2 table with the output divider for cl2FreqKHz.
// From mi0bot Console/setup.cs:21694-21721 [@c26a8a4] ControlCl2:
//   // MI0BOT: Support for HL2 Cl2 clock output
//   Decimal vco = (Decimal)1305.6;
//   if (chkExt10MHz.Checked)
//       vco = 1440;
//   ...
//   Decimal diviser = vco / udCl2Freq.Value;
//   int integer = (int)Decimal.Truncate(diviser);
//   clockRegisterDataCl2[7] = (Byte)((integer >> 4) & 0xff);
//   clockRegisterDataCl2[9] = (Byte)((integer << 4) & 0xf0);
//   Decimal frac = diviser - integer;
//   int intFrac = (int)(frac * (Decimal)(1 << 24));
//   clockRegisterDataCl2[11] = (Byte)((intFrac >> 22) & 0xff);
//   clockRegisterDataCl2[13] = (Byte)((intFrac >> 14) & 0xff);
//   clockRegisterDataCl2[15] = (Byte)((intFrac >> 6) & 0xff);
//   clockRegisterDataCl2[17] = (Byte)((intFrac << 2) & 0xf6);
// udCl2Freq holds three decimal places (setup.designer.cs:11133-11163
// [@c26a8a4], DecimalPlaces = 3), so the frequency here is whole kHz and
// the Decimal division is done exactly in integers: the VCO in kHz
// (1305600 or 1440000) over the frequency in kHz. Checked against Decimal
// arithmetic for every kHz from 1 MHz to 200 MHz on both VCOs: the same
// integer and intFrac every time.
ClockWrites hl2Cl2Writes(bool ext10MHz, int freqKHz)
{
    const qint64 vcoKHz = ext10MHz ? 1440000 : 1305600;
    const qint64 denom = freqKHz;
    const int integer = static_cast<int>(vcoKHz / denom);
    const int intFrac = static_cast<int>(((vcoKHz % denom) << 24) / denom);
    ClockWrites writes = kHl2ClockCl2;
    writes[3].second = static_cast<quint8>((integer >> 4) & 0xff);   // [7]
    writes[4].second = static_cast<quint8>((integer << 4) & 0xf0);   // [9]
    writes[5].second = static_cast<quint8>((intFrac >> 22) & 0xff);  // [11]
    writes[6].second = static_cast<quint8>((intFrac >> 14) & 0xff);  // [13]
    writes[7].second = static_cast<quint8>((intFrac >> 6) & 0xff);   // [15]
    writes[8].second = static_cast<quint8>((intFrac << 2) & 0xf6);   // [17]
    return writes;
}

// Attempts in a row that may find the I2C queue full, with nothing
// leaving it, before a list is given up. From mi0bot Console/setup.cs:21658
// [@c26a8a4]: if (Timeout++ >= 50) break;
constexpr int kHl2ClockMaxStalledAttempts = 50;

} // namespace

// From mi0bot Console/setup.cs:21732-21756 [@c26a8a4], the three handlers:
//   // MI0BOT: Support for HL2 Cl2 clock output
//   chkCl2Enable_CheckedChanged: ControlCl2(chkCl2Enable.Checked);
//   udCl2Freq_ValueChanged:      ControlCl2(chkCl2Enable.Checked);
//   // MI0BOT: Support for HL2 10MHz clock input
//   chkExt10MHz_CheckedChanged:  Enable/DisableCl1_10MHz(); then
//                                ControlCl2(chkCl2Enable.Checked);
// ControlCl2(false) sends the CL2 off table, so a frequency change with
// CL2 off sends it too. mi0bot starts each write list unawaited, so two
// lists can interleave pair by pair; here they go one after the other,
// the 10 MHz list first, and lists not yet started are rebuilt from the
// values wanted now (hl2ClockRequest). mi0bot refuses with "Power must be
// on" while not connected; here the values are stored and sent at the
// next connect.
void P1RadioConnection::setHl2Clock(bool ext10MHz, bool cl2Enable, int cl2FreqKHz)
{
    // From mi0bot setup.designer.cs:11133-11163 [@c26a8a4] udCl2Freq:
    // Minimum 1, Maximum 200 (MHz), three decimal places.
    const int freq = std::clamp(cl2FreqKHz, 1000, 200000);
    const bool extChanged = (ext10MHz != m_hl2Ext10MHz);
    const bool cl2Changed = (cl2Enable != m_hl2Cl2Enable) || (freq != m_hl2Cl2FreqKHz);
    m_hl2Ext10MHz = ext10MHz;
    m_hl2Cl2Enable = cl2Enable;
    m_hl2Cl2FreqKHz = freq;
    if (!isHl2() || state() != ConnectionState::Connected) {
        return;
    }
    if (extChanged) {
        hl2ClockRequest(true, true);
    } else if (cl2Changed) {
        hl2ClockRequest(false, true);
    }
}

// From mi0bot Console/console.cs:28033-28040 [@c26a8a4], after
// DataFlowing = true on power on:
//   if (HardwareSpecific.Model == HPSDRModel.HERMESLITE)
//   {
//       if (SetupForm.Ext10MHzChecked)          // MI0BOT: HL2 external 10 MHz input
//           SetupForm.EnableCl1_10MHz();
//       if (SetupForm.Cl2Checked)               // MI0BOT: HL2 CL2 clock output
//           SetupForm.ControlCl2(SetupForm.Cl2Checked);
//   }
// An option left off sends nothing, unless its list did not finish last
// time: then its current value goes again (the off table included), and
// an External 10 MHz list that goes again takes its CL2 list with it, as
// the checkbox handler does. Called when the first ep6 frame promotes the
// link to Connected, which a reconnect after a lost link also does, so a
// radio that reset while the link was down is set again.
void P1RadioConnection::enterDataFlowing()
{
    setState(ConnectionState::Connected);
    // HL2 clock options that are on go to the radio now, as mi0bot's
    // power-on path sends them once data flows.
    hl2ClockOnDataFlowing();
}

void P1RadioConnection::hl2ClockOnDataFlowing()
{
    if (!isHl2()) {
        return;
    }
    hl2ClockDropPending();
    hl2ClockMarkUnconfirmed();
    // A clear while the link was down is covered by the marking above.
    if (m_ioBoard) {
        m_hl2ClockClearSeen = m_ioBoard->i2cClearCount();
    }
    const bool sendExt = m_hl2Ext10MHz || m_hl2Ext10Incomplete;  // MI0BOT: HL2 external 10 MHz input
    const bool sendCl2 = m_hl2Cl2Enable || m_hl2Cl2Incomplete    // MI0BOT: HL2 CL2 clock output
                         || (sendExt && m_hl2Ext10Incomplete);
    if (sendExt || sendCl2) {
        hl2ClockRequest(sendExt, sendCl2);
    }
}

// Drops every list still waiting and marks its option as not finished.
void P1RadioConnection::hl2ClockDropPending()
{
    for (const Hl2ClockSequence& seq : m_hl2ClockPending) {
        hl2ClockListDropped(seq);
    }
    m_hl2ClockPending.clear();
    if (m_hl2ClockRetryTimer) {
        m_hl2ClockRetryTimer->stop();
    }
}

// Every write of the list is in the I2C queue. The option's incomplete
// flag is left as it is until an ep6 frame shows the list went out
// (hl2ClockConfirmSent).
void P1RadioConnection::hl2ClockListDone(const Hl2ClockSequence& seq)
{
    const quint64 leavesAt = m_ioBoard ? m_ioBoard->i2cEnqueuedCount() : 0;
    for (Hl2ClockSent& sent : m_hl2ClockUnconfirmed) {
        if (sent.kind == seq.kind) {
            sent.leavesAt = leavesAt;
            sent.left = false;
            return;
        }
    }
    m_hl2ClockUnconfirmed.push_back(Hl2ClockSent{seq.kind, leavesAt, false});
}

void P1RadioConnection::hl2ClockListDropped(const Hl2ClockSequence& seq)
{
    if (seq.kind == Hl2ClockKind::Ext10) {
        m_hl2Ext10Incomplete = true;
    } else {
        m_hl2Cl2Incomplete = true;
    }
    // An earlier list of this kind waiting for its ep6 frame must not
    // clear the flag this list set.
    for (auto it = m_hl2ClockUnconfirmed.begin(); it != m_hl2ClockUnconfirmed.end(); ++it) {
        if (it->kind == seq.kind) {
            m_hl2ClockUnconfirmed.erase(it);
            break;
        }
    }
}

// Called for every ep6 frame once the link is Connected. A list's last
// write that has left the I2C queue went out in an ep2 frame; the radio
// still answering one frame later is taken as that frame having reached
// it, and the option's list counts as finished.
void P1RadioConnection::hl2ClockConfirmSent()
{
    if (!m_ioBoard) {
        return;
    }
    // The dequeued count is read before the clear count: writes a clear
    // dropped then never pass for writes that left.
    const quint64 dequeued = m_ioBoard->i2cDequeuedCount();
    std::atomic_thread_fence(std::memory_order_acquire);
    if (hl2ClockCheckCleared()) {
        return;
    }
    for (auto it = m_hl2ClockUnconfirmed.begin(); it != m_hl2ClockUnconfirmed.end();) {
        if (it->left) {
            if (it->kind == Hl2ClockKind::Ext10) {
                m_hl2Ext10Incomplete = false;
            } else {
                m_hl2Cl2Incomplete = false;
            }
            it = m_hl2ClockUnconfirmed.erase(it);
            continue;
        }
        if (dequeued >= it->leavesAt) {
            it->left = true;
        }
        ++it;
    }
}

// The link went down (disconnect, or the watchdog's LinkLost) before
// these lists were known to reach the radio: they go again at the next
// connect.
void P1RadioConnection::hl2ClockMarkUnconfirmed()
{
    for (const Hl2ClockSent& sent : m_hl2ClockUnconfirmed) {
        if (sent.kind == Hl2ClockKind::Ext10) {
            m_hl2Ext10Incomplete = true;
        } else {
            m_hl2Cl2Incomplete = true;
        }
    }
    m_hl2ClockUnconfirmed.clear();
}

// The manual I2C tool's Clear Queue (IoBoardHl2::clearI2cQueue) drops
// every write still in the queue, clock writes included. A cleared write
// was never sent, so this is handled as a disconnect is: every list not
// yet seen leaving the queue, and a list part way into it, is marked not
// finished, and while the link is up both go again at once, a started
// list from its first write. A list that did leave just before the clear
// is sent again too; the queue cannot tell it from one the clear dropped.
// Returns true when it sent lists again (hl2ClockRequest pumps them).
bool P1RadioConnection::hl2ClockCheckCleared()
{
    if (!m_ioBoard) {
        return false;
    }
    const quint64 clears = m_ioBoard->i2cClearCount();
    if (clears == m_hl2ClockClearSeen) {
        return false;
    }
    m_hl2ClockClearSeen = clears;
    bool ext10 = false;
    bool cl2 = false;
    for (auto it = m_hl2ClockUnconfirmed.begin(); it != m_hl2ClockUnconfirmed.end();) {
        if (it->left) {
            ++it;
            continue;
        }
        if (it->kind == Hl2ClockKind::Ext10) {
            m_hl2Ext10Incomplete = true;
            ext10 = true;
        } else {
            m_hl2Cl2Incomplete = true;
            cl2 = true;
        }
        it = m_hl2ClockUnconfirmed.erase(it);
    }
    if (!m_hl2ClockPending.empty() && m_hl2ClockPending.front().next > 0) {
        const Hl2ClockSequence started = m_hl2ClockPending.front();
        m_hl2ClockPending.erase(m_hl2ClockPending.begin());
        hl2ClockListDropped(started);
        if (started.kind == Hl2ClockKind::Ext10) {
            ext10 = true;
        } else {
            cl2 = true;
        }
    }
    if (!(ext10 || cl2) || !isHl2() || state() != ConnectionState::Connected) {
        return false;
    }
    // An External 10 MHz list that goes again takes its CL2 list with it.
    hl2ClockRequest(ext10, cl2 || ext10);
    return true;
}

// Queues the lists a change needs. A list already started stays at the
// front (the chip is part way through it); every list behind it is
// dropped, and what those lists were for is rebuilt from the values
// wanted now, so rapid changes end on the last one with no stale list in
// between. A list is left out only when the started list already sends
// exactly it; a CL2 list always follows an External 10 MHz list, as
// mi0bot's handler calls ControlCl2 after it.
void P1RadioConnection::hl2ClockRequest(bool ext10, bool cl2)
{
    // The started list, when there is one, is element 0 and stays there;
    // it is read by index because the push_backs below can move it.
    const bool hasStarted = !m_hl2ClockPending.empty() && m_hl2ClockPending.front().next > 0;
    const std::size_t keep = hasStarted ? 1 : 0;
    for (std::size_t i = keep; i < m_hl2ClockPending.size(); ++i) {
        if (m_hl2ClockPending[i].kind == Hl2ClockKind::Ext10) {
            ext10 = true;
        } else {
            cl2 = true;
        }
    }
    m_hl2ClockPending.resize(keep);

    const quint64 dequeued = m_ioBoard ? m_ioBoard->i2cDequeuedCount() : 0;
    bool extQueued = false;
    if (ext10) {
        if (!(hasStarted && m_hl2ClockPending[0].kind == Hl2ClockKind::Ext10
              && m_hl2ClockPending[0].extOn == m_hl2Ext10MHz)) {
            Hl2ClockSequence seq;
            seq.kind = Hl2ClockKind::Ext10;
            seq.extOn = m_hl2Ext10MHz;
            seq.writes = m_hl2Ext10MHz ? kHl2Clock10MhzEnable : kHl2Clock10MhzDisable;
            seq.dequeuedAtFailure = dequeued;
            m_hl2ClockPending.push_back(std::move(seq));
            extQueued = true;
        }
    }
    if (cl2) {
        ClockWrites writes = m_hl2Cl2Enable ? hl2Cl2Writes(m_hl2Ext10MHz, m_hl2Cl2FreqKHz)
                                            : kHl2ClockCl2Off;
        const bool startedSendsIt = !extQueued && hasStarted
            && m_hl2ClockPending[0].kind == Hl2ClockKind::Cl2
            && m_hl2ClockPending[0].writes == writes;
        if (!startedSendsIt) {
            Hl2ClockSequence seq;
            seq.kind = Hl2ClockKind::Cl2;
            seq.writes = std::move(writes);
            seq.dequeuedAtFailure = dequeued;
            m_hl2ClockPending.push_back(std::move(seq));
        }
    }
    // A started list keeps its retry timer; otherwise start sending now.
    if (!hasStarted || !(m_hl2ClockRetryTimer && m_hl2ClockRetryTimer->isActive())) {
        hl2ClockPump();
    }
}

// From mi0bot Console/setup.cs:21631-21680 [@c26a8a4] WriteVersaClockAsync:
//   // MI0BOT: Support for HL2 Cl2 clock output
//   console.SetI2CPollingPause(true);
//   for (int i = 0; i < registerData.Length; i += 2)
//   {
//       do
//       {
//           status = NetworkIO.I2CWrite(0, 0xd4, (int)registerData[i], registerData[i + 1]);
//           if (Timeout++ >= 50) break;
//           await Task.Delay(1);
//       } while (status != 0);
//       if (50 <= Timeout) { MessageBox.Show("IC2 timed out." ...); break; }
//       if (status == -1) { MessageBox.Show("IC2 write failed." ...); break; }
//   }
//   console.SetI2CPollingPause(false);
// I2CWrite fails only when the I2C queue is full (netInterface.c:1535-1564
// [@c26a8a4]). Two departures, both because the queue here drains one
// write per I2C frame (about every 4 ms) rather than on mi0bot's timing:
//  - mi0bot's Timeout counts every attempt in the call, successes
//    included, so a 50-write stretch could time out while the queue was
//    draining. Here a list is given up only after 50 attempts in a row
//    found the queue full with nothing leaving it; a write that goes in,
//    or a queue that moved since the last attempt, starts the count again.
//  - mi0bot waits 1 ms after every attempt, a write that went in
//    included. Here writes go in back to back while the queue has room
//    and a full queue is tried again 1 ms later; the I2C frame cadence
//    sets the pace on the wire either way.
// The I/O board poll waits while clock writes are pending
// (ioBoardPollTick), which is what mi0bot's polling pause does here; the
// shared pause the manual I2C tool holds is left alone.
void P1RadioConnection::hl2ClockPump()
{
    if (m_hl2ClockRetryTimer) {
        m_hl2ClockRetryTimer->stop();
    }
    if (!m_ioBoard) {
        hl2ClockDropPending();
        return;
    }
    // A clear sends the lists it dropped again (and pumps them itself).
    if (hl2ClockCheckCleared()) {
        return;
    }
    while (!m_hl2ClockPending.empty()) {
        Hl2ClockSequence& seq = m_hl2ClockPending.front();
        if (seq.next >= seq.writes.size()) {
            hl2ClockListDone(seq);
            m_hl2ClockPending.erase(m_hl2ClockPending.begin());
            continue;
        }
        IoBoardHl2::I2cTxn txn;
        txn.bus = 0;
        txn.address = 0xd4;
        txn.control = seq.writes[seq.next].first;
        txn.writeData = seq.writes[seq.next].second;
        txn.isRead = false;
        txn.needsResponse = false;
        if (m_ioBoard->enqueueI2c(txn)) {
            ++seq.next;
            seq.failures = 0;
            continue;
        }
        const quint64 dequeued = m_ioBoard->i2cDequeuedCount();
        if (dequeued != seq.dequeuedAtFailure) {
            seq.failures = 0;
            seq.dequeuedAtFailure = dequeued;
        }
        if (++seq.failures >= kHl2ClockMaxStalledAttempts) {
            qCWarning(lcConnection) << "HL2: clock chip I2C writes timed out;"
                                    << (seq.writes.size() - seq.next)
                                    << "writes not sent";
            hl2ClockListDropped(seq);
            m_hl2ClockPending.erase(m_hl2ClockPending.begin());
            continue;
        }
        if (!m_hl2ClockRetryTimer) {
            m_hl2ClockRetryTimer = new QTimer(this);
            m_hl2ClockRetryTimer->setSingleShot(true);
            connect(m_hl2ClockRetryTimer, &QTimer::timeout, this,
                    &P1RadioConnection::hl2ClockPump);
        }
        m_hl2ClockRetryTimer->start(1);
        return;
    }
}

void P1RadioConnection::setAttenuatorForAdc(int adc, int dB)
{
    // R-R3-46 / R-R3-11: ADC 0 is setAttenuator's, bank 11. ADC 1 and 2 go
    // out in bank 12 (composeCcForBank case 12 and the codecs), from
    // prn->adc[1..2].rx_step_attn, which Thetis sets with:
    // From Thetis netInterface.c:860-868 [v2.10.3.15]:
    //   void SetADC2StepAttenData(int data)
    //   {
    //       if (prn->adc[1].rx_step_attn != data)
    //       {
    //           prn->adc[1].rx_step_attn = data;
    // Bank 12 has no early flush; the round-robin reaches it within one
    // cycle of the banks.
    if (adc == 0) {
        setAttenuator(dB);
        return;
    }
    if (adc < 1 || adc > 2) {
        return;
    }
    // The same board range as ADC 0.
    if (m_caps && m_caps->attenuator.present) {
        // Level Cal: an Alex board's wire range reaches the Alex range + 2
        // (console.cs:11044-11056 [v2.10.3.15]).
        const int wireMax = BoardCapsTable::stepAttWireMaxDb(*m_caps);
        if (dB > wireMax) { dB = wireMax; }
        if (dB < m_caps->attenuator.minDb) { dB = m_caps->attenuator.minDb; }
    } else if (m_caps && !m_caps->attenuator.present) {
        dB = 0;
    }
    m_stepAttn[adc] = dB;
}

void P1RadioConnection::setPreamp(bool enabled)
{
    // v0.4.1 hotfix — flush bank 11 on the next EP2 frame so the new
    // preamp bit lands within ≤2.6 ms.  Same Codex P2 pattern as
    // setAttenuator above; matches peer-setter parity.
    m_forceBank11Next = true;
    m_rxPreamp[0] = enabled;
}

void P1RadioConnection::setRx2Preamp(bool enabled)
{
    // Level Cal: Thetis SetRX2Preamp sets prn->rx[1].preamp
    // (ChannelMaster/netInterface.c:758-767 [v2.10.3.15]), which
    // networkproto1.c:595 [v2.10.3.15] sends as C1 bit 1:
    //   C1 = (prn->rx[0].preamp & 1) | ((prn->rx[1].preamp & 1) << 1) |
    // Flushed on the next frame, as setPreamp is.
    m_forceBank11Next = true;
    m_rxPreamp[1] = enabled;
}
// ---------------------------------------------------------------------------
// setTxDrive — 3M-1c follow-up (HL2 bench triage 2026-04-29)
//
// Stores the TX drive level (0-255) and forces bank 10 onto the wire on the
// next sendCommandFrame() so the new drive level reaches the radio within
// ≤1 EP2 frame (~2.6 ms at 380.95 pps).
//
// Bank 10 C1 byte carries the drive level; the codec reads ctx.txDrive which
// is sourced from m_txDrive in buildCodecContext.  Without this setter, the
// drive level was silently fixed at zero and HL2 / Atlas / Hermes / Angelia /
// Orion never produced RF on TUN or SSB.  The bug was latent because the
// 3M-1b SSB voice bench tests ran on ANAN-G2 (P2), which has its own
// setTxDrive on P2RadioConnection that always worked.
//
// From Thetis ChannelMaster/networkproto1.c:579 [v2.10.3.13]:
//   C1 = prn->tx[0].drive_level & 0xFF;
// From mi0bot networkproto1.c:1061 [@c26a8a4]: identical encoding.
// ---------------------------------------------------------------------------
void P1RadioConnection::setTxDrive(int level)
{
    const int clamped = qBound(0, level, 255);
    if (m_txDrive == clamped) {
        return;  // idempotent — wire emit already in flight via round-robin
    }
    m_txDrive = clamped;
    // Codex P2 safety-effect pattern: force bank 10 on the next frame so
    // the new drive level reaches the wire within ≤1 EP2 frame.  Without
    // this, the round-robin schedule would defer bank 10 by up to 17
    // frames (~45 ms), which is plenty long for a TUN tap to land mid-
    // round-robin and never see a non-zero drive.
    m_forceBank10Next = true;
}

// ---------------------------------------------------------------------------
// setTxStepAttenuation — 3M-1a Task F.2
//
// Mirrors Thetis ChannelMaster/netInterface.c:1006 SetTxAttenData(int bits)
// [v2.10.3.13]: broadcasts the TX step attenuator value to all ADCs.
// The P1 codec reads m_txStepAttn in composeCcBank0 / composeCcBank1 and
// writes it to the appropriate C&C byte.
//
// From Thetis ChannelMaster/netInterface.c:1006-1016 [v2.10.3.13]:
//   void SetTxAttenData(int bits) {
//     for (i = 0; i < MAX_ADC; i++) prn->adc[i].tx_step_attn = bits;
//     if (listenSock != INVALID_SOCKET) CmdTx();
//   }
// ---------------------------------------------------------------------------
void P1RadioConnection::setTxStepAttenuation(int dB)
{
    // PR #212 follow-up bench fix (J.J. KG4VCF, 2026-05-07):
    // The HL2 PureSignal AutoAtt loop drives SATT into the negative range
    // (mi0bot HL2 signed ATT semantics: 0..31 = attenuation, -1..-28 = preamp
    // gain — see P1CodecHl2.cpp bank 11 C4 emission).  Pre-fix this clamped
    // negative dB to 0 unconditionally, silently dropping every preamp-gain
    // request.  AutoAtt would step the user-facing scalar from 0 to -28
    // searching for adequate feedback level, but the wire byte stayed at
    // (31 - 0) | 0x40 = 0x5F = no attenuation, so HL2 firmware never engaged
    // any preamp gain.  fbLevel pinned at the no-preamp value (~77 on the
    // user's 40m bench) and AutoAtt ran to the floor without converging.
    //
    // Mi0bot Thetis SetTxAttenData (netInterface.c:1027-1041) has NO clamp
    // at the equivalent layer — it forwards the user-facing signed value
    // straight through to the bank emission code, which performs the
    // (31 - userDb) inversion to produce the wire byte.  We mirror that
    // here: HL2 gets the signed [-28, +31] range; non-HL2 P1 boards keep
    // the original [0, 31] unsigned range (5-bit attenuator only).
    //
    // The codec-side qBound at P1CodecHl2.cpp:335 still clamps to
    // [-28, +31] as a defense-in-depth — that's the value the (31 - userDb)
    // inversion needs to produce wire bits in [0, 59].
    const bool isHl2 = (m_hardwareProfile.model == HPSDRModel::HERMESLITE);
    const int loBound = isHl2 ? -28 : 0;
    const int hiBound = isHl2 ?  31 : 31;  // standard boards 5-bit; HL2 6-bit signed range -28..+31
    if (dB < loBound) { dB = loBound; }
    if (dB > hiBound) { dB = hiBound; }

    // Phase 3M-4 Task 17 P1 follow-up: arm bank-4 flush BEFORE the
    // idempotent guard (Codex P2 ordering) so PureSignal auto-attenuate
    // tick gets deterministic ≤1-frame ATT updates instead of waiting
    // for bank 4 to come around in the round-robin (~16 frames / ~42 ms).
    //
    // Mirrors Thetis ChannelMaster/netInterface.c:1006-1016 SetTxAttenData()
    // [v2.10.3.13]:
    //   void SetTxAttenData(int bits) {
    //     for (i = 0; i < MAX_ADC; i++) prn->adc[i].tx_step_attn = bits;
    //     if (listenSock != INVALID_SOCKET) CmdTx();   // ← explicit send
    //   }
    // CmdTx() in P1 land is the equivalent of "flush a fresh bank 4
    // packet"; sendCommandFrame() with m_forceBank4Next=true is our
    // round-robin equivalent.
    m_forceBank4Next = true;

    if (m_txStepAttn == dB) {
        return;  // idempotent — flush flag already set above
    }
    m_txStepAttn = dB;
}
// ---------------------------------------------------------------------------
// setMox — 3M-1a Task E.3
//
// Wire-byte emission: the MOX bit is C0 byte 3 bit 0 (0x01) in the P1 C&C
// bank-0 frame.  composeCcBank0Full() writes:
//   out[0] = m_mox ? 0x01 : 0x00;
// Source: deskhpsdr/src/old_protocol.c:3597 [@120188f]
//   output_buffer[C0] |= 0x01;  // Always set MOX if non-CW transmitting
// HL2 firmware cross-check: dsopenhpsdr1.v:297
//   ds_cmd_ptt_next = eth_data[0]  // bit 0 of the C0 byte = PTT/MOX
//
// Codex P2 — safety-effect-first idempotent-guard pattern:
//   1. Force a bank-0 frame on the NEXT sendCommandFrame() call so the MOX
//      bit lands on the wire within ≤1 frame regardless of round-robin phase.
//      This is the safety effect; it fires even on repeated calls with the
//      same value (ensures the bit is actually emitted).
//   2. Guard: if the requested value equals the stored value, update the
//      flush flag and return — no further state change.
//
// CW gating (txmode == modeCWU/L branch from deskhpsdr:3596-3598) is 3M-2.
// ---------------------------------------------------------------------------
void P1RadioConnection::setMox(bool enabled)
{
    // Codex P2 safety effect: force bank 0 on next frame so the MOX bit
    // is emitted within ≤1 frame of this call.
    // Source: deskhpsdr/src/old_protocol.c:3595-3599 [@120188f]
    m_forceBank0Next = true;

    // TX safety (whole-branch review 2026-09-30, fix round 1): after a link
    // lost while keyed, a key is refused until the reconnect is back unkeyed
    // or the key has been released. A key still queued from before the loss,
    // or one made during the outage, must not ride the reconnect onto the
    // air. Follows P2RadioConnection::setMox, which refuses a key through
    // its link-loss latch the same way (there only a new connectToRadio
    // lifts it, as P2 does not reconnect by itself).
    if (enabled && m_linkLossLatched) {
        if (!m_linkLossRefusalLogged) {
            m_linkLossRefusalLogged = true;
            qCWarning(lcConnection) << "P1: key refused after a link loss;"
                                    << "it is refused until the key is released or"
                                    << "the link is back unkeyed";
        }
        return;
    }
    if (!enabled) {
        m_linkLossLatched = false;
        m_linkLossRefusalLogged = false;
    }

    if (m_mox == enabled) {
        return;  // idempotent — state unchanged, flush flag already set above
    }
    // On MOX engage, arm the TX I/Q ring pre-prime flag.  See P1 header
    // m_txIqPrimePending declaration + P2RadioConnection.h cushion
    // rationale.  At P1's 48 kHz wire rate, 20 ms cushion = 960 samples.
    if (enabled) {
        m_txIqPrimePending.store(true, std::memory_order_release);
        // G-07: the full-ring loss count runs per key, as on Protocol 2.
        m_txIqOverflowSamples.store(0, std::memory_order_relaxed);
    } else {
        // G-05 follow-up: nothing queued for this key goes out after it.
        discardTxIqOnUnkey();
    }
    m_mox = enabled;

    // The low-pass on both MOX edges, as HdwMOXChanged re-drives it:
    //   From Thetis console.cs:29097-29099 (key) and 29146-29148 (unkey)
    //   [v2.10.3.15]
    //     UpdateRX1DDSFreq();   // -> UpdateAlexTXFilter, if (!_mox) only
    //     UpdateRX2DDSFreq();
    //     UpdateTXDDSFreq();    // -> setAlexLPF(tx_dds_freq_mhz, true)
    // Keyed, the transmit selection goes to both words; unkeyed, the receive
    // selection returns to Alex0 and the transmit one stays on Alex1.
    if (!m_mox) {
        applyReceiveAlexLpf(m_rxFreqHz[m_rx1Slot], m_rxFreqHz[m_rx1Slot]);
    }
    if (m_txFreqHz != 0) {  // Thetis's tx_dds_freq_mhz is always set
        applyAlexLpf(double(m_txFreqHz) / 1e6, /*freqIsTx=*/true);
    }
    publishAlexLpfBits(effectiveAlexLpfBits());
    m_forceBank10Next = true;
}

// ---------------------------------------------------------------------------
// dropTransmitForNewLink: TX safety (whole-branch review, 2026-09-30).
//
// A P1 link that was lost while transmitting used to reconnect with m_mox,
// the PureSignal run flag, the T/R relay and the drive as they were, so the
// reconnect's priming burst and every ep2 frame after it carried C0 bit 0
// set and the radio keyed again with no operator action.
//
// Thetis never puts the MOX bit on a start-up frame. The frames it primes
// the radio with before and after metis-start are built from zeros:
//   From Thetis ChannelMaster/networkproto1.c:106-118 [v2.10.3.15]
//     void ForceCandCFrames(int count, int c0, int vfofreq) {
//         unsigned char buf[1024];
//         int	i;
//         memset(buf, 0, sizeof(buf));
//         ...
//         buf[3] = 0;						    /* c0 */
// (the second subframe's C0 is the bank address 2 or 4, bit 0 clear), and
// Thetis does not reconnect a lost link at all: the operator powers the
// radio on again. The reconnect is NereusSDR's (design doc section 3.6), so
// what it resets follows P2RadioConnection::connectToRadio on this branch,
// which starts every generation with MOX, PureSignal and the relay off.
// The drive is zeroed as well; RadioModel restores it on the next key
// (restoreNormalTxDrive) and TUNE and two-tone set their own.
//
// Bank 0 is forced onto the next frame so the cleared MOX bit leads.
// ---------------------------------------------------------------------------
void P1RadioConnection::dropTransmitForNewLink()
{
    if (m_mox) {
        // The ordinary unkey: queued TX I/Q dropped, the receive low-pass
        // back on Alex0, bank 0 and bank 10 flushed.
        setMox(false);
    }
    m_txIqPrimePending.store(false, std::memory_order_release);
    m_puresignalRun = false;
    m_trxRelay = false;
    m_txDrive = 0;
    m_forceBank0Next = true;
}

// ---------------------------------------------------------------------------
// discardTxIqOnUnkey: G-05 follow-up (2026-09-29).
//
// Porting from Thetis ChannelMaster/networkproto1.c:723 [v2.10.3.15]
// (sendProtocol1Samples), original C logic:
//   if (!XmitBit) memset(prn->outIQbufp, 0, sizeof(complex) * 126);
// Thetis never puts the transmit stream's I/Q on the wire while it is not
// transmitting, so nothing of one over can start the next. fillTxZone sends
// whole 63-sample zones only, so a remainder under one zone stayed in this
// ring after an unkey and led the next key. The unkey drops what is queued
// (after the send ring's wait, only that remainder; after a stop, whatever
// was left) and the key's cushion, if that key never sent a block.
//
// Runs on the connection thread, the ring's consumer: advancing the read
// position and taking the count down is the consumer's own operation, so
// the SPSC contract holds. When the TX channel's drain finishes normally,
// its RF gate is shut and its in-flight sends waited for before MOX off
// reaches here (TxChannel::closeRfGateAndWaitForSender), so no producer
// write follows. The exception is the drain wait's timeout: then the gate
// closes through txaFlushed's lambda queued to the TX thread
// (RadioModel's closeRfGate connection), so one late sendTxIq can land
// after this discard. At most one 63-sample zone of old audio or zeros
// then leads the next key.
// ---------------------------------------------------------------------------
void P1RadioConnection::discardTxIqOnUnkey() noexcept
{
    m_txIqPrimePending.store(false, std::memory_order_release);
    const int queued = m_txIqCount.load(std::memory_order_acquire);
    if (queued <= 0) {
        return;
    }
    static constexpr int kBufBytes = kTxIqBufSamples * kTxIqBytesPerSample;
    const int rp = m_txIqReadPos.load(std::memory_order_relaxed);
    m_txIqReadPos.store((rp + queued * kTxIqBytesPerSample) % kBufBytes,
                        std::memory_order_relaxed);
    m_txIqCount.fetch_sub(queued, std::memory_order_relaxed);
}
// ---------------------------------------------------------------------------
// setAntennaRouting — Phase 3P-I-a
//
// Stores trxAnt into m_antennaIdx; the next round-robin pass through
// bank 0 composes it into C4 bits 0-1 via P1CodecStandard::bank0 (or
// P1CodecHl2::bank0 on HL2 — identical encoding at the antenna bit level).
//
// From Thetis ChannelMaster/networkproto1.c:463-468 [v2.10.3.13 @501e3f5] —
//   if (prbpfilter->_ANT_3 == 1)       C4 = 0b10;
//   else if (prbpfilter->_ANT_2 == 1)  C4 = 0b01;
//   else                                C4 = 0b0;
//
// 3P-I-b (T4): rxOnlyAnt (C3 bits 5-6) and rxOut (C3 bit 7) are live —
// forwarded through buildCodecContext() into P1Codec::bank0 per Thetis
// networkproto1.c:455-461 [v2.10.3.13 @501e3f5].
// ---------------------------------------------------------------------------
void P1RadioConnection::setAntennaRouting(AntennaRouting r)
{
    const int clamped = (r.trxAnt < 1 || r.trxAnt > 3) ? 0 : (r.trxAnt - 1);
    m_antennaIdx = clamped;  // 0..2 (or 0 for "no selection")
    // RX-only input mux: clamp to 0..3 per netInterface.c:479-481 [v2.10.3.13 @501e3f5]
    m_rxOnlyAnt = (r.rxOnlyAnt < 0) ? 0 : (r.rxOnlyAnt > 3 ? 3 : r.rxOnlyAnt);
    m_rxOut     = r.rxOut;
    // P1 has no high-priority packet; the next EP2 frame picks up all
    // antenna fields via buildCodecContext() → P1Codec::bank0.
}

// ---------------------------------------------------------------------------
// setAlexRxBpf — Phase 3F. Per-ADC RX band-pass decision.
//
// Reported by CT1IQI on PR #293 (2026-05-31). Protocol 1 carries ONE Alex
// filter word (bank 10 C3) for the whole radio — there is no per-ADC split on
// the wire, so the single chain has to satisfy every slice that is receiving.
// Thetis reaches the same conclusion from the other end: its only
// multi-receiver filter logic runs precisely on the boards that have one
// filter board, and widens the single HPF rather than picking one receiver.
//   From Thetis console.cs:15500-15510 UpdateAlexRXFilter [v2.10.3.15]
//
// ADC0's decision is the one that applies: it is the chain every slice sits
// behind. hpfBitsAdc1 is accepted and ignored, because a P1 board cannot
// address a second filter chain even when it has a second ADC.
//
// The prior behaviour was not last-tune-wins on P1 (setReceiverFrequency
// gates on receiverIndex == 0, matching Thetis's setAlex1HPF(_rx1_dds_freq)),
// but it has the same defect: a slice on receiver 1 in another band was
// filtered out by receiver 0's HPF with nothing anywhere saying so.
// ---------------------------------------------------------------------------
void P1RadioConnection::setAlexRxBpf(AlexRxBpf b)
{
    m_alexRxHpfOverride = b.hpfBitsAdc0;
    // Shared-input filters, ruling (c): the receive low-pass (and on the
    // HL2 the N2ADR board's receive band, ocBandFrequencyHz) follows the
    // same counted slices. Unkeyed only, as every receive-derived low-pass
    // write (console.cs:15487-15498 [v2.10.3.15], `if (!_mox)`).
    if (m_countedSlotsAdc0 != b.countedSlotsAdc0) {
        m_countedSlotsAdc0 = b.countedSlotsAdc0;
        if (!m_mox) {
            applyReceiveAlexLpf(m_rxFreqHz[m_rx1Slot], m_rxFreqHz[m_rx1Slot]);
        }
    }
    // Same as setAntennaRouting: the next EP2 frame picks this up through
    // buildCodecContext() → P1Codec::bank10.
}

// The effective bank-10 C3 HPF bits: AlexController's decision for the chain
// when there is one, otherwise the RX0-frequency-derived value.
quint8 P1RadioConnection::effectiveAlexHpfBits() const
{
    const quint8 selected = m_alexRxHpfOverride >= 0
                                ? static_cast<quint8>(m_alexRxHpfOverride)
                                : m_alexHpfBits;
    // The Alex tab's high-pass switches (plan Task 14 and its fix wave):
    // HPF Bypass on TX, HPF Bypass, Disable 6m LNA on RX / TX, and on the
    // band-pass boards HPF Bypass on PureSignal feedback. Thetis, for both
    // the high-pass and the band-pass boards:
    //   From Thetis console.cs:6841-6848 [v2.10.3.15] (setAlexHPF)
    //     if (alexpresent && !initializing)
    //     {
    //         if (_mox && disable_hpf_on_tx)
    //         {
    //             NetworkIO.SetAlexHPFBits(0x20);
    //   From Thetis console.cs:6957 [v2.10.3.15] (setBPF1ForOrionIISaturn)
    //     if (_mox && (disable_hpf_on_tx || (disable_hpf_on_ps && PureSignalEnabled)))
    // codec::alex::applyAlex1HpfSwitches holds the rule and the board list.
    // `alexpresent` is hasAlexFilters here. The HL2 has no Alex board, and
    // its bank 10 C3 bits are left as they were.
    const BoardCapabilities* const fcaps = filterCaps();
    if (!fcaps || !fcaps->hasAlexFilters) {
        return selected;
    }
    return codec::alex::applyAlex1HpfSwitches(selected, fcaps->board, m_mox,
                                              m_puresignalRun, alexHpfSwitches());
}

// ---------------------------------------------------------------------------
// effectiveAlexLpfBits: which low-pass mask bank 10 C4 carries.
//
// Protocol 1 has one low-pass field where Protocol 2 has two words, and
// Thetis fills it from prbpfilter, the Alex0 struct
// (networkproto1.c:587-590 [v2.10.3.15]). Alex0 takes the transmit selection
// while keyed and the receive selection while not:
//   From Thetis ChannelMaster/netInterface.c:682-726 [v2.10.3.15]
//     if (isMox || !isTX) -> AlexLPFMask = bits
// with the transmit caller passing isTX = true and the receive caller
// unreachable while keyed (`if (!_mox)` around UpdateAlexTXFilter,
// console.cs:15487-15498 [v2.10.3.15]).
//
// Selecting here rather than re-driving on the MOX edges produces identical
// wire bytes in every state and cannot drift if an edge is ever missed. Same
// approach as P2RadioConnection::effectiveLpfBitsAlex0.
// ---------------------------------------------------------------------------
quint8 P1RadioConnection::effectiveAlexLpfBits() const
{
    return m_mox ? m_alexLpfBitsTx : m_alexLpfBitsRx;
}

// ---------------------------------------------------------------------------
// ocBandFrequencyHz: the frequency whose band selects the OC outputs.
//
// Plan Task 14 (3M-1 transmit). Thetis picks the transmit mask from the
// band of the VFO that transmits, and the receive mask from VFO A's band:
//   From Thetis HPSDR/Penny.cs:174-177 [v2.10.3.15]
//     if (tx && VFOBTX)
//         bits = TXABitMasks[idxb];
//     else if (tx)
//         bits = TXABitMasks[idx];
//     else bits = RXABitMasks[idx];
// mi0bot's HL2 branch has the same transmit rule:
//   From mi0bot-Thetis HPSDR/Penny.cs:174-181 [@c26a8a4]
//     if (HardwareSpecific.Model == HPSDRModel.HERMESLITE)    // MI0BOT: Select correct LPF for 2 receivers
//     {
//         if (tx)
//         {
//             if (VFOBTX)
//                 bits = TXABitMasks[idxb];
//             else
//                 bits = TXABitMasks[idx];
//         }
//         else
//         {
//             if (Console.getConsole().RX2Enabled && (idxb > idx))     // MI0BOT: Select the filter for the high band
// VFOBTX is chkVFOBTX:
//   From Thetis console.cs:39833-39834 [v2.10.3.15]
//     Audio.VFOBTX = chkVFOBTX.Checked;
//     Penny.getPenny().VFOBTX = chkVFOBTX.Checked; // MW0LGE_21j
// so both arms name the transmitting VFO. NereusSDR's transmitting slice is whichever holds
// the transmitter, and m_txFreqHz is its frequency plus XIT, the same
// frequency the Alex transmit low-pass uses
// (RadioModel::pushTxFrequencyFromTxSlice).
//
// Before this, the keyed byte read the RX1 stand-in's band. On the HL2 the
// N2ADR board's transmit low-pass is chosen by these pins, so a 40 m
// carrier on slice B with slice A on 20 m went out through the 30/20 m
// low-pass, its second harmonic unfiltered.
//
// A transmit frequency of 0 has never been pushed; the radio cannot be
// transmitting RF on it, and the receive band is kept rather than a band
// the transmitter is not on.
//
// Unkeyed, the band is the RX1 stand-in's VFO frequency, not its DDC
// centre. Thetis's band is the VFO's:
//   From Thetis console.cs:29101-29106 [v2.10.3.15] (HdwMOXChanged)
//     Band lo_band = BandByFreq(XVTRForm.TranslateFreq(VFOAFreq), rx1_xvtr_index, current_region);
//     Band lo_bandb = BandByFreq(XVTRForm.TranslateFreq(VFOBFreq), rx2_xvtr_index, current_region);
//     if (penny_ext_ctrl_enabled) //MW0LGE_21k
//     {
//         int bits = Penny.getPenny().UpdateExtCtrl(lo_band, lo_bandb, _mox, _tuning, SetupForm.TestIMD, chkExternalPA.Checked); //MW0LGE_21j
// and under CTUN the centre can sit in another band than the VFO. A slot
// whose VFO has not been told keeps its centre.
//
// Shared-input filters, ruling (c) 2026-09-30: on the HL2, unkeyed, the
// receive arm of mi0bot's rule is taken. The N2ADR board's low-pass is
// chosen by these pins, and one receive filter serves every receiver, so
// it is set for the HIGH band:
//   From mi0bot-Thetis HPSDR/Penny.cs:183-189 [@c26a8a4]
//         else
//         {
//             if (Console.getConsole().RX2Enabled && (idxb > idx))     // MI0BOT: Select the filter for the high band
//                 bits = RXABitMasks[idxb];
//             else
//                 bits = RXABitMasks[idx];
//         }
// mi0bot has two receivers. Here the candidates are the slices the model
// counted on the input (AlexRxBpf::countedSlotsAdc0), the same set the
// band-pass was chosen over, and the pins are those of the band of the one
// with the highest frequency, read as for the Alex low-pass (the DDC
// centre, SharedInputLowPass::Rule::HighestCentrePins, the one RadioModel
// names in the low-pass reason). The N2ADR bank is low-pass filters, so the
// filter for the highest counted receiver passes every lower one. On a
// frequency tie the earlier slot keeps it, RX1 first, as `idxb > idx` keeps
// RX1. With nothing counted RX1 decides alone, as before.
// Divergence (maintainer ruling, 2026-09-30): NereusSDR orders by
// frequency; mi0bot compares band enum values (Penny.cs:158-159 and
// 183-189 [@c26a8a4], over enums.cs:280-322), which rank WWV and the SWL
// bands above 10 m.
// Divergence: when RX1's band is out of range (GEN and the like, idx < 0),
// mi0bot sends bits = 0 (Penny.cs:162-165 [@c26a8a4]), but here an
// out-of-range band ranks below every band in range (extCtrlBandIndex -1),
// so the other counted slice's band is taken.
// ---------------------------------------------------------------------------
quint64 P1RadioConnection::ocBandFrequencyHz() const
{
    if (m_mox && m_txFreqHz != 0) {
        return m_txFreqHz;
    }
    const auto slotHz = [this](int slot) {
        return (m_rxVfoHz[slot] != 0) ? m_rxVfoHz[slot] : m_rxFreqHz[slot];
    };
    if (!m_mox && m_hardwareProfile.model == HPSDRModel::HERMESLITE
        && m_countedSlotsAdc0 != 0) {
        const QList<SharedInputLowPass::Candidate> candidates = countedCandidates();
        // The receiver hl2ReceivePins takes the pins from, so the band
        // reported with the byte is the one sent.
        const int best = m_ocMatrix
            ? SharedInputLowPass::hl2PinsReceiver(*m_ocMatrix, candidates)
            : SharedInputLowPass::highest(SharedInputLowPass::Rule::HighestCentrePins,
                                          candidates);
        if (best >= 0) {
            return SharedInputLowPass::ruleHz(SharedInputLowPass::Rule::HighestCentrePins,
                                              candidates.at(best));
        }
    }
    return slotHz(m_rx1Slot);
}

// ---------------------------------------------------------------------------
// countedCandidates: the receivers counted on the input
// (AlexRxBpf::countedSlotsAdc0, live ones when the live set is known), in
// slot order, each with its DDC centre and its slice's VFO, for
// SharedInputLowPass. Shared-input filters, ruling (c) 2026-09-30.
// ---------------------------------------------------------------------------
QList<SharedInputLowPass::Candidate> P1RadioConnection::countedCandidates() const
{
    const quint32 counted = (m_liveSlotMask != 0)
        ? (m_countedSlotsAdc0 & m_liveSlotMask) : m_countedSlotsAdc0;
    QList<SharedInputLowPass::Candidate> candidates;
    for (int slot = 0; slot < 7; ++slot) {
        if ((counted & (1u << slot)) != 0) {
            candidates.append({slot, m_rxFreqHz[slot], m_rxVfoHz[slot]});
        }
    }
    return candidates;
}

// ---------------------------------------------------------------------------
// setReceiverVfoFrequencies: each frame slot's slice VFO, for the OC band
// (ocBandFrequencyHz). Slots past the vector, or past the seven this
// connection carries, are 0.
// ---------------------------------------------------------------------------
void P1RadioConnection::setReceiverVfoFrequencies(const QVector<quint64>& vfoHzBySlot)
{
    for (int slot = 0; slot < 7; ++slot) {
        m_rxVfoHz[slot] = (slot < vfoHzBySlot.size()) ? vfoHzBySlot.at(slot) : 0;
    }
}

// ---------------------------------------------------------------------------
// setWatchdogEnabled
//
// R-R3-49: the Network Watchdog setting (Setup > General > Options). On
// Protocol 1 Thetis sends nothing for it: the start and stop packets are the
// same whatever it says (sendMetisStart / sendMetisStop). It sets how long
// the read loop waits for data before it declares the radio lost: three
// seconds with it on, for ever with it off. onWatchdogTick applies the wait.
//
// From Thetis setup.cs:18024-18028 [v2.10.3.15]:
//   private void chkNetworkWDT_CheckedChanged(object sender, EventArgs e)
//   {
//       if (initializing) return;
//       NetworkIO.SetWatchdogTimer(Convert.ToInt32(chkNetworkWDT.Checked));
//   }
// From Thetis netInterface.c:1364-1372 [v2.10.3.15]:
//   void SetWatchdogTimer(int enable)
//   {
//       if (prn->wdt != enable)
//       {
//           prn->wdt = enable;
//           if (listenSock != INVALID_SOCKET)
//               CmdGeneral();
//       }
//   }
// (CmdGeneral is the Protocol 2 general packet; nothing is sent on P1.)
// From Thetis networkproto1.c:292-294 [v2.10.3.15]:
//   //MW0LGE_21g WSAWaitForMultipleEvents(1, &prn->hDataEvent, FALSE, WSA_INFINITE, FALSE);
//   //added similar timout code from ReadThreadMainLoop
//   DWORD retVal = WSAWaitForMultipleEvents(1, &prn->hDataEvent, FALSE, prn->wdt ? 3000 : WSA_INFINITE, FALSE);
// (mi0bot-Thetis networkproto1.c:297 and 443 [@c26a8a4] are the same.)
//
// Turning it on while data has stopped starts the wait from then, so the
// radio is not declared lost the moment the box is ticked.
// ---------------------------------------------------------------------------
void P1RadioConnection::setWatchdogEnabled(bool enabled)
{
    if (m_watchdogEnabled == enabled) {
        return;
    }
    m_watchdogEnabled = enabled;
    if (enabled && m_lastEp6At.isValid()) {
        m_lastEp6At = QDateTime::currentDateTimeUtc();
    }
}

// ---------------------------------------------------------------------------
// txSendStats: G-07
//
// The transmit diagnostics' view of this path (the Core's "Transmit ended"
// line). Protocol 1 counts only what a full ring refused; the send-timing
// counters are Protocol 2's sender thread's.
// ---------------------------------------------------------------------------
RadioConnection::TxSendStats P1RadioConnection::txSendStats() const
{
    TxSendStats st;
    st.valid = true;
    st.overflowOnly = true;
    st.overflowSamples = m_txIqOverflowSamples.load(std::memory_order_relaxed);
    return st;
}

// ---------------------------------------------------------------------------
// txIqRingDrained / txIqRingLengthMs: G-05 (JJ's ruling 2026-09-28)
//
// The unkey waits for the ring to drain, bounded by its length. fillTxZone
// takes whole 63-sample zones only, so fewer than 63 queued samples are
// never sent on their own: the ring is drained below one zone. Its length
// is kTxIqBufSamples at the 48 kHz EP2 wire rate (84 ms).
// ---------------------------------------------------------------------------
bool P1RadioConnection::txIqRingDrained() const
{
    static constexpr int kSamplesPerZone = 63;   // fillTxZone's zone
    return m_txIqCount.load(std::memory_order_acquire) < kSamplesPerZone;
}

double P1RadioConnection::txIqRingLengthMs() const
{
    static constexpr double kWireRateHz = 48000.0;
    return static_cast<double>(kTxIqBufSamples) * 1000.0 / kWireRateHz;
}

// ---------------------------------------------------------------------------
// sendTxIq — 3M-1a Task E.2
//
// Porting from deskhpsdr/src/old_protocol.c:2373-2459 [@120188f]
// (old_protocol_iq_samples) — original C logic:
//
//   Per sample (8 bytes each, TXRING_AUDIO_SAMPLE_BYTES = 8):
//     TXRINGBUF[iptr++] = left_audio_sample >> 8;   // mic L hi
//     TXRINGBUF[iptr++] = left_audio_sample;         // mic L lo
//     TXRINGBUF[iptr++] = right_audio_sample >> 8;   // mic R hi
//     TXRINGBUF[iptr++] = right_audio_sample;         // mic R lo
//     TXRINGBUF[iptr++] = isample >> 8;               // I hi
//     TXRINGBUF[iptr++] = isample;                    // I lo
//     TXRINGBUF[iptr++] = qsample >> 8;               // Q hi
//     TXRINGBUF[iptr++] = qsample;                    // Q lo
//
//   Float→int16 gain (old protocol = 16-bit, not 24-bit):
//     gain = 32767.0  (deskhpsdr/src/transmitter.c:1541 [@120188f])
//     isample = (long)(is * gain + (is >= 0.0 ? 0.5 : -0.5))
//
// Accepts n interleaved float32 I/Q pairs [I0,Q0, I1,Q1, ...] from the
// WDSP TX channel output.  Converts each pair to 8 wire bytes and appends
// them to the ring buffer m_txIqBuf.  If the buffer is full, excess samples
// are dropped with a debug log (matches deskhpsdr overflow path).
//
// The EP2 pacer (onEp2PacerTick → sendCommandFrame) drains 63+63 = 126
// samples per frame call via fillTxZone().
//
// Cite: deskhpsdr/src/old_protocol.c:458-463 [@120188f]
//   TXRING_AUDIO_SAMPLE_BYTES   8
//   TXRING_AUDIO_FRAMES_PER_BLOCK 126  — one SDR block (= two EP2 subframes)
// ---------------------------------------------------------------------------
void P1RadioConnection::sendTxIq(const float* iq, int n)
{
    if (n <= 0 || iq == nullptr) { return; }

    // From deskhpsdr/src/transmitter.c:1541 [@120188f]
    //   gain = 32767.0;  // 16 bit (ORIGINAL_PROTOCOL)
    static constexpr float kGain = 32767.0f;

    static constexpr int kBufBytes = kTxIqBufSamples * kTxIqBytesPerSample;

    // First call after MOX engage: push a 20 ms cushion of zero samples
    // into the ring (960 samples at the P1 48 kHz wire rate, each sample
    // is a pre-zeroed 8-byte slot: mic L/R + I/Q all zero).  Gives
    // fillTxZone's 63-sample-per-zone drain headroom while the producer
    // settles.  Single-writer safety: only sendTxIq mutates the ring;
    // setMox sets the flag.  See header m_txIqPrimePending declaration.
    if (m_txIqPrimePending.exchange(false, std::memory_order_acq_rel)) {
        constexpr int kPrimeSamples = 960;  // 20 ms at 48 kHz wire rate
        // Clamp the cushion to what the ring can actually take.  The loop
        // below is the only write in this function that did not check the
        // count first: toggle MOX off and back on before the consumer has
        // drained the previous transmission and the unconditional
        // fetch_add pushed m_txIqCount past kTxIqBufSamples.  The write
        // pointer wraps and overwrites unread samples while fillTxZone()
        // trusts the inflated count and transmits the clobbered slots as
        // valid I/Q.  Codex review, PR #291; same clamp as the P2 path.
        const int used = m_txIqCount.load(std::memory_order_acquire);
        const int primeSamples =
            std::min(kPrimeSamples, std::max(0, kTxIqBufSamples - used));
        if (primeSamples > 0) {
            int wp = m_txIqWritePos.load(std::memory_order_relaxed);
            for (int i = 0; i < primeSamples; ++i) {
                // Zero all 8 bytes of this slot: mic_L hi/lo, mic_R hi/lo, I hi/lo, Q hi/lo.
                m_txIqBuf[wp++] = 0;
                m_txIqBuf[wp++] = 0;
                m_txIqBuf[wp++] = 0;
                m_txIqBuf[wp++] = 0;
                m_txIqBuf[wp++] = 0;
                m_txIqBuf[wp++] = 0;
                m_txIqBuf[wp++] = 0;
                m_txIqBuf[wp++] = 0;
                if (wp >= kBufBytes) { wp = 0; }
            }
            m_txIqWritePos.store(wp, std::memory_order_relaxed);
            m_txIqCount.fetch_add(primeSamples, std::memory_order_release);
        }
        if (primeSamples < kPrimeSamples) {
            qCDebug(lcConnection)
                << "P1 TX I/Q pre-prime truncated to" << primeSamples
                << "samples (ring already held" << used << ")";
        }
    }

    // HL2 CWX firmware workaround: clear LSB of I/Q low bytes to avoid
    // the CWX activation-while-key-asserted misbehavior. Per
    // deskhpsdr/src/old_protocol.c:2441-2453 [@120188f]. Cost: 1 LSB of
    // I/Q resolution on HL2's 12-bit DAC (immaterial).
    const bool isHl2 = (m_hardwareProfile.model == HPSDRModel::HERMESLITE);
    const quint8 iLoMask = isHl2 ? 0xFE : 0xFF;
    const quint8 qLoMask = isHl2 ? 0xFE : 0xFF;

    int pushedSamples = 0;
    for (int k = 0; k < n; ++k) {
        // Ring-buffer full: drop sample, matching deskhpsdr overflow path.
        // acquire: see the latest fetch_sub from the connection thread so we
        // don't overfill after a drain.
        // The rest of the block is dropped, never written over unread
        // slots; the loss is counted below (G-07).
        if (m_txIqCount.load(std::memory_order_acquire) >= kTxIqBufSamples) {
            qCDebug(lcConnection) << "P1 TX I/Q ring buffer overflow: dropping"
                                  << (n - k) << "samples";
            break;
        }

        const float fI = iq[k * 2];
        const float fQ = iq[k * 2 + 1];

        // Float → int16 conversion.
        // From deskhpsdr/src/transmitter.c:1787-1788 [@120188f]
        //   isample = (long)(is * gain + (is >= 0.0 ? 0.5 : -0.5));
        //   qsample = (long)(qs * gain + (qs >= 0.0 ? 0.5 : -0.5));
        auto toInt16 = [](float v) -> int16_t {
            const float scaled = v * kGain + (v >= 0.0f ? 0.5f : -0.5f);
            if (scaled >= 32767.0f)  { return  32767; }
            if (scaled <= -32767.0f) { return -32767; }
            return static_cast<int16_t>(scaled);
        };
        const int16_t iSample = toInt16(fI);
        const int16_t qSample = toInt16(fQ);

        // Write 8 bytes: [mic_L hi][mic_L lo][mic_R hi][mic_R lo][I hi][I lo][Q hi][Q lo]
        // From deskhpsdr/src/old_protocol.c:2429-2458 [@120188f]
        //   mic bytes zero — NullMicSource (3M-1b will fill them)
        // Radio codec (2026-09-30): the first four bytes are the receive
        // audio L/R for the radio's speaker out, not mic data. They are
        // zero here; fillRadioAudioZone writes them as the frame is built.
        int wp = m_txIqWritePos.load(std::memory_order_relaxed);
        m_txIqBuf[wp++] = 0;                                                      // mic_L hi
        m_txIqBuf[wp++] = 0;                                                      // mic_L lo
        m_txIqBuf[wp++] = 0;                                                      // mic_R hi
        m_txIqBuf[wp++] = 0;                                                      // mic_R lo
        m_txIqBuf[wp++] = static_cast<quint8>((iSample >> 8) & 0xFF);            // I hi
        m_txIqBuf[wp++] = static_cast<quint8>( iSample       & iLoMask);         // I lo
        m_txIqBuf[wp++] = static_cast<quint8>((qSample >> 8) & 0xFF);            // Q hi
        m_txIqBuf[wp++] = static_cast<quint8>( qSample       & qLoMask);         // Q lo
        if (wp >= kBufBytes) { wp = 0; }
        // relaxed: single writer; the release on m_txIqCount below provides
        // the visibility fence for the byte writes.
        m_txIqWritePos.store(wp, std::memory_order_relaxed);
        // release: publishes the byte writes above before the count increment
        // is observed by the connection thread's acquire load.
        m_txIqCount.fetch_add(1, std::memory_order_release);
        ++pushedSamples;
    }

    if (pushedSamples < n) {
        m_txIqOverflowSamples.fetch_add(static_cast<quint64>(n - pushedSamples),
                                        std::memory_order_relaxed);
    }

    if (pushedSamples > 0 && m_mox) {
        // Producer-side rate telemetry, the counterpart to the
        // incTxIqUnderrun() call in fillTxZone().  Without this the perf
        // overlay showed a producer rate of zero for every P1/HL2
        // transmission while valid I/Q was being queued, which reads as
        // producer starvation and points debugging at the wrong end of the
        // chain.  Compared against the 48 kHz P1 wire rate (P2's equivalent
        // is compared against 192 kHz).  Gated on m_mox so the metric
        // reflects only TX-engaged samples, matching P2RadioConnection.
        // Codex review, PR #291.
        PerfMonitor::instance().incTxIqProduced(pushedSamples);
    }
}

// ---------------------------------------------------------------------------
// fillTxZone
//
// Drains exactly 63 samples from the TX I/Q ring buffer into a 504-byte EP2
// TX data zone (63 × 8 bytes = 504 bytes).  If fewer than 63 samples are
// buffered, the zone is zero-filled (silence — matches deskhpsdr behaviour
// when the ring buffer underruns).
//
// Called from sendCommandFrame() for each of the two 504-byte zones in the
// 1032-byte Metis EP2 frame ([16..519] and [528..1031]).
//
// Cite: deskhpsdr/src/old_protocol.c:545-549 [@120188f]
//   memcpy(output_buffer + 8, &TXRINGBUF[out], 504);
//   ozy_send_buffer();
//   memcpy(output_buffer + 8, &TXRINGBUF[out + 504], 504);
//   ozy_send_buffer();
//
// Returns true when samples were available, false on underrun.
// ---------------------------------------------------------------------------
bool P1RadioConnection::fillTxZone(quint8* zone63) noexcept
{
    static constexpr int kSamplesPerZone = 63;
    static constexpr int kBufBytes       = kTxIqBufSamples * kTxIqBytesPerSample;

    // acquire: makes the audio thread's byte writes (published via release
    // fetch_add on m_txIqCount) visible before we read m_txIqBuf.
    if (m_txIqCount.load(std::memory_order_acquire) < kSamplesPerZone) {
        // Underrun — zero-fill the zone (silence).  zone63 is already zeroed
        // by sendCommandFrame()'s memset, so no explicit fill is needed.
        // Count the 63 sample slots that would have carried mic-derived I/Q
        // but ended up zero-padded on the wire, but ONLY while MOX is
        // engaged.  P1 EP2 emits TX zones continuously alongside command
        // banks even when not transmitting; the radio ignores those zones
        // when PA is off, so idle underrun is expected and not the bug
        // we're chasing.  Counting only during MOX makes the metric
        // directly answer "how much silence leaked into the transmission".
        if (m_mox) {
            PerfMonitor::instance().incTxIqUnderrun(kSamplesPerZone);
        }
        return false;
    }

    for (int i = 0; i < kSamplesPerZone; ++i) {
        int rp = m_txIqReadPos.load(std::memory_order_relaxed);
        zone63[i * kTxIqBytesPerSample + 0] = m_txIqBuf[rp++]; // mic_L hi
        zone63[i * kTxIqBytesPerSample + 1] = m_txIqBuf[rp++]; // mic_L lo
        zone63[i * kTxIqBytesPerSample + 2] = m_txIqBuf[rp++]; // mic_R hi
        zone63[i * kTxIqBytesPerSample + 3] = m_txIqBuf[rp++]; // mic_R lo
        zone63[i * kTxIqBytesPerSample + 4] = m_txIqBuf[rp++]; // I hi
        zone63[i * kTxIqBytesPerSample + 5] = m_txIqBuf[rp++]; // I lo
        zone63[i * kTxIqBytesPerSample + 6] = m_txIqBuf[rp++]; // Q hi
        zone63[i * kTxIqBytesPerSample + 7] = m_txIqBuf[rp++]; // Q lo
        if (rp >= kBufBytes) { rp = 0; }
        // relaxed: single writer on this side; the acquire on m_txIqCount
        // above already provides the required ordering fence.
        m_txIqReadPos.store(rp, std::memory_order_relaxed);
    }
    // relaxed: the audio thread observes this via its acquire load on
    // m_txIqCount before deciding whether the buffer has space.
    m_txIqCount.fetch_sub(kSamplesPerZone, std::memory_order_relaxed);
    return true;
}

// ---------------------------------------------------------------------------
// fillRadioAudioZone — radio codec (2026-09-30)
//
// Writes the receive audio for the radio's own speaker out into the L/R
// bytes of the 63 samples of one EP2 zone. Porting from Thetis
// ChannelMaster/networkproto1.c:726-740 [v2.10.3.15] (sendProtocol1Samples;
// pbuffs[0] is outLRbufp, the receive audio, 126 L/R pairs a packet):
//   for (i = 0; i < 4 * 63; i += 2)			// swap L & R audio; firmware bug fix
//   {
//       swap = pbuffs[0][i + 0];
//       pbuffs[0][i + 0] = pbuffs[0][i + 1];
//       pbuffs[0][i + 1] = swap;
//   }
//   for (i = 0; i < 2 * 63; i++)				// for each sample from both sets, 8 bytes per
//       for (j = 0; j < 2; j++)					// for a sample from each set, 4 bytes per
//           for (k = 0; k < 2; k++)				// for each component of the sample, 2 per
//           {
//               temp = pbuffs[j][i * 2 + k] >= 0.0 ? (short)floor(pbuffs[j][i * 2 + k] * 32767.0 + 0.5) :
//                   (short)ceil(pbuffs[j][i * 2 + k] * 32767.0 - 0.5);
//               ...
//               prn->OutBufp[8 * i + 4 * j + 2 * k + 0] = (char)((temp >> 8) & 0xff);
//               prn->OutBufp[8 * i + 4 * j + 2 * k + 1] = (char)(temp & 0xff);
//           }
// The HL2 swaps only while Swap audio channels is on. From mi0bot
// ChannelMaster/networkproto1.c:1231-1239 [@c26a8a4]:
//   if (prn->swap_audio_channels)				// To cater for different firmware at the hardware, allow control of audio channels swapping
//   {
//       for (i = 0; i < 4 * 63; i += 2)			// swap L & R audio; firmware bug fix
//       ...
//   }
//
// NereusSDR: the value is held to +/-32767 before the conversion, where
// Thetis's (short) cast of a sample past full scale is undefined in C++.
// Thetis overwrites the L/R data with EER data while transmitting in
// EER/ETR mode (networkproto1.c:717-721 [v2.10.3.15]); NereusSDR has no
// EER mode, so that branch is not ported. While the audio ring holds none
// due (takeRadioAudio), the zone's L/R bytes stay zero.
// ---------------------------------------------------------------------------
void P1RadioConnection::fillRadioAudioZone(quint8* zone63) noexcept
{
    static constexpr int kSamplesPerZone = 63;
    float lr[kSamplesPerZone * 2];
    if (!takeRadioAudio(lr, kSamplesPerZone)) {
        return;
    }
    const bool swap = isHl2() ? m_hl2SwapAudioChannels : true;
    const auto toShort = [](float x) -> int16_t {
        const double scaled = x >= 0.0f ? std::floor(double(x) * 32767.0 + 0.5)
                                        : std::ceil(double(x) * 32767.0 - 0.5);
        return static_cast<int16_t>(std::clamp(scaled, -32767.0, 32767.0));
    };
    for (int i = 0; i < kSamplesPerZone; ++i) {
        const float left = lr[i * 2 + 0];
        const float right = lr[i * 2 + 1];
        const int16_t first = toShort(swap ? right : left);
        const int16_t second = toShort(swap ? left : right);
        quint8* slot = zone63 + i * kTxIqBytesPerSample;
        slot[0] = static_cast<quint8>((first >> 8) & 0xFF);
        slot[1] = static_cast<quint8>(first & 0xFF);
        slot[2] = static_cast<quint8>((second >> 8) & 0xFF);
        slot[3] = static_cast<quint8>(second & 0xFF);
    }
}

// ---------------------------------------------------------------------------
// setTrxRelay — 3M-1a Task E.4
//
// Sets or clears the Alex T/R relay engage state.
// Wire location: bank 10 (C0=0x12), C3 byte, bit 7 (0x80).
// Semantic INVERTED vs MOX: 1 = relay disabled (PA bypass / RX-only protect),
//                           0 = relay engaged (normal TX path).
//
// enabled=true  → bit 7 = 0 (relay engaged, current flows through relay)
// enabled=false → bit 7 = 1 (relay open / PA bypassed)
//
// Primary cite: deskhpsdr/src/old_protocol.c:2909-2910 [@120188f]
//   if (txband->disablePA || !pa_enabled)
//       output_buffer[C3] |= 0x80; // disable Alex T/R relay
//
// HL2 note: HL2 clears C2/C3/C4 entirely for its own PA-enable path
// (old_protocol.c:2964-2966 [@120188f]), so this bit is irrelevant for
// HL2 hardware.  The composeCcForBank case 10 emits it only for
// Alex-equipped boards; the codec layer handles HL2-specific encoding.
//
// Codex P2 pattern (safety effect before idempotent guard):
// Force bank 10 onto the wire within ≤1 frame of this call so the relay
// state change is immediate, matching deskhpsdr's non-deferred behaviour.
// ---------------------------------------------------------------------------
void P1RadioConnection::setTrxRelay(bool enabled)
{
    // Codex P2: force bank 10 flush BEFORE the idempotent guard so the
    // relay bit lands on the next outbound frame regardless of whether
    // the state actually changed.
    // Source: deskhpsdr/src/old_protocol.c:2909-2910 [@120188f] — the
    // reference implementation writes the T/R relay bit every command
    // frame; we flush immediately on state change to match timing.
    m_forceBank10Next = true;

    if (m_trxRelay == enabled) {
        return;  // idempotent — flush flag already set above
    }
    m_trxRelay = enabled;
}

// ---------------------------------------------------------------------------
// setPaDisabled: "Disable HF PA" (Setup > Transmit > Power). Thetis's
// DisablePA sets tx[0].pa, which WriteMainLoop sends as bank 10 C3 bit 7
// (on the HL2 also bank 10 C2 bit 3, through EnableApolloTuner(!bit)).
//   From Thetis ChannelMaster/netInterface.c:623-631 [v2.10.3.15]
//     void DisablePA(int bit)
//     { if (prn->tx[0].pa != bit) { prn->tx[0].pa = bit; ... } }
//   From mi0bot ChannelMaster/netInterface.c:623-634 [@c26a8a4]
//     if (HPSDRModel == HPSDRModel_HERMESLITE)
//         EnableApolloTuner(!bit);	// MI0BOT: This call used on HL2 to enable/disable PA
// Bank 10 goes out on the next frame, as setTrxRelay flushes it.
// ---------------------------------------------------------------------------
void P1RadioConnection::setPaDisabled(bool disabled)
{
    m_forceBank10Next = true;
    RadioConnection::setPaDisabled(disabled);
}

// ---------------------------------------------------------------------------
// setAlexHpfEdges: the Alex tab's receive filter rows. Thetis's per-row
// bypass setters re-select the high-pass at once:
//   From Thetis console.cs:18823-18833 [v2.10.3.15]
//     public bool Alex1_5BPHPFBypass
//     { ... set { alex1_5bphpf_bypass = value; double freq = VFOAFreq; setAlex1HPF(freq); } }
// and an edge is read by the next selection; here both re-select from the
// frequency RX1 is on now. Bank 10 carries the word in its rotation.
// ---------------------------------------------------------------------------
void P1RadioConnection::setAlexHpfEdges(const codec::alex::AlexHpfEdges& edges)
{
    if (edges == m_alexHpfEdges) {
        return;
    }
    RadioConnection::setAlexHpfEdges(edges);
    recomputeReceiveFilters(-1);
    m_forceBank10Next = true;
}

// ---------------------------------------------------------------------------
// setAlexLpfEdges: the Alex-1 low-pass rows. Thetis's udAlex*LPF spinner
// handlers only keep the rows contiguous; none re-selects the low-pass
// (setup.cs:15888-15994 [v2.10.3.15]), so the rows are read by the next
// selection (a retune, a key or an unkey), keyed or not.
// ---------------------------------------------------------------------------
void P1RadioConnection::setAlexLpfEdges(const codec::alex::AlexLpfEdges& edges)
{
    RadioConnection::setAlexLpfEdges(edges);
}

// ---------------------------------------------------------------------------
// setAlexLpfBypass: 6m/ByPass on RX re-selects at once.
//   From Thetis console.cs:18775-18790 [v2.10.3.15]
//     lpf_bypass = value;
//     if (chkPower.Checked)
//     { double freq = VFOAFreq; if (_mox) freq = tx_dds_freq_mhz;
//       setAlexLPF(freq, _mox); ... txtVFOAFreq_LostFocus(...) }
// The LostFocus re-runs the receive selection, so unkeyed this is the
// receive selection over the live receivers.
// ---------------------------------------------------------------------------
void P1RadioConnection::setAlexLpfBypass(bool on)
{
    if (on == m_alexLpfBypass) {
        return;
    }
    RadioConnection::setAlexLpfBypass(on);
    if (m_mox) {
        if (m_txFreqHz != 0) {
            applyAlexLpf(double(m_txFreqHz) / 1e6, /*freqIsTx=*/true);
        }
    } else {
        const quint64 rx1Hz = m_rxFreqHz[m_rx1Slot];
        if (rx1Hz != 0) {
            applyReceiveAlexLpf(rx1Hz, rx1Hz);
        }
    }
    m_forceBank10Next = true;
}

// ---------------------------------------------------------------------------
// setMicBoost (3M-1b G.1)
//
// Sets the hardware mic-jack 20 dB boost preamp bit.
// Wire bit: bank 10 (C0=0x12) C2 byte bit 0 (mask 0x01).
// Polarity: 1 = boost on (no inversion).
//
// Porting from Thetis ChannelMaster/networkproto1.c:581 [v2.10.3.13]
//   C2 = ((prn->mic.mic_boost & 1) | ((prn->mic.line_in & 1) << 1) | ...)
//   mic_boost occupies the lowest bit of C2.
//
// Flush pattern mirrors setTrxRelay (Codex P2): m_forceBank10Next is set
// before the idempotent guard so the bit lands on the wire within ≤1 frame.
// ---------------------------------------------------------------------------
void P1RadioConnection::setMicBoost(bool on)
{
    // Codex P2: set flush flag BEFORE idempotent guard.
    m_forceBank10Next = true;

    if (m_micBoost == on) {
        return;  // idempotent — flush flag already set above
    }
    m_micBoost = on;
}

// ---------------------------------------------------------------------------
// setLineIn (3M-1b G.2)
//
// Sets the hardware mic-jack line-in path bit.
// Wire bit: bank 10 (C0=0x12) C2 byte bit 1 (mask 0x02).
// Polarity: 1 = line in active (no inversion).
//
// Porting from Thetis ChannelMaster/networkproto1.c:581 [v2.10.3.13]
//   C2 = ((prn->mic.mic_boost & 1) | ((prn->mic.line_in & 1) << 1) | ...)
//   line_in occupies bit 1 of C2 (mic_boost is bit 0).
//
// Flush pattern mirrors setMicBoost (Codex P2): m_forceBank10Next is set
// before the idempotent guard so the bit lands on the wire within ≤1 frame.
// ---------------------------------------------------------------------------
void P1RadioConnection::setLineIn(bool on)
{
    // Codex P2: set flush flag BEFORE idempotent guard.
    m_forceBank10Next = true;

    if (m_lineIn == on) {
        return;  // idempotent — flush flag already set above
    }
    m_lineIn = on;
}

// ---------------------------------------------------------------------------
// setMicTipRing (3M-1b G.3)
//
// Selects mic-jack Tip/Ring polarity.
// NereusSDR parameter convention: tipHot = true → Tip carries the mic signal.
//
// POLARITY INVERSION AT THE WIRE LAYER:
// Thetis field mic_trs and deskhpsdr field mic_ptt_tip_bias_ring both mean
// "1 = Tip is BIAS/PTT" (i.e. NOT the mic).  So:
//   tipHot = true  → Tip is mic    → wire bit CLEAR (0)
//   tipHot = false → Tip is BIAS   → wire bit SET   (1)
// The implementation writes (!m_micTipRing) to bit 4 of bank-11 C1.
//
// Wire bit: bank 11 (C0=0x14) C1 byte bit 4 (mask 0x10), INVERTED.
//
// Porting from Thetis ChannelMaster/networkproto1.c:597 [v2.10.3.13]
//   C1 = ... | ((prn->mic.mic_trs & 1) << 4) | ...
//   mic_trs: 1 = tip is BIAS/PTT (ring is mic), 0 = tip is mic (normal).
//
// First touch of case 11 / bank 11: adds m_forceBank11Next flush flag +
// round-robin chooser extension + captureBank11ForTest/forceBank11NextForTest
// test seams.  Bits 0-3 of C1 carry per-ADC preamp flags (Thetis quirk:
// bit 3 = rx0 again) and are untouched by this setter (OR into C1, never AND).
//
// Flush pattern mirrors setLineIn (Codex P2): m_forceBank11Next is set
// before the idempotent guard so the bit lands on the wire within ≤1 frame.
// ---------------------------------------------------------------------------
void P1RadioConnection::setMicTipRing(bool tipHot)
{
    // Codex P2: set flush flag BEFORE idempotent guard.
    m_forceBank11Next = true;

    if (m_micTipRing == tipHot) {
        return;  // idempotent — flush flag already set above
    }
    m_micTipRing = tipHot;
}

// ---------------------------------------------------------------------------
// setMicBias (3M-1b G.4)
//
// Enables or disables hardware mic-jack phantom power (bias voltage).
// Polarity: on=true → bias enabled → wire bit SET (no inversion).
//
// Wire bit: bank 11 (C0=0x14) C1 byte bit 5 (mask 0x20).
// This is the SAME C1 byte as G.3 (mic_trs bit 4) — both are OR'd in.
//
// Porting from Thetis ChannelMaster/networkproto1.c:597 [v2.10.3.13]
//   C1 = ... | ((prn->mic.mic_bias & 1) << 5) | ...
//   mic_bias: 1 = bias on, 0 = bias off (no polarity inversion).
//
// Flush pattern mirrors setMicTipRing (Codex P2): m_forceBank11Next is set
// before the idempotent guard so the bit lands on the wire within ≤1 frame.
// Reuses m_forceBank11Next + captureBank11ForTest infrastructure added in G.3.
// ---------------------------------------------------------------------------
void P1RadioConnection::setMicBias(bool on)
{
    // Codex P2: set flush flag BEFORE idempotent guard.
    m_forceBank11Next = true;

    if (m_micBias == on) {
        return;  // idempotent — flush flag already set above
    }
    m_micBias = on;
}

// ---------------------------------------------------------------------------
// setLineInGain (Task 2.1 of P1 full-parity epic)
//
// Updates m_lineInGain (shared base member) and arms m_forceBank11Next so
// the new line-in-gain value lands on the wire within ≤1 frame of the call.
// 5-bit field (0..31) — clamped at the API boundary.  Higher values produce
// greater line-in attenuation per Thetis upstream semantic.
//
// Wire bytes: bank 11 (C0=0x14) C2 low 5 bits.  C2 also carries
// puresignal_run at bit 6 (Task 2.5 plumbing); both bits OR into the same
// codec C2 byte and never collide.
//
// Porting from Thetis ChannelMaster/networkproto1.c:600 [v2.10.3.13]
//   C2 = (prn->mic.line_in_gain & 0b00011111) | ((prn->puresignal_run & 1) << 6);
//
// Flush pattern mirrors setMicBias (Codex P2): m_forceBank11Next is set
// BEFORE the idempotent guard so the bit lands on the wire within ≤1 frame.
// Reuses m_forceBank11Next + captureBank11ForTest infrastructure from G.3.
// ---------------------------------------------------------------------------
void P1RadioConnection::setLineInGain(int gain)
{
    const int clamped = qBound(0, gain, 31);

    // Codex P2: set flush flag BEFORE idempotent guard.
    m_forceBank11Next = true;

    if (m_lineInGain == clamped) {
        return;  // idempotent — flush flag already set above
    }
    m_lineInGain = clamped;
}

// ---------------------------------------------------------------------------
// setUserDigOut (Task 2.2 of P1 full-parity epic)
//
// Updates m_userDigOut (shared base member) and arms m_forceBank11Next so
// the new user-dig-out value lands on the wire within ≤1 frame of the call.
// 4-bit field (0..15) — masked at the API boundary via & 0x0F so values above
// 15 silently wrap to the low nibble (the codec also masks via & 0x0F, so
// stored state matches the wire bytes 1:1 either way).
//
// Wire bytes: bank 11 (C0=0x14) C3 low 4 bits — the same bank as
// setLineInGain (C2) and setMicBias / setMicTipRing / setMicPTT (C1), all of
// which OR into different bytes of the same case-11 frame.
//
// Porting from Thetis ChannelMaster/networkproto1.c:601 [v2.10.3.13]
//   C3 = prn->user_dig_out & 0b00001111;
//
// Flush pattern mirrors setLineInGain / setMicBias (Codex P2): m_forceBank11Next
// is set BEFORE the idempotent guard so the bits land on the wire within
// ≤1 frame.  Reuses m_forceBank11Next + captureBank11ForTest infrastructure
// from G.3.
// ---------------------------------------------------------------------------
void P1RadioConnection::setUserDigOut(quint8 dig)
{
    const quint8 masked = dig & 0x0F;

    // Codex P2: set flush flag BEFORE idempotent guard.
    m_forceBank11Next = true;

    if (m_userDigOut == masked) {
        return;  // idempotent — flush flag already set above
    }
    m_userDigOut = masked;
}

// ---------------------------------------------------------------------------
// setPuresignalRun (Task 2.3 of P1 full-parity epic)
//
// Updates m_puresignalRun (shared base member) and arms m_forceBank11Next so
// the new puresignal-run flag lands on the wire within ≤1 frame of the call.
// Bool field — wire bit 6 of bank 11 C2.
//
// Wire bytes: bank 11 (C0=0x14) C2 bit 6 (mask 0x40) — the same C2 byte that
// also carries line_in_gain (low 5 bits, Task 2.1).  Both fields OR together
// into the same byte; this setter only flips bit 6, never touching the low
// 5 bits — see test §3 (cross-bit guard for line_in_gain).
//
// Porting from Thetis ChannelMaster/networkproto1.c:600 [v2.10.3.13]
//   C2 = (prn->mic.line_in_gain & 0b00011111) | ((prn->puresignal_run & 1) << 6);
//
// Semantic: this flag tracks whether PureSignal feedback DDC routing is
// currently *active* on the wire — distinct from BoardCapabilities.hasPureSignal
// (capability) and from TransmitModel::puresignalEnabled (user toggle).  The
// wiring from PureSignalApplet's "Enable" toggle to this setter is Task 2.5,
// not this task; until 3M-4 lights up the actual feedback DDC routing, the
// applet toggle is the proxy that drives this flag.
//
// Flush pattern mirrors setLineInGain / setUserDigOut (Codex P2):
// m_forceBank11Next is set BEFORE the idempotent guard so the bit lands on
// the wire within ≤1 frame.  Reuses m_forceBank11Next + captureBank11ForTest
// infrastructure from G.3.
// ---------------------------------------------------------------------------
void P1RadioConnection::setPuresignalRun(bool run)
{
    // v0.4.1-rc3 bench-diagnostic: log every setPuresignalRun call so the
    // bench can confirm whether the chain (PS-A click → cmd-state machine →
    // setPsEnabledWithFanOut → emit psEnabledChanged → queued cross-thread
    // call → setPuresignalRun) is firing.  rc2 bench data showed FB ADC
    // stuck at noise floor even with bank-16 fix in place — need to confirm
    // m_puresignalRun actually reaches `true` on the connection thread.
    qCInfo(lcConnection) << "P1: setPuresignalRun(" << run << ")"
                          << "previous=" << m_puresignalRun;

    // Codex P2: set flush flag BEFORE idempotent guard.
    m_forceBank11Next = true;

    if (m_puresignalRun == run) {
        return;  // idempotent — flush flag already set above
    }
    m_puresignalRun = run;

    // Phase 3M-4 Task 17 P1 follow-up: also flush bank 2 + bank 3 indirectly
    // by arming bank-0 next; on the next round-robin cycle, banks 2/3 carry
    // the freq override (when m_psNDdc == 2 + m_mox + m_puresignalRun).
    // Bank 0 has the highest priority in the flush hierarchy, so it'll come
    // first; banks 2/3 land within ≤4 frames after.  Without the bank-0
    // flush, banks 2/3 already pick up the new state next cycle (380 fps),
    // but flushing makes the latency deterministic for tests.  Cheap.
    m_forceBank0Next = true;
}

// ---------------------------------------------------------------------------
// applyPsDdcConfig (Phase 3M-4 Task 17 P1 follow-up)
//
// P1 mirror of P2RadioConnection::applyPsDdcConfig.  Receives the wire-byte
// map computed by the per-board P1 codec (P1CodecHl2 / P1CodecStandard's
// applyPureSignalDdcConfig) and applies it to the connection-thread state
// that downstream bank composers read from buildCodecContext().
//
// Source map (mirrors Thetis console.cs:8527-8534 UpdateDDCs() [v2.10.3.13]):
//   NetworkIO.EnableRxs(ddcEnable)             → P1: implicit in m_activeRxCount
//                                                    + bank-0 nddc encoding
//   NetworkIO.EnableRxSync(0, syncEnable)      → P1: no per-stream sync
//                                                    on USB protocol — DDC1
//                                                    samples are interleaved
//                                                    in the EP6 frame slot
//                                                    layout when activeRxCount>=2
//   for (i<4) NetworkIO.SetDDCRate(i, rate[i]) → P1: single global m_sampleRate
//                                                    (P1 sends one rate via
//                                                    bank-0 C1 SampleRateIn2Bits)
//   NetworkIO.SetADC_cntrl1(cntrl1)            → m_adcCtrl low byte
//                                                    (bank-4 C1)
//   NetworkIO.SetADC_cntrl2(cntrl2)            → m_adcCtrl high bits
//                                                    (bank-4 C2 [5:0])
//   NetworkIO.Protocol1DDCConfig(p1DdcConfig,  → m_psNDdc (PS gate)
//     P1_diversity, p1RxCount, nDdc)             m_activeRxCount (EP6 layout)
//
// For HL2 in PS-MOX, the codec returns:
//   ddcEnable=DDC0, syncEnable=DDC1, rate[0]=rate[1]=rx1Rate,
//   cntrl1=4, cntrl2=0, nDdc=4, p1RxCount=4
// (mi0bot console.cs:8469-8488 [v2.10.3.13-beta2]).
//
// applyPsDdcConfig writes those into m_adcCtrl=4 (bank 4 will emit C1=0x04,
// C2=0x00) so the radio routes DDC1's input to the PS feedback path on
// the next round-robin cycle.  m_psNDdc=4 disables the bank-2/3 freq
// override (correct: HL2 firmware handles freq routing internally).
// ---------------------------------------------------------------------------
void P1RadioConnection::applyPsDdcConfig(const NereusSDR::PsDdcConfig& cfg)
{
    bool changed = false;

    // From mi0bot console.cs:8531-8532 [v2.10.3.13-beta2]:
    //   NetworkIO.SetADC_cntrl1(cntrl1);
    //   NetworkIO.SetADC_cntrl2(cntrl2);
    // P1 packs both into bank 4 C1/C2 — m_adcCtrl is the 14-bit
    // composite read by composeCcForBank case 4
    // (P1RadioConnection.cpp:2647-2651 + codec equivalents):
    //   bank 4 C1 = adcCtrl & 0xFF      (cntrl1)
    //   bank 4 C2 = (adcCtrl >> 8) & 0x3F (cntrl2 low 6 bits)
    const quint16 newAdcCtrl = static_cast<quint16>(
        (static_cast<quint16>(cfg.cntrl1) & 0x00FF) |
        ((static_cast<quint16>(cfg.cntrl2) & 0x003F) << 8));
    if (m_adcCtrl != newAdcCtrl) {
        m_adcCtrl = newAdcCtrl;
        changed = true;
    }

    // From mi0bot console.cs:8534 [v2.10.3.13-beta2]:
    //   NetworkIO.Protocol1DDCConfig(p1DdcConfig, P1_diversity, p1RxCount, nDdc);
    // We capture p1RxCount → m_activeRxCount so EP6 frame parsing
    // (parseEp6Frame's slotBytes = 6*numRx + 2) matches what the radio
    // actually sends during PS-MOX, and nDdc → m_psNDdc for the bank-2/3
    // freq override gate.
    if (cfg.p1RxCount > 0 && m_codecRxCount != cfg.p1RxCount) {
        // The DDC-configuration axis only. announceRxCount combines it with
        // the panadapter axis and restarts the stream if the announcement
        // moves, because the count is the ep6 slot layout (parseEp6Frame's
        // slotBytes = 6 * numRx + 2) and both sides have to change together:
        // a frame composed under the old layout and parsed under the new one
        // is silently misparsed, since the 7F 7F 7F sync check does not
        // encode the layout.
        //
        // Writing m_activeRxCount directly here is what let a panadapter
        // removal take the announcement below what PureSignal needed. See
        // P1RadioConnection.h::announceRxCount.
        m_codecRxCount = cfg.p1RxCount;
        announceRxCount();
        changed = true;
    }
    if (cfg.nDdc > 0 && m_psNDdc != cfg.nDdc) {
        m_psNDdc = cfg.nDdc;
        changed = true;
    }

    // Phase 3M-4 bench-fix 2026-05-23 (J.J. Boyd KG4VCF): latch the PS DDC
    // pair so parseEp6Frame can emit the source-first paired signal.
    // PsDdcConfig stores -1 when no PS pair applies (RX-only steady state);
    // a non-negative pair means PS-MOX is engaged on this connection.
    //
    // That was the intent from the start but not the behaviour until
    // 2026-08-01: PsDdcConfig defaulted the pair to (0, 1) and only the
    // PS-MOX branches assign it, so an RX-only config latched a valid-looking
    // pair here and left parseEp6Frame's gate open for the whole session.
    // The struct now defaults to the sentinel (CodecContext.h), so a config
    // without a PS pair clears the latch instead of arming it.
    if (cfg.psFbDdc != m_psFbDdc) {
        m_psFbDdc = cfg.psFbDdc;
        changed = true;
    }
    if (cfg.txMonDdc != m_psTxMonDdc) {
        m_psTxMonDdc = cfg.txMonDdc;
        changed = true;
    }

    if (changed) {
        // Flush bank 0 (carries nddc encoding) + bank 4 (ADC routing).
        // Bank 0 has highest priority; bank 4 fires next round-robin.
        m_forceBank0Next = true;
        m_forceBank4Next = true;

        qCInfo(lcConnection).nospace()
            << "P1: applyPsDdcConfig nDdc=" << cfg.nDdc
            << " activeRx=" << m_activeRxCount
            << " adcCtrl=0x" << QString::number(m_adcCtrl, 16)
            << " (cntrl1=" << cfg.cntrl1 << " cntrl2=" << cfg.cntrl2 << ")"
            << " p1DdcConfig=" << cfg.p1DdcConfig
            << " psFbDdc=" << m_psFbDdc
            << " psTxMonDdc=" << m_psTxMonDdc;
    }
}

// ---------------------------------------------------------------------------
// setDiversity (diversity lane B4)
//
// The en_diversity argument of Protocol1DDCConfig. Thetis sets it from the
// diversity state on every UpdateDDCs and the frame writer turns it into the
// VFO lock:
//   From Thetis console.cs:8215-8216 [v2.10.3.15]
//     bool diversity_enabled = Diversity2;
//     if (diversity_enabled) P1_diversity = 1;
//   From Thetis console.cs:8544 [v2.10.3.15]
//     NetworkIO.Protocol1DDCConfig(P1_DDCConfig, P1_diversity, P1_rxcount, nddc);
//   From Thetis ChannelMaster/netInterface.c:1249-1252 [v2.10.3.15]
//     void Protocol1DDCConfig(int ddcconfig, int en_diversity, int rxcount, int inddc)
//     ...
//     P1_en_diversity = en_diversity;
//   From Thetis ChannelMaster/networkproto1.c:471 [v2.10.3.15]
//     C4 |= (P1_en_diversity) << 7;		// if diversity, locks VFOs
// Bank 0 goes out on the next frame so the lock lands with the assignment
// that moved the pair, as setMox does for the MOX bit.
// ---------------------------------------------------------------------------
// NereusSDR-original remap publication. No receiver count/rate change.
void P1RadioConnection::applyDiversityAssignment(const DdcAssignment& assignment)
{
    const bool active = assignment.p1Diversity != 0;
    const std::array<int, 5> streamMap{{assignment.streamDdc[0], assignment.streamDdc[1],
        assignment.streamDdc[2], assignment.streamDdc[3], assignment.streamDdc[4]}};
    const bool remap = streamMap != m_diversityStreamMap;
    if (active || m_diversity) {
        setP1AdcCntrl(assignment.adcCtrl1 | (assignment.adcCtrl2 << 8));
        m_forceBank4Next = true;
        if (remap) {
            // P1 frequency banks for fixed frame slots 0..4; they must all
            // follow a remap before the normal round-robin reaches them.
            m_diversityFrequencyBanks |= (1 << 2) | (1 << 3) | (1 << 5) | (1 << 6) | (1 << 7);
        }
    }
    m_diversityStreamMap = streamMap;
    setDiversity(active);
}

void P1RadioConnection::setDiversity(bool on)
{
    if (m_diversity == on) {
        return;
    }
    m_diversity = on;
    m_forceBank0Next = true;
    qCInfo(lcConnection) << "P1: diversity VFO lock" << (on ? "on" : "off");
}

// ---------------------------------------------------------------------------
// setMicPTTDisabled (issue #182 — renamed from setMicPTT for parameter
// parity with Thetis MicPTTDisabled / mic_ptt_disabled storage name).
//
// Wire convention matches Thetis byte-for-byte:
//   disabled=true  → wire bit 6 SET   (firmware ignores mic-jack PTT line)
//   disabled=false → wire bit 6 CLEAR (firmware honors mic-jack PTT line)
//
// Wire bit: bank 11 (C0=0x14) C1 byte bit 6 (mask 0x40), direct polarity.
// This is the SAME C1 byte as G.3 (bit 4) + G.4 (bit 5) — all OR'd in.
//
// From Thetis console.cs:19757-19766 [v2.10.3.13+501e3f51]:
// Upstream tags preserved: //MW0LGE (from cited console.cs:19758) [v2.10.3.15]
//   private bool mic_ptt_disabled = false;        // default PTT enabled
//   public bool MicPTTDisabled {
//       set {
//           mic_ptt_disabled = value;
//           NetworkIO.SetMicPTT(Convert.ToInt32(value));
//       }
//   }
// From Thetis ChannelMaster/networkproto1.c:597-598 [v2.10.3.13+501e3f51]:
//   C1 = ... | ((prn->mic.mic_ptt & 1) << 6);
//
// Cross-reference notes:
//   deskhpsdr/src/old_protocol.c:3000-3002 [@120188f]: deskhpsdr's
//     `mic_ptt_enabled` is a higher-level wrapper that inverts before
//     writing the wire field; the wire field itself (mic_ptt) is direct.
//   deskhpsdr/src/new_protocol.c:1488-1490 [@120188f] — P2 byte 50 bit 2
//     (P2 mic_ptt path, separate codec, same direct-polarity convention).
//
// Flush pattern mirrors setMicBias (Codex P2): m_forceBank11Next is set
// BEFORE the idempotent guard so the bit lands on the wire within ≤1 frame.
// Reuses m_forceBank11Next + captureBank11ForTest infrastructure from G.3.
// ---------------------------------------------------------------------------
void P1RadioConnection::setMicPTTDisabled(bool disabled)
{
    // Codex P2: set flush flag BEFORE idempotent guard.
    m_forceBank11Next = true;

    if (m_micPTTDisabled == disabled) {
        return;  // idempotent — flush flag already set above
    }
    m_micPTTDisabled = disabled;
}

// ---------------------------------------------------------------------------
// setMicXlr (3M-1b G.6)
//
// Saturn G2 P2-only feature; P1 hardware has no XLR jack.
// Setter stores the flag for cross-board API consistency but does NOT
// emit any wire bytes. P1 case-10 and case-11 C&C bytes are UNCHANGED
// regardless of m_micXlr value.
//
// P2 source: deskhpsdr/src/new_protocol.c:1500-1502 [@120188f]:
//   if (mic_input_xlr) { transmit_specific_buffer[50] |= 0x20; }
//   (byte 50 bit 5 = 0x20, polarity 1=XLR jack — no inversion)
//   P2 implementation in P2RadioConnection::setMicXlr().
// ---------------------------------------------------------------------------
void P1RadioConnection::setMicXlr(bool xlrJack)
{
    // Saturn G2 P2-only feature; P1 hardware has no XLR jack.
    // Setter stores the flag for cross-board consistency but does NOT
    // emit any wire bytes. P1 case-10 and case-11 C&C bytes are
    // unchanged regardless of m_micXlr value.
    if (m_micXlr == xlrJack) {
        return;
    }
    m_micXlr = xlrJack;
}

// ---------------------------------------------------------------------------
// applyBoardQuirks
//
// Reads BoardCapabilities (m_caps) and enforces runtime constraints.
// Must be called after m_caps is set in connectToRadio() and from
// setBoardForTest() in unit tests.
//
// Source: specHPSDR.cs per-HPSDRHW branches + BoardCapabilities registry.
// Thetis clamps the step-attenuator value in SetupForm per board type and
// enforces the limits again in NetworkIO.cs before sending C&C frames.
//
// (HL2 IoBoardHl2 TLV init + bandwidth monitor come in Task 12)
// ---------------------------------------------------------------------------
void P1RadioConnection::applyBoardQuirks()
{
    if (!m_caps) { return; }

    // Clamp attenuator to board range.
    // Source: specHPSDR.cs — per-HPSDRHW min/max dB ranges enforced at setup.
    if (m_caps->attenuator.present) {
        const int wireMax = BoardCapsTable::stepAttWireMaxDb(*m_caps);
        if (m_stepAttn[0] > wireMax) { m_stepAttn[0] = wireMax; }
        if (m_stepAttn[0] < m_caps->attenuator.minDb) { m_stepAttn[0] = m_caps->attenuator.minDb; }
    } else {
        m_stepAttn[0] = 0;
    }

    // Board-aware default for the P1 per-DDC ADC routing word (Thetis
    // `P1_adc_cntrl`). The default mirrors what each board sees in
    // Thetis fresh-install with no Setup-form modifications, except for
    // the HermesII bench-override case noted below.
    //
    //   2-ADC boards (Angelia / Orion / OrionMkII / Saturn / G2-class):
    //     default 0x0004 — matches Thetis fresh-install
    //     (console.cs:15120 [v2.10.3.13] `rx_adc_ctrl_P1 = 4`).
    //     Bit 2 set → DDC1's ADC select = 01 (ADC1, the PA-feedback tap),
    //     which is what users running PureSignal on 2-ADC P1 boards
    //     expect out of the box.
    //
    //   HermesLite (HL2, 1-ADC):
    //     default 0x0004 — matches Thetis fresh-install and the wire
    //     bytes observed on working HL2 setups. HL2 firmware
    //     accepts the cntrl1=4 ADC-steering bits during PS-MOX.
    //
    //   Hermes / HermesII (1-ADC ANAN-10 / 10E / 100 / 100B):
    //     default 0x0000 — matches wire bytes observed on a working
    //     Thetis-driven ANAN-10E (HermesII) during PS-MOX on 2026-05-09.
    //     On 1-ADC Hermes-family hardware the per-DDC ADC selector is
    //     hardware-meaningless except that bit 2 set has been observed
    //     to confuse HermesII firmware's PA-loopback path in PS-MOX
    //     (calcc produces a wrong-shape correction map, PS predistortion
    //     has no effect). 0 is the empirically-safe default. Users who
    //     prefer the Thetis-faithful default of 4 can override per-MAC.
    //
    // Either default can be overridden per-MAC via the AppSettings key
    // `hardware/<mac>/p1AdcCntrl` (loaded by RadioModel after this
    // applyBoardQuirks() runs at connect time).
    if (m_caps->adcCount >= 2) {
        m_p1AdcCntrl = 0x0004;
    } else if (m_caps->board == HPSDRHW::HermesLite
               || m_caps->board == HPSDRHW::HermesLiteRxOnly) {
        // Plan Task 15: the receive-only kit is the same HL2 silicon.
        m_p1AdcCntrl = 0x0004;
    } else {
        // Hermes / HermesII (and any future 1-ADC non-HL2 board)
        m_p1AdcCntrl = 0x0000;
    }

    selectCodec();
}

// ---------------------------------------------------------------------------
// setP1AdcCntrl — Thetis SetADC_cntrl_P1 (netInterface.c:992-996 [v2.10.3.13])
//
// Stores the 14-bit per-DDC ADC routing word in m_p1AdcCntrl; the next
// bank-4 emit (round-robin) picks it up via buildCodecContext() →
// ctx.p1AdcCntrl → P1CodecStandard / P1CodecHl2 bank-4 C1/C2 bytes.
// Mirrors `NetworkIO.SetADC_cntrl_P1(rx_adc_ctrl_P1)` at
// console.cs:7327 [v2.10.3.13].
// ---------------------------------------------------------------------------
void P1RadioConnection::setP1AdcCntrl(int bits)
{
    const quint16 clamped = static_cast<quint16>(bits & 0x3FFF);  // 14 bits
    if (m_p1AdcCntrl != clamped) {
        m_p1AdcCntrl = clamped;
        qCInfo(lcConnection).nospace()
            << "P1: setP1AdcCntrl(0x" << QString::number(clamped, 16) << ")";
    }
}

// ---------------------------------------------------------------------------
// selectCodec
//
// Builds m_codec from m_hardwareProfile.model. Called from applyBoardQuirks().
// Phase 3P-A Task 12.
// ---------------------------------------------------------------------------
void P1RadioConnection::selectCodec()
{
    m_codec.reset();
    m_useLegacyCodec = (qEnvironmentVariableIntValue("NEREUS_USE_LEGACY_P1_CODEC") == 1);
    if (m_useLegacyCodec) {
        qCInfo(lcConnection) << "P1: NEREUS_USE_LEGACY_P1_CODEC=1 — using pre-refactor compose path";
        return;
    }
    if (!m_caps) {
        qCWarning(lcConnection) << "P1: no caps; codec selection deferred";
        return;
    }
    // Note: the per-board codec is keyed off the **logical** HPSDRModel
    // (which distinguishes HermesLite from Hermes-family even though they
    // share the same physical HPSDRHW::HermesLite wire dialect), not the
    // physical HPSDRHW. Use m_hardwareProfile.model for codec selection.
    // RedPitaya and AnvelinaPro3 are HPSDRModel values that map to physical
    // OrionMKII at the wire layer but need different bank-12/bank-17
    // encoding — that's why they get their own codec subclasses.
    using HPM = HPSDRModel;
    switch (m_hardwareProfile.model) {
        case HPM::HERMESLITE:   m_codec = std::make_unique<P1CodecHl2>();          break;
        case HPM::ANVELINAPRO3: m_codec = std::make_unique<P1CodecAnvelinaPro3>(); break;
        case HPM::REDPITAYA:    m_codec = std::make_unique<P1CodecRedPitaya>();    break;
        default:                m_codec = std::make_unique<P1CodecStandard>();     break;
    }
    // RadioModel calls setIoBoard() BEFORE the connection thread starts (and
    // therefore before applyBoardQuirks() runs selectCodec()), so the cached
    // pointer is already in m_ioBoard but did not land on the freshly-built
    // codec.  Push it now so HL2 I2C compose works on the very first frame.
    if (m_ioBoard) {
        if (auto* hl2Codec = dynamic_cast<P1CodecHl2*>(m_codec.get())) {
            hl2Codec->setIoBoard(m_ioBoard);
        }
    }
    qCInfo(lcConnection) << "P1: selected codec for HPSDRModel" << int(m_hardwareProfile.model);

    // Phase 3M-4 Task 17 P1 follow-up: notify RadioModel that m_codec is now
    // valid so ReceiverManager::setP1Codec can be called.  Without this,
    // ReceiverManager::m_p1Codec stays null, updateDdcAssignment() never
    // calls applyPureSignalDdcConfig, ddcConfigChanged never fires, and the
    // P1 PS DDC routing stays stuck at the RX-only steady state.  Mirror of
    // P2RadioConnection's p2CodecChanged() emit pattern.
    emit p1CodecChanged();
}

// ---------------------------------------------------------------------------
// setOcMatrix — Phase 3P-D Task 3
//
// Wires the RadioModel's OcMatrix to this connection so buildCodecContext()
// can source ctx.ocByte from maskFor(currentBand, mox) at C&C compose time.
// Called by RadioModel::connectToRadio() on the main thread before the
// connection thread is started, so no synchronisation is needed.
// ---------------------------------------------------------------------------
void P1RadioConnection::setOcMatrix(const OcMatrix* matrix)
{
    m_ocMatrix = matrix;
}

void P1RadioConnection::setCalibrationController(const CalibrationController* cal)
{
    m_calController = cal;
}

// R-R3-49 (found bug): Protocol 1 sent the tuned frequencies uncorrected;
// Thetis corrects every VFO it sends, as Hz on Protocol 1:
// From Thetis HPSDR/NetworkIO.cs:215-224 [v2.10.3.15] VFOfreq
//   f_freq = (int)((f * 1e6) * _freq_correction_factor);
//   if (f_freq >= 0)
//       if (CurrentRadioProtocol == RadioProtocol.USB)
//           SetVFOfreq(id, f_freq, tx);                  // sending freq Hz to firmware
// NereusSDR tunes in whole Hz, so f * 1e6 is the Hz value itself. The frame
// is composed every time it is sent, so a factor change reaches the radio
// with the next frame, as FreqCorrectionChanged re-sends the VFOs
// (NetworkIO.cs:237-248). A negative result (Thetis sends nothing) cannot
// arise from the 0..2 factor range; it is held at 0.
quint64 P1RadioConnection::wireFrequencyHz(quint64 tunedHz) const
{
    const double factor = m_calController
                          ? m_calController->effectiveFreqCorrectionFactor() : 1.0;
    if (factor == 1.0) {
        return tunedHz;
    }
    return static_cast<quint64>(std::max<qint64>(
        0, static_cast<qint64>(static_cast<double>(tunedHz) * factor)));
}

// ---------------------------------------------------------------------------
// setIoBoard — Phase 3P-E Task 2
//
// Wires the RadioModel's IoBoardHl2 to this connection for I2C intercept
// (outbound) and ep6 response parsing (inbound).  On HL2 boards, pushes
// the pointer into P1CodecHl2 so tryComposeI2cFrame() can dequeue txns.
// On non-HL2 boards, m_codec is not a P1CodecHl2, so the dynamic_cast
// returns null and the codec push is a noop — only m_ioBoard is stored
// (which is itself only used if the codec pushes a frame).
// Called by RadioModel::connectToRadio() after selectCodec() returns.
// ---------------------------------------------------------------------------
void P1RadioConnection::setIoBoard(IoBoardHl2* io)
{
    m_ioBoard = io;
    if (auto* hl2Codec = dynamic_cast<P1CodecHl2*>(m_codec.get())) {
        hl2Codec->setIoBoard(io);
    }
    // Non-HL2 boards: codec cast returns null — noop (no I2C support).
}

// ---------------------------------------------------------------------------
// setBandwidthMonitor — Phase 3P-E Task 3
//
// Wires the RadioModel-owned HermesLiteBandwidthMonitor into the connection.
// Called by RadioModel::connectToRadio() when m_caps->hasBandwidthMonitor.
// The pointer is non-owning — lifetime is managed by RadioModel.
// ---------------------------------------------------------------------------
void P1RadioConnection::setBandwidthMonitor(HermesLiteBandwidthMonitor* monitor)
{
    m_bwMonitor = monitor;
}

// ---------------------------------------------------------------------------
// setTxMicSource — Phase 3M-1c TX pump v3
//
// Wires the RadioModel-owned TxMicSource into the connection.  Called by
// RadioModel::connectToRadio() unconditionally (the source itself handles
// HL2 mic16-zero quirks via the existing PC mic-source-locked mechanism).
// The pointer is non-owning — lifetime is managed by RadioModel.
// ---------------------------------------------------------------------------
void P1RadioConnection::setTxMicSource(TxMicSource* src)
{
    // Caller contract: invoked on this connection's affinity thread.
    // Both callers now satisfy that by construction rather than by
    // ordering: RadioModel::connectToRadio marshals the attach through
    // QMetaObject::invokeMethod and RadioModel::teardownConnection
    // marshals the detach.  The assignment and the m_lastMicAt arming
    // below are therefore race-free with the connection-thread reads in
    // onWatchdogTick / parseEp6Frame mic16 extraction whether or not the
    // connection has already been moved to its worker thread.
    //
    // This used to rest on ordering alone (the attach ran before the
    // moveToThread in connectToRadio).  That held on the hot path only.
    // The issue #153 sub-bug 1 cold-start retry re-runs the same attach
    // from a WdspEngine::initializedChanged handler on the main thread,
    // long after the move, which is what made marshalling necessary.
    // A future caller that reaches this setter without marshalling will
    // need atomic / mutex protection.
    m_txMicSource = src;

    // Stage-2 review fix I3: arm the LOS timer at attach time so the
    // 3 s zero-block injection (onWatchdogTick) fires even if the
    // radio never delivers a mic frame.  Without this, m_lastMicAt
    // stays default-constructed (invalid) and the mic-LOS guard at
    // P1RadioConnection.cpp:1628 short-circuits forever — the worker
    // would block on waitForBlock(INFINITE) with no recovery.
    //
    // Mirrors Thetis network.c:655-666 [v2.10.3.13] — WSA_WAIT_TIMEOUT
    // injects zero buffer via Inbound regardless of whether real
    // samples have been observed.  Setting m_lastMicAt = now here is
    // equivalent to Thetis's implicit "the timer started counting the
    // moment we attached", because the watchdog tick does
    // sinceMicMs = now - m_lastMicAt > kMicLosTimeoutMs.
    m_lastMicAt = QDateTime::currentDateTimeUtc();
}


// ---------------------------------------------------------------------------
// parseI2cResponse — Phase 3P-E Task 2
//
// Called from the instance parseEp6Frame() when incoming C&C status byte C0
// has bit 7 set, indicating an I2C read response frame (not normal status).
// Routes C1-C4 read data back into the IoBoardHl2 register mirror.
//
// Source: mi0bot networkproto1.c:478-493 [@c26a8a4]
// ---------------------------------------------------------------------------
void P1RadioConnection::parseI2cResponse(quint8 c0, quint8 c1, quint8 c2,
                                          quint8 c3, quint8 c4)
{
    if (!m_ioBoard) { return; }  // no IoBoard wired (non-HL2 board)

    // Persist all 4 response bytes plus the 7-bit returned address into the
    // IoBoardHl2 model. Upstream stores these in prn->i2c.read_data[0..3]
    // and sets ctrl_read_available = 1; we mirror both via
    // IoBoardHl2::applyI2cReadResponse(), which also emits a signal so the
    // HL2 I/O diagnostics page can reflect live hardware state instead of
    // defaults.
    //
    // Full returnedAddress → Register slot dispatch (so reads auto-populate
    // m_registers[]) is Phase 3P-E Task 3 state-machine work; for now
    // downstream consumers read the response via IoBoardHl2::lastI2cRead().
    //
    // Source: mi0bot networkproto1.c:478-493 [@c26a8a4]
    m_ioBoard->applyI2cReadResponse(c0, c1, c2, c3, c4);
}

// ---------------------------------------------------------------------------
// buildCodecContext
//
// Snapshot all live state into a CodecContext for the codec call.
// Phase 3P-A Task 12.
// ---------------------------------------------------------------------------
CodecContext P1RadioConnection::buildCodecContext() const
{
    CodecContext ctx;
    ctx.mox            = m_mox;
    ctx.sampleRateCode = (m_sampleRate >= 384000) ? 3
                       : (m_sampleRate >= 192000) ? 2
                       : (m_sampleRate >=  96000) ? 1 : 0;
    ctx.activeRxCount  = m_activeRxCount;
    ctx.txDrive        = m_txDrive;
    ctx.paEnabled      = m_paEnabled;
    ctx.trxRelay       = m_trxRelay;
    ctx.txPaDisabled   = m_paDisabled;  // "Disable HF PA" (setPaDisabled)
    ctx.p1MicBoost     = m_micBoost;
    ctx.p1LineIn       = m_lineIn;
    ctx.p1MicTipRing   = m_micTipRing;
    ctx.p1MicBias      = m_micBias;     // 3M-1b G.4
    ctx.p1LineInGain   = m_lineInGain;  // Task 2.1 of P1 full-parity epic
    ctx.p1UserDigOut   = m_userDigOut;  // Task 2.2 of P1 full-parity epic
    ctx.p1PuresignalRun = m_puresignalRun;  // Task 2.3 of P1 full-parity epic
    ctx.p1PsNDdc       = m_psNDdc;          // Task 17 P1 follow-up: bank 2/3 PS gate
    ctx.model          = m_hardwareProfile.model;
    ctx.p1MicPTTDisabled = m_micPTTDisabled;  // 3M-1b G.5; renamed for issue #182
    ctx.duplex         = m_duplex;
    ctx.diversity      = m_diversity;
    ctx.antennaIdx     = m_antennaIdx;
    ctx.rxOnlyAnt      = m_rxOnlyAnt;
    ctx.rxOut          = m_rxOut;
    // Level Cal: the Alex attenuator (bank 0 C3 bits 0-1).
    ctx.alexAttenBits  = (m_alex20dB ? 0x2 : 0) | (m_alex10dB ? 0x1 : 0);

    // Source OC byte from OcMatrix per current band + MOX state.  Falls
    // through to legacy m_ocOutput when matrix is unset (e.g. tests that
    // construct P1RadioConnection without a RadioModel).  Default state is
    // byte-identical: empty matrix → maskFor()==0 == m_ocOutput==0.
    // Phase 3P-D Task 3 — From Thetis HPSDR/Penny.cs:117-132 [@501e3f5]
    // setBandABitMask — OC mask derived per-band at transmit time.
    //
    // Unkeyed the band is RX1's, as in Thetis (lo_band = the band of
    // VFOAFreq):
    //   From mi0bot-Thetis console.cs:14986-14988 [@c26a8a4]
    //     Band lo_band = BandByFreq(XVTRForm.TranslateFreq(VFOAFreq), rx1_xvtr_index, current_region);
    //     Band lo_bandb = BandByFreq(XVTRForm.TranslateFreq(VFOBFreq), rx2_xvtr_index, current_region);
    //     int bits = Penny.getPenny().ExtCtrlEnable(lo_band, lo_bandb, _mox, value, _tuning, SetupForm.TestIMD, chkExternalPA.Checked); // MW0LGE_21j
    // and RX1 is the stand-in slot, not slot 0 (Phase 3F section 16.3.2,
    // setLiveReceiverSlots). On the HL2 this byte drives the N2ADR filter
    // board, so reading slot 0 after slice A was closed left the board on
    // A's band.
    //
    // Keyed it is the TRANSMITTING slice's band (plan Task 14). See
    // ocBandFrequencyHz for the Penny.cs rule. Unkeyed on the HL2 it is the
    // band of the highest-frequency slice counted on the input (shared-input
    // filters, ruling (c), mi0bot's receive arm), also in ocBandFrequencyHz,
    // with the N2ADR broadcast-band high-pass (bit 6) cleared when a counted
    // slice's own receive pins lack it (JJ's ruling of 2026-09-30,
    // SharedInputLowPass::hl2ReceivePins, the one RadioModel names in the
    // low-pass reason).
    if (m_ocMatrix) {
        const Band currentBand = bandFromFrequency(static_cast<double>(ocBandFrequencyHz()));
        ctx.ocByte = m_ocMatrix->maskFor(currentBand, m_mox);
        if (!m_mox && m_hardwareProfile.model == HPSDRModel::HERMESLITE
            && m_countedSlotsAdc0 != 0) {
            const SharedInputLowPass::Hl2ReceivePins rx =
                SharedInputLowPass::hl2ReceivePins(*m_ocMatrix, countedCandidates());
            if (rx.best >= 0) {
                ctx.ocByte = rx.pins;
            }
        }
    } else {
        ctx.ocByte = m_ocOutput;
    }

    // The HL2 has no Alex board (hasAlexFilters=false, m_caps->hasAlex=
    // false): its RX preselector is the N2ADR board wired through these
    // same OC pins, driven by ctx.ocByte above rather than the Alex
    // bank-10 HPF word that effectiveAlexHpfBits() feeds. AlexController's
    // per-ADC BYPASS / WidebandLocked decision (see setAlexRxBpf above)
    // therefore has nowhere else to reach the wire on this board.
    //
    // Scoped to hasIoBoardHl2 -- true only for the two HL2 rows
    // (kHermesLite, kHermesLiteRxOnly) -- and not every P1 board:
    // Alex-equipped boards already carry this same decision on bank 10 via
    // effectiveAlexHpfBits() and must not also have it clobber their
    // independently-addressed OC bank.
    //
    // RX only (!m_mox). On TX the N2ADR pins double as the transmit
    // low-pass segment for the slice actually keying the radio
    // (N2adrPreset.cpp's chkPenOCxmit* entries): forcing 0x00 mid-transmit
    // because some OTHER slice's receive band conflicts with a third would
    // strip TX harmonic filtering for a decision that says nothing about
    // the transmitted signal. This scoping is a judgment call, not
    // something the bug report specified either way; flagged for
    // maintainer review in the PR report.
    //
    // 0x00 = "disable/bypass the N2ADR board": maintainer-supplied
    // hardware knowledge (J.J. Boyd / KG4VCF, 2026-08-01), not documented
    // in any upstream source -- mi0bot never commands 0x00.
    //
    // Which decisions reach here (JJ's ruling of 2026-09-30): ForceBypass
    // and WidebandLocked only. In Auto a difference between the counted
    // slices' pins never sends 0x00: RadioModel::republishAlexAdcSlices
    // hands the chain over filtered, and the pins above follow the highest
    // slice, mi0bot's way, with bit 6 cleared where a slice needs it off.
    static constexpr int kAlexBypassSentinel = 0x20;  // AlexRxBpf.hpfBitsAdc0 bypass encoding
    if (!m_mox && m_caps && m_caps->hasIoBoardHl2
        && m_alexRxHpfOverride == kAlexBypassSentinel) {
        ctx.ocByte = 0x00;
    }

    // TX wire-byte diagnostic. Added 2026-08-01 chasing a "TUNE and SSB key
    // but produce no RF" report on a live HL2, where the software TX
    // sequence logged clean end to end and gave nothing to work from.
    //
    // These four are what actually decide whether the radio emits: the drive
    // byte (bank 10 C1, zero drive means a silent carrier), the PTT bit
    // (bank 0 C0 bit 0), the transmit frequency, and the announced receiver
    // count that sizes the frame. Logged on MOX edges only, so an idle
    // receive session stays quiet.
    if (m_mox != m_lastMoxLogged) {
        m_lastMoxLogged = m_mox;
        // Read the MEMBERS, not ctx. This diagnostic sits above the block
        // that populates most of ctx (ctx.txFreqHz is assigned further down),
        // so reading ctx here reports default-initialised zeros rather than
        // live state. That cost several rebuilds chasing a transmit frequency
        // that was never actually zero. ctx.ocByte is the one exception,
        // assigned above this point, and is read from ctx deliberately
        // because the bypass override that rewrites it also lives above.
        qDebug("HL2 TX edge: mox=%d txDrive=%d (0x%02X) txFreq=%llu "
               "alexLpf=0x%02X activeRx=%d ocByte=0x%02X paEnabled=%d",
               int(m_mox), m_txDrive, quint8(m_txDrive & 0xFF),
               static_cast<unsigned long long>(m_txFreqHz),
               // effectiveAlexLpfBits(), not a raw member: PR #293 split the
               // single m_alexLpfBits this diagnostic was written against
               // into Rx/Tx halves, and the accessor is what bank 10 C4
               // actually carries. Logging either half directly would print
               // the wrong one on half the edges.
               effectiveAlexLpfBits(),
               m_activeRxCount, ctx.ocByte, int(m_paEnabled));
    }

    if (ctx.ocByte != m_lastOcByteLogged) {
        const int bandIdx = int(bandFromFrequency(static_cast<double>(ocBandFrequencyHz())));
        qDebug("HL2 ocByte=0x%02X band=%d mox=%d (matrix=%p)",
               ctx.ocByte, bandIdx, int(m_mox),
               static_cast<const void*>(m_ocMatrix));
        if (m_ioBoard) {
            emit m_ioBoard->currentOcByteChanged(ctx.ocByte, bandIdx, m_mox);
        }
        m_lastOcByteLogged = ctx.ocByte;
    }
    ctx.adcCtrl        = m_adcCtrl;
    ctx.p1AdcCntrl     = m_p1AdcCntrl;
    // Phase 3F: the RX band-pass follows AlexController's decision over every
    // slice on the chain when one exists (see setAlexRxBpf), else the
    // RX0-frequency-derived value.
    ctx.alexHpfBits    = effectiveAlexHpfBits();
    // The low-pass is NOT transmit-only, which is what the comment here used
    // to claim. Bank 10 C4 is the Alex0 word, and Alex0 carries the receive
    // selection while unkeyed (netInterface.c:705-717 [v2.10.3.15]).
    ctx.alexLpfBits    = effectiveAlexLpfBits();
    // R-R3-49: the frequencies the radio is sent, corrected (wireFrequencyHz).
    ctx.txFreqHz       = wireFrequencyHz(m_txFreqHz);
    for (int i = 0; i < 7; ++i) { ctx.rxFreqHz[i]   = wireFrequencyHz(m_rxFreqHz[i]); }
    for (int i = 0; i < 3; ++i) { ctx.rxStepAttn[i] = m_stepAttn[i]; }
    // m_txStepAttn is a single int; broadcast to all 3 ADCs (HL2 codec
    // only uses index 0). Standard codec uses a separate path for
    // bank 4 TX drive — txStepAttn array isn't read there.
    for (int i = 0; i < 3; ++i) { ctx.txStepAttn[i] = m_txStepAttn; }
    for (int i = 0; i < 3; ++i) { ctx.rxPreamp[i]   = m_rxPreamp[i]; }
    for (int i = 0; i < 3; ++i) { ctx.dither[i]     = m_dither[i]; }
    for (int i = 0; i < 3; ++i) { ctx.random[i]     = m_random[i]; }
    // HL2-only fields.  ptt_hang and tx_latency start at the mi0bot HL2
    // defaults (12 and 20); without them the HL2 firmware drops the PTT
    // immediately on any TX-buffer underrun and the T/R relay flutters.
    // Source: mi0bot ChannelMaster/netInterface.c:1713-1714 [v2.10.3.14-beta1]
    //   prn->tx[i].tx_latency = 20;  // MI0BOT: HL2
    //   prn->tx[i].ptt_hang   = 12;  // MI0BOT: HL2
    // The HL2 Options page's saved values replace them, as mi0bot's
    // udTxBufferLat / udPTTHang handlers do (setup.cs:21236-21248 [@c26a8a4]):
    //   // MI0BOT: Controls the hardware tx buffer in the HL2
    //   NetworkIO.SetTxLatency((int)udTxBufferLat.Value);
    //   // MI0BOT: Controls the hardware PTT hang in the HL2
    //   NetworkIO.SetPttHang((int)udPTTHang.Value);
    if (m_hardwareProfile.model == HPSDRModel::HERMESLITE) {
        ctx.hl2PttHang   = m_hl2PttHangMs;    // 5-bit field (bank 17 C3), ms
        ctx.hl2TxLatency = m_hl2TxLatencyMs;  // 7-bit field (bank 17 C4), ms
        // Bank 18 C4: reset on Ethernet disconnect, the HL2 Options check
        // box, as mi0bot's chkDisconnectReset handler sends it
        // (setup.cs:21257-21262 [@c26a8a4]):
        //   // MI0BOT: Controls if the HL2 will reset after an Ethernet disconnect
        //   NetworkIO.SetResetOnDisconnect(v);
        ctx.hl2ResetOnDisconnect = m_hl2ResetOnDisconnect;
    }
    // From Thetis cmaster.SetADCSupply / NetworkIO.LRAudioSwap [v2.10.3.15]
    // Per clsHardwareSpecific.cs:85-191 — forwarded to WDSP, not a P1 wire byte.
    ctx.adcSupplyVoltage = m_hardwareProfile.adcSupplyVoltage;
    ctx.lrAudioSwap      = m_hardwareProfile.lrAudioSwap;
    return ctx;
}

// ---------------------------------------------------------------------------
// onReadyRead
//
// Drains incoming datagrams. For each 1032-byte ep6 frame, calls the static
// parseEp6Frame helper and emits iqDataReceived for each receiver.
// Source: networkproto1.c:319-415 MetisReadThreadMainLoop
//
// Upstream inline attribution in that range — preserved verbatim per
// CLAUDE.md §"Inline comment preservation — SHIP-BLOCKING":
//   :335  adc[0].adc_overload |= ControlBytesIn[1] & 0x01; // only cleared by getAndResetADC_Overload(), or'ed with existing state //[2.10.3.13]MW0LGE
//   :353  adc[0].adc_overload |= ControlBytesIn[1] & 1;    // only cleared by getAndResetADC_Overload(), or'ed with existing state //[2.10.3.13]MW0LGE
//   :354  adc[1].adc_overload |= (ControlBytesIn[2] & 1) << 1; // only cleared by getAndResetADC_Overload(), or'ed with existing state //[2.10.3.13]MW0LGE
//   :355  adc[2].adc_overload |= (ControlBytesIn[3] & 1) << 2; // only cleared by getAndResetADC_Overload(), or'ed with existing state //[2.10.3.13]MW0LGE
//
// Handles both Connected and Connecting states so that reconnect attempts
// (which send metis-start from Connecting) can transition to Connected on
// the first good ep6 frame (design doc §3.6 step 5).
// ---------------------------------------------------------------------------
void P1RadioConnection::onReadyRead()
{
    if (!m_socket) { return; }

    // Note: not `const` — we update this local snapshot below when the
    // Connecting -> Connected promotion fires on the first 1032-byte
    // datagram, so the "ep6 stream established" log line emits exactly
    // once per readyRead batch instead of once per datagram in the batch.
    // Without the update, when UDP delivers a burst of ep6 frames in a
    // single readyRead (common on first-connect because the radio is
    // already streaming when metis-start lands), every iteration of the
    // drain loop below sees `cs == Connecting`, calls setState(Connected)
    // (idempotent), and re-emits the log line.  Issue #258 — observed
    // 181 spurious "Connected, ep6 stream established" lines in <10 ms
    // on first connect, misread as a "reconnect storm" in the bug report.
    ConnectionState cs = state();
    // Only process ep6 data when Connected or Connecting (reconnect attempt).
    if (cs != ConnectionState::Connected && cs != ConnectionState::Connecting) {
        // Drain the socket anyway to avoid buffering.
        while (m_socket->hasPendingDatagrams()) {
            m_socket->receiveDatagram();
        }
        return;
    }

    while (m_socket->hasPendingDatagrams()) {
        QNetworkDatagram dg = m_socket->receiveDatagram();
        const QByteArray& data = dg.data();

        if (data.isEmpty()) { continue; }

        // R-R3-32 / R-R3-49 (parity Task 6): every datagram from the radio
        // counts toward UDP packets seen and the packet gap. Atomic stores
        // only; no lock, no allocation (RadioLinkStats).
        const qint64 arrivalUs = RadioLinkStats::nowUs();
        m_linkStats.noteDatagram(arrivalUs);

        // ep6 frames are exactly 1032 bytes
        // Source: networkproto1.c:319 — MetisReadThreadMainLoop receives 1032-byte frames
        if (data.size() == 1032) {
            // Update watchdog timestamp on every good ep6 arrival.
            // Source: NereusSDR design doc §3.6 — successful data resets the retry counter.
            m_lastEp6At = QDateTime::currentDateTimeUtc();

            // Cancel the connect watchdog — first good ep6 means we reached
            // the radio successfully. Design §4.1.
            if (m_connectWatchdog && m_connectWatchdog->isActive()) {
                m_connectWatchdog->stop();
            }

            // Per-frame activity signal for TitleBar LED (throttled to 10 Hz
            // by the receiver). Design §4.1.
            emit frameReceived();

            // Diagnostic: log the first EP6 frame of this session so
            // remote debugging can tell "connected but no data" apart
            // from "connected and streaming".
            if (!m_firstEp6Logged) {
                m_firstEp6Logged = true;
                qCInfo(lcConnection) << "P1: first ep6 frame received from"
                                     << dg.senderAddress().toString()
                                     << "(1032 bytes)";
            }

            // First good ep6 frame promotes Connecting -> Connected. This
            // path serves both the initial connect (issue #239) and the
            // reconnect-after-LinkLost flow (design doc §3.6 step 5). The
            // log message branches on m_reconnectAttempts so an initial
            // connect doesn't claim it was "Reconnected".
            if (cs == ConnectionState::Connecting) {
                const bool wasReconnecting = (m_reconnectAttempts > 0);
                m_reconnectAttempts = 0;
                if (m_reconnectTimer) { m_reconnectTimer->stop(); }
                if (!m_watchdogTimer->isActive()) { m_watchdogTimer->start(); }
                enterDataFlowing();
                // Sync the local snapshot so subsequent iterations of the
                // drain loop don't re-enter this branch.  Issue #258.
                cs = ConnectionState::Connected;
                if (wasReconnecting) {
                    // TX safety fix round 1: the link is back unkeyed, so a
                    // latch from a loss while keyed has done its work (the
                    // reconnect's frames went out unkeyed) and the next key
                    // goes through. A key still set here keeps the latch.
                    if (!m_mox) {
                        m_linkLossLatched = false;
                        m_linkLossRefusalLogged = false;
                    }
                    if (!m_reconnectedLogged) {
                        qCDebug(lcConnection) << "P1: Reconnected, ep6 stream restored";
                        m_reconnectedLogged = true;
                    }
                } else {
                    qCDebug(lcConnection) << "P1: Connected, ep6 stream established";
                }
            }

            // HL2 clock lists queued before this frame count as sent once
            // the radio answers after their last write left the queue.
            if (cs == ConnectionState::Connected) {
                hl2ClockConfirmSent();
            }

            // Phase 3P-E Task 3: record ep6 ingress bytes for bandwidth monitor.
            // Source: mi0bot bandwidth_monitor.c:74-78 bandwidth_monitor_in() [@c26a8a4]
            if (m_bwMonitor) { m_bwMonitor->recordEp6Bytes(data.size()); }

            // Shell-chrome sub-PR-2 B.1: record ingress bytes for ▼ Mbps readout.
            recordBytesReceived(static_cast<qint64>(data.size()));

            // R-R3-21: EP6 sequence gaps, the count Diagnostics > Connection
            // Quality names. From Thetis networkproto1.c:171-194 MetisReadDirect
            // [v2.10.3.15]: an EF FE 01 frame for endpoint 6 carries its
            // sequence number big-endian in bytes 4-7; each frame whose
            // number is not one past the last counts one error:
            //   if (seqnum != (1 + MetisLastRecvSeq)) {
            //       SeqError += 1;
            //   }
            //   MetisLastRecvSeq = seqnum;
            {
                const auto* hdr = reinterpret_cast<const quint8*>(data.constData());
                if (hdr[0] == 0xEF && hdr[1] == 0xFE && hdr[2] == 0x01 && hdr[3] == 0x06) {
                    const quint32 seqnum = (quint32(hdr[4]) << 24) | (quint32(hdr[5]) << 16)
                                         | (quint32(hdr[6]) << 8)  |  quint32(hdr[7]);
                    const bool seqError = seqnum != (1 + m_ep6LastRecvSeq);
                    if (seqError) {
                        if (m_bwMonitor) { m_bwMonitor->recordEp6SequenceError(); }
                    }
                    // R-R3-32 (parity Task 6): the same count, one per
                    // mismatch, feeds the link's packet loss. The first
                    // frame of a connection has no number before it.
                    m_linkStats.noteSequenced(arrivalUs,
                                              (seqError && m_ep6SeqPrimed) ? 1U : 0U);
                    // NereusSDR-native jitter (RFC 3550 section 6.4.1, see
                    // RadioLinkStats): one ep6 datagram is two 504-byte
                    // sub-frames of spr samples each, spr = 504 / (6*nddc + 2)
                    // (networkproto1.c:358 [v2.10.3.15]).
                    const int spr = 504 / (6 * std::max(1, m_activeRxCount) + 2);
                    const double spacingUs = m_sampleRate > 0
                        ? (2.0 * spr * 1.0e6) / static_cast<double>(m_sampleRate) : 0.0;
                    m_linkStats.noteStreamArrival(0, seqnum, arrivalUs, spacingUs);
                    m_ep6SeqPrimed = true;
                    m_ep6LastRecvSeq = seqnum;
                }
            }

            parseEp6Frame(data);
        }
    }
}

// ---------------------------------------------------------------------------
// onWatchdogTick
//
// Fires every kWatchdogTickMs ms while connected or reconnecting.
// Silence-detection only — EP2 pacing lives on m_ep2PacerTimer
// (onEp2PacerTick) to match Thetis' 381 pps audio-clock cadence.
//
// Detects ep6 silence: if no frame has arrived for kWatchdogSilenceMs,
// transitions to LinkLost and arms the reconnect timer (Phase 3Q-1).
// Applies to both Connected (initial silence detection) and Connecting
// (reconnect attempt timed out — the retry got no response).
// Source: NereusSDR design doc §3.6.
// ---------------------------------------------------------------------------
void P1RadioConnection::onWatchdogTick()
{
    if (!m_running || !m_socket || m_radioInfo.address.isNull()) { return; }

    const ConnectionState cs = state();

    // Watchdog is silence-detection only. EP2 pacing lives on m_ep2PacerTimer
    // (onEp2PacerTick) to match Thetis' 381 pps audio-clock cadence. See
    // kEp2PacerIntervalMs.

    // HL2 bandwidth monitor — check for LAN PHY throttle on every watchdog tick.
    // Source: mi0bot bandwidth_monitor.{c,h} — NereusSDR sequence-gap adaptation.
    if (m_caps && m_caps->hasBandwidthMonitor) {
        hl2CheckBandwidthMonitor();
    }

    // Phase 3M-1c TX pump v3: mic-frame LOS injection.
    // Mirrors Thetis network.c:655-666 [v2.10.3.13] — when no UDP frame
    // has arrived for 3000 ms, push a zero block into the TX inbound ring
    // so the worker keeps ticking through silence (otherwise the worker
    // would block forever on waitForBlock(INFINITE)).  We use mono-sample
    // count == kBlockFrames so exactly one ring block is released.
    if (m_txMicSource != nullptr && m_lastMicAt.isValid()) {
        const qint64 sinceMicMs = m_lastMicAt.msecsTo(QDateTime::currentDateTimeUtc());
        if (sinceMicMs > kMicLosTimeoutMs) {
            std::array<float, TxMicSource::kBlockFrames> zeros{};
            m_txMicSource->inbound(zeros.data(), TxMicSource::kBlockFrames);
            // Reset the LOS timer so we inject one block per kMicLosTimeoutMs
            // window (not one block per watchdog tick).
            m_lastMicAt = QDateTime::currentDateTimeUtc();
        }
    }

    // Silence detection applies to both Connected and Connecting states.
    // In Connecting: the reconnect attempt sent metis-start but got no ep6 response.
    if (cs != ConnectionState::Connected && cs != ConnectionState::Connecting) { return; }

    // If we haven't received any ep6 frame yet, don't trip the watchdog.
    if (!m_lastEp6At.isValid()) { return; }

    // R-R3-49: with the Network Watchdog off an established link waits for
    // data for ever, as Thetis does. A reconnect attempt (Connecting) that
    // got no answer still times out: that is this program's own retry, run
    // only after a loss was declared.
    // From Thetis networkproto1.c:292-294 [v2.10.3.15]:
    //   //MW0LGE_21g WSAWaitForMultipleEvents(1, &prn->hDataEvent, FALSE, WSA_INFINITE, FALSE);
    //   //added similar timout code from ReadThreadMainLoop
    //   DWORD retVal = WSAWaitForMultipleEvents(1, &prn->hDataEvent, FALSE, prn->wdt ? 3000 : WSA_INFINITE, FALSE);
    if (cs == ConnectionState::Connected && !m_watchdogEnabled) { return; }

    const qint64 silenceMs = m_lastEp6At.msecsTo(QDateTime::currentDateTimeUtc());
    if (silenceMs > m_watchdogSilenceMs) {
        qCWarning(lcConnection) << "P1: Watchdog — ep6 silent for" << silenceMs
                                << "ms (state=" << static_cast<int>(cs)
                                << "); transitioning to LinkLost and scheduling reconnect";
        m_watchdogTimer->stop();
        if (m_ep2PacerTimer) {
            m_ep2PacerTimer->stop();
        }
        m_reconnectedLogged = false;
        // TX safety: the lost link drops out of transmit here, before any
        // observer of LinkLost runs, and a link lost keyed refuses a key
        // until it is back.
        // Thetis drops its keyed state on the same event: the read loop's
        // timeout reports loss of sync to the console,
        //   From Thetis ChannelMaster/networkproto1.c:292-297 [v2.10.3.15]
        //     //MW0LGE_21g WSAWaitForMultipleEvents(1, &prn->hDataEvent, FALSE, WSA_INFINITE, FALSE);
        //     //added similar timout code from ReadThreadMainLoop
        //     DWORD retVal = WSAWaitForMultipleEvents(1, &prn->hDataEvent, FALSE, prn->wdt ? 3000 : WSA_INFINITE, FALSE);
        //     if ((retVal == WSA_WAIT_FAILED) || (retVal == WSA_WAIT_TIMEOUT))
        //     {
        //         HaveSync = 0; //send console LOS
        // and the console then powers the radio off, which unchecks MOX
        // (console.cs:21336-21340 and 27488-27493 [v2.10.3.15], cited where
        // RadioModel unkeys on LinkLost). P2's established-silence stop
        // (P2RadioConnection::stopForEstablishedSilence) also clears MOX,
        // PureSignal and the relay, but it then sends one unkeyed stop and
        // stays down until a new connectToRadio; it does not reconnect by
        // itself, and it latches on every loss. Here the latch is set only
        // when the link was lost keyed (fix round 1): a link lost unkeyed
        // has nothing to refuse, and RadioModel's key block covers the
        // outage (MoxController::setRadioLinkDown).
        const bool keyedAtLoss = m_mox;
        dropTransmitForNewLink();
        if (keyedAtLoss) {
            m_linkLossLatched = true;
            m_linkLossRefusalLogged = false;
        }
        // Phase 3Q-1: ConnectionState::Error removed from the 5-value enum.
        // Watchdog timeout (was Connected, frames stopped) → LinkLost.
        setState(ConnectionState::LinkLost);
        // HL2 clock lists queued in the silence, or not yet answered, go
        // again when the link comes back (hl2ClockOnDataFlowing).
        hl2ClockMarkUnconfirmed();
        emit errorOccurred(RadioConnectionError::NoDataTimeout,
                           QStringLiteral("Radio stopped responding"));

        // Arm the reconnect timer for the next retry attempt (or first if from Connected).
        // Source: NereusSDR design doc §3.6 — 5-second reconnect interval.
        m_reconnectTimer->start(m_reconnectIntervalMs);
    }
}

// ---------------------------------------------------------------------------
// onEp2PacerTick
//
// Fires every kEp2PacerIntervalMs ms while connected. Uses a catch-up loop
// against m_ep2PacerClock so the aggregate send rate tracks 380.95 pps
// (48 kHz audio / 126 samples per EP2 frame) even when Qt's PreciseTimer
// under-delivers on Windows (10-15 ms resolution). Each tick emits a small
// burst to make up the deficit, capped at kEp2MaxBurstPerTick for safety.
//
// This matches Thetis sendProtocol1Samples (networkproto1.c:700-747), which
// is driven by a 48 kHz audio-semaphore clock — not by timer precision.
//
// Unlike the watchdog's former EP2 send, this path intentionally does NOT
// consult m_hl2Throttled. Thetis never pauses sends, and pausing egress is
// a weak remedy for an HL2 ingress PHY stall. The throttle flag and
// bandwidth monitor logic remain in place for future use.
// ---------------------------------------------------------------------------
void P1RadioConnection::onEp2PacerTick()
{
    if (!m_running || !m_socket || m_radioInfo.address.isNull()) { return; }

    const ConnectionState cs = state();
    if (cs != ConnectionState::Connected && cs != ConnectionState::Connecting) { return; }

    // Catch-up loop: compute how many packets should have been sent by now at
    // the 380.95 pps target, and emit the missing ones. On Windows the Qt
    // PreciseTimer only delivers ~10 ms ticks (multimedia timer floor), so
    // each tick typically emits a burst of 3-5 packets. This matches Thetis'
    // sendProtocol1Samples pacing (networkproto1.c:700-747), which is driven
    // by a 48 kHz audio-subsystem semaphore rather than timer precision.
    if (!m_ep2PacerClock.isValid()) {
        m_ep2PacerClock.start();
        m_ep2PacketsSent = 0;
    }

    const qint64 elapsedUs = m_ep2PacerClock.nsecsElapsed() / 1000;
    const qint64 due       = elapsedUs / kEp2PacketIntervalUs;
    int burst = 0;
    while (m_ep2PacketsSent < due && burst < kEp2MaxBurstPerTick) {
        sendCommandFrame();
        ++m_ep2PacketsSent;
        ++burst;
    }
}

// ---------------------------------------------------------------------------
// onReconnectTimeout
//
// Called when the single-shot reconnect timer fires.
// Implements bounded retries: up to kMaxReconnectAttempts, then stays in LinkLost.
// Source: NereusSDR design doc §3.6.
// ---------------------------------------------------------------------------
void P1RadioConnection::onReconnectTimeout()
{
    // Guard: don't retry after an intentional disconnect.
    if (m_intentionalDisconnect) { return; }

    if (m_reconnectAttempts >= kMaxReconnectAttempts) {
        qCWarning(lcConnection) << "P1: Reconnect — bounded retries exhausted after"
                                << kMaxReconnectAttempts << "attempts; staying in LinkLost";
        // Stay in Error — user must explicitly call connectToRadio() to reset.
        return;
    }

    ++m_reconnectAttempts;
    qCDebug(lcConnection) << "P1: Reconnect attempt" << m_reconnectAttempts
                          << "of" << kMaxReconnectAttempts;

    // The route to the radio may have changed, and a connect timeout closes
    // the socket; bind it again (returns at once when nothing changed).
    bindToRadioFacingAddress();

    // TX safety: the attempt goes out unkeyed. A key that reached the
    // connection during the outage is dropped here and latched again, so it
    // does not resume when the link is back. Without this a link lost while
    // keyed came back with C0 MOX = 1 on the priming burst and on every
    // frame after it.
    const bool keyedDuringOutage = m_mox;
    dropTransmitForNewLink();
    if (keyedDuringOutage) {
        m_linkLossLatched = true;
        m_linkLossRefusalLogged = false;
    }

    // Transition to Connecting for this retry attempt.
    setState(ConnectionState::Connecting);

    // Send stop then prime then start so the radio re-arms its ep6 sender
    // with the current RX1 frequency latched. Without the primed C&C burst,
    // the radio comes back up in ADC-idle state (I=DC, Q=0).
    // Source: networkproto1.c:49-110 SendStopToMetis / SendStartToMetis plus
    // the ForceCandCFrame bracket pattern from MetisReadThreadMainLoop:281.
    sendMetisStop();
    sendPrimingBurst(3);
    sendMetisStart(false);
    sendPrimingBurst(3);

    // Re-arm the watchdog so onReadyRead can complete the transition to Connected.
    if (!m_watchdogTimer->isActive()) {
        m_watchdogTimer->start();
    }
    if (m_ep2PacerTimer && !m_ep2PacerTimer->isActive()) {
        m_ep2PacerClock.restart();
        m_ep2PacketsSent = 0;
        m_ep2PacerTimer->start();
    }

    // Re-arm the reconnect timer so if this attempt also fails, the next retry
    // is scheduled automatically (the watchdog will stop itself and re-arm this
    // timer again when it detects silence).
    // We do NOT re-arm here unconditionally — the watchdog arms it when needed.
    // But we schedule a fallback in case no ep6 data arrives within the window
    // (i.e., watchdog trips again → re-arms reconnect timer).
    // No extra start() needed; see onWatchdogTick for the arming path.
}

// ---------------------------------------------------------------------------
// onConnectTimeout — Phase 3Q Task 3
//
// Fires kConnectTimeoutMs after connectToRadio() if no first ep6 frame
// arrived. The radio is either unreachable (wrong IP, powered off, VPN
// blocking UDP) or too slow to reply within the budget.
// Emits connectFailed(Timeout, ...) so the UI can surface a typed message.
// onReadyRead() cancels this timer on the first valid ep6 frame.
// ---------------------------------------------------------------------------
void P1RadioConnection::onConnectTimeout()
{
    // Guard: intentional disconnect() races are harmless (disconnect() stops
    // the timer, but if both fire in the same event-loop turn, check here).
    if (m_intentionalDisconnect) { return; }

    // Guard: if we somehow received a first frame already (timer fired late),
    // do nothing.
    if (m_lastEp6At.isValid()) { return; }

    qCWarning(lcConnection) << "P1: Connect watchdog fired — no ep6 frame within"
                            << kConnectTimeoutMs << "ms; tearing down and emitting connectFailed(Timeout)";

    // Issue #239: tear down to Disconnected so the UI does not claim
    // "Connected" while the radio is unreachable. We stop the ep2 pacer,
    // silence watchdog, and reconnect timer (none of these should keep
    // pumping after a failed initial connect) and close the socket; the
    // user has to click Connect again. m_intentionalDisconnect is then set
    // so any in-flight datagram drained by onReadyRead() is ignored.
    m_running = false;
    m_intentionalDisconnect = true;
    if (m_watchdogTimer) { m_watchdogTimer->stop(); }
    if (m_ep2PacerTimer) { m_ep2PacerTimer->stop(); }
    if (m_reconnectTimer) { m_reconnectTimer->stop(); }
    if (m_socket) { m_socket->close(); }
    setState(ConnectionState::Disconnected);

    emit connectFailed(ConnectFailure::Timeout,
                       QStringLiteral("No response from radio within %1 ms. Check the "
                                      "IP address, radio power and network.")
                           .arg(kConnectTimeoutMs));
}

// ---------------------------------------------------------------------------
// sendMetisStart
//
// Source: networkproto1.c:33-68 SendStartToMetis
//   outpacket.packetbuf[0] = 0xef  (line 43)
//   outpacket.packetbuf[1] = 0xfe  (line 44)
//   outpacket.packetbuf[2] = 0x04  (line 49)
//   outpacket.packetbuf[3] = 0x01  (line 50) — start IQ stream
//   Packet is 64 bytes, padded with zeros.
//   iqAndMic=true → cmd 0x02 (IQ+mic), false → cmd 0x01 (IQ only)
// ---------------------------------------------------------------------------
void P1RadioConnection::sendMetisStart(bool iqAndMic)
{
    if (!m_socket) { return; }

    // Source: networkproto1.c:33-68 SendStartToMetis — 64-byte packet
    QByteArray pkt(64, '\0');
    pkt[0] = static_cast<char>(0xEF);
    pkt[1] = static_cast<char>(0xFE);
    pkt[2] = static_cast<char>(0x04);

    // R-R3-49: the Network Watchdog setting never reaches this byte. Thetis
    // and mi0bot-Thetis send the same start command whatever the setting;
    // on Protocol 1 the setting only sets how long the read loop waits for
    // data (onWatchdogTick).
    // From Thetis networkproto1.c:47-50 [v2.10.3.15]:
    //   outpacket.packetbuf[0] = 0xef;
    //   outpacket.packetbuf[1] = 0xfe;
    //   outpacket.packetbuf[2] = 0x04;
    //   outpacket.packetbuf[3] = 0x01;
    // (mi0bot-Thetis networkproto1.c:50 [@c26a8a4] is the same.)
    // iqAndMic=true sends 0x02 (IQ + mic), as before this change.
    const quint8 runBits = iqAndMic ? quint8(0x02) : quint8(0x01);
    pkt[3] = static_cast<char>(runBits);

    m_socket->writeDatagram(pkt, m_radioInfo.address, m_radioInfo.port);
}

// ---------------------------------------------------------------------------
// sendMetisStop
//
// Source: networkproto1.c:72-110 SendStopToMetis
//   outpacket.packetbuf[2] = 0x04  (line 84)
//   outpacket.packetbuf[3] = 0x00  (stop command)
//   Packet is 64 bytes, padded with zeros.
// ---------------------------------------------------------------------------
void P1RadioConnection::sendMetisStop()
{
    if (!m_socket) { return; }

    // Source: networkproto1.c:72-110 SendStopToMetis — 64-byte packet
    QByteArray pkt(64, '\0');
    pkt[0] = static_cast<char>(0xEF);
    pkt[1] = static_cast<char>(0xFE);
    pkt[2] = static_cast<char>(0x04);

    // Stop packet (run = 0). R-R3-49: the Network Watchdog setting is not
    // sent (see sendMetisStart).
    // From Thetis networkproto1.c:85 [v2.10.3.15] (mi0bot-Thetis
    // networkproto1.c:85 [@c26a8a4] is the same):
    //   outpacket.packetbuf[3] = 0x00;
    pkt[3] = static_cast<char>(0x00);

    m_socket->writeDatagram(pkt, m_radioInfo.address, m_radioInfo.port);
}

// ---------------------------------------------------------------------------
// sendCommandFrame
//
// Builds a 1032-byte ep2 frame with two C&C subframes drawn from the full
// 17-bank round-robin sequence. Each call advances m_ccRoundRobinIdx by 2.
// Source: networkproto1.c:216-236 MetisWriteFrame + :597-884 WriteMainLoop
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// composeSubframe
//
// One subframe's five C&C bytes. On an HL2 a queued I2C transaction takes
// the subframe when its spacing allows, and the round-robin stays on the
// bank it was due to send; otherwise the bank goes and the round-robin
// moves on.
// From mi0bot ChannelMaster/networkproto1.c:898-906, 943-944 and
// 1180-1183 [@c26a8a4] (WriteMainLoop_HL2):
//   if (0 != prn->i2c.delay)
//   {
//       prn->i2c.delay--;
//   }
//   if ((0 >= --prn->i2c.delay) &&
//       (prn->i2c.in_index != prn->i2c.out_index))
//   {
//       prn->i2c.delay = 5;
//       ...                               // the I2C frame
//   }
//   else
//   {
//       switch (out_control_idx) { ... }  // the bank
//       if (out_control_idx < 18)         // ready for next USB frame
//           out_control_idx++;
//       else
//           out_control_idx = 0;
//   }
// So an I2C frame goes at most every third subframe. mi0bot's delay is a
// signed char that keeps falling while nothing is queued and wraps; here
// it stops at -1, so a transaction queued after a quiet spell goes on the
// next subframe.
// ---------------------------------------------------------------------------
void P1RadioConnection::composeSubframe(quint8 out[5], int maxBank)
{
    if (m_i2cDelay != 0) {
        --m_i2cDelay;
    }
    const bool i2cDue = (--m_i2cDelay <= 0);
    if (m_i2cDelay < -1) {
        m_i2cDelay = -1;
    }
    if (i2cDue && composeI2cFrame(out)) {
        m_i2cDelay = 5;
        return;
    }
    composeBank(m_ccRoundRobinIdx, out);
    m_ccRoundRobinIdx++;
    if (m_ccRoundRobinIdx > maxBank) { m_ccRoundRobinIdx = 0; }
}

int P1RadioConnection::ccMaxBank() const
{
    return m_codec
        ? m_codec->maxBank()
        : ((m_hardwareProfile.model == HPSDRModel::ANVELINAPRO3) ? 17 : 16);
}

// ---------------------------------------------------------------------------
// HL2 I/O board ongoing poll
//
// From mi0bot Console/console.cs:25781-25945 UpdateIOBoard [@c26a8a4], the
// loop after the board is found, one step per 40 ms:
//   switch (state++)
//   {
//       case 3:
//       case 6: // Secondary receive selection
//           if (IOBoardAerialMode != old_IOBoardAerialMode)
//           {
//               ioBoard.writeRequest(IOBoard.Registers.REG_RF_INPUTS, IOBoardAerialMode);
//               old_IOBoardAerialMode = IOBoardAerialMode;
//           }
//           break;
//       case 1:
//       case 4:
//       case 7:
//       case 10:
//           // Read the input pins
//           while (0 != ioBoard.readRequest(IOBoard.Registers.REG_INPUT_PINS)) ...
//           ... status = ioBoard.readResponse();      // [3] Input pins, [2] Ant tuner, [1] Fault, [0] Major Version
//           break;
//       case 8:
//       case 2: // Write current transmission frequency
//           ioBoard.setFrequency(currentFreq);
//           break;
//       case 9:
//       case 5: // Aerial selection
//           if (IOBoardAerialPorts != old_IOBoardAerialPorts)
//           {
//               ioBoard.writeRequest(IOBoard.Registers.REG_ANTENNA, IOBoardAerialPorts);
//               old_IOBoardAerialPorts = IOBoardAerialPorts;
//           }
//           break;
//       case 0: // Mode selection
//           ... CurrentMode = (Byte) _rx1_dsp_mode;  (the TX VFO's mode)
//           if (CurrentMode != old_IOBoardMode)
//           {
//               ioBoard.writeRequest(IOBoard.Registers.REG_OP_MODE, CurrentMode);
//               old_IOBoardMode = CurrentMode;
//           }
//           break;
//       case 11:
//       default:
//           state = 0;
//           break;
//   }
// IOBoard (HPSDR/IoBoardHl2.cs:129-201 [@c26a8a4]) sends every register to
// bus 1 at 0x1d: readRequest is NetworkIO.I2CReadInitiate(1, 0x1d, reg),
// writeRequest NetworkIO.I2CWrite(1, 0x1d, reg, data), and setFrequency,
// when the frequency changed, five writes of REG_TX_FREQ_BYTE4..BYTE0 with
// (frequency >> 32), >> 24, >> 16, >> 08, >> 00.
//
// Read answers land in IoBoardHl2's registers (applyI2cReadResponse), four
// from REG_INPUT_PINS: input pins, antenna tuner, fault, firmware major.
// A read goes only when the queue is empty, as I2CReadInitiate starts one
// only then (netInterface.c:1470-1497); where mi0bot then waits and gives
// up the whole loop after 20 tries, this step skips its read. The TX state
// arrives from RadioModel (setIoBoardTxState) rather than being read from
// the VFOs each step. RADE modes are not Thetis DSPMode values; RADE_U is
// written as DIGU and RADE_L as DIGL. The fault reaction (TX inhibit) lives in RadioModel, which hands
// each REG_FAULT read to TxInhibitMonitor. The aerial values arrive from
// RadioModel::applyAlexAntennaForBand (setIoBoardAerials), composed by
// AlexController::hl2IoBoardAerials. While the manual I2C tool holds the
// pause (IoBoardHl2::setPollingPause, mi0bot SetI2CPollingPause) a tick
// does nothing and the step stays where it is. The auto-tune protocol is
// not ported here.
// ---------------------------------------------------------------------------
void P1RadioConnection::setIoBoardTxState(int dspMode, qint64 frequencyHz)
{
    m_ioTxMode = dspMode;
    m_ioTxFrequencyHz = frequencyHz;
}

void P1RadioConnection::setIoBoardAerials(quint8 aerialMode, quint8 aerialPorts)
{
    m_ioAerialMode = aerialMode;
    m_ioAerialPorts = aerialPorts;
}

void P1RadioConnection::ioBoardPollTick()
{
    if (!m_caps || !m_caps->hasIoBoardHl2 || !m_ioBoard || !m_ioBoard->isDetected()) {
        return;
    }
    // From mi0bot console.cs:25930-25935 [@c26a8a4]:
    //   // Delay and continue to delay if we have been paused
    //   do { await Task.Delay(40); } while (I2CPollingPause);
    // The manual I2C tool holds the pause; the poll waits on this step.
    if (m_ioBoard->isPollingPaused()) {
        return;
    }
    // The clock chip writes hold the poll as mi0bot's SetI2CPollingPause
    // does around WriteVersaClockAsync (setup.cs:21644 [@c26a8a4]).
    if (!m_hl2ClockPending.empty()) {
        return;
    }
    using Reg = IoBoardHl2::Register;
    // mi0bot's writeRequest and setFrequency ignore I2CWrite's result
    // (IoBoardHl2.cs:176-202 [@c26a8a4]), and I2CWrite returns -1 when the
    // queue is full (netInterface.c:1536-1564 [@c26a8a4]), so a write that
    // meets a full queue is lost while currentFreq says it was sent. Here a
    // value is recorded as written only when it was queued; otherwise the
    // next poll on that step sends it again.
    const auto write = [this](Reg reg, quint8 data) -> bool {
        IoBoardHl2::I2cTxn txn;
        txn.bus = IoBoardHl2::kI2cBusIndex;
        txn.address = IoBoardHl2::kI2cAddrGeneral;
        txn.control = static_cast<quint8>(reg);
        txn.writeData = data;
        txn.isRead = false;
        txn.needsResponse = false;
        return m_ioBoard->enqueueI2c(txn);
    };

    switch (m_ioBoard->currentStep()) {
        case 3:
        case 6: // Secondary receive selection
            if (m_ioAerialMode != m_ioWrittenAerialMode) {
                if (write(Reg::REG_RF_INPUTS, m_ioAerialMode)) {
                    m_ioWrittenAerialMode = m_ioAerialMode;
                }
            }
            break;

        case 1:
        case 4:
        case 7:
        case 10: // Read the input pins
            if (m_ioBoard->i2cQueueIsEmpty()) {
                IoBoardHl2::I2cTxn txn;
                txn.bus = IoBoardHl2::kI2cBusIndex;
                txn.address = IoBoardHl2::kI2cAddrGeneral;
                txn.control = static_cast<quint8>(Reg::REG_INPUT_PINS);
                txn.writeData = 0x00;
                txn.isRead = true;
                txn.needsResponse = true;
                m_ioBoard->enqueueI2c(txn);
            }
            break;

        case 8:
        case 2: // Write current transmission frequency
            // The five bytes go only when all five fit, so the board is not
            // left holding part of a new frequency. If another thread takes
            // a slot in between, the whole frequency goes again next time.
            if (m_ioTxFrequencyHz != m_ioWrittenFrequencyHz
                && IoBoardHl2::kMaxI2cQueue - m_ioBoard->i2cQueueDepth() >= 5) {
                const qint64 f = m_ioTxFrequencyHz;
                bool queued = write(Reg::REG_TX_FREQ_BYTE4, static_cast<quint8>(f >> 32));
                queued = write(Reg::REG_TX_FREQ_BYTE3, static_cast<quint8>(f >> 24)) && queued;
                queued = write(Reg::REG_TX_FREQ_BYTE2, static_cast<quint8>(f >> 16)) && queued;
                queued = write(Reg::REG_TX_FREQ_BYTE1, static_cast<quint8>(f >> 8)) && queued;
                queued = write(Reg::REG_TX_FREQ_BYTE0, static_cast<quint8>(f >> 0)) && queued;
                if (queued) {
                    m_ioWrittenFrequencyHz = f;
                }
            }
            break;

        case 9:
        case 5: // Aerial selection
            if (m_ioAerialPorts != m_ioWrittenAerialPorts) {
                if (write(Reg::REG_ANTENNA, m_ioAerialPorts)) {
                    m_ioWrittenAerialPorts = m_ioAerialPorts;
                }
            }
            break;

        case 0: { // Mode selection
            // From mi0bot console.cs:25905-25921 [@c26a8a4]: CurrentMode =
            // (Byte) _rx1_dsp_mode, a Thetis DSPMode value (enums.cs:252-270:
            // LSB 0 .. DRM 11, DIGU 7, DIGL 9). RADE is NereusSDR's own mode,
            // so it goes as the digital mode on its sideband (operator ruling
            // 2026-09-29): RADE_U as DIGU, RADE_L as DIGL.
            int mode = m_ioTxMode;
            if (mode == static_cast<int>(DSPMode::RADE_U)) {
                mode = static_cast<int>(DSPMode::DIGU);
            } else if (mode == static_cast<int>(DSPMode::RADE_L)) {
                mode = static_cast<int>(DSPMode::DIGL);
            }
            if (mode >= static_cast<int>(DSPMode::LSB)
                && mode <= static_cast<int>(DSPMode::DRM)
                && mode != m_ioWrittenMode) {
                if (write(Reg::REG_OP_MODE, static_cast<quint8>(mode))) {
                    m_ioWrittenMode = mode;
                }
            }
            break;
        }

        case 11:
        default:
            break;
    }
    m_ioBoard->advanceStep();
}

void P1RadioConnection::sendCommandFrame()
{
    if (!m_socket) { return; }

    // Phase 3P-D follow-up: drive the bank ceiling from the active codec's
    // maxBank() so HL2 (18) and AnvelinaPro3 (17) both emit their full bank
    // range. Standard = 16. The legacy compose path (m_codec == nullptr under
    // NEREUS_USE_LEGACY_P1_CODEC=1) retains the pre-refactor model-keyed
    // constant to preserve the regression-freeze byte-identical guarantee.
    const int maxBank = ccMaxBank();

    quint8 frame[1032];
    memset(frame, 0, sizeof(frame));

    // EP2 header (networkproto1.c:223-230)
    frame[0] = 0xEF; frame[1] = 0xFE; frame[2] = 0x01; frame[3] = 0x02;
    const quint32 seq = m_epSendSeq++;
    frame[4] = static_cast<quint8>((seq >> 24) & 0xFF);
    frame[5] = static_cast<quint8>((seq >> 16) & 0xFF);
    frame[6] = static_cast<quint8>((seq >>  8) & 0xFF);
    frame[7] = static_cast<quint8>( seq        & 0xFF);

    // 3M-1a E.3: if setMox() requested a bank-0 flush, reset the round-robin
    // to 0 so this frame carries the MOX bit within ≤1 frame of the call.
    // Source: deskhpsdr/src/old_protocol.c:3595-3599 [@120188f] — the reference
    // implementation does not defer MOX; it sets the bit on the very next frame.
    // 3M-1a E.4: if setTrxRelay() requested a bank-10 flush, jump to bank 10
    // so the T/R relay bit (C3 bit 7) lands within ≤1 frame of the call.
    // 3M-1b G.3: if setMicTipRing() requested a bank-11 flush, jump to bank 11
    // so the mic_trs bit (C1 bit 4) lands within ≤1 frame of the call.
    // Priority: bank 0 > bank 10 > bank 11.  Losing flags are preserved and
    // fire on the following frame (same pattern as bank 0 vs bank 10).
    // Source: deskhpsdr/src/old_protocol.c:2909-2910 [@120188f].
    // Source: Thetis ChannelMaster/networkproto1.c:597 [v2.10.3.13].
    if (m_forceBank0Next) {
        m_ccRoundRobinIdx = 0;
        m_forceBank0Next  = false;
    } else if (m_forceBank10Next) {
        m_ccRoundRobinIdx = 10;
        m_forceBank10Next = false;
    } else if (m_forceBank11Next) {
        m_ccRoundRobinIdx = 11;
        m_forceBank11Next = false;
    } else if (m_forceBank4Next) {
        // Phase 3M-4 Task 17 P1 follow-up: priority just below bank 0/10/11
        // (which carry safety bits — MOX, T/R relay, mic_trs).  Bank 4
        // carries TX step attenuator (C3 bits 0-4) for PS auto-att updates.
        // Mirrors Thetis ChannelMaster/netInterface.c:1010 CmdTx() after
        // SetTxAttenData [v2.10.3.13].
        m_ccRoundRobinIdx = 4;
        m_forceBank4Next  = false;
    } else if (m_diversityFrequencyBanks != 0) {
        for (int bank : {2, 3, 5, 6, 7}) {
            if ((m_diversityFrequencyBanks & (1 << bank)) != 0) {
                m_ccRoundRobinIdx = bank;
                m_diversityFrequencyBanks &= ~(1 << bank);
                break;
            }
        }
    }

    // Subframe 0: current bank
    frame[8] = 0x7F; frame[9] = 0x7F; frame[10] = 0x7F;
    quint8 cc0[5] = {};
    composeSubframe(cc0, maxBank);
    frame[11] = cc0[0]; frame[12] = cc0[1]; frame[13] = cc0[2];
    frame[14] = cc0[3]; frame[15] = cc0[4];

    // Subframe 1: next bank
    frame[520] = 0x7F; frame[521] = 0x7F; frame[522] = 0x7F;
    quint8 cc1[5] = {};
    composeSubframe(cc1, maxBank);
    frame[523] = cc1[0]; frame[524] = cc1[1]; frame[525] = cc1[2];
    frame[526] = cc1[3]; frame[527] = cc1[4];

    // 3M-1a E.2: fill the two 504-byte TX I/Q zones from the ring buffer.
    // Each zone holds 63 samples × 8 bytes = 504 bytes.
    // From deskhpsdr/src/old_protocol.c:545-549 [@120188f]:
    //   memcpy(output_buffer + 8, &TXRINGBUF[out],       504); ozy_send_buffer();
    //   memcpy(output_buffer + 8, &TXRINGBUF[out + 504], 504); ozy_send_buffer();
    // frame[16..519]   = subframe 0 TX data zone (after 3-byte sync + 5-byte C&C)
    // frame[528..1031] = subframe 1 TX data zone (after 3-byte sync + 5-byte C&C)
    fillTxZone(frame + 16);
    fillTxZone(frame + 528);
    // Radio codec (2026-09-30): the L/R bytes of both zones carry the
    // receive audio for the radio's own speaker out (fillRadioAudioZone).
    fillRadioAudioZone(frame + 16);
    fillRadioAudioZone(frame + 528);

    QByteArray pkt(reinterpret_cast<const char*>(frame), 1032);
    m_socket->writeDatagram(pkt, m_radioInfo.address, m_radioInfo.port);

    // Phase 3P-E Task 3: record ep2 egress bytes for bandwidth monitor.
    // Source: mi0bot bandwidth_monitor.c:80-84 bandwidth_monitor_out() [@c26a8a4]
    if (m_bwMonitor) { m_bwMonitor->recordEp2Bytes(pkt.size()); }

    // Shell-chrome sub-PR-2 B.1: record egress bytes for ▲ Mbps readout.
    recordBytesSent(static_cast<qint64>(pkt.size()));

    // Shell-chrome sub-PR-2 B.2: bracket C&C round-trip for ping RTT.
    // P1 EP2 → EP6 is the existing once-per-frame (380.95 pps) exchange.
    // We note send here; receive is noted in parseEp6Frame on the success path.
    notePingSent();
}

// ---------------------------------------------------------------------------
// sendPrimingBurst
//
// Port of Thetis networkproto1.c:134-139 ForceCandCFrame(count):
//   ForceCandCFrames(count, 2, prn->tx[0].frequency);  // TX bank
//   Sleep(10);
//   ForceCandCFrames(count, 4, prn->rx[0].frequency);  // RX1 bank
//   Sleep(10);
//
// Thetis calls this with count=1 inside each retry of SendStartToMetis and
// with count=3 once at the top of MetisReadThreadMainLoop. We call it with
// count=3 before metis-start and count=3 after, matching the MetisReadThread
// invocation. The Sleep(10) gap between TX and RX bursts is preserved because
// some Hermes firmware revisions need a small idle gap between bank changes.
// ---------------------------------------------------------------------------
void P1RadioConnection::sendPrimingBurst(int countPerBank)
{
    for (int i = 0; i < countPerBank; ++i) {
        sendCommandFrame();
    }
    QThread::msleep(10);
    for (int i = 0; i < countPerBank; ++i) {
        sendCommandFrame();
    }
    QThread::msleep(10);
}

// ---------------------------------------------------------------------------
// parseEp6Frame (instance method)
//
// Calls the static parseEp6Frame helper and emits iqDataReceived for each
// receiver's interleaved I/Q samples.
// Source: networkproto1.c:319-415 MetisReadThreadMainLoop
// Upstream inline attribution preserved verbatim (networkproto1.c:335/353/354/355):
//   `// only cleared by getAndResetADC_Overload(), or'ed with existing state //[2.10.3.13]MW0LGE`
// ---------------------------------------------------------------------------
void P1RadioConnection::parseEp6Frame(const QByteArray& pkt)
{
    if (pkt.size() != 1032) { return; }

    std::vector<std::vector<float>> perRx;
    std::vector<float> micSamples;
    const auto* frame = reinterpret_cast<const quint8*>(pkt.constData());

    // Phase 3M-1c TX pump v3: extract mic16 byte zone alongside RX I/Q so
    // the network thread is the cadence source for TX (matches Thetis
    // network.c:655-666 [v2.10.3.13] which calls Inbound() for both rx and
    // mic in lockstep with EP6 frame arrival).
    std::vector<float>* micOut = (m_txMicSource != nullptr) ? &micSamples : nullptr;
    if (!P1RadioConnection::parseEp6Frame(frame, m_activeRxCount, perRx, micOut)) {
        if (!m_parseFailLogged) {
            m_parseFailLogged = true;
            qCWarning(lcConnection) << "P1: parseEp6Frame rejected frame;"
                                    << "activeRxCount=" << m_activeRxCount
                                    << "magic=" << QString::asprintf("%02X %02X %02X %02X",
                                                                     frame[0], frame[1], frame[2], frame[3])
                                    << "sync0=" << QString::asprintf("%02X %02X %02X",
                                                                     frame[8], frame[9], frame[10])
                                    << "sync1=" << QString::asprintf("%02X %02X %02X",
                                                                     frame[520], frame[521], frame[522]);
        }
        return;
    }

    // Cache each subframe's C0 byte for the mic_ptt extraction below.
    // ADC overflow does NOT live in C0 — per Thetis networkproto1.c:332-355
    // [v2.10.3.13] it lives in C1 bit 0 of case-0x00 frames and in
    // C1/C2/C3 bit 0 of case-0x20 frames.  See the C0-type switch lower
    // in this function.
    //
    // Phase 3P-E Task 2: C0 bit 7 = I2C response frame (HL2 only).
    // Source: mi0bot networkproto1.c:478-493 [@c26a8a4]
    const quint8 c0_sub0 = frame[11];
    const quint8 c0_sub1 = frame[523];

    // Check each subframe's C0 for I2C response marker (bit 7).
    // Source: mi0bot networkproto1.c:478-480 [@c26a8a4]
    for (int sub = 0; sub < 2; ++sub) {
        const int base = 8 + sub * 512;  // sync bytes at base+0..2, C&C at base+3..7
        const quint8 c0 = frame[base + 3];
        if (c0 & 0x80) {
            // I2C response frame: C1-C4 = read_data[0-3]
            // Source: mi0bot networkproto1.c:480-492 [@c26a8a4]
            parseI2cResponse(c0, frame[base + 4], frame[base + 5],
                             frame[base + 6], frame[base + 7]);
        }
    }

    // H.5: mic_ptt extraction — P1 status frame C0 bit 0 (PTT from radio).
    // From Thetis networkproto1.c:329 [v2.10.3.13]:
    //   prn->ptt_in = ControlBytesIn[0] & 0x1;
    // + console.cs:25426 [v2.10.3.13]:
    //   bool mic_ptt = (dotdashptt & 0x01) != 0; // PTT from radio
    //
    // C0 is ControlBytesIn[0]; bit 0 is ptt_in.  Both sub-frames carry the
    // same instantaneous PTT state; OR them to produce the frame-level value
    // (matches Thetis nativeGetDotDashPTT() which reads a single prn->ptt_in
    // accumulated across all sub-frame writes).
    //
    // Emitted unconditionally each frame: MoxController::onMicPttFromRadio
    // is idempotent, so repeated false→false calls are harmless.
    const bool micPtt = ((c0_sub0 & 0x01) != 0) || ((c0_sub1 & 0x01) != 0);
    emit micPttFromRadio(micPtt);

    // Phase 3P-H Task 4: PA telemetry — extract raw 16-bit ADC counts from
    // each subframe's C&C status bytes.  Per-board scaling lives in RadioModel
    // (console.cs computeAlexFwdPower / computeRefPower / convertToVolts /
    // convertToAmps), because bridge_volt / refvoltage / adc_cal_offset
    // depend on HardwareSpecific.Model.
    //
    // From Thetis networkproto1.c:332-356 [@501e3f5]
    //   case 0x00: // C0 0000 0000
    //       prn->adc[0].adc_overload = prn->adc[0].adc_overload || ControlBytesIn[1] & 0x01; // only cleared by getAndResetADC_Overload(), or'ed with existing state //[2.10.3.13]MW0LGE
    //   case 0x08: // C0 0000 1xxx
    //       prn->tx[0].exciter_power = ((C1 << 8) & 0xff00) | (C2 & 0xff);  // (AIN5) drive power
    //       prn->tx[0].fwd_power     = ((C3 << 8) & 0xff00) | (C4 & 0xff);  // (AIN1) PA coupler
    //   case 0x10: // C0 0001 0xxx
    //       prn->tx[0].rev_power     = ((C1 << 8) & 0xff00) | (C2 & 0xff);  // (AIN2) PA reverse power
    //       prn->user_adc0           = ((C3 << 8) & 0xff00) | (C4 & 0xff);  // AIN3 MKII PA Volts
    //   case 0x18: // C0 0001 1xxx
    //       prn->user_adc1           = ((C1 << 8) & 0xff00) | (C2 & 0xff);  // AIN4 MKII PA Amps
    //       prn->supply_volts        = ((C3 << 8) & 0xff00) | (C4 & 0xff);  // AIN6 Hermes Volts
    //   case 0x20: // C0 0010 0xxx
    //       prn->adc[0].adc_overload = prn->adc[0].adc_overload || ControlBytesIn[1] & 1;        // only cleared by getAndResetADC_Overload(), or'ed with existing state //[2.10.3.13]MW0LGE
    //       prn->adc[1].adc_overload = prn->adc[1].adc_overload || (ControlBytesIn[2] & 1) << 1; // only cleared by getAndResetADC_Overload(), or'ed with existing state //[2.10.3.13]MW0LGE
    //       prn->adc[2].adc_overload = prn->adc[2].adc_overload || (ControlBytesIn[3] & 1) << 2; // only cleared by getAndResetADC_Overload(), or'ed with existing state //[2.10.3.13]MW0LGE
    //
    // We accumulate the latest value of each field across this frame's two
    // subframes and emit one paTelemetryUpdated() with the latched values.
    // Last-writer-wins matches Thetis (which mutates prn->* in place each subframe).
    bool telemetryDirty = false;
    for (int sub = 0; sub < 2; ++sub) {
        const int base   = 8 + sub * 512;
        const quint8 c0  = frame[base + 3];
        const quint8 c1  = frame[base + 4];
        const quint8 c2  = frame[base + 5];
        const quint8 c3  = frame[base + 6];
        const quint8 c4  = frame[base + 7];
        if (c0 & 0x80) {
            // I2C response — already handled above; not a telemetry frame.
            continue;
        }
        // Source: networkproto1.c:332 — switch (ControlBytesIn[0] & 0xf8)
        // Cases 0x00/0x20 carry ADC-overload bits (one per ADC); the
        // `//[2.10.3.13]MW0LGE only cleared by getAndResetADC_Overload(),
        // or'ed with existing state` inline attributions are preserved
        // verbatim within each case body below.  In NereusSDR the SAC
        // hysteresis state machine (StepAttenuatorController) plays the
        // role of `getAndResetADC_Overload()` — it OR-accumulates every
        // adcOverflow() emission until its 100 ms tick consumes them.
        switch (c0 & 0xF8) {
        case 0x00: {
            // Issue #176 fix — ADC0 overload reported in C1 bit 0.
            // From Thetis networkproto1.c:335 [v2.10.3.13]:
            //   prn->adc[0].adc_overload = prn->adc[0].adc_overload || ControlBytesIn[1] & 0x01;
            //   // only cleared by getAndResetADC_Overload(), or'ed with existing state //[2.10.3.13]MW0LGE
            observeAdcOverloads(0x01, c1);
            if (c1 & 0x01) {
                emit adcOverflow(0);
            }
            // Task 13: the user digital inputs, which carry the TX inhibit
            // input TxInhibitMonitor reads.
            // From Thetis networkproto1.c:336 [v2.10.3.15]:
            //   prn->user_dig_in = ((ControlBytesIn[1] >> 1) & 0xf);
            // (networkproto1.c:335 above it: //[2.10.3.13]MW0LGE)
            reportUserDigitalInputs(static_cast<quint8>((c1 >> 1) & 0x0f));
            break;
        }
        case 0x08: {
            // From Thetis networkproto1.c:339 [@501e3f5] — exciter_power AIN5
            const quint16 exciter = static_cast<quint16>((c1 << 8) | c2);
            // From Thetis networkproto1.c:340 [@501e3f5] — fwd_power AIN1
            const quint16 fwd     = static_cast<quint16>((c3 << 8) | c4);
            m_paExciterRaw = exciter;
            m_paFwdRaw     = fwd;
            telemetryDirty = true;
            break;
        }
        case 0x10: {
            // From Thetis networkproto1.c:344 [@501e3f5] — rev_power AIN2
            const quint16 rev      = static_cast<quint16>((c1 << 8) | c2);
            // From Thetis networkproto1.c:346 [@501e3f5] — user_adc0 AIN3 MKII PA Volts
            const quint16 userAdc0 = static_cast<quint16>((c3 << 8) | c4);
            m_paRevRaw      = rev;
            m_paUserAdc0Raw = userAdc0;
            telemetryDirty  = true;
            // Shell-chrome sub-PR-2 B.3: emit userAdc0Changed for MKII-class boards.
            // Matches gate in RadioModel scalePaVolts() [RadioModel.cpp:402-417].
            // From Thetis networkproto1.c:346 [@501e3f5] — user_adc0 AIN3 MKII PA Volts
            switch (m_hardwareProfile.model) {
            case HPSDRModel::ORIONMKII:
            case HPSDRModel::ANAN8000D:
            case HPSDRModel::ANAN7000D:
            case HPSDRModel::ANAN_G2E: //N1GP G2E added [Thetis console.cs:25007 v2.10.3.15 grouping]
            case HPSDRModel::ANAN_G2:
            case HPSDRModel::ANAN_G2_1K:
            case HPSDRModel::ANVELINAPRO3:
                handleUserAdc0Raw(userAdc0);
                break;
            default:
                break;
            }
            break;
        }
        case 0x18: {
            // From Thetis networkproto1.c:349 [@501e3f5] — user_adc1 AIN4 MKII PA Amps
            // Upstream cite :353/:354/:355 (case 0x20) carry the MW0LGE ADC
            // overload comments — see comment block above the switch.
            const quint16 userAdc1 = static_cast<quint16>((c1 << 8) | c2);
            // From Thetis networkproto1.c:350 [@501e3f5] — supply_volts AIN6 Hermes Volts //[2.10.3.13]MW0LGE (nearby upstream cases)
            const quint16 supply   = static_cast<quint16>((c3 << 8) | c4);
            m_paUserAdc1Raw = userAdc1;
            m_paSupplyRaw   = supply;
            telemetryDirty  = true;
            // Shell-chrome sub-PR-2 B.3: emit supplyVoltsChanged for all radios.
            // From Thetis networkproto1.c:350 [@501e3f5] — supply_volts AIN6 Hermes Volts //[2.10.3.13]MW0LGE
            handleSupplyRaw(supply);
            break;
        }
        case 0x20: {
            // Issue #176 fix — multi-ADC overload report (Orion / ANAN-8000 class).
            // From Thetis networkproto1.c:353-355 [v2.10.3.13]:
            //   prn->adc[0].adc_overload = prn->adc[0].adc_overload || ControlBytesIn[1] & 1;        // only cleared by getAndResetADC_Overload(), or'ed with existing state //[2.10.3.13]MW0LGE
            //   prn->adc[1].adc_overload = prn->adc[1].adc_overload || (ControlBytesIn[2] & 1) << 1; // only cleared by getAndResetADC_Overload(), or'ed with existing state //[2.10.3.13]MW0LGE
            //   prn->adc[2].adc_overload = prn->adc[2].adc_overload || (ControlBytesIn[3] & 1) << 2; // only cleared by getAndResetADC_Overload(), or'ed with existing state //[2.10.3.13]MW0LGE
            observeAdcOverloads(0x07, static_cast<quint8>((c1 & 0x01)
                                | ((c2 & 0x01) << 1) | ((c3 & 0x01) << 2)));
            if (c1 & 0x01) { emit adcOverflow(0); }
            if (c2 & 0x01) { emit adcOverflow(1); }
            if (c3 & 0x01) { emit adcOverflow(2); }
            break;
        }
        default:
            break;
        }
    }
    if (telemetryDirty) {
        emit paTelemetryUpdated(m_paFwdRaw, m_paRevRaw, m_paExciterRaw,
                                m_paUserAdc0Raw, m_paUserAdc1Raw, m_paSupplyRaw);
    }

    // Shell-chrome sub-PR-2 B.2: complete the ping RTT measurement.
    // Each valid EP6 frame constitutes the inbound leg of the EP2→EP6 exchange.
    notePingReceived();

    // Materialize per-RX QVectors once (used by both the paired-emit and
    // the per-RX loop below).  Empty entries stay empty — same gate as the
    // legacy emit loop.
    std::vector<QVector<float>> perRxVecs(perRx.size());
    for (int r = 0; r < static_cast<int>(perRx.size()); ++r) {
        if (!perRx[static_cast<size_t>(r)].empty()) {
            perRxVecs[static_cast<size_t>(r)] = QVector<float>(
                perRx[static_cast<size_t>(r)].begin(),
                perRx[static_cast<size_t>(r)].end());
        }
    }

    // Phase 3M-4 bench-fix 2026-05-23 (J.J. Boyd KG4VCF): source-first PS
    // pairing per Thetis sync.c:53-58 InboundBlock(id=1) [v2.10.3.15] +
    // mi0bot networkproto1.c:549-553 [v2.10.3.13-beta2] HL2 case 4
    // (twist(spr, 2, 3, 1)).
    //
    // When this EP6 frame carries both PS DDC slots (latched from
    // applyPsDdcConfig), emit them as a single paired signal so PsccPump
    // can call pscc() once with both buffers from the SAME deinterleave
    // pass — no host-side ring buffering, no cross-stream drift.
    //
    // Issued BEFORE the per-RX iqDataReceived loop so the order of
    // observation matches Thetis: ChannelMaster fires the PS-paired
    // call (xrouter case 2 → InboundBlock(1)) on the same frame that
    // feeds the regular per-RX consumers.
    const bool psPairPresent =
        m_psFbDdc >= 0 && m_psTxMonDdc >= 0
        && m_psFbDdc < static_cast<int>(perRxVecs.size())
        && m_psTxMonDdc < static_cast<int>(perRxVecs.size())
        && !perRxVecs[m_psFbDdc].isEmpty()
        && !perRxVecs[m_psTxMonDdc].isEmpty();

    // PS stream diagnostic. Added 2026-08-01 (J.J. Boyd, KG4VCF) because
    // PureSignal parks in LCOLLECT on a live HL2 and the state alone cannot
    // say why: LCOLLECT bins by TX magnitude (calcc.c:733-756) and needs all
    // 16 bins filled, so a flat envelope and a dead stream look identical
    // from outside. The peaks distinguish them:
    //
    //   tx peak near zero          TX monitor is not carrying the drive
    //   tx peak steady, non-zero   constant envelope, PS cannot calibrate
    //                              on this signal (a TUNE carrier does this)
    //   tx peak varying            envelope is fine, look further downstream
    //   rx peak near zero          no feedback reaching DDC2 (coupler,
    //                              attenuation, or ADC steering)
    //
    // Nothing on this path logged anything, so a whole bench session
    // produced no evidence beyond "state=4". Once per second, and only while
    // the PS pair is latched, so it costs nothing in normal RX.
    // Gated on MOX, not merely on the PS indices being non-negative.
    //
    // The indices alone used to mean nothing at all: PsDdcConfig defaulted
    // psFbDdc=0 / txMonDdc=1, and no branch of applyPureSignalDdcConfig
    // outside PS-MOX assigns them, so "has a PS pair" was true with
    // PureSignal switched off entirely. An earlier revision of this
    // diagnostic keyed on that and measured itself running at 201,400
    // samples per second on a quiet receiver; the paired emit below shared
    // the same open gate. Fixed 2026-08-01 by defaulting both indices to -1
    // in CodecContext.h, so the gate now tracks the PS pair the codec
    // actually configured.
    if (m_mox && m_psFbDdc >= 0 && m_psTxMonDdc >= 0) {
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        if (nowMs - m_psDiagLastMs >= 1000) {
            m_psDiagLastMs = nowMs;
            if (!psPairPresent) {
                qCInfo(lcConnection).nospace()
                    << "PS streams: pair NOT emitted -- fb=" << m_psFbDdc
                    << " txMon=" << m_psTxMonDdc
                    << " slotsInFrame=" << int(perRxVecs.size())
                    << " fbEmpty=" << (m_psFbDdc < int(perRxVecs.size())
                                       ? perRxVecs[m_psFbDdc].isEmpty() : true)
                    << " txMonEmpty=" << (m_psTxMonDdc < int(perRxVecs.size())
                                       ? perRxVecs[m_psTxMonDdc].isEmpty() : true);
            } else {
                // The ENVELOPE, sample by sample, not the peak of the raw
                // floats. LCOLLECT bins on env = sqrt(I^2 + Q^2) of the TX
                // reference and needs all 16 bins filled (calcc.c:733-765).
                //
                // A peak alone cannot answer whether that happens: a
                // two-tone's envelope peak is constant by construction, so
                // the earlier probe read a rock-steady 0.230 whether the
                // envelope was sweeping 0 to peak or sitting flat at peak.
                // Only the spread distinguishes them.
                //
                //   min near 0, bins near 16   envelope sweeps, LCOLLECT
                //                              should complete
                //   min near max, bins 1 or 2  envelope is flat, so only
                //                              those bins ever fill and
                //                              full_ints resets every 4 s
                //
                // `bins` counts distinct bins this one block would touch,
                // computed exactly as LCOLLECT does, using the HL2's
                // hwPeak (BoardCapabilities psDefaultPeak 0.233, confirmed
                // reaching the engine via getPSHWPeak).
                const auto envStats = [](const QVector<float>& v,
                                         double hwScale, int ints) {
                    struct { double mn; double mx; double mean; int bins; } r
                        {1e9, 0.0, 0.0, 0};
                    QSet<int> touched;
                    const int n = v.size() / 2;
                    if (n <= 0) { r.mn = 0.0; return r; }
                    for (int i = 0; i < n; ++i) {
                        const double re = v[2 * i];
                        const double im = v[2 * i + 1];
                        const double e  = std::sqrt(re * re + im * im);
                        if (e < r.mn) { r.mn = e; }
                        if (e > r.mx) { r.mx = e; }
                        r.mean += e;
                        const double scaled = e * hwScale;
                        if (scaled <= 1.0) {
                            touched.insert(int(scaled * double(ints)));
                        }
                    }
                    r.mean /= double(n);
                    r.bins = touched.size();
                    return r;
                };
                // 0.233 is the HL2 psDefaultPeak; hw_scale is its reciprocal
                // (calcc.c:1049). Hard-coded here rather than plumbed from
                // BoardCapabilities because this is a diagnostic, and the
                // engine-side value is already logged by PureSignal.
                constexpr double kHwScale = 1.0 / 0.233;
                const auto tx = envStats(perRxVecs[m_psTxMonDdc], kHwScale, 16);
                const auto fb = envStats(perRxVecs[m_psFbDdc], kHwScale, 16);
                qCInfo(lcConnection).nospace()
                    << "PS env: txMon(DDC" << m_psTxMonDdc << ") min=" << tx.mn
                    << " max=" << tx.mx << " mean=" << tx.mean
                    << " bins=" << tx.bins << "/16"
                    << "  fb(DDC" << m_psFbDdc << ") min=" << fb.mn
                    << " max=" << fb.mx << " mean=" << fb.mean
                    << "  samples=" << perRxVecs[m_psTxMonDdc].size() / 2;
            }
        }
    }

    if (psPairPresent) {
        emit psPairedIqDataReceived(m_psFbDdc,    perRxVecs[m_psFbDdc],
                                    m_psTxMonDdc, perRxVecs[m_psTxMonDdc]);
    }

    // Emit iqDataReceived for each receiver
    // Contract: hwReceiverIndex (0-based), interleaved float I/Q pairs, [-1, 1]
    // Declared by our own RadioConnection.h (iqDataReceived signal). Was
    // written using `// Source: <file>:<line>` cite grammar, which the
    // author-tag verifier reads as an upstream Thetis cite; it is an
    // internal cross-reference, and the line number had drifted anyway.
    for (int r = 0; r < static_cast<int>(perRxVecs.size()); ++r) {
        if (!perRxVecs[static_cast<size_t>(r)].isEmpty()) {
            if (!m_firstEmitLogged) {
                m_firstEmitLogged = true;
                qCInfo(lcConnection) << "P1: first iqDataReceived emit; rx=" << r
                                     << "samples=" << perRxVecs[static_cast<size_t>(r)].size();
            }
            emit iqDataReceived(r, perRxVecs[static_cast<size_t>(r)]);
        }
    }

    // Phase 3M-1c TX pump v3: dispatch the extracted mic16 samples to
    // TxMicSource as the cadence source for TxWorkerThread.
    //
    // The radio embeds one mic sample per I/Q sample group in EP6 frames,
    // so mic arrives at the operating sample rate (192 kHz at sampleRate=
    // 192000).  TxMicSource and TxChannel WDSP both expect 48 kHz mic, so
    // we decimate by mic_decimation_factor before dispatch.
    //
    // Mirrors Thetis networkproto1.c:391-410 [v2.10.3.14] (the loop body
    // that gates each Inbound() call by mic_decimation_count).  Without
    // this, at 192 kHz the worker semaphore-wakes 4× per block (3000/sec
    // instead of 750/sec), CPU thrashes, the SPSC TX I/Q ring overflows
    // ~3000/sec during MOX-on, and the EP2 pacer is starved into bursty
    // delivery — observed as rapid HL2 T/R relay clicking on bench.
    if (m_txMicSource != nullptr && !micSamples.empty()) {
        std::vector<float> decimated;
        decimateMicSamples(micSamples.data(), static_cast<int>(micSamples.size()),
                           m_micDecimationFactor, m_micDecimationCount, decimated);
        if (!decimated.empty()) {
            m_txMicSource->inbound(decimated.data(),
                                   static_cast<int>(decimated.size()));
            m_lastMicAt = QDateTime::currentDateTimeUtc();
        }
    }
}

// ---------------------------------------------------------------------------
// composeCcForBankLegacy — pre-refactor compose path (kept for rollback hatch)
//
// Identical to the original composeCcForBank body. Preserved for one release
// cycle until Task 16 drops the dual-call diagnostic. Do not edit.
// Ported from Thetis networkproto1.c WriteMainLoop cases 0-17.
// ---------------------------------------------------------------------------
void P1RadioConnection::composeCcForBankLegacy(int bankIdx, quint8 out[5]) const
{
    memset(out, 0, 5);
    const quint8 C0base = m_mox ? 0x01 : 0x00;

    switch (bankIdx) {
    case 0: // General settings (networkproto1.c:450-471)
        composeCcBank0Full(out);
        return;

    case 1: // TX VFO (networkproto1.c:476-482)
        composeCcBankTxFreq(out, wireFrequencyHz(m_txFreqHz));
        return;

    case 2: { // RX1 VFO DDC0 (networkproto1.c:484-494)
        // Phase 3M-4 Task 17 P1 follow-up: PureSignal DDC0 freq override
        // for HermesII / ANAN10E / ANAN100B (nddc==2 boards).
        //
        // From mi0bot ChannelMaster/networkproto1.c:982-993 [v2.10.3.13-beta2]
        // (byte-for-byte identical to ramdor :484-494 [v2.10.3.13]):
        //   case 2: //RX1 VFO (DDC0) 0x02
        //       C0 |= 4;
        //       // DDC0 is always RX0 freqency, except if Puresignal TX with hermes-II
        //       if ((nddc == 2) && (XmitBit == 1) && (prn->puresignal_run))
        //           ddc_freq = prn->tx[0].frequency;
        //       else
        //           ddc_freq = prn->rx[0].frequency;
        //
        // NereusSDR mapping:
        //   nddc                    ≡  m_psNDdc (2 by default; 4 once HL2/Hermes
        //                              codec config arrives — disabling override
        //                              for those boards, correct per source).
        //   XmitBit == 1            ≡  m_mox
        //   prn->puresignal_run     ≡  m_puresignalRun
        //   prn->tx[0].frequency    ≡  m_txFreqHz
        //   prn->rx[0].frequency    ≡  m_rxFreqHz[0]
        //
        // For nddc==4 boards (HL2 / Hermes / ANAN10 / ANAN100), the firmware
        // handles DDC0/DDC1 routing internally via cntrl1=4 (ADC-to-DDC
        // steering set in P1CodecHl2::applyPureSignalDdcConfig from mi0bot
        // console.cs:8486 [v2.10.3.13-beta2]); no host-side freq override
        // is needed, hence the gate (m_psNDdc == 2).
        const quint64 freq = (m_psNDdc == 2 && m_mox && m_puresignalRun)
                           ? m_txFreqHz
                           : m_rxFreqHz[0];
        composeCcBankRxFreq(out, 0, wireFrequencyHz(freq));
        return;
    }

    case 3: { // RX2 VFO DDC1 (networkproto1.c:497-511)
        // Phase 3M-4 Task 17 P1 follow-up: PureSignal DDC1 freq override
        // for nddc==2 boards (mirrors the case 2 logic above).
        //
        // From mi0bot ChannelMaster/networkproto1.c:995-1009 [v2.10.3.13-beta2]
        // (byte-for-byte identical to ramdor :497-511 [v2.10.3.13]):
        //   case 3: //RX2 VFO (DDC1) 0x03
        //       C0 |= 6;
        //       // DDC1 is TX freq if Hermes-II && TX && Puresignal;
        //       // RX1 freq if Orion;
        //       // else RX2 freq if Hermes
        //       if ((nddc == 2) && (XmitBit == 1) && (prn->puresignal_run))
        //           ddc_freq = prn->tx[0].frequency;
        //       else if (nddc == 5)
        //           ddc_freq = prn->rx[0].frequency;
        //       else
        //           ddc_freq = prn->rx[1].frequency; //Hermes RX2 freq
        //
        // The nddc==5 branch (Orion-class P1 — uses P1 wire dialect with
        // 5-DDC config; very rare) selects rx[0].frequency to mirror the
        // single-VFO panadapter mode.  P1RadioConnection has m_rxFreqHz[1]
        // set by upstream callers; m_rxFreqHz[0] is the same value the
        // bank-2 path uses.  Default Hermes path uses m_rxFreqHz[1] for
        // RX2 VFO.
        quint64 freq;
        if (m_psNDdc == 2 && m_mox && m_puresignalRun) {
            freq = m_txFreqHz;            // Hermes-II PS override
        } else if (m_psNDdc == 5) {
            freq = m_rxFreqHz[0];         // Orion: DDC1 = RX1 freq
        } else {
            freq = m_rxFreqHz[1];         // Hermes (default): RX2 VFO
        }
        composeCcBankRxFreq(out, 1, wireFrequencyHz(freq));
        return;
    }

    case 4: // ADC assignments + TX ATT (networkproto1.c:517-523)
        out[0] = C0base | 0x1C;
        out[1] = static_cast<quint8>(m_adcCtrl & 0xFF);
        out[2] = static_cast<quint8>((m_adcCtrl >> 8) & 0x3F);
        out[3] = static_cast<quint8>(m_txStepAttn & 0x1F);
        out[4] = 0;
        return;

    case 5: case 6: case 7: case 8: case 9: {
        // RX3-RX7 VFOs (networkproto1.c:525-575)
        // Unused DDCs get TX freq as a safe default.
        int rxIdx = bankIdx - 3; // bank 5 → rxIdx 2, bank 9 → rxIdx 6
        composeCcBankRxFreq(out, rxIdx, wireFrequencyHz(m_txFreqHz));
        return;
    }

    case 10: // TX drive, mic, Alex HPF/LPF, T/R relay (networkproto1.c:578-591)
        // C3 bit 7 (0x80) = Alex T/R relay DISABLED.  Inverted vs MOX.
        // Write 0x80 only when relay should be disengaged (PA bypass / RX-only).
        // m_trxRelay: true  = relay engaged (normal TX path) → bit 7 = 0;
        //             false = relay disengaged (PA bypass)   → bit 7 = 1.
        // From deskhpsdr/src/old_protocol.c:2909-2910 [@120188f]:
        //   if (txband->disablePA || !pa_enabled)
        //       output_buffer[C3] |= 0x80; // disable Alex T/R relay
        // BUG FIX (3M-1a E.4): prior code wrote (m_paEnabled ? 0x80 : 0), which
        // is INVERTED — it asserted "disable" when PA was enabled.  Latent from
        // 3M-0; never surfaced because no TX I/Q was live on EP2 before E.4.
        out[0] = C0base | 0x12;
        out[1] = static_cast<quint8>(m_txDrive & 0xFF);
        // C2: mic_boost → bit 0 (0x01); line_in → bit 1 (0x02); bit 6 always set per upstream default.
        // From Thetis ChannelMaster/networkproto1.c:581 [v2.10.3.13]
        //   C2 = ((prn->mic.mic_boost & 1) | ((prn->mic.line_in & 1) << 1) | ... | 0b01000000) & 0x7f;
        out[2] = static_cast<quint8>((m_micBoost ? 0x01 : 0x00) | (m_lineIn ? 0x02 : 0x00) | 0x40); // 3M-1b G.1+G.2
        // Bit 7 is also Thetis's DisablePA bit ("Disable HF PA",
        // networkproto1.c:586 [v2.10.3.15]: ((prn->tx[0].pa & 1) << 7)).
        out[3] = effectiveAlexHpfBits()
               | ((!m_trxRelay || m_paDisabled) ? 0x80 : 0x00); // 3M-1a E.4
        // C4 is the Alex0 low-pass word: transmit selection while keyed,
        // receive selection while not (networkproto1.c:587-590 +
        // netInterface.c:705-717 [v2.10.3.15]).
        out[4] = effectiveAlexLpfBits();
        return;

    case 11: // Preamp control (networkproto1.c:593-601)
        out[0] = C0base | 0x14;
        // C1: preamp bits 0-3 (bit 3 = rx0 again, Thetis quirk) + mic_trs bit 4
        //     + mic_bias bit 5 + mic_ptt bit 6.
        // mic_trs polarity inversion: 1 = tip is BIAS/PTT → write !m_micTipRing.
        // mic_bias polarity: 1 = bias on (no inversion) → write m_micBias.
        // mic_ptt polarity: direct → write m_micPTTDisabled (Thetis convention,
        // bit set = PTT disabled at firmware). Matches the codec path; both
        // ramped to direct in the issue #182 follow-up.
        // From Thetis ChannelMaster/networkproto1.c:597-598 [v2.10.3.13+501e3f51]
        //   C1 = ... | ((prn->mic.mic_trs & 1) << 4) | ((prn->mic.mic_bias & 1) << 5)
        //           | ((prn->mic.mic_ptt & 1) << 6);
        // From Thetis console.cs:19764 [v2.10.3.13+501e3f51]:
        //   NetworkIO.SetMicPTT(Convert.ToInt32(mic_ptt_disabled));
        out[1] = static_cast<quint8>(
                   (m_rxPreamp[0] ? 0x01 : 0)
                 | (m_rxPreamp[1] ? 0x02 : 0)
                 | (m_rxPreamp[2] ? 0x04 : 0)
                 | (m_rxPreamp[0] ? 0x08 : 0)            // bit3 = rx0 again (Thetis quirk)
                 | (!m_micTipRing      ? 0x10 : 0x00)    // 3M-1b G.3 — mic_trs (inverted)
                 | (m_micBias          ? 0x20 : 0x00)    // 3M-1b G.4 — mic_bias (no inversion)
                 | (m_micPTTDisabled   ? 0x40 : 0x00));  // 3M-1b G.5 — mic_ptt (direct, issue #182)
        out[2] = 0; // line_in_gain + puresignal
        out[3] = 0; // user digital outputs
        out[4] = static_cast<quint8>((m_stepAttn[0] & 0x1F) | 0x20); // ADC0 step ATT + enable
        return;

    case 12: { // Step ATT ADC1/2, CW keyer (networkproto1.c:604-628)
        out[0] = C0base | 0x16;
        // RedPitaya-specific: don't force 31dB on ADC1 during TX
        // From networkproto1.c:606-616 (DH1KLM fix)
        if (m_mox && m_hardwareProfile.model != HPSDRModel::REDPITAYA) {
            out[1] = 0x1F;
        } else if (m_hardwareProfile.model == HPSDRModel::REDPITAYA) {
            out[1] = static_cast<quint8>(m_stepAttn[1] & 0x1F);
        } else {
            out[1] = static_cast<quint8>(m_stepAttn[1] & 0xFF);
        }
        out[1] |= 0x20; // enable bit
        out[2] = static_cast<quint8>((m_stepAttn[2] & 0x1F) | 0x20);
        out[3] = 0; // CW keyer defaults
        out[4] = 0;
        return;
    }

    case 13: // CW enable (networkproto1.c:633-639)
        out[0] = C0base | 0x1E;
        out[1] = 0; out[2] = 0; out[3] = 0; out[4] = 0;
        return;

    case 14: // CW hang/sidetone (networkproto1.c:641-646)
        out[0] = C0base | 0x20;
        out[1] = 0; out[2] = 0; out[3] = 0; out[4] = 0;
        return;

    case 15: // EER PWM (networkproto1.c:649-654)
        out[0] = C0base | 0x22;
        out[1] = 0; out[2] = 0; out[3] = 0; out[4] = 0;
        return;

    case 16: // BPF2 (networkproto1.c:657-665)
        out[0] = C0base | 0x24;
        out[1] = 0; out[2] = 0; out[3] = 0; out[4] = 0;
        return;

    case 17: // AnvelinaPro3 extra OC (networkproto1.c:668-673)
        out[0] = C0base | 0x26;
        out[1] = 0; out[2] = 0; out[3] = 0; out[4] = 0;
        return;

    default:
        return;
    }
}

// ---------------------------------------------------------------------------
// composeCcForBank — dispatcher for all 18 C&C banks (0-17)
//
// Phase 3P-A Task 12: delegates to per-board IP1Codec subclass chosen at
// applyBoardQuirks() time. Falls back to legacy path when:
//   - NEREUS_USE_LEGACY_P1_CODEC=1 env var is set, or
//   - m_codec is null (pre-connect).
//
// Regression-freeze gate (Task 16) proved codec and legacy agree byte-for-byte
// on all non-HL2 boards. Dual-call diagnostic dropped. Legacy path still
// reachable via NEREUS_USE_LEGACY_P1_CODEC=1 env var until Phase B merges.
// ---------------------------------------------------------------------------
void P1RadioConnection::composeCcForBank(int bankIdx, quint8 out[5]) const
{
    if (composeI2cFrame(out)) {
        return;  // I2C frame written; skip normal bank compose
    }
    composeBank(bankIdx, out);
}

bool P1RadioConnection::composeI2cFrame(quint8 out[5]) const
{
    if (m_useLegacyCodec || !m_codec) {
        return false;
    }
    // Phase 3P-E Task 2: HL2 I2C intercept — when IoBoardHl2 has pending
    // I2C transactions, the next C&C frame carries I2C TLV bytes instead of
    // the normal bank payload.
    // Source: mi0bot networkproto1.c:898-943 [@c26a8a4]
    if (m_codec->usesI2cIntercept()) {
        // const_cast: tryComposeI2cFrame mutates the queue (dequeues txn),
        // but composeCcForBank is const because the rest of compose is pure.
        // Documented exception: only the I2C queue pointer is mutated, not
        // the codec or connection state itself.
        auto* hl2Codec = const_cast<P1CodecHl2*>(
            dynamic_cast<const P1CodecHl2*>(m_codec.get()));
        if (hl2Codec && hl2Codec->tryComposeI2cFrame(out, m_mox)) {
            return true;
        }
    }
    return false;
}

void P1RadioConnection::composeBank(int bankIdx, quint8 out[5]) const
{
    if (m_useLegacyCodec || !m_codec) {
        composeCcForBankLegacy(bankIdx, out);
        if (bankIdx == 0) {
            publishBank0BandOutputs(out);
        }
        return;
    }

    const CodecContext ctx = buildCodecContext();
    m_codec->composeCcForBank(bankIdx, ctx, out);
    if (bankIdx == 0) {
        publishBank0BandOutputs(out);
    }
}

// ---------------------------------------------------------------------------
// publishBank0BandOutputs: plan Task 14 fix wave (R-R3-49). Bank 0 C2 carries
// the band outputs, (ocByte << 1) & 0xFE, so the byte read back from the
// composed bank is the one the radio gets, on the codec and the rollback
// compose alike. Every window shows it (RadioModel::bandOutputsByte), as
// Thetis's LED strip shows the bits UpdateExtCtrl returned
// (console.cs:29106-29107 [v2.10.3.15]).
// ---------------------------------------------------------------------------
void P1RadioConnection::publishBank0BandOutputs(const quint8 bank0[5]) const
{
    publishBandOutputs(static_cast<quint8>(bank0[2] >> 1),
                       int(bandFromFrequency(static_cast<double>(ocBandFrequencyHz()))),
                       m_mox);
}

// ---------------------------------------------------------------------------
// composeCcBank0Full — instance-level bank 0 using actual state
//
// Source: Thetis networkproto1.c:450-471 (WriteMainLoop case 0)
// ---------------------------------------------------------------------------
void P1RadioConnection::composeCcBank0Full(quint8 out[5]) const
{
    // C0: MOX bit (networkproto1.c:446)
    out[0] = m_mox ? 0x01 : 0x00;

    // C1: sample rate (networkproto1.c:451)
    quint8 srBits = 0;
    if      (m_sampleRate >= 384000) { srBits = 3; }
    else if (m_sampleRate >= 192000) { srBits = 2; }
    else if (m_sampleRate >= 96000)  { srBits = 1; }
    out[1] = srBits & 0x03;

    // C2: OC outputs (networkproto1.c:452)
    out[2] = (m_ocOutput << 1) & 0xFE;

    // C3: preamp, dither, random, RX input (networkproto1.c:453-461)
    out[3] = (m_rxPreamp[0] ? 0x04 : 0)
           | (m_dither[0]   ? 0x08 : 0)
           | (m_random[0]   ? 0x10 : 0);
    // RX input select: default Rx_1_In (networkproto1.c:458)
    out[3] |= 0x20;
    // Level Cal: the Alex attenuator (networkproto1.c:453 [v2.10.3.15]).
    out[3] |= (m_alex10dB ? 0x01 : 0) | (m_alex20dB ? 0x02 : 0);

    // C4: antenna, duplex, NDDCs, diversity (networkproto1.c:463-471)
    out[4] = static_cast<quint8>(m_antennaIdx & 0x03);
    out[4] |= 0x04; // duplex bit
    int nddc = (m_activeRxCount < 1) ? 1 : (m_activeRxCount > 7 ? 7 : m_activeRxCount);
    out[4] |= static_cast<quint8>((nddc - 1) << 3);
    out[4] |= (m_diversity ? 0x80 : 0);
}

void P1RadioConnection::composeCcBank0(quint8*) { /* full implementation in Task 7 static helpers */ }
void P1RadioConnection::composeCcBank1(quint8*) { /* Task 7 */ }
void P1RadioConnection::composeCcBank2(quint8*) { /* Task 7 */ }
void P1RadioConnection::composeCcBank3(quint8*) { /* Task 7 */ }
void P1RadioConnection::composeCcTxFreq(quint8*)  { /* Task 7 */ }
void P1RadioConnection::composeCcAlexRx(quint8*)  { /* Task 7 */ }
void P1RadioConnection::composeCcAlexTx(quint8*)  { /* Task 7 */ }
void P1RadioConnection::composeCcOcOutputs(quint8*) { /* Task 7 */ }

void P1RadioConnection::hl2SendIoBoardTlv(const QByteArray&) { /* internal TLV helper — used by hl2SendIoBoardInit */ }
void P1RadioConnection::checkFirmwareMinimum(int)  { /* superseded by connectToRadio firmware check (Task 11) */ }

// ---------------------------------------------------------------------------
// hl2SendIoBoardInit
//
// Called from connectToRadio() after applyBoardQuirks() when
// m_caps->hasIoBoardHl2 is true; also reachable from the user-initiated
// "Probe" button on Setup → Hardware → HL2 I/O Board.
//
// Enqueues 3 I2C read transactions on the IoBoardHl2 queue:
//   1. bus=1, addr=0x41, reg=0   → HW version   (sets m_hardwareVersion +
//                                                setDetected if version==0xF1)
//   2. bus=1, addr=0x1d, reg=9   → FW major     (REG_FIRMWARE_MAJOR)
//   3. bus=1, addr=0x1d, reg=10  → FW minor     (REG_FIRMWARE_MINOR)
//
// The wire encoder (P1CodecHl2::tryComposeI2cFrame) drains the queue one
// transaction per ep2 frame, pushing each read into IoBoardHl2's pending-read
// FIFO. Inbound responses (parseI2cResponse → applyI2cReadResponse) pop the
// oldest pending-read and steer the bytes to the right destination
// (hardwareVersion or m_registers[]).
//
// Source: mi0bot IoBoardHl2.cs:129-145 readRequest() [@c26a8a4]
//         mi0bot ChannelMaster/netInterface.c:1471-1499 I2CReadInitiate [@c26a8a4]
// ---------------------------------------------------------------------------
void P1RadioConnection::requestIoBoardProbe()
{
    hl2SendIoBoardInit();
}

void P1RadioConnection::hl2SendIoBoardInit()
{
    if (!m_caps || !m_caps->hasIoBoardHl2) { return; }
    if (!m_ioBoard) {
        qCWarning(lcConnection) << "HL2: I/O board init — m_ioBoard not wired; skipping probe";
        return;
    }

    // mi0bot enforces ONE outstanding read at a time.  I2CReadInitiate refuses
    // when in_index != out_index (netInterface.c:1478), and the C# driver
    // busy-waits for each response before issuing the next (console.cs:25796-
    // 25808).  We mirror that protocol via a step machine driven off the
    // IoBoardHl2::i2cReadAnswered signal.  Source: [@c26a8a4]
    //
    // How the radio answers (mi0bot networkproto1.c:478-493 [@c26a8a4]): an
    // EP6 frame whose C0 has bit 7 set carries the I2C answer, C1..C4 into
    // read_data[0..3].  mi0bot never reads C0's address bits: the answer is
    // the one outstanding read's, because there is only ever one.  Its
    // `returned_address`, which the error test at networkproto1.c:480
    // compares with 0x3f, is never set from the frame (only zeroed,
    // netInterface.c:1623).  The request's C0 named the bus's I2C
    // controller, (0x3d << 1) for bus 1 (networkproto1.c:912-919), not the
    // device, so matching the answer's C0 against the device address could
    // stall the probe on real firmware.  The answer is matched to its read
    // by the pending-read record it popped instead.
    m_hl2ProbeStep = Hl2ProbeStep::Idle;
    if (!m_hl2ProbeWired) {
        // Gate the step machine on (deviceAddr, register) of the read the
        // answer popped matching the expected pair for the current step.
        // Without this gate, unrelated I2C reads (e.g. from the
        // Hl2OptionsTab manual R/W tool, or NEREUS_HL2_I2C_SCAN traffic)
        // would advance the probe out of order (false aborts or
        // misleading "init complete" before the intended registers were
        // probed).  Codex P2 on PR #157.
        connect(m_ioBoard, &IoBoardHl2::i2cReadAnswered, this,
                [this](quint8 deviceAddr, quint8 subAddr,
                       quint8, quint8, quint8, quint8) {
                    hl2ProbeAdvance(deviceAddr, subAddr);
                });
        m_hl2ProbeWired = true;
    }
    // Initial dispatch: bootstrap from Idle.  No retAddr/retSubAddr
    // because no read has fired yet — Idle ignores them.
    hl2ProbeAdvance(/*retAddr=*/0, /*retSubAddr=*/0);

    if (qEnvironmentVariableIntValue("NEREUS_HL2_I2C_SCAN") != 0) {
        requestI2cBusScan();
    }
}

void P1RadioConnection::hl2ProbeAdvance(quint8 retAddr, quint8 retSubAddr)
{
    if (!m_caps || !m_caps->hasIoBoardHl2 || !m_ioBoard) { return; }
    using Reg = IoBoardHl2::Register;

    auto enqueueRead = [this](quint8 deviceAddr, quint8 subAddr) {
        IoBoardHl2::I2cTxn txn;
        txn.bus = IoBoardHl2::kI2cBusIndex;
        txn.address = deviceAddr;
        txn.control = subAddr;
        txn.writeData = 0x00;
        txn.isRead = true;
        txn.needsResponse = true;
        m_ioBoard->enqueueI2c(txn);
    };
    auto enqueueWrite = [this](quint8 deviceAddr, quint8 subAddr, quint8 data) {
        IoBoardHl2::I2cTxn txn;
        txn.bus = IoBoardHl2::kI2cBusIndex;
        txn.address = deviceAddr;
        txn.control = subAddr;
        txn.writeData = data;
        txn.isRead = false;
        txn.needsResponse = false;
        m_ioBoard->enqueueI2c(txn);
    };

    // Helper: did the just-received response answer the read this step
    // is waiting on (its device address and register, from the pending-read
    // record it popped)?  If not, the response belongs to some other consumer
    // (manual R/W tool, bus scan) and the step machine must NOT advance.
    auto matches = [retAddr, retSubAddr](quint8 wantAddr, quint8 wantSub) {
        return retAddr == wantAddr && retSubAddr == wantSub;
    };

    switch (m_hl2ProbeStep) {
        case Hl2ProbeStep::Idle:
            qCInfo(lcConnection) << "HL2: probe step 1 — reading HW version";
            enqueueRead(IoBoardHl2::kI2cAddrHwVersion, 0);
            m_hl2ProbeStep = Hl2ProbeStep::WaitingForHwVersion;
            return;

        case Hl2ProbeStep::WaitingForHwVersion:
            if (!matches(IoBoardHl2::kI2cAddrHwVersion, 0)) {
                // Stray response (likely from manual R/W tool) — ignore.
                return;
            }
            // Response landed.  IoBoardHl2 has already routed C4 → setHardwareVersion()
            // and called setDetected() if it matched 0xF1.  If not detected,
            // abort the probe — mi0bot does the same (console.cs:25810).
            if (!m_ioBoard->isDetected()) {
                qCInfo(lcConnection) << "HL2: HW version =" << Qt::hex
                                     << m_ioBoard->hardwareVersion()
                                     << "(expected 0xF1) — I/O board absent, probe aborted";
                m_hl2ProbeStep = Hl2ProbeStep::Done;
                return;
            }
            qCInfo(lcConnection) << "HL2: HW version 0xF1 confirmed — reading FW major";
            enqueueRead(IoBoardHl2::kI2cAddrGeneral,
                        static_cast<quint8>(Reg::REG_FIRMWARE_MAJOR));
            m_hl2ProbeStep = Hl2ProbeStep::WaitingForFwMajor;
            return;

        case Hl2ProbeStep::WaitingForFwMajor:
            if (!matches(IoBoardHl2::kI2cAddrGeneral,
                         static_cast<quint8>(Reg::REG_FIRMWARE_MAJOR))) {
                return;
            }
            qCInfo(lcConnection) << "HL2: FW major received — reading FW minor";
            enqueueRead(IoBoardHl2::kI2cAddrGeneral,
                        static_cast<quint8>(Reg::REG_FIRMWARE_MINOR));
            m_hl2ProbeStep = Hl2ProbeStep::WaitingForFwMinor;
            return;

        case Hl2ProbeStep::WaitingForFwMinor:
            if (!matches(IoBoardHl2::kI2cAddrGeneral,
                         static_cast<quint8>(Reg::REG_FIRMWARE_MINOR))) {
                return;
            }
            // Mi0bot writes REG_CONTROL=1 to enable the board after version check.
            // Source: console.cs:25831 — `ioBoard.writeRequest(REG_CONTROL, 1);`
            // Upstream tags preserved: //N1GP (from cited console.cs:25833) [v2.10.3.15]
            qCInfo(lcConnection) << "HL2: FW minor received — writing REG_CONTROL=1 (init)";
            enqueueWrite(IoBoardHl2::kI2cAddrGeneral,
                         static_cast<quint8>(Reg::REG_CONTROL), 1);
            // Writes don't have responses we wait for; advance immediately.
            m_hl2ProbeStep = Hl2ProbeStep::Done;
            qCInfo(lcConnection) << "HL2: I/O board init complete";
            // The ongoing poll starts once the board is found (mi0bot
            // console.cs:25809-25822 [@c26a8a4]: the loop runs while the
            // board is present).
            if (m_ioBoardPollTimer) {
                m_ioBoardPollTimer->start();
            }
            return;

        case Hl2ProbeStep::WaitingForControlInitAck:
        case Hl2ProbeStep::Done:
            return;
    }
}

void P1RadioConnection::requestI2cBusScan()
{
    if (!m_caps || !m_caps->hasIoBoardHl2 || !m_ioBoard) {
        qCWarning(lcConnection) << "HL2: I2C bus scan — caps/ioboard not wired; skipping";
        return;
    }
    // Curated probe set — 32 slot queue, common HL2 companion-board addresses:
    //   0x20-0x27 = MCP23008/MCP23017 GPIO expanders (HL2 smallio uses 0x20)
    //   0x1D, 0x41 = mi0bot custom IoBoardHl2 (general regs / HW version)
    //   0x40-0x4F = INA219, PCA9685, TMP102 sensor range
    //   0x68-0x6F = DS3231 RTC, MPU6050, etc.
    static const std::array<quint8, 14> kProbeAddrs = {{
        0x1D,
        0x20, 0x21, 0x22, 0x23,
        0x40, 0x41, 0x42, 0x44,
        0x48, 0x4A, 0x4C,
        0x50, 0x68,
    }};
    int enqueued = 0;
    for (quint8 bus : {quint8(0), quint8(1)}) {
        for (quint8 addr : kProbeAddrs) {
            IoBoardHl2::I2cTxn txn;
            txn.bus           = bus;
            txn.address       = addr;
            txn.control       = 0;
            txn.writeData     = 0;
            txn.isRead        = true;
            txn.needsResponse = true;
            if (m_ioBoard->enqueueI2c(txn)) { ++enqueued; }
            else { goto done; }  // queue full
        }
    }
done:
    qCInfo(lcConnection) << "HL2: I2C bus scan — enqueued" << enqueued
                         << "address probes across bus 0 + bus 1";
}

// ---------------------------------------------------------------------------
// hl2CheckBandwidthMonitor
//
// Called from onWatchdogTick() when m_caps->hasBandwidthMonitor is true.
//
// Source: mi0bot bandwidth_monitor.{c,h} (copyright MW0LGE) —
//   GetInboundBps / GetOutboundBps compute a rolling byte-rate using Windows
//   InterlockedAdd64 and GetTickCount64 (bandwidth_monitor.c:86-123).
//   The original does NOT implement throttle detection; it is a byte-rate
//   telemetry helper that callers compare against an expected rate.
//
// NereusSDR interpretation: use ep6 sequence-gap count as a throttle proxy.
//   m_epRecvSeqExpected is incremented by parseEp6Frame on every good frame;
//   if the watchdog fires and m_epRecvSeqExpected has not advanced since the
//   previous tick the HL2 LAN PHY may be throttling the ep6 stream.
//
// TODO(3I-T12): Port the full byte-rate monitor using std::atomic<int64_t>
//   and std::chrono::steady_clock once Phase 3L adds the I/Q byte accounting
//   plumbing.  The throttle threshold (kBwThrottleGapCount below) should be
//   calibrated against a real HL2.
// ---------------------------------------------------------------------------
void P1RadioConnection::hl2CheckBandwidthMonitor()
{
    if (!m_caps || !m_caps->hasBandwidthMonitor) { return; }

    // Phase 3P-E Task 3: delegate to HermesLiteBandwidthMonitor when wired.
    // The monitor records ep6/ep2 bytes via recordEp6Bytes()/recordEp2Bytes()
    // in onReadyRead()/sendCommandFrame() respectively, then tick() here runs
    // the upstream compute_bps() algorithm (mi0bot bandwidth_monitor.c:86-113
    // [@c26a8a4]) and the NereusSDR throttle-detection layer.
    if (m_bwMonitor) {
        m_bwMonitor->tick();
        // Mirror throttle state into the legacy m_hl2Throttled flag so that
        // hl2IsThrottled() and the test seam hl2ThrottledForTest() remain valid.
        const bool nowThrottled = m_bwMonitor->isThrottled();
        if (nowThrottled && !m_hl2Throttled) {
            m_hl2Throttled = true;
            m_hl2LastThrottleTick = QDateTime::currentDateTimeUtc();
            qCWarning(lcConnection) << "HL2: LAN PHY throttle detected via byte-rate monitor —"
                                    << "ep6 ingress silent for"
                                    << HermesLiteBandwidthMonitor::kThrottleTickThreshold
                                    << "watchdog ticks;"
                                    << "throttle events:" << m_bwMonitor->throttleEventCount();
            emit errorOccurred(RadioConnectionError::None,
                               QStringLiteral("The Hermes Lite 2 asked for a pause because its network link is busy."));
        } else if (!nowThrottled && m_hl2Throttled) {
            m_hl2Throttled = false;
            qCInfo(lcConnection) << "HL2: LAN throttle cleared — ep6 stream resumed";
        }
        return;
    }

    // Fallback: legacy sequence-gap heuristic used when m_bwMonitor is not wired
    // (non-HL2 board or test seam without RadioModel).
    // Source: NereusSDR design — sequence-gap proxy for byte-rate throttle detect.
    // The upstream bandwidth_monitor.{c,h} (MW0LGE [@c26a8a4]) is a byte-rate
    // telemetry helper; throttle detection is a NereusSDR addition.
    static constexpr int kBwThrottleGapCount = 3;  // NereusSDR heuristic
    static quint32 s_lastSeq = 0;

    if (!m_lastEp6At.isValid()) {
        // No frames yet — nothing to monitor.
        s_lastSeq = m_epRecvSeqExpected;
        return;
    }

    if (m_epRecvSeqExpected == s_lastSeq) {
        // Sequence stalled this tick.
        ++m_hl2ThrottleCount;
        if (!m_hl2Throttled && m_hl2ThrottleCount >= kBwThrottleGapCount) {
            m_hl2Throttled = true;
            m_hl2LastThrottleTick = QDateTime::currentDateTimeUtc();
            qCWarning(lcConnection) << "HL2: LAN PHY throttle detected (seq-gap fallback) —"
                                    << "ep6 sequence stalled for"
                                    << m_hl2ThrottleCount << "watchdog ticks;"
                                    << "pausing ep2 command frames";
            emit errorOccurred(RadioConnectionError::None,
                               QStringLiteral("The Hermes Lite 2 asked for a pause because its network link is busy."));
        }
    } else {
        // Sequence advanced — clear throttle.
        if (m_hl2Throttled) {
            qCInfo(lcConnection) << "HL2: LAN throttle cleared (seq-gap fallback) — ep6 stream resumed";
            m_hl2Throttled = false;
        }
        m_hl2ThrottleCount = 0;
    }

    s_lastSeq = m_epRecvSeqExpected;
}

// ---------------------------------------------------------------------------
// scaleSample24
//
// Source: networkproto1.c:367-374 MetisReadThreadMainLoop — sample extraction
// uses (bptr[k+0] << 24 | bptr[k+1] << 16 | bptr[k+2] << 8) to sign-extend
// the 24-bit big-endian value into a 32-bit int, then multiplies by
// const_1_div_2147483648_ (= 1/2^31). The << 8 fill + divide by 2^31 is
// mathematically equivalent to our sign-extend then divide by 2^23 (= 8388608).
// ---------------------------------------------------------------------------
float P1RadioConnection::scaleSample24(const quint8 be24[3]) noexcept
{
    // Sign-extend 24-bit big-endian to qint32 via left-shift trick.
    // Source: networkproto1.c:368-370 — (bptr[k+0] << 24 | bptr[k+1] << 16 | bptr[k+2] << 8)
    qint32 v = (qint32(be24[0]) << 24)
             | (qint32(be24[1]) << 16)
             | (qint32(be24[2]) << 8);
    v >>= 8;  // arithmetic right-shift to sign-extend from 24 bits
    // Source: networkproto1.c:367 — const_1_div_2147483648_ * (shifted value >> 8)
    // Equivalent: v / 2^23 = v / 8388608
    return float(v) / 8388608.0f;
}

// ---------------------------------------------------------------------------
// parseEp6Frame
//
// Source: networkproto1.c:319-415 MetisReadThreadMainLoop — iterates 2 subframes,
// each 512 bytes.
// Upstream inline attribution (networkproto1.c:335/353/354/355, preserved
// verbatim): `// only cleared by getAndResetADC_Overload(), or'ed with existing state //[2.10.3.13]MW0LGE`
// Within each subframe (bptr = FPGAReadBufp + 512*frame, which
// strips the 8-byte Metis header):
//   bptr[0..2]  = sync 7F 7F 7F
//   bptr[3..7]  = C0..C4 (C&C from radio)
//   samples at  bptr[8 + isample*(6*nddc+2) + iddc*6] (networkproto1.c:366)
//
// In our 1032-byte ep6 datagram the 8-byte Metis header is still present:
//   subframe 0: bptr equivalent starts at frame+8  → samples at frame+16
//   subframe 1: bptr equivalent starts at frame+520 → samples at frame+528
//
// Slot size: 6*numRx + 2  (networkproto1.c:361 — spr = 504 / (6*nddc + 2))
// Samples per subframe: 504 / slotBytes
// ---------------------------------------------------------------------------
bool P1RadioConnection::parseEp6Frame(const quint8 frame[1032],
                                       int numRx,
                                       std::vector<std::vector<float>>& perRx) noexcept
{
    // Delegate to the mic-aware overload with a null mic output (back-compat
    // for callers / tests that don't care about the mic16 byte zone).
    return parseEp6Frame(frame, numRx, perRx, /*micOut=*/nullptr);
}

bool P1RadioConnection::parseEp6Frame(const quint8 frame[1032],
                                       int numRx,
                                       std::vector<std::vector<float>>& perRx,
                                       std::vector<float>* micOut) noexcept
{
    // Validate numRx range (1..7 — Thetis supports up to 7 DDCs)
    if (numRx < 1 || numRx > 7) { return false; }

    // Validate Metis ep6 header: EF FE 01 06 + 4-byte sequence
    // Source: networkproto1.c:326-327 — check first four bytes (after MetisReadDirect strips header)
    // In our full datagram the magic lives at [0..3]
    if (frame[0] != 0xEF || frame[1] != 0xFE ||
        frame[2] != 0x01 || frame[3] != 0x06) {
        return false;
    }

    // Validate sync bytes for both USB subframes
    // Source: networkproto1.c:327 — (bptr[0]==0x7f && bptr[1]==0x7f && bptr[2]==0x7f)
    if (frame[8]   != 0x7F || frame[9]   != 0x7F || frame[10]  != 0x7F) { return false; }
    if (frame[520] != 0x7F || frame[521] != 0x7F || frame[522] != 0x7F) { return false; }

    // Source: networkproto1.c:361 — spr = 504 / (6 * nddc + 2)
    const int slotBytes        = 6 * numRx + 2;  // (I24+Q24)*numRx + Mic16
    const int samplesPerSubframe = 504 / slotBytes;

    perRx.assign(numRx, std::vector<float>());
    for (auto& v : perRx) {
        v.reserve(static_cast<size_t>(samplesPerSubframe * 2 * 2));  // 2 subframes × 2 floats/sample
    }

    if (micOut != nullptr) {
        micOut->clear();
        micOut->reserve(static_cast<size_t>(samplesPerSubframe * 2));
    }

    // Parse one 512-byte subframe; sampleStart is the offset of the first sample
    // slot within the full 1032-byte datagram.
    // Source: networkproto1.c:366 — k = 8 + isample*(6*nddc+2) + iddc*6
    //   (where k is relative to bptr which starts at sync bytes)
    //   In our frame: sampleStart = subframeBase + 8 (sync3 + C&C5)
    auto parseSubframe = [&](int sampleStart) {
        for (int s = 0; s < samplesPerSubframe; ++s) {
            for (int r = 0; r < numRx; ++r) {
                // Source: networkproto1.c:366 — k = 8 + isample*slotBytes + iddc*6
                const int off = sampleStart + s * slotBytes + r * 6;
                const float i = scaleSample24(&frame[off]);
                const float q = scaleSample24(&frame[off + 3]);
                perRx[static_cast<size_t>(r)].push_back(i);
                perRx[static_cast<size_t>(r)].push_back(q);
            }
            // Mic16 bytes at offset sampleStart + s*slotBytes + numRx*6.
            // From Thetis networkproto1.c:401-404 [v2.10.3.13] — extracts a
            // 16-bit big-endian mic sample from the slot's last two bytes.
            // We extract every mic sample at the operating sample rate
            // (one per I/Q sample group) — decimation to 48 kHz happens in
            // the caller via P1RadioConnection::decimateMicSamples (see
            // dispatch site above).  Thetis applies mic_decimation_factor
            // inline at networkproto1.c:391-410 [v2.10.3.14]; we split the
            // concerns so the static parser stays pure.
            //   prn->TxReadBufp[2 * mic_sample_count + 0] = const_1_div_2147483648_ *
            //       (double)(bptr[k + 0] << 24 |
            //                bptr[k + 1] << 16);
            //   prn->TxReadBufp[2 * mic_sample_count + 1] = 0.0;
            // Equivalent to: (int16)(bptr[k]<<8 | bptr[k+1]) / 32768.0  — both
            // give the same float in [-1, +1].  We use the int16/32768 form
            // because the bytes are signed 16-bit big-endian.
            if (micOut != nullptr) {
                const int micOff = sampleStart + s * slotBytes + numRx * 6;
                const int16_t mic16 = static_cast<int16_t>(
                    (static_cast<uint16_t>(frame[micOff]) << 8) |
                    static_cast<uint16_t>(frame[micOff + 1]));
                micOut->push_back(static_cast<float>(mic16) / 32768.0f);
            }
        }
    };

    // Subframe 0: sync at frame[8..10], C&C at [11..15], samples at [16..]
    parseSubframe(16);
    // Subframe 1: sync at frame[520..522], C&C at [523..527], samples at [528..]
    parseSubframe(528);

    return true;
}

// ---------------------------------------------------------------------------
// getAdcForDdc
// ---------------------------------------------------------------------------
int P1RadioConnection::getAdcForDdc(int /*ddc*/) const
{
    return 0;  // All P1 DDCs map to ADC0 for now
}

} // namespace NereusSDR
