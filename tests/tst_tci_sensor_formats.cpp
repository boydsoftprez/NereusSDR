// no-port-check: NereusSDR-original TDD test for TciSensorManager wire-format
// helpers and interval aggregation ported from Thetis TCIServer.cs:2314-2332,
// 501-506, 7571-7603 [v2.10.3.13].
//
// tests/tst_tci_sensor_formats.cpp  (NereusSDR)
// NereusSDR-original — unit tests for TciSensorManager static helpers.
//
// Modification history (NereusSDR):
//   2026-05-10 — Phase 3J-1 Task 19.1 by J.J. Boyd (KG4VCF);
//                AI-assisted transformation via Anthropic Claude Code.

#include <QtTest>
#include "core/TciSensorManager.h"

using namespace NereusSDR;

class TestTciSensorFormats : public QObject {
    Q_OBJECT
private slots:
    void formatRxSensors_F1_culture_invariant();
    void formatRxChannelSensors_basic_form();
    void formatRxChannelSensorsEx_5_args_F1();
    void formatTxSensors_F1_swr_not_F2();
    void minimumRequiredInterval_picks_minimum();
};

// From Thetis TCIServer.cs:2314-2316 [v2.10.3.13] — sendRxSensors.
// Verifies F1 formatting with a negative dBm value.
void TestTciSensorFormats::formatRxSensors_F1_culture_invariant()
{
    const QString result = TciSensorManager::formatRxSensors(0, -95.3);
    QCOMPARE(result, QStringLiteral("rx_sensors:0,-95.3;"));
}

// From Thetis TCIServer.cs:2318-2320 [v2.10.3.13] — sendRxChannelSensors
// basic form (first of the dual-emit).
void TestTciSensorFormats::formatRxChannelSensors_basic_form()
{
    const QString result = TciSensorManager::formatRxChannelSensors(0, 0, -95.3);
    QCOMPARE(result, QStringLiteral("rx_channel_sensors:0,0,-95.3;"));
}

// From Thetis TCIServer.cs:2321 [v2.10.3.13] — sendRxChannelSensors extended
// form (second of the dual-emit), with avg and peak-bin fields.
void TestTciSensorFormats::formatRxChannelSensorsEx_5_args_F1()
{
    const QString result = TciSensorManager::formatRxChannelSensorsEx(
        0, 0, -95.3, -97.1, -93.2);
    QCOMPARE(result, QStringLiteral("rx_channel_sensors_ex:0,0,-95.3,-97.1,-93.2;"));
}

// From Thetis TCIServer.cs:2323-2332 [v2.10.3.13] — sendTxSensors.
// Verifies ALL five fields are F1 (1 decimal place).
// Plan correction: SWR is F1 -> "1.1" NOT F2 -> "1.10".
// Thetis source: "{4:F1}" at TCIServer.cs:2326 [v2.10.3.13].
void TestTciSensorFormats::formatTxSensors_F1_swr_not_F2()
{
    const QString result = TciSensorManager::formatTxSensors(0, -30.0, 12.5, 12.5, 1.1);
    // SWR 1.1 NOT 1.10 — Thetis uses F1 for all fields including SWR
    QCOMPARE(result, QStringLiteral("tx_sensors:0,-30.0,12.5,12.5,1.1;"));
}

// Tests TciSensorManager::minimumRequiredInterval — ported from Thetis
// clampIntervalMs + MinimumRequired{Rx,Tx}SensorInterval logic.
// Verifies:
//   - empty list returns default 200 ms
//   - minimum across 3 different intervals is chosen
//   - clamp: value < 30 is bumped to 30
//   - clamp: value > 1000 is capped at 1000
void TestTciSensorFormats::minimumRequiredInterval_picks_minimum()
{
    // Three clients request 100 / 200 / 300 ms -> effective minimum is 100 ms
    const QList<int> threeClients = {100, 200, 300};
    QCOMPARE(TciSensorManager::minimumRequiredInterval(threeClients), 100);

    // Empty list -> default 200 ms
    QCOMPARE(TciSensorManager::minimumRequiredInterval({}), 200);

    // Value below minimum clamp (5 < 30) -> clamped to 30
    const QList<int> tooFast = {5};
    QCOMPARE(TciSensorManager::minimumRequiredInterval(tooFast), 30);

    // Value above maximum clamp (5000 > 1000) -> capped at 1000
    const QList<int> tooSlow = {5000};
    QCOMPARE(TciSensorManager::minimumRequiredInterval(tooSlow), 1000);

    // Mixed: one within range, one above -> picks the in-range value
    const QList<int> mixed = {5000, 150};
    QCOMPARE(TciSensorManager::minimumRequiredInterval(mixed), 150);
}

QTEST_GUILESS_MAIN(TestTciSensorFormats)
#include "tst_tci_sensor_formats.moc"
