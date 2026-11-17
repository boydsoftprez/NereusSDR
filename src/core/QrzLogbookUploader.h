#pragma once

// =================================================================
// src/core/QrzLogbookUploader.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// IMPORTANT — this is NOT the XML lookup service. QRZ runs two separate
// interfaces with separate credentials:
//
//   xmldata.qrz.com/xml/current  — callsign lookup, username + password,
//                                  needs an XML subscription.  (QrzClient)
//   logbook.qrz.com/api          — logbook upload, one API KEY taken
//                                  from the logbook's own settings page.
//
// The key is per-logbook, so an operator with several logbooks has
// several keys. Mixing the two up is the commonest way to get an
// unhelpful "invalid" back, which is why they are separate classes with
// separate settings rather than one "QRZ credentials" blob.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
//   2026-11-17 — serviceId()/shortName(); identifiable User-Agent, as
//                 the QRZ logbook API guide asks for. AI-assisted via
//                 Anthropic Claude Code, operator Martin Fischer.
// =================================================================

#include "QsoUploader.h"

#include <QNetworkAccessManager>
#include <QUrl>

class QNetworkReply;

namespace Longpath {

class QrzLogbookUploader : public QsoUploader {
    Q_OBJECT
public:
    explicit QrzLogbookUploader(QObject* parent = nullptr);

    void setApiKey(const QString& key);
    QString apiKey() const { return m_apiKey; }

    // Where the record is posted. logbook.qrz.com/api unless a test
    // bench says otherwise — a bench must never reach the real service
    // (same arrangement as QrzLogbookFetcher::setEndpointForTest).
    void setEndpointForTest(const QUrl& url) { m_endpoint = url; }

    QString serviceName() const override;
    QString shortName() const override { return QStringLiteral("QRZ"); }
    QString serviceId() const override { return QStringLiteral("qrz"); }
    bool isConfigured() const override { return !m_apiKey.isEmpty(); }
    void upload(const LogEntry& entry) override;

    // Response bodies are `KEY=VALUE&KEY=VALUE` — not XML, despite the
    // sibling service. Public so tests can pin the parsing without a
    // network round trip.
    struct Result {
        bool    ok{false};
        bool    duplicate{false};
        QString message;
        QString logId;
    };
    static Result parseResponse(const QByteArray& body);

private:
    QNetworkAccessManager m_nam;
    QString m_apiKey;
    QUrl    m_endpoint;
};

} // namespace Longpath
