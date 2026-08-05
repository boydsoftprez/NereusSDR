// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - POTA (Parks On The Air) HTTPS spot poller
//
// Ported from AetherSDR src/core/PotaClient.h [@0cd4559].
// AetherSDR is (C) its contributors and is licensed GPL-3.0-or-later
// (see https://github.com/ten9876/AetherSDR/blob/main/LICENSE).
//
// Modification history (NereusSDR):
//   2026-05-10  J.J. Boyd / KG4VCF  Phase 3J-2 Task B2. Initial port.
//                                    AetherSDR's "AetherSDR" namespace
//                                    becomes "NereusSDR". `DxSpot` include
//                                    moved to the extracted DxSpot.h
//                                    (Phase 3J-2 Task B1) instead of
//                                    AetherSDR's DxClusterClient.h. The
//                                    qCDebug/qCWarning category routes to
//                                    lcSpots ("nereus.spots") instead of
//                                    AetherSDR's lcDxCluster. The log file
//                                    path uses Qt's AppConfigLocation
//                                    (already lands under NereusSDR/) under
//                                    NereusSDR/pota.log instead of
//                                    AetherSDR's GenericConfigLocation +
//                                    "AetherSDR/pota.log". Added public
//                                    parseJsonForTest() seam returning the
//                                    vector of NEW (post-dedup) spots from
//                                    a poll, so unit tests can exercise the
//                                    JSON parser + dedup set without
//                                    instantiating a QNetworkAccessManager
//                                    or simulating an HTTPS round-trip.
//                                    AI tooling: Anthropic Claude Code.

#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QTimer>
#include <QFile>
#include <QSet>
#include <QVector>
#include <atomic>

#include "DxSpot.h"

namespace NereusSDR {

// From AetherSDR src/core/PotaClient.h:13-47 [@0cd4559]
//
// POTA (Parks on the Air) spot client - polls
// https://api.pota.app/spot/activator every 30 seconds for active
// activations. Emits spotReceived() for each new spot.
class PotaClient : public QObject {
    Q_OBJECT

public:
    explicit PotaClient(QObject* parent = nullptr);
    ~PotaClient() override;

    void startPolling(int intervalSec = 30);
    void stopPolling();
    bool isPolling() const { return m_polling; }

    QString logFilePath() const;

    // Public test seam. Returns the vector of NEW (not yet seen) DxSpot
    // records parsed from `data`, after running the same JSON-parse +
    // dedup logic the live poll uses. Exists so unit tests can validate
    // the parser without instantiating a QNetworkAccessManager or
    // simulating an HTTPS round-trip. Mutates m_seenSpotIds the same way
    // a real poll would, so consecutive calls correctly exercise the
    // dedup set.
    QVector<DxSpot> parseJsonForTest(const QByteArray& data) {
        return parseAndCollect(data);
    }

signals:
    void started();
    void stopped();
    void pollError(const QString& error);
    void spotReceived(const DxSpot& spot);
    void rawLineReceived(const QString& line);
    void pollComplete(int spotCount, int newCount);

private slots:
    void onPollTimer();

private:
    // Shared parse + dedup core. Returns the NEW spots from this `data`
    // payload. Used by both the live HTTPS poll path (onPollTimer) and
    // the public test seam (parseJsonForTest). Pure data transformation:
    // does not touch the log file or emit signals.
    QVector<DxSpot> parseAndCollect(const QByteArray& data);

    QNetworkAccessManager* m_nam;
    QTimer*     m_pollTimer;
    QFile       m_logFile;
    QSet<int>   m_seenSpotIds;   // track spotId to only emit new spots
    std::atomic<bool> m_polling{false};

    static constexpr const char* ApiUrl = "https://api.pota.app/spot/activator";
};

} // namespace NereusSDR
