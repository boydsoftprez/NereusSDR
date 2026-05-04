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
//   2026-05-03 — Phase 3M-3a-iii Task 20 by J.J. Boyd (KG4VCF):
//                 createTxChannel now ports cmaster.c:130-157 [v2.10.3.13]
//                 create_dexp call (the 26-arg DEXP DSP-instance allocation
//                 that was missing from NereusSDR's TX-init path until
//                 today), and destroyTxChannel ports the matching
//                 cmaster.c:267 [v2.10.3.13] destroy_dexp call.
//                 Bench-confirmed VOX-keying failure root cause: pdexp[1]
//                 was permanently nullptr because create_dexp was never
//                 called, so every SetDEXP* setter and the pushvox
//                 callback registration silently no-op'd via their
//                 null guards.  See the new comment block at the
//                 create_dexp callsite below for the full root-cause /
//                 buffer-architecture narrative.
//                 AI-assisted transformation via Anthropic Claude Code.
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
    //
    // Always run wisdom load/regenerate on a worker thread with progress UI.
    // We previously had a "fast path" that called WDSPwisdom synchronously on
    // the main thread when wdspWisdom00 already existed, on the assumption
    // that sync load is instant.  That assumption is wrong: WDSPwisdom
    // silently regenerates any plans missing from the cached file, which
    // can take minutes — the main thread freezes (beach ball on macOS) with
    // no progress dialog because wisdomProgress is never emitted.
    //
    // The MainWindow handler at MainWindow.cpp:583 already guards
    //   if (!m_wisdomDialog && percent < 100) { create dialog; }
    // so a genuinely cached/fast load that completes before the 250 ms poll
    // never pops a dialog.  Only sub-poll fast loads stay silent — which is
    // fine, the user doesn't need feedback for a sub-second operation.
    QByteArray configPath = m_configDir.toUtf8();

    qCInfo(lcDsp) << "Initializing WDSP wisdom on background thread"
                  << "(load if cached, regenerate any missing plans)";

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

    // ── Phase 3M-3a-iii Task 20: create_dexp (DEXP DSP instance) ────────────
    //
    // Until 2026-05-03 NereusSDR omitted this call entirely.  Symptom:
    // pdexp[m_channelId] was permanently nullptr, every SetDEXP* setter
    // and the SendCBPushDexpVox callback registration silently no-op'd
    // via their null guards (see TxChannel::registerVoxCallback at
    // TxChannel.cpp:527-530 [v2.10.3.13] and the matching guards in
    // setVoxRun, setDexpRun, etc.), and VOX-keying never engaged MOX
    // because xdexp() never ran (no DEXP module to drive).
    // Bench-confirmed by JJ on ANAN-G2:
    //   `[VOXDIAG] registerVoxCallback ch= 1 SKIPPED: pdexp NULL`
    //
    // Buffer architecture: PARALLEL-ONLY.  Thetis at cmaster.c:134-135
    // [v2.10.3.13] passes the SAME `pcm->in[in_id]` buffer to both the
    // `in` and `out` parameters of create_dexp — DEXP runs in-place on
    // ChannelMaster's mic ring buffer, and the same buffer is then read
    // by fexchange0 at cmaster.c:389 (chain-inserted).  NereusSDR
    // separates the two: m_dexpBuffers[id] is a private buffer used
    // ONLY by the DEXP detector, and TxWorkerThread's m_in is the
    // separate buffer that fexchange0 reads.  TxWorkerThread::
    // dispatchOneBlock copies a snapshot of m_in into m_dexpBuffers[id]
    // (via TxChannel::pumpDexp) before calling xdexp().  Trade-off:
    // VOX-keying works (the bench-failing feature), but DEXP audio
    // expansion (run_dexp=1) modifies m_dexpBuffers[id] which is then
    // discarded — operators using DEXP for audio gating get no audio
    // change.  Operators rarely use DEXP audio gating without VOX
    // anyway, so this is low priority for follow-up.  A chain-inserted
    // architecture (matching Thetis exactly) needs WdspEngine to own
    // the mic-ring buffer and TxWorkerThread to read from it — a more
    // disruptive refactor that is deferred.
    //
    // Default args sourced verbatim from Thetis cmaster.c:130-157
    // [v2.10.3.13] — every value matches the upstream call site exactly.
    // The pushvox parameter is nullptr at create time; TxChannel's
    // constructor calls registerVoxCallback() shortly after this
    // function returns, which calls SendCBPushDexpVox with the real
    // callback — pdexp[id] is non-null at that point so the
    // registration succeeds.
    //
    // Buffer sizing: 2 * inputBufferSize doubles for interleaved I/Q
    // (complex) samples — matches Thetis's complex-sample layout
    // (cmaster.c:285 [v2.10.3.13]: getbuffsize(rate) * sizeof(complex)).
    //
    // Order of operations: allocate buffer FIRST so the pointer passed
    // to create_dexp is stable for the entire DEXP lifetime (the WDSP
    // module retains the raw pointer set here until destroy_dexp clears
    // it).  emplace returns an iterator to the newly-inserted element;
    // we use that to extract a stable double* — std::map iterators are
    // never invalidated by other map modifications, and std::vector's
    // data pointer is stable across reads (we never resize this buffer
    // after create_dexp captures the pointer).
    auto [dexpIt, inserted] = m_dexpBuffers.emplace(
        channelId,
        std::vector<double>(static_cast<size_t>(inputBufferSize) * 2, 0.0));
    double* dexpBuf = dexpIt->second.data();

    // From Thetis ChannelMaster cmaster.c:130-157 [v2.10.3.13] — verbatim
    // create_dexp call site.  Every argument matches the upstream value;
    // the only deviations are:
    //   - id        : NereusSDR's WDSP channel id (1) instead of Thetis's
    //                 transmitter index (0) — they happen to coincide for
    //                 single-RX layouts but the semantics differ slightly
    //   - in / out  : NereusSDR's private dexpBuf (parallel-only — see
    //                 buffer architecture comment above) instead of
    //                 Thetis's pcm->in[in_id] (chain-inserted)
    //   - pushvox   : nullptr — TxChannel::registerVoxCallback registers
    //                 the real callback later via SendCBPushDexpVox
    //                 (avoids ordering problems; see comment above)
    create_dexp(
        channelId,                // transmitter id, txid
        0,                        // dexp initially set to OFF
        inputBufferSize,          // input buffer size
        dexpBuf,                  // input buffer
        dexpBuf,                  // output buffer
        inputSampleRate,          // sample-rate
        0.01,                     // detector smoothing time-constant
        0.025,                    // attack time
        0.100,                    // release time
        1.000,                    // hold time
        4.000,                    // expansion ratio
        0.750,                    // hysteresis ratio
        0.050,                    // attack threshold
        256,                      // 256 taps for side-channel filter
        0,                        // BH-4 window for side-channel filter
        1000.0,                   // low-cut for side-channel filter
        2000.0,                   // high-cut for side-channel filter
        0,                        // side-channel filter initially set to OFF
        // DEVIATION from Thetis cmaster.c:149 [v2.10.3.13] which passes 1
        // ("VOX initially set to ON"). Thetis relies on its CMSetTXAVoxRun
        // init pump firing SetDEXPRunVox(0) BEFORE the audio thread starts
        // processing mic data (Audio.VOXEnabled defaults false). NereusSDR's
        // MoxController only fires voxRunRequested on state CHANGES; at
        // startup voxEnabled=false equals m_lastVoxRunGated=false so no
        // emit happens, and a Thetis-faithful run_vox=1 boot value would
        // stay at 1 until the user toggles VOX, causing every mic envelope
        // crossing to fire pushvox -> onVoxActive(true) -> setMox(true)
        // immediately on connect. Caught at bench v5 (2026-05-04).
        // Boot run_vox=0 matches the Thetis-equivalent post-init state.
        0,                        // VOX initially OFF (NereusSDR deviation; see comment)
        1,                        // audio delay initially set to ON
        0.060,                    // audio delay set to 60ms
        nullptr,                  // pushvox: registered later by TxChannel::registerVoxCallback
        0,                        // anti-vox 'run' flag
        inputBufferSize,          // anti-vox data buffer size (complex samples)
        inputSampleRate,          // anti-vox data sample-rate
        0.01,                     // anti-vox gain
        0.01);                    // anti-vox smoothing time-constant
    qCInfo(lcDsp) << "TX channel" << channelId
                  << "DEXP created (parallel-only buffer architecture):"
                  << "size=" << inputBufferSize
                  << "rate=" << inputSampleRate
                  << "buf=" << static_cast<const void*>(dexpBuf);
    Q_UNUSED(inserted);   // emplace always inserts here (early-return guards above ensure new key)
#else
    // Non-HAVE_WDSP build (e.g. test runner that links WdspEngine but not
    // the WDSP DSP module): still allocate the buffer slot so map state
    // stays consistent across builds.  Without this, destroyTxChannel's
    // erase would silently no-op on the missing key and any future
    // diagnostic that introspects m_dexpBuffers would see a phantom
    // missing entry.  The buffer is unused in non-HAVE_WDSP builds (no
    // DEXP module to feed) but the storage is harmless.
    m_dexpBuffers.emplace(
        channelId,
        std::vector<double>(static_cast<size_t>(inputBufferSize) * 2, 0.0));
#endif

    // C.2 [3M-1a]: Construct the TxChannel C++ wrapper around the WDSP TXA
    // pipeline that OpenChannel(type=1) already built in WDSP-managed memory.
    // The 31 TXA stages (create_txa()) are live; TxChannel provides the typed
    // C++ facade.  unique_ptr destructor handles cleanup automatically on erase().
    //
    // Parent argument: nullptr — DO NOT pass `this` here.  RadioModel::
    // connectToRadio does m_txChannel->moveToThread(workerThread) shortly
    // after createTxChannel returns, and Qt's invariant is that a QObject
    // with a parent CANNOT be moved across threads (silent failure with one
    // warning, then TxWorkerThread::run drains sendPostedEvents on a
    // wrong-thread channel forever, generating an event-flood storm).
    // Ownership stays with std::unique_ptr in m_txChannels — the Qt parent
    // is redundant.  See tst_wdsp_engine_tx_channel.cpp:
    // createdTxChannelHasNoQtParentForThreadAffinity for the regression test.
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
    auto wrapper = std::make_unique<TxChannel>(channelId, inputBufferSize, outputBufferSize, nullptr);
    TxChannel* raw = wrapper.get();

    // Phase 3M-3a-iii Task 20: hand the wrapper a non-owning pointer to the
    // per-channel DEXP buffer so TxChannel::pumpDexp has a valid destination
    // for its per-block memcpy.  The wrapper holds a raw pointer; ownership
    // stays with WdspEngine's m_dexpBuffers map.  Lifetime is safe because
    // destroyTxChannel destroys the WDSP DEXP module (destroy_dexp) AND the
    // wrapper (m_txChannels.erase) BEFORE erasing m_dexpBuffers — the buffer
    // outlives every consumer that could read it.
    {
        auto bufIt = m_dexpBuffers.find(channelId);
        if (bufIt != m_dexpBuffers.end()) {
            raw->setDexpBuffer(bufIt->second.data(), bufIt->second.size());
        }
    }

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

    // Phase 3M-3a-iii Task 20: tear down the DEXP DSP module before closing
    // the channel.  Mirrors Thetis cmaster.c:267 [v2.10.3.13] which calls
    // destroy_dexp(i) BEFORE CloseChannel at cmaster.c:265 inside
    // destroy_xmtr.  After destroy_dexp returns, pdexp[channelId] is nullptr
    // again, which restores the pre-create_dexp state — any callback already
    // in flight on the WDSP audio worker thread becomes a no-op when it
    // executes (TxChannel::unregisterVoxCallback ran ahead of this in the
    // ~TxChannel destructor, which itself ran when m_txChannels.erase()
    // below destroys the unique_ptr — but we'll see that erase happens AFTER
    // this destroy_dexp call, so the order is: WDSP DEXP module dies first,
    // then the C++ wrapper).
    destroy_dexp(channelId);

    // Close the WDSP TX channel.
    CloseChannel(channelId);
#endif

    // Drop the DEXP buffer slot regardless of HAVE_WDSP — the non-HAVE_WDSP
    // createTxChannel branch also stores an entry, so the symmetry must
    // hold for both build configs.  Erase is idempotent on a missing key.
    m_dexpBuffers.erase(channelId);

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
