// no-port-check: NereusSDR-original geometry and binding regression for
// the Display pill using the existing approved Longpath-derived workspace look.
// Modification history (NereusSDR):
//   2026-10-12 - J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-13 - Runtime level refresh event regressions. J.J. Boyd
//                 (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-22 - A closed pill is not laid out or restyled on each remote
//                 telemetry tick or enables pass, and opens to the same
//                 geometry. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

#include <QtTest/QtTest>
#include <QApplication>
#include <QEventLoop>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QRegularExpression>
#include <QSlider>
#include <QSplitter>
#include <QScrollArea>
#include <QScrollBar>
#include <QWheelEvent>
#include <QSpinBox>
#include <QStyleOptionSlider>
#include <QStyleOptionSpinBox>
#include <QTextDocument>

#include "core/AppSettings.h"
#include "gui/SpectrumOverlayPanel.h"
#include "gui/SpectrumWidget.h"
#include "gui/styles/AppTheme.h"
#include "gui/styles/WorkspaceComponents.h"
#include "gui/styles/WorkspacePreview.h"

using namespace NereusSDR;

namespace {
using Tab = SpectrumOverlayPanel::DisplayPillTab;

struct Harness {
    SpectrumWidget pan;
    SpectrumOverlayPanel* panel;
    Harness() : panel(new SpectrumOverlayPanel(&pan)) { pan.resize(900, 600); }
    QWidget* pill() const { return pan.findChild<QWidget*>(QStringLiteral("displayPill")); }
    template<typename T> T* control(const char* name) const {
        return pill()->findChild<T*>(QString::fromLatin1(name));
    }
    QPushButton* displayButton() const {
        for (QPushButton* button : panel->findChildren<QPushButton*>(QString(), Qt::FindDirectChildrenOnly)) {
            if (button->text() == QStringLiteral("Display")) { return button; }
        }
        return nullptr;
    }
    void layout() {
        pill()->adjustSize();
        pill()->layout()->setGeometry(pill()->rect());
        for (QWidget* child : pill()->findChildren<QWidget*>()) {
            if (child->layout()) { child->layout()->setGeometry(child->rect()); }
        }
    }
};

// Drain zero-delay geometry callbacks and the layout events they post, without
// waiting for render timers or showing the pan's native GPU surface.
void drainPillEvents() {
    for (int turn = 0; turn < 4; ++turn) {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }
}

class PillRefreshEvents : public QObject {
public:
    int enabledChanges{0};
    int styleChanges{0};
    int layoutRequests{0};
    int geometryChanges{0};
    int layoutEdits{0};

    void watch(QWidget* pill) {
        pill->installEventFilter(this);
        for (QObject* child : pill->findChildren<QObject*>()) {
            if (child->isWidgetType() || qobject_cast<QLayout*>(child)) {
                child->installEventFilter(this);
            }
        }
    }
    QString counts() const {
        return QStringLiteral("enabled=%1 style=%2 layoutRequests=%3 geometry=%4 layoutEdits=%5")
            .arg(enabledChanges).arg(styleChanges).arg(layoutRequests)
            .arg(geometryChanges).arg(layoutEdits);
    }
protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() == QEvent::EnabledChange) { ++enabledChanges; }
        if (event->type() == QEvent::StyleChange) { ++styleChanges; }
        if (event->type() == QEvent::LayoutRequest) { ++layoutRequests; }
        if (event->type() == QEvent::Move || event->type() == QEvent::Resize) { ++geometryChanges; }
        if (qobject_cast<QLayout*>(watched)
            && (event->type() == QEvent::ChildAdded || event->type() == QEvent::ChildRemoved)) {
            ++layoutEdits;
        }
        return false;
    }
};

void prepareClarityTelemetry(Harness& h) {
    h.pan.setUpdatesEnabled(false);
    h.pan.setClarityEnabled(true);
    // A noise-floor black point and a 10 dB span give hand-checkable numbers.
    h.pan.setWfBlackLevel(50);
    h.pan.setWfColorGain(100);
    h.pan.setClarityFloorDbm(-110.0f);
    h.pan.setClarityActive(true);
    h.panel->setDisplayPillTab(Tab::Levels);
    h.panel->openDisplayPill();
    drainPillEvents();
}

QRect handleOf(QSlider* slider) {
    QStyleOptionSlider option;
    option.initFrom(slider);
    option.orientation = slider->orientation();
    option.minimum = slider->minimum();
    option.maximum = slider->maximum();
    option.sliderPosition = slider->value();
    option.sliderValue = slider->value();
    option.singleStep = slider->singleStep();
    option.pageStep = slider->pageStep();
    option.upsideDown = slider->invertedAppearance();
    option.state |= QStyle::State_Horizontal;
    return slider->style()->subControlRect(QStyle::CC_Slider, &option, QStyle::SC_SliderHandle, slider);
}

QSize textSize(QLabel* label) {
    const int width = label->contentsRect().width();
    if (label->textFormat() == Qt::RichText
        || (label->textFormat() == Qt::AutoText && Qt::mightBeRichText(label->text()))) {
        QTextDocument document;
        document.setDocumentMargin(0);
        document.setDefaultFont(label->font());
        document.setHtml(label->text());
        document.setTextWidth(width);
        return document.size().toSize();
    }
    return label->fontMetrics().boundingRect(QRect(0, 0, width, 1000),
        Qt::AlignLeft | Qt::AlignTop | (label->wordWrap() ? Qt::TextWordWrap : 0), label->text()).size();
}
}

class TstWorkspaceDisplayPill : public QObject {
    Q_OBJECT
    QFont m_font;
    QPalette m_palette;
    QString m_sheet;
private slots:
    void init() {
        m_font = qApp->font();
        m_palette = qApp->palette();
        m_sheet = qApp->styleSheet();
        AppSettings::instance().clear();
        WorkspacePreview::setEnabledForTest(true);
        applyDarkPalette(*qApp);
        applyAppBaselineQss(*qApp);
        applyWorkspacePalette(*qApp);
    }
    void cleanup() {
        WorkspacePreview::setEnabledForTest(false);
        qApp->setFont(m_font);
        qApp->setStyleSheet(m_sheet);
        qApp->setPalette(m_palette);
    }
    void classicPillPagesPreserveFlyoutTransparency() {
        WorkspacePreview::setEnabledForTest(false);
        Harness h;
        h.panel->openDisplayPill();
        auto* scroll = h.pill()->findChild<QScrollArea*>(QStringLiteral("displayPillScroll"));
        QVERIFY(scroll);
        QVERIFY(!scroll->widget()->autoFillBackground());
    }

    void liveLevelsDoNotRefreshStaticControls_data() {
        QTest::addColumn<bool>("workspace");
        QTest::addColumn<float>("floorDbm");
        QTest::addColumn<QString>("lowText");
        QTest::addColumn<QString>("highText");
        QTest::newRow("workspace-rounded-unchanged") << true << -110.01f << QStringLiteral("-110 dBm") << QStringLiteral("-100 dBm");
        QTest::newRow("classic-rounded-unchanged") << false << -110.01f << QStringLiteral("-110 dBm") << QStringLiteral("-100 dBm");
        QTest::newRow("workspace-rounded-boundary") << true << -110.6f << QStringLiteral("-111 dBm") << QStringLiteral("-101 dBm");
        QTest::newRow("classic-rounded-boundary") << false << -110.6f << QStringLiteral("-111 dBm") << QStringLiteral("-101 dBm");
    }
    // A runtime-only level change must update the numbers without temporarily
    // enabling held editors, restyling static rows or rebuilding their layouts.
    void liveLevelsDoNotRefreshStaticControls() {
        QFETCH(bool, workspace);
        QFETCH(float, floorDbm);
        QFETCH(QString, lowText);
        QFETCH(QString, highText);
        WorkspacePreview::setEnabledForTest(workspace);
        Harness h;
        prepareClarityTelemetry(h);
        auto* low = h.control<QSlider>("pillDisplayWfLowLevel");
        auto* high = h.control<QSlider>("pillDisplayWfHighLevel");
        QVERIFY(low && high);
        QVERIFY(!low->isEnabled() && !high->isEnabled());
        const float savedLow = h.pan.wfLowThreshold();
        const float savedHigh = h.pan.wfHighThreshold();
        const QStringList savedKeys = AppSettings::instance().allKeys();
        const QRect pillRect = h.pill()->geometry();
        QSignalSpy changes(&h.pan, &SpectrumWidget::displaySettingChanged);
        PillRefreshEvents events;
        events.watch(h.pill());

        h.pan.setClarityFloorDbm(floorDbm);
        drainPillEvents();

        QCOMPARE(changes.count(), 1);
        QCOMPARE(changes.first().first().toString(), QStringLiteral("DisplayWfEffectiveLevels"));
        QCOMPARE(h.control<QLabel>("pillDisplayWfLowLevelValue")->text(), lowText);
        QCOMPARE(h.control<QLabel>("pillDisplayWfHighLevelValue")->text(), highText);
        QCOMPARE(low->value(), lowText == QStringLiteral("-110 dBm") ? -110 : -111);
        QCOMPARE(high->value(), highText == QStringLiteral("-100 dBm") ? -100 : -101);
        QCOMPARE(h.pan.wfLowThreshold(), savedLow);
        QCOMPARE(h.pan.wfHighThreshold(), savedHigh);
        QCOMPARE(AppSettings::instance().allKeys(), savedKeys);
        QCOMPARE(h.pill()->geometry(), pillRect);
        qInfo().noquote() << "live level refresh" << events.counts();
        QVERIFY2(events.enabledChanges == 0, qPrintable(events.counts()));
        QVERIFY2(events.styleChanges == 0, qPrintable(events.counts()));
        QVERIFY2(events.layoutEdits == 0, qPrintable(events.counts()));
    }

    void identicalAndClosedLevelsDoNotRefreshPill_data() {
        QTest::addColumn<bool>("workspace");
        QTest::newRow("workspace") << true;
        QTest::newRow("classic") << false;
    }
    void identicalAndClosedLevelsDoNotRefreshPill() {
        QFETCH(bool, workspace);
        WorkspacePreview::setEnabledForTest(workspace);
        Harness h;
        prepareClarityTelemetry(h);
        QSignalSpy changes(&h.pan, &SpectrumWidget::displaySettingChanged);
        {
            PillRefreshEvents events;
            events.watch(h.pill());
            h.pan.setClarityFloorDbm(-110.0f);
            drainPillEvents();
            QCOMPARE(changes.count(), 0);
            qInfo().noquote() << "identical level refresh" << events.counts();
            QVERIFY2(events.enabledChanges == 0 && events.styleChanges == 0
                && events.layoutEdits == 0 && events.geometryChanges == 0,
                qPrintable(events.counts()));
        }
        QPushButton* display = h.displayButton();
        QVERIFY(display);
        display->click();
        QVERIFY(!h.panel->displayPillVisible());
        drainPillEvents();
        const QString lowText = h.control<QLabel>("pillDisplayWfLowLevelValue")->text();
        const float savedLow = h.pan.wfLowThreshold();
        const float savedHigh = h.pan.wfHighThreshold();
        const QStringList savedKeys = AppSettings::instance().allKeys();
        PillRefreshEvents events;
        events.watch(h.pill());
        h.pan.setClarityFloorDbm(-110.6f);
        drainPillEvents();
        QCOMPARE(changes.count(), 1);
        QCOMPARE(changes.first().first().toString(), QStringLiteral("DisplayWfEffectiveLevels"));
        QCOMPARE(h.control<QLabel>("pillDisplayWfLowLevelValue")->text(), lowText);
        QCOMPARE(h.pan.wfLowThreshold(), savedLow);
        QCOMPARE(h.pan.wfHighThreshold(), savedHigh);
        QCOMPARE(AppSettings::instance().allKeys(), savedKeys);
        qInfo().noquote() << "closed level refresh" << events.counts();
        QVERIFY2(events.enabledChanges == 0 && events.styleChanges == 0
            && events.layoutEdits == 0 && events.geometryChanges == 0,
            qPrintable(events.counts()));
    }

    void levelActivationAndModeChoiceStillRefreshControls_data() {
        identicalAndClosedLevelsDoNotRefreshPill_data();
    }
    // Bypassing static refresh for telemetry must preserve real mode edges:
    // no signal becomes measured, then the Manual button releases the editors.
    void levelActivationAndModeChoiceStillRefreshControls() {
        QFETCH(bool, workspace);
        WorkspacePreview::setEnabledForTest(workspace);
        Harness h;
        h.pan.setUpdatesEnabled(false);
        h.pan.setClarityEnabled(true);
        h.pan.setClarityActive(false);
        h.pan.setWfBlackLevel(50);
        h.pan.setWfColorGain(100);
        h.panel->setDisplayPillTab(Tab::Levels);
        h.panel->openDisplayPill();
        drainPillEvents();
        auto* low = h.control<QSlider>("pillDisplayWfLowLevel");
        auto* high = h.control<QSlider>("pillDisplayWfHighLevel");
        auto* retune = h.control<QPushButton>("pillClarityRetune");
        QVERIFY(low && high && retune);
        QCOMPARE(h.control<QLabel>("pillDisplayWfLowLevelValue")->text(), QStringLiteral("no signal"));
        QCOMPARE(h.control<QLabel>("pillDisplayWfHighLevelValue")->text(), QStringLiteral("no signal"));
        h.pan.setClarityFloorDbm(-110.0f);
        QSignalSpy changes(&h.pan, &SpectrumWidget::displaySettingChanged);
        h.pan.setClarityActive(true);
        drainPillEvents();
        QCOMPARE(changes.count(), 1);
        QCOMPARE(changes.first().first().toString(), QStringLiteral("DisplayWfEffectiveLevels"));
        QCOMPARE(h.control<QLabel>("pillDisplayWfLowLevelValue")->text(), QStringLiteral("-110 dBm"));
        QCOMPARE(h.control<QLabel>("pillDisplayWfHighLevelValue")->text(), QStringLiteral("-100 dBm"));
        QVERIFY(!low->isEnabled() && !high->isEnabled() && retune->isEnabled());
        auto* manual = h.control<QPushButton>("pillWaterfallLevelsManual");
        QVERIFY(manual);
        manual->click();
        h.pan.setClarityActive(false);
        drainPillEvents();
        QVERIFY(manual->isChecked());
        QVERIFY(low->isEnabled() && high->isEnabled());
        QVERIFY(!retune->isEnabled());
        QCOMPARE(low->value(), qRound(h.pan.wfLowThreshold()));
        QCOMPARE(high->value(), qRound(h.pan.wfHighThreshold()));
        QVERIFY(h.control<QLabel>("pillWaterfallNote")->text().contains(QStringLiteral("You set Low and High")));
    }

    void repeatedClarityStatusDoesNotRefreshStaticControls_data() {
        QTest::addColumn<bool>("workspace");
        QTest::addColumn<bool>("open");
        QTest::newRow("workspace-open") << true << true;
        QTest::newRow("classic-open") << false << true;
        QTest::newRow("workspace-closed") << true << false;
        QTest::newRow("classic-closed") << false << false;
    }
    // Registry output reports its flags before deduplicating its own signal.
    // An identical status tuple must leave the pill's static widgets alone.
    void repeatedClarityStatusDoesNotRefreshStaticControls() {
        QFETCH(bool, workspace);
        QFETCH(bool, open);
        WorkspacePreview::setEnabledForTest(workspace);
        Harness h;
        prepareClarityTelemetry(h);
        h.panel->setClarityState(true, true, false);
        if (!open) {
            QVERIFY(h.displayButton());
            h.displayButton()->click();
        }
        QCOMPARE(h.panel->displayPillVisible(), open);
        drainPillEvents();
        const float savedLow = h.pan.wfLowThreshold();
        const float savedHigh = h.pan.wfHighThreshold();
        const QStringList savedKeys = AppSettings::instance().allKeys();
        QSignalSpy changes(&h.pan, &SpectrumWidget::displaySettingChanged);
        PillRefreshEvents events;
        events.watch(h.pill());
        for (int reading = 0; reading < 3; ++reading) {
            h.panel->setClarityState(true, true, false);
            drainPillEvents();
        }
        QCOMPARE(changes.count(), 0);
        QCOMPARE(h.control<QLabel>("pillClarityBadge")->property("clarityState").toString(), QStringLiteral("active"));
        QCOMPARE(h.control<QLabel>("pillDisplayWfLowLevelValue")->text(), QStringLiteral("-110 dBm"));
        QCOMPARE(h.control<QLabel>("pillDisplayWfHighLevelValue")->text(), QStringLiteral("-100 dBm"));
        QCOMPARE(h.pan.wfLowThreshold(), savedLow);
        QCOMPARE(h.pan.wfHighThreshold(), savedHigh);
        QCOMPARE(AppSettings::instance().allKeys(), savedKeys);
        qInfo().noquote() << "repeated Clarity status" << open << events.counts();
        QVERIFY2(events.enabledChanges == 0, qPrintable(events.counts()));
        QVERIFY2(events.styleChanges == 0, qPrintable(events.counts()));
        QVERIFY2(events.layoutEdits == 0, qPrintable(events.counts()));
    }

    void clarityStatusSeedsNewPanelAndPreservesEdges_data() {
        identicalAndClosedLevelsDoNotRefreshPill_data();
    }
    void clarityStatusSeedsNewPanelAndPreservesEdges() {
        QFETCH(bool, workspace);
        WorkspacePreview::setEnabledForTest(workspace);
        Harness h;
        h.pan.setUpdatesEnabled(false);
        h.pan.setClarityEnabled(true);
        auto* badge = h.control<QLabel>("pillClarityBadge");
        QVERIFY(badge);
        // The target's requested choice wins over a stale incoming choice.
        h.panel->setClarityState(false, true, true);
        drainPillEvents();
        QCOMPARE(badge->property("clarityState").toString(), QStringLiteral("paused"));
        h.panel->openDisplayPill();
        drainPillEvents();
        QCOMPARE(badge->property("clarityState").toString(), QStringLiteral("paused"));
        h.panel->setClarityState(false, true, false);
        drainPillEvents();
        QCOMPARE(badge->property("clarityState").toString(), QStringLiteral("active"));
        h.panel->setClarityState(false, false, false);
        drainPillEvents();
        QCOMPARE(badge->property("clarityState").toString(), QStringLiteral("off"));
        h.pan.setClarityEnabled(false);
        h.panel->setClarityState(true, true, false);
        drainPillEvents();
        QCOMPARE(badge->property("clarityState").toString(), QStringLiteral("off"));
    }

    void shortDockedPanKeepsEveryDisplayTabAccessible() {
        QSplitter host(Qt::Vertical);
        SpectrumWidget first;
        SpectrumWidget second;
        first.setUpdatesEnabled(false);
        second.setUpdatesEnabled(false);
        host.addWidget(&first);
        host.addWidget(&second);
        host.resize(1000, 600);
        auto* panel = new SpectrumOverlayPanel(&first);
        panel->move(4, 4);
        panel->show();
        host.show();
        QCoreApplication::processEvents();
        panel->openDisplayPill();
        QWidget* pill = first.findChild<QWidget*>(QStringLiteral("displayPill"));
        QVERIFY(pill);
        QSignalSpy writes(&first, &SpectrumWidget::displaySettingChanged);
        QSignalSpy tuneClicks(&first, &SpectrumWidget::frequencyClicked);
        QSignalSpy retunes(&first, &SpectrumWidget::ddcRetuneRequested);
        const auto contained = [&]() {
            return first.rect().contains(pill->geometry());
        };
        const Tab tabs[] = {Tab::Spectrum, Tab::Levels, Tab::Waterfall, Tab::View3D, Tab::Fft};
        for (Tab tab : tabs) {
            panel->setDisplayPillTab(tab);
            QCoreApplication::processEvents();
            QVERIFY2(contained(), qPrintable(QStringLiteral("pan %1x%2, pill %3,%4 %5x%6")
                .arg(first.width()).arg(first.height()).arg(pill->x()).arg(pill->y())
                .arg(pill->width()).arg(pill->height())));
            auto* scroll = pill->findChild<QScrollArea*>(QStringLiteral("displayPillScroll"));
            QVERIFY(scroll);
            QVERIFY(!scroll->widget()->autoFillBackground());
            const QString pageNames[] = {QStringLiteral("displayPageSpectrum"),
                QStringLiteral("displayPageLevels"), QStringLiteral("displayPageWaterfall"),
                QStringLiteral("displayPage3D"), QStringLiteral("displayPageFft")};
            QWidget* page = pill->findChild<QWidget*>(pageNames[static_cast<int>(tab)]);
            QVERIFY(page && !page->isHidden());
            QPushButton* footer = page->findChild<QPushButton*>(QStringLiteral("pillResetPan"));
            QVERIFY(footer);
            scroll->ensureWidgetVisible(footer, 0, 0);
            QCoreApplication::processEvents();
            const QRect footerRect(footer->mapTo(scroll->viewport(), QPoint()), footer->size());
            QVERIFY(scroll->viewport()->rect().contains(footerRect));
        }
        panel->setDisplayPillTab(Tab::Spectrum);
        host.resize(1000, 1200);
        QCoreApplication::processEvents();
        QVERIFY(contained());
        QVERIFY(pill->height() <= 480);
        auto* scroll = pill->findChild<QScrollArea*>(QStringLiteral("displayPillScroll"));
        QCOMPARE(scroll->verticalScrollBar()->maximum(), 0);
        const int normalWidth = pill->width();
        host.resize(1000, 450);
        QCoreApplication::processEvents();
        QVERIFY(contained());
        QVERIFY(scroll->verticalScrollBar()->maximum() > 0);
        QScrollBar* bar = scroll->verticalScrollBar();
        bar->setValue(0);
        QStyleOptionSlider option;
        option.initFrom(bar);
        option.orientation = Qt::Vertical;
        option.minimum = bar->minimum();
        option.maximum = bar->maximum();
        option.sliderPosition = bar->value();
        option.sliderValue = bar->value();
        option.pageStep = bar->pageStep();
        const QRect handle = bar->style()->subControlRect(QStyle::CC_ScrollBar, &option,
            QStyle::SC_ScrollBarSlider, bar);
        QTest::mousePress(bar, Qt::LeftButton, Qt::NoModifier, handle.center());
        const QPoint bottom(handle.center().x(), bar->height() - handle.height() / 2 - 1);
        QTest::mouseMove(bar, bottom);
        QTest::mouseRelease(bar, Qt::LeftButton, Qt::NoModifier, bottom);
        QVERIFY(bar->value() > 0);
        bar->setValue(0);
        const QPoint point = scroll->viewport()->rect().center();
        QWheelEvent wheel(point, scroll->viewport()->mapToGlobal(point), QPoint(), QPoint(0, -120),
            Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QCoreApplication::sendEvent(scroll->viewport(), &wheel);
        QVERIFY(bar->value() > 0);
        QCOMPARE(tuneClicks.count(), 0);
        QCOMPARE(retunes.count(), 0);
        host.resize(1000, 1200);
        QCoreApplication::processEvents();
        QVERIFY(contained());
        QCOMPARE(scroll->verticalScrollBar()->maximum(), 0);
        QCOMPARE(pill->width(), normalWidth);
        QCOMPARE(writes.count(), 0);
    }

    void workspaceControlsAndUserSwatchesUseTheExistingPipeline() {
        Harness h;
        h.pan.setFillColor(QColor(QStringLiteral("#00b4d8")));
        QSignalSpy writes(&h.pan, &SpectrumWidget::displaySettingChanged);
        h.panel->openDisplayPill();
        QVERIFY(h.pill()->property(WorkspacePreview::kThemedIslandProperty).toBool());
        auto* swatch = h.control<QPushButton>("pillDisplayFillColor");
        QVERIFY(swatch);
        QVERIFY(swatch->property(WorkspacePreview::kSkipProperty).toBool());
        QVERIFY(swatch->styleSheet().contains(QStringLiteral("#00b4d8")));
        auto* slider = h.control<QSlider>("pillDisplayFftFillAlpha");
        QVERIFY(slider);
        QVERIFY(slider->property(WorkspacePreview::kThemedQssProperty).isValid());
        QVERIFY(slider->height() >= Workspace::Components::kSliderHandleSpanPx);
        QCOMPARE(writes.count(), 0);
        h.pan.setFillColor(QColor(QStringLiteral("#c8d8e8")));
        QCOMPARE(writes.count(), 1);
        WorkspacePreview::themeSubtree(h.pill());
        QEvent show(QEvent::Show);
        QCoreApplication::sendEvent(swatch, &show);
        QVERIFY(swatch->styleSheet().contains(QStringLiteral("#c8d8e8")));
        QCOMPARE(writes.count(), 1);
    }
    void allPagesFitTheWorkspaceFontAndHandle_data() {
        QTest::addColumn<bool>("wide");
        QTest::newRow("normal") << false;
        QTest::newRow("wide") << true;
    }
    void allPagesFitTheWorkspaceFontAndHandle() {
        QFETCH(bool, wide);
        if (wide) {
            QFont font = qApp->font();
            font.setLetterSpacing(QFont::AbsoluteSpacing, 1.5);
            qApp->setFont(font);
        }
        Harness h;
        h.panel->openDisplayPill();
        h.pan.setPeakHoldDelayMs(10000);
        h.pan.setDbmCalOffset(-30.0f);
        QSignalSpy writes(&h.pan, &SpectrumWidget::displaySettingChanged);
        WorkspacePreview::themeSubtree(h.pill());
        QCoreApplication::processEvents();
        const int descendants = h.pill()->findChildren<QWidget*>().size();
        for (const Tab tab : {Tab::Spectrum, Tab::Levels, Tab::Waterfall, Tab::View3D, Tab::Fft}) {
            h.panel->setDisplayPillTab(tab);
            WorkspacePreview::themeSubtree(h.pill());
            QCoreApplication::processEvents();
            h.layout();
            qInfo() << "workspace fit" << wide << "tab" << static_cast<int>(tab)
                    << "pill" << h.pill()->size() << "tab budget"
                    << h.pill()->property("pillTabTextBudget");
            const char* diagnostics[] = {"displayPageSpectrum", "displayPageLevels", "displayPageWaterfall",
                                         "displayPage3D", "displayPageFft"};
            QWidget* diagnosticPage = h.control<QWidget>(diagnostics[static_cast<int>(tab)]);
            qInfo() << "intrinsic" << diagnosticPage->layout()->sizeHint()
                    << "minimum" << diagnosticPage->layout()->minimumSize()
                    << "pill minimum" << h.pill()->minimumSize();
            QCOMPARE(h.pill()->width(), 268);
            QVERIFY2(h.pill()->height() <= 480, qPrintable(QString::number(h.pill()->height())));
            const char* pages[] = {"displayPageSpectrum", "displayPageLevels", "displayPageWaterfall",
                                   "displayPage3D", "displayPageFft"};
            QWidget* page = h.control<QWidget>(pages[static_cast<int>(tab)]);
            QList<QPushButton*> buttons = page->findChildren<QPushButton*>();
            buttons.append(h.pill()->findChildren<QPushButton*>(
                QRegularExpression(QStringLiteral("^displayTab"))));
            for (QPushButton* button : buttons) {
                if (button->text().isEmpty() || button->isHidden()) { continue; }
                QVERIFY2(button->contentsRect().width() >= button->fontMetrics().horizontalAdvance(button->text()),
                         qPrintable(QStringLiteral("%1 text %2px, control %3px, tab budget %4px")
                             .arg(button->objectName()).arg(button->fontMetrics().horizontalAdvance(button->text()))
                             .arg(button->width()).arg(h.pill()->property("pillTabTextBudget").toInt())));
                const QRect rect(button->mapTo(h.pill(), QPoint(0, 0)), button->size());
                QVERIFY2(h.pill()->rect().contains(rect), qPrintable(button->objectName()));
            }
            for (QSlider* slider : page->findChildren<QSlider*>()) {
                if (slider->isHidden() || slider->parentWidget()->isHidden()) { continue; }
                const QRect handle = handleOf(slider);
                QVERIFY2(slider->rect().contains(handle), qPrintable(slider->objectName()));
                QRect rect = handle;
                for (QWidget* child = slider; child != h.pill(); child = child->parentWidget()) {
                    rect.translate(child->pos());
                    QVERIFY2(child->parentWidget()->rect().contains(rect), qPrintable(slider->objectName()));
                }
            }
            for (QLabel* label : page->findChildren<QLabel*>()) {
                if (label->text().isEmpty() || label->isHidden() || label->parentWidget()->isHidden()) { continue; }
                const QFont font = label->font();
                const qreal pixels = font.pixelSize() > 0 ? font.pixelSize()
                    : font.pointSizeF() * QFontMetricsF(font, label).fontDpi() / 72.0;
                QVERIFY2(pixels >= 11.0, qPrintable(label->objectName()));
                const QSize needed = textSize(label);
                QVERIFY2(needed.width() <= label->contentsRect().width()
                         && needed.height() <= label->contentsRect().height(),
                    qPrintable(QStringLiteral("%1 needs %2x%3, has %4x%5")
                        .arg(label->objectName()).arg(needed.width()).arg(needed.height())
                        .arg(label->width()).arg(label->height())));
            }
        }
        QCOMPARE(writes.count(), 0);
        // Repeating a style refresh must not add overflow rows indefinitely.
        WorkspacePreview::themeSubtree(h.pill());
        QCoreApplication::processEvents();
        QCOMPARE(h.pill()->findChildren<QWidget*>().size(), descendants);
    }
    void captureActualWorkspacePagesWhenRequested() {
        const QString directory = qEnvironmentVariable("NEREUS_WORKSPACE_DISPLAY_CAPTURE_DIR");
        if (directory.isEmpty()) { return; }
        QVERIFY(QDir().mkpath(directory));
        Harness h;
        h.panel->openDisplayPill();
        WorkspacePreview::themeSubtree(h.pill());
        QCoreApplication::processEvents();
        QJsonArray receipts;
        const struct { Tab tab; const char* name; } pages[] = {
            {Tab::Spectrum, "spectrum"}, {Tab::Levels, "levels"},
            {Tab::Waterfall, "waterfall"}, {Tab::View3D, "view"}, {Tab::Fft, "fft"}
        };
        for (const bool disabled : {false, true}) {
            h.pan.setPanFillEnabled(!disabled);
            h.pan.setPeakHoldEnabled(!disabled);
            for (const auto& page : pages) {
                h.panel->setDisplayPillTab(page.tab);
                WorkspacePreview::themeSubtree(h.pill());
                QCoreApplication::processEvents();
                h.layout();
                QCOMPARE(h.pan.panFillEnabled(), !disabled);
                QCOMPARE(h.control<QPushButton>("pillDisplayPanFill")->isChecked(), !disabled);
                QCOMPARE(h.control<QSlider>("pillDisplayFftFillAlpha")->isEnabled(), !disabled);
                QVERIFY(h.pill()->height() <= 480);
                const QString stem = QString::fromLatin1(page.name)
                    + (disabled ? QStringLiteral("-disabled") : QStringLiteral("-normal"));
                QJsonArray fonts;
                for (QLabel* label : h.pill()->findChildren<QLabel*>()) {
                    if (label->text().isEmpty()) { continue; }
                    const QFont font = label->font();
                    const qreal pixels = font.pixelSize() > 0 ? font.pixelSize()
                        : font.pointSizeF() * QFontMetricsF(font, label).fontDpi() / 72.0;
                    QVERIFY2(pixels >= 11.0, "Capture requires the actual signed shared Font B implementation");
                    fonts.append(QJsonObject{{"object", label->objectName()}, {"pixels", pixels}});
                }
                const char* pageNames[] = {"displayPageSpectrum", "displayPageLevels", "displayPageWaterfall",
                                           "displayPage3D", "displayPageFft"};
                QJsonArray sliders;
                QWidget* actualPage = h.control<QWidget>(pageNames[static_cast<int>(page.tab)]);
                for (QSlider* slider : actualPage->findChildren<QSlider*>()) {
                    const QRect handle = handleOf(slider);
                    sliders.append(QJsonObject{{"object", slider->objectName()}, {"enabled", slider->isEnabled()},
                        {"height", slider->height()}, {"handleWidth", handle.width()},
                        {"handleHeight", handle.height()}, {"style", slider->styleSheet()}});
                }
                QImage image(h.pill()->size(), QImage::Format_ARGB32_Premultiplied);
                image.fill(h.pill()->palette().color(QPalette::Window));
                h.pill()->render(&image);
                QVERIFY(image.save(directory + QLatin1Char('/') + stem + QStringLiteral(".png")));
                receipts.append(QJsonObject{{"image", stem + QStringLiteral(".png")},
                    {"width", h.pill()->width()}, {"height", h.pill()->height()},
                    {"tabTextBudget", h.pill()->property("pillTabTextBudget").toInt()},
                    {"theme", "actual shared Longpath-derived workspace"}, {"fonts", fonts}, {"sliders", sliders}});
            }
        }
        QFile receipt(directory + QStringLiteral("/workspace-pill-receipt.json"));
        QVERIFY(receipt.open(QIODevice::WriteOnly));
        QVERIFY(receipt.write(QJsonDocument(receipts).toJson(QJsonDocument::Indented)) > 0);
    }

    // MainWindow repeats the remote and station gates on every remote
    // telemetry tick. A closed pill neither lays itself out nor touches
    // its rows for an unchanged gate, or for another enables pass.
    void closedPillSkipsRepeatedGateRefreshes() {
        Harness h;
        h.pan.setUpdatesEnabled(false);
        const QString reason = QStringLiteral("Connect to the Core to change these.");
        h.panel->setRemoteWindow(true, false);
        h.panel->setStationRowsAvailable(false, reason);
        drainPillEvents();
        QVERIFY(!h.panel->displayPillVisible());
        const int refreshes = h.panel->displayPillGeometryRefreshCountForTest();
        PillRefreshEvents events;
        events.watch(h.pill());
        for (int tick = 0; tick < 5; ++tick) {
            h.panel->setRemoteWindow(true, false);
            h.panel->setStationRowsAvailable(false, reason);
            drainPillEvents();
        }
        QCOMPARE(h.panel->displayPillGeometryRefreshCountForTest(), refreshes);
        qInfo().noquote() << "repeated gates, closed pill" << events.counts();
        QVERIFY2(events.enabledChanges == 0 && events.styleChanges == 0
            && events.layoutEdits == 0, qPrintable(events.counts()));
        // A full enables pass (a GPU capability report runs one) restyles
        // nothing and lays nothing out while closed.
        PillRefreshEvents enablesEvents;
        enablesEvents.watch(h.pill());
        for (int pass = 0; pass < 3; ++pass) {
            h.panel->setGpuCapabilities(true, true);
            drainPillEvents();
        }
        qInfo().noquote() << "enables passes, closed pill" << enablesEvents.counts();
        QVERIFY2(enablesEvents.styleChanges == 0 && enablesEvents.layoutEdits == 0,
            qPrintable(enablesEvents.counts()));
        QCOMPARE(h.panel->displayPillGeometryRefreshCountForTest(), refreshes);
        // Opening lays it out once, at once.
        h.panel->openDisplayPill();
        QVERIFY(h.panel->displayPillGeometryRefreshCountForTest() > refreshes);
        drainPillEvents();
        // While open, a change is still laid out.
        const int openRefreshes = h.panel->displayPillGeometryRefreshCountForTest();
        h.panel->setStationRowsAvailable(false, QStringLiteral("The Core is busy."));
        drainPillEvents();
        QVERIFY(h.panel->displayPillGeometryRefreshCountForTest() > openRefreshes);
    }

    // The reason alone changing still reaches the rows and the note while
    // the pill is closed, and so does the remote gate.
    void closedPillTakesAChangedReason() {
        Harness h;
        h.pan.setUpdatesEnabled(false);
        const QString first = QStringLiteral("Connect to the Core to change these.");
        const QString second = QStringLiteral("The Core is not answering.");
        auto* note = h.control<QLabel>("pillStationNote");
        auto* decimation = h.control<QSlider>("pillDisplayDecimation");
        QVERIFY(note && decimation);
        h.panel->setStationRowsAvailable(false, first);
        drainPillEvents();
        QCOMPARE(note->text(), first);
        QCOMPARE(decimation->toolTip(), first);
        QVERIFY(!decimation->isEnabled());
        h.panel->setStationRowsAvailable(false, second);
        drainPillEvents();
        QCOMPARE(note->text(), second);
        QCOMPARE(decimation->toolTip(), second);
        h.panel->setStationRowsAvailable(true, QString());
        drainPillEvents();
        QVERIFY(note->isHidden());
        QVERIFY(decimation->isEnabled());
        h.panel->setRemoteWindow(true, false);
        drainPillEvents();
        QVERIFY(!note->isHidden());
        QCOMPARE(note->text(), QStringLiteral("This Core is too old to change Decimation."));
        h.panel->setRemoteWindow(true, true);
        drainPillEvents();
        QVERIFY(note->isHidden());
        QVERIFY(!h.panel->displayPillVisible());
    }

    // Changes made while the pill is closed lay out on open exactly as the
    // same changes made while it was open.
    void pillOpenedAfterClosedChangesMatchesAnOpenPill() {
        const auto change = [](Harness& h) {
            h.panel->setRemoteWindow(true, false);
            h.panel->setStationRowsAvailable(false,
                QStringLiteral("Connect to the Core to change these settings, then try again."));
            h.pan.setClarityEnabled(true);
            h.panel->setClarityState(true, true, true);
            drainPillEvents();
        };
        const auto shape = [](Harness& h) {
            QStringList rows;
            rows << QStringLiteral("pill %1x%2").arg(h.pill()->width()).arg(h.pill()->height());
            const QList<QWidget*> widgets = h.pill()->findChildren<QWidget*>();
            for (int index = 0; index < widgets.size(); ++index) {
                const QWidget* w = widgets[index];
                rows << QStringLiteral("%1 %2 min %3x%4 max %5x%6 hint %7x%8 hidden %9")
                    .arg(index).arg(w->objectName())
                    .arg(w->minimumWidth()).arg(w->minimumHeight())
                    .arg(w->maximumWidth()).arg(w->maximumHeight())
                    .arg(w->sizeHint().width()).arg(w->sizeHint().height())
                    .arg(w->isHidden());
            }
            return rows;
        };
        Harness open;
        open.pan.setUpdatesEnabled(false);
        open.panel->openDisplayPill();
        drainPillEvents();
        change(open);
        Harness closed;
        closed.pan.setUpdatesEnabled(false);
        change(closed);
        closed.panel->openDisplayPill();
        drainPillEvents();
        QVERIFY(open.panel->displayPillVisible() && closed.panel->displayPillVisible());
        for (Tab tab : {Tab::Spectrum, Tab::Fft, Tab::Levels, Tab::Waterfall, Tab::View3D}) {
            open.panel->setDisplayPillTab(tab);
            closed.panel->setDisplayPillTab(tab);
            drainPillEvents();
            const QStringList a = shape(open);
            const QStringList b = shape(closed);
            QCOMPARE(a.size(), b.size());
            for (int row = 0; row < a.size(); ++row) {
                QVERIFY2(a[row] == b[row],
                         qPrintable(QStringLiteral("tab %1: open [%2] closed [%3]")
                             .arg(static_cast<int>(tab)).arg(a[row], b[row])));
            }
        }
    }

    void dynamicReasonsAndNumberBoxesRemainWhole() {
        Harness h;
        h.panel->openDisplayPill();
        WorkspacePreview::themeSubtree(h.pill());
        QCoreApplication::processEvents();
        h.panel->setDisplayPillTab(Tab::Spectrum);
        h.layout();
        auto* source = h.control<QSlider>("pillDisplayFftFillAlpha");
        h.pan.setPanFillEnabled(false);
        h.pan.setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode3D));
        QSignalSpy writes(&h.pan, &SpectrumWidget::displaySettingChanged);
        QEvent enter(QEvent::Enter);
        QCoreApplication::sendEvent(source, &enter);
        WorkspacePreview::themeSubtree(h.pill());
        QCoreApplication::processEvents();
        h.layout();
        auto* page = h.control<QWidget>("displayPageSpectrum");
        auto* reason = page->findChild<QLabel*>(QStringLiteral("pillReason"));
        QCOMPARE(reason->text(), source->toolTip());
        QVERIFY(textSize(reason).height() <= reason->contentsRect().height());
        for (const char* name : {"pillDisplaySpectrumAverageTimeMsBox", "pillDisplayWaterfallAverageTimeMsBox"}) {
            auto* box = h.control<QSpinBox>(name);
            QStyleOptionSpinBox option;
            option.initFrom(box);
            option.rect = box->rect();
            option.subControls = QStyle::SC_All;
            option.buttonSymbols = box->buttonSymbols();
            option.frame = box->hasFrame();
            const QRect field = box->style()->subControlRect(QStyle::CC_SpinBox, &option,
                QStyle::SC_SpinBoxEditField, box);
            auto* edit = box->findChild<QLineEdit*>();
            QVERIFY(edit);
            QVERIFY2(edit->fontMetrics().horizontalAdvance(QStringLiteral("9999 ms")) + 4 <= field.width(), name);
        }
        QCOMPARE(writes.count(), 0);
    }
};
QTEST_MAIN(TstWorkspaceDisplayPill)
#include "tst_workspace_display_pill.moc"
