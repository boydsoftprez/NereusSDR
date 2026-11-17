// =================================================================
// src/core/CloudlogUploader.cpp  (Longpath)
// =================================================================
//
// Longpath-original — see CloudlogUploader.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
//   2026-11-17 — parseResponse against the servers' own source, see
//                 the header. AI-assisted via Anthropic Claude Code,
//                 operator Martin Fischer.
// =================================================================

#include "CloudlogUploader.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrl>

#include <algorithm>

namespace Longpath {

namespace {
constexpr int kTimeoutMs = 20000;
}

CloudlogUploader::CloudlogUploader(QObject* parent) : QsoUploader(parent) {}

QString CloudlogUploader::serviceName() const
{
    // Wavelog first: it is the fork operators run today, and the one
    // this was last checked against.
    return QStringLiteral("Wavelog / Cloudlog");
}

void CloudlogUploader::setBaseUrl(const QString& url)
{
    m_baseUrl = url.trimmed();
}

void CloudlogUploader::setApiKey(const QString& key)
{
    m_apiKey = key.trimmed();
}

void CloudlogUploader::setStationProfileId(const QString& id)
{
    m_stationId = id.trimmed();
}

QString CloudlogUploader::qsoEndpoint(const QString& base)
{
    QString b = base.trimmed();
    if (b.isEmpty()) { return {}; }

    while (b.endsWith(QLatin1Char('/'))) { b.chop(1); }

    // Operators paste what the browser shows, which may already include
    // any of these tails. Stripping them is cheaper than a support
    // conversation about which form is "correct".
    static const QStringList tails = {
        QStringLiteral("/index.php/api/qso"),
        QStringLiteral("/api/qso"),
        QStringLiteral("/index.php"),
    };
    for (const QString& t : tails) {
        if (b.endsWith(t, Qt::CaseInsensitive)) {
            b.chop(t.size());
            break;
        }
    }
    while (b.endsWith(QLatin1Char('/'))) { b.chop(1); }

    if (b.isEmpty()) { return {}; }

    // Default to https when no scheme was given. An API key sent in
    // clear over http would be handing it to the network.
    if (!b.contains(QStringLiteral("://"))) {
        b = QStringLiteral("https://") + b;
    }
    return b + QStringLiteral("/index.php/api/qso");
}

void CloudlogUploader::upload(const LogEntry& entry)
{
    const QString call = entry.call;
    if (!entry.isValid()) {
        emit uploadFinished(call, false, false,
            QStringLiteral("Nothing to upload"));
        return;
    }
    if (!isConfigured()) {
        emit uploadFinished(call, false, false,
            QStringLiteral("Wavelog is not set up yet"));
        return;
    }

    const QString endpoint = qsoEndpoint(m_baseUrl);
    const QUrl url(endpoint);
    if (!url.isValid()) {
        emit uploadFinished(call, false, false,
            QStringLiteral("That doesn't look like a valid address: %1")
                .arg(endpoint));
        return;
    }

    // The record is LogEntry's ADIF, not a second serialisation — a
    // contact must be spelled the same everywhere it is sent.
    QJsonObject body;
    body.insert(QStringLiteral("key"), m_apiKey);
    body.insert(QStringLiteral("station_profile_id"), m_stationId);
    body.insert(QStringLiteral("type"), QStringLiteral("adif"));
    body.insert(QStringLiteral("string"), entry.toAdifRecord());

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/json"));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    // Without one a server that accepts the connection and then says
    // nothing holds this contact — and everything queued behind it —
    // for as long as the program runs.
    req.setTransferTimeout(kTimeoutMs);

    QNetworkReply* reply =
        m_nam.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));

    connect(reply, &QNetworkReply::finished, this, [this, reply, call]() {
        reply->deleteLater();
        const int status = reply
            ->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray payload = reply->readAll();

        if (reply->error() != QNetworkReply::NoError && status == 0) {
            // No HTTP status at all: the request never reached a server.
            emit uploadFinished(call, false, false, reply->errorString());
            return;
        }
        const Result r = parseResponse(status, payload);
        emit uploadFinished(call, r.ok, r.duplicate, r.message);
    });
}

namespace {

// "Differing locator <b>JN68</b> while importing …: SKIPPED<br>" — the
// messages are written for a web page. Tags out, entities back.
QString plainText(QString html)
{
    static const QRegularExpression tag(QStringLiteral("<[^>]*>"));
    html.replace(tag, QString{});
    html.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    html.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
    html.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
    html.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
    return html.simplified();
}

} // namespace

bool CloudlogUploader::isDuplicateMessage(const QString& message)
{
    // Wavelog, Logbook_model::import():
    //   "Date/Time: <t> Callsign: <c> Band: <b> " . __("Duplicate for") . " <own call><br>"
    // Only the frame is fixed; see the header.
    static const QRegularExpression frame(
        QStringLiteral("^\\s*Date/Time: .+ Callsign: .+ Band: "));
    return frame.match(message).hasMatch()
           || message.contains(QStringLiteral("dupl"), Qt::CaseInsensitive);
}

CloudlogUploader::Result CloudlogUploader::parseResponse(int httpStatus,
                                                         const QByteArray& body)
{
    Result r;
    const QString text = QString::fromUtf8(body).trimmed();

    // What the two servers send (Wavelog and Cloudlog,
    // application/controllers/Api.php::qso, both read 2026-11-17):
    //
    //   201 {"status":"created", …, "messages":[…]}            both
    //   400 {"status":"abort", …, "messages":["", "<why><br>"]} Wavelog:
    //        duplicate, differing locator, invalid date or call
    //   401 {"status":"failed","reason":"missing or wrong api key"}
    //   401 {"status":"failed","reason":"station id does not belong
    //        to the API key owner."}
    //   403 {"status":"failed","reason":"API key does not have write
    //        permissions"}                                      Wavelog
    //   429 {"status":"failed","reason":"Rate limit exceeded. …"}
    //   200 {"status":"failed","reason":"wrong JSON"}           (sic: 200)
    //
    // So neither the status code nor the body decides alone.
    QString status;
    QString reason;
    QStringList messages;
    int importedCount = -1;
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    const bool isJson = doc.isObject();
    if (isJson) {
        const QJsonObject o = doc.object();
        status = o.value(QLatin1String("status")).toString().toLower();
        for (const char* k : {"reason", "message", "error"}) {
            const QString v = o.value(QLatin1String(k)).toString();
            if (!v.isEmpty()) { reason = plainText(v); break; }
        }
        const QJsonArray arr = o.value(QLatin1String("messages")).toArray();
        for (const QJsonValue& v : arr) {
            // One string may carry several lines, joined by <br>.
            const QStringList lines = v.toString().split(
                QRegularExpression(QStringLiteral("<br\\s*/?>"),
                                   QRegularExpression::CaseInsensitiveOption),
                Qt::SkipEmptyParts);
            for (const QString& line : lines) {
                const QString m = plainText(line);
                if (!m.isEmpty()) { messages << m; }
            }
        }
        if (o.contains(QLatin1String("imported_count"))) {
            importedCount = o.value(QLatin1String("imported_count")).toInt();
        }
    }

    const bool allDuplicates = !messages.isEmpty()
        && std::all_of(messages.cbegin(), messages.cend(), isDuplicateMessage);
    const QString said = !reason.isEmpty() ? reason
                         : messages.join(QStringLiteral("; "));

    // ── The log has it ───────────────────────────────────────────────
    //
    // Wavelog reports a duplicate as HTTP 400 "abort". It is not a
    // failure: the contact IS in the log, which is what was asked for,
    // and treating it as one would retry it for ever.
    if (allDuplicates && (status == QLatin1String("abort")
                          || (httpStatus >= 200 && httpStatus < 300))) {
        r.ok = true;
        r.duplicate = true;
        r.message = QStringLiteral("already in the log");
        return r;
    }

    if (httpStatus >= 200 && httpStatus < 300) {
        if (!isJson || status.isEmpty()) {
            // A web page, a login form, a proxy's greeting: something
            // answered, but not the API. Counting that as "uploaded"
            // would mark contacts as sent that went nowhere.
            r.message = QStringLiteral("that address answered, but not as a "
                                       "Wavelog API — check the instance URL");
            return r;
        }
        if (status == QLatin1String("failed") || status == QLatin1String("abort")) {
            r.message = said.isEmpty() ? status : said;
            return r;
        }
        if (reason.contains(QStringLiteral("dupl"), Qt::CaseInsensitive)
            || status.contains(QStringLiteral("dupl"))) {
            r.ok = true;
            r.duplicate = true;
            r.message = QStringLiteral("already in the log");
            return r;
        }
        if (importedCount == 0 && !messages.isEmpty()) {
            // Cloudlog answers 201 even when it took nothing.
            r.message = messages.join(QStringLiteral("; "));
            return r;
        }
        r.ok = true;
        r.message = QStringLiteral("accepted");
        return r;
    }

    if (httpStatus == 401 || httpStatus == 403) {
        if (reason.contains(QStringLiteral("station id"), Qt::CaseInsensitive)) {
            r.message = QStringLiteral("station profile ID does not belong to "
                                       "this API key — check the number");
        } else if (reason.contains(QStringLiteral("write permission"),
                                   Qt::CaseInsensitive)) {
            r.message = QStringLiteral("the API key is read-only — it needs "
                                       "read + write access");
        } else if (reason.isEmpty()
                   || reason.contains(QStringLiteral("api key"),
                                      Qt::CaseInsensitive)) {
            r.message = QStringLiteral("API key rejected");
        } else {
            r.message = reason;
        }
    } else if (httpStatus == 404) {
        r.message = QStringLiteral("no API at that address — check the "
                                   "instance URL");
    } else if (!said.isEmpty()) {
        r.message = said;
    } else if (!isJson && !text.isEmpty() && !text.startsWith(QLatin1Char('<'))) {
        r.message = text.left(200);
    } else {
        r.message = QStringLiteral("HTTP %1").arg(httpStatus);
    }
    return r;
}

} // namespace Longpath
