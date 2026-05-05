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

    // ── 2. setCurrentBand recalls per-band into the RF slider ───────────────
    //
    // Mirrors Thetis TXBand setter at console.cs:17513 [v2.10.3.13]:
    //   PWR = power_by_band[(int)value];
    void setCurrentBand_recallsPerBandIntoSlider()
    {
        RadioModel rm;
        TxApplet applet(&rm);

        // Pre-seed three bands with distinct values.
        rm.transmitModel().setPowerForBand(Band::Band20m, 20);
        rm.transmitModel().setPowerForBand(Band::Band40m, 80);
        rm.transmitModel().setPowerForBand(Band::Band10m, 5);

        auto* slider = applet.findChild<QSlider*>(
            QStringLiteral("TxRfPowerSlider"));
        QVERIFY(slider != nullptr);

        applet.setCurrentBand(Band::Band40m);
        QCOMPARE(slider->value(), 80);

        applet.setCurrentBand(Band::Band10m);
        QCOMPARE(slider->value(), 5);

        applet.setCurrentBand(Band::Band20m);
        QCOMPARE(slider->value(), 20);
    }

    // ── 3. Bootstrap call with default-band still recalls ───────────────────
    //
    // Regression test for Gap 3: TxApplet::m_currentBand defaults to
    // Band20m.  When MainWindow.cpp:1578 calls
    //   txApplet->setCurrentBand(pan0->band())
    // and pan0->band() == Band20m, the previous early-return guard caused
    // the call to no-op, so the loaded m_powerByBand[Band20m] never made
    // it into the slider — slider stayed at construct-time default 100 W.
    void setCurrentBand_sameAsDefault_stillRecallsSlider()
    {
        RadioModel rm;
        // Pre-seed Band20m to a non-default value BEFORE the applet exists.
        rm.transmitModel().setPowerForBand(Band::Band20m, 35);

        TxApplet applet(&rm);
        auto* slider = applet.findChild<QSlider*>(
            QStringLiteral("TxRfPowerSlider"));
        QVERIFY(slider != nullptr);
        // Construct-time default is 100 (TxApplet.cpp:281).
        QCOMPARE(slider->value(), 100);

        // Bootstrap call — same as MainWindow.cpp:1578 with pan0 on Band20m.
        applet.setCurrentBand(Band::Band20m);

        // Slider must have pulled the per-band stored value, not stayed at
        // the construct-time default.
        QCOMPARE(slider->value(), 35);
    }

    // ── 4. Per-band isolation across slider moves ───────────────────────────
    //
    // Moving the slider on band A must not bleed into band B's stored value.
    void slider_movesOnDifferentBands_isolatedPerBand()
    {
        RadioModel rm;
        TxApplet applet(&rm);

        auto* slider = applet.findChild<QSlider*>(
            QStringLiteral("TxRfPowerSlider"));
        QVERIFY(slider != nullptr);

        applet.setCurrentBand(Band::Band20m);
        slider->setValue(40);
        QCOMPARE(rm.transmitModel().powerForBand(Band::Band20m), 40);

        applet.setCurrentBand(Band::Band40m);
        slider->setValue(90);
        QCOMPARE(rm.transmitModel().powerForBand(Band::Band40m), 90);

        applet.setCurrentBand(Band::Band10m);
        slider->setValue(15);
        QCOMPARE(rm.transmitModel().powerForBand(Band::Band10m), 15);

        // Earlier bands must still hold their original assignments.
        QCOMPARE(rm.transmitModel().powerForBand(Band::Band20m), 40);
        QCOMPARE(rm.transmitModel().powerForBand(Band::Band40m), 90);

        // Slider visually reflects the active-band value when re-selected.
        applet.setCurrentBand(Band::Band20m);
        QCOMPARE(slider->value(), 40);

        applet.setCurrentBand(Band::Band40m);
        QCOMPARE(slider->value(), 90);
    }

    // ── 5. Persistence round-trip across TransmitModel reload ───────────────
    //
    // Canonical bug repro: with a per-MAC scope active, slider moves on
    // distinct bands must survive TransmitModel reconstruction +
    // loadFromSettings(mac).
    void slider_writes_roundtripAcrossReload()
    {
        // Phase A — write through the applet → model → AppSettings chain.
        {
            RadioModel rm;
            // Activate per-MAC auto-persist.  After this call,
            // setPowerForBand writes to AppSettings on every change.
            rm.transmitModel().loadFromSettings(QString::fromLatin1(kTestMac));

            TxApplet applet(&rm);
            auto* slider = applet.findChild<QSlider*>(
                QStringLiteral("TxRfPowerSlider"));
            QVERIFY(slider != nullptr);

            applet.setCurrentBand(Band::Band20m);
            slider->setValue(72);

            applet.setCurrentBand(Band::Band40m);
            slider->setValue(33);

            applet.setCurrentBand(Band::Band6m);
            slider->setValue(8);
        }

        // Phase B — fresh TransmitModel reads the saved per-band values.
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
    // Confirms the slider lambda routes through setPowerForBand (which
    // emits powerByBandChanged), not through some side-channel that would
    // break listeners (e.g. PA-cal UI watching the per-band store).
    void slider_emitsPowerByBandChanged()
    {
        RadioModel rm;
        TxApplet applet(&rm);
        applet.setCurrentBand(Band::Band40m);

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

    // ── 7. CTUN pan: slider write goes to active-slice band, not pan band ──
    //
    // Codex P1 review on PR #192 thread r3190869829: when the user pans
    // the panadapter (CTUN mode) without retuning the slice, MainWindow's
    // PanadapterModel::bandChanged callback fires setCurrentBand(panBand),
    // which moves m_currentBand off the slice band.  Pre-fix, a slider
    // event in this state wrote to powerByBand[panBand] instead of the
    // canonical TX band slot (active-slice band), silently corrupting the
    // wrong band's stored value.
    //
    // Fix: TxApplet::txBand() pulls from the active slice's frequency.
    // setPowerForBand uses txBand(), not m_currentBand.
    void slider_write_usesActiveSliceBand_notPanBand()
    {
        RadioModel rm;
        rm.addSlice();
        SliceModel* slice = rm.activeSlice();
        QVERIFY(slice != nullptr);
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

        // Pre-condition: stored values for both bands at default 50 W.
        QCOMPARE(rm.transmitModel().powerForBand(Band::Band40m), 50);
        QCOMPARE(rm.transmitModel().powerForBand(Band::Band20m), 50);

        // User moves the slider while panadapter shows 20m.  The TX
        // band per RadioModel.cpp:903-905 is still 40m (the slice
        // didn't move), so the write must land on 40m.
        slider->setValue(85);

        QCOMPARE(rm.transmitModel().powerForBand(Band::Band40m), 85);
        // Pan band's slot must NOT be touched.
        QCOMPARE(rm.transmitModel().powerForBand(Band::Band20m), 50);
    }

    // ── 8. CTUN pan: setCurrentBand recall ignores non-TX-band updates ─────
    //
    // Codex P1 review on PR #192 thread r3190869831: setCurrentBand
    // unconditionally called tx.setPower(tx.powerForBand(band)) on every
    // invocation.  When fired from PanadapterModel::bandChanged with a
    // non-slice band, this paints the slider with the wrong band's value
    // AND the resulting powerChanged emission can write that wrong value
    // back into the active slice band's slot via setPowerUsingTargetDbm
    // txMode-0 side-effect (TransmitModel.cpp:825) — corruption.
    //
    // Fix: setCurrentBand only recalls the RF Power slider when
    // band == txBand() (the active slice's band).  Panadapter pans
    // without slice retune leave the slider locked on the TX band.
    void setCurrentBand_recall_skippedForNonTxBand()
    {
        RadioModel rm;
        rm.addSlice();
        SliceModel* slice = rm.activeSlice();
        QVERIFY(slice != nullptr);
        // Slice on 40m, with a pre-stored per-band value of 65 W.
        slice->setFrequency(7'150'000.0);
        rm.transmitModel().setPowerForBand(Band::Band40m, 65);
        rm.transmitModel().setPowerForBand(Band::Band20m, 5);

        TxApplet applet(&rm);

        auto* slider = applet.findChild<QSlider*>(
            QStringLiteral("TxRfPowerSlider"));
        QVERIFY(slider != nullptr);

        // Establish the slider on the slice band first (TX band recall
        // path — must work).
        applet.setCurrentBand(Band::Band40m);
        QCOMPARE(slider->value(), 65);

        // Simulate panadapter drifting to 20m: slider must stay on 65
        // because the slice didn't move.
        applet.setCurrentBand(Band::Band20m);
        QCOMPARE(slider->value(), 65);

        // m_power must also stay at 65 W — preventing the corruption
        // pipeline Codex flagged (powerChanged → setPowerUsingTargetDbm
        // → setPowerForBand(activeSliceBand, m_power) writing 5 W
        // into 40m's slot).
        QCOMPARE(rm.transmitModel().power(), 65);

        // Move the slice to 20m: now the slider SHOULD update because
        // the TX band actually changed.  Drive it via the same wire
        // MainWindow uses (slice frequencyChanged → setCurrentBand
        // with bandFromFrequency(slice->frequency())).
        slice->setFrequency(14'250'000.0);
        QCOMPARE(bandFromFrequency(slice->frequency()), Band::Band20m);
        applet.setCurrentBand(Band::Band20m);
        QCOMPARE(slider->value(), 5);
        QCOMPARE(rm.transmitModel().power(), 5);
    }
};

QTEST_MAIN(TestTxAppletRfPowerPerBand)
#include "tst_tx_applet_rf_power_per_band.moc"
