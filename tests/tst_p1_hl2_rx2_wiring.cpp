// no-port-check: NereusSDR-original wiring test, no ported logic.
#include <QtTest/QtTest>
#include <QSignalSpy>

#include "core/HpsdrModel.h"
#include "core/ReceiverManager.h"
#include "core/codec/CodecContext.h"
#include "core/codec/P1CodecHl2.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {
// ReceiverManager has no getter for the config it last computed, so read it
// off the signal it already emits.
PsDdcConfig lastConfig(QSignalSpy& spy)
{
    Q_ASSERT(spy.count() > 0);
    return spy.last().at(0).value<PsDdcConfig>();
}
} // namespace

class TestP1Hl2Rx2Wiring : public QObject {
    Q_OBJECT
private slots:
    // ReceiverManager::setRx2Enabled had no caller anywhere in src/ or
    // tests/, so m_rx2Enabled was permanently false and the rx2 arms of
    // applyPureSignalDdcConfig (P1CodecHl2.cpp:569-572 and :593-596, both
    // correctly ported all along) could never fire.
    void rx2_enabled_reaches_the_codec()
    {
        P1CodecHl2 codec;          // setP1Codec takes a raw pointer
        ReceiverManager rm;
        rm.setP1Codec(&codec);
        rm.setHpsdrModel(HPSDRModel::HERMESLITE);
        rm.setRx1Rate(192000);

        QSignalSpy spy(&rm, &ReceiverManager::ddcConfigChanged);

        rm.setRx2Rate(96000);
        rm.setRx2Enabled(true);

        QVERIFY(spy.count() > 0);
        const PsDdcConfig cfg = lastConfig(spy);
        QCOMPARE(int(cfg.ddcEnable), 1 + 2);      // DDC0 + DDC1
        QCOMPARE(int(cfg.rate[1]), 96000);
    }

    void rx2_disabled_drops_ddc1()
    {
        P1CodecHl2 codec;
        ReceiverManager rm;
        rm.setP1Codec(&codec);
        rm.setHpsdrModel(HPSDRModel::HERMESLITE);
        rm.setRx1Rate(192000);
        rm.setRx2Rate(96000);
        rm.setRx2Enabled(true);

        QSignalSpy spy(&rm, &ReceiverManager::ddcConfigChanged);
        rm.setRx2Enabled(false);

        QVERIFY(spy.count() > 0);
        const PsDdcConfig cfg = lastConfig(spy);
        QCOMPARE(int(cfg.ddcEnable), 1);          // DDC0 only
        QCOMPARE(int(cfg.rate[1]), 0);
    }

    // The production path. A second live stream must reach ReceiverManager
    // without anyone calling setRx2Enabled by hand, which is the wiring that
    // was missing. Driven through slice frequency changes because
    // requestDdcAssignment hangs off SliceModel::frequencyChanged.
    void second_live_stream_enables_rx2_end_to_end()
    {
        P1CodecHl2 codec;
        RadioModel model;
        model.setBoardForTest(HPSDRHW::HermesLite);
        model.receiverManager()->setP1Codec(&codec);
        model.receiverManager()->setHpsdrModel(HPSDRModel::HERMESLITE);
        model.configureStreamPool(/*userDdcCount=*/2, /*maxSlices=*/5, 192000);

        const int idA = model.addSlice();
        SliceModel* a = model.sliceById(idA);
        QVERIFY(a != nullptr);
        a->setFrequency(14200000);

        // Only stream 0 so far.
        QVERIFY(!model.receiverManager()->rx2Enabled());

        // 7.1 MHz is far outside slice A's 192 kHz window, so the allocator
        // must claim stream 1 rather than co-host on stream 0.
        const int idB = model.addSlice();
        SliceModel* b = model.sliceById(idB);
        QVERIFY(b != nullptr);
        b->setFrequency(7100000);

        QVERIFY(model.buildStreamConfigsForCodecForTest()[1].live);
        QVERIFY(model.receiverManager()->rx2Enabled());
    }
};

QTEST_MAIN(TestP1Hl2Rx2Wiring)
#include "tst_p1_hl2_rx2_wiring.moc"
