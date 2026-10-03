// =================================================================
// src/gui/TxDisplaySource.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original. Where a window's transmit display
// comes from while its radio is keyed: this computer's own transmit
// analyzer (a window that runs its own DSP), or the Core's transmit
// frames (a remote window). MoxDisplayController drives one of them on the
// pan hosting the transmitting slice. It cites Thetis's DisplayThread for
// the behaviour it follows; it translates no Thetis code.
//
// Thetis (v2.10.3.15), console.cs:24281-24338 (DisplayThread): while MOX is
// on and display duplex is off, the transmitting receiver's display takes
// the transmit analyzer's pixels (GetPixels(cmaster.inid(1, 0), 0, ...) for
// the trace, pixout 1 for the waterfall), never the receiver.
//
// Modification history (NereusSDR):
//   2026-09-26 : Created for remote-window parity Task 29 (A11, R-R3-49,
//                 R-R3-12) by J.J. Boyd (KG4VCF). AI-assisted
//                 implementation via Anthropic Claude Code.
// =================================================================

#pragma once

#include "core/TxAnalyzer.h"
#include "core/session/media/DisplayCodec.h"
#include "core/session/media/RemoteSpectrumContext.h"

#include <QMetaObject>
#include <QPointer>
#include <QString>
#include <QStringList>

#include <functional>

namespace NereusSDR {

class PanadapterStack;
class RemoteMediaController;
class SpectrumWidget;
class StationClient;
class TxDisplayFeed;

/// One source of the transmit display for one pan at a time.
class ITxDisplaySource {
public:
    virtual ~ITxDisplaySource() = default;

    /// The rise: from now on `pan` draws this source's transmit trace and
    /// waterfall around `carrierHz`, at the pan's current view.
    virtual void beginTransmitView(SpectrumWidget* pan, double carrierHz) = 0;
    /// The pan's view while keyed (already held inside the transmit
    /// baseband by the controller) and its width in pixels.
    virtual void requestView(double centreHz, double spanHz, int pixels) = 0;
    /// The fall. `pan` is null when the pan went while keyed (a layout
    /// change); the source still lets go of everything it holds.
    virtual void endTransmitView(SpectrumWidget* pan) = 0;
    /// Whether this source can show the transmit display at all.
    virtual bool available() const = 0;

    /// Where the transmit carrier moved while keyed (XIT, a retune):
    /// MoxDisplayController::carrierChanged.
    void setCarrierSink(std::function<void(double)> sink) { m_carrierSink = std::move(sink); }
    /// Test seam: the widget calls made, appended in order (null: none).
    void setCallLog(QStringList* log) { m_callLog = log; }

protected:
    void note(const QString& call) const
    {
        if (m_callLog != nullptr) {
            m_callLog->append(call);
        }
    }
    void reportCarrier(double carrierHz) const
    {
        if (m_carrierSink) {
            m_carrierSink(carrierHz);
        }
    }

private:
    std::function<void(double)> m_carrierSink;
    QStringList* m_callLog{nullptr};
};

/// A window running its own DSP: a viewer of RadioModel::txDisplayFeed(),
/// `local` true, so its view governs the analyzer; the feed's trace and
/// waterfall go straight into the pan.
class LocalTxDisplaySource final : public ITxDisplaySource {
public:
    explicit LocalTxDisplaySource(TxDisplayFeed* feed);
    ~LocalTxDisplaySource() override;

    void beginTransmitView(SpectrumWidget* pan, double carrierHz) override;
    void requestView(double centreHz, double spanHz, int pixels) override;
    void endTransmitView(SpectrumWidget* pan) override;
    bool available() const override;

    /// The feed's viewer id while a transmit view runs, else 0.
    int viewerId() const { return m_viewerId; }

private:
    void applyView(const TxDisplayView& view);
    void release();

    QPointer<TxDisplayFeed> m_feed;
    QPointer<SpectrumWidget> m_pan;
    int m_viewerId{0};
    QList<QMetaObject::Connection> m_connections;
};

/// A remote window: the pan's endpoint in RemoteMediaController. The pan's
/// receive frames are held while keyed; the Core's transmit context and
/// frames for the pan are drawn. available() is the Core's
/// txDisplayVersion of at least 1; below it the pan says so.
class RemoteTxDisplaySource final : public ITxDisplaySource {
public:
    RemoteTxDisplaySource(RemoteMediaController* media, StationClient* client,
                          PanadapterStack* pans);
    ~RemoteTxDisplaySource() override;

    void beginTransmitView(SpectrumWidget* pan, double carrierHz) override;
    void requestView(double centreHz, double spanHz, int pixels) override;
    void endTransmitView(SpectrumWidget* pan) override;
    bool available() const override;

private:
    void onContext(const QString& panId, const SpectrumContextMessage& context);
    void onFrame(const QString& panId, const DisplayCodecFrame& frame);
    void release();

    QPointer<RemoteMediaController> m_media;
    QPointer<StationClient> m_client;
    QPointer<PanadapterStack> m_pans;
    QPointer<SpectrumWidget> m_pan;
    QString m_panId;
    bool m_contextSeen{false};
    QList<QMetaObject::Connection> m_connections;
};

} // namespace NereusSDR
