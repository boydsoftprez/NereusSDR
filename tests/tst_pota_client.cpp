// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - PotaClient JSON parser + dedup tests
//
// Phase 3J-2 Task B2. Pins the contract that PotaClient parses
// api.pota.app/spot/activator JSON responses and dedups across
// consecutive polls via spotId.

#include <QtTest>

#include "core/PotaClient.h"
#include "core/DxSpot.h"

using namespace NereusSDR;

class TestPotaClient : public QObject {
    Q_OBJECT
private slots:
    void parsesPotaJsonResponse();
    void dedupsAcrossPolls();
    void rejectsMalformedJson();
};

void TestPotaClient::parsesPotaJsonResponse() {
    QByteArray json = R"([{
        "spotId": 12345,
        "activator": "K3POTA",
        "frequency": "14260.0",
        "mode": "SSB",
        "reference": "K-1234",
        "name": "Acadia NP",
        "spotTime": "2026-05-10T17:08:00Z",
        "spotter": "W1AW",
        "comments": "POTA K-1234"
    }])";

    PotaClient client;
    auto spots = client.parseJsonForTest(json);
    QCOMPARE(spots.size(), 1);
    QCOMPARE(spots[0].dxCall, QStringLiteral("K3POTA"));
    QCOMPARE(spots[0].freqMhz, 14.260);
    QCOMPARE(spots[0].source, QStringLiteral("POTA"));
}

void TestPotaClient::dedupsAcrossPolls() {
    PotaClient client;
    QByteArray json1 = R"([{"spotId":1,"activator":"A","frequency":"7000","mode":"SSB","spotTime":"2026-05-10T17:00:00Z","spotter":"S","comments":""}])";
    QByteArray json2 = R"([{"spotId":1,"activator":"A","frequency":"7000","mode":"SSB","spotTime":"2026-05-10T17:00:00Z","spotter":"S","comments":""},
                          {"spotId":2,"activator":"B","frequency":"7100","mode":"SSB","spotTime":"2026-05-10T17:01:00Z","spotter":"S","comments":""}])";
    QCOMPARE(client.parseJsonForTest(json1).size(), 1);
    QCOMPARE(client.parseJsonForTest(json2).size(), 1);  // only spotId=2 is new (spotId=1 already seen)
}

void TestPotaClient::rejectsMalformedJson() {
    PotaClient client;
    QCOMPARE(client.parseJsonForTest(QByteArray("not json")).size(), 0);
    QCOMPARE(client.parseJsonForTest(QByteArray("")).size(), 0);
    QCOMPARE(client.parseJsonForTest(QByteArray("{\"not\":\"array\"}")).size(), 0);
}

QTEST_GUILESS_MAIN(TestPotaClient)
#include "tst_pota_client.moc"
