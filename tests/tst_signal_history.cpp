// =================================================================
// tests/tst_signal_history.cpp — Task 3.3 Signal History Tests
// =================================================================
//
// Test suite for HistoryGraphItem duration setting.
// Verifies that setDurationMs() correctly configures the data buffer
// and that MultimeterPage broadcasts to all HistoryGraphItem instances.
//
// Modification history (NereusSDR):
//   2026-10-26 - A history graph whose length was set in its editor keeps
//                 it when the Multimeter history duration is applied, and
//                 across a save and reload; graphs that never set one, and
//                 old records, follow the Multimeter setting. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QTest>
#include <QSignalSpy>
#include <QCheckBox>
#include <QSpinBox>
#include "gui/meters/HistoryGraphItem.h"
#include "gui/containers/meter_property_editors/HistoryGraphItemEditor.h"
#include "gui/setup/MultimeterPage.h"
#include "core/AppSettings.h"

using namespace NereusSDR;

namespace {

const QString kDurationKey = QStringLiteral("MultimeterSignalHistoryDurationMs");

// The editor's Capacity spin box (range 10..3000); the base editor's own
// integer rows (Z-order, Display group) have other ranges.
QSpinBox* capacitySpin(HistoryGraphItemEditor& ed)
{
    const auto spins = ed.findChildren<QSpinBox*>();
    for (QSpinBox* s : spins) {
        if (s->minimum() == 10 && s->maximum() == 3000) {
            return s;
        }
    }
    return nullptr;
}

// The operator sets a graph's length in its editor, as the dialog does.
void setLengthInEditor(HistoryGraphItem& item, int capacity)
{
    HistoryGraphItemEditor ed;
    ed.setItem(&item);
    QSpinBox* spin = capacitySpin(ed);
    QVERIFY(spin != nullptr);
    spin->setValue(capacity);
}

}  // namespace

class TestSignalHistory : public QObject {
    Q_OBJECT

private slots:
    void cleanup()
    {
        AppSettings::instance().setValue(kDurationKey, 60000);
    }

    void own_length_survives_multimeter_apply()
    {
        HistoryGraphItem item;
        setLengthInEditor(item, 150);
        QCOMPARE(item.capacity(), 150);

        AppSettings::instance().setValue(kDurationKey, 30000);
        MultimeterPage::applyPersistedSettingsTo(&item);
        QCOMPARE(item.capacity(), 150);
    }

    void own_length_survives_save_reload_and_apply()
    {
        HistoryGraphItem item;
        setLengthInEditor(item, 150);
        const QString saved = item.serialize();

        HistoryGraphItem reloaded;
        QVERIFY(reloaded.deserialize(saved));
        QCOMPARE(reloaded.capacity(), 150);

        AppSettings::instance().setValue(kDurationKey, 30000);
        MultimeterPage::applyPersistedSettingsTo(&reloaded);
        QCOMPARE(reloaded.capacity(), 150);
        // A second save keeps the graph's own length.
        HistoryGraphItem again;
        QVERIFY(again.deserialize(reloaded.serialize()));
        MultimeterPage::applyPersistedSettingsTo(&again);
        QCOMPARE(again.capacity(), 150);
    }

    void saved_record_appends_own_length_field()
    {
        HistoryGraphItem plain;
        const QStringList plainParts = plain.serialize().split(QLatin1Char('|'));
        QCOMPARE(plainParts.size(), 18);
        QCOMPARE(plainParts[17], QStringLiteral("0"));

        HistoryGraphItem own;
        setLengthInEditor(own, 150);
        const QStringList ownParts = own.serialize().split(QLatin1Char('|'));
        QCOMPARE(ownParts.size(), 18);
        QCOMPARE(ownParts[7], QStringLiteral("150"));
        QCOMPARE(ownParts[17], QStringLiteral("1"));
    }

    void graph_without_own_length_follows_global_change()
    {
        HistoryGraphItem item;
        AppSettings::instance().setValue(kDurationKey, 30000);
        MultimeterPage::applyPersistedSettingsTo(&item);
        QCOMPARE(item.durationMs(), 30000);
        QCOMPARE(item.capacity(), 301);

        AppSettings::instance().setValue(kDurationKey, 45000);
        MultimeterPage::applyPersistedSettingsTo(&item);
        QCOMPARE(item.durationMs(), 45000);
        QCOMPARE(item.capacity(), 451);
    }

    void old_record_follows_global()
    {
        // A record saved before the own-length field (17 fields).
        HistoryGraphItem v2;
        QVERIFY(v2.deserialize(QStringLiteral(
            "HISTORY|0|0|1|0.8|5|5|601|#ff00b4d8|#ffffb800|1|1|1|1|0|-1|60000")));
        AppSettings::instance().setValue(kDurationKey, 30000);
        MultimeterPage::applyPersistedSettingsTo(&v2);
        QCOMPARE(v2.durationMs(), 30000);
        QCOMPARE(v2.capacity(), 301);

        // The oldest record (16 fields, no duration).
        HistoryGraphItem v1;
        QVERIFY(v1.deserialize(QStringLiteral(
            "HISTORY|10|20|100|50|5|2|300|#00b4d8ff|#ffb800ff|1|1|1|1|0|7")));
        MultimeterPage::applyPersistedSettingsTo(&v1);
        QCOMPARE(v1.durationMs(), 30000);
        QCOMPARE(v1.capacity(), 301);
    }

    void follow_choice_returns_graph_to_global()
    {
        HistoryGraphItem item;
        AppSettings::instance().setValue(kDurationKey, 30000);
        HistoryGraphItemEditor ed;
        ed.setItem(&item);
        auto* follow = ed.findChild<QCheckBox*>(QStringLiteral("followMultimeterLength"));
        QVERIFY(follow != nullptr);
        QVERIFY(follow->isChecked());

        QSpinBox* spin = capacitySpin(ed);
        QVERIFY(spin != nullptr);
        spin->setValue(150);
        QVERIFY(!follow->isChecked());
        QCOMPARE(item.capacity(), 150);

        follow->setChecked(true);
        QCOMPARE(item.durationMs(), 30000);
        QCOMPARE(item.capacity(), 301);
        QCOMPARE(spin->value(), 301);
        QCOMPARE(item.serialize().split(QLatin1Char('|'))[17], QStringLiteral("0"));

        AppSettings::instance().setValue(kDurationKey, 45000);
        MultimeterPage::applyPersistedSettingsTo(&item);
        QCOMPARE(item.capacity(), 451);
    }

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
        // Task 3.3: duration should be clamped to [1000, 600000] ms
        HistoryGraphItem item;

        item.setDurationMs(0);
        QVERIFY(item.durationMs() >= 1000);

        item.setDurationMs(500);
        QVERIFY(item.durationMs() >= 1000);

        item.setDurationMs(999);
        QVERIFY(item.durationMs() >= 1000);

        item.setDurationMs(700000);
        QVERIFY(item.durationMs() <= 600000);

        item.setDurationMs(1000000);
        QVERIFY(item.durationMs() <= 600000);
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

    void saved_capacity_reloads_unchanged()
    {
        // A saved capacity (field 7) loads as written. Loading used to
        // apply the saved duration last, which recomputed the capacity
        // from it (60000 ms -> 601) and dropped the saved value.
        HistoryGraphItem fresh;
        const QString created = fresh.serialize();
        QCOMPARE(created.split(QLatin1Char('|')).value(7), QStringLiteral("300"));
        QCOMPARE(created.split(QLatin1Char('|')).value(16), QStringLiteral("60000"));

        HistoryGraphItem reloaded;
        QVERIFY(reloaded.deserialize(created));
        QCOMPARE(reloaded.capacity(), 300);
        QCOMPARE(reloaded.durationMs(), 60000);
        QCOMPARE(reloaded.serialize(), created);

        // A capacity edited in the item editor survives too.
        HistoryGraphItem edited;
        edited.setCapacity(1000);
        HistoryGraphItem editedReloaded;
        QVERIFY(editedReloaded.deserialize(edited.serialize()));
        QCOMPARE(editedReloaded.capacity(), 1000);
        QCOMPARE(editedReloaded.durationMs(), 60000);
        QCOMPARE(editedReloaded.serialize(), edited.serialize());

        // A record saved after a reload on the old code (601) keeps 601.
        QStringList resaved = created.split(QLatin1Char('|'));
        resaved[7] = QStringLiteral("601");
        HistoryGraphItem old;
        QVERIFY(old.deserialize(resaved.join(QLatin1Char('|'))));
        QCOMPARE(old.capacity(), 601);

        // A v1 record (no duration field) keeps its capacity as well.
        HistoryGraphItem v1;
        QVERIFY(v1.deserialize(QStringLiteral(
            "HISTORY|10|20|100|50|5|2|300|#00b4d8ff|#ffb800ff|1|1|1|1|0|7")));
        QCOMPARE(v1.capacity(), 300);
        QCOMPARE(v1.durationMs(), 60000);
    }

    void loaded_capacity_clamped_to_largest_produced()
    {
        // The largest capacity NereusSDR produces is 600000 ms / 100 + 1.
        QCOMPARE(HistoryGraphItem::kMaxCapacity, 6001);
        HistoryGraphItem longest;
        longest.setDurationMs(600000);
        QCOMPARE(longest.capacity(), HistoryGraphItem::kMaxCapacity);

        const QStringList base = HistoryGraphItem().serialize().split(QLatin1Char('|'));

        // A pasted record asking for INT_MAX loads at the bound instead of
        // allocating two buffers of that size.
        QStringList huge = base;
        huge[7] = QStringLiteral("2147483647");
        HistoryGraphItem hugeItem;
        QVERIFY(hugeItem.deserialize(huge.join(QLatin1Char('|'))));
        QCOMPARE(hugeItem.capacity(), HistoryGraphItem::kMaxCapacity);
        QCOMPARE(hugeItem.serialize().split(QLatin1Char('|')).value(7),
                 QStringLiteral("6001"));

        // A record at the bound loads unchanged.
        QStringList atBound = base;
        atBound[7] = QStringLiteral("6001");
        const QString atBoundRecord = atBound.join(QLatin1Char('|'));
        HistoryGraphItem atBoundItem;
        QVERIFY(atBoundItem.deserialize(atBoundRecord));
        QCOMPARE(atBoundItem.capacity(), 6001);
        QCOMPARE(atBoundItem.serialize(), atBoundRecord);
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
