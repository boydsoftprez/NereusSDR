// The two parts of a Cloudlog upload that go wrong are the address the
// operator pasted and the answer the server gave. Both are testable
// without a server, and neither is the HTTP.
//
// 2026-11-17: the answers below "wavelog_" are the ones Wavelog's own
// source sends (read that day), not ones this file made up.
// no-port-check: Longpath-original.

#include <QtTest/QtTest>
#include "core/CloudlogUploader.h"

using namespace Longpath;

class TstCloudlogUploader : public QObject {
    Q_OBJECT
private slots:
    void endpoint_from_a_bare_host();
    void endpoint_tolerates_what_the_browser_shows();
    void endpoint_defaults_to_https();
    void endpoint_of_nothing_is_nothing();
    void success_is_success();
    void duplicate_counts_as_success();
    void rejected_key_says_so();
    void wrong_address_says_so();
    void not_configured_until_all_three_are_set();
    // 2026-11-17 — the answers as Wavelog's own source sends them
    // (application/controllers/Api.php::qso, Logbook_model::import).
    void wavelog_created_is_success();
    void wavelog_duplicate_is_http_400_and_still_done();
    void wavelog_duplicate_on_a_translated_instance();
    void wavelog_refusal_shows_the_servers_reason();
    void wavelog_wrong_station_id_says_so();
    void wavelog_read_only_key_says_so();
    void wavelog_wrong_json_is_a_failure_despite_http_200();
    void a_web_page_is_not_an_api();
    void cloudlog_created_is_success();
    void named_for_the_operator();
};

void TstCloudlogUploader::endpoint_from_a_bare_host()
{
    QCOMPARE(CloudlogUploader::qsoEndpoint(
                 QStringLiteral("https://log.example.org")),
             QStringLiteral("https://log.example.org/index.php/api/qso"));
}

void TstCloudlogUploader::endpoint_tolerates_what_the_browser_shows()
{
    // Operators paste the address bar. Every one of these is the same
    // instance, and refusing any of them buys a support conversation
    // about which spelling is "correct".
    const QString want =
        QStringLiteral("https://log.example.org/index.php/api/qso");
    for (const QString& in : {
             QStringLiteral("https://log.example.org/"),
             QStringLiteral("https://log.example.org/index.php"),
             QStringLiteral("https://log.example.org/index.php/"),
             QStringLiteral("https://log.example.org/api/qso"),
             QStringLiteral("https://log.example.org/index.php/api/qso"),
             QStringLiteral("  https://log.example.org  "),
         }) {
        QCOMPARE(CloudlogUploader::qsoEndpoint(in), want);
    }
}

void TstCloudlogUploader::endpoint_defaults_to_https()
{
    // An API key sent in clear over http is handed to the network.
    QCOMPARE(CloudlogUploader::qsoEndpoint(QStringLiteral("log.example.org")),
             QStringLiteral("https://log.example.org/index.php/api/qso"));
    // But an explicit http:// is the operator's decision to make — a
    // Cloudlog on the same LAN is a normal setup.
    QCOMPARE(CloudlogUploader::qsoEndpoint(QStringLiteral("http://192.168.1.9")),
             QStringLiteral("http://192.168.1.9/index.php/api/qso"));
}

void TstCloudlogUploader::endpoint_of_nothing_is_nothing()
{
    QVERIFY(CloudlogUploader::qsoEndpoint(QString{}).isEmpty());
    QVERIFY(CloudlogUploader::qsoEndpoint(QStringLiteral("   ")).isEmpty());
    QVERIFY(CloudlogUploader::qsoEndpoint(QStringLiteral("/")).isEmpty());
}

void TstCloudlogUploader::success_is_success()
{
    const auto r = CloudlogUploader::parseResponse(
        201, R"({"status":"created","reason":"QSO added"})");
    QVERIFY(r.ok);
    QVERIFY(!r.duplicate);
}

void TstCloudlogUploader::duplicate_counts_as_success()
{
    // Re-sending a contact the log already has is the normal outcome of
    // a retry. Reporting it as a failure would train the operator to
    // ignore upload errors.
    const auto r = CloudlogUploader::parseResponse(
        200, R"({"status":"duplicate","reason":"Duplicate QSO"})");
    QVERIFY(r.ok);
    QVERIFY(r.duplicate);
}

void TstCloudlogUploader::rejected_key_says_so()
{
    for (int code : {401, 403}) {
        const auto r = CloudlogUploader::parseResponse(code, "Forbidden");
        QVERIFY(!r.ok);
        QVERIFY2(r.message.contains(QStringLiteral("API key")),
                 qPrintable(r.message));
    }
}

void TstCloudlogUploader::wrong_address_says_so()
{
    // A 404 here almost always means the instance URL is wrong, not that
    // the contact was bad — saying which saves the operator checking the
    // QSO first.
    const auto r = CloudlogUploader::parseResponse(404, "<html>Not Found");
    QVERIFY(!r.ok);
    QVERIFY2(r.message.contains(QStringLiteral("instance URL")),
             qPrintable(r.message));
}

void TstCloudlogUploader::not_configured_until_all_three_are_set()
{
    CloudlogUploader u;
    QVERIFY(!u.isConfigured());
    u.setBaseUrl(QStringLiteral("https://log.example.org"));
    QVERIFY(!u.isConfigured());
    u.setApiKey(QStringLiteral("abc"));
    QVERIFY(!u.isConfigured());
    // The station profile is required rather than defaulted to 1: an
    // operator with a home and a portable profile would otherwise have
    // every contact filed under whichever came first.
    u.setStationProfileId(QStringLiteral("2"));
    QVERIFY(u.isConfigured());
}

void TstCloudlogUploader::wavelog_created_is_success()
{
    const auto r = CloudlogUploader::parseResponse(201,
        R"({"status":"created","type":"adif","string":"","adif_count":1,"adif_errors":0,"messages":[""]})");
    QVERIFY(r.ok);
    QVERIFY(!r.duplicate);
}

void TstCloudlogUploader::wavelog_duplicate_is_http_400_and_still_done()
{
    // Wavelog answers a contact it already has with 400 "abort". Read
    // as a failure, that contact would be retried at every start, for
    // ever, and the operator told each time that an upload failed.
    const auto r = CloudlogUploader::parseResponse(400,
        R"({"status":"abort","type":"adif","string":"","adif_count":1,"adif_errors":1,)"
        R"("messages":["","Date\/Time: 2026-11-17 14:01:15 Callsign: DL1ABC Band: 20M Duplicate for OE5SOS<br>"]})");
    QVERIFY2(r.ok, qPrintable(r.message));
    QVERIFY(r.duplicate);
}

void TstCloudlogUploader::wavelog_duplicate_on_a_translated_instance()
{
    // __("Duplicate for") is translated by the instance — German says
    // "Doublette zu". The frame around the words is what is fixed.
    const auto r = CloudlogUploader::parseResponse(400,
        R"({"status":"abort","type":"adif","string":"","adif_count":1,"adif_errors":1,)"
        R"("messages":["","Date\/Time: 2026-11-17 14:01:15 Callsign: DL1ABC Band: 20M Doublette zu OE5SOS<br>"]})");
    QVERIFY2(r.ok, qPrintable(r.message));
    QVERIFY(r.duplicate);
    QVERIFY(CloudlogUploader::isDuplicateMessage(QStringLiteral(
        "Date/Time: 2026-11-17 14:01:15 Callsign: DL1ABC Band: 20M Doublette zu OE5SOS")));
    QVERIFY(!CloudlogUploader::isDuplicateMessage(QStringLiteral(
        "Differing locator JN78 while importing QSO with DL1ABC for station locator JN68: SKIPPED")));
}

void TstCloudlogUploader::wavelog_refusal_shows_the_servers_reason()
{
    // The commonest real refusal: Longpath's own locator does not match
    // the station location in Wavelog. "abort" alone tells the operator
    // nothing; the server's sentence does — without its HTML.
    const auto r = CloudlogUploader::parseResponse(400,
        R"({"status":"abort","type":"adif","string":"","adif_count":1,"adif_errors":1,)"
        R"("messages":["","Differing locator <b>JN78<\/b> while importing QSO with DL1ABC for station locator <b>JN68<\/b>: SKIPPED<br>"]})");
    QVERIFY(!r.ok);
    QCOMPARE(r.message, QStringLiteral(
        "Differing locator JN78 while importing QSO with DL1ABC for station locator JN68: SKIPPED"));

    // A duplicate AND a real refusal in one answer is a refusal.
    const auto mixed = CloudlogUploader::parseResponse(400,
        R"({"status":"abort","messages":["","You tried to import a QSO without valid date.<br>)"
        R"(Date\/Time: 2026-11-17 14:01:15 Callsign: DL1ABC Band: 20M Duplicate for OE5SOS<br>"]})");
    QVERIFY(!mixed.ok);
}

void TstCloudlogUploader::wavelog_wrong_station_id_says_so()
{
    // Same 401 as a bad key, different cure. Saying "API key rejected"
    // here sends the operator to the wrong field.
    const auto r = CloudlogUploader::parseResponse(401,
        R"({"status":"failed","reason":"station id does not belong to the API key owner."})");
    QVERIFY(!r.ok);
    QVERIFY2(r.message.contains(QStringLiteral("station profile ID")), qPrintable(r.message));
    QVERIFY2(!r.message.contains(QStringLiteral("rejected")), qPrintable(r.message));

    const auto key = CloudlogUploader::parseResponse(401,
        R"({"status":"failed","reason":"missing or wrong api key"})");
    QCOMPARE(key.message, QStringLiteral("API key rejected"));
}

void TstCloudlogUploader::wavelog_read_only_key_says_so()
{
    const auto r = CloudlogUploader::parseResponse(403,
        R"({"status":"failed","reason":"API key does not have write permissions"})");
    QVERIFY(!r.ok);
    QVERIFY2(r.message.contains(QStringLiteral("read-only")), qPrintable(r.message));
}

void TstCloudlogUploader::wavelog_wrong_json_is_a_failure_despite_http_200()
{
    // Sent with HTTP 200 (no http_response_code in that branch).
    const auto r = CloudlogUploader::parseResponse(200,
        R"({"status":"failed","reason":"wrong JSON"})");
    QVERIFY(!r.ok);
    QCOMPARE(r.message, QStringLiteral("wrong JSON"));

    const auto limited = CloudlogUploader::parseResponse(429,
        R"({"status":"failed","reason":"Rate limit exceeded. Try again in 30 seconds.","retry_after":30})");
    QVERIFY(!limited.ok);
    QVERIFY(limited.message.contains(QStringLiteral("Rate limit")));
}

void TstCloudlogUploader::a_web_page_is_not_an_api()
{
    // A wrong address that serves a page — a login form, a proxy's
    // greeting — answers 200. Counting that as uploaded marks contacts
    // as sent that went nowhere.
    for (const char* body : {"<!DOCTYPE html><html><body>Login</body></html>", "", "OK"}) {
        const auto r = CloudlogUploader::parseResponse(200, body);
        QVERIFY2(!r.ok, body);
        QVERIFY2(r.message.contains(QStringLiteral("instance URL")), qPrintable(r.message));
    }
}

void TstCloudlogUploader::cloudlog_created_is_success()
{
    // Cloudlog proper: 201, imported_count, messages.
    const auto r = CloudlogUploader::parseResponse(201,
        R"({"status":"created","type":"adif","string":"<call:6>DL1ABC<eor>","imported_count":1,"messages":[]})");
    QVERIFY(r.ok);
    QVERIFY(!r.duplicate);

    const auto nothing = CloudlogUploader::parseResponse(201,
        R"({"status":"created","type":"adif","string":"","imported_count":0,"messages":["Station not accessible<br>"]})");
    QVERIFY(!nothing.ok);
    QCOMPARE(nothing.message, QStringLiteral("Station not accessible"));
}

void TstCloudlogUploader::named_for_the_operator()
{
    CloudlogUploader u;
    QVERIFY(u.serviceName().startsWith(QStringLiteral("Wavelog")));
    QCOMPARE(u.shortName(), QStringLiteral("Wavelog"));
    // The waiting list is filed under this; it must not follow the
    // display name.
    QCOMPARE(u.serviceId(), QStringLiteral("cloudlog"));
}

QTEST_MAIN(TstCloudlogUploader)
#include "tst_cloudlog_uploader.moc"
