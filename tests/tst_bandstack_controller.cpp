// =================================================================
// tests/tst_bandstack_controller.cpp  (NereusSDR)
// =================================================================
//
// The band stack on a real RadioModel in Role::Local: recalls, refusals,
// tracking, first run, the document under hardware/<mac>/, dwell, expiry and
// region changes. No radio, no WDSP.
//
// no-port-check: test only.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-05 - New test for NereusSDR by J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QRegularExpression>
#include <QSignalSpy>

#include <memory>

#include "core/AppSettings.h"
#include "core/MoxController.h"
#include "core/RadioDiscovery.h"
#include "core/WdspTypes.h"
#include "core/safety/BandPlanGuard.h"
#include "models/Band.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/bandstack/BandStackController.h"
#include "models/bandstack/BandStackSeeds.h"

using namespace NereusSDR;

namespace {

const QString kMacOne = QStringLiteral("AA:BB:CC:00:00:01");
const QString kMacTwo = QStringLiteral("AA:BB:CC:00:00:02");

QString documentKey(const QString& mac)
{
    return QStringLiteral("hardware/%1/bandstack/Document").arg(mac);
}

RadioInfo radioWith(const QString& mac)
{
    RadioInfo info;
    info.macAddress = mac;
    return info;
}

BandStackRecallRequest press(int sliceId, Band band,
                             BandStackDirection direction = BandStackDirection::Next,
                             const BandStackView& view = {})
{
    BandStackRecallRequest request;
    request.kind = BandStackRecallRequest::Press;
    request.sliceId = sliceId;
    request.band = band;
    request.direction = direction;
    request.view = view;
    return request;
}

BandStackRecallRequest step(int sliceId, BandStackDirection direction = BandStackDirection::Next)
{
    BandStackRecallRequest request;
    request.kind = BandStackRecallRequest::Step;
    request.sliceId = sliceId;
    request.direction = direction;
    return request;
}

BandStackRecallRequest goTo(int sliceId, const QString& entryId, const BandStackView& view = {})
{
    BandStackRecallRequest request;
    request.kind = BandStackRecallRequest::GoTo;
    request.sliceId = sliceId;
    request.entryId = entryId;
    request.view = view;
    return request;
}

QByteArray snapshot(const BandStackController& controller)
{
    return QJsonDocument(controller.store().toJson()).toJson(QJsonDocument::Compact);
}

QString idAt(const BandStackController& controller, Band band, qint64 frequencyHz)
{
    return controller.store().entryNear(band, frequencyHz);
}

// A model with one slice and a known radio.
struct Bench {
    RadioModel radio;
    SliceModel* a{nullptr};
    BandStackController* controller{nullptr};

    explicit Bench(const QString& mac = kMacOne)
    {
        radio.addSlice();
        radio.setActiveSlice(0);
        a = radio.sliceById(0);
        a->setFrequency(14100000.0);
        a->setDspMode(DSPMode::USB);
        radio.announceRadioForTest(radioWith(mac));
        controller = radio.bandStackController();
    }

    // The band with exactly the given frequencies on it, all LSB, built with
    // the slice the way an operator would, leaving the slice on the last one.
    void fill40(const QList<qint64>& frequencies)
    {
        QVERIFY(controller->clear(Band::Band40m, nullptr));
        a->setDspMode(DSPMode::LSB);
        for (const qint64 hz : frequencies) {
            a->setFrequency(static_cast<double>(hz));
            QString refusal;
            QVERIFY2(controller->add(a->sliceIndex(), {}, &refusal), qPrintable(refusal));
        }
    }
};

} // namespace

class TestBandStackController : public QObject {
    Q_OBJECT

private:
    static void clearSettings()
    {
        auto& settings = AppSettings::instance();
        const QStringList keys = settings.allKeys();
        for (const QString& key : keys) {
            if (key.startsWith(QStringLiteral("Slice")) || key.startsWith(QStringLiteral("hardware/"))
                || key == QStringLiteral("BandPlanRegion")) {
                settings.remove(key);
            }
        }
    }

private slots:
    void initTestCase()
    {
        // Every tune in a model with no radio says the TX frequency was not
        // pushed. None of these tests has a connection to push it to.
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.connection.warning=false"));
    }

    void init() { clearSettings(); }
    void cleanup() { clearSettings(); }

    // ── Ownership ────────────────────────────────────────────────────────

    void exists_onlyOnALocalModelWithAKnownRadio()
    {
        RadioModel local;
        QVERIFY(local.bandStackController() == nullptr);
        local.announceRadioForTest(radioWith(QString()));
        QVERIFY(local.bandStackController() == nullptr);
        local.announceRadioForTest(radioWith(kMacOne));
        QVERIFY(local.bandStackController() != nullptr);

        RadioModel remote(RadioModel::Role::Remote);
        remote.announceRadioForTest(radioWith(kMacOne));
        QVERIFY(remote.bandStackController() == nullptr);
    }

    // ── Recall ───────────────────────────────────────────────────────────

    void recall_restoresPerBandMemoryAndTheEntry_keepsRateAndStep()
    {
        Bench bench;
        SliceModel* a = bench.a;

        // 40 m's memory, with settings distinct from where the slice is now.
        a->setFrequency(7100000.0);
        a->setAgcMode(AGCMode::Slow);
        a->setNbMode(NbMode::NB2);
        a->setDiversityPhaseDeg(45.0);
        a->setDiversityGainDb(3.0);
        a->setStepHz(50);
        a->setSampleRateHz(96000);
        a->saveToSettings(Band::Band40m);

        a->setFrequency(14100000.0);
        a->setDspMode(DSPMode::USB);
        a->setAgcMode(AGCMode::Fast);
        a->setNbMode(NbMode::Off);
        a->setDiversityPhaseDeg(120.0);
        a->setDiversityGainDb(-3.0);
        a->setStepHz(1000);
        a->setSampleRateHz(192000);

        const QList<BandStackEntry> on40 = bench.controller->store().entries(Band::Band40m);
        QVERIFY(!on40.isEmpty());
        const BandStackEntry* target = nullptr;
        for (const BandStackEntry& entry : on40) {
            if (entry.mode == QStringLiteral("LSB")) {
                target = &entry;
                break;
            }
        }
        QVERIFY(target != nullptr);

        QSignalSpy report(bench.controller, &BandStackController::reportRecall);
        QString refusal;
        QVERIFY2(bench.controller->performRecall(press(0, Band::Band40m), &refusal),
                 qPrintable(refusal));

        QCOMPARE(a->agcMode(), AGCMode::Slow);
        QCOMPARE(a->nbMode(), NbMode::NB2);
        QCOMPARE(a->diversityPhaseDeg(), 45.0);
        QCOMPARE(a->diversityGainDb(), 3.0);

        QCOMPARE(a->frequency(), static_cast<double>(target->frequencyHz));
        QCOMPARE(a->dspMode(), DSPMode::LSB);
        QCOMPARE(a->filterLow(), target->filterLowHz);
        QCOMPARE(a->filterHigh(), target->filterHighHz);

        QCOMPARE(a->stepHz(), 1000);
        QCOMPARE(a->sampleRateHz(), 192000);

        QCOMPARE(report.count(), 1);
        QCOMPARE(bench.controller->store().place(QChar(u'A'), Band::Band40m), target->id);
    }

    void press_savesAndAdvances_wrapping_shiftGoesBack()
    {
        Bench bench;
        bench.fill40({7100000, 7150000, 7200000});
        // The slice is on its place at 7.200 after the last add.
        QVERIFY(bench.controller->performRecall(press(0, Band::Band40m), nullptr));
        QCOMPARE(bench.a->frequency(), 7100000.0); // wrapped
        QVERIFY(bench.controller->performRecall(step(0), nullptr));
        QCOMPARE(bench.a->frequency(), 7150000.0);
        QVERIFY(bench.controller->performRecall(press(0, Band::Band40m, BandStackDirection::Previous),
                                                nullptr));
        QCOMPARE(bench.a->frequency(), 7100000.0);

        // Tuned off the spot: the press writes it back, then moves on.
        const QString id = idAt(*bench.controller, Band::Band40m, 7100000);
        bench.a->setFrequency(7105000.0);
        QVERIFY(bench.controller->performRecall(press(0, Band::Band40m), nullptr));
        QCOMPARE(bench.controller->store().entry(id)->frequencyHz, qint64(7105000));
        QCOMPARE(bench.a->frequency(), 7150000.0);
    }

    void goTo_ownPlace_goesBackToIt_andTheEntryStays()
    {
        Bench bench;
        bench.fill40({7100000, 7185000});
        const QString id = idAt(*bench.controller, Band::Band40m, 7185000);
        QCOMPARE(bench.controller->store().place(QChar(u'A'), Band::Band40m), id);
        const BandStackEntry before = *bench.controller->store().entry(id);

        bench.a->setFrequency(7200000.0);
        bench.a->setDspMode(DSPMode::CWL);
        const BandStackView view{qint64(7200000), qint64(24000)};
        QVERIFY(bench.controller->performRecall(goTo(0, id, view), nullptr));

        QCOMPARE(bench.a->frequency(), 7185000.0);
        QCOMPARE(bench.a->dspMode(), DSPMode::LSB);
        const BandStackEntry* after = bench.controller->store().entry(id);
        QVERIFY(after != nullptr);
        QCOMPARE(after->frequencyHz, qint64(7185000));
        QCOMPARE(after->mode, before.mode);
        QCOMPARE(after->centerHz, before.centerHz);
        QCOMPARE(after->spanHz, before.spanHz);
        QCOMPARE(bench.controller->store().place(QChar(u'A'), Band::Band40m), id);
    }

    void goTo_ownPlace_keepsTheSeedMark()
    {
        Bench bench;
        const QList<BandStackEntry> seeds = bench.controller->store().entries(Band::Band40m);
        QVERIFY(!seeds.isEmpty());
        const BandStackEntry seed = seeds.first();
        QVERIFY(seed.seeded);
        QVERIFY(bench.controller->performRecall(goTo(0, seed.id), nullptr));

        bench.a->setFrequency(static_cast<double>(seed.frequencyHz + 5000));
        QVERIFY(bench.controller->performRecall(goTo(0, seed.id), nullptr));

        QCOMPARE(qRound64(bench.a->frequency()), seed.frequencyHz);
        const BandStackEntry* after = bench.controller->store().entry(seed.id);
        QVERIFY(after != nullptr);
        QCOMPARE(after->frequencyHz, seed.frequencyHz);
        QVERIFY(after->seeded);
    }

    void goTo_anotherRow_stillSavesThePlaceBeingLeft()
    {
        Bench bench;
        bench.fill40({7100000, 7185000});
        const QString low = idAt(*bench.controller, Band::Band40m, 7100000);
        const QString high = idAt(*bench.controller, Band::Band40m, 7185000);

        bench.a->setFrequency(7190000.0);
        QVERIFY(bench.controller->performRecall(goTo(0, low), nullptr));

        QCOMPARE(bench.a->frequency(), 7100000.0);
        QCOMPARE(bench.controller->store().entry(high)->frequencyHz, qint64(7190000));
    }

    void press_onOneEntry_landsOnWhatItJustSaved()
    {
        Bench bench;
        bench.fill40({7150000});
        bench.a->setFrequency(7151000.0);
        QCOMPARE(bench.controller->planRecall(press(0, Band::Band40m)).frequencyHz, qint64(7151000));
        QVERIFY(bench.controller->performRecall(press(0, Band::Band40m), nullptr));
        QCOMPARE(bench.a->frequency(), 7151000.0);
    }

    // A seed can be filed under a band its frequency is not in the range of.
    // GEN's rows sit in shortwave ranges, and re-pressing GEN must still cycle.
    void press_cyclesABandWhoseSeedsAreOutsideItsRange()
    {
        Bench bench;
        const QList<BandStackEntry> gen = bench.controller->store().entries(Band::GEN);
        QVERIFY(gen.size() >= 2);

        QVERIFY(bench.controller->performRecall(press(0, Band::GEN), nullptr));
        const QString first = bench.controller->store().place(QChar(u'A'), Band::GEN);
        QVERIFY(!first.isEmpty());
        QVERIFY(bench.controller->performRecall(press(0, Band::GEN), nullptr));
        const QString second = bench.controller->store().place(QChar(u'A'), Band::GEN);
        QVERIFY(!second.isEmpty());
        QVERIFY(first != second);
    }

    void leave_doesNotSaveOverASpotAnotherSliceIsOn()
    {
        Bench bench;
        bench.fill40({7100000, 7150000});
        const QString id = idAt(*bench.controller, Band::Band40m, 7150000);
        const int b = bench.radio.addSlice();
        SliceModel* sliceB = bench.radio.sliceById(b);
        sliceB->setFrequency(7150000.0);
        sliceB->setDspMode(DSPMode::LSB);
        QVERIFY(bench.controller->performRecall(goTo(b, id), nullptr));
        QVERIFY(bench.controller->performRecall(goTo(0, id), nullptr));

        // A tunes off and leaves: B is still on the spot, so it stays as saved.
        bench.a->setFrequency(7160000.0);
        QVERIFY(bench.controller->performRecall(press(0, Band::Band20m), nullptr));
        QCOMPARE(bench.controller->store().entry(id)->frequencyHz, qint64(7150000));

        // B is the last one on it, so its leaving saves.
        sliceB->setFrequency(7155000.0);
        QVERIFY(bench.controller->performRecall(press(b, Band::Band20m), nullptr));
        QCOMPARE(bench.controller->store().entry(id)->frequencyHz, qint64(7155000));
    }

    void refusedRetune_changesNothing()
    {
        RadioModel radio;
        // One receiver: B shares A's window and cannot leave it.
        radio.configureStreamPool(/*userDdcCount*/ 1, /*maxSlices*/ 5, 192000);
        const int a = radio.addSlice();
        radio.setActiveSlice(a);
        radio.sliceById(a)->setFrequency(14200000.0);
        const int b = radio.addSlice();
        SliceModel* sliceB = radio.sliceById(b);
        sliceB->setFrequency(14210000.0);
        sliceB->setAgcMode(AGCMode::Fast);
        radio.announceRadioForTest(radioWith(kMacOne));
        BandStackController* controller = radio.bandStackController();
        QVERIFY(controller != nullptr);

        QVERIFY(controller->add(b, {}, nullptr));
        const QByteArray stored = snapshot(*controller);
        const quint32 seq = controller->lastRecall().seq;
        QSignalSpy report(controller, &BandStackController::reportRecall);
        QSignalSpy rejected(&radio, &RadioModel::sliceRetuneRejected);

        sliceB->setFrequency(14211000.0); // would be written by a leave
        QCOMPARE(snapshot(*controller), stored);

        // The plan is still a plan: the shared-receiver question comes after it.
        QVERIFY(controller->planRecall(press(b, Band::Band40m)).valid);

        QString refusal;
        QVERIFY(!controller->performRecall(press(b, Band::Band40m), &refusal));
        QVERIFY2(refusal.contains(QStringLiteral("Slice B")), qPrintable(refusal));
        QCOMPARE(snapshot(*controller), stored);
        QCOMPARE(sliceB->frequency(), 14211000.0);
        QCOMPARE(sliceB->agcMode(), AGCMode::Fast);
        QCOMPARE(controller->lastRecall().seq, seq);
        QCOMPARE(report.count(), 0);
        QCOMPARE(rejected.count(), 0);
    }

    void locked_refusedByPlanAndPerform_withTheSameWords()
    {
        Bench bench;
        bench.a->setLocked(true);
        const QByteArray stored = snapshot(*bench.controller);
        const QString expected =
            QStringLiteral("Band 40m ignored: the slice is locked. Unlock it to change bands.");

        const BandStackRecallPlan plan = bench.controller->planRecall(press(0, Band::Band40m));
        QVERIFY(!plan.valid);
        QCOMPARE(plan.refusal, expected);

        QString refusal;
        QVERIFY(!bench.controller->performRecall(press(0, Band::Band40m), &refusal));
        QCOMPARE(refusal, expected);
        QCOMPARE(bench.a->frequency(), 14100000.0);
        QCOMPARE(snapshot(*bench.controller), stored);
    }

    void transmitting_refusedOnTheTransmittingSliceOnly()
    {
        Bench bench;
        const int b = bench.radio.addSlice();
        bench.radio.sliceById(b)->setFrequency(14200000.0);
        QCOMPARE(bench.radio.txBoundSlice(), bench.a);

        auto* mox = bench.radio.moxController();
        mox->setTimerIntervals(0, 0, 0, 0, 0, 0);
        mox->setMox(true);
        QTRY_VERIFY(bench.radio.isTransmitting());

        const QByteArray stored = snapshot(*bench.controller);
        const QString expected = QStringLiteral("Band 40m ignored: Slice A is transmitting.");
        const BandStackRecallPlan plan = bench.controller->planRecall(press(0, Band::Band40m));
        QVERIFY(!plan.valid);
        QCOMPARE(plan.refusal, expected);
        QString refusal;
        QVERIFY(!bench.controller->performRecall(press(0, Band::Band40m), &refusal));
        QCOMPARE(refusal, expected);
        QCOMPARE(bench.a->frequency(), 14100000.0);
        QCOMPARE(snapshot(*bench.controller), stored);

        // Another slice may still press (design V11).
        QVERIFY(bench.controller->planRecall(press(b, Band::Band40m)).valid);

        mox->setMox(false);
        QTRY_VERIFY(!bench.radio.isTransmitting());
    }

    void plan_changesNothing_andAnEmptyBandPlansItsMiddle()
    {
        Bench bench;
        QVERIFY(bench.controller->clear(Band::Band40m, nullptr));
        const QByteArray stored = snapshot(*bench.controller);
        const BandStackLastRecall last = bench.controller->lastRecall();

        const BandStackRecallPlan plan = bench.controller->planRecall(press(0, Band::Band40m));
        QVERIFY(plan.valid);
        QCOMPARE(plan.band, Band::Band40m);
        QCOMPARE(plan.frequencyHz, qint64(7150000));
        QCOMPARE(snapshot(*bench.controller), stored);
        QCOMPARE(bench.a->frequency(), 14100000.0);
        QCOMPARE(bench.controller->lastRecall().seq, last.seq);

        QSignalSpy report(bench.controller, &BandStackController::reportRecall);
        QVERIFY(bench.controller->performRecall(press(0, Band::Band40m), nullptr));
        QCOMPARE(bench.a->frequency(), 7150000.0);
        QCOMPARE(bench.controller->store().entries(Band::Band40m).size(), 0);
        QCOMPARE(report.first().at(1).toString(),
                 QStringLiteral("40m: empty stack, tuned to the middle of the band"));
    }

    void xvtr_refusedWithTodaysWords()
    {
        Bench bench;
        const BandStackRecallPlan plan = bench.controller->planRecall(press(0, Band::XVTR));
        QVERIFY(!plan.valid);
        QCOMPARE(plan.refusal,
                 QStringLiteral("Band XVTR ignored: transverter settings are not available"));
        QVERIFY(!bench.controller->performRecall(press(0, Band::XVTR), nullptr));
        QCOMPARE(bench.a->frequency(), 14100000.0);
    }

    void refusals_forAnEntryOrSliceThatIsGone()
    {
        Bench bench;
        QString refusal;
        QVERIFY(!bench.controller->performRecall(goTo(0, QStringLiteral("gone")), &refusal));
        QVERIFY(!refusal.isEmpty());
        refusal.clear();
        QVERIFY(!bench.controller->performRecall(press(9, Band::Band40m), &refusal));
        QVERIFY(!refusal.isEmpty());

        QVERIFY(bench.controller->clear(Band::Band20m, nullptr));
        refusal.clear();
        QVERIFY(!bench.controller->performRecall(step(0), &refusal));
        QVERIFY2(refusal.contains(QStringLiteral("20m")), qPrintable(refusal));
    }

    void report_namesTheSpotAndItsPosition()
    {
        Bench bench;
        bench.fill40({7100000, 7125000, 7150000, 7165000, 7185000, 7200000});
        QSignalSpy report(bench.controller, &BandStackController::reportRecall);
        QVERIFY(bench.controller->performRecall(
            goTo(0, idAt(*bench.controller, Band::Band40m, 7185000)), nullptr));
        QCOMPARE(report.count(), 1);
        QCOMPARE(report.first().at(0).toInt(), 0);
        QCOMPARE(report.first().at(1).toString(), QStringLiteral("40m: 7.185 LSB, 5 of 6"));
    }

    void lastRecall_alonePublishesTheView_sharedOnlyWhenItMustFollow()
    {
        Bench bench;
        bench.a->setPanKey(QStringLiteral("pan-0"));
        bench.fill40({7100000, 7150000});
        const BandStackView saved{qint64(7120000), qint64(96000)};
        const QString low = idAt(*bench.controller, Band::Band40m, 7100000);
        const QString high = idAt(*bench.controller, Band::Band40m, 7150000);

        // Save a view into the low spot by leaving it with one.
        QVERIFY(bench.controller->performRecall(goTo(0, low), nullptr));
        QVERIFY(bench.controller->performRecall(goTo(0, high, saved), nullptr));
        BandStackLastRecall last = bench.controller->lastRecall();
        const quint32 firstSeq = last.seq;
        QVERIFY(firstSeq >= 2);

        // Alone on the pan: the view applies, with the entry's center and zoom.
        QVERIFY(bench.controller->performRecall(goTo(0, low), nullptr));
        last = bench.controller->lastRecall();
        QCOMPARE(last.seq, firstSeq + 1);
        QCOMPARE(last.sliceId, 0);
        QVERIFY(last.applyView);
        QCOMPARE(last.centerHz, qint64(7120000));
        QCOMPARE(last.spanHz, qint64(96000));

        const int b = bench.radio.addSlice(QStringLiteral("pan-0"));
        QVERIFY(b >= 0);
        bench.radio.sliceById(b)->setPanKey(QStringLiteral("pan-0"));

        // Shared and on screen: the view stays.
        const BandStackView wide{qint64(7125000), qint64(192000)};
        QVERIFY(bench.controller->performRecall(goTo(0, high, wide), nullptr));
        QVERIFY(!bench.controller->lastRecall().applyView);
        // No saved zoom on this one: span 0.
        QCOMPARE(bench.controller->lastRecall().spanHz, qint64(0));

        // Shared and off screen: it follows, on the frequency at the span it
        // has. The low spot saved `wide` when it was left, and that stays there.
        const BandStackView away{qint64(14100000), qint64(192000)};
        QVERIFY(bench.controller->performRecall(goTo(0, low, away), nullptr));
        last = bench.controller->lastRecall();
        QVERIFY(last.applyView);
        QCOMPARE(last.centerHz, qint64(7100000));
        QCOMPARE(last.spanHz, qint64(0));
        QCOMPARE(bench.controller->store().entry(low)->spanHz, qint64(192000));

        // A shared pan is never handed a saved zoom, on screen or off. The high
        // spot now holds `away`, whose center is nowhere near the frequency.
        QCOMPARE(bench.controller->store().entry(high)->spanHz, qint64(192000));
        const BandStackView narrowOnLow{qint64(7100000), qint64(24000)};
        QVERIFY(bench.controller->performRecall(goTo(0, high, narrowOnLow), nullptr));
        last = bench.controller->lastRecall();
        QVERIFY(last.applyView);
        QCOMPARE(last.centerHz, qint64(7150000));
        QCOMPARE(last.spanHz, qint64(0));
    }

    // ── Seeds ────────────────────────────────────────────────────────────

    void seeds_resolveTheirFiltersFromThePresetTable()
    {
        Bench bench;
        // Seeds are United States by default; every row loads.
        const QList<SeedRow> rows = seedsForRegion(safety::Region::UnitedStates);
        qsizetype loaded = 0;
        for (int i = 0; i < static_cast<int>(Band::Count); ++i) {
            loaded += bench.controller->store().entries(static_cast<Band>(i)).size();
        }
        // First run may add this slice's saved spots; none are saved here.
        QCOMPARE(loaded, rows.size());

        // LSB F6 is slot index 5.
        bool sawF6 = false;
        for (const SeedRow& row : rows) {
            if (row.mode == QStringLiteral("LSB") && row.filterNumber == 6) {
                const QString id = idAt(*bench.controller, row.band, row.frequencyHz);
                const BandStackEntry* entry = bench.controller->store().entry(id);
                QVERIFY(entry != nullptr);
                QCOMPARE(entry->filterLowHz, -2800);
                QCOMPARE(entry->filterHighHz, -100);
                QVERIFY(entry->seeded);
                sawF6 = true;
            }
        }
        QVERIFY(sawF6);
    }

    void entryWithAnUnknownMode_refused_notTunedAsUsb()
    {
        Bench bench;
        const QString key = documentKey(kMacOne);
        QJsonObject document = bench.controller->store().toJson();
        bench.controller->flush();
        QJsonArray entries = document.value(QStringLiteral("entries")).toArray();
        QJsonObject first = entries.first().toObject();
        const QString id = first.value(QStringLiteral("id")).toString();
        first.insert(QStringLiteral("mode"), QStringLiteral("NOTAMODE"));
        entries.replace(0, first);
        document.insert(QStringLiteral("entries"), entries);
        AppSettings::instance().setValue(
            key, QString::fromUtf8(QJsonDocument(document).toJson(QJsonDocument::Compact)));
        bench.radio.announceRadioForTest(radioWith(kMacOne));

        QString refusal;
        QVERIFY(!bench.controller->performRecall(goTo(0, id), &refusal));
        QVERIFY(!refusal.isEmpty());
        QCOMPARE(bench.a->dspMode(), DSPMode::USB);
    }

    // ── Return modes ─────────────────────────────────────────────────────

    void lastUsed_returnsTheExactSpot_andClearsThePlace()
    {
        Bench bench;
        bench.fill40({7100000, 7150000});
        QVERIFY(bench.controller->setReturn(Band::Band40m, BandStackReturnMode::LastUsed, QString(),
                                            nullptr));
        bench.a->setFrequency(7123000.0);
        bench.a->setDspMode(DSPMode::CWL);
        bench.a->setFilter(-700, -300);
        QVERIFY(bench.controller->performRecall(press(0, Band::Band20m), nullptr));
        QCOMPARE(bandFromFrequency(bench.a->frequency()), Band::Band20m);

        QSignalSpy report(bench.controller, &BandStackController::reportRecall);
        QVERIFY(bench.controller->performRecall(press(0, Band::Band40m), nullptr));
        QCOMPARE(bench.a->frequency(), 7123000.0);
        QCOMPARE(bench.a->dspMode(), DSPMode::CWL);
        QCOMPARE(bench.a->filterLow(), -700);
        QCOMPARE(bench.a->filterHigh(), -300);
        QCOMPARE(bench.controller->store().place(QChar(u'A'), Band::Band40m), QString());
        QCOMPARE(report.first().at(1).toString(), QStringLiteral("40m: 7.123 CWL"));
    }

    void lastUsed_withNoLastFrequency_fallsBackToCurrent()
    {
        Bench bench;
        bench.fill40({7100000, 7150000});
        QVERIFY(bench.controller->performRecall(
            goTo(0, idAt(*bench.controller, Band::Band40m, 7150000)), nullptr));
        QVERIFY(bench.controller->performRecall(press(0, Band::Band20m), nullptr));
        const int b = bench.radio.addSlice();
        bench.radio.sliceById(b)->setFrequency(14200000.0);
        AppSettings::instance().remove(QStringLiteral("Slice1/Band15m/Frequency"));

        QVERIFY(bench.controller->setReturn(Band::Band15m, BandStackReturnMode::LastUsed, QString(),
                                            nullptr));
        const BandStackRecallPlan plan = bench.controller->planRecall(press(b, Band::Band15m));
        QVERIFY(plan.valid);
        const BandStackTarget current =
            bench.controller->store().planCurrent(QChar(u'B'), Band::Band15m);
        QCOMPARE(current.kind, BandStackTarget::Entry);
        QCOMPARE(plan.frequencyHz, bench.controller->store().entry(current.entryId)->frequencyHz);
    }

    void specific_returnsToItsEntry()
    {
        Bench bench;
        bench.fill40({7100000, 7150000, 7200000});
        const QString id = idAt(*bench.controller, Band::Band40m, 7100000);
        QVERIFY(bench.controller->setReturn(Band::Band40m, BandStackReturnMode::Specific, id, nullptr));
        QVERIFY(bench.controller->performRecall(press(0, Band::Band20m), nullptr));
        QCOMPARE(bench.controller->planRecall(press(0, Band::Band40m)).frequencyHz, qint64(7100000));
    }

    // ── Tracking ─────────────────────────────────────────────────────────

    void crossingABandEdgeByHand_savesNoSpot_movesNoPlace_updatesLastUsed()
    {
        Bench bench;
        bench.fill40({7100000, 7150000});
        const QString id = idAt(*bench.controller, Band::Band40m, 7150000);
        QVERIFY(bench.controller->performRecall(goTo(0, id), nullptr));
        const QByteArray before = snapshot(*bench.controller);

        bench.a->setFrequency(7290000.0);
        bench.a->setFrequency(14250000.0); // across the edge, by hand

        QCOMPARE(snapshot(*bench.controller), before);
        QCOMPARE(bench.controller->store().place(QChar(u'A'), Band::Band40m), id);

        QVERIFY(bench.controller->setReturn(Band::Band40m, BandStackReturnMode::LastUsed, QString(),
                                            nullptr));
        QCOMPARE(bench.controller->planRecall(press(0, Band::Band40m)).frequencyHz, qint64(7290000));
    }

    // ── Add and save here ────────────────────────────────────────────────

    void add_refusesADuplicate_andGoesInFrequencyOrder()
    {
        Bench bench;
        bench.fill40({7100000, 7200000});
        bench.a->setFrequency(7150000.0);
        QVERIFY(bench.controller->add(0, {}, nullptr));
        const QList<BandStackEntry> entries = bench.controller->store().entries(Band::Band40m);
        QCOMPARE(entries.at(1).frequencyHz, qint64(7150000));
        QCOMPARE(bench.controller->store().place(QChar(u'A'), Band::Band40m), entries.at(1).id);

        bench.a->setFrequency(7150050.0);
        QString refusal;
        QVERIFY(!bench.controller->add(0, {}, &refusal));
        QCOMPARE(refusal, QStringLiteral("7.150.050 is already in 40m"));
    }

    void saveHere_writesOntoAnotherSlicesSpot_refusedWhenLockedOrOffTheBand()
    {
        Bench bench;
        bench.fill40({7100000, 7150000});
        const QString id = idAt(*bench.controller, Band::Band40m, 7150000);
        const int b = bench.radio.addSlice();
        SliceModel* sliceB = bench.radio.sliceById(b);
        sliceB->setFrequency(7150000.0);
        QVERIFY(bench.controller->performRecall(goTo(b, id), nullptr));

        bench.a->setFrequency(7152000.0);
        const BandStackView view{qint64(7160000), qint64(48000)};
        QVERIFY(bench.controller->saveHere(0, id, view, nullptr));
        const BandStackEntry* entry = bench.controller->store().entry(id);
        QCOMPARE(entry->frequencyHz, qint64(7152000));
        QCOMPARE(entry->centerHz, qint64(7160000));
        QCOMPARE(entry->spanHz, qint64(48000));
        QCOMPARE(sliceB->frequency(), 7150000.0);

        QVERIFY(bench.controller->edit(id, std::nullopt, true, nullptr));
        QString refusal;
        QVERIFY(!bench.controller->saveHere(0, id, {}, &refusal));
        QVERIFY(!refusal.isEmpty());
        QVERIFY(bench.controller->edit(id, std::nullopt, false, nullptr));

        bench.a->setFrequency(14100000.0);
        refusal.clear();
        QVERIFY(!bench.controller->saveHere(0, id, {}, &refusal));
        QVERIFY2(refusal.contains(QStringLiteral("Slice A")), qPrintable(refusal));
    }

    void editRemoveReorderClearAndOptions_reportChange_orRefuse()
    {
        Bench bench;
        bench.fill40({7100000, 7150000});
        QSignalSpy changed(bench.controller, &BandStackController::changed);
        const QString low = idAt(*bench.controller, Band::Band40m, 7100000);
        const QString high = idAt(*bench.controller, Band::Band40m, 7150000);

        QVERIFY(bench.controller->edit(low, QStringLiteral("Net"), std::nullopt, nullptr));
        QCOMPARE(changed.count(), 1);
        QString refusal;
        QVERIFY(!bench.controller->edit(QStringLiteral("gone"), QStringLiteral("x"), std::nullopt,
                                        &refusal));
        QVERIFY(!refusal.isEmpty());
        QCOMPARE(changed.count(), 1);

        QVERIFY(bench.controller->reorder(Band::Band40m, {high, low}, nullptr));
        QCOMPARE(bench.controller->store().entries(Band::Band40m).first().id, high);
        refusal.clear();
        QVERIFY(!bench.controller->reorder(Band::Band40m, {high}, &refusal));
        QVERIFY(!refusal.isEmpty());

        refusal.clear();
        QVERIFY(!bench.controller->options(std::nullopt, 7, std::nullopt, &refusal));
        QVERIFY(!refusal.isEmpty());
        refusal.clear();
        QVERIFY(!bench.controller->options(std::nullopt, std::nullopt, 7, &refusal));
        QVERIFY(!refusal.isEmpty());
        QVERIFY(bench.controller->options(false, 15, 30, nullptr));
        QCOMPARE(bench.controller->store().options().dupeProtection, false);
        QCOMPARE(bench.controller->store().options().expiryMinutes, 15);
        QCOMPARE(bench.controller->store().options().dwellSeconds, 30);

        QVERIFY(bench.controller->remove(low, nullptr));
        QVERIFY(!bench.controller->remove(low, nullptr));
        QVERIFY(bench.controller->clear(std::nullopt, nullptr));
        QCOMPARE(bench.controller->store().entries(Band::Band40m).size(), 0);
    }

    // ── First run ────────────────────────────────────────────────────────

    void firstRun_savedSpotsBecomePlaces_andNoSliceMoves()
    {
        RadioModel radio;
        radio.addSlice();
        radio.setActiveSlice(0);
        SliceModel* a = radio.sliceById(0);

        const QList<SeedRow> seeds = seedsForRegion(safety::Region::UnitedStates);
        qint64 seed40 = 0;
        for (const SeedRow& row : seeds) {
            if (row.band == Band::Band40m && row.mode == QStringLiteral("LSB")) {
                seed40 = row.frequencyHz;
                break;
            }
        }
        QVERIFY(seed40 != 0);

        // A saved 40 m within 50 Hz of a seed, and a saved 20 m nowhere near one.
        a->setDspMode(DSPMode::LSB);
        a->setFrequency(static_cast<double>(seed40 + 50));
        a->saveToSettings(Band::Band40m);
        a->setDspMode(DSPMode::USB);
        a->setFrequency(14123456.0);
        a->saveToSettings(Band::Band20m);
        a->setFrequency(21234567.0);

        radio.announceRadioForTest(radioWith(kMacOne));
        BandStackController* controller = radio.bandStackController();
        QVERIFY(controller != nullptr);
        QCOMPARE(a->frequency(), 21234567.0);

        const QChar letter(u'A');
        const BandStackEntry* on40 = controller->store().entry(controller->store().place(letter, Band::Band40m));
        QVERIFY(on40 != nullptr);
        QCOMPARE(on40->frequencyHz, seed40); // took the seed
        QVERIFY(on40->seeded);

        const BandStackEntry* on20 = controller->store().entry(controller->store().place(letter, Band::Band20m));
        QVERIFY(on20 != nullptr);
        QCOMPARE(on20->frequencyHz, qint64(14123456)); // joined the stack
        QVERIFY(!on20->seeded);

        QCOMPARE(controller->store().place(letter, Band::Band15m), QString());
    }

    void firstRun_aSliceOpeningLaterBringsItsSpotsToo()
    {
        Bench bench;
        AppSettings::instance().setValue(QStringLiteral("Slice1/Band20m/Frequency"), 14222222.0);
        AppSettings::instance().setValue(QStringLiteral("Slice1/Band20m/DspMode"),
                                         static_cast<int>(DSPMode::USB));
        const int b = bench.radio.addSlice();
        QCOMPARE(b, 1);
        const BandStackEntry* entry = bench.controller->store().entry(
            bench.controller->store().place(QChar(u'B'), Band::Band20m));
        QVERIFY(entry != nullptr);
        QCOMPARE(entry->frequencyHz, qint64(14222222));
    }

    // ── The document ─────────────────────────────────────────────────────

    void document_savedWithinASecond_coalesced_perMac_reloadsOnReconnect()
    {
        Bench bench;
        auto& settings = AppSettings::instance();
        // The first run's document goes out on its own timer.
        QTRY_VERIFY_WITH_TIMEOUT(settings.contains(documentKey(kMacOne)), 3000);

        bench.fill40({7100000, 7150000});
        const QString id = idAt(*bench.controller, Band::Band40m, 7150000);
        QVERIFY(bench.controller->performRecall(goTo(0, id), nullptr));
        QVERIFY(bench.controller->edit(id, QStringLiteral("Net"), std::nullopt, nullptr));
        // Coalesced: not written by the change itself.
        QVERIFY(!settings.value(documentKey(kMacOne)).toString().contains(QStringLiteral("Net")));
        QTRY_VERIFY_WITH_TIMEOUT(
            settings.value(documentKey(kMacOne)).toString().contains(QStringLiteral("Net")), 3000);

        // A second radio has its own stack and starts from the seeds.
        bench.radio.announceRadioForTest(radioWith(kMacTwo));
        QVERIFY(bench.controller->store().entry(id) == nullptr);
        QVERIFY(settings.value(documentKey(kMacOne)).toString().contains(id));

        // Back to the first: everything is there, places too.
        bench.radio.announceRadioForTest(radioWith(kMacOne));
        QVERIFY(bench.controller->store().entry(id) != nullptr);
        QCOMPARE(bench.controller->store().entry(id)->name, QStringLiteral("Net"));
        QCOMPARE(bench.controller->store().place(QChar(u'A'), Band::Band40m), id);
    }

    void document_survivesTheSettingsFile_andShutdown()
    {
        QString id;
        {
            Bench bench;
            bench.fill40({7150000});
            id = idAt(*bench.controller, Band::Band40m, 7150000);
            QVERIFY(bench.controller->edit(id, QStringLiteral("Net"), std::nullopt, nullptr));
            // The model goes before the timer: the destructor writes it.
        }
        auto& settings = AppSettings::instance();
        const QString written = settings.value(documentKey(kMacOne)).toString();
        QVERIFY(written.contains(QStringLiteral("Net")));

        QVERIFY(settings.save());
        settings.load();
        QCOMPARE(settings.value(documentKey(kMacOne)).toString(), written);

        Bench again;
        QCOMPARE(again.controller->store().entry(id)->name, QStringLiteral("Net"));
        QCOMPARE(again.controller->store().place(QChar(u'A'), Band::Band40m), id);
    }

    void document_corrupt_keptAsBad_warned_andReseeded()
    {
        auto& settings = AppSettings::instance();
        const QString junk = QStringLiteral("{ not json");
        settings.setValue(documentKey(kMacOne), junk);

        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("could not be read")));
        Bench bench;
        QCOMPARE(settings.value(documentKey(kMacOne) + QStringLiteral(".bad")).toString(), junk);
        QVERIFY(!bench.controller->store().entries(Band::Band40m).isEmpty());
        QCOMPARE(bench.controller->regionName(), QStringLiteral("United States"));

        // A well-formed document of the wrong shape is corrupt too.
        settings.setValue(documentKey(kMacTwo), QStringLiteral("{\"schema\":99}"));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("could not be read")));
        bench.radio.announceRadioForTest(radioWith(kMacTwo));
        QCOMPARE(settings.value(documentKey(kMacTwo) + QStringLiteral(".bad")).toString(),
                 QStringLiteral("{\"schema\":99}"));
    }

    void placesAreKeptByLetter_whenASliceClosesAndReopens()
    {
        Bench bench;
        const int b = bench.radio.addSlice();
        QCOMPARE(b, 1);
        bench.radio.sliceById(b)->setFrequency(14200000.0);
        const QString id = idAt(*bench.controller, Band::Band40m,
                                bench.controller->store().entries(Band::Band40m).last().frequencyHz);
        QVERIFY(bench.controller->performRecall(goTo(b, id), nullptr));
        QCOMPARE(bench.controller->store().place(QChar(u'B'), Band::Band40m), id);

        bench.radio.removeSlice(b);
        QCOMPARE(bench.controller->store().place(QChar(u'B'), Band::Band40m), id);
        const int reopened = bench.radio.addSlice();
        QCOMPARE(reopened, 1);
        QCOMPARE(bench.controller->store().place(QChar(u'B'), Band::Band40m), id);
        // A reopened slice is wired again: its tuning arms dwell.
        QVERIFY(bench.controller->options(std::nullopt, std::nullopt, 10, nullptr));
        bench.radio.sliceById(reopened)->setFrequency(14300000.0);
        QVERIFY(bench.controller->dwellArmedForTest(reopened));
    }

    // ── Dwell and expiry ─────────────────────────────────────────────────

    void dwell_isOffByDefault_addsASpot_movesNoPlace_skipsASameModeSpot()
    {
        Bench bench;
        qint64 now = 1000000;
        bench.controller->setClockForTest([&now] { return now; });
        bench.a->setFrequency(14111000.0);
        QVERIFY(!bench.controller->dwellArmedForTest(0));
        const QString place = bench.controller->store().place(QChar(u'A'), Band::Band20m);

        QVERIFY(bench.controller->options(std::nullopt, std::nullopt, 10, nullptr));
        bench.a->setFrequency(14112000.0);
        QVERIFY(bench.controller->dwellArmedForTest(0));
        const qsizetype before = bench.controller->store().entries(Band::Band20m).size();

        QSignalSpy changed(bench.controller, &BandStackController::changed);
        bench.controller->runDwellForTest(0);
        QCOMPARE(bench.controller->store().entries(Band::Band20m).size(), before + 1);
        QCOMPARE(bench.controller->store().place(QChar(u'A'), Band::Band20m), place);
        QCOMPARE(changed.count(), 1);

        bench.controller->runDwellForTest(0); // the same spot again
        QCOMPARE(bench.controller->store().entries(Band::Band20m).size(), before + 1);
    }

    void dwell_aRecallStopsAPendingOne()
    {
        Bench bench;
        QVERIFY(bench.controller->options(std::nullopt, std::nullopt, 10, nullptr));
        bench.a->setFrequency(14112000.0);
        QVERIFY(bench.controller->dwellArmedForTest(0));
        QVERIFY(bench.controller->performRecall(press(0, Band::Band40m), nullptr));
        QVERIFY(!bench.controller->dwellArmedForTest(0));
    }

    void expiry_followsTheRules_undoRestoresForFiveSeconds()
    {
        Bench bench;
        qint64 now = 10000000;
        bench.controller->setClockForTest([&now] { return now; });
        bench.fill40({7100000, 7150000, 7200000});
        const QString low = idAt(*bench.controller, Band::Band40m, 7100000);
        const QString mid = idAt(*bench.controller, Band::Band40m, 7150000);
        const QString high = idAt(*bench.controller, Band::Band40m, 7200000);
        QVERIFY(bench.controller->edit(mid, std::nullopt, true, nullptr));
        QVERIFY(bench.controller->setReturn(Band::Band40m, BandStackReturnMode::Specific, low, nullptr));
        const qsizetype seeds = bench.controller->store().entries(Band::Band20m).size();

        // Off by default.
        now += 3600000;
        bench.controller->runExpiryForTest();
        QVERIFY(bench.controller->store().entry(low) != nullptr);
        QCOMPARE(bench.controller->lastExpiryCount(), 0);

        QVERIFY(bench.controller->options(std::nullopt, 5, std::nullopt, nullptr));
        // `high` is the slice's place and `mid` is locked: only `low` can go.
        QSignalSpy changed(bench.controller, &BandStackController::changed);
        bench.controller->runExpiryForTest();
        QVERIFY(bench.controller->store().entry(low) == nullptr);
        QVERIFY(bench.controller->store().entry(mid) != nullptr);
        QVERIFY(bench.controller->store().entry(high) != nullptr);
        QCOMPARE(bench.controller->store().entries(Band::Band20m).size(), seeds);
        QCOMPARE(bench.controller->lastExpiryCount(), 1);
        const QString batch = bench.controller->lastExpiryBatchId();
        QVERIFY(!batch.isEmpty());
        QCOMPARE(changed.count(), 1);

        // Within 5 seconds the right batch comes back, specific return included.
        QString refusal;
        QVERIFY(!bench.controller->restoreExpired(QStringLiteral("other"), &refusal));
        QVERIFY(!refusal.isEmpty());
        now += 5000;
        QVERIFY2(bench.controller->restoreExpired(batch, &refusal), qPrintable(refusal));
        QVERIFY(bench.controller->store().entry(low) != nullptr);
        QCOMPARE(bench.controller->store().specificId(Band::Band40m), low);
        QCOMPARE(bench.controller->lastExpiryBatchId(), QString());
        QVERIFY(!bench.controller->restoreExpired(batch, nullptr));

        // The undone batch does not expire again on the next pass.
        bench.controller->runExpiryForTest();
        QVERIFY(bench.controller->store().entry(low) != nullptr);

        // It does once its time is up, and past 5 seconds it stays gone.
        now += 5 * 60000;
        bench.controller->runExpiryForTest();
        QVERIFY(bench.controller->store().entry(low) == nullptr);
        const QString second = bench.controller->lastExpiryBatchId();
        now += 5001;
        refusal.clear();
        QVERIFY(!bench.controller->restoreExpired(second, &refusal));
        QVERIFY(!refusal.isEmpty());
        QVERIFY(bench.controller->store().entry(low) == nullptr);
    }

    void recallRestartsAnEntrysExpiryTime_tuningDoesNot()
    {
        Bench bench;
        qint64 now = 10000000;
        bench.controller->setClockForTest([&now] { return now; });
        bench.fill40({7100000, 7150000});
        const QString low = idAt(*bench.controller, Band::Band40m, 7100000);
        const qint64 addedAt = bench.controller->store().entry(low)->lastUsedMs;

        now += 1000;
        QVERIFY(bench.controller->performRecall(goTo(0, low), nullptr));
        QCOMPARE(bench.controller->store().entry(low)->lastUsedMs, now);

        const qint64 recalledAt = now;
        now += 1000;
        bench.a->setFrequency(7101000.0);
        QCOMPARE(bench.controller->store().entry(low)->lastUsedMs, recalledAt);
        QVERIFY(recalledAt > addedAt);
    }

    // ── Region ───────────────────────────────────────────────────────────

    void regionChange_updatesTheName_changesNoEntry_reseedMovesNoSlice()
    {
        Bench bench;
        QCOMPARE(bench.controller->regionName(), QStringLiteral("United States"));
        bench.fill40({7123000});
        const QString mine = idAt(*bench.controller, Band::Band40m, 7123000);
        QVERIFY(bench.controller->edit(mine, std::nullopt, true, nullptr));

        QSignalSpy changed(bench.controller, &BandStackController::changed);
        AppSettings::instance().setValue(
            QStringLiteral("BandPlanRegion"),
            QString::number(static_cast<int>(safety::Region::UnitedKingdom)));
        emit bench.radio.bandPlanRegionChanged();
        QCOMPARE(changed.count(), 1);
        QCOMPARE(bench.controller->regionName(), QStringLiteral("United Kingdom"));
        QVERIFY(bench.controller->store().entries(Band::Band40m).size() == 1);

        // The same region again is no change.
        emit bench.radio.bandPlanRegionChanged();
        QCOMPARE(changed.count(), 1);

        const double hz = bench.a->frequency();
        QVERIFY(bench.controller->reseed(nullptr));
        QCOMPARE(bench.a->frequency(), hz);
        QVERIFY(bench.controller->store().entry(mine) != nullptr);
        qsizetype uk60 = bench.controller->store().entries(Band::Band60m).size();
        QVERIFY(uk60 > 0);
        const QList<SeedRow> rows = seedsForRegion(safety::Region::UnitedKingdom);
        qsizetype expected = 0;
        for (const SeedRow& row : rows) {
            if (row.band == Band::Band60m) {
                ++expected;
            }
        }
        QCOMPARE(uk60, expected);
        QCOMPARE(bench.controller->regionName(), QStringLiteral("United Kingdom"));
    }

    // ── Parsing a link verb ──────────────────────────────────────────────

    void parseRecall_readsEachVerb_andRefusesAMalformedOne()
    {
        const auto parse = [](const char* json, const QString& verb) {
            return BandStackController::parseRecall(QJsonDocument::fromJson(json).object(), verb);
        };

        const auto pressed = parse(
            R"({"sliceId":1,"band":"31m","direction":"previous","viewCenterHz":9500000,"viewSpanHz":48000})",
            QStringLiteral("bandStack.press"));
        QVERIFY(pressed.has_value());
        QCOMPARE(pressed->kind, BandStackRecallRequest::Press);
        QCOMPARE(pressed->sliceId, 1);
        QCOMPARE(pressed->band, Band::Band31m);
        QCOMPARE(pressed->direction, BandStackDirection::Previous);
        QCOMPARE(pressed->view.centerHz.value(), qint64(9500000));
        QCOMPARE(pressed->view.spanHz.value(), qint64(48000));

        const auto stepped = parse(R"({"sliceId":0,"direction":"next"})",
                                   QStringLiteral("bandStack.step"));
        QVERIFY(stepped.has_value());
        QCOMPARE(stepped->kind, BandStackRecallRequest::Step);
        QVERIFY(!stepped->view.centerHz.has_value());

        const auto went = parse(R"({"sliceId":2,"entryId":"abc"})", QStringLiteral("bandStack.goTo"));
        QVERIFY(went.has_value());
        QCOMPARE(went->kind, BandStackRecallRequest::GoTo);
        QCOMPARE(went->entryId, QStringLiteral("abc"));

        const QString pressVerb = QStringLiteral("bandStack.press");
        QVERIFY(!parse(R"({"sliceId":0,"band":"40m","direction":"next"})", QStringLiteral("other")));
        QVERIFY(!parse(R"({"band":"40m","direction":"next"})", pressVerb));
        QVERIFY(!parse(R"({"sliceId":"0","band":"40m","direction":"next"})", pressVerb));
        QVERIFY(!parse(R"({"sliceId":-1,"band":"40m","direction":"next"})", pressVerb));
        QVERIFY(!parse(R"({"sliceId":0,"band":"nope","direction":"next"})", pressVerb));
        QVERIFY(!parse(R"({"sliceId":0,"band":"40m","direction":"up"})", pressVerb));
        QVERIFY(!parse(R"({"sliceId":0,"band":"40m"})", pressVerb));
        QVERIFY(!parse(R"({"sliceId":0,"band":"40m","direction":"next","viewSpanHz":"x"})", pressVerb));
        QVERIFY(!parse(R"({"sliceId":0})", QStringLiteral("bandStack.goTo")));
        QVERIFY(!parse(R"({"sliceId":0})", QStringLiteral("bandStack.step")));
    }
};

QTEST_MAIN(TestBandStackController)
#include "tst_bandstack_controller.moc"
