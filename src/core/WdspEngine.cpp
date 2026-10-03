// =================================================================
// src/core/WdspEngine.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/ChannelMaster/cmaster.c, original licence from Thetis source is included below
//
//   2026-09-24 - R-R3-39: FFTW's planner made thread-safe once per process
//                (fftw_make_planner_thread_safe), because WDSP's planning
//                calls run on more than one thread. NereusSDR-original.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24 - R-R3-39: the receive lane. RX channel create, destroy,
//                rebuild and rate change run as lane barriers that quiesce
//                the DSP worker; the PS feedback channel's WDSP calls and
//                the S-meter reads go through the lane; the RX channel map
//                is lock-guarded. NereusSDR-original. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
// =================================================================
// Modification history (NereusSDR):
//   2026-10-01 - #299: deferred real initialization test seam for startup
//                 wiring coverage. J.J. Boyd (KG4VCF), assisted by OpenAI Codex.
//   2026-09-30  J.J. Boyd / KG4VCF  RADE reason: test seam (createRadeChannel honours setRadeCreateFailsForTest and setRadeStartFailsForTest).
//                                    AI-assisted via Anthropic Claude Code.
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
//   2026-09-22 — Correct TX wrapper/DEXP teardown lifetime and upstream close
//                 ordering. J.J. Boyd (KG4VCF), assisted by OpenAI Codex.
//   2026-09-23 - nereusd thread placement (R-R3-41): the WDSP thread-start
//                 hook is installed when placement is on, the wisdom thread
//                 plans on a fast core, and each closed channel's threads
//                 are forgotten. J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-09-25 - R-R3-39: the TX destroy and rebuild barriers free the
//                 wrapper's TCI transmit resampler on the transmit lane.
//                 J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
//   2026-09-26 - R-R3-39: the thread-safe planner call moves to
//                 makeFftwPlannersThreadSafe() (FftwPlanner.cpp), which also
//                 covers single-precision FFTW. NereusSDR-original. J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.
//   2026-09-30 - RADE threads: createRadeChannel names the channel with its
//                 id; destroyRadeChannel joins the channel's decoder thread
//                 before stopping the codec. NereusSDR-original. J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.
//   2026-09-30 - RD-I9: the receive lane's destroy and rebuild barriers stop
//                 the channel without a drain before closing it, as Thetis's
//                 destroy_rcvr closes without one; the quiesced worker made
//                 every draining stop wait out WDSP's 100 ms bound. A
//                 rebuild reopens at the 48 kHz DSP and output rates the
//                 channel was created with, not its input rate. J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
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
#include "DspControlThread.h"
#include "RxChannel.h"

#include <cmath>
#include "TxChannel.h"
#include "PsFeedbackChannel.h"
#include "RadeChannel.h"
#include "AppSettings.h"
#include "LogCategories.h"
#include "wdsp_api.h"
#include "core/platform/ThreadPlacement.h"

#include "core/FftwPlanner.h"  // makeFftwPlannersThreadSafe (R-R3-39)

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QStandardPaths>
#include <QTimer>
#include <QThread>

#include <mutex>

#ifdef HAVE_WDSP
// Opaque WDSP lookup slot, as in TxChannel.cpp. dexp.h requires internal
// C DSP types; the owner only needs to clear this pointer after destruction.
extern "C" {
struct _dexp;
extern _dexp* pdexp[];
}
#endif

namespace NereusSDR {

// R-R3-39: WDSP plans its FFTs with double-precision FFTW, and FFTW's
// planner is not thread-safe by default. The receive lane (every RX
// channel's OpenChannel, SetInputSamplerate, RXASetNC, SetDSPBuffsize) and
// the transmit side (TX OpenChannel, TXASetNC; the transmit lane from Task
// 32) plan concurrently, and two threads in the planner at once corrupt
// its hash table and spin (seen as a hung connect: createTxChannel and the
// PS feedback OpenChannel both inside fftw's hinsert0). Made thread-safe
// once per process, before any plan or wisdom import. The operator chose
// FFTW's thread-safe planner (2026-09-24). makeFftwPlannersThreadSafe()
// (FftwPlanner.h) covers both precisions, since the display FFTs plan with
// single-precision FFTW in this same process.

WdspEngine::WdspEngine(QObject* parent)
    : QObject(parent)
{
    // Before anything this engine does can plan (a test may open channels
    // without initialize()).
    makeFftwPlannersThreadSafe();
#ifdef HAVE_WDSP
    m_extDivCreate = &create_divEXT;
    m_extDivDestroy = &destroy_divEXT;
    m_extDivProcess = &xdivEXT;
    m_extDivSetRun = &SetEXTDIVRun;
    m_extDivSetNr = &SetEXTDIVNr;
    m_extDivSetOutput = &SetEXTDIVOutput;
    m_extDivSetRotate = &SetEXTDIVRotate;
#endif
}

WdspEngine::~WdspEngine()
{
    // shutdown() also owns the process-wide external-diversity slots and
    // deliberately cleans them even when asynchronous wisdom initialization
    // never completed.
    shutdown();
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

void WdspEngine::prepareConfigDir(const QString& configDir)
{
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
}

bool WdspEngine::setNnrModelPaths(const std::array<QString, 2>& paths, QString* reason)
{
    if (reason) reason->clear();
    bool haveRxChannels = false;
    {
        std::shared_lock<std::shared_mutex> lock(m_rxChannelsMutex);
        haveRxChannels = !m_rxChannels.empty();
    }
    if (m_initialized || m_initializationInProgress || haveRxChannels) {
        if (reason) *reason = QStringLiteral("NNR model changes apply after the radio is disconnected and connected again.");
        return false;
    }
    for (const QString& path : paths) {
        const auto encoded = QFile::encodeName(path);
        if (encoded.size() >= 512 || encoded.contains('\0')) {
            if (reason) *reason = QStringLiteral("The NNR model path exceeds the engine's supported length.");
            return false;
        }
    }
    m_nnrModelPaths = paths;
    return true;
}

bool WdspEngine::initialize(const QString& configDir)
{
    if (m_initialized) {
        qCWarning(lcDsp) << "WdspEngine already initialized";
        return true;
    }

#ifdef HAVE_WDSP
    if (m_initializationInProgress) {
        qCWarning(lcDsp) << "WDSP initialization is already in progress";
        return false;
    }
    m_initializationInProgress = true;
    // R-R3-39: before the wisdom import below and before any channel plans
    // (see makeFftwPlannersThreadSafe).
    makeFftwPlannersThreadSafe();
#ifndef Q_OS_WIN
    // R-R3-41: nereusd places each channel's worker as it starts. Installed
    // before any channel opens; the GUI never turns placement on.
    if (ThreadPlacement::instance().isActive()) {
        WDSPSetThreadStartHook(&ThreadPlacement::wdspThreadStartHook);
    }
#endif
    // Empty explicitly selects embedded weights, avoiding upstream's default
    // search in the working directory. Both process-global paths are fixed
    // before finishInitialization can open even a feedback receiver.
    for (int slot = 0; slot < 2; ++slot) {
        const auto path = QFile::encodeName(m_nnrModelPaths[slot]);
        SetNNRModelPathSlot(slot, path.constData());
    }
    prepareConfigDir(configDir);

#ifdef NEREUS_BUILD_TESTS
    if (m_deferredInitForTest) {
        QTimer::singleShot(0, this, [this]() {
            finishInitialization(/*wisdomWasRebuilt=*/false);
        });
        return false;
    }
    if (m_synchronousInitForTest) {
        // Test-only path (remote-daemon R2 Task 2): never spawns
        // WisdomThread, never calls WDSPwisdom(). finishInitialization()
        // still runs for real -- impulse cache init, PS feedback channel
        // open, m_initialized=true, initializedChanged(true) emitted --
        // so the engine is genuinely functional afterward, not merely
        // flagged. See setSynchronousInitForTest()'s doc comment in
        // WdspEngine.h for why this branch lives inside initialize()
        // instead of a separate method called ahead of it.
        qCInfo(lcDsp) << "WdspEngine: synchronous test-only init"
                          " (WisdomThread not spawned)";
        finishInitialization(/*wisdomWasRebuilt=*/false);
        return true;
    }
#endif

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
    // Detect whether wisdom file already exists — controls impulse-cache
    // deletion AND the wisdomWasRebuilt flag passed to finishInitialization()
    // (Task 4.3 needs this to decide whether to load a stale on-disk impulse
    // cache after a wisdom rebuild).
    const bool needsGeneration = needsWisdomGeneration(m_configDir);

    // Wisdom rebuild also invalidates any saved impulse cache —
    // From Thetis radio.cs:140-152 [v2.10.3.13]: delete impulse_cache.bin
    // when wisdom is rebuilt so we start afresh.
    if (needsGeneration) {
        QString cacheFile = m_configDir + QStringLiteral("impulse_cache.bin");
        if (QFile::exists(cacheFile)) {
            QFile::remove(cacheFile);
            qCInfo(lcDsp) << "Deleted stale impulse cache (wisdom rebuilt)";
        }
    }

    QByteArray configPath = m_configDir.toUtf8();

    qCInfo(lcDsp) << "Initializing WDSP wisdom on background thread"
                  << "(load if cached, regenerate any missing plans)"
                  << "needsGeneration=" << needsGeneration;

    auto* wisdomThread = QThread::create([configPath]() {
        // FFTW times its plans on the core it runs on; with placement on
        // (nereusd), plan on the core signal processing will use.
        ThreadPlacement::instance().placeCurrentThreadOnFastCore();
        WDSPwisdom(const_cast<char*>(configPath.constData()));
    });
    wisdomThread->setObjectName(QStringLiteral("WisdomThread"));

#ifdef NEREUS_BUILD_TESTS
    // Test-only observability (remote-daemon R2 Task 2): record that this
    // engine instance took the async path and actually constructed the
    // WisdomThread. See wisdomThreadSpawnedForTest()'s doc comment in
    // WdspEngine.h. No production behaviour: this is a single bool write
    // guarded out of non-test builds entirely.
    m_wisdomThreadSpawnedForTest = true;
#endif

    // Poll wisdom_get_status() for progress updates
    auto* pollTimer = new QTimer(this);
    pollTimer->setInterval(250);

    connect(wisdomThread, &QThread::finished, this,
            [this, wisdomThread, pollTimer, needsGeneration]() {
        pollTimer->stop();
        pollTimer->deleteLater();
        wisdomThread->deleteLater();
        emit wisdomProgress(100, QStringLiteral("FFTW planning complete"));
        finishInitialization(/*wisdomWasRebuilt=*/needsGeneration);
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

void WdspEngine::finishInitialization(bool wisdomWasRebuilt)
{
    m_initializationInProgress = false;
#ifdef HAVE_WDSP
    qCInfo(lcDsp) << "WDSP wisdom initialized";

    // Read AppSettings flags.
    // From Thetis radio.cs:153-158 [v2.10.3.13] — CacheImpulse/CacheImpulseSaveRestore.
    const auto& s = AppSettings::instance();
    const bool cacheEnabled  = s.value("DspOptionsCacheImpulse",            "False").toString() == "True";
    const bool saveRestore   = s.value("DspOptionsCacheImpulseSaveRestore",  "False").toString() == "True";

    // From Thetis radio.cs:153 [v2.10.3.13]:
    //   WDSP.init_impulse_cache(_cache_impulse ? 1 : 0);
    // init_impulse_cache allocates the internal cache structure (use=1) or
    // sets it up in disabled mode (use=0). Must be called before any channel
    // is opened. Takes effect at channel-create time.
    init_impulse_cache(cacheEnabled ? 1 : 0);
    qCInfo(lcDsp) << "WDSP impulse cache" << (cacheEnabled ? "enabled" : "disabled");

    // From Thetis radio.cs:155-158 [v2.10.3.13]:
    //   if (_cache_impulse_save_restore && !rebuilt)
    //       WDSP.read_impulse_cache(...);
    // Skip loading if wisdom was just rebuilt — the old cache is stale.
    if (saveRestore && !wisdomWasRebuilt) {
        QString cacheFile = m_configDir + QStringLiteral("impulse_cache.bin");
        if (QFile::exists(cacheFile)) {
            QByteArray cachePath = cacheFile.toUtf8();
            int cacheResult = read_impulse_cache(cachePath.constData());
            qCDebug(lcDsp) << "Impulse cache loaded from disk, result:" << cacheResult;
        }
    }

    m_initialized = true;

    // Phase 3M-4 Task 4: open the PureSignal feedback RX channel.  Unlike
    // RX1 / TX channels (which are opened lazily by RadioModel::
    // connectToRadio() because they depend on per-board sample rates), the
    // PS feedback channel can be opened with the G2-class default rate
    // (192000); the PureSignal coordinator (Task 7) re-applies rx1_rate
    // for HL2 boards before MOX.  Opening here keeps the lifecycle
    // symmetric with init_impulse_cache (one-shot, engine-owned).
    openPsFeedbackChannel();

    emit initializedChanged(true);
    qCInfo(lcDsp) << "WDSP initialized successfully";
#else
    Q_UNUSED(wisdomWasRebuilt);
    m_initialized = true;
    emit initializedChanged(true);
#endif
}

void WdspEngine::shutdown()
{
    // External diversity accepts samples outside the RXA channel map, so it
    // must be stopped and destroyed before any channel teardown. This also
    // runs when m_initialized is false: a test seam or a partially completed
    // startup can own pdiv[] without owning a finished WDSP engine.
    destroyAllExternalDiversity();

    if (!m_initialized) {
        return;
    }

    qCInfo(lcDsp) << "Shutting down WDSP...";

    // R-R3-39: every receive-lane job already queued runs first, so none of
    // them reaches a channel this teardown closes.
    drainReceiveLane();

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
    // R-R3-39: the TX closes above are transmit-lane barriers; they finish
    // (with every transmit job queued before them) before the receive side
    // is torn down, keeping WDSP's TX → RX teardown order.
    drainTransmitLane();

    // Phase 3M-4 Task 4: close the PS feedback RX channel BEFORE the
    // primary RX channels.  Although calcc reads autonomously, the
    // PureSignal coordinator (Task 7) connects the PS feedback wrapper
    // back to the codec sample stream — closing it first ensures no
    // PS-feedback writes happen against a torn-down WDSP channel.
    closePsFeedbackChannel();

    // Destroy all RX channels (collect IDs first to avoid iterator invalidation)
    {
        std::vector<int> channelIds;
        {
            std::shared_lock<std::shared_mutex> lock(m_rxChannelsMutex);
            for (const auto& [id, ch] : m_rxChannels) {
                channelIds.push_back(id);
            }
        }
        for (int id : channelIds) {
            destroyRxChannel(id);
        }
    }
    // R-R3-39: the closes above are lane barriers; wait for them before the
    // impulse cache below goes.
    drainReceiveLane();

#ifdef HAVE_WDSP
    // From Thetis radio.cs:163-177 [v2.10.3.13] (DestroyDSP):
    //   if (_cache_impulse && _cache_impulse_save_restore)
    //       WDSP.save_impulse_cache(...)
    //   else
    //       // try to remove file if exists
    //       File.Delete(file)
    {
        const auto& s = AppSettings::instance();
        const bool cacheEnabled = s.value("DspOptionsCacheImpulse",           "False").toString() == "True";
        const bool saveRestore  = s.value("DspOptionsCacheImpulseSaveRestore", "False").toString() == "True";

        QString cacheFile = m_configDir + QStringLiteral("impulse_cache.bin");

        if (cacheEnabled && saveRestore) {
            QByteArray cachePath = cacheFile.toUtf8();
            int saveResult = save_impulse_cache(cachePath.constData());
            qCInfo(lcDsp) << "Impulse cache saved to disk, result:" << saveResult;
        } else {
            // Remove any stale file so a future session with save-restore
            // disabled doesn't accidentally load old data.
            // From Thetis radio.cs:170-175 [v2.10.3.13].
            if (QFile::exists(cacheFile)) {
                QFile::remove(cacheFile);
                qCDebug(lcDsp) << "Removed stale impulse cache file (save-restore disabled)";
            }
        }
    }

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

    {
        std::shared_lock<std::shared_mutex> lock(m_rxChannelsMutex);
        if (m_rxChannels.count(channelId)) {
            qCWarning(lcDsp) << "Channel" << channelId << "already exists";
            return m_rxChannels.at(channelId).get();
        }
    }

    if (m_rxLane != nullptr && !m_rxLane->isCurrentThread()) {
        // R-R3-39: the wrapper exists at once (silent until its channel is
        // open) and takes setters straight away; they queue behind the
        // barrier that opens the WDSP channel. No parent: the map owns it,
        // and a retired wrapper is deleted after its lane barrier.
        auto channel = std::make_unique<RxChannel>(channelId, inputBufferSize,
                                                   inputSampleRate, m_rxLane, nullptr);
        RxChannel* ptr = channel.get();
        ptr->setWdspReady(false);
        {
            std::unique_lock<std::shared_mutex> lock(m_rxChannelsMutex);
            m_rxChannels.emplace(channelId, std::move(channel));
        }
        m_rxLane->postBarrier([this, ptr, channelId, inputBufferSize, dspBufferSize,
                               inputSampleRate, dspSampleRate, outputSampleRate]() {
            openRxChannelWdsp(channelId, inputBufferSize, dspBufferSize,
                              inputSampleRate, dspSampleRate, outputSampleRate);
            // NbFamily's anb/nob objects, as the constructor makes them
            // without a lane.
            ptr->createWdspObjectsOnLane();
            // Admit the channel to processIq with the worker quiesced, so no
            // block starts half way through the switch.
            const std::function<void()> release = quiesceRxWorker();
            ptr->setWdspReady(true);
            if (release) {
                release();
            }
        });
#ifdef HAVE_WDSP
        // Queued behind the barrier above (see the no-lane path's note).
        if (auto* nb = ptr->nb()) { nb->seedSnbFromSettings(); }
#endif
        emit rxChannelCreated(channelId);
        return ptr;
    }

#ifdef HAVE_WDSP
    openRxChannelWdsp(channelId, inputBufferSize, dspBufferSize,
                      inputSampleRate, dspSampleRate, outputSampleRate);
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

    {
        std::unique_lock<std::shared_mutex> lock(m_rxChannelsMutex);
        m_rxChannels.emplace(channelId, std::move(channel));
    }
    emit rxChannelCreated(channelId);
    return ptr;
}

void WdspEngine::openRxChannelWdsp(int channelId, int inputBufferSize, int dspBufferSize,
                                   int inputSampleRate, int dspSampleRate,
                                   int outputSampleRate)
{
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
#else
    Q_UNUSED(channelId);
    Q_UNUSED(inputBufferSize);
    Q_UNUSED(dspBufferSize);
    Q_UNUSED(inputSampleRate);
    Q_UNUSED(dspSampleRate);
    Q_UNUSED(outputSampleRate);
#endif
}

void WdspEngine::destroyRxChannel(int channelId)
{
    std::unique_ptr<RxChannel> owned;
    {
        std::unique_lock<std::shared_mutex> lock(m_rxChannelsMutex);
        auto it = m_rxChannels.find(channelId);
        if (it == m_rxChannels.end()) {
            return;
        }
        owned = std::move(it->second);
        m_rxChannels.erase(it);
    }

    if (m_rxLane != nullptr && !m_rxLane->isCurrentThread()) {
        // R-R3-39: gone from the map at once; the lane closes it after every
        // job already queued for it, with the DSP worker quiesced, and the
        // wrapper is deleted back on this thread once nothing can reach it.
        std::shared_ptr<RxChannel> channel(std::move(owned));
        m_rxLane->postBarrier([this, channel, channelId]() mutable {
            const std::function<void()> release = quiesceRxWorker();
            channel->markRetired();
#ifdef HAVE_WDSP
            // Deactivate without a drain (RD-I9): the worker is quiesced, so
            // no I/Q reaches the channel and a draining stop could only wait
            // out WDSP's 100 x Sleep(1) bound (channel.c:288-304). Thetis
            // closes its receive channels with no drain at all:
            //   From Thetis ChannelMaster/cmaster.c:102-103 [v2.10.3.15]
            //     for (j = 0; j < pcm->cmSubRCVR; j++)
            //         CloseChannel (chid (inid (0, i), j));
            SetChannelState(channelId, 0, 0);
            // Close the WDSP channel
            CloseChannel(channelId);
            // Its worker and flush threads are gone; their IDs may be reused.
            ThreadPlacement::instance().forgetChannel(channelId);
#endif
            // NB / NB2 destroy, as ~NbFamily did after CloseChannel.
            channel->destroyWdspObjectsOnLane();
            if (release) {
                release();
            }
            retireRxChannel(std::move(channel));
            qCInfo(lcDsp) << "Destroyed RX channel" << channelId;
        });
        return;
    }

#ifdef HAVE_WDSP
    // Deactivate with drain
    SetChannelState(channelId, 0, 1);

    // NB / NB2 destroy is owned by ~NbFamily inside ~RxChannel — see
    // src/core/NbFamily.h. Do NOT re-add destroy_anbEXT/nobEXT here.

    // Close the WDSP channel
    CloseChannel(channelId);
    // Its worker and flush threads are gone; their IDs may be reused.
    ThreadPlacement::instance().forgetChannel(channelId);
#endif

    owned.reset();
    qCInfo(lcDsp) << "Destroyed RX channel" << channelId;
}

RxChannel* WdspEngine::rxChannel(int channelId) const
{
    std::shared_lock<std::shared_mutex> lock(m_rxChannelsMutex);
    auto it = m_rxChannels.find(channelId);
    if (it != m_rxChannels.end()) {
        return it->second.get();
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// External Diversity lifecycle
// ---------------------------------------------------------------------------
//
// Thetis owns pdiv[0] at ChannelMaster scope, not inside an RXA channel:
// CreateRadio -> create_sync -> create_divEXT, InboundBlock -> xdivEXT, and
// DestroyRadio -> destroy_sync -> destroy_divEXT. From Thetis
// ChannelMaster/cmsetup.c:89-102 and sync.c:32-51
// [v2.10.3.15].
//
// The WDSP C API performs no id or lifetime validation and dereferences
// pdiv[id] directly (div.c:104-186), so every public method below validates
// the two-slot range and the created/running state before crossing the ABI.

bool WdspEngine::createExternalDiversity(int id, int inputs,
                                         int complexSamples)
{
    if (!validExternalDiversityId(id) || inputs <= 0 || inputs > 8
        || complexSamples <= 0 || !m_extDivCreate) {
        return false;
    }

    ExternalDiversitySlot& slot =
        m_externalDiversity[static_cast<size_t>(id)];
    if (slot.created.load(std::memory_order_acquire)) {
        return true;
    }

    // Upstream create_sync creates stopped, then configuration/run are
    // applied later by the console. Preserve that ordering.
    m_extDivCreate(id, 0, inputs, complexSamples);
    slot.inputs = inputs;
    slot.complexSamples = complexSamples;
    slot.running.store(false, std::memory_order_relaxed);
    slot.created.store(true, std::memory_order_release);
    return true;
}

void WdspEngine::configureExternalDiversity(int id, int output,
                                            const double* iRotate,
                                            const double* qRotate,
                                            int inputs)
{
    if (!validExternalDiversityId(id) || inputs <= 0 || inputs > 8
        || output < 0 || output > inputs || !iRotate || !qRotate
        || !m_extDivSetNr || !m_extDivSetOutput || !m_extDivSetRotate) {
        return;
    }

    ExternalDiversitySlot& slot =
        m_externalDiversity[static_cast<size_t>(id)];
    if (!slot.created.load(std::memory_order_acquire)) {
        return;
    }

    // output == nr selects WDSP's mixed output (div.c:166-174). Nr must be
    // installed first so the output selector and rotation length describe
    // the same input set.
    m_extDivSetNr(id, inputs);
    m_extDivSetOutput(id, output);
    m_extDivSetRotate(id, inputs, const_cast<double*>(iRotate),
                      const_cast<double*>(qRotate));
    slot.inputs = inputs;
}

bool WdspEngine::processExternalDiversity(int id, int complexSamples,
                                          double** inputs, double* output)
{
    if (!validExternalDiversityId(id) || complexSamples <= 0 || !inputs
        || !output || !m_extDivProcess) {
        return false;
    }

    ExternalDiversitySlot& slot =
        m_externalDiversity[static_cast<size_t>(id)];
    if (!slot.created.load(std::memory_order_acquire)
        || !slot.running.load(std::memory_order_acquire)) {
        return false;
    }
    for (int input = 0; input < slot.inputs; ++input) {
        if (!inputs[input]) {
            return false;
        }
    }

    m_extDivProcess(id, complexSamples, inputs, output);
    return true;
}

void WdspEngine::setExternalDiversityRunning(int id, bool running)
{
    if (!validExternalDiversityId(id) || !m_extDivSetRun) {
        return;
    }

    ExternalDiversitySlot& slot =
        m_externalDiversity[static_cast<size_t>(id)];
    if (!slot.created.load(std::memory_order_acquire)) {
        return;
    }

    const bool current = slot.running.load(std::memory_order_acquire);
    if (current == running) {
        return;
    }

    if (running) {
        m_extDivSetRun(id, 1);
        slot.running.store(true, std::memory_order_release);
    } else {
        // Close the process gate before touching the C object so no new
        // worker call enters xdivEXT while the slot is being stopped.
        slot.running.store(false, std::memory_order_release);
        m_extDivSetRun(id, 0);
    }
}

void WdspEngine::destroyExternalDiversity(int id)
{
    if (!validExternalDiversityId(id) || !m_extDivDestroy) {
        return;
    }

    ExternalDiversitySlot& slot =
        m_externalDiversity[static_cast<size_t>(id)];
    if (!slot.created.load(std::memory_order_acquire)) {
        return;
    }

    if (slot.running.exchange(false, std::memory_order_acq_rel)
        && m_extDivSetRun) {
        m_extDivSetRun(id, 0);
    }
    slot.created.store(false, std::memory_order_release);
    m_extDivDestroy(id);
    slot.inputs = 0;
    slot.complexSamples = 0;
}

void WdspEngine::destroyAllExternalDiversity()
{
    for (int id = 0; id < kExternalDiversitySlots; ++id) {
        destroyExternalDiversity(id);
    }
}

#ifdef NEREUS_BUILD_TESTS
void WdspEngine::setExternalDiversityApiForTest(
    const ExternalDiversityApiForTest& api)
{
    m_extDivCreate = api.create;
    m_extDivDestroy = api.destroy;
    m_extDivProcess = api.process;
    m_extDivSetRun = api.setRun;
    m_extDivSetNr = api.setNr;
    m_extDivSetOutput = api.setOutput;
    m_extDivSetRotate = api.setRotate;
}
#endif

// ─────────────────────────────────────────────────────────────────────────
// Phase 3R Task J2: RadeChannel lifecycle.
// ─────────────────────────────────────────────────────────────────────────
//
// RadeChannel is a NereusSDR-native wrapper around third_party/rade (the
// librade neural codec).  It is NOT a WDSP channel - no OpenChannel /
// CloseChannel calls, no m_initialized requirement.  The methods below
// are structurally parallel to createRxChannel / destroyRxChannel /
// rxChannel (WdspEngine.cpp:356-468) but the WDSP-side bookkeeping is
// absent because WDSP has no concept of RADE.

RadeChannel* WdspEngine::createRadeChannel(int channelId)
{
    // Pre-existence guard, structural parallel to the createRxChannel
    // guard at WdspEngine.cpp:368-371: a second create call with the
    // same id returns the existing channel rather than leaking a fresh
    // construction.  Callers (J3 setDspMode swap) are responsible for
    // sequencing destroy-then-create when intentional replacement is
    // needed.
    auto existing = m_radeChannels.find(channelId);
    if (existing != m_radeChannels.end()) {
        qCWarning(lcDsp) << "RadeChannel" << channelId
                         << "already exists; returning existing pointer";
        return existing->second.get();
    }

    // Parent the channel to `this` so QObject ownership cleans up the
    // wrapper if WdspEngine is destroyed without an explicit
    // destroyRadeChannel(id) call.  unique_ptr deletes the object first;
    // the redundant Qt parent link is harmless because Qt's destructor
    // checks for already-deleted children.
#ifdef NEREUS_BUILD_TESTS
    if (m_radeCreateFailsForTest) {
        return nullptr;
    }
#endif
    auto channel = std::make_unique<RadeChannel>(this);
    RadeChannel* ptr = channel.get();
#ifdef NEREUS_BUILD_TESTS
    ptr->setStartFailsForTest(m_radeStartFailsForTest);
#endif
    // RADE threads: the id names the decoder thread and the tick log.
    ptr->setChannelId(channelId);

    m_radeChannels.emplace(channelId, std::move(channel));
    qCInfo(lcDsp) << "Created RADE channel" << channelId;
    return ptr;
}

void WdspEngine::destroyRadeChannel(int channelId)
{
    auto it = m_radeChannels.find(channelId);
    if (it == m_radeChannels.end()) {
        // Idempotent: destroy on a never-created (or already-destroyed)
        // id is a safe no-op.  Mirrors destroyRxChannel's early-return
        // pattern at WdspEngine.cpp:438-443.
        return;
    }

    // Stop the channel before erasing the wrapper.  RadeChannel::stop()
    // is idempotent (the I1 implementation checks m_active and returns
    // early if already stopped), so this is safe whether or not start()
    // was ever called.
    //
    // RADE threads: join the decoder thread first, so no block is mid-decode
    // when the codec goes. This waits for at most the block in hand.
    it->second->stopRxWorker();
    it->second->stop();

    m_radeChannels.erase(it);
    qCInfo(lcDsp) << "Destroyed RADE channel" << channelId;
}

RadeChannel* WdspEngine::radeChannel(int channelId) const
{
    auto it = m_radeChannels.find(channelId);
    if (it != m_radeChannels.end()) {
        return it->second.get();
    }
    return nullptr;
}

qint64 WdspEngine::rebuildRxChannel(int channelId, const ChannelConfig& cfg)
{
    if (!m_initialized) {
        qCWarning(lcDsp) << "rebuildRxChannel: WDSP not initialized";
        return -1;
    }

    QElapsedTimer timer;
    timer.start();

    std::unique_ptr<RxChannel> old;
    {
        std::unique_lock<std::shared_mutex> lock(m_rxChannelsMutex);
        auto it = m_rxChannels.find(channelId);
        if (it == m_rxChannels.end()) {
            qCWarning(lcDsp) << "rebuildRxChannel: channel" << channelId << "not found";
            return -1;
        }
        old = std::move(it->second);
        m_rxChannels.erase(it);
    }

    // Capture DSP state before tearing down the channel.
    const RxChannelState state = old->captureState();

    if (m_rxLane != nullptr && !m_rxLane->isCurrentThread()) {
        // R-R3-39: the new wrapper replaces the old one in the map at once;
        // the close and the reopen run as one lane barrier after every job
        // already queued for the old wrapper, with the DSP worker quiesced
        // across both. The state is re-applied to the new wrapper after the
        // barrier is posted, so its calls land on the reopened channel.
        std::shared_ptr<RxChannel> retired(std::move(old));
        auto channel = std::make_unique<RxChannel>(channelId, cfg.bufferSize,
                                                   cfg.sampleRate, m_rxLane, nullptr);
        RxChannel* ptr = channel.get();
        ptr->setWdspReady(false);
        {
            std::unique_lock<std::shared_mutex> lock(m_rxChannelsMutex);
            m_rxChannels.emplace(channelId, std::move(channel));
        }
        m_rxLane->postBarrier([this, retired, ptr, channelId, cfg]() mutable {
            const std::function<void()> release = quiesceRxWorker();
            retired->markRetired();
#ifdef HAVE_WDSP
            // Deactivate without a drain before closing, as the lane's
            // destroyRxChannel does (RD-I9): the worker is quiesced, so a
            // draining stop could only time out.
            SetChannelState(channelId, 0, 0);
            // Close the old WDSP channel.
            CloseChannel(channelId);
            ThreadPlacement::instance().forgetChannel(channelId);
#endif
            // NB / NB2 destroy, as ~NbFamily did in the old wrapper.
            retired->destroyWdspObjectsOnLane();
            qCInfo(lcDsp) << "Rebuild: closed RX channel" << channelId;
            openRxChannelWdsp(channelId, cfg.bufferSize, cfg.filterSize,
                              cfg.sampleRate, kRxDspSampleRate, kRxOutputSampleRate);
            ptr->createWdspObjectsOnLane();
            ptr->setWdspReady(true);
            if (release) {
                release();
            }
            retireRxChannel(std::move(retired));
        });
#ifdef HAVE_WDSP
        // Re-seed SNB defaults (same pattern as createRxChannel).
        if (auto* nb = ptr->nb()) { nb->seedSnbFromSettings(); }
#endif
        emit rxChannelCreated(channelId);
        // Reapply captured DSP state to the new channel (queued behind the
        // barrier).
        ptr->applyState(state);
        const qint64 elapsedMs = timer.elapsed();
        qCInfo(lcDsp) << "Rebuild: RX channel" << channelId << "queued in"
                      << elapsedMs << "ms";
        return elapsedMs;
    }

#ifdef HAVE_WDSP
    // Deactivate with drain before closing (mirrors destroyRxChannel).
    SetChannelState(channelId, 0, 1);

    // NB / NB2 destroy is owned by ~NbFamily inside ~RxChannel destructor.
    // Do NOT add destroy_anbEXT/nobEXT here.

    // Close the old WDSP channel.
    CloseChannel(channelId);
    ThreadPlacement::instance().forgetChannel(channelId);
#endif

    // Destroy the old RxChannel C++ wrapper (runs ~NbFamily, ~DeepFilterFilter, etc.).
    old.reset();
    qCInfo(lcDsp) << "Rebuild: closed RX channel" << channelId;

    // Recreate the WDSP channel with the new config. Re-seeds WDSP defaults
    // to match the RxChannel constructor defaults — same pattern as
    // createRxChannel() so that applyState() early-return guards fire
    // correctly for values that haven't changed.
    openRxChannelWdsp(channelId, cfg.bufferSize, cfg.filterSize,
                      cfg.sampleRate, kRxDspSampleRate, kRxOutputSampleRate);

    // Construct a new RxChannel C++ wrapper.
    auto channel = std::make_unique<RxChannel>(channelId, cfg.bufferSize,
                                               cfg.sampleRate, this);
    RxChannel* ptr = channel.get();

#ifdef HAVE_WDSP
    // Re-seed SNB defaults (same pattern as createRxChannel).
    if (auto* nb = ptr->nb()) { nb->seedSnbFromSettings(); }
#endif

    {
        std::unique_lock<std::shared_mutex> lock(m_rxChannelsMutex);
        m_rxChannels.emplace(channelId, std::move(channel));
    }
    emit rxChannelCreated(channelId);

    // Reapply captured DSP state to the new channel.
    ptr->applyState(state);

    const qint64 elapsedMs = timer.elapsed();
    qCInfo(lcDsp) << "Rebuild: RX channel" << channelId << "ready in"
                  << elapsedMs << "ms";
    return elapsedMs;
}

// ---------------------------------------------------------------------------
// Live sample-rate change for an existing RX channel.
//
// Source-first port of the RX path inside ChannelMaster/cmaster.c::
// SetXcmInrate at lines 453-507 [v2.10.3.13].  The relevant excerpt
// (NereusSDR ports the WDSP-channel calls; ANB/NOB/Siphon/IVAC are
// either NereusSDR-original infrastructure or covered by separate ports):
//
//   case 0:  // receiver
//       SetRCVRANBBuffsize  (0, rx, pcm->xcm_insize[in_id]);  // anb size
//       SetRCVRANBSamplerate(0, rx, rate);                    // anb rate
//       SetRCVRNOBBuffsize  (0, rx, pcm->xcm_insize[in_id]);  // nob size
//       SetRCVRNOBSamplerate(0, rx, rate);                    // nob rate
//       for (i = 0; i < pcm->cmSubRCVR; i++) {
//           SetInputSamplerate (chid (in_id, i), rate);                     // dsp channel input rate
//           SetInputBuffsize   (chid (in_id, i), pcm->xcm_insize[in_id]);   // dsp channel input size
//       }
//       SetIVACiqSizeAndRate (rx, pcm->xcm_insize[in_id], pcm->xcm_inrate[in_id]);
//
// NereusSDR scope here:
//   * SetInputSamplerate / SetInputBuffsize on the channel — full port.
//   * ANB / NOB rate-and-size — deferred (NbFamily currently re-seeds these
//     at construction; live propagation requires per-instance setters).
//   * Siphon / IVAC — NereusSDR uses different display + VAC paths.
// ---------------------------------------------------------------------------

bool WdspEngine::setRxChannelRate(int channelId, int newRateHz)
{
    RxChannel* ch = rxChannel(channelId);
    if (ch == nullptr) {
        qCWarning(lcDsp) << "setRxChannelRate: channel" << channelId << "not found";
        return false;
    }

    // Idempotent guard mirrors cmaster.c:457 [v2.10.3.13]
    //   if (pcm->xcm_inrate[in_id] != rate) { ... }
    if (ch->sampleRate() == newRateHz) {
        return true;
    }

    if (m_rxLane != nullptr && !m_rxLane->isCurrentThread()) {
        // R-R3-39: carried at once, applied on the lane as a barrier.
        setRxChannelRateAsync(channelId, newRateHz, nullptr, {});
        return true;
    }

    // Update the C++-side carry first so the wrapper's accessors agree
    // with the WDSP-side state if a reader peeks between calls.
    ch->setSampleRateCarry(newRateHz);
    applyRateOnLane(ch, newRateHz, ch->bufferSize());
    return true;
}

void WdspEngine::applyRateOnLane(RxChannel* ch, int rateHz, int bufferSize)
{
    // The wrapper's WDSP half first (pending stop, NB rate, notch-width
    // readout), as RxChannel::setSampleRate always ran ahead of the channel
    // calls below.
    ch->applySampleRateOnLane(rateHz, bufferSize);

#ifdef HAVE_WDSP
    // From Thetis cmaster.c:473-474 [v2.10.3.13]
    SetInputSamplerate(ch->channelId(), rateHz);
    SetInputBuffsize(ch->channelId(), bufferSize);
#endif

    qCInfo(lcDsp) << "setRxChannelRate: channel" << ch->channelId()
                  << "->" << rateHz << "Hz, in_size=" << bufferSize;
}

bool WdspEngine::applyRxChannelRateOnLane(int channelId, int rateHz, int bufferSize)
{
    RxChannel* ch = rxChannel(channelId);
    if (ch == nullptr) {
        return false;
    }
    applyRateOnLane(ch, rateHz, bufferSize);
    return true;
}

void WdspEngine::setRxChannelRateAsync(int channelId, int rateHz, QObject* context,
                                       std::function<void(bool)> done)
{
    RxChannel* ch = rxChannel(channelId);
    const bool lane = m_rxLane != nullptr && !m_rxLane->isCurrentThread();
    if (!lane) {
        const bool ok = setRxChannelRate(channelId, rateHz);
        if (done) {
            done(ok);
        }
        return;
    }
    auto ok = std::make_shared<std::atomic<bool>>(false);
    if (ch == nullptr) {
        qCWarning(lcDsp) << "setRxChannelRateAsync: channel" << channelId << "not found";
    } else if (ch->sampleRate() == rateHz) {
        // Mirrors SetXcmInrate's guard, cmaster.c:457 [v2.10.3.13].
        ok->store(true);
    } else {
        ch->setSampleRateCarry(rateHz);
        const int bufferSize = ch->bufferSize();
        m_rxLane->postBarrier([this, channelId, rateHz, bufferSize, ok]() {
            const std::function<void()> release = quiesceRxWorker();
            ok->store(applyRxChannelRateOnLane(channelId, rateHz, bufferSize));
            if (release) {
                release();
            }
        });
    }
    if (context != nullptr && done) {
        // Answered after the barrier (FIFO), on the context's thread.
        m_rxLane->request<bool>([ok]() { return ok->load(); }, context, std::move(done));
    }
}

void WdspEngine::setReceiveLane(DspControlThread* lane)
{
    m_rxLane = lane;
    if (m_psFeedbackChannel) {
        m_psFeedbackChannel->setControlLane(lane);
    }
    std::shared_lock<std::shared_mutex> lock(m_rxChannelsMutex);
    for (auto& [id, ch] : m_rxChannels) {
        Q_UNUSED(id);
        ch->setControlLane(lane);
    }
}

void WdspEngine::setRxWorkerQuiesce(RxWorkerQuiesce quiesce)
{
    std::lock_guard<std::mutex> lock(m_rxQuiesceMutex);
    m_rxQuiesce = std::move(quiesce);
}

std::function<void()> WdspEngine::quiesceRxWorker()
{
    RxWorkerQuiesce quiesce;
    {
        std::lock_guard<std::mutex> lock(m_rxQuiesceMutex);
        quiesce = m_rxQuiesce;
    }
    if (!quiesce) {
        return {};
    }
    return quiesce();
}

void WdspEngine::retireRxChannel(std::shared_ptr<RxChannel> channel)
{
    {
        std::lock_guard<std::mutex> lock(m_retiredMutex);
        m_retiredRxChannels.push_back(std::move(channel));
    }
    QMetaObject::invokeMethod(this, [this]() { reapRetiredRxChannels(); },
                              Qt::QueuedConnection);
}

void WdspEngine::reapRetiredRxChannels()
{
    std::vector<std::shared_ptr<RxChannel>> retired;
    {
        std::lock_guard<std::mutex> lock(m_retiredMutex);
        retired.swap(m_retiredRxChannels);
    }
    // Deleted here, on this object's thread, where they were made.
}

void WdspEngine::setTransmitLane(DspControlThread* lane)
{
    m_txLane = lane;
    for (auto& [id, ch] : m_txChannels) {
        Q_UNUSED(id);
        ch->setControlLane(lane);
    }
}

void WdspEngine::retireTxChannel(std::shared_ptr<TxChannel> channel)
{
    {
        std::lock_guard<std::mutex> lock(m_retiredTxMutex);
        m_retiredTxChannels.push_back(std::move(channel));
    }
    QMetaObject::invokeMethod(this, [this]() { reapRetiredTxChannels(); },
                              Qt::QueuedConnection);
}

void WdspEngine::reapRetiredTxChannels()
{
    std::vector<std::shared_ptr<TxChannel>> retired;
    {
        std::lock_guard<std::mutex> lock(m_retiredTxMutex);
        retired.swap(m_retiredTxChannels);
    }
    // Deleted here, on this object's thread, where they were made.
}

void WdspEngine::drainTransmitLane()
{
    if (m_txLane == nullptr || m_txLane->isCurrentThread()) {
        return;
    }
    m_txLane->stop();
    m_txLane->start();
    reapRetiredTxChannels();
}

void WdspEngine::drainReceiveLane()
{
    if (m_rxLane == nullptr || m_rxLane->isCurrentThread()) {
        return;
    }
    m_rxLane->stop();
    m_rxLane->start();
    reapRetiredRxChannels();
}

// ---------------------------------------------------------------------------
// Per-board ChannelMaster-layer WDSP calls — Phase B4'/B5'
//
// Both wrappers call ChannelMaster-exported symbols provided by
// third_party/wdsp/src/netinterface_stub.c (glue stubs) until the real
// ChannelMaster module is ported.  The forward-declarations mirror the
// TxChannel.cpp pattern used for SetTXFixedGain (txgain_stub.c).
// ---------------------------------------------------------------------------

#ifdef HAVE_WDSP
extern "C" {
    // From Thetis ChannelMaster/txgain.c:164 [v2.10.3.15]
    void SetADCSupply(int txid, int v);
    // From Thetis ChannelMaster/netInterface.c:1409 [v2.10.3.15]
    void LRAudioSwap(int swap);
}
#endif

// setAdcSupply
//
// Registers the per-board ADC supply voltage with the ChannelMaster TXGAIN
// DSP path so xtxgain() can apply the correct PA over-drive protection scaling
// (txgain.c:90-100 [v2.10.3.15]: case 33 uses adc_value/2730.0, case 50 uses
// adc_value/1802.0).
//
// From Thetis clsHardwareSpecific.cs:85-191 [v2.10.3.15] — called at connect
// time per SKU (e.g. line 90: cmaster.SetADCSupply(0, 33)).
// Upstream inline attribution preserved per CLAUDE.md §"Inline comment preservation":
//   :129 //N1GP G2E added
//   :171 // G8NJJ: likely to need further changes for PA
//   :185 //DH1KLM
//   :187 // DH1KLM: changed for compatibility reasons for OpenHPSDR compat. DIY PA/Filter boards
// Skips the call when v == 0 (sentinel "not set").
void WdspEngine::setAdcSupply(int txid, int v)
{
    if (v == 0) {
        return;  // sentinel: not set, leave WDSP default unchanged
    }
#ifdef HAVE_WDSP
    // From Thetis ChannelMaster/txgain.c:164 [v2.10.3.15] — SetADCSupply
    SetADCSupply(txid, v);
#endif
    qCInfo(lcDsp) << "setAdcSupply: txid=" << txid << "voltage=" << v << "V";
}

// setLRAudioSwap
//
// Registers the per-board L/R audio channel swap flag with the ChannelMaster
// network layer so the outbound P2/ETH audio path (sendOutbound() at
// netInterface.c:1277) swaps stereo pair order for Hermes-family boards.
//
// From Thetis clsHardwareSpecific.cs:85-191 [v2.10.3.15] — called at connect
// time per SKU (e.g. line 91: NetworkIO.LRAudioSwap(1) for HERMES/ANAN10/*).
// Upstream inline attribution preserved per CLAUDE.md §"Inline comment preservation":
//   :129 //N1GP G2E added
//   :171 // G8NJJ: likely to need further changes for PA
//   :185 //DH1KLM
//   :187 // DH1KLM: changed for compatibility reasons for OpenHPSDR compat. DIY PA/Filter boards
void WdspEngine::setLRAudioSwap(int swap)
{
#ifdef HAVE_WDSP
    // From Thetis ChannelMaster/netInterface.c:1409 [v2.10.3.15] — LRAudioSwap
    LRAudioSwap(swap);
#endif
    qCInfo(lcDsp) << "setLRAudioSwap: swap=" << swap;
}

// ---------------------------------------------------------------------------
// TX Channel management
// ---------------------------------------------------------------------------

// R-R3-39: the WDSP half of createTxChannel that runs before its wrapper
// exists (OpenChannel, the default seeds, create_dexp on the channel's DEXP
// buffer), at once without a transmit lane or in the create barrier with one.
void WdspEngine::openTxChannelWdsp(int channelId, int inputBufferSize, int dspBufferSize,
                                   int inputSampleRate, int dspSampleRate,
                                   int outputSampleRate, double* dexpBuf)
{
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
        inputBufferSize,          // anti-vox data buffer size (placeholder; overridden
                                  // by setAntiVoxSize once RxDspWorker geometry is known)
        inputSampleRate,          // anti-vox data sample-rate (placeholder; overridden
                                  // by setAntiVoxRate at the same point)
        0.01,                     // anti-vox gain (linear, -40 dB; overridden by
                                  // udAntiVoxGain handler at first paint)
        // From Thetis setup.designer.cs:44682-44686 [v2.10.3.13]:
        //   udAntiVoxTau default 20 ms (= 0.02 s).  Thetis ChannelMaster
        //   cmaster.c:157 passes 0.01 at create time but the spinbox-applied
        //   value overrides it on every Thetis startup; we initialize at the
        //   spinbox value so behavior is identical with or without an open
        //   Setup page.
        0.02);                    // anti-vox smoothing time-constant
    qCInfo(lcDsp) << "TX channel" << channelId
                  << "DEXP created (parallel-only buffer architecture):"
                  << "size=" << inputBufferSize
                  << "rate=" << inputSampleRate
                  << "buf=" << static_cast<const void*>(dexpBuf);
#else
    Q_UNUSED(channelId);
    Q_UNUSED(inputBufferSize);
    Q_UNUSED(dspBufferSize);
    Q_UNUSED(inputSampleRate);
    Q_UNUSED(dspSampleRate);
    Q_UNUSED(outputSampleRate);
    Q_UNUSED(dexpBuf);
#endif
}

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

    // R-R3-39: the channel's DEXP buffer exists before its WDSP channel, on
    // either path, so the wrapper and create_dexp share one stable pointer.
    //
    // Order of operations: allocate buffer FIRST so the pointer passed
    // to create_dexp is stable for the entire DEXP lifetime (the WDSP
    // module retains the raw pointer set here until destroy_dexp clears
    // it).  emplace returns an iterator to the newly-inserted element;
    // we use that to extract a stable double* — std::map iterators are
    // never invalidated by other map modifications, and std::vector's
    // data pointer is stable across reads (we never resize this buffer
    // after create_dexp captures the pointer).
    //
    // Non-HAVE_WDSP build (e.g. test runner that links WdspEngine but not
    // the WDSP DSP module): the buffer slot is still allocated so map state
    // stays consistent across builds.  Without this, destroyTxChannel's
    // erase would silently no-op on the missing key and any future
    // diagnostic that introspects m_dexpBuffers would see a phantom
    // missing entry.  The buffer is unused in non-HAVE_WDSP builds (no
    // DEXP module to feed) but the storage is harmless.
    auto [dexpIt, inserted] = m_dexpBuffers.emplace(
        channelId,
        std::vector<double>(static_cast<size_t>(inputBufferSize) * 2, 0.0));
    double* dexpBuf = dexpIt->second.data();
    Q_UNUSED(inserted);   // emplace always inserts here (early-return guards above ensure new key)

    // R-R3-39: with a transmit lane, OpenChannel and everything after it run
    // there as one barrier (below); the wrapper exists at once and keeps the
    // TX worker out until the barrier has opened the channel.
    const bool onLane = (m_txLane != nullptr && !m_txLane->isCurrentThread());
    if (!onLane) {
        openTxChannelWdsp(channelId, inputBufferSize, dspBufferSize,
                          inputSampleRate, dspSampleRate, outputSampleRate, dexpBuf);
    }

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
    auto wrapper = onLane
        ? std::make_unique<TxChannel>(channelId, inputBufferSize, outputBufferSize,
                                      m_txLane, nullptr)
        : std::make_unique<TxChannel>(channelId, inputBufferSize, outputBufferSize, nullptr);
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

    // ── PR #212 follow-up bench fix (J.J. KG4VCF, 2026-05-07) ────────────
    // Arm the TXA siphon to feed the WDSP analyzer for TX panadapter
    // display.  Mirrors Thetis cmaster.cs:538-540 [v2.10.3.13]:
    //   WDSP.TXASetSipMode(txch, 1);            // 1 => call analyzer
    //   WDSP.TXASetSipDisplay(txch, txinid);    // disp = txinid
    //
    // The siphon at TXA.c:586 (xsiphon, line 586 — BEFORE xiqc at line
    // 587) runs every fexchange0 invocation.  With mode=1 it dispatches
    // Spectrum0(1, disp, 0, 0, in) per siphon.c:129-132.  The disp ID
    // points to the WDSP analyzer instance owned by TxAnalyzer
    // (kTxDispId=5, mirroring Thetis's cmaster.inid(1, 0) = cmRCVR + 0 = 5).
    //
    // Pre-IQC tap = clean intended pre-PA signal (the operator's
    // intended modulation), as opposed to the antenna readback which
    // shows actual radiated RF including PA IMD.  Thetis chooses
    // pre-IQC; we match.
    //
    // Sequencing safety: TxAnalyzer is constructed in MainWindow setup
    // BEFORE any radio connect, so XCreateAnalyzer(kTxDispId) has
    // already populated pdisp[5] by the time this code runs and
    // xsiphon's first push fires.  See TxAnalyzer.h header for the
    // ordering invariant.
    if (onLane) {
        // R-R3-39: the create barrier, in the order the path above keeps:
        // OpenChannel and its seeds, create_dexp, the VOX callback, the
        // siphon, then the worker may run blocks through the channel.
        m_txLane->postBarrier([this, raw, channelId, inputBufferSize, dspBufferSize,
                               inputSampleRate, dspSampleRate, outputSampleRate, dexpBuf]() {
            // Runs even for a wrapper retired meanwhile: the destroy or
            // rebuild barrier queued after this one closes what it opens.
            openTxChannelWdsp(channelId, inputBufferSize, dspBufferSize,
                              inputSampleRate, dspSampleRate, outputSampleRate, dexpBuf);
            raw->onWdspOpenedOnLane();
#ifdef HAVE_WDSP
            TXASetSipMode(channelId, 1);
            TXASetSipDisplay(channelId, /*txinid=*/5);  // = TxAnalyzer::kTxDispId
#endif
            raw->admitWorkerOnLane();
            qCInfo(lcDsp) << "Created TX channel" << channelId << "(transmit lane)";
        });
        return raw;
    }

#ifdef HAVE_WDSP
    TXASetSipMode(channelId, 1);
    TXASetSipDisplay(channelId, /*txinid=*/5);  // = TxAnalyzer::kTxDispId
#endif

    qCInfo(lcDsp) << "Created TX channel" << channelId;
    return raw;
}

void WdspEngine::destroyTxChannel(int channelId)
{
    auto it = m_txChannels.find(channelId);
    if (it == m_txChannels.end()) {
        return;   // idempotent — not found, nothing to do
    }

    if (m_txLane != nullptr && !m_txLane->isCurrentThread()) {
        // R-R3-39: gone from the map at once. The lane closes it after every
        // job already queued for it, once the TX worker is out of its block,
        // in the order below; the wrapper is deleted back on this thread and
        // the DEXP buffer lives until destroy_dexp has run.
        std::shared_ptr<TxChannel> channel(std::move(it->second));
        m_txChannels.erase(it);
        auto buffer = std::make_shared<std::vector<double>>();
        if (auto bufIt = m_dexpBuffers.find(channelId); bufIt != m_dexpBuffers.end()) {
            *buffer = std::move(bufIt->second);
            m_dexpBuffers.erase(bufIt);
        }
        m_txLane->postBarrier([this, channel, buffer, channelId]() mutable {
            channel->quiesceWorkerOnLane();
            channel->markRetired();
            // R-R3-39: a TX cycle still running when the channel goes
            // leaves its TCI resampler; free it here, on the lane, after
            // every TCI block queued before this barrier.
            channel->releaseTciResamplerOnLane();
#ifdef HAVE_WDSP
            // Deactivate with drain before closing.
            // dmode=1: drain-mode close (mirrors destroyRxChannel pattern).
            SetChannelState(channelId, 0, 1);
            // The wrapper's VOX unregister, through the live DEXP object.
            channel->unregisterVoxCallbackOnLane();
            // From Thetis cmaster.c:265-267 [v2.10.3.15]: destroy_xmtr closes
            // the channel BEFORE destroying its DEXP module.
            CloseChannel(channelId);
            ThreadPlacement::instance().forgetChannel(channelId);
            destroy_dexp(channelId);
            // destroy_dexp frees the object but does not clear WDSP's lookup slot.
            // Native wrapper guards use a null slot to recognize absent DEXP.
            pdexp[channelId] = nullptr;
#endif
            buffer.reset();
            retireTxChannel(std::move(channel));
            qCInfo(lcDsp) << "Destroyed TX channel" << channelId;
        });
        return;
    }

#ifdef HAVE_WDSP
    // Deactivate with drain before closing.
    // dmode=1: drain-mode close (mirrors destroyRxChannel pattern).
    SetChannelState(channelId, 0, 1);
#endif

    // The C++ wrapper unregisters its VOX callback through the live DEXP
    // object. Retire it before either WDSP object is destroyed. The owner
    // has already stopped the TX pump before channel teardown.
    m_txChannels.erase(it);

#ifdef HAVE_WDSP
    // From Thetis cmaster.c:265-267 [v2.10.3.15]: destroy_xmtr closes
    // the channel BEFORE destroying its DEXP module.
    CloseChannel(channelId);
    ThreadPlacement::instance().forgetChannel(channelId);
    destroy_dexp(channelId);
    // destroy_dexp frees the object but does not clear WDSP's lookup slot.
    // Native wrapper guards use a null slot to recognize absent DEXP.
    pdexp[channelId] = nullptr;
#endif

    // Drop the DEXP buffer slot regardless of HAVE_WDSP — the non-HAVE_WDSP
    // createTxChannel branch also stores an entry, so the symmetry must
    // hold for both build configs.  Erase is idempotent on a missing key.
    m_dexpBuffers.erase(channelId);

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

// ---------------------------------------------------------------------------
// PureSignal feedback channel management (Phase 3M-4 Task 4)
// ---------------------------------------------------------------------------

void WdspEngine::openPsFeedbackChannel()
{
    if (m_psFeedbackChannel) {
        return;   // idempotent — already open
    }

    // OpenChannel parameters mirror createRxChannel (the existing RX
    // pattern at WdspEngine.cpp:311-324) — type=0 (RX), bfo=1, the same
    // tdelayup/tslewup/tdelaydown/tslewdown envelope.  The per-board PS
    // sample-rate divergence is handled later via setSampleRate(); here we
    // open with the G2-class default so the channel is live right after
    // wisdom completes.
    //
    // Buffer sizes (in_size / dsp_size) match the RX-1 defaults at
    // RadioModel.cpp:1458 (createRxChannel(0, wdspInSize, 4096, ...)) when
    // wdspInSize=64 — we use the canonical 64 / 4096 here to keep the
    // ring-buffer math identical to the live RX-1 channel.  The PureSignal
    // coordinator (Task 7) re-applies per-board input rates before MOX, so
    // these are init-time defaults only.
    constexpr int kPsInputBufferSize = 64;     // matches RX-1 wdspInSize default
    constexpr int kPsDspBufferSize   = 4096;   // matches RX-1 dsp_size
    constexpr int kPsDspSampleRate   = 48000;  // RX DSP rate (post-decimation)
    constexpr int kPsOutputSampleRate = 48000; // RX output rate

#ifdef HAVE_WDSP
    // R-R3-39: on the receive lane when there is one, as a barrier.
    auto open = [=]() {
        // From Thetis cmaster.c:72-86 (create_rcvr OpenChannel call) [v2.10.3.13]
        OpenChannel(
            kPsFeedbackChannelId,
            kPsInputBufferSize,                 // in_size
            kPsDspBufferSize,                   // dsp_size
            kPsFeedbackDefaultSampleRate,       // input sample rate (192000 default)
            kPsDspSampleRate,                   // dsp sample rate
            kPsOutputSampleRate,                // output sample rate
            kPsFeedbackChannelType,             // type=0 (RX) — from cmaster.c:184
            0,                                  // state: 0=off initially
            0.010,                              // tdelayup  — from cmaster.c:82
            0.025,                              // tslewup   — from cmaster.c:83
            0.000,                              // tdelaydown — from cmaster.c:84
            0.010,                              // tslewdown — from cmaster.c:85
            1);                                 // bfo: block until output available

        // Activate the channel immediately.  Mirrors createRxChannel's pattern
        // where SetRXAMode et al. run on a state=0 channel and the channel is
        // activated lazily; for PS feedback we activate up front because calcc
        // reads autonomously and expects the channel live.  state=1, dmode=0
        // (no drain — channel isn't running yet).
        SetChannelState(kPsFeedbackChannelId, 1, 0);

        qCInfo(lcDsp) << "Opened PS feedback RX channel"
                      << kPsFeedbackChannelId
                      << "rate=" << kPsFeedbackDefaultSampleRate;
    };
    if (m_rxLane != nullptr && !m_rxLane->isCurrentThread()) {
        m_rxLane->postBarrier(open);
    } else {
        open();
    }
#endif

    m_psFeedbackChannel = std::make_unique<PsFeedbackChannel>(
        kPsFeedbackChannelId, this);
    m_psFeedbackChannel->setControlLane(m_rxLane);
}

void WdspEngine::closePsFeedbackChannel()
{
    if (!m_psFeedbackChannel) {
        return;
    }

#ifdef HAVE_WDSP
    // R-R3-39: on the receive lane when there is one, as a barrier.
    auto close = []() {
        // Deactivate with drain before closing.  dmode=1: drain-mode close
        // (mirrors destroyRxChannel pattern at WdspEngine.cpp:381).
        SetChannelState(kPsFeedbackChannelId, 0, 1);
        CloseChannel(kPsFeedbackChannelId);
        ThreadPlacement::instance().forgetChannel(kPsFeedbackChannelId);
        qCInfo(lcDsp) << "Closed PS feedback RX channel" << kPsFeedbackChannelId;
    };
    if (m_rxLane != nullptr && !m_rxLane->isCurrentThread()) {
        m_rxLane->postBarrier(close);
    } else {
        close();
    }
#endif

    m_psFeedbackChannel.reset();
}

PsFeedbackChannel* WdspEngine::psFeedbackChannel() const
{
    return m_psFeedbackChannel.get();
}

#ifdef NEREUS_BUILD_TESTS
void WdspEngine::openPsFeedbackChannelForTesting()
{
    // Test path: caller has set m_initialized=true via friend access.
    // openPsFeedbackChannel() doesn't itself require m_initialized (it's
    // a private helper that finishInitialization invokes after setting
    // the flag), so this is just a public entry point for tests.
    openPsFeedbackChannel();
}
#endif

// R-R3-39: the TX defaults rebuildTxChannel seeds after OpenChannel (the
// same block createTxChannel issues; see its comments for the sources).
void WdspEngine::seedTxChannelWdsp(int channelId)
{
#ifdef HAVE_WDSP
    // Re-seed TX defaults — same block as createTxChannel() so that
    // applyState() setter guards fire correctly for unchanged values.
    SetTXABandpassWindow(channelId, 1);
    SetTXABandpassRun(channelId, 1);
    SetTXAAMSQRun(channelId, 0);
    SetTXAALCAttack(channelId, 1);
    SetTXAALCDecay(channelId, 10);
    SetTXAALCMaxGain(channelId, 0.0);
    SetTXAALCSt(channelId, 1);
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
#else
    Q_UNUSED(channelId);
#endif
}

qint64 WdspEngine::rebuildTxChannel(int channelId, const ChannelConfig& cfg)
{
    if (!m_initialized) {
        qCWarning(lcDsp) << "rebuildTxChannel: WDSP not initialized";
        return -1;
    }

    auto it = m_txChannels.find(channelId);
    if (it == m_txChannels.end()) {
        qCWarning(lcDsp) << "rebuildTxChannel: channel" << channelId << "not found";
        return -1;
    }

    QElapsedTimer timer;
    timer.start();

    // Capture DSP state before tearing down the channel.
    const TxChannelState state = it->second->captureState();

    // The OpenChannel call rebuildTxChannel makes, on either path.
    // Use the same OpenChannel arguments as createTxChannel() — kTxChannelType=1,
    // kTxBlockOnOutput=1, and the TX-specific slew constants.
    // cfg.bufferSize = new in_size; cfg.filterSize = new dsp_size.
    // For TX dsp_rate we reuse kTxDspSampleRate (96000) — the ChannelConfig
    // struct carries a single sampleRate field intended for the I/O rates.
    const auto reopen = [this, channelId, cfg]() {
#ifdef HAVE_WDSP
        OpenChannel(
            channelId,
            cfg.bufferSize,             // in_size (new input block size)
            cfg.filterSize,             // dsp_size
            cfg.sampleRate,             // input sample rate
            kTxDspSampleRate,           // dsp sample rate — always 96 kHz for TX
            cfg.sampleRate,             // output sample rate
            kTxChannelType,             // type=1 (TX)
            0,                          // initial state: off
            0.000,                      // tdelayup  — from cmaster.c:186
            kTxTSlewUpSecs,             // tslewup 0.010 s — from cmaster.c:187
            0.000,                      // tdelaydown — from cmaster.c:188
            kTxTSlewDownSecs,           // tslewdown 0.010 s — from cmaster.c:189
            kTxBlockOnOutput);          // bfo=1 — from cmaster.c:190

        seedTxChannelWdsp(channelId);

        qCInfo(lcDsp) << "Rebuild: opened TX channel" << channelId
                      << "bufSize=" << cfg.bufferSize
                      << "rate=" << cfg.sampleRate;
#else
        Q_UNUSED(this);
#endif
    };

    // outputBufferSize = inputBufferSize × outputSampleRate / inputSampleRate
    // (same ratio math as createTxChannel — integer multiply-then-divide is safe).
    const int outputBufferSize = cfg.bufferSize * cfg.sampleRate / cfg.sampleRate;

    if (m_txLane != nullptr && !m_txLane->isCurrentThread()) {
        // R-R3-39: the generation check. The old wrapper is retired at once:
        // every setter still queued for it, and any posted through a stale
        // pointer from now on, is skipped. The new wrapper takes the
        // captured state; its setters queue behind the barrier that closes
        // the old channel and opens the new one.
        std::shared_ptr<TxChannel> old(std::move(it->second));
        m_txChannels.erase(it);
        old->markRetired();

        auto channel = std::make_unique<TxChannel>(channelId, cfg.bufferSize,
                                                   outputBufferSize, m_txLane, this);
        TxChannel* ptr = channel.get();
        m_txChannels.emplace(channelId, std::move(channel));

        m_txLane->postBarrier([this, old, ptr, channelId, reopen]() mutable {
            old->quiesceWorkerOnLane();
            // R-R3-39: the old wrapper was retired at once, so a cycle-stop
            // destroy still queued for it is skipped; free its TCI
            // resampler here, on the lane.
            old->releaseTciResamplerOnLane();
#ifdef HAVE_WDSP
            // Deactivate with drain before closing (mirrors destroyTxChannel).
            // dmode=1: drain-mode close per Thetis console.cs:29607 [v2.10.3.13].
            SetChannelState(channelId, 0, 1);

            // Close the old WDSP TX channel.
            CloseChannel(channelId);
            ThreadPlacement::instance().forgetChannel(channelId);
#endif
            // The old wrapper's destructor unregistered here.
            old->unregisterVoxCallbackOnLane();
            qCInfo(lcDsp) << "Rebuild: closed TX channel" << channelId;

            reopen();
            ptr->onWdspOpenedOnLane();
            ptr->admitWorkerOnLane();
            retireTxChannel(std::move(old));
        });

        // Reapply captured DSP state to the new channel.
        ptr->applyState(state);

        const qint64 elapsedMs = timer.elapsed();
        qCInfo(lcDsp) << "Rebuild: TX channel" << channelId << "queued in"
                      << elapsedMs << "ms (transmit lane)";
        return elapsedMs;
    }

#ifdef HAVE_WDSP
    // Deactivate with drain before closing (mirrors destroyTxChannel).
    // dmode=1: drain-mode close per Thetis console.cs:29607 [v2.10.3.13].
    SetChannelState(channelId, 0, 1);

    // Close the old WDSP TX channel.
    CloseChannel(channelId);
    ThreadPlacement::instance().forgetChannel(channelId);
#endif

    // Destroy the old TxChannel C++ wrapper.
    m_txChannels.erase(it);
    qCInfo(lcDsp) << "Rebuild: closed TX channel" << channelId;

    // Recreate the WDSP TX channel with the new config.
    reopen();

    // Construct a new TxChannel C++ wrapper.
    auto channel = std::make_unique<TxChannel>(channelId, cfg.bufferSize,
                                               outputBufferSize, this);
    TxChannel* ptr = channel.get();
    m_txChannels.emplace(channelId, std::move(channel));

    // Reapply captured DSP state to the new channel.
    ptr->applyState(state);

    const qint64 elapsedMs = timer.elapsed();
    qCInfo(lcDsp) << "Rebuild: TX channel" << channelId << "ready in"
                  << elapsedMs << "ms";
    return elapsedMs;
}

// ---------------------------------------------------------------------------
// Metering wrappers (Phase 3P-II Phase 2 Tasks 31-32)
// ---------------------------------------------------------------------------

// Returns the averaged S-meter reading (dBm) from the RXA pipeline.
//
// Porting from Thetis Console/dsp.cs:387-388 [@501e3f5] -- original C# logic:
//   [DllImport("wdsp.dll", EntryPoint = "GetRXAMeter", ...)]
//   public static extern double GetRXAMeter(int channel, rxaMeterType meter);
// Selector site: Thetis Console/dsp.cs:957 [@501e3f5] (CalculateRXMeter):
//   case MeterType.AVG_SIGNAL_STRENGTH:
//       val = GetRXAMeter(channel, rxaMeterType.RXA_S_AV);
// The neighbouring ADC_REAL case at dsp.cs:959 carries //MW0LGE [2.9.0.7]
// attribution that we preserve verbatim per GPL inline-tag preservation.
//
// RXA_S_AV == 1 (see wdsp/RXA.h rxaMeterType enum / WdspTypes.h SignalAvg).
// The value is lock-free at the WDSP boundary (GetRXAMeter reads the WDSP
// meter output register directly, same as RxChannel::getMeter). Callers must
// ensure the channel is active before reading meaningful values.
//
// Guard: returns -140.0 sentinel if the engine is not yet initialized (mirrors
// RxChannel::getMeter's !m_active.load() guard at RxChannel.cpp:1537).
double WdspEngine::getRxaSignalAverage(int channel) const
{
    if (!m_initialized) {
        return -140.0;
    }
    if (m_rxLane != nullptr && !m_rxLane->isCurrentThread()) {
        // R-R3-39: the lane-refreshed meter cache, never WDSP here.
        const RxChannel* ch = rxChannel(channel);
        return ch ? ch->getMeter(RxMeterType::SignalAvg) : -140.0;
    }
#ifdef HAVE_WDSP
    // From Thetis Console/dsp.cs:387-388 [@501e3f5]
    // From Thetis Console/dsp.cs:957 [@501e3f5] (RXA_S_AV selector; preserves
    // //MW0LGE [2.9.0.7] attribution from adjacent ADC_REAL case at dsp.cs:959.)
    return ::GetRXAMeter(channel, /*RXA_S_AV=*/1);
#else
    Q_UNUSED(channel);
    return -140.0;
#endif
}

// Returns the peak S-meter reading (dBm) from the RXA pipeline.
//
// Porting from Thetis Console/dsp.cs:387-388 [@501e3f5] -- original C# P/Invoke:
//   [DllImport("wdsp.dll", EntryPoint = "GetRXAMeter", ...)]
//   public static extern double GetRXAMeter(int channel, rxaMeterType meter);
// Selector site: Thetis Console/dsp.cs:954 [@501e3f5] (CalculateRXMeter):
//   case MeterType.SIGNAL_STRENGTH:
//       val = GetRXAMeter(channel, rxaMeterType.RXA_S_PK);
// The neighbouring ADC_REAL case at dsp.cs:959 carries //MW0LGE [2.9.0.7]
// attribution that we preserve verbatim per GPL inline-tag preservation.
//
// RXA_S_PK == 0 (first entry in Thetis dsp.cs:889 rxaMeterType enum
// [@501e3f5]; also matches WdspTypes.h RxMeterType::SignalPeak = 0).
// Used by SMeterWidget RxMode::SMeter and RxMode::SMeterPeak paths in
// MeterPoller::pollSMeter() (Task 41, Phase 3P-II).
double WdspEngine::getRxaSignalPeak(int channel) const
{
    if (!m_initialized) {
        return -140.0;
    }
    if (m_rxLane != nullptr && !m_rxLane->isCurrentThread()) {
        // R-R3-39: the lane-refreshed meter cache, never WDSP here.
        const RxChannel* ch = rxChannel(channel);
        return ch ? ch->getMeter(RxMeterType::SignalPeak) : -140.0;
    }
#ifdef HAVE_WDSP
    // From Thetis Console/dsp.cs:387-388 [@501e3f5]
    // From Thetis Console/dsp.cs:954 [@501e3f5] (RXA_S_PK selector; preserves
    // //MW0LGE [2.9.0.7] attribution from adjacent ADC_REAL case at dsp.cs:959.)
    return ::GetRXAMeter(channel, /*RXA_S_PK=*/0);
#else
    Q_UNUSED(channel);
    return -140.0;
#endif
}

// Configure the strongest-bin-in-passband detector for a display channel.
//
// Algorithm ported from Thetis wdsp/analyzer.c:688-830 [@501e3f5]:
//   SetupDetectMaxBin / DetectMaxBin / GetDetectMaxBin -- bin-range scan,
//   slow-release smoothing (decay = exp(-1/(tau*fps))), peak attack.
//
// Originally wrapped Thetis's C ::SetupDetectMaxBin which requires a WDSP
// analyzer display channel (CreateAnalyzer + SetAnalyzer + Spectrum buffer
// feed).  NereusSDR's FFTEngine uses raw FFTW3 directly and does not wire
// the WDSP analyzer subsystem.  Wiring the analyzer pipeline is a
// follow-up epic; for now the algorithm runs against FFTEngine's existing
// dBm bins via onSpectrumBinsForMaxBin slot.  Operator-visible behavior
// matches the Thetis spec; the underlying DSP plumbing diverges.
//
// 'ss' and 'LO' Thetis arguments are accepted for API compatibility but
// unused in NereusSDR (Thetis multi-stream / multi-LO does not apply).
//
// Thetis call site at Console/console.cs:51150 [@501e3f5]:
//   WDSP.SetupDetectMaxBin(enabled ? 1 : 0, disp, 0, 0, sample_rate,
//                          low, high, 0.5, frame_rate);
// Default values match the developer example in wdsp/analyzer.c:1442 [@501e3f5]:
//   // SetupDetectMaxBin(1, 0, 0, 0, 192000.0, -3000.0, -300.0, 0.5, 60);
void WdspEngine::setupMaxBinDetector(int disp, int ss, int LO,
                                     double rate, double fLow, double fHigh,
                                     double tau, int frameRate)
{
    Q_UNUSED(ss); Q_UNUSED(LO);  // Thetis-API placeholders; not used in NereusSDR.

    if (disp < 0) { return; }
    if (m_maxBinDetectors.size() <= disp) {
        m_maxBinDetectors.resize(disp + 1);
    }
    auto& d = m_maxBinDetectors[disp];
    d.active    = true;
    d.rate      = rate;
    d.fLow      = fLow;
    d.fHigh     = fHigh;
    d.tau       = tau;
    d.frameRate = qMax(1, frameRate);
    d.decay     = std::exp(-1.0 / (d.tau * static_cast<double>(d.frameRate)));
    // From Thetis wdsp/analyzer.c:703 [@501e3f5] Init_DetectMaxBin sentinel.
    d.maxDb     = -400.0;
}

// Returns the strongest-bin dBm value from the configured detector.
//
// Algorithm ported from Thetis wdsp/analyzer.c:830 [@501e3f5] -- returns
// dmb_max_dB (the slow-release smoothed max).
// NereusSDR-native: state lives in m_maxBinDetectors[disp] rather than
// the WDSP pdisp[] array; see setupMaxBinDetector for the full rationale.
//
// Returns -400.0 sentinel when disp is out of range, the detector is not
// yet active, or no display frame has been processed yet.  Matches the
// Thetis Init_DetectMaxBin sentinel at wdsp/analyzer.c:703 [@501e3f5].
double WdspEngine::getMaxBinDbm(int disp) const
{
    if (disp < 0 || disp >= m_maxBinDetectors.size()) { return -400.0; }
    const auto& d = m_maxBinDetectors[disp];
    if (!d.active) { return -400.0; }
    return d.maxDb;
}

// Set the CTUN slice-to-DDC offset for the named detector.  Stored as a
// signed Hz value; consumed by onSpectrumBinsForMaxBin to shift the
// bin scan window away from DDC center to the user's tuned slice.
// Safe to call before setupMaxBinDetector (grows the vector to fit);
// safe to call repeatedly (idempotent same-value writes elided).
void WdspEngine::setMaxBinSliceOffsetHz(int disp, double sliceOffsetHz)
{
    if (disp < 0) { return; }
    if (m_maxBinDetectors.size() <= disp) {
        m_maxBinDetectors.resize(disp + 1);
    }
    auto& d = m_maxBinDetectors[disp];
    if (d.sliceOffsetHz == sliceOffsetHz) { return; }
    d.sliceOffsetHz = sliceOffsetHz;
}

// 2026-05-22 bench fix: direct override of the MaxBin detector value
// from SpectrumWidget's post detector + avenger pixel peak. See header
// doc for the rationale. Stamps active=true so the getter returns the
// new value rather than the -400 sentinel. Skips the peak-hold-with-
// decay smoothing the fftReady path does, because m_renderedPixels
// already carries the avenger's time smoothing.
void WdspEngine::setMaxBinDbmFromSpectrum(int disp, double dbm)
{
    if (disp < 0) { return; }
    if (m_maxBinDetectors.size() <= disp) {
        m_maxBinDetectors.resize(disp + 1);
    }
    auto& d = m_maxBinDetectors[disp];
    d.active = true;
    d.maxDb  = dbm;
}

// Slot: receive FFTEngine dBm bins and run the Max Bin scan + smoothing.
//
// Algorithm ported from Thetis wdsp/analyzer.c:800-822 [@501e3f5]
// (DetectMaxBin inner loop + smoothing step):
//
//   for (i = begin; i <= end; i++) {
//       mag = fft_out[i][0]^2 + fft_out[i][1]^2;
//       if (mag > dmb_max) dmb_max = mag;
//   }
//   a->dmb_max_dB -= fabs((1.0 - a->dmb_decay) * a->dmb_max_dB);
//   dmb_max_dB = 10.0 * mlog10(a->scale * dmb_max);
//   if (dmb_max_dB > a->dmb_max_dB) a->dmb_max_dB = dmb_max_dB;
//
// NereusSDR adaptations (not guessing -- explicit divergences):
//   1. binsDbm is already in dBm (FFTEngine applied 10*log10(scale*mag)),
//      so the magnitude scan and 10*log10 step are replaced by a direct
//      max-dBm scan over the window.
//   2. FFTEngine emits FFT-shifted bins (neg freqs first, then positive),
//      so Thetis's two-window split (begin0/end0 + begin1/end1 for
//      wdsp/analyzer.c:723-756 calc_dmb) collapses to a single contiguous
//      range: firstBin = N/2 + round(fLow / binSpacing).
//   3. Multi-panadapter (Phase 3F) will need a receiverId->disp mapping;
//      for now all data targets disp=0 (single-panadapter assumption).
void WdspEngine::onSpectrumBinsForMaxBin(int receiverId, const QVector<float>& binsDbm)
{
    Q_UNUSED(receiverId);  // single-panadapter: disp=0 for all receivers.
    const int N = binsDbm.size();
    if (N <= 0 || m_maxBinDetectors.isEmpty()) { return; }

    // Single-panadapter assumption (Phase 3F will add receiverId->disp mapping).
    auto& d = m_maxBinDetectors[0];
    if (!d.active) { return; }

    // Bin range computation.
    // From Thetis wdsp/analyzer.c:688-756 [@501e3f5] calc_dmb:
    //   bin_spacing = rate / size
    // FFT-shifted layout: bins[N/2 + k] = frequency k * binSpacing,
    // so the single-window collapsed form is:
    //   firstBin = clamp(N/2 + round((fLow  + sliceOffsetHz) / binSpacing), 0, N-1)
    //   lastBin  = clamp(N/2 + round((fHigh + sliceOffsetHz) / binSpacing), 0, N-1)
    //
    // NereusSDR-only sliceOffsetHz term: with CTUN on (default), the
    // user's slice does NOT match DDC center.  FFTEngine bins are in
    // DDC baseband, so we shift the scan window by (sliceFreq - ddcCenter)
    // to land on the user's tuned signal.  See setMaxBinSliceOffsetHz
    // for the architectural rationale (NereusSDR taps FFTEngine ahead of
    // the WDSP shift, where Thetis's analyzer is fed post-shift).
    const double binSpacing  = d.rate / static_cast<double>(N);
    const int    half        = N / 2;
    const double scanLowHz   = d.fLow  + d.sliceOffsetHz;
    const double scanHighHz  = d.fHigh + d.sliceOffsetHz;
    const int    firstBin    = qBound(0, half + static_cast<int>(std::round(scanLowHz  / binSpacing)), N - 1);
    const int    lastBin     = qBound(0, half + static_cast<int>(std::round(scanHighHz / binSpacing)), N - 1);
    if (lastBin < firstBin) { return; }  // degenerate window

    // 2026-05-22: this onSpectrumBinsForMaxBin path now serves only as
    // the fallback source for MaxBin. Bench-confirmed that the raw FFT
    // bin power scanned here reads ~12-17 dB below what the spectrum
    // visually displays for the same carrier (the spectrum runs the
    // FFT bins through a detector + windowEnb invEnb normalization +
    // avenger time-smoothing that reconstructs window-spread integrated
    // power a single bin can't show). SpectrumWidget::spectrumFrame-
    // Rendered now feeds WdspEngine::setMaxBinDbmFromSpectrum each
    // render frame which overrides d.maxDb with the post-pipeline value
    // the operator actually sees. Keeping the per-frame raw-bin smoother
    // here means MaxBin still has a value during early frames before
    // SpectrumWidget has pushed its first override, but the steady
    // state value comes from the spectrum pixel peak.

    // Scan: find max raw per-frame dBm in the window.
    //
    // Per-frame max with output-side peak-hold-with-decay smoothing
    // (Thetis analyzer.c:815-818 [v2.10.3.13]).  Riding peaks and
    // slow-decaying between is the right algorithm for modulation
    // envelopes -- voice peaks reaching -88 stay visible at -88 for
    // ~tau seconds rather than being averaged away.
    float newMaxDb = -400.0f;
    for (int i = firstBin; i <= lastBin; ++i) {
        if (binsDbm[i] > newMaxDb) {
            newMaxDb = binsDbm[i];
        }
    }

    // Output-side smoothing -- verbatim from wdsp/analyzer.c:815-818
    // [v2.10.3.13].  Peak attack (replace immediately on new max),
    // slow-release decay (drift toward more negative each frame).
    // For voice / modulated signals this rides peak frames at the
    // signal level and only falls between peaks, producing the
    // expected "pumping" behavior.
    d.maxDb -= std::abs((1.0 - d.decay) * d.maxDb);
    if (static_cast<double>(newMaxDb) > d.maxDb) { d.maxDb = static_cast<double>(newMaxDb); }
}

} // namespace NereusSDR
