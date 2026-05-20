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

signals:
    void clientConnected(const QString& peerHost, quint16 peerPort);
    void lineReceived(const QString& peerHost, quint16 peerPort,
                      const QString& line);

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
};

}  // namespace NereusSDR
