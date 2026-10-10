// =================================================================
// src/core/audio/cmasio.cpp  (NereusSDR)
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

#include "core/audio/cmasio.h"

#include "core/AudioDeviceConfig.h"
#include "core/LogCategories.h"
#include "core/audio/AsioDriverWin.h"

#include <memory>

namespace NereusSDR::CmAsio {

namespace {

// The cmASIO channel pair is two adjacent channels from the base.
constexpr int kPairChannels = 2;

// From Thetis cmasio.c:162-169 [v2.10.3.15]: left copies ch1 to both
// sides, right copies ch2, both keeps the pair.
MicChannelPick pickFor(InputMode mode)
{
    switch (mode) {
    case InputMode::Left:
        return MicChannelPick::Left;
    case InputMode::Right:
        return MicChannelPick::Right;
    case InputMode::Both:
        return MicChannelPick::Both;
    }
    return MicChannelPick::Both;
}

InputMode inputModeFor(MicChannelPick pick)
{
    switch (pick) {
    case MicChannelPick::Left:
        return InputMode::Left;
    case MicChannelPick::Right:
        return InputMode::Right;
    case MicChannelPick::Both:
        return InputMode::Both;
    }
    return InputMode::Both;
}

} // namespace

std::optional<Setup> createSetup(const AudioDeviceConfig& mic)
{
    Setup setup;
    // From Thetis cmasio.c:49-51 [v2.10.3.15]: the protocol, then the
    // block size and rate (the mic's saved buffer and rate here).
    setup.protocol = 1; // default Protocol 2
    setup.blockSize = mic.bufferSamples > 0 ? mic.bufferSamples : 0;
    setup.sampleRate = mic.sampleRate > 0 ? mic.sampleRate : 0;

    // From Thetis cmasio.c:52-56 [v2.10.3.15]: the driver (the ASIO device id), or no set-up.
    setup.driverName = !mic.deviceId.trimmed().isEmpty() ? mic.deviceId.trimmed()
                                                         : mic.deviceName.trimmed();
    if (setup.driverName.isEmpty()) {
        return std::nullopt;
    }
    qCInfo(lcAudio).noquote() << QStringLiteral(
        "Initializing cmASIO with: block size = %1, sample rate = %2, driver name = \"%3\"")
        .arg(setup.blockSize).arg(setup.sampleRate).arg(setup.driverName);
    // From Thetis cmasio.c:58-62 [v2.10.3.15]
    //[2.10.3.13]MW0LGE get explicit base channel indices for the stereo pair, default to 0 if none in registry
    long input_base_channel = 0;
    if (mic.firstChannel >= 1) {
        input_base_channel = static_cast<long>(mic.firstChannel - 1);
    }
    long output_base_channel = 0;
    setup.inputBaseChannel = input_base_channel;
    setup.outputBaseChannel = output_base_channel;

    // From Thetis cmasio.c:64-67 [v2.10.3.15]
    //[2.10.3.13]MW0LGE the input mode, left = ch1, right = ch2, both = stereo
    // The saved MicChannel always holds a pick, so Thetis's fallback
    // (input_mode = IM_BOTH, "//both is default") applies only to a pick
    // this build does not know.
    // [original inline comment from cmasio.c:66] //both is default
    setup.inputMode = inputModeFor(mic.micChannel);

    // From Thetis cmasio.c:69-70 [v2.10.3.15]: prepareASIO follows; here
    // the helper's session opens with the set-up (micUse).
    // [original inline comment from cmasio.c:69] //int result = prepareASIO(pcma->blocksize, samplerate, asioDriverName, &CallbackASIO)
    return setup;
}

CaptureHelperAsioMic micUse(const Setup& setup)
{
    CaptureHelperAsioMic use;
    use.driver = setup.driverName;
    use.bufferFrames = setup.blockSize;
    use.rate = static_cast<double>(setup.sampleRate);
    // From Thetis cmasio.c:151-169 [v2.10.3.15]
    //[2.10.3.13]MW0LGE added input mode, so can use ch1(L), ch2(R), or both for input
    use.pair.firstChannel = static_cast<int>(setup.inputBaseChannel) + 1;
    use.pair.channelCount = kPairChannels;
    use.pick = pickFor(setup.inputMode);
    return use;
}

CaptureHelperAsio helperHooks()
{
    CaptureHelperAsio hooks;
    hooks.makeDriver = []() -> std::unique_ptr<IAsioDriver> {
        return std::make_unique<AsioDriverWin>();
    };
    hooks.planMic = [](const AudioDeviceConfig& mic) -> std::optional<CaptureHelperAsioMic> {
        const std::optional<Setup> setup = createSetup(mic);
        if (!setup) {
            return std::nullopt;
        }
        return micUse(*setup);
    };
    return hooks;
}

} // namespace NereusSDR::CmAsio
