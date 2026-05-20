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

    bool start();              // bind to AnyIPv4:4992
    void stop();
    bool isListening() const;

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
    QTimer  m_pttAckTimeout;           // 500 ms per wiki spec
    void    advanceToTransmittingIfReady();
    void    onPttAckTimeout();
};

}  // namespace NereusSDR
