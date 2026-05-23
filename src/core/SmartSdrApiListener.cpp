// =================================================================
// src/core/SmartSdrApiListener.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-native class. No upstream port.
//
// Wire format reverse-engineered from FLEX-8600 v4.2.18.41174 SmartSDR
// TCP session 2 in
// captures/flex-pgxl-tgxl-capture_00001_20260519173452.pcapng.
//
// AI tooling: Anthropic Claude Code.

#include "SmartSdrApiListener.h"

#include <QLoggingCategory>
#include <QHostAddress>
#include <QDateTime>
#include <QRandomGenerator>

namespace NereusSDR {

Q_LOGGING_CATEGORY(lcSmartSdr, "nereus.smartsdr")

SmartSdrApiListener::SmartSdrApiListener(QObject* parent)
    : QObject(parent)
{
    connect(&m_server, &QTcpServer::newConnection,
            this, &SmartSdrApiListener::onNewConnection);

    // 1 Hz periodic S-frame push so PGXL/TGXL stay synced even if no
    // explicit change happened. PGXL's bandA tracking benefits from a
    // regular heartbeat; without it, accessories can drop state during a
    // long idle period.
    m_periodicTimer.setInterval(1000);
    connect(&m_periodicTimer, &QTimer::timeout,
            this, &SmartSdrApiListener::onPeriodicTick);
}

bool SmartSdrApiListener::start()
{
    // AnyIPv4 (not Any) because Qt's default Any binds IPv6-only on macOS,
    // which silently blocks IPv4 clients like Windows PowerGeniusDesktop.
    return start(QHostAddress::AnyIPv4, 4992);
}

bool SmartSdrApiListener::start(QHostAddress bindAddr, quint16 port)
{
    if (m_server.isListening()) {
        m_server.close();
    }
    bool ok = m_server.listen(bindAddr, port);
    if (!ok) {
        qCWarning(lcSmartSdr) << "failed to bind"
                               << bindAddr.toString() << ":" << port
                               << ":" << m_server.errorString();
        return false;
    }
    // 2026-05-21 4o3a-lan-ptt-pcap-divergence.md §8 C1: generate synthetic
    // local-client handle once per listener boot. Stable for the lifetime
    // of this start() call. Consumed by every interlock S-frame builder.
    m_localClientHandle = generateHandle();
    qCInfo(lcSmartSdr) << "local-client handle:" << m_localClientHandle;
    m_periodicTimer.start();
    qCInfo(lcSmartSdr) << "SmartSDR API listener listening on"
                       << m_server.serverAddress().toString()
                       << ":" << m_server.serverPort();
    return true;
}

void SmartSdrApiListener::stop()
{
    // PR #279 review #2 (2026-05-23): tear down accepted clients + cancel
    // pending interlock state on stop().  Earlier this only stopped the
    // periodic timer + closed the listening server; QTcpSockets already
    // accepted remained in m_clients with their readyRead handlers wired,
    // so an already-connected PGXL / TGXL could keep driving tune /
    // interlock state after the operator toggled 4O3A off in Setup.
    m_periodicTimer.stop();
    m_pttAckTimeout.stop();
    m_server.close();

    // Disconnect signals from each socket so the dangling deleteLater()
    // callbacks don't try to update m_clients while we're clearing it.
    // Mirror onClientDisconnected's deleteLater() so Qt cleans up the
    // socket after the event loop unwinds.
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        QTcpSocket* sock = it.key();
        if (sock) {
            QObject::disconnect(sock, nullptr, this, nullptr);
            sock->abort();
            sock->deleteLater();
        }
    }
    m_clients.clear();

    // Reset interlock-related local state.  m_localClientHandle is
    // regenerated on the next start() (line ~61); clearing it here makes
    // any post-stop frame-builder helper that fires from a deleteLater
    // tail produce an obviously-empty client_handle= instead of a stale
    // one.  m_lastTuneInitiator likewise.
    m_localClientHandle.clear();
    m_lastTuneInitiator.clear();
}

bool SmartSdrApiListener::isListening() const
{
    return m_server.isListening();
}

void SmartSdrApiListener::setSliceFrequencyHz(int sliceId, qint64 freqHz)
{
    // Only slice 0 (VFO A) is exposed for now. Multi-slice (B/C/D) follows
    // in the multi-pan epic.
    if (sliceId != 0) { return; }
    if (m_sliceFreqHz == freqHz) { return; }
    m_sliceFreqHz = freqHz;
    broadcastSliceState();
}

void SmartSdrApiListener::setSliceMode(int sliceId, const QString& mode)
{
    if (sliceId != 0) { return; }
    if (m_sliceMode == mode) { return; }
    m_sliceMode = mode;
    broadcastSliceState();
}

void SmartSdrApiListener::setTxActive(bool active)
{
    if (m_txActive == active) { return; }
    m_txActive = active;
    broadcastSliceState();
}

void SmartSdrApiListener::setTuneActive(bool active)
{
    if (m_tuneActive == active) { return; }
    m_tuneActive = active;
    qCInfo(lcSmartSdr) << "tune state change -> broadcasting transmit tune=" << (active ? 1 : 0);
    broadcastSliceState();
}

bool SmartSdrApiListener::hasInterlockedAmp() const
{
    for (auto it = m_clients.cbegin(); it != m_clients.cend(); ++it) {
        if (it->interlockId != 0 && it->interlockEnabled) {
            return true;
        }
    }
    return false;
}

QString SmartSdrApiListener::ampModelForHandle(const QString& ampHandle) const
{
    // ampHandle in our convention == client's banner handle (uppercase hex
    // string set by sendBanner). Compare case-insensitively since wire
    // commands can use either case.
    for (auto it = m_clients.cbegin(); it != m_clients.cend(); ++it) {
        if (!it->ampHandle.isEmpty()
            && it->ampHandle.compare(ampHandle, Qt::CaseInsensitive) == 0) {
            return it->ampModel;
        }
    }
    return {};
}

void SmartSdrApiListener::setInterlockTransmitting(bool transmitting,
                                                   const QString& source)
{
    if (m_clients.isEmpty()) {
        // No subscribers, so vacuously granted. Emit interlockGranted on
        // engage so any RadioModel orchestration gated on it (e.g. the
        // RF-flow gate in MoxController::txReady -> TxChannel::setRunning)
        // proceeds without a 1.5 s failsafe stall. No wire frames sent;
        // there's nobody to send them to.
        if (transmitting) {
            emit interlockGranted(source);
        }
        return;
    }

    // Wiki-canonical wire source values: "MIC" for voice MOX (operator
    // pressed MIC PTT or MOX in the radio's UI) and "TUNE" for the TUN
    // function (CW carrier). Per the wiki literal example in
    // TCPIP-interlock.md: `S0|interlock state=PTT_REQUESTED reason=...
    // source=MIC tx_allowed=1`. Our internal source label "MOX" (from
    // MoxController) maps to wire "MIC"; "TUNE" passes through.
    const QString wireSource = (source == QStringLiteral("MOX"))
        ? QStringLiteral("MIC")
        : source;

    if (transmitting) {
        // ── Step 3 of the canonical handshake: emit PTT_REQUESTED.
        // Reset per-amp ackReady flags, broadcast PTT_REQUESTED, then
        // arm the 500 ms wiki-spec ACK timeout. The TRANSMITTING
        // advance happens in advanceToTransmittingIfReady() once each
        // registered interlock amp ACKs via `C|interlock ready <id>`.

        m_pttPendingSource = wireSource;
        // Reset ACK state for every registered+enabled interlock amp,
        // PRESERVING recent pre-acks (within last 500 ms).
        //
        // 2026-05-20 bench fix: TGXL hardware TUNE button press sends
        // `transmit tune on` and `interlock ready N` in the same TCP
        // burst BEFORE our PTT_REQUESTED is broadcast (the request comes
        // ~230 ms later after PGXL standby + MoxController walk). Without
        // preserving the recent ack, we'd wipe it on engage, TGXL would
        // not re-send (since it already acked), and we'd time out with
        // grant never firing. setRunning never called -> no RF -> TGXL
        // reports "low input power" and aborts.
        //
        // The 500 ms window matches our existing PTT ACK timeout: any
        // ack arriving within that window of engage is considered valid
        // for this cycle. Acks older than 500 ms are stale (from a
        // previous cycle) and get wiped.
        static constexpr qint64 kPreAckWindowMs = 500;
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        bool anyInterlockedAmp = false;
        for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
            if (it->interlockId != 0 && it->interlockEnabled) {
                const bool recentAck = it->ackReady
                    && (nowMs - it->ackReceivedMs) < kPreAckWindowMs;
                if (!recentAck) {
                    it->ackReady = false;
                }
                anyInterlockedAmp = true;
            }
        }

        // 2026-05-21 4o3a-lan-ptt-pcap-divergence.md §8 C6: no explicit
        // pttA=1 push anywhere in the listener. The canonical PTT-in
        // indicator is the S0|amplifier 0x<h> state=TRANSMIT_A frame
        // broadcast 5 ms after TRANSMITTING in broadcastTransmitting().
        // Amps derive their PTT-in display from S0|interlock state= and
        // S0|amplifier state= per pcap T+167.734 -> T+167.740.

        // 2026-05-21 4o3a-lan-ptt-pcap-divergence.md §8 C2: ONE
        // PTT_REQUESTED frame broadcast to every subscriber, with
        // canonical fields. reason=AMP:<initiating-amp-name>;
        // amplifier= empty; tx_client_handle is the synthetic local
        // handle. Matches pcap T+167.678.
        if (anyInterlockedAmp) {
            const QString reasonName = initiatingAmpName(wireSource);
            const QString reasonField = reasonName.isEmpty()
                ? QString()
                : QStringLiteral("AMP:") + reasonName;
            const QString body =
                QStringLiteral("interlock tx_client_handle=0x%1"
                               " state=PTT_REQUESTED reason=%2"
                               " source=%3 tx_allowed=1 amplifier=")
                    .arg(m_localClientHandle)
                    .arg(reasonField)
                    .arg(wireSource);
            const QByteArray frame =
                QStringLiteral("S0|%1\n").arg(body).toUtf8();
            for (auto jt = m_clients.cbegin(); jt != m_clients.cend(); ++jt) {
                QTcpSocket* sock = jt.key();
                if (sock && sock->isOpen()) { sock->write(frame); }
            }
            qCInfo(lcSmartSdr) << "TX S0|interlock state=PTT_REQUESTED"
                               << "tx_client_handle=0x" << m_localClientHandle
                               << "reason=" << reasonField
                               << "source=" << wireSource
                               << "(canonical single-frame; waiting for interlock ready ACKs)";

            // Arm the 500 ms wiki-spec timeout (unchanged from C1 state).
            if (!m_pttAckTimeout.isActive()) {
                m_pttAckTimeout.setSingleShot(true);
                m_pttAckTimeout.setInterval(500);
                disconnect(&m_pttAckTimeout, &QTimer::timeout, nullptr, nullptr);
                connect(&m_pttAckTimeout, &QTimer::timeout,
                        this, &SmartSdrApiListener::onPttAckTimeout);
            }
            m_pttAckTimeout.start();
            QTimer::singleShot(0, this,
                               &SmartSdrApiListener::advanceToTransmittingIfReady);
        } else {
            // No registered amp interlocks: emit TRANSMITTING directly
            // with a single S0|interlock so any SmartSDR-API client (e.g.
            // a status-display UI) sees the radio is keyed. amplifier=
            // stays empty per wiki when no amp is in the chain.
            // 2026-05-21 4o3a-lan-ptt-pcap-divergence.md §8 C1: use the
            // synthetic local-client handle even on the no-amps fallback
            // path. (C3 will keep this as a single frame.)
            const QString body =
                QStringLiteral("interlock tx_client_handle=0x%1"
                               " state=TRANSMITTING reason="
                               " source=%2 tx_allowed=1 amplifier=")
                    .arg(m_localClientHandle)
                    .arg(wireSource);
            const QByteArray frame =
                QStringLiteral("S0|%1\n").arg(body).toUtf8();
            for (auto jt = m_clients.cbegin(); jt != m_clients.cend(); ++jt) {
                QTcpSocket* sock = jt.key();
                if (sock && sock->isOpen()) { sock->write(frame); }
            }
            qCInfo(lcSmartSdr) << "TX S0|interlock state=TRANSMITTING"
                               << "source=" << wireSource
                               << "(no registered amp interlocks)";
            // Also push pttA=1 to any amp that did `amplifier create`
            // without `interlock create` -- their UI still binds against
            // the amplifier state frame even if they opted out of the
            // interlock ACK chain.
            for (auto it = m_clients.cbegin(); it != m_clients.cend(); ++it) {
                if (it->ampHandle.isEmpty()) { continue; }
                const QString ampBody = QStringLiteral("amplifier 0x%1 pttA=1")
                                            .arg(it->ampHandle);
                const QByteArray ampFrame =
                    QStringLiteral("S%1|%2\n").arg(it->handle)
                                              .arg(ampBody).toUtf8();
                QTcpSocket* sock = it.key();
                if (sock && sock->isOpen()) { sock->write(ampFrame); }
            }
            // Still fire interlockGranted so RadioModel orchestration
            // (e.g. TGXL autotune wait + RF-flow gate) advances even
            // when no amp has registered an interlock yet.
            emit interlockGranted(wireSource);
        }
    } else {
        // 2026-05-21 4o3a-lan-ptt-pcap-divergence.md §8 C5: canonical
        // un-key sequence (UNKEY_REQUESTED then 2-frame READY pair). The
        // initiator name is the one recorded for the in-flight TX (TUNE
        // path) or the first PGXL-class amp's name (MIC/MOX path).
        m_pttAckTimeout.stop();
        const QString initiator = m_lastTuneInitiator.isEmpty()
            ? initiatingAmpName(QStringLiteral("MIC"))
            : m_lastTuneInitiator;
        m_pttPendingSource.clear();

        // 2026-05-21 4o3a-lan-ptt-pcap-divergence.md §8 C6: no explicit
        // pttA=0 push. Canonical FLEX never sends it. broadcastUnkey-
        // Sequence handles the state=IDLE broadcast 5 ms after the second
        // READY frame so it lands at pcap T+168.881 timing.
        broadcastUnkeySequence(initiator);
    }
}

void SmartSdrApiListener::advanceToTransmittingIfReady()
{
    if (m_pttPendingSource.isEmpty()) { return; }

    int totalInterlocks = 0;
    int readyCount = 0;
    for (auto it = m_clients.cbegin(); it != m_clients.cend(); ++it) {
        if (it->interlockId == 0 || !it->interlockEnabled) { continue; }
        ++totalInterlocks;
        if (it->ackReady) { ++readyCount; }
    }
    if (totalInterlocks == 0) { return; }
    if (readyCount < totalInterlocks) { return; }

    // All registered+enabled amps have ACKed. Stop the timeout, capture
    // the source, then schedule broadcastTransmitting() 30 ms in the
    // future per pcap T+167.704 -> T+167.734.
    m_pttAckTimeout.stop();
    const QString source = m_pttPendingSource;
    m_pttPendingSource.clear();

    // 2026-05-21 4o3a-lan-ptt-pcap-divergence.md §8 C4: 30 ms settle
    // before TRANSMITTING + RF-flow gate release so TGXL relays have
    // time to switch to TRANSMIT position before PGXL sees carrier.
    QTimer::singleShot(30, this, [this, source]() {
        broadcastTransmitting(source);
    });
}

void SmartSdrApiListener::broadcastTransmitting(const QString& source)
{
    // 2026-05-21 4o3a-lan-ptt-pcap-divergence.md §8 C3 + C4:
    // single S0|interlock state=TRANSMITTING frame with comma-separated
    // amplifier list, emitted 30 ms after the last ACK arrives.
    QStringList ampHandles;
    for (auto it = m_clients.cbegin(); it != m_clients.cend(); ++it) {
        if (it->interlockId == 0 || !it->interlockEnabled) { continue; }
        if (!it->ampHandle.isEmpty()) {
            ampHandles << (QStringLiteral("0x") + it->ampHandle);
        }
    }
    const QString body =
        QStringLiteral("interlock tx_client_handle=0x%1"
                       " state=TRANSMITTING reason= source=%2"
                       " tx_allowed=1 amplifier=%3")
            .arg(m_localClientHandle)
            .arg(source)
            .arg(ampHandles.join(QLatin1Char(',')));
    const QByteArray frame =
        QStringLiteral("S0|%1\n").arg(body).toUtf8();
    for (auto jt = m_clients.cbegin(); jt != m_clients.cend(); ++jt) {
        QTcpSocket* sock = jt.key();
        if (sock && sock->isOpen()) { sock->write(frame); }
    }
    qCInfo(lcSmartSdr) << "TX S0|interlock state=TRANSMITTING"
                       << "source=" << source
                       << "amplifier=" << ampHandles.join(QLatin1Char(','))
                       << "(post 30 ms settle)";

    // 2026-05-21 4o3a-lan-ptt-pcap-divergence.md §8 C6: replace the
    // explicit pttA=1 push with canonical state=TRANSMIT_A broadcasts,
    // emitted ~5 ms after TRANSMITTING (pcap T+167.734 -> T+167.740).
    // Per keyed amp, broadcast to every subscriber. Amps derive their
    // PTT-in display from state= now; pttA= is no longer needed.
    QStringList keyedHandles;
    for (auto it = m_clients.cbegin(); it != m_clients.cend(); ++it) {
        if (it->interlockId == 0 || !it->interlockEnabled) { continue; }
        if (!it->ampHandle.isEmpty()) {
            keyedHandles << it->ampHandle;
        }
    }
    QTimer::singleShot(5, this, [this, keyedHandles]() {
        for (const QString& h : keyedHandles) {
            const QString body = QStringLiteral("amplifier 0x%1 state=TRANSMIT_A").arg(h);
            const QByteArray frame =
                QStringLiteral("S0|%1\n").arg(body).toUtf8();
            for (auto jt = m_clients.cbegin(); jt != m_clients.cend(); ++jt) {
                QTcpSocket* sock = jt.key();
                if (sock && sock->isOpen()) { sock->write(frame); }
            }
            qCInfo(lcSmartSdr) << "TX S0|amplifier 0x" << h << "state=TRANSMIT_A";
        }
    });

    emit interlockGranted(source);
}

void SmartSdrApiListener::broadcastUnkeySequence(const QString& initiatorName)
{
    const QString reasonField = initiatorName.isEmpty()
        ? QString()
        : QStringLiteral("AMP:") + initiatorName;

    auto broadcast = [this](const QString& body) {
        const QByteArray frame =
            QStringLiteral("S0|%1\n").arg(body).toUtf8();
        for (auto jt = m_clients.cbegin(); jt != m_clients.cend(); ++jt) {
            QTcpSocket* sock = jt.key();
            if (sock && sock->isOpen()) { sock->write(frame); }
        }
    };

    // Frame 1: UNKEY_REQUESTED reason=AMP:<initiator>.
    const QString unkeyBody =
        QStringLiteral("interlock tx_client_handle=0x%1"
                       " state=UNKEY_REQUESTED reason=%2"
                       " source= tx_allowed=1 amplifier=")
            .arg(m_localClientHandle).arg(reasonField);
    broadcast(unkeyBody);
    qCInfo(lcSmartSdr) << "TX S0|interlock state=UNKEY_REQUESTED"
                       << "reason=" << reasonField;

    // Frame 2: READY (empty reason), 2 ms later.
    const QString readyEmptyBody =
        QStringLiteral("interlock tx_client_handle=0x%1"
                       " state=READY reason= source= tx_allowed=1"
                       " amplifier=")
            .arg(m_localClientHandle);
    QTimer::singleShot(2, this, [broadcast, readyEmptyBody]() {
        broadcast(readyEmptyBody);
    });

    // Frame 3: READY reason=AMP:<initiator>, ~0.5 ms after frame 2 (round
    // up to 3 ms total post-UNKEY since QTimer resolution on most
    // platforms is 1 ms minimum).
    const QString readyNamedBody =
        QStringLiteral("interlock tx_client_handle=0x%1"
                       " state=READY reason=%2 source= tx_allowed=1"
                       " amplifier=")
            .arg(m_localClientHandle).arg(reasonField);
    QTimer::singleShot(3, this, [broadcast, readyNamedBody]() {
        broadcast(readyNamedBody);
    });

    // 2026-05-21 4o3a-lan-ptt-pcap-divergence.md §8 C6: state=IDLE
    // broadcast ~5 ms after the second READY frame (pcap T+168.881).
    // One per amp that was keyed.
    QStringList keyedHandles;
    for (auto it = m_clients.cbegin(); it != m_clients.cend(); ++it) {
        if (it->interlockId == 0 || !it->interlockEnabled) { continue; }
        if (!it->ampHandle.isEmpty()) {
            keyedHandles << it->ampHandle;
        }
    }
    QTimer::singleShot(8, this, [this, keyedHandles]() {
        for (const QString& h : keyedHandles) {
            const QString body = QStringLiteral("amplifier 0x%1 state=IDLE").arg(h);
            const QByteArray frame =
                QStringLiteral("S0|%1\n").arg(body).toUtf8();
            for (auto jt = m_clients.cbegin(); jt != m_clients.cend(); ++jt) {
                QTcpSocket* sock = jt.key();
                if (sock && sock->isOpen()) { sock->write(frame); }
            }
            qCInfo(lcSmartSdr) << "TX S0|amplifier 0x" << h << "state=IDLE";
        }
    });
}

void SmartSdrApiListener::onPttAckTimeout()
{
    // Per SmartSDR API wiki TCPIP-interlock (verbatim quote):
    //   "Failure to send [interlock ready] within the timeout period of
    //   the interlock (generally 500ms) will result in an error message
    //   indicating that the external device is preventing transmit from
    //   occurring."
    //
    // FlexLib source confirms this is event-driven (Radio.cs:7440-7450:
    // InterlockState is mutated ONLY from inbound status broadcasts; no
    // client-side fallthrough timer).
    //
    // Source: https://github.com/flexradio/smartsdr-api-docs/wiki/TCPIP-interlock
    //         https://github.com/jeffu231/Flexlib/blob/master/FlexLib/Radio.cs#L7440
    //
    // Bench reality: amps in NereusSDR's test setup reconnect frequently
    // (~14 s cycles for TGXL). An amp that disconnects DURING our 500 ms
    // ACK wait is gone, not blocking. The wiki's "amp blocking transmit"
    // semantic only applies when the amp is connected and refusing to ACK.
    // We therefore treat amps that disappeared from m_clients as
    // implicitly granting (they're no longer in the chain to block).
    if (m_pttPendingSource.isEmpty()) { return; }  // already resolved

    int totalConnectedInterlocks = 0;
    int readyCount = 0;
    QStringList laggards;
    for (auto it = m_clients.cbegin(); it != m_clients.cend(); ++it) {
        if (it->interlockId == 0 || !it->interlockEnabled) { continue; }
        ++totalConnectedInterlocks;
        if (it->ackReady) {
            ++readyCount;
        } else {
            laggards << it->interlockName;
        }
    }

    // 2026-05-20 bench-driven policy change: ALWAYS advance to TRANSMITTING
    // on timeout, regardless of laggards.
    //
    // The wiki's "block TX on amp non-ACK" semantic does not match real-
    // world 4O3A behavior at the bench. Two key observations:
    //
    //   1. TGXL (`name=TG`, type=AMP) takes ~1.4 s to send `interlock
    //      ready <id>` -- WAY longer than the wiki's 500 ms window. We
    //      always time out before TGXL ACKs. Yet TGXL clearly *intends*
    //      to participate (it sends the create + the late ACK).
    //
    //   2. PGXL transitions IDLE -> TRANSMIT_A on PTT_REQUESTED alone
    //      (within ~80 ms of its own ACK at ~180 ms) -- but DROPS back
    //      to IDLE if it never sees TRANSMITTING. With the old strict-
    //      mode block, PGXL flashes TRANSMIT_A then falls out, which
    //      the operator sees as "amp briefly engages then quits".
    //
    // The fix: treat laggards as PASSIVE SUBSCRIBERS (they still want to
    // see TRANSMITTING for their internal pttA tracker; they just don't
    // veto the TX). Broadcast TRANSMITTING to all enabled interlocks.
    // PGXL stays in TRANSMIT_A. TGXL eventually sees TRANSMITTING and
    // sets pttA=1, which is exactly what its autotune sweep needs.
    //
    // Safety note: PGXL's own SWR / temp / V_dd protection still trips
    // independently of our interlock state. If real SWR is high, PGXL
    // self-faults and stops amplifying -- we're not bypassing safety,
    // just removing a coordination protocol assumption that didn't fit.
    if (!laggards.isEmpty()) {
        qCWarning(lcSmartSdr)
            << "interlock ACK timeout (500 ms):" << readyCount
            << "of" << totalConnectedInterlocks
            << "enabled amps acked; laggards=" << laggards
            << "-- advancing to TRANSMITTING anyway (passive-subscriber"
               " grant; PGXL self-protects if real SWR is high)";
    } else {
        qCInfo(lcSmartSdr)
            << "interlock ACK timeout: all enabled amps acked"
            << "(" << readyCount << "of" << totalConnectedInterlocks
            << ") -- some amps may have disconnected mid-cycle; granting TX";
    }
    // 2026-05-21 4o3a-lan-ptt-pcap-divergence.md §8 C4: timeout path
    // shares the same 30 ms settle as the success path. Set every
    // enabled amp's ackReady so the next call to broadcastTransmitting()
    // has a consistent view. Then capture + clear m_pttPendingSource and
    // schedule the broadcast.
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        if (it->interlockId != 0 && it->interlockEnabled) {
            it->ackReady = true;
        }
    }
    if (m_pttPendingSource.isEmpty()) { return; }
    const QString source = m_pttPendingSource;
    m_pttPendingSource.clear();
    QTimer::singleShot(30, this, [this, source]() {
        broadcastTransmitting(source);
    });
}

void SmartSdrApiListener::onNewConnection()
{
    while (m_server.hasPendingConnections()) {
        QTcpSocket* sock = m_server.nextPendingConnection();
        if (!sock) { continue; }
        connect(sock, &QTcpSocket::readyRead,
                this, &SmartSdrApiListener::onClientDataReady);
        connect(sock, &QTcpSocket::disconnected,
                this, &SmartSdrApiListener::onClientDisconnected);

        const QString host = sock->peerAddress().toString();
        const quint16 port = sock->peerPort();
        ClientState state;
        state.handle = generateHandle();
        m_clients.insert(sock, state);

        qCInfo(lcSmartSdr) << "client connected from" << host << ":" << port
                            << "handle=" << state.handle;

        sendBanner(sock, state.handle);
        // 2026-05-21 bench-fix: sign-correct filter passband per mode (see
        // defaultFilterForMode). LSB requires negative; TGXL drops the socket
        // otherwise.
        const QPair<int, int> initialFilter = defaultFilterForMode(m_sliceMode);
        // 2026-05-22 bench-fix + pcap-confirmed: slice 0's client_handle=
        // is the SLICE OWNER (the GUI client controlling the slice), NOT
        // the per-recipient banner. Canonical FLEX uses the same owner
        // handle on every recipient's slice frame (verified by tshark:
        // flex-tgxl-direct-CONTROL.pcapng T+206.741 shows
        //   S<radio>|slice 0 ... client_handle=0x66B137B7 ...
        // identically broadcast to TGXL (192.168.109.234) AND SmartSDR-Win
        // (192.168.109.19). The handle 0x66B137B7 is SmartSDR-Win's, the
        // slice owner.
        //
        // Amps gate `interlock ready N` ACK on slice.client_handle ==
        // PTT_REQUESTED.tx_client_handle. Before this fix we wrote each
        // amp's own banner into client_handle= which made each amp think
        // it owned slice 0; subsequent PTT_REQUESTED frames carried our
        // synthetic local-client handle which differed, so amps refused
        // to ACK (bench-confirmed 2026-05-22 16:02:39: "0 of 2 enabled
        // amps acked; laggards=TG,PG-XL").
        //
        // Now uses m_localClientHandle (the synthetic local-client handle
        // from C1) so the slice owner matches PTT_REQUESTED's
        // tx_client_handle. Same change applied to broadcastSliceState
        // for the 1 Hz periodic push.
        sendStatus(sock, state.handle,
                   QStringLiteral("slice 0 in_use=1 sample_rate=24000 RF_frequency=%1 "
                                      "client_handle=0x%2 index_letter=A rit_on=0 rit_freq=0 "
                                      "xit_on=0 xit_freq=0 rxant=ANT1 mode=%3 wide=0 "
                                      "filter_lo=%4 filter_hi=%5 step=100 "
                                      "agc_mode=med agc_threshold=60 agc_off_level=10 "
                                      "pan=0x40000000 txant=ANT1 loopa=0 loopb=0 qsk=0 "
                                      "lock=0 tx=1 active=1")
                       .arg(m_sliceFreqHz / 1.0e6, 0, 'f', 6)
                       .arg(m_localClientHandle)
                       .arg(m_sliceMode)
                       .arg(initialFilter.first)
                       .arg(initialFilter.second));
        sendStatus(sock, state.handle,
                   QStringLiteral("transmit freq=%1 rfpower=100 tunepower=10 tx_slice_mode=%2"
                                      " tune=%3 tune_mode=single_tone mon=0 mox=%4"
                                      " hwalc_enabled=0 inhibit=0 dax=0"
                                      " tx_rf_power_changes_allowed=1 max_power_level=100")
                       .arg(m_sliceFreqHz / 1.0e6, 0, 'f', 6)
                       .arg(m_sliceMode)
                       .arg(m_tuneActive ? 1 : 0)
                       .arg(m_txActive ? 1 : 0));

        emit clientConnected(host, port);
    }
}

void SmartSdrApiListener::onClientDataReady()
{
    QTcpSocket* sock = qobject_cast<QTcpSocket*>(sender());
    if (!sock) { return; }
    auto it = m_clients.find(sock);
    if (it == m_clients.end()) { return; }

    const QString peerHost = sock->peerAddress().toString();
    const quint16 peerPort = sock->peerPort();

    QByteArray chunk = sock->readAll();
    if (chunk.isEmpty()) { return; }

    qCInfo(lcSmartSdr) << "RX raw from" << peerHost << ":" << peerPort
                       << "(" << chunk.size() << "bytes):"
                       << chunk.toHex(' ').left(120)
                       << "| ascii:" << QString::fromUtf8(chunk).left(80);

    it->readBuffer.append(chunk);

    // SmartSDR frames are LF-terminated in practice (the captured stream uses
    // \n, not CR). Accept both to be safe.
    QByteArray& buf = it->readBuffer;
    int idx;
    while ((idx = buf.indexOf('\n')) != -1) {
        QByteArray rawLine = buf.left(idx);
        buf.remove(0, idx + 1);
        // Strip trailing CR if present (\r\n terminators).
        if (!rawLine.isEmpty() && rawLine.endsWith('\r')) {
            rawLine.chop(1);
        }
        const QString line = QString::fromUtf8(rawLine).trimmed();
        if (line.isEmpty()) { continue; }

        qCInfo(lcSmartSdr) << "RX from" << peerHost << ":" << peerPort
                            << "line:" << line;
        emit lineReceived(peerHost, peerPort, line);
        dispatchLine(sock, line);
    }
}

void SmartSdrApiListener::onClientDisconnected()
{
    QTcpSocket* sock = qobject_cast<QTcpSocket*>(sender());
    if (!sock) { return; }
    qCInfo(lcSmartSdr) << "client disconnected from"
                        << sock->peerAddress().toString() << ":"
                        << sock->peerPort()
                        << "buffered:" << m_clients.value(sock).readBuffer.size();
    m_clients.remove(sock);
    sock->deleteLater();
}

void SmartSdrApiListener::onPeriodicTick()
{
    if (m_clients.isEmpty()) { return; }
    broadcastSliceState();
}

void SmartSdrApiListener::sendBanner(QTcpSocket* sock, const QString& handle)
{
    if (!sock || !sock->isOpen()) { return; }
    // Version string copied verbatim from FLEX-8600 v4.2.18.41174 stream 2
    // (capture 2026-05-19) so PGXL's "min_software_version" gate (e.g.
    // 2.13.0.0) is comfortably satisfied. FlexAPI parses this as
    // X.Y.Z.NNNNN; 1.4.0.0 was specifically observed.
    const QByteArray banner =
        QStringLiteral("V1.4.0.0\nH%1\n").arg(handle).toUtf8();
    sock->write(banner);
    qCInfo(lcSmartSdr) << "TX banner to"
                       << sock->peerAddress().toString() << ":"
                       << sock->peerPort()
                       << "->" << QString::fromUtf8(banner).trimmed();
}

void SmartSdrApiListener::dispatchLine(QTcpSocket* sock, const QString& line)
{
    // Expected format: C<seq>|<command>
    if (line.isEmpty() || line.at(0) != QLatin1Char('C')) {
        // Unknown frame type; ignore so we don't break a chatty client.
        return;
    }
    const int barIdx = line.indexOf(QLatin1Char('|'));
    if (barIdx < 2) {
        return;
    }
    bool seqOk = false;
    const quint32 seq = line.mid(1, barIdx - 1).toUInt(&seqOk);
    if (!seqOk) { return; }
    const QString cmd = line.mid(barIdx + 1);

    // Minimum viable responder: ACK every command. PGXL is permissive about
    // empty response bodies for commands it doesn't strictly require data
    // for; for commands where it does need data we extend the body string
    // here as new bench data arrives.
    QString body;
    if (cmd.startsWith(QStringLiteral("info"))) {
        // PGXL uses `info` to fetch radio identity. Pad enough fields that
        // PGXL's parser sees a complete payload.
        body = QStringLiteral(
            "model=NereusSDR callsign=NEREUS nickname=NereusSDR "
            "name=NereusSDR options= "
            "atu_present=0 gps=Not Present "
            "ip=0.0.0.0 mac=00:00:00:00:00:00 ");
    } else if (cmd.startsWith(QStringLiteral("version"))) {
        body = QStringLiteral("1.4.0.0");
    } else if (cmd.startsWith(QStringLiteral("ant list"))) {
        body = QStringLiteral("ANT1,ANT2,RX_A,XVTR");
    } else if (cmd.startsWith(QStringLiteral("client ip"))) {
        body = sock->peerAddress().toString();
    } else if (cmd.startsWith(QStringLiteral("slice list"))) {
        body = QStringLiteral("0");
    } else if (cmd.startsWith(QStringLiteral("amplifier create"))) {
        // FLEX-canonical response: `R<seq>|0` with EMPTY body. Per
        // https://github.com/flexradio/smartsdr-api-docs/wiki/TCPIP-amplifier
        // the amplifier create response is status-code only. The amp
        // handle is the client's own connection handle (the one we
        // assigned in the H<handle> banner at TCP accept) -- it is NOT a
        // separate handle returned in the R-frame body. Earlier code
        // generated a random 8-hex value and returned it as `R<seq>|0|0xXXXX`
        // which TGXL parsed as an error code, breaking the amplifier
        // registration and causing the regression where TGXL hardware
        // TUNE reported "no PTT in" / "pse unkey".
        //
        // We still parse + cache the model / serial_num / ip / ant fields
        // from the create command so we can echo them in subsequent
        // S<client_handle>|amplifier ... broadcasts using the CLIENT's
        // banner handle (which is also the amp handle per the FlexLib
        // Amplifier(Radio,string) constructor convention).
        auto it = m_clients.find(sock);
        if (it != m_clients.end() && it->ampHandle.isEmpty()) {
            // Amp handle == client banner handle (FlexLib convention).
            it->ampHandle = it->handle;
            auto extractValue = [&cmd](const QString& key) -> QString {
                const int idx = cmd.indexOf(key);
                if (idx < 0) { return QString(); }
                const int valStart = idx + key.size();
                const int end = cmd.indexOf(QLatin1Char(' '), valStart);
                return (end >= 0)
                    ? cmd.mid(valStart, end - valStart)
                    : cmd.mid(valStart);
            };
            it->ampModel  = extractValue(QStringLiteral("model="));
            it->ampSerial = extractValue(QStringLiteral("serial_num="));
            it->ampIp     = extractValue(QStringLiteral("ip="));
            it->ampAnt    = extractValue(QStringLiteral("ant="));
            qCInfo(lcSmartSdr) << "amplifier create -> using client handle"
                               << it->ampHandle << "as amp handle, model="
                               << it->ampModel << "serial=" << it->ampSerial;
        }
        // body stays empty; canonical R-frame response is just status code.
    } else if (cmd.startsWith(QStringLiteral("amplifier set "))) {
        // 2026-05-20 pcap-driven (flex-tgxl-direct-CONTROL.pcapng @T+172.201):
        // Wire format: `amplifier set 0x<handle> <key>=<value>`
        // Real FlexRadio recognized handles e.g.
        //   amplifier set 0x22E8213A operate=0   (handle=PGXL, key=operate, value=0)
        //
        // Used by TGXL to control PGXL state during its own autotune cycle
        // (standby PGXL before tune, restore after). FlexRadio acts as a
        // passive proxy: looks up which amp owns the handle and forwards
        // the command via that amp's native protocol (PGXL:9008 / TGXL:9010).
        //
        // We just parse + emit; RadioModel does the actual proxy because it
        // owns the PgxlConnection / TgxlConnection objects.
        const QString rest = cmd.mid(QStringLiteral("amplifier set ").size())
                                .trimmed();
        const int firstSpace = rest.indexOf(QLatin1Char(' '));
        if (firstSpace > 0) {
            QString handleTok = rest.left(firstSpace);
            const QString kvTok = rest.mid(firstSpace + 1).trimmed();
            // Strip "0x" prefix (case-insensitive) to match the stored
            // ampHandle format (which is hex without the prefix).
            if (handleTok.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
                handleTok = handleTok.mid(2);
            }
            const int eq = kvTok.indexOf(QLatin1Char('='));
            if (eq > 0) {
                const QString key = kvTok.left(eq);
                const QString val = kvTok.mid(eq + 1);
                qCInfo(lcSmartSdr)
                    << "amplifier set received -> handle=0x" << handleTok
                    << "key=" << key << "value=" << val
                    << "from" << sock->peerAddress().toString()
                    << "(forwarding via amplifierSetRequested)";
                emit amplifierSetRequested(handleTok, key, val);
            } else {
                qCWarning(lcSmartSdr)
                    << "amplifier set: malformed key=value tail:" << kvTok;
            }
        } else {
            qCWarning(lcSmartSdr)
                << "amplifier set: malformed command:" << cmd;
        }
        // body stays empty per the FLEX response convention; ACK is the
        // R<seq>|0| status code from sendResponse below.
    } else if (cmd.startsWith(QStringLiteral("interlock create"))) {
        // Per smartsdr-api-docs TCPIP-interlock wiki:
        //   (1) Device: C2|interlock create type=AMP name=KZX-2500 serial=... valid_antennas=...
        //   (2) Radio:  R2|0|1                  <- response body is the assigned interlock ID
        //   (3) Radio:  S0|interlock state=PTT_REQUESTED reason=AMP:KZX-2500 source=MIC tx_allowed=1
        //   (4) Device: C2|interlock ready 1    <- amp ACKs the request using its id
        //   (5) Radio:  R2|0|
        //   (6) Radio:  S0|interlock state=TRANSMITTING source=MIC tx_allowed=1
        //
        // The amp keeps the interlock id locally and references it in step 4
        // to ACK ANY future PTT_REQUESTED. Empty R-frame body (our prior
        // behavior) means the amp never knows its id and can never ACK,
        // which blocks the entire interlock state machine.
        //
        // Source: https://github.com/flexradio/smartsdr-api-docs/wiki/TCPIP-interlock
        auto it = m_clients.find(sock);
        if (it != m_clients.end() && it->interlockId == 0) {
            it->interlockId = m_nextInterlockId++;
            // Default to enabled on create -- the wiki interlock state
            // machine starts in the enabled state; explicit
            // `interlock disable` later steps the amp out.
            it->interlockEnabled = true;
            // Parse name= and serial= for `reason=AMP:<name>` field selection
            // in the subsequent PTT_REQUESTED broadcasts.
            auto extractValue = [&cmd](const QString& key) -> QString {
                const int idx = cmd.indexOf(key);
                if (idx < 0) { return QString(); }
                const int valStart = idx + key.size();
                const int end = cmd.indexOf(QLatin1Char(' '), valStart);
                return (end >= 0)
                    ? cmd.mid(valStart, end - valStart)
                    : cmd.mid(valStart);
            };
            const QString name = extractValue(QStringLiteral("name="));
            if (!name.isEmpty()) { it->interlockName = name; }
            qCInfo(lcSmartSdr) << "interlock create -> id=" << it->interlockId
                               << "name=" << it->interlockName
                               << "for client" << sock->peerAddress().toString();
        }
        // Wiki returns the id as ASCII decimal in the R-frame body.
        body = QString::number(it->interlockId);
    } else if (cmd.startsWith(QStringLiteral("interlock ready"))) {
        // Amp ACK of our PTT_REQUESTED broadcast. Body is the interlock id
        // the amp got at step (2). We mark the amp as ready, then if all
        // pending amps are ready, we advance to TRANSMITTING (step 6).
        auto it = m_clients.find(sock);
        if (it != m_clients.end()) {
            // Parse the id from the command tail; tolerate "ready <id>" or
            // "ready=<id>".
            const QString tail = cmd.mid(QStringLiteral("interlock ready").size())
                                    .trimmed();
            bool ok = false;
            const int ackId = tail.startsWith(QLatin1Char('='))
                ? tail.mid(1).toInt(&ok)
                : tail.toInt(&ok);
            if (ok && ackId == it->interlockId) {
                it->ackReady = true;
                it->ackReceivedMs = QDateTime::currentMSecsSinceEpoch();
                qCInfo(lcSmartSdr) << "interlock ready ACK from"
                                   << sock->peerAddress().toString()
                                   << "id=" << ackId
                                   << "name=" << it->interlockName;
                // Body stays empty per wiki: R<seq>|0|
                // Defer the TRANSMITTING advance until AFTER we send the
                // R-frame back so the amp's seq counter is settled.
                QTimer::singleShot(0, this,
                                   &SmartSdrApiListener::advanceToTransmittingIfReady);
            } else {
                qCWarning(lcSmartSdr) << "interlock ready: mismatched id"
                                      << "(amp sent" << ackId << "expected"
                                      << it->interlockId << ")";
            }
        }
    } else if (cmd.startsWith(QStringLiteral("interlock enable"))
               || cmd.startsWith(QStringLiteral("interlock disable"))) {
        // C|interlock enable <id>   - amp re-joining the interlock chain
        // C|interlock disable <id>  - amp stepping out (e.g. on STANDBY)
        //
        // Bench-confirmed PGXL behavior 2026-05-20:
        //   t=0:    we send `operate=0` to PGXL
        //   t+~100ms: PGXL sends `interlock disable <id>` (stepping out)
        //   t+~200ms: PGXL state edges OPERATE -> STANDBY
        //   ...
        //   when PGXL transitions back to IDLE/OPERATE:
        //   t+0: PGXL sends `interlock enable <id>` (rejoining chain)
        //
        // A disabled interlock means the amp is NOT in the TX path and
        // CANNOT ACK PTT_REQUESTED. Tracking this correctly prevents the
        // amp from being mis-flagged as a 500 ms laggard during a normal
        // autotune-induced STANDBY cycle.
        const bool enabling = cmd.startsWith(QStringLiteral("interlock enable"));
        auto it = m_clients.find(sock);
        if (it != m_clients.end()) {
            // Parse trailing id (tolerates "<verb> <id>" or "<verb>=<id>"
            // formats; spec uses space-separated).
            const QString prefix = enabling
                ? QStringLiteral("interlock enable")
                : QStringLiteral("interlock disable");
            const QString tail = cmd.mid(prefix.size()).trimmed();
            bool ok = false;
            const int wireId = tail.startsWith(QLatin1Char('='))
                ? tail.mid(1).toInt(&ok)
                : tail.toInt(&ok);
            // Only honour the toggle if the id matches this client's
            // registered interlockId. Some amps may send the verb without
            // an id (or with id=0 meaning "self"); accept those too.
            if (!ok || wireId == 0 || wireId == it->interlockId) {
                it->interlockEnabled = enabling;
                qCInfo(lcSmartSdr) << "interlock" << (enabling ? "enable" : "disable")
                                   << "from" << sock->peerAddress().toString()
                                   << "id=" << it->interlockId
                                   << "name=" << it->interlockName;
                // If an in-progress PTT_REQUESTED wait was blocked on this
                // amp ACKing and the amp just disabled, re-check whether
                // we can advance. Otherwise the wait stalls until the
                // 500 ms wiki timeout for an amp that's deliberately
                // not participating in this cycle.
                if (!enabling && !m_pttPendingSource.isEmpty()) {
                    QTimer::singleShot(0, this,
                        &SmartSdrApiListener::advanceToTransmittingIfReady);
                }
            }
        }
        // No body needed.
    }

    // LAN PTT: TGXL (and other SmartSDR-API clients) request the FlexRadio to
    // emit a CW tune carrier by sending `transmit tune on` / `transmit tune off`
    // on this control channel. We must ACK with hex=0 AND fire a signal so
    // RadioModel can engage / drop the local TUN. Order: ACK first (so the
    // client's seq counter advances), then emit. Bench-confirmed wire format:
    //   C7|transmit tune on    -> emit tuneRequested(true)
    //   C7|transmit tune off   -> emit tuneRequested(false)
    // Captured 2026-05-19 22:29:59 / 22:30:00 from TGXL .234 after the
    // operator pressed the device's hardware TUNE button.
    bool emitTuneOn  = false;
    bool emitTuneOff = false;
    bool emitMoxOn   = false;
    bool emitMoxOff  = false;
    if (cmd == QStringLiteral("transmit tune on")) {
        emitTuneOn = true;
    } else if (cmd == QStringLiteral("transmit tune off")) {
        emitTuneOff = true;
    } else if (cmd == QStringLiteral("transmit mox on")) {
        emitMoxOn = true;
    } else if (cmd == QStringLiteral("transmit mox off")) {
        emitMoxOff = true;
    }

    sendResponse(sock, seq, /*err=*/0, body);

    if (emitTuneOn) {
        qCInfo(lcSmartSdr) << "LAN PTT tune on from"
                           << sock->peerAddress().toString();
        // 2026-05-21 4o3a-lan-ptt-pcap-divergence.md §8 C2: record
        // which amp sent the tune-on so PTT_REQUESTED can carry
        // reason=AMP:<name>. Falls back to that amp's interlockName
        // (set when it sent `interlock create`) so the wire matches
        // pcap T+167.678 where reason=AMP:TG names the TGXL initiator.
        auto initIt = m_clients.find(sock);
        if (initIt != m_clients.end() && !initIt->interlockName.isEmpty()) {
            m_lastTuneInitiator = initIt->interlockName;
        }
        emit tuneRequested(true);
    } else if (emitTuneOff) {
        qCInfo(lcSmartSdr) << "LAN PTT tune off from"
                           << sock->peerAddress().toString();
        // 2026-05-21 4o3a-lan-ptt-pcap-divergence.md §8 C2: do NOT clear
        // m_lastTuneInitiator before emitting tuneRequested(false). The
        // emit chains synchronously into setInterlockTransmitting(false,
        // "TUNE"), which reads m_lastTuneInitiator to pick the canonical
        // reason=AMP:<initiator> for the UNKEY_REQUESTED frame. Clearing
        // first would cause the un-key wire to carry reason=AMP:PG-XL
        // (the MIC-source fallback) instead of the canonical
        // reason=AMP:TG. Clear AFTER the emit returns so the next TUNE
        // cycle starts with a fresh initiator slot.
        emit tuneRequested(false);
        m_lastTuneInitiator.clear();
    } else if (emitMoxOn) {
        qCInfo(lcSmartSdr) << "LAN PTT mox on from"
                           << sock->peerAddress().toString();
        emit moxRequested(true);
    } else if (emitMoxOff) {
        qCInfo(lcSmartSdr) << "LAN PTT mox off from"
                           << sock->peerAddress().toString();
        emit moxRequested(false);
    }

    // After a successful sub of slice / transmit, immediately push a fresh
    // S-frame so PGXL gets data without waiting for the next 1 Hz tick.
    if (cmd.startsWith(QStringLiteral("sub slice"))
        || cmd.startsWith(QStringLiteral("sub transmit"))
        || cmd.startsWith(QStringLiteral("sub radio"))) {
        const QString handle = m_clients.value(sock).handle;
        // PR #279 review #1 (2026-05-23): the immediate push must use
        // m_localClientHandle for the slice's client_handle= field, the
        // same way onNewConnection() (line ~616) and broadcastSliceState()
        // (line ~1102) do.  Earlier this path used the per-recipient
        // banner handle, so an amp that subscribed AFTER initial connect
        // would see slice.client_handle != PTT_REQUESTED.tx_client_handle
        // and refuse the `interlock ready N` ACK -- same bench failure
        // mode 2026-05-22 16:02:39 fixed for the other two paths.
        sendStatus(sock, handle,
                   QStringLiteral("slice 0 in_use=1 sample_rate=24000 RF_frequency=%1 "
                                      "client_handle=0x%2 index_letter=A rit_on=0 rit_freq=0 "
                                      "xit_on=0 xit_freq=0 rxant=ANT1 mode=%3 wide=0 "
                                      "filter_lo=100 filter_hi=2800 step=100 "
                                      "agc_mode=med agc_threshold=60 agc_off_level=10 "
                                      "pan=0x40000000 txant=ANT1 loopa=0 loopb=0 qsk=0 "
                                      "lock=0 tx=1 active=1")
                       .arg(m_sliceFreqHz / 1.0e6, 0, 'f', 6)
                       .arg(m_localClientHandle)
                       .arg(m_sliceMode));
        sendStatus(sock, handle,
                   QStringLiteral("transmit freq=%1 rfpower=100 tunepower=10 tx_slice_mode=%2"
                                      " tune=%3 tune_mode=single_tone mon=0 mox=%4"
                                      " hwalc_enabled=0 inhibit=0 dax=0"
                                      " tx_rf_power_changes_allowed=1 max_power_level=100")
                       .arg(m_sliceFreqHz / 1.0e6, 0, 'f', 6)
                       .arg(m_sliceMode)
                       .arg(m_tuneActive ? 1 : 0)
                       .arg(m_txActive ? 1 : 0));
    }
}

void SmartSdrApiListener::sendResponse(QTcpSocket* sock, quint32 seq,
                                       int err, const QString& body)
{
    if (!sock || !sock->isOpen()) { return; }
    // FlexAPI uses LF terminators on the captured stream.
    const QByteArray frame =
        QStringLiteral("R%1|%2|%3\n").arg(seq).arg(err).arg(body).toUtf8();
    sock->write(frame);
    qCInfo(lcSmartSdr) << "TX R-frame seq=" << seq << "err=" << err
                       << "body=" << body;
}

void SmartSdrApiListener::sendStatus(QTcpSocket* sock, const QString& handle,
                                     const QString& body)
{
    if (!sock || !sock->isOpen()) { return; }
    const QByteArray frame =
        QStringLiteral("S%1|%2\n").arg(handle).arg(body).toUtf8();
    sock->write(frame);
    qCInfo(lcSmartSdr) << "TX S-frame to" << sock->peerAddress().toString()
                       << "handle=" << handle << "body=" << body;
}

void SmartSdrApiListener::broadcastSliceState()
{
    if (m_clients.isEmpty()) { return; }
    // Build a fresh FLEX-canonical slice 0 S-frame per client because each
    // client has a unique handle that must be reflected in client_handle=
    // (FLEX format observed in capture stream 2). Without index_letter=A
    // and the client_handle on the slice, PGXL's bandA tracker won't pick
    // up the RF_frequency for band lookup.
    const QString txBody =
        QStringLiteral("transmit freq=%1 rfpower=100 tunepower=10 tx_slice_mode=%2"
                       " tune=%3 tune_mode=single_tone mon=0 mox=%4"
                       " hwalc_enabled=0 inhibit=0 dax=0"
                       " tx_rf_power_changes_allowed=1 max_power_level=100")
            .arg(m_sliceFreqHz / 1.0e6, 0, 'f', 6)
            .arg(m_sliceMode)
            .arg(m_tuneActive ? 1 : 0)
            .arg(m_txActive ? 1 : 0);
    // 2026-05-21 bench-fix: sign-correct filter passband per mode so TGXL
    // does not drop the SmartSDR API socket on LSB. See defaultFilterForMode.
    // 2026-05-22 bench-fix: client_handle= in the slice body is the slice
    // OWNER (m_localClientHandle), not the per-recipient banner. Required
    // for amps to ACK PTT_REQUESTED; see onNewConnection() block above.
    const QPair<int, int> filterRange = defaultFilterForMode(m_sliceMode);
    for (auto it = m_clients.cbegin(); it != m_clients.cend(); ++it) {
        const QString sliceBody =
            QStringLiteral("slice 0 in_use=1 sample_rate=24000 RF_frequency=%1 "
                           "client_handle=0x%2 index_letter=A rit_on=0 rit_freq=0 "
                           "xit_on=0 xit_freq=0 rxant=ANT1 mode=%3 wide=0 "
                           "filter_lo=%4 filter_hi=%5 step=100 "
                           "agc_mode=med agc_threshold=60 agc_off_level=10 "
                           "pan=0x40000000 txant=ANT1 loopa=0 loopb=0 qsk=0 "
                           "lock=0 tx=1 active=1")
                .arg(m_sliceFreqHz / 1.0e6, 0, 'f', 6)
                .arg(m_localClientHandle)
                .arg(m_sliceMode)
                .arg(filterRange.first)
                .arg(filterRange.second);
        // S-frame prefix: amp handle for amp clients (PGXL/TGXL), else the
        // banner-issued client handle. FLEX prefixes per-recipient S-frames
        // with the recipient's own handle so the client can filter at the
        // protocol layer -- TGXL sees `S<TGXL_amp_handle>|...` and matches
        // its own handle, then processes. With the client handle prefix
        // (which we generate at banner time, top nibble 4) TGXL didn't
        // match either of its handles and dropped the frame, leaving
        // freqA/modeA/bandA/antA at 0 even though the body was correct.
        // Bench-confirmed 23:51 on 2026-05-19.
        const QString prefix = it.value().ampHandle.isEmpty()
            ? it.value().handle
            : it.value().ampHandle;
        sendStatus(it.key(), prefix, sliceBody);
        sendStatus(it.key(), prefix, txBody);
    }
}

QString SmartSdrApiListener::generateHandle() const
{
    // 8-hex handle. Top nibble is FlexAPI's "client type" (4 = full GUI in
    // the captured stream); rest is randomized so each connection is
    // distinguishable in our own logs and PGXL's logs.
    const quint32 r = QRandomGenerator::global()->generate() & 0x0FFFFFFF;
    return QStringLiteral("4%1")
        .arg(r, 7, 16, QLatin1Char('0'))
        .toUpper();
}

QPair<int, int> SmartSdrApiListener::defaultFilterForMode(const QString& mode) const
{
    // 2026-05-21 bench-confirmed: canonical FLEX uses NEGATIVE filter
    // passband sign for lower-sideband modes (LSB, DIGL, FDVL) and
    // POSITIVE for everything else. Verified against
    // captures/flex-tgxl-direct-CONTROL.pcapng @ T+206.741:
    //   mode=LSB wide=0 filter_lo=-2800 filter_hi=-100
    // Without this TGXL drops the SmartSDR API socket after parsing the
    // initial slice S-frame (positive sign for LSB fails mode-vs-filter
    // validation). Conservative scope: pcap-verified for LSB only; all
    // other modes keep the prior USB-positive default. Widths come from
    // RX/TX channel pipelines elsewhere; this helper only fixes the sign.
    if (mode == QStringLiteral("LSB")
        || mode == QStringLiteral("DIGL")
        || mode == QStringLiteral("FDVL")) {
        return {-2800, -100};
    }
    return {100, 2800};
}

QString SmartSdrApiListener::initiatingAmpName(const QString& wireSource) const
{
    // 2026-05-21 4o3a-lan-ptt-pcap-divergence.md §8 C2:
    // - source=TUNE: name of the amp that sent `transmit tune on`.
    //   Falls back to first TunerGeniusXL-class amp when the operator
    //   pressed TUNE locally (NereusSDR UI button) and no remote amp
    //   recorded itself as initiator. Bench-confirmed 2026-05-21: without
    //   this fallback local TUNE emits reason= empty on PTT_REQUESTED
    //   instead of canonical reason=AMP:TG, matching pcap T+167.678.
    // - source=MIC : first amp registered with model=PowerGeniusXL (the
    //   power amp in the chain). Falls back to empty if no PGXL-class amp
    //   is connected, which is acceptable per the design doc Definitions.
    if (wireSource == QStringLiteral("TUNE")) {
        if (!m_lastTuneInitiator.isEmpty()) {
            return m_lastTuneInitiator;
        }
        // Local-TUNE fallback: first TunerGeniusXL-class amp, symmetric
        // to the MIC branch picking first PowerGeniusXL.
        for (auto it = m_clients.cbegin(); it != m_clients.cend(); ++it) {
            if (it->ampModel == QStringLiteral("TunerGeniusXL")
                && !it->interlockName.isEmpty()) {
                return it->interlockName;
            }
        }
        return QString();
    }
    if (wireSource == QStringLiteral("MIC")) {
        for (auto it = m_clients.cbegin(); it != m_clients.cend(); ++it) {
            if (it->ampModel == QStringLiteral("PowerGeniusXL")
                && !it->interlockName.isEmpty()) {
                return it->interlockName;
            }
        }
    }
    return QString();
}

}  // namespace NereusSDR
