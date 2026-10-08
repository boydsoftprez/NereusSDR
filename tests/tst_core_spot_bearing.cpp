// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - cty.dat on the headless Core, and each spot's bearing (rotor
// control plan, Task 4a).
//
// Linked to NereusCore alone, the way nereusd is: the Core's own :/cty.dat
// resource loads and places a callsign, it is parsed once however many
// callers ask, the Core's start (enableStationAccessoryIdentity) loads it
// so its rotor turns to a call, and a spot record's bearingDeg is the
// short-path bearing GreatCircle gives from the station's grid square, or
// -1 with no grid square, one that cannot be read, or a call cty.dat cannot
// place. No test opens a real serial port or turns a rotor.
//
// Modification history (NereusSDR):
//   2026-10-08: Initial version. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.

#include <QtTest>

#include "core/AppSettings.h"
#include "core/DxccColorProvider.h"
#include "core/GreatCircle.h"
#include "core/RotorConnection.h"
#include "core/SpotSourceHost.h"
#include "core/StationRotorController.h"
#include "models/RadioModel.h"
#include "models/SpotModel.h"

#include <QJsonObject>
#include <QLoggingCategory>
#include <QPointer>
#include <QTimeZone>

#include <cmath>
#include <memory>

using namespace NereusSDR;

namespace {

const QString kCall = QStringLiteral("JA1ABC");
const QString kGrid = QStringLiteral("FN31pr");
// A prefix no country holds (Q is not allocated), so cty.dat places nothing.
const QString kUnplaceableCall = QStringLiteral("QQ1ABC");

double oneDecimal(double deg)
{
    const double r = std::round(deg * 10.0) / 10.0;
    return r >= 360.0 ? 0.0 : r;
}

SpotData spotFor(const QString& call)
{
    SpotData spot;
    spot.index = 3;
    spot.callsign = call;
    spot.rxFreqMhz = 14.025;
    spot.mode = QStringLiteral("CW");
    spot.source = QStringLiteral("Cluster");
    spot.timestamp = QDateTime(QDate(2026, 10, 8), QTime(12, 0), QTimeZone::utc());
    return spot;
}

class FakeTransport : public RotorTransport {
public:
    QByteArray pending;
    void open() override { emit opened(); }
    void close() override {}
    qint64 write(const QByteArray& bytes) override { return bytes.size(); }
    QByteArray readAll() override
    {
        QByteArray out = pending;
        pending.clear();
        return out;
    }
    void feed(const QByteArray& bytes)
    {
        pending += bytes;
        emit readyRead();
    }
};

RotorConnection::Timing steppedTiming()
{
    RotorConnection::Timing t;
    t.pollStillMs = 3600000;
    t.pollTurningMs = 3599000;
    t.replyTimeoutMs = 3600000;
    t.settleMs = 3600000;
    t.staleMs = 3600000;
    t.answerDeadlineMs = 3600000;
    t.reconnectUnitMs = 3600000;
    t.rotctldStartDelayMs = 0;
    return t;
}

} // namespace

class TestCoreSpotBearing : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // The Core's start (RadioModel) logs its own progress; only
        // warnings matter here.
        QLoggingCategory::setFilterRules(QStringLiteral("*.debug=false\n*.info=false"));
    }

    void init() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    // nereusd links NereusCore alone: the Core's own resource is there.
    void theCoresOwnCtyDatPlacesACallsign()
    {
        DxccColorProvider dxcc;
        QVERIFY(!dxcc.positionForCallsign(kCall).has_value());
        QVERIFY(dxcc.ensureCtyDatLoaded());
        QVERIFY(QFile::exists(QStringLiteral(":/cty.dat")));
        const std::optional<GeoPosition> at = dxcc.positionForCallsign(kCall);
        QVERIFY(at.has_value());
        QVERIFY(at->latitudeDeg > 30.0 && at->latitudeDeg < 45.0);    // Japan
        QVERIFY(at->longitudeDeg > 130.0 && at->longitudeDeg < 145.0);
        QVERIFY(!dxcc.positionForCallsign(kUnplaceableCall).has_value());
    }

    // A second caller (the window's start after the Core's) parses nothing:
    // loading from a path that does not exist would fail, so success shows
    // the loaded table was kept.
    void aSecondLoadKeepsTheOneTable()
    {
        DxccColorProvider dxcc;
        QVERIFY(dxcc.ensureCtyDatLoaded());
        QVERIFY(!dxcc.loadCtyDat(QStringLiteral(":/no-such-cty.dat")));
        QVERIFY(dxcc.ensureCtyDatLoaded());
        QVERIFY(dxcc.positionForCallsign(kCall).has_value());
    }

    void aSpotCarriesTheGreatCircleBearing()
    {
        DxccColorProvider dxcc;
        QVERIFY(dxcc.ensureCtyDatLoaded());
        const std::optional<GeoPosition> at = dxcc.positionForCallsign(kCall);
        QVERIFY(at.has_value());
        const std::optional<double> shortPath = GreatCircle::bearingFromGrid(kGrid, *at);
        QVERIFY(shortPath.has_value());

        const QJsonObject f = SpotSourceHost::spotRecordFields(spotFor(kCall), &dxcc, kGrid);
        QVERIFY(f.contains(QStringLiteral("bearingDeg")));
        const double served = f.value(QStringLiteral("bearingDeg")).toDouble();
        QCOMPARE(served, oneDecimal(*shortPath));
        QVERIFY(std::abs(served - *shortPath) <= 0.05);
        // Connecticut to Japan is north-west over the pole.
        QVERIFY(served > 320.0 && served < 345.0);
        // A lower-case call places the same.
        QCOMPARE(SpotSourceHost::spotRecordFields(spotFor(kCall.toLower()), &dxcc, kGrid)
                     .value(QStringLiteral("bearingDeg")).toDouble(),
                 served);
    }

    // The two-argument form reads the station's grid square where FreeDV
    // Reporter does: FreeDvReporter/GridSquare, else User/GridSquare.
    void aSpotUsesTheStationsGridSquare()
    {
        DxccColorProvider dxcc;
        QVERIFY(dxcc.ensureCtyDatLoaded());
        const double expected = oneDecimal(
            *GreatCircle::bearingFromGrid(kGrid, *dxcc.positionForCallsign(kCall)));
        AppSettings::instance().setValue(QStringLiteral("User/GridSquare"), kGrid);
        QCOMPARE(SpotSourceHost::spotRecordFields(spotFor(kCall), &dxcc)
                     .value(QStringLiteral("bearingDeg")).toDouble(),
                 expected);
        AppSettings::instance().setValue(QStringLiteral("FreeDvReporter/GridSquare"),
                                         QStringLiteral("JO62"));
        const double fromBerlin = oneDecimal(*GreatCircle::bearingFromGrid(
            QStringLiteral("JO62"), *dxcc.positionForCallsign(kCall)));
        QVERIFY(fromBerlin != expected);
        QCOMPARE(SpotSourceHost::spotRecordFields(spotFor(kCall), &dxcc)
                     .value(QStringLiteral("bearingDeg")).toDouble(),
                 fromBerlin);
    }

    void noBearingIsMinusOneNeverNorth_data()
    {
        QTest::addColumn<QString>("call");
        QTest::addColumn<QString>("grid");
        QTest::addColumn<bool>("withTable");
        QTest::newRow("no grid square") << kCall << QString() << true;
        QTest::newRow("blank grid square") << kCall << QStringLiteral("   ") << true;
        QTest::newRow("unreadable grid square") << kCall << QStringLiteral("ZZ99") << true;
        QTest::newRow("odd-length grid square") << kCall << QStringLiteral("FN3") << true;
        QTest::newRow("call cty.dat cannot place") << kUnplaceableCall << kGrid << true;
        QTest::newRow("no callsign") << QString() << kGrid << true;
        QTest::newRow("no cty.dat loaded") << kCall << kGrid << false;
    }

    void noBearingIsMinusOneNeverNorth()
    {
        QFETCH(QString, call);
        QFETCH(QString, grid);
        QFETCH(bool, withTable);
        DxccColorProvider dxcc;
        if (withTable) {
            QVERIFY(dxcc.ensureCtyDatLoaded());
        }
        const QJsonObject f = SpotSourceHost::spotRecordFields(spotFor(call), &dxcc, grid);
        QCOMPARE(f.value(QStringLiteral("bearingDeg")).toDouble(0.0), -1.0);
    }

    void noProviderIsMinusOne()
    {
        const QJsonObject f = SpotSourceHost::spotRecordFields(spotFor(kCall), nullptr, kGrid);
        QCOMPARE(f.value(QStringLiteral("bearingDeg")).toDouble(0.0), -1.0);
    }

    // A remote window keeps the Core's bearing in its spot model; a spot
    // without one stays -1.
    void theSpotModelCarriesTheBearing()
    {
        SpotModel model;
        model.applySpotStatus(1, {{QStringLiteral("callsign"), kCall},
                                  {QStringLiteral("bearing_deg"), QStringLiteral("333.4")}});
        QCOMPARE(model.spots().value(1).bearingDeg, 333.4);
        model.applySpotStatus(2, {{QStringLiteral("callsign"), kCall}});
        QCOMPARE(model.spots().value(2).bearingDeg, -1.0);
        model.applySpotStatus(3, {{QStringLiteral("bearing_deg"), QStringLiteral("nan")}});
        QCOMPARE(model.spots().value(3).bearingDeg, -1.0);
    }

    // The headless Core's start loads cty.dat, so its rotor turns to a call
    // (before Task 4a it refused every call as not placed).
    void theHeadlessCoresRotorTurnsToACall()
    {
        RadioModel radio;
        QVERIFY(!radio.dxccColorProvider()->positionForCallsign(kCall).has_value());
        radio.enableStationAccessoryIdentity();
        DxccColorProvider* dxcc = radio.dxccColorProvider();
        QVERIFY(dxcc->positionForCallsign(kCall).has_value());

        StationRotorController* ctl = radio.stationRotorController();
        QVERIFY(ctl != nullptr);
        const QString port = QStringLiteral("/dev/ttyUSB0");
        ctl->setSerialPortListerForTesting([port] { return QStringList{port}; });
        QPointer<FakeTransport> fake;
        ctl->connection()->setTransportFactoryForTesting([&fake](const RotorTransportTarget&) {
            auto t = std::make_unique<FakeTransport>();
            fake = t.get();
            return std::unique_ptr<RotorTransport>(std::move(t));
        });
        ctl->connection()->setTimingForTesting(steppedTiming());
        RotorConfig c;
        c.driver = RotorDriver::Gs232b;
        c.serialPort = port;
        QString why;
        QVERIFY2(ctl->configureRotor(c, &why), qPrintable(why));
        QVERIFY(fake);
        fake->feed("AZ=090  EL=000\r\n");

        AppSettings::instance().setValue(QStringLiteral("User/GridSquare"), kGrid);
        QVERIFY2(ctl->turnRotorToCall(kCall, false, &why), qPrintable(why));
        const double shortPath =
            *GreatCircle::bearingFromGrid(kGrid, *dxcc->positionForCallsign(kCall));
        QVERIFY(std::abs(ctl->targetAzimuthDeg() - shortPath) < 1e-9);

        // The spot stream the Core serves reads the same table.
        QCOMPARE(SpotSourceHost::spotRecordFields(spotFor(kCall), dxcc)
                     .value(QStringLiteral("bearingDeg")).toDouble(),
                 oneDecimal(shortPath));
    }
};

QTEST_MAIN(TestCoreSpotBearing)
#include "tst_core_spot_bearing.moc"
