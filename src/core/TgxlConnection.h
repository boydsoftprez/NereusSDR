// =================================================================
// src/core/TgxlConnection.h  (NereusSDR)
// =================================================================
// Source attribution (AetherSDR, GPLv3):
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       per https://github.com/ten9876/AetherSDR (GPLv3)
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 section 5 requirements.
// =================================================================
// Modification history (NereusSDR):
//   2026-05-18  Ported in C++20/Qt6 for NereusSDR by J.J. Boyd (KG4VCF),
//                 with AI-assisted transformation via Anthropic Claude Code.
//                 Layout from AetherSDR src/core/TgxlConnection.{h,cpp} [@0cd4559].
// =================================================================
#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QByteArray>
#include <QMap>
#include <QString>

namespace NereusSDR {

// Direct TCP connection to a 4O3A Tuner Genius XL on port 9010.
// Provides manual relay control (C1/L/C2) via the TGXL's native protocol,
// which is independent of the FlexRadio on port 4992.
//
// Protocol format (same style as SmartSDR):
//   C<seq>|<command>\n          -- client command
//   R<seq>|<code>|<body>\n      -- TGXL response
//   S0|state key=val ...\n      -- unsolicited state push
//   V<version>\n                -- version line on connect
//
// Reverse-engineered from 4O3A TGXL management app pcap (#469).
class TgxlConnection : public QObject {
    Q_OBJECT
public:
    explicit TgxlConnection(QObject* parent = nullptr);

    bool    isConnected() const { return m_connected; }
    QString version()     const { return m_version; }
    QString peerAddress() const { return m_socket.peerAddress().toString(); }
    quint16 peerPort()    const { return m_socket.peerPort(); }

    // Test-only: feed a single line into processLine (no newline needed).
    // Used by tst_tgxl_connection_parse. Production code never calls this.
    void injectLineForTesting(const QString& line) { processLine(line); }

public slots:
    void connectToTgxl(const QString& host, quint16 port = 9010);
    void disconnect();
    void adjustRelay(int relay, int direction);
    quint32 sendCommand(const QString& cmd);

signals:
    void connected();
    void disconnected();
    void connectionFailed(const QString& errorString);
    void stateUpdated(const QMap<QString, QString>& kvs);
    void statusUpdated(const QMap<QString, QString>& kvs);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onError();
    void pollStatus();

private:
    void processLine(const QString& line);

    QTcpSocket m_socket;
    QTimer     m_pollTimer;       // 1/sec status poll
    QByteArray m_readBuf;
    quint32    m_seq{0};
    bool       m_connected{false};
    bool       m_gotVersion{false};
    QString    m_version;
};

}  // namespace NereusSDR
