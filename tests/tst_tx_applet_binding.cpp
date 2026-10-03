// =================================================================
// tests/tst_tx_applet_binding.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Slice control plan Task 11 (Q15, U8):
// the TX applet's band and per-band power follow the slice transmit is
// bound to, never the active or a listened slice, and its letter row
// offers only the slices this window controls.
//
// A real TxApplet on a RadioModel with no radio; the hosted cases run it
// on a real Core (StationServer) with a second device signed in over a
// loopback link. Nothing keys: MOX is only read, or set on a model with
// no radio.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-29: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code. Slice control plan Task 11.
//   2026-09-29: Task 11 fix: the transmit band holds while the Core
//               transmits (Thetis's MOX gate on TXBand). The fake MOX here
//               runs on a model with no radio, so nothing keys. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29: Merge with the PA lane: the per-band power stores are
//               written before the band they hold is entered, since
//               RadioModel's TXBand port now loads and saves PWR on each
//               transmit band change. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
// =================================================================

#include "MultiDeviceHarness.h"

#include <QApplication>
#include <QPushButton>
#include <QSignalSpy>
#include <QSlider>
#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "core/MoxController.h"
#include "core/SliceOwnership.h"
#include "core/TxSliceArbiter.h"
#include "core/safety/TransmitHolder.h"
#include "core/safety/TxRefusal.h"
#include "gui/HostingSliceActions.h"
#include "gui/MainWindow.h"
#include "gui/applets/TxApplet.h"
#include "models/Band.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;
using namespace NereusSDR::Test;

namespace {

QSlider* powerSlider(TxApplet& applet)
{
    return applet.findChild<QSlider*>(QStringLiteral("TxRfPowerSlider"));
}

QList<int> letterIds(const TxApplet& applet)
{
    QList<int> ids;
    for (QPushButton* button : applet.transmitSliceButtons()) {
        ids << button->property("sliceId").toInt();
    }
    return ids;
}

} // namespace

class TstTxAppletBinding : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        AppSettings::setProfileOverride(
            QStringLiteral("tx-applet-binding-%1").arg(QCoreApplication::applicationPid()));
        AppSettings::instance().clear();
    }

    void cleanup() { AppSettings::instance().clear(); }

    // Q15: the band and the power slider are the transmit slice's. The
    // active slice sits on 20 m, transmit is bound to a 40 m slice: the
    // slider loads the 40 m power, retuning the active slice leaves it, and
    // retuning the transmit slice carries it.
    void theTransmitSliceNotTheActiveOneSetsTheBand()
    {
        RadioModel rm;
        rm.setBoardForTest(HPSDRHW::HermesLite);
        rm.setConnectionStateForTest(ConnectionState::Connected);
        rm.configureStreamPool(4, 5, 192000);
        // The per-band store is written before any slice exists: the first
        // transmit band loads its stored power into PWR, and leaving a band
        // saves PWR back over that band's store (Thetis TXBand setter,
        // console.cs:17511-17545 [v2.10.3.15]), so a store written for a band
        // already loaded would be overwritten on the next band change.
        rm.transmitModel().setPowerForBand(Band::Band20m, 80);
        rm.transmitModel().setPowerForBand(Band::Band40m, 37);
        const int active = rm.addSlice(QStringLiteral("pan-0"));
        const int transmit = rm.addSlice(QStringLiteral("pan-0"));
        QVERIFY(active >= 0 && transmit >= 0 && active != transmit);
        rm.sliceById(active)->setFrequency(14200000.0);
        rm.sliceById(transmit)->setFrequency(7100000.0);
        QVERIFY(rm.setActiveSliceById(active));
        QVERIFY(rm.requestTxHandoffToSlice(transmit));

        TxApplet applet(&rm);
        QSlider* slider = powerSlider(applet);
        QVERIFY(slider != nullptr);
        QCOMPARE(applet.transmitSlice(), rm.sliceById(transmit));
        QCOMPARE(applet.currentBand(), Band::Band40m);
        QCOMPARE(slider->value(), 37);

        rm.sliceById(active)->setFrequency(14250000.0);
        QCOMPARE(applet.currentBand(), Band::Band40m);
        QCOMPARE(slider->value(), 37);

        rm.sliceById(transmit)->setFrequency(14100000.0);
        QCOMPARE(applet.currentBand(), Band::Band20m);
        QCOMPARE(slider->value(), 80);
    }

    // Moving transmit to another slice moves the band with it.
    void movingTransmitMovesTheBand()
    {
        RadioModel rm;
        rm.setBoardForTest(HPSDRHW::HermesLite);
        rm.setConnectionStateForTest(ConnectionState::Connected);
        rm.configureStreamPool(4, 5, 192000);
        // As above: the per-band store is written before any slice. The first
        // transmit band loads its stored power into PWR, and leaving a band
        // saves PWR back over that band's store (Thetis TXBand setter,
        // console.cs:17511-17545 [v2.10.3.15]), so a store written for a band
        // already loaded would be overwritten on the next band change.
        rm.transmitModel().setPowerForBand(Band::Band20m, 80);
        rm.transmitModel().setPowerForBand(Band::Band40m, 37);
        const int a = rm.addSlice(QStringLiteral("pan-0"));
        const int b = rm.addSlice(QStringLiteral("pan-0"));
        rm.sliceById(a)->setFrequency(14200000.0);
        rm.sliceById(b)->setFrequency(7100000.0);
        QVERIFY(rm.requestTxHandoffToSlice(a));
        TxApplet applet(&rm);
        QCOMPARE(powerSlider(applet)->value(), 80);
        QVERIFY(rm.requestTxHandoffToSlice(b));
        QCOMPARE(applet.currentBand(), Band::Band40m);
        QCOMPARE(powerSlider(applet)->value(), 37);
    }

    // U8: a letter per slice, the transmitting one checked; a press moves
    // transmit there (ruling 8.10: the arbiter drops MOX first, and nothing
    // here keys). A reason disables every letter and names it.
    void theLetterRowMovesTransmit()
    {
        RadioModel rm;
        rm.setBoardForTest(HPSDRHW::HermesLite);
        rm.setConnectionStateForTest(ConnectionState::Connected);
        rm.configureStreamPool(4, 5, 192000);
        const int a = rm.addSlice(QStringLiteral("pan-0"));
        const int b = rm.addSlice(QStringLiteral("pan-0"));
        QVERIFY(rm.requestTxHandoffToSlice(a));
        TxApplet applet(&rm);
        QCOMPARE(letterIds(applet), (QList<int>{a, b}));
        QVERIFY(applet.transmitSliceButtons().at(0)->isChecked());
        QVERIFY(!applet.transmitSliceButtons().at(1)->isChecked());

        QTest::mouseClick(applet.transmitSliceButtons().at(1), Qt::LeftButton);
        QCOMPARE(rm.txSliceArbiter()->txBoundSliceId(), b);
        QTRY_VERIFY(applet.transmitSliceButtons().size() == 2
                    && applet.transmitSliceButtons().at(1)->isChecked());
        QVERIFY(!rm.moxController() || !rm.moxController()->isMox());

        const QString reason = TxRefusals::notHolder().text;
        applet.setTransmitSliceChoices({}, {}, [reason]() { return reason; });
        for (QPushButton* button : applet.transmitSliceButtons()) {
            QVERIFY(!button->isEnabled());
            QCOMPARE(button->toolTip(), reason);
        }
    }

    // The acceptance case, hosted: the window receives on a slice it only
    // listens to (another device's, 20 m) while transmit is bound to a
    // slice it controls (40 m). The TX band reads 40 m and the slider loads
    // the 40 m power. Choosing the listened slice for receive changes
    // neither the bound slice, the chosen slice nor the holder, and nothing
    // keys. The letter row offers only the controlled slice.
    void aListenedReceiveSliceNeverMovesTransmit()
    {
        Core core;
        core.model->configureStreamPool(4, 5, 192000);
        const QByteArray& station = SliceOwnership::stationDevice();
        core.server->deviceSessions()->registerHostingDevice(station, QStringLiteral("Mac"),
                                                              QStringLiteral("Mac"));
        core.server->setStationDeviceWords(QStringLiteral("Mac"), QStringLiteral("Mac"));
        SliceOwnership* ownership = core.model->sliceOwnership();
        ownership->adoptUnowned(station);
        const int mine = ownership->ownedBy(station).first();
        Device other(QStringLiteral("iPad"), QStringLiteral("tablet"), QStringLiteral("iPad"));
        core.pair(other);
        LoopbackTransport* app = core.signIn(
            other, {{"deviceAuth", 1}, {"sessionHolder", 1}, {"sliceAccess", 1}});
        QVERIFY(admitted(app));
        const int theirs = ownership->ownedBy(other.key.fingerprint()).first();
        // The 40 m store is written before a retune enters 40 m, so the
        // band change loads it (Thetis TXBand setter, console.cs:17511-17545
        // [v2.10.3.15]); a store written for the band PWR already holds is
        // overwritten from PWR on the next band change.
        core.model->transmitModel().setPowerForBand(Band::Band20m, 80);
        core.model->transmitModel().setPowerForBand(Band::Band40m, 37);
        core.model->sliceById(mine)->setFrequency(7100000.0);
        core.model->sliceById(theirs)->setFrequency(14200000.0);

        HostingSliceActions host(core.server.get(), core.model.get());
        host.listen(theirs);
        QTRY_VERIFY(ownership->isListening(station, theirs));
        QVERIFY(core.model->requestTxHandoffToSlice(mine));

        RadioModel* model = core.model.get();
        TxApplet applet(model);
        applet.setTransmitSliceResolver([model]() {
            return MainWindow::stationTransmitSlice(model);
        });
        applet.setTransmitSliceChoices(
            [model](int id) { return MainWindow::stationControlsSlice(model, id); },
            [model](int id) { model->requestTxHandoffToSlice(id); });

        TxSliceArbiter* arbiter = model->txSliceArbiter();
        const std::optional<TransmitHolder::Holder> holderBefore =
            core.server->transmitHolder()->holder();
        const quint64 epochBefore = core.server->transmitHolder()->epoch();
        const int chosenBefore = core.server->chosenTxSliceForTest(station);

        QVERIFY(model->setActiveRxFor(station, theirs));

        QCOMPARE(applet.transmitSlice(), model->sliceById(mine));
        QCOMPARE(applet.currentBand(), Band::Band40m);
        QCOMPARE(powerSlider(applet)->value(), 37);
        QCOMPARE(arbiter->txBoundSliceId(), mine);
        QCOMPARE(core.server->chosenTxSliceForTest(station), chosenBefore);
        QCOMPARE(core.server->transmitHolder()->holder().has_value(), holderBefore.has_value());
        QCOMPARE(core.server->transmitHolder()->epoch(), epochBefore);
        QVERIFY(!model->moxController() || !model->moxController()->isMox());
        QCOMPARE(letterIds(applet), QList<int>{mine});

        // Their slice retuning is not this window's transmit band.
        model->sliceById(theirs)->setFrequency(21200000.0);
        QCOMPARE(applet.currentBand(), Band::Band40m);
        QCOMPARE(powerSlider(applet)->value(), 37);
    }

    // Thetis never changes the TX band while MOX is on (console.cs
    // TXBand setter and SetTXBand, [2.10.3.6]MW0LGE): retuning the
    // transmit slice while transmitting leaves the band, the RF power the
    // slider writes and the Core's tune power where they were. Nothing
    // re-evaluates on the unkey; the next retune carries the band.
    void theBandHoldsWhileTransmitting()
    {
        RadioModel rm;
        rm.setBoardForTest(HPSDRHW::HermesLite);
        rm.setConnectionStateForTest(ConnectionState::Connected);
        rm.configureStreamPool(4, 5, 192000);
        // The per-band stores are written before the slice exists, as in
        // theTransmitSliceNotTheActiveOneSetsTheBand.
        TransmitModel& tx = rm.transmitModel();
        tx.setPowerForBand(Band::Band20m, 80);
        tx.setPowerForBand(Band::Band40m, 37);
        tx.setTunePowerForBand(Band::Band20m, 22);
        tx.setTunePowerForBand(Band::Band40m, 11);
        const int a = rm.addSlice(QStringLiteral("pan-0"));
        QVERIFY(a >= 0);
        rm.sliceById(a)->setFrequency(7100000.0);
        QVERIFY(rm.requestTxHandoffToSlice(a));
        TxApplet applet(&rm);
        QSlider* slider = powerSlider(applet);
        QVERIFY(slider != nullptr);
        QCOMPARE(applet.currentBand(), Band::Band40m);
        QCOMPARE(slider->value(), 37);
        QCOMPARE(tx.tunePowerForTxBand(), 11);

        MoxController* mox = rm.moxController();
        QVERIFY(mox != nullptr);
        mox->setMox(true);
        QTRY_VERIFY(rm.isTransmitting());

        rm.sliceById(a)->setFrequency(14100000.0);
        QCOMPARE(tx.tunePowerForTxBand(), 11);
        QCOMPARE(applet.currentBand(), Band::Band40m);
        QCOMPARE(slider->value(), 37);
        slider->setValue(41);
        QCOMPARE(tx.powerForBand(Band::Band40m), 41);
        QCOMPARE(tx.powerForBand(Band::Band20m), 80);

        mox->setMox(false);
        QTRY_VERIFY(!rm.isTransmitting());
        QCOMPARE(applet.currentBand(), Band::Band40m);
        QCOMPARE(tx.tunePowerForTxBand(), 11);

        rm.sliceById(a)->setFrequency(14150000.0);
        QCOMPARE(applet.currentBand(), Band::Band20m);
        QCOMPARE(slider->value(), 80);
        QCOMPARE(tx.tunePowerForTxBand(), 22);
    }
};

QTEST_MAIN(TstTxAppletBinding)
#include "tst_tx_applet_binding.moc"
