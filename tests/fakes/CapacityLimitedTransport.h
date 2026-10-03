#pragma once
// no-port-check: NereusSDR-original test transport. It limits the receive
// rate of the real media transport; no upstream logic is ported here.
//
// Moved from tests/tst_remote_media_controller.cpp so tst_remote_vax_feeder
// can force the lossless link trial's Opus fallback with the same seam.

#include "core/session/media/IMediaTransport.h"
#include "core/session/media/LibDataChannelMediaTransport.h"

#include <QElapsedTimer>

#include <algorithm>
#include <optional>

// R-R3-23: a network with too little room for lossless audio. This
// computer's media transport is the real one (DTLS/SRTP over loopback), but
// received audio beyond a fixed byte rate is lost, as on a slow link: Opus
// (24 kbit/s) fits, lossless (about 1.6 Mbit/s) loses most of its packets.
// Built through the MediaPeer::TransportFactory seam the GUI's controller
// takes.
class CapacityLimitedTransport final : public NereusSDR::IMediaTransport {
public:
    // 400 kbit/s, with a burst of about five lossless packets.
    static constexpr double kBytesPerSecond = 50'000.0;
    static constexpr double kBurstBytes = 4'000.0;

    explicit CapacityLimitedTransport(QObject* parent)
        : IMediaTransport(parent), m_inner(new NereusSDR::LibDataChannelMediaTransport(this))
    {
        connect(m_inner, &IMediaTransport::localDescription, this, &IMediaTransport::localDescription);
        connect(m_inner, &IMediaTransport::localCandidate, this, &IMediaTransport::localCandidate);
        connect(m_inner, &IMediaTransport::displayReceived, this, &IMediaTransport::displayReceived);
        connect(m_inner, &IMediaTransport::ready, this, &IMediaTransport::ready);
        connect(m_inner, &IMediaTransport::closed, this, &IMediaTransport::closed);
        connect(m_inner, &IMediaTransport::connectionFailed, this, &IMediaTransport::connectionFailed);
        connect(m_inner, &IMediaTransport::errorOccurred, this, &IMediaTransport::errorOccurred);
        connect(m_inner, &IMediaTransport::displayWritable, this, &IMediaTransport::displayWritable);
        connect(m_inner, &IMediaTransport::displayErrorOccurred,
                this, &IMediaTransport::displayErrorOccurred);
        connect(m_inner, &IMediaTransport::rtpReceived, this, [this](const QByteArray& packet) {
            if (admit(packet.size())) {
                ++passed;
                emit rtpReceived(packet);
            } else {
                ++dropped;
            }
        });
        m_clock.start();
    }
    bool start(const StartOptions& options) override { return m_inner->start(options); }
    void stop() override { m_inner->stop(); }
    bool acceptDescription(const QString& sdp, const QString& type) override
    {
        return m_inner->acceptDescription(sdp, type);
    }
    bool acceptCandidate(const QString& candidate, const QString& mid) override
    {
        return m_inner->acceptCandidate(candidate, mid);
    }
    bool sendDisplay(const QByteArray& message) override { return m_inner->sendDisplay(message); }
    DisplaySendResult submitDisplay(const QByteArray& message) override
    {
        return m_inner->submitDisplay(message);
    }
    bool displayBusy() const override { return m_inner->displayBusy(); }
    bool sendRtp(const QByteArray& packet) override { return m_inner->sendRtp(packet); }
    bool isReady() const override { return m_inner->isReady(); }
    bool losslessAudioNegotiated() const override { return m_inner->losslessAudioNegotiated(); }
    std::optional<NereusSDR::MediaTransportTelemetry> telemetry() const override { return m_inner->telemetry(); }

    int passed = 0;
    int dropped = 0;

private:
    bool admit(qsizetype bytes)
    {
        const qint64 now = m_clock.nsecsElapsed();
        m_tokens = std::min(kBurstBytes,
                            m_tokens + double(now - m_lastNs) * kBytesPerSecond / 1e9);
        m_lastNs = now;
        if (m_tokens < double(bytes)) { return false; }
        m_tokens -= double(bytes);
        return true;
    }
    NereusSDR::LibDataChannelMediaTransport* m_inner;
    QElapsedTimer m_clock;
    qint64 m_lastNs = 0;
    double m_tokens = kBurstBytes;
};

