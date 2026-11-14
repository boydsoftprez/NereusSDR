// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// tests/tst_roundtable_hub.cpp  (Longpath)
// =================================================================
//
// RoundtableHub: was aus den Scheiben gemeldet wird, und was aus der
// Raumliste als Spot im SpotModel landet. Ohne Server und ohne Netz —
// der Weg zum Server ist tst_roundtable_client, der ganze Weg gegen den
// echten Referenzserver tst_roundtable_e2e.
//
// Modification history (Longpath):
//   2026-11-14 - Angelegt fuer Longpath von Martin Fischer (OE5SOS),
//                KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include "core/AppSettings.h"
#include "core/roundtable/RoundtableClient.h"
#include "models/RoundtableHub.h"
#include "models/SliceModel.h"
#include "models/SpotModel.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

using namespace Longpath;

namespace {

QJsonObject station(const QString& room, const QString& ref, const QString& call, qint64 hz,
                    bool keyed = false)
{
    return QJsonObject{{QStringLiteral("ref"), ref},          {QStringLiteral("sourceRoom"), room},
                       {QStringLiteral("slice"), 0},          {QStringLiteral("hz"), hz},
                       {QStringLiteral("family"), QStringLiteral("usb")},
                       {QStringLiteral("callsign"), call},    {QStringLiteral("nth"), 1},
                       {QStringLiteral("grid"), QJsonValue()}, {QStringLiteral("keyed"), keyed},
                       {QStringLiteral("away"), false},       {QStringLiteral("joining"), false}};
}

QJsonObject room(const QString& id, qint64 hz, const QJsonArray& stations)
{
    return QJsonObject{{QStringLiteral("id"), id},
                       {QStringLiteral("band"), QStringLiteral("20m")},
                       {QStringLiteral("family"), QStringLiteral("usb")},
                       {QStringLiteral("hz"), hz},
                       {QStringLiteral("stations"), stations}};
}

QList<SpotData> roundtableSpots(const SpotModel& model)
{
    QList<SpotData> out;
    for (const SpotData& s : model.spots()) {
        if (s.source == QLatin1String("Roundtable")) {
            out.append(s);
        }
    }
    return out;
}

} // namespace

class TstRoundtableHub : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // Nie in die echten Einstellungen schreiben.
        QStandardPaths::setTestModeEnabled(true);
    }

    void init()
    {
        auto& s = AppSettings::instance();
        for (const char* k : {"Roundtable/AutoStart", "Roundtable/FirstRunAnswer", "Roundtable/Hidden",
                              "Roundtable/Callsign", "Roundtable/GridSquare", "Roundtable/ServerUrl"}) {
            s.remove(QLatin1String(k));
        }
        s.setValue(QStringLiteral("Roundtable/Callsign"), QStringLiteral("OE5SOS"));
    }

    void betriebsartenWieUpstream()
    {
        QCOMPARE(RoundtableHub::modeOf(DSPMode::USB), QStringLiteral("USB"));
        QCOMPARE(RoundtableHub::modeOf(DSPMode::CWL), QStringLiteral("CWL"));
        QCOMPARE(RoundtableHub::modeOf(DSPMode::SAM), QStringLiteral("SAM"));
        QCOMPARE(RoundtableHub::modeOf(DSPMode::RADE_U), QStringLiteral("FDVU"));
        QCOMPARE(RoundtableHub::modeOf(DSPMode::RADE_L), QStringLiteral("FDVL"));
        QVERIFY(RoundtableHub::modeOf(DSPMode::DSB).isEmpty());
        QVERIFY(RoundtableHub::modeOf(DSPMode::SPEC).isEmpty());
        QVERIFY(RoundtableHub::modeOf(DSPMode::DRM).isEmpty());
    }

    void aktiveZuerstFremdeWegSendendeMarkiert()
    {
        SliceModel a(0);
        a.setSliceIndex(0);
        a.setFrequency(14212000.0);
        a.setDspMode(DSPMode::USB);
        a.setFilter(100, 2900);
        SliceModel b(1);
        b.setSliceIndex(1);
        b.setFrequency(7088000.0);
        b.setDspMode(DSPMode::LSB);
        b.setFilter(-2800, -200);
        SliceModel kiwi(2);
        kiwi.setSliceIndex(2);
        kiwi.setFrequency(3700000.0);
        kiwi.setSuppressAutoStreamBinding(true);

        const QList<RoundtableSliceState> states =
            RoundtableHub::sliceStates({&a, &b, &kiwi}, &b, &b);
        QCOMPARE(states.size(), 2);           // der KiwiSDR ist nicht diese Station
        QCOMPARE(states.at(0).id, 1);         // die aktive zuerst
        QCOMPARE(states.at(0).hz, qint64(7088000));
        QCOMPARE(states.at(0).mode, QStringLiteral("LSB"));
        QCOMPARE(states.at(0).filterHz, 2600);
        QVERIFY(states.at(0).keyed);
        QCOMPARE(states.at(1).id, 0);
        QCOMPARE(states.at(1).filterHz, 2800);
        QVERIFY(!states.at(1).keyed);
    }

    void andereWerdenSpotsDieEigeneNicht()
    {
        RoundtableHub hub;
        SpotModel spots;
        hub.attachSpots(&spots);

        hub.feedRoomsForTest(QJsonObject{
            {QStringLiteral("type"), QStringLiteral("rooms")},
            {QStringLiteral("gen"), 1},
            {QStringLiteral("rooms"),
             QJsonArray{room(QStringLiteral("usb-14212000"), 14212000,
                             QJsonArray{station(QStringLiteral("usb-14212000"), QStringLiteral("aaaaaaaa"),
                                                QStringLiteral("K4ABC"), 14212000, true),
                                        station(QStringLiteral("usb-14212000"), QStringLiteral("bbbbbbbb"),
                                                QStringLiteral("OE5SOS/P"), 14212150)})}}});
        QCOMPARE(hub.stations().size(), 1);
        QList<SpotData> list = roundtableSpots(spots);
        QCOMPARE(list.size(), 1);
        QCOMPARE(list.first().callsign, QStringLiteral("K4ABC"));
        QCOMPARE(list.first().rxFreqMhz, 14.212);
        QCOMPARE(list.first().mode, QStringLiteral("USB"));
        QVERIFY(list.first().comment.contains(QStringLiteral("on the air")));

        // Eine zweite Station kommt, die erste geht.
        hub.feedRoomsDeltaForTest(QJsonObject{
            {QStringLiteral("type"), QStringLiteral("roomsDelta")},
            {QStringLiteral("gen"), 2},
            {QStringLiteral("add"),
             QJsonArray{room(QStringLiteral("lsb-7188000"), 7188000,
                             QJsonArray{station(QStringLiteral("lsb-7188000"), QStringLiteral("cccccccc"),
                                                QStringLiteral("DL1XYZ"), 7188000)})}},
            {QStringLiteral("change"), QJsonArray{}},
            {QStringLiteral("remove"), QJsonArray{QStringLiteral("usb-14212000")}}});
        list = roundtableSpots(spots);
        QCOMPARE(list.size(), 1);
        QCOMPARE(list.first().callsign, QStringLiteral("DL1XYZ"));
    }

    void eineClusterMeldungBleibtStehen()
    {
        // Cluster und Roundtable melden denselben auf demselben kHz: das
        // SpotModel legt beide auf einen Index. Geht er bei Roundtable,
        // darf die Cluster-Meldung nicht mit verschwinden.
        RoundtableHub hub;
        SpotModel spots;
        hub.attachSpots(&spots);
        hub.feedRoomsForTest(QJsonObject{
            {QStringLiteral("type"), QStringLiteral("rooms")},
            {QStringLiteral("gen"), 1},
            {QStringLiteral("rooms"),
             QJsonArray{room(QStringLiteral("usb-14212000"), 14212000,
                             QJsonArray{station(QStringLiteral("usb-14212000"), QStringLiteral("aaaaaaaa"),
                                                QStringLiteral("K4ABC"), 14212000)})}}});
        const int idx = spots.dedupIndexFor(QStringLiteral("K4ABC"), 14.212);
        spots.applySpotStatus(idx, {{QStringLiteral("callsign"), QStringLiteral("K4ABC")},
                                    {QStringLiteral("rx_freq"), QStringLiteral("14.2120")},
                                    {QStringLiteral("source"), QStringLiteral("Cluster")}});
        hub.feedRoomsForTest(QJsonObject{{QStringLiteral("type"), QStringLiteral("rooms")},
                                         {QStringLiteral("gen"), 1},
                                         {QStringLiteral("rooms"), QJsonArray{}}});
        QVERIFY(hub.stations().isEmpty());
        QVERIFY(spots.spots().contains(idx));
        QCOMPARE(spots.spots().value(idx).source, QStringLiteral("Cluster"));
    }

    void vorDerAntwortGehtNichtsAuf()
    {
        QTemporaryDir dir;
        RoundtableHub hub;
        hub.setKeyPathForTest(dir.filePath(QStringLiteral("roundtable-identity.pem")));
        // Ein Server, den es nicht gibt, auf diesem Rechner: selbst wenn
        // der Client wollte, ginge nichts hinaus.
        AppSettings::instance().setValue(QStringLiteral("Roundtable/ServerUrl"),
                                         QStringLiteral("ws://127.0.0.1:9/v1/station"));
        hub.reloadSettings();
        QVERIFY(hub.enabled());
        QVERIFY(!hub.answered());
        QCOMPARE(hub.client()->linkState(), RoundtableClient::LinkState::Off);
        QVERIFY(hub.statusText().contains(QStringLiteral("nothing is sent")));
        // Kein Schluessel ohne Verbindung.
        QVERIFY(!QFile::exists(dir.filePath(QStringLiteral("roundtable-identity.pem"))));
    }
};

QTEST_GUILESS_MAIN(TstRoundtableHub)
#include "tst_roundtable_hub.moc"
