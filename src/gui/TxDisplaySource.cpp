// =================================================================
// src/gui/TxDisplaySource.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original. See TxDisplaySource.h.
//
// Modification history (NereusSDR):
//   2026-09-26 : Created for remote-window parity Task 29 (A11, R-R3-49,
//                 R-R3-12) by J.J. Boyd (KG4VCF). AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-09-26 : Tasks 27-29 fix wave (R-R3-49): the remote source replays
//                 a transmit context that beat the rise, and asks the Core
//                 again only when it sends a transmit display. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "gui/TxDisplaySource.h"

#include "core/TxDisplayFeed.h"
#include "core/session/StationCapabilities.h"
#include "core/session/StationClient.h"
#include "gui/PanadapterApplet.h"
#include "gui/PanadapterStack.h"
#include "gui/RemoteMediaController.h"
#include "gui/SpectrumWidget.h"

namespace NereusSDR {

// ── LocalTxDisplaySource ───────────────────────────────────────────────────

LocalTxDisplaySource::LocalTxDisplaySource(TxDisplayFeed* feed)
    : m_feed(feed)
{
}

LocalTxDisplaySource::~LocalTxDisplaySource()
{
    release();
}

bool LocalTxDisplaySource::available() const
{
    return !m_feed.isNull();
}

void LocalTxDisplaySource::beginTransmitView(SpectrumWidget* pan, double carrierHz)
{
    Q_UNUSED(carrierHz);  // The feed computes the carrier from the model.
    release();
    m_pan = pan;
    if (m_pan.isNull() || m_feed.isNull()) {
        return;
    }

    // The window's own pan governs the analyzer (Task 28: the local viewer
    // governs whenever it is present), so the analyzer's window is always
    // what this pan shows; one data point per pixel, as Thetis's
    // initAnalyzer derives span and display window from one zoom state
    // (specHPSDR.cs:565-584 [v2.10.3.15]).
    m_viewerId = m_feed->addViewer(pan->centerFrequency(), pan->bandwidth(), pan->width(),
                                   /*local=*/true);
    // Tell the widget where the analyzer's bins sit, so visibleBinRange maps
    // them to the axis the operator reads.
    applyView(m_feed->currentView());

    // The feed's trace and waterfall, straight into the transmit paths.
    // TxAnalyzer's output already had the TX Display detector and averaging
    // applied by WDSP; updateSpectrumLinear would run the RECEIVE detector
    // and avenger over it a second time. Found by Codex on PR #317. The
    // context object is the pan, so a pan destroyed while keyed (a layout
    // change) takes its connections with it.
    SpectrumWidget* sw = m_pan.data();
    m_connections.append(QObject::connect(m_feed, &TxDisplayFeed::traceReady, sw,
                                          [sw](const QVector<float>& dbm) {
        sw->updateSpectrumFromTxPixels(-1, dbm);
    }));
    note(QStringLiteral("connect updateSpectrumFromTxPixels"));
    // 3M-5d: the waterfall plane is pixout 1 (DetTypeWF + AverageModeWF
    // applied inside WDSP), wired straight into pushTxWaterfallRow.
    m_connections.append(QObject::connect(m_feed, &TxDisplayFeed::waterfallReady, sw,
                                          [sw](const QVector<float>& dbm) {
        sw->pushTxWaterfallRow(-1, dbm);
    }));
    note(QStringLiteral("connect pushTxWaterfallRow"));
    // Row 16: the feed follows the transmit slice's frequency and XIT while
    // keyed and re-announces the view; the widget's bins and the pan's view
    // follow it (Thetis console.cs:22069-22150 [v2.10.3.15],
    // getLowHighForRXn, "xit, only when txing").
    m_connections.append(QObject::connect(m_feed, &TxDisplayFeed::viewChanged, sw,
                                          [this](const TxDisplayView& view) {
        if (!m_feed.isNull() && m_feed->isGoverning(m_viewerId)) {
            applyView(view);
        }
    }));
}

void LocalTxDisplaySource::requestView(double centreHz, double spanHz, int pixels)
{
    if (m_feed.isNull() || m_viewerId == 0) {
        return;
    }
    m_feed->updateViewer(m_viewerId, centreHz, spanHz, pixels);
    // Explicitly, rather than trusting viewChanged: the feed announces only
    // while keyed and only when the view moved.
    applyView(m_feed->currentView());
}

void LocalTxDisplaySource::endTransmitView(SpectrumWidget* pan)
{
    Q_UNUSED(pan);
    if (!m_connections.isEmpty()) {
        note(QStringLiteral("disconnect updateSpectrumFromTxPixels"));
        note(QStringLiteral("disconnect pushTxWaterfallRow"));
    }
    release();
}

void LocalTxDisplaySource::applyView(const TxDisplayView& view)
{
    if (m_pan.isNull() || view.empty()) {
        return;
    }
    // The analyzer's edges are relative to the carrier, in 100 Hz steps
    // (TxAnalyzer::clampViewToBaseband): its bins are centred there and
    // span exactly the window.
    m_pan->setTxCenterFrequency(view.centreHz());
    note(QStringLiteral("setTxCenterFrequency"));
    m_pan->setTxSampleRate(view.spanHz());
    note(QStringLiteral("setTxSampleRate"));
    reportCarrier(view.carrierHz);
}

void LocalTxDisplaySource::release()
{
    for (const QMetaObject::Connection& connection : std::as_const(m_connections)) {
        QObject::disconnect(connection);
    }
    m_connections.clear();
    if (!m_feed.isNull() && m_viewerId != 0) {
        m_feed->removeViewer(m_viewerId);
    }
    m_viewerId = 0;
    m_pan.clear();
}

// ── RemoteTxDisplaySource ──────────────────────────────────────────────────

RemoteTxDisplaySource::RemoteTxDisplaySource(RemoteMediaController* media,
                                             StationClient* client,
                                             PanadapterStack* pans)
    : m_media(media)
    , m_client(client)
    , m_pans(pans)
{
    if (m_media.isNull()) {
        return;
    }
    m_connections.append(QObject::connect(
        m_media, &RemoteMediaController::transmitContextReceived, m_media,
        [this](const QString& panId, const SpectrumContextMessage& context) {
            onContext(panId, context);
        }));
    m_connections.append(QObject::connect(
        m_media, &RemoteMediaController::transmitFrameReceived, m_media,
        [this](const QString& panId, const DisplayCodecFrame& frame) {
            onFrame(panId, frame);
        }));
}

RemoteTxDisplaySource::~RemoteTxDisplaySource()
{
    release();
    for (const QMetaObject::Connection& connection : std::as_const(m_connections)) {
        QObject::disconnect(connection);
    }
}

bool RemoteTxDisplaySource::available() const
{
    return !m_client.isNull() && m_client->capabilities().txDisplayVersion >= 1;
}

void RemoteTxDisplaySource::beginTransmitView(SpectrumWidget* pan, double carrierHz)
{
    Q_UNUSED(carrierHz);  // The Core's transmit context names the carrier.
    release();
    m_pan = pan;
    m_contextSeen = false;
    m_panId.clear();
    if (m_pan.isNull() || m_pans.isNull()) {
        return;
    }
    for (PanadapterApplet* applet : m_pans->allApplets()) {
        if (applet != nullptr && applet->spectrumWidget() == pan) {
            m_panId = applet->panId();
            break;
        }
    }
    if (m_panId.isEmpty() || m_media.isNull()) {
        return;
    }
    // Receive frames for this pan are held from now on (they are the
    // receiver hearing its own transmitter); a Core that sends no transmit
    // display gets the pan's status line saying so.
    m_media->setPanTransmitting(m_panId, true, !available());
    if (!available()) {
        // No transmit display to ask for: asking again would only re-plan
        // the receive endpoint (and perhaps the shared FFT engine) on a
        // view the pan does not draw.
        return;
    }
    // The Core's transmit context can land before this rise (media and
    // transmit state travel on different channels): take the one it holds,
    // or the pan stays blank for the whole key when the request below does
    // not change and no new context follows.
    if (const std::optional<SpectrumContextMessage> held = m_media->heldTransmitContext(m_panId)) {
        onContext(m_panId, *held);
        note(QStringLiteral("replay held transmit context"));
    }
    // The pan's view moved to the carrier: ask the Core for it now, with the
    // transmit window, so its transmit context lands on this view.
    m_media->refreshTransmitView();
    note(QStringLiteral("refreshTransmitView"));
}

void RemoteTxDisplaySource::requestView(double centreHz, double spanHz, int pixels)
{
    Q_UNUSED(centreHz);
    Q_UNUSED(spanHz);
    Q_UNUSED(pixels);
    // The subscription carries the pan's view and width as the pan shows
    // them; asking again is the whole request.
    if (!m_media.isNull() && !m_panId.isEmpty() && available()) {
        m_media->refreshTransmitView();
        note(QStringLiteral("refreshTransmitView"));
    }
}

void RemoteTxDisplaySource::endTransmitView(SpectrumWidget* pan)
{
    Q_UNUSED(pan);
    release();
}

void RemoteTxDisplaySource::onContext(const QString& panId,
                                      const SpectrumContextMessage& context)
{
    if (m_pan.isNull() || panId != m_panId || context.spanHz <= 0.0) {
        return;
    }
    m_contextSeen = true;
    // The Core's transmit view: its bins are centred at centreHz and span
    // spanHz, and sourceCentreHz is the carrier (dial plus XIT).
    m_pan->setTxCenterFrequency(context.centreHz);
    m_pan->setTxSampleRate(context.spanHz);
    reportCarrier(context.sourceCentreHz);
}

void RemoteTxDisplaySource::onFrame(const QString& panId, const DisplayCodecFrame& frame)
{
    if (m_pan.isNull() || panId != m_panId || !m_contextSeen) {
        return;
    }
    // The existing transmit paths, as a local window feeds them from its
    // own analyzer: the trace, and the waterfall when the Core advanced it.
    m_pan->updateSpectrumFromTxPixels(-1, frame.traceDbm);
    if (frame.waterfallAdvance && !frame.waterfallDbm.isEmpty()) {
        m_pan->pushTxWaterfallRow(-1, frame.waterfallDbm);
    }
}

void RemoteTxDisplaySource::release()
{
    if (!m_media.isNull() && !m_panId.isEmpty()) {
        m_media->setPanTransmitting(m_panId, false, false);
        if (available()) {
            m_media->refreshTransmitView();
            note(QStringLiteral("refreshTransmitView"));
        }
    }
    m_panId.clear();
    m_pan.clear();
    m_contextSeen = false;
}

} // namespace NereusSDR
