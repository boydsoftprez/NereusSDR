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

    // Destroy all RX channels (collect IDs first to avoid iterator invalidation)
    std::vector<int> channelIds;
    for (const auto& [id, ch] : m_rxChannels) {
        channelIds.push_back(id);
    }
    for (int id : channelIds) {
        destroyRxChannel(id);
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

} // namespace NereusSDR
