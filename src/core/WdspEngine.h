#pragma once

// =================================================================
// src/core/WdspEngine.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/cmaster.cs, original licence from Thetis source is included below
//   Project Files/Source/ChannelMaster/cmaster.c, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
// =================================================================

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

/*  cmaster.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2014-2019 Warren Pratt, NR0V

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

warren@wpratt.com

*/

#include "WdspTypes.h"

#include <QObject>
#include <QString>

#include <map>
#include <memory>

namespace NereusSDR {

class RxChannel;
class TxChannel;

// Central WDSP manager. Owns all RxChannel instances and manages
// system-level initialization (FFTW wisdom, impulse cache).
//
// Owned by RadioModel. Created once per radio connection.
// Thread safety: create/destroy on main thread only.
//                processIq called from audio callback via RxChannel.
//
// Ported from Thetis cmaster.cs:491 (CMCreateCMaster) and
// cmaster.c:32-93 (create_rcvr).
class WdspEngine : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool initialized READ isInitialized NOTIFY initializedChanged)

public:
    explicit WdspEngine(QObject* parent = nullptr);
    ~WdspEngine() override;

    // --- System lifecycle ---

    // Check if wisdom file needs to be generated (first run).
    // If true, initialize() will take 30-60s and emit wisdomProgress.
    static bool needsWisdomGeneration(const QString& configDir);

    // Initialize WDSP: load FFTW wisdom, initialize impulse cache.
    // configDir: directory for wisdom file and impulse cache.
    // Wisdom runs async — listen to initializedChanged for completion.
    bool initialize(const QString& configDir);

    // Shutdown WDSP: save impulse cache, destroy all channels, free resources.
    void shutdown();

    bool isInitialized() const { return m_initialized; }

    // --- RX Channel management ---

    // Create an RX channel with the given parameters.
    // Returns the new RxChannel (owned by WdspEngine) or nullptr on failure.
    // channelId: WDSP channel number (0-31). Must be unique.
    //
    // Default parameters match our P2 DDC configuration:
    //   inputBufferSize=238 (one P2 packet), dspBufferSize=4096,
    //   all rates=48000 (no resampling needed)
    //
    // From Thetis cmaster.c:72-86 (OpenChannel call in create_rcvr)
    RxChannel* createRxChannel(int channelId,
                               int inputBufferSize = 238,
                               int dspBufferSize = 4096,
                               int inputSampleRate = 48000,
                               int dspSampleRate = 48000,
                               int outputSampleRate = 48000);

    // Destroy an RX channel by ID. The RxChannel pointer becomes invalid.
    void destroyRxChannel(int channelId);

    // Look up an existing RX channel by WDSP channel ID.
    RxChannel* rxChannel(int channelId) const;

    // --- TX Channel management ---

    // TX channel constants derived from Thetis cmaster.c:177-190 [v2.10.3.13].
    // From cmaster.c:184  — channel type 1 = TX (vs. RX = 0).
    static constexpr int kTxChannelType    = 1;
    // From cmaster.c:190  — block until output available (bfo = 1 for TX).
    static constexpr int kTxBlockOnOutput  = 1;
    // From cmaster.c:187  — tslewup  = 0.010 s (10 ms channel-level state envelope).
    static constexpr double kTxTSlewUpSecs   = 0.010;
    // From cmaster.c:189  — tslewdown = 0.010 s (10 ms channel-level state envelope).
    static constexpr double kTxTSlewDownSecs = 0.010;
    // From cmaster.c:182  — DSP sample rate for TX channel = 96000 Hz.
    static constexpr int kTxDspSampleRate  = 96000;
    // From cmaster.c:180  — DSP buffer size for TX channel = 4096 samples.
    static constexpr int kTxDspBufferSize  = 4096;

    // Create a TX channel with the given parameters.
    //
    // Channel ID convention: Thetis uses `chid(inid(1, 0), 0)`, which with
    // NereusSDR's single-RX (CMsubrcvr=1, CMrcvr=1) layout resolves to
    // channel 1.  C# equivalent: `WDSP.id(1, 0)` — dsp.cs:926-944 [v2.10.3.13]
    // case 2 returns `CMsubrcvr * CMrcvr = 1 * 1 = 1`.
    //
    // APPROACH A (3M-1a Task C.1): this method opens the WDSP-side channel
    // (via OpenChannel with type=1/TX) but returns nullptr for the C++ wrapper.
    // The TxChannel C++ wrapper class is constructed in Task C.2.
    // Document this in callsites: the returned pointer is null until C.2.
    //
    // Default parameters match our P2 configuration:
    //   inputBufferSize=238 (one P2 packet), dspBufferSize=4096,
    //   inputRate=48000, dspRate=96000, outputRate=48000
    //
    // From Thetis cmaster.c:177-190 (create_xmtr OpenChannel call) [v2.10.3.13]
    TxChannel* createTxChannel(int channelId,
                               int inputBufferSize = 238,
                               int dspBufferSize = kTxDspBufferSize,
                               int inputSampleRate = 48000,
                               int dspSampleRate = kTxDspSampleRate,
                               int outputSampleRate = 48000);

    // Destroy a TX channel by ID. Idempotent — safe to call even if the
    // channel was never created or was already destroyed.
    void destroyTxChannel(int channelId);

    // Look up an existing TX channel by WDSP channel ID.
    // Returns nullptr if not found OR if C.2 wrapper construction has not
    // yet run (Approach A: WDSP channel may be open while C++ wrapper is null).
    TxChannel* txChannel(int channelId) const;

signals:
    void initializedChanged(bool initialized);
    // Emitted during wisdom generation. percent=0-100, status=what's being planned.
    void wisdomProgress(int percent, const QString& status);

private:
    bool m_initialized{false};
    QString m_configDir;

    // Finish initialization after WDSPwisdom completes (called from timer)
    void finishInitialization();

    // RX channels keyed by WDSP channel ID.
    std::map<int, std::unique_ptr<RxChannel>> m_rxChannels;

    // TX channels keyed by WDSP channel ID.
    // NOTE (3M-1a Approach A): In Task C.1 the WDSP channel is opened but no
    // C++ TxChannel wrapper is constructed. The set of open TX channel IDs is
    // tracked here with nullptr values until Task C.2 fills the wrapper.
    std::map<int, TxChannel*> m_txChannels;
};

} // namespace NereusSDR
