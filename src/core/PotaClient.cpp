// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - POTA (Parks On The Air) HTTPS spot poller (implementation)
//
// Ported from AetherSDR src/core/PotaClient.cpp [@0cd4559].
// AetherSDR is (C) its contributors and is licensed GPL-3.0-or-later
// (see https://github.com/ten9876/AetherSDR/blob/main/LICENSE).
//
// Modification history (NereusSDR):
//   2026-05-10  J.J. Boyd / KG4VCF  Phase 3J-2 Task B2. Initial port.
//                                    See PotaClient.h for full attribution
//                                    notes. NereusSDR refactor: the parse
//                                    + dedup body of upstream onPollTimer()
//                                    is split into a pure
//                                    parseAndCollect(QByteArray) helper
//                                    that returns the vector of NEW spots
//                                    and a thin onPollTimer() that handles
//                                    HTTP, logging, and signal emission.
//                                    Logic, field mapping, lifetime
//                                    fallback, and color handling are
//                                    preserved verbatim from upstream.
//                                    AI tooling: Anthropic Claude Code.

#include "PotaClient.h"
#include "AppSettings.h"
#include "LogCategories.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkReply>
#include <QStandardPaths>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>

namespace NereusSDR {

// From AetherSDR src/core/PotaClient.cpp:16-23 [@0cd4559]
PotaClient::PotaClient(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_pollTimer(new QTimer(this))
{
    m_pollTimer->setSingleShot(false);
    connect(m_pollTimer, &QTimer::timeout, this, &PotaClient::onPollTimer);
}

// From AetherSDR src/core/PotaClient.cpp:25-29 [@0cd4559]
PotaClient::~PotaClient()
{
    stopPolling();
    m_logFile.close();
}

// From AetherSDR src/core/PotaClient.cpp:31-35 [@0cd4559]
// NereusSDR uses Qt's AppConfigLocation (which already lands under
// "NereusSDR/" when QCoreApplication::organizationName/applicationName
// are set) instead of AetherSDR's GenericConfigLocation +
// "AetherSDR/...". This keeps the file co-located with the rest of
// NereusSDR's per-user state, mirroring the SpotCollectorClient port
// from Task B1.
QString PotaClient::logFilePath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
           + "/pota.log";
}

// From AetherSDR src/core/PotaClient.cpp:37-59 [@0cd4559]
void PotaClient::startPolling(int intervalSec)
{
    if (m_polling) {
        return;
    }

    qCDebug(lcSpots) << "PotaClient: starting polling every" << intervalSec << "sec";

    // Open log file (truncate on each start)
    m_logFile.close();
    m_logFile.setFileName(logFilePath());
    QDir().mkpath(QFileInfo(m_logFile).absolutePath());
    if (m_logFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        m_logFile.write(QString("--- POTA polling started at %1 (every %2s) ---\n")
            .arg(QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd HH:mm:ss UTC"))
            .arg(intervalSec).toUtf8());
        m_logFile.flush();
    }

    m_seenSpotIds.clear();
    m_polling = true;
    m_pollTimer->start(intervalSec * 1000);
    onPollTimer();  // immediate first poll
    emit started();
}

// From AetherSDR src/core/PotaClient.cpp:61-67 [@0cd4559]
void PotaClient::stopPolling()
{
    if (!m_polling) {
        return;
    }
    m_pollTimer->stop();
    m_polling = false;
    emit stopped();
}

// From AetherSDR src/core/PotaClient.cpp:90-155 [@0cd4559]
//
// NereusSDR refactor: upstream did the parse + dedup + emit + log work
// inline in the QNetworkReply::finished lambda. NereusSDR splits the
// pure data-transformation core into parseAndCollect() so the public
// parseJsonForTest() seam can exercise the parser + dedup set without
// instantiating a QNetworkAccessManager or simulating an HTTPS round-
// trip. Field mapping, lifetime fallback (`expire > 0 ? expire : 600`),
// color formatting (#RRGGBB -> #FFRRGGBB), comment composition (ref +
// park + mode), spotTime parsing, and the `freqMhz <= 0 ||
// dxCall.isEmpty()` reject filter are byte-for-byte from upstream.
QVector<DxSpot> PotaClient::parseAndCollect(const QByteArray& data)
{
    QVector<DxSpot> newSpots;

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) {
        return newSpots;
    }

    QJsonArray arr = doc.array();
    for (const auto& val : arr) {
        QJsonObject obj = val.toObject();

        int spotId = obj.value("spotId").toInt();
        if (m_seenSpotIds.contains(spotId)) {
            continue;
        }
        m_seenSpotIds.insert(spotId);

        DxSpot spot;
        spot.dxCall      = obj.value("activator").toString();
        spot.spotterCall = obj.value("spotter").toString();
        spot.source      = QStringLiteral("POTA");

        // Frequency in kHz -> MHz
        QString freqStr = obj.value("frequency").toString();
        spot.freqMhz = freqStr.toDouble() / 1000.0;

        // Use API expire field for lifetime (seconds remaining)
        int expire = obj.value("expire").toInt();
        spot.lifetimeSec = (expire > 0) ? expire : 600;

        // Apply POTA spot color (#RRGGBB -> #FFRRGGBB for radio)
        QString potaColor = AppSettings::instance().value("PotaSpotColor", "#FFFF00").toString();
        if (potaColor.length() == 7) {
            potaColor = "#FF" + potaColor.mid(1);
        }
        spot.color = potaColor;

        // Build comment: park reference + park name + mode
        QString ref  = obj.value("reference").toString();
        QString park = obj.value("name").toString();
        QString mode = obj.value("mode").toString();
        spot.comment = ref;
        if (!park.isEmpty()) {
            spot.comment += " " + park;
        }
        if (!mode.isEmpty()) {
            spot.comment += " " + mode;
        }

        // Parse spot time
        QString timeStr = obj.value("spotTime").toString();
        QDateTime dt = QDateTime::fromString(timeStr, Qt::ISODate);
        if (dt.isValid()) {
            spot.utcTime = dt.toUTC().time();
        } else {
            spot.utcTime = QDateTime::currentDateTimeUtc().time();
        }

        if (spot.freqMhz <= 0.0 || spot.dxCall.isEmpty()) {
            continue;
        }

        newSpots.append(spot);
    }

    return newSpots;
}

// From AetherSDR src/core/PotaClient.cpp:69-159 [@0cd4559]
//
// NereusSDR refactor: parse + dedup body extracted into
// parseAndCollect() above. This slot now handles HTTP, logging, and
// signal emission only. Order of side-effects (log line per spot, then
// rawLineReceived, then spotReceived, then pollComplete at end) is
// preserved verbatim from upstream.
void PotaClient::onPollTimer()
{
    QNetworkRequest req{QUrl{ApiUrl}};
    req.setHeader(QNetworkRequest::UserAgentHeader, "NereusSDR");
    // Disable HTTP keep-alive to avoid a known Qt6/macOS bug where the
    // CFSocket source for a kept-alive socket fires into a destroyed
    // QSocketNotifier after the server's idle-close, crashing in
    // qt_mac_socket_callback -> QCoreApplication::sendEvent.  POTA polling
    // is once per minute over the public internet, so the extra TCP
    // handshake is negligible.
    req.setRawHeader("Connection", "close");
    auto* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            QString err = reply->errorString();
            qCWarning(lcSpots) << "PotaClient: poll failed:" << err;
            emit pollError(err);
            return;
        }

        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isArray()) {
            emit pollError(QStringLiteral("Invalid JSON response"));
            return;
        }

        int total = doc.array().size();
        QVector<DxSpot> newSpots = parseAndCollect(data);

        for (const DxSpot& spot : newSpots) {
            // Reconstruct the log line using the same format upstream
            // emitted: HH:mm  call  freq_kHz  ref  mode. Reference and
            // mode are recoverable from spot.comment ("ref park mode")
            // but logging the trimmed `spot.comment` is equivalent for
            // operator-readable output.
            QString logLine = QString("%1  %2  %3 kHz  %4")
                .arg(spot.utcTime.toString("HH:mm"),
                     spot.dxCall,
                     QString::number(spot.freqMhz * 1000.0, 'f', 1),
                     spot.comment);
            if (m_logFile.isOpen()) {
                m_logFile.write((logLine + "\n").toUtf8());
                m_logFile.flush();
            }
            emit rawLineReceived(logLine);
            emit spotReceived(spot);
        }

        emit pollComplete(total, newSpots.size());
    });
}

} // namespace NereusSDR
