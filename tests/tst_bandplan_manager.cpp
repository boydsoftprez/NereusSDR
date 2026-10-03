// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>
#include <QDir>
#include <QFile>
#include <QLoggingCategory>
#include <QSignalSpy>

#include "models/BandPlan.h"
#include "models/BandPlanManager.h"

using namespace NereusSDR;

class TestBandPlanManager : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    // Value types
    void segment_defaultIsEmpty();
    void spot_defaultIsEmpty();

    // R-IOS-27, R-IOS-11: the lowest licence class, one rule for the
    // band-plan strip and the station catalogue
    void lowestLicenceClass_data();
    void lowestLicenceClass();
    void lowestLicenceClass_stripLabelsUnchanged();

    // Loader: bundled resources
    void loadPlans_findsAllFiveRegions();
    void loadPlans_arrlUsHasSegmentsAndSpots();
    void loadPlans_iaruRegion2HasSegments();

    // Active plan
    void setActivePlan_emitsPlanChanged();
    void setActivePlan_unknownNameDoesNothing();
    void setActivePlan_segmentsReflectActivePlan();

    // Default selection
    void loadPlans_defaultsToArrlUs();

    // R-R3-10 / R-R3-21: a Core-only program loads the full set
    void coreOnlyLink_hasNoGuiResources();
    void coreOnlyLink_bandPlansMatchSourceFiles();
};

void TestBandPlanManager::initTestCase()
{
    // Resource init runs at static-init time; nothing to do here.
    // loadPlans() logs its count once per call (the nereusd start-up
    // proof, R-R3-10); keep that line out of the per-case output.
    QLoggingCategory::setFilterRules(QStringLiteral("nereussdr.bandplan.info=false"));
}

void TestBandPlanManager::segment_defaultIsEmpty()
{
    BandSegment s;
    QCOMPARE(s.lowMhz, 0.0);
    QCOMPARE(s.highMhz, 0.0);
    QVERIFY(s.label.isEmpty());
    QVERIFY(!s.color.isValid());
}

void TestBandPlanManager::spot_defaultIsEmpty()
{
    BandSpot s;
    QCOMPARE(s.freqMhz, 0.0);
    QVERIFY(s.label.isEmpty());
}

void TestBandPlanManager::lowestLicenceClass_data()
{
    QTest::addColumn<QString>("licence");
    QTest::addColumn<QString>("expected");
    QTest::newRow("T,G,E") << QStringLiteral("T,G,E") << QStringLiteral("Tech");
    QTest::newRow("E,G,T") << QStringLiteral("E,G,T") << QStringLiteral("Tech");
    QTest::newRow("E,G")   << QStringLiteral("E,G")   << QStringLiteral("General");
    QTest::newRow("E")     << QStringLiteral("E")     << QStringLiteral("Extra");
    QTest::newRow("empty") << QString()               << QString();
    QTest::newRow("unknown code") << QStringLiteral("A") << QString();
}

void TestBandPlanManager::lowestLicenceClass()
{
    QFETCH(QString, licence);
    QFETCH(QString, expected);
    QCOMPARE(NereusSDR::lowestLicenceClass(licence), expected);
}

// The strip's label for every segment of every bundled plan is what the
// rule SpectrumWidget held inline before the move gave: "<label> <class>"
// when there is room and a class, the bare label otherwise.
void TestBandPlanManager::lowestLicenceClass_stripLabelsUnchanged()
{
    const auto inlineRule = [](const QString& lic) {
        QString lowestClass;
        if      (lic.contains(QLatin1Char('T'))) { lowestClass = QStringLiteral("Tech"); }
        else if (lic.contains(QLatin1Char('G'))) { lowestClass = QStringLiteral("General"); }
        else if (lic == QLatin1String("E"))      { lowestClass = QStringLiteral("Extra"); }
        return lowestClass;
    };
    const auto stripLabel = [](const QString& label, const QString& lowestClass) {
        return lowestClass.isEmpty() ? label : QStringLiteral("%1 %2").arg(label, lowestClass);
    };

    BandPlanManager mgr;
    mgr.loadPlans();
    int checked = 0;
    for (const BandPlanManager::PlanData& plan : mgr.plans()) {
        for (const BandSegment& seg : plan.segments) {
            QCOMPARE(stripLabel(seg.label, NereusSDR::lowestLicenceClass(seg.license)),
                     stripLabel(seg.label, inlineRule(seg.license)));
            ++checked;
        }
    }
    QVERIFY(checked > 0);

    // The label the phone asked for, as the desktop draws it.
    QCOMPARE(stripLabel(QStringLiteral("PHONE"), NereusSDR::lowestLicenceClass(QStringLiteral("E,G"))),
             QStringLiteral("PHONE General"));
}

void TestBandPlanManager::loadPlans_findsAllFiveRegions()
{
    BandPlanManager mgr;
    mgr.loadPlans();
    const QStringList names = mgr.availablePlans();
    QCOMPARE(names.size(), 5);
    QVERIFY(names.contains("ARRL (US)"));
    QVERIFY(names.contains("IARU Region 1"));
    QVERIFY(names.contains("IARU Region 2"));
    QVERIFY(names.contains("IARU Region 3"));
    QVERIFY(names.contains("RAC (Canada)"));
}

void TestBandPlanManager::loadPlans_arrlUsHasSegmentsAndSpots()
{
    BandPlanManager mgr;
    mgr.loadPlans();
    mgr.setActivePlan("ARRL (US)");
    QVERIFY(mgr.segments().size() > 50);   // arrl-us.json has ~70 segments
    QVERIFY(mgr.spots().size()    > 50);   // arrl-us.json has ~80+ spots

    // 20m CW segment must exist (sanity-check round-trip of low/high/label/color)
    bool found20mCw = false;
    for (const auto& seg : mgr.segments()) {
        if (qFuzzyCompare(seg.lowMhz, 14.025) && seg.label == "CW" && seg.color.isValid()) {
            found20mCw = true;
            break;
        }
    }
    QVERIFY(found20mCw);
}

void TestBandPlanManager::loadPlans_iaruRegion2HasSegments()
{
    BandPlanManager mgr;
    mgr.loadPlans();
    mgr.setActivePlan("IARU Region 2");
    QVERIFY(mgr.segments().size() > 10);
}

void TestBandPlanManager::setActivePlan_emitsPlanChanged()
{
    BandPlanManager mgr;
    mgr.loadPlans();
    QSignalSpy spy(&mgr, &BandPlanManager::planChanged);
    mgr.setActivePlan("IARU Region 1");
    QCOMPARE(spy.count(), 1);
    QCOMPARE(mgr.activePlanName(), QString("IARU Region 1"));
}

void TestBandPlanManager::setActivePlan_unknownNameDoesNothing()
{
    BandPlanManager mgr;
    mgr.loadPlans();
    const QString before = mgr.activePlanName();
    QSignalSpy spy(&mgr, &BandPlanManager::planChanged);
    mgr.setActivePlan("Klingon Empire Bandplan");
    QCOMPARE(spy.count(), 0);
    QCOMPARE(mgr.activePlanName(), before);
}

void TestBandPlanManager::setActivePlan_segmentsReflectActivePlan()
{
    BandPlanManager mgr;
    mgr.loadPlans();
    mgr.setActivePlan("ARRL (US)");
    const int arrlCount = mgr.segments().size();
    mgr.setActivePlan("IARU Region 1");
    const int r1Count = mgr.segments().size();
    QVERIFY(arrlCount != r1Count);   // different region = different segment count
}

void TestBandPlanManager::loadPlans_defaultsToArrlUs()
{
    // First-launch default (no AppSettings key yet) is "ARRL (US)".
    // We can't easily reset AppSettings in a unit test, but we can verify
    // the ctor + loadPlans() leaves a non-empty active plan and that the
    // public API returns segments without a setActivePlan() call.
    BandPlanManager mgr;
    mgr.loadPlans();
    QVERIFY(!mgr.activePlanName().isEmpty());
    QVERIFY(!mgr.segments().isEmpty());
}

// This binary links NereusCore alone (CORE_ONLY in tests/CMakeLists.txt),
// like nereusd. The GUI library's resources must therefore be absent: if
// they were present, a passing band-plan load could still be coming from
// NereusSDRLib, which a Linux --as-needed link drops.
void TestBandPlanManager::coreOnlyLink_hasNoGuiResources()
{
    QVERIFY(!QFile::exists(QStringLiteral(":/icons/NereusSDR.png")));
    QVERIFY(!QFile::exists(QStringLiteral(":/meters/ananMM.png")));
}

// The Core carries exactly the band plans the app always shipped: every
// file in resources/bandplans, under the same name, with the same bytes,
// and nothing else.
void TestBandPlanManager::coreOnlyLink_bandPlansMatchSourceFiles()
{
    const QDir srcDir(QStringLiteral(NEREUS_SOURCE_DIR "/resources/bandplans"));
    const QStringList srcFiles =
        srcDir.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
    const QStringList resFiles = QDir(QStringLiteral(":/bandplans"))
        .entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);

    QCOMPARE(srcFiles.size(), 5);
    QCOMPARE(resFiles, srcFiles);

    for (const QString& name : srcFiles) {
        QFile src(srcDir.filePath(name));
        QFile res(QStringLiteral(":/bandplans/") + name);
        QVERIFY2(src.open(QIODevice::ReadOnly), qPrintable(name));
        QVERIFY2(res.open(QIODevice::ReadOnly), qPrintable(name));
        QVERIFY2(res.readAll() == src.readAll(), qPrintable(name));
    }

    BandPlanManager mgr;
    mgr.loadPlans();
    QCOMPARE(mgr.availablePlans().size(), srcFiles.size());
}

QTEST_MAIN(TestBandPlanManager)
#include "tst_bandplan_manager.moc"
