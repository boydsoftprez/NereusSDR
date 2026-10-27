// no-port-check: NereusSDR-original tests for the VFO display item's
// clicks, wheel, keys and screens by slice.
// Modification history (NereusSDR):
//   2026-10-26 - PR 677 review fix wave (I7): the long press tests wait
//                 for the timer's result (QTRY) instead of a fixed 1.1 s.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - Wheel and click stepping, typed digits, the lock, the long
//                 press, the BAND, MODE, FILTER and TUNE_STEP screens and
//                 their close, the meter's key and hover-leave dispatch, and
//                 MainWindow's routing (TX is a handoff only, never MOX),
//                 a VFO display face's own display included.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - Final review fix wave (I2): a VFO display or dial on
//                 another radio's entry shows none of this window's slices
//                 and says why. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.

#include <QtTest>
#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonObject>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QWheelEvent>
#include <memory>

#include "core/AppSettings.h"
#include "core/MoxController.h"
#include "core/RadioDiscovery.h"
#include "gui/MainWindow.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/meters/DialItem.h"
#include "gui/meters/MeterWidget.h"
#include "gui/meters/SpacerItem.h"
#include "gui/meters/VfoDisplayItem.h"
#include "gui/meters/presets/VfoDisplayPresetItem.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

namespace {

constexpr int kW = 800;
constexpr int kH = 100;
// One column: Thetis's x fractions are of w * 2.07 (renderVfoDisplay's
// x_multy for one VFO).
constexpr double kUnit = kW * 2.07;

QPointF at(double fx, double fy) { return QPointF(kUnit * fx, kH * fy); }

// The centres of the boxes the tests use.
const QPointF kThirdKhzBox = at(0.346 + (0.416 - 0.346) / 3.0 * 2.5, 0.30);  // 1 kHz
const QPointF kHundredHzBox = at(0.425 + (0.480 - 0.425) / 3.0 * 0.5, 0.35);  // 100 Hz
const QPointF kBandBox = at(0.40, 0.70);
const QPointF kModeBox = at(0.04, 0.70);
const QPointF kFilterBox = at(0.29, 0.70);
const QPointF kTxBox = at(0.176, 0.70);
const QPointF kLockBox = at(0.2115, 0.70);
const QPointF kSplitBox = at(0.15, 0.20);
const QPointF kSyncBox = at(0.237, 0.70);

// A screen's button `cell` in a grid of `columns` x 2 over the item.
QPointF cell(int index, int columns)
{
    const double gap = double(kW) / columns;
    return QPointF(gap * (index % columns) + gap / 2.0, kH / 4.0 + (index / columns) * (kH / 2.0));
}

VfoSliceState slice(int index, qint64 hz, bool locked = false)
{
    VfoSliceState s;
    s.index = index;
    s.frequencyHz = hz;
    s.mode = QStringLiteral("USB");
    s.filter = QStringLiteral("2.7k");
    s.band = QStringLiteral("20m");
    s.stepHz = 100;
    s.locked = locked;
    s.active = true;
    s.open = true;
    return s;
}

// A Thetis-layout display on Source slice, showing slice `index`.
void setUp(VfoDisplayItem& vfo, int index, qint64 hz, bool locked = false)
{
    vfo.setLayout(VfoDisplayItem::Layout::Thetis);
    vfo.setRect(0, 0, 1, 1);
    vfo.setSliceState(index, slice(index, hz, locked));
    vfo.setSourceSliceIndex(index);
}

bool wheel(VfoDisplayItem& vfo, QPointF pos, int angle)
{
    QWheelEvent e(pos, pos, QPoint(), QPoint(0, angle), Qt::NoButton, Qt::NoModifier,
                  Qt::NoScrollPhase, false);
    return vfo.handleWheel(&e, kW, kH);
}

void press(VfoDisplayItem& vfo, QPointF pos, Qt::MouseButton button = Qt::LeftButton)
{
    QMouseEvent e(QEvent::MouseButtonPress, pos, pos, button, button, Qt::NoModifier);
    vfo.handleMousePress(&e, kW, kH);
}

void release(VfoDisplayItem& vfo, QPointF pos, Qt::MouseButton button = Qt::LeftButton)
{
    QMouseEvent e(QEvent::MouseButtonRelease, pos, pos, button, Qt::NoButton, Qt::NoModifier);
    vfo.handleMouseRelease(&e, kW, kH);
}

void click(VfoDisplayItem& vfo, QPointF pos, Qt::MouseButton button = Qt::LeftButton)
{
    press(vfo, pos, button);
    release(vfo, pos, button);
}

void move(VfoDisplayItem& vfo, QPointF pos)
{
    QMouseEvent e(QEvent::MouseMove, pos, pos, Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    vfo.handleMouseMove(&e, kW, kH);
}

bool key(VfoDisplayItem& vfo, Qt::Key k)
{
    QKeyEvent e(QEvent::KeyPress, k, Qt::NoModifier, QString(QChar(char(k))));
    return vfo.handleKeyPress(&e);
}

// Every request the display can make, counted together.
struct Requests {
    explicit Requests(VfoDisplayItem* vfo)
        : step(vfo, &VfoDisplayItem::frequencyChangeRequested),
          set(vfo, &VfoDisplayItem::frequencySetRequested),
          band(vfo, &VfoDisplayItem::bandRequested),
          mode(vfo, &VfoDisplayItem::modeRequested),
          filter(vfo, &VfoDisplayItem::filterRequested),
          tuneStep(vfo, &VfoDisplayItem::tuneStepRequested),
          tx(vfo, &VfoDisplayItem::txRequested),
          lock(vfo, &VfoDisplayItem::lockToggleRequested),
          bandStack(vfo, &VfoDisplayItem::bandStackRequested),
          filterMenu(vfo, &VfoDisplayItem::filterContextRequested) {}
    int total() const
    {
        return int(step.count() + set.count() + band.count() + mode.count() + filter.count()
                   + tuneStep.count() + tx.count() + lock.count() + bandStack.count()
                   + filterMenu.count());
    }
    QSignalSpy step, set, band, mode, filter, tuneStep, tx, lock, bandStack, filterMenu;
};

} // namespace

class TestVfoDisplayItemInput : public QObject {
    Q_OBJECT

private slots:
    void wheelOverThirdKhzBoxStepsOneKilohertz()
    {
        VfoDisplayItem vfo;
        setUp(vfo, 1, 14'074'000);
        Requests r(&vfo);
        QVERIFY(wheel(vfo, kThirdKhzBox, 120));
        QCOMPARE(r.step.count(), 1);
        QCOMPARE(r.step.at(0).at(0).toInt(), 1);
        QCOMPARE(r.step.at(0).at(1).toLongLong(), 1000LL);
        QVERIFY(wheel(vfo, kThirdKhzBox, -120));
        QCOMPARE(r.step.at(1).at(1).toLongLong(), -1000LL);
        QVERIFY(wheel(vfo, kHundredHzBox, 120));
        QCOMPARE(r.step.at(2).at(1).toLongLong(), 100LL);
        // Off the digits the wheel is not taken.
        QVERIFY(!wheel(vfo, kModeBox, 120));
        QCOMPARE(r.total(), 3);
    }

    void lockedSliceRefusesEveryChange()
    {
        VfoDisplayItem vfo;
        setUp(vfo, 1, 14'074'000, true);
        Requests r(&vfo);
        QVERIFY(wheel(vfo, kThirdKhzBox, 120));   // taken, but nothing emitted
        click(vfo, kThirdKhzBox);
        click(vfo, kThirdKhzBox, Qt::RightButton);
        move(vfo, kHundredHzBox);
        QVERIFY(key(vfo, Qt::Key_7));
        click(vfo, kBandBox);
        QCOMPARE(vfo.screenFor(1), VfoDisplayItem::Screen::Vfo);
        click(vfo, kModeBox);
        QCOMPARE(vfo.screenFor(1), VfoDisplayItem::Screen::Vfo);
        QCOMPARE(r.total(), 0);
        // The lock itself still toggles.
        click(vfo, kLockBox);
        QCOMPARE(r.lock.count(), 1);
        QCOMPARE(r.lock.at(0).at(0).toInt(), 1);
    }

    void clicksStepByTheDigit()
    {
        VfoDisplayItem vfo;
        setUp(vfo, 1, 14'074'000);
        Requests r(&vfo);
        click(vfo, kThirdKhzBox);
        click(vfo, kThirdKhzBox, Qt::RightButton);
        QCOMPARE(r.step.count(), 2);
        QCOMPARE(r.step.at(0).at(1).toLongLong(), 1000LL);
        QCOMPARE(r.step.at(1).at(1).toLongLong(), -1000LL);
        QCOMPARE(r.total(), 2);
    }

    void typedDigitReplacesTheHoveredDigit()
    {
        VfoDisplayItem vfo;
        setUp(vfo, 1, 14'074'250);
        Requests r(&vfo);
        // Not over a digit: the key is not handled.
        move(vfo, kModeBox);
        QVERIFY(!key(vfo, Qt::Key_7));
        move(vfo, kHundredHzBox);
        QVERIFY(key(vfo, Qt::Key_7));
        QCOMPARE(r.set.count(), 1);
        QCOMPARE(r.set.at(0).at(0).toInt(), 1);
        QCOMPARE(r.set.at(0).at(1).toLongLong(), 14'074'750LL);
        // A letter is not a digit.
        QVERIFY(!key(vfo, Qt::Key_A));
        QCOMPARE(r.total(), 1);
    }

    void longPressOverKhzOpensTuneStep()
    {
        VfoDisplayItem vfo;
        setUp(vfo, 1, 14'074'000);
        Requests r(&vfo);
        QSignalSpy repaint(&vfo, &VfoDisplayItem::repaintRequested);
        press(vfo, kThirdKhzBox);
        // Waits for the long press timer rather than a fixed time, so a
        // loaded machine that fires it late still passes.
        QTRY_COMPARE_WITH_TIMEOUT(vfo.screenFor(1), VfoDisplayItem::Screen::TuneStep, 5000);
        QVERIFY(repaint.count() >= 1);
        // The release that ends the long press steps nothing.
        release(vfo, kThirdKhzBox);
        QCOMPARE(r.total(), 0);
        click(vfo, cell(8, 8));
        QCOMPARE(r.tuneStep.count(), 1);
        QCOMPARE(r.tuneStep.at(0).at(0).toInt(), 1);
        QCOMPARE(r.tuneStep.at(0).at(1).toInt(), kTuneStepList[11].stepHz);
        QCOMPARE(vfo.screenFor(1), VfoDisplayItem::Screen::Vfo);
        QCOMPARE(r.total(), 1);
    }

    void tuneStepButtonsMapToTheStepList()
    {
        const int expected[16] = {0, 2, 3, 4, 5, 6, 7, 8, 11, 12, 14, 15, 17, 18, 20, 21};
        for (int i = 0; i < 16; ++i) {
            QCOMPARE(kVfoStepButtonToIndex[i], expected[i]);
        }
    }

    void longPressOnBandAndFilterAsksForStackAndMenu()
    {
        VfoDisplayItem vfo;
        setUp(vfo, 1, 14'074'000);
        Requests r(&vfo);
        press(vfo, kBandBox);
        QTRY_COMPARE_WITH_TIMEOUT(r.bandStack.count(), 1, 5000);
        release(vfo, kBandBox);
        QCOMPARE(r.bandStack.count(), 1);
        QCOMPARE(vfo.screenFor(1), VfoDisplayItem::Screen::Vfo);
        press(vfo, kFilterBox);
        QTRY_COMPARE_WITH_TIMEOUT(r.filterMenu.count(), 1, 5000);
        release(vfo, kFilterBox);
        QCOMPARE(r.filterMenu.count(), 1);
        QCOMPARE(vfo.screenFor(1), VfoDisplayItem::Screen::Vfo);
        QCOMPARE(r.total(), 2);
    }

    void shortClicksOpenScreensThatAct()
    {
        VfoDisplayItem vfo;
        setUp(vfo, 1, 14'074'000);
        Requests r(&vfo);

        // BAND: 14 MHz opens the HF group; button 5 is 20 m.
        click(vfo, kBandBox);
        QCOMPARE(vfo.screenFor(1), VfoDisplayItem::Screen::Band);
        QCOMPARE(vfo.screenBandGroup(1), BandGroup::Hf);
        click(vfo, cell(5, 8));
        QCOMPARE(r.band.count(), 1);
        QCOMPARE(r.band.at(0).at(1).value<Band>(), Band::Band20m);
        QCOMPARE(vfo.screenFor(1), VfoDisplayItem::Screen::Vfo);
        // The SWL jump (button 14) stays open on the GEN group.
        click(vfo, kBandBox);
        click(vfo, cell(14, 8));
        QCOMPARE(vfo.screenFor(1), VfoDisplayItem::Screen::Band);
        QCOMPARE(vfo.screenBandGroup(1), BandGroup::Gen);
        click(vfo, cell(1, 8));
        QCOMPARE(r.band.count(), 2);
        QCOMPARE(r.band.at(1).at(1).value<Band>(), Band::Band120m);

        // MODE: button 0 is LSB.
        click(vfo, kModeBox);
        QCOMPARE(vfo.screenFor(1), VfoDisplayItem::Screen::Mode);
        click(vfo, cell(0, 6));
        QCOMPARE(r.mode.count(), 1);
        QCOMPARE(r.mode.at(0).at(1).value<DSPMode>(), DSPMode::LSB);
        QCOMPARE(vfo.screenFor(1), VfoDisplayItem::Screen::Vfo);

        // FILTER: button 2 is F3.
        click(vfo, kFilterBox);
        QCOMPARE(vfo.screenFor(1), VfoDisplayItem::Screen::Filter);
        click(vfo, cell(2, 6));
        QCOMPARE(r.filter.count(), 1);
        QCOMPARE(r.filter.at(0).at(1).toInt(), 2);
        QCOMPARE(vfo.screenFor(1), VfoDisplayItem::Screen::Vfo);
        QCOMPARE(r.total(), 4);
    }

    void screenClosesAfterFiveSecondsWithoutInput()
    {
        VfoDisplayItem vfo;
        setUp(vfo, 1, 14'074'000);
        QSignalSpy repaint(&vfo, &VfoDisplayItem::repaintRequested);
        QElapsedTimer opened;
        click(vfo, kModeBox);
        opened.start();
        QCOMPARE(vfo.screenFor(1), VfoDisplayItem::Screen::Mode);
        QTRY_VERIFY_WITH_TIMEOUT(vfo.screenFor(1) == VfoDisplayItem::Screen::Vfo, 9000);
        QVERIFY2(opened.elapsed() > 5000, qPrintable(QString::number(opened.elapsed())));
        QVERIFY(repaint.count() >= 1);
    }

    void txClickRequestsTheSliceOnly()
    {
        VfoDisplayItem vfo;
        setUp(vfo, 1, 14'074'000);
        Requests r(&vfo);
        click(vfo, kTxBox);
        QCOMPARE(r.tx.count(), 1);
        QCOMPARE(r.tx.at(0).at(0).toInt(), 1);
        QCOMPARE(r.total(), 1);
    }

    void splitAndSyncEmitNothing()
    {
        VfoDisplayItem vfo;
        setUp(vfo, 0, 14'074'000);   // the split box is on Slice A
        Requests r(&vfo);
        QVERIFY(!vfo.reasonAt(kSplitBox, kW, kH).isEmpty());
        click(vfo, kSplitBox);
        click(vfo, kSplitBox, Qt::RightButton);
        click(vfo, kSyncBox);
        QCOMPARE(r.total(), 0);
        QCOMPARE(vfo.screenFor(0), VfoDisplayItem::Screen::Vfo);
    }

    void clickActsOnTheColumnsSlice()
    {
        // All slices: Slice B's column acts on Slice B, not the Source.
        VfoDisplayItem vfo;
        vfo.setLayout(VfoDisplayItem::Layout::Thetis);
        vfo.setRect(0, 0, 1, 1);
        vfo.setDisplayMode(VfoDisplayItem::DisplayMode::AllSlices);
        vfo.setSliceState(0, slice(0, 14'074'000));
        vfo.setSliceState(1, slice(1, 7'074'000));
        vfo.setSourceSliceIndex(0);
        Requests r(&vfo);
        // Two columns of kW / 2; Thetis's fractions are of the full width.
        const QPointF txB(kW / 2.0 + kW * 0.176, kH * 0.7);
        click(vfo, txB);
        QCOMPARE(r.tx.count(), 1);
        QCOMPARE(r.tx.at(0).at(0).toInt(), 1);
    }

    void meterSendsKeysAndHoverLeave()
    {
        MeterWidget meter;
        meter.resize(kW, 2 * kH);
        QCOMPARE(meter.focusPolicy(), Qt::ClickFocus);
        auto* vfo = new VfoDisplayItem();
        setUp(*vfo, 1, 14'074'250);
        vfo->setRect(0, 0, 1, 0.5);
        meter.addItem(vfo);
        auto* below = new SpacerItem();
        below->setRect(0, 0.5, 1, 0.5);
        meter.addItem(below);
        Requests r(vfo);
        const auto sendMove = [&](QPointF pos) {
            QMouseEvent e(QEvent::MouseMove, pos, pos, Qt::NoButton, Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(&meter, &e);
        };
        const auto sendKey = [&](Qt::Key k) {
            QKeyEvent e(QEvent::KeyPress, k, Qt::NoModifier, QString(QChar(char(k))));
            QApplication::sendEvent(&meter, &e);
        };
        // Over the 100 Hz digit, a typed 7 replaces it.
        sendMove(kHundredHzBox);
        sendKey(Qt::Key_7);
        QCOMPARE(r.set.count(), 1);
        QCOMPARE(r.set.at(0).at(1).toLongLong(), 14'074'750LL);
        // Moved off onto the item below: the display hears the pointer
        // left and a key does nothing.
        sendMove(QPointF(kW / 2.0, kH * 1.5));
        sendKey(Qt::Key_7);
        QCOMPARE(r.set.count(), 1);
        // Back over the digit, then out of the meter: nothing again.
        sendMove(kHundredHzBox);
        QEvent leave(QEvent::Leave);
        QApplication::sendEvent(&meter, &leave);
        sendKey(Qt::Key_7);
        QCOMPARE(r.set.count(), 1);
    }

    // MainWindow: each request acts on the slice it names, through the
    // ordinary setters; TX is the handoff alone and never keys; another
    // radio's entry acts on nothing.
    void mainWindowRoutesRequestsAndNeverKeys()
    {
        AppSettings::setProfileOverride(QStringLiteral("vfo-input-%1").arg(QCoreApplication::applicationPid()));
        AppSettings::instance().clear();
        QVERIFY(AppSettings::instance().save());
        RadioDiscovery discovery;
        discovery.holdOffScans(std::chrono::minutes(5));
        const auto cleanup = qScopeGuard([] {
            QFile::remove(AppSettings::instance().filePath());
            RadioDiscovery::clearHoldOffForTest();
        });
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        RadioModel* model = window.radioModel();
        model->configureStreamPool(5, 5, 192000);
        while (model->slices().size() < 2) { QVERIFY(model->addSlice() >= 0); }
        SliceModel* a = model->sliceById(0);
        SliceModel* b = model->sliceById(1);
        QVERIFY(a && b);
        a->setFrequency(14'074'000.0);
        b->setFrequency(7'074'000.0);
        b->setDspMode(DSPMode::USB);

        ContainerManager* cm = model->containerManager();
        QVERIFY(cm);
        ContainerWidget* container = cm->createContainer(1, DockMode::OverlayDocked);
        QVERIFY(container);
        auto* meter = new MeterWidget();
        container->setContent(meter);
        auto* vfo = new VfoDisplayItem();
        vfo->setLayout(VfoDisplayItem::Layout::Thetis);
        vfo->setRect(0, 0, 1, 1);
        meter->addItem(vfo);
        QVERIFY(vfo->property("vfoRequestsWired").toBool());

        QSignalSpy handoff(model, &RadioModel::txSliceSelected);
        QSignalSpy mox(&model->transmitModel(), &TransmitModel::moxChanged);
        QSignalSpy tune(&model->transmitModel(), &TransmitModel::tuneChanged);
        std::unique_ptr<QSignalSpy> moxState;
        if (MoxController* controller = model->moxController()) {
            moxState = std::make_unique<QSignalSpy>(controller, &MoxController::moxStateChanged);
        }

        // TX on Slice B: the handoff with B's id, and nothing keys.
        emit vfo->txRequested(1);
        QCOMPARE(handoff.count(), 1);
        QCOMPARE(handoff.at(0).at(0).toInt(), 1);
        QCOMPARE(model->txBoundSlice(), b);
        QCOMPARE(mox.count(), 0);
        QCOMPARE(tune.count(), 0);
        if (moxState) { QCOMPARE(moxState->count(), 0); }

        // Mode, step, frequency and lock act on B alone.
        const DSPMode aMode = a->dspMode();
        emit vfo->modeRequested(1, DSPMode::LSB);
        QCOMPARE(b->dspMode(), DSPMode::LSB);
        QCOMPARE(a->dspMode(), aMode);
        emit vfo->tuneStepRequested(1, kTuneStepList[11].stepHz);
        QCOMPARE(b->stepHz(), kTuneStepList[11].stepHz);
        emit vfo->frequencyChangeRequested(1, 1000);
        QCOMPARE(b->frequency(), 7'075'000.0);
        QCOMPARE(a->frequency(), 14'074'000.0);
        emit vfo->frequencySetRequested(1, 7'074'750);
        QCOMPARE(b->frequency(), 7'074'750.0);
        emit vfo->lockToggleRequested(1);
        QVERIFY(b->locked());
        emit vfo->frequencyChangeRequested(1, 1000);
        emit vfo->modeRequested(1, DSPMode::USB);
        emit vfo->bandRequested(1, Band::Band20m);
        QCOMPARE(b->frequency(), 7'074'750.0);
        QCOMPARE(b->dspMode(), DSPMode::LSB);
        emit vfo->lockToggleRequested(1);
        QVERIFY(!b->locked());
        emit vfo->bandRequested(1, Band::Band20m);
        QCOMPARE(bandFromFrequency(b->frequency()), Band::Band20m);

        // A slice that is not open: nothing.
        emit vfo->txRequested(3);
        QCOMPARE(handoff.count(), 1);

        // A VFO display face's own display is wired too, and an entry whose
        // source its canvas cannot read acts on nothing.
        auto* face = new VfoDisplayPresetItem();
        meter->addItem(face);
        VfoDisplayItem* inner = face->vfoDisplay();
        QVERIFY(inner && inner->property("vfoRequestsWired").toBool());
        const double bBefore = b->frequency();
        emit inner->frequencyChangeRequested(1, 100);
        QCOMPARE(b->frequency(), bBefore + 100.0);
        inner->setProperty("containerUnsupportedSource", true);
        emit inner->frequencyChangeRequested(1, 100);
        emit inner->txRequested(0);
        QCOMPARE(b->frequency(), bBefore + 100.0);
        QCOMPARE(handoff.count(), 1);

        // Another radio's entry drives nothing in this window.
        vfo->setProperty("containerSourceContext",
                         QJsonObject{{"sessionId", "another-radio"}, {"sliceId", 0}});
        const double before = a->frequency();
        emit vfo->txRequested(0);
        emit vfo->frequencyChangeRequested(0, 1000);
        emit vfo->lockToggleRequested(0);
        QCOMPARE(handoff.count(), 1);
        QCOMPARE(model->txBoundSlice(), b);
        QCOMPARE(a->frequency(), before);
        QVERIFY(!a->locked());
        QCOMPARE(mox.count(), 0);
        QCOMPARE(tune.count(), 0);
        if (moxState) { QCOMPARE(moxState->count(), 0); }
    }
    // Fix wave I2: a VFO display (every mode) and a dial on another
    // session's radio show none of this window's slices; the display says
    // why, with the reason its clicks give.
    void anotherRadiosEntryShowsNoSlices()
    {
        AppSettings::setProfileOverride(QStringLiteral("vfo-foreign-%1").arg(QCoreApplication::applicationPid()));
        AppSettings::instance().clear();
        QVERIFY(AppSettings::instance().save());
        RadioDiscovery discovery;
        discovery.holdOffScans(std::chrono::minutes(5));
        const auto cleanup = qScopeGuard([] {
            QFile::remove(AppSettings::instance().filePath());
            RadioDiscovery::clearHoldOffForTest();
        });
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        RadioModel* model = window.radioModel();
        model->configureStreamPool(5, 5, 192000);
        while (model->slices().size() < 2) { QVERIFY(model->addSlice() >= 0); }
        SliceModel* a = model->sliceById(0);
        QVERIFY(a);
        // A frequency no item shows unless it is fed this window's slice.
        a->setFrequency(14'123'456.0);
        ContainerManager* cm = model->containerManager();
        QVERIFY(cm);
        ContainerWidget* container = cm->createContainer(1, DockMode::OverlayDocked);
        QVERIFY(container);
        auto* meter = new MeterWidget();
        container->setContent(meter);
        const QJsonObject foreign{{"sessionId", "another-radio"}, {"sliceId", 0}};
        QList<VfoDisplayItem*> vfos;
        for (const auto mode : {VfoDisplayItem::DisplayMode::SourceSlice,
                                VfoDisplayItem::DisplayMode::AllSlices,
                                VfoDisplayItem::DisplayMode::LegacySliceB}) {
            auto* vfo = new VfoDisplayItem();
            vfo->setDisplayMode(mode);
            vfo->setRect(0, 0, 1, 1);
            vfo->setProperty("containerSourceContext", foreign);
            meter->addItem(vfo);
            vfos.append(vfo);
        }
        auto* dial = new DialItem();
        dial->setRect(0, 0, 1, 1);
        dial->setProperty("containerSourceContext", foreign);
        meter->addItem(dial);
        // A slice change refreshes every container.
        a->setStepHz(kTuneStepList[3].stepHz);
        a->setStepHz(kTuneStepList[10].stepHz);
        QCoreApplication::processEvents();
        for (VfoDisplayItem* vfo : vfos) {
            QVERIFY(vfo->sliceState(0).frequencyHz != 14'123'456);
            QCOMPARE(vfo->unavailableText(), QStringLiteral("This control belongs to another radio"));
        }
        QCOMPARE(dial->sourceSliceIndex(), -1);
        QCOMPARE(dial->activeSliceIndex(), -1);
        QCOMPARE(dial->sliceState(0).index, -1);

        // The same items on this window's radio show its slices.
        for (VfoDisplayItem* vfo : vfos) { vfo->setProperty("containerSourceContext", QJsonObject{{"sliceId", 0}}); }
        dial->setProperty("containerSourceContext", QJsonObject{{"sliceId", 0}});
        a->setStepHz(kTuneStepList[4].stepHz);
        a->setStepHz(kTuneStepList[11].stepHz);
        QCoreApplication::processEvents();
        for (VfoDisplayItem* vfo : vfos) {
            QCOMPARE(vfo->sliceState(0).frequencyHz, qint64(14'123'456));
            QVERIFY(vfo->unavailableText().isEmpty());
        }
        QCOMPARE(dial->sliceState(0).index, 0);
    }
};

QTEST_MAIN(TestVfoDisplayItemInput)
#include "tst_vfo_display_item_input.moc"
