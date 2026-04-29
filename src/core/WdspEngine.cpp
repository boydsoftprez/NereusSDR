// =================================================================
// src/core/WdspEngine.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/ChannelMaster/cmaster.c, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
// =================================================================

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

#include "WdspEngine.h"
#include "RxChannel.h"
#include "TxChannel.h"
#include "LogCategories.h"
#include "wdsp_api.h"

#include <QDir>
#include <QStandardPaths>
#include <QTimer>
#include <QThread>

namespace NereusSDR {

WdspEngine::WdspEngine(QObject* parent)
    : QObject(parent)
{
}

WdspEngine::~WdspEngine()
{
    if (m_initialized) {
        shutdown();
    }
}

// Check if wisdom file needs generation (first run detection).
// From AetherSDR AudioEngine::needsWisdomGeneration() pattern.
bool WdspEngine::needsWisdomGeneration(const QString& configDir)
{
    // WDSP writes wisdom to "{configDir}wdspWisdom00"
    // (see wisdom.c: strncat(wisdom_file, "wdspWisdom00", 16))
    QString wisdomFile = configDir + QStringLiteral("wdspWisdom00");
    return !QFile::exists(wisdomFile);
}

// Parse an FFT size from the WDSP status string and estimate progress %.
// WDSP plans sizes 64..262144 (powers of 2) = 13 sizes.
// For filter sizes: 3 plans each (COMPLEX FWD, COMPLEX BWD, COMPLEX BWD+1).
// For display sizes: 1-2 more. Total ~42 steps.
static int estimateWisdomPercent(const char* status)
{
    if (!status || status[0] == '\0') {
        return 0;
    }
    // Extract FFT size from status like "Planning COMPLEX FORWARD  FFT size 4096"
    int fftSize = 0;
    const char* sizeStr = strstr(status, "size ");
    if (sizeStr) {
        fftSize = atoi(sizeStr + 5);
    }
    if (fftSize <= 0) {
        return 0;
    }
    // Map FFT size to approximate progress:
    // 64=5%, 128=10%, 256=15%, 512=20%, 1024=25%, 2048=30%, 4096=35%,
    // 8192=45%, 16384=55%, 32768=65%, 65536=75%, 131072=85%, 262144=95%
    int step = 0;
    for (int s = 64; s <= 262144; s *= 2) {
        step++;
        if (fftSize <= s) {
            break;
        }
    }
    return qBound(1, step * 100 / 14, 99);
}

bool WdspEngine::initialize(const QString& configDir)
{
    if (m_initialized) {
        qCWarning(lcDsp) << "WdspEngine already initialized";
        return true;
    }

#ifdef HAVE_WDSP
    m_configDir = configDir;

    // Ensure config directory exists
    QDir dir(m_configDir);
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }

    // WDSP appends "wdspWisdom00" directly to the path — ensure trailing separator
    if (!m_configDir.endsWith(QLatin1Char('/')) && !m_configDir.endsWith(QLatin1Char('\\'))) {
        m_configDir += QLatin1Char('/');
    }

    // Note: Thetis wisdom files are NOT reusable — FFTW wisdom is specific
    // to the exact FFTW build. Copying across builds hangs on import.
    bool needsGeneration = needsWisdomGeneration(m_configDir);
    QByteArray configPath = m_configDir.toUtf8();

    if (!needsGeneration) {
        // Fast path: wisdom file exists — WDSPwisdom just loads it (instant).
        // Safe to call on main thread.
        qCInfo(lcDsp) << "Loading existing WDSP wisdom file...";
        WDSPwisdom(configPath.data());
        qCInfo(lcDsp) << "WDSP wisdom loaded";
        finishInitialization();
    } else {
        // Slow path: generate wisdom on a background thread with progress.
        qCInfo(lcDsp) << "Generating WDSP wisdom (first run, may take several minutes)...";

        auto* wisdomThread = QThread::create([configPath]() {
            WDSPwisdom(const_cast<char*>(configPath.constData()));
        });
        wisdomThread->setObjectName(QStringLiteral("WisdomThread"));

        // Poll wisdom_get_status() for progress updates
        auto* pollTimer = new QTimer(this);
        pollTimer->setInterval(250);

        connect(wisdomThread, &QThread::finished, this, [this, wisdomThread, pollTimer]() {
            pollTimer->stop();
            pollTimer->deleteLater();
            wisdomThread->deleteLater();
            emit wisdomProgress(100, QStringLiteral("FFTW planning complete"));
            finishInitialization();
        });

        connect(pollTimer, &QTimer::timeout, this, [this]() {
            char* status = wisdom_get_status();
            if (status && status[0] != '\0') {
                int pct = estimateWisdomPercent(status);
                QString msg = QString::fromUtf8(status).trimmed();
                emit wisdomProgress(pct, msg);
            }
        });

        wisdomThread->start();
        pollTimer->start();
    }
    return true;

#else
    Q_UNUSED(configDir);
    qCInfo(lcDsp) << "WDSP not available (stub mode)";
    m_initialized = true;
    emit initializedChanged(true);
    return true;
#endif
}

void WdspEngine::finishInitialization()
{
#ifdef HAVE_WDSP
    qCInfo(lcDsp) << "WDSP wisdom initialized";

    // Initialize impulse cache for faster filter coefficient computation
    init_impulse_cache(1);

    // Load cached impulse data if available
    QString cacheFile = m_configDir + QStringLiteral("/impulse_cache.bin");
    QByteArray cachePath = cacheFile.toUtf8();
    if (QFile::exists(cacheFile)) {
        int cacheResult = read_impulse_cache(cachePath.constData());
        qCDebug(lcDsp) << "Impulse cache loaded, result:" << cacheResult;
    }

    m_initialized = true;
    emit initializedChanged(true);
    qCInfo(lcDsp) << "WDSP initialized successfully";
#endif
}

void WdspEngine::shutdown()
{
    if (!m_initialized) {
        return;
    }

    qCInfo(lcDsp) << "Shutting down WDSP...";

    // TX channels destroyed BEFORE RX: the TXA pipeline (post-uslew →
    // rsmpout → outmeter) feeds samples into shared output buffers; tearing
    // RX down first can leave the TXA chain reading freed channel state
    // during teardown. WDSP teardown ordering: TX → RX always.
    //
    // Destroy all TX channels (collect IDs first to avoid iterator invalidation)
    {
        std::vector<int> txIds;
        for (const auto& [id, ch] : m_txChannels) {
            txIds.push_back(id);
        }
        for (int id : txIds) {
            destroyTxChannel(id);
        }
    }

    // Destroy all RX channels (collect IDs first to avoid iterator invalidation)
    {
        std::vector<int> channelIds;
        for (const auto& [id, ch] : m_rxChannels) {
            channelIds.push_back(id);
        }
        for (int id : channelIds) {
            destroyRxChannel(id);
        }
    }

#ifdef HAVE_WDSP
    // Save impulse cache for next startup
    QString cacheFile = m_configDir + QStringLiteral("/impulse_cache.bin");
    QByteArray cachePath = cacheFile.toUtf8();
    save_impulse_cache(cachePath.constData());
    qCDebug(lcDsp) << "Impulse cache saved";

    destroy_impulse_cache();
#endif

    m_initialized = false;
    emit initializedChanged(false);
    qCInfo(lcDsp) << "WDSP shut down";
}

RxChannel* WdspEngine::createRxChannel(int channelId,
                                       int inputBufferSize,
                                       int dspBufferSize,
                                       int inputSampleRate,
                                       int dspSampleRate,
                                       int outputSampleRate)
{
    if (!m_initialized) {
        qCWarning(lcDsp) << "Cannot create channel: WDSP not initialized";
        return nullptr;
    }

    if (m_rxChannels.count(channelId)) {
        qCWarning(lcDsp) << "Channel" << channelId << "already exists";
        return m_rxChannels.at(channelId).get();
    }

#ifdef HAVE_WDSP
    // From Thetis cmaster.c:72-86 (create_rcvr OpenChannel call)
    OpenChannel(
        channelId,
        inputBufferSize,        // in_size
        dspBufferSize,          // dsp_size (4096 from Thetis)
        inputSampleRate,        // input sample rate
        dspSampleRate,          // dsp sample rate
        outputSampleRate,       // output sample rate
        0,                      // type: 0=RX
        0,                      // state: 0=off initially
        0.010,                  // tdelayup  — from Thetis cmaster.c:82
        0.025,                  // tslewup   — from Thetis cmaster.c:83
        0.000,                  // tdelaydown — from Thetis cmaster.c:84
        0.010,                  // tslewdown — from Thetis cmaster.c:85
        1);                     // bfo: block until output available

    // NB / NB2 lifecycle is owned by RxChannel::m_nb (NbFamily) — see
    // src/core/NbFamily.h. Do NOT re-add create/destroy_anbEXT/nobEXT
    // here; doing so double-constructs the WDSP anb/nob objects.

    // Initialize WDSP to match RxChannel's cached defaults so that
    // subsequent setMode/setFilterFreqs calls from RadioModel work correctly.
    // Without this, the RxChannel guard (if val == m_mode) would skip the
    // WDSP call when the requested mode matches the cached default.
    SetRXAMode(channelId, static_cast<int>(DSPMode::LSB));
    // Both bp1 AND nbp0 must be seeded — see RxChannel::setFilterFreqs
    // comment for why. Thetis seeds both at channel create.
    SetRXABandpassFreqs(channelId, -2850.0, -150.0);
    RXANBPSetFreqs(channelId, -2850.0, -150.0);
    SetRXAAGCMode(channelId, static_cast<int>(AGCMode::Med));
    SetRXAAGCTop(channelId, 80.0);

    // Dual-mono audio output. WDSP's create_panel default is copy=0
    // (binaural — I and Q carry separate phase-shifted content for
    // a headphone stereo image). Played through speakers the two
    // channels comb-filter each other into pitched, unintelligible
    // "Donald Duck" audio. Thetis radio.cs:1157 drives this from
    // BinOn which defaults to false → dual-mono. Match that default.
    SetRXAPanelBinaural(channelId, 0);

    qCInfo(lcDsp) << "Created RX channel" << channelId
                   << "bufSize=" << inputBufferSize
                   << "rate=" << inputSampleRate;
#endif

    auto channel = std::make_unique<RxChannel>(channelId, inputBufferSize,
                                               inputSampleRate, this);
    RxChannel* ptr = channel.get();

#ifdef HAVE_WDSP
    // Push persisted SNB Setup defaults to the RXA channel now that both
    // OpenChannel (above) and RxChannel ctor (just above — which created
    // NbFamily + ran create_anbEXT/create_nobEXT) have run. SNB lives
    // inside rxa[channelId].snba and requires OpenChannel first; we gate
    // the seed here rather than inside NbFamily so unit tests that use
    // fabricated channel ids (never Opened) don't null-deref SetRXASNBA*.
    // (Codex review PR #120, P2 — 2026-04-23.)
    if (auto* nb = ptr->nb()) { nb->seedSnbFromSettings(); }
#endif

    m_rxChannels.emplace(channelId, std::move(channel));
    return ptr;
}

void WdspEngine::destroyRxChannel(int channelId)
{
    auto it = m_rxChannels.find(channelId);
    if (it == m_rxChannels.end()) {
        return;
    }

#ifdef HAVE_WDSP
    // Deactivate with drain
    SetChannelState(channelId, 0, 1);

    // NB / NB2 destroy is owned by ~NbFamily inside ~RxChannel — see
    // src/core/NbFamily.h. Do NOT re-add destroy_anbEXT/nobEXT here.

    // Close the WDSP channel
    CloseChannel(channelId);
#endif

    m_rxChannels.erase(it);
    qCInfo(lcDsp) << "Destroyed RX channel" << channelId;
}

RxChannel* WdspEngine::rxChannel(int channelId) const
{
    auto it = m_rxChannels.find(channelId);
    if (it != m_rxChannels.end()) {
        return it->second.get();
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// TX Channel management
// ---------------------------------------------------------------------------

TxChannel* WdspEngine::createTxChannel(int channelId,
                                       int inputBufferSize,
                                       int dspBufferSize,
                                       int inputSampleRate,
                                       int dspSampleRate,
                                       int outputSampleRate)
{
    if (!m_initialized) {
        qCWarning(lcDsp) << "Cannot create TX channel: WDSP not initialized";
        return nullptr;
    }

    if (m_txChannels.count(channelId)) {
        qCWarning(lcDsp) << "TX channel" << channelId << "already exists";
        return m_txChannels.at(channelId).get();   // already exists — return existing wrapper
    }

#ifdef HAVE_WDSP
    // From Thetis cmaster.c:177-190 (create_xmtr OpenChannel call) [v2.10.3.13]
    // Differences vs. RX: type=1 (TX), bfo=1 (block-on-output), dsp_rate=96000,
    // tdelayup=0, tslewup=0.010, tdelaydown=0, tslewdown=0.010.
    OpenChannel(
        channelId,
        inputBufferSize,        // in_size — from cmaster.c:179 pcm->xcm_insize[in_id]
        dspBufferSize,          // dsp_size — from cmaster.c:180 hardcoded 4096
        inputSampleRate,        // input sample rate — from cmaster.c:181 pcm->xcm_inrate[in_id]
        dspSampleRate,          // dsp sample rate — from cmaster.c:182 96000
        outputSampleRate,       // output sample rate — from cmaster.c:183 pcm->xmtr[i].ch_outrate
        kTxChannelType,         // type=1 (TX) — from cmaster.c:184 [v2.10.3.13]
        0,                      // initial state: off — from cmaster.c:185
        0.000,                  // tdelayup  — from cmaster.c:186
        kTxTSlewUpSecs,         // tslewup 0.010 s — from cmaster.c:187 [v2.10.3.13]
        0.000,                  // tdelaydown — from cmaster.c:188
        kTxTSlewDownSecs,       // tslewdown 0.010 s — from cmaster.c:189 [v2.10.3.13]
        kTxBlockOnOutput);      // bfo=1 (block on output) — from cmaster.c:190 [v2.10.3.13]

    qCInfo(lcDsp) << "Opened TX WDSP channel" << channelId
                  << "bufSize=" << inputBufferSize
                  << "inRate=" << inputSampleRate
                  << "dspRate=" << dspSampleRate;

    // 3M-1a bench fix: TX channel default configuration.
    // Without this block the WDSP TX channel is in an undefined-default state
    // and ALC's gain integrator runs unbounded on silent input — output
    // diverges to inf within ~1 second of TUN.
    //
    // This is the standard set of init calls deskhpsdr issues right after
    // OpenChannel(type=1).  Cite: deskhpsdr/src/transmitter.c:1459-1473 [@120188f]:
    //   SetTXABandpassWindow(tx->id, 1);   // 7-term Blackman-Harris
    //   SetTXABandpassRun(tx->id, 1);
    //   SetTXAAMSQRun(tx->id, 0);          // disable mic noise gate
    //   SetTXAALCAttack(tx->id, 1);        // 1 ms attack
    //   SetTXAALCDecay(tx->id, 10);        // 10 ms decay
    //   SetTXAALCMaxGain(tx->id, 0);       // 0 dB max — KEY: caps ALC at 1.0×
    //   SetTXAALCSt(tx->id, 1);            // ALC on (never switch it off!)
    //   SetTXAPreGenMode/ToneMag/ToneFreq/Run — PreGen off (silence)
    //   SetTXAPanelRun(tx->id, 1);         // activate patch panel
    //   SetTXAPanelSelect(tx->id, 2);      // route Mic I sample
    //   SetTXAPostGenRun(tx->id, 0);       // PostGen off until setTuneTone
    SetTXABandpassWindow(channelId, 1);
    SetTXABandpassRun(channelId, 1);
    SetTXAAMSQRun(channelId, 0);
    SetTXAALCAttack(channelId, 1);
    SetTXAALCDecay(channelId, 10);
    SetTXAALCMaxGain(channelId, 0.0);
    SetTXAALCSt(channelId, 1);

    // Leveler — slow speech-leveling AGC stage that sits between mic
    // preamp/bandpass and the ALC. Without this enabled, the ALC alone
    // has to handle both intelligibility compression AND fast clip
    // protection, and (with ALCMaxGain=0 dB) it amplifies weak inputs
    // back to unity output, making the slider feel ineffective. Pulled
    // forward from 3M-3a per JJ's bench feedback (2026-04-28) — the
    // plan's "Leveler off in 3M-1b" left SSB sounding too hot.
    //
    // Defaults sourced from upstream:
    //   Thetis radio.cs:2979 [v2.10.3.13]: tx_leveler_max_gain = 15.0 dB
    //   Thetis radio.cs:2999 [v2.10.3.13]: tx_leveler_decay    = 100 ms
    //   Thetis radio.cs:3019 [v2.10.3.13]: tx_leveler_on       = true
    //   deskhpsdr/src/transmitter.c:1273 [@120188f]: lev_attack = 1 ms
    //     (Thetis doesn't expose attack as a setter; deskhpsdr's 1ms is
    //      the standard SSB attack value)
    SetTXALevelerAttack(channelId, 1);
    SetTXALevelerDecay(channelId, 100);
    SetTXALevelerTop(channelId, 15.0);
    SetTXALevelerSt(channelId, 1);
    SetTXAPreGenMode(channelId, 0);
    SetTXAPreGenToneMag(channelId, 0.0);
    SetTXAPreGenToneFreq(channelId, 0.0);
    SetTXAPreGenRun(channelId, 0);
    SetTXAPanelRun(channelId, 1);
    SetTXAPanelSelect(channelId, 2);
    SetTXAPostGenRun(channelId, 0);
    qCInfo(lcDsp) << "TX channel" << channelId
                  << "init: ALC max-gain capped at 0 dB (per deskhpsdr [@120188f])";
#endif

    // C.2 [3M-1a]: Construct the TxChannel C++ wrapper around the WDSP TXA
    // pipeline that OpenChannel(type=1) already built in WDSP-managed memory.
    // The 31 TXA stages (create_txa()) are live; TxChannel provides the typed
    // C++ facade.  unique_ptr destructor handles cleanup automatically on erase().
    //
    // Bench fix round 3 (Issue A): pass inputBufferSize and outputBufferSize so
    // TxChannel sizes its fexchange0 buffers correctly.  (3M-1c TX pump v3
    // changed the production callsite from fexchange2 → fexchange0; the
    // sizing math is identical, only the buffer layout differs.)
    //   outputBufferSize = inputBufferSize × outputSampleRate / inputSampleRate
    // At 48 kHz in / 48 kHz out (P1/HL2): 64 × 1 = 64.
    // At 48 kHz in / 192 kHz out (P2 Saturn): 64 × 4 = 256.
    //
    // Integer multiply-then-divide is safe here: inputBufferSize (64) × outputSampleRate
    // (192000 max) = 12,288,000 — well within int32 range.
    // From Thetis wdsp/cmaster.c:179-183 [v2.10.3.13] — in_size / ch_outrate.
    // From Thetis wdsp/cmsetup.c:106-110 [v2.10.3.13] — getbuffsize(48000)==64.
    const int outputBufferSize = inputBufferSize * outputSampleRate / inputSampleRate;
    auto wrapper = std::make_unique<TxChannel>(channelId, inputBufferSize, outputBufferSize, this);
    TxChannel* raw = wrapper.get();
    m_txChannels.emplace(channelId, std::move(wrapper));

    qCInfo(lcDsp) << "Created TX channel" << channelId;
    return raw;
}

void WdspEngine::destroyTxChannel(int channelId)
{
    auto it = m_txChannels.find(channelId);
    if (it == m_txChannels.end()) {
        return;   // idempotent — not found, nothing to do
    }

#ifdef HAVE_WDSP
    // Deactivate with drain before closing.
    // dmode=1: drain-mode close (mirrors destroyRxChannel pattern).
    SetChannelState(channelId, 0, 1);

    // Close the WDSP TX channel.
    CloseChannel(channelId);
#endif

    m_txChannels.erase(it);
    qCInfo(lcDsp) << "Destroyed TX channel" << channelId;
}

TxChannel* WdspEngine::txChannel(int channelId) const
{
    auto it = m_txChannels.find(channelId);
    if (it != m_txChannels.end()) {
        return it->second.get();
    }
    return nullptr;
}

} // namespace NereusSDR
