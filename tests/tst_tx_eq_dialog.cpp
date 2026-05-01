// no-port-check: NereusSDR-original test file.  All Thetis source cites
// for the underlying TransmitModel properties live in TransmitModel.h
// and the dialog source itself.
// =================================================================
// tests/tst_tx_eq_dialog.cpp  (NereusSDR)
// =================================================================
//
// Phase 3M-3a-i Batch 3 (Task A.1) — TxEqDialog scaffold smoke tests.
// Phase 3M-3a-ii follow-up Batch 9 — chkLegacyEQ + parametric panel
// + slider styling fix.
//
// TxEqDialog is the modeless TX EQ dialog launched from the
// TxApplet's [EQ] right-click and the Tools → TX Equalizer menu.
// The legacy panel is bidirectionally bound to RadioModel::transmit-
// Model() with an m_updatingFromModel echo guard.  The parametric
// panel embeds a ParametricEqWidget (Tasks 1-5) and round-trips its
// points through TransmitModel.txEqParaEqData (Task 6 Thetis gzip+
// base64url envelope).
//
// Tests:
//   1. Dialog constructs without crash (RadioModel default ctor).
//   2. Initial values populate from TransmitModel defaults
//      (preamp=0, band[0]=-12, freq[0]=32, enable=false, Nc=2048).
//   3. Move preamp slider → TransmitModel.txEqPreampChanged emitted.
//   4. Move band 0 slider → TransmitModel.txEqBandChanged emitted with
//      idx=0 + new value.
//   5. Move freq 0 spinbox → TransmitModel.txEqFreqChanged emitted.
//   6. Toggle enable checkbox → TransmitModel.txEqEnabledChanged emitted.
//   7. setTxEqPreamp(N) external setter → dialog preamp slider/spin
//      updates to N (round-trip via syncFromModel).
//   8. Echo guard: setting a TransmitModel value that triggers UI
//      update doesn't cause a re-emit storm (no infinite loop —
//      each setter only fires its own signal once).
//   9. Singleton: TxEqDialog::instance(...) returns the same pointer
//      on repeated calls.
//
// Batch 9 contracts:
//  10. Dialog contains chkLegacyEQ checkbox at top.
//  11. Toggling chkLegacyEQ flips the visible panel between legacy
//      sliders and the parametric panel via QStackedWidget.
//  12. Parametric panel embeds a ParametricEqWidget instance.
//  13. Parametric panel exposes 5/10/18 band-count radios.
//  14. Dialog does NOT contain a profile combo (profile mgmt lives
//      on TxApplet).
//  15. Dialog does NOT contain Save / Save As / Delete buttons.
//  16. Legacy band-column sliders carry the Style::sliderVStyle()
//      stylesheet (regression guard).
//  17. closeEvent hides the dialog instead of destroying it
//      (keeps the singleton alive for fast re-show).
//
// =================================================================

#include <QtTest/QtTest>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalSpy>
#include <QSlider>
#include <QSpinBox>
#include <QStackedWidget>

#include "core/AppSettings.h"
#include "core/ParaEqEnvelope.h"
#include "gui/applets/TxEqDialog.h"
#include "gui/widgets/ParametricEqWidget.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

class TestTxEqDialog : public QObject {
    Q_OBJECT

private slots:

    void initTestCase()
    {
        if (!qApp) {
            static int argc = 0;
            new QApplication(argc, nullptr);
        }
        AppSettings::instance().clear();
    }

    void cleanup()
    {
        AppSettings::instance().clear();
    }

    // ── 1. Construct ────────────────────────────────────────────────
    void constructsWithoutCrash()
    {
        RadioModel rm;
        TxEqDialog dlg(&rm);
        QVERIFY(dlg.findChild<QCheckBox*>(QStringLiteral("TxEqEnableChk")));
        QVERIFY(dlg.findChild<QSlider*>(QStringLiteral("TxEqPreampSlider")));
    }

    // ── 2. Initial values populate from TransmitModel defaults ─────
    void initialValues_matchTransmitModelDefaults()
    {
        RadioModel rm;
        TxEqDialog dlg(&rm);

        TransmitModel& tx = rm.transmitModel();

        // Enable default off.
        auto* en = dlg.findChild<QCheckBox*>(QStringLiteral("TxEqEnableChk"));
        QVERIFY(en);
        QCOMPARE(en->isChecked(), tx.txEqEnabled());
        QCOMPARE(en->isChecked(), false);

        // Preamp default 0.
        auto* pre = dlg.findChild<QSlider*>(QStringLiteral("TxEqPreampSlider"));
        auto* preSpin = dlg.findChild<QSpinBox*>(QStringLiteral("TxEqPreampSpin"));
        QVERIFY(pre);
        QVERIFY(preSpin);
        QCOMPARE(pre->value(), tx.txEqPreamp());
        QCOMPARE(preSpin->value(), tx.txEqPreamp());
        QCOMPARE(pre->value(), 0);

        // Band 0 default -12 (matches TransmitModel m_txEqBand init).
        auto* b0 = dlg.findChild<QSlider*>(QStringLiteral("TxEqBandSlider0"));
        auto* b0s = dlg.findChild<QSpinBox*>(QStringLiteral("TxEqBandSpin0"));
        QVERIFY(b0);
        QVERIFY(b0s);
        QCOMPARE(b0->value(), tx.txEqBand(0));
        QCOMPARE(b0s->value(), tx.txEqBand(0));
        QCOMPARE(b0->value(), -12);

        // Freq 0 default 32 Hz.
        auto* f0 = dlg.findChild<QSpinBox*>(QStringLiteral("TxEqFreqSpin0"));
        QVERIFY(f0);
        QCOMPARE(f0->value(), tx.txEqFreq(0));
        QCOMPARE(f0->value(), 32);

        // Nc default 2048.
        auto* nc = dlg.findChild<QSpinBox*>(QStringLiteral("TxEqNcSpin"));
        QVERIFY(nc);
        QCOMPARE(nc->value(), tx.txEqNc());
        QCOMPARE(nc->value(), 2048);

        // Mp default off.
        auto* mp = dlg.findChild<QCheckBox*>(QStringLiteral("TxEqMpChk"));
        QVERIFY(mp);
        QCOMPARE(mp->isChecked(), tx.txEqMp());
        QCOMPARE(mp->isChecked(), false);

        // Ctfmode default 0, Wintype default 0.
        auto* ctf = dlg.findChild<QComboBox*>(QStringLiteral("TxEqCtfmodeCombo"));
        auto* win = dlg.findChild<QComboBox*>(QStringLiteral("TxEqWintypeCombo"));
        QVERIFY(ctf);
        QVERIFY(win);
        QCOMPARE(ctf->currentIndex(), tx.txEqCtfmode());
        QCOMPARE(win->currentIndex(), tx.txEqWintype());
        QCOMPARE(ctf->currentIndex(), 0);
        QCOMPARE(win->currentIndex(), 0);
    }

    // ── 3. Preamp slider → txEqPreampChanged ────────────────────────
    void preampSlider_emitsTxEqPreampChanged()
    {
        RadioModel rm;
        TxEqDialog dlg(&rm);
        TransmitModel& tx = rm.transmitModel();
        QSignalSpy spy(&tx, &TransmitModel::txEqPreampChanged);

        auto* pre = dlg.findChild<QSlider*>(QStringLiteral("TxEqPreampSlider"));
        QVERIFY(pre);
        pre->setValue(7);

        // Slider emits valueChanged → onPreampChanged → setTxEqPreamp → signal.
        // Note: the model→UI sync handler also fires syncFromModel which
        // re-sets the spinbox; but setTxEqPreamp itself only fires once.
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(0).toInt(), 7);
        QCOMPARE(tx.txEqPreamp(), 7);
    }

    // ── 4. Band 0 slider → txEqBandChanged with idx=0 ───────────────
    void band0Slider_emitsTxEqBandChanged()
    {
        RadioModel rm;
        TxEqDialog dlg(&rm);
        TransmitModel& tx = rm.transmitModel();
        QSignalSpy spy(&tx, &TransmitModel::txEqBandChanged);

        auto* b0 = dlg.findChild<QSlider*>(QStringLiteral("TxEqBandSlider0"));
        QVERIFY(b0);
        b0->setValue(5);

        QCOMPARE(spy.count(), 1);
        const QList<QVariant> args = spy.takeFirst();
        QCOMPARE(args.at(0).toInt(), 0);
        QCOMPARE(args.at(1).toInt(), 5);
        QCOMPARE(tx.txEqBand(0), 5);
    }

    // ── 5. Freq 0 spinbox → txEqFreqChanged with idx=0 ──────────────
    void freq0Spin_emitsTxEqFreqChanged()
    {
        RadioModel rm;
        TxEqDialog dlg(&rm);
        TransmitModel& tx = rm.transmitModel();
        QSignalSpy spy(&tx, &TransmitModel::txEqFreqChanged);

        auto* f0 = dlg.findChild<QSpinBox*>(QStringLiteral("TxEqFreqSpin0"));
        QVERIFY(f0);
        f0->setValue(75);

        QCOMPARE(spy.count(), 1);
        const QList<QVariant> args = spy.takeFirst();
        QCOMPARE(args.at(0).toInt(), 0);
        QCOMPARE(args.at(1).toInt(), 75);
        QCOMPARE(tx.txEqFreq(0), 75);
    }

    // ── 6. Enable checkbox → txEqEnabledChanged ─────────────────────
    void enableCheckbox_emitsTxEqEnabledChanged()
    {
        RadioModel rm;
        TxEqDialog dlg(&rm);
        TransmitModel& tx = rm.transmitModel();
        QSignalSpy spy(&tx, &TransmitModel::txEqEnabledChanged);

        auto* en = dlg.findChild<QCheckBox*>(QStringLiteral("TxEqEnableChk"));
        QVERIFY(en);
        en->setChecked(true);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(0).toBool(), true);
        QCOMPARE(tx.txEqEnabled(), true);
    }

    // ── 7. External setTxEqPreamp(N) → dialog UI updates ────────────
    void externalSetTxEqPreamp_updatesDialogPreamp()
    {
        RadioModel rm;
        TxEqDialog dlg(&rm);
        TransmitModel& tx = rm.transmitModel();

        auto* pre     = dlg.findChild<QSlider*>(QStringLiteral("TxEqPreampSlider"));
        auto* preSpin = dlg.findChild<QSpinBox*>(QStringLiteral("TxEqPreampSpin"));
        QVERIFY(pre && preSpin);
        QCOMPARE(pre->value(), 0);

        tx.setTxEqPreamp(11);
        QCOMPARE(pre->value(), 11);
        QCOMPARE(preSpin->value(), 11);
    }

    // ── 8. Echo guard — model setter fires signal exactly once ──────
    // If the echo guard were missing, the slider valueChanged from
    // syncFromModel would call back into setTxEqPreamp, which would
    // emit again, etc.  Verify the signal count stays at 1.
    void echoGuard_externalSetterDoesNotReEmit()
    {
        RadioModel rm;
        TxEqDialog dlg(&rm);
        TransmitModel& tx = rm.transmitModel();
        QSignalSpy spy(&tx, &TransmitModel::txEqPreampChanged);

        tx.setTxEqPreamp(4);   // single emit expected
        QCOMPARE(spy.count(), 1);

        tx.setTxEqPreamp(4);   // no-op — value unchanged, no re-emit
        QCOMPARE(spy.count(), 1);
    }

    // ── 9. Singleton — instance() returns same pointer ──────────────
    void singleton_returnsSameInstance()
    {
        RadioModel rm;
        TxEqDialog* a = TxEqDialog::instance(&rm);
        TxEqDialog* b = TxEqDialog::instance(&rm);
        QVERIFY(a != nullptr);
        QCOMPARE(a, b);
        // Cleanup — the singleton survives across tests, so delete it
        // explicitly to avoid leaks across test-method boundaries.
        delete a;
    }

    // =====================================================================
    // Phase 3M-3a-ii follow-up Batch 9 — chkLegacyEQ + parametric panel
    // =====================================================================

    // ── 10. Dialog contains chkLegacyEQ checkbox ───────────────────
    void dialogContainsLegacyToggle()
    {
        RadioModel rm;
        TxEqDialog dlg(&rm);

        auto* tog = dlg.findChild<QCheckBox*>(QStringLiteral("TxEqLegacyToggle"));
        QVERIFY(tog);
        QCOMPARE(tog, dlg.legacyToggle());
        // Default checked (per Thetis eqform.cs:972-973 [v2.10.3.13]).
        QCOMPARE(tog->isChecked(), true);
    }

    // ── 11. Toggling chkLegacyEQ flips the visible panel ────────────
    void legacyTogglesBetweenLegacyAndParametricPanels()
    {
        RadioModel rm;
        TxEqDialog dlg(&rm);

        auto* stack = dlg.panelStack();
        QVERIFY(stack);
        QVERIFY(dlg.legacyPanel());
        QVERIFY(dlg.parametricPanel());

        // Default: legacy shown (index 0).
        QCOMPARE(dlg.legacyToggle()->isChecked(), true);
        QCOMPARE(stack->currentWidget(), dlg.legacyPanel());

        // Uncheck — parametric panel shown.
        dlg.legacyToggle()->setChecked(false);
        QCOMPARE(stack->currentWidget(), dlg.parametricPanel());

        // Re-check — legacy panel shown.
        dlg.legacyToggle()->setChecked(true);
        QCOMPARE(stack->currentWidget(), dlg.legacyPanel());
    }

    // ── 11b. chkLegacyEQ state persists in AppSettings ──────────────
    void legacyToggleStatePersistsInAppSettings()
    {
        RadioModel rm;
        {
            TxEqDialog dlg(&rm);
            QCOMPARE(dlg.legacyToggle()->isChecked(), true);
            dlg.legacyToggle()->setChecked(false);
            // Persistence is synchronous via setValue.
        }

        // Reconstruct — the new dialog should pick up the persisted False.
        {
            TxEqDialog dlg2(&rm);
            QCOMPARE(dlg2.legacyToggle()->isChecked(), false);
            QCOMPARE(dlg2.panelStack()->currentWidget(), dlg2.parametricPanel());
        }
    }

    // ── 12. Parametric panel embeds ParametricEqWidget ──────────────
    void parametricPanelContainsParametricEqWidget()
    {
        RadioModel rm;
        TxEqDialog dlg(&rm);

        auto* w = dlg.parametricWidget();
        QVERIFY(w);
        QVERIFY(dlg.findChild<ParametricEqWidget*>(
                   QStringLiteral("TxEqParametricWidget")));
        // Defaults match Thetis ucParametricEq1 widget property block
        // at eqform.cs:928-967 [v2.10.3.13].
        QCOMPARE(w->dbMin(),         -24.0);
        QCOMPARE(w->dbMax(),          24.0);
        QCOMPARE(w->frequencyMinHz(),  0.0);
        QCOMPARE(w->frequencyMaxHz(), 2700.0);
        QCOMPARE(w->qMin(),            0.2);
        QCOMPARE(w->qMax(),           20.0);
        QCOMPARE(w->bandCount(),      10);
        QCOMPARE(w->parametricEq(),   true);
    }

    // ── 13. Parametric panel exposes 5/10/18 band-count radios ──────
    void parametricPanelContainsBandCountRadios_5_10_18()
    {
        RadioModel rm;
        TxEqDialog dlg(&rm);

        auto* r5  = dlg.findChild<QRadioButton*>(
                       QStringLiteral("TxEqParaBands5Radio"));
        auto* r10 = dlg.findChild<QRadioButton*>(
                       QStringLiteral("TxEqParaBands10Radio"));
        auto* r18 = dlg.findChild<QRadioButton*>(
                       QStringLiteral("TxEqParaBands18Radio"));
        QVERIFY(r5);
        QVERIFY(r10);
        QVERIFY(r18);
        // Default 10-band (per Thetis eqform.cs:507 radParaEQ_10.Checked = true).
        QCOMPARE(r10->isChecked(), true);
        QCOMPARE(r5->isChecked(),  false);
        QCOMPARE(r18->isChecked(), false);

        // Group exposes the band counts as button IDs.
        auto* grp = dlg.bandCountGroup();
        QVERIFY(grp);
        QCOMPARE(grp->id(r5),  5);
        QCOMPARE(grp->id(r10), 10);
        QCOMPARE(grp->id(r18), 18);
    }

    // ── 14. Dialog does NOT contain a profile combo ─────────────────
    void dialogDoesNotContainProfileCombo()
    {
        RadioModel rm;
        TxEqDialog dlg(&rm);

        // No combo with the old TxEqProfileCombo object name.
        QVERIFY(!dlg.findChild<QComboBox*>(QStringLiteral("TxEqProfileCombo")));

        // The only QComboBoxes in the dialog should be the WDSP filter
        // controls (Cutoff, Window) — exactly two, no more.
        const auto combos = dlg.findChildren<QComboBox*>();
        QCOMPARE(combos.size(), 2);
        QStringList names;
        for (auto* c : combos) { names << c->objectName(); }
        QVERIFY2(names.contains(QStringLiteral("TxEqCtfmodeCombo")),
                 qPrintable(names.join(QStringLiteral(", "))));
        QVERIFY2(names.contains(QStringLiteral("TxEqWintypeCombo")),
                 qPrintable(names.join(QStringLiteral(", "))));
    }

    // ── 15. Dialog does NOT contain Save / Save As / Delete buttons ─
    void dialogDoesNotContainSaveSaveAsDelete()
    {
        RadioModel rm;
        TxEqDialog dlg(&rm);

        QVERIFY(!dlg.findChild<QPushButton*>(QStringLiteral("TxEqProfileSaveBtn")));
        QVERIFY(!dlg.findChild<QPushButton*>(QStringLiteral("TxEqProfileSaveAsBtn")));
        QVERIFY(!dlg.findChild<QPushButton*>(QStringLiteral("TxEqProfileDeleteBtn")));

        // Sanity: no QPushButton in the dialog has the strings "Save" /
        // "Save As" / "Delete" as its caption.
        const auto btns = dlg.findChildren<QPushButton*>();
        for (auto* b : btns) {
            const QString t = b->text();
            QVERIFY2(t != QStringLiteral("Save"),
                     qPrintable(QStringLiteral("rogue Save button: ") + b->objectName()));
            QVERIFY2(t != QStringLiteral("Save As..."),
                     qPrintable(QStringLiteral("rogue Save As... button: ") + b->objectName()));
            QVERIFY2(t != QStringLiteral("Delete"),
                     qPrintable(QStringLiteral("rogue Delete button: ") + b->objectName()));
        }
    }

    // ── 16. Legacy band-column slider style includes "QSlider" ──────
    // Regression guard for Batch 9's slider/spinbox styling fix —
    // confirms the per-column sliders pick up the project's vertical
    // slider stylesheet.  Style::sliderVStyle() always emits a
    // QSlider::groove:vertical / QSlider::handle:vertical block, so
    // any non-default project styling will contain "QSlider" in the
    // applied stylesheet.
    void legacyBandColumnSlidersUseSliderVStyle()
    {
        RadioModel rm;
        TxEqDialog dlg(&rm);

        // Pick a representative band slider.
        auto* b0 = dlg.findChild<QSlider*>(QStringLiteral("TxEqBandSlider0"));
        QVERIFY(b0);
        const QString css = b0->styleSheet();
        QVERIFY2(css.contains(QStringLiteral("QSlider")),
                 qPrintable(QStringLiteral("band slider stylesheet missing: ") + css));
        QVERIFY2(css.contains(QStringLiteral("vertical")),
                 qPrintable(QStringLiteral("band slider stylesheet missing 'vertical': ") + css));

        // Spinboxes too (kSpinBoxStyle has "QSpinBox" in the rule head).
        auto* b0s = dlg.findChild<QSpinBox*>(QStringLiteral("TxEqBandSpin0"));
        QVERIFY(b0s);
        const QString spinCss = b0s->styleSheet();
        QVERIFY2(spinCss.contains(QStringLiteral("QSpinBox")),
                 qPrintable(QStringLiteral("band db spinbox stylesheet missing: ") + spinCss));

        auto* f0 = dlg.findChild<QSpinBox*>(QStringLiteral("TxEqFreqSpin0"));
        QVERIFY(f0);
        QVERIFY2(f0->styleSheet().contains(QStringLiteral("QSpinBox")),
                 qPrintable(QStringLiteral("band freq spinbox stylesheet missing")));

        // Preamp column too (special case: bandIndex=-1).
        auto* pre = dlg.findChild<QSlider*>(QStringLiteral("TxEqPreampSlider"));
        QVERIFY(pre);
        QVERIFY(pre->styleSheet().contains(QStringLiteral("QSlider")));
    }

    // ── 16b. Parametric edit stores Thetis gzip/base64url envelope ────
    void parametricEditStoresThetisEnvelopeAndUpdatesLegacyPath()
    {
        RadioModel rm;
        TxEqDialog dlg(&rm);
        TransmitModel& tx = rm.transmitModel();
        ParametricEqWidget* w = dlg.parametricWidget();
        QVERIFY(w);

        w->setGlobalGainDb(3.0);

        const QString blob = tx.txEqParaEqData();
        QVERIFY(!blob.isEmpty());
        QVERIFY(!blob.trimmed().startsWith(QLatin1Char('{')));

        const std::optional<QString> decoded = ParaEqEnvelope::decode(blob);
        QVERIFY(decoded.has_value());
        QVERIFY(decoded->contains(QStringLiteral("\"global_gain_db\": 3")));

        // Regression guard for PR #159 review thread: parametric edits also
        // push through the legacy scalar path that RadioModel maps to WDSP.
        QCOMPARE(tx.txEqPreamp(), 3);
    }

    // ── 16c. Encoded model blob hydrates the parametric widget ─────────
    void encodedTxParaEqDataHydratesParametricWidget()
    {
        RadioModel rm;
        TxEqDialog dlg(&rm);
        TransmitModel& tx = rm.transmitModel();
        ParametricEqWidget* w = dlg.parametricWidget();
        QVERIFY(w);
        QCOMPARE(w->globalGainDb(), 0.0);

        ParametricEqWidget saved;
        saved.setGlobalGainDb(7.0);
        const QString blob = ParaEqEnvelope::encode(saved.saveToJson());
        QVERIFY(!blob.isEmpty());

        tx.setTxEqParaEqData(blob);
        QCOMPARE(w->globalGainDb(), 7.0);
    }

    // ── 17. closeEvent hides instead of destroying ──────────────────
    void closeEventHidesInsteadOfDestroying()
    {
        RadioModel rm;
        TxEqDialog dlg(&rm);
        dlg.show();
        QCOMPARE(dlg.isVisible(), true);

        QCloseEvent ev;
        ev.setAccepted(true);   // default; closeEvent should override
        QApplication::sendEvent(&dlg, &ev);

        QCOMPARE(ev.isAccepted(), false);   // event ignored
        QCOMPARE(dlg.isVisible(), false);   // dialog hidden
        // Pointer still valid — singleton lifecycle preserved.
        QVERIFY(dlg.legacyToggle());
    }
};

QTEST_MAIN(TestTxEqDialog)
#include "tst_tx_eq_dialog.moc"
