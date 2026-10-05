// =================================================================
// src/core/TxDisplayFeed.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original. See TxDisplayFeed.h.
//
// Modification history (NereusSDR):
//   2026-09-26 : Created for remote-window parity Task 28 (R-R3-49, A11,
//                 R-IOS-13) by J.J. Boyd (KG4VCF). AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-09-26 : Tasks 27-29 fix wave (R-R3-49): remote viewer changes
//                 coalesced while keyed; one SetAnalyzer per view. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-04 : Match both analyzers to the queued TX DSP block size on
//                 first key. J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
// =================================================================

#include "core/TxDisplayFeed.h"

#include "core/MoxController.h"
#include "core/TxChannel.h"
#include "core/TxSliceArbiter.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "AppSettings.h"
#include "core/DspControlThread.h"
#include "core/wdsp_api.h"

#include <cmath>

namespace NereusSDR {

namespace {

// The siphon's baseband: the TX DSP rate, 96 kHz (WdspEngine::
// kTxDspSampleRate), either side of the carrier.
constexpr int kHalfBasebandHz = 48000;

} // namespace

TxDisplayFeed::TxDisplayFeed(RadioModel* model, TxAnalyzer* analyzer, QObject* parent)
    : QObject(parent)
    , m_model(model)
    , m_analyzer(analyzer)
{
    m_remoteViewTimer.setSingleShot(true);
    connect(&m_remoteViewTimer, &QTimer::timeout, this, [this]() {
        m_remoteApplied.restart();
        recompute();
    });
    if (m_model.isNull() || m_analyzer.isNull()) {
        return;
    }
    m_view.carrierHz = carrierHz();
    m_view.lowHz = kDefaultLowHz;
    m_view.highHz = kDefaultHighHz;
    m_view.pixels = m_analyzer->numPixels();

    // Every key, whatever keyed it (a MOX click, a hardware PTT, TUNE, VOX,
    // a remote device): MoxController::moxStateChanged is the one edge.
    if (MoxController* mox = m_model->moxController()) {
        connect(mox, &MoxController::moxStateChanged, this,
                &TxDisplayFeed::onMoxStateChanged);
    }
    if (TxSliceArbiter* arbiter = m_model->txSliceArbiter()) {
        connect(arbiter, &TxSliceArbiter::txBoundSliceChanged, this, [this](int, int) {
            if (m_keyed) {
                watchTransmitSlice();
                recompute();
            }
        });
    }
    // The analyzer runs only while keyed; a poll that lands after the fall
    // is not the transmit display any more.
    connect(m_analyzer, &TxAnalyzer::txFftReady, this,
            [this](int, const QVector<float>& dbm) {
        if (m_keyed) {
            emit traceReady(dbm);
        }
    });
    connect(m_analyzer, &TxAnalyzer::txWaterfallReady, this,
            [this](int, const QVector<float>& dbm) {
        if (m_keyed) {
            emit waterfallReady(dbm);
        }
    });
}

TxDisplayFeed::~TxDisplayFeed() { stopMini(); }

int TxDisplayFeed::addViewer(double centreHz, double spanHz, int pixels, bool local,
                             bool mini)
{
    const int id = m_nextViewerId++;
    if (mini) {
        m_miniViewerIds.insert(id);
        ensureMini();
        return id;
    }
    Viewer viewer;
    // Held relative to the carrier, so the view moves with it (XIT, a
    // retune of the transmit slice) as Thetis's does (getLowHighForRXn).
    viewer.centreHz = centreHz - carrierHz();
    viewer.spanHz = spanHz;
    viewer.pixels = pixels;
    viewer.local = local;
    m_viewers.emplace(id, viewer);
    recompute();
    return id;
}

void TxDisplayFeed::updateViewer(int id, double centreHz, double spanHz, int pixels)
{
    if (isMiniViewer(id)) { return; } // the mini has a fixed Thetis RF window
    auto it = m_viewers.find(id);
    if (it == m_viewers.end()) {
        return;
    }
    it->second.centreHz = centreHz - carrierHz();
    it->second.spanHz = spanHz;
    it->second.pixels = pixels;
    if (!m_keyed || it->second.local) {
        recompute();
        return;
    }
    // A remote viewer while keyed: at most one change per coalescing
    // period reaches the analyzer; a held one follows when it ends, with
    // the latest values (the viewer's own, stored above).
    if (m_remoteViewTimer.isActive()) {
        return;
    }
    const qint64 sinceMs = m_remoteApplied.isValid() ? m_remoteApplied.elapsed()
                                                     : qint64(kRemoteViewCoalesceMs);
    if (sinceMs >= kRemoteViewCoalesceMs) {
        m_remoteApplied.restart();
        recompute();
        return;
    }
    m_remoteViewTimer.start(int(kRemoteViewCoalesceMs - sinceMs));
}

void TxDisplayFeed::removeViewer(int id)
{
    if (m_miniViewerIds.erase(id) != 0) {
        if (m_miniViewerIds.empty() && !m_localMiniDemand) { stopMini(); }
        return;
    }
    if (m_viewers.erase(id) == 0) {
        return;
    }
    recompute();
}

bool TxDisplayFeed::isGoverning(int id) const
{
    return id != 0 && id == m_governor;
}

int TxDisplayFeed::governingViewer() const
{
    return m_governor;
}

int TxDisplayFeed::fftSize() const
{
    return m_analyzer ? m_analyzer->fftSize() : 0;
}

int TxDisplayFeed::outputFps() const
{
    return m_analyzer ? m_analyzer->outputFps() : 0;
}

double TxDisplayFeed::carrierHz() const
{
    if (m_model.isNull()) {
        return m_view.carrierHz;
    }
    SliceModel* slice = m_model->txBoundSlice();
    if (slice == nullptr) {
        return m_view.carrierHz;
    }
    // The transmitter's own number (dial plus XIT when XIT is on), so the
    // display and the transmitter cannot disagree about the carrier.
    return static_cast<double>(m_model->txFrequencyForSlice(slice));
}

void TxDisplayFeed::onMoxStateChanged(bool keyed)
{
    if (m_analyzer.isNull() || keyed == m_keyed) {
        return;
    }
    if (keyed) {
        m_keyed = true;
        // SetAnalyzer follows the queued DSP resize on the transmit lane.
        // Use its requested block size: the native readback can still hold
        // the previous mode's size until that earlier lane job completes.
        if (m_model) {
            if (TxChannel* txc = m_model->txChannel()) {
                m_analyzer->setBlockSize(txc->txDspBlockSize());
            }
        }
        watchTransmitSlice();
        recompute();
        m_analyzer->start();
        ensureMini();
        emit keyedChanged(true);
        return;
    }
    m_analyzer->stop();
    stopMini();
    m_remoteViewTimer.stop();
    m_remoteApplied.invalidate();
    // Clear the clip so the next key starts from the full baseband, as the
    // local window's fall does.
    m_analyzer->setSpectrumWindow(0, 0);
    m_keyed = false;
    for (const QMetaObject::Connection& connection : std::as_const(m_sliceConnections)) {
        disconnect(connection);
    }
    m_sliceConnections.clear();
    m_watchedSlice.clear();
    emit keyedChanged(false);
}

void TxDisplayFeed::watchTransmitSlice()
{
    for (const QMetaObject::Connection& connection : std::as_const(m_sliceConnections)) {
        disconnect(connection);
    }
    m_sliceConnections.clear();
    m_watchedSlice = m_model ? m_model->txBoundSlice() : nullptr;
    if (m_watchedSlice.isNull()) {
        return;
    }
    // Row 16: the carrier follows the transmit slice's frequency and XIT
    // while keyed (Thetis console.cs:22138-22150 [v2.10.3.15], "xit, only
    // when txing").
    SliceModel* slice = m_watchedSlice.data();
    m_sliceConnections.append(connect(slice, &SliceModel::frequencyChanged, this,
                                      [this](double) { recompute(); }));
    m_sliceConnections.append(connect(slice, &SliceModel::xitEnabledChanged, this,
                                      [this](bool) { recompute(); }));
    m_sliceConnections.append(connect(slice, &SliceModel::xitHzChanged, this,
                                      [this](int) { recompute(); }));
}

void TxDisplayFeed::recompute()
{
    if (m_analyzer.isNull()) {
        return;
    }
    // The local viewer governs whenever there is one; otherwise the lowest
    // id. std::map orders by id.
    int governor = 0;
    for (const auto& [id, viewer] : m_viewers) {
        if (viewer.local) {
            governor = id;
            break;
        }
    }
    if (governor == 0 && !m_viewers.empty()) {
        governor = m_viewers.begin()->first;
    }

    const double carrier = carrierHz();
    TxDisplayView view;
    view.carrierHz = carrier;
    if (governor != 0) {
        Viewer& viewer = m_viewers.at(governor);
        view = TxAnalyzer::clampViewToBaseband(carrier, carrier + viewer.centreHz,
                                               viewer.spanHz, viewer.pixels);
        if (view.empty()) {
            // A span under 1000 Hz keeps the last good view.
            view = viewer.lastGood;
            if (view.empty()) {
                view.lowHz = kDefaultLowHz;
                view.highHz = kDefaultHighHz;
            }
            view.carrierHz = carrier;
            view.pixels = viewer.pixels;
        }
        viewer.lastGood = view;
    } else {
        // Nobody asked: the analyzer's own window (a local window that has
        // not moved onto the feed yet still sets it itself).
        view.lowHz = m_analyzer->spectrumWindowLowHz();
        view.highHz = m_analyzer->spectrumWindowHighHz();
        if (view.empty()) {
            view.lowHz = -kHalfBasebandHz;
            view.highHz = kHalfBasebandHz;
        }
        view.pixels = m_analyzer->numPixels();
    }

    if (m_keyed && governor != 0) {
        m_analyzer->setView(view.lowHz, view.highHz, view.pixels);
    }

    const bool governorMoved = governor != m_governor;
    m_governor = governor;
    const bool viewMoved = !(view == m_view);
    m_view = view;
    if (governorMoved) {
        emit governorChanged(governor);
    }
    if (viewMoved && m_keyed) {
        emit viewChanged(view);
    }
    if (m_keyed && m_miniAttached) { updateMiniView(); }
}

bool TxDisplayFeed::isMiniViewer(int id) const
{
    return m_miniViewerIds.contains(id);
}

void TxDisplayFeed::setLocalMiniDemand(bool wanted)
{
    if (m_localMiniDemand == wanted) { return; }
    m_localMiniDemand = wanted;
    if (wanted) { ensureMini(); }
    else if (m_miniViewerIds.empty()) { stopMini(); }
}

TxDisplayView TxDisplayFeed::miniView() const
{
    TxDisplayView view;
    view.carrierHz = carrierHz();
    view.lowHz = -20'000;
    view.highHz = 20'000;
    view.pixels = 1024; // MiniSpec.PIXELS, MeterManager.cs:43362
    return view;
}

int TxDisplayFeed::miniFftSize() const
{
    return m_miniAnalyzer ? m_miniAnalyzer->fftSize() : 0;
}

int TxDisplayFeed::miniOutputFps() const
{
    return m_miniAnalyzer ? m_miniAnalyzer->outputFps() : 0;
}

bool TxDisplayFeed::miniReady() const
{
    return m_miniAttached && m_miniAnalyzer && m_miniAnalyzer->analyzerReady();
}

void TxDisplayFeed::applyMiniRxSettings()
{
    if (!m_miniAnalyzer) { return; }
    auto& settings = AppSettings::instance();
    const auto integer = [&settings](const char* key, int fallback) {
        bool valid = false;
        const int value = settings.value(QString::fromLatin1(key), fallback).toInt(&valid);
        return valid ? value : fallback;
    };
    // Thetis MeterManager.cs:44118-44143 copies the RX analyzer's detector,
    // average, FFT and window settings into MiniSpec even during MOX.
    m_miniAnalyzer->setFftSize(integer("DisplayFftSize", 32768));
    m_miniAnalyzer->setWindowType(integer("DisplayFftWindow", 4));
    m_miniAnalyzer->setPanDetector(integer("DisplaySpectrumDetector", 0));
    m_miniAnalyzer->setPanAveraging(integer("DisplaySpectrumAveraging", 3));
    m_miniAnalyzer->setPanAvTimeMs(integer("DisplaySpectrumAverageTimeMs", 30));
    m_miniAnalyzer->setWfDetector(integer("DisplayWaterfallDetector", 0));
    m_miniAnalyzer->setWfAveraging(integer("DisplayWaterfallAveraging", 0));
    m_miniAnalyzer->setWfAvTimeMs(integer("DisplayWaterfallAverageTimeMs", 120));
}

void TxDisplayFeed::ensureMini()
{
    if (!m_keyed || (!m_localMiniDemand && m_miniViewerIds.empty())
        || m_miniAnalyzer || !m_model || !m_model->txChannel()
        || !m_model->transmitLane()) { return; }
    TxChannel* channel = m_model->txChannel();
    m_miniChannelId = channel->channelId();
    const quint64 epoch = ++m_miniEpoch;
    m_miniAnalyzer = std::make_unique<TxAnalyzer>(TxAnalyzer::kMiniTxDispId,
                                                  nullptr, m_model->transmitLane(),
                                                  /*persistSettings=*/false);
    m_miniAnalyzer->setNumPixels(1024);
    m_miniAnalyzer->setOutputFps(30);
    m_miniAnalyzer->setSampleRate(96000.0);
    // The pending channel resize precedes this analyzer's configuration on
    // the same lane; its requested size is the size the siphon will push.
    m_miniAnalyzer->setBlockSize(channel->txDspBlockSize());
    m_miniAnalyzer->setView(-20'000, 20'000, 1024);
    applyMiniRxSettings();
    connect(m_miniAnalyzer.get(), &TxAnalyzer::txFftReady, this,
            [this](int, const QVector<float>& dbm) {
        if (m_keyed && miniReady()) { emit miniTraceReady(dbm); }
    });
    connect(m_miniAnalyzer.get(), &TxAnalyzer::txWaterfallReady, this,
            [this](int, const QVector<float>& dbm) {
        if (m_keyed && miniReady()) { emit miniWaterfallReady(dbm); }
    });
    connect(m_miniAnalyzer.get(), &TxAnalyzer::analyzerCreated, this,
            [this, epoch](bool ready) {
        if (!ready || epoch != m_miniEpoch || !m_keyed || !m_model
            || !m_model->txChannel()
            || m_model->txChannel()->channelId() != m_miniChannelId
            || (!m_localMiniDemand && m_miniViewerIds.empty())) { return; }
#ifdef HAVE_WDSP
        if (DspControlThread* lane = m_model->transmitLane()) {
            const int channel = m_miniChannelId;
            QPointer<TxChannel> liveChannel = m_model->txChannel();
            // start queues SetAnalyzer and every per-plane setting on this
            // FIFO lane. Attach only after those jobs have run: xsiphon may
            // call Spectrum0 on its next TX block immediately after attach.
            m_miniAnalyzer->start();
            m_miniAttachPending = true;
            lane->request<bool>([channel, liveChannel]() {
                if (!liveChannel || liveChannel->channelId() != channel
                    || !liveChannel->canAttachMiniAnalyzerOnLane()) { return false; }
                int run = 1;
                int display = TxAnalyzer::kMiniTxDispId;
                TXASetSipAllocDisps(channel, 1, &run, &display);
                return true;
            }, this, [this, epoch](bool attached) {
                if (epoch != m_miniEpoch || !m_miniAnalyzer) { return; }
                m_miniAttachPending = false;
                m_miniAttachSettled = true;
                m_miniAttached = attached;
                if (attached) {
                    updateMiniView();
                } else {
                    m_miniAnalyzer->stop();
                }
            });
        }
#endif
    });
}

void TxDisplayFeed::updateMiniView()
{
    if (!miniReady()) { return; }
    const TxDisplayView view = miniView();
    m_miniAnalyzer->setView(view.lowHz, view.highHz, view.pixels);
    emit miniViewChanged(view);
}

void TxDisplayFeed::stopMini()
{
    ++m_miniEpoch;
    if (m_miniAnalyzer) { m_miniAnalyzer->stop(); }
#ifdef HAVE_WDSP
    if ((m_miniAttached || m_miniAttachPending) && m_model && m_model->transmitLane()
        && m_miniChannelId >= 0) {
        const int channel = m_miniChannelId;
        m_model->transmitLane()->post([channel]() {
            TXASetSipAllocDisps(channel, 0, nullptr, nullptr);
        });
    }
#endif
    m_miniAttached = false;
    m_miniAttachPending = false;
    m_miniAttachSettled = false;
    m_miniChannelId = -1;
    m_miniAnalyzer.reset(); // queues DestroyAnalyzer after detach on the same lane
}

} // namespace NereusSDR
