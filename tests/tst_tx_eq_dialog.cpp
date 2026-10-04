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
// R-IOS-13 / R-R3-49 (2026-09-28, J.J. Boyd (KG4VCF), AI-assisted via
// Anthropic Claude Code):
//  18. transmit.txEqCurve equals the parametric panel for every factory
//      profile, a custom curve, an out-of-order curve and odd values
//      (unrounded, missing fields, out of range); an unreadable
//      value is "unavailable" while the panel shows the Core's fallback.
//
// =================================================================

#include <QtTest/QtTest>
#include <cmath>
#include <QApplication>
#include <QSemaphore>
#include <QThread>
#include <QScopeGuard>
#include "core/TxChannel.h"
#ifdef HAVE_WDSP
extern "C" {
void OpenChannel(int, int, int, int, int, int, int, int, double, double, double, double, int);
void CloseChannel(int);
}
#endif
#include <QDir>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QLabel>
#include <QStyleOptionSlider>
#include <QScrollArea>
#include <QScrollBar>
#include <QButtonGroup>
#include <QCheckBox>
#include <QCloseEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QComboBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalSpy>
#include <QSlider>
#include <QSpinBox>
#include <QStackedWidget>

#include "core/AppSettings.h"
#include "core/MicProfileManager.h"
#include "core/ParaEqCurve.h"
#include "core/ParaEqEnvelope.h"
#include "core/MicProfileManager.h"
#include "gui/applets/TxEqDialog.h"
#include "gui/widgets/ParametricEqWidget.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

namespace {

// A custom five-point curve with Q factors off, as the dialog's widget
// saves one (values already at the panel's rounding).
QString customCurveJson()
{
    QJsonArray pts;
    const double f[] = {80.0, 400.0, 1250.5, 2200.0, 3100.0};
    const double g[] = {-8.5, 2.0, 0.0, 6.5, -3.0};
    const double q[] = {1.25, 3.0, 4.0, 2.5, 6.0};
    for (int i = 0; i < 5; ++i) {
        pts.append(QJsonObject{{QStringLiteral("frequency_hz"), f[i]},
                               {QStringLiteral("gain_db"), g[i]},
                               {QStringLiteral("q"), q[i]}});
    }
    const QJsonObject root{{QStringLiteral("band_count"), 5},
                           {QStringLiteral("parametric_eq"), false},
                           {QStringLiteral("global_gain_db"), -1.5},
                           {QStringLiteral("frequency_min_hz"), 80.0},
                           {QStringLiteral("frequency_max_hz"), 3100.0},
                           {QStringLiteral("points"), pts}};
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

QJsonObject curveOf(const TransmitModel& tx)
{
    return QJsonDocument::fromJson(tx.txEqCurve().toUtf8()).object();
}

// Half of the 0.001 Hz step a saved frequency is rounded to.
constexpr double kSavedFreqToleranceHz = 0.0005;

// The curve's every field against what the widget holds and draws.
// freqToleranceHz > 0 is for a curve the dialog saved after a range
// change: the saved frequencies are the widget's rounded to 0.001 Hz
// (Thetis ucParametricEq.cs:1474 [v2.10.3.15], SaveToJson, which the
// widget's saveToJson ports; SaveToJsonFromPoints rounds the same way at
// ucParametricEq.cs:1375), and a rescale leaves the widget's unrounded.
void compareToWidget(const QJsonObject& curve, const ParametricEqWidget* w,
                     double freqToleranceHz = 0.0)
{
    QVERIFY(w);
    QCOMPARE(curve.value(QStringLiteral("parametric")).toBool(), w->parametricEq());
    QCOMPARE(curve.value(QStringLiteral("preampDb")).toDouble(), w->globalGainDb());
    QCOMPARE(curve.value(QStringLiteral("minHz")).toDouble(), w->frequencyMinHz());
    QCOMPARE(curve.value(QStringLiteral("maxHz")).toDouble(), w->frequencyMaxHz());
    const QJsonArray points = curve.value(QStringLiteral("points")).toArray();
    QCOMPARE(points.size(), w->points().size());
    for (int i = 0; i < points.size(); ++i) {
        const QJsonObject p = points.at(i).toObject();
        const ParametricEqWidget::EqPoint& e = w->points().at(i);
        if (freqToleranceHz > 0.0) {
            QVERIFY2(std::abs(p.value(QStringLiteral("frequencyHz")).toDouble()
                              - e.frequencyHz) <= freqToleranceHz,
                     qPrintable(QStringLiteral("point %1").arg(i)));
        } else {
            QCOMPARE(p.value(QStringLiteral("frequencyHz")).toDouble(), e.frequencyHz);
        }
        QCOMPARE(p.value(QStringLiteral("gainDb")).toDouble(), e.gainDb);
        QCOMPARE(p.value(QStringLiteral("q")).toDouble(), e.q);
    }
}

} // namespace

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


    void graphicResetUndoRedoPublishOneAcceptedProfile()
    {
        RadioModel rm;
        auto& tx = rm.transmitModel();
        tx.setTxEqPreamp(7); tx.setTxEqBand(0, 3); tx.setTxEqBand(1, -5);
        tx.setTxEqFreq(0, 40); tx.setTxEqFreq(1, 80);
        TxChannel channel(1, 64, 64);
        rm.bindTxEqProfileChannelForTest(&channel);
        const auto original = channel.lastEqProfileForTest();
        TxEqDialog dlg(&rm);
        const quint64 before = channel.eqProfileApplyCountForTest();
        dlg.findChild<QPushButton*>("TxEqLegacyResetBtn")->click();
        QCOMPARE(channel.eqProfileApplyCountForTest(), before + 1);
        QCOMPARE(channel.lastEqProfileForTest()[0], original[0]);
        QCOMPARE(channel.lastEqProfileForTest()[1], std::vector<double>(11, 0));
        dlg.findChild<QPushButton*>("TxEqUndoBtn")->click();
        QCOMPARE(channel.eqProfileApplyCountForTest(), before + 2);
        QCOMPARE(channel.lastEqProfileForTest(), original);
        dlg.findChild<QPushButton*>("TxEqRedoBtn")->click();
        QCOMPARE(channel.eqProfileApplyCountForTest(), before + 3);
        QCOMPARE(channel.lastEqProfileForTest()[1], std::vector<double>(11, 0));
        dlg.findChild<QPushButton*>("TxEqLegacyResetBtn")->click();
        QCOMPARE(channel.eqProfileApplyCountForTest(), before + 3);
        tx.setTxEqBand(2, 9);
        QCOMPARE(channel.eqProfileApplyCountForTest(), before + 4);
        QCOMPARE(channel.lastEqProfileForTest()[1][3], 9.0);
    }

    void queuedLegacyEditsRetainIndependentAcceptedArguments()
    {
        RadioModel rm;
        auto& tx = rm.transmitModel();
        TxChannel channel(1, 64, 64);
        QThread worker;
        QSemaphore started, release;
        channel.moveToThread(&worker); worker.start();
        const auto finish = qScopeGuard([&] {
            release.release();
            rm.bindTxEqProfileChannelForTest(nullptr);
            QMetaObject::invokeMethod(&channel, [&] { channel.moveToThread(rm.thread()); }, Qt::BlockingQueuedConnection);
            worker.quit(); worker.wait();
        });
        rm.bindTxEqProfileChannelForTest(&channel);
        QList<std::array<std::vector<double>, 2>> observed;
        connect(&tx, &TransmitModel::txEqProfileChanged, &channel,
                [&](const QList<int>&, const QList<int>&) { observed.append(channel.lastEqProfileForTest()); });
        QMetaObject::invokeMethod(&channel, [&] { started.release(); release.acquire(); }, Qt::QueuedConnection);
        QVERIFY(started.tryAcquire(1, 5000));
        {
            const auto batch = tx.scopedTxEqProfileUpdate();
            tx.setTxEqPreamp(7); tx.setTxEqBand(0, 3); tx.setTxEqFreq(0, 40);
        }
        {
            const auto batch = tx.scopedTxEqProfileUpdate();
            tx.setTxEqPreamp(-2); tx.setTxEqBand(0, 9); tx.setTxEqFreq(0, 50);
        }
        release.release();
        QMetaObject::invokeMethod(&channel, [] {}, Qt::BlockingQueuedConnection);
        QCOMPARE(observed.size(), 2);
        QCOMPARE(observed[0][0], (std::vector<double>{40,63,125,250,500,1000,2000,4000,8000,16000}));
        QCOMPARE(observed[0][1], (std::vector<double>{7,3,-12,-12,-1,1,4,9,12,-10,-10}));
        QCOMPARE(observed[1][0], (std::vector<double>{50,63,125,250,500,1000,2000,4000,8000,16000}));
        QCOMPARE(observed[1][1], (std::vector<double>{-2,9,-12,-12,-1,1,4,9,12,-10,-10}));
    }


    void nestedLegacyBatchPublishesAtOutermostExitOnly()
    {
        RadioModel rm;
        auto& tx = rm.transmitModel();
        TxChannel channel(1, 64, 64);
        rm.bindTxEqProfileChannelForTest(&channel);
        QSignalSpy bands(&tx, &TransmitModel::txEqBandChanged);
        const quint64 before = channel.eqProfileApplyCountForTest();
        const auto edit = [&] {
            const auto outer = tx.scopedTxEqProfileUpdate();
            tx.setTxEqPreamp(7);
            {
                const auto inner = tx.scopedTxEqProfileUpdate();
                tx.setTxEqBand(0, 3); tx.setTxEqFreq(0, 40);
            }
            QCOMPARE(channel.eqProfileApplyCountForTest(), before);
            tx.setTxEqBand(1, -5);
            return; // RAII closes both nested and early-return paths.
        };
        edit();
        QCOMPARE(bands.count(), 2);
        QCOMPARE(channel.eqProfileApplyCountForTest(), before + 1);
        QCOMPARE(channel.lastEqProfileForTest()[0], (std::vector<double>{40,63,125,250,500,1000,2000,4000,8000,16000}));
        QCOMPARE(channel.lastEqProfileForTest()[1], (std::vector<double>{7,3,-5,-12,-1,1,4,9,12,-10,-10}));
        {
            const auto batch = tx.scopedTxEqProfileUpdate();
            tx.setTxEqBand(0, 3); tx.setTxEqBand(-1, 9);
        }
        {
            const auto batch = tx.scopedTxEqProfileUpdate();
            tx.setTxEqBand(0, 8); tx.setTxEqBand(0, 3);
        }
        QCOMPARE(channel.eqProfileApplyCountForTest(), before + 1);
    }

    void legacyBindAndMatchingProfileReplayApplyOnce()
    {
        RadioModel rm;
        auto& tx = rm.transmitModel();
        tx.loadFromSettings("aa:bb:cc:11:22:33");
        auto* manager = rm.micProfileManager(); QVERIFY(manager);
        manager->setMacAddress("aa:bb:cc:11:22:33"); manager->load();
        tx.setTxEqPreamp(7); tx.setTxEqBand(0, 3); tx.setTxEqFreq(0, 40);
        QVERIFY(manager->saveProfile("Graphic", &tx));
        TxChannel channel(1, 64, 64);
        rm.bindTxEqProfileChannelForTest(&channel);
        QCOMPARE(channel.eqProfileApplyCountForTest(), quint64(1));
        const auto original = channel.lastEqProfileForTest();
        QVERIFY(manager->setActiveProfile("Graphic", &tx));
        QCOMPARE(channel.eqProfileApplyCountForTest(), quint64(2));
        QCOMPARE(channel.lastEqProfileForTest(), original);
        rm.bindTxEqProfileChannelForTest(nullptr);
        tx.setTxEqBand(0, 9);
        QCOMPARE(channel.eqProfileApplyCountForTest(), quint64(2));
        rm.bindTxEqProfileChannelForTest(&channel);
        QCOMPARE(channel.eqProfileApplyCountForTest(), quint64(3));
        QCOMPARE(channel.lastEqProfileForTest()[1][1], 9.0);
        rm.bindTxEqProfileChannelForTest(&channel);
        QCOMPARE(channel.eqProfileApplyCountForTest(), quint64(4));
        tx.setTxEqBand(0, 8);
        QCOMPARE(channel.eqProfileApplyCountForTest(), quint64(5));
    }

    void focusedNumericTeardownIsSafe_data()
    {
        QTest::addColumn<QString>("field");
        QTest::newRow("graphic") << QString("graphic");
        QTest::newRow("parametric-amount") << QString("gain");
        QTest::newRow("parametric-Q") << QString("q");
    }
    void focusedNumericTeardownIsSafe()
    {
        QFETCH(QString, field);
        RadioModel rm;
        QString expectedBlob;
        {
            TxEqDialog dlg(&rm);
            QAbstractSpinBox* spin = dlg.findChild<QSpinBox*>("TxEqPreampSpin");
            if (field != "graphic") {
                dlg.modeSelector()->button(1)->click();
                dlg.parametricWidget()->setGlobalGainDb(3.0);
                dlg.parametricWidget()->setSelectedIndex(3);
                spin = dlg.findChild<QDoubleSpinBox*>(field == "gain" ? "TxEqParaGainSpin" : "TxEqParaQSpin");
            }
            dlg.show(); dlg.activateWindow(); spin->setFocus(); QApplication::processEvents();
            QVERIFY(spin->hasFocus());
            if (field == "graphic") { static_cast<QSpinBox*>(spin)->setValue(7); }
            else {
                static_cast<QDoubleSpinBox*>(spin)->setValue(5.5);
                expectedBlob = ParaEqEnvelope::encode(dlg.parametricWidget()->saveToJson());
            }
            // Leave the numeric session open during destruction.
        }
        QApplication::processEvents();
        if (field == "graphic") { QCOMPARE(rm.transmitModel().txEqPreamp(), 7); }
        else { QCOMPARE(rm.transmitModel().txEqParaEqData(), expectedBlob); }
    }

    void laptopLayoutAndNativeCapture()
    {
        RadioModel rm; TxEqDialog dlg(&rm); dlg.resize(853, 500); dlg.show();
        QApplication::processEvents();
        QVERIFY(dlg.minimumSizeHint().height() <= 500);
        QVERIFY(dlg.minimumSizeHint().width() <= 853);
        for (int i = 0; i < 10; ++i) {
            auto* input = dlg.findChild<QSpinBox*>(QString("TxEqFreqSpin%1").arg(i));
            const QPoint position = input->mapTo(&dlg, QPoint());
            QVERIFY(position.x() >= 0 && position.x() + input->width() <= dlg.width());
            QVERIFY(position.y() + input->height() <= dlg.height());
            auto* line = input->findChild<QLineEdit*>(); QVERIFY(line);
            QVERIFY(line->contentsRect().width() >= line->fontMetrics().horizontalAdvance(QString::number(TransmitModel::kTxEqFreqHzMax)) + 6);
        }
        if (qEnvironmentVariableIsSet("NEREUS_CAPTURE_TX")) {
            const QDir output(qEnvironmentVariable("NEREUS_CAPTURE_TX"));
            QVERIFY(output.exists());
            QVERIFY(dlg.grab().save(output.filePath("task-4-laptop-graphic.png")));
        }
        dlg.modeSelector()->button(1)->click();
        dlg.bandCountGroup()->button(18)->click();
        dlg.findChild<QPushButton*>("TxEqCountApplyBtn")->click();
        dlg.parametricWidget()->setSelectedIndex(8);
        dlg.findChild<QDoubleSpinBox*>("TxEqParaGainSpin")->setValue(5.0);
        QApplication::processEvents();
        QCOMPARE(dlg.findChild<QButtonGroup*>("TxEqBandSelector")->buttons().size(), 18);
        for (auto* button : dlg.findChild<QButtonGroup*>("TxEqBandSelector")->buttons()) {
            const QStringList lines = button->text().split('\n');
            QVERIFY(button->width() >= button->fontMetrics().horizontalAdvance(lines.last()) + 22);
        }
        auto* strip = dlg.findChild<QScrollArea*>("TxEqBandStripScroll"); QVERIFY(strip);
        QTRY_VERIFY(strip->widget()->width() >= 18 * 86);
        QTRY_VERIFY(dlg.findChild<QButtonGroup*>("TxEqBandSelector")->button(2)->geometry().left()
            > dlg.findChild<QButtonGroup*>("TxEqBandSelector")->button(1)->geometry().right());
        for (int id = 2; id <= 18; ++id) {
            const auto* previous = dlg.findChild<QButtonGroup*>("TxEqBandSelector")->button(id - 1);
            const auto* current = dlg.findChild<QButtonGroup*>("TxEqBandSelector")->button(id);
            QVERIFY(current->geometry().left() > previous->geometry().right());
        }
        QVERIFY(dlg.parametricWidget()->height() >= 180);
        QVERIFY(dlg.parametricWidget()->width() <= dlg.width());
        QVERIFY(dlg.findChild<QScrollArea*>("TxEqSelectedControlsScroll")->height() >= 100);
        if (qEnvironmentVariableIsSet("NEREUS_CAPTURE_TX")) {
            const QDir output(qEnvironmentVariable("NEREUS_CAPTURE_TX"));
            QVERIFY(dlg.grab().save(output.filePath("task-4-laptop-parametric.png")));
            auto* controls = dlg.findChild<QScrollArea*>("TxEqSelectedControlsScroll");
            controls->verticalScrollBar()->setValue(controls->verticalScrollBar()->maximum()); QApplication::processEvents();
            QVERIFY(dlg.grab().save(output.filePath("task-4-laptop-parametric-controls.png")));
            controls->verticalScrollBar()->setValue(0);
            dlg.resize(1000, 720); QApplication::processEvents();
            QVERIFY(dlg.grab().save(output.filePath("task-4-native-parametric.png")));
            dlg.modeSelector()->button(0)->click(); QApplication::processEvents();
            QVERIFY(dlg.grab().save(output.filePath("task-4-native-graphic.png")));
        }
    }

    void sharedAlgorithmsApplyAndUndoInBothModes()
    {
        RadioModel rm; TxEqDialog dlg(&rm); dlg.show();
        auto* nc = dlg.findChild<QSpinBox*>("TxEqNcSpin");
        auto* mp = dlg.findChild<QCheckBox*>("TxEqMpChk");
        auto* cutoff = dlg.findChild<QComboBox*>("TxEqCtfmodeCombo");
        auto* window = dlg.findChild<QComboBox*>("TxEqWintypeCombo");
        auto* undo = dlg.findChild<QPushButton*>("TxEqUndoBtn");
        dlg.findChild<QPushButton*>("TxEqAdvancedToggle")->click();
        for (int mode : {0, 1}) {
            dlg.modeSelector()->button(mode)->click();
            nc->setFocus(); nc->selectAll(); QTest::keyClicks(nc, "512"); QTest::keyClick(nc, Qt::Key_Return);
            QCOMPARE(rm.transmitModel().txEqNc(), 512); undo->click();
            QCOMPARE(nc->value(), 2048); QCOMPARE(rm.transmitModel().txEqNc(), 2048);
            mp->click(); cutoff->setCurrentIndex(1); window->setCurrentIndex(1);
            QCOMPARE(rm.transmitModel().txEqMp(), true);
            QCOMPARE(rm.transmitModel().txEqCtfmode(), 1); QCOMPARE(rm.transmitModel().txEqWintype(), 1);
            undo->click(); QCOMPARE(window->currentIndex(), 0); QCOMPARE(rm.transmitModel().txEqWintype(), 0);
            undo->click(); QCOMPARE(cutoff->currentIndex(), 0); QCOMPARE(rm.transmitModel().txEqCtfmode(), 0);
            undo->click(); QCOMPARE(mp->isChecked(), false); QCOMPARE(rm.transmitModel().txEqMp(), false);
        }
    }

    void sharedAlgorithmModeSwitchDoesNotMakeNoOpAudioEdit()
    {
        RadioModel rm; TxEqDialog dlg(&rm); dlg.show();
        dlg.findChild<QSpinBox*>("TxEqNcSpin")->setValue(512);
        dlg.modeSelector()->button(1)->click();
        const QString blob = rm.transmitModel().txEqParaEqData();
        QSignalSpy writes(&rm.transmitModel(), &TransmitModel::txEqParaEqDataChanged);
        dlg.findChild<QDoubleSpinBox*>("TxEqParaGainSpin")->setFocus();
        dlg.parametricWidget()->setFocus(); QApplication::processEvents();
        QCOMPARE(writes.count(), 0); QCOMPARE(rm.transmitModel().txEqParaEqData(), blob);
        QVERIFY(!dlg.findChild<QPushButton*>("TxEqUndoBtn")->isEnabled());
        dlg.modeSelector()->button(0)->click(); QVERIFY(dlg.findChild<QPushButton*>("TxEqUndoBtn")->isEnabled());
    }

    void fractionalRangeRescalesAndUndoRestores()
    {
        RadioModel rm; ParametricEqWidget saved;
        saved.setFrequencyMinHz(20.125); saved.setFrequencyMaxHz(4000.625);
        rm.transmitModel().setTxEqParaEqData(ParaEqEnvelope::encode(saved.saveToJson()));
        TxEqDialog dlg(&rm); dlg.modeSelector()->button(1)->click();
        auto* graph = dlg.parametricWidget(); const QByteArray before = graph->saveEditState();
        auto* low = dlg.findChild<QDoubleSpinBox*>("TxEqParaLowSpin");
        auto* high = dlg.findChild<QDoubleSpinBox*>("TxEqParaHighSpin");
        QCOMPARE(low->value(), 20.125); QCOMPARE(high->value(), 4000.625);
        low->setValue(3500.0); QCOMPARE(graph->frequencyMaxHz() - graph->frequencyMinHz(), 1000.0);
        QCOMPARE(low->value(), 3000.625);
        QCOMPARE(graph->points().first().frequencyHz, 3000.625);
        dlg.findChild<QPushButton*>("TxEqUndoBtn")->click(); QCOMPARE(graph->saveEditState(), before);
        QCOMPARE(low->value(), 20.125); QCOMPARE(high->value(), 4000.625);
        high->setValue(200.0); QCOMPARE(graph->frequencyMaxHz() - graph->frequencyMinHz(), 1000.0);
        dlg.findChild<QPushButton*>("TxEqUndoBtn")->click(); QCOMPARE(graph->saveEditState(), before);
    }

    void noOpFocusAndSelectionPreserveOpaqueBlob()
    {
        RadioModel rm; TxEqDialog dlg(&rm); dlg.modeSelector()->button(1)->click(); dlg.show();
        for (const QString& blob : {QString(), QStringLiteral("unknown opaque profile")}) {
            rm.transmitModel().setTxEqParaEqData(blob);
            QSignalSpy writes(&rm.transmitModel(), &TransmitModel::txEqParaEqDataChanged);
            auto* gain = dlg.findChild<QDoubleSpinBox*>("TxEqParaGainSpin"); gain->setFocus();
            dlg.parametricWidget()->setSelectedIndex(3); dlg.parametricWidget()->setFocus();
            QApplication::processEvents();
            QCOMPARE(writes.count(), 0); QCOMPARE(rm.transmitModel().txEqParaEqData(), blob);
            QVERIFY(!dlg.findChild<QPushButton*>("TxEqUndoBtn")->isEnabled());
        }
    }

    void focusedTextUndoWinsAndNumericProfileReplacementCancels()
    {
        RadioModel rm; TxEqDialog dlg(&rm); dlg.modeSelector()->button(1)->click(); dlg.show();
        auto* graph = dlg.parametricWidget(); graph->setSelectedIndex(3);
        auto* gain = dlg.findChild<QDoubleSpinBox*>("TxEqParaGainSpin");
        graph->setGlobalGainDb(3.0); // existing history must survive text-local undo
        gain->setFocus(); gain->selectAll(); QTest::keyClicks(gain, "12.5");
        auto* line = gain->findChild<QLineEdit*>(); QVERIFY(line->isUndoAvailable());
        QTest::keySequence(line, QKeySequence::Undo);
        QCOMPARE(graph->globalGainDb(), 3.0); QCOMPARE(graph->points()[3].gainDb, 0.0);
        gain->selectAll(); QTest::keyClicks(gain, "9.5");
        ParametricEqWidget saved; saved.setGlobalGainDb(7.0);
        const auto blob = ParaEqEnvelope::encode(saved.saveToJson()); rm.transmitModel().setTxEqParaEqData(blob);
        const auto loaded = graph->saveEditState();
        QTest::keyClick(gain, Qt::Key_Return); QCOMPARE(graph->saveEditState(), loaded);
        QCOMPARE(rm.transmitModel().txEqParaEqData(), blob);
        QVERIFY(!dlg.findChild<QPushButton*>("TxEqUndoBtn")->isEnabled());
    }

    void gainAndWidthKeepCloseFrequenciesAndExactHistory()
    {
        RadioModel rm; TxEqDialog dlg(&rm); dlg.modeSelector()->button(1)->click();
        auto* graph = dlg.parametricWidget();
        ParametricEqWidget::EqJsonState state;
        state.bandCount = graph->bandCount(); state.frequencyMinHz = graph->frequencyMinHz(); state.frequencyMaxHz = graph->frequencyMaxHz();
        state.points = graph->points(); state.points[2].frequencyHz = state.points[1].frequencyHz + 1.125;
        state.points[2].q = 1.23456789; QVERIFY(graph->setEditorCurveState(state));
        graph->setSelectedIndex(2); const auto before = graph->saveEditState();
        dlg.findChild<QDoubleSpinBox*>("TxEqParaGainSpin")->setValue(5.0);
        QCOMPARE(graph->points()[2].frequencyHz, state.points[2].frequencyHz);
        QCOMPARE(graph->points()[2].q, 1.23456789);
        dlg.findChild<QPushButton*>("TxEqUndoBtn")->click(); QCOMPARE(graph->saveEditState(), before);
        dlg.findChild<QDoubleSpinBox*>("TxEqParaQSpin")->setValue(2.5);
        QCOMPARE(graph->points()[2].q, 2.5);
        QCOMPARE(graph->selectedIndex(), 2);
        QCOMPARE(graph->points()[2].frequencyHz, state.points[2].frequencyHz);
        dlg.findChild<QPushButton*>("TxEqUndoBtn")->click(); QCOMPARE(graph->saveEditState(), before);
    }

    void sameBlobProfileActivationInterruptsWidthAndNumericEdits()
    {
        RadioModel rm; auto* profiles = rm.micProfileManager(); QVERIFY(profiles);
        profiles->setMacAddress(QStringLiteral("aa:bb:cc:11:22:33")); profiles->load();
        ParametricEqWidget saved; saved.setGlobalGainDb(2.0);
        const QString blob = ParaEqEnvelope::encode(saved.saveToJson());
        rm.transmitModel().setTxEqParaEqData(blob);
        QVERIFY(profiles->saveProfile("Identical A", &rm.transmitModel()));
        QVERIFY(profiles->saveProfile("Identical B", &rm.transmitModel()));
        QVERIFY(profiles->setActiveProfile("Identical A", &rm.transmitModel()));
        TxEqDialog dlg(&rm); dlg.modeSelector()->button(1)->click(); dlg.show();
        auto* graph = dlg.parametricWidget(); graph->setSelectedIndex(3);
        const auto savedGraph = graph->saveEditState();
        auto* slider = dlg.findChild<QSlider*>("TxEqWidthSlider");
        QStyleOptionSlider option; option.initFrom(slider); option.orientation = Qt::Horizontal;
        option.minimum = slider->minimum(); option.maximum = slider->maximum(); option.sliderPosition = slider->value(); option.sliderValue = slider->value();
        const QPoint handle = slider->style()->subControlRect(QStyle::CC_Slider, &option, QStyle::SC_SliderHandle, slider).center();
        QSignalSpy writes(&rm.transmitModel(), &TransmitModel::txEqParaEqDataChanged);
        QTest::mousePress(slider, Qt::LeftButton, Qt::NoModifier, handle);
        QTest::mouseMove(slider, handle + QPoint(-40, 0));
        QVERIFY(graph->saveEditState() != savedGraph); QCOMPARE(writes.count(), 0);
        QVERIFY(profiles->setActiveProfile("Identical B", &rm.transmitModel()));
        QTest::mouseRelease(slider, Qt::LeftButton, Qt::NoModifier, handle + QPoint(-40, 0));
        QCOMPARE(graph->saveEditState(), savedGraph); QCOMPARE(writes.count(), 0);
        auto* gain = dlg.findChild<QDoubleSpinBox*>("TxEqParaGainSpin");
        gain->setFocus(); gain->selectAll(); QTest::keyClicks(gain, "9.5");
        QVERIFY(profiles->setActiveProfile("Identical A", &rm.transmitModel()));
        QTest::keyClick(gain, Qt::Key_Return); QCOMPARE(graph->saveEditState(), savedGraph);
        QCOMPARE(writes.count(), 0); QVERIFY(!dlg.findChild<QPushButton*>("TxEqUndoBtn")->isEnabled());
    }

    void emptyAndUnknownProfilesSeedFreshDefaultWithoutWriteback()
    {
        RadioModel rm; TxEqDialog dlg(&rm); dlg.modeSelector()->button(1)->click();
        RadioModel freshRm; TxEqDialog fresh(&freshRm); const auto defaults = fresh.parametricWidget()->saveEditState();
        for (const QString& blob : {QString(), QStringLiteral("unknown opaque profile")}) {
            ParametricEqWidget saved; saved.setBandCount(18); saved.setGlobalGainDb(7.0);
            rm.transmitModel().setTxEqParaEqData(ParaEqEnvelope::encode(saved.saveToJson()));
            QCOMPARE(dlg.parametricWidget()->bandCount(), 18);
            rm.transmitModel().setTxEqParaEqData(blob);
            QSignalSpy writes(&rm.transmitModel(), &TransmitModel::txEqParaEqDataChanged);
            QCOMPARE(dlg.parametricWidget()->saveEditState(), defaults);
            QCOMPARE(rm.transmitModel().txEqParaEqData(), blob); QCOMPARE(writes.count(), 0);
            QVERIFY(!dlg.findChild<QPushButton*>("TxEqUndoBtn")->isEnabled());
        }
    }

    void realWidthGesturesCommitAndUndo_data()
    {
        QTest::addColumn<int>("count"); QTest::addColumn<bool>("live");
        for (int count : {5, 10, 18}) {
            for (bool live : {false, true}) { QTest::newRow(qPrintable(QString("%1-%2").arg(count).arg(live))) << count << live; }
        }
    }

    void realWidthGesturesCommitAndUndo()
    {
        QFETCH(int, count); QFETCH(bool, live);
        RadioModel rm; ParametricEqWidget saved; saved.setFrequencyMaxHz(2700.0); saved.setBandCount(count);
        rm.transmitModel().setTxEqParaEqData(ParaEqEnvelope::encode(saved.saveToJson()));
        TxEqDialog dlg(&rm); dlg.modeSelector()->button(1)->click(); dlg.show();
        dlg.findChild<QCheckBox*>("TxEqParaLiveUpdateChk")->setChecked(live);
        auto* graph = dlg.parametricWidget(); const int index = count / 2; graph->setSelectedIndex(index);
        const auto before = graph->saveEditState(); const auto point = graph->points()[index];
        const QRect plot = graph->plotRect().toRect();
        const double halfWidth = (graph->frequencyMaxHz() - graph->frequencyMinHz()) / (6 * point.q);
        QPoint start(plot.left() + qRound((point.frequencyHz + halfWidth) / graph->frequencyMaxHz() * plot.width()), plot.center().y() + 22);
        QSignalSpy writes(&rm.transmitModel(), &TransmitModel::txEqParaEqDataChanged);
        QTest::mousePress(graph, Qt::LeftButton, Qt::NoModifier, start);
        QTest::mouseMove(graph, start + QPoint(30, 0));
        QVERIFY(graph->points()[index].q != point.q);
        QCOMPARE(graph->points()[index].frequencyHz, point.frequencyHz); QCOMPARE(graph->points()[index].gainDb, point.gainDb);
        if (live) { QVERIFY(writes.count() >= 1); } else { QCOMPARE(writes.count(), 0); }
        QTest::mouseRelease(graph, Qt::LeftButton, Qt::NoModifier, start + QPoint(30, 0));
        if (!live) { QCOMPARE(writes.count(), 1); }
        auto* undo = dlg.findChild<QPushButton*>("TxEqUndoBtn"); QVERIFY(undo->isEnabled()); undo->click();
        QCOMPARE(graph->saveEditState(), before); QVERIFY(!undo->isEnabled());
        dlg.findChild<QPushButton*>("TxEqRedoBtn")->click(); QVERIFY(graph->saveEditState() != before);
    }

    void modesKeepIndependentValues()
    {
        RadioModel rm; TxEqDialog dlg(&rm);
        auto* modes = dlg.findChild<QButtonGroup*>("TxEqModeSelector"); QVERIFY(modes);
        rm.transmitModel().setTxEqBand(2, 8);
        modes->button(1)->click();
        dlg.parametricWidget()->setGlobalGainDb(3.5);
        modes->button(0)->click();
        QCOMPARE(rm.transmitModel().txEqBand(2), 8);
        modes->button(1)->click();
        QCOMPARE(dlg.parametricWidget()->globalGainDb(), 3.5);
    }

    void legacyAllInputsRemainEditable()
    {
        RadioModel rm; TxEqDialog dlg(&rm);
        QVERIFY(dlg.findChild<QPushButton*>("TxEqUndoBtn"));
        for (int i = 0; i < 10; ++i) {
            auto* gain = dlg.findChild<QSpinBox*>(QString("TxEqBandSpin%1").arg(i));
            auto* freq = dlg.findChild<QSpinBox*>(QString("TxEqFreqSpin%1").arg(i));
            QVERIFY(gain && freq); QVERIFY(gain->isEnabled()); QVERIFY(freq->isEnabled());
            gain->setValue(i); freq->setValue(100 + i * 150);
            QCOMPARE(rm.transmitModel().txEqBand(i), i);
            QCOMPARE(rm.transmitModel().txEqFreq(i), 100 + i * 150);
        }
    }

    void selectedEditorMatchesGraph()
    {
        RadioModel rm; TxEqDialog dlg(&rm);
        auto* chips = dlg.findChild<QButtonGroup*>("TxEqBandSelector"); QVERIFY(chips);
        auto* graph = dlg.parametricWidget(); graph->setSelectedIndex(3);
        QCOMPARE(chips->checkedId(), graph->points()[3].bandId);
        chips->button(graph->points()[5].bandId)->click();
        QCOMPARE(graph->selectedIndex(), 5);
        QCOMPARE(dlg.findChild<QSpinBox*>("TxEqParaFreqSpin")->value(), qRound(graph->points()[5].frequencyHz));
    }

    void widthSliderAndEntryAgree()
    {
        RadioModel rm; TxEqDialog dlg(&rm);
        auto* slider = dlg.findChild<QSlider*>("TxEqWidthSlider"); QVERIFY(slider);
        dlg.parametricWidget()->setSelectedIndex(3);
        auto* q = dlg.findChild<QDoubleSpinBox*>("TxEqParaQSpin");
        q->setValue(2.0);
        QVERIFY(qAbs(slider->value() - 500) <= 1);
        slider->setValue(1000); QCOMPARE(q->value(), 20.0);
        QCOMPARE(dlg.parametricWidget()->points()[3].q, 20.0);
        dlg.findChild<QCheckBox*>("TxEqParaUseQFactorsChk")->setChecked(false);
        QVERIFY(!slider->isEnabled()); QVERIFY(!q->isEnabled());
        QCOMPARE(dlg.parametricWidget()->points()[3].q, 20.0);
    }

    void oneDragOneUndo()
    {
        RadioModel rm; TxEqDialog dlg(&rm);
        auto* undo = dlg.findChild<QPushButton*>("TxEqUndoBtn"); QVERIFY(undo);
        QMetaObject::invokeMethod(&dlg, "onLegacyToggled", Q_ARG(bool, false)); dlg.show();
        auto* graph = dlg.parametricWidget(); const auto before = graph->saveEditState();
        const QRect plot = graph->plotRect().toRect(); const auto point = graph->points()[4];
        QPoint start(plot.left() + qRound(point.frequencyHz / graph->frequencyMaxHz() * plot.width()), plot.center().y());
        QTest::mousePress(graph, Qt::LeftButton, Qt::NoModifier, start);
        QTest::mouseMove(graph, start + QPoint(0, -30));
        QTest::mouseRelease(graph, Qt::LeftButton, Qt::NoModifier, start + QPoint(0, -30));
        QVERIFY(graph->saveEditState() != before); QVERIFY(undo->isEnabled()); undo->click();
        QCOMPARE(graph->saveEditState(), before); QVERIFY(!undo->isEnabled());
    }

    void directEntryCommitsOnce()
    {
        RadioModel rm; TxEqDialog dlg(&rm);
        auto* undo = dlg.findChild<QPushButton*>("TxEqUndoBtn"); QVERIFY(undo);
        QMetaObject::invokeMethod(&dlg, "onLegacyToggled", Q_ARG(bool, false)); dlg.show();
        auto* graph = dlg.parametricWidget(); graph->setSelectedIndex(3);
        const auto before = graph->saveEditState();
        auto* gain = dlg.findChild<QDoubleSpinBox*>("TxEqParaGainSpin"); gain->setFocus(); gain->selectAll();
        QTest::keyClicks(gain, "12.5"); QTest::keyClick(gain, Qt::Key_Return);
        QCOMPARE(graph->points()[3].gainDb, 12.5); QVERIFY(undo->isEnabled());
        undo->click(); QCOMPARE(graph->saveEditState(), before); QVERIFY(!undo->isEnabled());
    }

    void advancedDisclosureKeepsHeaderAnchor_data()
    {
        QTest::addColumn<bool>("legacy");
        QTest::newRow("graphic") << true;
        QTest::newRow("parametric") << false;
    }

    void advancedDisclosureKeepsHeaderAnchor()
    {
        QFETCH(bool, legacy);
        RadioModel rm;
        TxEqDialog dlg(&rm);
        dlg.modeSelector()->button(legacy ? 0 : 1)->click();
        dlg.resize(1000, 720);
        dlg.show();
        QApplication::processEvents();
        auto* toggle = dlg.findChild<QPushButton*>("TxEqAdvancedToggle");
        auto* advanced = dlg.findChild<QWidget*>("TxEqAdvancedControls");
        auto* enable = dlg.findChild<QCheckBox*>("TxEqEnableChk");
        QVERIFY(toggle && advanced && enable);
        const QPoint anchor = toggle->mapTo(&dlg, QPoint());
        QVERIFY2(qAbs(anchor.y() - enable->mapTo(&dlg, QPoint()).y()) <= 8,
                 "Advanced must remain beside Enable in the fixed header");
        QVERIFY(toggle->width() < 200);
        for (int repeat = 0; repeat < 2; ++repeat) {
            toggle->click();
            QApplication::processEvents();
            QCOMPARE(toggle->mapTo(&dlg, QPoint()), anchor);
            QVERIFY(advanced->isVisible());
            const int top = advanced->mapTo(&dlg, QPoint()).y();
            QVERIFY(top >= anchor.y() + toggle->height());
            QVERIFY(top - (anchor.y() + toggle->height()) <= 20);
            auto* advancedScroll = dlg.findChild<QScrollArea*>("TxEqAdvancedScroll");
            QVERIFY(advancedScroll);
            // The visible section is capped; taller controls remain scrollable.
            QVERIFY(advancedScroll->mapTo(&dlg, QPoint(0, advancedScroll->height())).y()
                    <= dlg.panelStack()->mapTo(&dlg, QPoint()).y());
            toggle->click();
            QApplication::processEvents();
            QCOMPARE(toggle->mapTo(&dlg, QPoint()), anchor);
            QVERIFY(!advanced->isVisible());
        }
    }

    void bandCountRequestReflectsAppliedCountAndRebuildsVisibleSelectors()
    {
        RadioModel rm;
        TxEqDialog dlg(&rm);
        dlg.modeSelector()->button(1)->click();
        dlg.show();
        auto* graph = dlg.parametricWidget();
        auto* selectors = dlg.findChild<QButtonGroup*>("TxEqBandSelector");
        auto* apply = dlg.findChild<QPushButton*>("TxEqCountApplyBtn");
        auto* cancel = dlg.findChild<QPushButton*>("TxEqCountCancelBtn");
        QVERIFY(selectors && apply && cancel);
        for (int count : {5, 18, 10}) {
            const int current = graph->bandCount();
            const QByteArray original = graph->saveEditState();
            QSignalSpy writes(&rm.transmitModel(), &TransmitModel::txEqParaEqDataChanged);
            dlg.bandCountGroup()->button(count)->click();
            QCOMPARE(dlg.bandCountGroup()->checkedId(), current);
            QCOMPARE(graph->saveEditState(), original);
            QCOMPARE(writes.count(), 0);
            QVERIFY(apply->isVisible());
            QCOMPARE(apply->text(), QString("Apply %1 bands").arg(count));
            cancel->click();
            QCOMPARE(dlg.bandCountGroup()->checkedId(), current);
            QCOMPARE(graph->saveEditState(), original);
            QCOMPARE(writes.count(), 0);
            dlg.bandCountGroup()->button(count)->click();
            apply->click();
            QApplication::processEvents();
            QCOMPARE(graph->bandCount(), count);
            QCOMPARE(graph->points().size(), count);
            QCOMPARE(curveOf(rm.transmitModel()).value("points").toArray().size(), count);
            compareToWidget(curveOf(rm.transmitModel()), graph, kSavedFreqToleranceHz);
            QCOMPARE(dlg.bandCountGroup()->checkedId(), count);
            QCOMPARE(selectors->buttons().size(), count);
            for (auto* button : selectors->buttons()) {
                QVERIFY(button->isVisible());
            }
            QCOMPARE(writes.count(), 1);
            QVERIFY(!apply->isVisible());
            dlg.findChild<QPushButton*>("TxEqUndoBtn")->click();
            QCOMPARE(graph->saveEditState(), original);
            QCOMPARE(selectors->buttons().size(), current);
            dlg.findChild<QPushButton*>("TxEqRedoBtn")->click();
            QCOMPARE(graph->bandCount(), count);
            QCOMPARE(selectors->buttons().size(), count);
        }
    }

    void countApplyCancelAndUndo()
    {
        RadioModel rm; TxEqDialog dlg(&rm);
        auto* apply = dlg.findChild<QPushButton*>("TxEqCountApplyBtn"); QVERIFY(apply);
        QMetaObject::invokeMethod(&dlg, "onLegacyToggled", Q_ARG(bool, false));
        auto* graph = dlg.parametricWidget(); const auto before = graph->saveEditState();
        QSignalSpy writes(&rm.transmitModel(), &TransmitModel::txEqParaEqDataChanged);
        dlg.bandCountGroup()->button(5)->click(); QCOMPARE(graph->bandCount(), 10);
        dlg.findChild<QPushButton*>("TxEqCountCancelBtn")->click();
        QCOMPARE(graph->saveEditState(), before); QCOMPARE(writes.count(), 0);
        dlg.bandCountGroup()->button(5)->click(); apply->click();
        QCOMPARE(graph->bandCount(), 5); QCOMPARE(writes.count(), 1);
        dlg.findChild<QPushButton*>("TxEqUndoBtn")->click(); QCOMPARE(graph->saveEditState(), before);
    }

    void liveOffCommitsOnRelease()
    {
        RadioModel rm; TxEqDialog dlg(&rm);
        auto* slider = dlg.findChild<QSlider*>("TxEqWidthSlider"); QVERIFY(slider);
        QMetaObject::invokeMethod(&dlg, "onLegacyToggled", Q_ARG(bool, false)); dlg.show();
        dlg.parametricWidget()->setSelectedIndex(3);
        QSignalSpy writes(&rm.transmitModel(), &TransmitModel::txEqParaEqDataChanged);
        QStyleOptionSlider option; option.initFrom(slider); option.orientation = Qt::Horizontal;
        option.minimum = slider->minimum(); option.maximum = slider->maximum();
        option.sliderPosition = slider->value(); option.sliderValue = slider->value();
        const QPoint handle = slider->style()->subControlRect(QStyle::CC_Slider, &option, QStyle::SC_SliderHandle, slider).center();
        QTest::mousePress(slider, Qt::LeftButton, Qt::NoModifier, handle);
        QTest::mouseMove(slider, handle + QPoint(-35, 0)); QCOMPARE(writes.count(), 0);
        QTest::mouseRelease(slider, Qt::LeftButton, Qt::NoModifier, handle + QPoint(-35, 0));
        QCOMPARE(writes.count(), 1);
    }

    void profileSwitchDuringDragRebasesHistory()
    {
        RadioModel rm; TxEqDialog dlg(&rm);
        auto* undo = dlg.findChild<QPushButton*>("TxEqUndoBtn"); QVERIFY(undo);
        QMetaObject::invokeMethod(&dlg, "onLegacyToggled", Q_ARG(bool, false)); dlg.show();
        auto* graph = dlg.parametricWidget(); const QRect plot = graph->plotRect().toRect();
        const auto point = graph->points()[4];
        const auto before = graph->saveEditState();
        QPoint start(plot.left() + qRound(point.frequencyHz / graph->frequencyMaxHz() * plot.width()), plot.center().y());
        QTest::mousePress(graph, Qt::LeftButton, Qt::NoModifier, start);
        QTest::mouseMove(graph, start + QPoint(0, -30));
        QVERIFY(graph->saveEditState() != before);
        ParametricEqWidget saved; saved.setGlobalGainDb(7.0);
        const QString blob = ParaEqEnvelope::encode(saved.saveToJson());
        rm.transmitModel().setTxEqParaEqData(blob);
        const auto loaded = graph->saveEditState();
        QTest::mouseRelease(graph, Qt::LeftButton, Qt::NoModifier, start + QPoint(0, -30));
        QCOMPARE(graph->saveEditState(), loaded); QCOMPARE(rm.transmitModel().txEqParaEqData(), blob);
        QVERIFY(!undo->isEnabled());
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

        QVERIFY(dlg.modeSelector());
        QCOMPARE(dlg.usingLegacyEq(), true);
        QVERIFY(dlg.modeSelector()->button(0));
        QVERIFY(dlg.modeSelector()->button(1));
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
        QCOMPARE(dlg.usingLegacyEq(), true);
        QCOMPARE(stack->currentWidget(), dlg.legacyPanel());

        // Uncheck — parametric panel shown.
        dlg.modeSelector()->button(1)->click();
        QCOMPARE(stack->currentWidget(), dlg.parametricPanel());

        // Re-check — legacy panel shown.
        dlg.modeSelector()->button(0)->click();
        QCOMPARE(stack->currentWidget(), dlg.legacyPanel());
    }

    // ── 11b. chkLegacyEQ state persists in AppSettings ──────────────
    void modeSelectorStateFollowsTransmitModel()
    {
        RadioModel rm;
        {
            TxEqDialog dlg(&rm);
            QCOMPARE(dlg.usingLegacyEq(), true);
            dlg.modeSelector()->button(1)->click();
            // Persistence is synchronous via setValue.
        }

        // Reconstruct — the new dialog should pick up the persisted False.
        {
            TxEqDialog dlg2(&rm);
            QCOMPARE(dlg2.usingLegacyEq(), false);
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
        // Limits match Thetis ucParametricEq1 widget property block
        // at eqform.cs:928-967 [v2.10.3.13]. The range is the curve the
        // model's empty txEqParaEqData stands for: GetDefaults' 0 to 4000
        // Hz, as Thetis's ParaEQTXData setter loads it
        // (eqform.cs:3312-3317 [v2.10.3.15]), not the designer's 2700.
        QCOMPARE(w->dbMin(),         -24.0);
        QCOMPARE(w->dbMax(),          24.0);
        QCOMPARE(w->frequencyMinHz(),  0.0);
        QCOMPARE(w->frequencyMaxHz(), 4000.0);
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
    //         AND leaves legacy scalar fields untouched.
    void parametricEditEncodesEnvelopeAndPreservesLegacyScalars()
    {
        RadioModel rm;
        TransmitModel& tx = rm.transmitModel();

        // Capture legacy field defaults BEFORE the dialog constructs,
        // since TransmitModel's defaults aren't necessarily zero
        // (txEqBand defaults to -12 at the slider min, for example).
        const int defaultPreamp = tx.txEqPreamp();
        std::array<int, 10> defaultBands;
        for (int i = 0; i < 10; ++i) {
            defaultBands[i] = tx.txEqBand(i);
        }

        TxEqDialog dlg(&rm);
        ParametricEqWidget* w = dlg.parametricWidget();
        QVERIFY(w);

        dlg.modeSelector()->button(1)->click();
        w->setGlobalGainDb(3.0);

        // 1. Blob is encoded (gzip+base64url envelope, not raw JSON) and
        //    decodes back to JSON containing the new global gain --
        //    Codex P1 #1 envelope-encoding fix from f5b24ef.
        const QString blob = tx.txEqParaEqData();
        QVERIFY(!blob.isEmpty());
        QVERIFY(!blob.trimmed().startsWith(QLatin1Char('{')));

        const std::optional<QString> decoded = ParaEqEnvelope::decode(blob);
        QVERIFY(decoded.has_value());
        QVERIFY(decoded->contains(QStringLiteral("\"global_gain_db\": 3")));

        // 2. Legacy scalar fields stay at their pre-edit values --
        //    parametric edits push the curve directly to WDSP via
        //    TxChannel::setTxEqProfile (see 9e6de26 commit message), NOT
        //    via the legacy txEqPreamp/txEqBand setter chain.  Mutating
        //    legacy scalars here would corrupt the user's legacy-mode
        //    settings on toggle-back to chkLegacyEQ.  The earlier
        //    dd03b70 approach DID push to legacy scalars and lost
        //    parametric precision (4.6 dB rounded to 5) plus discarded
        //    parametric band centers (sampled at the legacy ISO grid).
        QCOMPARE(tx.txEqPreamp(), defaultPreamp);
        for (int i = 0; i < 10; ++i) {
            QCOMPARE(tx.txEqBand(i), defaultBands[i]);
        }
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

    // ── 18. R-IOS-13 / R-R3-49: the curve on the link is the curve the
    //        dialog shows (transmit.txEqCurve, ParaEqCurve::txEqCurveJson)
    //        for every factory profile, a custom curve, an out-of-order
    //        curve, and says "unavailable" where the panel falls back.
    void curveMatchesDialogForEveryFactoryProfile()
    {
        RadioModel rm;
        TxEqDialog dlg(&rm);
        TransmitModel& tx = rm.transmitModel();
        ParametricEqWidget* w = dlg.parametricWidget();
        QVERIFY(w);

        MicProfileManager mgr;
        mgr.setMacAddress(QStringLiteral("aa:bb:cc:dd:ee:18"));
        mgr.load();
        const QStringList names = mgr.profileNames();
        QVERIFY(names.size() >= 22);
        for (const QString& name : names) {
            // A custom curve first, so each profile's value has to move
            // the dialog.
            tx.setTxEqParaEqData(ParaEqEnvelope::encode(customCurveJson()));
            QVERIFY(mgr.setActiveProfile(name, &tx));
            // Every factory profile saves an empty TXParaEQData
            // (database.cs AddTXProfileTable [v2.10.3.15]).
            QCOMPARE(tx.txEqParaEqData(), QString());
            const QJsonObject curve = curveOf(tx);
            QCOMPARE(curve.value(QStringLiteral("state")).toString(), QStringLiteral("default"));
            compareToWidget(curve, w);
            if (QTest::currentTestFailed()) {
                qWarning() << "profile" << name;
                return;
            }
        }
    }

    void curveMatchesDialogForACustomCurve()
    {
        RadioModel rm;
        TxEqDialog dlg(&rm);
        TransmitModel& tx = rm.transmitModel();
        tx.setTxEqParaEqData(ParaEqEnvelope::encode(customCurveJson()));
        const QJsonObject curve = curveOf(tx);
        QCOMPARE(curve.value(QStringLiteral("state")).toString(), QStringLiteral("saved"));
        QCOMPARE(curve.value(QStringLiteral("points")).toArray().size(), 5);
        compareToWidget(curve, dlg.parametricWidget());
        // The panel's controls follow the loaded curve (setParaEQData).
        QCOMPARE(dlg.findChild<QDoubleSpinBox*>(QStringLiteral("TxEqParaLowSpin"))->value(), 80);
        QCOMPARE(dlg.findChild<QDoubleSpinBox*>(QStringLiteral("TxEqParaHighSpin"))->value(), 3100);
        QCOMPARE(dlg.findChild<QRadioButton*>(QStringLiteral("TxEqParaBands5Radio"))->isChecked(),
                 true);
        QCOMPARE(dlg.findChild<QCheckBox*>(QStringLiteral("TxEqParaUseQFactorsChk"))->isChecked(),
                 false);
        // Loading moved nothing back into the model.
        QCOMPARE(tx.txEqParaEqData(), ParaEqEnvelope::encode(customCurveJson()));
    }

    void curveMatchesDialogForAnOutOfOrderCurve()
    {
        RadioModel rm;
        TxEqDialog dlg(&rm);
        TransmitModel& tx = rm.transmitModel();
        // Saved out of order, two points 2 Hz apart, an 18-band count:
        // the panel sorts and spaces them (enforceOrdering).
        QJsonArray pts;
        for (int i = 0; i < 18; ++i) {
            const double f = i == 5 ? 902.0 : i == 6 ? 900.0 : 150.0 * i;
            pts.append(QJsonObject{{QStringLiteral("frequency_hz"), f},
                                   {QStringLiteral("gain_db"), (i % 5) - 2.0},
                                   {QStringLiteral("q"), 1.0 + i}});
        }
        const QJsonObject root{{QStringLiteral("band_count"), 18},
                               {QStringLiteral("parametric_eq"), true},
                               {QStringLiteral("global_gain_db"), 1.5},
                               {QStringLiteral("frequency_min_hz"), 0.0},
                               {QStringLiteral("frequency_max_hz"), 2550.0},
                               {QStringLiteral("points"), pts}};
        tx.setTxEqParaEqData(ParaEqEnvelope::encode(
            QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact))));
        const QJsonObject curve = curveOf(tx);
        QCOMPARE(curve.value(QStringLiteral("state")).toString(), QStringLiteral("saved"));
        const QJsonArray shown = curve.value(QStringLiteral("points")).toArray();
        QCOMPARE(shown.at(5).toObject().value(QStringLiteral("frequencyHz")).toDouble(), 900.0);
        QCOMPARE(shown.at(6).toObject().value(QStringLiteral("frequencyHz")).toDouble(), 905.0);
        compareToWidget(curve, dlg.parametricWidget());
        QCOMPARE(dlg.findChild<QRadioButton*>(QStringLiteral("TxEqParaBands18Radio"))->isChecked(),
                 true);
    }

    // Odd saved values: the panel and the Core's curve read them by one
    // parser (ParaEqCurve), so they agree on rounding, missing fields and
    // out-of-range points. Each value is loaded after a different curve,
    // so nothing is left over from the widget's earlier state.
    void curveMatchesDialogForOddValues()
    {
        RadioModel rm;
        TxEqDialog dlg(&rm);
        TransmitModel& tx = rm.transmitModel();
        const QString odd[] = {
            // Unrounded: PointsFromJson rounds F to 0.001 Hz, G and the
            // preamp to 0.1 dB, Q to 0.01.
            QStringLiteral(R"({"band_count":5,"parametric_eq":true,"global_gain_db":2.345,)"
                           R"("frequency_min_hz":20.00049,"frequency_max_hz":3333.3333,)"
                           R"("points":[{"frequency_hz":20,"gain_db":1.26,"q":7.777},)"
                           R"({"frequency_hz":500.0004,"gain_db":-3.14159,"q":1.234},)"
                           R"({"frequency_hz":1234.5678,"gain_db":0.05,"q":2.005},)"
                           R"({"frequency_hz":2000.0006,"gain_db":0.15,"q":3.335},)"
                           R"({"frequency_hz":3333,"gain_db":-0.25,"q":9.999}]})"),
            // Missing fields: no frequency_min_hz (0), parametric_eq,
            // band_count or global_gain_db; a point with only a frequency.
            QStringLiteral(R"({"frequency_max_hz":1800,)"
                           R"("points":[{"frequency_hz":100,"gain_db":3,"q":2},)"
                           R"({"frequency_hz":700},)"
                           R"({"gain_db":-5,"q":4},)"
                           R"({"frequency_hz":1800,"gain_db":1,"q":1}]})"),
            // Out of range: a point above the range, one below, gains and
            // Qs past the panel's limits, the preamp too.
            QStringLiteral(R"({"band_count":5,"parametric_eq":true,"global_gain_db":50,)"
                           R"("frequency_min_hz":100,"frequency_max_hz":2600,)"
                           R"("points":[{"frequency_hz":100,"gain_db":40,"q":0},)"
                           R"({"frequency_hz":9000,"gain_db":-40,"q":99},)"
                           R"({"frequency_hz":-50,"gain_db":12,"q":0.1},)"
                           R"({"frequency_hz":1300,"gain_db":24.04,"q":20.004},)"
                           R"({"frequency_hz":2600,"gain_db":-24.06,"q":0.199}]})"),
            // A range wholly above the panel's previous one.
            QStringLiteral(R"({"band_count":3,"parametric_eq":true,"global_gain_db":0,)"
                           R"("frequency_min_hz":5000,"frequency_max_hz":9000,)"
                           R"("points":[{"frequency_hz":5000,"gain_db":1,"q":1},)"
                           R"({"frequency_hz":7000,"gain_db":2,"q":2},)"
                           R"({"frequency_hz":9000,"gain_db":3,"q":3}]})"),
        };
        for (const QString& json : odd) {
            tx.setTxEqParaEqData(ParaEqEnvelope::encode(customCurveJson()));
            tx.setTxEqParaEqData(ParaEqEnvelope::encode(json));
            const QJsonObject curve = curveOf(tx);
            QCOMPARE(curve.value(QStringLiteral("state")).toString(), QStringLiteral("saved"));
            compareToWidget(curve, dlg.parametricWidget());
            if (QTest::currentTestFailed()) {
                qWarning() << json;
                return;
            }
        }
        // Spot checks on the last three, worked from PointsFromJson.
        tx.setTxEqParaEqData(ParaEqEnvelope::encode(odd[0]));
        QJsonObject c = curveOf(tx);
        QCOMPARE(c.value(QStringLiteral("preampDb")).toDouble(), 2.3);
        QCOMPARE(c.value(QStringLiteral("minHz")).toDouble(), 20.0);
        QCOMPARE(c.value(QStringLiteral("points")).toArray().at(1).toObject()
                     .value(QStringLiteral("gainDb")).toDouble(), -3.1);
        tx.setTxEqParaEqData(ParaEqEnvelope::encode(odd[1]));
        c = curveOf(tx);
        QCOMPARE(c.value(QStringLiteral("minHz")).toDouble(), 0.0);
        QCOMPARE(c.value(QStringLiteral("parametric")).toBool(), false);
        // The point with no frequency reads 0 Hz and sorts second, moved
        // to 5 Hz; the one with only a frequency (700 Hz) is third, gain
        // 0 and Q 0 clamped to 0.2.
        QCOMPARE(c.value(QStringLiteral("points")).toArray().at(1).toObject()
                     .value(QStringLiteral("frequencyHz")).toDouble(), 5.0);
        QCOMPARE(c.value(QStringLiteral("points")).toArray().at(2).toObject()
                     .value(QStringLiteral("q")).toDouble(), 0.2);
        tx.setTxEqParaEqData(ParaEqEnvelope::encode(odd[2]));
        c = curveOf(tx);
        QCOMPARE(c.value(QStringLiteral("preampDb")).toDouble(), 24.0);
        QCOMPARE(c.value(QStringLiteral("points")).toArray().at(0).toObject()
                     .value(QStringLiteral("gainDb")).toDouble(), 24.0);
        QCOMPARE(dlg.parametricWidget()->frequencyMinHz(), 100.0);
        tx.setTxEqParaEqData(ParaEqEnvelope::encode(odd[3]));
        QCOMPARE(dlg.parametricWidget()->frequencyMinHz(), 5000.0);
        QCOMPARE(dlg.parametricWidget()->frequencyMaxHz(), 9000.0);
    }

    void unreadableCurveIsUnavailableAndDialogShowsTheCoresFallback()
    {
        RadioModel rm;
        TxEqDialog dlg(&rm);
        TransmitModel& tx = rm.transmitModel();
        tx.setTxEqParaEqData(ParaEqEnvelope::encode(customCurveJson()));
        tx.setTxEqParaEqData(QStringLiteral("not a curve"));
        QCOMPARE(tx.txEqCurve(), QStringLiteral("{\"state\":\"unavailable\"}"));
        // The panel shows what the Core applies in its place, Thetis's
        // GetDefaults (eqform.cs:3312-3315 [v2.10.3.15]), not the
        // previous curve.
        compareToWidget(QJsonDocument::fromJson(
                            ParaEqCurve::txEqCurveJson(QString()).toUtf8()).object(),
                        dlg.parametricWidget());
    }

    // ── 16d. Low / High spread guard ───────────────────────────────
    // From Thetis eqform.cs:3539-3577 [v2.10.3.15]: a Low within 1000 Hz
    // of High is set to High - 1000 with the handler still attached, so
    // ValueChanged re-fires and the clamped value reaches the curve
    // (FrequencyMinHz), whose PointsChanged stores the rescaled points.
    void lowWithinSpreadClampsAndReachesCurve()
    {
        RadioModel rm;
        TxEqDialog dlg(&rm);
        dlg.modeSelector()->button(1)->click();
        TransmitModel& tx = rm.transmitModel();
        ParametricEqWidget* w = dlg.parametricWidget();
        auto* low  = dlg.findChild<QDoubleSpinBox*>(QStringLiteral("TxEqParaLowSpin"));
        auto* high = dlg.findChild<QDoubleSpinBox*>(QStringLiteral("TxEqParaHighSpin"));
        QVERIFY(w && low && high);
        high->setValue(3000);
        QCOMPARE(w->frequencyMaxHz(), 3000.0);

        QSignalSpy spy(&tx, &TransmitModel::txEqParaEqDataChanged);
        low->setValue(2500);
        QCOMPARE(low->value(), 2000);
        QCOMPARE(w->frequencyMinHz(), 2000.0);
        QCOMPARE(spy.count(), 1);
        const QJsonObject curve = curveOf(tx);
        QCOMPARE(curve.value(QStringLiteral("minHz")).toDouble(), 2000.0);
        compareToWidget(curve, w, kSavedFreqToleranceHz);
    }

    void highWithinSpreadClampsAndReachesCurve()
    {
        RadioModel rm;
        TxEqDialog dlg(&rm);
        dlg.modeSelector()->button(1)->click();
        TransmitModel& tx = rm.transmitModel();
        ParametricEqWidget* w = dlg.parametricWidget();
        auto* low  = dlg.findChild<QDoubleSpinBox*>(QStringLiteral("TxEqParaLowSpin"));
        auto* high = dlg.findChild<QDoubleSpinBox*>(QStringLiteral("TxEqParaHighSpin"));
        QVERIFY(w && low && high);
        low->setValue(1500);
        QCOMPARE(w->frequencyMinHz(), 1500.0);

        QSignalSpy spy(&tx, &TransmitModel::txEqParaEqDataChanged);
        high->setValue(2000);
        QCOMPARE(high->value(), 2500);
        QCOMPARE(w->frequencyMaxHz(), 2500.0);
        QCOMPARE(spy.count(), 1);
        const QJsonObject curve = curveOf(tx);
        QCOMPARE(curve.value(QStringLiteral("maxHz")).toDouble(), 2500.0);
        compareToWidget(curve, w, kSavedFreqToleranceHz);
    }

    // A Low or High outside the spread moves the curve's range, rescales
    // its points (ucParametricEq.cs:606-645 [v2.10.3.15], FrequencyMinHz /
    // FrequencyMaxHz), and
    // the rescaled points reach the model once (eqform.cs:3197-3213
    // [v2.10.3.15], ucParametricEq1_PointsChanged).
    void lowHighRescaleReachesModelOnce()
    {
        RadioModel rm;
        TxEqDialog dlg(&rm);
        dlg.modeSelector()->button(1)->click();
        TransmitModel& tx = rm.transmitModel();
        ParametricEqWidget* w = dlg.parametricWidget();
        auto* low  = dlg.findChild<QDoubleSpinBox*>(QStringLiteral("TxEqParaLowSpin"));
        auto* high = dlg.findChild<QDoubleSpinBox*>(QStringLiteral("TxEqParaHighSpin"));
        QVERIFY(w && low && high);

        QSignalSpy spy(&tx, &TransmitModel::txEqParaEqDataChanged);
        high->setValue(3000);
        QCOMPARE(spy.count(), 1);
        QJsonObject curve = curveOf(tx);
        QCOMPARE(curve.value(QStringLiteral("maxHz")).toDouble(), 3000.0);
        compareToWidget(curve, w, kSavedFreqToleranceHz);

        low->setValue(200);
        QCOMPARE(spy.count(), 2);
        curve = curveOf(tx);
        QCOMPARE(curve.value(QStringLiteral("minHz")).toDouble(), 200.0);
        compareToWidget(curve, w, kSavedFreqToleranceHz);
        // The spin boxes still show the curve's range.
        QCOMPARE(low->value(), 200);
        QCOMPARE(high->value(), 3000);
    }

    // Typed input: Thetis's udParaEQ_low / udParaEQ_high are NumericUpDowns
    // (NumericUpDownTS overrides no text handling, numericupdownts.cs:33),
    // which raise ValueChanged only when the typed text is committed, so
    // eqform.cs:3539-3577 [v2.10.3.15] runs once per typed value. Typing
    // "3000" into High must not clamp at the "3" or push per keystroke.
    void typedHighCommitsOnEnterOnce()
    {
        RadioModel rm;
        TxEqDialog dlg(&rm);
        TransmitModel& tx = rm.transmitModel();
        dlg.modeSelector()->button(1)->click();
        dlg.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dlg));
        ParametricEqWidget* w = dlg.parametricWidget();
        auto* low  = dlg.findChild<QDoubleSpinBox*>(QStringLiteral("TxEqParaLowSpin"));
        auto* high = dlg.findChild<QDoubleSpinBox*>(QStringLiteral("TxEqParaHighSpin"));
        QVERIFY(w && low && high);
        QCOMPARE(low->value(), 0);

        QSignalSpy spy(&tx, &TransmitModel::txEqParaEqDataChanged);
        high->setFocus();
        high->selectAll();
        QTest::keyClicks(high, QStringLiteral("3000"));
        QCOMPARE(spy.count(), 0);
        QTest::keyClick(high, Qt::Key_Return);
        QCOMPARE(high->value(), 3000);
        QCOMPARE(low->value(), 0);
        QCOMPARE(w->frequencyMaxHz(), 3000.0);
        QCOMPARE(spy.count(), 1);
        compareToWidget(curveOf(tx), w, kSavedFreqToleranceHz);
    }

    // The same, committed by leaving the box with Tab.
    void typedLowCommitsOnTabOnce()
    {
        RadioModel rm;
        TxEqDialog dlg(&rm);
        TransmitModel& tx = rm.transmitModel();
        dlg.modeSelector()->button(1)->click();
        dlg.show();
        dlg.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&dlg));
        ParametricEqWidget* w = dlg.parametricWidget();
        auto* low  = dlg.findChild<QDoubleSpinBox*>(QStringLiteral("TxEqParaLowSpin"));
        auto* high = dlg.findChild<QDoubleSpinBox*>(QStringLiteral("TxEqParaHighSpin"));
        QVERIFY(w && low && high);

        QSignalSpy spy(&tx, &TransmitModel::txEqParaEqDataChanged);
        low->setFocus();
        QVERIFY(low->hasFocus());
        low->selectAll();
        QTest::keyClicks(low, QStringLiteral("500"));
        QCOMPARE(spy.count(), 0);
        QTest::keyClick(low, Qt::Key_Tab);
        QVERIFY(!low->hasFocus());
        QCOMPARE(low->value(), 500);
        QCOMPARE(high->value(), 4000);
        QCOMPARE(w->frequencyMinHz(), 500.0);
        QCOMPARE(spy.count(), 1);
        compareToWidget(curveOf(tx), w, kSavedFreqToleranceHz);
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
        QVERIFY(dlg.modeSelector());
    }
};

QTEST_MAIN(TestTxEqDialog)
#include "tst_tx_eq_dialog.moc"
