// =================================================================
// src/core/audio/cmasio.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/ChannelMaster/cmasio.h
//   Project Files/Source/ChannelMaster/cmasio.c
//   [v2.10.3.15 @3759d096]
//
// The mic helper's ASIO set-up on Windows: the cmASIO set-up of the
// mic (create_cmasio, cmasio.c:47-102) and the hooks capture_main.cpp
// installs (the Steinberg SDK adapter AsioDriverWin and the mic plan).
//
// Where NereusSDR differs from cmASIO:
//   - The clock matcher in shared memory (DeviceRateMatcher and
//     MatcherReader) takes the place of rmatchV, its forceRMatchVar and
//     the bufferFull / bufferEmpty semaphores of lock mode (D34): no
//     lock and no wait in the buffer switch.
//   - The driver's control panel opens from Setup through the helper
//     (R-AUD-22); cmASIO has none.
//   - A reset request (or a buffer size change) from the driver restarts
//     the session outside the callback with the driver's new settings
//     (AsioSession, D14), where cmASIO stops.
//   - One session carries every role on the driver (R-AUD-19), each on
//     its own pair; cmASIO plays one stereo pair from a base channel.
//   - The saved mic config stands in for the registry cmASIO reads: the
//     driver is the mic's device, the base input channel its first
//     channel, the input mode its MicChannel.  Both adds the two
//     channels, as combinebuff does.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 15 (R-AUD-01, R-AUD-22): ported
//               create_cmasio's set-up order, base channels and input
//               mode for the mic helper on Windows. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-09: native audio plan final fix wave (R-AUD-07): the note on
//               Both corrected; it adds the two channels.  J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

// --- From cmasio.h ---
/*  cmasio.h

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2023 Bryan Rambo W4WMT

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

bryanr@bometals.com

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

// --- From cmasio.c ---
/*  cmasio.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2023 Bryan Rambo W4WMT

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

bryanr@bometals.com

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

#pragma once

#include "core/audio/CaptureHelper.h"

#include <QString>

#include <optional>

namespace NereusSDR {

struct AudioDeviceConfig;

namespace CmAsio {

// From Thetis cmasio.h:62-67 [v2.10.3.15]
enum class InputMode : long {
    Left = 0,   // IM_LEFT
    Right,      // IM_RIGHT
    Both        // IM_BOTH
};

// What create_cmasio gathers before prepareASIO (cmasio.c:47-70), in its
// order.  From Thetis cmasio.h:75-89 [v2.10.3.15]
struct Setup {
    int protocol = 1; // W4WMT cmASIO via Protocol 1
    int blockSize = 0;                       // 0: the driver's preferred size
    int sampleRate = 0;                      // 0: the driver's own rate
    QString driverName;
    long inputBaseChannel = 0; //[2.10.3.13]MW0LGE added explicit base channel indices
    long outputBaseChannel = 0;              // 0-based; the mic plays nothing
    InputMode inputMode = InputMode::Both; //[2.10.3.13]MW0LGE added input mode, so would use ch1, ch2, or both for input
};

// create_cmasio's set-up from the mic's saved config; nullopt when no
// driver is named (cmasio.c:53 returns there).
std::optional<Setup> createSetup(const AudioDeviceConfig& mic);

// The mic's input use on the session: the stereo pair from the base
// input channel, picked by the input mode (cmasio.c:148-170).
CaptureHelperAsioMic micUse(const Setup& setup);

// What the Windows helper installs with setCaptureHelperAsio().
CaptureHelperAsio helperHooks();

} // namespace CmAsio

} // namespace NereusSDR
