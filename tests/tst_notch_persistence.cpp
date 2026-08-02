// =================================================================
// tests/tst_notch_persistence.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// TNF section 5.5: AppSettings round-trip at global scope
// (NotchGlobalEnabled, NotchVisualEnabled, NotchAutoIncrease, NotchCount,
// Notch<i>Center / Width / Active), the save-on-mutate contract Task 4
// relies on, plus the section 5.3 clear() contract: clear() MUST emit
// notchesReset() because the RadioModel fan-out is purely signal-driven,
// so a silent clear would leave every channel's notch set installed.
// Design: docs/architecture/2026-07-28-tunable-notch-filter-design.md
//         section 5.3 (clear() contract), section 5.5 (keys),
//         section 8.3 (visual toggle), section 9 (auto-increase).
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>
#include "models/NotchModel.h"
#include "core/AppSettings.h"

using namespace NereusSDR;

class TestNotchPersistence : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { AppSettings::instance().clear(); }
    void cleanup()      { AppSettings::instance().clear(); }

    // Defaults (design sections 5.4 / 8.3 / 9)
    void defaults_match_the_wdsp_and_upstream_state()
    {
        NotchModel m;
        // Maintainer decision D-a: OFF. Thetis chkTNF ships unchecked and
        // WDSP creates the notch database with master run 0 (RXA.c:87).
        QVERIFY(!m.globalEnabled());
        // WDSP creates nbp0 with autoincr = 1 (RXA.c:105), so ON, not OFF.
        QVERIFY(m.autoIncrease());
        // Thetis chkVisualNotch has no designer Checked assignment.
        QVERIFY(!m.visualEnabled());
        QVERIFY(!m.adminBusy());
        QVERIFY(m.notches().isEmpty());
    }

    // Global flags
    void setGlobalEnabled_emits_only_on_change()
    {
        NotchModel m;
        QSignalSpy spy(&m, &NotchModel::globalEnabledChanged);
        m.setGlobalEnabled(false);         // already false
        QCOMPARE(spy.count(), 0);
        m.setGlobalEnabled(true);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toBool(), true);
        m.setGlobalEnabled(true);
        QCOMPARE(spy.count(), 1);
    }

    void setAutoIncrease_emits_only_on_change()
    {
        NotchModel m;
        QSignalSpy spy(&m, &NotchModel::autoIncreaseChanged);
        m.setAutoIncrease(true);
        QCOMPARE(spy.count(), 0);
        m.setAutoIncrease(false);
        QCOMPARE(spy.count(), 1);
        QVERIFY(!m.autoIncrease());
    }

    void setVisualEnabled_emits_only_on_change()
    {
        NotchModel m;
        QSignalSpy spy(&m, &NotchModel::visualEnabledChanged);
        m.setVisualEnabled(false);
        QCOMPARE(spy.count(), 0);
        m.setVisualEnabled(true);
        QCOMPARE(spy.count(), 1);
        QVERIFY(m.visualEnabled());
    }

    // clear() contract (section 5.3)
    void clear_empties_the_list_and_emits_notchesReset()
    {
        NotchModel m;
        m.addNotch(14074000.0);
        m.addNotch(14075000.0);
        QSignalSpy spy(&m, &NotchModel::notchesReset);

        m.clear();

        QVERIFY(m.notches().isEmpty());
        QCOMPARE(spy.count(), 1);
    }

    void clear_emits_notchesReset_even_when_already_empty()
    {
        // The signal is the fan-out's reconcile trigger, not a change
        // notification: a channel reopened with a stale set still needs it.
        NotchModel m;
        QSignalSpy spy(&m, &NotchModel::notchesReset);
        m.clear();
        QCOMPARE(spy.count(), 1);
    }

    void clear_does_not_emit_per_notch_removal()
    {
        NotchModel m;
        m.addNotch(14074000.0);
        QSignalSpy removed(&m, &NotchModel::notchRemoved);
        m.clear();
        QCOMPARE(removed.count(), 0);
    }

    // Save-on-mutate (Task 4 constructs NotchModel and calls
    // restoreFromSettings(); nothing anywhere calls saveToSettings() on the
    // operator's behalf, so every mutator has to persist itself).
    void add_persists_without_an_explicit_save()
    {
        NotchModel m;
        m.addNotch(14074000.0);

        auto& s = AppSettings::instance();
        QCOMPARE(s.value(QStringLiteral("NotchCount")).toString(),
                 QStringLiteral("1"));
        QCOMPARE(s.value(QStringLiteral("Notch0Center")).toDouble(), 14074000.0);
    }

    void edits_persist_without_an_explicit_save()
    {
        NotchModel m;
        const int id = m.addNotch(14074000.0);
        auto& s = AppSettings::instance();

        QVERIFY(m.setCenter(id, 14075000.0));
        QCOMPARE(s.value(QStringLiteral("Notch0Center")).toDouble(), 14075000.0);

        QVERIFY(m.setWidth(id, 400.0));
        QCOMPARE(s.value(QStringLiteral("Notch0Width")).toDouble(), 400.0);

        QVERIFY(m.setActive(id, false));
        QCOMPARE(s.value(QStringLiteral("Notch0Active")).toString(),
                 QStringLiteral("False"));
    }

    void remove_persists_without_an_explicit_save()
    {
        NotchModel m;
        const int a = m.addNotch(14074000.0);
        m.addNotch(14075000.0);
        QVERIFY(m.removeNotch(a));

        auto& s = AppSettings::instance();
        QCOMPARE(s.value(QStringLiteral("NotchCount")).toString(),
                 QStringLiteral("1"));
        QCOMPARE(s.value(QStringLiteral("Notch0Center")).toDouble(), 14075000.0);
        QVERIFY(!s.contains(QStringLiteral("Notch1Center")));
    }

    void global_flags_persist_without_an_explicit_save()
    {
        NotchModel m;
        m.setGlobalEnabled(true);
        m.setVisualEnabled(true);
        m.setAutoIncrease(false);

        auto& s = AppSettings::instance();
        QCOMPARE(s.value(QStringLiteral("NotchGlobalEnabled")).toString(),
                 QStringLiteral("True"));
        QCOMPARE(s.value(QStringLiteral("NotchVisualEnabled")).toString(),
                 QStringLiteral("True"));
        QCOMPARE(s.value(QStringLiteral("NotchAutoIncrease")).toString(),
                 QStringLiteral("False"));
    }

    void clear_persists_without_an_explicit_save()
    {
        NotchModel m;
        m.addNotch(14074000.0);
        m.clear();

        auto& s = AppSettings::instance();
        QCOMPARE(s.value(QStringLiteral("NotchCount")).toString(),
                 QStringLiteral("0"));
        QVERIFY(!s.contains(QStringLiteral("Notch0Center")));
    }

    void a_rejected_add_does_not_touch_the_store()
    {
        NotchModel m;
        m.addNotch(14074000.0);
        AppSettings::instance().setValue(QStringLiteral("NotchCount"),
                                         QStringLiteral("sentinel"));
        QCOMPARE(m.addNotch(14074002.0), -1);
        QCOMPARE(AppSettings::instance().value(QStringLiteral("NotchCount"))
                     .toString(), QStringLiteral("sentinel"));
    }

    // Persistence round-trip (section 5.5)
    void save_writes_the_documented_keys()
    {
        NotchModel m;
        const int id = m.addNotch(14074123.0);
        QVERIFY(m.setWidth(id, 400.0));
        QVERIFY(m.setActive(id, false));
        m.setGlobalEnabled(true);
        m.setVisualEnabled(true);
        m.setAutoIncrease(false);

        m.saveToSettings();

        auto& s = AppSettings::instance();
        QCOMPARE(s.value(QStringLiteral("NotchGlobalEnabled")).toString(),
                 QStringLiteral("True"));
        QCOMPARE(s.value(QStringLiteral("NotchVisualEnabled")).toString(),
                 QStringLiteral("True"));
        QCOMPARE(s.value(QStringLiteral("NotchAutoIncrease")).toString(),
                 QStringLiteral("False"));
        QCOMPARE(s.value(QStringLiteral("NotchCount")).toString(),
                 QStringLiteral("1"));
        QCOMPARE(s.value(QStringLiteral("Notch0Center")).toDouble(), 14074123.0);
        QCOMPARE(s.value(QStringLiteral("Notch0Width")).toDouble(), 400.0);
        QCOMPARE(s.value(QStringLiteral("Notch0Active")).toString(),
                 QStringLiteral("False"));
    }

    void save_does_not_lose_frequency_precision()
    {
        // QVariant::toString() on a raw double is how AppSettings stores
        // everything; the centre must survive it exactly.
        NotchModel m;
        m.addNotch(28074123.0);
        m.saveToSettings();
        QCOMPARE(AppSettings::instance().value(QStringLiteral("Notch0Center"))
                     .toDouble(), 28074123.0);
    }

    void restore_round_trips_the_list_in_order()
    {
        NotchModel src;
        src.addNotch(14074000.0);
        const int b = src.addNotch(14075000.0);
        QVERIFY(src.setWidth(b, 600.0));
        QVERIFY(src.setActive(b, false));
        src.addNotch(14076000.0);
        src.saveToSettings();

        NotchModel dst;
        dst.restoreFromSettings();

        QCOMPARE(dst.notches().size(), 3);
        QCOMPARE(dst.notches().at(0).centerHz, 14074000.0);
        QCOMPARE(dst.notches().at(1).centerHz, 14075000.0);
        QCOMPARE(dst.notches().at(1).widthHz, 600.0);
        QCOMPARE(dst.notches().at(1).active, false);
        QCOMPARE(dst.notches().at(2).centerHz, 14076000.0);
        QVERIFY(dst.notches().at(0).active);
    }

    void restore_round_trips_the_three_global_flags()
    {
        NotchModel src;
        src.setGlobalEnabled(true);
        src.setVisualEnabled(true);
        src.setAutoIncrease(false);
        src.saveToSettings();

        NotchModel dst;
        dst.restoreFromSettings();

        QVERIFY(dst.globalEnabled());
        QVERIFY(dst.visualEnabled());
        QVERIFY(!dst.autoIncrease());
    }

    void restore_mints_fresh_distinct_ids()
    {
        // ids are session-local hit-test keys and are deliberately not
        // persisted (design sections 5.1 / 5.5).
        NotchModel src;
        src.addNotch(14074000.0);
        src.addNotch(14075000.0);
        src.saveToSettings();

        NotchModel dst;
        dst.restoreFromSettings();

        const int idA = dst.notches().at(0).id;
        const int idB = dst.notches().at(1).id;
        QVERIFY(idA != idB);
        QCOMPARE(dst.indexOfId(idA), 0);
        QCOMPARE(dst.indexOfId(idB), 1);
        QCOMPARE(dst.notchById(idB)->centerHz, 14075000.0);
    }

    void restore_emits_notchesReset()
    {
        NotchModel src;
        src.addNotch(14074000.0);
        src.saveToSettings();

        NotchModel dst;
        QSignalSpy spy(&dst, &NotchModel::notchesReset);
        dst.restoreFromSettings();
        QCOMPARE(spy.count(), 1);
    }

    void restore_leaves_defaults_alone_when_nothing_is_persisted()
    {
        NotchModel m;
        m.restoreFromSettings();
        QVERIFY(!m.globalEnabled());
        QVERIFY(m.autoIncrease());
        QVERIFY(!m.visualEnabled());
        QVERIFY(m.notches().isEmpty());
    }

    void save_prunes_keys_left_by_a_longer_previous_list()
    {
        NotchModel m;
        m.addNotch(14074000.0);
        const int b = m.addNotch(14075000.0);
        m.addNotch(14076000.0);
        m.saveToSettings();
        QVERIFY(AppSettings::instance().contains(QStringLiteral("Notch2Center")));

        QVERIFY(m.removeNotch(b));
        m.saveToSettings();

        auto& s = AppSettings::instance();
        QCOMPARE(s.value(QStringLiteral("NotchCount")).toString(),
                 QStringLiteral("2"));
        QVERIFY(!s.contains(QStringLiteral("Notch2Center")));
        QVERIFY(!s.contains(QStringLiteral("Notch2Width")));
        QVERIFY(!s.contains(QStringLiteral("Notch2Active")));

        NotchModel dst;
        dst.restoreFromSettings();
        QCOMPARE(dst.notches().size(), 2);
        QCOMPARE(dst.notches().at(1).centerHz, 14076000.0);
    }

    void restore_replaces_rather_than_appends()
    {
        // dst mutates first so its own save-on-mutate write is the one the
        // src save below has to overwrite; the restore must then discard
        // dst's in-memory entry rather than appending to it.
        NotchModel dst;
        dst.addNotch(21074000.0);

        NotchModel src;
        src.addNotch(14074000.0);
        src.saveToSettings();

        dst.restoreFromSettings();

        QCOMPARE(dst.notches().size(), 1);
        QCOMPARE(dst.notches().at(0).centerHz, 14074000.0);
    }

    void restore_does_not_write_back_over_its_own_source()
    {
        // Without the re-entrancy guard the flag restores below would fire
        // save-on-mutate against the half-restored list, truncating
        // NotchCount to dst's one entry and pruning Notch1* before the
        // notch loop ever read them.
        NotchModel dst;
        dst.addNotch(21074000.0);

        NotchModel src;
        src.addNotch(14074000.0);
        src.addNotch(14075000.0);
        src.setGlobalEnabled(true);
        src.saveToSettings();

        dst.restoreFromSettings();

        QCOMPARE(dst.notches().size(), 2);
        auto& s = AppSettings::instance();
        QCOMPARE(s.value(QStringLiteral("NotchCount")).toString(),
                 QStringLiteral("2"));
        QCOMPARE(s.value(QStringLiteral("Notch1Center")).toDouble(), 14075000.0);
    }

    void save_of_an_empty_list_round_trips_as_empty()
    {
        NotchModel src;
        src.addNotch(14074000.0);
        src.saveToSettings();
        src.clear();
        src.saveToSettings();

        NotchModel dst;
        dst.restoreFromSettings();
        QVERIFY(dst.notches().isEmpty());
    }
};

QTEST_MAIN(TestNotchPersistence)
#include "tst_notch_persistence.moc"
