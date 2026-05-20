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
    if (m_server.isListening()) {
        m_server.close();
    }
    // AnyIPv4 (not Any) because Qt's default Any binds IPv6-only on macOS,
    // which silently blocks IPv4 clients like Windows PowerGeniusDesktop.
    bool ok = m_server.listen(QHostAddress::AnyIPv4, 4992);
    if (!ok) {
        qCWarning(lcSmartSdr) << "failed to bind TCP 4992:"
                               << m_server.errorString();
        return false;
    }
    m_periodicTimer.start();
    qCInfo(lcSmartSdr) << "SmartSDR API listener listening on"
                       << m_server.serverAddress().toString()
                       << ":" << m_server.serverPort();
    return true;
}

void SmartSdrApiListener::stop()
{
    m_periodicTimer.stop();
    m_server.close();
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

void SmartSdrApiListener::setInterlockTransmitting(bool transmitting,
                                                   const QString& source)
{
    if (m_clients.isEmpty()) { return; }

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
        // Reset ACK state for every registered interlock amp.
        bool anyInterlockedAmp = false;
        for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
            if (it->interlockId != 0) {
                it->ackReady = false;
                anyInterlockedAmp = true;
            }
        }

        // Emit one PTT_REQUESTED S-frame per registered amp, matching the
        // FLEX pcap format (not the wiki example, which differs).
        // Pcap reality from FLEX-8600 stream 2 at T=541.716:
        //   S0|interlock tx_client_handle=0x4C70DC9B state=PTT_REQUESTED
        //               reason= source=TUNE tx_allowed=1 amplifier=0x7CC669A7
        // Note: reason= is EMPTY (not AMP:<name>) and tx_client_handle is
        // NON-ZERO (the SmartSDR client who initiated TX). We pick the amp's
        // own banner handle as tx_client_handle so the frame self-references
        // -- semantically "this amp's chain is being keyed". Real FLEX uses
        // SmartSDR-Win's handle here; we don't have a separate GUI client.
        if (anyInterlockedAmp) {
            for (auto it = m_clients.cbegin(); it != m_clients.cend(); ++it) {
                if (it->interlockId == 0) { continue; }
                const QString body =
                    QStringLiteral("interlock tx_client_handle=0x%1"
                                   " state=PTT_REQUESTED reason="
                                   " source=%2 tx_allowed=1 amplifier=0x%3")
                        .arg(it->handle)        // non-zero per pcap
                        .arg(wireSource)
                        .arg(it->ampHandle);
                const QByteArray frame =
                    QStringLiteral("S0|%1\n").arg(body).toUtf8();
                // Broadcast to all connections (FLEX broadcasts S0 globally).
                for (auto jt = m_clients.cbegin(); jt != m_clients.cend(); ++jt) {
                    QTcpSocket* sock = jt.key();
                    if (sock && sock->isOpen()) { sock->write(frame); }
                }
                qCInfo(lcSmartSdr) << "TX S0|interlock state=PTT_REQUESTED"
                                   << "tx_client_handle=0x" << it->handle
                                   << "source=" << wireSource
                                   << "amplifier=0x" << it->ampHandle
                                   << "(waiting for interlock ready ack)";
            }
            // Arm the 500 ms wiki-spec timeout. If the ACKs don't all
            // arrive in time, advanceToTransmittingIfReady() is called
            // anyway so we still send TRANSMITTING (degraded path) and
            // RF flows. Better UX than aborting TX silently.
            if (!m_pttAckTimeout.isActive()) {
                m_pttAckTimeout.setSingleShot(true);
                m_pttAckTimeout.setInterval(500);
                disconnect(&m_pttAckTimeout, &QTimer::timeout, nullptr, nullptr);
                connect(&m_pttAckTimeout, &QTimer::timeout,
                        this, &SmartSdrApiListener::onPttAckTimeout);
            }
            m_pttAckTimeout.start();
            // Also check immediately in case there were no amps to wait
            // for (i.e. interlockId==0 for every client). The check is
            // cheap and handles the edge case.
            QTimer::singleShot(0, this,
                               &SmartSdrApiListener::advanceToTransmittingIfReady);
        } else {
            // No registered amp interlocks: emit TRANSMITTING directly
            // with a single S0|interlock so any SmartSDR-API client (e.g.
            // a status-display UI) sees the radio is keyed. amplifier=
            // stays empty per wiki when no amp is in the chain.
            const QString body =
                QStringLiteral("interlock tx_client_handle=0x00000000"
                               " state=TRANSMITTING reason="
                               " source=%1 tx_allowed=1 amplifier=")
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
        }
    } else {
        // ── Release path: stop the ACK wait, then broadcast READY.
        // Per wiki: when TX drops, the radio emits S0|interlock
        // state=READY reason=AMP:<name> tx_allowed=1 once per amp.
        // No more amplifier= field on the READY (pcap T=543.292 shows
        // amplifier= empty on the unkey transition).
        m_pttAckTimeout.stop();
        m_pttPendingSource.clear();

        bool anyInterlockedAmp = false;
        for (auto it = m_clients.cbegin(); it != m_clients.cend(); ++it) {
            if (it->interlockId == 0) { continue; }
            anyInterlockedAmp = true;
            // Pcap T=543.292: S0|interlock tx_client_handle=0x4C70DC9B
            //   state=UNKEY_REQUESTED reason=AMP:TG source= tx_allowed=1
            //   amplifier=
            // Then T=543.292 also: state=READY reason= source= ... amplifier=
            // So READY has empty reason in real FLEX too. We keep reason=AMP:
            // for one-amp consumers (wiki spec) but drop it for strict pcap
            // alignment if needed -- leave AMP:<name> for now.
            const QString body =
                QStringLiteral("interlock tx_client_handle=0x%1"
                               " state=READY reason=AMP:%2"
                               " source= tx_allowed=1 amplifier=")
                    .arg(it->handle).arg(it->interlockName);
            const QByteArray frame =
                QStringLiteral("S0|%1\n").arg(body).toUtf8();
            for (auto jt = m_clients.cbegin(); jt != m_clients.cend(); ++jt) {
                QTcpSocket* sock = jt.key();
                if (sock && sock->isOpen()) { sock->write(frame); }
            }
            qCInfo(lcSmartSdr) << "TX S0|interlock state=READY"
                               << "reason=AMP:" << it->interlockName;
        }
        if (!anyInterlockedAmp) {
            const QString body =
                QStringLiteral("interlock tx_client_handle=0x00000000"
                               " state=READY reason= source= tx_allowed=1"
                               " amplifier=");
            const QByteArray frame =
                QStringLiteral("S0|%1\n").arg(body).toUtf8();
            for (auto jt = m_clients.cbegin(); jt != m_clients.cend(); ++jt) {
                QTcpSocket* sock = jt.key();
                if (sock && sock->isOpen()) { sock->write(frame); }
            }
            qCInfo(lcSmartSdr) << "TX S0|interlock state=READY (no amps)";
        }
    }
}

void SmartSdrApiListener::advanceToTransmittingIfReady()
{
    // Called after every `interlock ready` ACK arrives (and once after
    // PTT_REQUESTED is broadcast, as a no-amps shortcut). Checks if all
    // registered amps have ackReady=true; if yes, broadcasts TRANSMITTING.
    if (m_pttPendingSource.isEmpty()) { return; }  // not currently engaging

    int totalInterlocks = 0;
    int readyCount = 0;
    for (auto it = m_clients.cbegin(); it != m_clients.cend(); ++it) {
        if (it->interlockId == 0) { continue; }
        ++totalInterlocks;
        if (it->ackReady) { ++readyCount; }
    }
    if (totalInterlocks == 0) { return; }  // no amps to wait for
    if (readyCount < totalInterlocks) { return; }  // still waiting

    // All registered amps have ACKed. Stop the timeout, advance to
    // TRANSMITTING, then clear m_pttPendingSource so a stale ACK doesn't
    // re-fire this path.
    m_pttAckTimeout.stop();
    const QString source = m_pttPendingSource;
    m_pttPendingSource.clear();

    // One TRANSMITTING S-frame per amp, matching the pcap PTT_REQUESTED
    // format exactly (non-zero tx_client_handle, empty reason).
    for (auto it = m_clients.cbegin(); it != m_clients.cend(); ++it) {
        if (it->interlockId == 0) { continue; }
        const QString body =
            QStringLiteral("interlock tx_client_handle=0x%1"
                           " state=TRANSMITTING reason= source=%2"
                           " tx_allowed=1 amplifier=0x%3")
                .arg(it->handle).arg(source).arg(it->ampHandle);
        const QByteArray frame =
            QStringLiteral("S0|%1\n").arg(body).toUtf8();
        for (auto jt = m_clients.cbegin(); jt != m_clients.cend(); ++jt) {
            QTcpSocket* sock = jt.key();
            if (sock && sock->isOpen()) { sock->write(frame); }
        }
        qCInfo(lcSmartSdr) << "TX S0|interlock state=TRANSMITTING"
                           << "source=" << source
                           << "amplifier=0x" << it->ampHandle
                           << "(all" << totalInterlocks << "amps acked)";
    }

    // Signal RadioModel that the handshake completed successfully so
    // any TX-gating logic on the RadioModel side (e.g. logging,
    // metering) knows the amps are now in their TRANSMITTING state.
    emit interlockGranted(source);
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
        if (it->interlockId == 0) { continue; }
        ++totalConnectedInterlocks;
        if (it->ackReady) {
            ++readyCount;
        } else {
            laggards << it->interlockName;
        }
    }

    // Lenient grant: if every currently-connected interlocked amp has ACKed
    // (or there are no interlocked amps left at all, because they all
    // disconnected mid-cycle), grant the TX. The amps that disappeared
    // aren't blocking us; only an amp that's still connected and refusing
    // counts as blocking per the wiki "preventing transmit" semantic.
    if (laggards.isEmpty()) {
        qCInfo(lcSmartSdr)
            << "interlock ACK timeout: all connected amps acked"
            << "(" << readyCount << "of" << totalConnectedInterlocks << ")"
            << "-- some amps may have disconnected mid-cycle; granting TX";
        // Mark stale entries as ready so advanceToTransmittingIfReady's
        // own check passes, then advance.
        for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
            if (it->interlockId != 0) { it->ackReady = true; }
        }
        advanceToTransmittingIfReady();
        return;
    }

    m_pttPendingSource.clear();
    const QString blockedReason =
        QStringLiteral("AMP:%1").arg(laggards.join(QStringLiteral(",")));
    qCWarning(lcSmartSdr) << "interlock ACK timeout (500 ms):"
                          << readyCount << "of" << totalConnectedInterlocks
                          << "amps acked; laggards=" << laggards
                          << "-- emitting interlock-blocked, NOT advancing to TX";

    // Per wiki: emit READY with reason=AMP:<laggard_name> so the laggard
    // amp learns it blocked the TX (some amp UIs surface this).
    for (auto it = m_clients.cbegin(); it != m_clients.cend(); ++it) {
        if (it->interlockId == 0) { continue; }
        if (it->ackReady) { continue; }  // only emit for laggards
        const QString body =
            QStringLiteral("interlock tx_client_handle=0x%1"
                           " state=READY reason=AMP:%2"
                           " source= tx_allowed=0 amplifier=")
                .arg(it->handle).arg(it->interlockName);
        const QByteArray frame =
            QStringLiteral("S0|%1\n").arg(body).toUtf8();
        for (auto jt = m_clients.cbegin(); jt != m_clients.cend(); ++jt) {
            QTcpSocket* sock = jt.key();
            if (sock && sock->isOpen()) { sock->write(frame); }
        }
    }

    // Signal RadioModel to roll back the local MOX -- the operator's
    // TX request was denied by an amp interlock. RadioModel surfaces
    // this as a toast and drops MoxController::setMox(false).
    emit interlockBlocked(blockedReason);
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
        // Push initial state so PGXL has band/mode data before any C-frames.
        sendStatus(sock, state.handle,
                   QStringLiteral("slice 0 in_use=1 sample_rate=24000 RF_frequency=%1 "
                                      "client_handle=0x%2 index_letter=A rit_on=0 rit_freq=0 "
                                      "xit_on=0 xit_freq=0 rxant=ANT1 mode=%3 wide=0 "
                                      "filter_lo=100 filter_hi=2800 step=100 "
                                      "agc_mode=med agc_threshold=60 agc_off_level=10 "
                                      "pan=0x40000000 txant=ANT1 loopa=0 loopb=0 qsk=0 "
                                      "lock=0 tx=1 active=1")
                       .arg(m_sliceFreqHz / 1.0e6, 0, 'f', 6)
                       .arg(state.handle)
                       .arg(m_sliceMode));
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
    } else if (cmd.startsWith(QStringLiteral("interlock enable"))) {
        // C|interlock enable 0|1  - amp toggling its interlock enable flag.
        // We acknowledge but don't track this internally yet. PGXL sends
        // `interlock enable 0` at boot (bench-confirmed) before the create.
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
        emit tuneRequested(true);
    } else if (emitTuneOff) {
        qCInfo(lcSmartSdr) << "LAN PTT tune off from"
                           << sock->peerAddress().toString();
        emit tuneRequested(false);
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
        sendStatus(sock, handle,
                   QStringLiteral("slice 0 in_use=1 sample_rate=24000 RF_frequency=%1 "
                                      "client_handle=0x%2 index_letter=A rit_on=0 rit_freq=0 "
                                      "xit_on=0 xit_freq=0 rxant=ANT1 mode=%3 wide=0 "
                                      "filter_lo=100 filter_hi=2800 step=100 "
                                      "agc_mode=med agc_threshold=60 agc_off_level=10 "
                                      "pan=0x40000000 txant=ANT1 loopa=0 loopb=0 qsk=0 "
                                      "lock=0 tx=1 active=1")
                       .arg(m_sliceFreqHz / 1.0e6, 0, 'f', 6)
                       .arg(handle)
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
    for (auto it = m_clients.cbegin(); it != m_clients.cend(); ++it) {
        const QString sliceBody =
            QStringLiteral("slice 0 in_use=1 sample_rate=24000 RF_frequency=%1 "
                           "client_handle=0x%2 index_letter=A rit_on=0 rit_freq=0 "
                           "xit_on=0 xit_freq=0 rxant=ANT1 mode=%3 wide=0 "
                           "filter_lo=100 filter_hi=2800 step=100 "
                           "agc_mode=med agc_threshold=60 agc_off_level=10 "
                           "pan=0x40000000 txant=ANT1 loopa=0 loopb=0 qsk=0 "
                           "lock=0 tx=1 active=1")
                .arg(m_sliceFreqHz / 1.0e6, 0, 'f', 6)
                .arg(it.value().handle)
                .arg(m_sliceMode);
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

}  // namespace NereusSDR
