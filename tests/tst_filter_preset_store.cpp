// tst_filter_preset_store.cpp
//
// no-port-check: NereusSDR-original test file. All Thetis filter-preset
// source cites are in SliceModel.cpp (console.cs:5180-5575 [v2.10.3.13]).
// =================================================================
// tests/tst_filter_preset_store.cpp  (NereusSDR)
// =================================================================
//
// Stage C2 (Task TDD) — verifies FilterPresetStore:
//   1. defaultsMatchSliceModel:     defaultPreset(mode,slot) matches
//      SliceModel::presetsForMode(mode)[slot] for representative modes/slots.
//   2. setPresetPersists:           mutate a slot, round-trip through
//      a fresh store, verify the override survived.
//   3. resetPresetRestoresDefault:  mutate then resetPreset, verify default.
//   4. setPresetsForModeReorderPersists: swap two slots, verify order survives.
//   5. presetsChangedSignalFires:   QSignalSpy confirms every mutator emits.
//   6. followStationSetting... (iPhone app Task 19, D40): a Core setting
//      that changes in a remote window emits presetsChanged for its mode.
//   7. aRemoteWindowReadsAndWritesTheCoresPresets / aLocalWindowIsUnchanged:
//      with "filters/" in Station scope, the settings proxy carries the
//      presets to and from the Core; without it they stay local.
//
// Isolation: uses AppSettings::clear() in initTestCase/init/cleanup so that
// filter overrides written here cannot bleed into other tests.
// =================================================================
// Modification history (NereusSDR):
//   2026-05-02 — Original implementation for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted authoring via Anthropic
//                 Claude Code (Stage C2 filter preset editor).
//   2026-09-24 - iPhone app Task 19 (R-IOS-06, D40): the presets follow the
//                 Core in a remote window. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/settings/SettingsProxy.h"
#include "core/settings/SettingsScope.h"
#include "models/FilterPresetStore.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

class TstFilterPresetStore : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { AppSettings::instance().clear(); }
    void init()         { AppSettings::instance().clear(); }
    void cleanup()
    {
        AppSettings::instance().setRemoteBackend(nullptr);
        AppSettings::instance().clear();
    }

    // ── 1. Defaults match SliceModel ────────────────────────────────────────
    // Verifies that FilterPresetStore::defaultPreset(mode, slot) returns the
    // same (low, high) values as SliceModel::presetsForMode(mode)[slot].
    // Tests a representative subset: USB/LSB/CWU slots 0,4,9 and AM slot 0.
    void defaultsMatchSliceModel()
    {
        const struct { DSPMode mode; int slot; } cases[] = {
            {DSPMode::USB, 0}, {DSPMode::USB, 4}, {DSPMode::USB, 9},
            {DSPMode::LSB, 0}, {DSPMode::LSB, 4}, {DSPMode::LSB, 9},
            {DSPMode::CWU, 0}, {DSPMode::CWU, 4}, {DSPMode::CWU, 9},
            {DSPMode::AM,  0}, {DSPMode::AM,  4}, {DSPMode::AM,  9},
            {DSPMode::FM,  0}, {DSPMode::FM,  1}, {DSPMode::FM,  2},
        };

        for (const auto& c : cases) {
            const auto pairs = SliceModel::presetsForMode(c.mode);
            if (c.slot >= pairs.size()) { continue; }  // FM only has 3 slots
            const FilterPreset def = FilterPresetStore::defaultPreset(c.mode, c.slot);
            QCOMPARE(def.low,  pairs[c.slot].first);
            QCOMPARE(def.high, pairs[c.slot].second);
            // Name should be "F<slot+1>"
            QCOMPARE(def.name, QStringLiteral("F%1").arg(c.slot + 1));
        }
    }

    // ── 2. setPreset persists across store re-creation ──────────────────────
    // After writing a preset and clearing the in-memory store (simulated by
    // constructing a fresh FilterPresetStore), the value survives because the
    // underlying AppSettings flush persisted it.
    void setPresetPersists()
    {
        {
            FilterPresetStore store;
            FilterPreset p;
            p.name = QStringLiteral("DX-2.4k");
            p.low  = 100;
            p.high = 2500;
            store.setPreset(DSPMode::USB, 4, p);
        }

        // Construct a fresh store — reads from AppSettings.
        FilterPresetStore store2;
        const auto presets = store2.presetsForMode(DSPMode::USB);
        QVERIFY(presets.size() > 4);
        QCOMPARE(presets[4].name, QStringLiteral("DX-2.4k"));
        QCOMPARE(presets[4].low,  100);
        QCOMPARE(presets[4].high, 2500);
    }

    // ── 3. resetPreset restores default ─────────────────────────────────────
    void resetPresetRestoresDefault()
    {
        FilterPresetStore store;

        // Mutate slot 2 of LSB.
        FilterPreset custom;
        custom.name = QStringLiteral("Narrow");
        custom.low  = -800;
        custom.high = -200;
        store.setPreset(DSPMode::LSB, 2, custom);

        // Verify the override is there.
        auto presets = store.presetsForMode(DSPMode::LSB);
        QCOMPARE(presets[2].name, QStringLiteral("Narrow"));

        // Reset to default.
        store.resetPreset(DSPMode::LSB, 2);

        // Verify default restored.
        presets = store.presetsForMode(DSPMode::LSB);
        const FilterPreset def = FilterPresetStore::defaultPreset(DSPMode::LSB, 2);
        QCOMPARE(presets[2].low,  def.low);
        QCOMPARE(presets[2].high, def.high);
        QCOMPARE(presets[2].name, def.name);
    }

    // ── 4. setPresetsForMode reorder persists ───────────────────────────────
    void setPresetsForModeReorderPersists()
    {
        FilterPresetStore store;

        // Get USB defaults.
        QList<FilterPreset> presets = store.presetsForMode(DSPMode::USB);
        QVERIFY(presets.size() >= 2);

        // Swap slots 0 and 1.
        const FilterPreset slot0 = presets[0];
        const FilterPreset slot1 = presets[1];
        presets.swapItemsAt(0, 1);
        store.setPresetsForMode(DSPMode::USB, presets);

        // Verify in memory.
        const auto after = store.presetsForMode(DSPMode::USB);
        QCOMPARE(after[0].low,  slot1.low);
        QCOMPARE(after[0].high, slot1.high);
        QCOMPARE(after[1].low,  slot0.low);
        QCOMPARE(after[1].high, slot0.high);

        // Verify persisted: construct a fresh store and read back.
        FilterPresetStore store2;
        const auto persisted = store2.presetsForMode(DSPMode::USB);
        QCOMPARE(persisted[0].low,  slot1.low);
        QCOMPARE(persisted[0].high, slot1.high);
        QCOMPARE(persisted[1].low,  slot0.low);
        QCOMPARE(persisted[1].high, slot0.high);
    }

    // ── 5. presetsChanged signal fires on each mutator ──────────────────────
    void presetsChangedSignalFires()
    {
        FilterPresetStore store;
        QSignalSpy spy(&store, &FilterPresetStore::presetsChanged);
        QVERIFY(spy.isValid());

        // setPreset → 1 signal
        FilterPreset p;
        p.name = QStringLiteral("Test");
        p.low  = 100;
        p.high = 2900;
        store.setPreset(DSPMode::USB, 0, p);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.last().first().value<DSPMode>(), DSPMode::USB);

        spy.clear();

        // resetPreset → 1 signal
        store.resetPreset(DSPMode::USB, 0);
        QCOMPARE(spy.count(), 1);

        spy.clear();

        // setPresetsForMode → 1 signal
        auto presets = store.presetsForMode(DSPMode::LSB);
        store.setPresetsForMode(DSPMode::LSB, presets);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.last().first().value<DSPMode>(), DSPMode::LSB);

        spy.clear();

        // resetMode → 1 signal
        store.resetMode(DSPMode::AM);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.last().first().value<DSPMode>(), DSPMode::AM);

        spy.clear();

        // resetAll → 12 signals (one per DSPMode)
        store.resetAll();
        // DSPMode has 12 values (LSB..DRM = 0..11)
        QCOMPARE(spy.count(), 12);
    }

    // ── 6. A Core setting that changes emits for its mode ───────────────────
    // RadioModel forwards stationSettingChanged (remote windows only) here.
    void followStationSettingEmitsForItsMode()
    {
        FilterPresetStore store;
        QSignalSpy spy(&store, &FilterPresetStore::presetsChanged);

        store.followStationSetting(QStringLiteral("filters/USB/3/low"));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.last().first().value<DSPMode>(), DSPMode::USB);

        store.followStationSetting(QStringLiteral("filters/RADE-U/0/name"));
        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.last().first().value<DSPMode>(), DSPMode::RADE_U);

        // Not a preset: nothing.
        store.followStationSetting(QStringLiteral("Slice0/Band20m/FilterLow"));
        store.followStationSetting(QStringLiteral("filters/NotAMode/1/low"));
        store.followStationSetting(QStringLiteral("filters"));
        QCOMPARE(spy.count(), 2);

        // A whole snapshot: every one of the 14 modes.
        spy.clear();
        store.followStationSetting(QString());
        QCOMPARE(spy.count(), 14);
    }

    // ── 7. The presets are the Core's in a remote window (D40) ──────────────
    void aRemoteWindowReadsAndWritesTheCoresPresets()
    {
        QCOMPARE(classifySettingsKey(QStringLiteral("filters/USB/0/name")),
                 SettingsScope::Station);

        // This computer's own override, which a remote window must not show.
        FilterPreset local;
        local.name = QStringLiteral("Local");
        local.low = 200;
        local.high = 2600;
        FilterPresetStore store;
        store.setPreset(DSPMode::USB, 0, local);

        SettingsProxy proxy;
        AppSettings::instance().setRemoteBackend(&proxy);
        proxy.applySnapshot({{QStringLiteral("filters/USB/0/name"), QStringLiteral("Core")},
                             {QStringLiteral("filters/USB/0/low"), QStringLiteral("150")},
                             {QStringLiteral("filters/USB/0/high"), QStringLiteral("2850")}});
        proxy.setReady(true);

        // Shows the Core's.
        const FilterPreset shown = store.presetsForMode(DSPMode::USB).first();
        QCOMPARE(shown.name, QStringLiteral("Core"));
        QCOMPARE(shown.low, 150);
        QCOMPARE(shown.high, 2850);

        // Edits the Core's: the three settings go out through the proxy.
        QSignalSpy outbound(&proxy, &SettingsProxy::outboundWriteRequested);
        FilterPreset edited;
        edited.name = QStringLiteral("Rag chew");
        edited.low = 100;
        edited.high = 3100;
        store.setPreset(DSPMode::USB, 1, edited);
        QStringList keys;
        for (const QList<QVariant>& call : outbound) {
            keys << call.first().toString();
        }
        keys.sort();
        QCOMPARE(keys, (QStringList{QStringLiteral("filters/USB/1/high"),
                                    QStringLiteral("filters/USB/1/low"),
                                    QStringLiteral("filters/USB/1/name")}));

        // And this computer's own store was not touched.
        AppSettings::instance().setRemoteBackend(nullptr);
        const FilterPreset mine = store.presetsForMode(DSPMode::USB).first();
        QCOMPARE(mine.name, QStringLiteral("Local"));
        QCOMPARE(mine.low, 200);
        QVERIFY(!AppSettings::instance().contains(QStringLiteral("filters/USB/1/name")));
    }

    // A window running its radio locally has no remote backend: its presets
    // stay in its own settings, as before.
    void aLocalWindowIsUnchanged()
    {
        QCOMPARE(AppSettings::instance().remoteBackend(), nullptr);
        FilterPresetStore store;
        FilterPreset p;
        p.name = QStringLiteral("Mine");
        p.low = 300;
        p.high = 2400;
        store.setPreset(DSPMode::LSB, 2, p);
        QCOMPARE(AppSettings::instance().value(QStringLiteral("filters/LSB/2/name")).toString(),
                 QStringLiteral("Mine"));
        QCOMPARE(store.presetsForMode(DSPMode::LSB).at(2).low, 300);
    }
};

QTEST_MAIN(TstFilterPresetStore)
#include "tst_filter_preset_store.moc"
