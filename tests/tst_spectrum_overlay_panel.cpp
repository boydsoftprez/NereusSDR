// =================================================================
// tests/tst_spectrum_overlay_panel.cpp  (NereusSDR)
// =================================================================
//
// Smoke tests for SpectrumOverlayPanel::setRadioModel() — Phase 3O
// Sub-Phase 9 Task 9.2c (issue #70 fold-in).
//
// Coverage: the VAX Ch combo on the left-edge overlay is wired
// bidirectionally to slice 0's vaxChannel() with echo prevention.
// The IQ Ch combo stays disabled (feature-flagged per design spec
// §6.7/§11.3).
//
// 2026-09-25 (R-IOS-27, R-IOS-06): the BAND flyout draws the same twelve
// buttons, in the same places, and emits the same bandSelected arguments
// now that its table lives in models/BandGrid.h. J.J. Boyd (KG4VCF),
// AI-assisted via Anthropic Claude Code.
//
// 2026-09-30 (TX rulings, item 3): the ATT flyout is held with the reason
// on a pan whose slice this window only listens to, and writes nothing.
// J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//
// 2026-10-10 (display pill plan Task 16): the Display pill. Four tabs in
// order, each with its header and footer; the Spectrum and 3D View rows
// seeded from a real SpectrumWidget with nothing written, following it
// with no echo, and writing it once per change; the rows that cannot act
// greyed and still shown; the colour choosers opened on the pan's colours;
// the band's dB Max and Min; the background rows; reset; the wheel
// swallowed; the last tab kept; the Spectrum tab's size.
// J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//
// 2026-10-10 (display pill plan Task 17): the Waterfall and FFT tabs. Rows
// in the design's order with their ranges; the scheme list; Black Level
// and Color Gain as offset and span; the rows Clarity, AGC and "Use
// spectrum min/max" grey, with their notes; the NF-AGC offset in tenths;
// NB Blank; the Clarity row; FFT Size asking on release and once after
// keys; the readout; Hz/bin; the station and old-Core gates; every
// Waterfall row seeded, followed and written once; each tab's size.
// J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//
// 2026-10-14: align the tall-pan geometry and wheel assertions with the
// contained scrolling Display pill used by short docked pans. J.J. Boyd
// (KG4VCF), AI-assisted via OpenAI Codex.
//
// 2026-10-15: carry two checks from Combined K (55ce07963, 49ce1a80d)
// onto the contained-scroller rule: each marker pill's text fits its
// styled content rectangle, not only its outer width, and every tab's
// pill stays inside the pan. J.J. Boyd (KG4VCF), AI-assisted via
// Anthropic Claude Code.
//
// 2026-10-15: whenPeakValueWrapsTheKeyedSpectrumTabStillFits steps the
// letter spacing up from 1.5 px, at most 13 steps, until the four marker
// pills overflow their line, as Combined K (fe27683af) does; a fixed
// 1.5 px did not force the wrap on every platform font. J.J. Boyd
// (KG4VCF), AI-assisted via Anthropic Claude Code.
//
// 2026-10-16: "Setup defaults" names the Setup Display pages as the tree
// shows them, and the FFT tab's opens FFT & Engine. J.J. Boyd (KG4VCF),
// AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QEnterEvent>
#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QSpinBox>
#include <QComboBox>
#include <QGridLayout>
#include <QImage>
#include <QDir>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QSlider>
#include <QStyle>
#include <QStyleOptionButton>
#include <QStyleOptionSpinBox>
#include <QTemporaryDir>
#include <QTimer>
#include <QWheelEvent>

#include "OperatorWording.h"
#include "core/AppSettings.h"
#include "core/BoardCapabilities.h"
#include "core/ControlRanges.h"
#include "core/StepAttenuatorFacade.h"
#include "core/spectrum/WaterfallPalettes.h"
#include "gui/SpectrumOverlayPanel.h"
#include "gui/SpectrumWidget.h"
#include "gui/styles/WorkspacePreview.h"
#include "models/DisplaySettingsModel.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

class TstSpectrumOverlayPanel : public QObject {
    Q_OBJECT

private:
    // SpectrumOverlayPanel parents its flyouts to `parentWidget()` (the
    // host SpectrumWidget in production — a bare QWidget in these tests).
    // Give it a host parent and search from there so findChild reaches the
    // combo's setObjectName("vaxCombo") in buildVaxFlyout.
    struct PanelHarness {
        QWidget host;
        SpectrumOverlayPanel* panel{nullptr};
        PanelHarness() {
            panel = new SpectrumOverlayPanel(&host);
        }
    };

    QComboBox* vaxCombo(PanelHarness& h) {
        return h.host.findChild<QComboBox*>(QStringLiteral("vaxCombo"));
    }

    QComboBox* rxAntennaCombo(PanelHarness& h) {
        return h.host.findChild<QComboBox*>(QStringLiteral("m_rxAntCmb"));
    }

    QComboBox* txAntennaCombo(PanelHarness& h) {
        return h.host.findChild<QComboBox*>(QStringLiteral("m_txAntCmb"));
    }

    // Display pill plan Task 16: the strip on a real SpectrumWidget, which
    // is the pill's target from the start, as it is in the window.
    using Tab = SpectrumOverlayPanel::DisplayPillTab;
    struct PillHarness {
        SpectrumWidget pan;
        SpectrumOverlayPanel* panel{nullptr};
        PillHarness() {
            pan.resize(900, 600);
            // MainWindow loads the renderer before binding its Display UI.
            pan.loadSettings();
            panel = new SpectrumOverlayPanel(&pan);
        }
        QWidget* pill() {
            return pan.findChild<QWidget*>(QStringLiteral("displayPill"));
        }
        QWidget* page(const char* name) {
            return pill()->findChild<QWidget*>(QString::fromLatin1(name));
        }
        QScrollArea* scroll() {
            return pill()->findChild<QScrollArea*>(QStringLiteral("displayPillScroll"));
        }
        // The pan is never shown, so nothing lays the pill's rows out:
        // do it by hand, parents first, before reading a child's place.
        void layOut() {
            if (QLayout* layout = pill()->layout()) {
                layout->setGeometry(pill()->rect());
            }
            const QList<QWidget*> children = pill()->findChildren<QWidget*>();
            for (QWidget* child : children) {
                if (QLayout* layout = child->layout()) {
                    layout->setGeometry(child->rect());
                }
            }
        }
        template <typename T>
        T* control(const char* name) {
            return pill()->findChild<T*>(QString::fromLatin1(name));
        }
        QPushButton* toggle(const char* name) { return control<QPushButton>(name); }
        QSlider* slider(const char* name) { return control<QSlider>(name); }
        QComboBox* combo(const char* name) { return control<QComboBox>(name); }
        // The number box of an averaging time row.
        QSpinBox* box(const char* name) {
            return pill()->findChild<QSpinBox*>(
                QString::fromLatin1(name) + QStringLiteral("Box"));
        }
        QString valueText(const char* name) {
            auto* label = pill()->findChild<QLabel*>(
                QString::fromLatin1(name) + QStringLiteral("Value"));
            return label ? label->text() : QStringLiteral("<no value label>");
        }
    };

    // The grid row a named control sits on (a pill inside a line counts as
    // the line's row), or -1.
    static int gridRowOf(QWidget* page, const QString& name) {
        auto* grid = qobject_cast<QGridLayout*>(page->layout());
        QWidget* widget = page->findChild<QWidget*>(name);
        if (!grid || !widget) {
            return -1;
        }
        while (widget->parentWidget() && widget->parentWidget() != page) {
            widget = widget->parentWidget();
        }
        const int index = grid->indexOf(widget);
        if (index < 0) {
            return -1;
        }
        int row = -1;
        int column = 0;
        int rowSpan = 0;
        int columnSpan = 0;
        grid->getItemPosition(index, &row, &column, &rowSpan, &columnSpan);
        return row;
    }

    // The rectangle a styled push button leaves for its text, inside the
    // style's padding.
    static QRect pushButtonContents(const QPushButton* button) {
        QStyleOptionButton option;
        option.initFrom(button);
        option.text = button->text();
        option.icon = button->icon();
        option.iconSize = button->iconSize();
        return button->style()->subElementRect(
            QStyle::SE_PushButtonContents, &option, button);
    }

    // Every setting the Spectrum and 3D View tabs show, away from its
    // default.
    static void setEveryRowAwayFromDefault(SpectrumWidget& pan) {
        pan.setPanFillEnabled(false);
        pan.setGridEnabled(false);
        pan.setCursorFreqVisible(false);
        pan.setPeakHoldEnabled(true);
        pan.setFillColor(QColor(QStringLiteral("#123456")));
        pan.setFillAlpha(0.35f);
        pan.setGradientEnabled(true);
        pan.setShowNoiseFloor(true);
        pan.setShowBinWidth(true);
        pan.setShowPeakValueOverlay(true);
        pan.setTraceColor(QColor(QStringLiteral("#abcdef")));
        pan.setLineWidth(2.5f);
        pan.setSpectrumDetector(static_cast<SpectrumDetector>(3));    // Sample
        pan.setSpectrumAveraging(SpectrumAveraging::Recursive);
        pan.setSpectrumAverageTimeMs(650);
        pan.setDbmCalOffset(-4.5f);
        pan.setPeakHoldDelayMs(4200);
        pan.setFreqGridSpacingHz(25000);
        pan.setFreqScaleFontPx(13);
        pan.setBackgroundEnabled(true);
        pan.setBackgroundOpacity(42);
        pan.setBackgroundFillColor(QColor(QStringLiteral("#201008")));
        pan.setSpectrumRenderMode(1);
        pan.setDssFloorDepth(11);
        pan.setDssGain(33);
        pan.setDssRowSpan(44);
        pan.setDssAngle(77);
        pan.setDssRowDivider(4);
        pan.setThreeDSliceDepth(true);
    }

    // The signals a write to the pan would raise: the announce, the two
    // fill signals on its model, and the 3D ones.
    struct WriteSpies {
        QSignalSpy announced;
        QSignalSpy panFill;
        QSignalSpy fillAlpha;
        QSignalSpy renderMode;
        QSignalSpy floor;
        QSignalSpy gain;
        QSignalSpy span;
        QSignalSpy angle;
        QSignalSpy speed;
        QSignalSpy shadow;
        QSignalSpy peakHold;
        explicit WriteSpies(SpectrumWidget& pan)
            : announced(&pan, &SpectrumWidget::displaySettingChanged)
            , panFill(pan.displaySettings(), &DisplaySettingsModel::panFillChanged)
            , fillAlpha(pan.displaySettings(), &DisplaySettingsModel::fillAlphaChanged)
            , renderMode(&pan, &SpectrumWidget::spectrumRenderModeChanged)
            , floor(&pan, &SpectrumWidget::dssFloorDepthChanged)
            , gain(&pan, &SpectrumWidget::dssGainChanged)
            , span(&pan, &SpectrumWidget::dssRowSpanChanged)
            , angle(&pan, &SpectrumWidget::dssAngleChanged)
            , speed(&pan, &SpectrumWidget::dssRowDividerChanged)
            , shadow(&pan, &SpectrumWidget::threeDSliceDepthChanged)
            , peakHold(&pan, &SpectrumWidget::peakHoldEnabledChanged) {}
        int total() const {
            return announced.count() + panFill.count() + fillAlpha.count() + peakHold.count()
                + renderMode.count() + floor.count() + gain.count() + span.count()
                + angle.count() + speed.count() + shadow.count();
        }
    };

    static bool sendWheel(QWidget* target) {
        const QPointF local(target->width() / 2.0, target->height() / 2.0);
        QWheelEvent wheel(local, target->mapToGlobal(local), QPoint(), QPoint(0, 120),
                          Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        wheel.ignore();
        QApplication::sendEvent(target, &wheel);
        return wheel.isAccepted();
    }

    // One of the two averaging time rows of the pill.
    struct AvgTimeRow {
        PillHarness& h;
        bool waterfall;
        AvgTimeRow(PillHarness& harness, bool isWaterfall) : h(harness), waterfall(isWaterfall) {
            // Time can be edited only while averaging is active.
            if (waterfall) {
                h.pan.setWaterfallAveraging(SpectrumAveraging::Recursive);
            }
        }
        const char* name() const {
            return waterfall ? "pillDisplayWaterfallAverageTimeMs"
                             : "pillDisplaySpectrumAverageTimeMs";
        }
        QString key() const {
            return waterfall ? QStringLiteral("DisplayWaterfallAverageTimeMs")
                             : QStringLiteral("DisplaySpectrumAverageTimeMs");
        }
        QSlider* slider() const { return h.slider(name()); }
        QSpinBox* box() const { return h.box(name()); }
        int ms() const {
            return waterfall ? h.pan.waterfallAverageTimeMs() : h.pan.spectrumAverageTimeMs();
        }
        void setMs(int value) const {
            if (waterfall) {
                h.pan.setWaterfallAverageTimeMs(value);
            } else {
                h.pan.setSpectrumAverageTimeMs(value);
            }
        }
        // How often the pan announced this row's key.
        static int announced(const QSignalSpy& spy, const QString& key) {
            int count = 0;
            for (const QList<QVariant>& call : spy) {
                count += call.at(0).toString() == key ? 1 : 0;
            }
            return count;
        }
    };

private slots:

    void init() {
        WorkspacePreview::setEnabledForTest(false);
        AppSettings::instance().clear();
    }

    // ---- The two averaging time rows: a 1 ms slider and a number box ----

    void avgTimeRows_data() {
        QTest::addColumn<bool>("waterfall");
        QTest::newRow("Avg Time") << false;
        QTest::newRow("WF Avg Time") << true;
    }

    // The slider is one position per ms from 1 to 500; the box is Thetis's
    // whole range in steps of 1 ms.
    void avgTimeSliderIsOnePositionPerMs_data() { avgTimeRows_data(); }
    void avgTimeSliderIsOnePositionPerMs() {
        QFETCH(bool, waterfall);
        PillHarness h;
        h.panel->openDisplayPill();
        const AvgTimeRow row{h, waterfall};
        QSlider* slider = row.slider();
        QSpinBox* box = row.box();
        QVERIFY(slider);
        QVERIFY(box);
        QCOMPARE(ControlRanges::kDisplayAvgTimeMinMs, 1);
        QCOMPARE(ControlRanges::kDisplayAvgTimePillMaxMs, 500);
        QCOMPARE(slider->minimum(), 1);
        QCOMPARE(slider->maximum(), 500);
        QCOMPARE(slider->singleStep(), 1);
        // A click on the groove moves it as it moves the pill's others.
        QCOMPARE(slider->pageStep(), slider->singleStep());
        QCOMPARE(h.slider("pillDisplayWfOpacity")->pageStep(),
                 h.slider("pillDisplayWfOpacity")->singleStep());

        QCOMPARE(box->minimum(), 1);
        QCOMPARE(box->maximum(), 9999);
        QCOMPARE(box->singleStep(), 1);
        QCOMPARE(box->suffix(), QStringLiteral(" ms"));
        QVERIFY(!box->keyboardTracking());
        QCOMPARE(box->toolTip(), slider->toolTip());

        // Every position is its own ms, end to end.
        for (const int ms : {1, 2, 9, 30, 31, 119, 120, 499, 500}) {
            slider->setValue(ms);
            QCOMPARE(row.ms(), ms);
            QCOMPARE(box->value(), ms);
            QCOMPARE(box->text(), QStringLiteral("%1 ms").arg(ms));
        }
        // The defaults are Thetis's.
        QCOMPARE(ControlRanges::kDisplaySpectrumAvgTimeDefaultMs, 30);
        QCOMPARE(ControlRanges::kDisplayWaterfallAvgTimeDefaultMs, 120);
        SpectrumWidget fresh;
        QCOMPARE(fresh.spectrumAverageTimeMs(), 30);
        QCOMPARE(fresh.waterfallAverageTimeMs(), 120);
    }

    // An arrow key or a wheel notch is 1 ms; Page Up and Page Down are 25.
    void avgTimeKeysAndWheelMoveOneMsAndPagesTwentyFive_data() { avgTimeRows_data(); }
    void avgTimeKeysAndWheelMoveOneMsAndPagesTwentyFive() {
        QFETCH(bool, waterfall);
        PillHarness h;
        const AvgTimeRow row{h, waterfall};
        row.setMs(30);
        h.panel->openDisplayPill();
        QSlider* slider = row.slider();
        QCOMPARE(slider->value(), 30);

        QSignalSpy announced(&h.pan, &SpectrumWidget::displaySettingChanged);
        QTest::keyClick(slider, Qt::Key_Right);
        QCOMPARE(row.ms(), 31);
        QCOMPARE(slider->value(), 31);
        QCOMPARE(row.box()->value(), 31);
        QCOMPARE(AvgTimeRow::announced(announced, row.key()), 1);
        QTest::keyClick(slider, Qt::Key_Left);
        QTest::keyClick(slider, Qt::Key_Left);
        QCOMPARE(row.ms(), 29);
        QTest::keyClick(slider, Qt::Key_Up);
        QCOMPARE(row.ms(), 30);
        QTest::keyClick(slider, Qt::Key_Down);
        QCOMPARE(row.ms(), 29);

        // One wheel notch, handed to the slider itself (the pill gives it
        // the wheel only while it has the focus, which nothing has here).
        {
            const QPointF local(slider->width() / 2.0, slider->height() / 2.0);
            QWheelEvent up(local, slider->mapToGlobal(local), QPoint(), QPoint(0, 120),
                           Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
            static_cast<QObject*>(slider)->event(&up);
            QCOMPARE(row.ms(), 30);
            QWheelEvent down(local, slider->mapToGlobal(local), QPoint(), QPoint(0, -120),
                             Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
            static_cast<QObject*>(slider)->event(&down);
            static_cast<QObject*>(slider)->event(&down);
            QCOMPARE(row.ms(), 28);
        }

        QCOMPARE(ControlRanges::kDisplayAvgTimePillPageMs, 25);
        QTest::keyClick(slider, Qt::Key_PageUp);
        QCOMPARE(row.ms(), 53);
        QCOMPARE(row.box()->value(), 53);
        QTest::keyClick(slider, Qt::Key_PageDown);
        QCOMPARE(row.ms(), 28);
        QTest::keyClick(slider, Qt::Key_PageDown);
        QCOMPARE(row.ms(), 3);
        // The ends hold.
        QTest::keyClick(slider, Qt::Key_PageDown);
        QCOMPARE(row.ms(), 1);
        QTest::keyClick(slider, Qt::Key_Left);
        QCOMPARE(row.ms(), 1);
        QTest::keyClick(slider, Qt::Key_End);
        QCOMPARE(row.ms(), 500);
        QTest::keyClick(slider, Qt::Key_PageUp);
        QCOMPARE(row.ms(), 500);
        QTest::keyClick(slider, Qt::Key_Home);
        QCOMPARE(row.ms(), 1);
    }

    // The box sets the time by a step or a typed value, over the whole
    // range, and the slider follows it.
    void avgTimeBoxSetsTheTimeAndTheSliderFollows_data() { avgTimeRows_data(); }
    void avgTimeBoxSetsTheTimeAndTheSliderFollows() {
        QFETCH(bool, waterfall);
        PillHarness h;
        const AvgTimeRow row{h, waterfall};
        row.setMs(30);
        h.panel->openDisplayPill();
        QSlider* slider = row.slider();
        QSpinBox* box = row.box();
        QCOMPARE(box->value(), 30);
        QCOMPARE(box->text(), QStringLiteral("30 ms"));

        QSignalSpy announced(&h.pan, &SpectrumWidget::displaySettingChanged);
        box->stepUp();
        QCOMPARE(row.ms(), 31);
        QCOMPARE(slider->value(), 31);
        QCOMPARE(AvgTimeRow::announced(announced, row.key()), 1);
        box->stepDown();
        box->stepDown();
        QCOMPARE(row.ms(), 29);
        QCOMPARE(slider->value(), 29);
        // The arrow keys step the box too.
        QTest::keyClick(box, Qt::Key_Up);
        QCOMPARE(row.ms(), 30);
        QTest::keyClick(box, Qt::Key_Down);
        QCOMPARE(row.ms(), 29);

        // A typed time is taken on Enter, not digit by digit.
        announced.clear();
        box->selectAll();
        QTest::keyClicks(box, QStringLiteral("250"));
        QCOMPARE(row.ms(), 29);
        QCOMPARE(AvgTimeRow::announced(announced, row.key()), 0);
        QTest::keyClick(box, Qt::Key_Return);
        QCOMPARE(row.ms(), 250);
        QCOMPARE(slider->value(), 250);
        QCOMPARE(box->text(), QStringLiteral("250 ms"));
        QCOMPARE(AvgTimeRow::announced(announced, row.key()), 1);

        // Past the slider's top: the handle is at the right end.
        box->setValue(1200);
        QCOMPARE(row.ms(), 1200);
        QCOMPARE(slider->value(), 500);
        QCOMPARE(box->value(), 1200);
        box->setValue(9999);
        QCOMPARE(row.ms(), 9999);
        QCOMPARE(slider->value(), 500);
        // The floor is 1 ms, in the box, on the slider and on the pan.
        box->setValue(1);
        QCOMPARE(row.ms(), 1);
        QCOMPARE(slider->value(), 1);
        row.setMs(0);
        QCOMPARE(row.ms(), 1);
        QCOMPARE(box->value(), 1);
    }

    // A time past the slider's top is shown and never written: not by
    // opening the pill, changing tab, closing and opening it, or pressing
    // against the right end. The slider takes over only when it is moved.
    void aStoredAvgTimeAboveTheSliderIsLeftAlone_data() { avgTimeRows_data(); }
    void aStoredAvgTimeAboveTheSliderIsLeftAlone() {
        QFETCH(bool, waterfall);
        PillHarness h;
        const AvgTimeRow row{h, waterfall};
        row.setMs(900);
        QSignalSpy announced(&h.pan, &SpectrumWidget::displaySettingChanged);

        h.panel->openDisplayPill();
        QCoreApplication::processEvents();
        QCOMPARE(row.ms(), 900);
        QCOMPARE(row.slider()->value(), 500);
        QCOMPARE(row.box()->value(), 900);
        QCOMPARE(row.box()->text(), QStringLiteral("900 ms"));

        for (const Tab tab : {Tab::Fft, Tab::Waterfall, Tab::View3D, Tab::Spectrum,
                              waterfall ? Tab::Waterfall : Tab::Spectrum}) {
            h.panel->setDisplayPillTab(tab);
            QCoreApplication::processEvents();
            QCOMPARE(row.ms(), 900);
        }
        QPushButton* att = h.panel->findChild<QPushButton*>(QStringLiteral("attMenuButton"));
        QVERIFY(att);
        att->click();
        QVERIFY(!h.panel->displayPillVisible());
        h.panel->openDisplayPill();
        QCoreApplication::processEvents();
        QCOMPARE(row.ms(), 900);
        QCOMPARE(row.slider()->value(), 500);
        QCOMPARE(row.box()->value(), 900);

        // Against the right end: the handle cannot move, so nothing is set.
        QTest::keyClick(row.slider(), Qt::Key_Right);
        QTest::keyClick(row.slider(), Qt::Key_PageUp);
        QTest::keyClick(row.slider(), Qt::Key_End);
        QCOMPARE(row.ms(), 900);
        QCOMPARE(announced.count(), 0);

        // A time set from elsewhere, also past the top.
        row.setMs(1101);
        QCOMPARE(row.box()->value(), 1101);
        QCOMPARE(row.slider()->value(), 500);
        QCOMPARE(AvgTimeRow::announced(announced, row.key()), 1);

        // Moved: the slider takes over from its top down.
        announced.clear();
        QTest::keyClick(row.slider(), Qt::Key_Left);
        QCOMPARE(row.ms(), 499);
        QCOMPARE(row.box()->value(), 499);
        QCOMPARE(AvgTimeRow::announced(announced, row.key()), 1);
    }

    // The box sits in its row: the pill keeps its width, the box ends where
    // the page's values end, "9999 ms" fits beside the step buttons, and
    // the slider keeps a usable travel.
    void avgTimeBoxFitsItsRow_data() { avgTimeRows_data(); }
    void avgTimeBoxFitsItsRow() {
        QFETCH(bool, waterfall);
        PillHarness h;
        h.panel->openDisplayPill();
        h.panel->setDisplayPillTab(waterfall ? Tab::Waterfall : Tab::Spectrum);
        h.layOut();
        const AvgTimeRow row{h, waterfall};
        QSlider* slider = row.slider();
        QSpinBox* box = row.box();
        QWidget* page = h.page(waterfall ? "displayPageWaterfall" : "displayPageSpectrum");
        QVERIFY(page);
        QCOMPARE(h.pill()->width(), 268);

        const auto inPage = [page](QWidget* widget) {
            return QRect(widget->mapTo(page, QPoint(0, 0)), widget->size());
        };
        const QRect boxRect = inPage(box);
        const QRect sliderRect = inPage(slider);
        // Another row's value on the same page ends at the same place.
        auto* otherValue = h.control<QLabel>("pillDisplayWfOpacityValue");
        if (!waterfall) {
            otherValue = h.control<QLabel>("pillDisplayCalOffsetValue");
        }
        QVERIFY(otherValue);
        QCOMPARE(boxRect.right(), inPage(otherValue).right());
        QVERIFY(boxRect.right() < page->width());
        QVERIFY(sliderRect.right() < boxRect.left());
        QVERIFY(sliderRect.left() >= inPage(h.slider(waterfall ? "pillDisplayWfOpacity"
                                                              : "pillDisplayCalOffset")).left());

        box->ensurePolished();
        QStyleOptionSpinBox option;
        option.initFrom(box);
        option.rect = box->rect();
        option.subControls = QStyle::SC_All;
        option.buttonSymbols = box->buttonSymbols();
        option.frame = box->hasFrame();
        const QRect field = box->style()->subControlRect(
            QStyle::CC_SpinBox, &option, QStyle::SC_SpinBoxEditField, box);
        auto* edit = box->findChild<QLineEdit*>();
        QVERIFY(edit);
        edit->ensurePolished();
        const int text = QFontMetrics(edit->font()).horizontalAdvance(QStringLiteral("9999 ms"));
        // A line edit keeps 2 px either side of its text.
        static constexpr int kLineEditMargin = 2;
        qInfo().noquote() << QStringLiteral("pill %1 px, slider %2 px, box %3 px, its text "
                                            "field %4 px, \"9999 ms\" %5 px")
                                 .arg(h.pill()->width()).arg(sliderRect.width())
                                 .arg(boxRect.width()).arg(field.width()).arg(text);
        QVERIFY2(text + 2 * kLineEditMargin <= field.width(),
                 qPrintable(QStringLiteral("text %1 px, field %2 px").arg(text).arg(field.width())));
        QCOMPARE(box->height(), h.combo(waterfall ? "pillDisplayWaterfallAveraging"
                                                  : "pillDisplaySpectrumAveraging")->height());
        QVERIFY2(sliderRect.width() >= 60, qPrintable(QString::number(sliderRect.width())));
    }

    // ── 1. setRadioModel enables the VAX combo once slice 0 exists ─────
    void setRadioModelEnablesVaxCombo() {
        RadioModel radio;
        radio.addSlice();  // slice 0

        PanelHarness h;
        QComboBox* combo = vaxCombo(h);
        QVERIFY(combo);
        QVERIFY(!combo->isEnabled());  // unbound → disabled
        // Fix wave M6: the unbound tooltip is in plain operator words, not
        // "not yet bound to a radio model".
        for (const QString& word : {QStringLiteral("model"), QStringLiteral("bound"),
                                    QStringLiteral("not yet")}) {
            QVERIFY2(!combo->toolTip().contains(word, Qt::CaseInsensitive),
                     qPrintable(combo->toolTip()));
        }
        QVERIFY(OperatorWording::isPlain(combo->toolTip()));

        h.panel->setRadioModel(&radio);

        QVERIFY(combo->isEnabled());
        QVERIFY(combo->toolTip().contains("VAX", Qt::CaseInsensitive));
    }

    // ── 2. User picking an index writes through to slice 0 ─────────────
    void vaxComboWritesToSlice() {
        RadioModel radio;
        radio.addSlice();
        SliceModel* s = radio.sliceById(0);
        QVERIFY(s);

        PanelHarness h;
        h.panel->setRadioModel(&radio);

        QComboBox* combo = vaxCombo(h);
        QVERIFY(combo);

        combo->setCurrentIndex(2);  // "2" → vaxChannel 2
        QCOMPARE(s->vaxChannel(), 2);

        combo->setCurrentIndex(0);  // back to Off
        QCOMPARE(s->vaxChannel(), 0);
    }

    // ── 3. Setting vaxChannel on the model echoes into the combo ───────
    void sliceUpdateEchoesToCombo() {
        RadioModel radio;
        radio.addSlice();
        SliceModel* s = radio.sliceById(0);
        QVERIFY(s);

        PanelHarness h;
        h.panel->setRadioModel(&radio);

        QComboBox* combo = vaxCombo(h);
        QVERIFY(combo);

        s->setVaxChannel(3);
        QCOMPARE(combo->currentIndex(), 3);

        s->setVaxChannel(0);
        QCOMPARE(combo->currentIndex(), 0);
    }

    // ── 4. No feedback loop on echo (exactly one emission per setVax) ──
    void noFeedbackLoopOnEcho() {
        RadioModel radio;
        radio.addSlice();
        SliceModel* s = radio.sliceById(0);
        QVERIFY(s);

        PanelHarness h;
        h.panel->setRadioModel(&radio);

        QSignalSpy spy(s, &SliceModel::vaxChannelChanged);

        s->setVaxChannel(2);

        // If the combo's currentIndexChanged re-entered into
        // setVaxChannel, we would see 2 emissions here (second one
        // would no-op at the guard in SliceModel::setVaxChannel, but
        // the sequence would still pass through the combo callback
        // twice). The m_updatingFromModel flag short-circuits the
        // widget→model path so exactly one emission reaches the spy.
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 2);
    }

    // ── 5. Removed controls are gone (R-R3-49) ─────────────────────────
    // The operator removed the RF Gain slider, the WNB button and the
    // IQ channel combo from the overlay: none is built at all.
    void removedControlsAreGone() {
        RadioModel radio;
        radio.addSlice();

        PanelHarness h;
        h.panel->setRadioModel(&radio);

        QVERIFY(h.host.findChild<QComboBox*>(QStringLiteral("vaxIqCombo")) == nullptr);
        for (QLabel* l : h.host.findChildren<QLabel*>()) {
            QVERIFY2(l->text() != QStringLiteral("RF Gain:"), "RF Gain slider label still built");
            QVERIFY2(l->text() != QStringLiteral("IQ Ch"), "IQ channel label still built");
        }
        for (QPushButton* b : h.host.findChildren<QPushButton*>()) {
            QVERIFY2(b->text() != QStringLiteral("WNB"), "WNB button still built");
            QVERIFY2(!b->toolTip().contains(QStringLiteral("RF gain"), Qt::CaseInsensitive),
                     "a button still offers RF gain");
        }
    }

    // ── 5b. No ANT button over an empty flyout (R-R3-49, R-R3-21) ──────
    // On a board with no antenna choices (Hermes Lite 2, Atlas) both ANT
    // rows hide, so the ANT button is not shown either, as an empty Setup
    // page is not offered. A board with antenna choices keeps it, and the
    // strip keeps its other buttons in both cases.
    void antButtonHiddenWhenItsFlyoutWouldBeEmpty() {
        const auto antButton = [](PanelHarness& h) -> QPushButton* {
            for (QPushButton* b : h.host.findChildren<QPushButton*>()) {
                if (b->text() == QStringLiteral("ANT")) { return b; }
            }
            return nullptr;
        };
        const auto shownButtons = [](PanelHarness& h) {
            int n = 0;
            for (QPushButton* b : h.panel->findChildren<QPushButton*>(
                     QString(), Qt::FindDirectChildrenOnly)) {
                if (!b->isHidden()) { ++n; }
            }
            return n;
        };
        struct Case { HPSDRHW board; bool antennas; const char* name; };
        const Case cases[] = {
            {HPSDRHW::HermesLite, false, "Hermes Lite 2"},
            {HPSDRHW::Atlas, false, "Atlas"},
            {HPSDRHW::OrionMKII, true, "ANAN-7000DLE"},
            {HPSDRHW::Saturn, true, "ANAN-G2"},
        };
        for (const Case& c : cases) {
            PanelHarness h;
            QPushButton* ant = antButton(h);
            QVERIFY(ant != nullptr);
            QVERIFY2(!ant->isHidden(), c.name);  // before any radio's caps
            const int before = shownButtons(h);
            const int heightBefore = h.panel->height();

            h.panel->setBoardCapabilities(BoardCapsTable::forBoard(c.board));
            QCOMPARE(!ant->isHidden(), c.antennas);
            QCOMPARE(shownButtons(h), c.antennas ? before : before - 1);
            QCOMPARE(h.panel->height() < heightBefore, !c.antennas);
            if (c.antennas) {
                QVERIFY2(rxAntennaCombo(h)->count() > 0, c.name);
            } else {
                QCOMPARE(rxAntennaCombo(h)->count(), 0);
            }
        }

        // A board change back to one with antennas brings the button back.
        PanelHarness h;
        h.panel->setBoardCapabilities(BoardCapsTable::forBoard(HPSDRHW::HermesLite));
        QVERIFY(antButton(h)->isHidden());
        h.panel->setBoardCapabilities(BoardCapsTable::forBoard(HPSDRHW::Saturn));
        QVERIFY(!antButton(h)->isHidden());
    }

    // ── 6. Removing slice 0 disables the combo; replacing it rebinds ───
    void sliceZeroReplacedRebindsCombo() {
        RadioModel radio;
        radio.addSlice();  // slice 0 (A)
        SliceModel* first = radio.sliceById(0);
        QVERIFY(first);
        first->setVaxChannel(2);

        // Phase 3F Sub-Epic C Task 7 added a "never remove the last
        // remaining slice" invariant to RadioModel::removeSlice, which this
        // test predates. Keep a second slice alive so removing slice 0
        // below is legal. It takes id 1, so sliceById(0) still returns
        // nothing once A is gone, and the re-add reclaims id 0.
        const int keeper = radio.addSlice();
        QCOMPARE(keeper, 1);

        PanelHarness h;
        h.panel->setRadioModel(&radio);

        QComboBox* combo = vaxCombo(h);
        QVERIFY(combo);
        QCOMPARE(combo->currentIndex(), 2);
        QVERIFY(combo->isEnabled());

        // Remove slice 0. The combo should reset to Off and disable.
        // `first` is now dangling — do not dereference it below.
        radio.removeSlice(0);
        QCOMPARE(combo->currentIndex(), 0);
        QVERIFY(!combo->isEnabled());

        // Add a replacement slice; the combo must rebind to the new
        // slice (Model→Widget) and route writes back (Widget→Model).
        radio.addSlice();
        SliceModel* second = radio.sliceById(0);
        QVERIFY(second);
        QVERIFY(combo->isEnabled());

        second->setVaxChannel(4);
        QCOMPARE(combo->currentIndex(), 4);

        combo->setCurrentIndex(1);
        QCOMPARE(second->vaxChannel(), 1);
    }

    // ── 7. setRadioModel before any slice exists defers the bind ───────
    void setRadioModelDefersWhenNoSlice() {
        RadioModel radio;  // no slices yet

        PanelHarness h;
        h.panel->setRadioModel(&radio);

        QComboBox* combo = vaxCombo(h);
        QVERIFY(combo);
        QVERIFY(!combo->isEnabled());  // still disabled — waiting

        // Now add slice 0 — sliceAdded should trigger the deferred bind.
        radio.addSlice();

        QVERIFY(combo->isEnabled());

        // Sanity: forward path works after the deferred bind.
        SliceModel* s = radio.sliceById(0);
        QVERIFY(s);
        combo->setCurrentIndex(4);
        QCOMPARE(s->vaxChannel(), 4);
    }

    void controls_bind_to_the_resolved_pan_slice()
    {
        RadioModel radio;
        SliceModel* first = radio.sliceById(radio.addSlice());
        SliceModel* second = radio.sliceById(radio.addSlice());
        QVERIFY(first);
        QVERIFY(second);
        first->setRxAntenna(QStringLiteral("ANT1"));
        first->setTxAntenna(QStringLiteral("ANT1"));
        first->setVaxChannel(1);
        second->setRxAntenna(QStringLiteral("ANT2"));
        second->setTxAntenna(QStringLiteral("ANT3"));
        second->setVaxChannel(4);

        PanelHarness h;
        h.panel->setSliceResolver([second]() { return second; });
        h.panel->setRadioModel(&radio);

        QComboBox* rx = rxAntennaCombo(h);
        QComboBox* tx = txAntennaCombo(h);
        QComboBox* vax = vaxCombo(h);
        QVERIFY(rx);
        QVERIFY(tx);
        QVERIFY(vax);
        QCOMPARE(rx->currentText(), QStringLiteral("ANT2"));
        QCOMPARE(tx->currentText(), QStringLiteral("ANT3"));
        QCOMPARE(vax->currentIndex(), 4);

        rx->setCurrentText(QStringLiteral("ANT3"));
        tx->setCurrentText(QStringLiteral("ANT2"));
        vax->setCurrentIndex(2);
        QCOMPARE(second->rxAntenna(), QStringLiteral("ANT3"));
        QCOMPARE(second->txAntenna(), QStringLiteral("ANT2"));
        QCOMPARE(second->vaxChannel(), 2);
        QCOMPARE(first->rxAntenna(), QStringLiteral("ANT1"));
        QCOMPARE(first->txAntenna(), QStringLiteral("ANT1"));
        QCOMPARE(first->vaxChannel(), 1);
    }

    void changing_the_pan_slice_rebinds_and_missing_slice_disables()
    {
        RadioModel radio;
        SliceModel* first = radio.sliceById(radio.addSlice());
        SliceModel* second = radio.sliceById(radio.addSlice());
        QVERIFY(first);
        QVERIFY(second);
        first->setVaxChannel(1);
        second->setVaxChannel(3);

        QPointer<SliceModel> resolved = first;
        PanelHarness h;
        h.panel->setSliceResolver([&resolved]() { return resolved.data(); });
        h.panel->setRadioModel(&radio);
        QComboBox* vax = vaxCombo(h);
        QVERIFY(vax);
        QCOMPARE(vax->currentIndex(), 1);

        resolved = second;
        h.panel->bindToPanSlice();
        QCOMPARE(vax->currentIndex(), 3);

        radio.removeSlice(second->sliceIndex());
        resolved = nullptr;
        h.panel->bindToPanSlice();
        QVERIFY(!vax->isEnabled());

        resolved = first;
        h.panel->bindToPanSlice();
        QVERIFY(vax->isEnabled());
        QCOMPARE(vax->currentIndex(), 1);
    }

    // The BAND flyout as it was drawn before its table moved to
    // models/BandGrid.h: four to a row, each emitting the name, frequency
    // and mode it always did, and 2 m after 6 m (R-IOS-26), which puts WWV
    // on a fourth row: thirteen buttons.
    void bandFlyoutDrawsAsBefore() {
        PanelHarness h;
        QGridLayout* grid = nullptr;
        for (QGridLayout* candidate : h.host.findChildren<QGridLayout*>()) {
            if (candidate->count() == 13) {
                grid = candidate;
            }
        }
        QVERIFY(grid);
        const struct {
            const char* label;
            const char* name;
            double freqHz;
            const char* mode;
        } expected[] = {
            {"160", "160m", 1.8e6, "LSB"},   {"80", "80m", 3.5e6, "LSB"},
            {"60", "60m", 5.3e6, "USB"},     {"40", "40m", 7.0e6, "LSB"},
            {"30", "30m", 10.1e6, "DIGU"},   {"20", "20m", 14.0e6, "USB"},
            {"17", "17m", 18.068e6, "USB"},  {"15", "15m", 21.0e6, "USB"},
            {"12", "12m", 24.89e6, "USB"},   {"10", "10m", 28.0e6, "USB"},
            {"6", "6m", 50.0e6, "USB"},      {"2", "2m", 144.2e6, "USB"},
            {"WWV", "WWV", 10.0e6, "AM"},
        };
        QSignalSpy selected(h.panel, &SpectrumOverlayPanel::bandSelected);
        for (int i = 0; i < 13; ++i) {
            QLayoutItem* item = grid->itemAtPosition(i / 4, i % 4);
            QVERIFY(item);
            auto* button = qobject_cast<QPushButton*>(item->widget());
            QVERIFY(button);
            QCOMPARE(button->text(), QString::fromLatin1(expected[i].label));
            QCOMPARE(button->size(), QSize(48, 26));
            button->click();
            QCOMPARE(selected.count(), i + 1);
            QCOMPARE(selected.last().at(0).toString(), QString::fromLatin1(expected[i].name));
            QCOMPARE(selected.last().at(1).toDouble(), expected[i].freqHz);
            QCOMPARE(selected.last().at(2).toString(), QString::fromLatin1(expected[i].mode));
        }
    }
    // TX rulings (item 3, JJ): a listened pan's ATT flyout is held with
    // the reason naming the controller, and writes nothing.
    void attFlyoutHeldOnAListenedSlice() {
        RadioModel remote(RadioModel::Role::Remote);
        remote.setBoardForTest(HPSDRHW::Hermes);
        remote.addSlice();
        StepAttenuatorFacade* att = remote.stepAttFacade();
        att->setWindowAvailability(true, QString());
        att->applyRemoteProperty("enabled", true);

        PanelHarness h;
        h.panel->setRadioModel(&remote);
        auto* chk = h.host.findChild<QCheckBox*>(QStringLiteral("attEnableCheck"));
        auto* spin = h.host.findChild<QSpinBox*>(QStringLiteral("attSpin"));
        auto* reason = h.host.findChild<QLabel*>(QStringLiteral("attReason"));
        QVERIFY(chk && spin && reason);
        QVERIFY(chk->isEnabled());
        QVERIFY(spin->isEnabled());

        const int dB = att->attenuationDb();
        const QString held = QStringLiteral("Shack iPad controls this slice");
        h.panel->setAttHeldReason(held);
        QVERIFY(!chk->isEnabled());
        QVERIFY(!spin->isEnabled());
        QCOMPARE(chk->toolTip(), held);
        QCOMPARE(spin->toolTip(), held);
        QCOMPARE(reason->text(), held);
        QVERIFY(!reason->isHidden());
        chk->setChecked(false);
        spin->setValue(dB == 20 ? 21 : 20);
        QVERIFY(att->enabled());
        QCOMPARE(att->attenuationDb(), dB);

        h.panel->setAttHeldReason(QString());
        QVERIFY(chk->isEnabled());
        QVERIFY(spin->isEnabled());
        QVERIFY(chk->toolTip() != held);
        QVERIFY(reason->isHidden());
    }

    // ── The Display pill (display pill plan Task 16) ────────────────────

    // JJ approved the live mockup on 2026-10-12. This is the rebuild's
    // contract, separate from the original four-tab branch's tests below.
    // Run it red before moving any controls; update the superseded layout
    // tests as the corresponding rebuild step replaces each old layout.
    void ownDotCountsPendingValuesWithoutWritingAndResetsOnlyItsRow() {
        PillHarness h;
        h.pan.setPanIndex(3);
        h.panel->setDisplayPillTab(Tab::Spectrum);
        h.panel->openDisplayPill();
        h.layOut();
        QLabel* label = h.control<QLabel>("pillDisplayLineWidthLabel");
        QVERIFY(label);
        const QPoint labelPosition = label->pos();
        const int initialFooterWidth = h.page("displayPageSpectrum")
            ->findChild<QPushButton*>(QStringLiteral("pillResetPan"))->width();
        h.pan.setLineWidth(2.5f);
        h.pan.setWfOpacity(73);
        QVERIFY(h.pan.settingsSavePendingForTest());
        const QStringList before = AppSettings::instance().allKeys();
        h.panel->setDisplayTarget(nullptr);
        h.panel->setDisplayTarget(&h.pan);
        h.panel->openDisplayPill();
        h.layOut();
        QCOMPARE(AppSettings::instance().allKeys(), before);
        QVERIFY(h.pan.settingsSavePendingForTest());
        auto* dot = h.control<QPushButton>("pillOwnDisplayLineWidth");
        QVERIFY(dot);
        QVERIFY(!dot->isHidden());
        QVERIFY(dot->toolTip().contains(QStringLiteral("Pan 4")));
        QVERIFY(dot->toolTip().contains(QStringLiteral("own value")));
        QCOMPARE(label->pos(), labelPosition);
        const QRect dotRect(dot->mapTo(h.page("displayPageSpectrum"), QPoint(0, 0)), dot->size());
        const QPoint labelPoint = label->mapTo(h.page("displayPageSpectrum"), QPoint(0, 0));
        QVERIFY(dotRect.right() < labelPoint.x());
        QVERIFY(h.page("displayPageSpectrum")->rect().contains(dotRect));
        auto* footer = h.page("displayPageSpectrum")->findChild<QPushButton*>(QStringLiteral("pillResetPan"));
        QCOMPARE(footer->property("ownValueCount").toInt(), 2);
        QCOMPARE(footer->text(), QStringLiteral("Reset this pan (2)"));
        QCOMPARE(footer->width(), initialFooterWidth);
        dot->click();
        QCOMPARE(h.pan.lineWidth(), h.pan.perPanShipDefault(QStringLiteral("DisplayLineWidth")).toFloat());
        QCOMPARE(h.pan.wfOpacity(), 73);
        QVERIFY(dot->isHidden());
        QCOMPARE(footer->property("ownValueCount").toInt(), 1);
        // The explicit reset preserves other pending edits by saving them;
        // counting and reseeding above must still leave them pending.
        QCOMPARE(AppSettings::instance().value(QStringLiteral("DisplayWfOpacity_3")).toInt(), 73);
        QVERIFY(!h.pan.settingsSavePendingForTest());
    }

    void waterfallOwnDotCountsAndResetsTheWholeModeTupleOnce() {
        PillHarness h;
        h.panel->setDisplayPillTab(Tab::Levels);
        h.panel->openDisplayPill();
        h.pan.setClarityEnabled(false);
        h.pan.setWfAgcEnabled(false);
        h.pan.setWaterfallNFAGCEnabled(true);
        h.pan.setWfUseSpectrumMinMax(true);
        h.pan.setWfOpacity(71);
        auto* dot = h.control<QPushButton>("pillOwnWaterfallLevelsMode");
        QVERIFY(dot);
        QVERIFY(!dot->isHidden());
        auto* footer = h.page("displayPageLevels")->findChild<QPushButton*>(QStringLiteral("pillResetPan"));
        QCOMPARE(footer->property("ownValueCount").toInt(), 2);
        QSignalSpy resets(&h.pan, &SpectrumWidget::panDisplayReset);
        dot->click();
        QCOMPARE(resets.count(), 1);
        QCOMPARE(h.pan.clarityEnabled(), h.pan.perPanShipDefault(QStringLiteral("ClarityEnabled")) == QStringLiteral("True"));
        QCOMPARE(h.pan.wfAgcEnabled(), h.pan.perPanShipDefault(QStringLiteral("DisplayWfAgc")) == QStringLiteral("True"));
        QCOMPARE(h.pan.waterfallNFAGCEnabled(), h.pan.perPanShipDefault(QStringLiteral("WaterfallNFAGCEnabled")) == QStringLiteral("True"));
        QCOMPARE(h.pan.wfUseSpectrumMinMax(), h.pan.perPanShipDefault(QStringLiteral("DisplayWfUseSpectrumMinMax")) == QStringLiteral("True"));
        QCOMPARE(h.pan.wfOpacity(), 71);
        QVERIFY(dot->isHidden());
        QCOMPARE(footer->property("ownValueCount").toInt(), 1);
    }

    void ownFooterIncludesSetupOnlyValuesAndExcludesSharedAndViewSettings() {
        PillHarness h;
        h.panel->setDisplayPillTab(Tab::Spectrum);
        h.panel->openDisplayPill();
        auto* footer = h.page("displayPageSpectrum")->findChild<QPushButton*>(QStringLiteral("pillResetPan"));
        QCOMPARE(footer->property("ownValueCount").toInt(), 0);
        h.pan.setGridColor(QColor(QStringLiteral("#123456")));
        h.pan.setDssFloorDepth(15);
        h.pan.setFrequencyRange(14200000.0, 96000.0);
        QCOMPARE(footer->property("ownValueCount").toInt(), 0);
        h.pan.setPeakTextDelayMs(777);
        QCOMPARE(footer->property("ownValueCount").toInt(), 1);
        QCOMPARE(footer->property("setupOnlyOwnValueCount").toInt(), 1);
        QVERIFY(footer->toolTip().contains(QStringLiteral("Setup")));
        auto* reason = h.page("displayPageSpectrum")->findChild<QLabel*>(QStringLiteral("pillReason"));
        QVERIFY(reason);
        QVERIFY(reason->text().contains(QStringLiteral("Setup")));
        QVERIFY(reason->text().contains(QStringLiteral("1")));
        QCOMPARE(footer->text(), QStringLiteral("Reset this pan (1)"));
    }

    void ownDotsNameCompactValuesAndFollowTheirAvailability() {
        PillHarness h;
        h.panel->openDisplayPill();
        h.pan.setPeakHoldEnabled(true);
        h.pan.setPeakHoldDelayMs(1234);
        h.pan.setWaterfallBlankingEnabled(true);
        h.pan.setWaterfallBlankingThresholdDb(22);
        h.pan.setBackgroundImagePath(QStringLiteral("/test/picture.png"));
        auto* peak = h.control<QPushButton>("pillOwnDisplayPeakHoldResetMs");
        auto* blank = h.control<QPushButton>("pillOwnWaterfallBlankingThresholdDb");
        auto* picture = h.control<QPushButton>("pillOwnDisplayBackgroundImage");
        QVERIFY(peak);
        QVERIFY(blank);
        QVERIFY(picture);
        QVERIFY(peak->toolTip().contains(QStringLiteral("Peak Delay")));
        QVERIFY(blank->toolTip().contains(QStringLiteral("NB threshold")));
        QVERIFY(picture->toolTip().contains(QStringLiteral("Picture")));
        QVERIFY(peak->isEnabled());
        h.pan.setPeakHoldEnabled(false);
        QVERIFY(!peak->isEnabled());
        h.pan.setWaterfallBlankingEnabled(false);
        QVERIFY(!blank->isEnabled());
        const int delay = h.pan.peakHoldDelayMs();
        peak->click();
        QCOMPARE(h.pan.peakHoldDelayMs(), delay);
    }

    void approvedLevelsAndViewHaveUnlabelledChooserRows() {
        PillHarness h;
        const struct { const char* page; const char* pattern; QStringList texts; } groups[] = {
            {"displayPageLevels", "^pillSpectrumScale(?:Manual|Track)$", {"Manual", "Track floor"}},
            {"displayPageLevels", "^pillWaterfallLevels(?:Clarity|Auto|Floor|Manual|Scale)$",
                {"Clarity", "Auto", "Floor", "Manual", "Scale"}},
            {"displayPage3D", "^pillDisplaySpectrumRenderMode(?:2D|3D)$", {"2D Waterfall", "3D Stacked Trace"}}
        };
        for (const auto& group : groups) {
            QWidget* page = h.page(group.page);
            QStringList texts;
            for (QPushButton* button : page->findChildren<QPushButton*>(
                     QRegularExpression(QString::fromLatin1(group.pattern)))) {
                texts << button->text();
            }
            QCOMPARE(texts, group.texts);
        }
        QVERIFY(h.slider("pillDisplayNFOffsetGridFollow"));
        QVERIFY(h.toggle("pillDisplayMaintainNFAdjustDelta"));
        for (QLabel* label : h.page("displayPageFft")->findChildren<QLabel*>()) {
            QVERIFY(label->text() != QStringLiteral("Sizing:"));
        }
        QVERIFY(h.control<QLabel>("pillFftSizingNote"));
        QVERIFY(h.control<QLabel>("pillViewNote"));
    }

    void waterfallChooserSeedsPreservesClarityFlagsAndSameChoiceDoesNothing() {
        PillHarness h;
        h.pan.setWfAgcEnabled(true);
        h.pan.setWaterfallNFAGCEnabled(true);
        h.pan.setWfUseSpectrumMinMax(true);
        h.pan.setClarityEnabled(true);
        h.panel->setClarityState(true, false, false);
        QSignalSpy changed(&h.pan, &SpectrumWidget::displaySettingChanged);
        h.panel->openDisplayPill();
        QPushButton* clarity = h.toggle("pillWaterfallLevelsClarity");
        QVERIFY(clarity);
        QVERIFY(clarity->isChecked());
        QCOMPARE(changed.count(), 0);
        const QStringList keys = AppSettings::instance().allKeys();
        clarity->click();
        QVERIFY(clarity->isChecked());
        QVERIFY(h.pan.wfAgcEnabled());
        QVERIFY(h.pan.waterfallNFAGCEnabled());
        QVERIFY(h.pan.wfUseSpectrumMinMax());
        QCOMPARE(changed.count(), 0);
        QCOMPARE(AppSettings::instance().allKeys(), keys);
        QSignalSpy retune(h.panel, &SpectrumOverlayPanel::clarityRetuneRequested);
        QPushButton* retuneButton = h.toggle("pillClarityRetune");
        QVERIFY(retuneButton->isEnabled());
        retuneButton->click();
        QCOMPARE(retune.count(), 1);
        QVERIFY(!h.pan.clarityActive());
        QCOMPARE(h.control<QLabel>("pillClarityBadge")->property("clarityState").toString(), QStringLiteral("off"));
    }

    void waterfallChooserMapsEachNonClarityChoiceAndIsIdempotent() {
        PillHarness h;
        h.panel->openDisplayPill();
        const struct { const char* button; bool agc; bool floor; bool scale; } cases[] = {
            {"pillWaterfallLevelsAuto", true, false, false},
            {"pillWaterfallLevelsFloor", true, true, false},
            {"pillWaterfallLevelsManual", false, false, false},
            {"pillWaterfallLevelsScale", false, false, true}
        };
        for (const auto& mode : cases) {
            QPushButton* button = h.toggle(mode.button);
            QVERIFY2(button, mode.button);
            button->click();
            QVERIFY(button->isChecked());
            QCOMPARE(h.pan.wfAgcEnabled(), mode.agc);
            QCOMPARE(h.pan.waterfallNFAGCEnabled(), mode.floor);
            QCOMPARE(h.pan.wfUseSpectrumMinMax(), mode.scale);
            QVERIFY(!h.toggle("pillClarityRetune")->isEnabled());
            QSignalSpy changed(&h.pan, &SpectrumWidget::displaySettingChanged);
            const QStringList keys = AppSettings::instance().allKeys();
            button->click();
            QCOMPARE(changed.count(), 0);
            QCOMPARE(AppSettings::instance().allKeys(), keys);
        }
    }

    void chooserGrayLevelsReadTheRendererWithoutSavingThresholds() {
        PillHarness h;
        h.panel->openDisplayPill();
        QVERIFY(h.pan.clarityEnabled());
        QCOMPARE(h.valueText("pillDisplayWfLowLevel"), QStringLiteral("no signal"));
        QCOMPARE(h.valueText("pillDisplayWfHighLevel"), QStringLiteral("no signal"));
        const float storedLow = h.pan.wfLowThreshold();
        const float storedHigh = h.pan.wfHighThreshold();
        h.pan.setClarityFloorDbm(-110.0f);
        const QStringList storedKeys = AppSettings::instance().allKeys();
        QSignalSpy changed(&h.pan, &SpectrumWidget::displaySettingChanged);
        h.pan.setClarityActive(true);
        const auto effective = h.pan.effectiveWaterfallLevels();
        QCOMPARE(h.valueText("pillDisplayWfLowLevel"), QStringLiteral("%1 dBm").arg(qRound(effective.first)));
        QCOMPARE(h.valueText("pillDisplayWfHighLevel"), QStringLiteral("%1 dBm").arg(qRound(effective.second)));
        QCOMPARE(h.pan.wfLowThreshold(), storedLow);
        QCOMPARE(h.pan.wfHighThreshold(), storedHigh);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(changed.first().first().toString(), QStringLiteral("DisplayWfEffectiveLevels"));
        QCOMPARE(AppSettings::instance().allKeys(), storedKeys);
    }

    void spectrumScaleChooserUsesExistingFlagsWithoutSeedWrites() {
        PillHarness h;
        h.pan.setAdjustGridMinToNoiseFloor(true);
        h.pan.setNFOffsetGridFollow(-17);
        h.pan.setMaintainNFAdjustDelta(true);
        QSignalSpy changed(&h.pan, &SpectrumWidget::displaySettingChanged);
        h.panel->openDisplayPill();
        QPushButton* track = h.toggle("pillSpectrumScaleTrack");
        QVERIFY(track);
        QVERIFY(track->isChecked());
        QCOMPARE(changed.count(), 0);
        QCOMPARE(h.slider("pillDisplayNFOffsetGridFollow")->value(), -17);
        QCOMPARE(h.slider("pillDisplayNFOffsetGridFollow")->minimum(), -60);
        QCOMPARE(h.slider("pillDisplayNFOffsetGridFollow")->maximum(), 60);
        QVERIFY(h.toggle("pillDisplayMaintainNFAdjustDelta")->isChecked());
        track->click();
        QCOMPARE(changed.count(), 0);
        QPushButton* manual = h.toggle("pillSpectrumScaleManual");
        manual->click();
        QVERIFY(!h.pan.adjustGridMinToNoiseFloor());
        QCOMPARE(h.pan.nfOffsetGridFollow(), -17);
        QVERIFY(h.pan.maintainNFAdjustDelta());
        QVERIFY(!h.slider("pillDisplayNFOffsetGridFollow")->isEnabled());
        QVERIFY(!h.toggle("pillDisplayMaintainNFAdjustDelta")->isEnabled());
    }

    void compactSpectrumRowsMatchTheApprovedReference() {
        PillHarness h;
        h.panel->openDisplayPill();
        h.layOut();
        QWidget* page = h.page("displayPageSpectrum");
        const char* order[] = {
            "pillDisplaySpectrumDetector", "pillDisplayDispNormalize", "pillDisplayPanFill",
            "pillDisplayGradientEnabled", "pillDisplayFftFillAlpha", "pillDisplaySpectrumAveraging",
            "pillDisplaySpectrumAverageTimeMs", "pillDisplayLineWidth", "pillDisplayPeakHoldEnabled",
            "pillDisplayPeakHoldResetMs", "pillDisplayCalOffset", "pillDisplayGridEnabled",
            "pillDisplayFreqGridSpacing", "pillDisplayFreqScaleFontPx", "pillDisplayShowNoiseFloor",
            "pillDisplayShowBinWidth", "pillDisplayShowCursorFreq", "pillDisplayShowPeakValueOverlay"
        };
        int previous = 0;
        for (const char* name : order) {
            const int row = gridRowOf(page, QString::fromLatin1(name));
            QVERIFY2(row >= previous, name);
            previous = row;
        }
        QCOMPARE(gridRowOf(page, QStringLiteral("pillDisplayPeakHoldEnabled")),
                 gridRowOf(page, QStringLiteral("pillDisplayPeakHoldResetMs")));
        QCOMPARE(gridRowOf(page, QStringLiteral("pillDisplayGridEnabled")),
                 gridRowOf(page, QStringLiteral("pillDisplayFreqGridSpacing")));
        QStringList headers;
        for (QLabel* label : page->findChildren<QLabel*>(QStringLiteral("pillHeader"))) {
            headers << label->accessibleName();
        }
        QCOMPARE(headers, QStringList({"Trace", "Grid", "Markers"}));
        QLabel* note = h.control<QLabel>("pillSpectrumMarkersNote");
        QVERIFY(note);
        QVERIFY(note->text().contains(QStringLiteral("Cursor Freq is on Spectrum")));
        QVERIFY(note->text().contains(QStringLiteral("Peaks &amp; Markers")));
    }

    void compactNormalizeSeedsWritesAndGatesWithoutEcho() {
        PillHarness h;
        h.pan.setSpectrumDetector(SpectrumDetector::Average);
        h.pan.setDispNormalize(true);
        QSignalSpy announced(&h.pan, &SpectrumWidget::displaySettingChanged);
        h.panel->openDisplayPill();
        QPushButton* normalize = h.toggle("pillDisplayDispNormalize");
        QVERIFY(normalize);
        QCOMPARE(normalize->text(), QStringLiteral("Normalize to 1 Hz"));
        QVERIFY(normalize->isChecked());
        QVERIFY(normalize->isEnabled());
        QCOMPARE(announced.count(), 0);
        normalize->click();
        QVERIFY(!h.pan.dispNormalize());
        QCOMPARE(announced.count(), 1);
        h.pan.setDispNormalize(true);
        QVERIFY(normalize->isChecked());
        QCOMPARE(announced.count(), 2);
        h.pan.setSpectrumDetector(SpectrumDetector::Peak);
        QVERIFY(!normalize->isEnabled());
        QCOMPARE(normalize->toolTip(), QStringLiteral(
            "Normalize to 1 Hz needs the Average, Sample or RMS detector."));
    }

    void compactParentLabelsFollowTheSwitchRatherThanTheChild() {
        PillHarness h;
        h.panel->openDisplayPill();
        const struct { const char* label; const char* toggle; const char* child; } rows[] = {
            {"pillDisplayPeakHoldResetMsLabel", "pillDisplayPeakHoldEnabled", "pillDisplayPeakHoldResetMs"},
            {"pillDisplayFreqGridSpacingLabel", "pillDisplayGridEnabled", "pillDisplayFreqGridSpacing"},
            {"pillWaterfallBlankingThresholdDbLabel", "pillWaterfallBlankingEnabled", "pillWaterfallBlankingThresholdDb"}
        };
        for (const auto& row : rows) {
            QPushButton* toggle = h.toggle(row.toggle);
            QLabel* label = h.control<QLabel>(row.label);
            QWidget* child = h.control<QWidget>(row.child);
            QVERIFY(toggle && label && child);
            toggle->setChecked(false);
            QVERIFY(toggle->isEnabled());
            QVERIFY(label->isEnabled());
            QVERIFY(!child->isEnabled());
            toggle->click();
            QVERIFY(child->isEnabled());
            QVERIFY(label->isEnabled());
        }
        h.pan.setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode3D));
        QVERIFY(!h.control<QLabel>("pillDisplayPeakHoldResetMsLabel")->isEnabled());
    }

    void compactWaterfallHasDrawingOverlaysAndTime() {
        PillHarness h;
        QWidget* page = h.page("displayPageWaterfall");
        QVERIFY(gridRowOf(page, QStringLiteral("pillDisplayWfOpacity"))
                < gridRowOf(page, QStringLiteral("pillWaterfallBlankingThresholdDb")));
        QStringList headers;
        for (QLabel* label : page->findChildren<QLabel*>(QStringLiteral("pillHeader"))) {
            headers << label->accessibleName();
        }
        QCOMPARE(headers, QStringList({"Drawing", "Overlays", "Time"}));
        h.panel->openDisplayPill();
        h.panel->setDisplayPillTab(Tab::Waterfall);
        h.pan.setWaterfallHistoryMs(20LL * 60 * 1000);
        h.pan.setWfUpdatePeriodMs(10);
        QLabel* note = h.control<QLabel>("pillWaterfallRewindNote");
        QVERIFY(note);
        QCOMPARE(note->text(), QStringLiteral(
            "At this Rate the waterfall keeps 2:43 (16384 rows). Slow Rate to keep more."));
        QVERIFY(h.pill()->height() <= h.pan.height());
        QVERIFY(h.scroll());
        QCOMPARE(h.scroll()->verticalScrollBar()->maximum(), 0);
        h.pan.setWfUpdatePeriodMs(500);
        QCOMPARE(note->text(), QStringLiteral("Keeps 20:00 at this Rate."));
        QVERIFY(h.pill()->height() <= h.pan.height());
        QCOMPARE(h.scroll()->verticalScrollBar()->maximum(), 0);
    }

    void approvedLiveMockupHasFiveTabsAndKeepsBothLevelJobsTogether() {
        PillHarness h;
        QStringList texts;
        for (QPushButton* button : h.pill()->findChildren<QPushButton*>(
                 QRegularExpression(QStringLiteral("^displayTab")))) {
            texts << button->text();
        }
        QCOMPARE(texts, QStringList({QStringLiteral("Spectrum"), QStringLiteral("Levels"),
                                     QStringLiteral("Waterfall"), QStringLiteral("View"),
                                     QStringLiteral("FFT")}));

        h.panel->openDisplayPill();
        h.layOut();
        for (QPushButton* button : h.pill()->findChildren<QPushButton*>(
                 QRegularExpression(QStringLiteral("^displayTab")))) {
            QVERIFY2(button->width() >= button->fontMetrics().horizontalAdvance(button->text()) + 2,
                     qPrintable(button->text()));
        }

        QWidget* levels = h.page("displayPageLevels");
        QWidget* spectrum = h.page("displayPageSpectrum");
        QWidget* waterfall = h.page("displayPageWaterfall");
        // The existing 3D page object name remains a stable test seam;
        // its visible tab becomes View and also owns Background.
        QWidget* view = h.page("displayPage3D");
        QVERIFY(levels && spectrum && waterfall && view);
        const char* levelControls[] = {
            "pillDisplayGridMax", "pillDisplayGridMin",
            "pillDisplayWfBlackLevel", "pillDisplayWfColorGain",
            "pillDisplayWfLowLevel", "pillDisplayWfHighLevel", "pillClarityRetune",
        };
        for (const char* name : levelControls) {
            const QString objectName = QString::fromLatin1(name);
            QVERIFY2(levels->findChild<QWidget*>(objectName), name);
            QVERIFY2(!spectrum->findChild<QWidget*>(objectName), name);
            QVERIFY2(!waterfall->findChild<QWidget*>(objectName), name);
        }
        const char* backgroundControls[] = {
            "pillDisplayBackgroundImage", "pillDisplayBackgroundEnabled",
            "pillDisplayBackgroundOpacity", "pillDisplayBackgroundColor",
        };
        for (const char* name : backgroundControls) {
            const QString objectName = QString::fromLatin1(name);
            QVERIFY2(view->findChild<QWidget*>(objectName), name);
            QVERIFY2(!spectrum->findChild<QWidget*>(objectName), name);
        }

        // Optional actual-widget captures for the implementation handoff.
        // No live app, radio, sound device, or native window is started.
        const QString captureDir = qEnvironmentVariable("NEREUS_DISPLAY_REBUILD_CAPTURE_DIR");
        if (!captureDir.isEmpty()) {
            QVERIFY(QDir().mkpath(captureDir));
            h.panel->openDisplayPill();
            const struct { Tab tab; const char* name; } captures[] = {
                {Tab::Spectrum, "spectrum"}, {Tab::Levels, "levels"},
                {Tab::Waterfall, "waterfall"}, {Tab::View3D, "view"}, {Tab::Fft, "fft"},
            };
            for (const auto& capture : captures) {
                h.panel->setDisplayPillTab(capture.tab);
                h.layOut();
                QImage image(h.pill()->size(), QImage::Format_ARGB32_Premultiplied);
                image.fill(QColor(QStringLiteral("#0a0a14")));
                h.pill()->render(&image);
                QVERIFY(image.save(captureDir + QLatin1Char('/')
                                   + QString::fromLatin1(capture.name) + QStringLiteral(".png")));
            }
        }
    }

    void approvedParentGatesShowTheirCompleteReasonInThePill() {
        PillHarness h;
        h.panel->openDisplayPill();
        h.panel->setDisplayPillTab(Tab::Spectrum);
        h.pan.setGridEnabled(false);
        auto* spacing = h.combo("pillDisplayFreqGridSpacing");
        QVERIFY(spacing);
        QVERIFY(!spacing->isEnabled());
        QCOMPARE(spacing->toolTip(), QStringLiteral("Grid is off, so no grid lines are drawn."));
        auto* reason = h.page("displayPageSpectrum")->findChild<QLabel*>(QStringLiteral("pillReason"));
        QVERIFY(reason);
        const int height = reason->height();
        QEnterEvent enter{QPointF{}, QPointF{}, QPointF{}};
        QApplication::sendEvent(spacing, &enter);
        QCOMPARE(reason->text(), spacing->toolTip());
        QCOMPARE(reason->height(), height);
        QVERIFY(reason->wordWrap());
        h.pan.setGridEnabled(true);
        QVERIFY(spacing->isEnabled());

        h.pan.setSpectrumAveraging(SpectrumAveraging::None);
        QVERIFY(!h.slider("pillDisplaySpectrumAverageTimeMs")->isEnabled());
        QVERIFY(!h.box("pillDisplaySpectrumAverageTimeMs")->isEnabled());
        h.pan.setSpectrumAveraging(SpectrumAveraging::Recursive);
        QVERIFY(h.slider("pillDisplaySpectrumAverageTimeMs")->isEnabled());

        h.pan.setPanFillEnabled(false);
        QVERIFY(!h.slider("pillDisplayFftFillAlpha")->isEnabled());
        h.pan.setTraceColor(h.pan.fillColor());
        QVERIFY(!h.toggle("pillDisplayFillColor")->isEnabled());
        QVERIFY(!h.toggle("pillDisplayGradientEnabled")->isEnabled());
        h.pan.setPanFillEnabled(true);
        QVERIFY(h.slider("pillDisplayFftFillAlpha")->isEnabled());
        h.pan.setWaterfallBlankingEnabled(false);
        QVERIFY(!h.slider("pillWaterfallBlankingThresholdDb")->isEnabled());
        h.pan.setWaterfallBlankingEnabled(true);
        QVERIFY(h.slider("pillWaterfallBlankingThresholdDb")->isEnabled());

        h.panel->setDisplayPillTab(Tab::View3D);
        h.pan.setBackgroundEnabled(false);
        QVERIFY(!h.slider("pillDisplayBackgroundOpacity")->isEnabled());
        h.pan.setSpectrumRenderMode(1);
        QVERIFY(!h.slider("pillDisplayLineWidth")->isEnabled());
        QVERIFY(!h.toggle("pillDisplayGradientEnabled")->isEnabled());
        h.pan.setSpectrumRenderMode(0);
        QVERIFY(h.slider("pillDisplayLineWidth")->isEnabled());
        QVERIFY(h.toggle("pillDisplayGradientEnabled")->isEnabled());
    }

    // Five tab buttons, in the accepted live mockup order.
    void pillHasFiveTabsInOrder() {
        PillHarness h;
        QVERIFY(h.pill());
        QStringList names;
        QStringList texts;
        for (QPushButton* button : h.pill()->findChildren<QPushButton*>(
                 QRegularExpression(QStringLiteral("^displayTab")))) {
            names << button->objectName();
            texts << button->text();
            QCOMPARE(button->height(), 22);
        }
        QCOMPARE(names, QStringList({QStringLiteral("displayTabSpectrum"),
                                     QStringLiteral("displayTabLevels"),
                                     QStringLiteral("displayTabWaterfall"),
                                     QStringLiteral("displayTab3D"), QStringLiteral("displayTabFft")}));
        QCOMPARE(texts, QStringList({QStringLiteral("Spectrum"), QStringLiteral("Levels"),
                                     QStringLiteral("Waterfall"), QStringLiteral("View"),
                                     QStringLiteral("FFT")}));

        // One tab is checked and one page shown at a time.
        const struct {
            Tab tab;
            const char* button;
            const char* page;
        } tabs[] = {
            {Tab::Spectrum, "displayTabSpectrum", "displayPageSpectrum"},
            {Tab::Levels, "displayTabLevels", "displayPageLevels"},
            {Tab::Fft, "displayTabFft", "displayPageFft"},
            {Tab::Waterfall, "displayTabWaterfall", "displayPageWaterfall"},
            {Tab::View3D, "displayTab3D", "displayPage3D"},
        };
        for (const auto& chosen : tabs) {
            h.toggle(chosen.button)->click();
            QCOMPARE(h.panel->displayPillTab(), chosen.tab);
            for (const auto& other : tabs) {
                const bool current = other.tab == chosen.tab;
                QCOMPARE(h.toggle(other.button)->isChecked(), current);
                QCOMPARE(!h.page(other.page)->isHidden(), current);
            }
        }
    }

    // Every tab has the accent header and the footer; FFT is tagged.
    void everyTabHasItsHeaderAndFooter() {
        PillHarness h;
        const struct {
            const char* page;
            const char* title;
            const char* scope;
        } pages[] = {
            {"displayPageSpectrum", "Trace", ""},
            {"displayPageLevels", "Spectrum scale", ""},
            {"displayPageFft", "FFT", "CORE"},
            {"displayPageWaterfall", "Drawing", ""},
            {"displayPage3D", "View", ""},
        };
        for (const auto& expected : pages) {
            QWidget* page = h.page(expected.page);
            QVERIFY2(page, expected.page);
            auto* grid = qobject_cast<QGridLayout*>(page->layout());
            QVERIFY(grid);
            // spec 3.1: margins 8/6, spacing 4.
            QCOMPARE(grid->contentsMargins(), QMargins(8, 6, 8, 6));
            QCOMPARE(grid->horizontalSpacing(), 4);
            QVERIFY(grid->verticalSpacing() == 3 || grid->verticalSpacing() == 4);

            auto* header = page->findChild<QLabel*>(QStringLiteral("pillHeader"));
            QVERIFY(header);
            // Lower case under the small-caps font (F4).
            QCOMPARE(header->text(), QString::fromLatin1(expected.title).toLower());
            QCOMPARE(header->accessibleName(), QString::fromLatin1(expected.title));
            QCOMPARE(gridRowOf(page, QStringLiteral("pillHeader")), 0);
            auto* scope = page->findChild<QLabel*>(QStringLiteral("pillHeaderScope"));
            QCOMPARE(scope ? scope->text() : QString(), QString::fromLatin1(expected.scope));

            auto* reset = page->findChild<QPushButton*>(QStringLiteral("pillResetPan"));
            auto* setup = page->findChild<QPushButton*>(QStringLiteral("pillSetupDefaults"));
            QVERIFY(reset && setup);
            const int ownCount = reset->property("ownValueCount").toInt();
            QCOMPARE(reset->text(), ownCount ? QStringLiteral("Reset this pan (%1)").arg(ownCount)
                                            : QStringLiteral("Reset this pan"));
            QCOMPARE(setup->text(), QStringLiteral("Setup defaults →"));
            // The footer is the last row.
            QCOMPARE(gridRowOf(page, QStringLiteral("pillResetPan")), grid->rowCount() - 1);
            QCOMPARE(gridRowOf(page, QStringLiteral("pillSetupDefaults")), grid->rowCount() - 1);
        }
    }

    // Follow-up wave F4: a header is all small caps, as the mockup draws it.
    // The text is lower case under a small-caps font, so no letter (not the
    // first, and none of "FFT") is drawn full height.
    void headersAreAllSmallCaps() {
        PillHarness h;
        const QList<QLabel*> headers =
            h.pill()->findChildren<QLabel*>(QStringLiteral("pillHeader"));
        QStringList texts;
        for (const QLabel* header : headers) {
            texts << header->text();
            QCOMPARE(header->font().capitalization(), QFont::SmallCaps);
            QCOMPARE(header->font().pixelSize(), 9);
            QCOMPARE(header->text(), header->text().toLower());
            QCOMPARE(header->accessibleName().toLower(), header->text());
        }
        QVERIFY(texts.contains(QStringLiteral("trace")));
        QVERIFY(texts.contains(QStringLiteral("fft")));
        QVERIFY(texts.contains(QStringLiteral("drawing")));
        QVERIFY(texts.contains(QStringLiteral("view")));
        QVERIFY(texts.contains(QStringLiteral("background")));
    }

    // Follow-up wave F4: the marker pills retain their text when Peak Value
    // takes the preferred overflow row.
    void theMarkerPillsStayUnclippedWhenPeakValueWraps() {
        PillHarness h;
        h.panel->setDisplayPillTab(Tab::Spectrum);
        h.panel->openDisplayPill();
        h.layOut();
        QWidget* page = h.page("displayPageSpectrum");
        const char* names[] = {"pillDisplayShowNoiseFloor", "pillDisplayShowBinWidth",
                               "pillDisplayShowCursorFreq", "pillDisplayShowPeakValueOverlay"};
        int lastRow = -1;
        int right = 0;
        for (const char* name : names) {
            QPushButton* pill = h.toggle(name);
            QVERIFY2(pill, name);
            const int row = gridRowOf(page, QString::fromLatin1(name));
            QVERIFY(row >= lastRow);
            if (row != lastRow) { right = 0; }
            QVERIFY(pill->x() >= right);
            QVERIFY2(pill->width() >= pill->sizeHint().width(), name);
            QVERIFY2(pill->width() >= pill->fontMetrics().horizontalAdvance(pill->text()) + 2, name);
            // The text fits inside the style's padding, not only the
            // button's outer width.
            const int textWidth = pill->fontMetrics().horizontalAdvance(pill->text());
            const int contentWidth = pushButtonContents(pill).width();
            QVERIFY2(contentWidth >= textWidth,
                     qPrintable(QStringLiteral("%1: %2 px content width, %3 px needed (%4)")
                         .arg(pill->text()).arg(contentWidth).arg(textWidth)
                         .arg(pill->objectName())));
            right = pill->x() + pill->width();
            QVERIFY(right <= 250);
            lastRow = row;
        }
    }

    // Second pass S4: on a font too wide for the four overlay pills to
    // share a line, Peak Value takes a line of its own, at the other
    // toggles' size, and the Spectrum tab still fits the pill's 480 px
    // with the pan keyed (the keyed note is two lines) and nothing cut off.
    // The font here is the application's with its letters spaced out, so
    // every pill is wider than this computer's font makes it. Measure the
    // styled pills before choosing the extra letter spacing: 1.5 px alone
    // does not force a wrap with every platform's default font. The
    // stepping is bounded at 13 steps (1.5 px to 4.5 px).
    void whenPeakValueWrapsTheKeyedSpectrumTabStillFits() {
        struct FontRestore {
            QFont saved{QApplication::font()};
            ~FontRestore() { QApplication::setFont(saved); }
        } restore;
        QFont wide = restore.saved;
        constexpr int kMarkerRoom = 268 - 2 - 2 * 8;
        constexpr int kMinimumGap = 2;
        constexpr int kMaxSpacingSteps = 13;
        int firstThreeWidth = 0;
        int allFourWidth = 0;
        for (int step = 0; step < kMaxSpacingSteps; ++step) {
            wide.setLetterSpacing(QFont::AbsoluteSpacing, 1.5 + step * 0.25);
            QApplication::setFont(wide);
            PillHarness probe;
            firstThreeWidth = 2 * kMinimumGap;
            const char* names[] = {"pillDisplayShowNoiseFloor", "pillDisplayShowBinWidth",
                                   "pillDisplayShowCursorFreq", "pillDisplayShowPeakValueOverlay"};
            for (int index = 0; index < 4; ++index) {
                QPushButton* button = probe.toggle(names[index]);
                QVERIFY(button);
                QString style = button->styleSheet();
                style.replace(QRegularExpression(QStringLiteral("padding: 0 [0-9]+px;")),
                              QStringLiteral("padding: 0 1px;"));
                button->setStyleSheet(style);
                button->ensurePolished();
                if (index < 3) {
                    firstThreeWidth += button->sizeHint().width();
                } else {
                    allFourWidth = firstThreeWidth + kMinimumGap + button->sizeHint().width();
                }
            }
            qInfo("marker wrap fixture: spacing=%.2f three=%d four=%d room=%d",
                  wide.letterSpacing(), firstThreeWidth, allFourWidth, kMarkerRoom);
            if (allFourWidth > kMarkerRoom) { break; }
        }
        QVERIFY2(allFourWidth > kMarkerRoom,
                 "The fixture font must make four marker pills exceed even the tightest line");
        QVERIFY2(firstThreeWidth <= kMarkerRoom,
                 "The fixture font must still let the first three marker pills share a line");
        QApplication::setFont(wide);
        PillHarness h;
        h.panel->openDisplayPill();
        h.pan.setPeakHoldDelayMs(10000);
        h.pan.setDbmCalOffset(-30.0f);
        h.pan.setDisplayKeyed(true, 0, 0);
        QWidget* spectrumPage = h.page("displayPageSpectrum");
        const int firstMarkerRow = gridRowOf(
            spectrumPage, QStringLiteral("pillDisplayShowNoiseFloor"));
        const int peakRow = gridRowOf(
            spectrumPage, QStringLiteral("pillDisplayShowPeakValueOverlay"));
        const int cursorRow = gridRowOf(
            spectrumPage, QStringLiteral("pillDisplayShowCursorFreq"));
        QCOMPARE(cursorRow, firstMarkerRow);
        QVERIFY2(peakRow > firstMarkerRow,
                 qPrintable(QStringLiteral("rows first=%1 peak=%2 cursor=%3")
                     .arg(firstMarkerRow).arg(peakRow).arg(cursorRow)));
        for (const Tab tab : {Tab::Spectrum, Tab::Levels, Tab::Waterfall, Tab::View3D, Tab::Fft}) {
            h.panel->setDisplayPillTab(tab);
            h.layOut();
            const char* pages[] = {"displayPageSpectrum", "displayPageLevels", "displayPageWaterfall",
                                   "displayPage3D", "displayPageFft"};
            QWidget* page = h.page(pages[static_cast<int>(tab)]);
            QCOMPARE(h.pill()->width(), 268);
            QVERIFY2(h.pill()->height() <= 480, qPrintable(QString::number(h.pill()->height())));
            QVERIFY(h.scroll());
            QCOMPARE(h.scroll()->verticalScrollBar()->maximum(), 0);
            for (QPushButton* button : page->findChildren<QPushButton*>()) {
                if (button->text().isEmpty() || button->isHidden()) { continue; }
                const QRect rect(button->mapTo(page, QPoint(0, 0)), button->size());
                QVERIFY2(rect.left() >= 0 && rect.right() < page->width(), qPrintable(button->objectName()));
                QVERIFY2(button->width() >= button->fontMetrics().horizontalAdvance(button->text()) + 2,
                         qPrintable(QStringLiteral("%1: %2 px available, %3 px needed")
                             .arg(button->objectName()).arg(button->width())
                             .arg(button->fontMetrics().horizontalAdvance(button->text()) + 2)));
            }
            for (QLabel* label : page->findChildren<QLabel*>()) {
                if (label->text().isEmpty() || label->isHidden() || label->wordWrap()) { continue; }
                QVERIFY2(label->width() >= label->fontMetrics().horizontalAdvance(label->text()),
                         qPrintable(QStringLiteral("%1: %2 px available, %3 px needed (%4)")
                             .arg(label->text()).arg(label->width())
                             .arg(label->fontMetrics().horizontalAdvance(label->text()))
                             .arg(label->objectName())));
            }
        }
    }

    void withoutAWrapTheSpectrumTabKeepsItsUsualSpacing() {
        PillHarness h;
        QWidget* page = h.page("displayPageSpectrum");
        auto* grid = qobject_cast<QGridLayout*>(page->layout());
        QVERIFY(grid);
        QVERIFY(grid->verticalSpacing() == 3 || grid->verticalSpacing() == 4);
        QCOMPARE(grid->horizontalSpacing(), 4);
        QCOMPARE(gridRowOf(page, QStringLiteral("pillDisplayPanFill")),
                 gridRowOf(page, QStringLiteral("pillDisplayDispNormalize")));
    }

    // Follow-up wave F4: on the FFT tab every value ends at the right edge,
    // the value column is the other tabs' 50 px, and the sliders are as
    // wide as each other.
    void fftValuesEndAtTheRightEdge() {
        PillHarness h;
        h.panel->setDisplayPillTab(Tab::Fft);
        h.panel->openDisplayPill();
        h.layOut();
        QWidget* page = h.page("displayPageFft");
        // The page's right margin is 8 px.
        const int edge = page->width() - 8;
        const auto rightOf = [page](QWidget* widget) {
            return widget->mapTo(page, QPoint(widget->width(), 0)).x();
        };
        for (const char* name : {"pillDisplayFftSizeValue", "pillDisplaySpectrumFpsValue",
                                 "pillDisplayDecimationValue",
                                 "pillDisplayHzPerBinTargetValue"}) {
            auto* value = h.control<QLabel>(name);
            QVERIFY2(value, name);
            QCOMPARE(rightOf(value), edge);
            QVERIFY(value->alignment() & Qt::AlignRight);
        }
        auto* scope = page->findChild<QLabel*>(QStringLiteral("pillHeaderScope"));
        QCOMPARE(rightOf(scope), edge);
        // FFT sizing mode (design D38): the Hz/bin value is in the column
        // too, now that its row has no Off/On button; "200.0 Hz" is its
        // widest and is not cut off.
        const struct { const char* name; const char* endpoint; } endpoints[] = {
            {"pillDisplayFftSizeValue", "262144"},
            {"pillDisplaySpectrumFpsValue", "60"},
            {"pillDisplayDecimationValue", "16"},
            {"pillDisplayHzPerBinTargetValue", "200.0 Hz"}
        };
        for (const auto& endpoint : endpoints) {
            QLabel* value = h.control<QLabel>(endpoint.name);
            value->ensurePolished();
            QVERIFY2(value->width() >= 50, endpoint.name);
            QVERIFY2(value->contentsRect().width() >= value->fontMetrics().horizontalAdvance(
                         QString::fromLatin1(endpoint.endpoint)), endpoint.name);
            QVERIFY2(value->contentsRect().width() >= value->fontMetrics().horizontalAdvance(
                         value->text()), endpoint.name);
        }
        auto* target = h.control<QLabel>("pillDisplayHzPerBinTargetValue");
        target->ensurePolished();
        QVERIFY2(target->fontMetrics().horizontalAdvance(QStringLiteral("200.0 Hz"))
                     <= target->contentsRect().width(),
                 qPrintable(QStringLiteral("\"200.0 Hz\" is %1 px in %2")
                                .arg(target->fontMetrics().horizontalAdvance(
                                    QStringLiteral("200.0 Hz")))
                                .arg(target->contentsRect().width())));

        const int sliderWidth = h.slider("pillDisplayFftSize")->width();
        for (const char* name : {"pillDisplayFftSize", "pillDisplaySpectrumFps",
                                 "pillDisplayDecimation", "pillDisplayHzPerBinTarget"}) {
            QSlider* slider = h.slider(name);
            QLabel* value = h.control<QLabel>(qPrintable(QString::fromLatin1(name) + QStringLiteral("Value")));
            QCOMPARE(slider->mapTo(page, QPoint(0, 0)).x(),
                     h.slider("pillDisplayFftSize")->mapTo(page, QPoint(0, 0)).x());
            QCOMPARE(rightOf(slider), edge - value->width() - 4);
            QVERIFY(slider->width() > 0);
        }
        // The three Sizing buttons fill the slider and value columns.
        QPushButton* first = h.toggle("pillDisplayFftSizingModeAuto");
        QPushButton* last = h.toggle("pillDisplayFftSizingModeFixed");
        QVERIFY(first && last);
        QCOMPARE(first->mapTo(page, QPoint(0, 0)).x(), 8);
        QCOMPARE(rightOf(last), edge);
        QCOMPARE(rightOf(h.slider("pillDisplayFftSize")),
                 edge - h.control<QLabel>("pillDisplayFftSizeValue")->width() - 4);
        // Band values on Levels keep the same right alignment and enough
        // room for the range endpoints, including the complete dBm unit.
        h.panel->setDisplayPillTab(Tab::Levels);
        h.layOut();
        QWidget* levels = h.page("displayPageLevels");
        for (const char* name : {"pillDisplayGridMaxValue", "pillDisplayGridMinValue"}) {
            QLabel* value = h.control<QLabel>(name);
            QVERIFY(value->alignment() & Qt::AlignRight);
            QVERIFY(value->width() >= 50);
            QVERIFY(value->contentsRect().width() >= value->fontMetrics().horizontalAdvance(
                        QStringLiteral("-200 dBm")));
            QCOMPARE(value->mapTo(levels, QPoint(value->width(), 0)).x(), levels->width() - 8);
        }
        qInfo() << "FFT slider" << sliderWidth << "Spectrum slider"
                << h.slider("pillDisplayGridMax")->width();
    }

    // Follow-up wave F4: the note under Decimation takes no height while it
    // is empty, and is back, whole, when there is a reason.
    void theEmptyStationNoteTakesNoHeight() {
        PillHarness h;
        h.panel->setDisplayPillTab(Tab::Fft);
        h.panel->openDisplayPill();
        h.layOut();
        QWidget* pill = h.pill();
        auto* note = h.control<QLabel>("pillStationNote");
        QVERIFY(note);
        QVERIFY(note->isHidden());
        QCOMPARE(note->text(), QString());
        const int emptyHeight = pill->height();
        QVERIFY(emptyHeight <= 480);
        QVERIFY(h.scroll());
        QCOMPARE(h.scroll()->verticalScrollBar()->maximum(), 0);
        // The fixed reason area sits between Decimation and the divider.
        QWidget* page = h.page("displayPageFft");
        QWidget* reset = page->findChild<QWidget*>(QStringLiteral("pillResetPan"));
        const int footTop = reset->mapTo(page, QPoint(0, 0)).y();
        QSlider* decimation = h.slider("pillDisplayDecimation");
        const int rowBottom = decimation->mapTo(page, QPoint(0, 0)).y() + decimation->height();
        qInfo() << "FFT tab is" << emptyHeight << "px; Decimation ends at" << rowBottom
                << "footer starts at" << footTop;
        // 4 (spacing) + 5 (divider) + 4 (spacing), and at most the 2 px a
        // 14 px slider is centred in an 18 px row.
        auto* why = page->findChild<QLabel*>(QStringLiteral("pillReason"));
        QVERIFY(why);
        QVERIFY2(footTop - rowBottom <= 4 + why->height() + 4 + 5 + 4 + 2,
                 qPrintable(QString::number(footTop - rowBottom)));

        const QString reason = QStringLiteral("Connect to the Core to change these.");
        h.panel->setStationRowsAvailable(false, reason);
        h.layOut();
        QVERIFY(!note->isHidden());
        QCOMPARE(note->text(), reason);
        // One 12 px line and its 4 px of spacing.
        QCOMPARE(pill->height(), emptyHeight + 12 + 4);
        QCOMPARE(h.scroll()->verticalScrollBar()->maximum(), 0);
        QCOMPARE(note->height(), 12);
        QVERIFY(note->width()
                >= QFontMetrics(note->font()).horizontalAdvance(reason));

        h.panel->setStationRowsAvailable(true, QString());
        h.layOut();
        QVERIFY(note->isHidden());
        QCOMPARE(pill->height(), emptyHeight);

        // The old-Core reason shows it too.
        h.panel->setRemoteWindow(true, false);
        QVERIFY(!note->isHidden());
        QCOMPARE(note->text(), QStringLiteral("This Core is too old to change Decimation."));
        h.panel->setRemoteWindow(false, false);
        QVERIFY(note->isHidden());
    }

    // "Reset this pan" asks; "Setup defaults" names the page for the tab.
    void footerAsksForResetAndForTheTabsSetupPage() {
        PillHarness h;
        QSignalSpy reset(h.panel, &SpectrumOverlayPanel::resetPanRequested);
        QSignalSpy setup(h.panel, &SpectrumOverlayPanel::openSetupRequested);
        const struct {
            Tab tab;
            const char* page;
            const char* setupPage;
        } tabs[] = {
            {Tab::Spectrum, "displayPageSpectrum", "Spectrum"},
            {Tab::Levels, "displayPageLevels", "Levels"},
            {Tab::Fft, "displayPageFft", "FFT & Engine"},
            {Tab::Waterfall, "displayPageWaterfall", "Waterfall"},
            {Tab::View3D, "displayPage3D", "View"},
        };
        int clicks = 0;
        for (const auto& tab : tabs) {
            h.panel->setDisplayPillTab(tab.tab);
            QWidget* page = h.page(tab.page);
            page->findChild<QPushButton*>(QStringLiteral("pillSetupDefaults"))->click();
            page->findChild<QPushButton*>(QStringLiteral("pillResetPan"))->click();
            ++clicks;
            QCOMPARE(setup.count(), clicks);
            QCOMPARE(setup.last().at(0).toString(), QString::fromLatin1(tab.setupPage));
            QCOMPARE(reset.count(), clicks);
        }
    }

    // The Spectrum tab's rows, top to bottom as spec table 3.3 lists them,
    // with their ranges.
    void spectrumTabRowsInSpecOrderWithTheirRanges() {
        PillHarness h;
        QWidget* page = h.page("displayPageSpectrum");
        QVERIFY(page);
        const char* order[] = {
            "pillDisplaySpectrumDetector", "pillDisplayDispNormalize", "pillDisplayPanFill",
            "pillDisplayGradientEnabled", "pillDisplayFftFillAlpha", "pillDisplaySpectrumAveraging",
            "pillDisplaySpectrumAverageTimeMs", "pillDisplayLineWidth", "pillDisplayPeakHoldEnabled",
            "pillDisplayPeakHoldResetMs", "pillDisplayCalOffset", "pillDisplayGridEnabled",
            "pillDisplayFreqGridSpacing", "pillDisplayFreqScaleFontPx", "pillDisplayShowNoiseFloor",
            "pillDisplayShowBinWidth", "pillDisplayShowCursorFreq", "pillDisplayShowPeakValueOverlay",
        };
        int last = 0;
        for (const char* name : order) {
            const int row = gridRowOf(page, QString::fromLatin1(name));
            QVERIFY2(row >= last, name);
            last = row;
        }
        // The pills of one group share a line; the swatches share their
        // slider's row; Choose..., Clear and Off share one.
        QCOMPARE(gridRowOf(page, QStringLiteral("pillDisplayPeakHoldResetMs")),
                 gridRowOf(page, QStringLiteral("pillDisplayPeakHoldEnabled")));
        QCOMPARE(gridRowOf(page, QStringLiteral("pillDisplayFillColor")),
                 gridRowOf(page, QStringLiteral("pillDisplayFftFillAlpha")));
        QCOMPARE(gridRowOf(page, QStringLiteral("pillDisplayTraceColor")),
                 gridRowOf(page, QStringLiteral("pillDisplayLineWidth")));
        QCOMPARE(gridRowOf(h.page("displayPage3D"), QStringLiteral("pillDisplayBackgroundImageClear")),
                 gridRowOf(h.page("displayPage3D"), QStringLiteral("pillDisplayBackgroundImage")));
        QCOMPARE(gridRowOf(h.page("displayPage3D"), QStringLiteral("pillDisplayBackgroundEnabled")),
                 gridRowOf(h.page("displayPage3D"), QStringLiteral("pillDisplayBackgroundImage")));
        // Both level jobs moved to Levels; the band's note stays below its pair.
        QCOMPARE(gridRowOf(h.page("displayPageLevels"), QStringLiteral("pillDisplayGridMin")),
                 gridRowOf(h.page("displayPageLevels"), QStringLiteral("pillDisplayGridMax")) + 1);
        QCOMPARE(gridRowOf(h.page("displayPageLevels"), QStringLiteral("pillBandGridNote")),
                 gridRowOf(h.page("displayPageLevels"), QStringLiteral("pillDisplayMaintainNFAdjustDelta")) + 1);

        const struct {
            const char* name;
            int min;
            int max;
        } sliders[] = {
            {"pillDisplayFftFillAlpha", 0, 100},
            {"pillDisplayGridMax", -200, 0},
            {"pillDisplayGridMin", -200, 0},
            {"pillDisplayLineWidth", 1, 50},         // 0.1 to 5 px in 0.1 steps
            {"pillDisplaySpectrumAverageTimeMs", 1, 500},
            {"pillDisplayCalOffset", -60, 60},       // -30 to 30 dB in 0.5 steps
            {"pillDisplayPeakHoldResetMs", 100, 10000},
            {"pillDisplayBackgroundOpacity", 0, 100},
        };
        for (const auto& expected : sliders) {
            QSlider* slider = h.slider(expected.name);
            QVERIFY2(slider, expected.name);
            QCOMPARE(slider->minimum(), expected.min);
            QCOMPARE(slider->maximum(), expected.max);
        }

        const auto items = [&h](const char* name) {
            QStringList texts;
            QComboBox* combo = h.combo(name);
            for (int i = 0; combo && i < combo->count(); ++i) {
                texts << combo->itemText(i);
            }
            return texts;
        };
        QCOMPARE(items("pillDisplaySpectrumDetector"),
                 QStringList({QStringLiteral("Peak"), QStringLiteral("Rosenfell"),
                              QStringLiteral("Average"), QStringLiteral("Sample"),
                              QStringLiteral("RMS")}));
        QCOMPARE(items("pillDisplaySpectrumAveraging"),
                 QStringList({QStringLiteral("None"), QStringLiteral("Recursive"),
                              QStringLiteral("Time Window"), QStringLiteral("Log Recursive")}));
        QCOMPARE(items("pillDisplayFreqGridSpacing"),
                 QStringList({QStringLiteral("Auto"), QStringLiteral("1 kHz"),
                              QStringLiteral("2 kHz"), QStringLiteral("5 kHz"),
                              QStringLiteral("10 kHz"), QStringLiteral("25 kHz"),
                              QStringLiteral("50 kHz"), QStringLiteral("100 kHz")}));
        QCOMPARE(items("pillDisplayFreqScaleFontPx"),
                 QStringList({QStringLiteral("8 px"), QStringLiteral("9 px"),
                              QStringLiteral("10 px"), QStringLiteral("11 px"),
                              QStringLiteral("12 px"), QStringLiteral("13 px"),
                              QStringLiteral("14 px")}));
        auto* choose = h.control<QPushButton>("pillDisplayBackgroundImage");
        auto* clear = h.control<QPushButton>("pillDisplayBackgroundImageClear");
        QVERIFY(choose && clear);
        QCOMPARE(choose->text(), QStringLiteral("Choose..."));
        QCOMPARE(clear->text(), QStringLiteral("Clear"));
    }

    // The 3D View tab's rows, top to bottom as spec table 3.6 lists them.
    void view3DTabRowsInSpecOrderWithTheirRanges() {
        PillHarness h;
        QWidget* page = h.page("displayPage3D");
        QVERIFY(page);
        const struct {
            const char* name;
            int min;
            int max;
        } rows[] = {
            {"pillDisplaySpectrumRenderMode", 0, 0},
            {"pillDisplay3DFloor", 0, 24},
            {"pillDisplay3DGain", 0, 100},
            {"pillDisplay3DSpan", 0, 100},
            {"pillDisplay3DAngle", 0, 100},
            {"pillDisplay3DSpeed", 0, 10},
            {"pillDisplay3DSliceShadow", 0, 0},
        };
        int last = 0;
        for (const auto& expected : rows) {
            const int row = gridRowOf(page, QString::fromLatin1(expected.name));
            QVERIFY2(row > last, expected.name);
            last = row;
            if (QSlider* slider = h.slider(expected.name)) {
                QCOMPARE(slider->minimum(), expected.min);
                QCOMPARE(slider->maximum(), expected.max);
            }
        }
        QCOMPARE(h.toggle("pillDisplaySpectrumRenderMode2D")->text(), QStringLiteral("2D Waterfall"));
        QCOMPARE(h.toggle("pillDisplaySpectrumRenderMode3D")->text(), QStringLiteral("3D Stacked Trace"));
    }

    // Opened on a pan with every row away from its default, each control
    // shows the pan's value and opening writes nothing.
    void openingSeedsEveryRowAndWritesNothing() {
        PillHarness h;
        setEveryRowAwayFromDefault(h.pan);
        QCoreApplication::processEvents();

        WriteSpies spies(h.pan);
        h.panel->openDisplayPill();
        QVERIFY(h.panel->displayPillVisible());
        QCOMPARE(spies.total(), 0);

        QCOMPARE(h.toggle("pillDisplayPanFill")->isChecked(), false);
        QCOMPARE(h.toggle("pillDisplayGridEnabled")->isChecked(), false);
        QCOMPARE(h.toggle("pillDisplayShowCursorFreq")->isChecked(), false);
        QCOMPARE(h.toggle("pillDisplayPeakHoldEnabled")->isChecked(), true);
        QVERIFY(h.toggle("pillDisplayFillColor")->styleSheet().contains(QStringLiteral("#123456")));
        QCOMPARE(h.slider("pillDisplayFftFillAlpha")->value(), 35);
        QCOMPARE(h.valueText("pillDisplayFftFillAlpha"), QStringLiteral("35%"));
        QCOMPARE(h.toggle("pillDisplayGradientEnabled")->isChecked(), true);
        QCOMPARE(h.toggle("pillDisplayShowNoiseFloor")->isChecked(), true);
        QCOMPARE(h.toggle("pillDisplayShowBinWidth")->isChecked(), true);
        QCOMPARE(h.toggle("pillDisplayShowPeakValueOverlay")->isChecked(), true);
        QVERIFY(h.toggle("pillDisplayTraceColor")->styleSheet().contains(QStringLiteral("#abcdef")));
        QCOMPARE(h.slider("pillDisplayLineWidth")->value(), 25);
        QCOMPARE(h.valueText("pillDisplayLineWidth"), QStringLiteral("2.5 px"));
        QCOMPARE(h.combo("pillDisplaySpectrumDetector")->currentText(), QStringLiteral("Sample"));
        QCOMPARE(h.combo("pillDisplaySpectrumAveraging")->currentText(), QStringLiteral("Recursive"));
        // 650 ms is past the slider's top: the handle is at the right end.
        QCOMPARE(h.slider("pillDisplaySpectrumAverageTimeMs")->value(), 500);
        QCOMPARE(h.box("pillDisplaySpectrumAverageTimeMs")->text(), QStringLiteral("650 ms"));
        QCOMPARE(h.slider("pillDisplayCalOffset")->value(), -9);
        QCOMPARE(h.valueText("pillDisplayCalOffset"), QStringLiteral("-4.5 dB"));
        QCOMPARE(h.slider("pillDisplayPeakHoldResetMs")->value(), 4200);
        QCOMPARE(h.valueText("pillDisplayPeakHoldResetMs"), QStringLiteral("4200 ms"));
        QCOMPARE(h.combo("pillDisplayFreqGridSpacing")->currentText(), QStringLiteral("25 kHz"));
        QCOMPARE(h.combo("pillDisplayFreqScaleFontPx")->currentText(), QStringLiteral("13 px"));
        QCOMPARE(h.toggle("pillDisplayBackgroundEnabled")->isChecked(), true);
        QCOMPARE(h.toggle("pillDisplayBackgroundEnabled")->text(), QStringLiteral("On"));
        QCOMPARE(h.slider("pillDisplayBackgroundOpacity")->value(), 42);
        QCOMPARE(h.valueText("pillDisplayBackgroundOpacity"), QStringLiteral("42%"));
        QVERIFY(h.toggle("pillDisplayBackgroundColor")->styleSheet().contains(QStringLiteral("#201008")));
        // dB Max and Min, before a band is given: the pan's range.
        QCOMPARE(h.slider("pillDisplayGridMax")->value(), qRound(h.pan.refLevel()));
        QCOMPARE(h.slider("pillDisplayGridMin")->value(),
                 qRound(h.pan.refLevel() - h.pan.dynamicRange()));

        QVERIFY(h.toggle("pillDisplaySpectrumRenderMode3D")->isChecked());
        QCOMPARE(h.slider("pillDisplay3DFloor")->value(), 11);
        QCOMPARE(h.valueText("pillDisplay3DFloor"), QStringLiteral("11 dB"));
        QCOMPARE(h.slider("pillDisplay3DGain")->value(), 33);
        QCOMPARE(h.valueText("pillDisplay3DGain"), QStringLiteral("33"));
        QCOMPARE(h.slider("pillDisplay3DSpan")->value(), 44);
        QCOMPARE(h.slider("pillDisplay3DAngle")->value(), 77);
        QCOMPARE(h.slider("pillDisplay3DSpeed")->value(), 4);
        QCOMPARE(h.valueText("pillDisplay3DSpeed"), QStringLiteral("4"));
        QCOMPARE(h.toggle("pillDisplay3DSliceShadow")->isChecked(), true);

        // Closing and opening again is as quiet.
        h.panel->setDisplayPillTab(Tab::View3D);
        QCOMPARE(spies.total(), 0);
    }

    // A fresh pan: the defaults read as the spec shows them.
    void defaultsReadAsTheSpecShowsThem() {
        PillHarness h;
        h.panel->openDisplayPill();
        QCOMPARE(h.valueText("pillDisplayFftFillAlpha"), QStringLiteral("70%"));
        QCOMPARE(h.valueText("pillDisplayLineWidth"), QStringLiteral("1.0 px"));
        QCOMPARE(h.valueText("pillDisplayCalOffset"), QStringLiteral("0.0 dB"));
        QCOMPARE(h.valueText("pillDisplayPeakHoldResetMs"), QStringLiteral("2000 ms"));
        QCOMPARE(h.combo("pillDisplayFreqGridSpacing")->currentText(), QStringLiteral("Auto"));
        QCOMPARE(h.combo("pillDisplayFreqScaleFontPx")->currentText(), QStringLiteral("10 px"));
        QCOMPARE(h.toggle("pillDisplayBackgroundEnabled")->text(), QStringLiteral("Off"));
        QCOMPARE(h.valueText("pillDisplay3DSpeed"), QStringLiteral("Match"));
        QVERIFY(h.toggle("pillDisplaySpectrumRenderMode2D")->isChecked());
    }

    // The Line row: 0.1 to 5.0 px in steps of 0.1, shown with one decimal.
    void lineWidthRunsInTenthsFromPointOneToFive() {
        PillHarness h;
        h.panel->openDisplayPill();
        QSlider* slider = h.slider("pillDisplayLineWidth");
        QVERIFY(slider);
        QCOMPARE(slider->minimum(), 1);
        QCOMPARE(slider->maximum(), 50);
        QCOMPARE(slider->singleStep(), 1);
        QCOMPARE(slider->value(), 10);

        slider->setValue(3);
        QCOMPARE(h.pan.lineWidth(), 0.3f);
        QCOMPARE(h.valueText("pillDisplayLineWidth"), QStringLiteral("0.3 px"));
        slider->setValue(slider->minimum());
        QCOMPARE(h.pan.lineWidth(), 0.1f);
        QCOMPARE(h.valueText("pillDisplayLineWidth"), QStringLiteral("0.1 px"));
        slider->setValue(slider->maximum());
        QCOMPARE(h.pan.lineWidth(), 5.0f);
        QCOMPARE(h.valueText("pillDisplayLineWidth"), QStringLiteral("5.0 px"));
        slider->setValue(15);
        QCOMPARE(h.pan.lineWidth(), 1.5f);
        QCOMPARE(h.valueText("pillDisplayLineWidth"), QStringLiteral("1.5 px"));

        // A width set on the pan lands on its tenth.
        h.pan.setLineWidth(0.3f);
        QCOMPARE(slider->value(), 3);
        h.pan.setLineWidth(4.7f);
        QCOMPARE(slider->value(), 47);
        QCOMPARE(h.valueText("pillDisplayLineWidth"), QStringLiteral("4.7 px"));
    }

    // While the pill is open, a change made on the pan (as Setup or the
    // right-click menu would) shows in the pill, and the pill does not
    // write it back: one change, one signal.
    void anOpenPillFollowsThePanWithNoEcho() {
        PillHarness h;
        h.panel->openDisplayPill();
        WriteSpies spies(h.pan);

        h.pan.setGridEnabled(false);
        QCOMPARE(h.toggle("pillDisplayGridEnabled")->isChecked(), false);
        QCOMPARE(spies.total(), 1);
        h.pan.setLineWidth(3.0f);
        QCOMPARE(h.slider("pillDisplayLineWidth")->value(), 30);
        QCOMPARE(h.valueText("pillDisplayLineWidth"), QStringLiteral("3.0 px"));
        QCOMPARE(spies.total(), 2);
        h.pan.setSpectrumAveraging(SpectrumAveraging::TimeWindow);
        QCOMPARE(h.combo("pillDisplaySpectrumAveraging")->currentText(), QStringLiteral("Time Window"));
        QCOMPARE(spies.total(), 3);
        h.pan.setTraceColor(QColor(QStringLiteral("#ff8800")));
        QVERIFY(h.toggle("pillDisplayTraceColor")->styleSheet().contains(QStringLiteral("#ff8800")));
        QCOMPARE(spies.total(), 4);
        h.pan.setFreqScaleFontPx(12);
        QCOMPARE(h.combo("pillDisplayFreqScaleFontPx")->currentText(), QStringLiteral("12 px"));
        QCOMPARE(spies.total(), 5);
        h.pan.setBackgroundOpacity(15);
        QCOMPARE(h.slider("pillDisplayBackgroundOpacity")->value(), 15);
        QCOMPARE(spies.total(), 6);

        // The fill rows report through the pan's model.
        const int before = spies.total();
        h.pan.setPanFillEnabled(false);
        QCOMPARE(h.toggle("pillDisplayPanFill")->isChecked(), false);
        QCOMPARE(spies.panFill.count(), 1);
        h.pan.setFillAlpha(0.2f);
        QCOMPARE(h.slider("pillDisplayFftFillAlpha")->value(), 20);
        QCOMPARE(spies.fillAlpha.count(), 1);
        QCOMPARE(spies.total(), before + 2);

        // The 3D rows have their own signals.
        h.pan.setSpectrumRenderMode(1);
        QVERIFY(h.toggle("pillDisplaySpectrumRenderMode3D")->isChecked());
        h.pan.setDssFloorDepth(9);
        QCOMPARE(h.slider("pillDisplay3DFloor")->value(), 9);
        h.pan.setDssGain(12);
        QCOMPARE(h.slider("pillDisplay3DGain")->value(), 12);
        h.pan.setDssRowSpan(13);
        QCOMPARE(h.slider("pillDisplay3DSpan")->value(), 13);
        h.pan.setDssAngle(14);
        QCOMPARE(h.slider("pillDisplay3DAngle")->value(), 14);
        h.pan.setDssRowDivider(7);
        QCOMPARE(h.slider("pillDisplay3DSpeed")->value(), 7);
        h.pan.setThreeDSliceDepth(true);
        QCOMPARE(h.toggle("pillDisplay3DSliceShadow")->isChecked(), true);
        QCOMPARE(spies.renderMode.count(), 1);
        QCOMPARE(spies.floor.count(), 1);
        QCOMPARE(spies.gain.count(), 1);
        QCOMPARE(spies.span.count(), 1);
        QCOMPARE(spies.angle.count(), 1);
        QCOMPARE(spies.speed.count(), 1);
        QCOMPARE(spies.shadow.count(), 1);
    }

    // A closed pill has let go of the pan, and reads it again when opened.
    void aClosedPillLetsGoAndReseedsWhenOpened() {
        PillHarness h;
        h.panel->openDisplayPill();
        QVERIFY(h.toggle("pillDisplayGridEnabled")->isChecked());
        // Another flyout takes the pill's place.
        QPushButton* att = h.panel->findChild<QPushButton*>(QStringLiteral("attMenuButton"));
        QVERIFY(att);
        att->click();
        QVERIFY(!h.panel->displayPillVisible());

        h.pan.setGridEnabled(false);
        QVERIFY(h.toggle("pillDisplayGridEnabled")->isChecked());   // not following
        h.panel->openDisplayPill();
        QVERIFY(h.panel->displayPillVisible());
        QVERIFY(!h.toggle("pillDisplayGridEnabled")->isChecked());
    }

    // Changing a control calls the pan's setter once.
    void changingAControlWritesThePanOnce() {
        PillHarness h;
        h.panel->openDisplayPill();
        WriteSpies spies(h.pan);
        int writes = 0;

        h.toggle("pillDisplayGridEnabled")->click();
        QCOMPARE(h.pan.gridEnabled(), false);
        QCOMPARE(spies.total(), ++writes);
        QCOMPARE(spies.announced.last().at(0).toString(), QStringLiteral("DisplayGridEnabled"));
        h.toggle("pillDisplayShowCursorFreq")->click();
        QCOMPARE(h.pan.cursorFreqVisible(), false);
        QCOMPARE(spies.total(), ++writes);
        h.toggle("pillDisplayPeakHoldEnabled")->click();
        QCOMPARE(h.pan.peakHoldEnabled(), true);
        h.toggle("pillDisplayGradientEnabled")->click();
        QCOMPARE(h.pan.gradientEnabled(), true);
        h.toggle("pillDisplayShowNoiseFloor")->click();
        QCOMPARE(h.pan.showNoiseFloor(), true);
        h.toggle("pillDisplayShowBinWidth")->click();
        QCOMPARE(h.pan.showBinWidth(), true);
        h.toggle("pillDisplayShowPeakValueOverlay")->click();
        QCOMPARE(h.pan.showPeakValueOverlay(), true);

        const auto writesOnce = [&spies](const char* key) {
            int count = 0;
            for (const QList<QVariant>& call : spies.announced) {
                if (call.at(0).toString() == QLatin1String(key)) {
                    ++count;
                }
            }
            return count == 1;
        };
        // Peak Hold has a signal of its own.
        QCOMPARE(spies.peakHold.count(), 1);
        QVERIFY(writesOnce("DisplayGradientEnabled"));
        QVERIFY(writesOnce("DisplayShowNoiseFloor"));
        QVERIFY(writesOnce("DisplayShowBinWidth"));
        QVERIFY(writesOnce("DisplayShowPeakValueOverlay"));

        // Away from the 1.0 px default (it was 1.5 when this moved to 1.0).
        h.slider("pillDisplayLineWidth")->setValue(15);
        QCOMPARE(h.pan.lineWidth(), 1.5f);
        QVERIFY(writesOnce("DisplayLineWidth"));
        h.slider("pillDisplaySpectrumAverageTimeMs")->setValue(420);
        QCOMPARE(h.pan.spectrumAverageTimeMs(), 420);
        QVERIFY(writesOnce("DisplaySpectrumAverageTimeMs"));
        h.slider("pillDisplayCalOffset")->setValue(7);
        QCOMPARE(h.pan.dbmCalOffset(), 3.5f);
        QCOMPARE(h.valueText("pillDisplayCalOffset"), QStringLiteral("3.5 dB"));
        QVERIFY(writesOnce("DisplayCalOffset"));
        h.slider("pillDisplayPeakHoldResetMs")->setValue(700);
        QCOMPARE(h.pan.peakHoldDelayMs(), 700);
        QVERIFY(writesOnce("DisplayPeakHoldResetMs"));
        h.combo("pillDisplaySpectrumDetector")->setCurrentIndex(4);
        QCOMPARE(static_cast<int>(h.pan.spectrumDetector()), 4);
        QVERIFY(writesOnce("DisplaySpectrumDetector"));
        h.combo("pillDisplaySpectrumAveraging")->setCurrentIndex(0);
        QCOMPARE(h.pan.spectrumAveraging(), SpectrumAveraging::None);
        QVERIFY(writesOnce("DisplaySpectrumAveraging"));
        h.combo("pillDisplayFreqGridSpacing")->setCurrentIndex(3);
        QCOMPARE(h.pan.freqGridSpacingHz(), 5000);
        QVERIFY(writesOnce("DisplayFreqGridSpacing"));
        h.combo("pillDisplayFreqScaleFontPx")->setCurrentIndex(6);
        QCOMPARE(h.pan.freqScaleFontPx(), 14);
        QVERIFY(writesOnce("DisplayFreqScaleFontPx"));

        // Fill and its opacity report through the model.
        h.toggle("pillDisplayPanFill")->click();
        QCOMPARE(h.pan.panFillEnabled(), false);
        QCOMPARE(spies.panFill.count(), 1);
        h.slider("pillDisplayFftFillAlpha")->setValue(25);
        QCOMPARE(h.pan.fillAlpha(), 0.25f);
        QCOMPARE(spies.fillAlpha.count(), 1);

        // 3D View.
        h.toggle("pillDisplaySpectrumRenderMode3D")->click();
        QCOMPARE(h.pan.spectrumRenderMode(), 1);
        QCOMPARE(spies.renderMode.count(), 1);
        h.slider("pillDisplay3DFloor")->setValue(20);
        QCOMPARE(h.pan.dssFloorDepth(), 20);
        QCOMPARE(spies.floor.count(), 1);
        h.slider("pillDisplay3DGain")->setValue(21);
        QCOMPARE(h.pan.dssGain(), 21);
        QCOMPARE(spies.gain.count(), 1);
        h.slider("pillDisplay3DSpan")->setValue(22);
        QCOMPARE(h.pan.dssRowSpan(), 22);
        QCOMPARE(spies.span.count(), 1);
        h.slider("pillDisplay3DAngle")->setValue(23);
        QCOMPARE(h.pan.dssAngle(), 23);
        QCOMPARE(spies.angle.count(), 1);
        h.slider("pillDisplay3DSpeed")->setValue(3);
        QCOMPARE(h.pan.dssRowDivider(), 3);
        QCOMPARE(spies.speed.count(), 1);
        h.toggle("pillDisplay3DSliceShadow")->click();
        QCOMPARE(h.pan.threeDSliceDepth(), true);
        QCOMPARE(spies.shadow.count(), 1);
    }

    // Rows that cannot act are greyed with the reason and stay shown.
    void rowsThatCannotActAreGreyedAndStillShown() {
        PillHarness h;
        h.panel->openDisplayPill();

        // Peak Delay follows Peak Hold.
        QSlider* delay = h.slider("pillDisplayPeakHoldResetMs");
        QVERIFY(!h.pan.peakHoldEnabled());
        QVERIFY(!delay->isEnabled());
        QVERIFY(!delay->isHidden());
        QVERIFY(!delay->toolTip().isEmpty());
        const QString heldTip = delay->toolTip();
        h.toggle("pillDisplayPeakHoldEnabled")->click();
        QVERIFY(delay->isEnabled());
        QVERIFY(delay->toolTip() != heldTip);
        h.pan.setPeakHoldEnabled(false);          // from elsewhere
        QVERIFY(!delay->isEnabled());
        QCOMPARE(delay->toolTip(), heldTip);

        // In the 3D view, Span and Slice Shadow follow what the renderer can draw.
        h.pan.setSpectrumRenderMode(1);
        QSlider* span = h.slider("pillDisplay3DSpan");
        QPushButton* shadow = h.toggle("pillDisplay3DSliceShadow");
        QVERIFY(span->isEnabled());
        QVERIFY(shadow->isEnabled());
        h.panel->setGpuCapabilities(false, false);
        QVERIFY(!span->isEnabled());
        QVERIFY(!shadow->isEnabled());
        QVERIFY(!span->isHidden());
        QVERIFY(!shadow->isHidden());
        QVERIFY(h.slider("pillDisplay3DGain")->isEnabled());
        h.panel->setGpuCapabilities(true, false);
        QVERIFY(span->isEnabled());
        QVERIFY(!shadow->isEnabled());
        h.panel->setGpuCapabilities(true, true);
        QVERIFY(shadow->isEnabled());

        // Cal Offset is the Core's in a remote window.
        QSlider* cal = h.slider("pillDisplayCalOffset");
        QVERIFY(cal->isEnabled());
        const QString calTip = cal->toolTip();
        h.panel->setRemoteWindow(true, false);
        QVERIFY(!cal->isEnabled());
        QVERIFY(!cal->isHidden());
        QCOMPARE(cal->toolTip(),
                 QStringLiteral("The Core calibrates the display for its radio."));
        // The reason survives a re-seed.
        h.pan.setGridEnabled(false);
        emit h.pan.panDisplayReset();
        QVERIFY(!cal->isEnabled());
        h.panel->setRemoteWindow(false, false);
        QVERIFY(cal->isEnabled());
        QCOMPARE(cal->toolTip(), calTip);
    }

    // The colour buttons open their chooser on the pan's colour, and set
    // what was chosen.
    void colourButtonsOpenOnThePansColours() {
        PillHarness h;
        h.pan.setFillColor(QColor(QStringLiteral("#113355")));
        h.pan.setTraceColor(QColor(QStringLiteral("#557799")));
        h.pan.setBackgroundFillColor(QColor(QStringLiteral("#0b0c0d")));
        h.pan.setBackgroundEnabled(true);
        h.panel->openDisplayPill();

        QList<QColor> opened;
        QStringList titles;
        QColor answer;
        h.panel->setPillColorChooser([&](const QColor& initial, const QString& title) {
            opened << initial;
            titles << title;
            return answer;
        });

        answer = QColor(QStringLiteral("#aa0000"));
        h.toggle("pillDisplayFillColor")->click();
        QCOMPARE(opened.last(), QColor(QStringLiteral("#113355")));
        QCOMPARE(h.pan.fillColor(), QColor(QStringLiteral("#aa0000")));
        QVERIFY(h.toggle("pillDisplayFillColor")->styleSheet().contains(QStringLiteral("#aa0000")));
        // The second time it opens on the colour just chosen.
        answer = QColor();   // cancelled
        h.toggle("pillDisplayFillColor")->click();
        QCOMPARE(opened.last(), QColor(QStringLiteral("#aa0000")));
        QCOMPARE(h.pan.fillColor(), QColor(QStringLiteral("#aa0000")));

        answer = QColor(QStringLiteral("#00bb00"));
        h.toggle("pillDisplayTraceColor")->click();
        QCOMPARE(opened.last(), QColor(QStringLiteral("#557799")));
        QCOMPARE(h.pan.traceColor(), QColor(QStringLiteral("#00bb00")));
        // The fill colour is its own.
        QCOMPARE(h.pan.fillColor(), QColor(QStringLiteral("#aa0000")));

        answer = QColor(QStringLiteral("#0000cc"));
        h.toggle("pillDisplayBackgroundColor")->click();
        QCOMPARE(opened.last(), QColor(QStringLiteral("#0b0c0d")));
        QCOMPARE(h.pan.backgroundFillColor(), QColor(QStringLiteral("#0000cc")));

        QCOMPARE(opened.size(), 4);
        for (const QString& title : titles) {
            QVERIFY(!title.isEmpty());
        }
    }

    // dB Max and dB Min show the band's stored values and ask to change
    // them; the pill writes nothing to the pan.
    void bandGridRowsShowTheBandAndAsk() {
        PillHarness h;
        h.panel->openDisplayPill();
        auto* note = h.control<QLabel>("pillBandGridNote");
        QVERIFY(note);
        QCOMPARE(note->text(), QStringLiteral("dB Max and Min are kept per band."));

        h.panel->setBandGrid(QStringLiteral("20 m"), -40, -140);
        QCOMPARE(h.slider("pillDisplayGridMax")->value(), -40);
        QCOMPARE(h.slider("pillDisplayGridMin")->value(), -140);
        QCOMPARE(h.valueText("pillDisplayGridMax"), QStringLiteral("-40 dBm"));
        QCOMPARE(h.valueText("pillDisplayGridMin"), QStringLiteral("-140 dBm"));
        QCOMPARE(note->text(), QStringLiteral("dB Max and Min are kept per band (20 m)."));

        // The pan's live range moving (the noise floor follow) is not the
        // band's stored value.
        h.pan.setDbmRange(-150.0f, -30.0f);
        QCOMPARE(h.slider("pillDisplayGridMax")->value(), -40);
        QCOMPARE(h.slider("pillDisplayGridMin")->value(), -140);

        QSignalSpy maxAsked(h.panel, &SpectrumOverlayPanel::perBandDbMaxRequested);
        QSignalSpy minAsked(h.panel, &SpectrumOverlayPanel::perBandDbMinRequested);
        WriteSpies spies(h.pan);
        h.slider("pillDisplayGridMax")->setValue(-55);
        QCOMPARE(maxAsked.count(), 1);
        QCOMPARE(maxAsked.last().at(0).toInt(), -55);
        QCOMPARE(h.valueText("pillDisplayGridMax"), QStringLiteral("-55 dBm"));
        h.slider("pillDisplayGridMin")->setValue(-120);
        QCOMPARE(minAsked.count(), 1);
        QCOMPARE(minAsked.last().at(0).toInt(), -120);
        QCOMPARE(spies.total(), 0);
        QCOMPARE(h.pan.refLevel(), -30.0f);

        // Showing a band's values asks for nothing.
        h.panel->setBandGrid(QStringLiteral("40 m"), -50, -130);
        QCOMPARE(maxAsked.count(), 1);
        QCOMPARE(minAsked.count(), 1);
        QCOMPARE(note->text(), QStringLiteral("dB Max and Min are kept per band (40 m)."));
        QCOMPARE(h.slider("pillDisplayGridMax")->value(), -50);
    }

    // Background: Choose..., Clear, the Off pill, BG Opacity and BG Color.
    void backgroundRowsDriveThePan() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString picture = dir.filePath(QStringLiteral("picture.png"));
        QImage image(4, 4, QImage::Format_RGB32);
        image.fill(Qt::darkBlue);
        QVERIFY(image.save(picture));

        PillHarness h;
        h.panel->openDisplayPill();
        QPushButton* shown = h.toggle("pillDisplayBackgroundEnabled");
        QVERIFY(!h.pan.backgroundEnabled());
        QCOMPARE(shown->text(), QStringLiteral("Off"));

        QStringList startDirs;
        QString answer;
        h.panel->setPillPictureChooser([&](const QString& startDir) {
            startDirs << startDir;
            return answer;
        });

        // Cancelled: nothing changes.
        h.toggle("pillDisplayBackgroundImage")->click();
        QCOMPARE(startDirs.size(), 1);
        QVERIFY(h.pan.backgroundImagePath().isEmpty());
        QVERIFY(!h.pan.backgroundEnabled());

        // Chosen: the picture is set and shown.
        answer = picture;
        h.toggle("pillDisplayBackgroundImage")->click();
        QCOMPARE(h.pan.backgroundImagePath(), picture);
        QVERIFY(h.pan.backgroundEnabled());
        QVERIFY(shown->isChecked());
        QCOMPARE(shown->text(), QStringLiteral("On"));
        // The next time the chooser starts where the picture is.
        h.toggle("pillDisplayBackgroundImage")->click();
        QCOMPARE(startDirs.last(), QFileInfo(picture).absolutePath());

        // Off hides it and keeps it chosen.
        shown->click();
        QVERIFY(!h.pan.backgroundEnabled());
        QCOMPARE(h.pan.backgroundImagePath(), picture);
        QCOMPARE(shown->text(), QStringLiteral("Off"));
        shown->click();
        QVERIFY(h.pan.backgroundEnabled());

        // Clear removes the picture.
        h.toggle("pillDisplayBackgroundImageClear")->click();
        QVERIFY(h.pan.backgroundImagePath().isEmpty());

        h.slider("pillDisplayBackgroundOpacity")->setValue(33);
        QCOMPARE(h.pan.backgroundOpacity(), 33);
        QCOMPARE(h.valueText("pillDisplayBackgroundOpacity"), QStringLiteral("33%"));

        h.panel->setPillColorChooser([](const QColor&, const QString&) {
            return QColor(QStringLiteral("#102030"));
        });
        h.toggle("pillDisplayBackgroundColor")->click();
        QCOMPARE(h.pan.backgroundFillColor(), QColor(QStringLiteral("#102030")));
    }

    // After the pan's reset the pill reads every row again (the reset
    // announces nothing row by row).
    void aResetPanIsReadAgain() {
        PillHarness h;
        const bool gridDefault = h.pan.gridEnabled();
        const float lineDefault = h.pan.lineWidth();
        const int fontDefault = h.pan.freqScaleFontPx();
        setEveryRowAwayFromDefault(h.pan);
        h.panel->openDisplayPill();
        QCOMPARE(h.toggle("pillDisplayGridEnabled")->isChecked(), !gridDefault);

        h.pan.resetPanDisplayOverrides();
        QCOMPARE(h.pan.gridEnabled(), gridDefault);
        QCOMPARE(h.toggle("pillDisplayGridEnabled")->isChecked(), gridDefault);
        QCOMPARE(h.slider("pillDisplayLineWidth")->value(), qRound(lineDefault * 10.0f));
        QCOMPARE(h.combo("pillDisplayFreqScaleFontPx")->currentText(),
                 QStringLiteral("%1 px").arg(fontDefault));
        QCOMPARE(h.toggle("pillDisplayPeakHoldEnabled")->isChecked(), h.pan.peakHoldEnabled());
        QCOMPARE(h.slider("pillDisplayPeakHoldResetMs")->isEnabled(), h.pan.peakHoldEnabled());
        QCOMPARE(h.slider("pillDisplayFftFillAlpha")->value(), qRound(h.pan.fillAlpha() * 100.0f));
        QCOMPARE(h.toggle("pillDisplayPanFill")->isChecked(), h.pan.panFillEnabled());
        QCOMPARE(h.slider("pillDisplay3DGain")->value(), h.pan.dssGain());
    }

    // The wheel over the pill goes nowhere: it is accepted, it moves no
    // slider that was not clicked or tabbed to, and it does not reach the
    // pan underneath.
    void theWheelOverThePillGoesNowhere() {
        PillHarness h;
        h.panel->openDisplayPill();
        const double centre = h.pan.centerFrequency();
        WriteSpies spies(h.pan);

        QSlider* fill = h.slider("pillDisplayFftFillAlpha");
        const int fillBefore = fill->value();
        QVERIFY(!fill->hasFocus());
        // The wheel must not hand a slider the focus either.
        QVERIFY(!(fill->focusPolicy() & Qt::WheelFocus & ~Qt::StrongFocus));
        QVERIFY(sendWheel(fill));
        QCOMPARE(fill->value(), fillBefore);

        QComboBox* detector = h.combo("pillDisplaySpectrumDetector");
        const int detectorBefore = detector->currentIndex();
        QVERIFY(sendWheel(detector));
        QCOMPARE(detector->currentIndex(), detectorBefore);

        QVERIFY(sendWheel(h.pill()));
        QVERIFY(sendWheel(h.page("displayPageSpectrum")));
        QVERIFY(sendWheel(h.toggle("pillDisplayGridEnabled")));
        QVERIFY(sendWheel(h.control<QLabel>("pillBandGridNote")));
        QVERIFY(sendWheel(h.toggle("displayTabFft")));
        // A disabled row is no way through either.
        QVERIFY(!h.slider("pillDisplayPeakHoldResetMs")->isEnabled());
        QVERIFY(sendWheel(h.slider("pillDisplayPeakHoldResetMs")));

        // Every child, on every tab.
        for (const Tab tab : {Tab::Spectrum, Tab::Levels, Tab::Fft, Tab::Waterfall, Tab::View3D}) {
            h.panel->setDisplayPillTab(tab);
            QScrollArea* scroll = h.scroll();
            QVERIFY(scroll);
            for (QWidget* child : h.pill()->findChildren<QWidget*>()) {
                if (child->window() != h.pill()->window()) {
                    continue;   // a combo's own list, a window of its own
                }
                if (child == scroll->viewport() || child == scroll->verticalScrollBar()) {
                    continue;   // wheel input belongs to the containment scroller
                }
                QVERIFY2(sendWheel(child), qPrintable(child->objectName()));
            }
            QVERIFY(!sendWheel(scroll->viewport()));
        }
        for (QSlider* slider : h.pill()->findChildren<QSlider*>()) {
            QVERIFY2(!(slider->focusPolicy() & Qt::WheelFocus & ~Qt::StrongFocus),
                     qPrintable(slider->objectName()));
        }

        QCOMPARE(h.pan.centerFrequency(), centre);
        QCOMPARE(spies.total(), 0);
        QCOMPARE(fill->value(), fillBefore);
    }

    // The pill opens on the tab last used, after the strip is rebuilt.
    void theLastTabIsKeptAcrossARebuild() {
        {
            PillHarness h;
            QCOMPARE(h.panel->displayPillTab(), Tab::Spectrum);
            h.toggle("displayTab3D")->click();
            QCOMPARE(AppSettings::instance().value(QStringLiteral("DisplayPillLastTab")).toString(),
                     QStringLiteral("View3D"));
        }
        {
            PillHarness h;
            QCOMPARE(h.panel->displayPillTab(), Tab::View3D);
            QVERIFY(h.toggle("displayTab3D")->isChecked());
            QVERIFY(!h.toggle("displayTabSpectrum")->isChecked());
            QVERIFY(!h.page("displayPage3D")->isHidden());
            QVERIFY(h.page("displayPageSpectrum")->isHidden());
            h.panel->setDisplayPillTab(Tab::Waterfall);
        }
        {
            PillHarness h;
            QCOMPARE(h.panel->displayPillTab(), Tab::Waterfall);
        }
        // A value this build does not know is the first tab.
        AppSettings::instance().setValue(QStringLiteral("DisplayPillLastTab"),
                                         QStringLiteral("Somewhere"));
        PillHarness h;
        QCOMPARE(h.panel->displayPillTab(), Tab::Spectrum);
    }

    // Rulings wave R8: the page follows a tab button however it is
    // activated. A press sent through macOS accessibility changes the
    // checked state without `clicked`; toggle() and setChecked() do the same.
    void aTabButtonSwitchesThePageHoweverItIsActivated() {
        PillHarness h;
        h.panel->openDisplayPill();
        const char* tabNames[] = {"displayTabSpectrum", "displayTabLevels",
                                  "displayTabWaterfall", "displayTab3D", "displayTabFft"};
        const char* pageNames[] = {"displayPageSpectrum", "displayPageLevels",
                                   "displayPageWaterfall", "displayPage3D", "displayPageFft"};
        const auto onlyShows = [&](int index) {
            for (int i = 0; i < 5; ++i) {
                if (h.toggle(tabNames[i])->isChecked() != (i == index)
                    || h.page(pageNames[i])->isHidden() != (i != index)) {
                    return false;
                }
            }
            return h.panel->displayPillTab() == static_cast<Tab>(index);
        };
        const auto lastTab = []() {
            return AppSettings::instance().value(QStringLiteral("DisplayPillLastTab")).toString();
        };
        QVERIFY(onlyShows(0));

        // toggle(): what an accessibility press does.
        h.toggle("displayTabWaterfall")->toggle();
        QVERIFY(onlyShows(2));
        QCOMPARE(lastTab(), QStringLiteral("Waterfall"));
        // setChecked(true).
        h.toggle("displayTabFft")->setChecked(true);
        QVERIFY(onlyShows(4));
        QCOMPARE(lastTab(), QStringLiteral("Fft"));
        // Every tab in turn, so all five cannot end up lit.
        for (int i = 0; i < 5; ++i) {
            h.toggle(tabNames[i])->toggle();
            QVERIFY2(onlyShows(i), tabNames[i]);
        }
        QVERIFY(onlyShows(4));
        h.toggle("displayTab3D")->click();
        QVERIFY(onlyShows(3));

        // The open tab's own button cannot be unchecked: by a click, by
        // toggle() or by setChecked(false).
        h.toggle("displayTab3D")->click();
        QVERIFY(onlyShows(3));
        h.toggle("displayTab3D")->toggle();
        QVERIFY(onlyShows(3));
        h.toggle("displayTab3D")->setChecked(false);
        QVERIFY(onlyShows(3));
        QCOMPARE(lastTab(), QStringLiteral("View3D"));

        // A mouse click behaves as it did, and saves the tab.
        h.toggle("displayTabSpectrum")->click();
        QVERIFY(onlyShows(0));
        QCOMPARE(lastTab(), QStringLiteral("Spectrum"));
        // The panel's own call still works and is saved once by itself.
        h.panel->setDisplayPillTab(Tab::Waterfall);
        QVERIFY(onlyShows(2));
        QCOMPARE(lastTab(), QStringLiteral("Waterfall"));
    }

    // 268 px wide; every tab fits a tall pan without needing page scrolling.
    void thePillIs268WideAndTheSpectrumTabFitsIn480() {
        PillHarness h;
        h.panel->setDisplayPillTab(Tab::Spectrum);
        h.panel->openDisplayPill();
        QWidget* pill = h.pill();
        QCOMPARE(pill->width(), 268);
        QCOMPARE(pill->minimumWidth(), 268);
        QCOMPARE(pill->maximumWidth(), 268);
        // The rows fit that width: nothing is squeezed or cut off.
        QVERIFY2(pill->layout()->minimumSize().width() <= 268,
                 qPrintable(QString::number(pill->layout()->minimumSize().width())));
        QVERIFY2(pill->height() <= 480, qPrintable(QString::number(pill->height())));
        QVERIFY(h.scroll());
        QCOMPARE(h.scroll()->verticalScrollBar()->maximum(), 0);
        QVERIFY(h.pan.rect().contains(pill->geometry()));
        qInfo() << "Spectrum tab height" << pill->height();

        for (const Tab tab : {Tab::Levels, Tab::Fft, Tab::Waterfall, Tab::View3D}) {
            h.panel->setDisplayPillTab(tab);
            QCOMPARE(pill->width(), 268);
            QVERIFY2(pill->height() <= 480, qPrintable(QString::number(pill->height())));
            QCOMPARE(h.scroll()->verticalScrollBar()->maximum(), 0);
            QVERIFY(h.pan.rect().contains(pill->geometry()));
            QVERIFY2(pill->layout()->minimumSize().width() <= 268,
                     qPrintable(QString::number(pill->layout()->minimumSize().width())));
        }
    }

    // No string the pill shows carries an em dash, and its tooltips are in
    // plain operator words.
    void pillStringsHaveNoEmDash() {
        PillHarness h;
        h.panel->setRemoteWindow(true, false);
        h.panel->setGpuCapabilities(false, false);
        h.panel->openDisplayPill();
        QStringList strings;
        for (QWidget* child : h.pill()->findChildren<QWidget*>()) {
            strings << child->toolTip();
            if (auto* label = qobject_cast<QLabel*>(child)) {
                strings << label->text();
            } else if (auto* button = qobject_cast<QAbstractButton*>(child)) {
                strings << button->text();
            } else if (auto* combo = qobject_cast<QComboBox*>(child)) {
                for (int i = 0; i < combo->count(); ++i) {
                    strings << combo->itemText(i);
                }
            }
        }
        strings.removeAll(QString());
        QVERIFY(strings.size() > 60);
        for (const QString& text : std::as_const(strings)) {
            QVERIFY2(!text.contains(QChar(0x2014)), qPrintable(text));
            QVERIFY2(!text.contains(QStringLiteral("From ")), qPrintable(text));
        }
    }

    // A strip on a plain widget has no pan to edit: its rows are shown,
    // greyed, and write nothing.
    void aPillWithNoPanIsGreyedNotHidden() {
        PanelHarness h;
        QCOMPARE(h.panel->displayTarget(), nullptr);
        auto* grid = h.host.findChild<QPushButton*>(QStringLiteral("pillDisplayGridEnabled"));
        auto* fill = h.host.findChild<QSlider*>(QStringLiteral("pillDisplayFftFillAlpha"));
        QVERIFY(grid && fill);
        QVERIFY(!grid->isEnabled());
        QVERIFY(!fill->isEnabled());
        QVERIFY(!grid->isHidden());

        // Given a pan, they come alive on it.
        SpectrumWidget pan;
        pan.setGridEnabled(false);
        h.panel->setDisplayTarget(&pan);
        QCOMPARE(h.panel->displayTarget(), &pan);
        QVERIFY(grid->isEnabled());
        QVERIFY(!grid->isChecked());
        grid->click();
        QVERIFY(pan.gridEnabled());
        h.panel->setDisplayTarget(nullptr);
        QVERIFY(!grid->isEnabled());
    }

    // ── Display pill plan Task 17: the Waterfall and FFT tabs ────────────

    // The Waterfall tab's rows, in the order of the design's table 3.5.
    void waterfallTabRowsInSpecOrderWithTheirRanges() {
        PillHarness h;
        QWidget* page = h.page("displayPageWaterfall");
        QVERIFY(page);
        const char* order[] = {
            "pillDisplayWfColorScheme", "pillDisplayWfUpdatePeriodMs", "pillDisplayWfOpacity",
            "pillWaterfallBlankingThresholdDb", "pillDisplayWaterfallAveraging",
            "pillDisplayWaterfallAverageTimeMs",
            "pillWaterfallStopOnTx", "pillDisplayShowRxFilterOnWaterfall",
            "pillDisplayShowTxFilterOnRxWaterfall", "pillDisplayShowRxZeroLine",
            "pillDisplayShowTxZeroLine", "pillDisplayWfTimestampPos",
            "pillDisplayWaterfallHistoryMs",
        };
        int last = 0;
        for (const char* name : order) {
            const int row = gridRowOf(page, QString::fromLatin1(name));
            QVERIFY2(row >= last, name);
            last = row;
        }
        // Each of these rows is its own.
        const char* ownRow[] = {
            "pillDisplayWfColorScheme", "pillDisplayWfUpdatePeriodMs", "pillDisplayWfOpacity",
            "pillWaterfallBlankingThresholdDb", "pillDisplayWaterfallAveraging",
            "pillDisplayWaterfallAverageTimeMs",
            "pillWaterfallStopOnTx", "pillDisplayShowRxZeroLine",
            "pillDisplayWfTimestampPos", "pillDisplayWaterfallHistoryMs",
        };
        int previous = -1;
        for (const char* name : ownRow) {
            const int row = gridRowOf(page, QString::fromLatin1(name));
            QVERIFY2(row > previous, name);
            previous = row;
        }
        // The badge, the On pill and Re-tune share the Clarity row; an
        // Off/On pill shares its slider's row.
        QCOMPARE(gridRowOf(h.page("displayPageLevels"), QStringLiteral("pillClarityBadge")),
                 gridRowOf(h.page("displayPageLevels"), QStringLiteral("pillClarityRetune")));
        QCOMPARE(gridRowOf(h.page("displayPageLevels"), QStringLiteral("pillClarityRetune")),
                 gridRowOf(h.page("displayPageLevels"), QStringLiteral("pillClarityRetune")));
        QVERIFY(gridRowOf(h.page("displayPageLevels"), QStringLiteral("pillWaterfallAGCOffsetDb"))
                > gridRowOf(h.page("displayPageLevels"), QStringLiteral("pillWaterfallLevelsMode")));
        QCOMPARE(gridRowOf(page, QStringLiteral("pillWaterfallBlankingEnabled")),
                 gridRowOf(page, QStringLiteral("pillWaterfallBlankingThresholdDb")));
        QCOMPARE(gridRowOf(page, QStringLiteral("pillDisplayShowTxFilterOnRxWaterfall")),
                 gridRowOf(page, QStringLiteral("pillWaterfallStopOnTx")));
        QCOMPARE(gridRowOf(page, QStringLiteral("pillDisplayShowTxZeroLine")),
                 gridRowOf(page, QStringLiteral("pillDisplayShowRxZeroLine")));

        const struct { const char* name; int min; int max; } ranges[] = {
            {"pillDisplayWfBlackLevel", 0, 100},
            {"pillDisplayWfColorGain", 0, 100},
            {"pillWaterfallAGCOffsetDb", -60, 60},
            {"pillDisplayWfLowLevel", -200, 0},
            {"pillDisplayWfHighLevel", -200, 0},
            {"pillDisplayWfUpdatePeriodMs", 10, 500},
            {"pillWaterfallBlankingThresholdDb", 1, 20},
            {"pillDisplayWaterfallAverageTimeMs", 1, 500},
            {"pillDisplayWfOpacity", 0, 100},
        };
        for (const auto& range : ranges) {
            QSlider* slider = h.slider(range.name);
            QVERIFY2(slider, range.name);
            QCOMPARE(slider->minimum(), range.min);
            QCOMPARE(slider->maximum(), range.max);
        }

        QSlider* rate = h.slider("pillDisplayWfUpdatePeriodMs");
        QCOMPARE(rate->singleStep(), 1);
        rate->setValue(10);
        rate->triggerAction(QAbstractSlider::SliderSingleStepAdd);
        QCOMPARE(h.pan.wfUpdatePeriodMs(), 11);
        rate->triggerAction(QAbstractSlider::SliderSingleStepAdd);
        QCOMPARE(h.pan.wfUpdatePeriodMs(), 12);

        QStringList averaging;
        for (int i = 0; i < h.combo("pillDisplayWaterfallAveraging")->count(); ++i) {
            averaging << h.combo("pillDisplayWaterfallAveraging")->itemText(i);
        }
        QCOMPARE(averaging, QStringList({QStringLiteral("None"), QStringLiteral("Recursive"),
                                         QStringLiteral("Time Window"),
                                         QStringLiteral("Log Recursive")}));
        QStringList stamps;
        for (int i = 0; i < h.combo("pillDisplayWfTimestampPos")->count(); ++i) {
            stamps << h.combo("pillDisplayWfTimestampPos")->itemText(i);
        }
        QCOMPARE(stamps, QStringList({QStringLiteral("None"), QStringLiteral("Left"),
                                      QStringLiteral("Right")}));
        QStringList rewind;
        for (int i = 0; i < h.combo("pillDisplayWaterfallHistoryMs")->count(); ++i) {
            rewind << h.combo("pillDisplayWaterfallHistoryMs")->itemText(i);
        }
        QCOMPARE(rewind, QStringList({QStringLiteral("60 seconds"), QStringLiteral("5 minutes"),
                                      QStringLiteral("15 minutes"), QStringLiteral("20 minutes")}));
    }

    // The FFT tab's rows, in the order of the design's table 3.4.
    void fftTabRowsInSpecOrderWithTheirRanges() {
        PillHarness h;
        QWidget* page = h.page("displayPageFft");
        QVERIFY(page);
        // FFT sizing mode (design D38): "Sizing:" is the first row, and the
        // Hz/bin row has no Off/On button. The three sizing rows sit
        // together, with the running numbers under the pair.
        const char* order[] = {
            "pillDisplayFftSizingModeAuto",
            "pillDisplayFftSize", "pillDisplayHzPerBinTarget", "pillFftReadout",
            "pillDisplaySpectrumFps", "pillDisplayFftWindow", "pillDisplayDecimation",
            "pillStationNote",
        };
        int previous = -1;
        for (const char* name : order) {
            const int row = gridRowOf(page, QString::fromLatin1(name));
            QVERIFY2(row > previous, name);
            previous = row;
        }
        QVERIFY(!page->findChild<QWidget*>(QStringLiteral("pillDisplayHzPerBinTargetOn")));
        const struct { const char* name; const char* text; } sizing[] = {
            {"pillDisplayFftSizingModeAuto", "Auto"},
            {"pillDisplayFftSizingModeHzPerBin", "Hz/bin"},
            {"pillDisplayFftSizingModeFixed", "Fixed"},
        };
        for (const auto& choice : sizing) {
            QPushButton* button = h.toggle(choice.name);
            QVERIFY2(button, choice.name);
            QCOMPARE(button->text(), QLatin1String(choice.text));
            QVERIFY(button->isCheckable());
            QCOMPARE(gridRowOf(page, QString::fromLatin1(choice.name)),
                     gridRowOf(page, QStringLiteral("pillDisplayFftSizingModeAuto")));
        }
        QStringList labels;
        for (QLabel* label : page->findChildren<QLabel*>()) {
            labels << label->text();
        }
        QVERIFY(!labels.contains(QStringLiteral("Sizing:")));
        for (const char* label : {"FFT Size:", "Hz/bin:"}) {
            QVERIFY2(labels.contains(QLatin1String(label)), label);
        }

        // The Hz/bin slider is 0.5 Hz steps from one step (0.5 Hz) to 200;
        // with no target remembered and nothing running it shows the
        // coarsest.
        const struct { const char* name; int min; int max; int value; } ranges[] = {
            {"pillDisplayFftSize", 0, 6, 0},
            {"pillDisplaySpectrumFps", 10, 60, 30},
            {"pillDisplayHzPerBinTarget", 1, 400, 400},
            {"pillDisplayDecimation", 1, 16, 1},
        };
        for (const auto& range : ranges) {
            QSlider* slider = h.slider(range.name);
            QVERIFY2(slider, range.name);
            QCOMPARE(slider->minimum(), range.min);
            QCOMPARE(slider->maximum(), range.max);
            QCOMPARE(slider->value(), range.value);
            QCOMPARE(slider->singleStep(), 1);
        }
        // Positions 0 to 6 are 4096 to 262144, the stored size in Fixed.
        QSlider* size = h.slider("pillDisplayFftSize");
        const int sizes[] = {4096, 8192, 16384, 32768, 65536, 131072, 262144};
        for (int position = 0; position <= 6; ++position) {
            SpectrumOverlayPanel::DisplayPillFftState state = h.panel->fftState();
            state.sizingMode = ControlRanges::DisplayFftSizingMode::Fixed;
            state.baselineSize = sizes[position];
            h.panel->setFftState(state);
            QCOMPARE(size->value(), position);
            QCOMPARE(h.valueText("pillDisplayFftSize"), QString::number(sizes[position]));
        }

        QComboBox* window = h.combo("pillDisplayFftWindow");
        QStringList windows;
        for (int i = 0; i < window->count(); ++i) {
            windows << window->itemText(i);
        }
        QCOMPARE(windows, QStringList({QStringLiteral("Rectangular"),
                                       QStringLiteral("Blackman-Harris 4T"),
                                       QStringLiteral("Hann"), QStringLiteral("Flat-Top"),
                                       QStringLiteral("Hamming"), QStringLiteral("Kaiser"),
                                       QStringLiteral("Blackman-Harris 7T")}));
        QCOMPARE(window->currentText(), QStringLiteral("Blackman-Harris 4T"));
        QCOMPARE(h.valueText("pillDisplayHzPerBinTarget"), QStringLiteral("200.0 Hz"));

        // The header says whose these are.
        auto* scope = page->findChild<QLabel*>(QStringLiteral("pillHeaderScope"));
        QVERIFY(scope);
        QCOMPARE(scope->text(), QStringLiteral("CORE"));
    }

    void schemeListsAllEightAndShowsThePans() {
        PillHarness h;
        h.pan.setWfColorScheme(WfColorScheme::ClarityBlue);
        h.panel->openDisplayPill();
        QComboBox* scheme = h.combo("pillDisplayWfColorScheme");
        QVERIFY(scheme);
        QCOMPARE(scheme->count(), 8);
        QCOMPARE(scheme->count(), static_cast<int>(WfColorScheme::Count));
        for (int i = 0; i < scheme->count(); ++i) {
            QCOMPARE(scheme->itemText(i),
                     QString::fromUtf8(wfSchemeName(static_cast<WfColorScheme>(i))));
        }
        QCOMPARE(scheme->currentText(), QStringLiteral("Clarity Blue"));

        // Choosing one sets the pan; the pan changing is shown.
        scheme->setCurrentIndex(scheme->findText(QStringLiteral("Spectran")));
        QCOMPARE(h.pan.wfColorScheme(), WfColorScheme::Spectran);
        h.pan.setWfColorScheme(WfColorScheme::LinRad);
        QCOMPARE(scheme->currentText(), QStringLiteral("LinRad"));
    }

    // Black Level reads as an offset from the noise, Color Gain as the span.
    void blackLevelAndColorGainReadAsOffsetAndSpan() {
        PillHarness h;
        h.panel->setClarityState(true, true, false);
        h.panel->openDisplayPill();
        QSlider* black = h.slider("pillDisplayWfBlackLevel");
        QSlider* gain = h.slider("pillDisplayWfColorGain");
        QVERIFY(black && gain);

        black->setValue(57);
        QCOMPARE(h.pan.wfBlackLevel(), 57);
        QCOMPARE(h.valueText("pillDisplayWfBlackLevel"), QStringLiteral("N+3 dB"));
        black->setValue(50);
        QCOMPARE(h.valueText("pillDisplayWfBlackLevel"), QStringLiteral("N+0 dB"));
        black->setValue(40);
        QCOMPARE(h.pan.wfBlackLevel(), 40);
        QCOMPARE(h.valueText("pillDisplayWfBlackLevel"), QStringLiteral("N-5 dB"));
        black->setValue(100);
        QCOMPARE(h.valueText("pillDisplayWfBlackLevel"), QStringLiteral("N+25 dB"));
        black->setValue(0);
        QCOMPARE(h.valueText("pillDisplayWfBlackLevel"), QStringLiteral("N-25 dB"));

        gain->setValue(60);
        QCOMPARE(h.pan.wfColorGain(), 60);
        QCOMPARE(h.valueText("pillDisplayWfColorGain"), QStringLiteral("38 dB"));
        gain->setValue(0);
        QCOMPARE(h.valueText("pillDisplayWfColorGain"), QStringLiteral("80 dB"));
        gain->setValue(100);
        QCOMPARE(h.valueText("pillDisplayWfColorGain"), QStringLiteral("10 dB"));

        // The pan changing is shown, in the same words.
        h.pan.setWfBlackLevel(57);
        h.pan.setWfColorGain(60);
        QCOMPARE(black->value(), 57);
        QCOMPARE(h.valueText("pillDisplayWfBlackLevel"), QStringLiteral("N+3 dB"));
        QCOMPARE(gain->value(), 60);
        QCOMPARE(h.valueText("pillDisplayWfColorGain"), QStringLiteral("38 dB"));

        QVERIFY(black->toolTip().contains(QStringLiteral("Up is blacker")));
    }

    // The waterfall's levels have one owner at a time; the rows of the
    // others are greyed, still shown, with the reason.
    void clarityAgcAndMinMaxGreyTheRowsTheyReplace() {
        PillHarness h;
        h.panel->openDisplayPill();
        const struct { const char* mode; bool low; bool high; bool offset; const char* note; } rows[] = {
            {"Clarity", false, false, false, "Clarity sets Low and High from the noise floor."},
            {"Auto", false, true, false, "Auto sets Low from the weakest signals. You set High."},
            {"Floor", false, true, true, "Low is the noise floor plus NF Offset. You set High."},
            {"Manual", true, true, false, "You set Low and High."},
            {"Scale", false, false, false, "Low and High are dB Min and dB Max, above."}
        };
        for (const auto& row : rows) {
            h.toggle(qPrintable(QStringLiteral("pillWaterfallLevels") + QLatin1String(row.mode)))->click();
            QCOMPARE(h.slider("pillDisplayWfLowLevel")->isEnabled(), row.low);
            QCOMPARE(h.slider("pillDisplayWfHighLevel")->isEnabled(), row.high);
            QCOMPARE(h.slider("pillWaterfallAGCOffsetDb")->isEnabled(), row.offset);
            QCOMPARE(h.control<QLabel>("pillDisplayWfLowLevelValue")->isEnabled(), row.low);
            QCOMPARE(h.control<QLabel>("pillWaterfallNote")->text(), QString::fromLatin1(row.note));
            for (const char* key : {"pillDisplayWfLowLevel", "pillDisplayWfHighLevel", "pillWaterfallAGCOffsetDb"}) {
                QVERIFY(!h.control<QWidget>(key)->isHidden());
                QVERIFY(!h.control<QWidget>(key)->toolTip().isEmpty());
            }
            QVERIFY(h.slider("pillDisplayWfBlackLevel")->isEnabled());
            QVERIFY(h.slider("pillDisplayWfColorGain")->isEnabled());
        }
        // Requested Clarity, before a runtime level, still owns both ends.
        h.pan.setClarityEnabled(true);
        h.pan.setClarityActive(false);
        QVERIFY(h.toggle("pillWaterfallLevelsClarity")->isChecked());
        QVERIFY(!h.slider("pillDisplayWfLowLevel")->isEnabled());
        QVERIFY(!h.slider("pillDisplayWfHighLevel")->isEnabled());
        h.pan.setClarityActive(true);
        QVERIFY(!h.slider("pillDisplayWfLowLevel")->isEnabled());
        h.pan.setDisplayKeyed(true, 0, 0);
        for (const char* key : {"pillWaterfallLevelsClarity", "pillSpectrumScaleManual", "pillDisplayWfBlackLevel",
                               "pillDisplayWfColorGain", "pillDisplayWfLowLevel", "pillDisplayWfHighLevel", "pillClarityRetune"}) {
            QVERIFY(!h.control<QWidget>(key)->isEnabled());
            QVERIFY(!h.control<QWidget>(key)->isHidden());
        }
        h.pan.setDisplayKeyed(false, 0, 0);
        QVERIFY(h.toggle("pillClarityRetune")->isEnabled());
        h.pan.setClarityEnabled(false);
        h.pan.setWfUseSpectrumMinMax(false);
        h.pan.setWfAgcEnabled(true);
        h.pan.setWaterfallNFAGCEnabled(true);
        QVERIFY(h.toggle("pillWaterfallLevelsFloor")->isChecked());
        QVERIFY(h.slider("pillWaterfallAGCOffsetDb")->isEnabled());
    }

    // Rulings wave R6: whole dB, 1 dB a step, -60 to 60 as Setup's box, and
    // no decimal in the text. (It was nfAgcOffsetMovesInTenthsOfADb.)
    void nfAgcOffsetMovesInWholeDb() {
        PillHarness h;
        h.panel->openDisplayPill();
        QSlider* offset = h.slider("pillWaterfallAGCOffsetDb");
        QVERIFY(offset);
        QCOMPARE(offset->minimum(), -60);
        QCOMPARE(offset->maximum(), 60);
        QCOMPARE(offset->singleStep(), 1);
        QCOMPARE(h.valueText("pillWaterfallAGCOffsetDb"), QStringLiteral("0 dB"));

        QSignalSpy announced(&h.pan, &SpectrumWidget::displaySettingChanged);
        offset->setValue(-35);
        QCOMPARE(h.pan.waterfallAGCOffsetDb(), -35);
        QCOMPARE(h.pan.waterfallAGCOffsetTenthsDb(), -350);
        QCOMPARE(h.valueText("pillWaterfallAGCOffsetDb"), QStringLiteral("-35 dB"));
        QCOMPARE(announced.count(), 1);
        QCOMPARE(announced.at(0).at(0).toString(), QStringLiteral("WaterfallAGCOffsetDb"));
        // One step is one dB.
        offset->triggerAction(QAbstractSlider::SliderSingleStepAdd);
        QCOMPARE(offset->value(), -34);
        QCOMPARE(h.pan.waterfallAGCOffsetDb(), -34);
        QCOMPARE(h.pan.waterfallAGCOffsetTenthsDb(), -340);
        QCOMPARE(h.valueText("pillWaterfallAGCOffsetDb"), QStringLiteral("-34 dB"));
        offset->setValue(60);
        QCOMPARE(h.pan.waterfallAGCOffsetDb(), 60);
        QCOMPARE(h.valueText("pillWaterfallAGCOffsetDb"), QStringLiteral("60 dB"));
        offset->setValue(-60);
        QCOMPARE(h.pan.waterfallAGCOffsetDb(), -60);

        // Setup's whole dB is shown too.
        h.pan.setWaterfallAGCOffsetDb(12);
        QCOMPARE(offset->value(), 12);
        QCOMPARE(h.valueText("pillWaterfallAGCOffsetDb"), QStringLiteral("12 dB"));
        // Out of range is held at the ends.
        h.pan.setWaterfallAGCOffsetDb(999);
        QCOMPARE(h.pan.waterfallAGCOffsetDb(), 60);
        QCOMPARE(offset->value(), 60);

        h.toggle("pillWaterfallLevelsFloor")->click();
        QVERIFY(h.pan.waterfallNFAGCEnabled());
        QVERIFY(h.pan.wfAgcEnabled());
        QVERIFY(h.toggle("pillWaterfallLevelsFloor")->isChecked());
        QVERIFY(offset->isEnabled());
    }

    // Rulings wave R6: a fraction stored by an earlier build of this branch
    // is rounded to the nearest dB when it is read, the pill shows the whole
    // dB, and the next save writes it. (It was nfAgcOffsetIsKeptToTheTenth,
    // which expected -3.5 to come back.)
    void aStoredFractionalNfAgcOffsetIsRoundedWhenRead() {
        const auto saved = []() {
            QStringList values;
            for (const QString& key : AppSettings::instance().allKeys()) {
                if (key.contains(QStringLiteral("WaterfallAGCOffsetDb"))) {
                    values << AppSettings::instance().value(key).toString();
                }
            }
            return values;
        };
        {
            SpectrumWidget pan;
            pan.setWaterfallAGCOffsetTenthsDb(-35);
            pan.saveSettingsForTest();
        }
        QCOMPARE(saved(), QStringList{QStringLiteral("-3.5")});
        {
            SpectrumWidget pan;
            pan.loadSettingsForTest();
            QCOMPARE(pan.waterfallAGCOffsetTenthsDb(), -40);
            QCOMPARE(pan.waterfallAGCOffsetDb(), -4);
            pan.saveSettingsForTest();
        }
        QCOMPARE(saved(), QStringList{QStringLiteral("-4")});
        {
            PillHarness h;
            h.pan.loadSettingsForTest();
            h.panel->openDisplayPill();
            QCOMPARE(h.slider("pillWaterfallAGCOffsetDb")->value(), -4);
            QCOMPARE(h.valueText("pillWaterfallAGCOffsetDb"), QStringLiteral("-4 dB"));
        }
        // A fraction below the half rounds toward zero.
        for (const QString& key : AppSettings::instance().allKeys()) {
            if (key.contains(QStringLiteral("WaterfallAGCOffsetDb"))) {
                AppSettings::instance().setValue(key, QStringLiteral("3.4"));
            }
        }
        {
            SpectrumWidget pan;
            pan.loadSettingsForTest();
            QCOMPARE(pan.waterfallAGCOffsetTenthsDb(), 30);
            // A whole dB is written as it always was.
            pan.setWaterfallAGCOffsetDb(7);
            pan.saveSettingsForTest();
        }
        QCOMPARE(saved(), QStringList{QStringLiteral("7")});
    }

    void nbBlankReadsAsTheThreshold() {
        PillHarness h;
        h.panel->openDisplayPill();
        QSlider* threshold = h.slider("pillWaterfallBlankingThresholdDb");
        QPushButton* pill = h.toggle("pillWaterfallBlankingEnabled");
        QVERIFY(threshold && pill);
        QCOMPARE(threshold->minimum(), 1);
        QCOMPARE(threshold->maximum(), 20);
        QCOMPARE(threshold->singleStep(), 1);
        QCOMPARE(threshold->value(), 6);
        QCOMPARE(h.valueText("pillWaterfallBlankingThresholdDb"), QStringLiteral("6 dB"));
        QCOMPARE(pill->text(), QStringLiteral("Off"));
        QVERIFY(!pill->isChecked());

        threshold->setValue(20);
        QCOMPARE(h.pan.waterfallBlankingThresholdDb(), 20);
        QCOMPARE(h.valueText("pillWaterfallBlankingThresholdDb"), QStringLiteral("20 dB"));
        threshold->setValue(1);
        QCOMPARE(h.pan.waterfallBlankingThresholdDb(), 1);
        QCOMPARE(h.valueText("pillWaterfallBlankingThresholdDb"), QStringLiteral("1 dB"));
        pill->click();
        QVERIFY(h.pan.waterfallBlankingEnabled());
        QCOMPARE(pill->text(), QStringLiteral("On"));

        h.pan.setWaterfallBlankingEnabled(false);
        h.pan.setWaterfallBlankingThresholdDb(6);
        QVERIFY(!pill->isChecked());
        QCOMPARE(pill->text(), QStringLiteral("Off"));
        QCOMPARE(h.valueText("pillWaterfallBlankingThresholdDb"), QStringLiteral("6 dB"));
    }

    // The Clarity row: the badge, the On pill and Re-tune.
    void clarityRowShowsTheStateAndAsks() {
        PillHarness h;
        h.panel->openDisplayPill();
        auto* badge = h.control<QLabel>("pillClarityBadge");
        auto* clarity = h.toggle("pillWaterfallLevelsClarity");
        auto* retune = h.toggle("pillClarityRetune");
        QVERIFY(badge && clarity && retune);
        QCOMPARE(badge->text(), QStringLiteral("C"));
        QCOMPARE(retune->text(), QStringLiteral("Re-tune"));
        QVERIFY(clarity->isChecked());
        QVERIFY(retune->isEnabled());
        QCOMPARE(badge->property("clarityState").toString(), QStringLiteral("off"));
        h.panel->setClarityState(true, true, false);
        QCOMPARE(badge->property("clarityState").toString(), QStringLiteral("active"));
        QVERIFY(badge->styleSheet().contains(QStringLiteral("#00ff88")));
        h.panel->setClarityState(true, true, true);
        QCOMPARE(badge->property("clarityState").toString(), QStringLiteral("paused"));
        QVERIFY(badge->styleSheet().contains(QStringLiteral("#d4a017")));
        QVERIFY(!badge->isHidden());
        QSignalSpy retuned(h.panel, &SpectrumOverlayPanel::clarityRetuneRequested);
        retune->click();
        QCOMPARE(retuned.count(), 1);
        h.toggle("pillWaterfallLevelsAuto")->click();
        QVERIFY(!h.pan.clarityEnabled());
        QVERIFY(!retune->isEnabled());
        QCOMPARE(badge->property("clarityState").toString(), QStringLiteral("off"));
        clarity->click();
        QVERIFY(h.pan.clarityEnabled());
        QVERIFY(retune->isEnabled());
        QSignalSpy changes(&h.pan, &SpectrumWidget::displaySettingChanged);
        clarity->click();
        QCOMPARE(changes.count(), 0);
    }

    // A drag asks once, when the handle is let go; keyboard steps ask once,
    // 300 ms after the last.
    void fftSizeAsksOnReleaseAndOnceAfterKeys() {
        PillHarness h;
        h.panel->openDisplayPill();
        // The Fixed sizing mode, the one where the slider is live (D38).
        {
            SpectrumOverlayPanel::DisplayPillFftState fixed = h.panel->fftState();
            fixed.sizingMode = ControlRanges::DisplayFftSizingMode::Fixed;
            fixed.baselineSize = 4096;
            h.panel->setFftState(fixed);
        }
        QSlider* size = h.slider("pillDisplayFftSize");
        QVERIFY(size->isEnabled());
        auto* timer = h.panel->findChild<QTimer*>(QStringLiteral("pillFftSizeKeyTimer"));
        QVERIFY(size && timer);
        QVERIFY(!size->hasTracking());
        QCOMPARE(timer->interval(), 300);
        QVERIFY(timer->isSingleShot());
        QSignalSpy asked(h.panel, &SpectrumOverlayPanel::fftBaselineRequested);

        size->setSliderDown(true);
        size->setSliderPosition(3);
        QCOMPARE(h.valueText("pillDisplayFftSize"), QStringLiteral("32768"));
        size->setSliderPosition(6);
        size->setSliderPosition(5);
        QCOMPARE(h.valueText("pillDisplayFftSize"), QStringLiteral("131072"));
        QCOMPARE(asked.count(), 0);
        QVERIFY(!timer->isActive());
        size->setSliderDown(false);
        QCOMPARE(asked.count(), 1);
        QCOMPARE(asked.at(0).at(0).toInt(), 131072);
        QVERIFY(!timer->isActive());

        // Pressed and let go where it was: nothing to ask.
        size->setSliderDown(true);
        size->setSliderDown(false);
        QCOMPARE(asked.count(), 1);

        // The station takes the size.
        SpectrumOverlayPanel::DisplayPillFftState state = h.panel->fftState();
        state.baselineSize = 131072;
        h.panel->setFftState(state);
        QCOMPARE(asked.count(), 1);
        QCOMPARE(size->value(), 5);

        const auto key = [size](Qt::Key which) {
            QKeyEvent press(QEvent::KeyPress, which, Qt::NoModifier);
            QApplication::sendEvent(size, &press);
            QKeyEvent release(QEvent::KeyRelease, which, Qt::NoModifier);
            QApplication::sendEvent(size, &release);
        };
        key(Qt::Key_Right);
        QCOMPARE(size->value(), 6);
        key(Qt::Key_Left);
        key(Qt::Key_Left);
        QCOMPARE(size->value(), 4);
        QCOMPARE(h.valueText("pillDisplayFftSize"), QStringLiteral("65536"));
        QCOMPARE(asked.count(), 1);
        QVERIFY(timer->isActive());
        QTRY_COMPARE(asked.count(), 2);
        QCOMPARE(asked.at(1).at(0).toInt(), 65536);
        QVERIFY(!timer->isActive());

        // Stepped away and back again: nothing to ask.
        state.baselineSize = 65536;
        h.panel->setFftState(state);
        key(Qt::Key_Right);
        key(Qt::Key_Left);
        QVERIFY(timer->isActive());
        QTRY_VERIFY(!timer->isActive());
        QCOMPARE(asked.count(), 2);

        // A drag while the key timer runs asks once, for the drag.
        key(Qt::Key_Right);
        QVERIFY(timer->isActive());
        size->setSliderDown(true);
        size->setSliderPosition(1);
        size->setSliderDown(false);
        QCOMPARE(asked.count(), 3);
        QCOMPARE(asked.at(2).at(0).toInt(), 8192);
        QVERIFY(!timer->isActive());

        // A new size from the station does not pull a handle being dragged.
        size->setSliderDown(true);
        size->setSliderPosition(2);
        state.baselineSize = 262144;
        h.panel->setFftState(state);
        QCOMPARE(size->sliderPosition(), 2);
        size->setSliderDown(false);
        QCOMPARE(asked.count(), 4);
        QCOMPARE(asked.at(3).at(0).toInt(), 16384);
    }

    void fftReadoutSaysBinWidthAndTimePerTransform() {
        PillHarness h;
        auto* readout = h.control<QLabel>("pillFftReadout");
        QVERIFY(readout);
        // Nothing known yet: nothing claimed.
        QCOMPARE(readout->text(), QString());

        SpectrumOverlayPanel::DisplayPillFftState state;
        state.baselineSize = 131072;
        state.fps = 30;
        state.windowType = 1;
        state.hzPerBinTarget = 0.0;
        state.decimation = 1;
        state.runningSize = 131072;
        state.sampleRateHz = 192000.0;
        h.panel->setFftState(state);
        const QString dot = QString(QChar(0x00B7));
        QCOMPARE(readout->text(),
                 QStringLiteral("1.46 Hz per bin ") + dot
                     + QStringLiteral(" 0.68 s per transform at 192 kHz"));

        state.runningSize = 4096;
        state.sampleRateHz = 48000.0;
        h.panel->setFftState(state);
        QCOMPARE(readout->text(),
                 QStringLiteral("11.72 Hz per bin ") + dot
                     + QStringLiteral(" 0.09 s per transform at 48 kHz"));

        // FFT sizing mode (design D38): the line is the running numbers in
        // every mode, never a sentence about the target.
        state.sizingMode = ControlRanges::DisplayFftSizingMode::HzPerBin;
        state.hzPerBinTarget = 2.0;
        h.panel->setFftState(state);
        QCOMPARE(readout->text(),
                 QStringLiteral("11.72 Hz per bin ") + dot
                     + QStringLiteral(" 0.09 s per transform at 48 kHz"));
        QVERIFY(!readout->isHidden());

        // 1.536 MHz zoomed to 35 kHz runs 262144 points: the same line in
        // Auto, Hz/bin and Fixed, on a local pan and a remote one.
        state.runningSize = 262144;
        state.sampleRateHz = 1536000.0;
        const QString wide = QStringLiteral("5.86 Hz per bin ") + dot
            + QStringLiteral(" 0.17 s per transform at 1536 kHz");
        using Mode = ControlRanges::DisplayFftSizingMode;
        for (bool remote : {false, true}) {
            h.panel->setRemoteWindow(remote, true);
            for (Mode mode : {Mode::Auto, Mode::HzPerBin, Mode::Fixed}) {
                state.sizingMode = mode;
                state.hzPerBinTarget = mode == Mode::HzPerBin ? 2.0 : 0.0;
                h.panel->setFftState(state);
                QCOMPARE(readout->text(), wide);
            }
        }
    }

    // "Sizing:" (design D38): three buttons, one checked, the station's
    // mode. FFT Size is live in Fixed only and, grayed, shows the size that
    // runs; Hz/bin is live in Hz/bin only and, grayed, shows the remembered
    // target. Each grayed row says which choice makes it live.
    void sizingRowPicksWhatSetsTheSize() {
        using Mode = ControlRanges::DisplayFftSizingMode;
        PillHarness h;
        h.panel->openDisplayPill();
        QPushButton* autoBtn = h.toggle("pillDisplayFftSizingModeAuto");
        QPushButton* hzBtn = h.toggle("pillDisplayFftSizingModeHzPerBin");
        QPushButton* fixedBtn = h.toggle("pillDisplayFftSizingModeFixed");
        QSlider* size = h.slider("pillDisplayFftSize");
        QSlider* target = h.slider("pillDisplayHzPerBinTarget");
        QVERIFY(autoBtn && hzBtn && fixedBtn && size && target);
        QSignalSpy modeAsked(h.panel, &SpectrumOverlayPanel::fftSizingModeRequested);
        QSignalSpy sizeAsked(h.panel, &SpectrumOverlayPanel::fftBaselineRequested);
        QSignalSpy targetAsked(h.panel, &SpectrumOverlayPanel::hzPerBinTargetRequested);
        const QString sizingTip = QStringLiteral(
            "How the FFT size is chosen. Auto: the zoom picks the size to keep the picture "
            "sharp. Hz/bin: the size follows the bin width you set. Fixed: the FFT Size "
            "slider sets the size and zoom does not change it.");
        const QString sizeHeldInAuto = QStringLiteral(
            "Auto is chosen, so the zoom sets the FFT size. Choose Fixed to set it yourself.");
        const QString sizeHeldInHzPerBin = QStringLiteral(
            "Hz/bin is chosen, so the bin width sets the FFT size. "
            "Choose Fixed to set it yourself.");
        const QString targetHeld =
            QStringLiteral("Choose Hz/bin above to set the bin width yourself.");
        const QString targetTip = QStringLiteral(
            "The bin width to hold at every zoom. The FFT size follows it: sample rate "
            "divided by bin width, rounded up to a power of two, within the size limits.");
        const auto checkedAre = [&](bool a, bool hz, bool fixed) {
            return autoBtn->isChecked() == a && hzBtn->isChecked() == hz
                && fixedBtn->isChecked() == fixed;
        };

        // 192 kHz with 8192 running, and 65536 stored for Fixed.
        SpectrumOverlayPanel::DisplayPillFftState state = h.panel->fftState();
        state.sizingMode = Mode::Auto;
        state.baselineSize = 65536;
        state.runningSize = 8192;
        state.sampleRateHz = 192000.0;
        state.decimation = 1;
        h.panel->setFftState(state);

        // Auto.
        QVERIFY(checkedAre(true, false, false));
        for (QPushButton* button : {autoBtn, hzBtn, fixedBtn}) {
            QVERIFY(button->isEnabled());
            QCOMPARE(button->toolTip(), sizingTip);
        }
        QVERIFY(!size->isEnabled());
        QVERIFY(!size->isHidden());
        QCOMPARE(size->toolTip(), sizeHeldInAuto);
        QCOMPARE(size->value(), 1);
        QCOMPARE(h.valueText("pillDisplayFftSize"), QStringLiteral("8192"));
        QVERIFY(!target->isEnabled());
        QVERIFY(!target->isHidden());
        QCOMPARE(target->toolTip(), targetHeld);
        // No target remembered: the bin width running, on the 0.5 Hz step.
        QCOMPARE(target->value(), 47);
        QCOMPARE(h.valueText("pillDisplayHzPerBinTarget"), QStringLiteral("23.5 Hz"));
        // A zoom: the grayed slider follows the size that runs.
        state.runningSize = 32768;
        h.panel->setFftState(state);
        QCOMPARE(size->value(), 3);
        QCOMPARE(h.valueText("pillDisplayFftSize"), QStringLiteral("32768"));
        state.runningSize = 8192;
        h.panel->setFftState(state);
        // The grayed sliders are not requests.
        size->setSliderDown(true);
        size->setSliderPosition(5);
        size->setSliderDown(false);
        target->setValue(10);
        QCOMPARE(sizeAsked.count() + targetAsked.count() + modeAsked.count(), 0);
        QCOMPARE(size->value(), 1);
        QCOMPARE(target->value(), 47);

        // Hz/bin is asked for with the bin width running, for a station that
        // remembers no target. One button stays checked.
        hzBtn->click();
        QCOMPARE(modeAsked.count(), 1);
        QCOMPARE(modeAsked.at(0).at(0).value<Mode>(), Mode::HzPerBin);
        QCOMPARE(modeAsked.at(0).at(1).toDouble(), 23.5);
        QVERIFY(checkedAre(false, true, false));
        // The checked one clicked again stays checked and asks nothing.
        hzBtn->click();
        QCOMPARE(modeAsked.count(), 1);
        QVERIFY(checkedAre(false, true, false));

        // The station took it: 23.5 Hz plans the 8192 that ran.
        state.sizingMode = Mode::HzPerBin;
        state.hzPerBinTarget = 23.5;
        state.lastHzPerBinTarget = 23.5;
        h.panel->setFftState(state);
        QCOMPARE(modeAsked.count(), 1);
        QVERIFY(checkedAre(false, true, false));
        QVERIFY(target->isEnabled());
        QCOMPARE(target->toolTip(), targetTip);
        QCOMPARE(target->value(), 47);
        QVERIFY(!size->isEnabled());
        QCOMPARE(size->toolTip(), sizeHeldInHzPerBin);
        QCOMPARE(size->value(), 1);
        QCOMPARE(h.valueText("pillDisplayFftSize"), QStringLiteral("8192"));
        // The live target asks, in 0.5 Hz steps from 0.5 to 200.
        target->setValue(25);
        QCOMPARE(targetAsked.count(), 1);
        QCOMPARE(targetAsked.at(0).at(0).toDouble(), 12.5);
        QCOMPARE(h.valueText("pillDisplayHzPerBinTarget"), QStringLiteral("12.5 Hz"));
        target->setValue(target->minimum());
        QCOMPARE(targetAsked.at(1).at(0).toDouble(), 0.5);
        QCOMPARE(h.valueText("pillDisplayHzPerBinTarget"), QStringLiteral("0.5 Hz"));
        target->setValue(target->maximum());
        QCOMPARE(targetAsked.at(2).at(0).toDouble(), 200.0);
        QCOMPARE(h.valueText("pillDisplayHzPerBinTarget"), QStringLiteral("200.0 Hz"));
        state.hzPerBinTarget = 12.5;
        state.lastHzPerBinTarget = 12.5;
        state.runningSize = 16384;
        h.panel->setFftState(state);
        QCOMPARE(target->value(), 25);
        QCOMPARE(targetAsked.count(), 3);

        // Fixed: the slider is live on the stored size again; Hz/bin is
        // grayed on the target remembered.
        fixedBtn->click();
        QCOMPARE(modeAsked.count(), 2);
        QCOMPARE(modeAsked.at(1).at(0).value<Mode>(), Mode::Fixed);
        state.sizingMode = Mode::Fixed;
        state.hzPerBinTarget = 0.0;
        state.runningSize = 65536;
        h.panel->setFftState(state);
        QVERIFY(checkedAre(false, false, true));
        QVERIFY(size->isEnabled());
        QVERIFY(size->toolTip() != sizeHeldInAuto && size->toolTip() != sizeHeldInHzPerBin);
        QVERIFY(!size->toolTip().isEmpty());
        QCOMPARE(size->value(), 4);
        QCOMPARE(h.valueText("pillDisplayFftSize"), QStringLiteral("65536"));
        QVERIFY(!target->isEnabled());
        QCOMPARE(target->toolTip(), targetHeld);
        QCOMPARE(target->value(), 25);
        QCOMPARE(h.valueText("pillDisplayHzPerBinTarget"), QStringLiteral("12.5 Hz"));

        // Auto, then Hz/bin again: the remembered 12.5 is what it shows.
        autoBtn->click();
        QCOMPARE(modeAsked.at(2).at(0).value<Mode>(), Mode::Auto);
        state.sizingMode = Mode::Auto;
        state.runningSize = 8192;
        h.panel->setFftState(state);
        QVERIFY(checkedAre(true, false, false));
        QCOMPARE(size->value(), 1);
        QCOMPARE(target->value(), 25);
        hzBtn->click();
        state.sizingMode = Mode::HzPerBin;
        state.hzPerBinTarget = 12.5;
        h.panel->setFftState(state);
        QVERIFY(target->isEnabled());
        QCOMPARE(target->value(), 25);
        QCOMPARE(targetAsked.count(), 3);

        // A request the station did not take: the buttons go back.
        fixedBtn->click();
        QVERIFY(checkedAre(false, false, true));
        h.panel->setFftState(state);
        QVERIFY(checkedAre(false, true, false));

        // With nothing running there is no bin width to offer.
        PillHarness fresh;
        QSignalSpy freshAsked(fresh.panel, &SpectrumOverlayPanel::fftSizingModeRequested);
        fresh.toggle("pillDisplayFftSizingModeHzPerBin")->click();
        QCOMPARE(freshAsked.count(), 1);
        QCOMPARE(freshAsked.at(0).at(1).toDouble(), 0.0);
    }

    // The FFT tab shows the station's settings and asks for changes; it
    // writes no setting.
    void fftRowsShowTheStationAndAsk() {
        PillHarness h;
        h.panel->openDisplayPill();
        QSignalSpy size(h.panel, &SpectrumOverlayPanel::fftBaselineRequested);
        QSignalSpy fps(h.panel, &SpectrumOverlayPanel::fftFpsRequested);
        QSignalSpy window(h.panel, &SpectrumOverlayPanel::fftWindowRequested);
        QSignalSpy target(h.panel, &SpectrumOverlayPanel::hzPerBinTargetRequested);
        QSignalSpy decimation(h.panel, &SpectrumOverlayPanel::decimationRequested);
        QSignalSpy announced(&h.pan, &SpectrumWidget::displaySettingChanged);
        const QStringList keysBefore = AppSettings::instance().allKeys();

        SpectrumOverlayPanel::DisplayPillFftState state;
        state.baselineSize = 16384;
        state.fps = 45;
        state.windowType = 6;
        state.sizingMode = ControlRanges::DisplayFftSizingMode::HzPerBin;
        state.hzPerBinTarget = 5.0;
        state.decimation = 4;
        state.runningSize = 16384;
        state.sampleRateHz = 96000.0;
        h.panel->setFftState(state);
        QCOMPARE(h.slider("pillDisplayFftSize")->value(), 2);
        QCOMPARE(h.valueText("pillDisplayFftSize"), QStringLiteral("16384"));
        QCOMPARE(h.slider("pillDisplaySpectrumFps")->value(), 45);
        QCOMPARE(h.valueText("pillDisplaySpectrumFps"), QStringLiteral("45"));
        QCOMPARE(h.combo("pillDisplayFftWindow")->currentText(),
                 QStringLiteral("Blackman-Harris 7T"));
        QCOMPARE(h.slider("pillDisplayHzPerBinTarget")->value(), 10);
        QCOMPARE(h.slider("pillDisplayDecimation")->value(), 4);
        QCOMPARE(h.valueText("pillDisplayDecimation"), QStringLiteral("4"));
        QCOMPARE(size.count() + fps.count() + window.count() + target.count()
                     + decimation.count(), 0);

        h.slider("pillDisplaySpectrumFps")->setValue(20);
        QCOMPARE(fps.count(), 1);
        QCOMPARE(fps.at(0).at(0).toInt(), 20);
        h.combo("pillDisplayFftWindow")->setCurrentIndex(
            h.combo("pillDisplayFftWindow")->findText(QStringLiteral("Hann")));
        QCOMPARE(window.count(), 1);
        QCOMPARE(window.at(0).at(0).toInt(), 2);
        h.slider("pillDisplayDecimation")->setValue(8);
        QCOMPARE(decimation.count(), 1);
        QCOMPARE(decimation.at(0).at(0).toInt(), 8);

        // Until the station takes a change, the rows go back to what it has.
        h.panel->setFftState(state);
        QCOMPARE(h.slider("pillDisplaySpectrumFps")->value(), 45);
        QCOMPARE(h.slider("pillDisplayDecimation")->value(), 4);
        QCOMPARE(fps.count(), 1);
        QCOMPARE(decimation.count(), 1);

        // Nothing was written here and the pan was told nothing.
        QCOMPARE(announced.count(), 0);
        QCOMPARE(AppSettings::instance().allKeys(), keysBefore);
    }

    // The station's rows are greyed, with the reason, when the Core cannot
    // be reached; Decimation when the Core is too old to change it.
    void stationRowsAreGreyedWithTheReason() {
        PillHarness h;
        h.panel->openDisplayPill();
        const char* fftRows[] = {"pillDisplayFftSizingModeAuto",
                                 "pillDisplayFftSizingModeHzPerBin",
                                 "pillDisplayFftSizingModeFixed",
                                 "pillDisplayFftSize", "pillDisplaySpectrumFps",
                                 "pillDisplayFftWindow",
                                 "pillDisplayHzPerBinTarget", "pillDisplayDecimation"};
        const char* stationRows[] = {"pillDisplayFftSizingModeAuto",
                                     "pillDisplayFftSizingModeHzPerBin",
                                     "pillDisplayFftSizingModeFixed",
                                     "pillDisplayFftSize", "pillDisplaySpectrumFps",
                                     "pillDisplayFftWindow",
                                     "pillDisplayHzPerBinTarget", "pillDisplayDecimation",
                                     "pillDisplayGridMax", "pillDisplayGridMin"};
        // FFT sizing mode (design D38): with the station there, FFT Size is
        // live in Fixed only and Hz/bin in Hz/bin only; every other station
        // row is live in every mode.
        using Mode = ControlRanges::DisplayFftSizingMode;
        const auto liveIn = [](const char* name, Mode mode) {
            if (QLatin1String(name) == QLatin1String("pillDisplayFftSize")) {
                return mode == Mode::Fixed;
            }
            if (QLatin1String(name) == QLatin1String("pillDisplayHzPerBinTarget")) {
                return mode == Mode::HzPerBin;
            }
            return true;
        };
        auto* note = h.control<QLabel>("pillStationNote");
        QVERIFY(note);
        const QString reason = QStringLiteral("Connect to the Core to change these.");
        for (Mode mode : {Mode::HzPerBin, Mode::Fixed, Mode::Auto}) {
            SpectrumOverlayPanel::DisplayPillFftState state = h.panel->fftState();
            state.sizingMode = mode;
            state.hzPerBinTarget = mode == Mode::HzPerBin ? 5.0 : 0.0;
            h.panel->setFftState(state);
            for (const char* name : stationRows) {
                QCOMPARE(h.control<QWidget>(name)->isEnabled(), liveIn(name, mode));
            }
            QCOMPARE(note->text(), QString());

            // The station's reason wins over the sizing mode's.
            h.panel->setStationRowsAvailable(false, reason);
            for (const char* name : stationRows) {
                QWidget* control = h.control<QWidget>(name);
                QVERIFY2(!control->isEnabled(), name);
                QVERIFY2(!control->isHidden(), name);
                QCOMPARE(control->toolTip(), reason);
            }
            QCOMPARE(note->text(), reason);
            QVERIFY(!h.control<QLabel>("pillDisplayFftSizeValue")->isEnabled());
            // A row of this computer's is untouched.
            QVERIFY(h.slider("pillDisplayFftFillAlpha")->isEnabled());
            QVERIFY(h.combo("pillDisplayWfColorScheme")->isEnabled());

            h.panel->setStationRowsAvailable(true, QString());
            for (const char* name : stationRows) {
                QWidget* control = h.control<QWidget>(name);
                QCOMPARE(control->isEnabled(), liveIn(name, mode));
                QVERIFY2(control->toolTip() != reason, name);
                QVERIFY2(!control->toolTip().isEmpty(), name);
            }
            QCOMPARE(note->text(), QString());
        }
        // The rest is in Auto, the mode a station starts in.

        // A remote window on a Core that does not offer Decimation.
        const QString oldCore = QStringLiteral("This Core is too old to change Decimation.");
        h.panel->setRemoteWindow(true, false);
        QSlider* decimation = h.slider("pillDisplayDecimation");
        QVERIFY(!decimation->isEnabled());
        QVERIFY(!decimation->isHidden());
        QCOMPARE(decimation->toolTip(), oldCore);
        QCOMPARE(note->text(), oldCore);
        for (const char* name : fftRows) {
            if (QLatin1String(name) != QLatin1String("pillDisplayDecimation")) {
                QCOMPARE(h.control<QWidget>(name)->isEnabled(), liveIn(name, Mode::Auto));
            }
        }
        h.panel->setRemoteWindow(true, true);
        QVERIFY(decimation->isEnabled());
        QCOMPARE(note->text(), QString());
        h.panel->setRemoteWindow(false, false);
        QVERIFY(decimation->isEnabled());
    }

    // Follow-up wave F3: dB Max and dB Min are greyed while this pan is
    // keyed, with the reason as their tooltip and in the note under them,
    // and come back on un-key. The station's reason wins when both apply.
    void dbMaxAndMinAreGreyedWhileThePanIsKeyed() {
        PillHarness h;
        h.panel->setDisplayPillTab(Tab::Levels);
        h.panel->setBandGrid(QStringLiteral("20 m"), -40, -140);
        h.panel->openDisplayPill();
        const QString keyed = QStringLiteral(
            "These set the receive scale. Change them when not transmitting.");
        const QString perBand = QStringLiteral("dB Max and Min are kept per band (20 m).");
        const char* rows[] = {"pillDisplayGridMax", "pillDisplayGridMaxValue",
                              "pillDisplayGridMin", "pillDisplayGridMinValue"};
        auto* note = h.control<QLabel>("pillBandGridNote");
        QVERIFY(note);
        const auto allEnabled = [&](bool enabled) {
            for (const char* name : rows) {
                if (h.control<QWidget>(name)->isEnabled() != enabled) {
                    qWarning() << name << "is not" << enabled;
                    return false;
                }
            }
            return true;
        };
        QVERIFY(allEnabled(true));
        QCOMPARE(note->text(), perBand);
        QCOMPARE(note->height(), 12);
        const int restHeight = h.pill()->height();
        QVERIFY(restHeight <= 480);
        QVERIFY(h.scroll());
        QCOMPARE(h.scroll()->verticalScrollBar()->maximum(), 0);
        const QString maxTip = h.slider("pillDisplayGridMax")->toolTip();
        const QString minTip = h.slider("pillDisplayGridMin")->toolTip();
        QVERIFY(!maxTip.isEmpty() && maxTip != keyed);

        // The key edge.
        QSignalSpy maxAsked(h.panel, &SpectrumOverlayPanel::perBandDbMaxRequested);
        QSignalSpy minAsked(h.panel, &SpectrumOverlayPanel::perBandDbMinRequested);
        h.pan.setDisplayKeyed(true, 0, 0);
        QVERIFY(allEnabled(false));
        for (const char* name : rows) {
            QWidget* control = h.control<QWidget>(name);
            QVERIFY2(!control->isHidden(), name);
            QCOMPARE(control->toolTip(), keyed);
        }
        QCOMPARE(note->text(), keyed);
        // The sentence is too long for one line, so the note takes two,
        // whole, and the tab is one line taller and still fits a pan.
        h.layOut();
        const int room = 268 - 2 - 16;
        QCOMPARE(note->width(), room);
        QCOMPARE(note->height(), 24);
        QVERIFY(note->wordWrap());
        QCOMPARE(note->font().pixelSize(), 9);
        QCOMPARE(note->font().letterSpacing(), 0.0);
        const QRect drawn = QFontMetrics(note->font()).boundingRect(
            QRect(0, 0, room, 1000), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, keyed);
        qInfo() << keyed << "is drawn in" << drawn.size() << "of" << note->size();
        QVERIFY2(drawn.width() <= room, qPrintable(QString::number(drawn.width())));
        QVERIFY2(drawn.height() <= note->height(), qPrintable(QString::number(drawn.height())));
        QCOMPARE(h.pill()->height(), restHeight + 12);
        QVERIFY2(h.pill()->height() <= 480, qPrintable(QString::number(h.pill()->height())));
        QCOMPARE(h.scroll()->verticalScrollBar()->maximum(), 0);
        // The values shown are still the band's, and nothing was asked.
        QCOMPARE(h.slider("pillDisplayGridMax")->value(), -40);
        QCOMPARE(h.slider("pillDisplayGridMin")->value(), -140);
        // Another row of the tab is untouched.
        QVERIFY(h.slider("pillDisplayFftFillAlpha")->isEnabled());

        // A station that cannot be reached: its reason wins while keyed.
        const QString reason = QStringLiteral("Connect to the Core to change these.");
        h.panel->setStationRowsAvailable(false, reason);
        QVERIFY(allEnabled(false));
        for (const char* name : rows) {
            QCOMPARE(h.control<QWidget>(name)->toolTip(), reason);
        }
        QCOMPARE(note->text(), perBand);
        h.panel->setStationRowsAvailable(true, QString());
        QVERIFY(allEnabled(false));
        QCOMPARE(h.slider("pillDisplayGridMax")->toolTip(), keyed);
        QCOMPARE(note->text(), keyed);

        // The un-key edge.
        h.pan.setDisplayKeyed(false, 0, 0);
        QVERIFY(allEnabled(true));
        QCOMPARE(h.slider("pillDisplayGridMax")->toolTip(), maxTip);
        QCOMPARE(h.slider("pillDisplayGridMin")->toolTip(), minTip);
        QCOMPARE(note->text(), perBand);
        QCOMPARE(note->height(), 12);
        QVERIFY(!note->wordWrap());
        QCOMPARE(h.pill()->height(), restHeight);
        QCOMPARE(maxAsked.count(), 0);
        QCOMPARE(minAsked.count(), 0);
        h.slider("pillDisplayGridMax")->setValue(-55);
        QCOMPARE(maxAsked.count(), 1);

        // A pill opened on a pan that is already keyed shows it greyed, and
        // a closed pill lets go of the pan's key edges.
        QPushButton* att = h.panel->findChild<QPushButton*>(QStringLiteral("attMenuButton"));
        QVERIFY(att);
        att->click();
        QVERIFY(!h.panel->displayPillVisible());
        h.pan.setDisplayKeyed(true, 0, 0);
        QVERIFY(allEnabled(true));   // not following
        h.panel->openDisplayPill();
        QVERIFY(allEnabled(false));
        QCOMPARE(note->text(), keyed);
        h.pan.setDisplayKeyed(false, 0, 0);
        QVERIFY(allEnabled(true));
    }

    // Every Waterfall row shows the pan when the pill opens, follows it
    // while open, and writes it once when moved.
    void waterfallRowsSeedFollowAndWriteThePan() {
        PillHarness h;
        h.pan.setClarityEnabled(false);
        h.pan.setWfColorScheme(WfColorScheme::Enhanced);
        h.pan.setWfBlackLevel(61);
        h.pan.setWfColorGain(35);
        h.pan.setWfAgcEnabled(false);
        h.pan.setWaterfallNFAGCEnabled(true);
        h.pan.setWaterfallAGCOffsetDb(47);
        h.pan.setWfLowThreshold(-131.0f);
        h.pan.setWfHighThreshold(-71.0f);
        h.pan.setWfUpdatePeriodMs(120);
        h.pan.setWaterfallBlankingEnabled(true);
        h.pan.setWaterfallBlankingThresholdDb(14);
        h.pan.setWaterfallAveraging(SpectrumAveraging::TimeWindow);
        h.pan.setWaterfallAverageTimeMs(450);
        h.pan.setWfOpacity(66);
        h.pan.setWaterfallStopOnTx(true);
        h.pan.setShowRxFilterOnWaterfall(true);
        h.pan.setShowTxFilterOnRxWaterfall(false);
        h.pan.setShowRxZeroLineOnWaterfall(true);
        h.pan.setShowTxZeroLineOnWaterfall(true);
        h.pan.setWfTimestampPosition(SpectrumWidget::TimestampPosition::Right);
        h.pan.setWaterfallHistoryMs(5LL * 60 * 1000);
        QCoreApplication::processEvents();

        QSignalSpy announced(&h.pan, &SpectrumWidget::displaySettingChanged);
        QSignalSpy scheme(h.pan.displaySettings(), &DisplaySettingsModel::wfColorSchemeChanged);
        QSignalSpy gain(h.pan.displaySettings(), &DisplaySettingsModel::wfColorGainChanged);
        QSignalSpy black(h.pan.displaySettings(), &DisplaySettingsModel::wfBlackLevelChanged);
        const auto writes = [&]() {
            int persisted = 0;
            for (const QList<QVariant>& call : announced) {
                persisted += call.first().toString() != QStringLiteral("DisplayWfEffectiveLevels") ? 1 : 0;
            }
            return persisted + scheme.count() + gain.count() + black.count();
        };

        h.panel->openDisplayPill();
        QCOMPARE(writes(), 0);
        QCOMPARE(h.combo("pillDisplayWfColorScheme")->currentText(), QStringLiteral("Enhanced"));
        QCOMPARE(h.slider("pillDisplayWfBlackLevel")->value(), 61);
        QCOMPARE(h.slider("pillDisplayWfColorGain")->value(), 35);
        QVERIFY(!h.pan.wfAgcEnabled());
        QVERIFY(h.pan.waterfallNFAGCEnabled());
        QCOMPARE(h.slider("pillWaterfallAGCOffsetDb")->value(), 47);
        QCOMPARE(h.valueText("pillWaterfallAGCOffsetDb"), QStringLiteral("47 dB"));
        QCOMPARE(h.slider("pillDisplayWfLowLevel")->value(), -131);
        QCOMPARE(h.valueText("pillDisplayWfLowLevel"), QStringLiteral("-131 dBm"));
        QCOMPARE(h.slider("pillDisplayWfHighLevel")->value(), -71);
        QCOMPARE(h.valueText("pillDisplayWfHighLevel"), QStringLiteral("-71 dBm"));
        QVERIFY(!h.pan.wfUseSpectrumMinMax());
        QCOMPARE(h.slider("pillDisplayWfUpdatePeriodMs")->value(), 120);
        QCOMPARE(h.valueText("pillDisplayWfUpdatePeriodMs"), QStringLiteral("120 ms"));
        QVERIFY(h.toggle("pillWaterfallBlankingEnabled")->isChecked());
        QCOMPARE(h.valueText("pillWaterfallBlankingThresholdDb"), QStringLiteral("14 dB"));
        QCOMPARE(h.combo("pillDisplayWaterfallAveraging")->currentText(),
                 QStringLiteral("Time Window"));
        QCOMPARE(h.box("pillDisplayWaterfallAverageTimeMs")->text(), QStringLiteral("450 ms"));
        QCOMPARE(h.slider("pillDisplayWaterfallAverageTimeMs")->value(), 450);
        QCOMPARE(h.valueText("pillDisplayWfOpacity"), QStringLiteral("66%"));
        QVERIFY(h.toggle("pillWaterfallStopOnTx")->isChecked());
        QVERIFY(h.toggle("pillDisplayShowRxFilterOnWaterfall")->isChecked());
        QVERIFY(!h.toggle("pillDisplayShowTxFilterOnRxWaterfall")->isChecked());
        QVERIFY(h.toggle("pillDisplayShowRxZeroLine")->isChecked());
        QVERIFY(h.toggle("pillDisplayShowTxZeroLine")->isChecked());
        QCOMPARE(h.combo("pillDisplayWfTimestampPos")->currentText(), QStringLiteral("Right"));
        QCOMPARE(h.combo("pillDisplayWaterfallHistoryMs")->currentText(), QStringLiteral("5 minutes"));

        // The pan changing is shown, and showing it writes nothing back:
        // each change is announced once, by the pan.
        int expected = writes();
        const auto followed = [&](int line) {
            ++expected;
            if (writes() != expected) {
                qWarning() << "a follow wrote back, line" << line << writes() << expected;
                return false;
            }
            return true;
        };
        h.pan.setWfColorScheme(WfColorScheme::LinLog);
        QVERIFY(followed(__LINE__));
        QCOMPARE(h.combo("pillDisplayWfColorScheme")->currentText(), QStringLiteral("LinLog"));
        h.pan.setWfBlackLevel(44);
        QVERIFY(followed(__LINE__));
        QCOMPARE(h.slider("pillDisplayWfBlackLevel")->value(), 44);
        h.pan.setWfColorGain(80);
        QVERIFY(followed(__LINE__));
        QCOMPARE(h.slider("pillDisplayWfColorGain")->value(), 80);
        h.pan.setWfAgcEnabled(true);
        QVERIFY(followed(__LINE__));
        QVERIFY(h.pan.wfAgcEnabled());
        h.pan.setWaterfallNFAGCEnabled(false);
        QVERIFY(followed(__LINE__));
        QVERIFY(!h.pan.waterfallNFAGCEnabled());
        h.pan.setWaterfallAGCOffsetDb(-12);
        QVERIFY(followed(__LINE__));
        QCOMPARE(h.slider("pillWaterfallAGCOffsetDb")->value(), -12);
        h.pan.setWfLowThreshold(-140.0f);
        QVERIFY(followed(__LINE__));
        QCOMPARE(h.slider("pillDisplayWfLowLevel")->value(), -140);
        h.pan.setWfHighThreshold(-50.0f);
        QVERIFY(followed(__LINE__));
        QCOMPARE(h.slider("pillDisplayWfHighLevel")->value(), -50);
        // Scale projects the live spectrum pair and keeps remembered
        // manual endpoints. Runtime pair notifications are not writes.
        const float rememberedLow = h.pan.wfLowThreshold();
        const float rememberedHigh = h.pan.wfHighThreshold();
        h.pan.setWfUseSpectrumMinMax(true);
        QVERIFY(followed(__LINE__));
        QVERIFY(h.pan.wfUseSpectrumMinMax());
        QCOMPARE(h.pan.wfLowThreshold(), rememberedLow);
        QCOMPARE(h.pan.wfHighThreshold(), rememberedHigh);
        const auto scaleLevels = h.pan.effectiveWaterfallLevels();
        QCOMPARE(h.valueText("pillDisplayWfLowLevel"), QStringLiteral("%1 dBm").arg(qRound(scaleLevels.first)));
        QCOMPARE(h.valueText("pillDisplayWfHighLevel"), QStringLiteral("%1 dBm").arg(qRound(scaleLevels.second)));
        h.pan.setWfUseSpectrumMinMax(false);
        QVERIFY(followed(__LINE__));
        QVERIFY(!h.pan.wfUseSpectrumMinMax());
        h.pan.setWfUpdatePeriodMs(60);
        QVERIFY(followed(__LINE__));
        QCOMPARE(h.slider("pillDisplayWfUpdatePeriodMs")->value(), 60);
        h.pan.setWaterfallBlankingEnabled(false);
        QVERIFY(followed(__LINE__));
        QVERIFY(!h.toggle("pillWaterfallBlankingEnabled")->isChecked());
        h.pan.setWaterfallBlankingThresholdDb(17);
        QVERIFY(followed(__LINE__));
        QCOMPARE(h.slider("pillWaterfallBlankingThresholdDb")->value(), 17);
        h.pan.setWaterfallAveraging(SpectrumAveraging::Recursive);
        QVERIFY(followed(__LINE__));
        QCOMPARE(h.combo("pillDisplayWaterfallAveraging")->currentText(),
                 QStringLiteral("Recursive"));
        h.pan.setWaterfallAverageTimeMs(900);
        QVERIFY(followed(__LINE__));
        QCOMPARE(h.slider("pillDisplayWaterfallAverageTimeMs")->value(), 500);
        QCOMPARE(h.box("pillDisplayWaterfallAverageTimeMs")->value(), 900);
        h.pan.setWfOpacity(20);
        QVERIFY(followed(__LINE__));
        QCOMPARE(h.slider("pillDisplayWfOpacity")->value(), 20);
        h.pan.setWaterfallStopOnTx(false);
        QVERIFY(followed(__LINE__));
        QVERIFY(!h.toggle("pillWaterfallStopOnTx")->isChecked());
        h.pan.setShowRxFilterOnWaterfall(false);
        QVERIFY(followed(__LINE__));
        QVERIFY(!h.toggle("pillDisplayShowRxFilterOnWaterfall")->isChecked());
        h.pan.setShowTxFilterOnRxWaterfall(true);
        QVERIFY(followed(__LINE__));
        QVERIFY(h.toggle("pillDisplayShowTxFilterOnRxWaterfall")->isChecked());
        h.pan.setShowRxZeroLineOnWaterfall(false);
        QVERIFY(followed(__LINE__));
        QVERIFY(!h.toggle("pillDisplayShowRxZeroLine")->isChecked());
        h.pan.setShowTxZeroLineOnWaterfall(false);
        QVERIFY(followed(__LINE__));
        QVERIFY(!h.toggle("pillDisplayShowTxZeroLine")->isChecked());
        h.pan.setWfTimestampPosition(SpectrumWidget::TimestampPosition::Left);
        QVERIFY(followed(__LINE__));
        QCOMPARE(h.combo("pillDisplayWfTimestampPos")->currentText(), QStringLiteral("Left"));
        h.pan.setWaterfallHistoryMs(15LL * 60 * 1000);
        QVERIFY(followed(__LINE__));
        QCOMPARE(h.combo("pillDisplayWaterfallHistoryMs")->currentText(),
                 QStringLiteral("15 minutes"));

        // A control moved writes the pan, once.
        h.slider("pillDisplayWfOpacity")->setValue(75);
        QVERIFY(followed(__LINE__));
        QCOMPARE(h.pan.wfOpacity(), 75);
        h.slider("pillDisplayWfLowLevel")->setValue(-125);
        QVERIFY(followed(__LINE__));
        QCOMPARE(h.pan.wfLowThreshold(), -125.0f);
        h.slider("pillDisplayWfHighLevel")->setValue(-45);
        QVERIFY(followed(__LINE__));
        QCOMPARE(h.pan.wfHighThreshold(), -45.0f);
        h.slider("pillDisplayWfUpdatePeriodMs")->setValue(200);
        QVERIFY(followed(__LINE__));
        QCOMPARE(h.pan.wfUpdatePeriodMs(), 200);
        h.slider("pillDisplayWaterfallAverageTimeMs")->setValue(300);
        QVERIFY(followed(__LINE__));
        QCOMPARE(h.pan.waterfallAverageTimeMs(), 300);
        h.slider("pillDisplayWfBlackLevel")->setValue(52);
        QVERIFY(followed(__LINE__));
        QCOMPARE(h.pan.wfBlackLevel(), 52);
        h.combo("pillDisplayWaterfallAveraging")->setCurrentIndex(
            h.combo("pillDisplayWaterfallAveraging")->findText(QStringLiteral("Log Recursive")));
        QVERIFY(followed(__LINE__));
        QCOMPARE(h.pan.waterfallAveraging(), SpectrumAveraging::LogRecursive);
        h.combo("pillDisplayWfTimestampPos")->setCurrentIndex(0);
        QVERIFY(followed(__LINE__));
        QCOMPARE(h.pan.wfTimestampPosition(), SpectrumWidget::TimestampPosition::None);
        h.combo("pillDisplayWaterfallHistoryMs")->setCurrentIndex(0);
        QVERIFY(followed(__LINE__));
        QCOMPARE(h.pan.waterfallHistoryMs(), qint64(60000));
        h.toggle("pillWaterfallStopOnTx")->click();
        QVERIFY(followed(__LINE__));
        QVERIFY(h.pan.waterfallStopOnTx());
        h.toggle("pillDisplayShowRxFilterOnWaterfall")->click();
        QVERIFY(followed(__LINE__));
        QVERIFY(h.pan.showRxFilterOnWaterfall());
        h.toggle("pillDisplayShowTxFilterOnRxWaterfall")->click();
        QVERIFY(followed(__LINE__));
        QVERIFY(!h.pan.showTxFilterOnRxWaterfall());
        h.toggle("pillDisplayShowRxZeroLine")->click();
        QVERIFY(followed(__LINE__));
        QVERIFY(h.pan.showRxZeroLineOnWaterfall());
        h.toggle("pillDisplayShowTxZeroLine")->click();
        QVERIFY(followed(__LINE__));
        QVERIFY(h.pan.showTxZeroLineOnWaterfall());

        // The rows are this pan's, kept on this computer: no key of the
        // station's is among what the pan saves.
        h.pan.saveSettingsForTest();
        const QStringList keys = AppSettings::instance().allKeys();
        bool opacitySaved = false;
        for (const QString& key : keys) {
            if (key.contains(QStringLiteral("DisplayWfOpacity"))) {
                opacitySaved = AppSettings::instance().value(key).toString()
                            == QStringLiteral("75");
            }
            QVERIFY2(!key.startsWith(QStringLiteral("DisplayFftSize")), qPrintable(key));
            QVERIFY2(!key.startsWith(QStringLiteral("DisplayFftWindow")), qPrintable(key));
            QVERIFY2(!key.startsWith(QStringLiteral("DisplayHzPerBinTarget")), qPrintable(key));
            QVERIFY2(!key.startsWith(QStringLiteral("DisplaySpectrumFps")), qPrintable(key));
            QVERIFY2(!key.startsWith(QStringLiteral("DisplayGridMax_")), qPrintable(key));
            QVERIFY2(!key.startsWith(QStringLiteral("DisplayGridMin_")), qPrintable(key));
        }
        QVERIFY2(opacitySaved, qPrintable(keys.join(QLatin1Char(' '))));
    }

    // A reset is read again on the Waterfall tab too.
    void aResetPanIsReadAgainOnTheWaterfallTab() {
        PillHarness h;
        h.panel->openDisplayPill();
        h.slider("pillDisplayWfOpacity")->setValue(40);
        h.slider("pillWaterfallAGCOffsetDb")->setValue(-55);
        QCOMPARE(h.pan.wfOpacity(), 40);
        h.pan.resetPanDisplayOverrides();
        QCOMPARE(h.slider("pillDisplayWfOpacity")->value(), h.pan.wfOpacity());
        QCOMPARE(h.slider("pillWaterfallAGCOffsetDb")->value(),
                 h.pan.waterfallAGCOffsetDb());
        QCOMPARE(h.pan.wfOpacity(), 100);
        QCOMPARE(h.pan.waterfallAGCOffsetTenthsDb(), 0);
    }

    // No tab is taller than a pan; the one-line notes fit the pill's width.
    void theWaterfallAndFftTabsFitWithTheirNotes() {
        PillHarness h;
        h.panel->openDisplayPill();
        QWidget* pill = h.pill();
        for (const Tab tab : {Tab::Spectrum, Tab::Levels, Tab::Fft, Tab::Waterfall, Tab::View3D}) {
            h.panel->setDisplayPillTab(tab);
            qInfo() << "tab" << static_cast<int>(tab) << "height" << pill->height();
            QVERIFY2(pill->height() <= 480, qPrintable(QString::number(pill->height())));
            QVERIFY(h.scroll());
            QCOMPARE(h.scroll()->verticalScrollBar()->maximum(), 0);
        }
        // 268 less the frame's border and the page's margins.
        const int room = 268 - 2 - 16;
        const auto fits = [&](const char* name, const QString& text) {
            auto* label = h.control<QLabel>(name);
            if (!label || label->text() != text) {
                qWarning() << name << "reads" << (label ? label->text() : QString());
                return false;
            }
            const int width = QFontMetrics(label->font()).horizontalAdvance(text);
            qInfo() << text << "is" << width << "px of" << room;
            const int available = label->width() > 0 ? std::min(room, label->width()) : room;
            const QRect drawn = QFontMetrics(label->font()).boundingRect(
                QRect(0, 0, available, 1000), Qt::AlignLeft | Qt::AlignTop
                    | (label->wordWrap() ? Qt::TextWordWrap : 0), text);
            qInfo() << name << "drawn" << drawn.size() << "available" << label->size()
                    << "font pixels" << label->font().pixelSize();
            return drawn.width() <= room && drawn.height() <= label->height()
                && label->font().pixelSize() == 9;
        };
        h.panel->setClarityState(true, true, false);
        QVERIFY(fits("pillWaterfallNote",
                     QStringLiteral("Clarity sets Low and High from the noise floor.")));
        h.pan.setClarityEnabled(false);
        h.panel->setClarityState(false, false, false);
        // AGC is on as shipped: it sets Low.
        QVERIFY(h.pan.wfAgcEnabled());
        QVERIFY(fits("pillWaterfallNote",
                     QStringLiteral("Auto sets Low from the weakest signals. You set High.")));
        h.pan.setWfAgcEnabled(false);
        QVERIFY(fits("pillWaterfallNote",
                     QStringLiteral("You set Low and High.")));
        h.pan.setWfUseSpectrumMinMax(true);
        QVERIFY(fits("pillWaterfallNote",
                     QStringLiteral("Low and High are dB Min and dB Max, above.")));

        const QString dot = QString(QChar(0x00B7));
        SpectrumOverlayPanel::DisplayPillFftState state = h.panel->fftState();
        state.runningSize = 262144;
        state.sampleRateHz = 44100.0;
        state.decimation = 16;
        h.panel->setFftState(state);
        QVERIFY(fits("pillFftReadout",
                     QStringLiteral("0.01 Hz per bin ") + dot
                         + QStringLiteral(" 95.11 s per transform at 44.1 kHz")));
        // The line is the same in every sizing mode (design D38).
        state.sizingMode = ControlRanges::DisplayFftSizingMode::HzPerBin;
        state.hzPerBinTarget = 0.5;
        h.panel->setFftState(state);
        QVERIFY(fits("pillFftReadout",
                     QStringLiteral("0.01 Hz per bin ") + dot
                         + QStringLiteral(" 95.11 s per transform at 44.1 kHz")));
        h.panel->setRemoteWindow(true, false);
        QVERIFY(fits("pillStationNote",
                     QStringLiteral("This Core is too old to change Decimation.")));
        h.panel->setStationRowsAvailable(false,
                                         QStringLiteral("Connect to the Core to change these."));
        QVERIFY(fits("pillStationNote", QStringLiteral("Connect to the Core to change these.")));
    }
};

QTEST_MAIN(TstSpectrumOverlayPanel)
#include "tst_spectrum_overlay_panel.moc"
