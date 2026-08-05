// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - DXLab SpotCollector UDP listener
//
// Ported from AetherSDR src/core/SpotCollectorClient.h [@0cd4559].
// AetherSDR is (C) its contributors and is licensed GPL-3.0-or-later
// (see https://github.com/ten9876/AetherSDR/blob/main/LICENSE).
//
// Modification history (NereusSDR):
//   2026-05-10  J.J. Boyd / KG4VCF  Phase 3J-2 Task B1. Initial port.
//                                    DxSpot extracted to src/core/DxSpot.h
//                                    so that other spot-ingest clients
//                                    can include the type without pulling
//                                    in DX-cluster code. Added
//                                    parseDxSpotLineForTest() public
//                                    seam for the parser unit test.
//                                    AetherSDR's "AetherSDR" namespace
//                                    becomes "NereusSDR" and the qCDebug
//                                    category name routes to lcSpots
//                                    (NereusSDR's "nereus.spots") instead
//                                    of AetherSDR's lcDxCluster. The log
//                                    file path uses Qt's
//                                    AppConfigLocation under
//                                    NereusSDR/spotcollector.log instead
//                                    of AetherSDR's GenericConfigLocation
//                                    + "AetherSDR/spotcollector.log".
//                                    AI tooling: Anthropic Claude Code.

#pragma once

#include <QObject>
#include <QUdpSocket>
#include <QFile>
#include <QString>
#include <atomic>

#include "DxSpot.h"

namespace NereusSDR {

// From AetherSDR src/core/SpotCollectorClient.h [@0cd4559]
//
// DXLab SpotCollector UDP listener - receives spot push packets in
// standard "DX de" cluster format on a configurable UDP port (default
// 9999).
class SpotCollectorClient : public QObject {
    Q_OBJECT

public:
    explicit SpotCollectorClient(QObject* parent = nullptr);
    ~SpotCollectorClient() override;

    void startListening(quint16 port);
    void stopListening();
    bool isListening() const { return m_listening; }

    QString logFilePath() const;

    // Public test seam for the parser. Same body as the private
    // parseDxSpotLine() impl; exists so unit tests can validate the
    // regex without instantiating a UDP socket or simulating a packet.
    bool parseDxSpotLineForTest(const QString& line, DxSpot& spot) const {
        return parseDxSpotLine(line, spot);
    }

signals:
    void listening();
    void stopped();
    void spotReceived(const DxSpot& spot);
    void rawLineReceived(const QString& line);

private slots:
    void onReadyRead();

private:
    bool parseDxSpotLine(const QString& line, DxSpot& spot) const;

    QUdpSocket* m_socket;
    QFile       m_logFile;
    quint16     m_port{9999};
    std::atomic<bool> m_listening{false};
};

} // namespace NereusSDR
