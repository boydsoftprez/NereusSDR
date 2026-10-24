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
//
// 2026-10-18: neither power slider changes the tune drive source; only
// Setup > Transmit > Power's Tune radios do, as in Thetis (setup.cs:
// 22902-22915 [v2.10.3.15]; ptbPWR_Scroll and ptbTune_Scroll never write
// _tuneDrivePowerSource). The last tests pin that, locally and in a remote
// window, and that TUNE follows the chosen source across a band change
// whichever slider moved last. J.J. Boyd (KG4VCF), AI-assisted via
// Anthropic Claude Code.
//
// 2026-10-21: a grayed power slider's accessible description is the drive
// source gate's reason, as the transmit gates set theirs. J.J. Boyd
// (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QApplication>
#include <QSignalSpy>
#include <QSlider>

#include "core/AppSettings.h"
#include "core/HpsdrModel.h"
#include "core/PaProfile.h"
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

    // ── A remote window's RF Power move sends `power` alone ────────────────
    //
    // It never writes powerByBandJson (the Core is the table's only writer:
    // it saves PWR into the transmit band's slot itself) and never the tune
    // drive source, which only Setup > Transmit > Power changes.
    void remoteSlider_writesPowerOnly_leavesTuneSource()
    {
        RadioModel rm(RadioModel::Role::Remote);
        TxApplet applet(&rm);
        auto* slider = applet.findChild<QSlider*>(
            QStringLiteral("TxRfPowerSlider"));
        QVERIFY(slider != nullptr);
        TransmitModel& tx = rm.transmitModel();
        const int band20Before = tx.powerForBand(Band::Band20m);

        for (const DrivePowerSource source : {DrivePowerSource::TuneSlider,
                                              DrivePowerSource::Fixed,
                                              DrivePowerSource::DriveSlider}) {
            tx.setTuneDrivePowerSource(source);
            const int value = slider->value() == 33 ? 34 : 33;
            slider->setValue(value);
            QCOMPARE(tx.power(), value);
            QCOMPARE(tx.powerForBand(Band::Band20m), band20Before);
            QCOMPARE(tx.tuneDrivePowerSource(), source);
        }
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
        // The tune drive source stays as Setup chose it (default DriveSlider).
        QCOMPARE(tx.tuneDrivePowerSource(), DrivePowerSource::DriveSlider);

        // Repaints on the transmit band's change only.
        tx.setTunePowerForBand(Band::Band20m, 42);
        QCOMPARE(slider->value(), 42);
        tx.setTunePowerForBand(Band::Band40m, 7);
        QCOMPARE(slider->value(), 42);
    }

    // ── Neither slider changes the tune drive source (local window) ───────
    //
    // Thetis ptbPWR_Scroll and ptbTune_Scroll (console.cs:28682-28693 and
    // 46607-46630 [v2.10.3.15]) never write _tuneDrivePowerSource; the
    // Setup radios are its only writers (setup.cs:22902-22915 [v2.10.3.15]).
    void sliders_leaveTuneSource_data()
    {
        QTest::addColumn<int>("source");
        QTest::newRow("DriveSlider") << static_cast<int>(DrivePowerSource::DriveSlider);
        QTest::newRow("TuneSlider")  << static_cast<int>(DrivePowerSource::TuneSlider);
        QTest::newRow("Fixed")       << static_cast<int>(DrivePowerSource::Fixed);
    }

    void sliders_leaveTuneSource()
    {
        QFETCH(int, source);
        const auto chosen = static_cast<DrivePowerSource>(source);
        RadioModel rm;
        rm.addSlice();
        SliceModel* slice = rm.activeSlice();
        QVERIFY(slice != nullptr);
        slice->setFrequency(14'200'000.0);
        TransmitModel& tx = rm.transmitModel();
        tx.setTuneDrivePowerSource(chosen);
        TxApplet applet(&rm);
        auto* rf = applet.findChild<QSlider*>(QStringLiteral("TxRfPowerSlider"));
        QSlider* tune = applet.tunePowerSlider();
        QVERIFY(rf != nullptr);
        QVERIFY(tune != nullptr);
        QSignalSpy changed(&tx, &TransmitModel::tuneDrivePowerSourceChanged);

        rf->setValue(rf->value() == 61 ? 62 : 61);
        QCOMPARE(tx.tuneDrivePowerSource(), chosen);
        tune->setValue(tune->value() == 23 ? 24 : 23);
        QCOMPARE(tx.tuneDrivePowerSource(), chosen);
        rf->setValue(rf->value() == 44 ? 45 : 44);
        QCOMPARE(tx.tuneDrivePowerSource(), chosen);
        QCOMPARE(changed.count(), 0);
    }

    // ── TUNE follows Setup's source across a band change ─────────────────
    //
    // The PA calibration case: per-band drive and tune power differ, and the
    // operator touches both sliders in turn. TUNE (setPowerUsingTargetDbm
    // txMode 1, Thetis console.cs:46757-46770 [v2.10.3.15]) must read the
    // source Setup chose on every band, whichever slider moved last.
    void tunePower_followsSetupSource_acrossBandChange_data()
    {
        QTest::addColumn<int>("source");
        QTest::addColumn<int>("want20m");
        QTest::addColumn<int>("want40m");
        // Drive slider: PWR, which the transmit band change loads from
        // power_by_band (20 m: the RF slider's 60; 40 m: 80).
        QTest::newRow("DriveSlider")
            << static_cast<int>(DrivePowerSource::DriveSlider) << 60 << 80;
        // Tune slider: the band's tune power (20 m: the tune slider's 30;
        // 40 m: 25).
        QTest::newRow("TuneSlider")
            << static_cast<int>(DrivePowerSource::TuneSlider) << 30 << 25;
    }

    void tunePower_followsSetupSource_acrossBandChange()
    {
        QFETCH(int, source);
        QFETCH(int, want20m);
        QFETCH(int, want40m);
        const auto chosen = static_cast<DrivePowerSource>(source);
        const PaProfile profile(QStringLiteral("Default - ANAN_G2"),
                                HPSDRModel::ANAN_G2, true);

        RadioModel rm;
        rm.addSlice();
        SliceModel* slice = rm.activeSlice();
        QVERIFY(slice != nullptr);
        slice->setFrequency(14'200'000.0);
        TransmitModel& tx = rm.transmitModel();
        tx.setTuneDrivePowerSource(chosen);
        tx.setPowerForBand(Band::Band40m, 80);
        tx.setTunePowerForBand(Band::Band40m, 25);
        TxApplet applet(&rm);
        auto* rf = applet.findChild<QSlider*>(QStringLiteral("TxRfPowerSlider"));
        QSlider* tune = applet.tunePowerSlider();
        QVERIFY(rf != nullptr);
        QVERIFY(tune != nullptr);

        const auto tunePower = [&](Band band) {
            return tx.setPowerUsingTargetDbm(profile, band, /*bSetPower=*/false,
                                             /*bFromTune=*/true, /*bTwoTone=*/false,
                                             HPSDRModel::ANAN_G2).newPower;
        };

        // 20 m: tune slider, then RF Power last.
        tune->setValue(30);
        rf->setValue(60);
        QCOMPARE(tx.tuneDrivePowerSource(), chosen);
        QCOMPARE(tunePower(Band::Band20m), want20m);

        // 40 m: the band change loads that band's PWR and tune power.
        slice->setFrequency(7'150'000.0);
        QCOMPARE(rf->value(), 80);
        QCOMPARE(tune->value(), 25);
        QCOMPARE(tunePower(Band::Band40m), want40m);

        // Back to 20 m, RF Power then tune slider last: still Setup's source.
        slice->setFrequency(14'200'000.0);
        rf->setValue(60);
        tune->setValue(30);
        QCOMPARE(tx.tuneDrivePowerSource(), chosen);
        QCOMPARE(tunePower(Band::Band20m), want20m);
    }

    // ── The slider TUNE or the two-tone test is not using is grayed ───────
    //
    // Thetis setupTuneDriveSlider (console.cs:46677-46709 [v2.10.3.15]):
    // during TUN the slider the tune source does not name is disabled, and
    // during 2TONE the one the two-tone source does not name; otherwise both
    // are live, except that the Tune slider, which Thetis hides when neither
    // source is the tune slider, is grayed (never hidden) with a reason.
    void driveSourceGate_graysTheUnusedSlider()
    {
        RadioModel rm;
        TransmitModel& tx = rm.transmitModel();
        TxApplet applet(&rm);
        auto* rf = applet.findChild<QSlider*>(QStringLiteral("TxRfPowerSlider"));
        QSlider* tune = applet.tunePowerSlider();
        QVERIFY(rf != nullptr);
        QVERIFY(tune != nullptr);

        // Idle, both sources DriveSlider (the default): RF Power live, the
        // Tune slider grayed on screen with its reason.
        QVERIFY(rf->isEnabled());
        QVERIFY(!tune->isEnabled());
        QVERIFY(!tune->isHidden());
        QVERIFY(tune->toolTip().contains(QStringLiteral("Setup > Transmit > Power")));

        // Idle, either source on the tune slider: both live.
        tx.setTuneDrivePowerSource(DrivePowerSource::TuneSlider);
        QVERIFY(rf->isEnabled());
        QVERIFY(tune->isEnabled());
        tx.setTuneDrivePowerSource(DrivePowerSource::DriveSlider);
        tx.setTwoToneDrivePowerSource(DrivePowerSource::TuneSlider);
        QVERIFY(tune->isEnabled());
        tx.setTwoToneDrivePowerSource(DrivePowerSource::DriveSlider);
        QVERIFY(!tune->isEnabled());

        // During TUNE: only the slider the tune source names.
        tx.setTune(true);
        QVERIFY(rf->isEnabled());
        QVERIFY(!tune->isEnabled());
        tx.setTuneDrivePowerSource(DrivePowerSource::TuneSlider);
        QVERIFY(!rf->isEnabled());
        QVERIFY(rf->toolTip().contains(QStringLiteral("TUNE is not using this slider.")));
        QVERIFY(tune->isEnabled());
        tx.setTuneDrivePowerSource(DrivePowerSource::Fixed);
        QVERIFY(!rf->isEnabled());
        QVERIFY(!tune->isEnabled());
        tx.setTune(false);
        QVERIFY(rf->isEnabled());
        QVERIFY(!rf->toolTip().contains(QStringLiteral("not using")));

        // During the two-tone test: only the slider the two-tone source names.
        tx.setTuneDrivePowerSource(DrivePowerSource::DriveSlider);
        tx.setTwoToneDrivePowerSource(DrivePowerSource::TuneSlider);
        tx.setTwoToneActive(true);
        QVERIFY(!rf->isEnabled());
        QVERIFY(tune->isEnabled());
        tx.setTwoToneActive(false);
        QVERIFY(rf->isEnabled());
        QVERIFY(tune->isEnabled());
    }

    // A remote window's transmit settings gate and the drive source gate
    // together: the slider comes back as the source says when the gate opens.
    void driveSourceGate_underTransmitSettingsGate()
    {
        RadioModel rm(RadioModel::Role::Remote);
        TransmitModel& tx = rm.transmitModel();
        TxApplet applet(&rm);
        QSlider* tune = applet.tunePowerSlider();
        QVERIFY(tune != nullptr);

        applet.setTransmitChainSettingsPermitted(false, QStringLiteral("Closed."));
        QVERIFY(!tune->isEnabled());
        tx.setTuneDrivePowerSource(DrivePowerSource::TuneSlider);
        QVERIFY(!tune->isEnabled());
        QCOMPARE(tune->toolTip(), QStringLiteral("Closed."));
        applet.setTransmitChainSettingsPermitted(true);
        QVERIFY(tune->isEnabled());

        applet.setTransmitChainSettingsPermitted(false, QStringLiteral("Closed."));
        tx.setTuneDrivePowerSource(DrivePowerSource::DriveSlider);
        applet.setTransmitChainSettingsPermitted(true);
        QVERIFY(!tune->isEnabled());
        QVERIFY(tune->toolTip().contains(QStringLiteral("Setup > Transmit > Power")));
    }

    // The drive source gate's reason is the grayed slider's accessible
    // description too, as the transmit gates set theirs (gateTransmitControl),
    // and the description clears when the slider is live again; under a
    // remote window's transmit settings gate it comes back with the slider.
    void driveSourceGate_setsAccessibleDescription()
    {
        RadioModel rm;
        TransmitModel& tx = rm.transmitModel();
        TxApplet applet(&rm);
        auto* rf = applet.findChild<QSlider*>(QStringLiteral("TxRfPowerSlider"));
        QSlider* tune = applet.tunePowerSlider();
        QVERIFY(rf != nullptr);
        QVERIFY(tune != nullptr);

        const QString neither = QStringLiteral(
            "Neither TUNE nor the two-tone test uses this slider. "
            "TUNE's drive source is chosen in Setup > Transmit > Power.");
        QVERIFY(rf->accessibleDescription().isEmpty());
        QCOMPARE(tune->accessibleDescription(), neither);
        QVERIFY(tune->toolTip().endsWith(neither));

        tx.setTuneDrivePowerSource(DrivePowerSource::TuneSlider);
        QVERIFY(tune->accessibleDescription().isEmpty());
        tx.setTune(true);
        const QString tuneNotUsing = QStringLiteral(
            "TUNE is not using this slider. "
            "TUNE's drive source is chosen in Setup > Transmit > Power.");
        QCOMPARE(rf->accessibleDescription(), tuneNotUsing);
        QVERIFY(rf->toolTip().endsWith(tuneNotUsing));
        QVERIFY(tune->accessibleDescription().isEmpty());
        tx.setTune(false);
        QVERIFY(rf->accessibleDescription().isEmpty());

        tx.setTwoToneDrivePowerSource(DrivePowerSource::Fixed);
        tx.setTwoToneActive(true);
        const QString twoToneNotUsing = QStringLiteral(
            "The two-tone test is not using this slider. The two-tone test's "
            "drive source is chosen in Setup > Test > Two-Tone IMD.");
        QCOMPARE(rf->accessibleDescription(), twoToneNotUsing);
        QCOMPARE(tune->accessibleDescription(), twoToneNotUsing);
        tx.setTwoToneActive(false);
        QVERIFY(rf->accessibleDescription().isEmpty());
        QVERIFY(tune->accessibleDescription().isEmpty());
    }

    void driveSourceGate_accessibleDescriptionUnderTransmitSettingsGate()
    {
        RadioModel rm(RadioModel::Role::Remote);
        TransmitModel& tx = rm.transmitModel();
        TxApplet applet(&rm);
        QSlider* tune = applet.tunePowerSlider();
        QVERIFY(tune != nullptr);
        const QString neither = QStringLiteral(
            "Neither TUNE nor the two-tone test uses this slider. "
            "TUNE's drive source is chosen in Setup > Transmit > Power.");

        applet.setTransmitChainSettingsPermitted(false, QStringLiteral("Closed."));
        QCOMPARE(tune->accessibleDescription(), QStringLiteral("Closed."));
        tx.setTuneDrivePowerSource(DrivePowerSource::TuneSlider);
        QCOMPARE(tune->accessibleDescription(), QStringLiteral("Closed."));
        applet.setTransmitChainSettingsPermitted(true);
        QVERIFY(tune->isEnabled());
        QVERIFY(tune->accessibleDescription().isEmpty());

        applet.setTransmitChainSettingsPermitted(false, QStringLiteral("Closed."));
        tx.setTuneDrivePowerSource(DrivePowerSource::DriveSlider);
        QCOMPARE(tune->accessibleDescription(), QStringLiteral("Closed."));
        applet.setTransmitChainSettingsPermitted(true);
        QVERIFY(!tune->isEnabled());
        QCOMPARE(tune->accessibleDescription(), neither);
    }
};

QTEST_MAIN(TestTxAppletRfPowerPerBand)
#include "tst_tx_applet_rf_power_per_band.moc"
