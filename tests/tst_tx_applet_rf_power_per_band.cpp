// no-port-check: NereusSDR-original test file.  Cite comments below point
// at Thetis lines that the asserted wiring mirrors; no Thetis logic is
// ported in this test file.
// =================================================================
// tests/tst_tx_applet_rf_power_per_band.cpp  (NereusSDR)
// =================================================================
//
// Regression tests for RF Power per-band persistence wiring.
//
// Bug pre-fix: moving the RF Power slider only wrote to TransmitModel::
// m_power (the runtime PWR scalar) and emitted powerChanged.  The per-band
// store m_powerByBand[currentBand] only updated indirectly via the
// setPowerUsingTargetDbm txMode-0 side-effect (TransmitModel.cpp:825),
// which is gated on connected radio + loaded PA profile + !TUNE.  And
// TxApplet::setCurrentBand only repainted the *Tune* Power slider — it
// never recalled the per-band value into the *RF* Power slider.  Result:
// app restart on a different band showed the wrong slider position, and
// disconnected slider moves never persisted.
//
// Fix: TxApplet's RF Power slider lambda now calls setPowerForBand
// directly (mirrors Thetis ptbPWR_Scroll at console.cs:28642), and
// setCurrentBand routes the per-band value through tx.setPower so the
// existing reverse-binding paints the slider (mirrors Thetis TXBand
// setter at console.cs:17513).
//
// 2026-09-29: RadioModel now loads power_by_band on a transmit band change
// and saves PWR into the transmit band (the TXBand setter and ptbPWR_Scroll,
// console.cs:17511-17545 and 28682-28693 [v2.10.3.15]), so a Core with no
// window gets both. The applet paints PWR from powerChanged and a local
// window no longer recalls or saves the band slot itself.
//
// Source references (cite comments only — no Thetis logic in tests):
//   console.cs:28642 [v2.10.3.13] — power_by_band[(int)_tx_band] = ptbPWR.Value
//   console.cs:17513 [v2.10.3.13] — PWR = power_by_band[(int)value]
//   console.cs:1813-1814 [v2.10.3.13] — power_by_band default 50 W
//
// Codex P1 review on PR #192 (threads r3190869829 + r3190869831):
// flagged that the original fix used m_currentBand as the per-band
// storage key.  m_currentBand is fed by both PanadapterModel::
// bandChanged AND SliceModel::frequencyChanged from MainWindow, so a
// CTUN pan that does not retune the slice would leave m_currentBand on
// the panadapter band while the actual TX band (per RadioModel.cpp:
// 903-905) was the active slice's band.  Slider writes through that
// stale key would silently corrupt the wrong band's stored value, and
// the recall on panadapter-only band changes would jump live RF drive
// to a non-TX band's preset and leak that wrong value back into the
// slice band's slot via the setPowerUsingTargetDbm txMode-0 side-
// effect (TransmitModel.cpp:825).  Tests 7 and 8 below pin both
// regressions.
// =================================================================

#include <QtTest/QtTest>
#include <QApplication>
#include <QSignalSpy>
#include <QSlider>

#include "core/AppSettings.h"
#include "gui/applets/TxApplet.h"
#include "models/Band.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

namespace {
constexpr const char* kTestMac = "AA:BB:CC:DD:EE:FF";
}  // namespace

class TestTxAppletRfPowerPerBand : public QObject
{
    Q_OBJECT

private slots:

    void initTestCase()
    {
        if (!qApp) {
            static int argc = 0;
            new QApplication(argc, nullptr);
        }
    }

    void init()
    {
        AppSettings::instance().clear();
    }

    void cleanup()
    {
        AppSettings::instance().clear();
    }

    // ── 1. Slider valueChanged writes per-band ──────────────────────────────
    //
    // Mirrors Thetis ptbPWR_Scroll at console.cs:28642 [v2.10.3.13]:
    //   power_by_band[(int)_tx_band] = ptbPWR.Value;
    //
    // The handler must update m_powerByBand[currentBand] directly — not
    // wait on the setPowerUsingTargetDbm gating.
    void slider_valueChanged_writesPerBand()
    {
        RadioModel rm;
        TxApplet applet(&rm);

        // Default m_currentBand is Band20m.
        auto* slider = applet.findChild<QSlider*>(
            QStringLiteral("TxRfPowerSlider"));
        QVERIFY(slider != nullptr);

        // Pre-condition: Band20m is at the default 50 W.
        QCOMPARE(rm.transmitModel().powerForBand(Band::Band20m), 50);

        // Drive a slider event — same path the user takes.
        slider->setValue(75);

        // Per-band slot for current band must now reflect the slider value.
        QCOMPARE(rm.transmitModel().powerForBand(Band::Band20m), 75);
        // m_power (current-PWR runtime scalar) tracks too.
        QCOMPARE(rm.transmitModel().power(), 75);
    }

    // ── 2. A band change on the transmit slice paints the model's recall ────
    //
    // Thetis TXBand setter at console.cs:17511-17545 [v2.10.3.15]:
    //   PWR = power_by_band[(int)value];
    // RadioModel owns the recall; the applet paints PWR from powerChanged.
    void bandChange_paintsModelRecallIntoSlider()
    {
        RadioModel rm;
        rm.addSlice();
        SliceModel* slice = rm.activeSlice();
        QVERIFY(slice != nullptr);
        slice->setFrequency(14'200'000.0);
        TxApplet applet(&rm);
        auto* slider = applet.findChild<QSlider*>(
            QStringLiteral("TxRfPowerSlider"));
        QVERIFY(slider != nullptr);

        rm.transmitModel().setPower(20);
        rm.transmitModel().setPowerForBand(Band::Band40m, 80);
        rm.transmitModel().setPowerForBand(Band::Band10m, 5);

        slice->setFrequency(7'150'000.0);
        QCOMPARE(slider->value(), 80);
        slice->setFrequency(28'400'000.0);
        QCOMPARE(slider->value(), 5);
        slice->setFrequency(14'200'000.0);
        QCOMPARE(slider->value(), 20);
    }

    // ── 3. setCurrentBand leaves PWR to the model ───────────────────────────
    //
    // The applet used to recall power_by_band itself in setCurrentBand. The
    // model now does it on every transmit band change (and at connect), so
    // a second recall here would load the band twice; setCurrentBand only
    // repaints the tune slider.
    void setCurrentBand_leavesPowerToModel()
    {
        RadioModel rm;
        rm.transmitModel().setPowerForBand(Band::Band40m, 80);
        TxApplet applet(&rm);
        auto* slider = applet.findChild<QSlider*>(
            QStringLiteral("TxRfPowerSlider"));
        QVERIFY(slider != nullptr);
        const int before = rm.transmitModel().power();
        QVERIFY(before != 80);

        QSignalSpy spy(&rm.transmitModel(), &TransmitModel::powerChanged);
        applet.setCurrentBand(Band::Band40m);
        QCOMPARE(spy.count(), 0);
        QCOMPARE(rm.transmitModel().power(), before);
        QCOMPARE(slider->value(), before);
    }

    // ── 3b. The MainWindow band wiring loads the band once ──────────────────
    //
    // MainWindow feeds each slice's frequencyChanged into setCurrentBand.
    // With the model recalling too, a band crossing must still move PWR
    // once, to the stored value.
    void bandCrossingWithApplet_loadsOnce()
    {
        RadioModel rm;
        rm.addSlice();
        SliceModel* slice = rm.activeSlice();
        QVERIFY(slice != nullptr);
        slice->setFrequency(7'150'000.0);
        TxApplet applet(&rm);
        connect(slice, &SliceModel::frequencyChanged, &applet,
                [&applet](double hz) { applet.setCurrentBand(bandFromFrequency(hz)); });
        auto* slider = applet.findChild<QSlider*>(
            QStringLiteral("TxRfPowerSlider"));
        QVERIFY(slider != nullptr);
        TransmitModel& tx = rm.transmitModel();
        tx.setPower(65);
        tx.setPowerForBand(Band::Band20m, 5);

        QSignalSpy spy(&tx, &TransmitModel::powerChanged);
        slice->setFrequency(14'250'000.0);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 5);
        QCOMPARE(tx.power(), 5);
        QCOMPARE(slider->value(), 5);
        QCOMPARE(tx.powerForBand(Band::Band40m), 65);
        QCOMPARE(tx.powerForBand(Band::Band20m), 5);
    }

    // ── 4. Per-band isolation across slider moves ───────────────────────────
    //
    // Moving the slider on band A must not bleed into band B's stored value.
    void slider_movesOnDifferentBands_isolatedPerBand()
    {
        RadioModel rm;
        rm.addSlice();
        SliceModel* slice = rm.activeSlice();
        QVERIFY(slice != nullptr);
        TxApplet applet(&rm);

        auto* slider = applet.findChild<QSlider*>(
            QStringLiteral("TxRfPowerSlider"));
        QVERIFY(slider != nullptr);

        slice->setFrequency(14'200'000.0);
        slider->setValue(40);
        QCOMPARE(rm.transmitModel().powerForBand(Band::Band20m), 40);

        slice->setFrequency(7'150'000.0);
        slider->setValue(90);
        QCOMPARE(rm.transmitModel().powerForBand(Band::Band40m), 90);

        slice->setFrequency(28'400'000.0);
        slider->setValue(15);
        QCOMPARE(rm.transmitModel().powerForBand(Band::Band10m), 15);

        // Earlier bands must still hold their original assignments.
        QCOMPARE(rm.transmitModel().powerForBand(Band::Band20m), 40);
        QCOMPARE(rm.transmitModel().powerForBand(Band::Band40m), 90);

        // Slider visually reflects the band's value when the slice returns.
        slice->setFrequency(14'200'000.0);
        QCOMPARE(slider->value(), 40);

        slice->setFrequency(7'150'000.0);
        QCOMPARE(slider->value(), 90);
    }

    // ── 5. Persistence round-trip across TransmitModel reload ───────────────
    //
    // Canonical bug repro: with a per-MAC scope active, slider moves on
    // distinct bands must survive TransmitModel reconstruction +
    // loadFromSettings(mac).
    void slider_writes_roundtripAcrossReload()
    {
        // Phase A: write through the applet → model → AppSettings chain.
        {
            RadioModel rm;
            rm.addSlice();
            SliceModel* slice = rm.activeSlice();
            QVERIFY(slice != nullptr);
            slice->setFrequency(14'200'000.0);
            // Activate per-MAC auto-persist.  After this call,
            // setPowerForBand writes to AppSettings on every change.
            rm.transmitModel().loadFromSettings(QString::fromLatin1(kTestMac));

            TxApplet applet(&rm);
            auto* slider = applet.findChild<QSlider*>(
                QStringLiteral("TxRfPowerSlider"));
            QVERIFY(slider != nullptr);

            slider->setValue(72);

            slice->setFrequency(7'150'000.0);
            slider->setValue(33);

            slice->setFrequency(50'125'000.0);
            slider->setValue(8);
        }

        // Phase B: fresh TransmitModel reads the saved per-band values.
        TransmitModel reloaded;
        reloaded.loadFromSettings(QString::fromLatin1(kTestMac));

        QCOMPARE(reloaded.powerForBand(Band::Band20m), 72);
        QCOMPARE(reloaded.powerForBand(Band::Band40m), 33);
        QCOMPARE(reloaded.powerForBand(Band::Band6m),  8);

        // Untouched bands remain at the default.
        QCOMPARE(reloaded.powerForBand(Band::Band80m), 50);
        QCOMPARE(reloaded.powerForBand(Band::Band10m), 50);
    }

    // ── 6. powerByBandChanged signal fires on slider events ─────────────────
    //
    // Confirms the slider save routes through setPowerForBand (which
    // emits powerByBandChanged), not through some side-channel that would
    // break listeners (e.g. PA-cal UI watching the per-band store).
    void slider_emitsPowerByBandChanged()
    {
        RadioModel rm;
        rm.addSlice();
        SliceModel* slice = rm.activeSlice();
        QVERIFY(slice != nullptr);
        slice->setFrequency(7'150'000.0);
        TxApplet applet(&rm);

        QSignalSpy spy(&rm.transmitModel(),
                       &TransmitModel::powerByBandChanged);

        auto* slider = applet.findChild<QSlider*>(
            QStringLiteral("TxRfPowerSlider"));
        QVERIFY(slider != nullptr);

        slider->setValue(60);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).value<Band>(), Band::Band40m);
        QCOMPARE(spy.at(0).at(1).toInt(), 60);
    }

    // ── 7. CTUN pan: slider write goes to the transmit band, not pan band ──
    //
    // Codex P1 review on PR #192 thread r3190869829: when the user pans
    // the panadapter (CTUN mode) without retuning the slice, MainWindow's
    // PanadapterModel::bandChanged callback fires setCurrentBand(panBand),
    // which moves m_currentBand off the slice band.  A slider event in
    // this state must still save to the transmit band's slot.
    void slider_write_usesActiveSliceBand_notPanBand()
    {
        RadioModel rm;
        rm.addSlice();
        SliceModel* slice = rm.activeSlice();
        QVERIFY(slice != nullptr);
        rm.transmitModel().setPower(50);
        // Active slice on 40m (7.150 MHz).
        slice->setFrequency(7'150'000.0);
        QCOMPARE(bandFromFrequency(slice->frequency()), Band::Band40m);

        TxApplet applet(&rm);

        // Simulate the panadapter drifting to 20m (panadapter
        // bandChanged → setCurrentBand(Band20m)).  Slice still on 40m.
        applet.setCurrentBand(Band::Band20m);

        auto* slider = applet.findChild<QSlider*>(
            QStringLiteral("TxRfPowerSlider"));
        QVERIFY(slider != nullptr);

        // Pre-condition: stored values for both bands at 50 W.
        QCOMPARE(rm.transmitModel().powerForBand(Band::Band40m), 50);
        QCOMPARE(rm.transmitModel().powerForBand(Band::Band20m), 50);

        slider->setValue(85);

        QCOMPARE(rm.transmitModel().powerForBand(Band::Band40m), 85);
        // Pan band's slot must NOT be touched.
        QCOMPARE(rm.transmitModel().powerForBand(Band::Band20m), 50);
    }

    // ── 8. CTUN pan: a panadapter band change leaves PWR alone ──────────────
    //
    // Codex P1 review on PR #192 thread r3190869831: a recall on a
    // panadapter band change that does not move the slice would jump live
    // RF drive to a non-transmit band's preset and leak it into the
    // transmit band's slot.  Only a transmit slice band change loads PWR.
    void setCurrentBand_recall_skippedForNonTxBand()
    {
        RadioModel rm;
        rm.addSlice();
        SliceModel* slice = rm.activeSlice();
        QVERIFY(slice != nullptr);
        slice->setFrequency(7'150'000.0);

        TxApplet applet(&rm);
        auto* slider = applet.findChild<QSlider*>(
            QStringLiteral("TxRfPowerSlider"));
        QVERIFY(slider != nullptr);

        // Slice on 40m at 65 W; 20m stored at 5 W.
        rm.transmitModel().setPower(65);
        rm.transmitModel().setPowerForBand(Band::Band20m, 5);
        applet.setCurrentBand(Band::Band40m);
        QCOMPARE(slider->value(), 65);

        // Simulate panadapter drifting to 20m: slider must stay on 65
        // because the slice didn't move.
        applet.setCurrentBand(Band::Band20m);
        QCOMPARE(slider->value(), 65);
        QCOMPARE(rm.transmitModel().power(), 65);
        QCOMPARE(rm.transmitModel().powerForBand(Band::Band40m), 65);

        // Move the slice to 20m: now PWR follows the transmit band, with
        // MainWindow's wire (slice frequencyChanged → setCurrentBand).
        slice->setFrequency(14'250'000.0);
        QCOMPARE(bandFromFrequency(slice->frequency()), Band::Band20m);
        applet.setCurrentBand(Band::Band20m);
        QCOMPARE(slider->value(), 5);
        QCOMPARE(rm.transmitModel().power(), 5);
    }

    // ── Group A fix wave, M3: an older Core takes `power` alone ─────────────
    //
    // A remote window on a Core below transmitSettingsVersion 5 must not
    // write tuneDrivePowerSource, which that Core refuses; on version 5 or
    // later it writes that too. It never writes powerByBandJson: the Core
    // is the table's only writer (it saves PWR into the transmit band's
    // slot itself), so the window sends the power setting alone.
    void slider_withoutPowerByBand_writesPowerOnly()
    {
        RadioModel rm(RadioModel::Role::Remote);
        TxApplet applet(&rm);
        auto* slider = applet.findChild<QSlider*>(
            QStringLiteral("TxRfPowerSlider"));
        QVERIFY(slider != nullptr);
        TransmitModel& tx = rm.transmitModel();
        tx.setTuneDrivePowerSource(DrivePowerSource::TuneSlider);
        const int band20Before = tx.powerForBand(Band::Band20m);

        applet.setPowerByBandPermitted(false);
        slider->setValue(band20Before == 60 ? 61 : 60);
        QCOMPARE(tx.power(), slider->value());
        QCOMPARE(tx.powerForBand(Band::Band20m), band20Before);
        QCOMPARE(tx.tuneDrivePowerSource(), DrivePowerSource::TuneSlider);

        applet.setPowerByBandPermitted(true);
        slider->setValue(33);
        QCOMPARE(tx.power(), 33);
        QCOMPARE(tx.powerForBand(Band::Band20m), band20Before);
        QCOMPARE(tx.tuneDrivePowerSource(), DrivePowerSource::DriveSlider);
    }

    // ── PA on-air gate review: the Tune Power slider is the transmit band's ─
    //
    // Thetis ptbTune_Scroll writes tunePower_by_band[(int)_tx_band] and the
    // slider shows TunePWR, which the TXBand setter loads from that band
    // (console.cs:46618 [v2.10.3.15]). A pan or receive slice on another
    // band neither shows nor takes the tune power; TUNE uses the transmit
    // band's.
    void tuneSlider_usesTransmitBand_notAppletBand()
    {
        RadioModel rm;
        rm.addSlice();
        SliceModel* slice = rm.activeSlice();
        QVERIFY(slice != nullptr);
        slice->setFrequency(14'200'000.0);
        TransmitModel& tx = rm.transmitModel();
        tx.setTunePowerForBand(Band::Band20m, 100);
        tx.setTunePowerForBand(Band::Band40m, 5);
        QCOMPARE(tx.tunePowerForTxBand(), 100);

        TxApplet applet(&rm);
        applet.setCurrentBand(Band::Band40m);
        QSlider* slider = applet.tunePowerSlider();
        QVERIFY(slider != nullptr);
        // Shown: the transmit band's tune power, not the applet band's.
        QCOMPARE(slider->value(), 100);

        slider->setValue(30);
        QCOMPARE(tx.tunePowerForBand(Band::Band20m), 30);
        QCOMPARE(tx.tunePowerForBand(Band::Band40m), 5);
        QCOMPARE(tx.tunePowerForTxBand(), 30);
        QCOMPARE(slider->value(), tx.tunePowerForTxBand());
        QCOMPARE(tx.tuneDrivePowerSource(), DrivePowerSource::TuneSlider);

        // Repaints on the transmit band's change only.
        tx.setTunePowerForBand(Band::Band20m, 42);
        QCOMPARE(slider->value(), 42);
        tx.setTunePowerForBand(Band::Band40m, 7);
        QCOMPARE(slider->value(), 42);
    }
};

QTEST_MAIN(TestTxAppletRfPowerPerBand)
#include "tst_tx_applet_rf_power_per_band.moc"
