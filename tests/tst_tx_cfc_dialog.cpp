// no-port-check: NereusSDR-original test file.  All Thetis source cites
// for the underlying TransmitModel CFC properties live in TransmitModel.h
// and the dialog source itself.
// =================================================================
// tests/tst_tx_cfc_dialog.cpp  (NereusSDR)
// =================================================================
//
// Phase 3M-3a-ii follow-up sub-PR Batch 8 — TxCfcDialog
// Thetis-verbatim rewrite tests.
//
// TxCfcDialog is the modeless CFC editor launched from the TxApplet's
// [CFC] right-click and from CfcSetupPage's [Configure CFC bands…]
// button.  It now embeds two ParametricEqWidget instances (compression
// curve + post-EQ curve) cross-synced per Thetis frmCFCConfig.cs:218-306
// [v2.10.3.13], with a 50ms QTimer feeding the comp widget's bar chart
// from TxChannel::getCfcDisplayCompression (Task 7 wrapper).
//
// Tests:
//   1. Dialog constructs with two ParametricEqWidget instances + the
//      documented control surface (top + middle edit rows, right column
//      band-count radios / freq spinboxes / checkboxes / reset buttons /
//      OG CFC Guide link).
//   2. Initial values match TransmitModel CFC defaults seeded into the
//      widgets (cfcEqFreq[0]=0 / [9]=10000, cfcCompression[i]=5,
//      cfcPostEqBandGain[i]=0).
//   3. Bar chart timer starts on show, stops on hide.
//   4. closeEvent hides the dialog instead of destroying it.
//   5. Cross-sync: setSelectedIndex on comp widget mirrors to post-EQ
//      widget (and vice versa).
//   6. Band-count radio switching changes both widgets' point counts.
//   7. Freq-range spinbox change clamps both widgets' frequency
//      min/max envelopes.
//   8. Spinbox-driven precomp / post-EQ gain push through to TransmitModel.
//   9. Per-band Comp / Gain spinbox writes update the selected widget point
//      and the TransmitModel array.
//  10. External TM setter updates dialog spinboxes (model → UI sync).
//  11. OG CFC Guide button exists and triggers (we don't open the URL
//      under test; just verify the button is wired).
//  12. Use Q Factors checkbox toggles ParametricEq on both widgets.
//
// =================================================================

#include <QtTest/QtTest>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QCloseEvent>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTimer>
#include <QWheelEvent>
#include <QSlider>
#include <QLineEdit>
#include <QLabel>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QScrollArea>
#include <QScrollBar>
#ifdef HAVE_WDSP
extern "C" {
void OpenChannel(int, int, int, int, int, int, int, int, double, double, double, double, int);
void CloseChannel(int);
}
#endif
#include "core/CfcProfile.h"
#include "core/CfcEditProfile.h"
#include "core/TxChannel.h"

#include "core/AppSettings.h"
#include "gui/applets/TxCfcDialog.h"
#include "gui/widgets/ParametricEqWidget.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

static CfcEditProfile pairedProfile(int count = 18)
{
    CfcEditProfile p;
    p.compression.frequencyMaxHz = p.postEq.frequencyMaxHz = 10000;
    p.compression.globalGainDb = 3.25;
    p.postEq.globalGainDb = -2.75;
    p.compression.useQ = p.postEq.useQ = true;
    for (int i = 0; i < count; ++i) {
        p.compression.frequenciesHz.append(i * 10000.0 / (count - 1));
        p.compression.gainsDb.append(5 + i * 0.1);
        p.compression.q.append(1.234567 + i * 0.1);
        p.postEq.gainsDb.append(-4 + i * 0.2);
        p.postEq.q.append(2.345678 + i * 0.1);
    }
    p.postEq.frequenciesHz = p.compression.frequenciesHz;
    return p;
}

static QPoint pointPosition(ParametricEqWidget* w, int index)
{
    const auto& p = w->points().at(index);
    const QRectF plot = w->plotRect();
    return QPoint(qRound(plot.left() + (p.frequencyHz - w->frequencyMinHz()) /
                         (w->frequencyMaxHz() - w->frequencyMinHz()) * (plot.width() - 1)),
                  qRound(plot.top() + (w->dbMax() - p.gainDb) /
                         (w->dbMax() - w->dbMin()) * (plot.height() - 1)));
}


static QPoint widthHandlePosition(ParametricEqWidget* w, int index)
{
    const auto point = w->points()[index];
    const QRectF plot = w->plotRect();
    const double half = (w->frequencyMaxHz() - w->frequencyMinHz()) / (6 * point.q);
    const int centerY = pointPosition(w, index).y();
    const int bottom = qRound(plot.top() + plot.height() - 1);
    const int primaryY = qBound(qRound(plot.top()) + 6,
                               centerY + (centerY > bottom - 28 ? -22 : 22), bottom - 6);
    return QPoint(qRound(plot.left() + (point.frequencyHz + half - w->frequencyMinHz()) /
                         (w->frequencyMaxHz() - w->frequencyMinHz()) * (plot.width() - 1)), primaryY);
}

class TestTxCfcDialog : public QObject {
    Q_OBJECT

private slots:

    void initTestCase()
    {
        if (!qApp) {
            static int argc = 0;
            new QApplication(argc, nullptr);
        }
        AppSettings::instance().clear();
#ifdef HAVE_WDSP
        OpenChannel(1, 64, 1024, 48000, 96000, 48000, 1, 0, 0, 0.010, 0, 0.010, 1);
#endif
    }

    void cleanupTestCase()
    {
#ifdef HAVE_WDSP
        CloseChannel(1);
#endif
    }

    void cleanup()
    {
        AppSettings::instance().clear();
    }


    void focusedNativeWheelEventsHaveIndependentHistory_data()
    {
        QTest::addColumn<QString>("field");
        for (const QString& field : {QString("comp"), QString("gain"), QString("compQ"), QString("eqQ"),
                                    QString("precomp"), QString("postEq"), QString("low"), QString("high")}) {
            QTest::newRow(qPrintable(field)) << field;
        }
    }
    void focusedNativeWheelEventsHaveIndependentHistory()
    {
        QFETCH(QString, field);
        RadioModel rm;
        const CfcEditProfile original = pairedProfile(10);
        QVERIFY(rm.transmitModel().setCfcProfile(original));
        TxCfcDialog dlg(&rm.transmitModel(), nullptr);
        dlg.compWidget()->setSelectedIndex(3);
        QAbstractSpinBox* spin = field == "comp" ? static_cast<QAbstractSpinBox*>(dlg.compSpin())
            : field == "gain" ? static_cast<QAbstractSpinBox*>(dlg.gainSpin())
            : field == "compQ" ? static_cast<QAbstractSpinBox*>(dlg.compQSpin())
            : field == "eqQ" ? static_cast<QAbstractSpinBox*>(dlg.eqQSpin())
            : field == "precomp" ? static_cast<QAbstractSpinBox*>(dlg.precompSpin())
            : field == "postEq" ? static_cast<QAbstractSpinBox*>(dlg.postEqGainSpin())
            : field == "low" ? static_cast<QAbstractSpinBox*>(dlg.lowSpin())
            : static_cast<QAbstractSpinBox*>(dlg.highSpin());
        if (field == "low" || field == "high") { dlg.findChild<QPushButton*>("TxCfcAdvanced")->click(); }
        dlg.show(); dlg.activateWindow(); spin->setFocus(); QApplication::processEvents();
        QVERIFY(spin->hasFocus());
        QSignalSpy publications(&rm.transmitModel(), &TransmitModel::cfcEditProfileChanged);
        const auto wheel = [&] {
            const QPointF pos = spin->rect().center();
            QWheelEvent event(pos, spin->mapToGlobal(pos.toPoint()), {}, QPoint(0, field == "high" ? -120 : 120),
                              Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
            QApplication::sendEvent(spin, &event);
            QApplication::processEvents();
        };
        wheel();
        const CfcEditProfile first = rm.transmitModel().effectiveCfcProfile();
        QVERIFY(first != original); QVERIFY(spin->hasFocus());
        wheel();
        const CfcEditProfile second = rm.transmitModel().effectiveCfcProfile();
        QVERIFY(second != first); QVERIFY(spin->hasFocus());
        QCOMPARE(publications.count(), 2);
        if (field != "low" && field != "high") {
            QCOMPARE(second.compression.frequenciesHz, original.compression.frequenciesHz);
            QCOMPARE(second.postEq.frequenciesHz, original.postEq.frequenciesHz);
        } else {
            QCOMPARE(second.compression.frequenciesHz, second.postEq.frequenciesHz);
        }
        if (field != "compQ") { QCOMPARE(second.compression.q, original.compression.q); }
        if (field != "eqQ") { QCOMPARE(second.postEq.q, original.postEq.q); }
        if (field != "precomp") { QCOMPARE(second.compression.globalGainDb, original.compression.globalGainDb); }
        if (field != "postEq") { QCOMPARE(second.postEq.globalGainDb, original.postEq.globalGainDb); }
        auto* undo = dlg.findChild<QPushButton*>("TxCfcUndo");
        auto* redo = dlg.findChild<QPushButton*>("TxCfcRedo");
        undo->click(); QCOMPARE(rm.transmitModel().effectiveCfcProfile(), first);
        undo->click(); QCOMPARE(rm.transmitModel().effectiveCfcProfile(), original);
        QVERIFY(!undo->isEnabled());
        redo->click(); QCOMPARE(rm.transmitModel().effectiveCfcProfile(), first);
        redo->click(); QCOMPARE(rm.transmitModel().effectiveCfcProfile(), second);
    }


    void pendingNativeWheelFinishCannotOverwriteReload()
    {
        RadioModel rm;
        QVERIFY(rm.transmitModel().setCfcProfile(pairedProfile(10)));
        TxCfcDialog dlg(&rm.transmitModel(), nullptr);
        dlg.compWidget()->setSelectedIndex(3);
        auto* spin = dlg.compSpin();
        dlg.show(); dlg.activateWindow(); spin->setFocus(); QApplication::processEvents();
        const QPointF pos = spin->rect().center();
        QWheelEvent event(pos, spin->mapToGlobal(pos.toPoint()), {}, QPoint(0,120),
                          Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(spin, &event);
        auto authoritative = pairedProfile(5);
        authoritative.compression.globalGainDb = 9.25;
        QVERIFY(rm.transmitModel().setCfcProfile(authoritative));
        QSignalSpy publications(&rm.transmitModel(), &TransmitModel::cfcEditProfileChanged);
        QApplication::processEvents();
        QCOMPARE(rm.transmitModel().effectiveCfcProfile(), authoritative);
        QCOMPARE(publications.count(), 0);
        QVERIFY(!dlg.findChild<QPushButton*>("TxCfcUndo")->isEnabled());
    }

    void noOpWheelRetainsOpaqueBlobAndHistory()
    {
        RadioModel rm;
        const QString opaque = QStringLiteral("future-version-opaque-profile");
        rm.transmitModel().setCfcParaEqData(opaque);
        TxCfcDialog dlg(&rm.transmitModel(), nullptr);
        auto* spin = dlg.highSpin();
        spin->setValue(spin->maximum());
        // The preceding explicit edit may repair the blob; reestablish authority.
        rm.transmitModel().setCfcParaEqData(opaque);
        dlg.findChild<QPushButton*>("TxCfcAdvanced")->click();
        dlg.show(); dlg.activateWindow(); spin->setFocus(); QApplication::processEvents();
        QSignalSpy publications(&rm.transmitModel(), &TransmitModel::cfcEditProfileChanged);
        const QPointF pos = spin->rect().center();
        QWheelEvent event(pos, spin->mapToGlobal(pos.toPoint()), {}, QPoint(0,120),
                          Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(spin, &event); QApplication::processEvents();
        QCOMPARE(publications.count(), 0);
        QCOMPARE(rm.transmitModel().cfcParaEqData(), opaque);
        QVERIFY(!dlg.findChild<QPushButton*>("TxCfcUndo")->isEnabled());
    }

    void focusedNumericTeardownCancelsPendingWheelFinish()
    {
        RadioModel rm;
        QVERIFY(rm.transmitModel().setCfcProfile(pairedProfile(10)));
        {
            TxCfcDialog dlg(&rm.transmitModel(), nullptr);
            dlg.compWidget()->setSelectedIndex(3);
            auto* spin = dlg.compSpin();
            dlg.show(); dlg.activateWindow(); spin->setFocus(); QApplication::processEvents();
            QVERIFY(spin->hasFocus());
            const QPointF pos = spin->rect().center();
            QWheelEvent event(pos, spin->mapToGlobal(pos.toPoint()), {}, QPoint(0,120),
                              Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
            QApplication::sendEvent(spin, &event); // Destroy before the queued finish.
        }
        QSignalSpy publications(&rm.transmitModel(), &TransmitModel::cfcEditProfileChanged);
        QApplication::processEvents();
        QCOMPARE(publications.count(), 0);
    }

    void plotsShareExactBounds()
    {
        RadioModel rm;
        TxCfcDialog dlg(&rm.transmitModel(), nullptr);
        dlg.show();
        QApplication::processEvents();
        const auto a = dlg.compWidget()->plotRect();
        const auto b = dlg.postEqWidget()->plotRect();
        QCOMPARE(a.left(), b.left());
        QCOMPARE(a.right(), b.right());
        QVERIFY(dlg.height() < 800);
        QVERIFY(dlg.minimumSizeHint().height() <= 480);
        if (qEnvironmentVariableIsSet("NEREUS_CAPTURE_CFC")) {
            const int count = qEnvironmentVariableIntValue("NEREUS_CAPTURE_CFC_COUNT");
            QVERIFY(rm.transmitModel().setCfcProfile(pairedProfile(count > 0 ? count : 5)));
            const auto size = qEnvironmentVariable("NEREUS_CAPTURE_CFC_SIZE").split('x');
            if (size.size() == 2) { dlg.resize(size[0].toInt(), size[1].toInt()); }
            dlg.compWidget()->setSelectedIndex(2);
            QApplication::processEvents();
            QVERIFY(dlg.grab().save(qEnvironmentVariable("NEREUS_CAPTURE_CFC")));
        }
    }

    void sharedSelectionSurvivesCrossing18Bands()
    {
        RadioModel rm;
        const CfcEditProfile original = pairedProfile();
        QVERIFY(rm.transmitModel().setCfcProfile(original));
        TxCfcDialog dlg(&rm.transmitModel(), nullptr);
        QCOMPARE(dlg.compWidget()->bandCount(), 18);
        dlg.compWidget()->setSelectedIndex(3);
        const int id = dlg.compWidget()->points().at(3).bandId;
        dlg.freqSpin()->setValue(6200);
        const int moved = dlg.compWidget()->getIndexFromBandId(id);
        QVERIFY(moved > 3);
        QCOMPARE(dlg.compWidget()->selectedIndex(), moved);
        QCOMPARE(dlg.postEqWidget()->selectedIndex(), moved);
        QCOMPARE(dlg.postEqWidget()->points().at(moved).bandId, id);
        QCOMPARE(dlg.postEqWidget()->points().at(moved).gainDb, original.postEq.gainsDb[3]);
        QCOMPARE(dlg.postEqWidget()->points().at(moved).q, original.postEq.q[3]);
        QCOMPARE(dlg.selectedBandSpin()->value(), id);
        auto* chip = dlg.findChild<QPushButton*>(QString("TxCfcBand%1").arg(id));
        QVERIFY(chip && chip->isChecked());
        QVERIFY(chip->text().startsWith(QString::number(id) + "\n"));
        dlg.compWidget()->setSelectedIndex(1);
        chip->click();
        QCOMPARE(dlg.compWidget()->points()[dlg.compWidget()->selectedIndex()].bandId, id);
        QCOMPARE(dlg.postEqWidget()->points()[dlg.postEqWidget()->selectedIndex()].bandId, id);
        QCOMPARE(rm.transmitModel().effectiveCfcProfile().compression.frequenciesHz,
                 rm.transmitModel().effectiveCfcProfile().postEq.frequenciesHz);
    }

    void numericAndDragPathsCommitSameProfile()
    {
        for (int count : {5, 10, 18}) {
            for (bool live : {false, true}) {
                RadioModel numericRm, dragRm;
                const auto p = pairedProfile(count);
                QVERIFY(numericRm.transmitModel().setCfcProfile(p));
                QVERIFY(dragRm.transmitModel().setCfcProfile(p));
                TxCfcDialog numeric(&numericRm.transmitModel(), nullptr);
                TxCfcDialog drag(&dragRm.transmitModel(), nullptr);
                numeric.compWidget()->setSelectedIndex(2);
                numeric.compSpin()->setValue(8.5);
                drag.liveUpdateChk()->setChecked(live);
                drag.compWidget()->setSelectedIndex(2);
                // Widget's point edit path emits the same paired graph signals as a drag.
                const auto band = drag.compWidget()->points().at(2);
                drag.compWidget()->editStarted();
                drag.compWidget()->setPointData(2, band.frequencyHz, 8.5, band.q);
                drag.compWidget()->editFinished();
                QCOMPARE(numericRm.transmitModel().effectiveCfcProfile(),
                         dragRm.transmitModel().effectiveCfcProfile());
            }
        }
    }

    void independentWidthsReachAudio()
    {
        for (int count : {5, 10, 18}) {
            RadioModel rm;
            TxChannel channel(1, 64, 64);
            rm.bindCfcProfileChannelForTest(&channel);
            QVERIFY(rm.transmitModel().setCfcProfile(pairedProfile(count)));
            TxCfcDialog dlg(&rm.transmitModel(), nullptr);
            dlg.compWidget()->setSelectedIndex(2);
            dlg.compQSpin()->setValue(0.75);
            dlg.eqQSpin()->setValue(8.25);
            const auto p = rm.transmitModel().effectiveCfcProfile();
            QCOMPARE(p.compression.q[2], 0.75);
            QCOMPARE(p.postEq.q[2], 8.25);
            QVERIFY(p.compression.useQ && p.postEq.useQ);
#ifdef HAVE_WDSP
            QCOMPARE(channel.lastCfcProfileForTest()[0].size(), std::size_t(count));
            QCOMPARE(channel.lastCfcProfileForTest()[3][2], 0.75);
            QCOMPARE(channel.lastCfcProfileForTest()[4][2], 8.25);
#endif
            dlg.useQFactorsChk()->setChecked(false);
            const auto off = rm.transmitModel().effectiveCfcProfile();
            QVERIFY(!off.compression.useQ && !off.postEq.useQ);
            QVERIFY(!dlg.compQSpin()->isEnabled());
            QVERIFY(!dlg.eqQSpin()->isEnabled());
            QCOMPARE(off.compression.q[2], 0.75);
#ifdef HAVE_WDSP
            QVERIFY(channel.lastCfcProfileForTest()[3].empty());
            QVERIFY(channel.lastCfcProfileForTest()[4].empty());
#endif
            dlg.useQFactorsChk()->setChecked(true);
            QVERIFY(rm.transmitModel().effectiveCfcProfile().compression.useQ);
        }
    }

    void undoRestoresBothGraphsAndGlobals()
    {
        RadioModel rm;
        const auto original = pairedProfile();
        QVERIFY(rm.transmitModel().setCfcProfile(original));
        TxCfcDialog dlg(&rm.transmitModel(), nullptr);
        auto* undo = dlg.findChild<QPushButton*>("TxCfcUndo");
        auto* redo = dlg.findChild<QPushButton*>("TxCfcRedo");
        QVERIFY(undo && redo);
        dlg.compWidget()->setSelectedIndex(3);
        dlg.freqSpin()->setValue(6200);
        dlg.precompSpin()->setValue(6.5);
        dlg.postEqGainSpin()->setValue(-5.5);
        const auto edited = rm.transmitModel().effectiveCfcProfile();
        undo->click(); undo->click(); undo->click();
        QCOMPARE(rm.transmitModel().effectiveCfcProfile(), original);
        QCOMPARE(dlg.compWidget()->selectedIndex(), 3);
        QCOMPARE(dlg.postEqWidget()->selectedIndex(), 3);
        redo->click(); redo->click(); redo->click();
        QCOMPARE(rm.transmitModel().effectiveCfcProfile(), edited);
        dlg.hide(); dlg.show();
        QVERIFY(undo->isEnabled());
    }

    void amountAndWidthEditsPreserveCloseLoadedFrequencies()
    {
        RadioModel rm;
        auto original = pairedProfile(5);
        original.compression.frequenciesHz = {0, 100.111111, 100.222222, 5000, 10000};
        original.postEq.frequenciesHz = original.compression.frequenciesHz;
        QVERIFY(rm.transmitModel().setCfcProfile(original));
        TxCfcDialog dlg(&rm.transmitModel(), nullptr);
        dlg.compWidget()->setSelectedIndex(1);
        dlg.compSpin()->setValue(8.5);
        QCOMPARE(rm.transmitModel().effectiveCfcProfile().compression.frequenciesHz,
                 original.compression.frequenciesHz);
        dlg.compQSpin()->setValue(5.5);
        dlg.gainSpin()->setValue(-8.5);
        dlg.eqQSpin()->setValue(6.5);
        const auto edited = rm.transmitModel().effectiveCfcProfile();
        QCOMPARE(edited.compression.frequenciesHz, original.compression.frequenciesHz);
        QCOMPARE(edited.postEq.frequenciesHz, original.postEq.frequenciesHz);
        QCOMPARE(edited.compression.q[1], 5.5);
        QCOMPARE(edited.postEq.q[1], 6.5);
    }

    void resetsPreserveOtherGraphAndFrequencies()
    {
        RadioModel rm;
        const auto original = pairedProfile();
        QVERIFY(rm.transmitModel().setCfcProfile(original));
        TxCfcDialog dlg(&rm.transmitModel(), nullptr);
        dlg.compWidget()->setSelectedIndex(4);
        dlg.resetCompBtn()->click();
        const auto reset = rm.transmitModel().effectiveCfcProfile();
        QCOMPARE(reset.postEq, original.postEq);
        QCOMPARE(reset.compression.frequenciesHz, original.compression.frequenciesHz);
        QCOMPARE(reset.compression.globalGainDb, 0.0);
        for (int i = 0; i < 18; ++i) {
            QCOMPARE(reset.compression.gainsDb[i], 0.0);
            QCOMPARE(reset.compression.q[i], 4.0);
        }
        QCOMPARE(dlg.compWidget()->selectedIndex(), 4);
        dlg.resetEqBtn()->click();
        const auto both = rm.transmitModel().effectiveCfcProfile();
        QCOMPARE(both.compression, reset.compression);
        QCOMPARE(both.postEq.frequenciesHz, original.postEq.frequenciesHz);
        QCOMPARE(both.postEq.globalGainDb, 0.0);
    }

    void rangeAndCountAreSingleTransactions()
    {
        RadioModel rm;
        const auto original = pairedProfile(10);
        QVERIFY(rm.transmitModel().setCfcProfile(original));
        TxCfcDialog dlg(&rm.transmitModel(), nullptr);
        QSignalSpy spy(&rm.transmitModel(), &TransmitModel::cfcEditProfileChanged);
        dlg.highSpin()->setValue(8000);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(rm.transmitModel().effectiveCfcProfile().postEq.frequenciesHz,
                 rm.transmitModel().effectiveCfcProfile().compression.frequenciesHz);
        dlg.bands5Radio()->setChecked(true);
        QCOMPARE(dlg.compWidget()->bandCount(), 10);
        auto* apply = dlg.findChild<QPushButton*>("TxCfcApplyBands");
        auto* cancel = dlg.findChild<QPushButton*>("TxCfcCancelBands");
        auto* undo = dlg.findChild<QPushButton*>("TxCfcUndo");
        QVERIFY(apply && cancel && undo);
        cancel->click();
        QCOMPARE(dlg.currentBandCount(), 10);
        dlg.bands5Radio()->setChecked(true);
        apply->click();
        QCOMPARE(spy.count(), 2);
        QCOMPARE(dlg.compWidget()->bandCount(), 5);
        undo->click();
        QCOMPARE(dlg.compWidget()->bandCount(), 10);
        undo->click();
        QCOMPARE(rm.transmitModel().effectiveCfcProfile(), original);
    }

    void externalProfileInterruptsGesture()
    {
        RadioModel rm;
        QVERIFY(rm.transmitModel().setCfcProfile(pairedProfile(10)));
        TxCfcDialog dlg(&rm.transmitModel(), nullptr);
        dlg.show(); QApplication::processEvents();
        dlg.liveUpdateChk()->setChecked(false);
        auto* w = dlg.compWidget();
        const QPoint start = pointPosition(w, 3);
        QTest::mousePress(w, Qt::LeftButton, Qt::NoModifier, start);
        QTest::mouseMove(w, start + QPoint(20, -12));
        const auto replacement = pairedProfile(5);
        QVERIFY(rm.transmitModel().setCfcProfile(replacement));
        QTest::mouseRelease(w, Qt::LeftButton, Qt::NoModifier, start + QPoint(30, -15));
        QCOMPARE(rm.transmitModel().effectiveCfcProfile(), replacement);
        auto* undo = dlg.findChild<QPushButton*>("TxCfcUndo");
        QVERIFY(undo);
        QVERIFY(!undo->isEnabled());
    }

    void barSamplesDoNotCreateUndo()
    {
        RadioModel rm;
        TxCfcDialog dlg(&rm.transmitModel(), nullptr);
        auto* undo = dlg.findChild<QPushButton*>("TxCfcUndo");
        QVERIFY(undo);
        const QString before = rm.transmitModel().cfcParaEqData();
        dlg.compWidget()->drawBarChartData(QVector<double>{1, 5, 2, 7});
        dlg.compWidget()->setSelectedIndex(4);
        QVERIFY(!undo->isEnabled());
        QCOMPARE(rm.transmitModel().cfcParaEqData(), before);
    }

    void realGraphGesturesAreOneTransaction_data()
    {
        QTest::addColumn<int>("count");
        QTest::addColumn<bool>("live");
        QTest::addColumn<QString>("kind");
        for (int count : {5, 10, 18}) {
            for (bool live : {false, true}) {
                for (const QString& kind : {QString("point"), QString("width"), QString("global")}) {
                    QTest::newRow(qPrintable(QString("%1-%2-%3").arg(count).arg(live).arg(kind))) << count << live << kind;
                }
            }
        }
    }

    void realGraphGesturesAreOneTransaction()
    {
        QFETCH(int, count); QFETCH(bool, live); QFETCH(QString, kind);
        RadioModel rm;
        const auto original = pairedProfile(count);
        QVERIFY(rm.transmitModel().setCfcProfile(original));
        TxCfcDialog dlg(&rm.transmitModel(), nullptr);
        dlg.show(); QVERIFY(QTest::qWaitForWindowExposed(&dlg));
        dlg.liveUpdateChk()->setChecked(live);
        auto* w = dlg.compWidget();
        const int index = 2;
        w->setSelectedIndex(kind == "global" ? -1 : index);
        const QRectF plot = w->plotRect();
        QPoint start = pointPosition(w, index);
        QPoint delta(20, -15);
        if (kind == "width") {
            start = widthHandlePosition(w, index);
            delta = QPoint(20, 0);
        } else if (kind == "global") {
            start = QPoint(qRound(plot.right()) + 6,
                           qRound(plot.top() + (16 - original.compression.globalGainDb) / 16 * (plot.height() - 1)));
            delta = QPoint(0, -15);
        }
        QSignalSpy spy(&rm.transmitModel(), &TransmitModel::cfcEditProfileChanged);
        QTest::mousePress(w, Qt::LeftButton, Qt::NoModifier, start);
        QTest::mouseMove(w, start + delta);
        if (live) { QVERIFY(spy.count() >= 1); }
        else { QCOMPARE(spy.count(), 0); QCOMPARE(rm.transmitModel().effectiveCfcProfile(), original); }
        QTest::mouseRelease(w, Qt::LeftButton, Qt::NoModifier, start + delta);
        QVERIFY(rm.transmitModel().effectiveCfcProfile() != original);
        if (!live) { QCOMPARE(spy.count(), 1); }
        auto* undo = dlg.findChild<QPushButton*>("TxCfcUndo");
        QVERIFY(undo->isEnabled());
        undo->click();
        QCOMPARE(rm.transmitModel().effectiveCfcProfile(), original);
        QVERIFY(!undo->isEnabled());
    }

    void widthSliderDefersUntilRelease()
    {
        for (bool live : {false, true}) {
            RadioModel rm;
            const auto original = pairedProfile(5);
            QVERIFY(rm.transmitModel().setCfcProfile(original));
            TxCfcDialog dlg(&rm.transmitModel(), nullptr);
            dlg.show(); QVERIFY(QTest::qWaitForWindowExposed(&dlg));
            dlg.compWidget()->setSelectedIndex(2);
            dlg.liveUpdateChk()->setChecked(live);
            auto* slider = dlg.findChild<QSlider*>("TxCfcCompQSlider");
            QVERIFY(slider && slider->isVisible());
            QStyleOptionSlider option;
            option.initFrom(slider);
            option.orientation = Qt::Horizontal;
            option.minimum = slider->minimum();
            option.maximum = slider->maximum();
            option.sliderPosition = slider->sliderPosition();
            option.sliderValue = slider->value();
            const QPoint start = slider->style()->subControlRect(QStyle::CC_Slider, &option,
                                                                 QStyle::SC_SliderHandle, slider).center();
            QSignalSpy spy(&rm.transmitModel(), &TransmitModel::cfcEditProfileChanged);
            QTest::mousePress(slider, Qt::LeftButton, Qt::NoModifier, start);
            QTest::mouseMove(slider, start + QPoint(35, 0));
            if (!live) { QCOMPARE(spy.count(), 0); }
            QTest::mouseRelease(slider, Qt::LeftButton, Qt::NoModifier, start + QPoint(35, 0));
            QApplication::processEvents();
            QVERIFY(rm.transmitModel().effectiveCfcProfile().compression.q[2] != original.compression.q[2]);
            if (!live) { QCOMPARE(spy.count(), 1); }
            auto* undo = dlg.findChild<QPushButton*>("TxCfcUndo");
            undo->click();
            QCOMPARE(rm.transmitModel().effectiveCfcProfile(), original);
            QVERIFY(!undo->isEnabled());
        }
    }

    void unsubmittedNumericTextFinalizesWhileStateIsAlive_data()
    {
        QTest::addColumn<bool>("reopen");
        QTest::newRow("destroy") << false;
        QTest::newRow("hide-reopen") << true;
    }
    void unsubmittedNumericTextFinalizesWhileStateIsAlive()
    {
        QFETCH(bool, reopen);
        RadioModel rm;
        const auto original = pairedProfile(5);
        QVERIFY(rm.transmitModel().setCfcProfile(original));
        {
            TxCfcDialog dlg(&rm.transmitModel(), nullptr);
            dlg.compWidget()->setSelectedIndex(2);
            auto* field = dlg.compSpin();
            dlg.show(); dlg.activateWindow(); field->setFocus(); QApplication::processEvents();
            auto* text = field->findChild<QLineEdit*>();
            text->selectAll(); QTest::keyClicks(text, "8.5");
            QCOMPARE(rm.transmitModel().effectiveCfcProfile(), original);
            if (reopen) {
                dlg.hide();
                QCOMPARE(rm.transmitModel().effectiveCfcProfile().compression.gainsDb[2], 8.5);
                dlg.show();
                auto* undo = dlg.findChild<QPushButton*>("TxCfcUndo");
                QVERIFY(undo->isEnabled()); undo->click();
                QCOMPARE(rm.transmitModel().effectiveCfcProfile(), original);
            }
        }
        QApplication::processEvents();
        if (!reopen) { QCOMPARE(rm.transmitModel().effectiveCfcProfile().compression.gainsDb[2], 8.5); }
        else { QCOMPARE(rm.transmitModel().effectiveCfcProfile(), original); }
    }

    void textAfterWheelKeepsLocalUndoAndOneTextTransaction()
    {
        RadioModel rm;
        const auto original = pairedProfile(5);
        QVERIFY(rm.transmitModel().setCfcProfile(original));
        TxCfcDialog dlg(&rm.transmitModel(), nullptr);
        dlg.compWidget()->setSelectedIndex(2);
        auto* field = dlg.compSpin();
        auto* text = field->findChild<QLineEdit*>();
        dlg.show(); dlg.activateWindow(); field->setFocus(); QApplication::processEvents();
        const QPointF pos = field->rect().center();
        QWheelEvent wheel(pos, field->mapToGlobal(pos.toPoint()), {}, QPoint(0,120),
                          Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(field, &wheel); QApplication::processEvents();
        const auto afterWheel = rm.transmitModel().effectiveCfcProfile();
        QVERIFY(afterWheel != original);
        text->selectAll(); QTest::keyClicks(text, "8.5");
        QCOMPARE(rm.transmitModel().effectiveCfcProfile(), afterWheel);
        QTest::keyClick(text, Qt::Key_Z, Qt::ControlModifier);
        QCOMPARE(rm.transmitModel().effectiveCfcProfile(), afterWheel);
        text->selectAll(); QTest::keyClicks(text, "8.5");
        QTest::keyClick(text, Qt::Key_Return);
        QCOMPARE(rm.transmitModel().effectiveCfcProfile().compression.gainsDb[2], 8.5);
        auto* undo = dlg.findChild<QPushButton*>("TxCfcUndo");
        undo->click(); QCOMPARE(rm.transmitModel().effectiveCfcProfile(), afterWheel);
        undo->click(); QCOMPARE(rm.transmitModel().effectiveCfcProfile(), original);
        QVERIFY(!undo->isEnabled());
    }

    void numericTextEditAndTextUndo()
    {
        RadioModel rm;
        const auto original = pairedProfile(5);
        QVERIFY(rm.transmitModel().setCfcProfile(original));
        TxCfcDialog dlg(&rm.transmitModel(), nullptr);
        dlg.show(); QVERIFY(QTest::qWaitForWindowExposed(&dlg));
        dlg.compWidget()->setSelectedIndex(2);
        auto* field = dlg.compSpin();
        auto* text = field->findChild<QLineEdit*>();
        field->setFocus();
        text->selectAll();
        QTest::keyClicks(text, "8.5");
        QCOMPARE(rm.transmitModel().effectiveCfcProfile(), original);
        QTest::keyClick(text, Qt::Key_Z, Qt::ControlModifier);
        QVERIFY(!dlg.findChild<QPushButton*>("TxCfcUndo")->isEnabled());
        text->selectAll();
        QTest::keyClicks(text, "8.5");
        QTest::keyClick(text, Qt::Key_Return);
        QCOMPARE(rm.transmitModel().effectiveCfcProfile().compression.gainsDb[2], 8.5);
        auto* undo = dlg.findChild<QPushButton*>("TxCfcUndo");
        QVERIFY(undo->isEnabled());
        QTest::keyClick(text, Qt::Key_Z, Qt::ControlModifier);
        QCOMPARE(rm.transmitModel().effectiveCfcProfile(), original);
        QVERIFY(!undo->isEnabled());
    }

    void externalReplacementDuringNumericAndWidthEdit()
    {
        for (bool width : {false, true}) {
            RadioModel rm;
            QVERIFY(rm.transmitModel().setCfcProfile(pairedProfile(10)));
            TxCfcDialog dlg(&rm.transmitModel(), nullptr);
            dlg.show(); QVERIFY(QTest::qWaitForWindowExposed(&dlg));
            dlg.compWidget()->setSelectedIndex(3);
            dlg.liveUpdateChk()->setChecked(false);
            auto* widget = dlg.compWidget();
            QPoint start;
            if (width) {
                start = widthHandlePosition(widget, 3);
                QTest::mousePress(widget, Qt::LeftButton, Qt::NoModifier, start);
                QTest::mouseMove(widget, start + QPoint(20, 0));
            } else {
                dlg.compSpin()->setFocus();
                auto* text = dlg.compSpin()->findChild<QLineEdit*>();
                text->selectAll(); QTest::keyClicks(text, "14.5");
            }
            const auto replacement = pairedProfile(5);
            QVERIFY(rm.transmitModel().setCfcProfile(replacement));
            if (width) { QTest::mouseRelease(widget, Qt::LeftButton, Qt::NoModifier, start + QPoint(20, 0)); }
            else { QTest::keyClick(dlg.compSpin(), Qt::Key_Return); }
            QCOMPARE(rm.transmitModel().effectiveCfcProfile(), replacement);
            QVERIFY(!dlg.findChild<QPushButton*>("TxCfcUndo")->isEnabled());
        }
    }

    void opaqueProfileAndNoOpEditsDoNotWrite()
    {
        RadioModel rm;
        rm.transmitModel().setCfcParaEqData("future-version-profile");
        QSignalSpy spy(&rm.transmitModel(), &TransmitModel::cfcEditProfileChanged);
        TxCfcDialog dlg(&rm.transmitModel(), nullptr);
        dlg.show(); QVERIFY(QTest::qWaitForWindowExposed(&dlg));
        dlg.compWidget()->setSelectedIndex(3);
        dlg.precompSpin()->setFocus();
        dlg.selectedBandSpin()->setFocus();
        const QPoint point = pointPosition(dlg.compWidget(), 3);
        QTest::mouseClick(dlg.compWidget(), Qt::LeftButton, Qt::NoModifier, point);
        dlg.hide(); dlg.show(); QApplication::processEvents();
        QCOMPARE(rm.transmitModel().cfcParaEqData(), QString("future-version-profile"));
        QCOMPARE(spy.count(), 0);
        QVERIFY(!dlg.findChild<QPushButton*>("TxCfcUndo")->isEnabled());
    }

    void asymmetricQFlagsSurviveOrdinaryEditsAndUndo()
    {
        RadioModel rm;
        auto original = pairedProfile(5);
        original.postEq.useQ = false;
        QVERIFY(rm.transmitModel().setCfcProfile(original));
        TxCfcDialog dlg(&rm.transmitModel(), nullptr);
        QVERIFY(!dlg.useQFactorsChk()->isChecked());
        QVERIFY(!dlg.compWidget()->parametricEq());
        QVERIFY(!dlg.postEqWidget()->parametricEq());
        dlg.compWidget()->setSelectedIndex(2);
        dlg.compSpin()->setValue(8.5);
        const auto edited = rm.transmitModel().effectiveCfcProfile();
        QVERIFY(edited.compression.useQ && !edited.postEq.useQ);
        dlg.useQFactorsChk()->setChecked(true);
        QVERIFY(rm.transmitModel().effectiveCfcProfile().postEq.useQ);
        dlg.findChild<QPushButton*>("TxCfcUndo")->click();
        QCOMPARE(rm.transmitModel().effectiveCfcProfile(), edited);
        dlg.findChild<QPushButton*>("TxCfcUndo")->click();
        QCOMPARE(rm.transmitModel().effectiveCfcProfile(), original);
    }

    void nonzeroLegacyEndpointLoadsWithoutWrite()
    {
        RadioModel rm;
        rm.transmitModel().setCfcEqFreq(0, 100);
        rm.transmitModel().setCfcCompression(0, 9);
        rm.transmitModel().setCfcPostEqBandGain(0, -8);
        rm.transmitModel().setCfcParaEqData("unknown-opaque");
        QSignalSpy spy(&rm.transmitModel(), &TransmitModel::cfcEditProfileChanged);
        TxCfcDialog dlg(&rm.transmitModel(), nullptr);
        QCOMPARE(dlg.compWidget()->frequencyMinHz(), 100.0);
        QCOMPARE(dlg.compWidget()->points()[0].frequencyHz, 100.0);
        QCOMPARE(dlg.compWidget()->points()[0].gainDb, 9.0);
        QCOMPARE(dlg.postEqWidget()->points()[0].gainDb, -8.0);
        QCOMPARE(rm.transmitModel().cfcParaEqData(), QString("unknown-opaque"));
        QCOMPARE(spy.count(), 0);
    }

    void collapsedLegacyCurveIsUnavailableUntilExplicitRepair()
    {
        RadioModel rm;
        auto& tm = rm.transmitModel();
        for (int i = 0; i < 10; ++i) { tm.setCfcEqFreq(i, 200); }
        tm.setCfcParaEqData("collapsed-opaque");
        QSignalSpy spy(&tm, &TransmitModel::cfcEditProfileChanged);
        TxCfcDialog dlg(&tm, nullptr);
        dlg.show(); QApplication::processEvents();
        QVERIFY(dlg.findChild<QLabel*>("TxCfcInvalidCurve")->isVisible());
        QVERIFY(!dlg.compWidget()->isVisible());
        QVERIFY(!dlg.precompSpin()->isEnabled());
        QCOMPARE(tm.cfcParaEqData(), QString("collapsed-opaque"));
        QCOMPARE(spy.count(), 0);
        dlg.bands5Radio()->setChecked(true);
        dlg.findChild<QPushButton*>("TxCfcApplyBands")->click();
        QVERIFY(dlg.compWidget()->isVisible());
        QVERIFY(dlg.precompSpin()->isEnabled());
        QVERIFY(isValidCfcEditProfile(tm.effectiveCfcProfile()));
        QCOMPARE(tm.effectiveCfcProfile().compression.frequenciesHz.size(), 5);
        QCOMPARE(spy.count(), 1);
        dlg.findChild<QPushButton*>("TxCfcUndo")->click();
        QCOMPARE(tm.cfcParaEqData(), QString("collapsed-opaque"));
        for (int i = 0; i < 10; ++i) { QCOMPARE(tm.cfcEqFreq(i), 200); }
        QVERIFY(!dlg.compWidget()->isVisible());
        QVERIFY(!dlg.precompSpin()->isEnabled());
        QCOMPARE(spy.count(), 2);
        dlg.findChild<QPushButton*>("TxCfcRedo")->click();
        QVERIFY(dlg.compWidget()->isVisible());
        QVERIFY(isValidCfcEditProfile(tm.effectiveCfcProfile()));
        QCOMPARE(tm.effectiveCfcProfile().compression.frequenciesHz.size(), 5);
        QCOMPARE(spy.count(), 3);
    }

    void authoritativeCollapsedReplacementRebasesRepairMetadata()
    {
        RadioModel rm;
        auto& tm = rm.transmitModel();
        CfcEditProfile original = pairedProfile(18);
        original.compression.frequencyMaxHz = original.postEq.frequencyMaxHz = 12000;
        for (int i = 0; i < 18; ++i) {
            original.compression.frequenciesHz[i] = original.postEq.frequenciesHz[i] = i * 12000.0 / 17;
        }
        QVERIFY(tm.setCfcProfile(original));
        TxCfcDialog dlg(&tm, nullptr);
        dlg.show(); QApplication::processEvents();
        dlg.bands5Radio()->setChecked(true);
        auto* apply = dlg.findChild<QPushButton*>("TxCfcApplyBands");
        QVERIFY(apply->isVisible());
        QSignalSpy spy(&tm, &TransmitModel::cfcEditProfileChanged);
        tm.beginCfcProfileUpdate();
        for (int i = 0; i < 10; ++i) {
            tm.setCfcEqFreq(i, 200);
            tm.setCfcCompression(i, i + 2);
            tm.setCfcPostEqBandGain(i, -i - 1);
        }
        tm.setCfcPrecompDb(7); tm.setCfcPostEqGainDb(-6);
        tm.setCfcParaEqData("collapsed-external-opaque");
        tm.endCfcProfileUpdate();
        const auto checkOriginal = [&] {
            QCOMPARE(tm.cfcParaEqData(), QString("collapsed-external-opaque"));
            QCOMPARE(tm.cfcPrecompDb(), 7); QCOMPARE(tm.cfcPostEqGainDb(), -6);
            for (int i = 0; i < 10; ++i) {
                QCOMPARE(tm.cfcEqFreq(i), 200);
                QCOMPARE(tm.cfcCompression(i), i + 2);
                QCOMPARE(tm.cfcPostEqBandGain(i), -i - 1);
            }
        };
        checkOriginal();
        QCOMPARE(spy.count(), 1);
        QVERIFY(!dlg.compWidget()->isVisible());
        QVERIFY(dlg.findChild<QLabel*>("TxCfcInvalidCurve")->isVisible());
        QCOMPARE(dlg.currentBandCount(), 10);
        QVERIFY(dlg.bands10Radio()->isChecked());
        QVERIFY(!apply->isVisible());
        QVERIFY(!dlg.useQFactorsChk()->isChecked());
        QVERIFY(!dlg.compQSpin()->isEnabled());
        QVERIFY(!dlg.eqQSpin()->isEnabled());
        QVERIFY(!dlg.findChild<QSlider*>("TxCfcCompQSlider")->isEnabled());
        QVERIFY(!dlg.findChild<QSlider*>("TxCfcEqQSlider")->isEnabled());
        const auto* lastBand = dlg.findChild<QPushButton*>("TxCfcBand10");
        QVERIFY(lastBand); QVERIFY(!lastBand->isEnabled());
        QCOMPARE(lastBand->text(), QString("10\n200 Hz"));
        QVERIFY(!dlg.findChild<QPushButton*>("TxCfcBand18"));
        QCOMPARE(dlg.lowSpin()->value(), 0);
        QCOMPARE(dlg.highSpin()->value(), 4000);
        dlg.bands18Radio()->setChecked(true);
        QVERIFY(apply->isVisible());
        apply->click();
        const CfcEditProfile repaired = tm.effectiveCfcProfile();
        QVERIFY(isValidCfcEditProfile(repaired));
        QCOMPARE(repaired.compression.frequenciesHz.size(), 18);
        QCOMPARE(repaired.compression.frequencyMinHz, 0.0);
        QCOMPARE(repaired.compression.frequencyMaxHz, 4000.0);
        QVERIFY(!repaired.compression.useQ); QVERIFY(!repaired.postEq.useQ);
        QCOMPARE(spy.count(), 2);
        dlg.findChild<QPushButton*>("TxCfcUndo")->click();
        checkOriginal();
        QCOMPARE(dlg.currentBandCount(), 10);
        QVERIFY(dlg.bands10Radio()->isChecked());
        QVERIFY(!dlg.compWidget()->isVisible());
        QVERIFY(!apply->isVisible());
        QCOMPARE(spy.count(), 3);
        dlg.findChild<QPushButton*>("TxCfcRedo")->click();
        QCOMPARE(tm.effectiveCfcProfile(), repaired);
        QCOMPARE(dlg.currentBandCount(), 18);
        QVERIFY(dlg.compWidget()->isVisible());
        QCOMPARE(spy.count(), 4);
    }

    void eighteenBandSelectorsFitAndScrollAtLaptopSize()
    {
        RadioModel rm;
        QVERIFY(rm.transmitModel().setCfcProfile(pairedProfile(18)));
        TxCfcDialog dlg(&rm.transmitModel(), nullptr);
        dlg.resize(853, 500); dlg.show(); QApplication::processEvents();
        // Authoritative replacement rebuilds the strip while its parent is visible.
        QVERIFY(rm.transmitModel().setCfcProfile(pairedProfile(5)));
        QVERIFY(rm.transmitModel().setCfcProfile(pairedProfile(18)));
        QApplication::processEvents();
        const auto* first = dlg.findChild<QPushButton*>("TxCfcBand1");
        QVERIFY(first);
        auto* host = first->parentWidget();
        auto* strip = qobject_cast<QScrollArea*>(host->parentWidget()->parentWidget());
        QVERIFY(strip);
        for (int id = 1; id <= 18; ++id) {
            const auto* button = dlg.findChild<QPushButton*>(QStringLiteral("TxCfcBand%1").arg(id));
            QVERIFY(button);
            QVERIFY(button->isVisible());
            QVERIFY(host->rect().contains(button->geometry()));
            const auto lines = button->text().split('\n');
            for (const auto& line : lines) {
                QVERIFY2(button->width() >= button->fontMetrics().horizontalAdvance(line) + 22,
                         qPrintable(QStringLiteral("Clipped band %1: %2 in %3px").arg(id).arg(line).arg(button->width())));
            }
            if (id > 1) {
                const auto* previous = dlg.findChild<QPushButton*>(QStringLiteral("TxCfcBand%1").arg(id - 1));
                QVERIFY(button->geometry().left() > previous->geometry().right());
            }
        }
        QVERIFY(host->width() > strip->viewport()->width());
        QVERIFY(strip->viewport()->height() >= host->height());
        QVERIFY(strip->horizontalScrollBar()->maximum() > 0);
        QVERIFY(dlg.compWidget()->width() <= dlg.width());
        QVERIFY(dlg.postEqWidget()->width() <= dlg.width());
        QCOMPARE(dlg.size(), QSize(853, 500));
    }

    void pairedCurveRefreshesAndEditsFiveAndEighteen()
    {
        RadioModel rm;
        TransmitModel& tx = rm.transmitModel();
        TxCfcDialog dlg(&tx, nullptr);
        dlg.bands18Radio()->setChecked(true);
        dlg.findChild<QPushButton*>(QStringLiteral("TxCfcApplyBands"))->click();
        CfcProfile::Profile p;
        QVERIFY(CfcProfile::decode(tx.cfcParaEqData(), p));
        QCOMPARE(p.f.size(), std::size_t(18));
        dlg.compWidget()->setSelectedIndex(3);
        dlg.compQSpin()->setValue(7.25);
        QVERIFY(CfcProfile::decode(tx.cfcParaEqData(), p));
        QCOMPARE(p.qg.at(3), 7.25);
        QCOMPARE(p.qe.at(3), 4.0);

        dlg.bands5Radio()->setChecked(true);
        dlg.findChild<QPushButton*>(QStringLiteral("TxCfcApplyBands"))->click();
        QVERIFY(CfcProfile::decode(tx.cfcParaEqData(), p));
        QCOMPARE(p.f.size(), std::size_t(5));
        tx.setCfcParaEqData(CfcProfile::encode(p));
        QCOMPARE(dlg.currentBandCount(), 5);
    }

    // ── 1. Dialog constructs with documented control surface ───────────
    void constructsWithExpectedControls()
    {
        RadioModel rm;
        TxCfcDialog dlg(&rm.transmitModel(), nullptr);

        // Two ParametricEqWidget instances embedded.
        const auto widgets = dlg.findChildren<ParametricEqWidget*>();
        QCOMPARE(widgets.size(), 2);
        QVERIFY(dlg.compWidget());
        QVERIFY(dlg.postEqWidget());
        QVERIFY(dlg.compWidget()   != dlg.postEqWidget());

        // Top edit row.
        QVERIFY(dlg.selectedBandSpin());
        QVERIFY(dlg.freqSpin());
        QVERIFY(dlg.precompSpin());
        QVERIFY(dlg.compSpin());
        QVERIFY(dlg.compQSpin());

        // Middle edit row.
        QVERIFY(dlg.postEqGainSpin());
        QVERIFY(dlg.gainSpin());
        QVERIFY(dlg.eqQSpin());

        // Right column.
        QVERIFY(dlg.bands5Radio());
        QVERIFY(dlg.bands10Radio());
        QVERIFY(dlg.bands18Radio());
        QVERIFY(dlg.lowSpin());
        QVERIFY(dlg.highSpin());
        QVERIFY(dlg.useQFactorsChk());
        QVERIFY(dlg.liveUpdateChk());
        QVERIFY(dlg.logScaleChk());
        QVERIFY(dlg.resetCompBtn());
        QVERIFY(dlg.resetEqBtn());
        QVERIFY(dlg.ogGuideLink());

        // Default radio selection: 10-band.
        QVERIFY(dlg.bands10Radio()->isChecked());
        QVERIFY(!dlg.bands5Radio()->isChecked());
        QVERIFY(!dlg.bands18Radio()->isChecked());
        QCOMPARE(dlg.currentBandCount(), 10);

        // Use Q Factors default = checked (Designer.cs:487 [v2.10.3.13]).
        QVERIFY(!dlg.useQFactorsChk()->isChecked());

        // Modeless.
        QVERIFY(!dlg.isModal());
    }

    // ── 2. Initial values match TransmitModel CFC defaults ─────────────
    void initialValues_matchTmDefaults()
    {
        RadioModel rm;
        TransmitModel& tx = rm.transmitModel();
        TxCfcDialog dlg(&tx, nullptr);

        // Globals (top + middle edit rows).
        QCOMPARE(dlg.precompSpin()->value(),    static_cast<double>(tx.cfcPrecompDb()));
        QCOMPARE(dlg.postEqGainSpin()->value(), static_cast<double>(tx.cfcPostEqGainDb()));
        QCOMPARE(tx.cfcPrecompDb(),    0);
        QCOMPARE(tx.cfcPostEqGainDb(), 0);

        // The compression widget should hold the TM per-band defaults.
        // TransmitModel.h:1337 [v2.10.3.13] defines:
        //   m_cfcEqFreqHz       = {0, 125, 250, 500, 1000, 2000, 3000, 4000, 5000, 10000};
        //   m_cfcCompressionDb  = {5, 5, 5, 5, 5, 5, 5, 5, 5, 5};
        //   m_cfcPostEqBandGainDb = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        QVector<double> cf, cg, cq;
        dlg.compWidget()->getPointsData(cf, cg, cq);
        QCOMPARE(cf.size(), 10);
        QCOMPARE(static_cast<int>(std::round(cf[0])),    0);
        QCOMPARE(static_cast<int>(std::round(cf[9])),    10000);
        QCOMPARE(static_cast<int>(std::round(cg[0])),    5);

        QVector<double> ef, eg, eq;
        dlg.postEqWidget()->getPointsData(ef, eg, eq);
        QCOMPARE(ef.size(), 10);
        QCOMPARE(static_cast<int>(std::round(eg[0])),    0);
    }

    // ── 3. Bar chart timer starts on show, stops on hide ───────────────
    void barChartTimerStartsOnShow_StopsOnHide()
    {
        RadioModel rm;
        TxCfcDialog dlg(&rm.transmitModel(), nullptr);

        QVERIFY(dlg.barChartTimer());
        QVERIFY(!dlg.barChartTimer()->isActive());

        dlg.show();
        QApplication::processEvents();
        QVERIFY(dlg.barChartTimer()->isActive());
        QCOMPARE(dlg.barChartTimer()->interval(), 50);  // From Thetis cs:447

        dlg.hide();
        QApplication::processEvents();
        QVERIFY(!dlg.barChartTimer()->isActive());
    }

    // ── 4. closeEvent hides instead of destroys ─────────────────────────
    void closeEventHidesInsteadOfDestroying()
    {
        RadioModel rm;
        TxCfcDialog dlg(&rm.transmitModel(), nullptr);
        dlg.show();
        QApplication::processEvents();
        QVERIFY(dlg.isVisible());

        QCloseEvent ev;
        QApplication::sendEvent(&dlg, &ev);
        QApplication::processEvents();

        // Event was consumed (ignored) and the dialog was hidden but
        // not deleted — the QPointer is still valid.
        QVERIFY(!ev.isAccepted());
        QVERIFY(!dlg.isVisible());
    }

    // ── 5a. Cross-sync: comp.setSelectedIndex → post-EQ selects same idx ─
    //
    // Thetis frmCFCConfig.cs:240-246 [v2.10.3.13] — pointSelected
    // handler walks getIndexFromBandId on the other widget.  Since both
    // widgets are seeded with the same bandIds in resetPointsDefault
    // (1..bandCount) the index match is direct.
    void crossSync_compSelect_setsPostEq()
    {
        RadioModel rm;
        TxCfcDialog dlg(&rm.transmitModel(), nullptr);
        dlg.show();
        QApplication::processEvents();

        dlg.compWidget()->setSelectedIndex(3);
        QApplication::processEvents();

        QCOMPARE(dlg.postEqWidget()->selectedIndex(), 3);
    }

    // ── 5b. Cross-sync: post-EQ.setSelectedIndex → comp selects same idx ─
    void crossSync_postEqSelect_setsComp()
    {
        RadioModel rm;
        TxCfcDialog dlg(&rm.transmitModel(), nullptr);
        dlg.show();
        QApplication::processEvents();

        dlg.postEqWidget()->setSelectedIndex(7);
        QApplication::processEvents();

        QCOMPARE(dlg.compWidget()->selectedIndex(), 7);
    }

    // ── 6a. Band-count radio: switch to 5 changes both widgets' point counts ─
    void advancedDisclosureKeepsHeaderAnchor()
    {
        RadioModel rm;
        TxCfcDialog dlg(&rm.transmitModel(), nullptr);
        dlg.resize(760, 730);
        dlg.show();
        QApplication::processEvents();
        auto* toggle = dlg.findChild<QPushButton*>("TxCfcAdvanced");
        QVERIFY(toggle);
        const QPoint anchor = toggle->mapTo(&dlg, QPoint());
        QVERIFY2(anchor.y() < dlg.compWidget()->mapTo(&dlg, QPoint()).y(),
                 "Advanced disclosure must stay in the fixed header above the plots");
        QVERIFY(toggle->width() < 200);
        QWidget* advanced = dlg.lowSpin()->parentWidget();
        for (int repeat = 0; repeat < 2; ++repeat) {
            toggle->click();
            QApplication::processEvents();
            QCOMPARE(toggle->mapTo(&dlg, QPoint()), anchor);
            QVERIFY(advanced->isVisible());
            const int top = advanced->mapTo(&dlg, QPoint()).y();
            QVERIFY(top >= anchor.y() + toggle->height());
            QVERIFY(top - (anchor.y() + toggle->height()) <= 20);
            QVERIFY(advanced->mapTo(&dlg, QPoint(0, advanced->height())).y()
                    <= dlg.compWidget()->mapTo(&dlg, QPoint()).y());
            toggle->click();
            QApplication::processEvents();
            QCOMPARE(toggle->mapTo(&dlg, QPoint()), anchor);
            QVERIFY(!advanced->isVisible());
        }
    }

    void bandCountRequestReflectsAppliedCountAndRebuildsVisibleSelectors()
    {
        RadioModel rm;
        TxCfcDialog dlg(&rm.transmitModel(), nullptr);
        dlg.show();
        auto* apply = dlg.findChild<QPushButton*>("TxCfcApplyBands");
        auto* cancel = dlg.findChild<QPushButton*>("TxCfcCancelBands");
        auto* undo = dlg.findChild<QPushButton*>("TxCfcUndo");
        auto* redo = dlg.findChild<QPushButton*>("TxCfcRedo");
        QVERIFY(apply && cancel && undo && redo);
        for (int count : {5, 18, 10}) {
            const auto original = rm.transmitModel().effectiveCfcProfile();
            const int current = dlg.currentBandCount();
            auto* requested = count == 5 ? dlg.bands5Radio()
                : count == 18 ? dlg.bands18Radio() : dlg.bands10Radio();
            auto* actual = current == 5 ? dlg.bands5Radio()
                : current == 18 ? dlg.bands18Radio() : dlg.bands10Radio();
            QSignalSpy writes(&rm.transmitModel(), &TransmitModel::cfcEditProfileChanged);
            requested->click();
            QVERIFY(actual->isChecked());
            QCOMPARE(dlg.currentBandCount(), current);
            QCOMPARE(writes.count(), 0);
            QCOMPARE(apply->text(), QString("Apply %1 bands").arg(count));
            QVERIFY(apply->isVisible());
            cancel->click();
            QVERIFY(actual->isChecked());
            QCOMPARE(rm.transmitModel().effectiveCfcProfile(), original);
            QCOMPARE(writes.count(), 0);
            requested->click();
            apply->click();
            QApplication::processEvents();
            QCOMPARE(dlg.currentBandCount(), count);
            QCOMPARE(dlg.postEqWidget()->bandCount(), count);
            QCOMPARE(rm.transmitModel().effectiveCfcProfile().compression.frequenciesHz.size(), count);
            QCOMPARE(rm.transmitModel().effectiveCfcProfile().postEq.frequenciesHz.size(), count);
            QVERIFY(requested->isChecked());
            int selectors = 0;
            for (auto* button : dlg.findChildren<QPushButton*>()) {
                if (button->objectName().startsWith("TxCfcBand")) {
                    ++selectors;
                    QVERIFY(button->isVisible());
                }
            }
            QCOMPARE(selectors, count);
            QCOMPARE(writes.count(), 1);
            QVERIFY(!apply->isVisible());
            undo->click();
            QCOMPARE(dlg.currentBandCount(), current);
            QCOMPARE(rm.transmitModel().effectiveCfcProfile(), original);
            redo->click();
            QCOMPARE(dlg.currentBandCount(), count);
        }
    }

    void bandCountRadio_5_switchesBothWidgets()
    {
        RadioModel rm;
        TxCfcDialog dlg(&rm.transmitModel(), nullptr);

        QCOMPARE(dlg.compWidget()->bandCount(),   10);
        QCOMPARE(dlg.postEqWidget()->bandCount(), 10);

        dlg.bands5Radio()->setChecked(true);
        dlg.findChild<QPushButton*>("TxCfcApplyBands")->click();
        QApplication::processEvents();

        QCOMPARE(dlg.currentBandCount(), 5);
        QCOMPARE(dlg.compWidget()->bandCount(),   5);
        QCOMPARE(dlg.postEqWidget()->bandCount(), 5);
        QCOMPARE(dlg.selectedBandSpin()->maximum(), 5);
    }

    // ── 6b. Band-count radio: switch to 18 changes both widgets' point counts ─
    void bandCountRadio_18_switchesBothWidgets()
    {
        RadioModel rm;
        TxCfcDialog dlg(&rm.transmitModel(), nullptr);

        dlg.bands18Radio()->setChecked(true);
        dlg.findChild<QPushButton*>("TxCfcApplyBands")->click();
        QApplication::processEvents();

        QCOMPARE(dlg.currentBandCount(), 18);
        QCOMPARE(dlg.compWidget()->bandCount(),   18);
        QCOMPARE(dlg.postEqWidget()->bandCount(), 18);
        QCOMPARE(dlg.selectedBandSpin()->maximum(), 18);
    }

    // ── 7. Freq range spinbox changes clamp both widgets ───────────────
    //
    // Note: the dialog widens the freq envelope at construction to cover
    // the highest TM CFC freq (default cfcEqFreq[9]=10000 Hz).  So initial
    // High is 10000, not Thetis's nominal 4000.
    void freqRangeSpinbox_clampsBothWidgets()
    {
        RadioModel rm;
        TxCfcDialog dlg(&rm.transmitModel(), nullptr);

        // Initial envelope after seed: low=0, high=10000 (max TM freq).
        QCOMPARE(dlg.compWidget()->frequencyMinHz(),    0.0);
        QCOMPARE(dlg.compWidget()->frequencyMaxHz(),    10000.0);
        QCOMPARE(dlg.postEqWidget()->frequencyMinHz(),  0.0);
        QCOMPARE(dlg.postEqWidget()->frequencyMaxHz(),  10000.0);
        QCOMPARE(dlg.lowSpin()->value(),  0);
        QCOMPARE(dlg.highSpin()->value(), 10000);

        // Increase low to 200 Hz — should propagate to both widgets.
        dlg.lowSpin()->setValue(200);
        QApplication::processEvents();
        QCOMPARE(dlg.compWidget()->frequencyMinHz(),   200.0);
        QCOMPARE(dlg.postEqWidget()->frequencyMinHz(), 200.0);

        // Decrease high to 8000 Hz.
        dlg.highSpin()->setValue(8000);
        QApplication::processEvents();
        QCOMPARE(dlg.compWidget()->frequencyMaxHz(),   8000.0);
        QCOMPARE(dlg.postEqWidget()->frequencyMaxHz(), 8000.0);
    }

    // ── 7b. Freq range clamp guard: low cannot exceed high - 1000 Hz ───
    //
    // Thetis frmCFCConfig.cs:122-126 [v2.10.3.13] enforces the 1 kHz
    // minimum spread.  Initial high = 10000 (TM seed widens envelope) —
    // setting low to 10000 should clamp it back to 9000 (high - 1000).
    void freqRangeSpinbox_clampGuardEnforces1kHzSpread()
    {
        RadioModel rm;
        TxCfcDialog dlg(&rm.transmitModel(), nullptr);

        QCOMPARE(dlg.highSpin()->value(), 10000);
        dlg.lowSpin()->setValue(10000);
        QApplication::processEvents();
        QCOMPARE(dlg.lowSpin()->value(), 9000);
    }

    // ── 8a. Precomp spin → setCfcPrecompDb ─────────────────────────────
    void precompSpin_drivesTm()
    {
        RadioModel rm;
        TransmitModel& tx = rm.transmitModel();
        TxCfcDialog dlg(&tx, nullptr);
        QSignalSpy spy(&tx, &TransmitModel::cfcPrecompDbChanged);

        dlg.precompSpin()->setValue(10.0);
        QApplication::processEvents();

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(0).toInt(), 10);
        QCOMPARE(tx.cfcPrecompDb(), 10);
    }

    // ── 8b. Post-EQ gain spin → setCfcPostEqGainDb ─────────────────────
    void postEqGainSpin_drivesTm()
    {
        RadioModel rm;
        TransmitModel& tx = rm.transmitModel();
        TxCfcDialog dlg(&tx, nullptr);
        QSignalSpy spy(&tx, &TransmitModel::cfcPostEqGainDbChanged);

        dlg.postEqGainSpin()->setValue(-12.0);
        QApplication::processEvents();

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(0).toInt(), -12);
        QCOMPARE(tx.cfcPostEqGainDb(), -12);
    }

    // ── 9a. Per-band Comp spin (selected band) → updates TM array ──────
    //
    // Select band 5 on the comp widget, then drive nudCFC_c (compSpin):
    // - widget point 5 gain should reflect the new value
    // - TM cfcCompression(5) should reflect the new value
    void compSpin_drivesSelectedBand()
    {
        RadioModel rm;
        TransmitModel& tx = rm.transmitModel();
        TxCfcDialog dlg(&tx, nullptr);
        dlg.show();
        QApplication::processEvents();

        // Select band index 5.
        dlg.compWidget()->setSelectedIndex(5);
        QApplication::processEvents();

        QSignalSpy spy(&tx, &TransmitModel::cfcCompressionChanged);
        dlg.compSpin()->setValue(12.0);
        QApplication::processEvents();

        // Verify widget point 5 gain.
        double f = 0.0, g = 0.0, q = 0.0;
        dlg.compWidget()->getPointData(5, f, g, q);
        QCOMPARE(g, 12.0);

        // Verify TM band 5 compression.  At least one signal fires for the
        // explicit setCfcCompression(5, 12) call; cross-sync may also fire
        // additional signals from the per-band push, so use >= 1.
        QVERIFY(spy.count() >= 1);
        QCOMPARE(tx.cfcCompression(5), 12);
    }

    // ── 9b. Per-band Gain spin (selected band, post-EQ) → updates TM array ─
    void gainSpin_drivesSelectedBand_postEq()
    {
        RadioModel rm;
        TransmitModel& tx = rm.transmitModel();
        TxCfcDialog dlg(&tx, nullptr);
        dlg.show();
        QApplication::processEvents();

        // Select band index 7.
        dlg.postEqWidget()->setSelectedIndex(7);
        QApplication::processEvents();

        QSignalSpy spy(&tx, &TransmitModel::cfcPostEqBandGainChanged);
        dlg.gainSpin()->setValue(-6.0);
        QApplication::processEvents();

        // Verify widget point 7 gain.
        double f = 0.0, g = 0.0, q = 0.0;
        dlg.postEqWidget()->getPointData(7, f, g, q);
        QCOMPARE(g, -6.0);

        QVERIFY(spy.count() >= 1);
        QCOMPARE(tx.cfcPostEqBandGain(7), -6);
    }

    // ── 10. External TM setter updates dialog (model → UI sync) ────────
    //
    // Note: per-band freq updates re-sort the widget's points by frequency
    // (ParametricEqWidget::enforceOrdering — Thetis ucParametricEq.cs:3223
    // [v2.10.3.13]).  After a freq change the value can land at a different
    // widget index — the bandId stays pinned to the band, but the index
    // position shifts to keep the array sorted.  We verify the value
    // appears SOMEWHERE in the widget rather than at a fixed index.
    void externalSetters_updateDialog()
    {
        RadioModel rm;
        TransmitModel& tx = rm.transmitModel();
        TxCfcDialog dlg(&tx, nullptr);

        tx.setCfcPrecompDb(8);
        QApplication::processEvents();
        QCOMPARE(dlg.precompSpin()->value(), 8.0);

        tx.setCfcPostEqGainDb(-3);
        QApplication::processEvents();
        QCOMPARE(dlg.postEqGainSpin()->value(), -3.0);

        // External per-band freq write — should appear in the widget after
        // the sort.  Default cfcEqFreq[2]=250; setting to 1500 puts it
        // between original indices 4 (1000) and 5 (2000) post-sort.
        tx.setCfcEqFreq(2, 1500);
        QApplication::processEvents();
        QVector<double> cf, cg, cq;
        dlg.compWidget()->getPointsData(cf, cg, cq);
        bool found1500 = false;
        for (double f : cf) {
            if (static_cast<int>(std::round(f)) == 1500) { found1500 = true; break; }
        }
        QVERIFY2(found1500, "External setCfcEqFreq value did not propagate to widget");

        // External per-band post-EQ gain write — gains don't affect ordering,
        // so the value should land at the widget index matching its bandId.
        // Default bandId-to-index for sorted defaults: bandId 5 = index 4.
        tx.setCfcPostEqBandGain(4, 9);
        QApplication::processEvents();
        QVector<double> ef, eg, eq;
        dlg.postEqWidget()->getPointsData(ef, eg, eq);
        bool foundGain9 = false;
        for (double g : eg) {
            if (static_cast<int>(std::round(g)) == 9) { foundGain9 = true; break; }
        }
        QVERIFY2(foundGain9, "External setCfcPostEqBandGain value did not propagate to widget");
    }

    // ── 11. OG CFC Guide button is wired (smoke test — we don't open URL) ─
    void ogGuideLink_isClickable()
    {
        RadioModel rm;
        TxCfcDialog dlg(&rm.transmitModel(), nullptr);

        QVERIFY(dlg.ogGuideLink()->isEnabled());
        QVERIFY(dlg.ogGuideLink()->cursor().shape() == Qt::PointingHandCursor);
        // We don't actually click() to avoid spawning a browser during CI.
    }

    // ── 12. Use Q Factors checkbox toggles ParametricEq on both widgets ─
    void useQFactorsCheckbox_togglesParametricEqOnBothWidgets()
    {
        RadioModel rm;
        TxCfcDialog dlg(&rm.transmitModel(), nullptr);

        QVERIFY(!dlg.compWidget()->parametricEq());
        QVERIFY(!dlg.postEqWidget()->parametricEq());
        dlg.useQFactorsChk()->setChecked(true);
        QVERIFY(dlg.compWidget()->parametricEq());
        QVERIFY(dlg.postEqWidget()->parametricEq());
        dlg.useQFactorsChk()->setChecked(false);
        QApplication::processEvents();

        QVERIFY(!dlg.compWidget()->parametricEq());
        QVERIFY(!dlg.postEqWidget()->parametricEq());
    }

    // ── 13. Log scale checkbox toggles logScale on both widgets ────────
    void logScaleCheckbox_togglesBothWidgets()
    {
        RadioModel rm;
        TxCfcDialog dlg(&rm.transmitModel(), nullptr);

        QVERIFY(!dlg.compWidget()->logScale());
        QVERIFY(!dlg.postEqWidget()->logScale());

        dlg.logScaleChk()->setChecked(true);
        QApplication::processEvents();

        QVERIFY(dlg.compWidget()->logScale());
        QVERIFY(dlg.postEqWidget()->logScale());
    }

    // ── 14. Reset Comp button restores flat curve to comp widget ──────
    void resetCompButton_restoresFlatComp()
    {
        RadioModel rm;
        TransmitModel& tx = rm.transmitModel();
        TxCfcDialog dlg(&tx, nullptr);

        // Mutate the comp widget away from defaults.
        tx.setCfcPrecompDb(8);
        tx.setCfcCompression(3, 12);
        QApplication::processEvents();
        const CfcEditProfile beforeReset = tx.effectiveCfcProfile();
        const QString untouchedEq = dlg.postEqWidget()->saveToJson();

        dlg.resetCompBtn()->click();
        QApplication::processEvents();
        QCOMPARE(dlg.postEqWidget()->saveToJson(), untouchedEq);
        CfcProfile::Profile paired;
        QVERIFY(CfcProfile::decode(tx.cfcParaEqData(), paired));
        QCOMPARE(paired.f, std::vector<double>(beforeReset.compression.frequenciesHz.begin(),
                                              beforeReset.compression.frequenciesHz.end()));
        QCOMPARE(paired.postF, std::vector<double>(beforeReset.postEq.frequenciesHz.begin(),
                                                  beforeReset.postEq.frequenciesHz.end()));
        QCOMPARE(paired.f, paired.postF);

        // Comp widget global gain reset to 0.
        QCOMPARE(dlg.compWidget()->globalGainDb(), 0.0);
        // All comp point gains reset to 0.
        QVector<double> cf, cg, cq;
        dlg.compWidget()->getPointsData(cf, cg, cq);
        for (double g : cg) {
            QCOMPARE(g, 0.0);
        }
        // Post-EQ widget untouched (still default flat 0).
        QCOMPARE(dlg.postEqWidget()->globalGainDb(), 0.0);
    }

    // ── 15. Reset EQ button restores flat curve to post-EQ widget ──────
    void resetEqButton_restoresFlatEq()
    {
        RadioModel rm;
        TransmitModel& tx = rm.transmitModel();
        TxCfcDialog dlg(&tx, nullptr);

        tx.setCfcPostEqGainDb(-6);
        tx.setCfcPostEqBandGain(4, 9);
        QApplication::processEvents();
        const CfcEditProfile beforeReset = tx.effectiveCfcProfile();
        const QString untouchedComp = dlg.compWidget()->saveToJson();

        dlg.resetEqBtn()->click();
        QApplication::processEvents();
        QCOMPARE(dlg.compWidget()->saveToJson(), untouchedComp);
        CfcProfile::Profile paired;
        QVERIFY(CfcProfile::decode(tx.cfcParaEqData(), paired));
        QCOMPARE(paired.f, std::vector<double>(beforeReset.compression.frequenciesHz.begin(),
                                              beforeReset.compression.frequenciesHz.end()));
        QCOMPARE(paired.postF, std::vector<double>(beforeReset.postEq.frequenciesHz.begin(),
                                                  beforeReset.postEq.frequenciesHz.end()));
        QCOMPARE(paired.f, paired.postF);

        QCOMPARE(dlg.postEqWidget()->globalGainDb(), 0.0);
        QVector<double> ef, eg, eq;
        dlg.postEqWidget()->getPointsData(ef, eg, eq);
        for (double g : eg) {
            QCOMPARE(g, 0.0);
        }
    }

    // ── 16. Selected-band spinbox drives both widget selections ────────
    //
    // Thetis frmCFCConfig.cs:465-475 [v2.10.3.13] — typing in
    // nudCFC_selected_band updates both ucCFC_comp.SelectedIndex and
    // ucCFC_eq.SelectedIndex.
    void selectedBandSpin_drivesBothWidgetSelections()
    {
        RadioModel rm;
        TxCfcDialog dlg(&rm.transmitModel(), nullptr);

        // Default selectedBand value is 10 (1-based) → both widgets at -1
        // until user activates.
        // Force a different value via the spinbox.
        dlg.selectedBandSpin()->setValue(4);
        QApplication::processEvents();

        // 1-based → 0-based: 4 → 3.
        QCOMPARE(dlg.compWidget()->selectedIndex(),   3);
        QCOMPARE(dlg.postEqWidget()->selectedIndex(), 3);
    }
};

QTEST_MAIN(TestTxCfcDialog)
#include "tst_tx_cfc_dialog.moc"
