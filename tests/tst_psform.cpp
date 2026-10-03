// no-port-check: NereusSDR-original unit-test file.  Thetis cite comments
// document upstream sources; no Thetis logic ported in this test file.
// =================================================================
// tests/tst_psform.cpp  (NereusSDR)
// =================================================================
//
// Unit tests for the Phase 3M-4 Task 8 PsForm modeless dialog.
//
// PsForm ports Thetis PSForm.cs (1,164 LOC) [v2.10.3.13] verbatim — title
// "PureSignal 3.0", ClientSize 560x300 default with Advanced collapse to
// 560x60.  This test file exercises:
//
//   1. The dialog constructs with null RadioModel + PureSignal pointers
//      (test-friendly seam).
//
//   2. The retained PS3 controls exist by objectName and the removed PS2
//      controls are absent.
//
//   3. The Advanced toggle collapses the body widgets and restores them.
//
//   4. The OFF button invokes PureSignal::reset() when wired.
//
//   5. The Single Cal button invokes PureSignal::singleCalibrate() when wired.
//
//   6. The Two-tone toggle invokes PureSignal::setTwoToneOn(checked).
//
//   7. The Save button is gated on PureSignal::correctionsBeingAppliedChanged
//      (post-Codex Fix D — see PR #212 comments).
//
//   8. The Always On Top checkbox toggles Qt::WindowStaysOnTopHint.
//
//   9. The retained Auto Attenuate default is checked.
//
//   10. The udPSMoxDelay default value is 0.2, range 0.1 to 1.0, per
//       PSForm.Designer.cs:346-372 [v2.10.3.15] (fix wave RD-I7; the
//       decimal arrays are tenths).
//
// Source: NereusSDR-original.  See PsForm.h for the Thetis cite map.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-06 — New test file for Phase 3M-4 Task 8: PsForm dialog
//                 unit tests.  J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include "OperatorWording.h"
#include "core/AppSettings.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QDir>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QSignalSpy>
#include <QSpinBox>

#include "core/PureSignal.h"
#include "core/session/PureSignalSessionFacade.h"
#include "core/TxChannel.h"
#include "gui/PsForm.h"
#include "models/PureSignalSettings.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

// WDSP TX channel id — from Thetis cmaster.c:177-190 [v2.10.3.13].
static constexpr int kTxChannelId = 1;

class TstPsForm : public QObject {
    Q_OBJECT

private slots:

    // ── Test 1: construct + destruct without any wiring ─────────────────────
    //
    // PsForm must tolerate being constructed with a nullptr PureSignal
    // pointer (radio not yet connected case).  The dialog appears but its
    // controls are inert.

    void constructAndDestruct_withNoPureSignal_doesNotCrash()
    {
        PsForm form(/*radioModel=*/nullptr, /*pureSignal=*/nullptr);
        QCOMPARE(form.windowTitle(), QStringLiteral("PureSignal 3.0"));
        QCOMPARE(form.isModal(), false);
    }

    // ── Test 2: retained PS3 controls exist by objectName ───────────────────

    void retainedPs3ControlsExistByObjectName()
    {
        PsForm form(nullptr, nullptr);

        // Top action row (7 buttons)
        QVERIFY(form.findChild<QPushButton*>(QStringLiteral("btnPSTwoToneGen")));
        QVERIFY(form.findChild<QPushButton*>(QStringLiteral("btnPSCalibrate")));
        QVERIFY(form.findChild<QPushButton*>(QStringLiteral("btnPSAmpView")));
        QVERIFY(form.findChild<QPushButton*>(QStringLiteral("btnPSAdvanced")));
        QVERIFY(form.findChild<QPushButton*>(QStringLiteral("btnPSSave")));
        QVERIFY(form.findChild<QPushButton*>(QStringLiteral("btnPSRestore")));
        QVERIFY(form.findChild<QPushButton*>(QStringLiteral("btnPSReset")));
        QVERIFY(form.findChild<QPushButton*>(QStringLiteral("btnPSAutomatic")));
        QVERIFY(form.findChild<QPushButton*>(QStringLiteral("btnPSApplyCurrent")));

        // Status row (2 badges)
        QVERIFY(form.findChild<QLabel*>(QStringLiteral("lblPSInfoFB")));
        QVERIFY(form.findChild<QLabel*>(QStringLiteral("lblPSInfoCO")));

        // Calibration option checkboxes
        QVERIFY(form.findChild<QCheckBox*>(QStringLiteral("chkPSAutoAttenuate")));
        QVERIFY(form.findChild<QCheckBox*>(QStringLiteral("chkQuickAttenuate")));
        QVERIFY(form.findChild<QCheckBox*>(QStringLiteral("chkPSAutoCalEnabled")));
        QVERIFY(form.findChild<QCheckBox*>(QStringLiteral("chkPSRunCalibrationProcessing")));
        QVERIFY(form.findChild<QCheckBox*>(QStringLiteral("chkPSHardwarePeakOverride")));

        // Timing controls
        QVERIFY(form.findChild<QDoubleSpinBox*>(QStringLiteral("udPSMoxDelay")));
        QVERIFY(form.findChild<QSpinBox*>(QStringLiteral("udPSPhnum")));
        QVERIFY(form.findChild<QDoubleSpinBox*>(QStringLiteral("udPSCalWait")));

        // Always-on-top + 2-Tone + warning icon
        QVERIFY(form.findChild<QCheckBox*>(QStringLiteral("chkPSOnTop")));
        QVERIFY(form.findChild<QCheckBox*>(QStringLiteral("chkShow2ToneMeasurements")));
        QVERIFY(form.findChild<QLabel*>(QStringLiteral("pbWarningSetPk")));

        // Calibration Information group (advanced section)
        QVERIFY(form.findChild<QGroupBox*>(QStringLiteral("grpPSInfo")));
        QVERIFY(form.findChild<QPushButton*>(QStringLiteral("btnDefaultPeaks")));
        QVERIFY(form.findChild<QCheckBox*>(QStringLiteral("checkLoopback")));
        QVERIFY(form.findChild<QLabel*>(QStringLiteral("lblPSActionStatus")));
        QVERIFY(form.findChild<QLabel*>(QStringLiteral("lblPSNativeStatus")));
        QVERIFY(form.findChild<QLabel*>(QStringLiteral("lblPSRoutingStatus")));
        for (int i = 0; i < 16; ++i) {
            QVERIFY2(form.findChild<QLabel*>(QStringLiteral("lblPSInfo%1").arg(i)),
                     qPrintable(QStringLiteral("missing raw PS3 info[%1]").arg(i)));
        }
        auto* loopback = form.findChild<QCheckBox*>(QStringLiteral("checkLoopback"));
        QVERIFY(!loopback->isEnabled());
        QVERIFY(loopback->toolTip().contains(QStringLiteral("AmpView")));
    }

    // ── Test 3: defaults match Thetis designer values ────────────────────────

    void defaultsMatchThetisDesignerValues()
    {
        PsForm form(nullptr, nullptr);

        // Retained Auto Attenuate default from PSForm.designer.cs.
        QCOMPARE(form.findChild<QCheckBox*>(QStringLiteral("chkPSAutoAttenuate"))->isChecked(), true);

        // Default unchecked
        QCOMPARE(form.findChild<QCheckBox*>(QStringLiteral("chkQuickAttenuate"))->isChecked(), false);
        QCOMPARE(form.findChild<QCheckBox*>(QStringLiteral("chkPSOnTop"))->isChecked(), false);
        QCOMPARE(form.findChild<QCheckBox*>(QStringLiteral("chkShow2ToneMeasurements"))->isChecked(), false);

        // From PSForm.Designer.cs:346-372 [v2.10.3.15]: udPSMoxDelay Value
        // 0.2, Minimum 0.1, Maximum 1.0 (fix wave RD-I7).
        auto* moxDelay = form.findChild<QDoubleSpinBox*>(QStringLiteral("udPSMoxDelay"));
        QCOMPARE(moxDelay->value(), 0.2);
        QCOMPARE(moxDelay->minimum(), 0.1);
        QCOMPARE(moxDelay->maximum(), 1.0);

        // From PSForm.designer.cs:801-805 [v2.10.3.13] — udPSCalWait default 0
        QCOMPARE(form.findChild<QDoubleSpinBox*>(QStringLiteral("udPSCalWait"))->value(), 0.0);

        // From PSForm.designer.cs:409-413 [v2.10.3.13] — udPSPhnum default 150
        QCOMPARE(form.findChild<QSpinBox*>(QStringLiteral("udPSPhnum"))->value(), 150);
    }

    // ── Test 4: PS2-only controls are not exposed by the PS3 form ──────────

    void removedPs2ControlsAreNotExposed()
    {
        PsForm form(nullptr, nullptr);
        QVERIFY(!form.findChild<QObject*>(QStringLiteral("chkPSPin")));
        QVERIFY(!form.findChild<QObject*>(QStringLiteral("chkPSMap")));
        QVERIFY(!form.findChild<QObject*>(QStringLiteral("chkPSStbl")));
        QVERIFY(!form.findChild<QObject*>(QStringLiteral("chkPSRelaxPtol")));
        QVERIFY(!form.findChild<QObject*>(QStringLiteral("lblPSTint")));
        QVERIFY(!form.findChild<QObject*>(QStringLiteral("comboPSTint")));
    }

    // ── Test 5: Advanced toggle collapses + restores body widgets ────────────
    //
    // From Thetis PSForm.cs:889-902 setAdvancedView [v2.10.3.13] —
    // sits immediately above the _advancedON declaration on line 888
    // (//MW0LGE attribution preserved — author tag from upstream
    // //MW0LGE_[2.9.0.7] version-stamped comment):
    //   _advancedON = !_advancedON;
    //   if (_advancedON) ClientSize = 560x60;
    //   else             ClientSize = 560x300;
    // NereusSDR mirrors via hide/show on the body widget container.

    void advancedTogglesBodyVisibility()
    {
        PsForm form(nullptr, nullptr);
        // Force layout so initial sizes are valid
        form.show();
        form.adjustSize();

        const auto* moxSpin =
            form.findChild<QDoubleSpinBox*>(QStringLiteral("udPSMoxDelay"));
        QVERIFY(moxSpin);
        QCOMPARE(moxSpin->isVisible(), true);

        // Toggle into Advanced (collapsed) — body hides.
        auto* btnAdv = form.findChild<QPushButton*>(QStringLiteral("btnPSAdvanced"));
        QVERIFY(btnAdv);
        btnAdv->click();
        QCOMPARE(moxSpin->isVisible(), false);
        QCOMPARE(form.isAdvancedCollapsed(), true);

        // Top action row stays visible (verify a few canonical buttons).
        QVERIFY(form.findChild<QPushButton*>(QStringLiteral("btnPSReset"))->isVisible());
        QVERIFY(form.findChild<QPushButton*>(QStringLiteral("btnPSCalibrate"))->isVisible());

        // Toggle back — body returns.
        btnAdv->click();
        QCOMPARE(moxSpin->isVisible(), true);
        QCOMPARE(form.isAdvancedCollapsed(), false);
    }

    // ── Test 6: Single Cal button invokes PureSignal::singleCalibrate ────────

    void singleCalButtonInvokesPureSignalSingleCalibrate()
    {
        TxChannel tx(kTxChannelId);
        PureSignal ps(nullptr, &tx, nullptr, nullptr, nullptr, nullptr);
        PsForm form(/*radioModel=*/nullptr, /*pureSignal=*/&ps);

        QSignalSpy startSpy(&ps, &PureSignal::calibrationStarted);
        auto* btn =
            form.findChild<QPushButton*>(QStringLiteral("btnPSCalibrate"));
        QVERIFY(btn);
        btn->click();
        QTRY_COMPARE(startSpy.count(), 1);
    }

    // ── Test 7: OFF button invokes PureSignal::reset ─────────────────────────

    void offButtonInvokesPureSignalReset()
    {
        TxChannel tx(kTxChannelId);
        PureSignal ps(nullptr, &tx, nullptr, nullptr, nullptr, nullptr);
        // Make sure auto-cal is on so reset() actually flips state.
        ps.setAutoCalEnabled(true);
        QCOMPARE(ps.isAutoCalEnabled(), true);

        PsForm form(nullptr, &ps);

        QSignalSpy autoSpy(&ps, &PureSignal::autoCalEnabledChanged);
        auto* btn = form.findChild<QPushButton*>(QStringLiteral("btnPSReset"));
        QVERIFY(btn);
        btn->click();
        // reset() calls forceAutoCalDisable() → setAutoCalEnabled(false).
        QTRY_COMPARE(autoSpy.count(), 1);
        QCOMPARE(ps.isAutoCalEnabled(), false);
    }

    // ── Test 8: Two-tone toggle invokes PureSignal::setTwoToneOn ─────────────
    //
    // From Thetis PSForm.cs:508-522 btnPSTwoToneGen_Click [v2.10.3.13] —
    // toggles _ttgenON and SetupForm.TTgenrun.  In NereusSDR this routes
    // through PureSignal → TwoToneController.

    void twoToneButtonInvokesPureSignalSetTwoToneOn()
    {
        TxChannel tx(kTxChannelId);
        PureSignal ps(nullptr, &tx, nullptr, nullptr, nullptr, nullptr);
        PsForm form(nullptr, &ps);

        auto* btn =
            form.findChild<QPushButton*>(QStringLiteral("btnPSTwoToneGen"));
        QVERIFY(btn);
        QCOMPARE(btn->isCheckable(), true);

        // Flip ON
        btn->click();
        QCOMPARE(btn->isChecked(), true);
        // Flip OFF
        btn->click();
        QCOMPARE(btn->isChecked(), false);
    }

    // ── Test 9: Always On Top checkbox sets WindowStaysOnTopHint ────────────

    void alwaysOnTopToggleSetsWindowStaysOnTopHint()
    {
        PsForm form(nullptr, nullptr);
        form.show();

        auto* chk = form.findChild<QCheckBox*>(QStringLiteral("chkPSOnTop"));
        QVERIFY(chk);
        QCOMPARE(chk->isChecked(), false);

        chk->setChecked(true);
        QCOMPARE(form.windowFlags() & Qt::WindowStaysOnTopHint,
                 Qt::WindowStaysOnTopHint);

        chk->setChecked(false);
        QCOMPARE(form.windowFlags() & Qt::WindowStaysOnTopHint,
                 Qt::WindowFlags{});
    }

    void geometryAndAlwaysOnTopRoundTripAcrossReopen()
    {
        AppSettings& settings = AppSettings::instance();
        const QString geometryKey = QStringLiteral("puresignal/geometry");
        const QString onTopKey = QStringLiteral("puresignal/alwaysOnTop");
        const QVariant oldGeometry = settings.value(geometryKey);
        const QVariant oldOnTop = settings.value(onTopKey);
        settings.setValue(geometryKey, QString());
        settings.setValue(onTopKey, false);

        QRect saved;
        {
            PsForm form(nullptr, nullptr);
            form.adjustSize();
            if (QScreen* screen = QGuiApplication::primaryScreen()) {
                const QRect available = screen->availableGeometry();
                // Leave title-bar room even on the 533-point scaled offscreen screen.
                form.move(available.left(), available.top() + 35);
            }
            form.show();
            QCoreApplication::processEvents();
            auto* onTop = form.findChild<QCheckBox*>(QStringLiteral("chkPSOnTop"));
            QVERIFY(onTop);
            onTop->setChecked(true);
            QCoreApplication::processEvents();
            saved = form.geometry();
            form.close();
            QVERIFY(!settings.value(geometryKey).toString().isEmpty());
        }
        {
            PsForm reopened(nullptr, nullptr);
            auto* onTop = reopened.findChild<QCheckBox*>(QStringLiteral("chkPSOnTop"));
            QVERIFY(onTop);
            QVERIFY(onTop->isChecked());
            QCOMPARE(reopened.windowFlags() & Qt::WindowStaysOnTopHint,
                     Qt::WindowStaysOnTopHint);
            QCOMPARE(reopened.size(), saved.size());
            const QRect available = reopened.screen()->availableGeometry();
            QVERIFY2((reopened.pos() - saved.topLeft()).manhattanLength() <= 8,
                     qPrintable(QStringLiteral("restored %1,%2 saved %3,%4; available %5x%6; size %7x%8")
                         .arg(reopened.x()).arg(reopened.y()).arg(saved.x()).arg(saved.y())
                         .arg(available.width()).arg(available.height())
                         .arg(reopened.width()).arg(reopened.height())));
        }

        settings.setValue(geometryKey, oldGeometry);
        settings.setValue(onTopKey, oldOnTop);
    }

    // ── Test 10: MOX-delay spinbox forwards to PureSignal::setMoxDelay ─────

    void moxDelaySpinBoxForwardsToPureSignalSetMoxDelay()
    {
        TxChannel tx(kTxChannelId);
        PureSignal ps(nullptr, &tx, nullptr, nullptr, nullptr, nullptr);
        PsForm form(nullptr, &ps);

        QSignalSpy spy(&ps, &PureSignal::moxDelayChanged);
        auto* spin =
            form.findChild<QDoubleSpinBox*>(QStringLiteral("udPSMoxDelay"));
        QVERIFY(spin);
        spin->setValue(0.5);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(ps.moxDelay(), 0.5);
    }

    void acceptedSettingsRoundTripThroughTheSettingsModel()
    {
        TxChannel tx(kTxChannelId);
        PureSignal ps(nullptr, &tx, nullptr, nullptr, nullptr, nullptr);
        PsForm form(nullptr, &ps);
        PureSignalSettings* settings = ps.settings();
        QVERIFY(settings);

        auto* desiredAuto = form.findChild<QCheckBox*>(QStringLiteral("chkPSAutoCalEnabled"));
        auto* runCal = form.findChild<QCheckBox*>(QStringLiteral("chkPSRunCalibrationProcessing"));
        auto* autoAttenuate = form.findChild<QCheckBox*>(QStringLiteral("chkPSAutoAttenuate"));
        auto* peakOverride = form.findChild<QCheckBox*>(QStringLiteral("chkPSHardwarePeakOverride"));
        QVERIFY(desiredAuto && runCal && autoAttenuate && peakOverride);

        desiredAuto->setChecked(true);
        runCal->setChecked(false);
        autoAttenuate->setChecked(false);
        settings->setHardwarePeakOverride(0.42);
        peakOverride->setChecked(true);
        QCOMPARE(settings->autoCalEnabled(), true);
        QCOMPARE(settings->runCalibrationProcessing(), false);
        QCOMPARE(settings->autoAttenuate(), false);
        QCOMPARE(settings->hardwarePeakOverrideEnabled(), true);
        QCOMPARE(settings->hardwarePeakOverride(), 0.42);
    }

    void showTwoTonePreferenceEmitsGuiViewIntent()
    {
        PsForm form(nullptr, nullptr);
        auto* check = form.findChild<QCheckBox*>(
            QStringLiteral("chkShow2ToneMeasurements"));
        QVERIFY(check);
        QSignalSpy changed(&form, &PsForm::showTwoToneMeasurementsChanged);

        check->setChecked(!check->isChecked());

        QCOMPARE(changed.count(), 1);
        QCOMPARE(changed.takeFirst().at(0).toBool(), check->isChecked());
    }

    void remoteAssetManagerEntryDoesNotRequireTransmitPermission()
    {
        RadioModel radio(RadioModel::Role::Remote);
        PureSignalSessionFacade* facade = radio.pureSignalFacade();
        QVERIFY(facade);
        facade->setRemoteCapabilities(true, true);
        QVERIFY(facade->applyRemoteProperty("available", true));
        QVERIFY(facade->applyRemoteProperty("canActuate", false));

        PsForm form(&radio, nullptr);
        auto* restore = form.findChild<QPushButton*>(QStringLiteral("btnPSRestore"));
        auto* single = form.findChild<QPushButton*>(QStringLiteral("btnPSCalibrate"));
        QVERIFY(restore);
        QVERIFY(single);
        QVERIFY(restore->isEnabled());
        QVERIFY(!single->isEnabled());
    }

    void radioBackedFormRoutesActionsThroughTheSharedFacade()
    {
        RadioModel radio(RadioModel::Role::Remote);
        PureSignalSessionFacade* facade = radio.pureSignalFacade();
        QVERIFY(facade);
        facade->setRemoteCapabilities(true, true);
        QVERIFY(facade->applyRemoteProperty("available", true));
        QVERIFY(facade->applyRemoteProperty("canActuate", true));

        Ps3Action observed = Ps3Action::OffReset;
        QVariantMap observedArguments;
        quint32 nextId = 72;
        facade->setRemoteRequestHandler(
            [&observed, &observedArguments, &nextId](Ps3Action action, const QVariantMap& arguments) {
                observed = action;
                observedArguments = arguments;
                return ++nextId;
            });

        TxChannel unusedTx(kTxChannelId);
        PureSignal unusedCoordinator(nullptr, &unusedTx, nullptr, nullptr, nullptr, nullptr);
        PsForm form(&radio, &unusedCoordinator);
        form.findChild<QPushButton*>(QStringLiteral("btnPSCalibrate"))->click();
        QCOMPARE(observed, Ps3Action::Single);
        QVERIFY(observedArguments.isEmpty());

        form.findChild<QPushButton*>(QStringLiteral("btnPSTwoToneGen"))->click();
        QCOMPARE(observed, Ps3Action::SetTwoTone);
        QCOMPARE(observedArguments.value("enabled").toBool(), true);
    }

    void actionProgressAndSessionRetirementAreVisible()
    {
        RadioModel radio(RadioModel::Role::Remote);
        PureSignalSessionFacade* facade = radio.pureSignalFacade();
        facade->setRemoteCapabilities(true, true);
        QVERIFY(facade->applyRemoteProperty("available", true));
        QVERIFY(facade->applyRemoteProperty("canActuate", true));
        facade->setRemoteRequestHandler([](Ps3Action, const QVariantMap&) { return 91u; });
        PsForm form(&radio, nullptr);
        auto* status = form.findChild<QLabel*>(QStringLiteral("lblPSActionStatus"));
        QVERIFY(status);

        form.findChild<QPushButton*>(QStringLiteral("btnPSCalibrate"))->click();
        facade->receiveRemoteActionResult(91, "ps3.single", Ps3ActionPhase::Pending, {}, {});
        QVERIFY(status->text().contains(QStringLiteral("pending"), Qt::CaseInsensitive));
        facade->receiveRemoteActionResult(91, "ps3.single", Ps3ActionPhase::Completed, {}, {});
        QVERIFY(status->text().contains(QStringLiteral("completed"), Qt::CaseInsensitive));

        form.findChild<QPushButton*>(QStringLiteral("btnPSCalibrate"))->click();
        facade->receiveRemoteActionResult(91, "ps3.single", Ps3ActionPhase::Pending, {}, {});
        facade->resetSession();
        // R-R3-21 wording plan: the retirement is said in user words.
        QVERIFY2(status->text().contains(QStringLiteral("connection to the Core changed")),
                 qPrintable(status->text()));
        QVERIFY(OperatorWording::isPlain(status->text()));
    }

    // ── Test 12: Save button gating on correctionsBeingApplied ──────────────
    //
    // From Thetis PSForm.cs:574-590 [v2.10.3.13]:
    //   if (puresignal.CorrectionsBeingApplied)
    //       btnPSSave.Enabled = true;
    //   else
    //       btnPSSave.Enabled = false;

    void saveButtonStartsDisabledWhenNoCorrections()
    {
        TxChannel tx(kTxChannelId);
        PureSignal ps(nullptr, &tx, nullptr, nullptr, nullptr, nullptr);
        PsForm form(nullptr, &ps);

        auto* btn = form.findChild<QPushButton*>(QStringLiteral("btnPSSave"));
        QVERIFY(btn);
        QCOMPARE(btn->isEnabled(), false);
    }

    void captureExpandedFormWhenRequested()
    {
        const QString directory = qEnvironmentVariable("NEREUS_DSP_UI_CAPTURE_DIR");
        if (directory.isEmpty()) {
            QSKIP("NEREUS_DSP_UI_CAPTURE_DIR is not set");
        }
        QVERIFY(QDir().mkpath(directory));
        PsForm form(nullptr, nullptr);
        if (form.isAdvancedCollapsed()) {
            form.findChild<QPushButton*>(QStringLiteral("btnPSAdvanced"))->click();
        }
        form.show();
        form.adjustSize();
        QCoreApplication::processEvents();
        QVERIFY(form.grab().save(directory + QStringLiteral("/psform-expanded.png")));
    }
};

QTEST_MAIN(TstPsForm)
#include "tst_psform.moc"
