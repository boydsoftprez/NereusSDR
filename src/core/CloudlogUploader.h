#pragma once

// =================================================================
// src/core/CloudlogUploader.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// Cloudlog and its fork Wavelog take the same JSON QSO endpoint, so one
// class covers both. The operator supplies the base URL of their own
// instance — these are usually self-hosted, which is the point of them —
// plus an API key and the numeric station profile ID.
//
// The station profile ID matters more than it looks: an operator with a
// home station and a portable profile has two, and posting a contact
// against the wrong one files it under the wrong locator and the wrong
// grid. It is therefore required rather than defaulted to 1.
//
// The API key is a credential and goes to the keychain, never into the
// settings XML — settings files end up in backups, screenshots and
// support bundles.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
//   2026-11-17 — Answers checked against the Wavelog source
//                 (application/controllers/Api.php::qso, master of
//                 that day) and Cloudlog's: a duplicate is HTTP 400
//                 "abort" on Wavelog, "wrong JSON" is HTTP 200, and a
//                 web page answering 200 is not an API. Named Wavelog
//                 first. AI-assisted via Anthropic Claude Code,
//                 operator Martin Fischer.
// =================================================================

#include "QsoUploader.h"

#include <QNetworkAccessManager>

class QNetworkReply;

namespace Longpath {

class CloudlogUploader : public QsoUploader {
    Q_OBJECT
public:
    explicit CloudlogUploader(QObject* parent = nullptr);

    // Base URL of the instance, with or without a trailing slash and
    // with or without "/index.php" — operators paste whatever their
    // browser shows, and refusing that is a support question we would
    // rather not answer.
    void setBaseUrl(const QString& url);
    QString baseUrl() const { return m_baseUrl; }

    void setApiKey(const QString& key);
    void setStationProfileId(const QString& id);
    QString stationProfileId() const { return m_stationId; }

    QString serviceName() const override;
    QString shortName() const override { return QStringLiteral("Wavelog"); }
    // "cloudlog", like the settings keys (CloudlogUrl, cloudlog.apikey):
    // renaming the storage would orphan what an operator already has.
    QString serviceId() const override { return QStringLiteral("cloudlog"); }
    bool isConfigured() const override
    {
        return !m_baseUrl.isEmpty() && !m_apiKey.isEmpty()
               && !m_stationId.isEmpty();
    }
    void upload(const LogEntry& entry) override;

    // Turn whatever the operator pasted into the QSO endpoint. Public so
    // the normalisation can be tested without a server: this is the part
    // that goes wrong, not the HTTP.
    static QString qsoEndpoint(const QString& base);

    struct Result {
        bool    ok{false};
        bool    duplicate{false};
        QString message;
    };
    static Result parseResponse(int httpStatus, const QByteArray& body);

    // Wavelog's own line for "this contact is already in the log". The
    // words "Duplicate for" in it are TRANSLATED by the instance (a
    // German one says "Doublette zu"), so the fixed frame around them
    // is what identifies it, not the word.
    static bool isDuplicateMessage(const QString& message);

private:
    QNetworkAccessManager m_nam;
    QString m_baseUrl;
    QString m_apiKey;
    QString m_stationId;
};

} // namespace Longpath
