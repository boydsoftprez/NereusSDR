// =================================================================
// tests/tst_mnf_setup_page.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure. Thetis control
// names appear so the assertions document which upstream widget each
// NereusSDR control stands in for; no upstream logic is ported here.
//
// Tunable Notch Filter, Task 9: Setup -> DSP -> MNF, filling in the page
// that already existed as a disabled placeholder.
//
// Design: docs/architecture/2026-07-28-tunable-notch-filter-design.md
//   section 9    the page contents (table, Add, min width, auto-increase)
//   section 5.3  NotchModel::adminBusy, the Settings-side edit lock
//   section 5.4  the ported guard table the lock belongs to
//   section 8.3  the visual-notch toggle, whose checkbox lives here
//
// Coverage:
//   A. Auto-increase and visual-notch checkboxes exist with the Thetis
//      object names, carry the right defaults, and bind both directions
//      without an echo loop.
//   B. The table carries one row per notch; Add seeds from the active slice.
//   C. Row edits (frequency, width, active) and Delete reach NotchModel.
//   D. An in-flight row edit holds adminBusy and blocks the panadapter path;
//      committing clears the lock BEFORE the write lands.
//   E. The minimum-notch-width readout reads the live RxChannel and follows
//      RxChannel::minNotchWidthChanged.
// =================================================================

#include <QtTest/QtTest>
#include <QApplication>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QTableWidget>

#include "core/AppSettings.h"
#include "core/RxChannel.h"
#include "core/WdspEngine.h"
#include "gui/setup/DspSetupPages.h"
#include "models/NotchModel.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestMnfSetupPage : public QObject
{
    Q_OBJECT

private:
    // A RadioModel with a stream pool and one bound slice. Same fixture shape
    // as tests/tst_notch_channel_sync.cpp.
    static int seedSlice(RadioModel& model, double frequencyHz)
    {
        model.configureStreamPool(/*userDdcCount*/ 5, /*maxSlices*/ 5,
                                  /*defaultRateHz*/ 192000);
        const int id = model.addSlice();
        SliceModel* slice = model.sliceById(id);
        if (slice) {
            slice->setFrequency(frequencyHz);
        }
        return id;
    }

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

    // -- A. Auto-increase and visual notch --------------------------------

    void autoIncrease_controlExistsAndIsEnabled()
    {
        RadioModel model;
        MnfSetupPage page(&model);
        page.show();

        auto* chk = page.findChild<QCheckBox*>(QStringLiteral("chkMNFAutoIncrease"));
        QVERIFY(chk);
        // The placeholder ended in disableGroup(); the wired page must not.
        QVERIFY(chk->isEnabled());
    }

    // Decision D-b: the default is TRUE, confirmed in both upstreams.
    // WDSP creates nbp0 with `1, // auto-increase notch width`
    // (third_party/wdsp/src/RXA.c:105) and Thetis ships
    // chkMNFAutoIncrease.Checked = true (setup.designer.cs:44197
    // [v2.10.3.15]).
    void autoIncrease_defaultsChecked()
    {
        RadioModel model;
        QVERIFY(model.notchModel());
        QCOMPARE(model.notchModel()->autoIncrease(), true);

        MnfSetupPage page(&model);
        auto* chk = page.findChild<QCheckBox*>(QStringLiteral("chkMNFAutoIncrease"));
        QVERIFY(chk);
        QCOMPARE(chk->isChecked(), true);
    }

    void autoIncrease_mirrorsModelOnConstruction()
    {
        RadioModel model;
        model.notchModel()->setAutoIncrease(false);

        MnfSetupPage page(&model);
        auto* chk = page.findChild<QCheckBox*>(QStringLiteral("chkMNFAutoIncrease"));
        QVERIFY(chk);
        QCOMPARE(chk->isChecked(), false);
    }

    void autoIncrease_toggleWritesModel()
    {
        RadioModel model;
        model.notchModel()->setAutoIncrease(false);

        MnfSetupPage page(&model);
        auto* chk = page.findChild<QCheckBox*>(QStringLiteral("chkMNFAutoIncrease"));
        QVERIFY(chk);

        chk->setChecked(true);
        QCOMPARE(model.notchModel()->autoIncrease(), true);
    }

    void autoIncrease_modelChangeUpdatesCheckboxWithoutEcho()
    {
        RadioModel model;
        model.notchModel()->setAutoIncrease(false);

        MnfSetupPage page(&model);
        auto* chk = page.findChild<QCheckBox*>(QStringLiteral("chkMNFAutoIncrease"));
        QVERIFY(chk);

        QSignalSpy spy(model.notchModel(), &NotchModel::autoIncreaseChanged);
        model.notchModel()->setAutoIncrease(true);

        QCOMPARE(chk->isChecked(), true);
        // One emit only: the QSignalBlocker in the model->widget direction
        // stops the checkbox echoing back into setAutoIncrease.
        QCOMPARE(spy.count(), 1);
    }

    // Decision D-c: chkVisualNotch lives in Thetis's grpDSPMNF
    // (setup.designer.cs:44145 [v2.10.3.15]), so it belongs on this page and
    // not on a display page. It has no `Checked` assignment anywhere in the
    // designer (:44167-44179), so Windows Forms leaves it unchecked and
    // false is the correct default.
    void visualNotch_defaultsUncheckedAndBindsBothWays()
    {
        RadioModel model;
        MnfSetupPage page(&model);
        page.show();

        auto* chk = page.findChild<QCheckBox*>(QStringLiteral("chkVisualNotch"));
        QVERIFY2(chk, "MnfSetupPage is missing chkVisualNotch");
        QVERIFY(model.notchModel());
        QCOMPARE(model.notchModel()->visualEnabled(), false);
        QCOMPARE(chk->isChecked(), false);

        // Widget -> model.
        chk->setChecked(true);
        QCOMPARE(model.notchModel()->visualEnabled(), true);

        // Model -> widget, without an echo loop.
        QSignalSpy spy(model.notchModel(), &NotchModel::visualEnabledChanged);
        model.notchModel()->setVisualEnabled(false);
        QCOMPARE(chk->isChecked(), false);
        QCOMPARE(spy.count(), 1);
    }

    void nullModel_doesNotCrash()
    {
        MnfSetupPage page(nullptr);
        page.show();
        QVERIFY(!page.findChild<QCheckBox*>(QStringLiteral("chkMNFAutoIncrease")));
    }

    // -- B. Table and Add -------------------------------------------------

    void table_hasOneRowPerNotch()
    {
        RadioModel model;
        model.notchModel()->addNotch(14200000.0);
        model.notchModel()->addNotch(7100000.0);

        MnfSetupPage page(&model);
        page.show();

        auto* table = page.findChild<QTableWidget*>(QStringLiteral("tblMNFNotches"));
        QVERIFY(table);
        QCOMPARE(table->rowCount(), 2);
        QCOMPARE(table->columnCount(), 4);
    }

    void table_rowEditorsCarryThetisNamesAndValues()
    {
        RadioModel model;
        model.notchModel()->addNotch(14200000.0, 200.0);

        MnfSetupPage page(&model);
        page.show();

        auto* table = page.findChild<QTableWidget*>(QStringLiteral("tblMNFNotches"));
        QVERIFY(table);
        QCOMPARE(table->rowCount(), 1);

        auto* freq   = qobject_cast<QDoubleSpinBox*>(table->cellWidget(0, 0));
        auto* width  = qobject_cast<QDoubleSpinBox*>(table->cellWidget(0, 1));
        auto* active = qobject_cast<QCheckBox*>(table->cellWidget(0, 2));
        auto* del    = qobject_cast<QPushButton*>(table->cellWidget(0, 3));

        QVERIFY(freq);
        QVERIFY(width);
        QVERIFY(active);
        QVERIFY(del);
        QCOMPARE(freq->objectName(),   QStringLiteral("udMNFFreq"));
        QCOMPARE(width->objectName(),  QStringLiteral("udMNFWidth"));
        QCOMPARE(active->objectName(), QStringLiteral("chkMNFActive"));
        QCOMPARE(del->objectName(),    QStringLiteral("btnMNFDelete"));

        QCOMPARE(freq->value(), 14200000.0);
        QCOMPARE(width->value(), 200.0);
        QCOMPARE(active->isChecked(), true);

        // From Thetis setup.designer.cs:44329-44338 [v2.10.3.15] --
        // udMNFWidth.Maximum = 10000, udMNFWidth.Minimum = 0.
        QCOMPARE(width->minimum(), 0.0);
        QCOMPARE(width->maximum(), NotchModel::kMaxNotchWidthHz);

        // Correction 6: the centre bounds are NotchModel's, not local copies.
        QCOMPARE(freq->minimum(), NotchModel::kMinNotchCentreHz);
        QCOMPARE(freq->maximum(), NotchModel::kMaxNotchCentreHz);
    }

    void addButton_createsNotchAtVfoFrequency()
    {
        RadioModel model;
        seedSlice(model, 14074000.0);

        MnfSetupPage page(&model);
        page.show();

        auto* table = page.findChild<QTableWidget*>(QStringLiteral("tblMNFNotches"));
        auto* add   = page.findChild<QPushButton*>(QStringLiteral("btnMNFAdd"));
        QVERIFY(table);
        QVERIFY(add);
        QCOMPARE(table->rowCount(), 0);

        add->click();

        QCOMPARE(static_cast<int>(model.notchModel()->notches().size()), 1);
        QCOMPARE(model.notchModel()->notches().first().centerHz, 14074000.0);
        // Structural change reaches the table on the queued rebuild.
        QTRY_COMPARE(table->rowCount(), 1);
    }

    void addButton_isNoOpWithoutAnActiveSlice()
    {
        RadioModel model;
        MnfSetupPage page(&model);
        page.show();

        auto* add = page.findChild<QPushButton*>(QStringLiteral("btnMNFAdd"));
        QVERIFY(add);
        add->click();

        QCOMPARE(static_cast<int>(model.notchModel()->notches().size()), 0);
    }

    void table_followsNotchRemovedFromElsewhere()
    {
        RadioModel model;
        model.notchModel()->addNotch(14200000.0);
        const int id = model.notchModel()->notches().first().id;

        MnfSetupPage page(&model);
        page.show();

        auto* table = page.findChild<QTableWidget*>(QStringLiteral("tblMNFNotches"));
        QVERIFY(table);
        QCOMPARE(table->rowCount(), 1);

        model.notchModel()->removeNotch(id);
        QTRY_COMPARE(table->rowCount(), 0);
    }

    // -- C. Row edits and the adminBusy lock ------------------------------

    void rowFrequencyEdit_commitsToModel()
    {
        RadioModel model;
        model.notchModel()->addNotch(14200000.0, 200.0);
        const int id = model.notchModel()->notches().first().id;

        MnfSetupPage page(&model);
        page.show();
        auto* table = page.findChild<QTableWidget*>(QStringLiteral("tblMNFNotches"));
        QVERIFY(table);
        auto* freq = qobject_cast<QDoubleSpinBox*>(table->cellWidget(0, 0));
        QVERIFY(freq);

        freq->setValue(14200500.0);
        // QAbstractSpinBox::keyPressEvent emits editingFinished on Return.
        QTest::keyClick(freq, Qt::Key_Return);

        const Notch* n = model.notchModel()->notchById(id);
        QVERIFY(n);
        QCOMPARE(n->centerHz, 14200500.0);
    }

    void rowWidthEdit_commitsToModel()
    {
        RadioModel model;
        model.notchModel()->addNotch(14200000.0, 200.0);
        const int id = model.notchModel()->notches().first().id;

        MnfSetupPage page(&model);
        page.show();
        auto* table = page.findChild<QTableWidget*>(QStringLiteral("tblMNFNotches"));
        QVERIFY(table);
        auto* width = qobject_cast<QDoubleSpinBox*>(table->cellWidget(0, 1));
        QVERIFY(width);

        width->setValue(400.0);
        QTest::keyClick(width, Qt::Key_Return);

        const Notch* n = model.notchModel()->notchById(id);
        QVERIFY(n);
        QCOMPARE(n->widthHz, 400.0);
    }

    void rowActiveCheckbox_commitsToModel()
    {
        RadioModel model;
        model.notchModel()->addNotch(14200000.0);
        const int id = model.notchModel()->notches().first().id;

        MnfSetupPage page(&model);
        page.show();
        auto* table = page.findChild<QTableWidget*>(QStringLiteral("tblMNFNotches"));
        QVERIFY(table);
        auto* active = qobject_cast<QCheckBox*>(table->cellWidget(0, 2));
        QVERIFY(active);
        QCOMPARE(active->isChecked(), true);

        active->setChecked(false);

        const Notch* n = model.notchModel()->notchById(id);
        QVERIFY(n);
        QCOMPARE(n->active, false);
    }

    void rowDeleteButton_removesNotch()
    {
        RadioModel model;
        model.notchModel()->addNotch(14200000.0);

        MnfSetupPage page(&model);
        page.show();
        auto* table = page.findChild<QTableWidget*>(QStringLiteral("tblMNFNotches"));
        QVERIFY(table);
        auto* del = qobject_cast<QPushButton*>(table->cellWidget(0, 3));
        QVERIFY(del);

        del->click();

        QCOMPARE(static_cast<int>(model.notchModel()->notches().size()), 0);
        QTRY_COMPARE(table->rowCount(), 0);
    }

    void rowValueChangedFromElsewhere_refreshesEditors()
    {
        RadioModel model;
        model.notchModel()->addNotch(14200000.0, 200.0);
        const int id = model.notchModel()->notches().first().id;

        MnfSetupPage page(&model);
        page.show();
        auto* table = page.findChild<QTableWidget*>(QStringLiteral("tblMNFNotches"));
        QVERIFY(table);
        auto* width = qobject_cast<QDoubleSpinBox*>(table->cellWidget(0, 1));
        QVERIFY(width);

        // Panadapter-side wheel resize while the page is idle.
        QVERIFY(model.notchModel()->setWidth(id, 350.0));
        QCOMPARE(width->value(), 350.0);
    }

    void rowEdit_holdsAdminBusy()
    {
        RadioModel model;
        model.notchModel()->addNotch(14200000.0);

        MnfSetupPage page(&model);
        page.show();
        auto* table = page.findChild<QTableWidget*>(QStringLiteral("tblMNFNotches"));
        QVERIFY(table);
        auto* freq = qobject_cast<QDoubleSpinBox*>(table->cellWidget(0, 0));
        QVERIFY(freq);
        QVERIFY(!model.notchModel()->adminBusy());

        freq->setValue(14200500.0);
        QVERIFY(model.notchModel()->adminBusy());
    }

    void rowCommit_clearsAdminBusyBeforeWriting()
    {
        RadioModel model;
        model.notchModel()->addNotch(14200000.0);
        const int id = model.notchModel()->notches().first().id;

        MnfSetupPage page(&model);
        page.show();
        auto* table = page.findChild<QTableWidget*>(QStringLiteral("tblMNFNotches"));
        QVERIFY(table);
        auto* freq = qobject_cast<QDoubleSpinBox*>(table->cellWidget(0, 0));
        QVERIFY(freq);

        freq->setValue(14200500.0);
        QVERIFY(model.notchModel()->adminBusy());
        QTest::keyClick(freq, Qt::Key_Return);

        QVERIFY(!model.notchModel()->adminBusy());
        // Thetis's ENTER clears the flag first and only then writes:
        // btnMNFEnter_Click (setup.cs:17738 [v2.10.3.15]) sets AddActive
        // false at :17744 before RXANBPAddNotch at :17749, and EditActive
        // false at :17759 before RXANBPEditNotch at :17766. The write must
        // land, which it cannot if the lock is still up.
        QCOMPARE(model.notchModel()->notchById(id)->centerHz, 14200500.0);
    }

    void adminBusy_blocksThePanadapterPathDuringAnEdit()
    {
        RadioModel model;
        model.notchModel()->addNotch(14200000.0);
        const int id = model.notchModel()->notches().first().id;

        MnfSetupPage page(&model);
        page.show();
        auto* table = page.findChild<QTableWidget*>(QStringLiteral("tblMNFNotches"));
        QVERIFY(table);
        auto* freq = qobject_cast<QDoubleSpinBox*>(table->cellWidget(0, 0));
        QVERIFY(freq);

        freq->setValue(14200500.0);   // edit opens

        // From Thetis console.cs:40079 [v2.10.3.15]:
        //   "if (SetupForm.NotchAdminBusy) return false;"
        QCOMPARE(model.notchModel()->setCenter(id, 21000000.0), false);
        QCOMPARE(model.notchModel()->notchById(id)->centerHz, 14200000.0);
    }

    void rebuildingTheTable_doesNotOpenAnEdit()
    {
        RadioModel model;
        model.notchModel()->addNotch(14200000.0);

        MnfSetupPage page(&model);
        page.show();
        auto* table = page.findChild<QTableWidget*>(QStringLiteral("tblMNFNotches"));
        QVERIFY(table);

        // Seeding a second row re-runs rebuildTable(); the setValue() calls it
        // makes must not read as operator edits.
        model.notchModel()->addNotch(7100000.0);
        QTRY_COMPARE(table->rowCount(), 2);
        QVERIFY(!model.notchModel()->adminBusy());
    }

    // -- E. Minimum notch width -------------------------------------------

    void minWidthLabel_showsPlaceholderWithoutAChannel()
    {
        RadioModel model;
        MnfSetupPage page(&model);
        page.show();

        auto* lbl = page.findChild<QLabel*>(QStringLiteral("lblMNFMinWidth"));
        QVERIFY(lbl);
        QCOMPARE(lbl->text(), QStringLiteral("--"));
    }

    void minWidthLabel_readsTheLiveChannel()
    {
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        QVERIFY(engine);
        engine->m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)

        RxChannel* ch = engine->createRxChannel(WdspEngine::kFirstSliceChannelId,
                                                /*inputBufferSize*/ 238,
                                                /*dspBufferSize*/ 4096,
                                                /*inputSampleRate*/ 48000,
                                                /*dspSampleRate*/ 48000,
                                                /*outputSampleRate*/ 48000);
        QVERIFY(ch);

        MnfSetupPage page(&model);
        page.show();

        auto* lbl = page.findChild<QLabel*>(QStringLiteral("lblMNFMinWidth"));
        QVERIFY(lbl);
        QCOMPARE(lbl->text(),
                 QStringLiteral("%1 Hz").arg(ch->minNotchWidthHz(), 0, 'f', 1));
        QVERIFY(lbl->text() != QStringLiteral("--"));
    }

    void minWidthLabel_refreshesOnShow()
    {
        RadioModel model;
        MnfSetupPage page(&model);
        page.show();

        auto* lbl = page.findChild<QLabel*>(QStringLiteral("lblMNFMinWidth"));
        QVERIFY(lbl);

        // SetupDialog navigates back to a page that was built long before;
        // the readout re-reads on every visit rather than trusting the
        // value it cached at construction.
        lbl->setText(QStringLiteral("stale"));
        page.hide();
        page.show();
        QCOMPARE(lbl->text(), QStringLiteral("--"));
    }

    // Bench row 7: "Min-width readout changes when nc changes". WDSP's
    // min_notch_width is 1600.0 / (nc / 256) * (rate / 48000) for wintype 0
    // (third_party/wdsp/src/nbp.c:82-96), so halving the coefficient count
    // doubles it. Nothing in the tree signalled that before this task, which
    // is why RxChannel gained minNotchWidthChanged.
    void minWidthLabel_followsMinNotchWidthChanged()
    {
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        QVERIFY(engine);
        engine->m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)

        RxChannel* ch = engine->createRxChannel(WdspEngine::kFirstSliceChannelId,
                                                238, 4096, 48000, 48000, 48000);
        QVERIFY(ch);

        MnfSetupPage page(&model);
        page.show();

        auto* lbl = page.findChild<QLabel*>(QStringLiteral("lblMNFMinWidth"));
        QVERIFY(lbl);
        const QString before = lbl->text();

        QSignalSpy spy(ch, &RxChannel::minNotchWidthChanged);
        ch->setFilterSizeSamples(2048);
        QVERIFY(spy.count() >= 1);

        const double after = ch->minNotchWidthHz();
        QVERIFY(after != 0.0);
        QCOMPARE(lbl->text(), QStringLiteral("%1 Hz").arg(after, 0, 'f', 1));
        QVERIFY(lbl->text() != before);
    }
};

QTEST_MAIN(TestMnfSetupPage)
#include "tst_mnf_setup_page.moc"
