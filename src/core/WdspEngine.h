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

#ifdef NEREUS_BUILD_TESTS
// Forward declaration for test-only friend access (see end of class).  The
// test class lives in the global namespace because it inherits from QObject
// in tests/tst_wdsp_engine_tx_channel.cpp without a NereusSDR namespace
// wrapper — friend declarations need the fully-qualified name.
class TestWdspEngineTxChannel;
#endif

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
    // DSP buffer size for TX channel = 2048 samples.
    //
    // Deviation from Thetis: cmaster.c:180 [v2.10.3.13] hardcodes 4096.
    // We adopt deskhpsdr's 2048 (transmitter.c:1072 [@120188f] —
    // `tx->dsp_size = 2048`) for two reasons:
    //   1. WDSP iobuffs.c:577 wraps r2_outidx with `==` rather than modulo:
    //        `if ((a->r2_outidx += a->out_size) == a->r2_active_buffsize) ...`
    //      With dsp_size=4096 + in_size=238 (P2 5 ms tick), out_size=952
    //      and r2_active_buffsize=16384, which is not a multiple of 952 —
    //      the wrap never triggers and fexchange2 reads past the end of
    //      r2_baseptr into random heap (verified by bench, r2_outidx grew
    //      unbounded to >900 000).  With dsp_size=2048 + in_size=256
    //      (this header), out_size=1024 and r2_active_buffsize=8192,
    //      which divides cleanly (8 wraps per ring cycle).
    //   2. ~50 % lower TX pipeline latency: dsp_insize/in_rate +
    //      dsp_outsize/out_rate drops from 85 ms to 42 ms.
    static constexpr int kTxDspBufferSize  = 2048;

    // Create a TX channel with the given parameters.
    //
    // Channel ID convention: Thetis uses `chid(inid(1, 0), 0)`, which with
    // NereusSDR's single-RX (CMsubrcvr=1, CMrcvr=1) layout resolves to
    // channel 1.  C# equivalent: `WDSP.id(1, 0)` — dsp.cs:926-944 [v2.10.3.13]
    // case 2 returns `CMsubrcvr * CMrcvr = 1 * 1 = 1`.
    //
    // Opens the WDSP TX channel (OpenChannel type=1) and constructs the
    // TxChannel C++ wrapper around the 31-stage TXA pipeline that WDSP built.
    // Returns the TxChannel pointer on success, nullptr if WDSP is not
    // initialized.
    //
    // Default parameters match our P2 configuration:
    //   inputBufferSize=256 (5.33 ms at 48 kHz; satisfies WDSP r1 ring
    //                       wrap math — must divide DSP_MULT × dsp_insize
    //                       = 2 × 1024 = 2048; 2048/256 = 8 ✓),
    //   dspBufferSize=kTxDspBufferSize (2048, deskhpsdr-derived; see
    //                                  the `kTxDspBufferSize` definition
    //                                  above for the full rationale),
    //   inputRate=48000, dspRate=96000, outputRate=48000.
    //
    // From Thetis cmaster.c:177-190 (create_xmtr OpenChannel call) [v2.10.3.13]
    TxChannel* createTxChannel(int channelId,
                               int inputBufferSize = 256,
                               int dspBufferSize = kTxDspBufferSize,
                               int inputSampleRate = 48000,
                               int dspSampleRate = kTxDspSampleRate,
                               int outputSampleRate = 48000);

    // Destroy a TX channel by ID. Idempotent — safe to call even if the
    // channel was never created or was already destroyed.
    void destroyTxChannel(int channelId);

    // Look up an existing TX channel by WDSP channel ID.
    // Returns nullptr if not found. If the channel exists, the pointer is
    // always non-null (wrapper is always constructed alongside the WDSP channel).
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
    // Each entry holds a TxChannel C++ wrapper around the 31-stage TXA
    // pipeline that WDSP constructs when OpenChannel(type=1) is called.
    // destroyTxChannel's erase() runs the unique_ptr destructor automatically.
    std::map<int, std::unique_ptr<TxChannel>> m_txChannels;

#ifdef NEREUS_BUILD_TESTS
    // Test-only friend: lets unit tests bypass async wisdom load by setting
    // m_initialized = true directly so they can exercise createTxChannel /
    // createRxChannel without a running event loop or a real WDSP wisdom
    // file.  Production builds (without NEREUS_BUILD_TESTS) never see this.
    friend class ::TestWdspEngineTxChannel;
#endif
};

} // namespace NereusSDR
