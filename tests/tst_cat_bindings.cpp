// no-port-check: NereusSDR-original CAT policy/lifecycle regression tests.
// 2026-10-04 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include <QtTest>
#include "core/cat/CatModelAdapter.h"
#include "core/SliceOwnership.h"
#define private public
#include "core/MoxController.h"
#undef private
#include "core/TxSliceArbiter.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
using namespace NereusSDR;
class TstCatBindings : public QObject {
    Q_OBJECT
private slots:
    void identitiesAndAdmission()
    {
        RadioModel model;
        QCOMPARE(model.addSlice(), 0);
        QCOMPARE(model.addSlice(), 1);
        QCOMPARE(model.addSlice(), 2);
        model.removeSlice(1);
        CatModelAdapter adapter(model);
        CatBinding binding; binding.primarySliceId = 0; binding.secondarySliceId = 2;
        binding = adapter.snapshotBinding(binding);
        QCOMPARE(adapter.resolveSlice(binding, CatVfo::Secondary), model.sliceById(2));
        SliceOwnership& ownership = *model.sliceOwnership();
        QVERIFY(!adapter.mayRead(binding, CatVfo::Primary));
        QVERIFY(adapter.mayChange(binding, CatVfo::Primary, "afGain"));
        ownership.setOwner(0, SliceOwnership::stationDevice());
        QVERIFY(adapter.mayRead(binding, CatVfo::Primary));
        QVERIFY(adapter.mayChange(binding, CatVfo::Primary, "frequency"));
        ownership.setOwner(0, "foreign");
        QVERIFY(adapter.mayRead(binding, CatVfo::Primary)); // Former owner stays a listener.
        QVERIFY(!adapter.mayChange(binding, CatVfo::Primary, "frequency"));
        ownership.setOwner(0, {});
        QVERIFY(!adapter.mayChange(binding, CatVfo::Primary, "frequency"));
        ownership.leave(SliceOwnership::stationDevice(), 0);
        ownership.leave("foreign", 0);
        QVERIFY(adapter.mayChange(binding, CatVfo::Primary, "frequency"));
        ownership.hold(0, "away");
        // The public visibility predicate includes the current station controller.
        QVERIFY(adapter.mayRead(binding, CatVfo::Primary));
        QVERIFY(adapter.mayChange(binding, CatVfo::Primary, "frequency"));
        ownership.setOwner(2, "foreign");
        QVERIFY(!adapter.mayRead(binding, CatVfo::Secondary));
        QVERIFY(!adapter.mayChange(binding, CatVfo::Secondary, "frequency"));
        model.removeSlice(0);
        QCOMPARE(model.addSlice(), 0);
        QVERIFY(!adapter.resolveSlice(binding, CatVfo::Primary));
        binding = adapter.snapshotBinding(binding);
        QVERIFY(adapter.resolveSlice(binding, CatVfo::Primary));
        binding.secondarySliceId = 99;
        binding = adapter.snapshotBinding(binding);
        QVERIFY(!adapter.resolveSlice(binding, CatVfo::Secondary));
        QCOMPARE(model.slices().size(), 2);
    }
    void onAirFreeze()
    {
        RadioModel model;
        const int primary = model.addSlice();
        const int secondary = model.addSlice();
        QVERIFY(primary >= 0 && secondary >= 0);
        model.sliceOwnership()->setOwner(primary, SliceOwnership::stationDevice());
        model.sliceOwnership()->setOwner(secondary, SliceOwnership::stationDevice());
        QVERIFY(model.txSliceArbiter()->requestHandoff(primary, SliceOwnership::stationDevice()));
        QCOMPARE(model.txBoundSlice(), model.sliceById(primary));
        CatModelAdapter adapter(model);
        CatBinding binding; binding.primarySliceId = primary; binding.secondarySliceId = secondary;
        binding = adapter.snapshotBinding(binding);
        const CatWriteToken token = adapter.prepareWrite(binding, CatVfo::Primary, "frequency");
        QVERIFY(adapter.revalidateWrite(token));
        // Inject only the actual controller's on-air identity/state; no radio or DSP startup.
        model.moxController()->m_mox = true;
        model.moxController()->m_currentKeyer = KeyerIdentity::station(PttMode::Cat);
        for (const QByteArray& property : QList<QByteArray>{"frequency", "dspMode", "filterLow", "filterHigh", "txAntenna", "band", "xitEnabled", "xitHz"}) {
            QVERIFY(!adapter.mayChange(binding, CatVfo::Primary, property));
            QVERIFY(adapter.mayChange(binding, CatVfo::Secondary, property));
        }
        QVERIFY(!adapter.revalidateWrite(token));
        QVERIFY(adapter.mayRead(binding, CatVfo::Primary));
        QVERIFY(adapter.mayChange(binding, CatVfo::Primary, "afGain"));
        QString reason;
        QVERIFY(!adapter.mayChangeGlobalDsp(&reason)); QVERIFY(!reason.isEmpty());
        model.moxController()->m_mox = false;
        QVERIFY(adapter.mayChangeGlobalDsp());
    }
    void compositeRevalidates()
    {
        RadioModel model;
        QCOMPARE(model.addSlice(), 0);
        CatModelAdapter adapter(model);
        CatBinding binding; binding.primarySliceId = 0;
        binding = adapter.snapshotBinding(binding);
        const CatWriteToken token = adapter.prepareWrite(binding, CatVfo::Primary, "filterLow");
        QVERIFY(adapter.revalidateWrite(token));
        model.sliceOwnership()->setOwner(0, "foreign");
        QVERIFY(!adapter.revalidateWrite(token));
        model.sliceOwnership()->setOwner(0, {});
        model.sliceOwnership()->leave("foreign", 0);
        QVERIFY(model.closeUnclaimedSlice(0));
        QCOMPARE(model.addSlice(), 0);
        QVERIFY(!adapter.revalidateWrite(token));
    }
};
QTEST_GUILESS_MAIN(TstCatBindings)
#include "tst_cat_bindings.moc"
