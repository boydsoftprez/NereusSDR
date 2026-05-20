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
                   QStringLiteral("transmit tune=%1 mon=0 mox=%2")
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
                   QStringLiteral("transmit tune=%1 mon=0 mox=%2")
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
        QStringLiteral("transmit tune=%1 mon=0 mox=%2")
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
        sendStatus(it.key(), it.value().handle, sliceBody);
        sendStatus(it.key(), it.value().handle, txBody);
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
