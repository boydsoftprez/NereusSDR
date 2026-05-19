// =================================================================
// src/core/PgxlConnection.cpp  (NereusSDR)
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
//                 Layout from AetherSDR src/core/PgxlConnection.{h,cpp} [@0cd4559].
//                 processLine() stubbed; V/R/S frame parsing lands in Tasks 6+7.
// =================================================================
#include "PgxlConnection.h"
#include <QLoggingCategory>

namespace NereusSDR {

Q_LOGGING_CATEGORY(lcPgxl, "nereus.pgxl")

PgxlConnection::PgxlConnection(QObject* parent)
    : QObject(parent) {
    connect(&m_socket, &QTcpSocket::connected,    this, &PgxlConnection::onConnected);
    connect(&m_socket, &QTcpSocket::disconnected, this, &PgxlConnection::onDisconnected);
    connect(&m_socket, &QTcpSocket::readyRead,    this, &PgxlConnection::onReadyRead);
    connect(&m_socket, &QTcpSocket::errorOccurred, this, &PgxlConnection::onError);

    m_pollTimer.setInterval(200);  // 5 Hz per AetherSDR
    connect(&m_pollTimer, &QTimer::timeout, this, &PgxlConnection::pollStatus);
}

void PgxlConnection::connectToPgxl(const QString& host, quint16 port) {
    if (m_connected) { disconnect(); }
    m_seq = 0;
    m_gotVersion = false;
    m_version.clear();
    m_readBuf.clear();
    qCDebug(lcPgxl) << "connecting to" << host << ":" << port;
    m_socket.connectToHost(host, port);
}

void PgxlConnection::disconnect() {
    m_pollTimer.stop();
    m_connected = false;
    m_socket.disconnectFromHost();
}

quint32 PgxlConnection::sendCommand(const QString& cmd) {
    quint32 seq = ++m_seq;
    QString line = QString("C%1|%2\n").arg(seq).arg(cmd);
    m_socket.write(line.toUtf8());
    qCDebug(lcPgxl) << "sent" << line.trimmed();
    return seq;
}

void PgxlConnection::onConnected() {
    qCDebug(lcPgxl) << "TCP connected, waiting for version line";
}

void PgxlConnection::onDisconnected() {
    qCDebug(lcPgxl) << "disconnected";
    m_pollTimer.stop();
    m_connected = false;
    emit disconnected();
}

void PgxlConnection::onError() {
    QString err = m_socket.errorString();
    qCWarning(lcPgxl) << "socket error:" << err;
    emit connectionFailed(err);
}

void PgxlConnection::onReadyRead() {
    m_readBuf.append(m_socket.readAll());
    while (true) {
        int idx = m_readBuf.indexOf('\n');
        if (idx < 0) { break; }
        QString line = QString::fromUtf8(m_readBuf.left(idx)).trimmed();
        m_readBuf.remove(0, idx + 1);
        if (!line.isEmpty()) { processLine(line); }
    }
}

void PgxlConnection::pollStatus() {
    if (m_connected) {
        sendCommand("status");
    }
}

void PgxlConnection::processLine(const QString& line) {
    // Version: V3.8.9
    if (!m_gotVersion && line.startsWith('V')) {
        m_version = line.mid(1);
        m_gotVersion = true;
        qCInfo(lcPgxl) << "PGXL version" << m_version;
        sendCommand("info");
        sendCommand("status");
        m_connected = true;
        m_pollTimer.start();
        emit connected();
        return;
    }
    // Response: R<seq>|<hex>|<body>
    if (line.startsWith('R')) {
        int pipe1 = line.indexOf('|');
        int pipe2 = (pipe1 >= 0) ? line.indexOf('|', pipe1 + 1) : -1;
        if (pipe2 >= 0) {
            QString body = line.mid(pipe2 + 1).trimmed();
            if (!body.isEmpty()) {
                QMap<QString,QString> kvs;
                const auto parts = body.split(' ', Qt::SkipEmptyParts);
                for (const auto& p : parts) {
                    int eq = p.indexOf('=');
                    if (eq > 0) kvs.insert(p.left(eq), p.mid(eq + 1));
                }
                if (!kvs.isEmpty()) emit statusUpdated(kvs);
            }
        }
        return;
    }
    // Status push: S0|state ... or S0|temp=... etc.
    if (line.startsWith('S')) {
        int pipe = line.indexOf('|');
        if (pipe < 0) return;
        QString rest = line.mid(pipe + 1);
        int firstEq = rest.indexOf('=');
        if (firstEq < 0) return;
        int lastSpaceBeforeEq = rest.lastIndexOf(' ', firstEq);
        QString kvString = (lastSpaceBeforeEq >= 0)
            ? rest.mid(lastSpaceBeforeEq + 1) : rest;
        QMap<QString,QString> kvs;
        const auto parts = kvString.split(' ', Qt::SkipEmptyParts);
        for (const auto& part : parts) {
            int eq = part.indexOf('=');
            if (eq > 0)
                kvs.insert(part.left(eq), part.mid(eq + 1));
        }
        if (!kvs.isEmpty())
            emit statusUpdated(kvs);
        return;
    }
}

}  // namespace NereusSDR
