#include <QtTest/QtTest>
#include "core/Rf2ksConnection.h"

using namespace NereusSDR;

class Rf2ksConnectionParseTest : public QObject {
    Q_OBJECT
private slots:
    void parsesInfo();
    void parsesPower();
    void parsesTuner();
    void parsesAntennas();
    void parsesActiveAntenna();
    void parsesOperateMode();
    void parsesOperationalInterface();
    void parsesData();
    void operationalInterfaceCapturesErrorField();
};

void Rf2ksConnectionParseTest::parsesInfo() {
    Rf2ksConnection conn;
    conn.injectJsonForTesting(
        "/info",
        R"({"device":"RF2K-S","software_version":{"GUI":200,"controller":267},"custom_device_name":"KG4VCF"})");
    QCOMPARE(conn.deviceName(),       QString("KG4VCF"));     // custom_device_name
    QCOMPARE(conn.softwareVersion(),  QString("G200C267"));   // GUI/controller composed
}

void Rf2ksConnectionParseTest::parsesPower() {
    Rf2ksConnection conn;
    QSignalSpy spy(&conn, &Rf2ksConnection::powerUpdated);
    conn.injectJsonForTesting(
        "/power",
        R"({"temperature":{"value":27.0,"unit":"°C"},"voltage":{"value":52.7,"unit":"V"},"current":{"value":0.0,"unit":"A"},"forward":{"value":850,"max_value":1200,"unit":"W"},"reflected":{"value":3,"max_value":20,"unit":"W"},"swr":{"value":1.4,"max_value":2.1,"unit":""}})");
    QCOMPARE(spy.count(), 1);
    const auto snap = qvariant_cast<RfKitPowerSnapshot>(spy.takeFirst().at(0));
    QCOMPARE(snap.forwardW,      850);
    QCOMPARE(snap.forwardMaxW,   1200);
    QCOMPARE(snap.reflectedW,    3);
    QCOMPARE(snap.swr,           1.4f);
    QCOMPARE(snap.temperatureC,  27.0f);
    QCOMPARE(snap.voltageV,      52.7f);
    QCOMPARE(snap.currentA,      0.0f);
}

void Rf2ksConnectionParseTest::parsesTuner() {
    Rf2ksConnection conn;
    QSignalSpy spy(&conn, &Rf2ksConnection::tunerUpdated);
    conn.injectJsonForTesting(
        "/tuner",
        R"({"mode":"AUTO","setup":"LC","L":{"value":1200,"unit":"nH"},"C":{"value":345,"unit":"pF"},"tuned_frequency":{"value":3891,"unit":"kHz"},"segment_size":{"value":9,"unit":"kHz"}})");
    QCOMPARE(spy.count(), 1);
    const auto snap = qvariant_cast<RfKitTunerSnapshot>(spy.takeFirst().at(0));
    QCOMPARE(snap.mode,                RfKitTunerSnapshot::Mode::Auto);
    QCOMPARE(snap.setup,               QString("LC"));
    QCOMPARE(snap.lValuenH,            1200);
    QCOMPARE(snap.cValuepF,            345);
    QCOMPARE(snap.tunedFrequencyKHz,   3891);
    QCOMPARE(snap.segmentSizeKHz,      9);
}

void Rf2ksConnectionParseTest::parsesAntennas() {
    Rf2ksConnection conn;
    QSignalSpy spy(&conn, &Rf2ksConnection::antennasUpdated);
    conn.injectJsonForTesting(
        "/antennas",
        R"({"antennas":[{"type":"INTERNAL","number":1,"state":"ACTIVE"},{"type":"INTERNAL","number":2,"state":"AVAILABLE"},{"type":"INTERNAL","number":3,"state":"AVAILABLE"},{"type":"INTERNAL","number":4,"state":"AVAILABLE"},{"type":"EXTERNAL","state":"AVAILABLE"}]})");
    QCOMPARE(spy.count(), 1);
    const auto list = qvariant_cast<QList<RfKitAntenna>>(spy.takeFirst().at(0));
    QCOMPARE(list.size(),               5);
    QCOMPARE(list[0].type,              RfKitAntenna::Type::Internal);
    QCOMPARE(list[0].number,            1);
    QCOMPARE(list[0].state,             RfKitAntenna::State::Active);
    QCOMPARE(list[1].state,             RfKitAntenna::State::Available);
    QCOMPARE(list[4].type,              RfKitAntenna::Type::External);
}

void Rf2ksConnectionParseTest::parsesActiveAntenna() {
    Rf2ksConnection conn;
    QSignalSpy spy(&conn, &Rf2ksConnection::activeAntennaUpdated);
    conn.injectJsonForTesting(
        "/antennas/active",
        R"({"type":"INTERNAL","number":2})");
    QCOMPARE(spy.count(), 1);
    const auto a = qvariant_cast<RfKitAntenna>(spy.takeFirst().at(0));
    QCOMPARE(a.type,    RfKitAntenna::Type::Internal);
    QCOMPARE(a.number,  2);
}

void Rf2ksConnectionParseTest::parsesOperateMode() {
    Rf2ksConnection conn;
    QSignalSpy spy(&conn, &Rf2ksConnection::operateModeUpdated);
    conn.injectJsonForTesting("/operate-mode", R"({"operate_mode":"OPERATE"})");
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toString(), QString("OPERATE"));
}

void Rf2ksConnectionParseTest::parsesOperationalInterface() {
    Rf2ksConnection conn;
    QSignalSpy spy(&conn, &Rf2ksConnection::operationalInterfaceUpdated);
    conn.injectJsonForTesting(
        "/operational-interface",
        R"({"operational_interface":"TCI"})");
    QCOMPARE(spy.count(), 1);
    const auto args = spy.takeFirst();
    QCOMPARE(args.at(0).toString(), QString("TCI"));
    QCOMPARE(args.at(1).toString(), QString());  // no error field
}

void Rf2ksConnectionParseTest::parsesData() {
    Rf2ksConnection conn;
    QSignalSpy spy(&conn, &Rf2ksConnection::dataUpdated);
    conn.injectJsonForTesting(
        "/data",
        R"({"band":{"value":80,"unit":"m"},"frequency":{"value":3895.0,"unit":"kHz"},"status":""})");
    QCOMPARE(spy.count(), 1);
    const auto args = spy.takeFirst();
    QCOMPARE(args.at(0).toInt(),    80);
    QCOMPARE(args.at(1).toInt(),    3895);
    QCOMPARE(args.at(2).toString(), QString());
}

void Rf2ksConnectionParseTest::operationalInterfaceCapturesErrorField() {
    Rf2ksConnection conn;
    QSignalSpy spy(&conn, &Rf2ksConnection::operationalInterfaceUpdated);
    conn.injectJsonForTesting(
        "/operational-interface",
        R"({"operational_interface":"UNIV","error":"No TCI available"})");
    const auto args = spy.takeFirst();
    QCOMPARE(args.at(0).toString(), QString("UNIV"));
    QCOMPARE(args.at(1).toString(), QString("No TCI available"));
}

QTEST_MAIN(Rf2ksConnectionParseTest)
#include "tst_rf2ks_connection_parse.moc"
