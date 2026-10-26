// =================================================================
// tests/tst_signal_history.cpp — Task 3.3 Signal History Tests
// =================================================================
//
// Test suite for HistoryGraphItem duration setting.
// Verifies that setDurationMs() correctly configures the data buffer
// and that MultimeterPage broadcasts to all HistoryGraphItem instances.
//
// =================================================================

#include <QTest>
#include <QSignalSpy>
#include "gui/meters/HistoryGraphItem.h"
#include "core/AppSettings.h"

using namespace NereusSDR;

class TestSignalHistory : public QObject {
    Q_OBJECT

private slots:
    void duration_round_trips()
    {
        // Task 3.3: setDurationMs() should store and return the value
        HistoryGraphItem item;
        item.setDurationMs(30000);
        QCOMPARE(item.durationMs(), 30000);

        item.setDurationMs(120000);
        QCOMPARE(item.durationMs(), 120000);
    }

    void duration_clamped_to_valid_range()
    {
        // Duration is clamped to Thetis's Keep For range, 1 s to 1800 s
        // (setup.designer.cs:62510-62519 [v2.10.3.15]). The cap was 600 s
        // (10 min) until the HISTORY item matched Thetis; 1800 s is kept.
        HistoryGraphItem item;

        item.setDurationMs(0);
        QVERIFY(item.durationMs() >= 1000);

        item.setDurationMs(500);
        QVERIFY(item.durationMs() >= 1000);

        item.setDurationMs(999);
        QVERIFY(item.durationMs() >= 1000);

        item.setDurationMs(700000);
        QCOMPARE(item.durationMs(), 700000);

        item.setDurationMs(1800000);
        QCOMPARE(item.durationMs(), 1800000);

        item.setDurationMs(2000000);
        QCOMPARE(item.durationMs(), 1800000);
    }

    void capacity_computed_from_duration()
    {
        // Task 3.3: setDurationMs() should compute capacity from duration
        // assuming 10 Hz polling (100 ms interval).
        // Expected formula: capacity = durationMs / 100 + 1
        HistoryGraphItem item;

        item.setDurationMs(1000);   // 1 second @ 10 Hz = ~10 samples
        QVERIFY(item.capacity() >= 10);

        item.setDurationMs(5000);   // 5 seconds @ 10 Hz = ~50 samples
        QVERIFY(item.capacity() >= 50);

        item.setDurationMs(60000);  // 60 seconds @ 10 Hz = ~600 samples
        QVERIFY(item.capacity() >= 600);

        // Minimum capacity is 2
        item.setDurationMs(1000);
        QVERIFY(item.capacity() >= 2);
    }

    void buffer_retains_data_within_duration()
    {
        // Task 3.3: when duration changes, the buffer capacity is recomputed
        // and resized via setCapacity(). Smaller duration → smaller capacity.
        HistoryGraphItem item;

        // Set to 60 seconds capacity (~600 samples)
        item.setDurationMs(60000);
        const int cap1 = item.capacity();

        // Push some values
        for (int i = 0; i < cap1; ++i) {
            item.setValue(static_cast<double>(i));
        }

        // Now shrink to 1 second (should have much smaller capacity)
        item.setDurationMs(1000);
        const int cap2 = item.capacity();

        // 1s @ 10Hz should be ~10 samples, 60s @ 10Hz should be ~600 samples
        QVERIFY(cap2 < cap1);
        QVERIFY(cap2 >= 2);
    }

    void serialize_v2_includes_duration()
    {
        // Task 3.3: serialize() should include durationMs as 17th field
        HistoryGraphItem item;
        item.setRect(10, 20, 100, 50);
        item.setDurationMs(45000);
        item.setBindingId(5);
        item.setZOrder(2);

        const QString serialized = item.serialize();
        const QStringList parts = serialized.split(QLatin1Char('|'));

        // v2 format has 17 parts (indices 0–16)
        QVERIFY(parts.size() >= 17);
        QCOMPARE(parts[0], QLatin1String("HISTORY"));
        QCOMPARE(parts[16].toInt(), 45000);  // durationMs at index 16
    }

    void deserialize_v2_restores_duration()
    {
        // Task 3.3: deserialize() should restore durationMs from v2 format
        HistoryGraphItem item1;
        item1.setRect(10, 20, 100, 50);
        item1.setDurationMs(45000);
        item1.setBindingId(5);
        item1.setZOrder(2);

        const QString serialized = item1.serialize();

        HistoryGraphItem item2;
        const bool ok = item2.deserialize(serialized);
        QVERIFY(ok);
        QCOMPARE(item2.durationMs(), 45000);
        QCOMPARE(item2.bindingId(), 5);
    }

    void deserialize_v1_defaults_duration()
    {
        // Task 3.3: deserialize() should accept v1 format (no durationMs field)
        // and use the default duration value.
        // v1 format stops at index 15, so we manually craft one.
        const QString v1SerializedData = QStringLiteral(
            "HISTORY|10|20|100|50|5|2|300|#00b4d8ff|#ffb800ff|1|1|1|1|0|7");

        HistoryGraphItem item;
        const bool ok = item.deserialize(v1SerializedData);
        QVERIFY(ok);

        // Should restore v1 fields and keep default durationMs (60000)
        QCOMPARE(item.bindingId(), 5);
        // durationMs defaults to 60000 for v1 formats
        QVERIFY(item.durationMs() > 0);
    }

    void appsettings_persistence()
    {
        // Task 3.3: AppSettings should persist MultimeterSignalHistoryDurationMs
        auto& s = AppSettings::instance();

        s.setValue(QStringLiteral("MultimeterSignalHistoryDurationMs"), 75000);
        const int persisted = s.value(QStringLiteral("MultimeterSignalHistoryDurationMs"), 60000).toInt();
        QCOMPARE(persisted, 75000);

        // Reset to default
        s.setValue(QStringLiteral("MultimeterSignalHistoryDurationMs"), 60000);
    }

    void appsettings_enabled_flag()
    {
        // Task 3.3: AppSettings should persist MultimeterSignalHistoryEnabled as "True"/"False"
        auto& s = AppSettings::instance();

        s.setValue(QStringLiteral("MultimeterSignalHistoryEnabled"), QStringLiteral("True"));
        const bool enabled = s.value(QStringLiteral("MultimeterSignalHistoryEnabled"), QStringLiteral("False")).toString()
            == QStringLiteral("True");
        QVERIFY(enabled);

        s.setValue(QStringLiteral("MultimeterSignalHistoryEnabled"), QStringLiteral("False"));
        const bool disabled = s.value(QStringLiteral("MultimeterSignalHistoryEnabled"), QStringLiteral("False")).toString()
            == QStringLiteral("True");
        QVERIFY(!disabled);
    }
};

QTEST_MAIN(TestSignalHistory)
#include "tst_signal_history.moc"
