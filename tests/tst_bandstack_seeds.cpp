// =================================================================
// tests/tst_bandstack_seeds.cpp  (NereusSDR)
// =================================================================
//
// Pins the band stack seed tables to Thetis clsBandStackManager.cs
// [v2.10.3.15]: row counts per routine, one sample row per routine, the
// region mapping and the seed lock rules.
//
// no-port-check: test cites upstream values as expected results; the port is BandStackSeeds.cpp.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-05 - New test for NereusSDR by J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include "core/safety/BandPlanGuard.h"
#include "models/Band.h"
#include "models/bandstack/BandStackSeeds.h"

using namespace NereusSDR;
using safety::Region;

namespace {

// Rows in each upstream routine, counted from clsBandStackManager.cs
// [v2.10.3.15] line ranges, commented-out rows excluded.
constexpr int kRegion1Count = 65; // 2022-2098
constexpr int kRegion2Count = 66; // 2099-2177
constexpr int kRegion3Count = 48; // 2178-2235
constexpr int kSwlCount     = 53; // 2236-2314
constexpr int kUkPlusCount  = 77; // 2315-2399
constexpr int kUsPlusCount  = 5;  // 2400-2411
constexpr int kSwedenCount  = 73; // 2412-2491

constexpr int kRegionValueCount = 24;

Region regionAt(int i)
{
    return static_cast<Region>(i);
}

bool hasRow(const QList<SeedRow>& rows, Band band, const QString& mode, int filterNumber,
            qint64 frequencyHz)
{
    for (const SeedRow& row : rows) {
        if (row.band == band && row.mode == mode && row.filterNumber == filterNumber
            && row.frequencyHz == frequencyHz) {
            return true;
        }
    }
    return false;
}

} // namespace

class TestBandStackSeeds : public QObject {
    Q_OBJECT

private slots:
    void regionsMapToTheRightRoutine_data()
    {
        QTest::addColumn<int>("region");
        QTest::addColumn<int>("count");

        const int region1Total = kRegion1Count + kSwlCount;
        const int region2Total = kRegion2Count + kSwlCount;
        const int region3Total = kRegion3Count + kSwlCount;

        const auto add = [](const char* name, Region region, int count) {
            QTest::newRow(name) << static_cast<int>(region) << count;
        };

        // The US drops Region2's one 60 m row and adds its five channels.
        add("United States", Region::UnitedStates,
            kRegion2Count - 1 + kUsPlusCount + kSwlCount);
        add("Australia", Region::Australia, region2Total);
        add("IARU Region 2", Region::Region2, region2Total);
        add("Japan", Region::Japan, region3Total);
        add("IARU Region 3", Region::Region3, region3Total);
        add("United Kingdom", Region::UnitedKingdom, kUkPlusCount + kSwlCount);
        add("Sweden", Region::Sweden, kSwedenCount + kSwlCount);

        add("Europe", Region::Europe, region1Total);
        add("India", Region::India, region1Total);
        add("Italy", Region::Italy, region1Total);
        add("Israel", Region::Israel, region1Total);
        add("Spain", Region::Spain, region1Total);
        add("Norway", Region::Norway, region1Total);
        add("Denmark", Region::Denmark, region1Total);
        add("Latvia", Region::Latvia, region1Total);
        add("Slovakia", Region::Slovakia, region1Total);
        add("Bulgaria", Region::Bulgaria, region1Total);
        add("Greece", Region::Greece, region1Total);
        add("Hungary", Region::Hungary, region1Total);
        add("Netherlands", Region::Netherlands, region1Total);
        add("France", Region::France, region1Total);
        add("Russia", Region::Russia, region1Total);
        add("IARU Region 1", Region::Region1, region1Total);
        add("Germany", Region::Germany, region1Total);
    }

    void regionsMapToTheRightRoutine()
    {
        QFETCH(int, region);
        QFETCH(int, count);
        QCOMPARE(seedsForRegion(static_cast<Region>(region)).size(), count);
    }

    void everyRegionHasRowsAndTheShortwaveList()
    {
        for (int i = 0; i < kRegionValueCount; ++i) {
            const QList<SeedRow> rows = seedsForRegion(regionAt(i));
            QVERIFY(!rows.isEmpty());
            QVERIFY(rows.size() > kSwlCount);

            // The list comes last, in source order: :2240 first, :2308 last.
            const SeedRow& first = rows.at(rows.size() - kSwlCount);
            QCOMPARE(first.band, Band::GEN);
            QCOMPARE(first.mode, QStringLiteral("SAM"));
            QCOMPARE(first.filterNumber, 4);
            QCOMPARE(first.frequencyHz, qint64(560000));
            const SeedRow& last = rows.constLast();
            QCOMPARE(last.band, Band::Band11m);
            QCOMPARE(last.mode, QStringLiteral("SAM"));
            QCOMPARE(last.filterNumber, 4);
            QCOMPARE(last.frequencyHz, qint64(27800000));
        }
    }

    void sampleRowFromEachRoutine()
    {
        // AddRegion1BandStack :2026
        QVERIFY(hasRow(seedsForRegion(Region::Europe), Band::Band160m,
                       QStringLiteral("CWL"), 1, 1810000));
        // AddRegion1BandStack :2042, a frequency written with five decimals.
        QVERIFY(hasRow(seedsForRegion(Region::Europe), Band::Band40m,
                       QStringLiteral("LSB"), 6, 7090000));

        // AddRegion2BandStack :2104, :2114
        const QList<SeedRow> region2 = seedsForRegion(Region::Region2);
        QVERIFY(hasRow(region2, Band::Band160m, QStringLiteral("LSB"), 6, 1840000));
        QVERIFY(hasRow(region2, Band::Band60m, QStringLiteral("USB"), 6, 5354000));

        // AddRegion3BandStack :2181, and F5 on WWV at :2218.
        const QList<SeedRow> region3 = seedsForRegion(Region::Region3);
        QVERIFY(hasRow(region3, Band::Band160m, QStringLiteral("CWL"), 1, 1820000));
        QVERIFY(hasRow(region3, Band::WWV, QStringLiteral("SAM"), 5, 2500000));

        // AddBandStackSWL :2246
        QVERIFY(hasRow(region3, Band::Band120m, QStringLiteral("SAM"), 4, 2400000));

        // AddUK_PlusBandStack :2328
        QVERIFY(hasRow(seedsForRegion(Region::UnitedKingdom), Band::Band60m,
                       QStringLiteral("USB"), 6, 5258500));

        // AddSwedenBandStack :2425
        QVERIFY(hasRow(seedsForRegion(Region::Sweden), Band::Band60m,
                       QStringLiteral("USB"), 6, 5310000));
    }

    void usGetsItsChannelsInPlaceOfRegion2s60mRow()
    {
        const QList<SeedRow> rows = seedsForRegion(Region::UnitedStates);

        // AddUS_PlusBandStack :2403-2407, each channel center less half of
        // 2800 Hz, in the order the source lists them.
        const QList<qint64> expected{5330600, 5346600, 5357100, 5371600, 5403600};
        QList<qint64> actual;
        for (const SeedRow& row : rows) {
            if (row.band == Band::Band60m) {
                QCOMPARE(row.mode, QStringLiteral("USB"));
                QCOMPARE(row.filterNumber, 6);
                actual.append(row.frequencyHz);
            }
        }
        QCOMPARE(actual, expected);
        QVERIFY(!hasRow(rows, Band::Band60m, QStringLiteral("USB"), 6, 5354000));
    }

    void japanUsesRegion3AndNotTheUnportedTable()
    {
        const QList<SeedRow> rows = seedsForRegion(Region::Japan);
        QVERIFY(hasRow(rows, Band::Band160m, QStringLiteral("CWL"), 1, 1820000));
        // First row of AddRegionJapanBandStack, whose call is commented out
        // at :1096.
        QVERIFY(!hasRow(rows, Band::Band160m, QStringLiteral("CWL"), 5, 1810000));
    }

    void lockRules()
    {
        // :2001-2002, WWV always and 60 m only in the United States.
        for (int i = 0; i < kRegionValueCount; ++i) {
            const Region region = regionAt(i);
            int wwvRows = 0;
            int sixtyRows = 0;
            for (const SeedRow& row : seedsForRegion(region)) {
                if (row.band == Band::WWV) {
                    ++wwvRows;
                    QVERIFY(row.locked);
                } else if (row.band == Band::Band60m) {
                    ++sixtyRows;
                    QCOMPARE(row.locked, region == Region::UnitedStates);
                } else {
                    QVERIFY(!row.locked);
                }
            }
            QVERIFY(wwvRows > 0);
            QVERIFY(sixtyRows > 0);
        }
    }

    void bandStringsAreMappedByThisTable()
    {
        const QList<QPair<QString, Band>> mapped{
            {QStringLiteral("160M"), Band::Band160m}, {QStringLiteral("80M"), Band::Band80m},
            {QStringLiteral("60M"), Band::Band60m},   {QStringLiteral("40M"), Band::Band40m},
            {QStringLiteral("30M"), Band::Band30m},   {QStringLiteral("20M"), Band::Band20m},
            {QStringLiteral("17M"), Band::Band17m},   {QStringLiteral("15M"), Band::Band15m},
            {QStringLiteral("12M"), Band::Band12m},   {QStringLiteral("10M"), Band::Band10m},
            {QStringLiteral("6M"), Band::Band6m},     {QStringLiteral("2M"), Band::Band2m},
            {QStringLiteral("120M"), Band::Band120m}, {QStringLiteral("90M"), Band::Band90m},
            {QStringLiteral("61M"), Band::Band61m},   {QStringLiteral("49M"), Band::Band49m},
            {QStringLiteral("41M"), Band::Band41m},   {QStringLiteral("31M"), Band::Band31m},
            {QStringLiteral("25M"), Band::Band25m},   {QStringLiteral("22M"), Band::Band22m},
            {QStringLiteral("19M"), Band::Band19m},   {QStringLiteral("16M"), Band::Band16m},
            {QStringLiteral("14M"), Band::Band14m},   {QStringLiteral("13M"), Band::Band13m},
            {QStringLiteral("11M"), Band::Band11m},   {QStringLiteral("WWV"), Band::WWV},
            {QStringLiteral("GEN"), Band::GEN},       {QStringLiteral("LMF"), Band::GEN},
        };
        for (const auto& [name, band] : mapped) {
            const std::optional<Band> result = seedBandFromName(name);
            QVERIFY2(result.has_value(), qPrintable(name));
            QCOMPARE(*result, band);
        }

        // No mapping is no band, never GEN.
        for (const QString& name : {QStringLiteral("XVTR"), QStringLiteral("4M"),
                                    QStringLiteral("VHF0"), QStringLiteral("40m"), QString()}) {
            QVERIFY2(!seedBandFromName(name).has_value(), qPrintable(name));
        }
    }

    void lmfRowsGoUnderGenAndNoRowIsXvtr()
    {
        for (int i = 0; i < kRegionValueCount; ++i) {
            const QList<SeedRow> rows = seedsForRegion(regionAt(i));
            for (const SeedRow& row : rows) {
                QVERIFY(row.band != Band::XVTR);
                QVERIFY(row.filterNumber >= 1 && row.filterNumber <= 10);
                QVERIFY(row.frequencyHz > 0);
            }
            // AddBandStackSWL :2240-2244
            for (const qint64 hz : {560000, 720000, 780000, 1000000, 1700000}) {
                QVERIFY(hasRow(rows, Band::GEN, QStringLiteral("SAM"), 4, hz));
            }
        }
    }

    void regionNamesMatchGeneralOptions()
    {
        QCOMPARE(regionName(Region::UnitedStates), QStringLiteral("United States"));
        QCOMPARE(regionName(Region::UnitedKingdom), QStringLiteral("United Kingdom"));
        QCOMPARE(regionName(Region::Region2), QStringLiteral("IARU Region 2"));

        QSet<QString> names;
        for (int i = 0; i < kRegionValueCount; ++i) {
            const QString name = regionName(regionAt(i));
            QVERIFY(!name.isEmpty());
            names.insert(name);
        }
        QCOMPARE(names.size(), kRegionValueCount);
    }
};

QTEST_MAIN(TestBandStackSeeds)
#include "tst_bandstack_seeds.moc"
