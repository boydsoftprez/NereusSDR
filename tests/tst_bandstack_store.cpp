// =================================================================
// tests/tst_bandstack_store.cpp  (NereusSDR)
// =================================================================
//
// The band stack's rules on their own: return modes, the cycle, saving on
// leave, adding, reorder, seeds, expiry and the document.
//
// no-port-check: test only; the port is BandStackStore.cpp.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-05 - New test for NereusSDR by J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>

#include "models/Band.h"
#include "models/bandstack/BandStackStore.h"

using namespace NereusSDR;

namespace {

const QChar kA(u'A');
const QChar kB(u'B');
const QChar kC(u'C');

constexpr qint64 kNow = 1000000;

BandStackEntry seed(const QString& id, Band band, qint64 frequencyHz,
                    const QString& mode = QStringLiteral("LSB"), bool locked = false)
{
    BandStackEntry entry;
    entry.id = id;
    entry.band = band;
    entry.frequencyHz = frequencyHz;
    entry.mode = mode;
    entry.filterLowHz = -2800;
    entry.filterHighHz = -100;
    entry.centerHz = frequencyHz;
    entry.locked = locked;
    return entry;
}

BandStackSpot spotAt(qint64 frequencyHz, const QString& mode = QStringLiteral("LSB"))
{
    return {frequencyHz, mode, -2700, -300, frequencyHz + 5000, 96000};
}

// a 7.100 CW, b 7.150 LSB, c 7.200 LSB, all on 40 m.
BandStackStore threeOn40()
{
    BandStackStore store;
    store.loadSeeds({seed(QStringLiteral("a"), Band::Band40m, 7100000, QStringLiteral("CWL")),
                     seed(QStringLiteral("b"), Band::Band40m, 7150000),
                     seed(QStringLiteral("c"), Band::Band40m, 7200000)});
    return store;
}

QStringList idsOn(const BandStackStore& store, Band band)
{
    QStringList ids;
    const QList<BandStackEntry> entries = store.entries(band);
    for (const BandStackEntry& entry : entries) {
        ids.append(entry.id);
    }
    return ids;
}

} // namespace

class TestBandStackStore : public QObject {
    Q_OBJECT

private slots:
    void pressFromAnotherBand_current_usesPlace()
    {
        BandStackStore store = threeOn40();
        store.setPlace(kA, Band::Band40m, QStringLiteral("c"));
        const BandStackTarget target =
            store.planPress(kA, Band::Band20m, Band::Band40m, BandStackDirection::Next);
        QCOMPARE(target.kind, BandStackTarget::Entry);
        QCOMPARE(target.entryId, QStringLiteral("c"));
        QCOMPARE(target.band, Band::Band40m);
    }

    void pressFromAnotherBand_current_noPlace_firstVoiceEntry()
    {
        BandStackStore store = threeOn40();
        const BandStackTarget target =
            store.planPress(kA, Band::Band20m, Band::Band40m, BandStackDirection::Next);
        QCOMPARE(target.kind, BandStackTarget::Entry);
        QCOMPARE(target.entryId, QStringLiteral("b"));
    }

    void pressFromAnotherBand_current_noVoiceEntry_firstEntry()
    {
        BandStackStore store;
        store.loadSeeds({seed(QStringLiteral("x"), Band::Band40m, 7030000, QStringLiteral("CWL")),
                         seed(QStringLiteral("y"), Band::Band40m, 7040000, QStringLiteral("DIGU"))});
        QCOMPARE(store.planPress(kA, Band::Band20m, Band::Band40m, BandStackDirection::Next).entryId,
                 QStringLiteral("x"));
    }

    void pressFromAnotherBand_currentMatchesPerSlice()
    {
        BandStackStore store = threeOn40();
        store.setPlace(kA, Band::Band40m, QStringLiteral("c"));
        store.setPlace(kB, Band::Band40m, QStringLiteral("a"));
        QCOMPARE(store.planPress(kB, Band::Band20m, Band::Band40m, BandStackDirection::Next).entryId,
                 QStringLiteral("a"));
    }

    void pressFromAnotherBand_lastUsed()
    {
        BandStackStore store = threeOn40();
        store.setPlace(kA, Band::Band40m, QStringLiteral("c"));
        QVERIFY(store.setReturn(Band::Band40m, BandStackReturnMode::LastUsed, QString()));
        const BandStackTarget target =
            store.planPress(kA, Band::Band20m, Band::Band40m, BandStackDirection::Next);
        QCOMPARE(target.kind, BandStackTarget::LastUsed);
        QCOMPARE(target.band, Band::Band40m);
    }

    void pressFromAnotherBand_lastUsedNeedsNoEntries()
    {
        BandStackStore store;
        QVERIFY(store.setReturn(Band::Band40m, BandStackReturnMode::LastUsed, QString()));
        QCOMPARE(store.planPress(kA, Band::Band20m, Band::Band40m, BandStackDirection::Next).kind,
                 BandStackTarget::LastUsed);
    }

    void pressFromAnotherBand_specific()
    {
        BandStackStore store = threeOn40();
        store.setPlace(kA, Band::Band40m, QStringLiteral("b"));
        QVERIFY(store.setReturn(Band::Band40m, BandStackReturnMode::Specific, QStringLiteral("a")));
        QCOMPARE(store.planPress(kA, Band::Band20m, Band::Band40m, BandStackDirection::Next).entryId,
                 QStringLiteral("a"));
    }

    void pressFromAnotherBand_specificDeleted_fallsToCurrent()
    {
        BandStackStore store = threeOn40();
        store.setPlace(kA, Band::Band40m, QStringLiteral("c"));
        QVERIFY(store.setReturn(Band::Band40m, BandStackReturnMode::Specific, QStringLiteral("a")));
        QVERIFY(store.remove(QStringLiteral("a")));
        QCOMPARE(store.planPress(kA, Band::Band20m, Band::Band40m, BandStackDirection::Next).entryId,
                 QStringLiteral("c"));

        const QJsonObject band =
            store.toJson().value(QStringLiteral("bands")).toObject().value(QStringLiteral("40m")).toObject();
        QCOMPARE(band.value(QStringLiteral("returnMode")).toString(), QStringLiteral("current"));
        QCOMPARE(band.value(QStringLiteral("specificId")).toString(), QString());
    }

    void setReturn_specificNeedsAnEntryOnTheBand()
    {
        BandStackStore store = threeOn40();
        store.loadSeeds({seed(QStringLiteral("a"), Band::Band40m, 7100000),
                         seed(QStringLiteral("z"), Band::Band20m, 14200000)});
        QVERIFY(!store.setReturn(Band::Band40m, BandStackReturnMode::Specific, QStringLiteral("z")));
        QVERIFY(!store.setReturn(Band::Band40m, BandStackReturnMode::Specific, QStringLiteral("gone")));
        QVERIFY(store.setReturn(Band::Band40m, BandStackReturnMode::Specific, QStringLiteral("a")));
    }

    void pressFromAnotherBand_emptyBand()
    {
        BandStackStore store = threeOn40();
        const BandStackTarget target =
            store.planPress(kA, Band::Band40m, Band::Band20m, BandStackDirection::Next);
        QCOMPARE(target.kind, BandStackTarget::EmptyBand);
        QCOMPARE(target.band, Band::Band20m);
    }

    void pressOnSameBand_nextAndPreviousWrap_data()
    {
        QTest::addColumn<int>("modeIndex");
        QTest::newRow("current") << 0;
        QTest::newRow("lastUsed") << 1;
        QTest::newRow("specific") << 2;
    }

    void pressOnSameBand_nextAndPreviousWrap()
    {
        QFETCH(int, modeIndex);
        BandStackStore store = threeOn40();
        if (modeIndex == 1) {
            QVERIFY(store.setReturn(Band::Band40m, BandStackReturnMode::LastUsed, QString()));
        } else if (modeIndex == 2) {
            QVERIFY(store.setReturn(Band::Band40m, BandStackReturnMode::Specific, QStringLiteral("b")));
        }

        const auto press = [&](BandStackDirection d) {
            const BandStackTarget target = store.planPress(kA, Band::Band40m, Band::Band40m, d);
            store.setPlace(kA, Band::Band40m, target.entryId);
            return target.entryId;
        };

        // No place starts from the top in either direction.
        QCOMPARE(press(BandStackDirection::Next), QStringLiteral("a"));
        QCOMPARE(press(BandStackDirection::Next), QStringLiteral("b"));
        QCOMPARE(press(BandStackDirection::Next), QStringLiteral("c"));
        QCOMPARE(press(BandStackDirection::Next), QStringLiteral("a"));
        QCOMPARE(press(BandStackDirection::Previous), QStringLiteral("c"));
        QCOMPARE(press(BandStackDirection::Previous), QStringLiteral("b"));

        store.clearPlace(kA, Band::Band40m);
        QCOMPARE(press(BandStackDirection::Previous), QStringLiteral("a"));
    }

    void pressOnSameBand_emptyBand()
    {
        BandStackStore store;
        QCOMPARE(store.planPress(kA, Band::Band40m, Band::Band40m, BandStackDirection::Next).kind,
                 BandStackTarget::EmptyBand);
    }

    void step_followsStoredOrderAndStaysOnBand()
    {
        BandStackStore store = threeOn40();
        QVERIFY(store.reorder(Band::Band40m,
                              {QStringLiteral("c"), QStringLiteral("a"), QStringLiteral("b")}));
        QCOMPARE(store.planStep(kA, Band::Band40m, BandStackDirection::Next).entryId,
                 QStringLiteral("c"));
        store.setPlace(kA, Band::Band40m, QStringLiteral("c"));
        QCOMPARE(store.planStep(kA, Band::Band40m, BandStackDirection::Next).entryId,
                 QStringLiteral("a"));
        QCOMPARE(store.planStep(kA, Band::Band40m, BandStackDirection::Previous).entryId,
                 QStringLiteral("b"));
    }

    void step_emptyBandIsNone()
    {
        BandStackStore store;
        QCOMPARE(store.planStep(kA, Band::Band40m, BandStackDirection::Next).kind,
                 BandStackTarget::None);
    }

    void saveOnLeave_writesAndClearsSeed()
    {
        BandStackStore store = threeOn40();
        store.setPlace(kA, Band::Band40m, QStringLiteral("b"));
        QVERIFY(store.saveOnLeave(kA, Band::Band40m, spotAt(7160000, QStringLiteral("USB")), kNow, {kA}));
        const BandStackEntry* entry = store.entry(QStringLiteral("b"));
        QVERIFY(entry != nullptr);
        QCOMPARE(entry->frequencyHz, qint64(7160000));
        QCOMPARE(entry->mode, QStringLiteral("USB"));
        QCOMPARE(entry->filterLowHz, -2700);
        QCOMPARE(entry->filterHighHz, -300);
        QCOMPARE(entry->centerHz, qint64(7165000));
        QCOMPARE(entry->spanHz, qint64(96000));
        QCOMPARE(entry->lastUsedMs, kNow);
        QVERIFY(!entry->seeded);
    }

    void saveOnLeave_unchangedKeepsSeedAndUpdatesLastUsed()
    {
        BandStackStore store = threeOn40();
        store.setPlace(kA, Band::Band40m, QStringLiteral("b"));
        const BandStackEntry before = *store.entry(QStringLiteral("b"));
        const BandStackSpot same{before.frequencyHz, before.mode, before.filterLowHz,
                                 before.filterHighHz, before.centerHz, before.spanHz};
        QVERIFY(store.saveOnLeave(kA, Band::Band40m, same, kNow, {kA}));
        QVERIFY(store.entry(QStringLiteral("b"))->seeded);
        QCOMPARE(store.entry(QStringLiteral("b"))->lastUsedMs, kNow);
    }

    // How the pan looked is saved, but it is not a change to the spot.
    void saveOnLeave_centerOrSpanAloneKeepsTheSeedAndSavesThem()
    {
        BandStackStore store = threeOn40();
        store.setPlace(kA, Band::Band40m, QStringLiteral("b"));
        const BandStackEntry before = *store.entry(QStringLiteral("b"));
        const BandStackSpot view{before.frequencyHz, before.mode, before.filterLowHz,
                                 before.filterHighHz, before.centerHz + 5000, 96000};
        QVERIFY(store.saveOnLeave(kA, Band::Band40m, view, kNow, {kA}));
        const BandStackEntry* entry = store.entry(QStringLiteral("b"));
        QVERIFY(entry->seeded);
        QCOMPARE(entry->centerHz, before.centerHz + 5000);
        QCOMPARE(entry->spanHz, qint64(96000));

        BandStackSpot filter = view;
        filter.filterHighHz = before.filterHighHz + 100;
        QVERIFY(store.saveOnLeave(kA, Band::Band40m, filter, kNow, {kA}));
        QVERIFY(!store.entry(QStringLiteral("b"))->seeded);
    }

    void replaceWithSeeds_skipsASeedWithin100HzOfAKeptEntry()
    {
        BandStackStore store;
        store.loadSeeds({seed(QStringLiteral("mine"), Band::Band40m, 7150000)});
        QVERIFY(store.saveOnLeave(kA, Band::Band40m, spotAt(7150000), kNow, {kA}) == false);
        store.setPlace(kA, Band::Band40m, QStringLiteral("mine"));
        QVERIFY(store.saveOnLeave(kA, Band::Band40m, spotAt(7150000), kNow, {kA}));
        QVERIFY(store.edit(QStringLiteral("mine"), std::nullopt, true));

        store.replaceWithSeeds({seed(QString(), Band::Band40m, 7150050),   // near: skipped
                                seed(QString(), Band::Band40m, 7150100),   // exactly 100: kept
                                seed(QString(), Band::Band20m, 7150050)}); // other band: kept
        QCOMPARE(store.entries(Band::Band40m).size(), qsizetype(2));
        QCOMPARE(store.entries(Band::Band20m).size(), qsizetype(1));
        QVERIFY(store.entry(QStringLiteral("mine")) != nullptr);
    }

    void planCurrent_isWhatCurrentWouldChoose()
    {
        BandStackStore store = threeOn40();
        QCOMPARE(store.planCurrent(kA, Band::Band40m).entryId, QStringLiteral("b")); // first voice
        store.setPlace(kA, Band::Band40m, QStringLiteral("c"));
        QCOMPARE(store.planCurrent(kA, Band::Band40m).entryId, QStringLiteral("c"));
        QCOMPARE(store.planCurrent(kA, Band::Band20m).kind, BandStackTarget::EmptyBand);
    }

    void markUsed_entryNear_specificId_bandFromKey()
    {
        BandStackStore store = threeOn40();
        store.markUsed(QStringLiteral("b"), kNow);
        QCOMPARE(store.entry(QStringLiteral("b"))->lastUsedMs, kNow);
        store.markUsed(QStringLiteral("gone"), kNow); // ignored

        QCOMPARE(store.entryNear(Band::Band40m, 7150099), QStringLiteral("b"));
        QCOMPARE(store.entryNear(Band::Band40m, 7150100), QString());
        QCOMPARE(store.entryNear(Band::Band20m, 7150000), QString());

        QCOMPARE(store.specificId(Band::Band40m), QString());
        QVERIFY(store.setReturn(Band::Band40m, BandStackReturnMode::Specific, QStringLiteral("c")));
        QCOMPARE(store.specificId(Band::Band40m), QStringLiteral("c"));

        QCOMPARE(BandStackStore::bandFromKey(QStringLiteral("31m")), std::optional<Band>(Band::Band31m));
        QVERIFY(!BandStackStore::bandFromKey(QStringLiteral("nope")).has_value());
    }

    void saveOnLeave_lockedIsNoOp()
    {
        BandStackStore store;
        store.loadSeeds({seed(QStringLiteral("w"), Band::WWV, 10000000, QStringLiteral("SAM"), true)});
        store.setPlace(kA, Band::WWV, QStringLiteral("w"));
        QVERIFY(!store.saveOnLeave(kA, Band::WWV, spotAt(10000500), kNow, {kA}));
        QCOMPARE(store.entry(QStringLiteral("w"))->frequencyHz, qint64(10000000));
        QCOMPARE(store.entry(QStringLiteral("w"))->lastUsedMs, qint64(0));
        QVERIFY(store.entry(QStringLiteral("w"))->seeded);
    }

    void saveOnLeave_sharedSpot_firstAThenB()
    {
        BandStackStore store = threeOn40();
        store.setPlace(kA, Band::Band40m, QStringLiteral("b"));
        store.setPlace(kB, Band::Band40m, QStringLiteral("b"));

        QVERIFY(!store.saveOnLeave(kA, Band::Band40m, spotAt(7160000), kNow, {kA, kB}));
        QCOMPARE(store.entry(QStringLiteral("b"))->frequencyHz, qint64(7150000));

        // B is now the last slice on the spot, and it does write.
        QVERIFY(store.saveOnLeave(kB, Band::Band40m, spotAt(7170000), kNow, {kB}));
        QCOMPARE(store.entry(QStringLiteral("b"))->frequencyHz, qint64(7170000));
    }

    void saveOnLeave_sharedSpot_firstBThenA()
    {
        BandStackStore store = threeOn40();
        store.setPlace(kA, Band::Band40m, QStringLiteral("b"));
        store.setPlace(kB, Band::Band40m, QStringLiteral("b"));

        QVERIFY(!store.saveOnLeave(kB, Band::Band40m, spotAt(7170000), kNow, {kA, kB}));
        QCOMPARE(store.entry(QStringLiteral("b"))->frequencyHz, qint64(7150000));

        QVERIFY(store.saveOnLeave(kA, Band::Band40m, spotAt(7160000), kNow, {kA}));
        QCOMPARE(store.entry(QStringLiteral("b"))->frequencyHz, qint64(7160000));
    }

    void saveOnLeave_otherSlicesPlaceButOffTheBandDoesNotBlock()
    {
        BandStackStore store = threeOn40();
        store.setPlace(kA, Band::Band40m, QStringLiteral("b"));
        store.setPlace(kB, Band::Band40m, QStringLiteral("b"));
        // B's place on 40 m is this entry, but B is on another band now.
        QVERIFY(store.saveOnLeave(kA, Band::Band40m, spotAt(7160000), kNow, {kA}));
    }

    void saveOnLeave_otherSliceOnBandElsewhereDoesNotBlock()
    {
        BandStackStore store = threeOn40();
        store.setPlace(kA, Band::Band40m, QStringLiteral("b"));
        store.setPlace(kB, Band::Band40m, QStringLiteral("c"));
        QVERIFY(store.saveOnLeave(kA, Band::Band40m, spotAt(7160000), kNow, {kA, kB}));
    }

    void saveOnLeave_duplicateWindow_data()
    {
        QTest::addColumn<qint64>("frequencyHz");
        QTest::addColumn<bool>("written");
        QTest::newRow("99 below c") << qint64(7199901) << false;
        QTest::newRow("exactly 100 below c") << qint64(7199900) << true;
        QTest::newRow("99 above c") << qint64(7200099) << false;
        QTest::newRow("exactly 100 above c") << qint64(7200100) << true;
    }

    void saveOnLeave_duplicateWindow()
    {
        QFETCH(qint64, frequencyHz);
        QFETCH(bool, written);
        BandStackStore store = threeOn40();
        store.setPlace(kA, Band::Band40m, QStringLiteral("b"));
        QCOMPARE(store.saveOnLeave(kA, Band::Band40m, spotAt(frequencyHz), kNow, {kA}), written);
        QCOMPARE(store.entry(QStringLiteral("b"))->frequencyHz, written ? frequencyHz : qint64(7150000));
        QCOMPARE(store.place(kA, Band::Band40m), QStringLiteral("b"));
    }

    void saveOnLeave_nearItsOwnFrequencyIsNotADuplicate()
    {
        BandStackStore store = threeOn40();
        store.setPlace(kA, Band::Band40m, QStringLiteral("b"));
        QVERIFY(store.saveOnLeave(kA, Band::Band40m, spotAt(7150050), kNow, {kA}));
    }

    void saveOnLeave_duplicateProtectionOff()
    {
        BandStackStore store = threeOn40();
        BandStackOptions options;
        options.dupeProtection = false;
        store.setOptions(options);
        store.setPlace(kA, Band::Band40m, QStringLiteral("b"));
        QVERIFY(store.saveOnLeave(kA, Band::Band40m, spotAt(7200000), kNow, {kA}));
    }

    void saveOnLeave_noPlaceIsNoOp()
    {
        BandStackStore store = threeOn40();
        QVERIFY(!store.saveOnLeave(kA, Band::Band40m, spotAt(7160000), kNow, {kA}));
    }

    void saveHere_refusesLockedAndUnknown()
    {
        BandStackStore store;
        store.loadSeeds({seed(QStringLiteral("w"), Band::WWV, 10000000, QStringLiteral("SAM"), true)});
        QVERIFY(!store.saveHere(kA, QStringLiteral("w"), spotAt(10000500), kNow));
        QCOMPARE(store.entry(QStringLiteral("w"))->frequencyHz, qint64(10000000));
        QCOMPARE(store.place(kA, Band::WWV), QString());
        QVERIFY(!store.saveHere(kA, QStringLiteral("gone"), spotAt(10000500), kNow));
    }

    void saveHere_writesOntoASharedSpotAndLeavesTheOtherSliceAlone()
    {
        BandStackStore store = threeOn40();
        store.setPlace(kA, Band::Band40m, QStringLiteral("a"));
        store.setPlace(kB, Band::Band40m, QStringLiteral("b"));
        QVERIFY(store.saveHere(kA, QStringLiteral("b"), spotAt(7160000), kNow));
        QCOMPARE(store.entry(QStringLiteral("b"))->frequencyHz, qint64(7160000));
        QVERIFY(!store.entry(QStringLiteral("b"))->seeded);
        QCOMPARE(store.place(kA, Band::Band40m), QStringLiteral("b"));
        QCOMPARE(store.place(kB, Band::Band40m), QStringLiteral("b"));
    }

    void add_goesInFrequencyOrderAndBecomesThePlace()
    {
        BandStackStore store = threeOn40();
        QString refusal;
        const QString id = store.add(kA, Band::Band40m, spotAt(7175000), kNow, &refusal);
        QVERIFY(!id.isEmpty());
        QVERIFY(refusal.isEmpty());
        QCOMPARE(idsOn(store, Band::Band40m),
                 QStringList({QStringLiteral("a"), QStringLiteral("b"), id, QStringLiteral("c")}));
        QCOMPARE(store.place(kA, Band::Band40m), id);
        const BandStackEntry* entry = store.entry(id);
        QVERIFY(!entry->seeded);
        QVERIFY(!entry->locked);
        QCOMPARE(entry->band, Band::Band40m);
        QCOMPARE(entry->lastUsedMs, kNow);

        const QString last = store.add(kA, Band::Band40m, spotAt(7250000), kNow, &refusal);
        QCOMPARE(idsOn(store, Band::Band40m).last(), last);
    }

    void add_onADraggedList()
    {
        BandStackStore store = threeOn40();
        QVERIFY(store.reorder(Band::Band40m,
                              {QStringLiteral("c"), QStringLiteral("a"), QStringLiteral("b")}));
        // The first entry with a higher frequency is c, at the top.
        const QString first = store.add(kA, Band::Band40m, spotAt(7120000), kNow, nullptr);
        QCOMPARE(idsOn(store, Band::Band40m),
                 QStringList({first, QStringLiteral("c"), QStringLiteral("a"), QStringLiteral("b")}));
        const QString last = store.add(kA, Band::Band40m, spotAt(7300000), kNow, nullptr);
        QCOMPARE(idsOn(store, Band::Band40m).last(), last);
    }

    void add_duplicateIsRefused()
    {
        BandStackStore store = threeOn40();
        store.setPlace(kA, Band::Band40m, QStringLiteral("a"));
        QString refusal;
        QCOMPARE(store.add(kA, Band::Band40m, spotAt(7150099), kNow, &refusal), QString());
        QCOMPARE(refusal, QStringLiteral("7.150.099 is already in 40m"));
        QCOMPARE(store.add(kA, Band::Band40m, spotAt(7150000), kNow, &refusal), QString());
        QCOMPARE(refusal, QStringLiteral("7.150 is already in 40m"));
        QCOMPARE(store.add(kA, Band::Band40m, spotAt(7150000), kNow, nullptr), QString());
        QCOMPARE(store.entries(Band::Band40m).size(), qsizetype(3));
        QCOMPARE(store.place(kA, Band::Band40m), QStringLiteral("a"));

        QVERIFY(!store.add(kA, Band::Band40m, spotAt(7150100), kNow, &refusal).isEmpty());
    }

    void add_duplicateProtectionOff()
    {
        BandStackStore store = threeOn40();
        BandStackOptions options;
        options.dupeProtection = false;
        store.setOptions(options);
        QVERIFY(!store.add(kA, Band::Band40m, spotAt(7150000), kNow, nullptr).isEmpty());
    }

    void addDwell_movesNoPlace()
    {
        BandStackStore store = threeOn40();
        store.setPlace(kA, Band::Band40m, QStringLiteral("a"));
        QVERIFY(store.addDwell(Band::Band40m, spotAt(7175000), kNow));
        QCOMPARE(store.entries(Band::Band40m).size(), qsizetype(4));
        QCOMPARE(store.place(kA, Band::Band40m), QStringLiteral("a"));
        QCOMPARE(store.place(kB, Band::Band40m), QString());
    }

    void addDwell_skipsDuplicates()
    {
        BandStackStore store = threeOn40();
        QVERIFY(!store.addDwell(Band::Band40m, spotAt(7150099), kNow));
        // Duplicate protection also covers another mode.
        QVERIFY(!store.addDwell(Band::Band40m, spotAt(7150050, QStringLiteral("USB")), kNow));

        BandStackOptions options;
        options.dupeProtection = false;
        store.setOptions(options);
        QVERIFY(!store.addDwell(Band::Band40m, spotAt(7150099), kNow));
        QVERIFY(store.addDwell(Band::Band40m, spotAt(7150050, QStringLiteral("USB")), kNow));
        QVERIFY(store.addDwell(Band::Band40m, spotAt(7150100), kNow));
    }

    void reorder_refusesAnythingButTheBandsEntries()
    {
        BandStackStore store = threeOn40();
        const QStringList before = idsOn(store, Band::Band40m);
        QVERIFY(!store.reorder(Band::Band40m, {QStringLiteral("a"), QStringLiteral("b")}));
        QVERIFY(!store.reorder(Band::Band40m, {QStringLiteral("a"), QStringLiteral("b"),
                                               QStringLiteral("x")}));
        QVERIFY(!store.reorder(Band::Band40m, {QStringLiteral("a"), QStringLiteral("a"),
                                               QStringLiteral("b")}));
        QVERIFY(!store.reorder(Band::Band40m, {QStringLiteral("a"), QStringLiteral("b"),
                                               QStringLiteral("c"), QStringLiteral("c")}));
        QCOMPARE(idsOn(store, Band::Band40m), before);
        QVERIFY(store.reorder(Band::Band40m, {QStringLiteral("b"), QStringLiteral("c"),
                                              QStringLiteral("a")}));
        QCOMPARE(idsOn(store, Band::Band40m),
                 QStringList({QStringLiteral("b"), QStringLiteral("c"), QStringLiteral("a")}));
    }

    void remove_clearsPlaces()
    {
        BandStackStore store = threeOn40();
        store.setPlace(kA, Band::Band40m, QStringLiteral("b"));
        store.setPlace(kB, Band::Band40m, QStringLiteral("b"));
        store.setPlace(kC, Band::Band40m, QStringLiteral("c"));
        QVERIFY(store.remove(QStringLiteral("b")));
        QCOMPARE(store.place(kA, Band::Band40m), QString());
        QCOMPARE(store.place(kB, Band::Band40m), QString());
        QCOMPARE(store.place(kC, Band::Band40m), QStringLiteral("c"));
        QVERIFY(store.entry(QStringLiteral("b")) == nullptr);
        QVERIFY(!store.remove(QStringLiteral("b")));
    }

    void remove_lockedEntryCanBeDeleted()
    {
        BandStackStore store;
        store.loadSeeds({seed(QStringLiteral("w"), Band::WWV, 10000000, QStringLiteral("SAM"), true)});
        QVERIFY(store.remove(QStringLiteral("w")));
    }

    void setPlace_refusesAnEntryOffTheBand()
    {
        BandStackStore store = threeOn40();
        store.setPlace(kA, Band::Band20m, QStringLiteral("a"));
        QCOMPARE(store.place(kA, Band::Band20m), QString());
        store.setPlace(kA, Band::Band40m, QStringLiteral("gone"));
        QCOMPARE(store.place(kA, Band::Band40m), QString());
    }

    void clearUnlocked_keepsLocked()
    {
        BandStackStore store;
        store.loadSeeds({seed(QStringLiteral("a"), Band::Band40m, 7100000),
                         seed(QStringLiteral("l"), Band::Band40m, 7150000, QStringLiteral("LSB"), true),
                         seed(QStringLiteral("z"), Band::Band20m, 14200000)});
        store.setPlace(kA, Band::Band40m, QStringLiteral("a"));
        QCOMPARE(store.clearUnlocked(Band::Band40m), 1);
        QCOMPARE(idsOn(store, Band::Band40m), QStringList({QStringLiteral("l")}));
        QCOMPARE(store.place(kA, Band::Band40m), QString());
        QCOMPARE(idsOn(store, Band::Band20m), QStringList({QStringLiteral("z")}));
        QCOMPARE(store.clearUnlocked(std::nullopt), 1);
        QVERIFY(idsOn(store, Band::Band20m).isEmpty());
        QCOMPARE(idsOn(store, Band::Band40m), QStringList({QStringLiteral("l")}));
    }

    void expire_skipsLockedSeededAndPlaces()
    {
        BandStackStore store;
        store.loadSeeds({seed(QStringLiteral("s"), Band::Band40m, 7100000),
                         seed(QStringLiteral("l"), Band::Band40m, 7300000, QStringLiteral("LSB"), true)});
        BandStackOptions options;
        options.expiryMinutes = 5;
        store.setOptions(options);

        const qint64 windowMs = 5 * 60000;
        const QString old = store.add(kA, Band::Band40m, spotAt(7150000), kNow, nullptr);
        const QString fresh = store.add(kB, Band::Band40m, spotAt(7160000), kNow + 1, nullptr);
        const QString held = store.add(kC, Band::Band40m, spotAt(7170000), kNow, nullptr);
        store.clearPlace(kA, Band::Band40m);
        store.clearPlace(kB, Band::Band40m);

        const QList<BandStackEntry> removed = store.expire(kNow + windowMs);
        QCOMPARE(removed.size(), qsizetype(1));
        QCOMPARE(removed.first().id, old);
        QVERIFY(store.entry(old) == nullptr);
        QVERIFY(store.entry(fresh) != nullptr);
        QVERIFY(store.entry(held) != nullptr);
        QVERIFY(store.entry(QStringLiteral("s")) != nullptr);
        QVERIFY(store.entry(QStringLiteral("l")) != nullptr);
    }

    void expire_offRemovesNothing()
    {
        BandStackStore store;
        const QString id = store.add(kA, Band::Band40m, spotAt(7150000), kNow, nullptr);
        store.clearPlace(kA, Band::Band40m);
        QVERIFY(store.expire(kNow + 1000 * 60000).isEmpty());
        QVERIFY(store.entry(id) != nullptr);
    }

    void restore_putsEntriesBackInFrequencyOrder()
    {
        BandStackStore store = threeOn40();
        QVERIFY(store.reorder(Band::Band40m,
                              {QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")}));
        BandStackOptions options;
        options.expiryMinutes = 5;
        store.setOptions(options);
        const QString low = store.add(kA, Band::Band40m, spotAt(7050000), kNow, nullptr);
        const QString mid = store.add(kA, Band::Band40m, spotAt(7125000), kNow, nullptr);
        store.clearPlace(kA, Band::Band40m);

        const QList<BandStackEntry> removed = store.expire(kNow + 5 * 60000);
        QCOMPARE(removed.size(), qsizetype(2));
        QCOMPARE(idsOn(store, Band::Band40m),
                 QStringList({QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")}));

        store.restore(removed);
        QCOMPARE(idsOn(store, Band::Band40m),
                 QStringList({low, QStringLiteral("a"), mid, QStringLiteral("b"), QStringLiteral("c")}));

        store.restore(removed); // already back
        QCOMPARE(store.entries(Band::Band40m).size(), qsizetype(5));
    }

    void loadSeeds_keepsSeedLocksAndSorts()
    {
        BandStackStore store;
        store.loadSeeds({seed(QStringLiteral("high"), Band::Band60m, 5400000),
                         seed(QStringLiteral("low"), Band::Band60m, 5300000, QStringLiteral("USB"), true),
                         seed(QString(), Band::Band40m, 7100000)});
        QCOMPARE(idsOn(store, Band::Band60m),
                 QStringList({QStringLiteral("low"), QStringLiteral("high")}));
        QVERIFY(store.entry(QStringLiteral("low"))->locked);
        QVERIFY(store.entry(QStringLiteral("low"))->seeded);
        QVERIFY(store.entry(QStringLiteral("high"))->seeded);
        QVERIFY(!store.entries(Band::Band40m).first().id.isEmpty());
    }

    void loadSeeds_replacesEverything()
    {
        BandStackStore store = threeOn40();
        store.setPlace(kA, Band::Band40m, QStringLiteral("a"));
        store.loadSeeds({seed(QStringLiteral("z"), Band::Band20m, 14200000)});
        QVERIFY(idsOn(store, Band::Band40m).isEmpty());
        QCOMPARE(store.place(kA, Band::Band40m), QString());
    }

    void replaceWithSeeds_keepsOnlyLockedNonSeeds()
    {
        BandStackStore store;
        store.loadSeeds({seed(QStringLiteral("plain"), Band::Band40m, 7100000),
                         seed(QStringLiteral("seedlock"), Band::WWV, 10000000, QStringLiteral("SAM"), true),
                         seed(QStringLiteral("mine"), Band::Band40m, 7200000)});
        QVERIFY(store.edit(QStringLiteral("mine"), std::nullopt, true));
        const QString added = store.add(kA, Band::Band40m, spotAt(7150000), kNow, nullptr);
        store.setPlace(kA, Band::WWV, QStringLiteral("seedlock"));
        store.setPlace(kB, Band::Band40m, QStringLiteral("mine"));
        store.setPlace(kC, Band::Band40m, QStringLiteral("plain"));

        store.replaceWithSeeds({seed(QStringLiteral("new1"), Band::Band40m, 7050000),
                                seed(QStringLiteral("new2"), Band::Band20m, 14200000)});

        QVERIFY(store.entry(QStringLiteral("plain")) == nullptr);
        QVERIFY(store.entry(QStringLiteral("seedlock")) == nullptr);
        QVERIFY(store.entry(added) == nullptr);
        QVERIFY(store.entry(QStringLiteral("mine")) != nullptr);
        QCOMPARE(idsOn(store, Band::Band40m),
                 QStringList({QStringLiteral("new1"), QStringLiteral("mine")}));
        QVERIFY(store.entry(QStringLiteral("new1"))->seeded);

        QCOMPARE(store.place(kA, Band::Band40m), QString());
        QCOMPARE(store.place(kA, Band::WWV), QString());
        QCOMPARE(store.place(kC, Band::Band40m), QString());
        QCOMPARE(store.place(kB, Band::Band40m), QStringLiteral("mine"));
    }

    void edit_lockingByHandEndsTheSeed()
    {
        BandStackStore store = threeOn40();
        QVERIFY(store.edit(QStringLiteral("a"), QStringLiteral("Net"), std::nullopt));
        QCOMPARE(store.entry(QStringLiteral("a"))->name, QStringLiteral("Net"));
        QVERIFY(store.entry(QStringLiteral("a"))->seeded);

        QVERIFY(store.edit(QStringLiteral("a"), std::nullopt, true));
        QVERIFY(store.entry(QStringLiteral("a"))->locked);
        QVERIFY(!store.entry(QStringLiteral("a"))->seeded);

        QVERIFY(store.edit(QStringLiteral("a"), std::nullopt, false));
        QVERIFY(!store.entry(QStringLiteral("a"))->locked);
        QVERIFY(!store.edit(QStringLiteral("gone"), QStringLiteral("x"), std::nullopt));
    }

    void edit_repeatingASeedsLockKeepsTheSeed()
    {
        BandStackStore store;
        store.loadSeeds({seed(QStringLiteral("w"), Band::WWV, 10000000, QStringLiteral("SAM"), true)});
        QVERIFY(store.edit(QStringLiteral("w"), QStringLiteral("Time"), true));
        QVERIFY(store.entry(QStringLiteral("w"))->seeded);
    }

    void options_refuseValuesTheDocumentDoesNotAllow()
    {
        BandStackStore store;
        BandStackOptions options;
        options.dupeProtection = false;
        options.expiryMinutes = 15;
        options.dwellSeconds = 30;
        store.setOptions(options);

        options.expiryMinutes = 7;
        options.dwellSeconds = 11;
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("expiry")));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("dwell")));
        store.setOptions(options);
        QCOMPARE(store.options().expiryMinutes, 15);
        QCOMPARE(store.options().dwellSeconds, 30);
        QCOMPARE(store.options().dupeProtection, false);
    }

    void json_roundTripsExactly()
    {
        BandStackStore store;
        store.setRegion(QStringLiteral("United States"));
        store.loadSeeds({seed(QStringLiteral("a"), Band::Band40m, 7100000),
                         seed(QStringLiteral("b"), Band::Band40m, 7150000),
                         seed(QStringLiteral("w"), Band::WWV, 10000000, QStringLiteral("SAM"), true),
                         seed(QStringLiteral("swl"), Band::Band49m, 6000000, QStringLiteral("SAM")),
                         seed(QStringLiteral("two"), Band::Band2m, 144200000)});
        BandStackOptions options;
        options.dupeProtection = false;
        options.expiryMinutes = 30;
        options.dwellSeconds = 60;
        store.setOptions(options);
        QVERIFY(store.reorder(Band::Band40m, {QStringLiteral("b"), QStringLiteral("a")}));
        QVERIFY(store.setReturn(Band::Band40m, BandStackReturnMode::Specific, QStringLiteral("a")));
        QVERIFY(store.setReturn(Band::Band20m, BandStackReturnMode::LastUsed, QString()));
        QVERIFY(store.edit(QStringLiteral("a"), QStringLiteral("Net"), std::nullopt));
        const QString added = store.add(kA, Band::Band40m, spotAt(7175000), kNow, nullptr);
        store.setPlace(kB, Band::WWV, QStringLiteral("w"));

        const QJsonObject document = store.toJson();
        BandStackStore loaded;
        QVERIFY(loaded.fromJson(document));
        QCOMPARE(QJsonDocument(loaded.toJson()).toJson(), QJsonDocument(document).toJson());

        QCOMPARE(loaded.region(), QStringLiteral("United States"));
        QCOMPARE(loaded.options().expiryMinutes, 30);
        QCOMPARE(loaded.options().dwellSeconds, 60);
        QCOMPARE(loaded.options().dupeProtection, false);
        QCOMPARE(idsOn(loaded, Band::Band40m), idsOn(store, Band::Band40m));
        QCOMPARE(loaded.place(kA, Band::Band40m), added);
        QCOMPARE(loaded.place(kB, Band::WWV), QStringLiteral("w"));
        QCOMPARE(loaded.entry(QStringLiteral("a"))->name, QStringLiteral("Net"));
        QCOMPARE(loaded.entry(added)->spanHz, qint64(96000));
        QCOMPARE(loaded.entry(added)->lastUsedMs, kNow);
        QVERIFY(loaded.entry(QStringLiteral("w"))->locked);
        QVERIFY(loaded.entry(QStringLiteral("w"))->seeded);
        QCOMPARE(loaded.planPress(kC, Band::Band20m, Band::Band40m, BandStackDirection::Next).entryId,
                 QStringLiteral("a"));
        QCOMPARE(loaded.planPress(kC, Band::Band40m, Band::Band20m, BandStackDirection::Next).kind,
                 BandStackTarget::LastUsed);
    }

    void json_everyBandRoundTrips()
    {
        QList<BandStackEntry> seeds;
        for (int i = 0; i < static_cast<int>(Band::Count); ++i) {
            seeds.append(seed(QString::number(i), static_cast<Band>(i), 1000000 + i));
        }
        BandStackStore store;
        store.loadSeeds(seeds);

        BandStackStore loaded;
        QVERIFY(loaded.fromJson(store.toJson()));
        for (const BandStackEntry& entry : std::as_const(seeds)) {
            QCOMPARE(idsOn(loaded, entry.band), QStringList({entry.id}));
        }
    }

    void json_hasExactlyTheSpecKeys()
    {
        BandStackStore store = threeOn40();
        store.setRegion(QStringLiteral("United States"));
        store.setPlace(kA, Band::Band40m, QStringLiteral("a"));
        const QJsonObject document = store.toJson();

        QStringList top = document.keys();
        top.sort();
        QCOMPARE(top, QStringList({QStringLiteral("bands"), QStringLiteral("entries"),
                                   QStringLiteral("options"), QStringLiteral("places"),
                                   QStringLiteral("region"), QStringLiteral("schema")}));
        QCOMPARE(document.value(QStringLiteral("schema")).toInt(), 1);

        const QJsonObject options = document.value(QStringLiteral("options")).toObject();
        QCOMPARE(options.size(), qsizetype(3));
        QCOMPARE(options.value(QStringLiteral("dupeProtection")).toBool(), true);
        QCOMPARE(options.value(QStringLiteral("expiryMinutes")).toInt(-1), 0);
        QCOMPARE(options.value(QStringLiteral("dwellSeconds")).toInt(-1), 0);

        const QJsonObject band =
            document.value(QStringLiteral("bands")).toObject().value(QStringLiteral("40m")).toObject();
        QStringList bandKeys = band.keys();
        bandKeys.sort();
        QCOMPARE(bandKeys, QStringList({QStringLiteral("order"), QStringLiteral("returnMode"),
                                        QStringLiteral("specificId")}));

        const QJsonObject entry = document.value(QStringLiteral("entries")).toArray().at(0).toObject();
        QStringList entryKeys = entry.keys();
        entryKeys.sort();
        QCOMPARE(entryKeys,
                 QStringList({QStringLiteral("band"), QStringLiteral("centerHz"),
                              QStringLiteral("filterHighHz"), QStringLiteral("filterLowHz"),
                              QStringLiteral("frequencyHz"), QStringLiteral("id"),
                              QStringLiteral("lastUsedMs"), QStringLiteral("locked"),
                              QStringLiteral("mode"), QStringLiteral("name"),
                              QStringLiteral("seeded"), QStringLiteral("spanHz")}));

        QCOMPARE(document.value(QStringLiteral("places"))
                     .toObject()
                     .value(QStringLiteral("A"))
                     .toObject()
                     .value(QStringLiteral("40m"))
                     .toString(),
                 QStringLiteral("a"));
    }

    void json_badDocument_data()
    {
        BandStackStore good = threeOn40();
        good.setRegion(QStringLiteral("Japan"));
        const QJsonObject document = good.toJson();

        QTest::addColumn<QJsonObject>("bad");

        QJsonObject d = document;
        d.insert(QStringLiteral("schema"), 2);
        QTest::newRow("unknown schema") << d;

        d = document;
        d.remove(QStringLiteral("schema"));
        QTest::newRow("no schema") << d;

        d = document;
        d.insert(QStringLiteral("schema"), QStringLiteral("1"));
        QTest::newRow("schema is a string") << d;

        for (const char* key : {"region", "options", "bands", "entries", "places"}) {
            d = document;
            d.remove(QLatin1String(key));
            QTest::addRow("no %s", key) << d;
            d.insert(QLatin1String(key), 3);
            QTest::addRow("%s is a number", key) << d;
        }

        const QJsonArray entries = document.value(QStringLiteral("entries")).toArray();
        const auto withEntry = [&](const QJsonObject& entry) {
            QJsonObject result = document;
            QJsonArray array = entries;
            array.replace(0, entry);
            result.insert(QStringLiteral("entries"), array);
            return result;
        };
        QJsonObject e = entries.at(0).toObject();
        e.insert(QStringLiteral("band"), QStringLiteral("nope"));
        QTest::newRow("entry on an unknown band") << withEntry(e);
        e = entries.at(0).toObject();
        e.remove(QStringLiteral("frequencyHz"));
        QTest::newRow("entry without a frequency") << withEntry(e);
        e = entries.at(0).toObject();
        e.insert(QStringLiteral("frequencyHz"), 7.5);
        QTest::newRow("entry with a fractional frequency") << withEntry(e);
        e = entries.at(0).toObject();
        e.insert(QStringLiteral("id"), entries.at(1).toObject().value(QStringLiteral("id")));
        QTest::newRow("repeated entry id") << withEntry(e);
        e = entries.at(0).toObject();
        e.insert(QStringLiteral("id"), QString());
        QTest::newRow("empty entry id") << withEntry(e);

        QJsonObject bands = document.value(QStringLiteral("bands")).toObject();
        QJsonObject band = bands.value(QStringLiteral("40m")).toObject();
        band.insert(QStringLiteral("returnMode"), QStringLiteral("first"));
        bands.insert(QStringLiteral("40m"), band);
        d = document;
        d.insert(QStringLiteral("bands"), bands);
        QTest::newRow("unknown return mode") << d;

        QJsonObject options = document.value(QStringLiteral("options")).toObject();
        options.insert(QStringLiteral("expiryMinutes"), 7);
        d = document;
        d.insert(QStringLiteral("options"), options);
        QTest::newRow("expiry not allowed") << d;

        options = document.value(QStringLiteral("options")).toObject();
        options.insert(QStringLiteral("dwellSeconds"), 11);
        d = document;
        d.insert(QStringLiteral("options"), options);
        QTest::newRow("dwell not allowed") << d;

        QJsonObject places;
        places.insert(QStringLiteral("AB"), QJsonObject());
        d = document;
        d.insert(QStringLiteral("places"), places);
        QTest::newRow("place on a long slice name") << d;
    }

    void json_badDocument()
    {
        QFETCH(QJsonObject, bad);
        BandStackStore store;
        store.setRegion(QStringLiteral("United Kingdom"));
        store.loadSeeds({seed(QStringLiteral("keep"), Band::Band20m, 14200000)});
        const QJsonObject before = store.toJson();
        QVERIFY(!store.fromJson(bad));
        QCOMPARE(store.toJson(), before);
    }

    void json_noOrderYet_rebuiltInFrequencyOrder()
    {
        BandStackStore store;
        store.loadSeeds({seed(QStringLiteral("a"), Band::Band40m, 7100000),
                         seed(QStringLiteral("b"), Band::Band40m, 7150000),
                         seed(QStringLiteral("c"), Band::Band40m, 7200000)});
        QJsonObject document = store.toJson();

        QJsonArray reversed;
        const QJsonArray entries = document.value(QStringLiteral("entries")).toArray();
        for (qsizetype i = entries.size() - 1; i >= 0; --i) {
            reversed.append(entries.at(i));
        }
        document.insert(QStringLiteral("entries"), reversed);
        document.insert(QStringLiteral("bands"), QJsonObject());

        BandStackStore loaded;
        QVERIFY(loaded.fromJson(document));
        QCOMPARE(idsOn(loaded, Band::Band40m),
                 QStringList({QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")}));
    }

    void json_entryMissingFromADraggedOrder_goesWhereANewOneWould()
    {
        BandStackStore store = threeOn40();
        QVERIFY(store.reorder(Band::Band40m,
                              {QStringLiteral("c"), QStringLiteral("b"), QStringLiteral("a")}));
        QJsonObject document = store.toJson();
        QJsonObject bands = document.value(QStringLiteral("bands")).toObject();
        QJsonObject band = bands.value(QStringLiteral("40m")).toObject();
        band.insert(QStringLiteral("order"),
                    QJsonArray({QStringLiteral("c"), QStringLiteral("a")}));
        bands.insert(QStringLiteral("40m"), band);
        document.insert(QStringLiteral("bands"), bands);

        BandStackStore loaded;
        QVERIFY(loaded.fromJson(document));
        // b, 7.150, goes before the first entry above it, which is c at the top.
        QCOMPARE(idsOn(loaded, Band::Band40m),
                 QStringList({QStringLiteral("b"), QStringLiteral("c"), QStringLiteral("a")}));
    }

    void json_danglingReferencesAreDropped()
    {
        BandStackStore store = threeOn40();
        QVERIFY(store.setReturn(Band::Band40m, BandStackReturnMode::Specific, QStringLiteral("a")));
        store.setPlace(kA, Band::Band40m, QStringLiteral("a"));
        store.setPlace(kB, Band::Band40m, QStringLiteral("b"));
        QJsonObject document = store.toJson();

        // Take entry a out and leave every reference to it.
        QJsonArray entries;
        const QJsonArray all = document.value(QStringLiteral("entries")).toArray();
        for (const QJsonValue& value : all) {
            if (value.toObject().value(QStringLiteral("id")).toString() != QLatin1String("a")) {
                entries.append(value);
            }
        }
        document.insert(QStringLiteral("entries"), entries);

        BandStackStore loaded;
        QVERIFY(loaded.fromJson(document));
        QCOMPARE(idsOn(loaded, Band::Band40m),
                 QStringList({QStringLiteral("b"), QStringLiteral("c")}));
        QCOMPARE(loaded.place(kA, Band::Band40m), QString());
        QCOMPARE(loaded.place(kB, Band::Band40m), QStringLiteral("b"));
        const QJsonObject band = loaded.toJson()
                                     .value(QStringLiteral("bands"))
                                     .toObject()
                                     .value(QStringLiteral("40m"))
                                     .toObject();
        QCOMPARE(band.value(QStringLiteral("returnMode")).toString(), QStringLiteral("current"));
        QCOMPARE(band.value(QStringLiteral("specificId")).toString(), QString());
    }
};

QTEST_MAIN(TestBandStackStore)
#include "tst_bandstack_store.moc"
