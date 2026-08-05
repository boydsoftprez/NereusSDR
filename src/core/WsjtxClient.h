// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - WSJT-X UDP multicast client (binary protocol)
//
// Ported from AetherSDR src/core/WsjtxClient.h [@0cd4559].
// AetherSDR is (C) its contributors and is licensed GPL-3.0-or-later
// (see https://github.com/ten9876/AetherSDR/blob/main/LICENSE).
//
// Modification history (NereusSDR):
//   2026-05-10  J.J. Boyd / KG4VCF  Phase 3J-2 Task B4. Initial port.
//                                    AetherSDR's "AetherSDR" namespace
//                                    becomes "NereusSDR". DxSpot include
//                                    moved to the extracted DxSpot.h
//                                    (Phase 3J-2 Task B1) instead of
//                                    upstream's transitive include from
//                                    DxClusterClient.h. Logging routes
//                                    through lcSpots ("nereus.spots")
//                                    instead of upstream AetherSDR's
//                                    lcDxCluster. Log file path uses Qt's
//                                    AppConfigLocation (already lands
//                                    under NereusSDR/) instead of
//                                    upstream's GenericConfigLocation +
//                                    "AetherSDR/wsjtx.log". Added three
//                                    public test seams
//                                    processPacketForTest(),
//                                    setDialFreqForTest(), and
//                                    extractCallsignForTest() so unit
//                                    tests can drive the binary parser,
//                                    seed dial-freq state, and validate
//                                    the WSJT-X callsign extractor without
//                                    instantiating a QUdpSocket or
//                                    simulating a multicast sender.
//                                    AI tooling: Anthropic Claude Code.

#pragma once

#include <QObject>
#include <QUdpSocket>
#include <QHostAddress>
#include <QFile>
#include <QString>
#include <atomic>

#include "DxSpot.h"

namespace NereusSDR {

// From AetherSDR src/core/WsjtxClient.h:11-61 [@0cd4559]
//
// WSJT-X UDP multicast client - listens for Decode messages (type 2)
// from WSJT-X and emits spotReceived() for each decoded station.
// Protocol: binary QDataStream on 224.0.0.1:2237 (default).
class WsjtxClient : public QObject {
    Q_OBJECT

public:
    explicit WsjtxClient(QObject* parent = nullptr);
    ~WsjtxClient() override;

    void startListening(const QString& address, quint16 port);
    void stopListening();
    bool isListening() const { return m_listening; }

    QString logFilePath() const;

    // Public test seams. Same bodies as the private impls; exist so unit
    // tests can drive the binary parser, seed dial-freq state, and
    // validate the WSJT-X callsign extractor without instantiating a
    // QUdpSocket or simulating a multicast sender.
    void processPacketForTest(const QByteArray& data) {
        parseMessage(data);
    }
    void setDialFreqForTest(double dialFreqHz, const QString& mode) {
        m_dialFreqHz = dialFreqHz;
        m_mode = mode;
    }
    QString extractCallsignForTest(const QString& message) const {
        return extractCallsign(message);
    }

signals:
    void listening();
    void stopped();
    void spotReceived(const DxSpot& spot);
    void rawLineReceived(const QString& line);
    void statusReceived(const QString& id, double dialFreqHz, const QString& mode);

private slots:
    void onReadyRead();

private:
    static constexpr quint32 WsjtxMagic = 0xadbccbda;

    // QDataStream helpers - parse big-endian Qt-serialized types
    static bool readQString(QDataStream& ds, QString& out);
    static bool readBool(QDataStream& ds, bool& out);

    void parseMessage(const QByteArray& data);
    void parseStatus(QDataStream& ds);
    void parseDecode(QDataStream& ds);
    QString extractCallsign(const QString& message) const;

    QUdpSocket* m_socket;
    QFile       m_logFile;
    QHostAddress m_bindAddr;
    bool        m_isMulticast{false};
    quint16     m_port{2237};
    std::atomic<bool> m_listening{false};

    // Track dial frequency from Status messages (type 1)
    double m_dialFreqHz{0.0};
    QString m_mode;
};

} // namespace NereusSDR
