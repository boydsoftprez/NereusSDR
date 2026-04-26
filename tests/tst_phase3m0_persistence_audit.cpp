// tests/tst_phase3m0_persistence_audit.cpp
//
// Phase 3M-0 Task 15: AppSettings round-trip audit for all 15 keys
// introduced by Tasks 9-13.
//
// Uses AppSettings(filePath) direct constructor + QTemporaryDir for full
// isolation from the sandbox singleton. No Thetis port — NereusSDR-internal
// correctness check.
//
// Key coverage:
//   Task 9  — SwrProtectionEnabled, SwrProtectionLimit, SwrTuneProtectionEnabled,
//              TunePowerSwrIgnore, WindBackPowerSwr                        (5 keys)
//   Task 10 — TxInhibitMonitorEnabled, TxInhibitMonitorReversed           (2 keys)
//   Task 11 — AlexAnt2RxOnly, AlexAnt3RxOnly, DisableHfPa                 (3 keys)
//   Task 12 — Region, ExtendedTxAllowed, RxOnly, NetworkWatchdogEnabled   (4 keys)
//   Task 13 — PreventTxOnDifferentBandToRx                                (1 key)
//                                                              Total:      15 keys
//
// NereusSDR-original (not a Thetis port). No Thetis license header required.

#include <QtTest/QtTest>
#include <QTemporaryDir>

#include "core/AppSettings.h"

using namespace NereusSDR;

class TestPhase3m0Persistence : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_tempDir;

    // Build a fresh AppSettings backed by a temp file.
    // No load() call — store starts empty.
    AppSettings makeSettings() const
    {
        return AppSettings(m_tempDir.path() + QStringLiteral("/NereusSDR.settings"));
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_tempDir.isValid());
    }

    void allKeysRoundTrip()
    {
        const QString path = m_tempDir.path() + QStringLiteral("/NereusSDR.settings");

        // --- Write phase ---
        {
            AppSettings s(path);

            // Task 9: SWR protection
            s.setValue(QStringLiteral("SwrProtectionEnabled"),     QStringLiteral("True"));
            s.setValue(QStringLiteral("SwrProtectionLimit"),       QStringLiteral("2.5"));
            s.setValue(QStringLiteral("SwrTuneProtectionEnabled"), QStringLiteral("True"));
            s.setValue(QStringLiteral("TunePowerSwrIgnore"),       QStringLiteral("42"));
            s.setValue(QStringLiteral("WindBackPowerSwr"),         QStringLiteral("True"));

            // Task 10: External TX Inhibit
            s.setValue(QStringLiteral("TxInhibitMonitorEnabled"),  QStringLiteral("True"));
            s.setValue(QStringLiteral("TxInhibitMonitorReversed"), QStringLiteral("False"));

            // Task 11: Block-TX antennas + Disable HF PA
            s.setValue(QStringLiteral("AlexAnt2RxOnly"),           QStringLiteral("True"));
            s.setValue(QStringLiteral("AlexAnt3RxOnly"),           QStringLiteral("False"));
            s.setValue(QStringLiteral("DisableHfPa"),              QStringLiteral("True"));

            // Task 12: Hardware Configuration (General setup)
            s.setValue(QStringLiteral("Region"),                   QStringLiteral("Japan"));
            s.setValue(QStringLiteral("ExtendedTxAllowed"),        QStringLiteral("True"));
            s.setValue(QStringLiteral("RxOnly"),                   QStringLiteral("False"));
            s.setValue(QStringLiteral("NetworkWatchdogEnabled"),   QStringLiteral("True"));

            // Task 13: General Options
            s.setValue(QStringLiteral("PreventTxOnDifferentBandToRx"), QStringLiteral("True"));

            s.save();
        }

        // --- Read phase (fresh instance, load from disk) ---
        {
            AppSettings s(path);
            s.load();

            // Task 9
            QCOMPARE(s.value(QStringLiteral("SwrProtectionEnabled")).toString(),
                     QStringLiteral("True"));
            QCOMPARE(s.value(QStringLiteral("SwrProtectionLimit")).toString(),
                     QStringLiteral("2.5"));
            QCOMPARE(s.value(QStringLiteral("SwrTuneProtectionEnabled")).toString(),
                     QStringLiteral("True"));
            QCOMPARE(s.value(QStringLiteral("TunePowerSwrIgnore")).toString(),
                     QStringLiteral("42"));
            QCOMPARE(s.value(QStringLiteral("WindBackPowerSwr")).toString(),
                     QStringLiteral("True"));

            // Task 10
            QCOMPARE(s.value(QStringLiteral("TxInhibitMonitorEnabled")).toString(),
                     QStringLiteral("True"));
            QCOMPARE(s.value(QStringLiteral("TxInhibitMonitorReversed")).toString(),
                     QStringLiteral("False"));

            // Task 11
            QCOMPARE(s.value(QStringLiteral("AlexAnt2RxOnly")).toString(),
                     QStringLiteral("True"));
            QCOMPARE(s.value(QStringLiteral("AlexAnt3RxOnly")).toString(),
                     QStringLiteral("False"));
            QCOMPARE(s.value(QStringLiteral("DisableHfPa")).toString(),
                     QStringLiteral("True"));

            // Task 12
            QCOMPARE(s.value(QStringLiteral("Region")).toString(),
                     QStringLiteral("Japan"));
            QCOMPARE(s.value(QStringLiteral("ExtendedTxAllowed")).toString(),
                     QStringLiteral("True"));
            QCOMPARE(s.value(QStringLiteral("RxOnly")).toString(),
                     QStringLiteral("False"));
            QCOMPARE(s.value(QStringLiteral("NetworkWatchdogEnabled")).toString(),
                     QStringLiteral("True"));

            // Task 13
            QCOMPARE(s.value(QStringLiteral("PreventTxOnDifferentBandToRx")).toString(),
                     QStringLiteral("True"));
        }
    }
};

QTEST_GUILESS_MAIN(TestPhase3m0Persistence)
#include "tst_phase3m0_persistence_audit.moc"
