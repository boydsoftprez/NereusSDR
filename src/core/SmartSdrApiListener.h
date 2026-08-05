// =================================================================
// src/core/SmartSdrApiListener.h  (NereusSDR)
// =================================================================
//
// NereusSDR-native class. No upstream port. Wire format reverse-engineered
// from FLEX-8600 v4.2.18.41174 SmartSDR TCP session
// (captures/flex-pgxl-tgxl-capture_00001_20260519173452.pcapng, stream 2).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-19 - Implemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
// =================================================================

#pragma once
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHash>
#include <QTimer>
#include <QString>

namespace NereusSDR {

// Minimal SmartSDR API server on TCP 4992 so that FlexRadio-aware accessories
// (PGXL, TGXL) can pull slice / transmit / band state from NereusSDR after
// pairing via the UDP 4992 discovery beacon.
//
// Wire format (from FLEX-8600 capture stream 2):
//   On accept, the server sends:
//     V<version>\n            e.g. "V1.4.0.0\n"
//     H<8-hex-handle>\n       e.g. "H4C70DC9B\n"
//     S<handle>|radio ...\n   (zero or more initial status snapshots)
//   The client then sends commands as:
//     C<seq>|<command>\n
//   The server replies with:
//     R<seq>|<errcode>|<body>\n   (errcode 0 = OK)
//   The server may push status updates at any time:
//     S<handle>|<body>\n
//
// We respond permissively (R<seq>|0|<empty>) to every command we don't yet
// implement, so PGXL/TGXL stay connected and we can grow the responder
// incrementally without breaking pairing each time.
//
// We push slice / transmit S-frames at 1 Hz (and on every change) so that
// PGXL's bandA / bandB tracking and TGXL's CAT-following both work.
class SmartSdrApiListener : public QObject {
    Q_OBJECT
public:
    explicit SmartSdrApiListener(QObject* parent = nullptr);

    // Production entry point: binds to AnyIPv4:4992. Equivalent to
    // start(QHostAddress::AnyIPv4, 4992).
    bool start();

    // Test seam: bind to a caller-chosen address and port. Used by the
    // PTT-chain unit tests to drive a listener on loopback + ephemeral port
    // so multiple test cases can run concurrently and the production 4992
    // bind is not required. Production code never calls this overload.
    bool start(QHostAddress bindAddr, quint16 port);
    void stop();
    bool isListening() const;

    // Test-only convenience: return the actual port the server bound to.
    // When start() picked an ephemeral port (port=0), this is the kernel-
    // assigned value tests need to know in order to connect a client.
    quint16 serverPort() const { return m_server.serverPort(); }

    // Push current per-slice state into the listener. The listener mirrors
    // these into S<handle>|slice <id> ... and S<handle>|transmit frames for
    // every connected client at 1 Hz, plus an immediate push on change so
    // PGXL/TGXL band tracking is sub-second.
    //
    // freqHz: VFO A center frequency in Hz (qint64).
    // mode:   Thetis-style mode string ("USB", "LSB", "CW", "AM", "FM",
    //         "DIGU", "DIGL", "RTTY", "FDV", "RADE", etc.). Case-sensitive
    //         to the FlexAPI vocabulary; pass "USB" by default.
    void setSliceFrequencyHz(int sliceId, qint64 freqHz);
    void setSliceMode(int sliceId, const QString& mode);
    void setTxActive(bool active);
    // Local TUN state. When NereusSDR engages its CW tune carrier
    // (RadioModel::setTune(true) -> gen1 PostGen tone on-air), call
    // setTuneActive(true) so the listener's outbound `transmit` S-frame
    // carries tune=1. TGXL reads that key and starts its relay sweep
    // instead of waiting on the LAN PTT round-trip that only triggers
    // from a TGXL-initiated tune (hardware button or native app).
    void setTuneActive(bool active);

    // Broadcast a `S0|interlock` global state frame to all clients.
    // FLEX uses this object as the canonical PTT signal: PGXL and TGXL
    // watch the interlock state transitions (READY -> PTT_REQUESTED ->
    // TRANSMITTING -> UNKEY_REQUESTED -> READY) and set their internal
    // pttA flag from it. Without this broadcast a TGXL relay sweep
    // initiated by our local TUN/MOX press aborts with "no PTT in" even
    // though the carrier is on-air, because TGXL trusts the interlock
    // object more than direct RF sensing.
    //
    // source: "TUNE" for a TUN-engaged TX (gen1 tone), "MOX" for a
    // regular voice MOX. Empty string for the READY transition.
    // Wire format observed in stream 2 of the bench capture:
    //   S0|interlock tx_client_handle=0x00000000 state=TRANSMITTING
    //              reason= source=TUNE tx_allowed=1 amplifier=
    void setInterlockTransmitting(bool transmitting, const QString& source);

    // Returns true if any connected client has registered an interlock
    // (sent `interlock create`) AND the interlock is currently enabled.
    // Used by RadioModel's RF-flow gate (deck item #3) to decide whether
    // to defer TxChannel::setRunning until interlockGranted: if no amp
    // is in the chain, there's no relay-switch race to wait on, so we
    // engage RF immediately the Thetis-faithful way.
    bool hasInterlockedAmp() const;

    // Look up the registered amp model string for a given amp handle.
    // Used by RadioModel's `amplifier set` proxy: when TGXL sends
    // `amplifier set 0x<handle> <kv>` we need to figure out which amp
    // owns that handle so we can forward the cmd on the right native
    // protocol socket (PGXL:9008 vs TGXL:9010). ampHandle is the hex
    // string WITHOUT the "0x" prefix. Returns "PowerGeniusXL" or
    // "TunerGeniusXL" (per the `model=` field from `amplifier create`),
    // or an empty string if no client has registered that handle.
    QString ampModelForHandle(const QString& ampHandle) const;

    // Test-only accessor for the synthetic local-client handle (C1).
    // Production code does not call this; the value is consumed internally
    // by every interlock S-frame builder.
    QString localClientHandle() const { return m_localClientHandle; }

signals:
    void clientConnected(const QString& peerHost, quint16 peerPort);
    void lineReceived(const QString& peerHost, quint16 peerPort,
                      const QString& line);

    // LAN PTT received from a SmartSDR-API client (PGXL / TGXL). Fired
    // when the peer sends `transmit tune on` / `transmit tune off`. The
    // peer expects the (real) FlexRadio to engage / drop a CW tune
    // carrier on receipt. RadioModel connects this to its setTune slot
    // so a TGXL hardware TUNE press, which goes:
    //
    //   TGXL hardware TUNE -> TGXL state cycle -> C<n>|transmit tune on
    //   -> our listener -> tuneRequested(true) -> RadioModel::setTune(true)
    //   -> CW carrier on-air -> TGXL relay sweep -> ...
    //   -> C<n>|transmit tune off -> tuneRequested(false) -> setTune(false)
    //
    // ends up actually engaging the carrier instead of being ACKed-and-
    // dropped. NereusSDR-native: AetherSDR has no equivalent because the
    // real FlexRadio handles this internally.
    void tuneRequested(bool on);

    // LAN PTT MOX request from a SmartSDR-API client. Same pattern as
    // tuneRequested but for regular `transmit mox on/off` (no tune
    // carrier). Reserved for future wiring -- TGXL doesn't appear to
    // send this; PGXL might during some pairing paths.
    void moxRequested(bool on);

    // Interlock handshake resolution. Fired exactly once after each
    // setInterlockTransmitting(true) call:
    //
    //   - interlockGranted(source): all registered amps ACKed with
    //     `C<seq>|interlock ready <id>` and the TRANSMITTING S-frame was
    //     broadcast. The TX path is clear; RF may flow.
    //   - interlockBlocked(reason): 500 ms wiki-spec timeout elapsed
    //     without all amps ACKing. Per wiki TCPIP-interlock the radio
    //     does NOT fall through to TRANSMITTING; it emits an AMP-blocked
    //     READY and stays out of TX. RadioModel listens for this and
    //     rolls back the local MOX (degraded -> RX) + surfaces a toast.
    //
    // RadioModel wires these to the appropriate MOX / UI rollback paths.
    void interlockGranted(const QString& source);
    void interlockBlocked(const QString& reason);

    // 2026-05-20 pcap-driven (flex-tgxl-direct-CONTROL.pcapng @T+172.201):
    // When TGXL (or any amp) wants to coordinate with another amp in the
    // chain, it sends `C<seq>|amplifier set 0x<handle> <key>=<value>` to
    // the radio. The real FlexRadio acts as a passive proxy: it looks up
    // which amp owns the handle and forwards the command to that amp via
    // its native protocol (TCP 9008 for PGXL, 9010 for TGXL). Example:
    //
    //   TGXL -> FLEX:4992  "amplifier set 0x22E8213A operate=0"
    //   FLEX -> PGXL:9008  "operate=0"        (FLEX recognized 0x22E8213A
    //                                          as the PGXL amp handle)
    //
    // This is how TGXL auto-standbys PGXL during its own autotune cycle
    // -- and the path NereusSDR was previously missing. We emit this
    // signal whenever we receive an `amplifier set` from a client;
    // RadioModel listens, matches the handle against the right amp
    // connection, and forwards the command.
    //
    // ampHandle is the hex string (e.g. "22E8213A") WITHOUT the "0x"
    // prefix. key and value are the verbatim text from the wire.
    void amplifierSetRequested(const QString& ampHandle,
                               const QString& key,
                               const QString& value);

private slots:
    void onNewConnection();
    void onClientDataReady();
    void onClientDisconnected();
    void onPeriodicTick();

private:
    // Per-socket session state.
    struct ClientState {
        QByteArray readBuffer;     // line accumulator (CR-terminated)
        QString    handle;         // 8-hex unique handle for this client
        // Amp handle assigned when the client sends `amplifier create`.
        // Returned in the R-frame body so the client knows its own amp
        // handle, then echoed in `S0|amplifier <handle> pttA=X` updates
        // when local TUN/MOX engages. Empty until the client registers.
        QString    ampHandle;
        // What kind of amp this client registered as (PowerGeniusXL or
        // TunerGeniusXL). Drives interlock `amplifier=` selection.
        QString    ampModel;
        // Parsed from `amplifier create ip=... port=... model=... serial_num=... ant=...`
        // Echoed back in the S0|amplifier broadcast so PGXL/TGXL match on
        // their own serial (FLEX always carries serial_num + model in its
        // amp S-frame; bare partial updates may be ignored).
        QString    ampSerial;
        QString    ampIp;
        QString    ampAnt;

        // ── Ethernet Interlock state (per smartsdr-api-docs TCPIP-interlock) ──
        // When the amp sends `C<seq>|interlock create type=AMP name=<name>
        // serial=<serial> valid_antennas=...`, we assign a numeric interlock
        // id and return it in the R-frame body (e.g. `R<seq>|0|1`). The amp
        // then references this id in subsequent `C<seq>|interlock ready <id>`
        // ACKs we wait for during the PTT handshake.
        //
        // The handshake (driven by our setInterlockTransmitting() entry):
        //   1. amp -> us:    C|interlock create type=AMP name=X ...
        //   2. us -> amp:    R|0|<interlock_id>  (this stores interlockId + name)
        //   3. us -> all:    S0|interlock state=PTT_REQUESTED reason=AMP:X source=MIC
        //   4. amp -> us:    C|interlock ready <interlock_id>  (sets ackReady=true)
        //   5. us -> all:    S0|interlock state=TRANSMITTING source=MIC  (when all ready)
        //   6. us -> all:    S0|interlock state=READY reason=AMP:X (on TX release)
        int     interlockId{0};      // 0 = no interlock registered yet
        QString interlockName;       // e.g. "PG-XL", "TG" (from create name=)
        bool    ackReady{false};     // set by C|interlock ready <id>; reset on engage
        // Timestamp when ackReady was most recently set true. Used by
        // setInterlockTransmitting to decide whether to honor a "pre-ack"
        // (an `interlock ready N` that arrived BEFORE our PTT_REQUESTED
        // -- happens on TGXL hardware TUNE press, where TGXL sends
        // `transmit tune on` and `interlock ready N` in the same TCP
        // burst before we've completed PGXL standby + carrier engage).
        // If the ack is recent (within ~500 ms), we treat it as valid
        // for the current cycle and don't wipe it. Bench-confirmed
        // 2026-05-20 21:30: without this, the hardware-TUNE path's
        // pre-ack got wiped, grant never fired, TGXL aborted with
        // "low input power" because TxChannel never started.
        qint64  ackReceivedMs{0};
        // Per smartsdr-api-docs TCPIP-interlock, amps can toggle their
        // interlock enable/disable. PGXL bench-confirmed 2026-05-20:
        // sends `interlock disable <id>` immediately on operate=0 (going
        // STANDBY) and `interlock enable <id>` when transitioning back to
        // IDLE/OPERATE. A disabled interlock does NOT participate in the
        // PTT_REQUESTED ACK chain (the amp has stepped out of the TX
        // path). Default is true on `interlock create` per the wiki.
        bool    interlockEnabled{true};
    };

    void sendBanner(QTcpSocket* sock, const QString& handle);
    void dispatchLine(QTcpSocket* sock, const QString& line);
    void sendResponse(QTcpSocket* sock, quint32 seq, int err,
                      const QString& body);
    void sendStatus(QTcpSocket* sock, const QString& handle,
                    const QString& body);
    void broadcastSliceState();    // push current slice 0 + transmit to all
    QString generateHandle() const;

    QTcpServer                       m_server;
    QHash<QTcpSocket*, ClientState>  m_clients;
    QTimer                           m_periodicTimer;  // 1 Hz S-frame push

    // Mirrored radio state pushed in by RadioModel / MainWindow.
    qint64  m_sliceFreqHz{14250000};   // VFO A in Hz; default 14.250 USB
    QString m_sliceMode{QStringLiteral("USB")};
    bool    m_txActive{false};
    bool    m_tuneActive{false};

    // Monotonically increasing interlock id, assigned sequentially on each
    // `C|interlock create`. FLEX returns `R<seq>|0|1`, `R<seq>|0|2`, etc.
    int     m_nextInterlockId{1};

    // Pending PTT engage state: when we send PTT_REQUESTED and wait for
    // amps to ACK with `C|interlock ready <id>`. m_pttPendingSource records
    // the source (MIC / TUNE) the requester used so we can resume
    // TRANSMITTING with the same value once all amps are ready.
    QString m_pttPendingSource;        // empty when not waiting for ACKs

    // 2026-05-21 4o3a-lan-ptt-pcap-divergence.md §8 C1: synthetic local-client
    // handle used as tx_client_handle= in every PTT_REQUESTED / TRANSMITTING /
    // UNKEY_REQUESTED / READY S-frame. Generated once per start() so it is
    // stable for the listener's lifetime and distinct from every amp's banner
    // handle. NereusSDR equivalent of the SmartSDR-Win PC client's 0x66B137B7
    // in the canonical pcap (flex-tgxl-direct-CONTROL.pcapng @ T+167.678).
    QString m_localClientHandle;

    // 2026-05-21 4o3a-lan-ptt-pcap-divergence.md §8 C2: name= of the amp
    // that most recently sent `transmit tune on`. Used as reason=AMP:<name>
    // in the PTT_REQUESTED frame when source=TUNE. Cleared on
    // tuneRequested(false) or on next setInterlockTransmitting(false).
    QString m_lastTuneInitiator;

    QTimer  m_pttAckTimeout;           // 500 ms per wiki spec
    void    advanceToTransmittingIfReady();
    void    onPttAckTimeout();

    // 2026-05-21 4o3a-lan-ptt-pcap-divergence.md §8 C4: actual emission
    // of the canonical TRANSMITTING S-frame + RF-flow gate release.
    // Wrapped by both the success path (advanceToTransmittingIfReady)
    // and the timeout path (onPttAckTimeout) inside a 30 ms QTimer to
    // match pcap T+167.704 -> T+167.734 settle window.
    void broadcastTransmitting(const QString& source);

    // 2026-05-21 4o3a-lan-ptt-pcap-divergence.md §8 C2: resolve
    // reason=AMP:<name> for the canonical PTT_REQUESTED frame.
    // wireSource is "TUNE" or "MIC".
    QString initiatingAmpName(const QString& wireSource) const;

    // 2026-05-21 4o3a-lan-ptt-pcap-divergence.md §8 C5: canonical
    // un-key sequence. Emits UNKEY_REQUESTED reason=AMP:<initiator>,
    // then READY (empty reason) 2 ms later, then READY reason=AMP:<...>
    // 0.5 ms after that. Matches pcap T+168.874 -> T+168.877.
    void broadcastUnkeySequence(const QString& initiatorName);

    // 2026-05-21 bench-confirmed handshake-flap follow-up: canonical FLEX
    // uses NEGATIVE filter passband for lower-sideband modes (LSB, DIGL,
    // FDVL) and POSITIVE for everything else. Without sign-correct values
    // TGXL drops the SmartSDR API socket within ~2 sec of initial status
    // validation when the slice is in LSB. Pcap evidence: flex-tgxl-direct-
    // CONTROL.pcapng @ T+206.741 carries `mode=LSB ... filter_lo=-2800
    // filter_hi=-100`. Returns (lo, hi) for the slice S-frame.
    QPair<int, int> defaultFilterForMode(const QString& mode) const;
};

}  // namespace NereusSDR
