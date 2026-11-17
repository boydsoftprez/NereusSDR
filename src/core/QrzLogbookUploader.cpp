// =================================================================
// src/core/QrzLogbookUploader.cpp  (Longpath)
// =================================================================
//
// Longpath-original — see QrzLogbookUploader.h, in particular the
// note on why this is a different service from QrzClient.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
// =================================================================

#include "QrzLogbookUploader.h"

#include <QCoreApplication>
#include <QLoggingCategory>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

namespace Longpath {

namespace {
Q_LOGGING_CATEGORY(lcQrzLog, "longpath.qrz.logbook")

constexpr const char* kApi = "https://logbook.qrz.com/api";
constexpr int kTimeoutMs = 20000;
}

QrzLogbookUploader::QrzLogbookUploader(QObject* parent)
    : QsoUploader(parent), m_endpoint(QString::fromLatin1(kApi)) {}

QString QrzLogbookUploader::serviceName() const
{
    // User-visible name — plain English, no source cites.
    return QStringLiteral("QRZ Logbook");
}

void QrzLogbookUploader::setApiKey(const QString& key)
{
    m_apiKey = key.trimmed();
}

void QrzLogbookUploader::upload(const LogEntry& entry)
{
    const QString call = entry.call.trimmed().toUpper();
    if (!entry.isValid()) {
        emit uploadFinished(call, false, false,
                            QStringLiteral("Nothing to upload"));
        return;
    }
    if (!isConfigured()) {
        emit uploadFinished(call, false, false,
                            QStringLiteral("No QRZ logbook API key configured"));
        return;
    }

    QUrlQuery form;
    form.addQueryItem(QStringLiteral("KEY"), m_apiKey);
    form.addQueryItem(QStringLiteral("ACTION"), QStringLiteral("INSERT"));
    form.addQueryItem(QStringLiteral("ADIF"), entry.toAdifRecord());

    // POST, not GET: an ADIF record easily exceeds what is sane in a
    // query string, and the key would land in every proxy log.
    QNetworkRequest req{m_endpoint};
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/x-www-form-urlencoded"));
    // QRZ asks every program to name itself; a generic agent "could be
    // subject to rate limiting" (QRZ Logbook API guide). Same spelling
    // as QrzLogbookFetcher, so QRZ sees one program, not two.
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("Longpath/%1 (+https://www.longpath.at)")
                      .arg(QCoreApplication::applicationVersion()));
    req.setTransferTimeout(kTimeoutMs);

    QNetworkReply* reply =
        m_nam.post(req, form.toString(QUrl::FullyEncoded).toUtf8());

    connect(reply, &QNetworkReply::finished, this, [this, reply, call]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(lcQrzLog) << "upload transport failure:"
                                << reply->errorString();
            emit uploadFinished(call, false, false, reply->errorString());
            return;
        }
        const QByteArray body = reply->readAll();
        const Result r = parseResponse(body);
        // The body carries no secret — the key went up, not back.
        qCDebug(lcQrzLog).noquote() << "upload response:"
                                    << QString::fromUtf8(body.left(300));
        emit uploadFinished(call, r.ok, r.duplicate, r.message);
    });
}

QrzLogbookUploader::Result
QrzLogbookUploader::parseResponse(const QByteArray& body)
{
    Result out;

    // `RESULT=OK&LOGID=123&COUNT=1` — ampersand-separated pairs, and
    // the values are percent-encoded, so REASON text arrives readable
    // only after decoding.
    const QUrlQuery q(QString::fromUtf8(body));
    const QString result = q.queryItemValue(QStringLiteral("RESULT"),
                                            QUrl::FullyDecoded).toUpper();
    const QString reason = q.queryItemValue(QStringLiteral("REASON"),
                                            QUrl::FullyDecoded);
    out.logId = q.queryItemValue(QStringLiteral("LOGID"), QUrl::FullyDecoded);

    if (result == QLatin1String("OK")) {
        out.ok = true;
        out.message = out.logId.isEmpty()
            ? QStringLiteral("Uploaded")
            : QStringLiteral("Uploaded (log id %1)").arg(out.logId);
        return out;
    }

    // A duplicate is the normal outcome of a retry, not a failure the
    // operator should have to act on: the contact IS in the logbook,
    // which is what was asked for.
    if (reason.contains(QLatin1String("duplicate"), Qt::CaseInsensitive)) {
        out.ok = true;
        out.duplicate = true;
        out.message = QStringLiteral("Already in your QRZ logbook");
        return out;
    }

    out.ok = false;
    out.message = reason.isEmpty()
        ? (result.isEmpty() ? QStringLiteral("QRZ sent no result")
                            : QStringLiteral("QRZ said %1").arg(result))
        : reason;
    return out;
}

} // namespace Longpath
